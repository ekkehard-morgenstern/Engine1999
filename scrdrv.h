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



#ifndef SCRDRV_H
#define SCRDRV_H    1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

#ifndef KEYSEQ_H
#include "keyseq.h"
#endif

/*
The screen driver of the editors (no SDL, no terminal): the full-screen editor (fulledit.c), the monitor (monitor.c) and the one-line editor
(scredit.c) draw on text screens and read keys only through this table, so that the window version (sdlscreen.c) and the console version
(engine1999c.c, conscr.c) run the same code and behave the same (a standing rule of the project: editing and input are consistent in both hosts).

The text screen calls are those of textscreen.h (viewports, colors as palette entries, coordinates relative to the viewport). The size of
the screen is asked for with screensize(): the window's is always 80 x 25, a terminal's is whatever it is now, and it changes when the user
resizes the terminal: nextkey() then returns SK_RESIZE (the screens have been resized already, with their top left parts kept; the editors lay
themselves out again).
*/

typedef enum _scrkind_t {
    SK_NONE = 0,
    SK_CHAR,            // a character: ch (ISO-8859-15, printable); with KSMOD_CTRL: a letter (lower case) typed with Ctrl
    SK_RETURN,
    SK_ESCAPE,          // a single ESC
    SK_TAB,
    SK_BACKSPACE,
    SK_KEY,             // a cursor, editing or function key: key (KS_*), mods (KSMOD_*)
    SK_RESIZE,          // the screen has a new size
    SK_CLOSED,          // the screen is gone: the editor must return
    SK_BREAK,           // Ctrl+C or PAUSE/BREAK: the break key (the editors treat it like ESC; INPUT is stopped by it even after BREAK OFF)
    SK_EOF              // the end of the input (a redirected input, Ctrl-D on a terminal is a key of its own): a line typed so far is complete
} scrkind_t;

typedef struct _scrkey_t {
    int     kind;
    int     ch;
    int     key;
    int     mods;
} scrkey_t;

typedef struct _scrdrv_t {
    // geometry
    void    (*screensize)( int* cols, int* rows );              // the whole text screen
    void    (*getsize)( int* cols, int* rows );                 // the viewport of the selected screen
    // text screens (see textscreen.h)
    int     (*select)( int screen );
    int     (*selected)( void );
    bool    (*setviewport)( int x, int y, int w, int h );
    void    (*resetviewport)( void );
    void    (*getviewport)( int* x, int* y, int* w, int* h );
    void    (*setcolors)( int bg, int fg );
    void    (*setblank)( int bg, int fg );
    void    (*clear)( void );
    int     (*write)( int y, int x, int bg, int fg, const char* text, int len );
    void    (*print)( int y, int x, int bg, int fg, const char* text );     // prints at the cursor (-1: where it is), scrolling the viewport
    void    (*locate)( int x, int y );
    void    (*getcursor)( int* x, int* y );
    void    (*changed)( void );                                 // the screen was changed: show it
    void    (*reserve)( int rows );                             // a resize keeps the cursor row of the selected screen above its last rows (window: nothing)
    int     (*restart)( int absy, int startx, size_t pos );     // after SK_RESIZE: where (screen row) a line that started at row absy, column startx and has its cursor at pos starts now
    // keys
    int     (*begin)( void );                                   // the editors read keys: ESC is a key, not the break request; returns the state for end()
    void    (*end)( int state );
    bool    (*nextkey)( scrkey_t* key );                        // waits for a key (or a resize); false: the screen is closed (SK_CLOSED is in key)
    bool    (*pending)( void );                                 // more keys are waiting (the editors redraw once for a burst)
    void    (*pollkeys)( bool enable );                         // keys are queued for a running program; ESC / Ctrl-C then break it (the monitor runs programs)
} scrdrv_t;

// -- the one-line editor (scredit.c) ---------------------------------------------------------

#define SCREDIT_ACCEPTED    0
#define SCREDIT_CANCELLED   1
#define SCREDIT_TOOLONG     2       // the text doesn't fit on the screen: nothing was done
#define SCREDIT_CLOSED      3       // the screen was closed
#define SCREDIT_BREAK       5       // Ctrl+C / PAUSE/BREAK: the host stops what it runs (INPUT: Break; the prompt: the line is dropped)
#define SCREDIT_EOF         4       // the end of the input: Ctrl-D on an empty line, or the end of a redirected input (the console host ends; the window ignores it)

// the editing state of a line (buf holds maxlen characters + NUL)
typedef struct _scredit_t {
    char*   buf;
    size_t  len, pos, maxlen;
} scredit_t;

// the editing keys shared by the one-line editor and the full-screen editor: printable characters (inserted), LEFT, RIGHT, HOME, END,
// BACKSPACE, DELETE; returns true if the text or the position changed
bool scr_editkey( scredit_t* ed, const scrkey_t* key );

// the printable characters of ISO-8859-15
#define SCR_PRINTABLE(c)    ( ( (c) >= 32 && (c) < 127 ) || ( (c) >= 160 && (c) <= 255 ) )

// edits one line on the screen from the cursor on (the line of the prompt, of INPUT, of EDIT n and of the monitor: one editor for all of them, in every host): buf holds the text to edit and receives the result (bufsz includes the NUL). The line may wrap
// over several rows and is redrawn after every change; RETURN accepts, ESC cancels (buf stays; the host decides what that means: nothing at the prompt, Break at INPUT), Ctrl-D on an empty line is EOF, the cursor is placed on a new line afterwards.
// onresize (may be 0) is called after the screen was resized, so that the caller can set its viewport and bars again; the line is then
// redrawn where it was.
int scr_lineedit( const scrdrv_t* drv, char* buf, size_t bufsz, void (*onresize)( void* ), void* usrdata );

#endif
