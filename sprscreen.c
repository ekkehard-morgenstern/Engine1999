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
#include "tilescreen.h"

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
        spr_til_coll_ind : 1,
        use_image        : 1,   // a fixed image instead of the animation sequence (SPRITE n,IMAGE i)
        image            : 8;
    signed int
        screen_x         : 11,
        screen_y         : 11,  // 24
                         : 0;   // 32
} sprctrl_t;

static sprctrl_t    sprctrl[MAX_SPRITES];
static uint8_t      spranim[MAX_ANIMSEQ][MAX_SPRANIM];
static uint8_t      sprmap[SPRMAP_WIDTH * SPRMAP_HEIGHT];
static bool         sprchg = false;

// SPRITE n,MOVETO: the position moves by itself, one step per frame (sprscr_periodicals)
typedef struct _sprmove_t {
    bool    active;
    int32_t fx, fy;             // 16.16 fixed point position
    int32_t dx, dy;             // step per frame
    int     ticks;              // frames to go
    int     tx, ty;             // the target
} sprmove_t;

static sprmove_t    sprmove[MAX_SPRITES];
static int          sprcollwith[MAX_SPRITES];       // the sprite a sprite last collided with (-1 none)
static int          sprevent = -1;                  // a sprite with a new collision (for ON COLLISION), -1 none

static uint8_t sprscr_curimage( const sprctrl_t* ctrl ) {
    return ctrl->use_image ? (uint8_t) ctrl->image : spranim[ ctrl->anim_index ][ ctrl->anim_pos ];
}

void sprscr_init( void ) {
    memset( &sprctrl[0], 0, sizeof(sprctrl_t) * MAX_SPRITES );
    memset( &spranim[0][0], 0, MAX_ANIMSEQ * MAX_SPRANIM );
    memset( &sprctrl[0], 0, sizeof(sprctrl_t) * MAX_SPRITES );
    memset( &sprmove[0], 0, sizeof(sprmove) );
    for ( int i=0; i < MAX_SPRITES; ++i ) {
        sprcollwith[i] = -1;
    }
    sprevent = -1;
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
    ctrl->use_image        = 0;
    ctrl->anim_index       = animno;
    ctrl->anim_pos         = 0;
    ctrl->anim_size        = length;
    ctrl->anim_speed_count = speed;
    ctrl->anim_speed_reset = speed;
    sprchg = true;
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
    sprchg = true;
}

bool sprscr_changed( void ) {
    return sprchg;
}

void sprscr_setimage( int sprno, int image ) {
    if ( sprno < 0 || sprno >= MAX_SPRITES || image < 0 || image > 255 ) {
        return;
    }
    sprctrl[sprno].use_image = 1;
    sprctrl[sprno].image = (unsigned) image;
    sprchg = true;
}

int sprscr_getimage( int sprno ) {
    if ( sprno < 0 || sprno >= MAX_SPRITES ) {
        return -1;
    }
    return sprscr_curimage( &sprctrl[sprno] );
}

bool sprscr_getpos( int sprno, int* x, int* y ) {
    if ( sprno < 0 || sprno >= MAX_SPRITES ) {
        return false;
    }
    *x = sprctrl[sprno].screen_x;
    *y = sprctrl[sprno].screen_y;
    return true;
}

bool sprscr_isactive( int sprno ) {
    return sprno >= 0 && sprno < MAX_SPRITES && sprctrl[sprno].active;
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
    if ( sprno < 0 || sprno >= MAX_SPRITES ) {
        return;
    }
    sprmove_t* m = &sprmove[sprno];
    if ( ticks <= 0 ) {
        m->active = false;
        sprscr_move( sprno, x, y );
        return;
    }
    m->active = true;
    m->fx = (int32_t) sprctrl[sprno].screen_x * 65536; m->fy = (int32_t) sprctrl[sprno].screen_y * 65536;
    m->dx = (int32_t)( ( (int64_t) x * 65536 - m->fx ) / ticks );
    m->dy = (int32_t)( ( (int64_t) y * 65536 - m->fy ) / ticks );
    m->ticks = ticks;
    m->tx = x; m->ty = y;
}

bool sprscr_moving( int sprno ) {
    return sprno >= 0 && sprno < MAX_SPRITES && sprmove[sprno].active;
}

void sprscr_setcollide( int sprno, bool sprites, bool tiles ) {
    if ( sprno < 0 || sprno >= MAX_SPRITES ) {
        return;
    }
    sprctrl[sprno].spr_spr_coll_ena = sprites;
    sprctrl[sprno].spr_til_coll_ena = tiles;
    if ( !sprites ) sprctrl[sprno].spr_spr_coll_ind = 0;
    if ( !tiles )   sprctrl[sprno].spr_til_coll_ind = 0;
}

uint32_t sprscr_hits( int sprno ) {
    if ( sprno < 0 || sprno >= MAX_SPRITES ) {
        return 0;
    }
    sprctrl_t* c = &sprctrl[sprno];
    uint32_t r = ( c->spr_spr_coll_ind ? 1U : 0U ) | ( c->spr_til_coll_ind ? 2U : 0U );
    c->spr_spr_coll_ind = 0;
    c->spr_til_coll_ind = 0;
    return r;
}

