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

#include "sprscreen.h"
#include "tilescreen.h"
#include "cursors.h"

typedef struct _sprctrl_t {
    // The animation counters are the only fields the screen thread changes (sprscr_periodicals), so they are bytes of their own: a bitfield write is a
    // read-modify-write of the whole word, which would undo what the main thread writes to its neighbours (active, priority, ...) at the same moment.
    // Everything below is written by the main thread only (the screen thread reads it).
    uint8_t anim_pos;               // 0..14
    uint8_t anim_speed_count;       // 0..31
    unsigned int
        anim_index       : 8,
        anim_size        : 4,
        anim_speed_reset : 5,
        priority         : 3,
        active           : 1,
        spr_spr_coll_ena : 1;
    unsigned int
        spr_til_coll_ena : 1,
        use_image        : 1,   // a fixed image instead of the animation sequence (SPRITE n,IMAGE i)
        image            : 8;
    signed int
        screen_x         : 11,
        screen_y         : 11,  // 24
                         : 0;   // 32
} sprctrl_t;

static sprctrl_t    sprctrl[MAX_SPRITES];
static uint8_t      spranim[MAX_ANIMSEQ][MAX_SPRANIM];
static uint8_t      sprsysanim[MAX_SYSANIM][MAX_SPRANIM];      // the animation sequences of the engine (image numbers of the engine's half of the map)
static uint8_t      sprmap[SPRMAP_WIDTH * SPRMAP_HEIGHT];
static bool         sprchg = false;

/*
Threads: the main thread (BASIC) and the screen thread (sprscr_periodicals(), sprscr_render()) use this module without locks, on purpose: the layer
memory may be torn (a frame with a few wrong pixels is harmless), but nothing may be *lost*. So (1) a word that both threads write with
read-modify-write is never shared: the collision indicators are a byte array of their own (sprhit), set with an atomic OR by the screen thread and
taken with an atomic exchange by the main thread; (2) the event slot is a compare-exchange / exchange; (3) the MOVETO engine belongs to the screen
thread alone: the main thread only posts a request word (sprreq, one atomic 64-bit store, the last request wins), the screen thread picks it up;
(4) the "changed" flag is cleared before the data is read (atomic exchange) and set after the data was written (release store).
*/
#define SPRREQ_VALID    UINT64_C(0X8000000000000000)    // a request is posted
#define SPRREQ_CANCEL   UINT64_C(0X4000000000000000)    // stop the movement here
// ticks in bits 32..47 (0 = jump to the target at once), the target x in bits 16..31 and y in bits 0..15 (int16)

static uint64_t     sprreq[MAX_SPRITES];            // the main thread's request, 0 = none
static uint8_t      sprhit[MAX_SPRITES];            // collision indicators: bit 0 a sprite was hit, bit 1 a tile (read and cleared by SPRITE n,COLLISION)

static inline void sprscr_touch( void ) {
    __atomic_store_n( &sprchg, true, __ATOMIC_RELEASE );
}

// SPRITE n,MOVETO: the position moves by itself, one step per frame (sprscr_periodicals); owned by the screen thread
typedef struct _sprmove_t {
    bool    active;
    int32_t fx, fy;             // 16.16 fixed point position
    int32_t dx, dy;             // step per frame
    int     ticks;              // frames to go
    int     tx, ty;             // the target
} sprmove_t;

static sprmove_t    sprmove[MAX_SPRITES];

// Sprite groups: sprgrp_w / sprgrp_h are set for a head (0 = none), sprgrp_head for a member (SPRGRP_NONE = not a member). Written by the main thread
// only; the screen thread reads them without locks, so every use is bounded (sprscr_view() clamps what it reads).
#define SPRGRP_NONE     0xFFU
static uint8_t      sprgrp_w[MAX_SPRITES];
static uint8_t      sprgrp_h[MAX_SPRITES];
static uint8_t      sprgrp_head[MAX_SPRITES];
static int          sprcollwith[MAX_SPRITES];       // the sprite a sprite last collided with (-1 none)
static int          sprevent = -1;                  // a sprite with a new collision (for ON COLLISION), -1 none

