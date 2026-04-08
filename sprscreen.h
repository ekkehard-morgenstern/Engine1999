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

#ifndef SPRSCREEN_H
#define SPRSCREEN_H     1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

#define SPRMAP_WIDTH    256
#define SPRMAP_HEIGHT   384
#define SPRMAP_CELLSX   16
#define SPRMAP_CELLSY   16
#define MAX_SPRIMAG     256

#define SPRTGT_WIDTH    640
#define SPRTGT_HEIGHT   300

#define SPRITE_WIDTH    16
#define SPRITE_HEIGHT   24

#define MAX_SPRITES     256
#define MAX_ANIMSEQ     256
#define MAX_SPRANIM     16

void sprscr_init( void );
void sprscr_show( int sprno, bool active );
void sprscr_prio( int sprno, int prio );
void sprscr_move( int sprno, int x, int y );
void sprscr_animdata( int animno, const uint8_t* seq, size_t size );
void sprscr_animcfg( int sprno, int animno, int length, int speed );
void sprscr_writemap( int sprno, const uint8_t data[SPRITE_WIDTH * SPRITE_HEIGHT]);
bool sprscr_changed( void );
void sprscr_periodicals( void );
void sprscr_render( uint8_t* target );

#endif
