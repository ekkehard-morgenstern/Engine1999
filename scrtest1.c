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


#include "stdtypes.h"
#include "scrdrv.h"
#include "textscreen.h"
#include "fulledit.h"
#include "monitor.h"
#include "basrepl.h"

/*
Automated test of the editors on a fake screen driver (no SDL, no terminal): scripted keys go in, the text screens are looked at afterwards: the
one-line editor (scredit.c), the full-screen editor (fulledit.c) and the monitor (monitor.c), at several screen sizes and with resizes in the
middle (the console host's case). Exit status = number of failed checks.
*/

static int failures = 0, total = 0;

static void check( bool cond, const char* name ) {
    ++total;
    if ( !cond ) {
        printf( "FAIL  %s\n", name );
        ++failures;
    }
}

// -- the fake driver: the text screens of textscreen.c, keys from a script ------------------------------------

#define MAXKEYS 4000

static scrkey_t script[MAXKEYS];
static int nscript = 0, ipos = 0;

static void clearscript( void ) { nscript = ipos = 0; }
static void addkey( int kind, int ch, int key, int mods ) {
    script[nscript].kind = kind; script[nscript].ch = ch; script[nscript].key = key; script[nscript].mods = mods;
    ++nscript;
}
static void typetext( const char* t ) {
    for ( ; *t; ++t ) addkey( SK_CHAR, (unsigned char) *t, 0, 0 );
}
static void press( int kind ) { addkey( kind, 0, 0, 0 ); }
static void pressk( int key, int mods ) { addkey( SK_KEY, 0, key, mods ); }
static void resize( int cols, int rows ) { addkey( SK_RESIZE, cols, rows, 0 ); }     // (the driver resizes the screens when it hands the key out)

static bool d_nextkey( scrkey_t* k ) {
    if ( ipos >= nscript ) {
        memset( k, 0, sizeof(*k) );
        k->kind = SK_CLOSED;
        return false;
    }
    *k = script[ ipos++ ];
    if ( k->kind == SK_RESIZE ) {
        txtscr_resize( k->ch, k->key );
    }
    return true;
}
static bool d_pending( void ) { return ipos < nscript; }
static int d_select( int n ) { return txtscr_select( n ) ? n : txtscr_selected(); }
static void d_screensize( int* c, int* r ) { txtscr_getscreensize( c, r ); }
static void d_getsize( int* c, int* r ) { txtscr_getsize( c, r ); }
static void d_getviewport( int* x, int* y, int* w, int* h ) { txtscr_getviewport( x, y, w, h ); }
static void d_getcursor( int* x, int* y ) { txtscr_getcursor( x, y ); }
static void d_print( int y, int x, int bg, int fg, const char* t ) { txtscr_print( y, x, bg, fg, t ); }
static void d_changed( void ) { }
static int d_begin( void ) { return 0; }
static void d_end( int s ) { (void) s; }
static void d_pollkeys( bool e ) { (void) e; }
static int d_restart( int absy, int startx, size_t pos ) { (void) startx; (void) pos; return absy - txtscr_resizeshift(); }

static const scrdrv_t drv = {
    d_screensize, d_getsize, d_select, txtscr_selected, txtscr_setviewport, txtscr_resetviewport, d_getviewport,
    txtscr_setcolors, txtscr_setblank, txtscr_clear, txtscr_write, d_print, txtscr_locate, d_getcursor,
    d_changed, txtscr_setresizereserve, d_restart, d_begin, d_end, d_nextkey, d_pending, d_pollkeys
};

// row y of the selected screen as text (trailing blanks cut)
static const char* rowtext( int y ) {
    static char buf[TXTSCR_MAXW + 1];
    int w, h;
    txtscr_getscreensize( &w, &h );
    int n = 0;
    for ( int x = 0; x < w; ++x ) {
        buf[n++] = (char) TXTSCR_CELL_CHR( txtscr_getrawcell( y, x ) );
    }
    while ( n > 0 && buf[n - 1] == ' ' ) {
        --n;
    }
    buf[n] = '\0';
    return buf;
}

static bool rowis( int y, const char* text ) { return strcmp( rowtext( y ), text ) == 0; }
static bool rowhas( int y, const char* part ) { return strstr( rowtext( y ), part ) != 0; }

