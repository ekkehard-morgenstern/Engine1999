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


// Automated test of the text screen's viewport and its two screens (no display needed): exit status = failures.

#include "textscreen.h"

static int failures = 0, total = 0;

static void check( bool cond, const char* name ) {
    ++total;
    if ( !cond ) {
        printf( "FAIL  %s\n", name );
        ++failures;
    }
}

// character at absolute screen position, whatever the viewport is
static int abschr( int x, int y ) {
    int vx, vy, vw, vh, cx, cy;
    txtscr_getviewport( &vx, &vy, &vw, &vh );
    txtscr_getcursor( &cx, &cy );
    txtscr_resetviewport();
    int c = TXTSCR_CELL_CHR( txtscr_getcell( y, x ) );
    txtscr_setviewport( vx, vy, vw, vh );
    txtscr_locate( cx, cy );
    return c;
}

static void test_basics( void ) {
    txtscr_init();
    int w, h;
    txtscr_getsize( &w, &h );
    check( w == TXTSCR_WIDTH && h == TXTSCR_HEIGHT, "default viewport is the screen" );
    check( !txtscr_setviewport( 70, 0, 20, 5 ), "viewport beyond the right edge refused" );
    check( !txtscr_setviewport( 0, 0, 0, 5 ), "empty viewport refused" );
    check( !txtscr_setviewport( -1, 0, 5, 5 ), "negative viewport refused" );
    check( txtscr_setviewport( 10, 5, 20, 4 ), "viewport accepted" );
    txtscr_getsize( &w, &h );
    check( w == 20 && h == 4, "size is the viewport size" );
    int cx = 7, cy = 7;
    txtscr_getcursor( &cx, &cy );
    check( cx == 0 && cy == 0, "cursor at the viewport's top left" );
}

static void test_write( void ) {
    txtscr_init();
    txtscr_setviewport( 10, 5, 20, 4 );
    check( txtscr_write( 0, 0, 3, 4, "abc", -1 ) == 3, "write returns the length" );
    check( abschr( 10, 5 ) == 'a' && abschr( 12, 5 ) == 'c', "write is relative to the viewport" );
    check( TXTSCR_CELL_BG( txtscr_getcell( 0, 0 ) ) == 3 && TXTSCR_CELL_FG( txtscr_getcell( 0, 0 ) ) == 4, "write colors" );
    check( txtscr_write( 0, 18, -1, -1, "wxyz", -1 ) == 2, "write is clipped at the right edge" );
    check( abschr( 28, 5 ) == 'w' && abschr( 29, 5 ) == 'x' && abschr( 30, 5 ) == ' ', "nothing written outside, right" );
    check( txtscr_write( 4, 0, -1, -1, "x", -1 ) == 0, "write below the viewport is refused" );
    check( txtscr_write( 0, -2, -1, -1, "abcd", -1 ) == 2, "write is clipped at the left edge" );
    check( abschr( 10, 5 ) == 'c' && abschr( 9, 5 ) == ' ', "nothing written outside, left" );
}

static void test_wrap( void ) {
    txtscr_init();
    txtscr_setviewport( 10, 5, 20, 4 );
    txtscr_print( 0, 0, -1, -1, "0123456789012345678901234" );    // 25 characters: 20 + 5
    check( abschr( 29, 5 ) == '9' && abschr( 10, 6 ) == '0' && abschr( 14, 6 ) == '4', "text continues in the next viewport row" );
    check( abschr( 9, 6 ) == ' ' && abschr( 30, 5 ) == ' ' && abschr( 0, 6 ) == ' ', "wrap doesn't touch cells outside" );
    int cx, cy;
    txtscr_getcursor( &cx, &cy );
    check( cx == 5 && cy == 1, "cursor after wrapped text" );
    txtscr_print( 2, 0, -1, -1, "x\ny" );
    txtscr_getcursor( &cx, &cy );
    check( abschr( 10, 7 ) == 'x' && abschr( 10, 8 ) == 'y' && cx == 1 && cy == 3, "newline in a viewport" );
}

static void test_scroll( void ) {
    txtscr_init();
    for ( int y=0; y < TXTSCR_HEIGHT; ++y ) {       // marks outside: column 9 and 30
        txtscr_write( y, 9, -1, -1, "|", 1 );
        txtscr_write( y, 30, -1, -1, "|", 1 );
    }
    txtscr_setviewport( 10, 5, 20, 4 );
    for ( int y=0; y < 4; ++y ) {
        char c = (char)( 'A' + y );
        txtscr_write( y, 0, -1, -1, &c, 1 );
    }
    txtscr_scrollup();
    check( abschr( 10, 5 ) == 'B' && abschr( 10, 7 ) == 'D' && abschr( 10, 8 ) == ' ', "scroll up inside the viewport" );
    check( abschr( 9, 5 ) == '|' && abschr( 30, 8 ) == '|' && abschr( 10, 4 ) == ' ' && abschr( 10, 9 ) == ' ', "scroll up leaves the rest" );
    txtscr_scrolldown();
    check( abschr( 10, 5 ) == ' ' && abschr( 10, 6 ) == 'B' && abschr( 10, 8 ) == 'D', "scroll down inside the viewport" );
    check( abschr( 9, 8 ) == '|' && abschr( 30, 5 ) == '|' && abschr( 10, 9 ) == ' ', "scroll down leaves the rest" );
    // printing below the last row scrolls the viewport
    txtscr_locate( 0, 3 );
    txtscr_print( -1, -1, -1, -1, "end\nnext" );
    check( abschr( 10, 8 ) == 'n' && abschr( 10, 7 ) == 'e' && abschr( 10, 6 ) == 'C' && abschr( 10, 5 ) == 'B', "print scrolls the viewport" );
    check( abschr( 10, 9 ) == ' ' && abschr( 9, 9 ) == '|', "print doesn't scroll outside" );
    // backspace at the top left corner scrolls down
    txtscr_locate( 0, 0 );
    txtscr_backspace();
    int cx, cy;
    txtscr_getcursor( &cx, &cy );
    check( cx == 19 && cy == 0 && abschr( 10, 6 ) == 'B', "backspace at the corner scrolls down" );
}

