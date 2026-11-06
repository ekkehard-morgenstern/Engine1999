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

#include "stdtypes.h"
#include "clipboard.h"

static char* clip_text = 0;
static size_t clip_len = 0;

void clip_internal_set( const char* text, size_t len ) {
    char* copy = (char*) malloc( len + 1U );
    if ( copy == 0 ) {
        return;
    }
    memcpy( copy, text, len );
    copy[len] = '\0';
    free( clip_text );
    clip_text = copy;
    clip_len = len;
}

char* clip_internal_get( size_t* len ) {
    if ( clip_text == 0 ) {
        *len = 0;
        return 0;
    }
    char* copy = (char*) malloc( clip_len + 1U );
    if ( copy == 0 ) {
        *len = 0;
        return 0;
    }
    memcpy( copy, clip_text, clip_len + 1U );
    *len = clip_len;
    return copy;
}

size_t clip_base64( const unsigned char* in, size_t len, char* out ) {
    static const char tab[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t o = 0;
    for ( size_t i = 0; i < len; i += 3U ) {
        unsigned v = (unsigned) in[i] << 16;
        if ( i + 1U < len ) v |= (unsigned) in[i+1U] << 8;
        if ( i + 2U < len ) v |= in[i+2U];
        out[o++] = tab[ ( v >> 18 ) & 63U ];
        out[o++] = tab[ ( v >> 12 ) & 63U ];
        out[o++] = i + 1U < len ? tab[ ( v >> 6 ) & 63U ] : '=';
        out[o++] = i + 2U < len ? tab[ v & 63U ] : '=';
    }
    out[o] = '\0';
    return o;
}

size_t clip_normalize( char* text, size_t len ) {
    size_t w = 0;
    for ( size_t r = 0; r < len; ++r ) {
        unsigned char c = (unsigned char) text[r];
        if ( c == '\r' ) {
            if ( r + 1U < len && text[r+1U] == '\n' ) {
                ++r;
            }
            c = '\n';
        } else if ( c == '\t' ) {
            c = ' ';
        } else if ( c < 32 && c != '\n' ) {
            continue;
        } else if ( c >= 127 && c < 160 ) {
            continue;
        }
        text[w++] = (char) c;
    }
    while ( w > 0 && text[w-1U] == '\n' ) {
        --w;
    }
    return w;
}