static void freshscreens( int cols, int rows ) {
    txtscr_init();
    txtscr_resize( cols, rows );
}

// -- the one-line editor ---------------------------------------------------------------------------------

static void test_lineedit( void ) {
    char buf[100];
    freshscreens( 40, 10 );
    txtscr_select( 0 );
    txtscr_print( 0, 0, -1, -1, "> " );
    clearscript();
    typetext( "abc" ); pressk( KS_LEFT, 0 ); typetext( "X" ); press( SK_RETURN );
    strcpy( buf, "" );
    int r = scr_lineedit( &drv, buf, sizeof(buf), 0, 0 );
    check( r == SCREDIT_ACCEPTED && strcmp( buf, "abXc" ) == 0, "line editor: typing, Left, a character in the middle, RETURN" );
    check( rowis( 0, "> abXc" ), "line editor: the text is on the screen" );

    txtscr_print( 2, 0, -1, -1, "> " );
    clearscript();
    typetext( "hello" ); pressk( KS_HOME, 0 ); pressk( KS_DELETE, 0 ); pressk( KS_END, 0 ); press( SK_BACKSPACE ); press( SK_RETURN );
    strcpy( buf, "" );
    r = scr_lineedit( &drv, buf, sizeof(buf), 0, 0 );
    check( r == SCREDIT_ACCEPTED && strcmp( buf, "ell" ) == 0, "line editor: Home, Delete, End, Backspace" );

    txtscr_print( 4, 0, -1, -1, "> " );
    clearscript();
    typetext( "abcdef" ); press( SK_BACKSPACE ); press( SK_BACKSPACE ); press( SK_BACKSPACE ); press( SK_RETURN );     // a burst: drawn once
    strcpy( buf, "" );
    r = scr_lineedit( &drv, buf, sizeof(buf), 0, 0 );
    check( r == SCREDIT_ACCEPTED && strcmp( buf, "abc" ) == 0 && rowis( 4, "> abc" ), "line editor: a burst of Backspaces wipes all that was removed" );

    clearscript();
    typetext( "zzz" ); press( SK_ESCAPE );
    strcpy( buf, "keep" );
    r = scr_lineedit( &drv, buf, sizeof(buf), 0, 0 );
    check( r == SCREDIT_CANCELLED && strcmp( buf, "keep" ) == 0, "line editor: ESC cancels, the text stays" );

    clearscript();
    typetext( "ab" );
    strcpy( buf, "" );
    r = scr_lineedit( &drv, buf, sizeof(buf), 0, 0 );
    check( r == SCREDIT_CLOSED, "line editor: the end of the keys is CLOSED" );
    clearscript();
    typetext( "last" ); press( SK_EOF );
    strcpy( buf, "" );
    r = scr_lineedit( &drv, buf, sizeof(buf), 0, 0 );
    check( r == SCREDIT_ACCEPTED && strcmp( buf, "last" ) == 0, "line editor: the end of a redirected input completes the line typed so far" );
    clearscript();
    press( SK_EOF );
    strcpy( buf, "" );
    r = scr_lineedit( &drv, buf, sizeof(buf), 0, 0 );
    check( r == SCREDIT_EOF, "line editor: the end of the input with nothing typed is EOF" );
    clearscript();
    addkey( SK_CHAR, 'd', 0, KSMOD_CTRL );
    strcpy( buf, "" );
    r = scr_lineedit( &drv, buf, sizeof(buf), 0, 0 );
    check( r == SCREDIT_EOF, "line editor: Ctrl-D on an empty line is EOF" );
    clearscript();
    typetext( "ab" ); addkey( SK_CHAR, 'd', 0, KSMOD_CTRL ); typetext( "c" ); press( SK_RETURN );
    strcpy( buf, "" );
    r = scr_lineedit( &drv, buf, sizeof(buf), 0, 0 );
    check( r == SCREDIT_ACCEPTED && strcmp( buf, "abc" ) == 0, "line editor: Ctrl-D in the middle of a line does nothing" );

    // a line that wraps over rows, and one that is too long for the screen
    freshscreens( 10, 6 );
    txtscr_select( 0 );
    clearscript();
    typetext( "0123456789ABCDEFGHIJ" ); press( SK_RETURN );
    strcpy( buf, "" );
    r = scr_lineedit( &drv, buf, sizeof(buf), 0, 0 );
    check( r == SCREDIT_ACCEPTED && strcmp( buf, "0123456789ABCDEFGHIJ" ) == 0 && rowis( 0, "0123456789" ) && rowis( 1, "ABCDEFGHIJ" ), "line editor: a line wraps over rows" );
    clearscript();
    for ( int i = 0; i < 70; ++i ) addkey( SK_CHAR, 'x', 0, 0 );
    press( SK_RETURN );
    strcpy( buf, "" );
    r = scr_lineedit( &drv, buf, sizeof(buf), 0, 0 );
    check( r == SCREDIT_ACCEPTED && strlen( buf ) == 50U, "line editor: the length is limited to the screen (one row stays free)" );
}

