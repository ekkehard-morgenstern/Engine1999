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
#include "sdllayer.h"
#include "sdlevent.h"
#include "textscreen.h"
#include "tilescreen.h"
#include "sprscreen.h"
#include "sdlutil.h"

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

static uint32_t sdlscr_bgcol = UINT32_C(0XFF708090); // SlateGray

static SDL_Thread* sdlscr_workerthr = 0;

static uint64_t sdlscr_framecnt = 0;
static bool sdlscr_awaitinput = false;
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

static void sdlscr_deleteinputmsg( sdlscr_inputmsg_t* msg ) {
    SDL_free( msg );
}

static bool sdlscr_enqueuekeypress( SDL_Keycode key ) {
    sdlscr_inputmsg_t* msg = sdlscr_createinputmsg( SDLSCR_INPUT_KEYPRESS );
    if ( msg == 0 ) return false;
    msg->symbol = key;
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
    if ( !sdlscr_awaitinput ) return;
    if ( sdlscr_enqueuekeypress( key ) ) {
        sdlev_raise( SDLEV_INPUT );
    }
}

static void sdlscr_handletext( const char text[SDL_TEXTINPUTEVENT_TEXT_SIZE] ) {
    if ( !sdlscr_awaitinput ) return;
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

    struct timespec lts;
    uint64_t lastTick = sdlutil_getnsec( &lts );

    bool oldawaitinput = false;

    // main loop
    for (;;) {
        if ( sdlscr_doexit ) {
            break;
        }

        ++sdlscr_framecnt;

        if ( sdlscr_awaitinput != oldawaitinput ) {
            if ( sdlscr_awaitinput ) {
                SDL_StartTextInput();
            } else {
                SDL_StopTextInput();
            }
            oldawaitinput = sdlscr_awaitinput;
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

        // get current time
        struct timespec ts;
        uint64_t now  = sdlutil_getnsec( &ts );
        uint64_t nsec = now - lastTick;
        if ( nsec < UINT64_C(16666667) ) {
            // make sure at least a 1/60th sec will have passed
            uint64_t toWait = UINT64_C(16666667);
            sdlutil_nanosleep( toWait, &lts );
        }
        lastTick = now; lts = ts;
    }

    if ( oldawaitinput ) {
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
