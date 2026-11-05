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

#ifndef GFXFILE_H
#define GFXFILE_H   1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

/*
Pictures in BMP files for the graphics statements (no SDL, no I/O: the caller reads and writes the file bytes). The screen is reached through a
small table of functions, so the code can be tested with a fake. Colors are matched to the palette of the layer (nearest entry; entry 0 is
transparent); with 'setpal' the palette of the file is installed in the layer first. Errors are texts for the BASIC error message.
*/

#define GFXFILE_LW       640         // the bitmap layers
#define GFXFILE_LH       300
#define GFXFILE_CELLW   16          // sprite images and tiles
#define GFXFILE_CELLH   24
#define GFXFILE_SPRITE  0           // 'kind' of the cells
#define GFXFILE_TILE    1
#define GFXFILE_SPRMAX  248         // sprite images 0..247 are the program's (248..255 are the engine's)
#define GFXFILE_TILEMAX 256

typedef struct _gfxfile_host_t {
    void*       ud;
    uint32_t    (*palget)( void* ud, int layer, int index );
    void        (*palset)( void* ud, int layer, int index, uint32_t argb );
    int         (*point)( void* ud, int layer, int x, int y );              // -1 outside
    void        (*pset)( void* ud, int layer, int x, int y, int color );
    bool        (*getcell)( void* ud, int kind, int n, uint8_t* out );      // GFXFILE_CELLW * GFXFILE_CELLH bytes
    void        (*putcell)( void* ud, int kind, int n, const uint8_t* data );
} gfxfile_host_t;

// GFX LOAD: the picture at x, y of the bitmap layer (clipped)
bool gfxfile_load_layer( const gfxfile_host_t* h, int layer, const uint8_t* file, size_t len, int x, int y, bool setpal, const char** err );
// GFX SAVE: the rectangle (clipped to the layer) as an 8 bit BMP with the palette of the layer; *pout is malloc()ed
bool gfxfile_save_layer( const gfxfile_host_t* h, int layer, int x1, int y1, int x2, int y2, uint8_t** pout, size_t* plen, const char** err );
// SPRITE LOAD / TILE LOAD: a sheet cut into cells of 16 x 24, row by row, into the images first..; count < 0 = all that fit
bool gfxfile_load_cells( const gfxfile_host_t* h, int kind, int first, int count, const uint8_t* file, size_t len, bool setpal, const char** err );
// SPRITE SAVE / TILE SAVE: count images in a sheet 'cols' cells wide (cols < 1 = up to 16)
bool gfxfile_save_cells( const gfxfile_host_t* h, int kind, int first, int count, int cols, uint8_t** pout, size_t* plen, const char** err );
// GFX PALLOAD: the colors of the file (or the 255 best ones of a true color picture) into the layer's palette
bool gfxfile_load_palette( const gfxfile_host_t* h, int layer, const uint8_t* file, size_t len, const char** err );
// GFX PALSAVE: a picture of 16 x 16 cells, the cell at column c, row r has the color r * 16 + c
bool gfxfile_save_palette( const gfxfile_host_t* h, int layer, uint8_t** pout, size_t* plen, const char** err );

#endif
