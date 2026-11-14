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

    ADDITIONAL PERMISSION (GPLv3 section 7): the Engine1999 Runtime Exception
        applies. Programs written in the built-in programming language, and
        executables made from them with BUILD, do not fall under this license
        as the exception describes (see the file LICENSE-EXCEPTION).

    CONTACT INFO:
        E-Mail: ekkehard@ekkehardmorgenstern.de
        Mail: Ekkehard Morgenstern, Mozartstr. 1, D-76744 Woerth am Rhein,
              Germany, Europe
*/

#include "shot.h"

// -- composing ----------------------------------------------------------------------------------------------

void shot_compose( const shot_layer_t* lay, int n, uint32_t backdrop, uint32_t* out ) {
    size_t npix = (size_t) SHOT_WIDTH * (size_t) SHOT_HEIGHT;
    uint32_t bg = backdrop | UINT32_C(0xFF000000);
    for ( size_t i=0; i < npix; ++i ) {
        out[i] = bg;
    }
    for ( int pri=0; pri < n; ++pri ) {
        const shot_layer_t* l = 0;
        for ( int j=0; j < n; ++j ) {
            if ( lay[j].priority == (uint8_t) pri ) {
                l = &lay[j];
                break;
            }
        }
        if ( l == 0 || !l->enabled || ( l->argb == 0 && ( l->mem == 0 || l->pal == 0 ) ) ) {
            continue;
        }
        for ( size_t i=0; i < npix; ++i ) {
            uint32_t s = l->argb != 0 ? l->argb[i] : l->pal[ l->mem[i] ];
            uint32_t a = s >> 24;
            if ( a == 0U ) {
                continue;
            }
            if ( a == 255U ) {
                out[i] = s;
                continue;
            }
            uint32_t d = out[i];
            uint32_t r = ( ( s >> 16 & 255U ) * a + ( d >> 16 & 255U ) * ( 255U - a ) + 127U ) / 255U;
            uint32_t g = ( ( s >>  8 & 255U ) * a + ( d >>  8 & 255U ) * ( 255U - a ) + 127U ) / 255U;
            uint32_t b = ( ( s       & 255U ) * a + ( d       & 255U ) * ( 255U - a ) + 127U ) / 255U;
            out[i] = UINT32_C(0xFF000000) | r << 16 | g << 8 | b;
        }
    }
}

// -- names and files ----------------------------------------------------------------------------------------

static unsigned shot_counter = 0;

unsigned shot_nextnumber( void ) {
    return __atomic_add_fetch( &shot_counter, 1U, __ATOMIC_RELAXED );
}

void shot_autoname( char* buf, size_t size, time_t t, unsigned n ) {
    struct tm tmv;
    memset( &tmv, 0, sizeof(tmv) );
    localtime_r( &t, &tmv );
    snprintf( buf, size, "shot-%04d%02d%02d-%02d%02d%02d-%u.bmp", tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday, tmv.tm_hour, tmv.tm_min, tmv.tm_sec, n );
}

