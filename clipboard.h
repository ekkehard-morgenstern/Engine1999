#pragma once
#ifndef CLIPBOARD_H
#define CLIPBOARD_H

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

#include "stdtypes.h"

/*
    The clipboard of the editors (no SDL, no terminal): an internal clipboard that every host has (so that copy and paste work without any system
    clipboard), plus conversions. The text is ISO-8859-15, lines end with LF. The window host mirrors it to the system clipboard (SDL, in
    sdlscreen.c); the console host writes it to the terminal's with the OSC 52 sequence, and a bracketed paste of the terminal sets it.
*/

// the internal clipboard: set copies the text; get returns a malloc'ed copy (0 if empty or out of memory), *len its length
void clip_internal_set( const char* text, size_t len );
char* clip_internal_get( size_t* len );

// base 64 of len bytes into out (4 * ((len + 2) / 3) + 1 bytes); returns the length
size_t clip_base64( const unsigned char* in, size_t len, char* out );

// normalizes pasted text: CR LF and CR become LF, a TAB becomes a blank, other control codes and a final LF are dropped; works in place and
// returns the new length
size_t clip_normalize( char* text, size_t len );

#endif