int sprscr_collidewith( int sprno ) {
    return sprno >= 0 && sprno < MAX_SPRITES ? sprcollwith[sprno] : -1;
}

int sprscr_nextevent( void ) {
    int e = sprevent;
    sprevent = -1;
    return e;
}

// the palette index of a sprite at a screen pixel, 0 where it is transparent or the pixel is outside
static int sprscr_pixel( int sprno, int x, int y ) {
    const sprctrl_t* c = &sprctrl[sprno];
    int lx = x - c->screen_x, ly = y - c->screen_y;
    if ( lx < 0 || ly < 0 || lx >= SPRITE_WIDTH || ly >= SPRITE_HEIGHT ) {
        return 0;
    }
    uint8_t img = sprscr_curimage( c );
    return sprmap[ ( ( img / SPRMAP_CELLSX ) * SPRITE_HEIGHT + ly ) * SPRMAP_WIDTH + ( img % SPRMAP_CELLSX ) * SPRITE_WIDTH + lx ];
}

static void sprscr_collisions( void ) {
    for ( int i=0; i < MAX_SPRITES; ++i ) {
        sprcollwith[i] = -1;
    }
    for ( int a=0; a < MAX_SPRITES; ++a ) {
        const sprctrl_t* ca = &sprctrl[a];
        if ( !ca->active || ( !ca->spr_spr_coll_ena && !ca->spr_til_coll_ena ) ) {
            continue;
        }
        if ( ca->spr_til_coll_ena ) {
            bool hit = false;
            for ( int j=0; j < SPRITE_HEIGHT && !hit; ++j ) {
                for ( int i=0; i < SPRITE_WIDTH && !hit; ++i ) {
                    int x = ca->screen_x + i, y = ca->screen_y + j;
                    if ( sprscr_pixel( a, x, y ) != 0 && tilescr_pixel( x, y ) > 0 ) {
                        hit = true;
                    }
                }
            }
            if ( hit ) {
                if ( !sprctrl[a].spr_til_coll_ind && sprevent < 0 ) {
                    sprevent = a;
                }
                sprctrl[a].spr_til_coll_ind = 1;
            }
        }
        if ( !ca->spr_spr_coll_ena ) {
            continue;
        }
        for ( int b = a + 1; b < MAX_SPRITES; ++b ) {
            const sprctrl_t* cb = &sprctrl[b];
            if ( !cb->active || !cb->spr_spr_coll_ena ) {
                continue;
            }
            int dx = cb->screen_x - ca->screen_x, dy = cb->screen_y - ca->screen_y;
            if ( dx <= -SPRITE_WIDTH || dx >= SPRITE_WIDTH || dy <= -SPRITE_HEIGHT || dy >= SPRITE_HEIGHT ) {
                continue;
            }
            bool hit = false;
            for ( int j=0; j < SPRITE_HEIGHT && !hit; ++j ) {
                for ( int i=0; i < SPRITE_WIDTH && !hit; ++i ) {
                    int x = ca->screen_x + i, y = ca->screen_y + j;
                    if ( sprscr_pixel( a, x, y ) != 0 && sprscr_pixel( b, x, y ) != 0 ) {
                        hit = true;
                    }
                }
            }
            if ( hit ) {
                if ( sprevent < 0 && !( sprctrl[a].spr_spr_coll_ind && sprctrl[b].spr_spr_coll_ind ) ) {
                    sprevent = a;
                }
                sprctrl[a].spr_spr_coll_ind = 1;
                sprctrl[b].spr_spr_coll_ind = 1;
                if ( sprcollwith[a] < 0 ) sprcollwith[a] = b;
                if ( sprcollwith[b] < 0 ) sprcollwith[b] = a;
            }
        }
    }
}

static int sprscr_rendersort( const void* pa, const void* pb ) {
    uint8_t a = *( (const uint8_t*) pa );
    uint8_t b = *( (const uint8_t*) pb );
    return (int)(sprctrl[a].priority) - (int)(sprctrl[b].priority);
}

void sprscr_periodicals( void ) {
    // handle MOVETO
    for ( int i=0; i < MAX_SPRITES; ++i ) {
        sprmove_t* m = &sprmove[i];
        if ( !m->active ) {
            continue;
        }
        if ( --m->ticks <= 0 ) {
            m->active = false;
            sprctrl[i].screen_x = m->tx; sprctrl[i].screen_y = m->ty;
        } else {
            m->fx += m->dx; m->fy += m->dy;
            sprctrl[i].screen_x = (int)( m->fx >> 16 ); sprctrl[i].screen_y = (int)( m->fy >> 16 );
        }
        sprchg = true;
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
        sprchg = true;
    }
    sprscr_collisions();
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
        uint8_t sprimg  = sprscr_curimage( ctrl );
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