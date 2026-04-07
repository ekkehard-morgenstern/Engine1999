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
}

static bool sdlscr_initok = false;
static bool sdlscr_doexit = false;

static uint32_t sdlscr_bgcol = UINT32_C(0XFF708090); // SlateGray

static SDL_Thread* sdlscr_workerthr = 0;

static int sdlscr_worker( void* arg ) {

    sdlscr_initok = false;
    sdlscr_doexit = false;

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
        goto ERR1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if ( renderer == 0 ) {
        print_sdlerror( "create renderer" );
        goto ERR2;
    }

    SDL_SetHint( SDL_HINT_RENDER_SCALE_QUALITY, "linear" );
    int rv = SDL_RenderSetLogicalSize(
        renderer,
        SDL_SCREENWIDTH,
        SDL_SCREENHEIGHT
    );
    if ( rv < 0 ) {
        print_sdlerror( "set logical size" );
        goto ERR3;
    }

    if ( !init_layers( renderer ) ) {
        fprintf( stderr, "failed to initialize layers\n" );
        goto ERR3;
    }

    sdllay_setcall( &layers[LAY_TIL], render_tiles, 0 );
    sdllay_setcall( &layers[LAY_TXT], render_text, 0 );
    sdllay_setcall( &layers[LAY_SPR], render_sprites, 0 );

    txtscr_init();
    tilescr_init();

    // sdllay_disable( &layers[LAY_BG]  );
    // sdllay_disable( &layers[LAY_TIL] );
    // sdllay_disable( &layers[LAY_TXT] );
    // sdllay_disable( &layers[LAY_GFX] );
    // sdllay_disable( &layers[LAY_SPR] );

    // confirm init ok
    sdlscr_initok = true;
    sdlev_raise( SDLEV_SCREENWORKERINITDONE );

    Uint64 lastTick = SDL_GetTicks64();

    // main loop
    for (;;) {
        if ( sdlscr_doexit ) {
            break;
        }
        // process messages
        SDL_Event ev;
        while ( SDL_PollEvent( &ev ) ) {
            Uint32 ty = ev.type;
            switch ( ty ) {
                case SDL_QUIT:
                    sdlscr_doexit = true;
                    break;
            }
        }

        // get current time
        Uint64 now = SDL_GetTicks64();

        if ( !sdllay_needsredraw( &layers[0], NLAYERS ) ) {
            // no redraw: check time
            if ( now - lastTick < 20U ) {
                // make sure at least a 1/50 sec will have passed
                Uint32 toWait = (Uint32)( 20U - ( now - lastTick ) );
                SDL_Delay( toWait );
            }
            lastTick = now;
            continue;
        }
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
        SDL_RenderPresent( renderer );
        lastTick = now;
        // fprintf( stderr, "drawn frame, @%" PRIu64 "\n", lastTick );
    }

    // fprintf( stderr, "worker exiting @%" PRIu64 "...\n", lastTick );

    // cleanup
    sdlscr_initok = false;
    cleanup_layers();
    SDL_DestroyRenderer( renderer );
    SDL_DestroyWindow( window );
    sdlev_raise( SDLEV_SCREENWORKERFINISHED );
    return 0;

        // error handling
// ERR4:   cleanup_layers();
ERR3:   SDL_DestroyRenderer( renderer );
ERR2:   SDL_DestroyWindow( window );
ERR1:   sdlev_raise( SDLEV_SCREENWORKERINITDONE );
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
    for (;;) {
        int ev = sdlev_wait();
        switch ( ev ) {
            case SDLEV_ERROR:
                fprintf( stderr, "sdlev_init(): sdlev_wait() returned error\n" );
                return false;
            case SDLEV_SCREENWORKERINITDONE:
                return sdlscr_initok;
            default:    // SDLEV_SIGNAL, SDLEV_TIMEOUT, SDLEV_NONE
                break;
        }
    }

    return true;
}

static bool sdlscr_cleanup2( void ) {

    // wait for handshake (thread termination)
    for (;;) {
        int ev = sdlev_wait();
        switch ( ev ) {
            case SDLEV_ERROR:
                fprintf( stderr, "sdlev_cleanup2(): sdlev_wait() returned error\n" );
                // thread might still be running!
                return false;
            case SDLEV_SCREENWORKERFINISHED:
                goto THREAD_DONE;
            default:    // SDLEV_SIGNAL, SDLEV_TIMEOUT, SDLEV_NONE
                break;
        }
    }

    // wait for thread to terminate and reap the thread status
THREAD_DONE:
    int rv = 0;
    SDL_WaitThread( sdlscr_workerthr, &rv ); sdlscr_workerthr = 0;
    return true;
}

void sdlscr_cleanup( void ) {
    for (;;) {
        if ( sdlscr_cleanup2() ) break;
        // check if the thread ran into cleanup processing
        if ( !sdlscr_initok ) {
            // yes: wait for thread to terminate
            int rv = 0;
            SDL_WaitThread( sdlscr_workerthr, &rv ); sdlscr_workerthr = 0;
            break;
        }
        // event processing failed, cannot exit
        SDL_Delay( 1000 );
    }
}

void sdlscr_printf( int y, int x, int bg, int fg, const char* fmt, ... ) {
      static char buf[512];
      va_list ap;
      va_start( ap, fmt );
      vsnprintf( buf, 512U, fmt, ap );
      va_end( ap );
      txtscr_print( y, x, bg, fg, buf );
      sdllay_set_modified( &layers[LAY_TXT] );
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
