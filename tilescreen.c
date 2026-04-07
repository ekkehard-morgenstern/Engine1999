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

// copy logical tile from tile memory to screen, using scroll offsets
static void tilescr_copy_logical_tile(
    // target memory buffer
    uint8_t* target,
    // target on-screen coordinates
    uint16_t targetx,
    uint16_t targety,
    // tile memory X position
    uint8_t tileposx,
    uint8_t tileposy,
    // scroll offsets
    uint8_t scrolloffsx,
    uint8_t scrolloffsy
) {
    /* if ( scrolloffsx == 0 && scrolloffsy == 0 ) {
        // not scrolled: can copy full tile
        goto FULL_TILE;
    } */
    if ( tileposy >= UINT8_C(1) && tileposx >= UINT8_C(1) ) {
        // not at the left/bottom edge: can copy full tile
        goto FULL_TILE;
    }
    // TBD
    return;

FULL_TILE:
    // copy full tile (except at right / bottom border)
    uint16_t left = targetx + scrolloffsx;
    uint16_t top  = targety + scrolloffsy;
    uint16_t right = left + TILE_WIDTH;
    uint16_t bottom = top + TILE_HEIGHT;
    if ( right > TILETGT_WIDTH ) {
        right = TILETGT_WIDTH;
    }
    if ( bottom > TILETGT_HEIGHT ) {
        bottom = TILETGT_HEIGHT;
    }
    uint8_t tile = tilescr[ tileposy * TILESCR_WIDTH + tileposx ];
    tilescr_copy_tile_portion(
        target,
        left,
        top,
        tile,
        0,
        0,
        right - left,
        bottom - top
    );
}

void tilescr_render( uint8_t* target ) {
    uint8_t scrolloffsx = tileoffsx % TILE_WIDTH;
    uint8_t scrolloffsy = tileoffsy % TILE_HEIGHT;
    for ( uint8_t y=0; y < TILESCR_HEIGHT-1; ++y ) {
        uint16_t scry = y * TILE_HEIGHT;
        for ( uint8_t x=0; x < TILESCR_WIDTH-1; ++x ) {
            uint16_t scrx = x * TILE_WIDTH;
            tilescr_copy_logical_tile(
                target,
                scrx,
                scry,
                x,
                y,
                scrolloffsx,
                scrolloffsy
            );
        }
    }


    /*
    for ( int y=0; y < TILETGT_HEIGHT; ++y ) {
        short tiley = tileoffsy + y ) % TILETGT_HEIGHT;
        short tilesy = tiley / TILE_HEIGHT;
        short tileyo = tiley % TILE_HEIGHT;
        for ( int x=0; x < TILETGT_WIDTH; ++x ) {
            short tilex = ( tileoffsx + x ) % TILETGT_WIDTH;
            short tilesx = tilesx / TILE_WIDTH;
            short tilesxo = tilesx % TILE_WIDTH;
            short
        }
    }

    const uint8_t* tsp = &tilescr[0];
    for ( uint8_t y=0; y < TILESCR_HEIGHT; ++y ) {
        for ( uint8_t x=0; x < TILESCR_WIDTH; ++x ) {
            uint8_t tile = *tsp++;
            uint8_t mapx = tile % TILEMAP_CELLSX;
            uint8_t mapy = tile / TILEMAP_CELLSX;
            uint16_t mapcx = mapx * TILE_WIDTH;
            uint16_t mapcy = mapy * TILE_HEIGHT;
            uint32_t mapof = mapcy * TILEMAP_WIDTH + mapcx;
            int16_t left = x * TILE_WIDTH  + tileoffsx;
            int16_t top  = y * TILE_HEIGHT + tileoffsy;
            int16_t right = left + TILE_WIDTH - 1;
            int16_t bottom = top + TILE_HEIGHT - 1;
            uint8_t sx, sy, ex, ey;
            if ( left < 0 ) {
                sx = -left; left = 0;
            } else {
                sx = 0;
            }
            if ( top < 0 ) {
                sy = -top; top = 0;
            } else {
                sy = 0;
            }
            ex = TILE_WIDTH;
            ey = TILE_HEIGHT;
            if ( right >= TILETGT_WIDTH ) {
                int16_t delta = ( right - TILETGT_WIDTH ) + 1;
                if ( delta >= TILE_WIDTH ) {
                    ex = 0;
                } else {
                    ex -= delta;
                }
                right = TILETGT_WIDTH - 1;
            }
            if ( bottom >= TILETGT_HEIGHT ) {
                int16_t delta = ( bottom - TILETGT_HEIGHT ) + 1;
                if ( delta >= TILE_HEIGHT ) {
                    ey = 0;
                } else {
                    ey -= delta;
                }
                bottom = TILETGT_HEIGHT - 1;
            }
            uint8_t* tgp = &target[ top * TILETGT_WIDTH + left ];
            const uint8_t* tmp = &tilemap[ mapof + sy * TILEMAP_WIDTH + sx ];
            for ( uint8_t cy=sy; cy < ey; ++cy ) {
                uint8_t* tgp0 = tgp;
                const uint8_t* tmp0 = tmp;
                for ( uint8_t cx=sx; cx < ex; ++cx ) {
                    *tgp++ = *tmp++;
                }
                tgp = tgp0 + TILETGT_WIDTH;
                tmp = tmp0 + TILEMAP_WIDTH;
            }
        }
    }
        */
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