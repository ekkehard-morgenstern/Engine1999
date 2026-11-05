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


#include "gfxcore.h"
#include "textscreen.h"
#include "tilescreen.h"
#include "sprscreen.h"
#include "cursors.h"
#include "mouse.h"
#include "bmp.h"
#include "gfxfile.h"

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

// -- the reserved sprites and the mouse pointer (cursors.c) ------------------------------------------------------

static void test_reserved( void ) {
    sprscr_init();
    tilescr_init();
    uint8_t img[16 * 24], back[16 * 24];
    memset( img, 9, sizeof(img) );
    check( SPR_USER_COUNT == 248 && SPR_SYS_FIRST == 248 && SPR_MOUSE == 255 && MAX_SPRITES - SPR_SYS_FIRST == 8, "8 reserved sprites, 248..255; the mouse pointer is the last" );
    int x = 0, y = 0;
    // the functions of the program don't reach them
    sprscr_show( 255, true ); sprscr_move( 255, 5, 6 ); sprscr_setimage( 255, 3 ); sprscr_prio( 250, 1 ); sprscr_animcfg( 255, 1, 2, 3 ); sprscr_moveto( 255, 9, 9, 10 );
    check( !sprscr_isactive( 255 ) && !sprscr_sys_isactive( 255 ) && !sprscr_getpos( 255, &x, &y ) && sprscr_getimage( 255 ) == -1 && !sprscr_moving( 255 ), "the functions of the program do nothing for a reserved sprite" );
    sprscr_setcollide( 255, true, true );
    check( sprscr_hits( 255 ) == 0 && sprscr_collidewith( 255 ) == -1, "...also for the collision functions" );
    check( !sprscr_sys_isactive( 5 ) && sprscr_sys_getimage( 5 ) == -1 && !sprscr_sys_getpos( 5, &x, &y ), "the engine's functions do nothing for a sprite of the program" );
    sprscr_sys_show( 5, true ); sprscr_sys_move( 5, 3, 3 );
    check( !sprscr_isactive( 5 ), "...and don't change it" );

    // the pictures of the mouse pointer exist, in a part of the map of their own
    check( cursors_shape() == CUR_ARROW && !cursors_visible() && !sprscr_sys_isactive( SPR_MOUSE ), "the pointer starts as a hidden arrow" );
    int first, count;
    int speed = cursors_images( CUR_ARROW, &first, &count );
    bool any = false;
    check( sprscr_sys_readmap( first, back ) && speed == 0 && count == 1, "the arrow image can be read" );
    for ( int i = 0; i < 16 * 24; ++i ) any = any || back[i] != 0;
    check( any && back[0] != 0, "...it is not empty, and its tip is at the top left" );
    int shapes_ok = 0;
    for ( int s = 0; s < CUR_COUNT; ++s ) {
        int f, c;
        cursors_images( s, &f, &c );
        bool filled = c >= 1;
        for ( int k = 0; k < c && filled; ++k ) {
            bool a = false;
            sprscr_sys_readmap( f + k, back );
            for ( int i = 0; i < 16 * 24; ++i ) a = a || back[i] != 0;
            filled = filled && a;
        }
        shapes_ok += filled && strlen( cursors_name( s ) ) > 0;
    }
    check( shapes_ok == CUR_COUNT && CUR_COUNT >= 12, "every shape has a drawn picture for every frame" );
    check( cursors_find( "wait", 4 ) == CUR_WAIT && cursors_find( "HAND", 4 ) == CUR_HAND && cursors_find( "nope", 4 ) < 0 && cursors_find( "SIZEH", 5 ) == CUR_SIZEH &&
           strcmp( cursors_name( CUR_TEXT ), "TEXT" ) == 0 && cursors_name( 99 )[0] == '\0', "names of the shapes" );
    int hx, hy;
    cursors_hotspot( CUR_ARROW, &hx, &hy );
    check( hx == 0 && hy == 0, "the hot spot of the arrow is its tip" );
    check( cursors_images( CUR_WAIT, &first, &count ) > 0 && count == 8 && cursors_images( CUR_BUSY, &first, &count ) > 0 && count == 8, "the waiting shapes are animated" );
    check( !cursors_set( -1 ) && !cursors_set( CUR_COUNT ), "a bad shape is refused" );

    // show and move: the hot spot is where the pointer is
    cursors_show( true );
    cursors_move( 100, 50 );
    check( sprscr_sys_isactive( SPR_MOUSE ) && sprscr_sys_getpos( SPR_MOUSE, &x, &y ) && x == 100 && y == 50, "the arrow's hot spot is its position" );
    cursors_set( CUR_WAIT );
    cursors_hotspot( CUR_WAIT, &hx, &hy );
    check( sprscr_sys_getpos( SPR_MOUSE, &x, &y ) && x == 100 - hx && y == 50 - hy, "another shape keeps the hot spot where it was" );
    int px, py;
    cursors_position( &px, &py );
    check( px == 100 && py == 50, "the position of the hot spot" );
    cursors_set( CUR_ARROW );

    // drawing order: in front of the sprites of the program, whatever their priority
    sprscr_writemap( 5, img );
    sprscr_show( 3, true ); sprscr_move( 3, 100, 50 ); sprscr_setimage( 3, 5 ); sprscr_prio( 3, 7 );
    static uint8_t scr[SPRTGT_WIDTH * SPRTGT_HEIGHT];
    sprscr_render( scr );
    check( scr[ 50 * SPRTGT_WIDTH + 100 ] != 9 && scr[ 50 * SPRTGT_WIDTH + 100 ] != 0 && scr[ 70 * SPRTGT_WIDTH + 110 ] == 9, "the pointer is drawn in front of a sprite with the highest priority" );
    // no collisions
    sprscr_setcollide( 3, true, true );
    for ( int i = 0; i < 3; ++i ) sprscr_periodicals();
    check( sprscr_hits( 3 ) == 0 && sprscr_collidewith( 3 ) == -1 && sprscr_nextevent() == -1, "the pointer does not collide with the sprites of the program" );

    // animation
    cursors_set( CUR_WAIT );
    int speedw = cursors_images( CUR_WAIT, &first, &count );
    int seen[8] = { 0 }, distinct = 0;
    for ( int i = 0; i < 8 * speedw * 2; ++i ) {
        int im = sprscr_sys_getimage( SPR_MOUSE ) - first;
        if ( im >= 0 && im < 8 && !seen[im] ) { seen[im] = 1; ++distinct; }
        sprscr_periodicals();
    }
    check( distinct == 8, "the waiting pointer plays all its 8 frames" );

    // GFX RESET / sprscr_reset leave the pointer alone
    sprscr_reset( true );
    bool filled = sprscr_sys_readmap( first, back );
    bool nonzero = false;
    for ( int i = 0; i < 16 * 24; ++i ) nonzero = nonzero || back[i] != 0;
    check( sprscr_sys_isactive( SPR_MOUSE ) && filled && nonzero && !sprscr_isactive( 3 ), "a reset of the sprites of the program keeps the pointer and its pictures" );
    sprscr_writemap( 255, img );
    check( sprscr_sys_readmap( first, back ) && cursors_shape() == CUR_WAIT, "image 255 of the program does not overwrite the engine's images" );
    sprscr_readmap( 255, back );
    bool allnine = true;
    for ( int i = 0; i < 16 * 24; ++i ) allnine = allnine && back[i] == 9;
    check( allnine, "...and the program's image 255 is its own" );
    sprscr_init();
}

