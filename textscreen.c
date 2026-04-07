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

#include "textscreen.h"

#define FONTSIZE 3072

static textcell_t textscr[TXTSCR_WIDTH * TXTSCR_HEIGHT];
static uint8_t    currfont[FONTSIZE];
static uint8_t    pen, paper;

extern const uint8_t fontdef1[FONTSIZE];

static void txtscr_write( int y, int x, int bg, int fg, const char* text ) {
      if ( x < 0 || x > TXTSCR_WIDTH || y < 0 || y > TXTSCR_HEIGHT ) {
            return;
      }
      if ( bg < 0 || bg > 255 || fg < 0 || fg > 255 || text == 0 ) {
            return;
      }
      const char* s = text;
      textcell_t* d = &textscr[ y * TXTSCR_WIDTH + x ];
      size_t n = strlen( s );
      if ( n > (size_t)( TXTSCR_WIDTH - x ) ) {
            n = (size_t)( TXTSCR_WIDTH - x );
      }
      while ( n-- ) {
            *d++ = TXTSCR_MAKECELL( *s++, bg, fg );
      }
}

void txtscr_init( void ) {
      memcpy( currfont, fontdef1, FONTSIZE );
      textcell_t cell = TXTSCR_MAKECELL( 32, TXTSCR_BGCOL, TXTSCR_FGCOL );
      paper = TXTSCR_BGCOL;
      pen = TXTSCR_FGCOL;
      for ( size_t i=0; i < TXTSCR_WIDTH * TXTSCR_HEIGHT; ++i ) {
            textscr[i] = cell;
      }
      txtscr_write(
            0,
            0,
            TXTSCR_BGCOL,
            TXTSCR_FGCOL,
            "Hello world!"
      );
}

void txtscr_render( uint8_t* target ) {
      const textcell_t* s = &textscr[0];
      uint8_t* d = target;
      int stride = TXTSCR_WIDTH * 8;
      for ( uint8_t y=0; y < (uint8_t) TXTSCR_HEIGHT; ++y ) {
            for ( uint8_t x=0; x < (uint8_t) TXTSCR_WIDTH; ++x ) {
                  textcell_t cell = *s++;
                  uint8_t bg = TXTSCR_CELL_BG(cell);
                  uint8_t fg = TXTSCR_CELL_FG(cell);
                  uint8_t ch = TXTSCR_CELL_CHR(cell);
                  const uint8_t* fontchar = &currfont[ ch * 12 ];
                  uint8_t* d0 = d;
                  for ( uint8_t cy=0; cy < UINT8_C(12); ++cy ) {
                        uint8_t by = *fontchar++;
                        for ( uint8_t cx=0; cx < UINT8_C(8); ++cx ) {
                              *d++ = by & UINT8_C(0X80) ? fg : bg;
                              by <<= UINT8_C(1);
                        }
                        d += stride - 8;
                  }
                  d = d0 + 8;
            }
            d += stride * 11;
      }
}
