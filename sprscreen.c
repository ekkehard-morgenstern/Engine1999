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

#include "sprscreen.h"

typedef struct _sprctrl_t {
    unsigned int
        anim_index       : 8,
        anim_pos         : 4,
        anim_size        : 4,
        anim_speed_count : 5,
        anim_speed_reset : 5,
        priority         : 3,
        active           : 1,
        spr_spr_coll_ena : 1,
        spr_spr_coll_ind : 1;   // 32
    unsigned int
        spr_til_coll_ena : 1,
        spr_til_coll_ind : 1;
    signed int
        screen_x         : 11,
        screen_y         : 11,  // 24
                         : 0;   // 32
} sprctrl_t;

static sprctrl_t    sprctrl[MAX_SPRITES];
static uint8_t      spranim[MAX_ANIMSEQ][MAX_SPRANIM];
static uint8_t      sprmap[SPRMAP_WIDTH * SPRMAP_HEIGHT];
static bool         sprchg = false;

void sprscr_init( void ) {
    memset( &sprctrl[0], 0, sizeof(sprctrl_t) * MAX_SPRITES );
    memset( &spranim[0][0], 0, MAX_ANIMSEQ * MAX_SPRANIM );
    memset( &sprctrl[0], 0, sizeof(sprctrl_t) * MAX_SPRITES );
    sprchg = true;
}

void sprscr_show( int sprno, bool active ) {
    if ( sprno < 0 || sprno >= MAX_SPRITES ) {
        return;
    }
    sprctrl[sprno].active = active;
    sprchg = true;
}

void sprscr_prio( int sprno, int prio ) {
    if ( sprno < 0 || sprno >= MAX_SPRITES ) {
        return;
    }
    sprctrl[sprno].priority = prio;
    sprchg = true;
}

void sprscr_move( int sprno, int x, int y ) {
    if ( sprno < 0 || sprno >= MAX_SPRITES ) {
        return;
    }
    sprctrl[sprno].screen_x = x;
    sprctrl[sprno].screen_y = y;
    sprchg = true;
}

void sprscr_animdata( int animno, const uint8_t* seq, size_t size ) {
    if ( animno < 0 || animno >= MAX_ANIMSEQ || seq == 0 || size == 0 ) {
        return;
    }
    if ( size > 15U ) {
        size = 15U;
    }
    memcpy( &spranim[animno][0], seq, size );
    sprchg = true;
}

void sprscr_animcfg( int sprno, int animno, int length, int speed ) {
    if ( sprno < 0 || sprno >= MAX_SPRITES || animno < 0 || animno >= MAX_ANIMSEQ ) {
        return;
    }
    if ( length < 0 || length > 15 || speed < 0 || speed > 31 ) {
        return;
    }
    sprctrl_t* ctrl = &sprctrl[sprno];
    ctrl->anim_index       = animno;
    ctrl->anim_pos         = 0;
    ctrl->anim_size        = length;
    ctrl->anim_speed_count = speed;
    ctrl->anim_speed_reset = speed;
    sprchg = true;
}

void sprscr_writemap( int sprno, const uint8_t data[SPRITE_WIDTH * SPRITE_HEIGHT]) {
    if ( sprno < 0 || sprno > 255 ) return;
    uint8_t mapx = (uint8_t)( sprno % SPRMAP_CELLSX );
    uint8_t mapy = (uint8_t)( sprno / SPRMAP_CELLSX );
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
    sprchg = true;
}

bool sprscr_changed( void ) {
    return sprchg;
}

static int sprscr_rendersort( const void* pa, const void* pb ) {
    uint8_t a = *( (const uint8_t*) pa );
    uint8_t b = *( (const uint8_t*) pb );
    return (int)(sprctrl[a].priority) - (int)(sprctrl[b].priority);
}

void sprscr_periodicals( void ) {
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
        sprchg = true;
    }
}

void sprscr_render( uint8_t* target ) {
    if ( !sprchg ) {
        return;
    }
    sprchg = false;
    memset( target, 0, SPRTGT_WIDTH * SPRTGT_HEIGHT );

    uint8_t tmp[256];
    for ( uint16_t i=0; i < UINT16_C(256); ++i ) {
        tmp[i] = (uint8_t) i;
    }
    qsort( &tmp[0], 256U, 1U, sprscr_rendersort );

    for ( uint16_t i=0; i < UINT16_C(256); ++i ) {
        uint8_t sprno = tmp[i];
        const sprctrl_t* ctrl = &sprctrl[ sprno ];
        if ( !ctrl->active ) {
            // disabled, skip
            continue;
        }
        uint8_t animno  = ctrl->anim_index;
        uint8_t animpos = ctrl->anim_pos;
        uint8_t sprimg  = spranim[animno][animpos];
        uint8_t spriy   = sprimg / SPRMAP_CELLSX;
        uint8_t sprix   = sprimg % SPRMAP_CELLSX;
        uint16_t spricy = spriy * SPRITE_HEIGHT;
        uint16_t spricx = sprix * SPRITE_WIDTH;
        int16_t left    = ctrl->screen_x;
        int16_t top     = ctrl->screen_y;
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
            offsx = -left; width -= offsx; spricx += offsx; left = 0;
        } else if ( right > SPRTGT_WIDTH ) {
            // right edge is off-screen
            width -= right - SPRTGT_WIDTH;
        }
        if ( top < 0 ) {
            // top edge is off-screen
            offsy = -top; height -= offsy; spricy += offsy; top = 0;
        } else if ( bottom > SPRTGT_HEIGHT ) {
            // bottom edge is off-screen
            height -= bottom - SPRTGT_HEIGHT;
        }
        uint8_t spripos = spricy * SPRMAP_WIDTH + spricx;
        uint32_t tgtpos = top    * SPRTGT_WIDTH + left;
        const uint8_t* s = &sprmap[ spripos ];
        uint8_t*       d = &target[ tgtpos  ];
        for ( uint8_t y=0; y < height; ++y ) {
            const uint8_t* s0 = s;
            uint8_t*       d0 = d;
            for ( uint8_t x=0; x < width; ++x ) {
                *d++ = *s++;
            }
            s = s0 + SPRMAP_WIDTH;
            d = d0 + SPRTGT_WIDTH;
        }
    }
}