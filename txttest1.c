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


// Automated test of the text screen's viewport and its two screens (no display needed): exit status = failures.

#include "textscreen.h"
#include "palette.h"

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

static void test_zero_bytes( void ) {
    txtscr_init();
    txtscr_setviewport( 10, 5, 20, 4 );
    txtscr_print_n( 0, 0, -1, -1, "a\0b\0\nc", 6U );
    int cx, cy;
    txtscr_getcursor( &cx, &cy );
    check( abschr( 10, 5 ) == 'a' && abschr( 11, 5 ) == 0 && abschr( 12, 5 ) == 'b' && abschr( 13, 5 ) == 0, "zero bytes are characters (code 0)" );
    check( abschr( 14, 5 ) == ' ' && abschr( 10, 6 ) == 'c' && cx == 1 && cy == 1, "text goes on after zero bytes" );
    txtscr_print_n( 0, 18, -1, -1, "\0\0\0", 3U );
    check( abschr( 28, 5 ) == 0 && abschr( 29, 5 ) == 0 && abschr( 10, 6 ) == 0, "zero bytes wrap like other characters" );
}

// the font: every code except space and NBSP has a glyph (printable ISO-8859-15 or a control code glyph), and no two glyphs are equal
extern const uint8_t fontdef1[3072];