// the resize while editing: the line is drawn again at the same place, the cursor row stays on the screen
static int onresize_calls = 0;
static void onresize( void* u ) { (void) u; ++onresize_calls; }

static void test_lineedit_resize( void ) {
    char buf[200];
    freshscreens( 20, 8 );
    txtscr_select( 0 );
    txtscr_print( 0, 0, -1, -1, "top\nmiddle\n> " );
    clearscript();
    typetext( "abcdefghijklmnopqrstuvwxyz" ); resize( 12, 8 ); typetext( "!" ); press( SK_RETURN );
    strcpy( buf, "" );
    onresize_calls = 0;
    int r = scr_lineedit( &drv, buf, sizeof(buf), onresize, 0 );
    check( r == SCREDIT_ACCEPTED && strcmp( buf, "abcdefghijklmnopqrstuvwxyz!" ) == 0 && onresize_calls == 1, "resize while editing: the line goes on, the caller is told" );
    check( rowis( 0, "top" ) && rowis( 1, "middle" ) && rowis( 2, "> abcdefghij" ) && rowis( 3, "klmnopqrstuv" ) && rowis( 4, "wxyz!" ), "resize while editing: the line is drawn again in the new width" );
    // a smaller screen moves the contents up so that the cursor row stays
    freshscreens( 20, 8 );
    txtscr_select( 0 );
    txtscr_print( 0, 0, -1, -1, "r0\nr1\nr2\nr3\nr4\nr5\nr6\n> " );
    clearscript();
    typetext( "ab" ); resize( 20, 4 ); typetext( "c" ); press( SK_RETURN );
    strcpy( buf, "" );
    r = scr_lineedit( &drv, buf, sizeof(buf), 0, 0 );
    // (the contents moved up by 4 rows so that the prompt row stayed, then RETURN's new line scrolled once more)
    check( strcmp( buf, "abc" ) == 0 && rowis( 2, "> abc" ) && rowis( 1, "r6" ) && rowis( 0, "r5" ), "resize while editing: a smaller screen scrolls the prompt up with the text above it" );
}

// -- the screens -----------------------------------------------------------------------------------------

static void test_resize( void ) {
    freshscreens( 80, 25 );
    txtscr_select( 0 );
    txtscr_print( 0, 0, -1, -1, "abc\ndef" );
    txtscr_resize( 100, 30 );
    int w, h;
    txtscr_getscreensize( &w, &h );
    check( w == 100 && h == 30 && rowis( 0, "abc" ) && rowis( 1, "def" ) && rowis( 2, "" ), "resize: bigger screens keep their contents, the rest is blank" );
    txtscr_resize( 2, 2 );
    check( rowis( 0, "ab" ) && rowis( 1, "de" ), "resize: smaller screens keep the top left part" );
    int x, y;
    txtscr_getcursor( &x, &y );
    check( x == 1 && y == 1, "resize: the cursor stays on the screen" );
    txtscr_resize( 5000, 5000 );
    txtscr_getscreensize( &w, &h );
    check( w == TXTSCR_MAXW && h == TXTSCR_MAXH, "resize: limited to the largest screen" );
    txtscr_init();
    txtscr_getscreensize( &w, &h );
    check( w == TXTSCR_WIDTH && h == TXTSCR_HEIGHT, "init: back to the window's size" );
    // the cursor row falls off: the contents move up, a reserved row stays free
    txtscr_select( 1 );
    txtscr_print( 0, 0, -1, -1, "a\nb\nc\nd\ne\nf" );
    txtscr_getcursor( &x, &y );
    txtscr_resize( 80, 4 );
    check( txtscr_resizeshift() == 2 && rowis( 3, "f" ), "resize: a cursor below the new bottom moves the contents up" );
    txtscr_init();
    txtscr_select( 1 );
    txtscr_setresizereserve( 1 );
    txtscr_print( 0, 0, -1, -1, "a\nb\nc\nd\ne\nf" );
    txtscr_resize( 80, 4 );
    check( txtscr_resizeshift() == 3 && rowis( 2, "f" ), "resize: a reserved row (the monitor's help bar) stays free" );
    txtscr_init();
}

