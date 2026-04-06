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

extern const uint8_t fontdef1[FONTSIZE];

void txtscr_init( void ) {
      memcpy( currfont, fontdef1, FONTSIZE );
      textcell_t cell = TXTSCR_MAKECELL( 32, TXTSCR_BGCOL, TXTSCR_FGCOL );
      for ( size_t i=0; i < TXTSCR_WIDTH * TXTSCR_HEIGHT; ++i ) {
            textscr[i] = cell;
      }
      return true;
}

void txtscr_render( uint8_t* target, const uint32_t* palette ) {
      const textcell_t* s = &textscr[0];

}
