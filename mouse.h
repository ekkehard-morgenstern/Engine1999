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

#ifndef MOUSE_H
#define MOUSE_H   1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

/*
The mouse of the window (no SDL): the position and buttons, and when the pointer (the reserved sprite of cursors.h) is seen. The pointer is shown for a
while after every sign of life, so that it does not stand in the way of the text:

    - the mouse moves, a button is pressed or released, the wheel turns       (events of the window)
    - MOUSE SHOW, MOUSE POS x, y, MOUSE SHAPE name                            (requests of the program)

and hidden again when MOUSE_DEFAULT_TIMEOUT milliseconds (3 s) have passed without one of them (MOUSE TIMEOUT changes the time, 0 = it stays), or at once by
MOUSE HIDE.

Threads: everything that changes the pointer (cursors_*) is done by mouse_tick(), which the screen thread calls once per frame. The program's thread only
posts requests (mouse_request_*, atomic words) and reads values the screen thread has published (mouse_get): no locks, nothing gets lost (a request is a
word that is exchanged to zero when it is taken; button presses are latched bits). The events are posted by the screen thread itself.

Coordinates are those of the sprite layer: x 0..639, y 0..299 (the "area", mouse_setarea(): the console host sets it to the terminal's size in cells times
8 x 12 pixels, so that a cell is a mouse position of 8 x 12 pixels, and a program sees the same numbers in both hosts). Buttons are bits: 1 left, 2 right, 4 middle.
*/

#define MOUSE_DEFAULT_TIMEOUT   3000        // milliseconds

#define MOUSE_LEFT      1
#define MOUSE_RIGHT     2
#define MOUSE_MIDDLE    4

// the values mouse_get() returns
#define MOUSEV_X        1       // the position
#define MOUSEV_Y        2
#define MOUSEV_BUTTONS  3       // the buttons held down now
#define MOUSEV_PRESSED  4       // the buttons pressed since the last call of this (cleared by it)
#define MOUSEV_RELEASED 5       // the buttons released since the last call of this (cleared by it)
#define MOUSEV_WHEEL    6       // the wheel steps since the last call of this (cleared by it; up is positive)
#define MOUSEV_SHAPE    7       // the pointer's shape (CUR_*)
#define MOUSEV_VISIBLE  8       // 1 if the pointer is shown now
#define MOUSEV_EVENTS   9       // how many button presses, releases and wheel steps there have been (a counter that wraps at 2^31; ON MOUSE looks for a change)

#define MOUSE_AREA_MAXW   32768                         // the largest area (the VM accepts positions up to this)
#define MOUSE_AREA_MAXH   32768

void mouse_setarea( int w, int h );                     // the size of the area in pixels (default 640 x 300); positions are clamped to it
void mouse_init( void );                                // a hidden arrow at 0, 0 (after cursors_install()); the timeout is the default

// the screen thread: events of the window (x, y already in the coordinates of the sprite layer; they are clamped to 0..639, 0..299)
void mouse_event_motion( int x, int y );
void mouse_event_button( int button, bool down );       // MOUSE_LEFT, MOUSE_RIGHT or MOUSE_MIDDLE
void mouse_event_wheel( int steps );
bool mouse_tick( uint64_t now_ms );                     // once per frame: takes events and requests, shows / hides; true if the pointer's look changed (redraw)
bool mouse_warp_wanted( int* x, int* y );               // the window should put its pointer here (after a MOUSE POS); true once per request

// the program's thread: requests
void mouse_request_show( void );
void mouse_request_hide( void );
void mouse_request_pos( int x, int y );
void mouse_request_shape( int shape );                  // CUR_*
void mouse_request_timeout( int ms );                   // 0 = the pointer stays once shown; negative: the default

int  mouse_get( int what );                             // MOUSEV_*

#endif
