/*
    Engine1999 - A 2D games engine written in C
    Copyright (C) 2026  Ekkehard Morgenstern

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.

    ADDITIONAL PERMISSION (GPLv3 section 7): the Engine1999 Runtime Exception
        applies. Programs written in the built-in programming language, and
        executables made from them with BUILD, do not fall under this license
        as the exception describes (see the file LICENSE-EXCEPTION).

    CONTACT INFO:
        E-Mail: ekkehard@ekkehardmorgenstern.de
        Mail: Ekkehard Morgenstern, Mozartstr. 1, D-76744 Woerth am Rhein,
              Germany, Europe
*/

#include "sdlscreen.h"
#include "ansi.h"
#include "sdllayer.h"
#include "sdlevent.h"
#include "textscreen.h"
#include "tilescreen.h"
#include "sprscreen.h"
#include "sdlutil.h"
#include "gfxcore.h"
#include "sdlkeys.h"
#include "keystate.h"
#include "cursors.h"
#include "mouse.h"
#include "sdljoy.h"
#include "scrdrv.h"

#define NLAYERS 5
#define LAY_BG          0
#define LAY_TIL         1
#define LAY_TXT         2
#define LAY_GFX         3
#define LAY_SPR         4

static sdllayer_t layers[NLAYERS];
static const char* const layer_titles[NLAYERS] = {
    "background", "tiles", "text", "graphics", "sprites"
};

static bool init_layers( SDL_Renderer* renderer ) {
    return sdllay_init_many( &layers[0], NLAYERS, layer_titles, renderer );
}

static void cleanup_layers( void ) {
    sdllay_cleanup_many( &layers[0], NLAYERS );
}

static void draw_layers( SDL_Renderer* renderer ) {
    sdllay_draw_texture_many( &layers[0], NLAYERS, renderer );
}

static void print_sdlerror( const char* scope ) {
    const char* msg = SDL_GetError();
    fprintf( stderr, "SDL error [%s]: %s\n", scope, msg );
}

static void render_tiles( struct _sdllayer_t* lay, void* usrdata ) {
    tilescr_render( lay->memory );
}

static void render_text( struct _sdllayer_t* lay, void* usrdata ) {
    txtscr_render( lay->memory );
}

static void render_sprites( struct _sdllayer_t* lay, void* usrdata ) {
    sprscr_render( lay->memory );
}

static bool sdlscr_initok = false;
static bool sdlscr_doexit = false;

#define SDLSCR_DEFAULT_BGCOL    UINT32_C(0XFF708090)        // SlateGray
static uint32_t sdlscr_bgcol = SDLSCR_DEFAULT_BGCOL;

static SDL_Thread* sdlscr_workerthr = 0;

static uint64_t sdlscr_framecnt = 0;
static volatile bool sdlscr_awaitinput = false;     // line input in progress: queue keys, show the cursor
static volatile bool sdlscr_pollinput = false;      // a program is running: queue keys, ESC = break
static volatile bool sdlscr_breakreq = false;
static SDL_mutex* sdlscr_inputmtx = 0;

#define SDLSCR_INPUTQUEUE_SIZE  16

static sdlscr_inputmsg_t*   sdlscr_inputqueue[SDLSCR_INPUTQUEUE_SIZE];
static int                  sdlscr_iqr = 0;
static int                  sdlscr_iqw = 0;

void sdlscr_enableinput( bool enable ) {
    sdlscr_awaitinput = enable;
}

sdlscr_inputmsg_t* sdlscr_dequeueinputmsg( void ) {
    SDL_LockMutex( sdlscr_inputmtx );
    if ( sdlscr_iqr == sdlscr_iqw ) {
        SDL_UnlockMutex( sdlscr_inputmtx );
        return 0;
    }
    sdlscr_inputmsg_t* msg = sdlscr_inputqueue[ sdlscr_iqr++ ];
    if ( sdlscr_iqr >= SDLSCR_INPUTQUEUE_SIZE ) {
        sdlscr_iqr = 0;
    }
    SDL_UnlockMutex( sdlscr_inputmtx );
    return msg;
}

static bool sdlscr_enqueueinputmsg( sdlscr_inputmsg_t* msg ) {
    SDL_LockMutex( sdlscr_inputmtx );
    int limit = sdlscr_iqr - 1;
    if ( limit < 0 ) limit += SDLSCR_INPUTQUEUE_SIZE;
    if ( sdlscr_iqw == limit ) {
        SDL_UnlockMutex( sdlscr_inputmtx );
        return false;
    }
    sdlscr_inputqueue[ sdlscr_iqw++ ] = msg;
    if ( sdlscr_iqw >= SDLSCR_INPUTQUEUE_SIZE ) {
        sdlscr_iqw = 0;
    }
    SDL_UnlockMutex( sdlscr_inputmtx );
    return true;
}

static sdlscr_inputmsg_t* sdlscr_createinputmsg( int inputtype ) {
    sdlscr_inputmsg_t* msg = (sdlscr_inputmsg_t*) SDL_malloc( sizeof(sdlscr_inputmsg_t) );
    if ( msg == 0 ) return 0;
    msg->inputtype = inputtype;
    return msg;
}

void sdlscr_deleteinputmsg( sdlscr_inputmsg_t* msg ) {
    SDL_free( msg );
}

static bool sdlscr_enqueuekeypress( SDL_Keycode key, Uint16 mods ) {
    sdlscr_inputmsg_t* msg = sdlscr_createinputmsg( SDLSCR_INPUT_KEYPRESS );
    if ( msg == 0 ) return false;
    msg->symbol = key;
    msg->mods = mods;
    if ( !sdlscr_enqueueinputmsg( msg ) ) {
        sdlscr_deleteinputmsg( msg );
        return false;
    }
    return true;
}

// a mouse event for the full-screen editor (the cell of the 80 x 25 text screen under the pointer; x, y are the pointer in the layer's pixels)
static bool sdlscr_inputpending( void );
static volatile bool sdlscr_wantmouse = false;
static bool sdlscr_lbdown = false;                  // the left button is held (for the editor): SM_REPEAT every SM_REPEAT_MS after SM_REPEAT_DELAY_MS, at the pointer's cell
static Uint32 sdlscr_lbnext = 0;
static int sdlscr_lbx = 0, sdlscr_lby = 0;

static bool sdlscr_enqueuemouse( int mouse, int x, int y, int button, int wheel ) {
    sdlscr_inputmsg_t* msg = sdlscr_createinputmsg( SDLSCR_INPUT_MOUSE );
    if ( msg == 0 ) return false;
    msg->mouse = mouse; msg->button = button; msg->wheel = wheel;
    msg->mx = x < 0 ? 0 : x / 8; msg->my = y < 0 ? 0 : y / 12;
    if ( msg->mx > 79 ) msg->mx = 79;
    if ( msg->my > 24 ) msg->my = 24;
    if ( !sdlscr_enqueueinputmsg( msg ) ) {
        sdlscr_deleteinputmsg( msg );
        return false;
    }
    sdlev_raise( SDLEV_INPUT );
    return true;
}

static bool sdlscr_enqueuetext( const char text[SDL_TEXTINPUTEVENT_TEXT_SIZE] ) {
    sdlscr_inputmsg_t* msg = sdlscr_createinputmsg( SDLSCR_INPUT_TEXT );
    if ( msg == 0 ) return false;
    snprintf( msg->text, SDL_TEXTINPUTEVENT_TEXT_SIZE, "%s", text );
    if ( !sdlscr_enqueueinputmsg( msg ) ) {
        sdlscr_deleteinputmsg( msg );
        return false;
    }
    return true;
}

uint64_t sdlscr_getframecnt( void ) {
    return sdlscr_framecnt;
}

// -- key debugging (--debug-keys) ---------------------------------------------------------------

static volatile bool sdlscr_keydebug = false;

void sdlscr_setkeydebug( bool on ) {
    sdlscr_keydebug = on;
}

// -- frame timing (--debug-frames) -----------------------------------------------------------------

static volatile bool sdlscr_framedebug = false;

void sdlscr_setframedebug( bool on ) {
    sdlscr_framedebug = on;
}

static void sdlscr_modnames( Uint16 mods, char* out, size_t cap ) {
    static const struct { Uint16 bit; const char* name; } names[] = {
        { KMOD_LSHIFT, "LShift" }, { KMOD_RSHIFT, "RShift" }, { KMOD_LCTRL, "LCtrl" }, { KMOD_RCTRL, "RCtrl" }, { KMOD_LALT, "LAlt" }, { KMOD_RALT, "RAlt" },
        { KMOD_LGUI, "LGui" }, { KMOD_RGUI, "RGui" }, { KMOD_NUM, "Num" }, { KMOD_CAPS, "Caps" }, { KMOD_MODE, "Mode" } };
    size_t n = 0;
    out[0] = '\0';
    for ( size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i ) {
        if ( ( mods & names[i].bit ) != 0 && n + 12U < cap ) {
            n += (size_t) snprintf( out + n, cap - n, "%s%s", n ? "+" : "", names[i].name );
        }
    }
}

