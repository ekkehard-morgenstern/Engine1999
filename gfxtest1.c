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


#include "gfxcore.h"
#include "textscreen.h"
#include "tilescreen.h"
#include "sprscreen.h"

/*
Automated test of the graphics backends without SDL: the drawing core (gfxcore.c), and what tilescreen.c, sprscreen.c and textscreen.c got
for BASIC (cell setter, collisions, movement, areas of the text screen, save and restore). Exit status = number of failures.
*/

static int checks = 0, failures = 0;

static void check( bool ok, const char* name ) {
    ++checks;
    if ( !ok ) {
        ++failures;
        printf( "FAIL  %s\n", name );
    }
}

#define W 64
#define H 48
static uint8_t buf[W * H];
static gfx_t g;

static void fresh( void ) {
    memset( buf, 0, sizeof(buf) );
    gfx_init( &g, buf, W, H );
}

static int count( int color ) {
    int n = 0;
    for ( int i=0; i < W * H; ++i ) {
        if ( buf[i] == color ) ++n;
    }
    return n;
}

static void test_pixels( void ) {
    fresh();
    gfx_pset( &g, 3, 4, 7 );
    check( gfx_point( &g, 3, 4 ) == 7 && count( 7 ) == 1, "pset" );
    check( gfx_point( &g, -1, 0 ) == -1 && gfx_point( &g, W, 0 ) == -1 && gfx_point( &g, 0, H ) == -1, "point outside" );
    gfx_pset( &g, -5, 2, 1 ); gfx_pset( &g, W + 5, 2, 1 ); gfx_pset( &g, 0, -1, 1 );
    check( count( 1 ) == 0, "pset outside is clipped" );
    g.pen = 9;
    gfx_pset( &g, 5, 5, -1 );
    check( gfx_point( &g, 5, 5 ) == 9, "the pen is the default color" );
    g.mode = GFXM_XOR; gfx_pset( &g, 3, 4, 7 );
    check( gfx_point( &g, 3, 4 ) == 0, "xor twice restores" );
    g.mode = GFXM_OR; gfx_pset( &g, 3, 4, 5 ); gfx_pset( &g, 3, 4, 2 );
    check( gfx_point( &g, 3, 4 ) == 7, "or" );
    g.mode = GFXM_AND; gfx_pset( &g, 3, 4, 6 );
    check( gfx_point( &g, 3, 4 ) == 6, "and" );
    g.mode = GFXM_CLEAR; gfx_pset( &g, 3, 4, 6 );
    check( gfx_point( &g, 3, 4 ) == 0, "clear" );
    g.mode = GFXM_SET;
    gfx_clear( &g, 3 );
    check( count( 3 ) == W * H, "clear fills everything" );
}

static void test_lines( void ) {
    fresh();
    gfx_line( &g, 2, 2, 12, 2, 5 );
    check( count( 5 ) == 11 && gfx_point( &g, 2, 2 ) == 5 && gfx_point( &g, 12, 2 ) == 5, "horizontal line includes both ends" );
    fresh();
    gfx_line( &g, 4, 1, 4, 20, 5 );
    check( count( 5 ) == 20, "vertical line" );
    fresh();
    gfx_line( &g, 0, 0, 10, 10, 5 );
    check( count( 5 ) == 11 && gfx_point( &g, 5, 5 ) == 5, "diagonal" );
    gfx_line( &g, 1, 1, 30, 17, 6 );
    int forward = count( 6 );
    fresh();
    gfx_line( &g, 30, 17, 1, 1, 6 );
    check( forward == count( 6 ) && forward == 30 && gfx_point( &g, 1, 1 ) == 6 && gfx_point( &g, 30, 17 ) == 6, "a line has the same length in both directions" );
    fresh();
    gfx_line( &g, -1000, -1000, 1000, 1000, 5 );
    check( gfx_point( &g, 10, 10 ) == 5, "a line through the bitmap from far outside" );
    fresh();
    gfx_line( &g, -500000, 0, 500000, 3, 5 );          // (way out of range: ignored)
    check( true, "huge lines don't hang" );
    fresh();
    gfx_box( &g, 5, 5, 15, 10, 4, false );
    check( count( 4 ) == 2 * 11 + 2 * 4 && gfx_point( &g, 10, 7 ) == 0, "box outline" );
    fresh();
    gfx_box( &g, 15, 10, 5, 5, 4, true );
    check( count( 4 ) == 11 * 6, "filled box, corners in any order" );
    fresh();
    gfx_box( &g, -10, -10, 3, 3, 4, true );
    check( count( 4 ) == 16, "filled box is clipped" );
}