// the image a sprite shows now: 0..255 for the sprites of the program, SPRMAP_SYSBASE + 0..255 for the reserved ones (their own images and sequences)
static int sprscr_curimage( const sprctrl_t* ctrl ) {
    int n = (int)( ctrl - &sprctrl[0] );
    if ( n >= SPR_SYS_FIRST ) {
        int rel = ctrl->use_image ? (int) ctrl->image : sprsysanim[ ctrl->anim_index & ( MAX_SYSANIM - 1 ) ][ ctrl->anim_pos & 15U ];
        return SPRMAP_SYSBASE + rel;
    }
    return ctrl->use_image ? (int) ctrl->image : spranim[ ctrl->anim_index ][ ctrl->anim_pos & 15U ];
}

// What a sprite looks like on the screen now: for the member of a group the head's state and the member's place in the block
typedef struct _sprview_t {
    bool    active;
    int     x, y;           // the position of its top left corner
    int     image;          // the image shown (0..255; SPRMAP_SYSBASE + n for the reserved sprites)
    int     prio;
    bool    coll_spr, coll_til;
    int     head;           // the sprite that reports the hits and rules the block (the sprite itself if it is alone)
} sprview_t;

static void sprscr_view( int n, sprview_t* v ) {
    const sprctrl_t* c = &sprctrl[n];
    int head = n, col = 0, row = 0, w = 1;
    if ( n < SPR_USER_COUNT && sprgrp_head[n] != SPRGRP_NONE ) {
        int hd = sprgrp_head[n];
        int gw = sprgrp_w[hd], gh = sprgrp_h[hd];
        if ( hd < n && gw >= 1 && gw <= SPR_GROUP_MAX && gh >= 1 && gh <= SPR_GROUP_MAX && n - hd < gw * gh ) {
            head = hd;
            w = gw;
            col = ( n - hd ) % gw;
            row = ( n - hd ) / gw;
        }
    }
    const sprctrl_t* hc = &sprctrl[head];
    int img = sprscr_curimage( hc );
    if ( head != n ) {
        img += row * w + col;
        if ( img > 255 ) {
            img = 255;
        }
    }
    v->active   = hc->active;
    v->x        = hc->screen_x + col * SPRITE_WIDTH;
    v->y        = hc->screen_y + row * SPRITE_HEIGHT;
    v->image    = img;
    v->prio     = hc->priority;
    v->coll_spr = hc->spr_spr_coll_ena;
    v->coll_til = hc->spr_til_coll_ena;
    v->head     = head;
    (void) c;
}

int sprscr_headof( int sprno ) {
    if ( sprno < 0 || sprno >= MAX_SPRITES ) {
        return -1;
    }
    sprview_t v;
    sprscr_view( sprno, &v );
    return v.head;
}

void sprscr_getsize( int head, int* w, int* h ) {
    *w = *h = 1;
    if ( head >= 0 && head < SPR_USER_COUNT && sprgrp_w[head] * sprgrp_h[head] > 1 ) {
        *w = sprgrp_w[head];
        *h = sprgrp_h[head];
    }
}

bool sprscr_setsize( int head, int w, int h ) {
    if ( head < 0 || head >= SPR_USER_COUNT || w < 1 || w > SPR_GROUP_MAX || h < 1 || h > SPR_GROUP_MAX || head + w * h > SPR_USER_COUNT ) {
        return false;
    }
    if ( sprgrp_head[head] != SPRGRP_NONE ) {
        return false;                                       // a member of a group can't be a head
    }
    int ow = sprgrp_w[head], oh = sprgrp_h[head];
    int n = w * h;
    for ( int k = 1; k < n; ++k ) {                         // the members must be free (or already ours)
        int m = head + k;
        if ( ( sprgrp_head[m] != SPRGRP_NONE && sprgrp_head[m] != head ) || sprgrp_w[m] * sprgrp_h[m] > 1 ) {
            return false;
        }
    }
    for ( int k = 1; k < ow * oh; ++k ) {                   // the old group goes away
        sprgrp_head[ head + k ] = SPRGRP_NONE;
    }
    if ( n == 1 ) {
        sprgrp_w[head] = sprgrp_h[head] = 0;
    } else {
        sprgrp_w[head] = (uint8_t) w;
        sprgrp_h[head] = (uint8_t) h;
        for ( int k = 1; k < n; ++k ) {
            sprgrp_head[ head + k ] = (uint8_t) head;
        }
    }
    sprscr_touch();
    return true;
}

