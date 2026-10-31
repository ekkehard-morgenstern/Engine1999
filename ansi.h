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

    NOTE: Programs created with a built-in programming language (if any),
          do not fall under this license.

    CONTACT INFO:
        E-Mail: ekkehard@ekkehardmorgenstern.de
        Mail: Ekkehard Morgenstern, Mozartstr. 1, D-76744 Woerth am Rhein,
              Germany, Europe
*/

#ifndef ANSI_H
#define ANSI_H      1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

/*
Decoder for ANSI escape sequences in output text. Only SGR ("set graphic rendition", ESC [ params m) is acted upon,
and only the color parts of it; every other escape sequence is skipped. The decoder keeps the current colors and the
state of a sequence that was cut off at the end of a text, so text can be fed in pieces. It has no screen dependency:
the plain text between the sequences is handed to a callback together with the colors in effect.

    0               default colors
    30..37, 90..97  foreground: the 8 standard / 8 bright colors
    40..47, 100..107 background: likewise
    38;5;n, 48;5;n  foreground / background: palette entry n (0..255) of the engine's 256 color palette
    39, 49          default foreground / background
    38;2;r;g;b ...  (true color) is skipped, the colors stay as they are
    1, 22, 7, ...   other attributes are ignored

The 16 standard colors are mapped to entries of the engine palette (colinittab in sdllayer.c) in ansi.c.
*/

#define ANSI_MAXPARAMS  16          // more parameters of one sequence are ignored
#define ANSI_RUNMAX     255         // the longest text run handed to the callback

// a run of text (len bytes; it may contain zero bytes, which are glyphs, too) in the colors bg / fg
typedef void (*ansi_emit_t)( void* usrdata, int bg, int fg, const char* text, size_t len );

typedef struct _ansi_t {
    int         deffg, defbg;       // colors of "default" (palette entries)
    int         fg, bg;             // current colors (palette entries)
    int         state;              // ANSI_ST_*
    int         params[ANSI_MAXPARAMS];
    int         nparams;            // parameters stored so far (the one being read included once it has a digit or ';')
    bool        priv;               // the sequence has a private marker (? < = >): not SGR
} ansi_t;

void ansi_init( ansi_t* a, int deffg, int defbg );
void ansi_feed( ansi_t* a, const char* text, ansi_emit_t emit, void* usrdata );                    // text ends at the first zero byte
void ansi_feed_n( ansi_t* a, const char* text, size_t n, ansi_emit_t emit, void* usrdata );       // n bytes, zero bytes included

#endif
