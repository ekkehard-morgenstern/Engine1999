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
#include "sdlevent.h"

static void print_sdlerror( const char* scope ) {
    const char* msg = SDL_GetError();
    fprintf( stderr, "SDL error [%s]: %s\n", scope, msg );
}

static bool sdlscr_initok = false;
static bool sdlscr_doexit = false;

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

    // confirm init ok
    sdlscr_initok = true;
    sdlev_raise( SDLEV_SCREENWORKERINITDONE );

    // main loop
    for (;;) {
        if ( sdlscr_doexit ) {
            break;
        }

    }


    // cleanup
    sdlscr_initok = false;
    SDL_DestroyRenderer( renderer );
    SDL_DestroyWindow( window );
    sdlev_raise( SDLEV_SCREENWORKERFINISHED );
    return 0;

        // error handling
ERR3:   SDL_DestroyRenderer( renderer );
ERR2:   SDL_DestroyWindow( window );
ERR1:   sdlev_raise( SDLEV_SCREENWORKERINITDONE );
        return 0;
}