void sprscr_init( void ) {
    memset( &sprctrl[0], 0, sizeof(sprctrl_t) * MAX_SPRITES );
    memset( &spranim[0][0], 0, MAX_ANIMSEQ * MAX_SPRANIM );
    memset( &sprsysanim[0][0], 0, MAX_SYSANIM * MAX_SPRANIM );
    memset( &sprmap[0], 0, sizeof(sprmap) );
    memset( &sprctrl[0], 0, sizeof(sprctrl_t) * MAX_SPRITES );
    memset( &sprmove[0], 0, sizeof(sprmove) );
    memset( &sprreq[0], 0, sizeof(sprreq) );
    memset( &sprhit[0], 0, sizeof(sprhit) );
    memset( &sprgrp_w[0], 0, sizeof(sprgrp_w) );
    memset( &sprgrp_h[0], 0, sizeof(sprgrp_h) );
    memset( &sprgrp_head[0], SPRGRP_NONE, sizeof(sprgrp_head) );
    for ( int i=0; i < MAX_SPRITES; ++i ) {
        sprcollwith[i] = -1;
    }
    sprevent = -1;
    cursors_install();                      // the pictures of the mouse pointer (reserved images and sequences)
    sprscr_touch();
}

void sprscr_show( int sprno, bool active ) {
    if ( sprno < 0 || sprno >= SPR_USER_COUNT ) {
        return;
    }
    sprctrl[sprno].active = active;
    sprscr_touch();
}

void sprscr_prio( int sprno, int prio ) {
    if ( sprno < 0 || sprno >= SPR_USER_COUNT ) {
        return;
    }
    sprctrl[sprno].priority = prio;
    sprscr_touch();
}

void sprscr_move( int sprno, int x, int y ) {
    if ( sprno < 0 || sprno >= SPR_USER_COUNT ) {
        return;
    }
    sprctrl[sprno].screen_x = x;
    sprctrl[sprno].screen_y = y;
    sprscr_touch();
}

void sprscr_animdata( int animno, const uint8_t* seq, size_t size ) {
    if ( animno < 0 || animno >= MAX_ANIMSEQ || seq == 0 || size == 0 ) {
        return;
    }
    if ( size > 15U ) {
        size = 15U;
    }
    memcpy( &spranim[animno][0], seq, size );
    sprscr_touch();
}

void sprscr_animcfg( int sprno, int animno, int length, int speed ) {
    if ( sprno < 0 || sprno >= SPR_USER_COUNT || animno < 0 || animno >= MAX_ANIMSEQ ) {
        return;
    }
    if ( length < 0 || length > 15 || speed < 0 || speed > 31 ) {
        return;
    }
    sprctrl_t* ctrl = &sprctrl[sprno];
    ctrl->use_image        = 0;
    ctrl->anim_index       = animno;
    ctrl->anim_pos         = 0;
    ctrl->anim_size        = length;
    ctrl->anim_speed_count = speed;
    ctrl->anim_speed_reset = speed;
    sprscr_touch();
}

void sprscr_writemap( int imgno, const uint8_t data[SPRITE_WIDTH * SPRITE_HEIGHT]) {
    if ( imgno < 0 || imgno > 255 ) return;
    uint8_t mapx = (uint8_t)( imgno % SPRMAP_CELLSX );
    uint8_t mapy = (uint8_t)( imgno / SPRMAP_CELLSX );
    uint16_t mapcx = mapx * SPRITE_WIDTH;
    uint16_t mapcy = mapy * SPRITE_HEIGHT;
    uint32_t mapof = mapcy * SPRMAP_WIDTH + mapcx;
    const uint8_t* s = &data[0];
    uint8_t* d = &sprmap[ mapof ];
    for ( uint8_t y=0; y < (uint8_t) SPRITE_HEIGHT; ++y ) {
        for ( uint8_t x=0; x < (uint8_t) SPRITE_WIDTH; ++x ) {
            *d++ = *s++;
        }
        d += SPRMAP_WIDTH - SPRITE_WIDTH;
    }
    sprscr_touch();
}

// the images of a map cell (shared by the program's and the engine's)
static void sprscr_putcell( int cell, const uint8_t data[SPRITE_WIDTH * SPRITE_HEIGHT] ) {
    uint16_t mapcx = (uint16_t)( ( cell % SPRMAP_CELLSX ) * SPRITE_WIDTH );
    uint16_t mapcy = (uint16_t)( ( cell / SPRMAP_CELLSX ) * SPRITE_HEIGHT );
    uint32_t mapof = (uint32_t) mapcy * SPRMAP_WIDTH + mapcx;
    const uint8_t* s = &data[0];
    uint8_t* d = &sprmap[ mapof ];
    for ( int y=0; y < SPRITE_HEIGHT; ++y ) {
        for ( int x=0; x < SPRITE_WIDTH; ++x ) {
            *d++ = *s++;
        }
        d += SPRMAP_WIDTH - SPRITE_WIDTH;
    }
    sprscr_touch();
}

