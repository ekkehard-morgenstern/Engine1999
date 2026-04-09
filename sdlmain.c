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

#include "sdltypes.h"
#include "sdlmain.h"
#include "sdlevent.h"
#include "sdlscreen.h"
#include "sdlaudio.h"

static void sdl_cleanup( void ) {
    sdlaud_cleanup();
    sdlscr_cleanup();
    SDL_Quit();
}

void sdl_init( void ) {
    int rv = SDL_Init( SDL_INIT_EVERYTHING );
    if ( rv < 0 ) {
        fprintf( stderr, "sdl_init(): failed to initialize SDL: %s\n", SDL_GetError() );
        goto ERR1;
    }
    sdlev_init();
    if ( !sdlscr_init() ) {
        fprintf( stderr, "sdl_init(): failed to initialize screen\n" );
        goto ERR2;
    }
    if ( !sdlaud_init() ) {
        fprintf( stderr, "sdl_init(): failed to initialize audio\n" );
        goto ERR3;
    }
    atexit( sdl_cleanup );
    return;

// ERR4:   sdlaud_cleanup();
ERR3:   sdlscr_cleanup();
ERR2:   SDL_Quit();
ERR1:   exit( EXIT_FAILURE );
}
