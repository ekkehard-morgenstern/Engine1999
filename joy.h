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

#ifndef JOY_H
#define JOY_H   1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

/*
The game controllers (no SDL): up to JOY_MAXPLAYERS pads, each in a slot (player 1..4 = slot 0..3). A pad has the standard layout of SDL's game controller
API, whatever its brand: four face buttons named by position (A bottom, B right, X left, Y top), two shoulder buttons, Start / Back / Guide, the two stick clicks,
the D-pad, two sticks and two triggers. The state is the one JOY() of BASIC reads (docs/controller-support.md).

Threads, as for the mouse (mouse.h): the screen thread's event loop (sdljoy.c) calls the joy_event_* functions; the program's thread reads. Everything is an
atomic word, no locks, nothing is lost: button presses and releases are latched bits that are exchanged to zero when read, and a counter counts the events.

A pad keeps its slot while it is connected; when it is unplugged the slot is free for the next pad that is plugged in (the lowest free slot), the other pads keep their numbers.
Stick values are -1..1 with a dead zone (joy_setdeadzone(), default JOY_DEFAULT_DEADZONE) around the middle; up is negative (screen coordinates). Triggers are 0..1.
A slot without a pad reads as zero everywhere (a program needs no checks).
*/

#define JOY_MAXPLAYERS  4

// the buttons are bits (JOY("BUTTONS"), JOY("PRESSED"), JOY("RELEASED") return them added up)
#define JOYB_A          1U          // face button at the bottom (Xbox A, PlayStation Cross, Nintendo B)
#define JOYB_B          2U          // at the right (Xbox B, Circle, Nintendo A)
#define JOYB_X          4U          // at the left (Xbox X, Square, Nintendo Y)
#define JOYB_Y          8U          // at the top (Xbox Y, Triangle, Nintendo X)
#define JOYB_LB         16U         // left shoulder button
#define JOYB_RB         32U         // right shoulder button
#define JOYB_BACK       64U         // Back / Select / View
#define JOYB_START      128U        // Start / Menu / Options
#define JOYB_GUIDE      256U        // the big middle button (Xbox, PS, Home)
#define JOYB_LS         512U        // left stick pressed
#define JOYB_RS         1024U       // right stick pressed
#define JOYB_UP         2048U       // the D-pad
#define JOYB_DOWN       4096U
#define JOYB_LEFT       8192U
#define JOYB_RIGHT      16384U
#define JOYB_ALL        32767U
#define JOYB_COUNT      15

// the axes
#define JOYA_LX         0           // left stick, left -1 .. right 1
#define JOYA_LY         1           // up -1 .. down 1
#define JOYA_RX         2           // right stick
#define JOYA_RY         3
#define JOYA_LT         4           // left trigger 0 .. 1
#define JOYA_RT         5           // right trigger
#define JOYA_COUNT      6

#define JOY_DEFAULT_DEADZONE    0.2

void joy_init( void );                                  // no pads, the default dead zone, the event counter at 0

// the screen thread: the events of the pads
int  joy_attach( const char* name );                    // a pad was plugged in: its slot (0..3), -1 if all are taken (a counted event)
void joy_detach( int slot );                            // it was unplugged: its buttons are released (latched), the slot is free (a counted event)
void joy_event_button( int slot, unsigned button, bool down );      // button is one JOYB_* bit
void joy_event_axis( int slot, int axis, int raw );     // axis JOYA_*, raw -32768 .. 32767 (triggers 0 .. 32767)

// the program's thread
bool     joy_connected( int slot );
int      joy_count( void );                             // how many pads are plugged in
uint32_t joy_buttons( int slot );                       // the buttons held down now
uint32_t joy_pressed( int slot );                       // the buttons pressed since the last call of this (cleared by it)
uint32_t joy_released( int slot );                      // ... and released
double   joy_axis( int slot, int axis );                // dead zone applied; sticks -1 .. 1, triggers 0 .. 1
uint32_t joy_events( void );                            // button presses and releases, plugging in and out so far (wraps at 2^31; ON JOY looks for a change)
const char* joy_name( int slot );                       // the pad's name ("" if there is none); valid until the pad is unplugged
void   joy_setdeadzone( double dz );                    // 0 .. 0.9
double joy_getdeadzone( void );

// the names BASIC uses (case does not matter): "A" ... "RIGHT" are buttons, "LX" ... "RT" axes
int joy_button_by_name( const char* name, size_t len );    // the JOYB_* bit, 0 if it is none
int joy_axis_by_name( const char* name, size_t len );      // JOYA_*, -1 if it is none

#endif