// -- the reserved sprites (the engine's, not the program's) ---------------------------------------------

void sprscr_sys_writemap( int sysimg, const uint8_t data[SPRITE_WIDTH * SPRITE_HEIGHT] ) {
    if ( sysimg < 0 || sysimg > 255 ) {
        return;
    }
    sprscr_putcell( SPRMAP_SYSBASE + sysimg, data );
}

void sprscr_sys_animdata( int seq, const uint8_t* images, size_t size ) {
    if ( seq < 0 || seq >= MAX_SYSANIM || images == 0 || size == 0 ) {
        return;
    }
    if ( size > 15U ) {
        size = 15U;
    }
    memcpy( &sprsysanim[seq][0], images, size );
    sprscr_touch();
}

void sprscr_sys_setimage( int sprno, int sysimg ) {
    if ( sprno < SPR_SYS_FIRST || sprno >= MAX_SPRITES || sysimg < 0 || sysimg > 255 ) {
        return;
    }
    sprctrl[sprno].use_image = 1;
    sprctrl[sprno].image = (unsigned) sysimg;
    sprscr_touch();
}

void sprscr_sys_animcfg( int sprno, int seq, int length, int speed ) {
    if ( sprno < SPR_SYS_FIRST || sprno >= MAX_SPRITES || seq < 0 || seq >= MAX_SYSANIM || length < 0 || length > 15 || speed < 0 || speed > 31 ) {
        return;
    }
    sprctrl_t* ctrl = &sprctrl[sprno];
    ctrl->use_image        = 0;
    ctrl->anim_index       = seq;
    ctrl->anim_pos         = 0;
    ctrl->anim_size        = length;
    ctrl->anim_speed_count = speed;
    ctrl->anim_speed_reset = speed;
    sprscr_touch();
}

void sprscr_sys_show( int sprno, bool active ) {
    if ( sprno < SPR_SYS_FIRST || sprno >= MAX_SPRITES ) {
        return;
    }
    sprctrl[sprno].active = active;
    sprscr_touch();
}

void sprscr_sys_move( int sprno, int x, int y ) {
    if ( sprno < SPR_SYS_FIRST || sprno >= MAX_SPRITES ) {
        return;
    }
    sprctrl[sprno].screen_x = x;
    sprctrl[sprno].screen_y = y;
    sprscr_touch();
}

bool sprscr_sys_isactive( int sprno ) {
    return sprno >= SPR_SYS_FIRST && sprno < MAX_SPRITES && sprctrl[sprno].active;
}

bool sprscr_sys_readmap( int sysimg, uint8_t out[SPRITE_WIDTH * SPRITE_HEIGHT] ) {
    if ( sysimg < 0 || sysimg > 255 ) {
        return false;
    }
    int cell = SPRMAP_SYSBASE + sysimg;
    uint16_t mapcx = (uint16_t)( ( cell % SPRMAP_CELLSX ) * SPRITE_WIDTH );
    uint16_t mapcy = (uint16_t)( ( cell / SPRMAP_CELLSX ) * SPRITE_HEIGHT );
    for ( int y=0; y < SPRITE_HEIGHT; ++y ) {
        memcpy( &out[ y * SPRITE_WIDTH ], &sprmap[ ( mapcy + y ) * SPRMAP_WIDTH + mapcx ], SPRITE_WIDTH );
    }
    return true;
}

bool sprscr_sys_getpos( int sprno, int* x, int* y ) {
    if ( sprno < SPR_SYS_FIRST || sprno >= MAX_SPRITES ) {
        return false;
    }
    *x = sprctrl[sprno].screen_x;
    *y = sprctrl[sprno].screen_y;
    return true;
}

int sprscr_sys_getimage( int sprno ) {
    if ( sprno < SPR_SYS_FIRST || sprno >= MAX_SPRITES ) {
        return -1;
    }
    return sprscr_curimage( &sprctrl[sprno] ) - SPRMAP_SYSBASE;
}

