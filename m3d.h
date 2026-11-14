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



#ifndef M3D_H
#define M3D_H   1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

/*
The software 3D mesh renderer (docs/3d-mesh-subsystem.md): a world of objects, each a triangle mesh with a position and a heading, plus billboard sprites
("things"). Every triangle is transformed, clipped against the near plane, culled and rasterized with a depth buffer, perspective-correct texturing and a
light level per vertex (plus lamps and distance fog) through the color map of the palette. There are no sectors and no portals: anything can lie anywhere and
over anything. Collision (a vertical capsule against the triangles), floor following and rays (shots, sight, use) work on the same triangles. The module
draws into its own palette bitmap (one byte per pixel); the host copies that onto a graphics layer. No SDL, no global state: the M3D statements and the
functions M3D("...") of BASIC are carried out by m3d_op() / m3d_query().

Coordinates: x east, y north, z up. Angles in degrees: 0 = along +x, 90 = along +y; to the right of the view direction is clockwise. Triangles are
counterclockwise when seen from their front (the normal (b-a) x (c-a) points to the viewer), unless the object is two sided.
*/

#define M3D_MAXOBJECTS  16384
#define M3D_MAXTEX      1024
#define M3D_MAXTHINGS   1024
#define M3D_MAXLAMPS    8
#define M3D_MAXZONES    1024
#define M3D_MAXVIEWW    1280
#define M3D_MAXVIEWH    800
#define M3D_LEVELS      32

// object flags
#define M3D_OF_MASKED   1               // texture index 0 is see-through
#define M3D_OF_TRANS    2               // drawn mixed half and half with what is behind it (index 0 see-through), after everything else
#define M3D_OF_PASS     4               // movement passes through it and it is no floor or ceiling (a decoration, an open doorway)
#define M3D_OF_SEETHRU  8               // shots and sight pass through it (glass, a railing)
#define M3D_OF_TWOSIDED 16              // both sides of every triangle are drawn and solid

typedef struct _m3d_t m3d_t;

m3d_t* m3d_new( void );
void   m3d_free( m3d_t* m );
bool   m3d_op( m3d_t* m, int op, const double* a, int na, const char* s, size_t slen, const char** err );
bool   m3d_query( m3d_t* m, int q, const double* a, int na, double* result );
int    m3d_nverts( const m3d_t* m, int o );                  // the number of vertices of an object (0: none)
void   m3d_colormap( m3d_t* m, const uint32_t* argb );
const uint8_t* m3d_render( m3d_t* m );                       // draws a frame; the palette bitmap, or 0 when the renderer in use makes 32 bit pictures
bool   m3d_is_argb( const m3d_t* m );                         // the renderer in use makes 32 bit ARGB pictures (m3d_render32()), not palette bitmaps
const uint32_t* m3d_render32( m3d_t* m );                     // draws a frame; the ARGB picture (bw x bh, see m3d_getview()), or 0 when the renderer makes palette bitmaps
struct _m3drend_t;
void   m3d_set_hardware( m3d_t* m, const struct _m3drend_t* r );   // the host registers the hardware (OpenGL) renderer it has (0: none; M3D HARDWARE is then an error)
void   m3d_getview( const m3d_t* m, int* x, int* y, int* width, int* height, int* scale, int* bmpw, int* bmph );

#endif