// -- the full-screen editor ------------------------------------------------------------------------------

static repl_t repl;

static void newprogram( void ) {
    repl_enter( &repl, "NEW" );
    repl_enter( &repl, "10 PRINT \"A\"" );
    repl_enter( &repl, "20 FOR I=1 TO 3:PRINT I:NEXT" );
    repl_enter( &repl, "30 END" );
}

static int lineof( uint32_t no ) {
    for ( line_t* ln = repl.pgm.first; ln != 0; ln = ln->next ) {
        if ( ln->lineno == no ) return 1;
    }
    return 0;
}

static void test_fulledit( void ) {
    newprogram();
    freshscreens( 80, 25 );
    clearscript();
    press( SK_ESCAPE );
    fulledit_run( &repl, &drv );
    txtscr_select( 1 );
    check( rowhas( 0, "Text Editor" ), "editor 80x25: title bar" );
    check( rowhas( 1, "10 PRINT \"A\"" ) && rowhas( 2, "20 FOR I=1 TO 3:PRINT I:NEXT" ) && rowhas( 3, "30 END" ), "editor 80x25: the program lines with their numbers" );
    check( rowhas( 24, "ESC Close" ) && rowhas( 24, "First/last" ), "editor 80x25: help bar in the last row" );
    check( strcmp( rowtext( 1 ) + 79, "^" ) == 0 && strcmp( rowtext( 23 ) + 79, "v" ) == 0, "editor 80x25: scroll bar in column 79" );
    txtscr_select( 0 );

    // inserting a line: Down, End, RETURN, number 40, a space, text, RETURN (a new insertion follows), ESC (cancels it), ESC (leaves)
    clearscript();
    pressk( KS_DOWN, 0 ); pressk( KS_END, 0 ); press( SK_RETURN ); typetext( "40 PRINT 4" ); press( SK_RETURN ); press( SK_ESCAPE ); press( SK_ESCAPE );
    fulledit_run( &repl, &drv );
    check( lineof( 40 ), "editor: a new line is entered into the program" );
    clearscript();
    // Ctrl+Y deletes the line, editing the text of a line replaces it
    pressk( KS_DOWN, 0 ); pressk( KS_DOWN, 0 ); pressk( KS_DOWN, 0 );
    addkey( SK_CHAR, 'y', 0, KSMOD_CTRL );
    press( SK_ESCAPE );
    fulledit_run( &repl, &drv );
    check( !lineof( 40 ) && lineof( 30 ), "editor: Ctrl+Y deletes the line" );
    clearscript();
    pressk( KS_END, 0 ); typetext( ":PRINT 9" ); press( SK_ESCAPE );
    fulledit_run( &repl, &drv );
    check( lineof( 30 ), "editor: leaving enters the edited line" );

    // other sizes
    newprogram();
    freshscreens( 40, 10 );
    clearscript();
    press( SK_ESCAPE );
    fulledit_run( &repl, &drv );
    txtscr_select( 1 );
    check( rowhas( 0, "Text Editor" ) && rowhas( 1, "10 PRINT \"A\"" ) && rowhas( 9, "ESC Close" ), "editor 40x10: laid out for the size" );
    check( strcmp( rowtext( 1 ) + 39, "^" ) == 0 && strcmp( rowtext( 8 ) + 39, "v" ) == 0, "editor 40x10: the scroll bar is in the last column" );
    txtscr_select( 0 );
    // a resize in the middle (the terminal): it is laid out again
    freshscreens( 80, 25 );
    clearscript();
    resize( 50, 12 ); resize( 100, 30 ); press( SK_ESCAPE );
    fulledit_run( &repl, &drv );
    txtscr_select( 1 );
    check( rowhas( 0, "Text Editor" ) && rowhas( 29, "ESC Close" ) && strcmp( rowtext( 1 ) + 99, "^" ) == 0 && strcmp( rowtext( 28 ) + 99, "v" ) == 0,
        "editor: after resizes to 50x12 and 100x30 the layout fits 100x30" );
    txtscr_select( 0 );
    // too small
    freshscreens( 80, 25 );
    clearscript();
    resize( 20, 5 );
    fulledit_run( &repl, &drv );       // the keys end: closed
    txtscr_select( 1 );
    check( rowhas( 0, "Too small" ), "editor: a screen below 24 x 6 gets a message" );
    txtscr_select( 0 );
    clearscript();
    press( SK_ESCAPE );
    freshscreens( 20, 5 );
    fulledit_run( &repl, &drv );
    check( ipos == 1, "editor: ESC leaves the 'too small' screen" );
    freshscreens( 80, 25 );
}