// one line per event on standard output: what SDL says and the sequence made from it (the same code that feeds GET / INKEY$)
static void sdlscr_debugkeyevent( const SDL_KeyboardEvent* k ) {
    char mods[96], seq[KEYSEQ_MAXLEN], hex[64], esc[64], name[KEYSEQ_NAMELEN];
    sdlscr_modnames( k->keysym.mod, mods, sizeof(mods) );
    size_t n = sdlkeys_sequence( k->keysym.sym, k->keysym.mod, seq );
    printf( "[keys] %8u ms  SDL key %-4s sym=0x%08x \"%s\" scancode=%d mods=0x%04x(%s)%s", (unsigned) k->timestamp, k->type == SDL_KEYDOWN ? "down" : "up",
        (unsigned) k->keysym.sym, SDL_GetKeyName( k->keysym.sym ), (int) k->keysym.scancode, (unsigned) k->keysym.mod, mods, k->repeat ? " repeat" : "" );
    if ( n > 0 && k->type == SDL_KEYDOWN ) {
        int key, km;
        sdlkeys_map( k->keysym.sym, k->keysym.mod, &key, &km );
        keyseq_name( key, km, name );
        keyseq_hex( seq, n, hex, sizeof(hex) );
        keyseq_escape( seq, n, esc, sizeof(esc) );
        printf( "  ->  %s  \"%s\"  = %s", hex, esc, name );
    } else if ( k->type == SDL_KEYDOWN ) {
        printf( "  ->  (no sequence)" );
    }
    printf( "\n" );
    fflush( stdout );
}

static void sdlscr_debugtextevent( const SDL_TextInputEvent* t ) {
    char hex[64], esc[64];
    size_t len = strlen( t->text );
    keyseq_hex( t->text, len, hex, sizeof(hex) );
    keyseq_escape( t->text, len, esc, sizeof(esc) );
    printf( "[keys] %8u ms  SDL text input  %s  \"%s\"  (UTF-8, as the keyboard layout and the shift state make it)\n", (unsigned) t->timestamp, hex, esc );
    fflush( stdout );
}

static volatile bool sdlscr_escbreak = true;         // ESC is the break key (BREAK ON); with BREAK OFF it is an ordinary key

void sdlscr_setescbreak( bool on ) {
    sdlscr_escbreak = on;
}

// the break keys while a program runs: ESC (unless BREAK OFF), Ctrl+C, PAUSE/BREAK (also with Ctrl)
static bool sdlscr_isbreakkey( SDL_KeyCode key ) {
    if ( key == SDLK_PAUSE || key == SDLK_CANCEL ) {
        return true;
    }
    if ( key == SDLK_c && ( SDL_GetModState() & KMOD_CTRL ) != 0 ) {
        return true;
    }
    return key == SDLK_ESCAPE && sdlscr_escbreak;
}

// The window title: the name of the running executable (a program made with BUILD shows its own name), "Engine1999" if it can't be found.
static void sdlscr_settitle( SDL_Window* window ) {
    char path[4096];
    ssize_t len = readlink( "/proc/self/exe", path, sizeof(path) - 1U );
    const char* name = "Engine1999";
    if ( len > 0 ) {
        path[len] = '\0';
        char* slash = strrchr( path, '/' );
        const char* base = slash ? slash + 1 : path;
        const char* deleted = strstr( base, " (deleted)" );     // (an executable that was replaced while it runs)
        if ( deleted ) {
            path[ deleted - path ] = '\0';
        }
        if ( *base != '\0' ) {
            name = base;
        }
    }
    SDL_SetWindowTitle( window, name );
}

// F11 and ALT+RETURN switch the window between the full screen (the desktop's size) and a normal window. The window manager does not do this for
// F11: the application has to. The keys are used up here (a program does not see them; KEY("F11") still works, it reads the keyboard state).
static bool sdlscr_isfullscreenkey( const SDL_KeyboardEvent* k ) {
    return k->keysym.sym == SDLK_F11 || ( k->keysym.sym == SDLK_RETURN && ( k->keysym.mod & KMOD_ALT ) != 0 );
}

static void sdlscr_togglefullscreen( SDL_Window* window ) {
    bool isfull = ( SDL_GetWindowFlags( window ) & SDL_WINDOW_FULLSCREEN_DESKTOP ) != 0;
    if ( isfull ) {
        // to a window: a whole multiple of the 640 x 300 screen that fits into 80 % of the display, centered, resizable (the picture is scaled)
        int scale = 1;
        SDL_Rect usable;
        int disp = SDL_GetWindowDisplayIndex( window );
        if ( disp >= 0 && SDL_GetDisplayUsableBounds( disp, &usable ) == 0 ) {
            int sx = usable.w * 8 / 10 / SDL_SCREENWIDTH, sy = usable.h * 8 / 10 / SDL_SCREENHEIGHT;
            scale = sx < sy ? sx : sy;
            if ( scale < 1 ) {
                scale = 1;
            }
        }
        if ( SDL_SetWindowFullscreen( window, 0 ) != 0 ) {
            print_sdlerror( "leave fullscreen mode" );
            return;
        }
        SDL_SetWindowResizable( window, SDL_TRUE );
        SDL_SetWindowSize( window, SDL_SCREENWIDTH * scale, SDL_SCREENHEIGHT * scale );
        SDL_SetWindowPosition( window, SDL_WINDOWPOS_CENTERED_DISPLAY( disp >= 0 ? disp : 0 ), SDL_WINDOWPOS_CENTERED_DISPLAY( disp >= 0 ? disp : 0 ) );
    } else if ( SDL_SetWindowFullscreen( window, SDL_WINDOW_FULLSCREEN_DESKTOP ) != 0 ) {
        print_sdlerror( "enter fullscreen mode" );
    }
}

static void sdlscr_handlekey( SDL_KeyCode key ) {
    if ( !sdlscr_awaitinput && !sdlscr_pollinput ) return;
    if ( sdlscr_pollinput && sdlscr_isbreakkey( key ) ) {
        sdlscr_breakreq = true;
        sdlev_raise( SDLEV_BREAK );
        return;
    }
    if ( sdlscr_enqueuekeypress( key, (Uint16) SDL_GetModState() ) ) {
        sdlev_raise( SDLEV_INPUT );
    }
}

// text from the keyboard (UTF-8, as the layout makes it) as the printable ISO-8859-15 characters in it; the others are dropped. Returns the length.
static size_t sdlscr_textlatin9( const char* utf8, char* out, size_t cap ) {
    char tmp[ SDL_TEXTINPUTEVENT_TEXT_SIZE ];
    size_t len = strlen( utf8 );
    if ( len >= sizeof(tmp) ) {
        len = sizeof(tmp) - 1U;
    }
    size_t m = keyseq_utf8_to_latin9( utf8, len, tmp ), n = 0;
    for ( size_t i = 0; i < m && n + 1U < cap; ++i ) {
        int c = (unsigned char) tmp[i];
        if ( ( c >= 32 && c < 127 ) || c >= 160 ) {
            out[n++] = (char) c;
        }
    }
    out[n] = '\0';
    return n;
}

// a printable character of the character set (not a control code, not DEL)
#define SDLSCR_PRINTABLE(c)     ( ( (c) >= 32 && (c) < 127 ) || ( (c) >= 160 && (c) <= 255 ) )

static void sdlscr_handletext( const char text[SDL_TEXTINPUTEVENT_TEXT_SIZE] ) {
    if ( !sdlscr_awaitinput && !sdlscr_pollinput ) return;
    char latin[SDL_TEXTINPUTEVENT_TEXT_SIZE];
    if ( sdlscr_textlatin9( text, latin, sizeof(latin) ) == 0 ) return;
    if ( sdlscr_enqueuetext( latin ) ) {
        sdlev_raise( SDLEV_INPUT );
    }
}

// the index of the display that holds the mouse pointer (0 if it can't be found or SDL_VIDEO_FULLSCREEN_DISPLAY is set)
static int sdlscr_pointerdisplay( void ) {
    if ( getenv( "SDL_VIDEO_FULLSCREEN_DISPLAY" ) != 0 || getenv( "SDL_VIDEO_FULLSCREEN_HEAD" ) != 0 ) {
        return 0;
    }
    int n = SDL_GetNumVideoDisplays();
    if ( n < 2 ) {
        return 0;
    }
    int mx = 0, my = 0;
    SDL_GetGlobalMouseState( &mx, &my );
    for ( int i = 0; i < n; ++i ) {
        SDL_Rect r;
        if ( SDL_GetDisplayBounds( i, &r ) == 0 && mx >= r.x && mx < r.x + r.w && my >= r.y && my < r.y + r.h ) {
            return i;
        }
    }
    return 0;
}

// bring the window to the front: the window manager may have opened it behind other windows or not given it the focus
static void sdlscr_tryraise( SDL_Window* window ) {
    if ( ( SDL_GetWindowFlags( window ) & SDL_WINDOW_INPUT_FOCUS ) == 0 ) {
        SDL_ShowWindow( window );
        SDL_RaiseWindow( window );
    }
}

