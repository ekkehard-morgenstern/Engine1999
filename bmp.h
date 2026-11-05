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

#ifndef BMP_H
#define BMP_H   1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

/*
Windows BMP files (no SDL, no VM, no I/O): reading 1, 4, 8, 16, 24 and 32 bits per pixel (uncompressed; bit fields for 16 and 32), writing
8 bits per pixel with a 256 entry palette, nearest color matching and a small median cut quantizer. Used by gfxfile.c (GFX LOAD / SAVE,
SPRITE LOAD / SAVE, TILE LOAD / SAVE, GFX PALLOAD / PALSAVE). Colors are 0xAARRGGBB.
*/

#define BMP_MAXDIM  16384

typedef struct _bmp_image_t {
    int         w, h;
    uint32_t*   argb;           // w * h pixels, top row first
    uint8_t*    idx;            // w * h palette indices, top row first; 0 if the file has no palette (more than 8 bits per pixel)
    int         npal;           // the entries of the palette in the file (0 if none)
    uint32_t    pal[256];       // (entries npal..255 are 0)
} bmp_image_t;

bool bmp_decode( const uint8_t* data, size_t len, bmp_image_t* img, const char** err );     // false: *err says why; nothing to free then
void bmp_free( bmp_image_t* img );

// 8 bits per pixel, bottom-up, a 256 entry palette (the alpha of an entry goes into the reserved byte); the result is malloc()ed
bool bmp_encode8( int w, int h, const uint8_t* idx, const uint32_t pal[256], uint8_t** pout, size_t* plen );

// the nearest entry of pal[first..255] to an opaque color (RGB distance, entries that are fully transparent are skipped);
// a pixel with alpha < 128 is entry 0
int bmp_nearest( const uint32_t pal[256], int first, uint32_t argb );

// a palette for the pixels: all colors if there are at most maxcolors, else a median cut; entries from 'first'; returns the number of colors
int bmp_quantize( const uint32_t* argb, size_t n, uint32_t pal[256], int first, int maxcolors );

#endif
