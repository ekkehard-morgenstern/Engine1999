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
    sdllay_init_many( &layers[0], NLAYERS, layer_titles, renderer );
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

static bool sdlscr_initok = false;
static bool sdlscr_doexit = false;

static uint32_t sdlscr_bgcol = UINT32_C(0XFF708090); // SlateGray

static void* sdlscr_worker( void* arg ) {

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

    // confirm init ok
    sdlscr_initok = true;
    sdlev_raise( SDLEV_SCREENWORKERINITDONE );

    // main loop
    for (;;) {
        if ( sdlscr_doexit ) {
            break;
        }

        if ( !sdllay_needsredraw( &layers[0], NLAYERS ) ) {
            continue;
        }
        sdllay_2texture_many( &layers[0], NLAYERS );
        uint32_t c = sdlscr_bgcol;
        uint8_t a = (uint8_t)( c >> UINT8_C(24) );
        uint8_t r = (uint8_t)( c >> UINT8_C(16) );
        uint8_t g = (uint8_t)( c >> UINT8_C( 8) );
        uint8_t b = (uint8_t)( c                );
        SDL_SetRenderDrawColor( renderer, r, g, b, a );
        SDL_RenderClear( renderer );
        draw_layers( renderer );
        SDL_RenderPresent( renderer );
    }


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