void sprscr_reset( bool images ) {
    memset( &sprgrp_w[0], 0, SPR_USER_COUNT );                       // (the groups are dissolved)
    memset( &sprgrp_h[0], 0, SPR_USER_COUNT );
    memset( &sprgrp_head[0], SPRGRP_NONE, SPR_USER_COUNT );
    memset( &sprctrl[0], 0, sizeof(sprctrl_t) * SPR_USER_COUNT );      // (the reserved sprites, the mouse pointer, are not the program's to reset)
    for ( int i=0; i < SPR_USER_COUNT; ++i ) {
        // the movement belongs to the screen thread: it is asked to stop (and is told here, too, for a host that has no screen thread)
        __atomic_store_n( &sprreq[i], SPRREQ_VALID | SPRREQ_CANCEL, __ATOMIC_RELEASE );
        __atomic_store_n( &sprmove[i].active, false, __ATOMIC_RELEASE );
        __atomic_store_n( &sprhit[i], 0, __ATOMIC_RELAXED );
        __atomic_store_n( &sprcollwith[i], -1, __ATOMIC_RELAXED );
    }
    __atomic_store_n( &sprevent, -1, __ATOMIC_RELEASE );
    if ( images ) {
        memset( &spranim[0][0], 0, MAX_ANIMSEQ * MAX_SPRANIM );
        memset( &sprmap[0], 0, (size_t) SPRMAP_WIDTH * ( SPRMAP_HEIGHT / 2 ) );     // the program's half of the map; the engine's images stay
    }
    sprscr_touch();
}

bool sprscr_changed( void ) {
    return __atomic_load_n( &sprchg, __ATOMIC_ACQUIRE );
}

void sprscr_setimage( int sprno, int image ) {
    if ( sprno < 0 || sprno >= SPR_USER_COUNT || image < 0 || image > 255 ) {
        return;
    }
    sprctrl[sprno].use_image = 1;
    sprctrl[sprno].image = (unsigned) image;
    sprscr_touch();
}

int sprscr_getimage( int sprno ) {
    if ( sprno < 0 || sprno >= SPR_USER_COUNT ) {
        return -1;
    }
    sprview_t v;
    sprscr_view( sprno, &v );
    return v.image;
}

bool sprscr_getpos( int sprno, int* x, int* y ) {
    if ( sprno < 0 || sprno >= SPR_USER_COUNT ) {
        return false;
    }
    sprview_t v;
    sprscr_view( sprno, &v );
    *x = v.x;
    *y = v.y;
    return true;
}

bool sprscr_isactive( int sprno ) {
    if ( sprno < 0 || sprno >= SPR_USER_COUNT ) {
        return false;
    }
    sprview_t v;
    sprscr_view( sprno, &v );
    return v.active;
}

bool sprscr_readmap( int imgno, uint8_t out[SPRITE_WIDTH * SPRITE_HEIGHT] ) {
    if ( imgno < 0 || imgno > 255 ) {
        return false;
    }
    uint16_t mapcx = (uint16_t)( ( imgno % SPRMAP_CELLSX ) * SPRITE_WIDTH );
    uint16_t mapcy = (uint16_t)( ( imgno / SPRMAP_CELLSX ) * SPRITE_HEIGHT );
    for ( int y=0; y < SPRITE_HEIGHT; ++y ) {
        memcpy( &out[ y * SPRITE_WIDTH ], &sprmap[ ( mapcy + y ) * SPRMAP_WIDTH + mapcx ], SPRITE_WIDTH );
    }
    return true;
}

void sprscr_moveto( int sprno, int x, int y, int ticks ) {
    if ( sprno < 0 || sprno >= SPR_USER_COUNT ) {
        return;
    }
    // the movement is the screen thread's: a request word is posted (one atomic store; a newer request replaces an older one that was not picked up yet)
    if ( ticks < 0 ) ticks = 0;
    if ( ticks > 65535 ) ticks = 65535;
    if ( x < -32768 ) x = -32768;
    if ( x > 32767 ) x = 32767;
    if ( y < -32768 ) y = -32768;
    if ( y > 32767 ) y = 32767;
    uint64_t r = SPRREQ_VALID | ( (uint64_t) (uint32_t) ticks << 32 ) | ( (uint64_t)(uint16_t)(int16_t) x << 16 ) | (uint64_t)(uint16_t)(int16_t) y;
    __atomic_store_n( &sprreq[sprno], r, __ATOMIC_RELEASE );
}