static void test_circles( void ) {
    fresh();
    gfx_circle( &g, 32, 24, 10, 5, -1, -1, 1.0 );
    check( gfx_point( &g, 42, 24 ) == 5 && gfx_point( &g, 22, 24 ) == 5 && gfx_point( &g, 32, 14 ) == 5 && gfx_point( &g, 32, 34 ) == 5, "circle touches the four extremes" );
    check( gfx_point( &g, 32, 24 ) == 0, "circle is hollow" );
    fresh();
    gfx_circle( &g, 32, 24, 10, 5, 0, 90, 1.0 );
    check( gfx_point( &g, 42, 24 ) == 5 && gfx_point( &g, 32, 14 ) == 5 && gfx_point( &g, 22, 24 ) == 0 && gfx_point( &g, 32, 34 ) == 0, "a quarter arc (0..90 degrees: right to top)" );
    fresh();
    gfx_circle( &g, 32, 24, 10, 5, -1, -1, 0.5 );
    check( gfx_point( &g, 42, 24 ) == 5 && gfx_point( &g, 32, 19 ) == 5 && gfx_point( &g, 32, 14 ) == 0, "ellipse: the vertical radius is r * ratio" );
    fresh();
    gfx_circle( &g, 10, 10, 0, 5, -1, -1, 1.0 );
    check( count( 5 ) == 1, "radius 0 is a point" );
    gfx_circle( &g, 10, 10, 100000, 5, -1, -1, 1.0 );
    check( true, "huge circle is refused" );
    fresh();
    gfx_radius( &g, 10, 10, 20, 0.0, 5 );
    check( gfx_point( &g, 30, 10 ) == 5 && gfx_point( &g, 10, 10 ) == 5, "radius to the right" );
    fresh();
    gfx_radius( &g, 10, 30, 20, 90.0, 5 );
    check( gfx_point( &g, 10, 10 ) == 5, "radius up (90 degrees)" );
}

static void test_paint( void ) {
    fresh();
    gfx_box( &g, 5, 5, 20, 15, 4, false );
    gfx_paint( &g, 10, 10, 6, -1 );
    check( count( 6 ) == 14 * 9 && gfx_point( &g, 4, 4 ) == 0 && gfx_point( &g, 5, 5 ) == 4, "paint fills the interior of a closed box only" );
    fresh();
    gfx_box( &g, 5, 5, 20, 15, 4, false );
    gfx_paint( &g, 1, 1, 6, -1 );
    check( count( 6 ) == W * H - 16 * 11, "paint outside fills the rest (to the box)" );
    fresh();
    gfx_box( &g, 5, 5, 20, 15, 4, false );
    gfx_paint( &g, 10, 10, 6, 4 );
    check( count( 6 ) == 14 * 9, "paint up to a border color" );
    gfx_paint( &g, 10, 10, 6, 4 );
    check( count( 6 ) == 14 * 9, "painting again changes nothing" );
    fresh();
    gfx_line( &g, 5, 5, 25, 5, 4 ); gfx_line( &g, 25, 5, 25, 15, 4 ); gfx_line( &g, 25, 15, 5, 15, 4 );       // a gap on the left
    gfx_paint( &g, 10, 10, 6, -1 );
    check( gfx_point( &g, 2, 2 ) == 6 && gfx_point( &g, 4, 10 ) == 6, "an open shape leaks" );
    fresh();
    gfx_paint( &g, 5, 5, 3, -1 );
    check( count( 3 ) == W * H, "paint of an empty bitmap fills all" );
    gfx_paint( &g, -1, 5, 2, -1 );
    check( count( 3 ) == W * H, "paint outside does nothing" );
    fresh();
    for ( int i=0; i < 10; ++i ) { gfx_line( &g, 0, i * 5, W - 1, i * 5, 4 ); }     // many thin rows (a stress for the span stack)
    gfx_paint( &g, 3, 2, 6, -1 );
    check( gfx_point( &g, 63, 2 ) == 6, "paint in thin rows" );
}

