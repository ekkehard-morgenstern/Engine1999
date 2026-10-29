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

#ifndef SDLSCREEN_H
#define SDLSCREEN_H     1

#ifndef SDLTYPES_H
#include "sdltypes.h"
#endif

#ifndef TILESCREEN_H
#include "tilescreen.h"
#endif

#ifndef SPRSCREEN_H
#include "sprscreen.h"
#endif

#define SDL_SCREENWIDTH     640
#define SDL_SCREENHEIGHT    300

#define SDLSCR_INPUT_KEYPRESS       0
#define SDLSCR_INPUT_TEXT           1

typedef struct _sdlscr_inputmsg_t {
    int     inputtype;
    union {
        struct {
            SDL_Keycode symbol;
            Uint16      mods;       // SDL_Keymod bits (KMOD_CTRL, ...) at the time of the key press
        };
        char            text[SDL_TEXTINPUTEVENT_TEXT_SIZE];
    };
} sdlscr_inputmsg_t;

bool sdlscr_init( void );
void sdlscr_cleanup( void );
bool sdlscr_term( void );
uint64_t sdlscr_getnsec( struct timespec* pts );
uint64_t sdlscr_getframecnt( void );
sdlscr_inputmsg_t* sdlscr_dequeueinputmsg( void );
void sdlscr_deleteinputmsg( sdlscr_inputmsg_t* msg );     // frees a message returned by sdlscr_dequeueinputmsg()
void sdlscr_enableinput( bool enable );

// keyboard access for running programs (INKEY$, GET); the calling thread is the main thread, the keys come from the
// screen thread. ESC is not a key then: it is the break request.
void sdlscr_pollkeys( bool enable );        // queue keys (without the cursor) while a program runs; clears the break request
int sdlscr_pollkey( void );                 // the next key code, 0 = none
int sdlscr_getkey( void );                  // waits for a key; -1 = screen closed or break requested
bool sdlscr_breakrequested( void );         // ESC was pressed (or the screen was closed) since sdlscr_pollkeys( true )
// key codes: printable ASCII as is, 13 = RETURN, 8 = BACKSPACE, 9 = TAB

// text screen functions
void sdlscr_printf( int y, int x, int bg, int fg, const char* fmt, ... );
void sdlscr_puts( const char* text );        // prints text of any length at the cursor, in the current colors
void sdlscr_ansi_printf( const char* fmt, ... );     // printf-style sdlscr_ansi_puts (any length)
void sdlscr_ansi_puts( const char* text );   // like sdlscr_puts, but ESC [ ... m (SGR) sets the colors (see ansi.h); no lock
// simple line editor for one line (EDIT): buf holds the text to edit and receives the result (bufsz includes the
// terminating NUL). The editor starts at the cursor and redraws the line (which may wrap over several screen rows) after
// every change. Keys: printable characters (inserted), LEFT, RIGHT, HOME, END, BACKSPACE, DELETE; RETURN accepts, ESC
// cancels (buf is then unchanged). The cursor is placed on a new line afterwards. No lock, like all text screen output.
#define SDLSCR_EDIT_ACCEPTED    0
#define SDLSCR_EDIT_CANCELLED   1
#define SDLSCR_EDIT_TOOLONG     2       // the text doesn't fit on the screen: nothing was done
#define SDLSCR_EDIT_CLOSED      3       // the screen was closed
// the editing state of a line (buf holds maxlen characters + NUL) and the keys shared by both editors: printable
// characters (inserted), LEFT, RIGHT, HOME, END, BACKSPACE, DELETE; returns true if the text or the position changed
typedef struct _sdlscr_edit_t {
    char*   buf;
    size_t  len, pos, maxlen;
} sdlscr_edit_t;
bool sdlscr_editkey( sdlscr_edit_t* ed, const sdlscr_inputmsg_t* msg );
bool sdlscr_editmode_begin( void );             // keys are queued, ESC is a key; returns the state for _end
void sdlscr_editmode_end( bool oldstate );
void sdlscr_textchanged( void );                // the text screen was changed directly through txtscr_*: redraw it
int sdlscr_lineedit( char* buf, size_t bufsz, void (*vblank_handler)( void* ), void* usrdata );
int sdlscr_lineinput( int stop_evtmsk, char* buf, size_t bufsz, void (*vblank_handler)( void* ), void* usrdata );
void sdlscr_gettextscreensize( int* outsx, int* outsy );
void sdlscr_gettextcursor( int* outx, int* outy );
void sdlscr_locate( int x, int y );

// tile screen functions
void sdlscr_writetile( int tileno, const uint8_t data[TILE_WIDTH * TILE_HEIGHT]);
void sdlscr_scrolltiles( int sx, int sy );

// sprite screen functions
void sdlscr_showsprite( int sprno, bool active );
void sdlscr_spriteprio( int sprno, int prio );
void sdlscr_movesprite( int sprno, int x, int y );
void sdlscr_spriteanimdata( int animno, const uint8_t* seq, size_t size );
void sdlscr_spriteanimcfg( int sprno, int animno, int length, int speed );
void sdlscr_writesprite( int sprno, const uint8_t data[SPRITE_WIDTH * SPRITE_HEIGHT]);

#endif