// a movement is going on, or one has been asked for and not picked up yet (a request to stop or to jump is no movement)
bool sprscr_moving( int sprno ) {
    if ( sprno < 0 || sprno >= SPR_USER_COUNT ) {
        return false;
    }
    if ( __atomic_load_n( &sprmove[sprno].active, __ATOMIC_ACQUIRE ) ) {
        return true;
    }
    uint64_t r = __atomic_load_n( &sprreq[sprno], __ATOMIC_ACQUIRE );
    return ( r & SPRREQ_VALID ) != 0 && ( r & SPRREQ_CANCEL ) == 0 && ( ( r >> 32 ) & 0xFFFFU ) > 0;
}

void sprscr_setcollide( int sprno, bool sprites, bool tiles ) {
    if ( sprno < 0 || sprno >= SPR_USER_COUNT ) {
        return;
    }
    sprctrl[sprno].spr_spr_coll_ena = sprites;
    sprctrl[sprno].spr_til_coll_ena = tiles;
    if ( !sprites ) __atomic_fetch_and( &sprhit[sprno], (uint8_t) ~1U, __ATOMIC_ACQ_REL );
    if ( !tiles )   __atomic_fetch_and( &sprhit[sprno], (uint8_t) ~2U, __ATOMIC_ACQ_REL );
}

uint32_t sprscr_hits( int sprno ) {
    if ( sprno < 0 || sprno >= SPR_USER_COUNT ) {
        return 0;
    }
    return __atomic_exchange_n( &sprhit[sprno], 0, __ATOMIC_ACQ_REL );      // read and cleared in one step: a hit that arrives meanwhile is not lost
}

int sprscr_collidewith( int sprno ) {
    return sprno >= 0 && sprno < SPR_USER_COUNT ? __atomic_load_n( &sprcollwith[sprno], __ATOMIC_ACQUIRE ) : -1;
}

int sprscr_nextevent( void ) {
    return __atomic_exchange_n( &sprevent, -1, __ATOMIC_ACQ_REL );
}

// the palette index of a sprite at a screen pixel, 0 where it is transparent or the pixel is outside
static int sprscr_pixel( const sprview_t* v, int x, int y ) {
    int lx = x - v->x, ly = y - v->y;
    if ( lx < 0 || ly < 0 || lx >= SPRITE_WIDTH || ly >= SPRITE_HEIGHT ) {
        return 0;
    }
    int img = v->image;
    return sprmap[ ( ( img / SPRMAP_CELLSX ) * SPRITE_HEIGHT + ly ) * SPRMAP_WIDTH + ( img % SPRMAP_CELLSX ) * SPRITE_WIDTH + lx ];
}

// the first event wins until it is taken (a sprite number, -1 = none)
static void sprscr_postevent( int sprno ) {
    int expected = -1;
    __atomic_compare_exchange_n( &sprevent, &expected, sprno, false, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED );
}