static int sdlscr_worker( void* arg ) {

    sdlscr_initok = false;
    sdlscr_doexit = false;

    sdlscr_inputmtx = SDL_CreateMutex();
    if ( sdlscr_inputmtx == 0 ) {
        print_sdlerror( "create input mutex" );
        goto ERR1;
    }

    // the window opens on the display where the pointer is (the user started the program from a window on that display, as a rule);
    // SDL_VIDEO_FULLSCREEN_DISPLAY, if set, decides instead
    int startdisp = sdlscr_pointerdisplay();
    SDL_SetHint( SDL_HINT_FORCE_RAISEWINDOW, "1" );     // ask the window manager to activate the window, not only to raise it
    SDL_Window* window = SDL_CreateWindow(
        "",
        SDL_WINDOWPOS_CENTERED_DISPLAY( startdisp ),
        SDL_WINDOWPOS_CENTERED_DISPLAY( startdisp ),
        0,
        0,
        SDL_WINDOW_FULLSCREEN_DESKTOP
    );
    if ( window == 0 ) {
        print_sdlerror( "create window" );
        goto ERR2;
    }

    sdlscr_settitle( window );

    SDL_Renderer* renderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if ( renderer == 0 ) {
        print_sdlerror( "create renderer" );
        goto ERR3;
    }

    SDL_SetHint( SDL_HINT_RENDER_SCALE_QUALITY, "linear" );
    int rv = SDL_RenderSetLogicalSize(
        renderer,
        SDL_SCREENWIDTH,
        SDL_SCREENHEIGHT
    );
    if ( rv < 0 ) {
        print_sdlerror( "set logical size" );
        goto ERR4;
    }

    if ( !init_layers( renderer ) ) {
        fprintf( stderr, "failed to initialize layers\n" );
        goto ERR4;
    }

    sdllay_setcall( &layers[LAY_TIL], render_tiles, 0 );
    sdllay_setcall( &layers[LAY_TXT], render_text, 0 );
    sdllay_setcall( &layers[LAY_SPR], render_sprites, 0 );

    txtscr_init();
    tilescr_init();
    sprscr_init();
    mouse_init();
    sdljoy_init();                          // the game controllers (events of plugged in pads arrive in the loop below)
    SDL_ShowCursor( SDL_DISABLE );          // the pointer is the engine's own (cursors.h, mouse.h): shown for a while after the mouse did something

    // sdllay_disable( &layers[LAY_BG]  );
    // sdllay_disable( &layers[LAY_TIL] );
    // sdllay_disable( &layers[LAY_TXT] );
    // sdllay_disable( &layers[LAY_GFX] );
    // sdllay_disable( &layers[LAY_SPR] );

    // confirm init ok
    sdlscr_initok = true;
    sdlev_raise( SDLEV_SCREENWORKERINITDONE );

    // 1/60 s; lts/lastTick: due time of the current frame
    const uint64_t FRAME_NSEC = UINT64_C(16666667);
    struct timespec lts;
    uint64_t lastTick = sdlutil_getnsec( &lts );

    bool oldwantinput = false;
    uint64_t prevf0 = 0;                // --debug-frames: the start of the previous frame

    // main loop
    for (;;) {
        if ( sdlscr_doexit ) {
            break;
        }

        ++sdlscr_framecnt;
        if ( sdlscr_framecnt <= 180 && sdlscr_framecnt % 10 == 1 ) {         // up to 3 seconds: ask again every 10 frames until the window has the focus
            sdlscr_tryraise( window );
        }

        // --debug-frames: the times of this frame (nanoseconds from the monotonic clock; printed as microseconds)
        struct timespec fts;
        uint64_t tf0 = sdlscr_framedebug ? sdlutil_getnsec( &fts ) : 0;
        int fmouse = 0;                 // mouse events polled in this frame
        Uint32 foldest = 0;             // the SDL timestamp (ms) of the oldest of them

        bool wantinput = sdlscr_awaitinput || sdlscr_pollinput;
        if ( wantinput != oldwantinput ) {
            if ( wantinput ) {
                SDL_StartTextInput();
            } else {
                SDL_StopTextInput();
            }
            oldwantinput = wantinput;
        }

        if ( sdlscr_awaitinput && ( txtscr_enablecursor( true ) || txtscr_blinkcursor() ) ) {
            sdllay_set_modified( &layers[LAY_TXT] );
        } else if ( !sdlscr_awaitinput && txtscr_enablecursor( false ) ) {
            sdllay_set_modified( &layers[LAY_TXT] );
        }

        if ( sdllay_enabled( &layers[LAY_SPR] ) ) {
            sprscr_periodicals();
            if ( sprscr_changed() ) {
                sdllay_set_modified( &layers[LAY_SPR] );
            }
        }

        // process messages
        int dragx = 0, dragy = 0;
        bool dragged = false;
        SDL_Event ev;
        while ( SDL_PollEvent( &ev ) ) {
            Uint32 ty = ev.type;
            switch ( ty ) {
                case SDL_QUIT:
                    sdlscr_doexit = true;
                    break;
                case SDL_KEYDOWN:
                    if ( sdlscr_keydebug ) {
                        sdlscr_debugkeyevent( &ev.key );
                    }
                    if ( sdlscr_isfullscreenkey( &ev.key ) ) {
                        if ( !ev.key.repeat ) {
                            sdlscr_togglefullscreen( window );
                        }
                        break;
                    }
                    if ( sdlscr_pollinput && !ev.key.repeat ) {         // (a running program: the key events of ON KEY and KEY$("EVENT"))
                        int kid = sdlkeys_keyid( ev.key.keysym.sym );
                        if ( kid > 0 ) {
                            keystate_event_push( kid, true );
                        }
                    }
                    sdlscr_handlekey( ev.key.keysym.sym );
                    break;
                case SDL_KEYUP:
                    if ( sdlscr_keydebug ) {
                        sdlscr_debugkeyevent( &ev.key );
                    }
                    if ( sdlscr_pollinput ) {
                        int kid = sdlkeys_keyid( ev.key.keysym.sym );
                        if ( kid > 0 ) {
                            keystate_event_push( kid, false );
                        }
                    }
                    break;
                case SDL_MOUSEMOTION:
                    if ( fmouse++ == 0 ) foldest = ev.motion.timestamp;
                    mouse_event_motion( ev.motion.x, ev.motion.y );
                    if ( sdlscr_lbdown ) {
                        sdlscr_lbx = ev.motion.x; sdlscr_lby = ev.motion.y;
                    }
                    if ( sdlscr_wantmouse && ( ev.motion.state & SDL_BUTTON_LMASK ) != 0 ) {
                        dragx = ev.motion.x; dragy = ev.motion.y; dragged = true;      // (one event per frame: see below)
                    }
                    break;
                case SDL_MOUSEBUTTONDOWN:
                case SDL_MOUSEBUTTONUP: {
                    // (the position is that of the renderer's logical size, the sprite layer's: 640 x 300)
                    if ( fmouse++ == 0 ) foldest = ev.button.timestamp;
                    mouse_event_motion( ev.button.x, ev.button.y );
                    int b = ev.button.button == SDL_BUTTON_LEFT ? MOUSE_LEFT : ev.button.button == SDL_BUTTON_RIGHT ? MOUSE_RIGHT :
                        ev.button.button == SDL_BUTTON_MIDDLE ? MOUSE_MIDDLE : 0;
                    mouse_event_button( b, ty == SDL_MOUSEBUTTONDOWN );
                    if ( sdlscr_wantmouse && ev.button.button == SDL_BUTTON_LEFT ) {
                        sdlscr_enqueuemouse( ty == SDL_MOUSEBUTTONDOWN ? SM_PRESS : SM_RELEASE, ev.button.x, ev.button.y, 1, 0 );
                        dragged = false;
                        sdlscr_lbdown = ty == SDL_MOUSEBUTTONDOWN;
                        sdlscr_lbx = ev.button.x; sdlscr_lby = ev.button.y;
                        sdlscr_lbnext = SDL_GetTicks() + SM_REPEAT_DELAY_MS;
                    }
                    break;
                }
                case SDL_MOUSEWHEEL:
                    if ( fmouse++ == 0 ) foldest = ev.wheel.timestamp;
                    mouse_event_wheel( ev.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -ev.wheel.y : ev.wheel.y );
                    if ( sdlscr_wantmouse && ev.wheel.y != 0 ) {
                        int wy = ev.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -ev.wheel.y : ev.wheel.y;
                        int steps = wy < 0 ? -wy : wy;
                        for ( int i = 0; i < steps && i < 4; ++i ) {
                            sdlscr_enqueuemouse( SM_WHEEL, 0, 0, 0, wy > 0 ? 1 : -1 );     // (the wheel is not tied to a place)
                        }
                    }
                    break;
                case SDL_TEXTINPUT:
                    if ( sdlscr_keydebug ) {
                        sdlscr_debugtextevent( &ev.text );
                    }
                    sdlscr_handletext( ev.text.text );
                    break;
                default:
                    sdljoy_handle_event( &ev );     // the game controllers (joy.h)
                    break;
            }
        }

        if ( sdlscr_lbdown && !sdlscr_wantmouse ) {
            sdlscr_lbdown = false;
        }
        if ( sdlscr_lbdown && (Sint32)( SDL_GetTicks() - sdlscr_lbnext ) >= 0 ) {
            if ( !sdlscr_inputpending() ) {         // (an editor that is behind gets no pile of repeats)
                sdlscr_enqueuemouse( SM_REPEAT, sdlscr_lbx, sdlscr_lby, 1, 0 );
            }
            sdlscr_lbnext = SDL_GetTicks() + SM_REPEAT_MS;
        }
        if ( dragged && sdlscr_wantmouse ) {
            sdlscr_enqueuemouse( SM_DRAG, dragx, dragy, 1, 0 );        // (the last position of the frame)
        }

        // the mouse pointer: the events of this frame are applied now, so that the pointer is drawn where the mouse is as of this frame's poll
        // (the events and requests are taken, the pointer is shown or hidden: the only place that changes it)
        struct timespec tfs;
        uint64_t tf1 = sdlscr_framedebug ? sdlutil_getnsec( &tfs ) : 0;     // after the events were polled
        {
            struct timespec mts;
            uint64_t mnow = sdlutil_getnsec( &mts ) / UINT64_C(1000000);
            mouse_tick( mnow );
            int wx = 0, wy = 0;
            if ( mouse_warp_wanted( &wx, &wy ) ) {
                int px = 0, py = 0;
                SDL_RenderLogicalToWindow( renderer, (float) wx, (float) wy, &px, &py );
                SDL_WarpMouseInWindow( window, px, py );
            }
            if ( sdllay_enabled( &layers[LAY_SPR] ) && sprscr_changed() ) {
                sdllay_set_modified( &layers[LAY_SPR] );
            }
        }
        uint64_t tf2 = sdlscr_framedebug ? sdlutil_getnsec( &tfs ) : 0;     // after the pointer was moved
        bool fredraw = false;

        if ( sdllay_needsredraw( &layers[0], NLAYERS ) ) {
            // needs redraw
            if ( !sdllay_to_texture_many( &layers[0], NLAYERS ) ) {
                break;
            }
            uint32_t c = sdlscr_bgcol;
            uint8_t a = (uint8_t)( c >> UINT8_C(24) );
            uint8_t r = (uint8_t)( c >> UINT8_C(16) );
            uint8_t g = (uint8_t)( c >> UINT8_C( 8) );
            uint8_t b = (uint8_t)( c                );
            SDL_SetRenderDrawColor( renderer, r, g, b, a );
            SDL_RenderClear( renderer );
            draw_layers( renderer );
            fredraw = true;
        }
        uint64_t tf3 = sdlscr_framedebug ? sdlutil_getnsec( &tfs ) : 0;     // after the layers were drawn (the GPU may still be working)

        sdlev_raise( SDLEV_VBLANK );
        SDL_RenderPresent( renderer );
        uint64_t tf4 = sdlscr_framedebug ? sdlutil_getnsec( &tfs ) : 0;     // after the present (it waits for the vertical blank)

        // pace the frames: each one is due exactly one period after the previous one's due time
        // (not after the time it was actually finished, so jitter and the work done in a frame don't
        // accumulate into a lower or irregular rate); sleeping covers the display's own rate if it is faster
        lastTick += FRAME_NSEC;
        sdlutil_projecttime( FRAME_NSEC, &lts, &lts );
        struct timespec ts;
        uint64_t now = sdlutil_getnsec( &ts );
        if ( now < lastTick ) {
            sdlutil_nanosleep( 0, &lts );
        } else if ( now - lastTick > FRAME_NSEC ) {
            // more than a frame late (slow machine, suspended, clock set): don't try to catch up
            lastTick = now; lts = ts;
        }
        if ( sdlscr_framedebug && fmouse > 0 ) {
            // one line for every frame in which the mouse did something; all times in microseconds
            uint64_t tf5 = sdlutil_getnsec( &tfs );
            unsigned age = (unsigned)( SDL_GetTicks() - foldest );      // (SDL's time stamps have a resolution of 1 ms: this one is a multiple of 1000 us)
            printf( "[frames] #%-6u mouse events %3d | period %6llu | poll at +%5llu | pointer moved %4llu | redraw %5llu%s | present %6llu | sleep %6llu | oldest event %5u (SDL ms) -> on screen after about %llu\n",
                (unsigned) sdlscr_framecnt, fmouse,
                (unsigned long long)( prevf0 ? ( tf0 - prevf0 ) / 1000U : 0U ), (unsigned long long)( ( tf1 - tf0 ) / 1000U ),
                (unsigned long long)( ( tf2 - tf1 ) / 1000U ), (unsigned long long)( ( tf3 - tf2 ) / 1000U ), fredraw ? "" : " (none)",
                (unsigned long long)( ( tf4 - tf3 ) / 1000U ), (unsigned long long)( ( tf5 - tf4 ) / 1000U ),
                age * 1000U, (unsigned long long)( age * 1000U + ( tf4 - tf1 ) / 1000U ) );
            fflush( stdout );
        }
        if ( sdlscr_framedebug ) {
            prevf0 = tf0;
        }
    }

    if ( oldwantinput ) {
        SDL_StopTextInput();
    }

    // cleanup
    sdlscr_initok = false;
    sdljoy_shutdown();

/* ERR5: */ cleanup_layers();
ERR4:   SDL_DestroyRenderer( renderer );
ERR3:   SDL_DestroyWindow( window );
ERR2:   SDL_DestroyMutex( sdlscr_inputmtx ); sdlscr_inputmtx = 0;
ERR1:   sdlev_raise( SDLEV_SCREENWORKERINITDONE | SDLEV_SCREENWORKERFINISHED );
        return 0;
}

