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

#ifndef SPRPATH_H
#define SPRPATH_H       1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

// Sprite paths (2026-11-05, roadmap item 52): a string of commands that the screen thread runs for a sprite, one step per frame, like a PLAY string runs
// per note. This module only translates the string into a list of commands (no screen, no threads); sprscreen.c runs them.
//
// Commands (letters in either case, blanks between commands are ignored, numbers are decimal):
//   X+n  X-n  Y+n  Y-n      move by n pixels over T frames (T n, default 1); with *k: n pixels per frame for k frames (X+3*20)
//   X=n  Y=n                set the coordinate at once
//   M dx,dy [*k]            move in both directions (the same rules as X and Y)
//   W n                     wait n frames
//   A a,len,speed           play animation sequence a (SPRITE n,ANIM)       D i   show the fixed image i
//   H  S                    hide / show the sprite                          P n   the priority 0..7
//   E n                     event n (0..255): ON SPRITE / SPRITE("EVENT")   T n   the default number of frames of a move (1..65535)
//   [ ... ]n                repeat the commands between the brackets n times (nested up to SPRPATH_NEST deep)
//   R                       start again from the beginning (forever)
// At the end of the string the path is finished; the sprite stays where it is.

#define SPRPATH_MAX     4096            // commands per path
#define SPRPATH_NEST    4               // nesting of [ ]

enum {
    SPO_MOVE = 1,                       // a, b = total dx, dy; c = frames (>= 1)
    SPO_SETX, SPO_SETY,                 // a = value
    SPO_WAIT,                           // c = frames (>= 1)
    SPO_ANIM,                           // a = sequence, b = length (0..15), c = speed (0..31)
    SPO_IMAGE,                          // a = image 0..255
    SPO_EVENT,                          // a = event 0..255
    SPO_SHOW, SPO_HIDE,
    SPO_PRIO,                           // a = 0..7
    SPO_LOOP,                           // "[": b = nesting depth
    SPO_ENDLOOP,                        // "]n": a = target (the command after the "["), b = nesting depth, c = n
    SPO_REPEAT                          // "R"
};

typedef struct _sprpath_cmd_t {
    uint8_t     op;
    int32_t     a, b, c;
} sprpath_cmd_t;

typedef struct _sprpath_t {
    uint32_t        n;
    sprpath_cmd_t   cmd[1];             // (n entries)
} sprpath_t;

// translates a path string; 0 on an error (the text is in err, the byte position in *errpos), an empty path (no commands) is a valid one
sprpath_t* sprpath_parse( const char* s, size_t len, char* err, size_t errsize, size_t* errpos );
void       sprpath_free( sprpath_t* p );

#endif
