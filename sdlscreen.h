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
        SDL_Keycode     symbol;
        char            text[SDL_TEXTINPUTEVENT_TEXT_SIZE];
    };
} sdlscr_inputmsg_t;

bool sdlscr_init( void );
void sdlscr_cleanup( void );
bool sdlscr_term( void );
uint64_t sdlscr_getnsec( struct timespec* pts );
uint64_t sdlscr_getframecnt( void );
sdlscr_inputmsg_t* sdlscr_dequeueinputmsg( void );
void sdlscr_enableinput( bool enable );

// text screen functions
void sdlscr_printf( int y, int x, int bg, int fg, const char* fmt, ... );
int sdlscr_lineinput( int stop_evtmsk, char* buf, size_t bufsz, void (*vblank_handler)( void* ), void* usrdata );

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
