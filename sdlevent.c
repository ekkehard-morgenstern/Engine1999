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

static int sdlev_hnd = -1;

static struct epoll_event sdlev_ary[SDLEV_COUNT];

static void sdlev_cleanup2( int highest ) {
    for ( int i = highest-1; i >= 0; --i ) {
        int rv = epoll_ctl(
            sdlev_hnd,
            EPOLL_CTL_DEL,
            sdlev_ary[i].data.fd,
            0
        );
        if ( rv == -1 ) {
            perror( "epoll_ctl(2)" );
        }
        rv = close( sdlev_ary[i].data.fd );
        if ( rv == -1 ) {
            perror( "close(2)");
        }
        sdlev_ary[i].data.fd = -1;
    }
}

static void sdlev_cleanup( void ) {
    sdlev_cleanup2( SDLEV_COUNT );
    int rv = close( sdlev_hnd );
    if ( rv == -1 ) {
        perror( "close(2)" );
    }
    sdlev_hnd = -1;
}

void sdlev_init( void ) {

    memset( &sdlev_ary[0], 0, sizeof(struct epoll_event) * SDLEV_COUNT );

    sdlev_hnd = epoll_create1( 0 );
    if ( sdlev_hnd == -1 ) {
        perror( "epoll_create1(2)" );
        goto ERR1;
    }

    int i;
    for ( i=0; i < SDLEV_COUNT; ++i ) {
        int fd = eventfd( 0, 0 );
        if ( fd == -1 ) {
            perror( "eventfd(2)" );
            goto ERR2;
        }
        sdlev_ary[i].events = EPOLLIN;
        sdlev_ary[i].data.fd = fd;
        int rv = epoll_ctl(
            sdlev_hnd,
            EPOLL_CTL_ADD,
            fd,
            &sdlev_ary[i]
        );
        if ( rv == -1 ) {
            perror( "epoll_ctl(2)" );
            close( fd );
            goto ERR2;
        }
    }

    atexit( sdlev_cleanup );
    return;

ERR2:     sdlev_cleanup2( i );
          close( sdlev_hnd ); sdlev_hnd = -1;
ERR1:     exit( EXIT_FAILURE );
}

void sdlev_raise( int evt ) {
    if ( evt < 0 || evt >= SDLEV_COUNT ) {
        return;
    }
    uint64_t dummy = UINT64_C(1);
RETRY:
    int rv = write( sdlev_ary[evt].data.fd, &dummy, sizeof(dummy) );
    if ( rv == -1 && errno == EINTR ) {
        goto RETRY;
    }
    if ( rv == -1 ) {
        perror( "write(2)" );
    }
}

static bool sdlev_check( int evt, int fd ) {
    if ( evt < 0 || evt >= SDLEV_COUNT || fd == -1 ) {
        return false;
    }
    if ( sdlev_ary[evt].data.fd != fd ) {
        return false;
    }
    uint64_t dummy = 0;
RETRY:
    int rv = read( sdlev_ary[evt].data.fd, &dummy, sizeof(dummy) );
    if ( rv == -1 && errno == EINTR ) {
        goto RETRY;
    }
    if ( rv == -1 ) {
        perror( "read(2)" );
    }
    return true;
}

int sdlev_wait( void ) {
    struct epoll_event ev;
    memset( &ev, 0, sizeof(ev) );
RETRY:
    int rv = epoll_wait( sdlev_hnd, &ev, 1, 20 );
    if ( rv == -1 && errno == EINTR ) {
        return SDLEV_SIGNAL;
    }
    if ( rv == -1 ) {
        perror( "epoll_wait(2)" );
        return SDLEV_ERROR;
    }
    if ( rv == 0 ) {
        return SDLEV_TIMEOUT;
    }
    if ( ev.events & EPOLLIN ) {
        for ( int i=0; i < SDLEV_COUNT; ++i ) {
            if ( sdlev_check( i, ev.data.fd ) ) {
                return i;
            }
        }
    }
    return SDLEV_NONE;
}