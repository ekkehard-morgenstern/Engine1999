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


#ifndef WORLD_H
#define WORLD_H     1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

/*
The static world of the 1987 game (docs/world-subsystem.md; no SDL): made once when a new game starts, kept on disk, never decided again during play (owner, 2026-11-21).
A world is n x n chunks (n 1 .. 1000) of WORLD_CHUNK units (500 m at 3.2 cm a unit); the origin (0, 0) lies at the corner where the four middle chunks meet; x grows east
(chunk column), y north (chunk row). The file `<dir>/world.dat` holds a header and one record of WORLD_RECSIZE bytes a chunk (all numbers big endian): the chunk's seed (from the
RANDPATH field: the state of a 1024 bit random field walked south and east from the world seed, so every chunk has a seed of its own that depends on its place), a summary of the
ground (lowest, highest and mean height, the dominant kind of ground, flags for roads, water and mountains) and the number of road links near it. The geometry is not stored: it is
made again from the records and the world's ground function (the pure layers of m3d.c: hills, mountains, lakes, the road networks of roadnet.c), which depends on the world seed only,
so the same world gives the same ground on every machine, and neighbouring chunks agree at their borders.
*/

#define WORLD_MAXN      1000
#define WORLD_CHUNK     15625.0         // units a chunk is wide (500 m)
#define WORLD_RECSIZE   32
#define WORLD_HDRSIZE   64
#define WORLD_VERSION   4

// flags of a chunk
#define WCF_TAR         1               // a tarred road passes through the chunk
#define WCF_DIRT        2               // a dirt track does
#define WCF_WATER       4               // some of the ground lies below the water level
#define WCF_MOUNTAIN    8               // some of the ground is above MOUNTAIN_H
#define WCF_EDGE        16              // in the wall round the world

#define WORLD_WATERZ    (-12.0)         // the water level (WZ of demos/1987.bas)
#define WORLD_MOUNTH    1500.0          // units: 48 m: above that the ground counts as mountain

typedef struct {
    uint64_t seed;                      // the chunk's seed
    int32_t  hmin, hmax, hmean;         // heights in units (sampled on 5 x 5 points, 125 m apart)
    uint8_t  kind;                      // 0 plain, 1 meadow, 2 field, 3 forest (at the chunk's middle)
    uint8_t  flags;                     // WCF_*
    uint8_t  ntar, ndirt;               // tarred road links through the chunk (ndirt: unused, 0)
} wchunk_t;

typedef struct _world world_t;

typedef void (*world_progress_t)( void* ud, int done, int total );     // called by world_create() now and then (done rows of total)

// makes a new world in the folder 'dir' (created when it does not exist; an existing world.dat is replaced), 'threads' workers (0: by the number of processors), returns it opened
// (0 on error: errno / the text of world_error() say why)
world_t*    world_create( const char* dir, int n, uint64_t seed, int threads, world_progress_t cb, void* ud );
world_t*    world_open( const char* dir );                      // opens a finished world (0: no world, wrong version, not finished)
void        world_close( world_t* w );
const char* world_error( void );                                // the text of the last error (per thread)

int         world_size( const world_t* w );                     // n
uint64_t    world_seed( const world_t* w );
bool        world_chunk( const world_t* w, int cx, int cy, wchunk_t* out );     // reads the record from disk (thread safe); false outside the world
double      world_x0( const world_t* w, int cx );               // the west edge of chunk column cx
double      world_y0( const world_t* w, int cy );               // the south edge of chunk row cy
bool        world_chunk_at( const world_t* w, double x, double y, int* cx, int* cy );   // the chunk that holds the place (false: outside the world)
double      world_height( const world_t* w, double x, double y );   // the ground height, units: the same everywhere on every call (thread safe, 0 outside)

// creation in the background (for a program that must go on: the game shows a progress bar): world_make_start() makes the world on a thread of its own
typedef struct _wmake wmake_t;
wmake_t*    world_make_start( const char* dir, int n, uint64_t seed, int threads );    // 0: out of memory
int         world_make_progress( wmake_t* m, int* total );     // rows done (of *total); -1 when the creation failed; the end is world_make_done()
bool        world_make_done( wmake_t* m );
world_t*    world_make_finish( wmake_t* m );                   // waits for the end, frees the handle, returns the opened world (0: failed, world_error() says why)

// the place's seed field, for the test and for tools: the seed of chunk (cx, cy) as the creation computes it (walks the field: O(cx + cy) steps)
uint64_t    world_field_seed( uint64_t seed, int cx, int cy );

#endif