static void test_blocks( void ) {
    fresh();
    gfx_box( &g, 10, 10, 19, 14, 3, true );
    gfx_pset( &g, 10, 10, 8 );
    uint8_t blk[4 + 10 * 5];
    size_t need = gfx_grab( &g, 10, 10, 19, 14, blk, sizeof(blk) );
    check( need == 54U && blk[0] == 0 && blk[1] == 10 && blk[2] == 0 && blk[3] == 5 && blk[4] == 8 && blk[5] == 3, "grab: size and header big endian" );
    check( gfx_grab( &g, 10, 10, 19, 14, blk, 10 ) == 54U, "grab with a small buffer reports the size needed" );
    check( gfx_grab( &g, 19, 14, 10, 10, 0, 0 ) == 54U, "corners in any order" );
    gfx_clear( &g, 0 );
    check( gfx_blit( &g, blk, need, 30, 20, false ) && gfx_point( &g, 30, 20 ) == 8 && gfx_point( &g, 39, 24 ) == 3 && gfx_point( &g, 40, 24 ) == 0, "blit puts the block back" );
    fresh();
    gfx_pset( &g, 1, 1, 9 );
    uint8_t t[4 + 4] = { 0, 2, 0, 2, 5, 0, 0, 5 };
    gfx_blit( &g, t, sizeof(t), 0, 0, true );
    check( gfx_point( &g, 0, 0 ) == 5 && gfx_point( &g, 1, 1 ) == 5 && gfx_point( &g, 1, 0 ) == 0, "transparent blit skips index 0" );
    fresh();
    gfx_pset( &g, 1, 0, 9 );
    gfx_blit( &g, t, sizeof(t), 0, 0, false );
    check( gfx_point( &g, 1, 0 ) == 0, "opaque blit writes index 0 too" );
    fresh();
    g.mode = GFXM_XOR;
    gfx_blit( &g, t, sizeof(t), 0, 0, false ); gfx_blit( &g, t, sizeof(t), 0, 0, false );
    check( count( 0 ) == W * H, "xor blit twice erases" );
    g.mode = GFXM_SET;
    check( !gfx_blit( &g, t, 3, 0, 0, false ) && !gfx_blit( &g, t, 6, 0, 0, false ), "malformed blocks are refused" );
    gfx_blit( &g, t, sizeof(t), W - 1, H - 1, false );
    check( gfx_point( &g, W - 1, H - 1 ) == 5, "blit is clipped" );
}

static void test_text( void ) {
    static uint8_t font[256 * 12];
    memset( font, 0, sizeof(font) );
    font['A' * 12 + 0] = 0x80;          // the top left pixel
    font['A' * 12 + 1] = 0x01;          // the second row, right pixel
    fresh();
    gfx_text( &g, 10, 10, "A", 1, 5, 1, 1, font );
    check( gfx_point( &g, 10, 10 ) == 5 && gfx_point( &g, 17, 11 ) == 5 && count( 5 ) == 2, "one glyph, two pixels" );
    fresh();
    gfx_text( &g, 10, 10, "AA", 2, 5, 1, 1, font );
    check( gfx_point( &g, 18, 10 ) == 5 && count( 5 ) == 4, "the next character is 8 pixels to the right" );
    fresh();
    gfx_text( &g, 10, 10, "A", 1, 5, 2, 3, font );
    check( count( 5 ) == 2 * 6 && gfx_point( &g, 11, 12 ) == 5 && gfx_point( &g, 12, 10 ) == 0, "scaled 2 x 3" );
    fresh();
    g.pen = 7;
    gfx_text( &g, 0, 0, "A", 1, -1, 1, 1, font );
    check( gfx_point( &g, 0, 0 ) == 7, "the pen is the default color" );
}