static void sprscr_collisions( void ) {
    int with[MAX_SPRITES];              // (published at the end: a reader never sees "no collision" in between)
    sprview_t vw[SPR_USER_COUNT];
    for ( int i=0; i < MAX_SPRITES; ++i ) {
        with[i] = -1;
    }
    for ( int i=0; i < SPR_USER_COUNT; ++i ) {
        sprscr_view( i, &vw[i] );
    }
    for ( int a=0; a < SPR_USER_COUNT; ++a ) {            // (the reserved sprites don't collide)
        const sprview_t* va = &vw[a];
        if ( !va->active || ( !va->coll_spr && !va->coll_til ) ) {
            continue;
        }
        int ha = va->head;                                  // (the members of a group report through their head)
        if ( va->coll_til ) {
            bool hit = false;
            for ( int j=0; j < SPRITE_HEIGHT && !hit; ++j ) {
                for ( int i=0; i < SPRITE_WIDTH && !hit; ++i ) {
                    int x = va->x + i, y = va->y + j;
                    if ( sprscr_pixel( va, x, y ) != 0 && tilescr_pixel( x, y ) > 0 ) {
                        hit = true;
                    }
                }
            }
            if ( hit ) {
                if ( ( __atomic_fetch_or( &sprhit[ha], 2, __ATOMIC_ACQ_REL ) & 2U ) == 0 ) {
                    sprscr_postevent( ha );
                }
            }
        }
        if ( !va->coll_spr ) {
            continue;
        }
        for ( int b = a + 1; b < SPR_USER_COUNT; ++b ) {
            const sprview_t* vb = &vw[b];
            if ( !vb->active || !vb->coll_spr || vb->head == ha ) {
                continue;
            }
            int hb = vb->head;
            int dx = vb->x - va->x, dy = vb->y - va->y;
            if ( dx <= -SPRITE_WIDTH || dx >= SPRITE_WIDTH || dy <= -SPRITE_HEIGHT || dy >= SPRITE_HEIGHT ) {
                continue;
            }
            bool hit = false;
            for ( int j=0; j < SPRITE_HEIGHT && !hit; ++j ) {
                for ( int i=0; i < SPRITE_WIDTH && !hit; ++i ) {
                    int x = va->x + i, y = va->y + j;
                    if ( sprscr_pixel( va, x, y ) != 0 && sprscr_pixel( vb, x, y ) != 0 ) {
                        hit = true;
                    }
                }
            }
            if ( hit ) {
                uint8_t olda = __atomic_fetch_or( &sprhit[ha], 1, __ATOMIC_ACQ_REL );
                uint8_t oldb = __atomic_fetch_or( &sprhit[hb], 1, __ATOMIC_ACQ_REL );
                if ( !( ( olda & 1U ) && ( oldb & 1U ) ) ) {
                    sprscr_postevent( ha );
                }
                if ( with[ha] < 0 ) with[ha] = hb;
                if ( with[hb] < 0 ) with[hb] = ha;
            }
        }
    }
    for ( int i=0; i < MAX_SPRITES; ++i ) {
        __atomic_store_n( &sprcollwith[i], with[i], __ATOMIC_RELEASE );
    }
}

// the drawing order: by priority, the reserved sprites (the mouse pointer) after all of the program (key 8), equal keys by number; the keys are
// copied before sorting so that a priority that changes meanwhile can't make the comparisons inconsistent
static uint8_t sprsortkey[MAX_SPRITES];

static int sprscr_rendersort( const void* pa, const void* pb ) {
    int a = *( (const uint8_t*) pa );
    int b = *( (const uint8_t*) pb );
    int d = (int) sprsortkey[a] - (int) sprsortkey[b];
    return d != 0 ? d : a - b;
}

void sprscr_periodicals( void ) {
    // handle MOVETO: first the requests of the main thread, then one step of each movement
    for ( int i=0; i < MAX_SPRITES; ++i ) {
        sprmove_t* m = &sprmove[i];
        uint64_t r = __atomic_load_n( &sprreq[i], __ATOMIC_ACQUIRE );
        if ( r != 0 ) {
            if ( r & SPRREQ_CANCEL ) {
                __atomic_store_n( &m->active, false, __ATOMIC_RELEASE );
            } else {
                int ticks = (int)( ( r >> 32 ) & 0xFFFFU );
                int tx = (int16_t)(uint16_t)( ( r >> 16 ) & 0xFFFFU ), ty = (int16_t)(uint16_t)( r & 0xFFFFU );
                if ( ticks == 0 ) {
                    __atomic_store_n( &m->active, false, __ATOMIC_RELEASE );
                    sprctrl[i].screen_x = tx; sprctrl[i].screen_y = ty;
                    sprscr_touch();
                } else {
                    m->fx = (int32_t) sprctrl[i].screen_x * 65536; m->fy = (int32_t) sprctrl[i].screen_y * 65536;
                    m->dx = (int32_t)( ( (int64_t) tx * 65536 - m->fx ) / ticks );
                    m->dy = (int32_t)( ( (int64_t) ty * 65536 - m->fy ) / ticks );
                    m->ticks = ticks;
                    m->tx = tx; m->ty = ty;
                    __atomic_store_n( &m->active, true, __ATOMIC_RELEASE );      // (set before the request is taken off: moving() never sees a gap)
                }
            }
            __atomic_compare_exchange_n( &sprreq[i], &r, 0, false, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED );   // a newer request stays for the next frame
        }
        if ( !__atomic_load_n( &m->active, __ATOMIC_ACQUIRE ) ) {
            continue;
        }
        if ( --m->ticks <= 0 ) {
            __atomic_store_n( &m->active, false, __ATOMIC_RELEASE );
            sprctrl[i].screen_x = m->tx; sprctrl[i].screen_y = m->ty;
        } else {
            m->fx += m->dx; m->fy += m->dy;
            sprctrl[i].screen_x = (int)( m->fx >> 16 ); sprctrl[i].screen_y = (int)( m->fy >> 16 );
        }
        sprscr_touch();
    }
    // handle animation
    for ( uint16_t i=0; i < UINT16_C(256); ++i ) {
        sprctrl_t* ctrl = &sprctrl[i];
        if ( !ctrl->active ) {
            // disabled, skip
            continue;
        }
        uint8_t oldpos = ctrl->anim_pos;
        if ( ctrl->anim_speed_count > UINT8_C(0) ) {
            if ( --ctrl->anim_speed_count == 0 ) {
                ctrl->anim_speed_count = ctrl->anim_speed_reset;
                if ( ++ctrl->anim_pos >= ctrl->anim_size ) {
                    ctrl->anim_pos = 0;
                }
            }
        }
        if ( ctrl->anim_pos == oldpos ) {
            // no change, skip
            continue;
        }
        sprscr_touch();
    }
    sprscr_collisions();
}

