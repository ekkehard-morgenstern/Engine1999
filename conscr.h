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
void conscr_text_setcolors( int bg, int fg );           // COLOR: the pen and paper of the selected screen (the decoder of PRINT starts again from them)
void conscr_dump( void );                               // the BASIC screen (screen 0) as plain lines on standard output, trailing blanks and rows cut (call after conscr_end())

// the palette entry nearest to an xterm 256 color number's RGB, and the other way: used by the painter (exposed for the test)
int conscr_rgb_to_256( int r, int g, int b );

// -- line mode: the normal terminal as one screen for the one-line editor ---------------------------------
//
// The one-line editor (scredit.c) of the prompt and of INPUT needs absolute positions on a screen. On the normal, scrolling terminal the host asks where
// the cursor is when a line starts (ESC [ 6 n) and the functions below keep a model of it (autowrap, scrolling at the bottom), so that the editor draws and
// wipes with absolute cursor addressing and the line behaves as it does on the window's text screen. The screen's contents and colors are not touched.

void conscr_line_init( int cols, int rows, int cx, int cy );        // the size and the cursor position (0-based) now
void conscr_line_setsilent( bool silent );                          // write nothing (the input or the output is redirected): the editor still runs, on the model only
void conscr_line_getsize( int* cols, int* rows );
void conscr_line_getviewport( int* x, int* y, int* w, int* h );
void conscr_line_getcursor( int* x, int* y );
void conscr_line_locate( int x, int y );
int conscr_line_write( int y, int x, int bg, int fg, const char* text, int len );   // blanks and the like: positioned, clipped at the right edge, no wrap
void conscr_line_print( int y, int x, int bg, int fg, const char* text );           // text at the cursor (or at x, y): wraps, scrolls at the bottom
void conscr_line_changed( void );
int conscr_line_restart( int absy, int startx, size_t pos );        // after a resize (the new size and cursor are set by conscr_line_init()): the row the line starts at
void conscr_termsize( int* cols, int* rows );                       // the terminal's size now

#endif