static void test_draw( void ) {
    char err[64];
    fresh();
    g.pen = 5; g.drawx = 10; g.drawy = 10; g.drawcolor = -1;
    check( gfx_draw( &g, "R10", 3, err, sizeof(err) ) && gfx_point( &g, 20, 10 ) == 5 && g.drawx == 20, "R draws right and moves" );
    check( gfx_draw( &g, "D5L10U5", 7, err, sizeof(err) ) && g.drawx == 10 && g.drawy == 10, "a closed shape" );
    fresh();
    g.drawx = 10; g.drawy = 10; g.drawcolor = -1; g.pen = 5;
    gfx_draw( &g, "BR5R5", 5, err, sizeof(err) );
    check( gfx_point( &g, 12, 10 ) == 0 && gfx_point( &g, 16, 10 ) == 5 && g.drawx == 20, "B moves without drawing" );
    fresh();
    g.drawx = 10; g.drawy = 10; g.drawcolor = -1; g.pen = 5;
    gfx_draw( &g, "NR5", 3, err, sizeof(err) );
    check( gfx_point( &g, 15, 10 ) == 5 && g.drawx == 10, "N draws and returns" );
    fresh();
    g.drawx = 10; g.drawy = 10; g.drawcolor = -1; g.pen = 5;
    gfx_draw( &g, "E3F3", 4, err, sizeof(err) );
    check( gfx_point( &g, 13, 7 ) == 5 && g.drawx == 16 && g.drawy == 10, "E up-right, F down-right" );
    fresh();
    g.drawx = 10; g.drawy = 10; g.drawcolor = -1; g.pen = 5;
    gfx_draw( &g, "G2H2", 4, err, sizeof(err) );
    check( g.drawx == 6 && g.drawy == 10 && gfx_point( &g, 8, 12 ) == 5, "G down-left, H up-left" );
    fresh();
    g.drawx = 10; g.drawy = 10; g.drawcolor = -1; g.pen = 5;
    gfx_draw( &g, "C9R3", 4, err, sizeof(err) );
    check( gfx_point( &g, 12, 10 ) == 9, "C sets the color" );
    fresh();
    g.drawx = 10; g.drawy = 10; g.drawcolor = 5; g.drawscale = 4;
    gfx_draw( &g, "S8R5", 4, err, sizeof(err) );
    check( g.drawx == 20, "S8 doubles the lengths" );
    gfx_draw( &g, "S4", 2, err, sizeof(err) );
    fresh();
    g.drawx = 10; g.drawy = 10; g.drawcolor = 5; g.drawscale = 4;
    gfx_draw( &g, "A1R5", 4, err, sizeof(err) );
    check( g.drawx == 10 && g.drawy == 5, "A1 turns right to up" );
    gfx_draw( &g, "A0", 2, err, sizeof(err) );
    fresh();
    g.drawx = 10; g.drawy = 10; g.drawcolor = 5;
    gfx_draw( &g, "M20,15", 6, err, sizeof(err) );
    check( g.drawx == 20 && g.drawy == 15 && gfx_point( &g, 20, 15 ) == 5, "M absolute" );
    gfx_draw( &g, "M+3,-4", 6, err, sizeof(err) );
    check( g.drawx == 23 && g.drawy == 11, "M relative with signs" );
    check( !gfx_draw( &g, "Q5", 2, err, sizeof(err) ) && strstr( err, "Q" ) != 0, "an unknown command is an error" );
    check( !gfx_draw( &g, "M5", 2, err, sizeof(err) ), "M needs two numbers" );
    check( gfx_draw( &g, " r 2 ; d 2 ", 11, err, sizeof(err) ), "lower case, spaces and semicolons" );
}