static void test_clear_blank( void ) {
    txtscr_init();
    txtscr_write( 0, 0, -1, -1, "keep", -1 );
    txtscr_write( 5, 12, -1, -1, "gone", -1 );
    txtscr_setviewport( 10, 5, 20, 4 );
    txtscr_setblank( 9, 8 );
    txtscr_clear();
    check( abschr( 12, 5 ) == ' ' && TXTSCR_CELL_BG( txtscr_getcell( 0, 2 ) ) == 9, "clear uses the blank colors" );
    check( abschr( 0, 0 ) == 'k', "clear stays in the viewport" );
    txtscr_scrollup();
    check( TXTSCR_CELL_BG( txtscr_getcell( 3, 0 ) ) == 9, "scrolling fills with the blank colors" );
}

static void test_colors( void ) {
    txtscr_init();
    int bg, fg;
    txtscr_getcolors( &bg, &fg );
    check( bg == TXTSCR_BGCOL && fg == TXTSCR_FGCOL, "default colors" );
    txtscr_select( 2 );
    txtscr_setcolors( 7, 9 );
    txtscr_write( 0, 0, -1, -1, "x", 1 );
    check( TXTSCR_CELL_BG( txtscr_getcell( 0, 0 ) ) == 7 && TXTSCR_CELL_FG( txtscr_getcell( 0, 0 ) ) == 9, "write uses the screen's colors" );
    txtscr_select( 0 );
    txtscr_getcolors( &bg, &fg );
    check( bg == TXTSCR_BGCOL && fg == TXTSCR_FGCOL, "colors belong to their screen" );
}

static void test_screens( void ) {
    txtscr_init();
    txtscr_print( 0, 0, -1, -1, "basic" );
    check( txtscr_selected() == 0, "screen 0 is selected first" );
    check( !txtscr_select( TXTSCR_NSCREENS ) && !txtscr_select( -1 ), "no such screen" );
    check( txtscr_select( 1 ), "select screen 1" );
    check( TXTSCR_CELL_CHR( txtscr_getcell( 0, 0 ) ) == ' ', "screen 1 starts empty" );
    txtscr_setviewport( 5, 5, 10, 3 );
    txtscr_print( 0, 0, -1, -1, "editor" );
    txtscr_select( 0 );
    int w, h;
    txtscr_getsize( &w, &h );
    check( w == TXTSCR_WIDTH && h == TXTSCR_HEIGHT, "the viewport belongs to its screen" );
    check( TXTSCR_CELL_CHR( txtscr_getcell( 0, 0 ) ) == 'b' && TXTSCR_CELL_CHR( txtscr_getcell( 5, 5 ) ) == ' ', "screen 0 is untouched" );
    int cx, cy;
    txtscr_getcursor( &cx, &cy );
    check( cx == 5 && cy == 0, "the cursor belongs to its screen" );
    txtscr_select( 1 );
    check( TXTSCR_CELL_CHR( txtscr_getcell( 0, 0 ) ) == 'e', "screen 1 keeps its text" );
    txtscr_getcursor( &cx, &cy );
    check( cx == 6 && cy == 0, "screen 1 keeps its cursor" );
}

static void test_render( void ) {
    static uint8_t img[TXTSCR_WIDTH * 8 * TXTSCR_HEIGHT * 12];
    txtscr_init();
    txtscr_enablecursor( false );
    txtscr_select( 1 );
    txtscr_setviewport( 3, 2, 4, 1 );
    txtscr_write( 0, 0, 77, 78, "M", 1 );
    txtscr_render( img );
    bool inside = false, bg = false, outside = true;
    for ( int y=0; y < 12; ++y ) {
        for ( int x=0; x < 8; ++x ) {
            uint8_t px = img[ ( 2 * 12 + y ) * TXTSCR_WIDTH * 8 + 3 * 8 + x ];
            inside = inside || px == 78;
            bg = bg || px == 77;
        }
    }
    for ( int x=0; x < 8; ++x ) {
        outside = outside && img[ 2 * 12 * TXTSCR_WIDTH * 8 + 2 * 8 + x ] == TXTSCR_BGCOL;
    }
    check( inside && bg && outside, "the selected screen is rendered at the viewport position" );
    txtscr_select( 0 );
    txtscr_render( img );
    check( img[ 2 * 12 * TXTSCR_WIDTH * 8 + 3 * 8 ] == TXTSCR_BGCOL, "screen 0 renders its own cells" );
}

int main( void ) {
    test_basics();
    test_write();
    test_wrap();
    test_scroll();
    test_clear_blank();
    test_screens();
    test_colors();
    test_render();
    printf( "%d of %d checks passed\n", total - failures, total );
    return failures;
}