bool shot_encode( const uint32_t* argb, uint8_t** pout, size_t* plen ) {
    size_t stride = (size_t) SHOT_WIDTH * 3U;
    stride = ( stride + 3U ) & ~(size_t) 3U;
    size_t datasz = stride * (size_t) SHOT_HEIGHT, off = 14U + 40U;
    uint8_t* o = (uint8_t*) calloc( 1U, off + datasz );
    if ( o == 0 ) {
        return false;
    }
    uint32_t total = (uint32_t)( off + datasz );
    o[0] = 'B'; o[1] = 'M';
    o[2] = (uint8_t) total; o[3] = (uint8_t)( total >> 8 ); o[4] = (uint8_t)( total >> 16 ); o[5] = (uint8_t)( total >> 24 );
    o[10] = (uint8_t) off;
    o[14] = 40;
    o[18] = (uint8_t) SHOT_WIDTH; o[19] = (uint8_t)( SHOT_WIDTH >> 8 );
    o[22] = (uint8_t) SHOT_HEIGHT; o[23] = (uint8_t)( SHOT_HEIGHT >> 8 );
    o[26] = 1; o[28] = 24;
    o[34] = (uint8_t) datasz; o[35] = (uint8_t)( datasz >> 8 ); o[36] = (uint8_t)( datasz >> 16 ); o[37] = (uint8_t)( datasz >> 24 );
    o[38] = 0x13; o[39] = 0x0B; o[42] = 0x13; o[43] = 0x0B;        // 2835 pixels per metre
    for ( int y=0; y < SHOT_HEIGHT; ++y ) {
        uint8_t* row = o + off + stride * (size_t)( SHOT_HEIGHT - 1 - y );
        const uint32_t* src = argb + (size_t) y * SHOT_WIDTH;
        for ( int x=0; x < SHOT_WIDTH; ++x ) {
            row[3*x] = (uint8_t) src[x]; row[3*x+1] = (uint8_t)( src[x] >> 8 ); row[3*x+2] = (uint8_t)( src[x] >> 16 );
        }
    }
    *pout = o;
    *plen = off + datasz;
    return true;
}

bool shot_write_file( const char* name, const uint32_t* argb, const char** err ) {
    uint8_t* data = 0; size_t len = 0;
    if ( !shot_encode( argb, &data, &len ) ) {
        *err = "Out of memory";
        return false;
    }
    FILE* f = fopen( name, "wb" );
    bool ok = f != 0 && fwrite( data, 1U, len, f ) == len;
    if ( f != 0 && fclose( f ) != 0 ) {
        ok = false;
    }
    free( data );
    if ( !ok ) {
        *err = "Can't write the file";
    }
    return ok;
}

// -- the request slot -----------------------------------------------------------------------------------------

static volatile int shot_state = 0;             // 0 idle, 1 requested by the program thread, 2 served (the pixels are in shot_buffer)
static uint32_t shot_buffer[SHOT_WIDTH * SHOT_HEIGHT];

bool shot_wanted( void ) {
    return __atomic_load_n( &shot_state, __ATOMIC_ACQUIRE ) == 1;
}

void shot_serve( const uint32_t* composed ) {
    if ( __atomic_load_n( &shot_state, __ATOMIC_ACQUIRE ) != 1 ) {
        return;
    }
    memcpy( shot_buffer, composed, sizeof(shot_buffer) );
    __atomic_store_n( &shot_state, 2, __ATOMIC_RELEASE );
}

bool shot_grab( uint32_t* out ) {
    int expect = 0;
    // (one request at a time: a second program thread is not a thing here, but a stale 2 from a request that timed out is cleaned up)
    int st = __atomic_load_n( &shot_state, __ATOMIC_ACQUIRE );
    if ( st == 2 ) {
        __atomic_store_n( &shot_state, 0, __ATOMIC_RELEASE );
    }
    if ( !__atomic_compare_exchange_n( &shot_state, &expect, 1, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE ) ) {
        return false;
    }
    for ( int i=0; i < 200; ++i ) {             // 200 x 5 ms
        if ( __atomic_load_n( &shot_state, __ATOMIC_ACQUIRE ) == 2 ) {
            memcpy( out, shot_buffer, sizeof(shot_buffer) );
            __atomic_store_n( &shot_state, 0, __ATOMIC_RELEASE );
            return true;
        }
        { struct timespec ts = { 0, 5000000L }; nanosleep( &ts, 0 ); }
    }
    int one = 1;
    if ( __atomic_compare_exchange_n( &shot_state, &one, 0, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE ) ) {
        return false;                           // withdrawn
    }
    // the screen thread served it just now
    memcpy( out, shot_buffer, sizeof(shot_buffer) );
    __atomic_store_n( &shot_state, 0, __ATOMIC_RELEASE );
    return true;
}