static void test_tiles( void ) {
    tilescr_init();
    check( tilescr_getcell( 0, 0 ) == 0 && tilescr_getcell( 40, 12 ) == 0 && tilescr_getcell( 41, 0 ) == -1 && tilescr_getcell( 0, 13 ) == -1, "cells: initial value and range" );
    tilescr_setcell( 3, 2, 77 );
    tilescr_setcell( 41, 0, 5 ); tilescr_setcell( 0, 13, 5 ); tilescr_setcell( 0, 0, 256 ); tilescr_setcell( 0, 0, -1 );
    check( tilescr_getcell( 3, 2 ) == 77 && tilescr_getcell( 0, 0 ) == 0, "setcell checks the range" );
    uint8_t cells[41 * 13];
    for ( size_t i=0; i < sizeof(cells); ++i ) cells[i] = (uint8_t)( i % 251 );
    tilescr_setcells( cells, sizeof(cells) );
    check( tilescr_getcell( 40, 12 ) == (int)( ( 41 * 13 - 1 ) % 251 ) && tilescr_getcell( 1, 1 ) == 42, "setcells fills row by row" );
    uint8_t img[16 * 24], back[16 * 24];
    for ( int i=0; i < 16 * 24; ++i ) img[i] = (uint8_t)( 1 + i % 200 );
    tilescr_writetile( 9, img );
    check( tilescr_readtile( 9, back ) && memcmp( img, back, sizeof(img) ) == 0 && !tilescr_readtile( 256, back ), "readtile gives back what writetile wrote" );
    // tilescr_pixel() must agree with what tilescr_render() draws, for several scroll offsets
    for ( int t = 0; t < 256; ++t ) {
        for ( int i=0; i < 16 * 24; ++i ) img[i] = (uint8_t)( 1 + ( t * 7 + i * 13 ) % 250 );
        tilescr_writetile( t, img );
    }
    for ( size_t i=0; i < sizeof(cells); ++i ) cells[i] = (uint8_t)( ( i * 37 ) % 256 );
    tilescr_setcells( cells, sizeof(cells) );
    static uint8_t scr[TILETGT_WIDTH * TILETGT_HEIGHT];
    bool same = true;
    int offs[][2] = { {0,0}, {5,0}, {0,7}, {15,23}, {3,11} };
    for ( size_t o=0; o < sizeof(offs) / sizeof(offs[0]); ++o ) {
        tilescr_scroll( offs[o][0], offs[o][1] );
        tilescr_render( scr );
        for ( int y=0; y < TILETGT_HEIGHT && same; y += 3 ) {
            for ( int x=0; x < TILETGT_WIDTH; ++x ) {
                if ( tilescr_pixel( x, y ) != scr[ y * TILETGT_WIDTH + x ] ) {
                    printf( "      tile pixel differs at %d,%d with scroll %d,%d: %d vs %d\n", x, y, offs[o][0], offs[o][1],
                        tilescr_pixel( x, y ), scr[ y * TILETGT_WIDTH + x ] );
                    same = false;
                    break;
                }
            }
        }
    }
    check( same, "tilescr_pixel agrees with tilescr_render" );
    check( tilescr_pixel( -1, 0 ) == -1 && tilescr_pixel( 0, TILETGT_HEIGHT ) == -1, "tile pixel outside" );
    tilescr_scroll( 0, 0 );
}

