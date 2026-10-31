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

#ifndef GFXCORE_H
#define GFXCORE_H   1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

/*
Drawing on a bitmap of palette indices (one byte per pixel), without SDL. Only sdlscreen.c calls it, for the layers it owns
(docs/graphics-subsystem.md). All coordinates are clipped to the bitmap; nothing here can fail or write outside of it.
*/

#define GFXM_SET    0       // dst = color
#define GFXM_CLEAR  1       // dst = 0
#define GFXM_XOR    2       // dst ^= color
#define GFXM_OR     3       // dst |= color
#define GFXM_AND    4       // dst &= color

typedef struct _gfx_t {
    uint8_t*    buf;            // w * h bytes
    int         w, h;
    int         mode;           // GFXM_*
    int         pen;            // the color used when a statement gives none (0..255)
    int         drawx, drawy;   // the position of DRAW
    int         drawcolor;      // the color of DRAW (-1 = the pen)
    int         drawscale;      // DRAW "S": quarters (4 = 1:1)
    int         drawangle;      // DRAW "A": 0..3 quarter turns
} gfx_t;

void gfx_init( gfx_t* g, uint8_t* buf, int w, int h );

void gfx_clear( gfx_t* g, int color );                              // fills the bitmap (ignores the mode)
void gfx_pset( gfx_t* g, int x, int y, int color );                 // color < 0: the pen
int  gfx_point( const gfx_t* g, int x, int y );                     // -1 outside
void gfx_line( gfx_t* g, int x1, int y1, int x2, int y2, int color );
void gfx_box( gfx_t* g, int x1, int y1, int x2, int y2, int color, bool fill );
// a circle, arc or ellipse: radius r in x, r * ratio in y; angles in degrees, 0 = right, counterclockwise on the screen;
// a1 == a2 (or both < 0): the whole circle
void gfx_circle( gfx_t* g, int x, int y, int r, int color, int a1, int a2, double ratio );
void gfx_radius( gfx_t* g, int x, int y, int r, double angle, int color );
// flood fill from x,y with color; border < 0: replaces the area of the color at x,y, else fills up to pixels of the border color
void gfx_paint( gfx_t* g, int x, int y, int color, int border );
// a block as a string: MKI(w) MKI(h) w*h pixels (big endian); returns the size (also when out is too small: then nothing is written)
size_t gfx_grab( const gfx_t* g, int x1, int y1, int x2, int y2, uint8_t* out, size_t outsize );
bool gfx_blit( gfx_t* g, const uint8_t* block, size_t size, int x, int y, bool transparent );   // false: the block is malformed
// text from an 8 x 12 font (12 bytes per character, bit 7 = the left pixel), each pixel as sx * sy pixels
void gfx_text( gfx_t* g, int x, int y, const char* text, size_t len, int color, int sx, int sy, const uint8_t* font );
// the GW-BASIC drawing language: U D L R E F G H n, M x,y (a sign makes it relative), B (move only), N (come back), C n, S n, A n
// returns false with a message in err for a command it does not understand
bool gfx_draw( gfx_t* g, const char* cmd, size_t len, char* err, size_t errsz );

#endif