bool sdlscr_init( void ) {

    // create worker thread
    sdlscr_workerthr = SDL_CreateThread(
        sdlscr_worker,
        "sdlscr_worker",
        0
    );
    if ( sdlscr_workerthr == 0 ) {
        print_sdlerror( "sdlscr_init" );
        return false;
    }

    // wait for handshake (init completion)
    sdlev_wait( SDLEV_SCREENWORKERINITDONE );
    if ( !sdlscr_initok ) {
        // wait for thread to terminate and reap the thread status
        int rv = 0;
        SDL_WaitThread( sdlscr_workerthr, &rv );
        sdlscr_workerthr = 0;
        return false;
    }

    return true;
}

void sdlscr_cleanup( void ) {

    if ( sdlscr_workerthr == 0 ) {
        // thread already cleaned up or not running
        return;
    }

    // wait for handshake (thread termination)
    if ( sdlscr_initok ) {
        // thread did not reach its cleanup code yet: set termination flag and wait for signal
        sdlscr_doexit = true;
        sdlev_wait( SDLEV_SCREENWORKERFINISHED );
    }

    // wait for thread to terminate and reap the thread status
    int rv = 0;
    SDL_WaitThread( sdlscr_workerthr, &rv ); sdlscr_workerthr = 0;
}

void sdlscr_printf( int y, int x, int bg, int fg, const char* fmt, ... ) {
      static char buf[2048U];
      va_list ap;
      va_start( ap, fmt );
      vsnprintf( buf, 2048U, fmt, ap );
      va_end( ap );
      txtscr_print( y, x, bg, fg, buf );
      sdllay_set_modified( &layers[LAY_TXT] );
}

void sdlscr_puts( const char* text ) {
    txtscr_print( -1, -1, -1, -1, text );
    sdllay_set_modified( &layers[LAY_TXT] );
}

// text with ANSI escape sequences (SGR colors only, see ansi.h). No locking, like the other text screen functions: the
// screen thread may render while this writes, and the text screen tolerates that; rendering must never block.
// one decoder per text screen: the default colors and the colors set by a sequence belong to the screen
static ansi_t sdlscr_ansi[TXTSCR_NSCREENS];
static bool   sdlscr_ansiok[TXTSCR_NSCREENS];

static void sdlscr_ansi_emit( void* usrdata, int bg, int fg, const char* text, size_t len ) {
    txtscr_print_n( -1, -1, bg, fg, text, len );
    sdllay_set_modified( &layers[LAY_TXT] );
}

void sdlscr_ansi_puts( const char* text ) {
    sdlscr_ansi_write( text, strlen( text ) );
}

void sdlscr_ansi_write( const char* text, size_t len ) {
    int n = txtscr_selected();
    if ( !sdlscr_ansiok[n] ) {
        int bg = TXTSCR_BGCOL, fg = TXTSCR_FGCOL;
        txtscr_getcolors( &bg, &fg );
        ansi_init( &sdlscr_ansi[n], fg, bg );
        sdlscr_ansiok[n] = true;
    }
    ansi_feed_n( &sdlscr_ansi[n], text, len, sdlscr_ansi_emit, 0 );
}

void sdlscr_ansi_printf( const char* fmt, ... ) {
    char buf[1024U];
    va_list ap;
    va_start( ap, fmt );
    int n = vsnprintf( buf, sizeof(buf), fmt, ap );
    va_end( ap );
    if ( n < 0 ) {
        return;
    }
    if ( (size_t) n < sizeof(buf) ) {
        sdlscr_ansi_puts( buf );
        return;
    }
    // longer than the buffer: format again into a heap buffer instead of truncating
    char* big = (char*) malloc( (size_t) n + 1U );
    if ( big == 0 ) {
        sdlscr_ansi_puts( buf );        // truncated, but better than nothing
        return;
    }
    va_start( ap, fmt );
    vsnprintf( big, (size_t) n + 1U, fmt, ap );
    va_end( ap );
    sdlscr_ansi_puts( big );
    free( big );
}