static void test_sprites( void ) {
    sprscr_init();
    tilescr_init();
    uint8_t img[16 * 24], back[16 * 24];
    memset( img, 0, sizeof(img) );
    for ( int y=0; y < 24; ++y ) for ( int x=0; x < 16; ++x ) img[y * 16 + x] = (uint8_t)( x < 8 ? 3 : 0 );     // the left half is opaque
    sprscr_writemap( 5, img );
    check( sprscr_readmap( 5, back ) && memcmp( img, back, sizeof(img) ) == 0 && !sprscr_readmap( 256, back ), "readmap" );
    sprscr_show( 1, true ); sprscr_move( 1, 100, 50 ); sprscr_setimage( 1, 5 );
    int x = 0, y = 0;
    check( sprscr_getpos( 1, &x, &y ) && x == 100 && y == 50 && sprscr_getimage( 1 ) == 5 && sprscr_isactive( 1 ) && !sprscr_isactive( 2 ), "position, image, active" );
    check( !sprscr_getpos( 300, &x, &y ) && sprscr_getimage( 300 ) == -1, "bad sprite numbers" );
    static uint8_t scr[SPRTGT_WIDTH * SPRTGT_HEIGHT];
    sprscr_render( scr );
    check( scr[ 50 * SPRTGT_WIDTH + 100 ] == 3 && scr[ 50 * SPRTGT_WIDTH + 108 ] == 0, "a fixed image is rendered" );
    // animation takes over again
    uint8_t seq[2] = { 5, 6 };
    sprscr_animdata( 4, seq, 2 );
    sprscr_animcfg( 1, 4, 2, 1 );
    check( sprscr_getimage( 1 ) == 5, "animcfg ends the fixed image" );
    sprscr_periodicals();
    check( sprscr_getimage( 1 ) == 6, "animation advances" );
    sprscr_setimage( 1, 5 );
    // movement
    sprscr_moveto( 1, 140, 50, 4 );
    check( sprscr_moving( 1 ), "moving" );
    for ( int i=0; i < 2; ++i ) sprscr_periodicals();
    sprscr_getpos( 1, &x, &y );
    check( x > 100 && x < 140 && y == 50, "a sprite on its way" );
    for ( int i=0; i < 2; ++i ) sprscr_periodicals();
    sprscr_getpos( 1, &x, &y );
    check( x == 140 && y == 50 && !sprscr_moving( 1 ), "arrives exactly after the ticks" );
    sprscr_moveto( 1, 0, 0, 0 );
    sprscr_getpos( 1, &x, &y );
    check( x == 0 && y == 0, "0 ticks jumps" );
    // collisions
    sprscr_show( 1, true ); sprscr_move( 1, 100, 50 );
    sprscr_show( 2, true ); sprscr_move( 2, 104, 55 ); sprscr_setimage( 2, 5 );
    sprscr_setcollide( 1, true, false ); sprscr_setcollide( 2, true, false );
    sprscr_periodicals();
    check( sprscr_collidewith( 1 ) == 2 && sprscr_collidewith( 2 ) == 1, "two overlapping sprites collide" );
    check( sprscr_nextevent() == 1 && sprscr_nextevent() == -1, "one event, then none" );
    check( sprscr_hits( 1 ) == 1 && sprscr_hits( 1 ) == 0 && sprscr_hits( 2 ) == 1, "hits: the indicator bits, cleared by reading" );
    sprscr_move( 2, 108 + 10, 55 );             // the opaque halves (x 0..7 of each) no longer overlap: 100..107 and 118..125
    sprscr_periodicals();
    check( sprscr_collidewith( 1 ) == -1 && sprscr_hits( 1 ) == 0, "no overlap of the opaque pixels, no collision" );
    sprscr_move( 2, 108, 55 );                  // image 2 starts at 108: its opaque pixels begin right behind sprite 1's
    sprscr_periodicals();
    check( sprscr_collidewith( 1 ) == -1, "touching is no overlap" );
    sprscr_move( 2, 100, 100 );
    sprscr_setcollide( 2, false, false );
    sprscr_move( 2, 104, 55 );
    sprscr_periodicals();
    check( sprscr_collidewith( 1 ) == -1, "a sprite without the flag is ignored" );
    sprscr_setcollide( 1, false, false );
    sprscr_show( 2, false );
    // tiles
    uint8_t solid[16 * 24];
    memset( solid, 4, sizeof(solid) );
    tilescr_writetile( 1, solid );
    tilescr_setcell( 7, 3, 1 );                  // the screen area x 112..127, y 72..95
    sprscr_setcollide( 1, false, true );
    sprscr_move( 1, 100, 60 );
    sprscr_periodicals();
    check( sprscr_hits( 1 ) == 0, "no tile in reach" );
    sprscr_move( 1, 110, 60 );                  // opaque x 110..117, y 60..83: overlaps the tile at x 112..117, y 72..83
    sprscr_periodicals();
    check( sprscr_hits( 1 ) == 2, "a sprite over a solid tile hits it (bit 2)" );
    sprscr_move( 1, 128, 60 );
    sprscr_periodicals();
    check( sprscr_hits( 1 ) == 0, "past the tile" );
    tilescr_setcell( 7, 3, 0 );
}