static void test_font( void ) {
    int empty = 0, same = 0;
    for ( int c = 0; c < 256; ++c ) {
        bool blank = true;
        for ( int r = 0; r < 12; ++r ) {
            blank = blank && fontdef1[ c * 12 + r ] == 0;
        }
        if ( blank && c != 0x20 && c != 0xA0 ) {
            ++empty;
        }
        for ( int d = c + 1; d < 256; ++d ) {
            if ( memcmp( fontdef1 + c * 12, fontdef1 + d * 12, 12U ) == 0 && !( c == 0x20 && d == 0xA0 ) && !( c == '-' && d == 0xAD ) ) {
                ++same;
            }
        }
    }
    check( empty == 0, "the font has a glyph for every code except space and NBSP" );
    check( same == 0, "no two glyphs of the font are equal (but NBSP = space, soft hyphen = hyphen)" );
    txtscr_init();
    txtscr_print_n( 0, 0, -1, -1, "\xe4\x80\xff", 3U );
    check( abschr( 0, 0 ) == 0xE4 && abschr( 1, 0 ) == 0x80 && abschr( 2, 0 ) == 0xFF, "codes >= 128 are stored as they are" );
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

// -- cell flags (EMPTY, WRAP) and the overlays of the screen editor ------------------------

static void test_flags( void ) {
    txtscr_init();
    check( txtscr_isempty( 0, 0 ) && txtscr_isempty( 24, 79 ), "a fresh screen is empty" );
    check( TXTSCR_CELL_CHR( txtscr_getcell( 0, 0 ) ) == ' ' && ( txtscr_getcell( 0, 0 ) & TXTSCR_FLAGS ) == 0, "empty cells look like spaces, the getters hide the flags" );
    txtscr_write( 2, 3, -1, -1, "ab", 2 );
    check( !txtscr_isempty( 2, 3 ) && !txtscr_isempty( 2, 4 ) && txtscr_isempty( 2, 5 ), "writing clears EMPTY" );
    txtscr_write( 2, 5, -1, -1, " ", 1 );
    check( !txtscr_isempty( 2, 5 ), "a written space is not empty" );
    txtscr_setempty( 2, 5 );
    check( txtscr_isempty( 2, 5 ), "txtscr_setempty" );
    txtscr_clear();
    check( txtscr_isempty( 2, 3 ), "CLS makes cells empty" );

    // wrap: text running over the right edge links the rows
    txtscr_clear();
    char line[200];
    memset( line, 'x', sizeof line );
    txtscr_print_n( 0, 0, -1, -1, line, 100 );
    check( txtscr_iswrapped( 0, 79 ) && !txtscr_iswrapped( 0, 78 ) && !txtscr_iswrapped( 1, 19 ), "a wrapped row has WRAP on its last cell only" );
    check( !txtscr_isempty( 1, 19 ) && txtscr_isempty( 1, 20 ), "...and the text continues in the next row" );
    txtscr_write( 0, 79, -1, -1, "y", 1 );
    check( !txtscr_iswrapped( 0, 79 ), "writing the last cell clears WRAP" );

    // exactly full row: the wrap happens when the cursor leaves the row
    txtscr_clear();
    txtscr_print_n( 0, 0, -1, -1, line, 80 );
    int cx, cy;
    txtscr_getcursor( &cx, &cy );
    check( cx == 0 && cy == 1 && txtscr_iswrapped( 0, 79 ) && txtscr_isempty( 1, 0 ), "a full row wraps; the next row stays empty" );
    txtscr_clear();
    txtscr_print_n( 0, 0, -1, -1, line, 79 );
    check( !txtscr_iswrapped( 0, 79 ) && !txtscr_iswrapped( 0, 78 ), "79 characters do not wrap" );
    txtscr_clear();
    txtscr_print_n( 0, 0, -1, -1, "abc\ndef", 7 );
    check( !txtscr_iswrapped( 0, 79 ) && !txtscr_iswrapped( 0, 2 ), "a line feed is no wrap" );

    // the flags move with the rows
    txtscr_clear();
    txtscr_print_n( 23, 0, -1, -1, line, 100 );        // rows 23, 24 (wrapped)
    txtscr_scrollup();
    check( txtscr_iswrapped( 22, 79 ) && !txtscr_isempty( 23, 0 ) && txtscr_isempty( 24, 0 ), "scrolling up keeps the flags; the new row is empty" );
    txtscr_scrolldown();
    check( txtscr_iswrapped( 23, 79 ) && txtscr_isempty( 0, 0 ), "scrolling down keeps the flags; the new row is empty" );
    // wrapping at the bottom scrolls
    txtscr_clear();
    txtscr_locate( 0, 24 );
    txtscr_print_n( -1, -1, -1, -1, line, 100 );
    check( txtscr_iswrapped( 23, 79 ) && !txtscr_isempty( 24, 19 ), "wrapping on the last row scrolls and keeps the link" );

    // fill / invert / save / restore
    txtscr_clear();
    txtscr_fill( 0, 0, 3, 0, -1, 5, 6 );
    check( txtscr_isempty( 0, 0 ), "a color-only fill keeps EMPTY" );
    txtscr_fill( 0, 0, 3, 0, 'q', 5, 6 );
    check( !txtscr_isempty( 0, 0 ) && !txtscr_isempty( 0, 3 ), "a fill with a character makes cells written" );
    txtscr_invert( 0, 0, 3, 0 );
    check( !txtscr_isempty( 0, 0 ) && txtscr_isempty( 1, 0 ), "invert keeps the flags" );
    txtscr_clear();
    txtscr_print_n( 0, 0, -1, -1, line, 100 );
    static uint8_t sv[TXTSCR_WIDTH * TXTSCR_HEIGHT * 4];
    size_t need = txtscr_save( sv, sizeof sv );
    txtscr_clear();
    check( txtscr_restore( sv, need ) && txtscr_iswrapped( 0, 79 ) && !txtscr_isempty( 1, 19 ) && txtscr_isempty( 1, 20 ), "save / restore carry the flags" );
    check( TXTSCR_CELL_CHR( txtscr_getrawcell( 0, 0 ) ) == 'x' && ( txtscr_getrawcell( 0, 0 ) & TXTSCR_FLAGS ) == 0, "the raw getter hides the flags" );

    // viewport relative
    txtscr_clear();
    txtscr_setviewport( 10, 5, 20, 3 );
    txtscr_print_n( 0, 0, -1, -1, line, 25 );
    check( txtscr_iswrapped( 0, 19 ) && !txtscr_isempty( 1, 4 ) && txtscr_isempty( 1, 5 ) && txtscr_getflags( 9, 9 ) == 0, "wrapping at the viewport's edge" );
    txtscr_resetviewport();
    check( !txtscr_iswrapped( 5, 79 ) && txtscr_iswrapped( 5, 29 ), "...is a link on the viewport's last column" );
}

static void test_resize_flags( void ) {
    txtscr_init();
    char line[200];
    memset( line, 'x', sizeof line );
    txtscr_print_n( 0, 0, -1, -1, line, 100 );
    txtscr_resize( 100, 25 );
    check( !txtscr_iswrapped( 0, 79 ) && !txtscr_isempty( 0, 79 ) && txtscr_isempty( 0, 80 ) && txtscr_isempty( 24, 99 ), "resize wider: old link gone, new cells empty" );
    txtscr_resize( 60, 10 );
    check( !txtscr_iswrapped( 0, 59 ) && !txtscr_isempty( 0, 59 ), "resize narrower: the cut row has no link" );
    txtscr_resize( TXTSCR_WIDTH, TXTSCR_HEIGHT );
}

static void test_overlays( void ) {
    static uint8_t img[TXTSCR_WIDTH * 8 * TXTSCR_HEIGHT * 12];
    txtscr_init();
    txtscr_enablecursor( false );
    txtscr_setcolors( 3, 4 );
    txtscr_write( 24, 0, -1, -1, "zzz", 3 );
    txtscr_write( 5, 0, -1, -1, "hi", 2 );
    check( !txtscr_gethighlight( 0, 0 ), "no highlight at start" );
    char buf[100];
    check( !txtscr_getstatus( buf, sizeof buf ) && buf[0] == 0, "no status at start" );

    txtscr_setstatus( "OVR  F12=INS", 9, 8 );
    check( txtscr_getstatus( buf, sizeof buf ) && strcmp( buf, "OVR  F12=INS" ) == 0, "status set" );
    textcell_t c = txtscr_getdisplaycell( 24, 1 );
    check( TXTSCR_CELL_CHR( c ) == 'V' && TXTSCR_CELL_BG( c ) == 9 && TXTSCR_CELL_FG( c ) == 8, "the bottom row shows the status text" );
    c = txtscr_getdisplaycell( 24, 79 );
    check( TXTSCR_CELL_CHR( c ) == ' ' && TXTSCR_CELL_BG( c ) == 9, "...padded over the whole row" );
    check( TXTSCR_CELL_CHR( txtscr_getcell( 24, 0 ) ) == 'z' && TXTSCR_CELL_BG( txtscr_getcell( 24, 0 ) ) == 3, "the cells below are untouched" );
    txtscr_render( img );
    bool seen9 = false, seen8 = false;
    for ( int y=0; y < 12; ++y ) {
        for ( int x=0; x < 8; ++x ) {
            uint8_t px = img[ ( 24 * 12 + y ) * TXTSCR_WIDTH * 8 + 8 + x ];     // the 'V'
            seen9 = seen9 || px == 9;
            seen8 = seen8 || px == 8;
        }
    }
    check( seen9 && seen8, "the renderer draws the status line" );
    uint8_t save[TXTSCR_WIDTH * TXTSCR_HEIGHT * 4];
    txtscr_save( save, sizeof save );
    check( save[ 24 * TXTSCR_WIDTH * 4 ] == 'z', "save does not see the overlay" );
    txtscr_setstatus( 0, 0, 0 );
    check( !txtscr_getstatus( 0, 0 ) && TXTSCR_CELL_CHR( txtscr_getdisplaycell( 24, 0 ) ) == 'z', "status off: the cells show again" );

    // highlight
    uint8_t map[256];
    for ( int i=0; i < 256; ++i ) map[i] = (uint8_t)( 255 - i );
    txtscr_sethighlightmap( map );
    txtscr_sethighlight( 4, 6 );
    int f, l;
    check( txtscr_gethighlight( &f, &l ) && f == 4 && l == 6, "highlight range" );
    c = txtscr_getdisplaycell( 5, 0 );
    check( TXTSCR_CELL_CHR( c ) == 'h' && TXTSCR_CELL_BG( c ) == 252 && TXTSCR_CELL_FG( c ) == 4, "a highlighted row shows the mapped paper" );
    c = txtscr_getdisplaycell( 6, 70 );
    check( TXTSCR_CELL_BG( c ) == 255 - TXTSCR_BGCOL, "...also its empty cells" );
    check( TXTSCR_CELL_BG( txtscr_getdisplaycell( 3, 0 ) ) == TXTSCR_BGCOL && TXTSCR_CELL_BG( txtscr_getdisplaycell( 7, 0 ) ) == TXTSCR_BGCOL, "rows outside are normal" );
    check( TXTSCR_CELL_BG( txtscr_getcell( 5, 0 ) ) == 3, "the cells are not changed" );
    txtscr_render( img );
    check( img[ 5 * 12 * TXTSCR_WIDTH * 8 + 40 * 8 ] == 255 - TXTSCR_BGCOL, "the renderer draws the highlight" );
    txtscr_sethighlight( -1, -1 );
    check( !txtscr_gethighlight( 0, 0 ) && TXTSCR_CELL_BG( txtscr_getdisplaycell( 5, 0 ) ) == 3, "highlight off" );

    // default map: brighter, same or nearest; black stays dark-ish, never darker
    txtscr_sethighlightmap( 0 );
    txtscr_sethighlight( 0, 0 );
    uint32_t a0 = pal_argb( 1 ), a1 = pal_argb( TXTSCR_CELL_BG( txtscr_getdisplaycell( 0, 0 ) ) );
    check( TXTSCR_CELL_BG( txtscr_getdisplaycell( 0, 0 ) ) != TXTSCR_BGCOL && ( a1 & 255U ) >= ( a0 & 255U ), "the default map makes a paper brighter" );
    // the start paper is transparent (the backdrop shows): the highlight must be lighter than the backdrop, opaque, and readable with the default pen
    txtscr_sethighlight( 0, 0 );
    uint32_t hl = pal_argb( TXTSCR_CELL_BG( txtscr_getdisplaycell( 0, 5 ) ) ), bd = PAL_DEFAULT_BACKDROP;
    check( TXTSCR_BGCOL == 0 && pal_argb( 0 ) >> 24 == 0, "(the start paper is the transparent entry)" );
    (void) bd;
    check( TXTSCR_CELL_BG( txtscr_getdisplaycell( 0, 5 ) ) == 255 && hl == UINT32_C(0X88FFFFFF), "the highlight of a transparent paper is entry 255, TransparentWhite 0x88FFFFFF (translucent: lighter than whatever is behind)" );
    txtscr_sethighlight( 0, 0 );
    check( TXTSCR_CELL_BG( txtscr_getdisplaycell( 0, 5 ) ) == 255, "(the same for other empty cells)" );
    txtscr_write( 0, 0, 2, 140, "N", 1 );
    uint32_t hn = pal_argb( TXTSCR_CELL_BG( txtscr_getdisplaycell( 0, 0 ) ) );
    check( hn >> 24 == 255 && hn != pal_argb( 2 ), "an opaque paper gets a lighter opaque color" );
    txtscr_sethighlight( -1, -1 );

    // overlays are per screen
    txtscr_setstatus( "x", 1, 2 );
    txtscr_select( 1 );
    check( !txtscr_getstatus( 0, 0 ), "screen 1 has no status" );
    txtscr_select( 0 );
    txtscr_setstatus( 0, 0, 0 );
}

int main( void ) {
    test_basics();
    test_write();
    test_wrap();
    test_zero_bytes();
    test_font();
    test_scroll();
    test_clear_blank();
    test_screens();
    test_colors();
    test_render();
    test_flags();
    test_resize_flags();
    test_overlays();
    printf( "%d of %d checks passed\n", total - failures, total );
    return failures;
}