// -- keyboard access for running programs ---------------------------------------------

// Keys that are not one byte arrive as ANSI escape sequences (keyseq.h): the host hands out one byte per call, the rest of the sequence waits here.
static char sdlscr_pend[ KEYSEQ_MAXLEN > SDL_TEXTINPUTEVENT_TEXT_SIZE ? KEYSEQ_MAXLEN : SDL_TEXTINPUTEVENT_TEXT_SIZE ];
static size_t sdlscr_pendpos = 0, sdlscr_pendlen = 0;

static void sdlscr_flushinput( void ) {
    sdlscr_pendpos = sdlscr_pendlen = 0;
    sdlscr_inputmsg_t* msg;
    while ( ( msg = sdlscr_dequeueinputmsg() ) != 0 ) {
        sdlscr_deleteinputmsg( msg );
    }
}

void sdlscr_pollkeys( bool enable ) {
    sdlscr_escbreak = true;                 // (a run starts, and ends, with BREAK ON)
    if ( enable ) {
        sdlscr_flushinput();
        sdlscr_breakreq = false;
        sdlev_clear( SDLEV_BREAK );
    }
    sdlscr_pollinput = enable;
    if ( !enable ) {
        sdlscr_flushinput();
    }
}

// the bytes a message stands for (0 = it isn't a key we deliver): RETURN 13, BACKSPACE 8, TAB 9, the escape sequence of a cursor / function key
// with its modifiers, the characters of ISO-8859-15
static size_t sdlscr_msgbytes( const sdlscr_inputmsg_t* msg, char* out ) {
    if ( msg->inputtype == SDLSCR_INPUT_KEYPRESS ) {
        switch ( msg->symbol ) {
            case SDLK_RETURN:       out[0] = 13; return 1U;
            case SDLK_BACKSPACE:    out[0] = 8;  return 1U;
            case SDLK_ESCAPE:       out[0] = 27; return 1U;     // (only reaches here after BREAK OFF)
            case SDLK_TAB:
                if ( ( msg->mods & KMOD_SHIFT ) == 0 ) {
                    out[0] = 9;
                    return 1U;
                }
                break;                  // Shift+Tab is a sequence (BackTab)
            default:                break;
        }
        return sdlkeys_sequence( msg->symbol, msg->mods, out );
    }
    if ( msg->inputtype == SDLSCR_INPUT_TEXT ) {
        size_t n = 0;
        for ( const char* p = msg->text; *p != '\0' && n < sizeof(sdlscr_pend); ++p ) {
            int chr = *p & 255;
            if ( SDLSCR_PRINTABLE( chr ) ) {
                out[n++] = (char) chr;
            }
        }
        return n;
    }
    return 0;
}

int sdlscr_pollkey( void ) {
    if ( sdlscr_pendpos < sdlscr_pendlen ) {
        return (unsigned char) sdlscr_pend[ sdlscr_pendpos++ ];
    }
    sdlscr_inputmsg_t* msg;
    while ( ( msg = sdlscr_dequeueinputmsg() ) != 0 ) {
        size_t n = sdlscr_msgbytes( msg, sdlscr_pend );
        sdlscr_deleteinputmsg( msg );
        if ( n > 0 ) {
            sdlscr_pendlen = n;
            sdlscr_pendpos = 1U;
            return (unsigned char) sdlscr_pend[0];
        }
    }
    return 0;
}

bool sdlscr_breakrequested( void ) {
    return sdlscr_breakreq || sdlscr_term();
}

int sdlscr_getkey( void ) {
    bool oldpoll = sdlscr_pollinput;
    sdlscr_pollinput = true;
    int key = 0;
    for (;;) {
        key = sdlscr_pollkey();
        if ( key ) {
            break;
        }
        int ev = sdlev_wait( SDLEV_INPUT | SDLEV_BREAK | SDLEV_SCREENWORKERFINISHED | SDLEV_VBLANK );
        if ( ( ev & ( SDLEV_BREAK | SDLEV_SCREENWORKERFINISHED ) ) || sdlscr_breakrequested() ) {
            key = -1;
            break;
        }
    }
    sdlscr_pollinput = oldpoll;
    return key;
}

void sdlscr_setbreak( void ) {
    sdlscr_breakreq = true;
    sdlev_raise( SDLEV_BREAK );
}

// the mouse (mouse.h): requests of the program, values of the screen thread
void sdlscr_mouse_show( void )                  { mouse_request_show(); }
void sdlscr_mouse_hide( void )                  { mouse_request_hide(); }
void sdlscr_mouse_pos( int x, int y )           { mouse_request_pos( x, y ); }
void sdlscr_mouse_shape( int shape )            { mouse_request_shape( shape ); }
void sdlscr_mouse_timeout( int ms )             { mouse_request_timeout( ms ); }
int  sdlscr_mouse_get( int what )               { return mouse_get( what ); }

void sdlscr_cursor_show( bool on ) {
    cursors_show( on );
}

void sdlscr_cursor_move( int x, int y ) {
    cursors_move( x, y );
}

bool sdlscr_cursor_shape( int shape ) {
    return cursors_set( shape );
}

int sdlscr_cursor_getshape( void ) {
    return cursors_shape();
}

bool sdlscr_cursor_image( int image, uint8_t out[SPRITE_WIDTH * SPRITE_HEIGHT] ) {
    return sprscr_sys_readmap( image, out );
}

// KEY("name"): the state of the key right now, from SDL's keyboard state (which the event loop of the screen thread keeps; no lock: a read that
// races an update sees the old or the new state). A key that types a character is found by that character, whatever the keyboard layout is.
bool sdlscr_keydown( int keyid ) {
    int n = 0;
    const Uint8* st = SDL_GetKeyboardState( &n );
    if ( st == 0 ) {
        return false;
    }
    #define SDLSCR_DOWN(sc) ( (int)(sc) > 0 && (int)(sc) < n && st[ (int)(sc) ] != 0 )
    if ( keyid == KEYID_ANY ) {
        for ( int i = 1; i < n; ++i ) {
            if ( st[i] != 0 ) {
                return true;
            }
        }
        return false;
    }
    if ( keyid >= 32 && keyid < 127 ) {
        return SDLSCR_DOWN( SDL_GetScancodeFromKey( (SDL_Keycode) keyid ) );
    }
    if ( keyid >= KEYID_KP0 && keyid <= KEYID_KP0 + 9 ) {
        static const int kp[10] = { SDL_SCANCODE_KP_0, SDL_SCANCODE_KP_1, SDL_SCANCODE_KP_2, SDL_SCANCODE_KP_3, SDL_SCANCODE_KP_4,
            SDL_SCANCODE_KP_5, SDL_SCANCODE_KP_6, SDL_SCANCODE_KP_7, SDL_SCANCODE_KP_8, SDL_SCANCODE_KP_9 };
        return SDLSCR_DOWN( kp[ keyid - KEYID_KP0 ] );
    }
    int ks = keystate_ks( keyid );
    if ( ks != KS_NONE ) {
        static const struct { int ks, sc; } map[] = {
            { KS_UP, SDL_SCANCODE_UP }, { KS_DOWN, SDL_SCANCODE_DOWN }, { KS_LEFT, SDL_SCANCODE_LEFT }, { KS_RIGHT, SDL_SCANCODE_RIGHT },
            { KS_HOME, SDL_SCANCODE_HOME }, { KS_END, SDL_SCANCODE_END }, { KS_INSERT, SDL_SCANCODE_INSERT }, { KS_DELETE, SDL_SCANCODE_DELETE },
            { KS_PGUP, SDL_SCANCODE_PAGEUP }, { KS_PGDN, SDL_SCANCODE_PAGEDOWN },
            { KS_F1, SDL_SCANCODE_F1 }, { KS_F2, SDL_SCANCODE_F2 }, { KS_F3, SDL_SCANCODE_F3 }, { KS_F4, SDL_SCANCODE_F4 },
            { KS_F5, SDL_SCANCODE_F5 }, { KS_F6, SDL_SCANCODE_F6 }, { KS_F7, SDL_SCANCODE_F7 }, { KS_F8, SDL_SCANCODE_F8 },
            { KS_F9, SDL_SCANCODE_F9 }, { KS_F10, SDL_SCANCODE_F10 }, { KS_F11, SDL_SCANCODE_F11 }, { KS_F12, SDL_SCANCODE_F12 }
        };
        for ( size_t i = 0; i < sizeof(map) / sizeof(map[0]); ++i ) {
            if ( map[i].ks == ks ) {
                return SDLSCR_DOWN( map[i].sc );
            }
        }
        return false;
    }
    switch ( keyid ) {
        case KEYID_ESC:         return SDLSCR_DOWN( SDL_SCANCODE_ESCAPE );
        case KEYID_RETURN:      return SDLSCR_DOWN( SDL_SCANCODE_RETURN ) || SDLSCR_DOWN( SDL_SCANCODE_KP_ENTER );
        case KEYID_TAB:         return SDLSCR_DOWN( SDL_SCANCODE_TAB );
        case KEYID_BACKSPACE:   return SDLSCR_DOWN( SDL_SCANCODE_BACKSPACE );
        case KEYID_SHIFT:       return SDLSCR_DOWN( SDL_SCANCODE_LSHIFT ) || SDLSCR_DOWN( SDL_SCANCODE_RSHIFT );
        case KEYID_LSHIFT:      return SDLSCR_DOWN( SDL_SCANCODE_LSHIFT );
        case KEYID_RSHIFT:      return SDLSCR_DOWN( SDL_SCANCODE_RSHIFT );
        case KEYID_CTRL:        return SDLSCR_DOWN( SDL_SCANCODE_LCTRL ) || SDLSCR_DOWN( SDL_SCANCODE_RCTRL );
        case KEYID_LCTRL:       return SDLSCR_DOWN( SDL_SCANCODE_LCTRL );
        case KEYID_RCTRL:       return SDLSCR_DOWN( SDL_SCANCODE_RCTRL );
        case KEYID_ALT:         return SDLSCR_DOWN( SDL_SCANCODE_LALT ) || SDLSCR_DOWN( SDL_SCANCODE_RALT );
        case KEYID_LALT:        return SDLSCR_DOWN( SDL_SCANCODE_LALT );
        case KEYID_RALT:        return SDLSCR_DOWN( SDL_SCANCODE_RALT );
        case KEYID_KPENTER:     return SDLSCR_DOWN( SDL_SCANCODE_KP_ENTER );
        case KEYID_KPPLUS:      return SDLSCR_DOWN( SDL_SCANCODE_KP_PLUS );
        case KEYID_KPMINUS:     return SDLSCR_DOWN( SDL_SCANCODE_KP_MINUS );
        case KEYID_KPMUL:       return SDLSCR_DOWN( SDL_SCANCODE_KP_MULTIPLY );
        case KEYID_KPDIV:       return SDLSCR_DOWN( SDL_SCANCODE_KP_DIVIDE );
        case KEYID_KPDOT:       return SDLSCR_DOWN( SDL_SCANCODE_KP_PERIOD );
        default:                return false;
    }
    #undef SDLSCR_DOWN
}

