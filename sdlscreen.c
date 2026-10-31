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

    NOTE: Programs created with a built-in programming language (if any),
          do not fall under this license.

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

static void sdlscr_handlekey( SDL_KeyCode key ) {
    if ( !sdlscr_awaitinput && !sdlscr_pollinput ) return;
    if ( key == SDLK_ESCAPE && sdlscr_pollinput ) {
        sdlscr_breakreq = true;
        sdlev_raise( SDLEV_BREAK );
        return;
    }
    if ( sdlscr_enqueuekeypress( key, (Uint16) SDL_GetModState() ) ) {
        sdlev_raise( SDLEV_INPUT );
    }
}

static void sdlscr_handletext( const char text[SDL_TEXTINPUTEVENT_TEXT_SIZE] ) {
    if ( !sdlscr_awaitinput && !sdlscr_pollinput ) return;
    if ( sdlscr_enqueuetext( text ) ) {
        sdlev_raise( SDLEV_INPUT );
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

    SDL_Window* window = SDL_CreateWindow(
        "",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        0,
        0,
        SDL_WINDOW_FULLSCREEN_DESKTOP
    );
    if ( window == 0 ) {
        print_sdlerror( "create window" );
        goto ERR2;
    }

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

    // main loop
    for (;;) {
        if ( sdlscr_doexit ) {
            break;
        }

        ++sdlscr_framecnt;

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
        SDL_Event ev;
        while ( SDL_PollEvent( &ev ) ) {
            Uint32 ty = ev.type;
            switch ( ty ) {
                case SDL_QUIT:
                    sdlscr_doexit = true;
                    break;
                case SDL_KEYDOWN:
                    sdlscr_handlekey( ev.key.keysym.sym );
                    break;
                case SDL_TEXTINPUT:
                    sdlscr_handletext( ev.text.text );
                    break;
            }
        }

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
        }

        sdlev_raise( SDLEV_VBLANK );
        SDL_RenderPresent( renderer );

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
    }

    if ( oldwantinput ) {
        SDL_StopTextInput();
    }

    // cleanup
    sdlscr_initok = false;

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

static void sdlscr_flushinput( void ) {
    sdlscr_inputmsg_t* msg;
    while ( ( msg = sdlscr_dequeueinputmsg() ) != 0 ) {
        sdlscr_deleteinputmsg( msg );
    }
}

void sdlscr_pollkeys( bool enable ) {
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

// the key code of an input message, 0 if it isn't a key we deliver
static int sdlscr_msgtokey( const sdlscr_inputmsg_t* msg ) {
    if ( msg->inputtype == SDLSCR_INPUT_KEYPRESS ) {
        switch ( msg->symbol ) {
            case SDLK_RETURN:       return 13;
            case SDLK_BACKSPACE:    return 8;
            case SDLK_TAB:          return 9;
            default:                return 0;
        }
    }
    if ( msg->inputtype == SDLSCR_INPUT_TEXT && msg->text[0] != '\0' && msg->text[1] == '\0' ) {
        int chr = msg->text[0] & 255;
        if ( chr >= 32 && chr < 127 ) {
            return chr;
        }
    }
    return 0;       // other keys and Unicode text: ignored for now
}

int sdlscr_pollkey( void ) {
    sdlscr_inputmsg_t* msg;
    while ( ( msg = sdlscr_dequeueinputmsg() ) != 0 ) {
        int key = sdlscr_msgtokey( msg );
        sdlscr_deleteinputmsg( msg );
        if ( key ) {
            return key;
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

int sdlscr_lineinput( int stop_evtmsk, char* buf, size_t bufsz, void (*vblank_handler)( void* ), void* usrdata ) {
    if ( buf == 0 || bufsz < 2U ) return 0;
    sdlscr_awaitinput = true;
    int outmsk = 0;
    int bufpos = 0;
    int remain = bufsz - 1;
    for (;;) {
        int ev = sdlev_wait( stop_evtmsk | SDLEV_VBLANK | SDLEV_SCREENWORKERFINISHED | SDLEV_INPUT );
        if ( ev & SDLEV_SCREENWORKERFINISHED ) {
            outmsk |= SDLEV_SCREENWORKERFINISHED;
            break;
        }
        if ( ev & stop_evtmsk ) {
            outmsk |= ev & stop_evtmsk;
            break;
        }
        if ( ev & SDLEV_VBLANK ) {
            if ( vblank_handler ) {
                vblank_handler( usrdata );
            }
        }
        if ( !( ev & SDLEV_INPUT ) ) {
            continue;
        }
        bool done = false;
        for (;;) {
            sdlscr_inputmsg_t* msg = sdlscr_dequeueinputmsg();
            if ( msg == 0 ) break;
            switch ( msg->inputtype ) {
                case SDLSCR_INPUT_KEYPRESS:
                    switch ( msg->symbol ) {
                        case SDLK_BACKSPACE:
                            if ( bufpos > 0 ) {
                                txtscr_rubout();
                                --bufpos; ++remain;
                                sdllay_set_modified( &layers[LAY_TXT] );
                            }
                            break;
                        case SDLK_RETURN:
                            done = true;
                            break;
                    }
                    break;
                case SDLSCR_INPUT_TEXT:
                    {
                        size_t len = strlen( msg->text );
                        if ( len > 1U ) {
                            // Unicode: Ignore for now
                            break;
                        }
                        int chr = msg->text[0] & 255;
                        if ( chr < 32 || chr > 127 ) {
                            // Non-printable character: Ignore for now
                            break;
                        }
                        if ( remain < 1 ) {
                            // no room in buffer: stop
                            break;
                        }
                        buf[ bufpos++ ] = (char) chr; --remain;
                        txtscr_print( -1, -1, -1, -1, msg->text );
                        sdllay_set_modified( &layers[LAY_TXT] );
                    }
                    break;
            }
            sdlscr_deleteinputmsg( msg );
        }
        if ( done ) {
            buf[ bufpos ] = '\0';
            txtscr_print( -1, -1, -1, -1, "\n" );
            sdllay_set_modified( &layers[LAY_TXT] );
            break;
        }
    }
    sdlscr_awaitinput = false;
    return outmsk;
}

// -- line editor -----------------------------------------------------------------------

// the editing keys shared by the one-line editor and the full-screen editor
bool sdlscr_editkey( sdlscr_edit_t* ed, const sdlscr_inputmsg_t* msg ) {
    char* work = ed->buf;
    if ( msg->inputtype == SDLSCR_INPUT_KEYPRESS ) {
        switch ( msg->symbol ) {
            case SDLK_LEFT:
                if ( ed->pos > 0 ) { --ed->pos; return true; }
                break;
            case SDLK_RIGHT:
                if ( ed->pos < ed->len ) { ++ed->pos; return true; }
                break;
            case SDLK_HOME:
                if ( ed->pos != 0 ) { ed->pos = 0; return true; }
                break;
            case SDLK_END:
                if ( ed->pos != ed->len ) { ed->pos = ed->len; return true; }
                break;
            case SDLK_BACKSPACE:
                if ( ed->pos > 0 ) {
                    memmove( work + ed->pos - 1U, work + ed->pos, ed->len - ed->pos + 1U );
                    --ed->pos; --ed->len;
                    return true;
                }
                break;
            case SDLK_DELETE:
                if ( ed->pos < ed->len ) {
                    memmove( work + ed->pos, work + ed->pos + 1U, ed->len - ed->pos );
                    --ed->len;
                    return true;
                }
                break;
        }
    } else if ( msg->inputtype == SDLSCR_INPUT_TEXT ) {
        // one printable ASCII character; Unicode and control characters are ignored for now
        int chr = msg->text[0] & 255;
        if ( msg->text[0] != '\0' && msg->text[1] == '\0' && chr >= 32 && chr < 127 && ed->len < ed->maxlen ) {
            memmove( work + ed->pos + 1U, work + ed->pos, ed->len - ed->pos + 1U );
            work[ ed->pos++ ] = (char) chr; ++ed->len;
            return true;
        }
    }
    return false;
}

bool sdlscr_editmode_begin( void ) {
    bool oldpoll = sdlscr_pollinput;
    sdlscr_pollinput = false;       // ESC is a key here, not the break request
    sdlscr_awaitinput = true;
    return oldpoll;
}

void sdlscr_editmode_end( bool oldpoll ) {
    sdlscr_awaitinput = false;
    sdlscr_pollinput = oldpoll;
}

void sdlscr_textchanged( void ) {
    sdllay_set_modified( &layers[LAY_TXT] );
}

// draws the text of the editor from (startx, starty) in the viewport, wipes what a shorter text leaves behind, puts the
// cursor at pos and returns the row the text starts at now (it moves up when printing scrolled the screen)
static int sdlscr_editdraw( const char* text, size_t len, size_t pos, int startx, int starty ) {
    int sx = 0, sy = 0;
    txtscr_getsize( &sx, &sy );
    txtscr_print( starty, startx, -1, -1, text );
    int ey = starty + (int)( ( (size_t) startx + len ) / (size_t) sx );
    if ( ey > sy - 1 ) {
        starty -= ey - ( sy - 1 );      // printing scrolled the screen
    }
    int cx = 0, cy = 0;
    txtscr_getcursor( &cx, &cy );
    txtscr_write( cy, cx, -1, -1, " ", 1 );
    int row = starty + (int)( ( (size_t) startx + pos ) / (size_t) sx );
    int col = (int)( ( (size_t) startx + pos ) % (size_t) sx );
    txtscr_locate( col, row );
    sdllay_set_modified( &layers[LAY_TXT] );
    return starty;
}

int sdlscr_lineedit( char* buf, size_t bufsz, void (*vblank_handler)( void* ), void* usrdata ) {
    if ( buf == 0 || bufsz < 2U ) {
        return SDLSCR_EDIT_TOOLONG;
    }
    int sx = 0, sy = 0;
    txtscr_getsize( &sx, &sy );
    int startx = 0, starty = 0;
    txtscr_getcursor( &startx, &starty );
    // the line must leave at least one row free, so that scrolling can't push its start off the screen
    size_t maxlen = (size_t) sx * (size_t)( sy - 1 ) - (size_t) startx;
    if ( maxlen > bufsz - 1U ) {
        maxlen = bufsz - 1U;
    }
    size_t len = strlen( buf );
    if ( len > maxlen ) {
        return SDLSCR_EDIT_TOOLONG;
    }
    char* work = (char*) malloc( bufsz );
    if ( work == 0 ) {
        return SDLSCR_EDIT_TOOLONG;
    }
    memcpy( work, buf, len + 1U );
    sdlscr_edit_t ed = { work, len, len, maxlen };
    int result = SDLSCR_EDIT_CLOSED;
    bool oldpoll = sdlscr_editmode_begin();
    starty = sdlscr_editdraw( work, ed.len, ed.pos, startx, starty );
    bool finished = false;
    while ( !finished ) {
        int ev = sdlev_wait( SDLEV_VBLANK | SDLEV_SCREENWORKERFINISHED | SDLEV_INPUT );
        if ( ev & SDLEV_SCREENWORKERFINISHED ) {
            break;
        }
        if ( ev & SDLEV_VBLANK ) {
            if ( vblank_handler ) {
                vblank_handler( usrdata );
            }
        }
        if ( !( ev & SDLEV_INPUT ) ) {
            continue;
        }
        bool changed = false;
        sdlscr_inputmsg_t* msg;
        while ( !finished && ( msg = sdlscr_dequeueinputmsg() ) != 0 ) {
            if ( msg->inputtype == SDLSCR_INPUT_KEYPRESS && ( msg->symbol == SDLK_RETURN || msg->symbol == SDLK_KP_ENTER ) ) {
                result = SDLSCR_EDIT_ACCEPTED; finished = true;
            } else if ( msg->inputtype == SDLSCR_INPUT_KEYPRESS && msg->symbol == SDLK_ESCAPE ) {
                result = SDLSCR_EDIT_CANCELLED; finished = true;
            } else if ( sdlscr_editkey( &ed, msg ) ) {
                changed = true;
            }
            sdlscr_deleteinputmsg( msg );
        }
        if ( changed || finished ) {
            starty = sdlscr_editdraw( work, ed.len, finished ? ed.len : ed.pos, startx, starty );
        }
    }
    if ( result == SDLSCR_EDIT_ACCEPTED ) {
        memcpy( buf, work, ed.len + 1U );
    }
    if ( result != SDLSCR_EDIT_CLOSED ) {
        txtscr_print( -1, -1, -1, -1, "\n" );      // the cursor is at the end of the text now
        sdllay_set_modified( &layers[LAY_TXT] );
    }
    sdlscr_editmode_end( oldpoll );
    free( work );
    return result;
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
