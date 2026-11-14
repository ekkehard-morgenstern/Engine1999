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

#ifndef SHOT_H
#define SHOT_H   1

#include "stdtypes.h"

/*
Screenshots (roadmap item 82; no SDL, no VM, no I/O except shot_write_file()). The screen thread owns the layer memories, so the picture is always composed
there: shot_compose() draws the layers (8 bit pictures with an ARGB palette each) over the backdrop, in priority order, with the same alpha blending as the
renderer, into a 640 x 300 buffer of opaque ARGB pixels. Two ways to get a picture out of the screen thread, both without locks:

  - A program's SCREENSHOT [file$]: the program thread calls shot_grab() (it asks the screen thread by an atomic word and waits, at most about 1 s, until the
    screen thread has composed the picture into the slot's buffer and says so) and writes the BMP itself with the I/O layer (policy, drivers), so a file name
    outside the program's folder is asked about in the program's thread as everywhere else. The screen thread only copies pixels (well below a millisecond).
  - The screenshot key (PrtScr): the screen thread composes into a buffer of its own and hands it to a short-lived thread that writes
    shot-YYYYMMDD-HHMMSS-n.bmp in the current directory (shot_autoname() / shot_write_file()), so the screen thread never waits for the disk.

The picture is the composed layers, not the window (no scaling, no letterbox bars, the mouse pointer sprite is in it when it is shown).
*/

#define SHOT_WIDTH      640
#define SHOT_HEIGHT 300

typedef struct _shot_layer_t {
    const uint8_t*  mem;                // SHOT_WIDTH * SHOT_HEIGHT palette indices
    const uint32_t* pal;                // 256 ARGB entries
    bool            enabled;
    uint8_t         priority;           // drawn in ascending order (every number 0..n-1 once)
    const uint32_t* argb;               // or (when not 0, then mem and pal are not used): a 32 bit layer, SHOT_WIDTH * SHOT_HEIGHT ARGB pixels
} shot_layer_t;

// draws n layers over the backdrop (ARGB, opaque) into out (SHOT_WIDTH * SHOT_HEIGHT opaque ARGB pixels)
void shot_compose( const shot_layer_t* lay, int n, uint32_t backdrop, uint32_t* out );

// "shot-YYYYMMDD-HHMMSS-n.bmp" for the time t (local time); n is a running number (1, 2, ...) that keeps two shots in one second apart
void shot_autoname( char* buf, size_t size, time_t t, unsigned n );
unsigned shot_nextnumber( void );       // 1, 2, 3 ... (atomic)

// 24 bit BMP of the pixels (the alpha is ignored); *pout is malloc()ed
bool shot_encode( const uint32_t* argb, uint8_t** pout, size_t* plen );
// writes the BMP of the pixels to the file (plain stdio, the current directory unless the name has a path); false with *err
bool shot_write_file( const char* name, const uint32_t* argb, const char** err );

// -- program thread <-> screen thread --------------------------------------------------------------------------
// the program thread: asks for a picture and waits for it (false after about 1 s: nobody serves requests, e.g. no screen thread); out gets SHOT_WIDTH * SHOT_HEIGHT pixels
bool shot_grab( uint32_t* out );
// the screen thread, once per frame, with the picture composed from the current layers: serves a request of the program thread; returns true when it did
// (the caller composes only when shot_wanted() says so)
bool shot_wanted( void );
void shot_serve( const uint32_t* composed );

#endif
