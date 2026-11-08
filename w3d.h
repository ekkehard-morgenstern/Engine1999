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


#ifndef W3D_H
#define W3D_H   1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

/*
The software 3D sector renderer (docs/3d-subsystem.md): a world of sectors (floor and ceiling height, flats, light) bounded by walls, some of
which are portals into the neighboring sector, in the style of Doom and Duke Nukem 3D, plus billboard sprites ("things"). Every screen column is
traced through the sectors with its own 2D ray, so any sector shape (concave, with holes) works and nothing has to be sorted. The module draws
into its own palette bitmap (one byte per pixel); the host copies that onto a graphics layer. It has no SDL and no global state: the
statements W3D ... and the functions W3D("...") of BASIC are carried out by w3d_op() / w3d_query().

World units are free; the defaults assume a player 56 high with the eye at 46 and sectors about 128 high, a texture pixel = one unit.
Angles are in degrees: 0 = along +x, 90 = along +y (counterclockwise on the map with y up); to the right of the view direction is clockwise.
*/

#define W3D_MAXSECTORS  4096
#define W3D_MAXWALLS    4096            // per sector
#define W3D_MAXTEX      1024
#define W3D_MAXTHINGS   1024
#define W3D_MAXVIEWW    1280
#define W3D_MAXVIEWH    800
#define W3D_LEVELS      32              // light levels of the color map (31 = full brightness)

// wall flags
#define W3D_WF_MASKED   1               // a portal: the middle texture is drawn over the opening (palette index 0 is see-through)
#define W3D_WF_BLOCK    2               // a portal that movement cannot pass (a window, a railing)
#define W3D_WF_SOLID    4               // a portal that stops shots and sight (glass that is not clear)

// results of w3d_move()
#define W3D_HIT_WALL    1               // a wall pushed the circle aside
#define W3D_HIT_STUCK   2               // no movement was possible at all
#define W3D_HIT_SECTOR  4               // the circle crossed into another sector

typedef struct _w3d_t w3d_t;

w3d_t* w3d_new( void );
void   w3d_free( w3d_t* w );

// BASIC: W3D statements by operation number (GFXOP_W3D* of gfxops.h) and W3D("...") functions (GFXQ_W3D*). Both return false with *err set to a
// message. The operations RENDER and COLORMAP are the host's (they need the layer and its palette): see w3d_render() / w3d_colormap().
bool w3d_op( w3d_t* w, int op, const double* a, int na, const char* s, size_t slen, const char** err );
bool w3d_query( w3d_t* w, int q, const double* a, int na, double* result );

// the color map: 32 brightness rows from a 256 entry palette (ARGB); index 0 stays 0 everywhere
void w3d_colormap( w3d_t* w, const uint32_t* argb );

// draws the world into the internal bitmap (view width / scale by view height / scale) and returns it; 0 if there is none
const uint8_t* w3d_render( w3d_t* w );

// the view on the layer: x, y, size in layer pixels, and the scale (1..4): the internal bitmap is that size divided by the scale
void w3d_getview( const w3d_t* w, int* x, int* y, int* width, int* height, int* scale, int* bmpw, int* bmph );

#endif
