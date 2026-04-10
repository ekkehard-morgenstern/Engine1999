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

#include "sdlevent.h"
#include "sdltypes.h"
#include "sdlutil.h"

static SDL_mutex* sdlev_mtx = 0;
static int        sdlev_pending = 0;

bool sdlev_init( void ) {
    sdlev_mtx = SDL_CreateMutex();
    if ( sdlev_mtx == 0 ) {
        fprintf( stderr, "failed to create mutex: %s\n", SDL_GetError() );
        return false;
    }
    return true;
}

void sdlev_cleanup( void ) {
    SDL_DestroyMutex( sdlev_mtx ); sdlev_mtx = 0;
}

void sdlev_raise( int evtmsk ) {
    SDL_LockMutex( sdlev_mtx );
    sdlev_pending |= evtmsk;
    SDL_UnlockMutex( sdlev_mtx );
}

#define SDLEV_WAIT_MIN_NSEC     UINT64_C(250000)
#define SDLEV_WAIT_MAX_NSEC     UINT64_C(4000000)

int sdlev_wait( int evtmsk ) {
    uint64_t waitnsec = SDLEV_WAIT_MIN_NSEC;
    uint64_t result = 0;
    for (;;) {
        SDL_LockMutex( sdlev_mtx );
        if ( sdlev_pending & evtmsk ) {
            result = sdlev_pending & evtmsk;
            sdlev_pending &= ~evtmsk;
        }
        SDL_UnlockMutex( sdlev_mtx );
        if ( result ) {
            break;
        }
        sdlutil_nanosleep( waitnsec, 0 );
        waitnsec *= 2U;
        if ( waitnsec > SDLEV_WAIT_MAX_NSEC ) {
            waitnsec = SDLEV_WAIT_MAX_NSEC;
        }
    }
    return result;
}
