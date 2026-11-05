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

#include "font.h"

extern const uint8_t fontdef1[3072];            // 8x12font1.c: 256 glyphs of 12 bytes

static font_t fonts[FONT_COUNT];
static bool   built[FONT_COUNT];

// the columns the ink of a glyph covers: the leftmost and the rightmost set bit (0 = left); false if the glyph is blank
static bool ink_columns( const uint8_t* rows, int* first, int* last ) {
    int lo = 8, hi = -1;
    for ( int r = 0; r < 12; ++r ) {
        for ( int c = 0; c < 8; ++c ) {
            if ( rows[r] & ( 0x80 >> c ) ) {
                if ( c < lo ) {
                    lo = c;
                }
                if ( c > hi ) {
                    hi = c;
                }
            }
        }
    }
    *first = lo;
    *last = hi;
    return hi >= 0;
}

static void build_font( int id ) {
    font_t* f = &fonts[id];
    memset( f, 0, sizeof(*f) );
    f->id = id;
    for ( int c = 0; c < 256; ++c ) {
        const uint8_t* src = &fontdef1[ c * 12 ];
        bool control = c < 32 || ( c >= 128 && c < 160 );
        if ( id == FONT_FIXED || control ) {
            memcpy( f->rows[c], src, 12 );
            f->width[c] = 8;
            continue;
        }
        int first, last;
        if ( !ink_columns( src, &first, &last ) ) {
            f->width[c] = 4;                    // a space (and the blank glyphs: the no-break space)
            continue;
        }
        int w = last - first + 1;
        for ( int r = 0; r < 12; ++r ) {
            uint8_t b = (uint8_t)( src[r] << first );
            if ( id == FONT_BOLD && c != '#' ) {      // (the bars of # are a pixel apart: the double strike made a block of it)
                b |= (uint8_t)( b >> 1 );       // one pixel to the right: stems of 3 pixels (a glyph of 8 pixels loses the ninth)
            }
            f->rows[c][r] = b;
        }
        f->width[c] = (uint8_t)( w + 1 + ( id == FONT_BOLD ? 1 : 0 ) );
        if ( f->width[c] > 9 ) {
            f->width[c] = 9;
        }
    }
}

const font_t* font_get( int id ) {
    if ( id < 0 || id >= FONT_COUNT ) {
        return 0;
    }
    if ( !built[id] ) {
        build_font( id );
        built[id] = true;
    }
    return &fonts[id];
}

int font_measure( const font_t* f, const char* text, size_t len ) {
    int w = 0;
    if ( f == 0 ) {
        return 0;
    }
    for ( size_t i = 0; i < len; ++i ) {
        w += f->width[ (uint8_t) text[i] ];
    }
    return w;
}

int font_draw( const font_t* f, int x, int y, const char* text, size_t len, int color, font_span_t span, void* ctx ) {
    if ( f == 0 ) {
        return 0;
    }
    int x0 = x;
    for ( size_t i = 0; i < len; ++i ) {
        const uint8_t* rows = f->rows[ (uint8_t) text[i] ];
        int gw = f->width[ (uint8_t) text[i] ];
        if ( gw > 8 ) {
            gw = 8;                                   // a 9 wide advance still has 8 bits of ink
        }
        for ( int r = 0; r < 12; ++r ) {
            uint8_t b = rows[r];
            int c = 0;
            while ( c < gw ) {
                if ( b & ( 0x80 >> c ) ) {
                    int run = 1;
                    while ( c + run < gw && ( b & ( 0x80 >> ( c + run ) ) ) ) {
                        ++run;
                    }
                    if ( span != 0 ) {
                        span( ctx, x + c, y + r, run, color );
                    }
                    c += run;
                } else {
                    ++c;
                }
            }
        }
        x += f->width[ (uint8_t) text[i] ];
    }
    return x - x0;
}

size_t font_fit( const font_t* f, const char* text, size_t len, int maxw ) {
    if ( f == 0 ) {
        return 0;
    }
    int w = 0;
    size_t i = 0;
    while ( i < len ) {
        int gw = f->width[ (uint8_t) text[i] ];
        if ( w + gw > maxw ) {
            break;
        }
        w += gw;
        ++i;
    }
    return i;
}