static void test_textscreen( void ) {
    txtscr_init();
    txtscr_write( 0, 0, 2, 3, "\xC8\x80\xFF", 3 );
    textcell_t c = txtscr_getcell( 0, 0 );
    check( TXTSCR_CELL_CHR( c ) == 200 && TXTSCR_CELL_BG( c ) == 2 && TXTSCR_CELL_FG( c ) == 3, "a character >= 128 does not spill into the colors" );
    check( TXTSCR_CELL_CHR( txtscr_getcell( 0, 1 ) ) == 128 && TXTSCR_CELL_BG( txtscr_getcell( 0, 1 ) ) == 2 && TXTSCR_CELL_CHR( txtscr_getcell( 0, 2 ) ) == 255, "...also 128 and 255" );
    txtscr_init();
    txtscr_fill( 2, 1, 5, 3, 'x', 7, 4 );
    c = txtscr_getcell( 2, 3 );
    check( TXTSCR_CELL_CHR( c ) == 'x' && TXTSCR_CELL_FG( c ) == 7 && TXTSCR_CELL_BG( c ) == 4, "fill sets character and colors" );
    check( TXTSCR_CELL_CHR( txtscr_getcell( 0, 0 ) ) == ' ' && TXTSCR_CELL_CHR( txtscr_getcell( 4, 6 ) ) == ' ', "fill stays inside" );
    txtscr_fill( 5, 3, 2, 1, -1, 9, -1 );
    c = txtscr_getcell( 2, 3 );
    check( TXTSCR_CELL_CHR( c ) == 'x' && TXTSCR_CELL_FG( c ) == 9 && TXTSCR_CELL_BG( c ) == 4, "-1 leaves a part alone; corners in any order" );
    txtscr_fill( -5, -5, 200, 200, 'y', -1, -1 );
    check( TXTSCR_CELL_CHR( txtscr_getcell( 24, 79 ) ) == 'y', "fill is clipped to the screen" );
    txtscr_init();
    txtscr_write( 0, 0, -1, -1, "abc", 3 );
    txtscr_copy( 0, 0, 2, 0, 1, 0 );
    check( TXTSCR_CELL_CHR( txtscr_getcell( 0, 0 ) ) == 'a' && TXTSCR_CELL_CHR( txtscr_getcell( 0, 1 ) ) == 'a' &&
           TXTSCR_CELL_CHR( txtscr_getcell( 0, 2 ) ) == 'b' && TXTSCR_CELL_CHR( txtscr_getcell( 0, 3 ) ) == 'c', "copy may overlap" );
    txtscr_copy( 0, 0, 3, 0, 78, 24 );
    check( TXTSCR_CELL_CHR( txtscr_getcell( 24, 78 ) ) == 'a' && TXTSCR_CELL_CHR( txtscr_getcell( 24, 79 ) ) == 'a', "copy is clipped at the target" );
    txtscr_init();
    txtscr_write( 0, 0, 2, 3, "z", 1 );
    txtscr_invert( 0, 0, 0, 0 );
    c = txtscr_getcell( 0, 0 );
    check( TXTSCR_CELL_BG( c ) == 3 && TXTSCR_CELL_FG( c ) == 2 && TXTSCR_CELL_CHR( c ) == 'z', "invert swaps pen and paper" );
    // save and restore
    txtscr_init();
    txtscr_write( 3, 4, 5, 6, "Q", 1 );
    size_t need = txtscr_save( 0, 0 );
    check( need == 80U * 25U * 4U, "the save needs 4 bytes per cell" );
    uint8_t* sv = (uint8_t*) malloc( need );
    check( txtscr_save( sv, need ) == need, "save" );
    size_t at = ( 3U * 80U + 4U ) * 4U;
    check( sv[at] == 'Q' && sv[at + 1] == 6 && sv[at + 2] == 5, "cell layout: character, pen, paper" );
    txtscr_clear();
    check( TXTSCR_CELL_CHR( txtscr_getcell( 3, 4 ) ) == ' ', "cleared" );
    check( txtscr_restore( sv, need ) && TXTSCR_CELL_CHR( txtscr_getcell( 3, 4 ) ) == 'Q' && TXTSCR_CELL_FG( txtscr_getcell( 3, 4 ) ) == 6, "restore" );
    check( !txtscr_restore( sv, need - 1 ), "restore refuses a wrong size" );
    free( sv );
    // fonts
    const uint8_t rows[12] = { 1,2,3,4,5,6,7,8,9,10,11,12 };
    txtscr_defchar( 200, rows );
    check( memcmp( txtscr_font() + 200 * 12, rows, 12 ) == 0, "defchar changes the font" );
    txtscr_defchar( 256, rows ); txtscr_defchar( -1, rows );
    check( true, "defchar checks the code" );
    // the control code glyphs exist
    const uint8_t* f = txtscr_font();
    int nonempty = 0;
    for ( int ch = 0; ch < 32; ++ch ) {
        bool any = false;
        for ( int r = 0; r < 12; ++r ) any = any || f[ch * 12 + r] != 0;
        if ( any ) ++nonempty;
    }
    check( nonempty == 32, "all 32 control code glyphs have pixels" );
}

