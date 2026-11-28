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

#pragma once
#ifndef HOUSE_H
#define HOUSE_H

#include "stdtypes.h"

/*
The family houses of the static world (docs/world-subsystem.md "Ruins"): the C version of the generator of tools/housegen.bas / tools/house3d.bas (no SDL, no m3d): a house
from a type (1 plain gable house, 2 with a garage annex, 3 hip roof, 4 dormer and porch, 5 bay window and a flat roofed extension at the back) and a seed, with the same
random numbers as the BASIC generator (HASH(seed * 7919, counter, 17)), so the same seed gives the same house in both; and the outside of it as a triangle mesh for a
stage of damage by a directed energy weapon (0 intact .. 4 a shell) in two levels of detail (0 far, 1 middle). Units: the generator works in dm (10 cm), the mesh in game units
(3.2 cm, 3.125 a dm), the house's south west corner at (0, 0), the ground at z = 0, the street side at y = 0 (the front door).
*/

#define HOUSE_TW        4               // the thickness of the outer walls in dm
#define HOUSE_SKIRT     22              // the outer walls reach this far (dm, 69 units) below the base, into the ground: a site may slope by 60 units, the ground inside the footprint is cut away (m3d.c)
#define HOUSE_MAXROOMS  8
#define HOUSE_MAXDOORS  16
#define HOUSE_MAXWINS   40
#define HOUSE_MAXFURN   64
#define HOUSE_MAXCLEAR  24

// the kinds of window (hwin_t.kind): 0 window, 1 terrace door, 2 front door, 3 door to the garage or the extension, 4 garage door, 5 bay window
// the sides (hwin_t.side): 0 south (front) 1 north 2 west 3 east, 4 / 5 / 6 the garage's south / east / north wall, 7 / 8 / 9 the extension's west / east / north wall
typedef struct { double x1, y1, x2, y2; int type; int color; } hroom_t;      // type 1 hall, 2 WC, 3 living room, 4 dining room, 5 kitchen, 6 study, 7 bedroom, 8 children's room, 9 bathroom, 10 garage, 11 extension
typedef struct { int axis; double x, y, w; } hdoor_t;                        // axis 0: a wall at x with the door centred at y; axis 1: a wall at y, centred at x
typedef struct { int floor, side; double pos, w, sill, head; int kind; } hwin_t;
typedef struct { double x1, y1, x2, y2; int color; } hfurn_t;

typedef struct {
    int      type;
    int64_t  seed;
    double   lx, dy;                    // the outer size in dm (long side, short side)
    double   flz[2];                    // the floor levels: 3, 32
    hroom_t  rm[2][HOUSE_MAXROOMS];     int nrm[2];
    hdoor_t  dor[2][HOUSE_MAXDOORS];    int ndor[2];
    hwin_t   win[HOUSE_MAXWINS];        int nwin;
    hfurn_t  fu[2][HOUSE_MAXFURN];      int nfu[2];
    double   cl[2][HOUSE_MAXCLEAR][4];  int ncl[2];
    int      ss;                        // the stairs: 0 along the west wall of the hall, 1 along the east wall
    double   sx1, sx2, sy1, sy2;        // the stairs
    double   wy;                        // the wall between the hall and the WC / bathroom
    double   kn, ez;                    // the knee wall and the height of the eaves
    double   zr, chx;                   // the ridge, the chimney
    int      plc, rfc, att;
    double   ai, ay;                    // type 2: the garage's inner width and depth
    double   bc, bw, bx1, bx2;          // type 5: the bay window
    double   exw, exy, exx1, exx2;      // type 5: the extension
} house_t;

void house_gen( house_t* h, int type, int64_t seed );
void house_gen_basic( house_t* h, int type, int64_t seed );   // without the clear zones at the outer doors and without removing furniture that cuts a room off: the same house as tools/house3d.bas
void house_size( int type, int64_t seed, double* lx, double* dy );     // the outer size alone (cheap)

// the number of rooms (both floors) one cannot walk into from the front door (a walker of 3 dm width, over the stairs to the upper floor); 0 = every room can be entered, -1 out of memory
int house_unreachable( const house_t* h );

// -- the outside as a mesh ------------------------------------------------------------------------------------------------------
enum { HM_PLASTER, HM_CHAR, HM_BRICK, HM_ROOF, HM_GLASS, HM_DOOR, HM_WOOD, HM_CONCRETE, HM_RIM, HM_GARAGE, HM_FRAME, HM_PAINT0, HM_PAINT1, HM_PAINT2, HM_PAINT3, HM_PAINT4, HM_PAINT5, HM_NMAT };     // HM_PAINT0 .. 5: the paint of the rooms inside (the same colours as the plaster 0 .. 5)

typedef struct { double x, y, z, u, v; } hvert_t;          // u, v: 0 .. 1 over the whole picture of the material (the frame, doors and garage doors), or -1: the engine finds them from the position (the walls, roof, ...)
typedef struct { int a, b, c, mat; } htri_t;
typedef struct {
    hvert_t* v;  int nv, capv;
    htri_t*  t;  int nt, capt;
    int      plaster;                   // the colour of the plaster (0 .. 5) and of the roof (0 .. 2) of this house, from the seed
    int      roofc;
    double   w, d, h;                   // the footprint (x, y from 0) and the height of the ridge, in units
} hmesh_t;

// the outside of the house h at the stage 0 .. 4 (damage 0, 0.25, 0.5, 0.75, 1) in the level of detail 0 (far, about 60 .. 250 triangles) or 1 (middle, a few hundred
// to about 1,500); false when out of memory; free with house_mesh_free
bool house_mesh( const house_t* h, int stage, int level, hmesh_t* m );
void house_mesh_free( hmesh_t* m );

// the inside of the house at the stage 0 .. 4: the walls of the rooms (paint, with the doorways and painted windows), floors, ceilings, the stairs, the furniture; the same house
// coordinates as house_mesh (so the two fit); meant for a two sided object that is shown while the player is near (docs/world-subsystem.md "Every room can be entered")
bool house_mesh_inside( const house_t* h, int stage, hmesh_t* m );

#endif
