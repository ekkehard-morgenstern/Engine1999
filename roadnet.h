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

#ifndef ROADNET_H
#define ROADNET_H   1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

/*
The road network of a procedural world (no SDL, pure: everything is a function of the numbers, like noise.c). The plane is cut into regions of 'spacing' units; every region has a
feature point (anywhere in it) with a kind of ground (forest, meadow, field, plain); a road joins a region's point to the point of its west and of its south neighbour with a chance
('chance'), a part of the roads ('tarfrac') are tarred, the others dirt tracks. A road is a path from point to point that wanders sideways (noise, 'wdirt' / 'wtar' units at most,
zero at both ends) and is cut into steps (50 units for a dirt track, 200 for a tarred road); the tarred roads are a second network of their own with far apart regions; the same numbers are used by demos/1987.bas (HASH(rx, ry, 1 / 2 / 3) = the point, HASH(rx, ry, 60 + e) = the road exists,
the tarred network: HASH(rx, ry, 11 / 12) = the point, HASH(rx, ry, 160 + e) = the road exists, NOISE(77, t * 3, rx * 31 + ry * 57 + e * 7 + 7777.5) = the wander; dirt: NOISE(77, t * 3, rx * 31 + ry * 57 + e * 7 + 0.5)), so the game draws the roads where the ground function expects them.
*/

// where the mountains are (the mask of M3D MOUNTAINS: a slow noise between lo and hi, faded in between the distances r0 and r1 from the origin): a road is not made where it is up
typedef struct _rmask_t {
    bool     on;
    uint64_t seed;
    double   scale, lo, hi, r0, r1;
    double   limit;                         // a road keeps away from every place where the mask is above this (0 .. 1)
    double   margin;                        // ... and from the places up to 'margin' units beside its way (the mountain's foot)
} rmask_t;

typedef struct _rnet_t {
    bool    on;
    double  spacing, chance, wdirt;         // the dirt tracks: regions of 'spacing' units, a track to the west / south neighbour with a chance, wandering by at most wdirt
    double  tspacing, tchance, wtar;        // the tarred roads (a network of its own, far apart and long): the same with tspacing, tchance, wtar
    rmask_t mask;                           // roads never cross a mountain (they go round: a road that would is not there; a tunnel would be the way through)
} rnet_t;

typedef struct _rlink_t {
    double  x1, y1, x2, y2, len;            // the two points and the distance between them
    bool    tar;
    double  key;                            // the second argument of the wander noise
    int     steps;                          // len / step + 1 (50 units for a dirt track, 200 for a tarred road)
    double  step;
    double  wa;                             // the wander
} rlink_t;

typedef struct _rnear_t {
    double  dirt, tar;                      // the distance to the nearest dirt / tarred road (1e9: none within 'reach')
    rlink_t tlink;                          // the nearest tarred road
    double  tt;                             // and the place on it, 0 .. 1
} rnear_t;

// how much mountain the mask says there is at (x, y): 0 (none) .. 1 (the full range)
double rnet_mountain( const rmask_t* m, double x, double y );
// the road of the network (0 dirt, 1 tarred) from the point of region (rx, ry) to its neighbour west (e = 0) or south (e = 1); false when there is none (the hash says no, or it would cross mountains)
bool rnet_link( const rnet_t* n, int net, int64_t rx, int64_t ry, int e, rlink_t* l );
// frees the cache of the calling thread (the worker thread of m3d.c calls it when it ends)
void rnet_flush( void );
void rnet_set_simd( int on );                               // rnet_near(): 1 vector loop, 0 scalar twin (same bits), -1 by the environment ENGINE1999_NOSIMD
// the roads of a network (0 dirt, 1 tarred) that may touch the rectangle (their bounding box and their wander cross it): up to max of them, how many
int  rnet_links( const rnet_t* n, int net, double x0, double y0, double x1, double y1, rlink_t* out, int max );
// the place at t (0 .. 1) of a road
void rnet_point( const rlink_t* l, double t, double* x, double* y );
// the nearest roads to (x, y) within reach (at most 700)
void rnet_near( const rnet_t* n, double x, double y, double reach, rnear_t* out );
// the kind of ground at (x, y): 0 plain, 1 meadow, 2 field, 3 forest (the nearest feature point of the 3 x 3 regions)
int  rnet_kind( const rnet_t* n, double x, double y );

#endif