// a line for the prompt, INPUT, AUTO and the pagers: the one-line editor (scr_lineedit(), the same as for EDIT n and in the console host). ESC
// cancels: at INPUT (stop_evtmsk has SDLEV_BREAK) that is Break, elsewhere the line is empty. Returns the events that ended it.
int sdlscr_lineinput( int stop_evtmsk, char* buf, size_t bufsz, void (*vblank_handler)( void* ), void* usrdata ) {
    if ( buf == 0 || bufsz < 2U ) return 0;
    int r = scr_lineedit( sdlscr_driver( vblank_handler, usrdata ), buf, bufsz, 0, 0 );
    switch ( r ) {
        case SCREDIT_CLOSED:
            return SDLEV_SCREENWORKERFINISHED;
        case SCREDIT_BREAK:
            if ( ( stop_evtmsk & SDLEV_BREAK ) != 0 ) {
                sdlscr_setbreak();
                return SDLEV_BREAK;
            }
            buf[0] = '\0';
            return 0;
        case SCREDIT_CANCELLED:
        case SCREDIT_EOF:
            if ( ( stop_evtmsk & SDLEV_BREAK ) != 0 && sdlscr_escbreak ) {
                sdlscr_setbreak();
                return SDLEV_BREAK;
            }
            buf[0] = '\0';         // (after BREAK OFF ESC at INPUT is an empty line)
            return 0;
        case SCREDIT_TOOLONG:
            buf[0] = '\0';
            return 0;
        default:
            return 0;
    }
}

// -- the screen driver of the editors (scrdrv.h) --------------------------------------------------------
//
// The full-screen editor, the monitor and the one-line editor draw and read keys through scrdrv_t, so that the console host runs the same code.

static bool sdlscr_editmode_begin( void ) {
    bool oldpoll = sdlscr_pollinput;
    sdlscr_pollinput = false;       // ESC is a key here, not the break request
    sdlscr_awaitinput = true;
    return oldpoll;
}

static void sdlscr_editmode_end( bool oldpoll ) {
    sdlscr_awaitinput = false;
    sdlscr_pollinput = oldpoll;
}

void sdlscr_textchanged( void ) {
    sdllay_set_modified( &layers[LAY_TXT] );
}

static void (*sdlscr_drv_vblank)( void* ) = 0;
static void* sdlscr_drv_usrdata = 0;

static bool sdlscr_inputpending( void ) {
    SDL_LockMutex( sdlscr_inputmtx );
    bool pending = sdlscr_iqr != sdlscr_iqw;
    SDL_UnlockMutex( sdlscr_inputmtx );
    return pending;
}

// the key an input message stands for; false for keys the editors don't use
static bool sdlscr_msgtoscrkey( const sdlscr_inputmsg_t* msg, scrkey_t* k ) {
    memset( k, 0, sizeof(*k) );
    if ( msg->inputtype == SDLSCR_INPUT_MOUSE ) {
        k->kind = SK_MOUSE; k->mouse = msg->mouse; k->mx = msg->mx; k->my = msg->my; k->button = msg->button; k->wheel = msg->wheel;
        return true;
    }
    if ( msg->inputtype == SDLSCR_INPUT_TEXT ) {
        int chr = msg->text[0] & 255;
        if ( msg->text[0] != '\0' && msg->text[1] == '\0' && SDLSCR_PRINTABLE( chr ) ) {
            k->kind = SK_CHAR; k->ch = chr;
            return true;
        }
        return false;
    }
    if ( msg->inputtype != SDLSCR_INPUT_KEYPRESS ) {
        return false;
    }
    int key, mods;
    if ( msg->symbol == SDLK_PAUSE || msg->symbol == SDLK_CANCEL || ( msg->symbol == SDLK_c && ( msg->mods & KMOD_CTRL ) != 0 ) ) {
        k->kind = SK_BREAK;
        return true;
    }
    switch ( msg->symbol ) {
        case SDLK_RETURN:
        case SDLK_KP_ENTER:     k->kind = SK_RETURN;    return true;
        case SDLK_ESCAPE:       k->kind = SK_ESCAPE;    return true;
        case SDLK_BACKSPACE:    k->kind = SK_BACKSPACE; return true;
        default:
            break;
    }
    if ( sdlkeys_map( msg->symbol, msg->mods, &key, &mods ) ) {
        k->kind = SK_KEY; k->key = key; k->mods = mods;
        return true;
    }
    if ( msg->symbol == SDLK_TAB ) {
        k->kind = SK_TAB;
        return true;
    }
    if ( ( msg->mods & KMOD_CTRL ) != 0 && msg->symbol >= 'a' && msg->symbol <= 'z' ) {
        k->kind = SK_CHAR; k->ch = (int) msg->symbol; k->mods = KSMOD_CTRL;
        return true;
    }
    return false;
}

static bool sdlscr_drv_nextkey( scrkey_t* k ) {
    for (;;) {
        sdlscr_inputmsg_t* msg;
        while ( ( msg = sdlscr_dequeueinputmsg() ) != 0 ) {
            bool got = sdlscr_msgtoscrkey( msg, k );
            sdlscr_deleteinputmsg( msg );
            if ( got ) {
                return true;
            }
        }
        int ev = sdlev_wait( SDLEV_VBLANK | SDLEV_SCREENWORKERFINISHED | SDLEV_INPUT );
        if ( ev & SDLEV_SCREENWORKERFINISHED ) {
            memset( k, 0, sizeof(*k) );
            k->kind = SK_CLOSED;
            return false;
        }
        if ( ( ev & SDLEV_VBLANK ) != 0 && sdlscr_drv_vblank != 0 ) {
            sdlscr_drv_vblank( sdlscr_drv_usrdata );
        }
    }
}

static void sdlscr_drv_wantmouse( bool enable ) {
    sdlscr_wantmouse = enable;
    if ( !enable ) {
        sdlscr_lbdown = false;
    }                                   // (mouse events that are still queued afterwards are ignored by every reader)
}

static void sdlscr_drv_screensize( int* cols, int* rows ) { txtscr_getscreensize( cols, rows ); }
static void sdlscr_drv_getsize( int* cols, int* rows ) { txtscr_getsize( cols, rows ); }
static void sdlscr_drv_print( int y, int x, int bg, int fg, const char* text ) { sdlscr_printf( y, x, bg, fg, "%s", text ); }
static int sdlscr_drv_begin( void ) { return sdlscr_editmode_begin() ? 1 : 0; }
static void sdlscr_drv_end( int state ) { sdlscr_editmode_end( state != 0 ); }
static int sdlscr_drv_restart( int absy, int startx, size_t pos ) { (void) startx; (void) pos; return absy; }
static void sdlscr_drv_reserve( int rows ) { (void) rows; }
static void sdlscr_drv_getviewport( int* x, int* y, int* w, int* h ) { sdlscr_text_getviewport( x, y, w, h ); }
static void sdlscr_drv_getcursor( int* x, int* y ) { sdlscr_text_getcursor( x, y ); }

static void sdlscr_drv_palette( uint32_t pal[256], uint32_t* backdrop ) {
    memcpy( pal, layers[LAY_TXT].palette, 256 * sizeof(uint32_t) );
    *backdrop = sdlscr_bgcol;
}