// -- the monitor -----------------------------------------------------------------------------------------

static void typeline( const char* t ) { typetext( t ); press( SK_RETURN ); }

static void test_monitor( void ) {
    newprogram();
    freshscreens( 80, 25 );
    clearscript();
    typeline( "?" ); typeline( "u" ); typeline( "q" );
    monitor_run( &repl, &drv );
    txtscr_select( 2 );
    check( rowhas( 0, "Machine Monitor" ) && rowhas( 0, "region code" ), "monitor 80x25: title bar" );
    check( rowhas( 24, "?help" ) && rowhas( 24, "Qquit" ), "monitor 80x25: help bar in the last row" );
    bool found = false;
    for ( int y = 1; y < 24; ++y ) found = found || rowhas( y, "LINE     10" );
    check( found, "monitor 80x25: the disassembly is in the output area" );
    txtscr_select( 0 );

    // another size, and a resize at the prompt with something typed: the prompt stays above the help bar
    freshscreens( 60, 15 );
    clearscript();
    typeline( "u" ); typetext( "ab" ); resize( 40, 9 ); typetext( "c" ); press( SK_ESCAPE ); typeline( "q" );
    monitor_run( &repl, &drv );
    txtscr_select( 2 );
    check( rowhas( 0, "Machine Monitor" ) && rowhas( 8, "?help" ), "monitor: after a resize to 40x9 the bars are in the first and last row" );
    bool prompt = false;
    for ( int y = 1; y < 8; ++y ) prompt = prompt || rowhas( y, "code> " );
    check( prompt, "monitor: after a resize the prompt is still on the screen" );
    check( !rowhas( 8, "code>" ), "monitor: the prompt is not on the help bar row" );
    txtscr_select( 0 );
    // the monitor keeps its screen between calls and follows a resize that happened while it was not shown
    clearscript();
    typeline( "q" );
    freshscreens( 60, 15 );
    monitor_run( &repl, &drv );
    txtscr_resize( 70, 20 );
    clearscript();
    typeline( "q" );
    monitor_run( &repl, &drv );
    txtscr_select( 2 );
    check( rowhas( 0, "Machine Monitor" ) && rowhas( 19, "?help" ), "monitor: a resize between two calls is followed" );
    txtscr_select( 0 );
    // too small: waits for a size that fits
    freshscreens( 80, 25 );
    clearscript();
    resize( 10, 3 ); resize( 80, 25 ); typeline( "q" );
    monitor_run( &repl, &drv );
    txtscr_select( 2 );
    check( rowhas( 0, "Machine Monitor" ) && rowhas( 24, "?help" ), "monitor: too small a screen waits until it is big enough" );
    txtscr_select( 0 );
}

int main( void ) {
    if ( !init_repl( &repl ) ) {
        printf( "init_repl failed\n" );
        return 1;
    }
    test_resize();
    test_lineedit();
    test_lineedit_resize();
    test_fulledit();
    test_monitor();
    cleanup_repl( &repl );
    printf( "%d of %d checks passed\n", total - failures, total );
    return failures;
}
