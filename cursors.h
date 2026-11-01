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

#ifndef CURSORS_H
#define CURSORS_H   1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

/*
The pictures of the mouse pointer (no SDL): the sprite SPR_MOUSE (sprscreen.h, a reserved sprite that a program can't touch) shows one of these. They are drawn
once, at start-up, into the images and animation sequences of the engine (the second half of the sprite map) by cursors_install(), which sprscr_init() calls.
Like the cursors of a desktop: an arrow, a busy ring that turns, an arrow with a small turning ring ("working in the background"), a text cursor, a pointing
hand, a closed hand (dragging), a crosshair, the two resize arrows, the move arrows, the forbidden sign and the help arrow. The colors are entries of the sprite
layer's palette (black outline, white fill, ...), so a program that changes that palette changes the pointer, too.

Every shape has a hot spot: the pixel of the picture that is "where the pointer is". cursors_move( x, y ) puts the hot spot at x, y of the sprite layer
(0..639, 0..299), whatever the shape is.
*/

#define CUR_ARROW       0       // the pointer (hot spot: the tip)
#define CUR_WAIT        1       // a ring of dots that turns (animated)
#define CUR_BUSY        2       // the pointer with a small turning ring (animated)
#define CUR_TEXT        3       // the text cursor (I-beam)
#define CUR_HAND        4       // a pointing hand
#define CUR_GRAB        5       // a closed hand (dragging)
#define CUR_CROSS       6       // a crosshair
#define CUR_SIZEH       7       // resize left / right
#define CUR_SIZEV       8       // resize up / down
#define CUR_MOVE        9       // move: arrows in four directions
#define CUR_NO          10      // forbidden
#define CUR_HELP        11      // the pointer with a question mark
#define CUR_COUNT       12

void cursors_install( void );                                   // draws the pictures into the reserved images; the pointer starts hidden, as an arrow, at 0,0
bool cursors_set( int shape );                                  // the shape of the pointer (CUR_*); keeps its hot spot where it was; false if the shape does not exist
int  cursors_shape( void );
void cursors_move( int x, int y );                              // the hot spot to x, y
void cursors_position( int* x, int* y );                        // where the hot spot is
void cursors_show( bool on );
bool cursors_visible( void );
void cursors_hotspot( int shape, int* hx, int* hy );            // the hot spot of a shape, relative to the top left corner of its picture
int  cursors_find( const char* name, size_t len );              // a shape by its name ("ARROW", "WAIT", "BUSY", "TEXT", "HAND", "GRAB", "CROSS", "SIZEH", "SIZEV", "MOVE", "NO", "HELP", case does not matter), -1 if none
const char* cursors_name( int shape );                          // "" for a bad number
int  cursors_images( int shape, int* first, int* count );       // the images of a shape (numbers of the engine's half of the sprite map); returns the animation speed (0 = not animated)

#endif