static const scrdrv_t sdlscr_drv = {
    sdlscr_drv_screensize, sdlscr_drv_getsize,
    sdlscr_text_select, sdlscr_text_selected, sdlscr_text_setviewport, sdlscr_text_resetviewport, sdlscr_drv_getviewport,
    sdlscr_text_setcolors, sdlscr_text_setblank, sdlscr_text_clear, sdlscr_text_write, sdlscr_drv_print, sdlscr_locate, sdlscr_drv_getcursor,
    sdlscr_textchanged, sdlscr_drv_reserve, sdlscr_drv_restart,
    sdlscr_drv_begin, sdlscr_drv_end, sdlscr_drv_nextkey, sdlscr_inputpending, sdlscr_pollkeys,
    sdlscr_drv_palette, sdlscr_drv_wantmouse
};

const scrdrv_t* sdlscr_driver( void (*vblank_handler)( void* ), void* usrdata ) {
    sdlscr_drv_vblank = vblank_handler;
    sdlscr_drv_usrdata = usrdata;
    return &sdlscr_drv;
}

// the line of the prompt: the screen editor (scr_screenedit()) of text screen 0. Same results as sdlscr_lineinput(); the typing mode
// (insert / overwrite) is kept between calls.
int sdlscr_screeninput( char* buf, size_t bufsz, void (*vblank_handler)( void* ), void* usrdata ) {
    static bool insert = false;
    if ( buf == 0 || bufsz < 2U ) return 0;
    int r = scr_screenedit( sdlscr_driver( vblank_handler, usrdata ), buf, bufsz, &insert, 0, 0 );
    switch ( r ) {
        case SCREDIT_CLOSED:
            return SDLEV_SCREENWORKERFINISHED;
        case SCREDIT_CANCELLED:     // (the screen is too small)
            return sdlscr_lineinput( 0, buf, bufsz, vblank_handler, usrdata );
        case SCREDIT_ACCEPTED:
            return 0;
        default:                    // Break, end of input: no line
            buf[0] = '\0';
            return 0;
    }
}

int sdlscr_lineedit( char* buf, size_t bufsz, void (*vblank_handler)( void* ), void* usrdata ) {
    return scr_lineedit( sdlscr_driver( vblank_handler, usrdata ), buf, bufsz, 0, 0 );
}

void sdlscr_gettextscreensize( int* outsx, int* outsy ) {
    txtscr_getsize( outsx, outsy );
}

void sdlscr_gettextcursor( int* outx, int* outy ) {
    txtscr_getcursor( outx, outy );
}

void sdlscr_locate( int x, int y ) {
    txtscr_locate( x, y );
}

void sdlscr_writetile( int tileno, const uint8_t data[TILE_WIDTH * TILE_HEIGHT]) {
    tilescr_writetile( tileno, data );
    sdllay_set_modified( &layers[LAY_TIL] );
}

bool sdlscr_term( void ) {
    return !sdlscr_initok;
}

void sdlscr_scrolltiles( int sx, int sy ) {
    tilescr_scroll( sx, sy );
    sdllay_set_modified( &layers[LAY_TIL] );
}

void sdlscr_showsprite( int sprno, bool active ) {
    sprscr_show( sprno, active );
}

void sdlscr_spriteprio( int sprno, int prio ) {
    sprscr_prio( sprno, prio );
}

void sdlscr_movesprite( int sprno, int x, int y ) {
    sprscr_move( sprno, x, y );
}

void sdlscr_spriteanimdata( int animno, const uint8_t* seq, size_t size ) {
    sprscr_animdata( animno, seq, size );
}

void sdlscr_spriteanimcfg( int sprno, int animno, int length, int speed ) {
    sprscr_animcfg( sprno, animno, length, speed );
}

void sdlscr_writesprite( int sprno, const uint8_t data[SPRITE_WIDTH * SPRITE_HEIGHT]) {
    sprscr_writemap( sprno, data );
}

// -- the internal API (see sdlscreen.h) ------------------------------------------------------

static bool sdlscr_goodlayer( int layer ) {
    return layer >= 0 && layer < NLAYERS;
}

void sdlscr_layer_enable( int layer, bool enable ) {
    if ( !sdlscr_goodlayer( layer ) ) {
        return;
    }
    if ( enable ) {
        sdllay_enable( &layers[layer] );
    } else {
        sdllay_disable( &layers[layer] );
    }
    sdllay_set_modified( &layers[layer] );
    sdllay_set_modified( &layers[LAY_BG] );     // (something has to make the screen redraw when a layer vanishes)
}

bool sdlscr_layer_enabled( int layer ) {
    return sdlscr_goodlayer( layer ) && sdllay_enabled( &layers[layer] );
}

void sdlscr_layer_setpriority( int layer, int prio ) {
    if ( !sdlscr_goodlayer( layer ) || prio < 0 || prio >= NLAYERS ) {
        return;
    }
    for ( int i=0; i < NLAYERS; ++i ) {
        if ( layers[i].priority == (uint8_t) prio ) {
            sdllay_switch_priority( &layers[0], NLAYERS, (uint8_t) layer, (uint8_t) i );
            break;
        }
    }
    sdllay_set_modified( &layers[LAY_BG] );
}

int sdlscr_layer_getpriority( int layer ) {
    return sdlscr_goodlayer( layer ) ? (int) layers[layer].priority : -1;
}

void sdlscr_palette_set( int layer, int index, uint32_t argb ) {
    if ( !sdlscr_goodlayer( layer ) || index < 0 || index > 255 ) {
        return;
    }
    layers[layer].palette[index] = argb;
    sdllay_set_modified( &layers[layer] );
}

uint32_t sdlscr_palette_get( int layer, int index ) {
    if ( !sdlscr_goodlayer( layer ) || index < 0 || index > 255 ) {
        return 0;
    }
    return layers[layer].palette[index];
}

void sdlscr_palette_reset( int layer ) {
    for ( int i=0; i < NLAYERS; ++i ) {
        if ( layer < 0 || layer == i ) {
            sdllay_reset_palette( &layers[i] );
        }
    }
}

bool sdlscr_palette_setbyname( int layer, int index, const char* name ) {
    uint32_t argb = 0;
    if ( !sdlscr_goodlayer( layer ) || index < 0 || index > 255 || !sdllay_findcolor( name, &argb ) ) {
        return false;
    }
    sdlscr_palette_set( layer, index, argb );
    return true;
}

void sdlscr_setbackdrop( uint32_t argb ) {
    sdlscr_bgcol = argb;
    sdllay_set_modified( &layers[LAY_BG] );
}

// the drawing contexts of the two bitmap layers
static gfx_t sdlscr_gfxctx[2];
static bool  sdlscr_gfxok = false;
static int   sdlscr_gfxtgt = LAY_GFX;
static int   sdlscr_gfxpen = 1, sdlscr_gfxmode = GFXM_SET;

static gfx_t* sdlscr_gfxof( int layer ) {
    if ( layer != LAY_BG && layer != LAY_GFX ) {
        return 0;
    }
    if ( !sdlscr_gfxok ) {
        if ( layers[LAY_BG].memory == 0 || layers[LAY_GFX].memory == 0 ) {
            return 0;
        }
        gfx_init( &sdlscr_gfxctx[0], layers[LAY_BG].memory, SDL_SCREENWIDTH, SDL_SCREENHEIGHT );
        gfx_init( &sdlscr_gfxctx[1], layers[LAY_GFX].memory, SDL_SCREENWIDTH, SDL_SCREENHEIGHT );
        sdlscr_gfxok = true;
    }
    gfx_t* g = &sdlscr_gfxctx[ layer == LAY_BG ? 0 : 1 ];
    g->pen = sdlscr_gfxpen;
    g->mode = sdlscr_gfxmode;
    return g;
}

void sdlscr_gfx_settarget( int layer ) {
    if ( layer == LAY_BG || layer == LAY_GFX ) {
        sdlscr_gfxtgt = layer;
    }
}

int sdlscr_gfx_target( void ) {
    return sdlscr_gfxtgt;
}

void sdlscr_gfx_setpen( int color ) {
    if ( color >= 0 && color <= 255 ) {
        sdlscr_gfxpen = color;
    }
}

void sdlscr_gfx_setmode( int mode ) {
    if ( mode >= GFXM_SET && mode <= GFXM_AND ) {
        sdlscr_gfxmode = mode;
    }
}

#define SDLSCR_GFX(layer) gfx_t* g = sdlscr_gfxof( layer ); if ( g == 0 ) return

void sdlscr_gfx_clear( int layer, int color ) {
    SDLSCR_GFX( layer );
    gfx_clear( g, color );
    sdllay_set_modified( &layers[layer] );
}

void sdlscr_gfx_pset( int layer, int x, int y, int color ) {
    SDLSCR_GFX( layer );
    gfx_pset( g, x, y, color );
    sdllay_set_modified( &layers[layer] );
}

int sdlscr_gfx_point( int layer, int x, int y ) {
    gfx_t* g = sdlscr_gfxof( layer );
    return g ? gfx_point( g, x, y ) : -1;
}

void sdlscr_gfx_line( int layer, int x1, int y1, int x2, int y2, int color ) {
    SDLSCR_GFX( layer );
    gfx_line( g, x1, y1, x2, y2, color );
    sdllay_set_modified( &layers[layer] );
}

