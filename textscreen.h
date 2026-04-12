#pragma once

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

#ifndef TEXTSCREEN_H
#define TEXTSCREEN_H        1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

#define TXTSCR_WIDTH    80
#define TXTSCR_HEIGHT   25

#define TXTSCR_FGCOL    1
#define TXTSCR_BGCOL    0

#define TXTSCR_MAKECELL(c,b,f) ( \
( ((uint32_t)(b)) << UINT8_C(16) ) | \
( ((uint32_t)(f)) << UINT8_C(8)  ) | \
(  (uint32_t)(c)                 ) )

#define TXTSCR_CELL_BG(n) \
    ( (uint8_t)( ( (n) >> UINT8_C(16) ) & UINT32_C(0XFF) ) )

#define TXTSCR_CELL_FG(n) \
    ( (uint8_t)( ( (n) >> UINT8_C(8) ) & UINT32_C(0XFF) ) )

#define TXTSCR_CELL_CHR(n) \
    ( (uint8_t)( (n) & UINT32_C(0XFF) ) )

typedef uint32_t textcell_t;

bool txtscr_enablecursor( bool enable );
bool txtscr_blinkcursor( void );
void txtscr_getsize( int* outsx, int* outsy );
void txtscr_getcursor( int* outx, int* outy );

void txtscr_init( void );

void txtscr_render( uint8_t* target );

int txtscr_write( int y, int x, int bg, int fg, const char* text, int len );
void txtscr_locate( int x, int y );
void txtscr_scrolldown( void );
void txtscr_backspace( void );
void txtscr_rubout( void );
void txtscr_scrollup( void );
void txtscr_print( int y, int x, int bg, int fg, const char* text );
void txtscr_printf( int y, int x, int bg, int fg, const char* fmt, ... );

#endif
