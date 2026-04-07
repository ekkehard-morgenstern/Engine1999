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

#include "tilescreen.h"

static uint8_t tilemap[TILEMAP_WIDTH * TILEMAP_HEIGHT];
static uint8_t tilescr[TILESCR_WIDTH * TILESCR_HEIGHT];
static uint8_t tileoffsx = UINT8_C(0);
static uint8_t tileoffsy = UINT8_C(0);

void tilescr_init( void ) {
    memset( &tilemap[0], 0, TILEMAP_WIDTH * TILEMAP_HEIGHT );
    memset( &tilescr[0], 0, TILESCR_WIDTH * TILESCR_HEIGHT );
}

// copy portion of tile from tilemap to screen
static void tilescr_copy_tile_portion(
    // target memory buffer
    uint8_t* target,
    // target on-screen coordinates
    uint16_t targetx,
    uint16_t targety,
    // tile index
    uint8_t tile,
    // tile portion to be transferred
    uint8_t tilel, uint8_t tilet, uint8_t tilew, uint8_t tileh
) {
    if ( targety >= TILETGT_HEIGHT || targetx >= TILETGT_WIDTH ) return;
    if ( targetx + tilew >= TILETGT_WIDTH ) {
        tilew = TILETGT_WIDTH - targetx;
    }
    if ( targety + tileh >= TILETGT_HEIGHT ) {
        tileh = TILETGT_HEIGHT - targety;
    }
    uint8_t* tgtptr = &target[ targety * TILETGT_WIDTH + targetx ];
    uint8_t tiley = tile / TILEMAP_CELLSX;
    uint8_t tilex = tile % TILEMAP_CELLSX;
    uint16_t tilecy = tiley * TILE_HEIGHT + tilet;
    uint16_t tilecx = tilex * TILE_WIDTH + tilel;
    const uint8_t* srcptr = &tilemap[ tilecy * TILEMAP_WIDTH + tilecx ];
    for ( uint8_t y=0; y < tileh; ++y ) {
        uint8_t*       tgt0 = tgtptr;
        const uint8_t* src0 = srcptr;
        for ( uint8_t x=0; x < tilew; ++x ) {
            *tgtptr++ = *srcptr++;
        }
        tgtptr = tgt0 + TILETGT_WIDTH;
        srcptr = src0 + TILEMAP_WIDTH;
    }
}

typedef struct _rect_t {
    uint16_t left, top;
    uint8_t width, height, empty, tile, offx, offy;
} rect_t;

static void init_rect( rect_t* r, uint16_t l, uint16_t t, uint8_t w, uint8_t h, int8_t tx, int8_t ty, uint8_t ox, uint8_t oy ) {
    r->left = l; r->top = t; r->width = w; r->height = h; r->offx = ox; r->offy = oy;
    r->empty = w == 0 || h == 0;
    if ( tx < 0 || tx >= TILESCR_WIDTH ) {
        tx = TILESCR_WIDTH - 1;
    }
    if ( ty < 0 || ty >= TILESCR_HEIGHT ) {
        ty = TILESCR_HEIGHT - 1;
    }
    r->tile = tilescr[ ty * TILESCR_WIDTH + tx ];
}

static void init_zones( rect_t r[4], uint16_t l, uint16_t t, uint8_t sx, uint8_t sy, int8_t tx, int8_t ty ) {
    /*
    tile zones:
      A | B
        |
      --+----
      C | D
        |
        |
    zone D is the normal tile shifted by the scroll offsets.
    */
    init_rect( &r[0], l     , t     , sx             , sy              , tx - 1, ty - 1, TILE_WIDTH - sx, TILE_HEIGHT - sy );
    init_rect( &r[1], l + sx, t     , TILE_WIDTH - sx, sy              , tx    , ty - 1, 0              , TILE_HEIGHT - sy );
    init_rect( &r[2], l     , t + sy, sx             , TILE_HEIGHT - sy, tx - 1, ty    , TILE_WIDTH - sx, 0                );
    init_rect( &r[3], l + sx, t + sy, TILE_WIDTH - sx, TILE_HEIGHT - sy, tx    , ty    , 0              , 0                );
    if ( sx == 0 && sy == 0 ) { // need only D
        r[2].empty = r[1].empty = r[0].empty = true;
    } else if ( sx == 0 ) { // need only B and D
        r[2].empty = r[0].empty = true;
    } else if ( sy == 0 ) { // need only C and D
        r[1].empty = r[0].empty = true;
    }
}

// copy logical tile from tile memory to screen, using scroll offsets
static void tilescr_copy_logical_tile(
    // target memory buffer
    uint8_t* target,
    // target on-screen coordinates
    uint16_t targetx,
    uint16_t targety,
    // scroll offsets
    uint8_t scrolloffsx,
    uint8_t scrolloffsy
) {
    /*
    tile zones:
      A | B
        |
      --+----
      C | D
        |
        |

    zone D is the normal tile shifted by the scroll offsets.
    */
    int8_t tilex = (int8_t)( targetx / TILE_WIDTH  );
    int8_t tiley = (int8_t)( targety / TILE_HEIGHT );
    rect_t z[4];
    init_zones( z, targetx, targety, scrolloffsx, scrolloffsy, tilex, tiley );
    for ( uint8_t i=0; i < UINT8_C(4); ++i ) {
        if ( z[i].empty ) continue;
        tilescr_copy_tile_portion(
            target,
            z[i].left,
            z[i].top,
            z[i].tile,
            z[i].offx,
            z[i].offy,
            z[i].width,
            z[i].height
        );
    }
}

void tilescr_render( uint8_t* target ) {
    uint8_t scrolloffsx = tileoffsx % TILE_WIDTH;
    uint8_t scrolloffsy = tileoffsy % TILE_HEIGHT;
    for ( int y=0; y < TILETGT_HEIGHT; y += TILE_HEIGHT ) {
        for ( int x=0; x < TILETGT_WIDTH; x += TILE_WIDTH ) {
            tilescr_copy_logical_tile(
                target,
                (uint16_t) x,
                (uint16_t) y,
                scrolloffsx,
                scrolloffsy
            );
        }
    }
}

void tilescr_writetile( int tileno, const uint8_t data[TILE_WIDTH * TILE_HEIGHT]) {
    if ( tileno < 0 || tileno > 255 ) return;
    uint8_t mapx = (uint8_t)( tileno % TILEMAP_CELLSX );
    uint8_t mapy = (uint8_t)( tileno / TILEMAP_CELLSX );
    uint16_t mapcx = mapx * TILE_WIDTH;
    uint16_t mapcy = mapy * TILE_HEIGHT;
    uint32_t mapof = mapcy * TILEMAP_WIDTH + mapcx;
    const uint8_t* s = &data[0];
    uint8_t* d = &tilemap[ mapof ];
    for ( uint8_t y=0; y < (uint8_t) TILE_HEIGHT; ++y ) {
        for ( uint8_t x=0; x < (uint8_t) TILE_WIDTH; ++x ) {
            *d++ = *s++;
        }
        d += TILEMAP_WIDTH - TILE_WIDTH;
    }
}

void tilescr_scroll( int sx, int sy ) {
    if ( sx < 0 || sy < 0 ) return;
    tileoffsx = (uint8_t)( sx % TILE_WIDTH );
    tileoffsy = (uint8_t)( sy % TILE_HEIGHT );
}