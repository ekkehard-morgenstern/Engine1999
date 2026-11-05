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

#ifndef SPRSCREEN_H
#define SPRSCREEN_H     1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

#include "sprpath.h"

#define SPRMAP_WIDTH    256
#define SPRMAP_HEIGHT   768         // 32 rows of cells: the first 16 rows hold the 256 images of the program, the other 16 the 256 images of the engine
#define SPRMAP_CELLSX   16
#define SPRMAP_CELLSY   32
#define MAX_SPRIMAG     256         // the images the program can reach: 0..255
#define SPRMAP_SYSBASE  256         // the index of the first image of the engine (a cell of the second half of the map)

#define SPRTGT_WIDTH    640
#define SPRTGT_HEIGHT   300

#define SPRITE_WIDTH    16
#define SPRITE_HEIGHT   24

#define MAX_SPRITES     256
#define MAX_ANIMSEQ     256
#define MAX_SPRANIM     16

// Reserved sprites (2026-11-01): the sprites 248..255 belong to the engine. A program can't reach them: `SPRITE n` with n > 247 is "Illegal argument", the
// functions below with such a sprite number do nothing (the engine uses the sprscr_sys_* ones), and they have images and animation sequences of their own
// (the second half of the image map, MAX_SYSANIM sequences). They are drawn in front of every sprite of the program, don't take part in collisions
// and are left alone by sprscr_reset() (so GFX RESET does not remove the mouse pointer). See cursors.h for the mouse pointer.
#define SPR_USER_COUNT  248         // the sprites of the program: 0..247
#define SPR_SYS_FIRST   248         // the first reserved sprite
#define SPR_MOUSE       255         // the mouse pointer (cursors.h); 248..254 are free for future use
#define MAX_SYSANIM     16

// Sprite groups (2026-11-05, roadmap item 62): SPRITE n,SIZE w,h makes the sprite n the head of a w x h block of sprites: the sprites n .. n+w*h-1 (row by
// row) are drawn side by side as one large sprite. The head's position, priority, shown / hidden state, animation, MOVETO and collision flags rule the
// block: member k (column k % w, row k / w) is drawn at the head's position + (16 * column, 24 * row) and shows the head's image + k. A member's own
// settings are ignored while it belongs to a group. The block collides as one object (the head's number is reported); the members don't collide
// with each other. w, h 1..8, w * h <= 64; (1, 1) dissolves the group.
#define SPR_GROUP_MAX   8
bool sprscr_setsize( int head, int w, int h );              // false: bad numbers, the block does not fit below 248 or touches another group
void sprscr_getsize( int head, int* w, int* h );            // 1, 1 for a sprite that is no head
int  sprscr_headof( int sprno );                            // the head of the group the sprite belongs to (itself if it is none), -1 for a bad number

void sprscr_init( void );
void sprscr_show( int sprno, bool active );
void sprscr_prio( int sprno, int prio );
void sprscr_move( int sprno, int x, int y );
void sprscr_animdata( int animno, const uint8_t* seq, size_t size );
void sprscr_animcfg( int sprno, int animno, int length, int speed );
void sprscr_writemap( int imgno, const uint8_t data[SPRITE_WIDTH * SPRITE_HEIGHT]);
bool sprscr_changed( void );
void sprscr_reset( bool images );          // the sprites of the program (not the reserved ones) hidden, in the default state and at 0,0, no movement, no collision flags; images: the images and animation sequences of the program are cleared as well
// the reserved sprites (SPR_SYS_FIRST..MAX_SPRITES-1), for the engine only; images are numbered from 0 (the cell SPRMAP_SYSBASE + n of the map)
void sprscr_sys_writemap( int sysimg, const uint8_t data[SPRITE_WIDTH * SPRITE_HEIGHT] );       // an image of the engine, sysimg 0..255
void sprscr_sys_animdata( int seq, const uint8_t* images, size_t size );                          // a sequence of the engine (seq 0..MAX_SYSANIM-1, 1..15 images)
void sprscr_sys_setimage( int sprno, int sysimg );                                                // a fixed image
void sprscr_sys_animcfg( int sprno, int seq, int length, int speed );                             // plays a sequence
void sprscr_sys_show( int sprno, bool active );
void sprscr_sys_move( int sprno, int x, int y );
bool sprscr_sys_isactive( int sprno );
bool sprscr_sys_readmap( int sysimg, uint8_t out[SPRITE_WIDTH * SPRITE_HEIGHT] );                  // an image of the engine, read back (the gallery and the tests)
bool sprscr_sys_getpos( int sprno, int* x, int* y );
int  sprscr_sys_getimage( int sprno );                                                            // the image shown now (0..255 of the engine), -1 for a bad number
void sprscr_setimage( int sprno, int image );               // a fixed image (until sprscr_animcfg)
int  sprscr_getimage( int sprno );                          // the image shown now, -1 for a bad sprite number
bool sprscr_getpos( int sprno, int* x, int* y );
bool sprscr_isactive( int sprno );
bool sprscr_readmap( int imgno, uint8_t out[SPRITE_WIDTH * SPRITE_HEIGHT] );
void sprscr_moveto( int sprno, int x, int y, int ticks );  // moves by itself, one step per sprscr_periodicals() call
bool sprscr_moving( int sprno );
void sprscr_setcollide( int sprno, bool sprites, bool tiles );
uint32_t sprscr_hits( int sprno );                          // indicator bits (1 sprite, 2 tile), cleared by reading
int  sprscr_collidewith( int sprno );                       // the other sprite of the last overlap found, -1 none
int  sprscr_nextevent( void );                              // a sprite with a new collision since the last call, -1 none
// Sprite paths (roadmap item 52, see sprpath.h): the screen thread runs a translated path for the sprite, one step per sprscr_periodicals() call.
void sprscr_setpath( int sprno, sprpath_t* path );  // takes over the path (0 or an empty one stops the running path); a new path replaces the old at once
bool sprscr_pathrunning( int sprno );
int  sprscr_pathevent( void );                              // takes the oldest event of a path (E n): its number 0..255, -1 none; the sprite is next
int  sprscr_pathevent_sprite( void );                       // the sprite of the event taken last, -1 none
uint32_t sprscr_pathevents( void );                         // the number of events so far (a change is an event; the VM polls it for ON SPRITE)
void sprscr_periodicals( void );
void sprscr_render( uint8_t* target );

#endif