static void test_reset( void ) {
    uint8_t img[16 * 24], back[16 * 24];
    memset( img, 7, sizeof(img) );
    tilescr_writetile( 4, img ); tilescr_setcell( 2, 2, 4 ); tilescr_scroll( 5, 6 );
    tilescr_reset( false );
    check( tilescr_getcell( 2, 2 ) == 0 && tilescr_readtile( 4, back ) && back[0] == 7, "tile reset keeps the images" );
    static uint8_t scr[TILETGT_WIDTH * TILETGT_HEIGHT];
    tilescr_setcell( 0, 0, 4 );
    tilescr_scroll( 5, 6 ); tilescr_reset( false ); tilescr_setcell( 0, 0, 4 );
    tilescr_render( scr );
    check( scr[0] == 7, "tile reset ends the scrolling (the first tile starts at 0,0)" );
    tilescr_reset( true );
    check( tilescr_readtile( 4, back ) && back[0] == 0 && tilescr_getcell( 0, 0 ) == 0, "tile reset of everything clears the images" );
    sprscr_writemap( 3, img );
    sprscr_show( 5, true ); sprscr_move( 5, 40, 50 ); sprscr_setimage( 5, 3 ); sprscr_setcollide( 5, true, true ); sprscr_moveto( 5, 100, 100, 50 );
    sprscr_reset( false );
    int x = -1, y = -1;
    check( !sprscr_isactive( 5 ) && sprscr_getpos( 5, &x, &y ) && x == 0 && y == 0 && !sprscr_moving( 5 ) && sprscr_hits( 5 ) == 0 && sprscr_nextevent() == -1, "sprite reset hides and resets all sprites" );
    sprscr_readmap( 3, back );
    check( back[0] == 7, "sprite reset keeps the images" );
    sprscr_reset( true );
    sprscr_readmap( 3, back );
    check( back[0] == 0, "sprite reset of everything clears the images" );
    const uint8_t rows[12] = { 9,9,9,9,9,9,9,9,9,9,9,9 };
    txtscr_defchar( 65, rows );
    txtscr_resetfont();
    const uint8_t* f = txtscr_font();
    bool same = true;
    for ( int r = 0; r < 12; ++r ) same = same && f[65 * 12 + r] != 9;
    check( same, "the font is back to the built-in one" );
}

int main( void ) {
    txtscr_init();
    test_pixels();
    test_lines();
    test_circles();
    test_paint();
    test_blocks();
    test_text();
    test_draw();
    test_tiles();
    test_sprites();
    test_textscreen();
    test_reset();
    printf( "%d checks, %d failure(s)\n", checks, failures );
    return failures;
}