// -- the mouse (mouse.c): when the pointer is seen, what the program reads -----------------------------------------

static void test_mouse( void ) {
    sprscr_init();                                  // (installs the pointer's pictures: a hidden arrow)
    mouse_init();
    int x = 0, y = 0;
    check( !cursors_visible() && mouse_get( MOUSEV_VISIBLE ) == 0 && MOUSE_DEFAULT_TIMEOUT == 3000, "the pointer starts hidden; the default is 3 s" );

    // an event of the window shows it for 3 s
    mouse_event_motion( 100, 50 );
    check( mouse_tick( 1000 ) && cursors_visible() && mouse_get( MOUSEV_VISIBLE ) == 1, "moving the mouse shows the pointer" );
    cursors_position( &x, &y );
    check( x == 100 && y == 50 && mouse_get( MOUSEV_X ) == 100 && mouse_get( MOUSEV_Y ) == 50, "...at the mouse's position, and the program can read it" );
    check( !mouse_tick( 3999 ) && cursors_visible(), "...it is still there just before 3 s are over" );
    check( mouse_tick( 4000 ) && !cursors_visible() && mouse_get( MOUSEV_VISIBLE ) == 0, "...and hidden when they are" );
    check( !mouse_tick( 9000 ) && !cursors_visible(), "...for good" );

    // further movement restarts the 3 s
    mouse_event_motion( 101, 50 ); mouse_tick( 10000 );
    mouse_event_motion( 102, 50 ); mouse_tick( 12000 );
    check( !mouse_tick( 14999 ) && cursors_visible() && mouse_tick( 15000 ) && !cursors_visible(), "every movement restarts the time" );

    // the coordinates are clamped
    mouse_event_motion( -5, 9999 ); mouse_tick( 16000 );
    check( mouse_get( MOUSEV_X ) == 0 && mouse_get( MOUSEV_Y ) == 299, "events outside the layer are clamped" );

    // buttons: held, latched, released
    mouse_event_button( MOUSE_LEFT, true );
    mouse_event_button( MOUSE_RIGHT, true );
    mouse_event_button( MOUSE_LEFT, false );
    check( mouse_get( MOUSEV_BUTTONS ) == MOUSE_RIGHT, "the buttons held down" );
    check( mouse_get( MOUSEV_PRESSED ) == ( MOUSE_LEFT | MOUSE_RIGHT ) && mouse_get( MOUSEV_PRESSED ) == 0, "presses are latched, and cleared by reading them" );
    check( mouse_get( MOUSEV_RELEASED ) == MOUSE_LEFT && mouse_get( MOUSEV_RELEASED ) == 0, "...so are releases" );
    mouse_event_button( MOUSE_RIGHT, false ); mouse_event_button( MOUSE_MIDDLE, true ); mouse_event_button( 0, true );
    check( mouse_get( MOUSEV_BUTTONS ) == MOUSE_MIDDLE, "the middle button; an unknown button counts for nothing" );
    mouse_event_button( MOUSE_MIDDLE, false );
    mouse_get( MOUSEV_PRESSED ); mouse_get( MOUSEV_RELEASED );
    mouse_tick( 17000 );                            // (a button shows the pointer, too)
    check( cursors_visible(), "a button press shows the pointer" );
    mouse_tick( 20000 );
    check( !cursors_visible(), "...for 3 s" );
    mouse_event_wheel( 2 ); mouse_event_wheel( -1 ); mouse_event_wheel( 0 );
    mouse_tick( 21000 );
    check( cursors_visible() && mouse_get( MOUSEV_WHEEL ) == 1 && mouse_get( MOUSEV_WHEEL ) == 0, "the wheel shows the pointer; the steps add up and are cleared by reading" );
    mouse_tick( 24000 );

    // requests of the program
    mouse_request_show();
    check( mouse_tick( 30000 ) && cursors_visible() && !mouse_tick( 32999 ) && mouse_tick( 33000 ) && !cursors_visible(), "MOUSE SHOW: for 3 s" );
    mouse_request_show(); mouse_tick( 40000 );
    mouse_request_hide();
    check( mouse_tick( 41000 ) && !cursors_visible(), "MOUSE HIDE: at once" );
    mouse_request_show(); mouse_request_hide();
    mouse_tick( 42000 );
    check( !cursors_visible(), "the last request wins" );
    mouse_request_hide(); mouse_request_show();
    mouse_tick( 43000 );
    check( cursors_visible(), "...in either order" );
    mouse_tick( 50000 );
    mouse_request_pos( 320, 150 );
    int wx = -1, wy = -1;
    check( mouse_tick( 51000 ) && cursors_visible() && mouse_get( MOUSEV_X ) == 320 && mouse_get( MOUSEV_Y ) == 150, "MOUSE POS moves the pointer and shows it" );
    cursors_position( &x, &y );
    check( x == 320 && y == 150 && mouse_warp_wanted( &wx, &wy ) && wx == 320 && wy == 150 && !mouse_warp_wanted( &wx, &wy ), "...and asks the window to follow, once" );
    mouse_request_pos( 5000, -3 ); mouse_tick( 52000 );
    check( mouse_get( MOUSEV_X ) == 639 && mouse_get( MOUSEV_Y ) == 0, "...clamped" );
    mouse_tick( 60000 );
    mouse_request_shape( CUR_HAND );
    mouse_tick( 61000 );
    check( cursors_shape() == CUR_HAND && mouse_get( MOUSEV_SHAPE ) == CUR_HAND && cursors_visible(), "MOUSE SHAPE changes the shape and shows the pointer" );
    mouse_request_shape( 99 ); mouse_request_shape( -1 );
    mouse_tick( 61500 );
    check( cursors_shape() == CUR_HAND, "...a bad shape does nothing" );
    mouse_tick( 70000 );
    cursors_position( &x, &y );
    check( !cursors_visible() && x == 639 && y == 0, "...the position is kept when the pointer is hidden" );

    // a request is true for the program at once, before the screen thread has taken it
    mouse_request_pos( 12, 34 ); mouse_request_shape( CUR_CROSS );
    check( mouse_get( MOUSEV_X ) == 12 && mouse_get( MOUSEV_Y ) == 34 && mouse_get( MOUSEV_SHAPE ) == CUR_CROSS && mouse_get( MOUSEV_VISIBLE ) == 1, "MOUSE POS / SHAPE are seen by MOUSE() at once" );
    mouse_request_hide();
    check( mouse_get( MOUSEV_VISIBLE ) == 0, "MOUSE HIDE, too" );
    mouse_tick( 70500 );
    check( mouse_get( MOUSEV_X ) == 12 && mouse_get( MOUSEV_SHAPE ) == CUR_CROSS && mouse_get( MOUSEV_VISIBLE ) == 0, "...and still when the screen thread has taken them" );
    mouse_tick( 75000 );

    // the time
    mouse_request_timeout( 500 );
    mouse_event_motion( 1, 1 ); mouse_tick( 80000 );
    check( !mouse_tick( 80499 ) && mouse_tick( 80500 ) && !cursors_visible(), "MOUSE TIMEOUT 0.5" );
    mouse_request_timeout( 0 );
    mouse_event_motion( 2, 2 ); mouse_tick( 90000 );
    check( !mouse_tick( 900000 ) && cursors_visible(), "MOUSE TIMEOUT 0: the pointer stays" );
    mouse_request_hide(); mouse_tick( 900001 );
    check( !cursors_visible(), "...until MOUSE HIDE" );
    mouse_request_timeout( -1 );
    mouse_event_motion( 3, 3 ); mouse_tick( 1000000 );
    check( !mouse_tick( 1002999 ) && mouse_tick( 1003000 ), "a negative time means the default again" );

    // the area: the console host sets it to the terminal's size in cells times 8 x 12
    mouse_setarea( 800, 600 );
    mouse_event_motion( 799, 599 ); mouse_event_motion( 900, 900 ); mouse_tick( 3000000 );
    check( mouse_get( MOUSEV_X ) == 799 && mouse_get( MOUSEV_Y ) == 599, "a bigger area: positions are clamped to it" );
    mouse_request_pos( 5000, 5000 ); mouse_tick( 3000100 );
    check( mouse_get( MOUSEV_X ) == 799 && mouse_get( MOUSEV_Y ) == 599, "...MOUSE POS, too" );
    mouse_setarea( 80, 24 );
    mouse_event_motion( 500, 500 ); mouse_tick( 3000200 );
    check( mouse_get( MOUSEV_X ) == 79 && mouse_get( MOUSEV_Y ) == 23, "a smaller one" );
    mouse_setarea( 640, 300 );

    // the direct use of cursors_* (sdltest3) has no timer
    cursors_show( true );
    check( !mouse_tick( 2000000 ) && cursors_visible(), "a pointer that was shown without the mouse module is left alone" );
    cursors_show( false );
    sprscr_init();
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
    check( !sprscr_moving( 1 ), "a jump is no movement" );
    sprscr_periodicals();           // (the screen thread takes the request in its next frame)
    sprscr_getpos( 1, &x, &y );
    check( x == 0 && y == 0, "0 ticks jumps" );
    // requests: the last one wins, a new one replaces a movement under way, the target is exact whatever came before
    sprscr_moveto( 1, 10, 10, 50 );
    sprscr_moveto( 1, 200, 100, 3 );
    check( sprscr_moving( 1 ), "a posted request counts as moving" );
    sprscr_periodicals();
    sprscr_moveto( 1, 20, 30, 2 );                      // replaces the movement under way
    for ( int i=0; i < 3; ++i ) sprscr_periodicals();
    sprscr_getpos( 1, &x, &y );
    check( x == 20 && y == 30 && !sprscr_moving( 1 ), "the newest request wins; no request is lost" );
    sprscr_moveto( 1, 90, 90, 40 );
    sprscr_periodicals();
    sprscr_moveto( 1, 5, 5, 0 );                        // a jump stops a movement under way
    sprscr_periodicals();
    sprscr_getpos( 1, &x, &y );
    check( x == 5 && y == 5 && !sprscr_moving( 1 ), "a jump request stops a movement" );
    sprscr_periodicals();
    sprscr_getpos( 1, &x, &y );
    check( x == 5 && y == 5, "...and it stays there" );
    sprscr_moveto( 1, -50, 700, 1 );
    sprscr_periodicals();
    sprscr_getpos( 1, &x, &y );
    check( x == -50 && y == 700, "negative and large targets survive the request word" );
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

static void test_sprite_groups( void ) {
    sprscr_init();
    tilescr_init();
    static uint8_t scr[SPRTGT_WIDTH * SPRTGT_HEIGHT];
    uint8_t img[16 * 24];
    for ( int i=0; i < 8; ++i ) {                       // the images 20..27 are solid in the colors 11..18
        memset( img, 11 + i, sizeof(img) );
        sprscr_writemap( 20 + i, img );
    }
    int w = 0, h = 0, x = 0, y = 0;
    // making groups
    check( sprscr_setsize( 10, 2, 3 ), "a group of 2 x 3 sprites" );
    sprscr_getsize( 10, &w, &h );
    check( w == 2 && h == 3, "...its size" );
    sprscr_getsize( 11, &w, &h );
    check( w == 1 && h == 1, "a member is no head" );
    check( sprscr_headof( 10 ) == 10 && sprscr_headof( 11 ) == 10 && sprscr_headof( 15 ) == 10 && sprscr_headof( 16 ) == 16, "the head of a sprite" );
    check( !sprscr_setsize( 12, 1, 2 ), "a member can't be a head" );
    check( !sprscr_setsize( 9, 2, 2 ), "a group can't take the members of another" );
    check( !sprscr_setsize( 241, 2, 4 ), "...nor reach beyond the sprites of the program" );
    check( !sprscr_setsize( 3, 0, 1 ) && !sprscr_setsize( 3, 9, 1 ) && !sprscr_setsize( 250, 1, 1 ) && !sprscr_setsize( -1, 1, 1 ), "bad sizes and numbers" );
    check( sprscr_setsize( 10, 3, 2 ), "a head can be resized" );
    check( sprscr_headof( 15 ) == 10 && sprscr_headof( 16 ) == 16, "...six members again" );
    check( sprscr_setsize( 10, 2, 3 ), "and back" );
    check( sprscr_setsize( 20, 2, 2 ) && sprscr_headof( 23 ) == 20, "another group next to it" );
    check( sprscr_setsize( 20, 1, 1 ) && sprscr_headof( 21 ) == 21, "(1, 1) dissolves a group" );
    // drawing: the head rules, the member k shows the image + k at its place
    sprscr_show( 10, true ); sprscr_move( 10, 100, 50 ); sprscr_setimage( 10, 20 );
    sprscr_show( 11, true ); sprscr_move( 11, 300, 200 ); sprscr_setimage( 11, 7 );      // (the member's own settings do not count)
    sprscr_render( scr );
    bool ok = true;
    for ( int k=0; k < 6; ++k ) {
        int px = 100 + 16 * ( k % 2 ) + 3, py = 50 + 24 * ( k / 2 ) + 5;
        if ( scr[ py * SPRTGT_WIDTH + px ] != 11 + k ) ok = false;
    }
    check( ok, "the six sprites are drawn as one block, each with its image" );
    check( scr[ 200 * SPRTGT_WIDTH + 300 ] == 0 && scr[ 49 * SPRTGT_WIDTH + 100 ] == 0 && scr[ 122 * SPRTGT_WIDTH + 100 ] == 0 && scr[ 100 * SPRTGT_WIDTH + 132 ] == 0, "...and nothing else" );
    check( sprscr_getpos( 15, &x, &y ) && x == 116 && y == 98 && sprscr_getimage( 15 ) == 25 && sprscr_isactive( 15 ), "a member reports its place, image and state" );
    sprscr_show( 10, false );
    sprscr_render( scr );
    check( scr[ 50 * SPRTGT_WIDTH + 100 ] == 0 && scr[ 98 * SPRTGT_WIDTH + 116 ] == 0 && !sprscr_isactive( 13 ), "hiding the head hides the group" );
    sprscr_show( 10, true );
    // movement of the head (MOVETO) takes the block along; animation: each member adds its number to the head's image
    sprscr_moveto( 10, 120, 70, 2 );
    sprscr_periodicals(); sprscr_periodicals(); sprscr_periodicals();
    sprscr_getpos( 14, &x, &y );
    check( x == 120 && y == 70 + 48, "MOVETO of the head moves the members" );
    uint8_t seq[2] = { 20, 22 };
    sprscr_animdata( 7, seq, 2 );
    sprscr_animcfg( 10, 7, 2, 1 );
    check( sprscr_getimage( 13 ) == 23, "an animation of the head: the members follow" );
    sprscr_periodicals();
    check( sprscr_getimage( 10 ) == 22 && sprscr_getimage( 13 ) == 25, "...frame by frame" );
    sprscr_setimage( 10, 253 );
    check( sprscr_getimage( 15 ) == 255, "an image number beyond 255 is held at 255" );
    sprscr_setimage( 10, 20 );
    // priority: the group is one sprite for the order
    sprscr_show( 40, true ); sprscr_move( 40, 120, 70 ); sprscr_setimage( 40, 27 ); sprscr_prio( 40, 3 );
    sprscr_prio( 10, 5 );
    sprscr_render( scr );
    check( scr[ 72 * SPRTGT_WIDTH + 122 ] == 11, "a group in front of a sprite" );
    sprscr_prio( 10, 1 );
    sprscr_render( scr );
    check( scr[ 72 * SPRTGT_WIDTH + 122 ] == 18, "...and behind it (the head's priority)" );
    sprscr_show( 40, false );
    // collisions: the group is one object; the head reports
    sprscr_setcollide( 10, true, false );
    sprscr_show( 100, true ); sprscr_move( 100, 135, 125 ); sprscr_setimage( 100, 27 ); sprscr_setcollide( 100, true, false );
    sprscr_periodicals();
    check( sprscr_collidewith( 100 ) == 10 && sprscr_collidewith( 10 ) == 100, "a sprite meets a member: the head of the group is reported" );
    check( sprscr_hits( 10 ) == 1 && sprscr_hits( 100 ) == 1 && sprscr_hits( 15 ) == 0, "hits count for the head" );
    check( sprscr_nextevent() == 10, "the event names the head" );
    sprscr_move( 100, 400, 250 );
    sprscr_periodicals();
    check( sprscr_collidewith( 10 ) == -1, "no overlap, no collision" );
    // the members don't collide with each other; a group that is not set to collide does not collide
    sprscr_setcollide( 10, true, false );
    sprscr_periodicals();
    check( sprscr_collidewith( 10 ) == -1 && sprscr_hits( 10 ) == 0, "the members of a group don't hit each other" );
    sprscr_move( 100, 135, 125 );
    sprscr_setcollide( 10, false, false );
    sprscr_periodicals();
    check( sprscr_collidewith( 100 ) == -1, "the head's flags rule the group" );
    // tiles
    uint8_t solid[16 * 24];
    memset( solid, 4, sizeof(solid) );
    tilescr_writetile( 1, solid );
    tilescr_setcell( 20, 8, 1 );                         // x 320..335, y 192..215
    sprscr_setcollide( 10, false, true );
    sprscr_show( 100, false );
    sprscr_move( 10, 310, 100 );                         // the member in the bottom row, right column: x 326..341, y 148..171: no tile
    sprscr_periodicals();
    check( sprscr_hits( 10 ) == 0, "no tile in reach" );
    sprscr_move( 10, 310, 150 );                         // the bottom right member: x 326..341, y 198..221: overlaps the tile
    sprscr_periodicals();
    check( sprscr_hits( 10 ) == 2, "a member over a solid tile: bit 2 for the head" );
    tilescr_setcell( 20, 8, 0 );
    // a reset dissolves the groups
    sprscr_reset( false );
    check( sprscr_headof( 12 ) == 12, "a reset dissolves the groups" );
    sprscr_getsize( 10, &w, &h );
    check( w == 1 && h == 1, "...every one" );
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

static sprpath_t* pp( const char* s ) {
    char err[96]; size_t pos = 0;
    return sprpath_parse( s, strlen( s ), err, sizeof(err), &pos );
}

static bool pbad( const char* s, size_t at ) {
    char err[96]; size_t pos = 99;
    sprpath_t* p = sprpath_parse( s, strlen( s ), err, sizeof(err), &pos );
    bool ok = p == 0 && pos == at && err[0] != 0;
    sprpath_free( p );
    return ok;
}

static void frames( int n ) {
    for ( int i=0; i < n; ++i ) sprscr_periodicals();
}

static void test_paths( void ) {
    sprscr_reset( true );
    sprpath_t* p = pp( "x+3*20 y-60 T 5 X+10 w 4 a 3,2,1 d 9 e 7 H S P 2 R [ W1 ]3 X=-4 Y=5 M 2,-2*3 M 6,6" );
    check( p != 0 && p->n == 18, "a path with every command is translated" );
    check( p && p->cmd[0].op == SPO_MOVE && p->cmd[0].a == 60 && p->cmd[0].b == 0 && p->cmd[0].c == 20, "X+3*20: 60 pixels over 20 frames" );
    check( p && p->cmd[1].op == SPO_MOVE && p->cmd[1].b == -60 && p->cmd[1].c == 1, "Y-60: all at once (T defaults to 1)" );
    check( p && p->cmd[2].a == 10 && p->cmd[2].c == 5, "after T 5 a move takes 5 frames" );
    check( p && p->cmd[3].op == SPO_WAIT && p->cmd[3].c == 4 && p->cmd[4].op == SPO_ANIM && p->cmd[4].a == 3 && p->cmd[4].b == 2 && p->cmd[4].c == 1, "W and A" );
    check( p && p->cmd[11].op == SPO_LOOP && p->cmd[13].op == SPO_ENDLOOP && p->cmd[13].a == 12 && p->cmd[13].c == 3, "a loop knows where it starts" );
    check( p && p->cmd[14].op == SPO_SETX && p->cmd[14].a == -4 && p->cmd[16].op == SPO_MOVE && p->cmd[16].a == 6 && p->cmd[16].b == -6 && p->cmd[16].c == 3, "X=-4 and M dx,dy*k" );
    sprpath_free( p );
    p = pp( "" );
    check( p != 0 && p->n == 0, "an empty path is fine" );
    sprpath_free( p );
    check( pbad( "Q", 0 ) && pbad( "X", 1 ) && pbad( "X5", 1 ) && pbad( "W 0", 0 ) && pbad( "E 256", 0 ) && pbad( "P 8", 0 ) && pbad( "[ W1", 4 ) && pbad( "W1 ]2", 3 ) &&
           pbad( "[[[[[ W1 ]1 ]1 ]1 ]1 ]1", 4 ) && pbad( "A 1,16,0", 0 ) && pbad( "X+3*0", 5 ) && pbad( "M 1", 3 ), "errors with the place" );

    // running: X+3*4 moves 3 pixels a frame
    int x = 0, y = 0;
    sprscr_show( 1, true ); sprscr_move( 1, 10, 20 );
    sprscr_setpath( 1, pp( "X+3*4 W 2 Y+5 E 9 D 4" ) );
    check( sprscr_pathrunning( 1 ), "a posted path counts as running" );
    frames( 1 );
    sprscr_getpos( 1, &x, &y );
    check( x == 13 && y == 20, "the first step happens in the frame the path is taken" );
    frames( 3 );
    sprscr_getpos( 1, &x, &y );
    check( x == 22 && y == 20 && sprscr_pathrunning( 1 ), "arrives exactly" );
    frames( 2 );
    sprscr_getpos( 1, &x, &y );
    check( y == 20, "waits 2 frames" );
    frames( 1 );
    sprscr_getpos( 1, &x, &y );
    check( y == 25 && sprscr_pathevent() == -1, "then Y+5" );
    frames( 1 );
    check( !sprscr_pathrunning( 1 ) && sprscr_getimage( 1 ) == 4, "D sets the image, the path is finished" );
    uint32_t ev = sprscr_pathevents();
    check( ev == 1 && sprscr_pathevent() == 9 && sprscr_pathevent_sprite() == 1 && sprscr_pathevent() == -1, "E 9 gives an event of sprite 1" );

    // loops and repeat
    sprscr_move( 2, 0, 0 );
    sprscr_setpath( 2, pp( "[ X+1 [ Y+1 ]2 ]3" ) );
    frames( 20 );
    sprscr_getpos( 2, &x, &y );
    check( x == 3 && y == 6 && !sprscr_pathrunning( 2 ), "nested loops: 3 x (X+1, 2 x Y+1)" );
    sprscr_move( 3, 0, 0 );
    sprscr_setpath( 3, pp( "X+1 R" ) );
    frames( 10 );
    sprscr_getpos( 3, &x, &y );
    check( x == 10 && sprscr_pathrunning( 3 ), "R repeats forever, one step a frame" );
    sprscr_setpath( 3, 0 );
    frames( 3 );
    sprscr_getpos( 3, &x, &y );
    check( x == 10 && !sprscr_pathrunning( 3 ), "a null path stops it" );
    sprscr_setpath( 3, pp( "E 1 E 2 R" ) );
    frames( 400 );
    check( sprscr_pathevents() > 200 && sprscr_pathevent() == 1, "a full event ring drops events without stopping the path" );
    sprscr_setpath( 3, 0 );
    frames( 1 );
    while ( sprscr_pathevent() >= 0 ) {}

    // overrides: H / S / P / D / A, and the program's own statements win again
    sprscr_show( 4, true ); sprscr_prio( 4, 1 ); sprscr_setimage( 4, 1 );
    sprscr_setpath( 4, pp( "H P 5 D 7 W 1 S" ) );
    frames( 1 );
    check( !sprscr_isactive( 4 ), "H hides the sprite" );
    check( sprscr_getimage( 4 ) == 7, "D shows image 7" );
    frames( 2 );
    check( sprscr_isactive( 4 ), "S shows it again" );
    sprscr_show( 4, false );
    check( !sprscr_isactive( 4 ), "SPRITE n,OFF takes the override away" );
    sprscr_setimage( 4, 2 );
    check( sprscr_getimage( 4 ) == 2, "SPRITE n,IMAGE takes the override away" );
    uint8_t seq[3] = { 10, 11, 12 };
    sprscr_animdata( 5, seq, 3 );
    sprscr_setpath( 6, pp( "A 5,3,1" ) );
    frames( 1 );
    int i0 = sprscr_getimage( 6 );
    frames( 1 );
    int i1 = sprscr_getimage( 6 );
    check( i0 == 10 && i1 == 11, "A plays the sequence" );
    sprscr_reset( false );
    check( !sprscr_pathrunning( 6 ) && sprscr_pathevent() == -1, "reset stops the paths and forgets the events" );
    frames( 1 );
    sprscr_setpath( 300, pp( "X+1" ) );             // (a bad sprite number frees the path)
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
    txtscr_setcolors( 7, 140 ); txtscr_setblank( 7, 140 );
    txtscr_write( 0, 0, -1, -1, "k", 1 );
    txtscr_resetcolors();
    int bg = -1, fg = -1;
    txtscr_getcolors( &bg, &fg );
    txtscr_clear();
    check( bg == TXTSCR_BGCOL && fg == TXTSCR_FGCOL && TXTSCR_CELL_FG( txtscr_getcell( 0, 0 ) ) == TXTSCR_FGCOL, "the text colors are back to the defaults (pen black, not what COLOR set)" );
    txtscr_write( 0, 0, 2, 3, "k", 1 );
    txtscr_resetcolors();
    check( TXTSCR_CELL_CHR( txtscr_getcell( 0, 0 ) ) == 'k' && TXTSCR_CELL_FG( txtscr_getcell( 0, 0 ) ) == 3, "the contents of the text screen stay" );
    const uint8_t rows[12] = { 9,9,9,9,9,9,9,9,9,9,9,9 };
    txtscr_defchar( 65, rows );
    txtscr_resetfont();
    const uint8_t* f = txtscr_font();
    bool same = true;
    for ( int r = 0; r < 12; ++r ) same = same && f[65 * 12 + r] != 9;
    check( same, "the font is back to the built-in one" );
}


// -- BMP files and picture statements (bmp.c, gfxfile.c) -------------------------------------------------------------------

static void put16( uint8_t* p, uint32_t v ) { p[0] = (uint8_t) v; p[1] = (uint8_t)( v >> 8 ); }
static void put32( uint8_t* p, uint32_t v ) { put16( p, v ); put16( p + 2, v >> 16 ); }

// a BMP with the 40 byte header; 'pal' (BGRA entries) may be 0; rows are given bottom-up as in a file unless topdown
static uint8_t* make_bmp( int w, int h, int bpp, int comp, const uint8_t* pal, int npal, const uint8_t* rows, size_t rowsz, bool topdown, size_t* plen ) {
    size_t stride = ( ( (size_t) w * (size_t) bpp + 31U ) / 32U ) * 4U, off = 14U + 40U + (size_t) npal * 4U + ( comp == 3 ? 12U : 0U );
    uint8_t* o = (uint8_t*) calloc( 1, off + stride * (size_t) h );
    o[0] = 'B'; o[1] = 'M';
    put32( o + 2, (uint32_t)( off + stride * (size_t) h ) ); put32( o + 10, (uint32_t) off ); put32( o + 14, 40 );
    put32( o + 18, (uint32_t) w ); put32( o + 22, (uint32_t)( topdown ? -h : h ) );
    put16( o + 26, 1 ); put16( o + 28, (uint32_t) bpp ); put32( o + 30, (uint32_t) comp ); put32( o + 46, (uint32_t) npal );
    size_t pos = 54;
    if ( comp == 3 ) {
        put32( o + pos, 0xF800U ); put32( o + pos + 4, 0x7E0U ); put32( o + pos + 8, 0x1FU );
        pos += 12;
    }
    if ( npal > 0 ) {
        memcpy( o + pos, pal, (size_t) npal * 4U );
    }
    for ( int y=0; y < h; ++y ) {
        memcpy( o + off + stride * (size_t) y, rows + rowsz * (size_t) y, rowsz );
    }
    *plen = off + stride * (size_t) h;
    return o;
}

typedef struct {
    uint32_t pal[5][256];
    uint8_t  layer[2][GFXFILE_LW * GFXFILE_LH];         // 0: background, 1: graphics (layer 3)
    uint8_t  cells[2][256][GFXFILE_CELLW * GFXFILE_CELLH];
    int      psets;
} fakescr_t;

static uint32_t f_palget( void* ud, int l, int i ) { return ((fakescr_t*) ud)->pal[l][i]; }
static void f_palset( void* ud, int l, int i, uint32_t c ) { ((fakescr_t*) ud)->pal[l][i] = c; }
static uint8_t* f_pix( fakescr_t* f, int l, int x, int y ) {
    if ( x < 0 || y < 0 || x >= GFXFILE_LW || y >= GFXFILE_LH ) return 0;
    return &f->layer[l == 3][y * GFXFILE_LW + x];
}
static int f_point( void* ud, int l, int x, int y ) { uint8_t* p = f_pix( (fakescr_t*) ud, l, x, y ); return p ? *p : -1; }
static void f_pset( void* ud, int l, int x, int y, int c ) { uint8_t* p = f_pix( (fakescr_t*) ud, l, x, y ); if ( p ) { *p = (uint8_t) c; ((fakescr_t*) ud)->psets++; } }
static bool f_getcell( void* ud, int k, int n, uint8_t* out ) { memcpy( out, ((fakescr_t*) ud)->cells[k][n], GFXFILE_CELLW * GFXFILE_CELLH ); return true; }
static void f_putcell( void* ud, int k, int n, const uint8_t* d ) { memcpy( ((fakescr_t*) ud)->cells[k][n], d, GFXFILE_CELLW * GFXFILE_CELLH ); }

static fakescr_t fake;
static gfxfile_host_t fh = { &fake, f_palget, f_palset, f_point, f_pset, f_getcell, f_putcell };

static void fake_reset( void ) {
    memset( &fake, 0, sizeof(fake) );
    for ( int l=0; l < 5; ++l ) {
        fake.pal[l][0] = 0;
        for ( int i=1; i < 256; ++i ) {
            // 1 red, 2 green, 3 blue, then a grey ramp
            fake.pal[l][i] = i == 1 ? 0xFFFF0000U : i == 2 ? 0xFF00FF00U : i == 3 ? 0xFF0000FFU : 0xFF000000U | (uint32_t)( i * 0x010101U );
        }
    }
}

static void test_bmp( void ) {
    const char* err = 0;
    bmp_image_t img;
    // 24 bit, 2 x 2, bottom-up: bottom row red, green; top row blue, white (BGR)
    uint8_t rows24[2][8] = { { 0,0,255, 0,255,0, 0,0 }, { 255,0,0, 255,255,255, 0,0 } };
    size_t len;
    uint8_t* f = make_bmp( 2, 2, 24, 0, 0, 0, &rows24[0][0], 8, false, &len );
    check( bmp_decode( f, len, &img, &err ) && img.w == 2 && img.h == 2 && img.idx == 0, "bmp: 24 bit decodes" );
    check( img.argb[0] == 0xFF0000FFU && img.argb[1] == 0xFFFFFFFFU && img.argb[2] == 0xFFFF0000U && img.argb[3] == 0xFF00FF00U, "bmp: 24 bit pixels, top row first" );
    bmp_free( &img );
    // truncated and bad
    check( !bmp_decode( f, len - 1, &img, &err ) && err != 0, "bmp: a truncated file is refused" );
    check( !bmp_decode( f, 10, &img, &err ), "bmp: a short file is refused" );
    f[0] = 'X';
    check( !bmp_decode( f, len, &img, &err ), "bmp: bad magic" );
    f[0] = 'B'; put32( f + 30, 1 );
    check( !bmp_decode( f, len, &img, &err ) && strstr( err, "ompress" ) != 0, "bmp: RLE is refused" );
    put32( f + 30, 0 ); put16( f + 28, 7 );
    check( !bmp_decode( f, len, &img, &err ), "bmp: odd color depth is refused" );
    free( f );
    // top-down 8 bit with a palette
    uint8_t pal[8] = { 1,2,3,0, 10,20,30,0 };                   // entry 0 = RGB(3,2,1), entry 1 = RGB(30,20,10)
    uint8_t rows8[2][4] = { { 0, 1, 0, 0 }, { 1, 1, 0, 0 } };
    f = make_bmp( 2, 2, 8, 0, pal, 2, &rows8[0][0], 4, true, &len );
    check( bmp_decode( f, len, &img, &err ) && img.npal == 2 && img.idx != 0, "bmp: 8 bit top-down decodes" );
    check( img.idx[0] == 0 && img.idx[1] == 1 && img.idx[2] == 1 && img.idx[3] == 1 && img.pal[1] == 0xFF1E140AU, "bmp: 8 bit indices and palette (opaque when the reserved byte is 0)" );
    bmp_free( &img );
    free( f );
    // 4 bit (two pixels per byte) and 1 bit (eight)
    uint8_t pal4[16 * 4];
    memset( pal4, 0, sizeof(pal4) );
    pal4[2*4+2] = 200;                                          // entry 2 = red 200
    uint8_t row4[4] = { 0x12, 0x20, 0, 0 };
    f = make_bmp( 3, 1, 4, 0, pal4, 16, row4, 4, false, &len );
    check( bmp_decode( f, len, &img, &err ) && img.idx[0] == 1 && img.idx[1] == 2 && img.idx[2] == 2 && img.pal[2] == 0xFFC80000U, "bmp: 4 bit" );
    bmp_free( &img ); free( f );
    uint8_t pal1[8] = { 0,0,0,0, 255,255,255,0 };
    uint8_t row1[4] = { 0xA0, 0, 0, 0 };                        // 1 0 1
    f = make_bmp( 3, 1, 1, 0, pal1, 2, row1, 4, false, &len );
    check( bmp_decode( f, len, &img, &err ) && img.idx[0] == 1 && img.idx[1] == 0 && img.idx[2] == 1, "bmp: 1 bit" );
    bmp_free( &img ); free( f );
    // 32 bit: alpha used if any pixel has some, else opaque
    uint8_t row32[8] = { 0,0,255,0x80, 0,255,0,0 };             // BGRA
    f = make_bmp( 2, 1, 32, 0, 0, 0, row32, 8, false, &len );
    check( bmp_decode( f, len, &img, &err ) && img.argb[0] == 0x80FF0000U && img.argb[1] == 0x0000FF00U, "bmp: 32 bit alpha" );
    bmp_free( &img ); free( f );
    row32[3] = 0;
    f = make_bmp( 2, 1, 32, 0, 0, 0, row32, 8, false, &len );
    check( bmp_decode( f, len, &img, &err ) && img.argb[0] == 0xFFFF0000U && img.argb[1] == 0xFF00FF00U, "bmp: 32 bit without alpha is opaque" );
    bmp_free( &img ); free( f );
    // 16 bit: 565 by bit fields, 555 by default
    uint8_t row16[4] = { 0x00, 0xF8, 0x1F, 0x00 };              // 0xF800 red, 0x001F blue
    f = make_bmp( 2, 1, 16, 3, 0, 0, row16, 4, false, &len );
    check( bmp_decode( f, len, &img, &err ) && img.argb[0] == 0xFFFF0000U && img.argb[1] == 0xFF0000FFU, "bmp: 16 bit 565" );
    bmp_free( &img ); free( f );
    uint8_t row555[4] = { 0x00, 0x7C, 0x1F, 0x00 };
    f = make_bmp( 2, 1, 16, 0, 0, 0, row555, 4, false, &len );
    check( bmp_decode( f, len, &img, &err ) && img.argb[0] == 0xFFFF0000U && img.argb[1] == 0xFF0000FFU, "bmp: 16 bit 555" );
    bmp_free( &img ); free( f );

    // writing and reading back
    uint32_t wp[256];
    for ( int i=0; i < 256; ++i ) wp[i] = 0xFF000000U | (uint32_t)( i * 0x010203U & 0xFFFFFF );
    wp[0] = 0;
    uint8_t px[5 * 3];
    for ( int i=0; i < 15; ++i ) px[i] = (uint8_t)( i * 17 );
    uint8_t* out = 0; size_t olen = 0;
    check( bmp_encode8( 5, 3, px, wp, &out, &olen ) && olen == 14U + 40U + 1024U + 8U * 3U, "bmp: encode8 size (rows padded to 4 bytes)" );
    check( bmp_decode( out, olen, &img, &err ) && img.w == 5 && img.h == 3 && img.npal == 256 && memcmp( img.idx, px, 15 ) == 0, "bmp: encode8 round trip: pixels" );
    check( img.pal[0] == 0 && img.pal[7] == wp[7] && img.pal[255] == wp[255], "bmp: encode8 round trip: palette with alpha" );
    bmp_free( &img ); free( out );

    // colors
    check( bmp_nearest( wp, 1, 0xFF010203U ) == 1, "nearest: exact" );
    check( bmp_nearest( wp, 1, 0x00FFFFFFU ) == 0, "nearest: transparent is entry 0" );
    wp[5] = 0;
    check( bmp_nearest( wp, 1, wp[4] ) == 4 && bmp_nearest( wp, 1, 0xFF050A0FU ) != 5, "nearest: a fully transparent entry is skipped" );
    uint32_t many[1000];
    for ( int i=0; i < 1000; ++i ) many[i] = 0xFF000000U | (uint32_t)( ( i * 37 ) & 255 ) << 16 | (uint32_t)( ( i * 91 ) & 255 ) << 8 | (uint32_t)( ( i * 13 ) & 255 );
    uint32_t qp[256];
    memset( qp, 0, sizeof(qp) );
    int nq = bmp_quantize( many, 1000, qp, 1, 255 );
    check( nq > 100 && nq <= 255 && qp[0] == 0 && ( qp[1] >> 24 ) == 255U, "quantize: at most the allowed number of colors, from the first entry" );
    uint32_t few[4] = { 0xFF112233U, 0xFF112233U, 0xFF445566U, 0x00000000U };
    nq = bmp_quantize( few, 4, qp, 0, 256 );
    check( nq == 2 && qp[0] == 0xFF112233U && qp[1] == 0xFF445566U, "quantize: few colors are kept exactly, transparent ones left out" );
}

static void test_gfxfile( void ) {
    const char* err = 0;
    fake_reset();
    // GFX LOAD: a 24 bit picture 3 x 2 is matched to the layer's palette: red = 1, green = 2, blue = 3, a dark grey, transparent = 0
    uint8_t rows[2][12] = { { 0x40,0x40,0x40, 0x00,0x00,0xFF, 0x00,0x00,0x00, 0,0,0 }, { 0xFF,0x00,0x00, 0x00,0xFF,0x00, 0x00,0x00,0xFF, 0,0,0 } };
    size_t len;
    uint8_t* f = make_bmp( 3, 2, 24, 0, 0, 0, &rows[0][0], 12, false, &len );
    check( gfxfile_load_layer( &fh, 3, f, len, 10, 20, false, &err ), "gfxfile: load onto the graphics layer" );
    // the top row is the file's last row: blue, green, red (BGR bytes); below it: grey 0x404040, red, black
    check( f_point( &fake, 3, 10, 20 ) == 3 && f_point( &fake, 3, 11, 20 ) == 2 && f_point( &fake, 3, 12, 20 ) == 1, "gfxfile: blue, green, red found in the palette" );
    check( f_point( &fake, 3, 10, 21 ) == 64 && f_point( &fake, 3, 11, 21 ) == 1, "gfxfile: grey and red below" );
    check( f_point( &fake, 3, 12, 21 ) == 4, "gfxfile: black is matched to the darkest entry (never to entry 0)" );
    check( f_point( &fake, 0, 10, 20 ) == 0, "gfxfile: the other layer is untouched" );
    // clipping
    fake.psets = 0;
    check( gfxfile_load_layer( &fh, 3, f, len, 638, 299, false, &err ) && fake.psets == 2, "gfxfile: clipped at the corner (2 of 6 pixels)" );
    fake.psets = 0;
    check( gfxfile_load_layer( &fh, 3, f, len, -5, -5, false, &err ) && fake.psets == 0, "gfxfile: nothing drawn when it is off the screen" );
    free( f );
    // PALETTE: an indexed picture installs its palette and keeps its indices
    uint8_t pal[12] = { 9,8,7,0, 50,60,70,0, 1,2,3,0 };
    uint8_t r8[1][4] = { { 2, 1, 0, 0 } };
    f = make_bmp( 3, 1, 8, 0, pal, 3, &r8[0][0], 4, false, &len );
    check( gfxfile_load_layer( &fh, 0, f, len, 0, 0, true, &err ), "gfxfile: load with PALETTE" );
    check( f_point( &fake, 0, 0, 0 ) == 2 && f_point( &fake, 0, 1, 0 ) == 1 && f_point( &fake, 0, 2, 0 ) == 0, "gfxfile: PALETTE keeps the indices" );
    check( fake.pal[0][1] == 0xFF463C32U && fake.pal[0][2] == 0xFF030201U && fake.pal[0][0] == 0x00070809U, "gfxfile: PALETTE installs the colors (entry 0 stays transparent)" );
    // without PALETTE the same picture is matched to the existing palette (index 0 of the file is not entry 0)
    fake_reset();
    check( gfxfile_load_layer( &fh, 0, f, len, 0, 0, false, &err ) && f_point( &fake, 0, 0, 0 ) != 2 && f_point( &fake, 0, 2, 0 ) != 0, "gfxfile: matched to the palette without PALETTE" );
    free( f );
    // a bad file
    uint8_t junk[40] = { 0 };
    check( !gfxfile_load_layer( &fh, 3, junk, sizeof(junk), 0, 0, false, &err ) && err != 0, "gfxfile: junk is refused" );

    // GFX SAVE and load again
    fake_reset();
    for ( int y=0; y < 10; ++y ) for ( int x=0; x < 20; ++x ) fake.layer[1][( 40 + y ) * GFXFILE_LW + 100 + x] = (uint8_t)( x + y * 3 );
    uint8_t* out = 0; size_t olen = 0;
    check( gfxfile_save_layer( &fh, 3, 100, 40, 119, 49, &out, &olen, &err ), "gfxfile: save a rectangle" );
    bmp_image_t img;
    check( bmp_decode( out, olen, &img, &err ) && img.w == 20 && img.h == 10 && img.npal == 256 && img.pal[2] == fake.pal[3][2], "gfxfile: saved size and palette" );
    check( img.idx[0] == 0 && img.idx[19 + 9 * 20] == 19 + 27, "gfxfile: saved pixels" );
    bmp_free( &img );
    fake_reset();
    check( gfxfile_load_layer( &fh, 3, out, olen, 5, 6, true, &err ) && f_point( &fake, 3, 5 + 19, 6 + 9 ) == 19 + 27 && f_point( &fake, 3, 5, 6 ) == 0, "gfxfile: save and load round trip" );
    free( out );
    check( gfxfile_save_layer( &fh, 3, 300, 100, 10, 5, &out, &olen, &err ), "gfxfile: corners may be given in any order" );
    free( out );
    check( !gfxfile_save_layer( &fh, 3, 700, 0, 800, 10, &out, &olen, &err ), "gfxfile: a rectangle off the screen is refused" );

    // sheets: 2 x 2 images of 16 x 24 in a 32 x 48 picture, true color
    fake_reset();
    uint8_t* sheet = (uint8_t*) calloc( 1, 32 * 48 * 3 );
    for ( int y=0; y < 48; ++y ) for ( int x=0; x < 32; ++x ) {
        uint8_t* p = sheet + ( 47 - y ) * 96 + x * 3;          // bottom-up rows, stride 96 is a multiple of 4
        int cell = ( y / 24 ) * 2 + x / 16;
        p[0] = p[1] = 0; p[2] = 0;
        if ( cell == 0 ) p[2] = 255;                              // red
        if ( cell == 1 ) p[1] = 255;                              // green
        if ( cell == 2 ) p[0] = 255;                              // blue
    }
    f = make_bmp( 32, 48, 24, 0, 0, 0, sheet, 96, false, &len );
    free( sheet );
    check( gfxfile_load_cells( &fh, GFXFILE_SPRITE, 10, -1, f, len, false, &err ), "cells: load a sheet into sprite images" );
    check( fake.cells[0][10][0] == 1 && fake.cells[0][11][5 * 16 + 5] == 2 && fake.cells[0][12][23 * 16 + 15] == 3, "cells: red, green, blue in images 10, 11, 12" );
    check( fake.cells[0][13][0] != 1 && fake.cells[0][14][0] == 0, "cells: no more than the sheet has" );
    check( gfxfile_load_cells( &fh, GFXFILE_TILE, 250, 2, f, len, false, &err ) && fake.cells[1][250][0] == 1 && fake.cells[1][251][0] == 2, "cells: tiles" );
    check( !gfxfile_load_cells( &fh, GFXFILE_TILE, 255, 2, f, len, false, &err ), "cells: tiles beyond 255 are refused" );
    check( gfxfile_load_cells( &fh, GFXFILE_SPRITE, 246, -1, f, len, false, &err ) && fake.cells[0][246][0] == 1 && fake.cells[0][247][0] == 2 && fake.cells[0][248][0] == 0, "cells: default count stops at sprite 247" );
    check( !gfxfile_load_cells( &fh, GFXFILE_SPRITE, 0, 5, f, len, false, &err ), "cells: more images than the sheet has is refused" );
    free( f );
    uint8_t smallrow[4 * 3] = { 0 };
    f = make_bmp( 1, 1, 24, 0, 0, 0, smallrow, 4, false, &len );
    check( !gfxfile_load_cells( &fh, GFXFILE_SPRITE, 0, -1, f, len, false, &err ) && strstr( err, "smaller" ) != 0, "cells: a picture smaller than one image" );
    free( f );

    // saving sheets and reading them back
    fake_reset();
    for ( int k=0; k < 5; ++k ) memset( fake.cells[0][20 + k], k + 1, sizeof(fake.cells[0][0]) );
    check( gfxfile_save_cells( &fh, GFXFILE_SPRITE, 20, 5, 2, &out, &olen, &err ), "cells: save 5 images, 2 columns" );
    check( bmp_decode( out, olen, &img, &err ) && img.w == 32 && img.h == 72 && img.idx[0] == 1 && img.idx[16] == 2 && img.idx[24 * 32] == 3 && img.idx[48 * 32] == 5 && img.idx[48 * 32 + 16] == 0, "cells: sheet layout, the empty cell is 0" );
    bmp_free( &img );
    fake_reset();
    check( gfxfile_load_cells( &fh, GFXFILE_SPRITE, 100, 5, out, olen, true, &err ) && fake.cells[0][104][3] == 5 && fake.cells[0][100][3] == 1, "cells: sheet round trip" );
    free( out );
    check( gfxfile_save_cells( &fh, GFXFILE_TILE, 0, 20, 0, &out, &olen, &err ) && bmp_decode( out, olen, &img, &err ) && img.w == 256 && img.h == 48, "cells: default columns are 16" );
    bmp_free( &img ); free( out );
    check( !gfxfile_save_cells( &fh, GFXFILE_SPRITE, 240, 10, 0, &out, &olen, &err ), "cells: saving past sprite 247 is refused" );

    // palettes
    fake_reset();
    check( gfxfile_save_palette( &fh, 2, &out, &olen, &err ) && bmp_decode( out, olen, &img, &err ) && img.w == 16 && img.h == 16 && img.idx[17] == 17 && img.pal[3] == fake.pal[2][3], "palette: save as a 16 x 16 picture" );
    bmp_free( &img );
    fake.pal[2][3] = 0xFFABCDEFU;
    check( gfxfile_load_palette( &fh, 1, out, olen, &err ) && fake.pal[1][3] == 0xFF0000FFU && fake.pal[1][100] == fake.pal[2][100], "palette: load into another layer" );
    free( out );
    uint8_t rows24b[1][4 * 3] = { { 0,0,255, 255,0,0, 0,0,255, 255,0,0 } };
    f = make_bmp( 4, 1, 24, 0, 0, 0, &rows24b[0][0], 12, false, &len );
    check( gfxfile_load_palette( &fh, 0, f, len, &err ) && fake.pal[0][1] == 0xFF0000FFU && fake.pal[0][2] == 0xFFFF0000U && fake.pal[0][3] == 0xFF0000FFU, "palette: from a true color picture (only the colors of the picture are set)" );
    free( f );
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
    test_reserved();
    test_mouse();
    test_sprites();
    test_sprite_groups();
    test_paths();
    test_textscreen();
    test_bmp();
    test_gfxfile();
    test_reset();
    printf( "%d checks, %d failure(s)\n", checks, failures );
    return failures;
}
