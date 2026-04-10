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

#include "sdlutil.h"

uint64_t sdlutil_getnsec( struct timespec* pts ) {
    struct timespec ts;
    memset( &ts, 0, sizeof(ts) );
    clock_gettime( CLOCK_REALTIME, &ts );
    if ( pts ) {
        *pts = ts;
    }
    return ( ts.tv_sec * UINT64_C(1000000000) ) + ts.tv_nsec;
}

int sdlutil_comparetime( const struct timespec* a, const struct timespec* b ) {
    if ( a->tv_sec > b->tv_sec ) {
        return 1;
    } else if ( a->tv_sec < b->tv_sec ) {
        return -1;
    } else if ( a->tv_nsec > b->tv_nsec ) {
        return 1;
    } else if ( a->tv_nsec < b->tv_nsec ) {
        return -1;
    }
    return 0;
}

void sdlutil_projecttime( uint64_t nsec, const struct timespec* from, struct timespec* to ) {
    struct timespec ts;
    if ( from ) {
        ts = *from;
    } else {
        memset( &ts, 0, sizeof(ts) );
        clock_gettime( CLOCK_REALTIME, &ts );
    }
    long secs  = nsec / UINT64_C(1000000000);
    long nsecs = nsec % UINT64_C(1000000000);
    ts.tv_nsec += nsecs;
    if ( ts.tv_nsec >= 1000000000L ) {
        ts.tv_nsec -= 1000000000L;
        ts.tv_sec++;
    }
    ts.tv_sec += secs;
    *to = ts;
}

void sdlutil_nanosleep( uint64_t nsec, const struct timespec* pts ) {
    struct timespec ts;
    sdlutil_projecttime( nsec, pts, &ts );
RETRY:
    int rv = clock_nanosleep( CLOCK_REALTIME, TIMER_ABSTIME, &ts, 0 );
    if ( rv == EINTR ) goto RETRY;
}
