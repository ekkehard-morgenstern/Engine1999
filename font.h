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

    ADDITIONAL PERMISSION (GPLv3 section 7): the Engine1999 Runtime Exception
        applies. Programs written in the built-in programming language, and
        executables made from them with BUILD, do not fall under this license
        as the exception describes (see the file LICENSE-EXCEPTION).

    CONTACT INFO:
        E-Mail: ekkehard@ekkehardmorgenstern.de
        Mail: Ekkehard Morgenstern, Mozartstr. 1, D-76744 Woerth am Rhein,
              Germany, Europe
*/

#ifndef FONT_H
#define FONT_H      1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

/*
Fonts for the UI (docs/ui-subsystem.md): glyph bitmaps with a width per glyph, no SDL, no dependencies besides the 8 x 12 font of the text screen.
The text screen keeps its own font (a grid of cells); these fonts are for buttons, labels and text fields.

    FONT_FIXED   the 8 x 12 font of the text screen, every glyph 8 wide (a monospaced font for entry fields and lists of numbers)
    FONT_SANS    a proportional font: the glyphs of the 8 x 12 font cut to their ink plus one pixel of space (an "i" is 5 wide, a full stop 3, a "W" 8)
    FONT_BOLD    the same, smeared one pixel to the right (a stem is 3 pixels wide)

All three have 12 rows; the baseline is under row 9 (capitals and digits rows 2 to 9, x-height rows 4 to 9, descenders rows 10 and 11). The character set
is ISO-8859-15 like everywhere in the Engine; the control glyphs 0..31 and 128..159 are 8 wide in all fonts. Text is drawn as horizontal runs through a
callback, so any target (a gfxcore bitmap, a layer of sdlscreen, a test) can use it.
*/

#define FONT_FIXED      0
#define FONT_SANS       1
#define FONT_BOLD       2
#define FONT_COUNT      3

#define FONT_HEIGHT     12
#define FONT_ASCENT     10          // the baseline: the rows 0 .. FONT_ASCENT - 1 are above it

typedef struct _font_t {
    int         id;
    uint8_t     width[256];         // the advance of each glyph in pixels (space between glyphs included)
    uint8_t     rows[256][12];      // 12 rows of bits, bit 7 = the left pixel; the ink starts at bit 7 in the proportional fonts
} font_t;

// the font (built when first asked for); 0 for an invalid id
const font_t* font_get( int id );

// the width of len bytes of text (an embedded zero byte is a glyph like any other); the height is FONT_HEIGHT
int font_measure( const font_t* f, const char* text, size_t len );

// a horizontal run of w pixels at x, y in the given color
typedef void (*font_span_t)( void* ctx, int x, int y, int w, int color );

// draws the text with its top left corner at x, y; returns the width. A NULL font draws nothing.
int font_draw( const font_t* f, int x, int y, const char* text, size_t len, int color, font_span_t span, void* ctx );

// how many bytes of the text fit into maxw pixels (at least 0), for clipping and for finding the character under a point (x pixels from the left)
size_t font_fit( const font_t* f, const char* text, size_t len, int maxw );

#endif
