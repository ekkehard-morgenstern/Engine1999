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

#include "sdlmain.h"
#include "sdlscreen.h"

#define BLU 2
#define WHT 140
#define GOL 123

static const uint8_t tiletest1[TILE_WIDTH * TILE_HEIGHT] = {
    GOL, GOL, GOL, GOL, GOL, GOL, GOL, GOL, GOL, GOL, GOL, GOL, GOL, GOL, GOL, GOL,
    GOL, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, GOL,
    GOL, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, GOL,
    GOL, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, GOL,
    GOL, WHT, WHT, BLU, BLU, BLU, BLU, BLU, BLU, BLU, BLU, BLU, BLU, WHT, WHT, GOL,
    GOL, WHT, WHT, WHT, BLU, WHT, WHT, WHT, WHT, WHT, WHT, BLU, WHT, WHT, WHT, GOL,
    GOL, WHT, WHT, WHT, WHT, BLU, WHT, WHT, WHT, WHT, BLU, WHT, WHT, WHT, WHT, GOL,
    GOL, WHT, WHT, WHT, WHT, WHT, BLU, WHT, WHT, BLU, WHT, WHT, WHT, WHT, WHT, GOL,
    GOL, WHT, WHT, WHT, WHT, WHT, WHT, BLU, BLU, WHT, WHT, WHT, WHT, WHT, WHT, GOL,
    GOL, WHT, WHT, WHT, WHT, WHT, WHT, BLU, BLU, WHT, WHT, WHT, WHT, WHT, WHT, GOL,
    GOL, WHT, WHT, WHT, WHT, WHT, WHT, BLU, BLU, WHT, WHT, WHT, WHT, WHT, WHT, GOL,
    GOL, WHT, WHT, WHT, WHT, WHT, WHT, BLU, BLU, WHT, WHT, WHT, WHT, WHT, WHT, GOL,
    GOL, WHT, WHT, WHT, WHT, WHT, WHT, BLU, BLU, WHT, WHT, WHT, WHT, WHT, WHT, GOL,
    GOL, WHT, WHT, WHT, WHT, WHT, WHT, BLU, BLU, WHT, WHT, WHT, WHT, WHT, WHT, GOL,
    GOL, WHT, WHT, WHT, WHT, WHT, WHT, BLU, BLU, WHT, WHT, WHT, WHT, WHT, WHT, GOL,
    GOL, WHT, WHT, WHT, WHT, WHT, WHT, BLU, BLU, WHT, WHT, WHT, WHT, WHT, WHT, GOL,
    GOL, WHT, WHT, WHT, WHT, WHT, BLU, WHT, WHT, BLU, WHT, WHT, WHT, WHT, WHT, GOL,
    GOL, WHT, WHT, WHT, WHT, BLU, WHT, WHT, WHT, WHT, BLU, WHT, WHT, WHT, WHT, GOL,
    GOL, WHT, WHT, WHT, BLU, WHT, WHT, WHT, WHT, WHT, WHT, BLU, WHT, WHT, WHT, GOL,
    GOL, WHT, WHT, BLU, BLU, BLU, BLU, BLU, BLU, BLU, BLU, BLU, BLU, WHT, WHT, GOL,
    GOL, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, GOL,
    GOL, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, GOL,
    GOL, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, WHT, GOL,
    GOL, GOL, GOL, GOL, GOL, GOL, GOL, GOL, GOL, GOL, GOL, GOL, GOL, GOL, GOL, GOL
};

int main( int argc, char** argv ) {

    sdl_init();
    sdlscr_printf( 0, 0, 0, 1, "Hello world!\nThere!" );
    sdlscr_writetile( 0, tiletest1 );

    int scrollx = 0, scrolly = 0;
    while ( !sdlscr_term() ) {
        scrollx = ( scrollx + 1 ) % TILE_WIDTH;
        scrolly = ( scrolly + 1 ) % TILE_HEIGHT;
        sdlscr_scrolltiles( scrollx, scrolly );
        SDL_Delay( 100 );
    }

    return EXIT_SUCCESS;
}