void sprscr_render( uint8_t* target ) {
    // (the flag is cleared before the data is read, atomically: a change made while the frame is built sets it again, and is never lost)
    if ( !__atomic_load_n( &sprchg, __ATOMIC_ACQUIRE ) || !__atomic_exchange_n( &sprchg, false, __ATOMIC_ACQ_REL ) ) {
        return;
    }
    memset( target, 0, SPRTGT_WIDTH * SPRTGT_HEIGHT );

    uint8_t tmp[256];
    sprview_t views[256];
    for ( uint16_t i=0; i < UINT16_C(256); ++i ) {
        tmp[i] = (uint8_t) i;
        sprscr_view( i, &views[i] );                // (a member of a group takes the head's priority, position and image + its place)
        sprsortkey[i] = i >= SPR_SYS_FIRST ? 8 : (uint8_t) views[i].prio;
    }
    qsort( &tmp[0], 256U, 1U, sprscr_rendersort );

    for ( uint16_t i=0; i < UINT16_C(256); ++i ) {
        uint8_t sprno = tmp[i];
        const sprview_t* ctrl = &views[ sprno ];
        if ( !ctrl->active ) {
            // disabled, skip
            continue;
        }
        int sprimg      = ctrl->image;
        int spriy       = sprimg / SPRMAP_CELLSX;
        int sprix       = sprimg % SPRMAP_CELLSX;
        uint16_t spricy = spriy * SPRITE_HEIGHT;
        uint16_t spricx = sprix * SPRITE_WIDTH;
        int16_t left    = (int16_t) ctrl->x;
        int16_t top     = (int16_t) ctrl->y;
        int16_t right   = left + SPRITE_WIDTH;
        int16_t bottom  = top + SPRITE_HEIGHT;
        uint8_t offsx   = 0;
        uint8_t offsy   = 0;
        uint8_t width   = SPRITE_WIDTH;
        uint8_t height  = SPRITE_HEIGHT;
        if ( right <= 0 || bottom <= 0 || left >= SPRTGT_WIDTH || top >= SPRTGT_HEIGHT ) {
            // off-screen, skip
            continue;
        }
        if ( left < 0 ) {
            // left edge is off-screen
            offsx = -left; spricx += offsx; left = 0;
        } else if ( right > SPRTGT_WIDTH ) {
            // right edge is off-screen
            width -= right - SPRTGT_WIDTH;
        }
        if ( top < 0 ) {
            // top edge is off-screen
            offsy = -top; spricy += offsy; top = 0;
        } else if ( bottom > SPRTGT_HEIGHT ) {
            // bottom edge is off-screen
            height -= bottom - SPRTGT_HEIGHT;
        }
        uint32_t spripos = spricy * SPRMAP_WIDTH + spricx;
        uint32_t tgtpos  = top    * SPRTGT_WIDTH + left;
        const uint8_t* s = &sprmap[ spripos ];
        uint8_t*       d = &target[ tgtpos  ];
        for ( uint8_t y=offsy; y < height; ++y ) {
            const uint8_t* s0 = s;
            uint8_t*       d0 = d;
            for ( uint8_t x=offsx; x < width; ++x ) {
                *d = *s ? *s : *d;
                ++d; ++s;
            }
            s = s0 + SPRMAP_WIDTH;
            d = d0 + SPRTGT_WIDTH;
        }
    }
}