void sdlscr_gfx_box( int layer, int x1, int y1, int x2, int y2, int color, bool fill ) {
    SDLSCR_GFX( layer );
    gfx_box( g, x1, y1, x2, y2, color, fill );
    sdllay_set_modified( &layers[layer] );
}

void sdlscr_gfx_circle( int layer, int x, int y, int r, int color, int a1, int a2, double ratio ) {
    SDLSCR_GFX( layer );
    gfx_circle( g, x, y, r, color, a1, a2, ratio );
    sdllay_set_modified( &layers[layer] );
}

void sdlscr_gfx_radius( int layer, int x, int y, int r, double angle, int color ) {
    SDLSCR_GFX( layer );
    gfx_radius( g, x, y, r, angle, color );
    sdllay_set_modified( &layers[layer] );
}

void sdlscr_gfx_paint( int layer, int x, int y, int color, int border ) {
    SDLSCR_GFX( layer );
    gfx_paint( g, x, y, color, border );
    sdllay_set_modified( &layers[layer] );
}

size_t sdlscr_gfx_grab( int layer, int x1, int y1, int x2, int y2, uint8_t* out, size_t outsize ) {
    gfx_t* g = sdlscr_gfxof( layer );
    return g ? gfx_grab( g, x1, y1, x2, y2, out, outsize ) : 0;
}

bool sdlscr_gfx_blit( int layer, const uint8_t* block, size_t size, int x, int y, bool transparent ) {
    gfx_t* g = sdlscr_gfxof( layer );
    if ( g == 0 ) {
        return false;
    }
    bool ok = gfx_blit( g, block, size, x, y, transparent );
    sdllay_set_modified( &layers[layer] );
    return ok;
}

void sdlscr_gfx_text( int layer, int x, int y, const char* text, size_t len, int color, int sx, int sy ) {
    SDLSCR_GFX( layer );
    gfx_text( g, x, y, text, len, color, sx, sy, txtscr_font() );
    sdllay_set_modified( &layers[layer] );
}

bool sdlscr_gfx_draw( int layer, const char* cmd, size_t len, char* err, size_t errsz ) {
    gfx_t* g = sdlscr_gfxof( layer );
    if ( g == 0 ) {
        return false;
    }
    bool ok = gfx_draw( g, cmd, len, err, errsz );
    sdllay_set_modified( &layers[layer] );
    return ok;
}

// text screen

int sdlscr_text_select( int screen ) {
    int old = txtscr_selected();
    if ( !txtscr_select( screen ) ) {
        return -1;
    }
    sdllay_set_modified( &layers[LAY_TXT] );
    return old;
}

int sdlscr_text_selected( void ) {
    return txtscr_selected();
}

bool sdlscr_text_setviewport( int x, int y, int w, int h ) {
    return txtscr_setviewport( x, y, w, h );
}

void sdlscr_text_resetviewport( void ) {
    txtscr_resetviewport();
}

void sdlscr_text_getviewport( int* x, int* y, int* w, int* h ) {
    txtscr_getviewport( x, y, w, h );
}

void sdlscr_text_setblank( int bg, int fg ) {
    txtscr_setblank( bg, fg );
}

void sdlscr_text_setcolors( int bg, int fg ) {
    txtscr_setcolors( bg, fg );
    // PRINT goes through the ANSI decoder of the screen, which holds its own current colors: it starts again from the new ones
    sdlscr_ansiok[ txtscr_selected() ] = false;
}

void sdlscr_text_getcolors( int* bg, int* fg ) {
    txtscr_getcolors( bg, fg );
}

int sdlscr_text_write( int y, int x, int bg, int fg, const char* text, int len ) {
    int n = txtscr_write( y, x, bg, fg, text, len );
    sdllay_set_modified( &layers[LAY_TXT] );
    return n;
}

uint32_t sdlscr_text_getcell( int y, int x ) {
    return txtscr_getcell( y, x );
}

void sdlscr_text_getcursor( int* x, int* y ) {
    txtscr_getcursor( x, y );
}

void sdlscr_text_clear( void ) {
    txtscr_clear();
    sdllay_set_modified( &layers[LAY_TXT] );
}

void sdlscr_text_fill( int x1, int y1, int x2, int y2, int ch, int fg, int bg ) {
    txtscr_fill( x1, y1, x2, y2, ch, fg, bg );
    sdllay_set_modified( &layers[LAY_TXT] );
}

void sdlscr_text_copy( int x1, int y1, int x2, int y2, int tx, int ty ) {
    txtscr_copy( x1, y1, x2, y2, tx, ty );
    sdllay_set_modified( &layers[LAY_TXT] );
}

void sdlscr_text_invert( int x1, int y1, int x2, int y2 ) {
    txtscr_invert( x1, y1, x2, y2 );
    sdllay_set_modified( &layers[LAY_TXT] );
}

size_t sdlscr_text_save( uint8_t* out, size_t outsize ) {
    return txtscr_save( out, outsize );
}

bool sdlscr_text_restore( const uint8_t* data, size_t size ) {
    bool ok = txtscr_restore( data, size );
    sdllay_set_modified( &layers[LAY_TXT] );
    return ok;
}

void sdlscr_text_defchar( int code, const uint8_t rows[12] ) {
    txtscr_defchar( code, rows );
    sdllay_set_modified( &layers[LAY_TXT] );
}

// tiles

void sdlscr_tile_setcell( int x, int y, int tile ) {
    tilescr_setcell( x, y, tile );
    sdllay_set_modified( &layers[LAY_TIL] );
}

int sdlscr_tile_getcell( int x, int y ) {
    return tilescr_getcell( x, y );
}

void sdlscr_tile_setcells( const uint8_t* cells, size_t count ) {
    tilescr_setcells( cells, count );
    sdllay_set_modified( &layers[LAY_TIL] );
}

bool sdlscr_tile_getimage( int tile, uint8_t out[TILE_WIDTH * TILE_HEIGHT] ) {
    return tilescr_readtile( tile, out );
}

// sprites

void sdlscr_sprite_setimage( int sprno, int image ) {
    sprscr_setimage( sprno, image );
    sdllay_set_modified( &layers[LAY_SPR] );
}

int sdlscr_sprite_getimage( int sprno ) {
    return sprscr_getimage( sprno );
}

bool sdlscr_sprite_getpos( int sprno, int* x, int* y ) {
    return sprscr_getpos( sprno, x, y );
}

bool sdlscr_sprite_active( int sprno ) {
    return sprscr_isactive( sprno );
}

bool sdlscr_sprite_getmap( int image, uint8_t out[SPRITE_WIDTH * SPRITE_HEIGHT] ) {
    return sprscr_readmap( image, out );
}

void sdlscr_sprite_moveto( int sprno, int x, int y, int ticks ) {
    sprscr_moveto( sprno, x, y, ticks );
    sdllay_set_modified( &layers[LAY_SPR] );
}

bool sdlscr_sprite_moving( int sprno ) {
    return sprscr_moving( sprno );
}

void sdlscr_sprite_setcollide( int sprno, bool sprites, bool tiles ) {
    sprscr_setcollide( sprno, sprites, tiles );
}

uint32_t sdlscr_sprite_hits( int sprno ) {
    return sprscr_hits( sprno );
}

int sdlscr_sprite_collide( int sprno ) {
    return sprscr_collidewith( sprno );
}

int sdlscr_sprite_nextevent( void ) {
    return sprscr_nextevent();
}

// back to the defaults

void sdlscr_gfx_reset( bool images ) {
    for ( int i=0; i < NLAYERS; ++i ) {
        sdllay_enable( &layers[i] );
        layers[i].priority = (uint8_t) i;
        sdllay_reset_palette( &layers[i] );
    }
    sdlscr_bgcol = SDLSCR_DEFAULT_BGCOL;
    sdlscr_gfxtgt = LAY_GFX;
    sdlscr_gfxpen = 1;
    sdlscr_gfxmode = GFXM_SET;
    if ( layers[LAY_BG].memory != 0 && layers[LAY_GFX].memory != 0 ) {
        memset( layers[LAY_BG].memory, 0, (size_t) SDL_SCREENWIDTH * SDL_SCREENHEIGHT );
        memset( layers[LAY_GFX].memory, 0, (size_t) SDL_SCREENWIDTH * SDL_SCREENHEIGHT );
    }
    sdlscr_gfxok = false;           // the contexts (the DRAW position, color and scale) start again
    txtscr_resetcolors();           // the pen and paper of the text screen (what COLOR and ANSI sequences set), not its contents
    for ( int i=0; i < TXTSCR_NSCREENS; ++i ) {
        sdlscr_ansiok[i] = false;   // the ANSI decoders start from the default colors again
    }
    tilescr_reset( images );
    sprscr_reset( images );
    if ( images ) {
        txtscr_resetfont();
    }
    for ( int i=0; i < NLAYERS; ++i ) {
        sdllay_set_modified( &layers[i] );
    }
}

// timing

void sdlscr_vsync( void ) {
    uint64_t f = sdlscr_framecnt;
    struct timespec nap = { 0, 1000000L };
    while ( sdlscr_framecnt == f && !sdlscr_doexit && sdlscr_initok ) {
        nanosleep( &nap, 0 );
    }
}
