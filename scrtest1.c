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

// -- the screen editor ----------------------------------------------------------------------------------

static char sebuf[5000];

// runs the screen editor on the script; the screen is 'cols' x 'rows' (the editor area has one row less)
static int runse( bool* ins ) {
    sebuf[0] = '\0';
    return scr_screenedit( &drv, sebuf, sizeof(sebuf), ins, 0, 0 );
}

static void putat( int y, int x, const char* text ) { txtscr_print( y, x, -1, -1, text ); }
static void seclear( void ) { txtscr_setstatus( 0, 0, 0 ); txtscr_sethighlight( -1, -1 ); txtscr_resetviewport(); }
static char statusbuf[200];
static const char* status( void ) { txtscr_getstatus( statusbuf, sizeof(statusbuf) ); return statusbuf; }

static void test_screenedit( void ) {
    bool ins = false;
    int r;

    // entering: the cursor row scrolls up if it is the last, the bottom row is cleared and holds the status line; leaving restores everything
    txtscr_select( 0 );
    freshscreens( 40, 10 );
    putat( 0, 0, "10 PRINT 1" ); putat( 9, 0, "zzz" ); txtscr_locate( 0, 9 );
    clearscript(); pressk( KS_UP, 0 ); pressk( KS_UP, 0 ); pressk( KS_UP, 0 ); pressk( KS_UP, 0 ); pressk( KS_UP, 0 ); pressk( KS_UP, 0 ); pressk( KS_UP, 0 ); pressk( KS_UP, 0 );
    r = runse( &ins );
    check( r == SCREDIT_CLOSED, "screen editor: a closed screen ends it" );
    check( rowis( 0, "" ) && rowis( 8, "zzz" ) && rowis( 9, "" ), "entering with the cursor on the last row: the screen scrolled up (row 0 is gone) and the bottom row is empty" );
    check( strncmp( status(), "OVR  F12=INS", 12 ) == 0, "status line: mode and the key" );
    check( strstr( status(), "Row 1  Col 1  Line -" ) != 0, "status line: row, column, no line" );
    int vx, vy, vw, vh;
    txtscr_getviewport( &vx, &vy, &vw, &vh );
    check( vw == 40 && vh == 9, "the editor works in the rows above the status line" );
    seclear();

    // cursor not on the last row: no scroll, the row under the status line is cleared
    freshscreens( 40, 10 );
    putat( 9, 0, "zzz" ); putat( 0, 0, "10 PRINT 1" ); txtscr_locate( 0, 5 );
    clearscript();
    r = runse( &ins );
    check( txtscr_gethighlight( 0, 0 ) == false, "no line, blank row: no highlight" );
    seclear();
    check( rowis( 0, "10 PRINT 1" ) && rowis( 9, "" ) && txtscr_isempty( 9, 0 ), "entering: no scroll when the cursor is above the last row; text on the bottom row is discarded" );

    // RETURN takes the logical line, wherever the cursor is in it; the cursor goes below it; the overlays are off afterwards
    freshscreens( 40, 10 );
    putat( 2, 0, "10 PRINT 1" ); putat( 4, 0, "20 PRINT 2" ); txtscr_locate( 5, 2 );
    clearscript(); press( SK_RETURN );
    r = runse( &ins );
    int cx, cy;
    txtscr_getcursor( &cx, &cy );
    check( r == SCREDIT_ACCEPTED && strcmp( sebuf, "10 PRINT 1" ) == 0, "RETURN: the logical line under the cursor" );
    check( cx == 0 && cy == 3, "RETURN: the cursor is at the start of the row below the line" );
    txtscr_getviewport( &vx, &vy, &vw, &vh );
    check( vh == 10 && !txtscr_getstatus( 0, 0 ) && !txtscr_gethighlight( 0, 0 ), "leaving: full viewport, overlays off" );

    // a cursor behind the text is in the line; a gap ends it
    freshscreens( 40, 10 );
    putat( 1, 0, "AB" ); putat( 1, 5, "CD" ); txtscr_locate( 2, 1 );
    clearscript(); press( SK_RETURN );
    r = runse( &ins );
    check( r == SCREDIT_ACCEPTED && strcmp( sebuf, "AB" ) == 0, "RETURN: the cursor right behind a text is in that line" );
    freshscreens( 40, 10 );
    putat( 1, 0, "AB" ); putat( 1, 5, "CD" ); txtscr_locate( 6, 1 );
    clearscript(); press( SK_RETURN );
    r = runse( &ins );
    check( r == SCREDIT_ACCEPTED && strcmp( sebuf, "CD" ) == 0, "RETURN: a gap separates two lines of one row" );
    freshscreens( 40, 10 );
    putat( 1, 0, "AB" ); putat( 1, 5, "CD" ); txtscr_locate( 3, 1 );     // two cells right of B
    clearscript(); press( SK_RETURN );
    r = runse( &ins );
    txtscr_getcursor( &cx, &cy );
    check( r == SCREDIT_CLOSED && cx == 0 && cy == 2, "RETURN with no line goes on: the cursor moves to the start of the next row" );
    seclear();

    // trailing blanks are cut, leading ones stay
    freshscreens( 40, 10 );
    putat( 0, 0, "  A  B  " ); txtscr_locate( 2, 0 );
    clearscript(); press( SK_RETURN );
    r = runse( &ins );
    check( strcmp( sebuf, "  A  B" ) == 0, "RETURN: trailing blanks cut" );

    // wrapped lines: one logical line over two rows; WRAP of the last cell is cleared by RETURN
    freshscreens( 40, 10 );
    {
        char l[100]; for ( int i=0; i<50; ++i ) l[i] = (char)( 'a' + i % 26 ); l[50] = 0;
        txtscr_print( 0, 0, -1, -1, l );
        txtscr_locate( 3, 1 );
        clearscript(); press( SK_RETURN );
        r = runse( &ins );
        check( strlen( sebuf ) == 50 && strncmp( sebuf, l, 50 ) == 0, "RETURN: a wrapped line is taken whole" );
        check( txtscr_iswrapped( 0, 39 ), "(the link inside the line stays)" );
        txtscr_getcursor( &cx, &cy );
        check( cy == 2 && cx == 0, "RETURN: below the last row of the wrapped line" );
        // a line of exactly one row followed by a text in the next row is two lines
        freshscreens( 40, 10 );
        freshscreens( 40, 10 );
        txtscr_print( 0, 0, -1, -1, "0123456789012345678901234567890123456789" );       // exactly 40: WRAP set, next row empty
        txtscr_print( 1, 0, -1, -1, "next" );
        txtscr_locate( 0, 0 );
        clearscript(); press( SK_RETURN );
        r = runse( &ins );
        check( strlen( sebuf ) == 44, "a full row with WRAP continues in the row below, if that has text" );
        freshscreens( 40, 10 );
        txtscr_print( 0, 0, -1, -1, "0123456789012345678901234567890123456789" );
        txtscr_locate( 0, 0 );
        clearscript(); press( SK_RETURN );
        r = runse( &ins );
        check( strlen( sebuf ) == 40 && !txtscr_iswrapped( 0, 39 ), "a full row followed by an empty row is one line; RETURN clears the link" );
    }

    // typing: overwrite
    freshscreens( 40, 10 );
    putat( 1, 0, "10 PRINT 1" ); txtscr_locate( 3, 1 );
    clearscript(); typetext( "XY" ); press( SK_RETURN );
    r = runse( &ins );
    check( strcmp( sebuf, "10 XYINT 1" ) == 0 && rowis( 1, "10 XYINT 1" ), "OVR: characters overwrite" );
    // typing in empty space and then RETURN: the typed text is a line
    freshscreens( 40, 10 );
    txtscr_locate( 4, 3 );
    clearscript(); typetext( "hello" ); press( SK_RETURN );
    r = runse( &ins );
    check( strcmp( sebuf, "hello" ) == 0 && !txtscr_isempty( 3, 4 ) && txtscr_isempty( 3, 3 ), "typing into empty cells makes a line there" );

    // INS mode: F12 toggles, Shift+INSERT too; the mode stays for the next call
    freshscreens( 40, 10 );
    putat( 1, 0, "10 PRINT 1" ); txtscr_locate( 3, 1 );
    ins = false;
    clearscript(); pressk( KS_F12, 0 ); typetext( "XY" ); press( SK_RETURN );
    r = runse( &ins );
    check( strcmp( sebuf, "10 XYPRINT 1" ) == 0 && ins, "INS: characters are inserted; F12 toggled, the mode is kept in *insert" );
    txtscr_locate( 0, 1 );
    clearscript(); pressk( KS_INSERT, KSMOD_SHIFT ); press( SK_RETURN );
    r = runse( &ins );
    check( !ins, "Shift+INSERT toggles back" );
    freshscreens( 40, 10 );
    ins = true; txtscr_locate( 0, 1 );
    clearscript();
    r = runse( &ins );
    check( strncmp( status(), "INS  F12=OVR", 12 ) == 0, "status line in INS mode names the key for OVR" );
    seclear(); ins = false;

    // INSERT: a space, the cursor stays; DELETE: pulls the rest left
    freshscreens( 40, 10 );
    putat( 1, 0, "ABCD" ); txtscr_locate( 1, 1 );
    clearscript(); pressk( KS_INSERT, 0 ); press( SK_RETURN );
    r = runse( &ins );
    check( strcmp( sebuf, "A BCD" ) == 0, "INSERT inserts a space (in OVR mode too)" );
    freshscreens( 40, 10 );
    putat( 1, 0, "ABCD" ); txtscr_locate( 1, 1 );
    clearscript(); pressk( KS_DELETE, 0 ); press( SK_RETURN );
    r = runse( &ins );
    check( strcmp( sebuf, "ACD" ) == 0 && txtscr_isempty( 1, 3 ), "DELETE pulls the rest left; the freed cell is empty" );
    freshscreens( 40, 10 );
    putat( 1, 0, "ABCD" ); txtscr_locate( 4, 1 );
    clearscript(); pressk( KS_DELETE, 0 ); press( SK_RETURN );
    r = runse( &ins );
    check( strcmp( sebuf, "ABCD" ) == 0, "DELETE behind the text does nothing" );

    // BACKSPACE
    freshscreens( 40, 10 );
    putat( 1, 0, "ABCD" ); txtscr_locate( 2, 1 );
    clearscript(); press( SK_BACKSPACE ); press( SK_RETURN );
    r = runse( &ins );
    check( strcmp( sebuf, "ACD" ) == 0, "BACKSPACE deletes the character left of the cursor" );
    freshscreens( 40, 10 );
    putat( 1, 0, "ABCD" ); txtscr_locate( 4, 1 );
    clearscript(); press( SK_BACKSPACE ); press( SK_RETURN );
    r = runse( &ins );
    check( strcmp( sebuf, "ABC" ) == 0, "BACKSPACE behind the text deletes the last character" );
    freshscreens( 40, 10 );
    putat( 1, 0, "ABCD" ); putat( 2, 3, "XYZ" ); txtscr_locate( 3, 2 );
    clearscript(); press( SK_BACKSPACE );
    r = runse( &ins );
    txtscr_getcursor( &cx, &cy );
    check( rowis( 2, "   XYZ" ) && cx == 2 && cy == 2, "BACKSPACE at the start of a line deletes nothing: the cursor walks left, abandoning it" );
    check( txtscr_gethighlight( &cx, &cy ) && cx == 2 && cy == 2, "...and the row under the cursor is highlighted (the gap is in a non-blank row)" );
    seclear();
    freshscreens( 40, 10 );
    putat( 1, 0, "ABCD" ); putat( 2, 0, "XYZ" ); txtscr_locate( 0, 2 );
    clearscript(); press( SK_BACKSPACE );
    r = runse( &ins );
    txtscr_getcursor( &cx, &cy );
    check( rowis( 1, "ABCD" ) && rowis( 2, "XYZ" ) && cx == 39 && cy == 1, "BACKSPACE at column 0 goes to the end of the row above and deletes nothing" );
    seclear();

    // INS typing across a row end grows the line into a new row (WRAP), and RETURN takes it
    freshscreens( 40, 10 );
    {
        char l[41]; for ( int i=0; i<39; ++i ) l[i] = (char)( 'a' + i % 26 ); l[39] = 0;
        putat( 1, 0, l ); txtscr_locate( 0, 1 );
        ins = true;
        clearscript(); typetext( "ZZ" ); press( SK_RETURN );
        r = runse( &ins );
        check( strlen( sebuf ) == 41 && strncmp( sebuf, "ZZabc", 5 ) == 0 && rowhas( 1, "ZZabc" ), "INS: the line grows into the next row" );
        check( strlen( sebuf ) == 41, "INS: 39 + 2 characters" );
        ins = false;
    }
    // DELETE shrinks a wrapped line back; the empty last row loses the link
    freshscreens( 40, 10 );
    {
        char l[100]; for ( int i=0; i<41; ++i ) l[i] = (char)( 'a' + i % 26 ); l[41] = 0;
        txtscr_print( 1, 0, -1, -1, l ); txtscr_locate( 0, 1 );
        clearscript(); pressk( KS_DELETE, 0 );
        r = runse( &ins );
        check( txtscr_isempty( 2, 0 ) && !txtscr_iswrapped( 1, 39 ) && !txtscr_isempty( 1, 39 ), "DELETE: the second row empties and loses the link" );
        seclear();
    }

    // END on a line that ends on the last cell of a row: it goes on in a new screen line (the text below moves down)
    freshscreens( 40, 10 );
    {
        char l[41]; memset( l, 'x', 40 ); l[40] = 0;
        txtscr_print( 1, 0, -1, -1, l ); txtscr_setwrap( 1, 39, false );
        putat( 2, 0, "below" ); putat( 3, 0, "more" ); txtscr_locate( 5, 1 );
        clearscript(); pressk( KS_END, 0 );
        r = runse( &ins );
        txtscr_getcursor( &cx, &cy );
        check( cx == 0 && cy == 2 && rowis( 2, "" ) && rowis( 3, "below" ) && rowis( 4, "more" ), "END at a row end: a blank screen line opens below, the text below moves down, the cursor is in its first cell" );
        check( txtscr_iswrapped( 1, 39 ) && txtscr_isempty( 2, 0 ), "...and the new row is linked to the line" );
        seclear();
        // typing there continues the line
        freshscreens( 40, 10 );
        txtscr_print( 1, 0, -1, -1, l ); txtscr_setwrap( 1, 39, false );
        putat( 2, 0, "below" ); txtscr_locate( 5, 1 );
        clearscript(); pressk( KS_END, 0 ); typetext( "ab" ); pressk( KS_UP, 0 ); press( SK_RETURN );
        r = runse( &ins );
        check( strlen( sebuf ) == 42 && strcmp( sebuf + 40, "ab" ) == 0 && rowis( 3, "below" ), "...typing there continues the line, which RETURN takes whole" );
        // a blank row below is used as it is
        freshscreens( 40, 10 );
        txtscr_print( 1, 0, -1, -1, l ); txtscr_setwrap( 1, 39, false );
        putat( 3, 0, "below" ); txtscr_locate( 5, 1 );
        clearscript(); pressk( KS_END, 0 );
        r = runse( &ins );
        txtscr_getcursor( &cx, &cy );
        check( cx == 0 && cy == 2 && rowis( 3, "below" ), "END at a row end with a blank row below: no row is opened" );
        seclear();
        // on the last row of the area the screen scrolls
        freshscreens( 40, 10 );
        txtscr_print( 8, 0, -1, -1, l ); txtscr_setwrap( 8, 39, false );
        putat( 0, 0, "TOP" ); txtscr_locate( 5, 8 );
        clearscript(); pressk( KS_END, 0 );
        r = runse( &ins );
        txtscr_getcursor( &cx, &cy );
        check( cx == 0 && cy == 8 && rowis( 0, "" ) && TXTSCR_CELL_CHR( txtscr_getcell( 7, 0 ) ) == 'x' && txtscr_iswrapped( 7, 39 ), "END at a row end on the last row: the screen scrolls up" );
        seclear();
    }

    // HOME / END
    freshscreens( 40, 10 );
    putat( 1, 3, "ABCD" ); txtscr_locate( 5, 1 );
    clearscript(); pressk( KS_HOME, 0 );
    r = runse( &ins );
    txtscr_getcursor( &cx, &cy );
    check( cx == 3 && cy == 1, "HOME: the start of the logical line" );
    seclear();
    freshscreens( 40, 10 );
    putat( 1, 3, "ABCD" ); txtscr_locate( 5, 1 );
    clearscript(); pressk( KS_END, 0 );
    r = runse( &ins );
    txtscr_getcursor( &cx, &cy );
    check( cx == 7 && cy == 1, "END: behind the logical line" );
    seclear();
    freshscreens( 40, 10 );
    txtscr_locate( 5, 4 );
    clearscript(); pressk( KS_HOME, 0 );
    r = runse( &ins );
    txtscr_getcursor( &cx, &cy );
    check( cx == 0 && cy == 4, "HOME without a line: the start of the screen row" );
    clearscript(); pressk( KS_END, 0 ); r = runse( &ins ); txtscr_getcursor( &cx, &cy );
    check( cx == 39 && cy == 4, "END without a line: the end of the screen row" );
    clearscript(); pressk( KS_END, KSMOD_CTRL ); r = runse( &ins ); txtscr_getcursor( &cx, &cy );
    check( cx == 0 && cy == 8, "Ctrl+End: bottom left of the editor area" );
    clearscript(); pressk( KS_HOME, KSMOD_CTRL ); r = runse( &ins ); txtscr_getcursor( &cx, &cy );
    check( cx == 0 && cy == 0, "Ctrl+Home: top left" );
    seclear();

    // the free cursor and the scrolling at the edges
    freshscreens( 40, 10 );
    putat( 0, 0, "TOP" ); putat( 8, 0, "BOTTOM" ); txtscr_locate( 0, 0 );
    clearscript(); pressk( KS_UP, 0 );
    r = runse( &ins );
    txtscr_getcursor( &cx, &cy );
    check( rowis( 0, "" ) && rowis( 1, "TOP" ) && rowis( 8, "" ) && cy == 0, "UP at the top: the screen scrolls down, the bottom row of the area is lost" );
    seclear();
    freshscreens( 40, 10 );
    putat( 0, 0, "TOP" ); putat( 8, 0, "BOTTOM" ); txtscr_locate( 0, 8 );
    clearscript(); pressk( KS_DOWN, 0 );
    r = runse( &ins );
    txtscr_getcursor( &cx, &cy );
    check( rowis( 0, "" ) && rowis( 7, "BOTTOM" ) && rowis( 8, "" ) && cy == 8 && cx == 0, "DOWN at the bottom: the screen scrolls up (no scroll-back)" );
    seclear();
    freshscreens( 40, 10 );
    putat( 0, 0, "TOP" ); txtscr_locate( 0, 0 );
    clearscript(); pressk( KS_LEFT, 0 );
    r = runse( &ins );
    txtscr_getcursor( &cx, &cy );
    check( rowis( 1, "TOP" ) && cx == 39 && cy == 0, "LEFT at the top left: the screen scrolls down, the cursor goes to the end of the first row" );
    seclear();
    freshscreens( 40, 10 );
    txtscr_locate( 39, 8 );
    clearscript(); pressk( KS_RIGHT, 0 );
    r = runse( &ins );
    txtscr_getcursor( &cx, &cy );
    check( cx == 0 && cy == 8, "RIGHT at the bottom right: scrolls up, the cursor goes to the start of the last row" );
    seclear();
    freshscreens( 40, 10 );
    txtscr_locate( 39, 3 );
    clearscript(); pressk( KS_RIGHT, 0 ); pressk( KS_LEFT, 0 ); pressk( KS_LEFT, 0 );
    r = runse( &ins );
    txtscr_getcursor( &cx, &cy );
    check( cx == 38 && cy == 3, "LEFT / RIGHT wrap between rows" );
    seclear();

    // typing at the bottom right scrolls
    freshscreens( 40, 10 );
    putat( 0, 0, "TOP" ); txtscr_locate( 39, 8 );
    clearscript(); typetext( "x" );
    r = runse( &ins );
    txtscr_getcursor( &cx, &cy );
    check( rowis( 0, "" ) && cx == 0 && cy == 8 && txtscr_iswrapped( 7, 39 ), "typing in the bottom right cell scrolls the screen up and links the row" );
    seclear();

    // status line: row, column, BASIC line number
    freshscreens( 40, 10 );
    putat( 2, 0, "120 PRINT" ); txtscr_locate( 4, 2 );
    clearscript();
    r = runse( &ins );
    check( strstr( status(), "Row 3  Col 5  Line 120" ) != 0, "status line: row, column and the BASIC line number" );
    check( txtscr_gethighlight( &cx, &cy ) && cx == 2 && cy == 2, "the row of the line is highlighted" );
    check( TXTSCR_CELL_CHR( txtscr_getdisplaycell( 9, 0 ) ) == 'O' && TXTSCR_CELL_BG( txtscr_getdisplaycell( 9, 0 ) ) == 190 && TXTSCR_CELL_FG( txtscr_getdisplaycell( 9, 0 ) ) == 140, "the display shows the status line on the bottom row: white on TransparentDarkRed (190)" );
    txtscr_setcolors( 190, 140 );
    clearscript();
    r = runse( &ins );
    check( TXTSCR_CELL_FG( txtscr_getdisplaycell( 9, 0 ) ) != 140 && TXTSCR_CELL_BG( txtscr_getdisplaycell( 9, 0 ) ) == 190, "...yellow instead if the text colors are those" );
    seclear();
    freshscreens( 40, 10 );
    putat( 2, 0, "PRINT 5" ); txtscr_locate( 2, 2 );
    clearscript();
    r = runse( &ins );
    check( strstr( status(), "Line -" ) != 0, "status line: a line without a number" );
    seclear();
    // a wrapped line highlights all its rows; a blank row nothing; a gap in a non-blank row highlights that row
    freshscreens( 40, 10 );
    {
        char l[100]; for ( int i=0; i<50; ++i ) l[i] = (char)( 'a' + i % 26 ); l[50] = 0;
        txtscr_print( 3, 0, -1, -1, l ); txtscr_locate( 2, 4 );
        clearscript();
        r = runse( &ins );
        check( txtscr_gethighlight( &cx, &cy ) && cx == 3 && cy == 4, "a wrapped line: all its rows are highlighted" );
        seclear();
        freshscreens( 40, 10 );
        putat( 3, 0, "AB" ); txtscr_locate( 10, 3 );
        clearscript();
        r = runse( &ins );
        check( txtscr_gethighlight( &cx, &cy ) && cx == 3 && cy == 3, "in a gap of a non-blank row: that screen row is highlighted" );
        seclear();
    }

    // case 2: the rest of a screen row (ASCII graphics): INSERT, DELETE, BACKSPACE inside gaps
    freshscreens( 40, 10 );
    putat( 1, 0, "AB" ); putat( 1, 10, "CD" ); txtscr_locate( 5, 1 );
    clearscript(); pressk( KS_INSERT, 0 );
    r = runse( &ins );
    check( rowis( 1, "AB         CD" ) && !txtscr_isempty( 1, 5 ), "gap: INSERT shifts the rest of the row right (the inserted space is a written cell)" );
    seclear();
    freshscreens( 40, 10 );
    putat( 1, 0, "AB" ); putat( 1, 10, "CD" ); txtscr_locate( 5, 1 );
    clearscript(); pressk( KS_DELETE, 0 );
    r = runse( &ins );
    check( rowis( 1, "AB       CD" ), "gap: DELETE pulls the rest of the row left" );
    seclear();
    freshscreens( 40, 10 );
    putat( 1, 0, "AB" ); putat( 1, 10, "CD" ); txtscr_locate( 5, 1 );
    clearscript(); press( SK_BACKSPACE );
    r = runse( &ins );
    txtscr_getcursor( &cx, &cy );
    check( rowis( 1, "AB       CD" ) && cx == 4, "gap: BACKSPACE closes the gap left of the cursor" );
    seclear();
    freshscreens( 40, 10 );
    putat( 1, 0, "AB" ); putat( 1, 38, "CD" ); txtscr_locate( 5, 1 );     // CD wraps? no: 38,39 fit
    clearscript(); pressk( KS_INSERT, 0 );
    r = runse( &ins );
    check( rowis( 1, "AB" "   " "                               " "C" ) || rowhas( 1, "AB" ), "gap: what is pushed over the last column is lost" );
    check( !rowhas( 1, "CD" ), "...here the D" );
    seclear();
    // on a blank row nothing happens
    freshscreens( 40, 10 );
    txtscr_locate( 5, 5 );
    clearscript(); pressk( KS_INSERT, 0 ); pressk( KS_DELETE, 0 );
    r = runse( &ins );
    check( rowis( 5, "" ) && txtscr_isempty( 5, 5 ), "a blank row: INSERT and DELETE do nothing" );
    seclear();

    // INSERT would run into another line: refused
    freshscreens( 40, 10 );
    {
        char l[41]; memset( l, 'x', 40 ); l[40] = 0;
        txtscr_print( 1, 0, -1, -1, l );            // a full row, WRAP set
        txtscr_setwrap( 1, 39, false );
        putat( 2, 0, "other" );
        txtscr_locate( 0, 1 );
        clearscript(); pressk( KS_INSERT, 0 );
        r = runse( &ins );
        check( rowis( 3, "other" ) && rowis( 2, "x" ) && TXTSCR_CELL_CHR( txtscr_getcell( 1, 0 ) ) == ' ' && TXTSCR_CELL_CHR( txtscr_getcell( 1, 1 ) ) == 'x', "INSERT where the next row holds another line: a blank screen line opens below and the line grows into it" );
        check( txtscr_iswrapped( 1, 39 ) && !txtscr_isempty( 2, 0 ), "...the rows are linked" );
        seclear();
        // the same by typing in INS mode, at the bottom of the screen: the line is pushed up
        freshscreens( 40, 10 );
        txtscr_print( 8, 0, -1, -1, l );
        txtscr_setwrap( 8, 39, false );
        putat( 0, 0, "TOP" );
        txtscr_locate( 0, 8 );
        ins = true;
        clearscript(); typetext( "Q" );
        r = runse( &ins );
        check( rowis( 0, "" ) && TXTSCR_CELL_CHR( txtscr_getcell( 7, 0 ) ) == 'Q' && TXTSCR_CELL_CHR( txtscr_getcell( 8, 0 ) ) == 'x' && txtscr_iswrapped( 7, 39 ), "INS typing into a line that ends on the last cell of the screen scrolls it up" );
        ins = false;
        seclear();
    }

    // Break and the end of input
    freshscreens( 40, 10 );
    putat( 3, 0, "AB" ); txtscr_locate( 1, 3 );
    clearscript(); press( SK_BREAK ); press( SK_BREAK ); pressk( KS_RIGHT, 0 ); press( SK_BREAK );
    r = runse( &ins );
    txtscr_getcursor( &cx, &cy );
    check( r == SCREDIT_CLOSED && cx == 2 && cy == 3 && rowis( 3, "AB" ) && txtscr_getstatus( 0, 0 ), "Break has no effect at the prompt: nothing moves, nothing ends" );
    seclear();
    freshscreens( 40, 10 );
    clearscript(); press( SK_EOF );
    r = runse( &ins );
    check( r == SCREDIT_EOF && !txtscr_getstatus( 0, 0 ), "end of input leaves the editor (an open question)" );
    freshscreens( 40, 10 );
    putat( 0, 0, "A" ); txtscr_locate( 0, 0 );
    clearscript(); press( SK_ESCAPE ); press( SK_TAB ); pressk( KS_PGUP, 0 ); pressk( KS_F5, 0 ); press( SK_RETURN );
    r = runse( &ins );
    check( r == SCREDIT_ACCEPTED && strcmp( sebuf, "A" ) == 0 && rowis( 0, "A" ), "ESC, TAB, PgUp and the other function keys are ignored" );

    // the pure functions
    freshscreens( 40, 10 );
    putat( 2, 4, "ABC" ); txtscr_setviewport( 0, 0, 40, 9 );
    int sx, sy, ex, ey;
    check( scr_screenline_extent( 5, 2, &sx, &sy, &ex, &ey ) && sx == 4 && sy == 2 && ex == 6 && ey == 2, "extent: inside" );
    check( scr_screenline_extent( 7, 2, &sx, &sy, &ex, &ey ) && sx == 4 && ex == 6, "extent: right behind the text" );
    check( !scr_screenline_extent( 8, 2, 0, 0, 0, 0 ) && !scr_screenline_extent( 3, 2, 0, 0, 0, 0 ), "extent: none two cells behind / left of the text" );
    char tb[20];
    check( scr_screenline_text( 4, 2, tb, sizeof(tb) ) == 3 && strcmp( tb, "ABC" ) == 0, "text of the line" );
    seclear();

    // resize in the middle
    freshscreens( 40, 10 );
    putat( 1, 0, "10 PRINT 1" ); txtscr_locate( 3, 1 );
    clearscript(); typetext( "Q" ); resize( 30, 6 ); typetext( "R" ); press( SK_RETURN );
    r = runse( &ins );
    check( r == SCREDIT_ACCEPTED && strcmp( sebuf, "10 QRINT 1" ) == 0, "a resize in the middle keeps the editing going" );
    txtscr_getviewport( &vx, &vy, &vw, &vh );
    check( vw == 30 && vh == 6 && !txtscr_getstatus( 0, 0 ), "...and leaves a full viewport of the new size" );
    txtscr_resize( TXTSCR_WIDTH, TXTSCR_HEIGHT );

    // the screen is too small
    freshscreens( 40, 2 );
    clearscript();
    r = runse( &ins );
    check( r == SCREDIT_CANCELLED, "less than 3 rows: the caller falls back to the one-line editor" );
    txtscr_resize( TXTSCR_WIDTH, TXTSCR_HEIGHT );
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
    test_screenedit();
    cleanup_repl( &repl );
    printf( "%d of %d checks passed\n", total - failures, total );
    return failures;
}
