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



#ifndef CONSCR_H
#define CONSCR_H    1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

/*
The terminal as a screen for the console host's full-screen editor and monitor (no curses: ANSI escape sequences written by us, no library).
The text screens of textscreen.c are the screen buffers (screen 1: the editor, screen 2: the monitor, as in the window); the terminal shows
the selected one. Entering full-screen mode switches the terminal to its alternate screen, so the shell's text is back when it is left. The
screens are as big as the terminal and follow its size: conscr_syncsize() reads it (the host calls it after SIGWINCH).

Painting is incremental: what the terminal shows is remembered and only the cells that differ are written. Colors are palette entries of
the engine (palette.h); a terminal that says COLORTERM=truecolor or 24bit gets them exactly, any other the nearest of the 256 colors.
Characters are ISO-8859-15, written as UTF-8; control codes are drawn as '?'.
*/

bool conscr_begin( void );              // full-screen mode on the terminal (needs standard output to be a terminal); false if not possible
void conscr_end( void );                // back to the normal screen
bool conscr_active( void );
bool conscr_syncsize( void );           // the terminal's size is read and the screens are resized to it; true if it changed
void conscr_paint( void );              // shows the selected screen (and the cursor) on the terminal
void conscr_write( const char* text, size_t len );      // output of a running program while the screen is active: text with ANSI colors, printed to the selected screen

// the palette entry nearest to an xterm 256 color number's RGB, and the other way: used by the painter (exposed for the test)
int conscr_rgb_to_256( int r, int g, int b );

#endif
