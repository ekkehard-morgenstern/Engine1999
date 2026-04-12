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
#include "sdlevent.h"
#include "sdlaudio.h"

typedef struct _usrdata_t {
    int dummy;
} usrdata_t;

static void init_usrdata( usrdata_t* data ) {
    data->dummy = 0;
}

static void vblank_handler( void* usrdata ) {
    usrdata_t* data = (usrdata_t*) usrdata;
    data->dummy++;
}

int main( int argc, char** argv ) {

    sdl_init();

    srand( time(0) );

    usrdata_t usrdata;
    init_usrdata( &usrdata );

    // uint64_t last = sdlscr_getnsec(0);
    while ( !sdlscr_term() ) {
        char buf[256];
        int ev = sdlscr_lineinput( 0, buf, 256U, vblank_handler, &usrdata );
        if ( ev & SDLEV_SCREENWORKERFINISHED ) break;
        sdlscr_printf( -1, -1, -1, -1, "%s\n", buf );
    }

    return EXIT_SUCCESS;
}
