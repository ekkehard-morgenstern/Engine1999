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

#include "ui.h"
#include "joy.h"
#include "gfxcore.h"
#include "clipboard.h"

/*
Automated test of the UI subsystem (ui.[ch]) without a screen or a VM: widgets are created, mouse and keys are fed in, the events and the state are checked,
and the drawing is checked by pixels on a gfxcore bitmap. The exit status is the number of failed checks.
*/

static int checks = 0, failures = 0;

static void check( const char* name, bool ok ) {
    ++checks;
    if ( !ok ) {
        ++failures;
        printf( "FAIL  %s\n", name );
    }
}

static uint8_t buf[ 400 * 240 ];
static gfx_t   g;
static ui_t    ui;
static uint32_t now = 1000;

static long painted = 0;                                // the area of the boxes drawn since it was last cleared (to see how much a redraw touches)

static void c_box( void* ctx, int x1, int y1, int x2, int y2, int color, bool fill ) {
    painted += (long)( x2 - x1 + 1 ) * ( y2 - y1 + 1 );
    gfx_box( ctx, x1, y1, x2, y2, color, fill );
}

static void c_pset( void* ctx, int x, int y, int color ) {
    gfx_pset( ctx, x, y, color );
}

static uint32_t rnd_state = 12345U;
static int RND( int n ) {
    rnd_state = rnd_state * 1103515245U + 12345U;
    return (int)( ( rnd_state >> 16 ) % (unsigned) n );
}

static void setup( void ) {
    gfx_init( &g, buf, 400, 240 );
    gfx_clear( &g, 0 );
    ui_canvas_t c = { &g, 400, 240, c_box, c_pset };
    ui_init( &ui, &c, 100 );                       // the theme in the palette entries 100 .. 115
}

static void done( void ) {
    ui_free( &ui );
}

static void mouse( int x, int y, int buttons, int wheel ) {
    ui_input( &ui, x, y, buttons, wheel, 0, 0, now );
}

static void click( int x, int y ) {
    mouse( x, y, UIB_LEFT, 0 );
    mouse( x, y, 0, 0 );
}

static void keys( const int* k, int n ) {
    ui_input( &ui, 0, 0, 0, 0, k, n, now );
}

static void key( int k ) {
    keys( &k, 1 );
}

static void typed( const char* s ) {
    for ( ; *s; ++s ) {
        key( (unsigned char) *s );
    }
}

static int nevents( void ) {
    return ui.qcount;
}

// the next event must be this one
static bool expect( int id, int kind, int value ) {
    ui_event_t e;
    return ui_next_event( &ui, &e ) && e.id == id && e.kind == kind && e.value == value;
}

static bool none( void ) {
    ui_event_t e;
    return !ui_next_event( &ui, &e );
}

static void drain( void ) {
    ui_event_t e;
    while ( ui_next_event( &ui, &e ) ) {
    }
}

static int count_color( int x1, int y1, int x2, int y2, int color ) {
    int n = 0;
    for ( int y = y1; y <= y2; ++y ) {
        for ( int x = x1; x <= x2; ++x ) {
            if ( gfx_point( &g, x, y ) == color ) {
                ++n;
            }
        }
    }
    return n;
}


// -- the multi-line text editor (UIK_EDIT) ---------------------------------------------------------------------------------------------------------------

static char fakeclip[400];
static char* fake_clip_get( size_t* len ) {
    if ( fakeclip[0] == '\0' ) { *len = 0; return 0; }
    *len = strlen( fakeclip );
    return strdup( fakeclip );
}
static void fake_clip_set( const char* t, size_t n ) {
    snprintf( fakeclip, sizeof(fakeclip), "%.*s", (int) n, t );
}

static int ed_caret( void ) { return ui_edit_query( &ui, 1, 0 ); }
static const char* ed_text( void ) { return ui_get_text( &ui, 1 ); }
static void keymod( int k, int mods ) { key( k | mods ); }

// the pixel position of the character at column col of the first line (fixed font: 8 wide), for the widget at 10, 10
static int colx( int col ) { return 10 + 3 + col * 8 + 1; }

static void test_edit( void ) {
    setup();
    ui.clip_get = fake_clip_get; ui.clip_set = fake_clip_set;
    fakeclip[0] = '\0';
    check( "edit: a multi-line editor is created", ui_add( &ui, UIK_EDIT, 1, 10, 10, 200, 80, "hello\nworld", 0 ) && ui.focus == 1 );
    check( "edit: the text, the caret, the lines", strcmp( ed_text(), "hello\nworld" ) == 0 && ed_caret() == 0 && ui_edit_query( &ui, 1, 4 ) == 2 );
    drain();
    typed( "ab" );
    check( "edit: typing inserts at the caret and queues changes", strcmp( ed_text(), "abhello\nworld" ) == 0 && ed_caret() == 2 && expect( 1, UIE_CHANGE, 12 ) );
    drain();
    key( UIKEY_END );
    check( "edit: End goes to the end of the line", ed_caret() == 7 );
    key( UIKEY_ENTER );
    check( "edit: Return inserts a line end", strcmp( ed_text(), "abhello\n\nworld" ) == 0 && ed_caret() == 8 && ui_edit_query( &ui, 1, 4 ) == 3 );
    key( UIKEY_BACKSPACE );
    check( "edit: BackSpace removes it again", strcmp( ed_text(), "abhello\nworld" ) == 0 && ed_caret() == 7 );
    key( UIKEY_DOWN );
    check( "edit: Down goes to the next line, the column is kept as far as the line goes", ui_edit_query( &ui, 1, 1 ) == 1 && ui_edit_query( &ui, 1, 2 ) == 5 );
    key( UIKEY_UP );
    check( "edit: Up goes back to the wanted column", ed_caret() == 7 );
    key( UIKEY_HOME );
    key( UIKEY_DELETE );
    check( "edit: Delete removes the character", strcmp( ed_text(), "bhello\nworld" ) == 0 );
    // selection by Shift + cursor keys, typing replaces it
    keymod( UIKEY_RIGHT, UIKEY_SHIFT ); keymod( UIKEY_RIGHT, UIKEY_SHIFT ); keymod( UIKEY_RIGHT, UIKEY_SHIFT );
    check( "edit: Shift + Right selects", ui_edit_query( &ui, 1, 3 ) == 3 && ed_caret() == 3 );
    typed( "X" );
    check( "edit: typing replaces the selection", strcmp( ed_text(), "Xllo\nworld" ) == 0 && ui_edit_query( &ui, 1, 3 ) == 0 );
    // the clipboard
    key( UIKEY_CTRL | 'a' );
    check( "edit: Ctrl+A selects everything", ui_edit_query( &ui, 1, 3 ) == 10 );
    key( UIKEY_CTRL | 'c' );
    check( "edit: Ctrl+C copies the selection", strcmp( fakeclip, "Xllo\nworld" ) == 0 );
    key( UIKEY_CTRL | UIKEY_END );
    check( "edit: Ctrl+End goes to the end of the text", ed_caret() == 10 );
    key( UIKEY_CTRL | 'v' );
    check( "edit: Ctrl+V pastes", strcmp( ed_text(), "Xllo\nworldXllo\nworld" ) == 0 && ed_caret() == 20 );
    key( UIKEY_CTRL | 'z' );
    check( "edit: Ctrl+Z undoes the paste", strcmp( ed_text(), "Xllo\nworld" ) == 0 );
    key( UIKEY_CTRL | 'y' );
    check( "edit: Ctrl+Y redoes it", strcmp( ed_text(), "Xllo\nworldXllo\nworld" ) == 0 );
    key( UIKEY_CTRL | 'z' ); key( UIKEY_CTRL | 'z' );
    check( "edit: typing was one undo step (the second undo goes back further)", ed_text() != 0 );
    // cut
    ui_set_text( &ui, 1, "one two three" );
    check( "edit: a new text resets the caret and the undo", ed_caret() == 0 && ui_edit_query( &ui, 1, 3 ) == 0 );
    for ( int i = 0; i < 3; ++i ) keymod( UIKEY_RIGHT, UIKEY_SHIFT );
    key( UIKEY_CTRL | 'x' );
    check( "edit: Ctrl+X cuts", strcmp( ed_text(), " two three" ) == 0 && strcmp( fakeclip, "one" ) == 0 );
    key( UIKEY_CTRL | 'z' );
    check( "edit: Ctrl+Z brings it back", strcmp( ed_text(), "one two three" ) == 0 );
    // typing is one undo step
    ui_set_text( &ui, 1, "" );
    typed( "abc" );
    key( UIKEY_CTRL | 'z' );
    check( "edit: typed characters are one undo step", strcmp( ed_text(), "" ) == 0 );
    // pasted text may contain line ends and is normalized
    snprintf( fakeclip, sizeof(fakeclip), "a\r\nb\tc" );
    key( UIKEY_CTRL | 'v' );
    check( "edit: a paste is normalized (CR LF, tab)", strcmp( ed_text(), "a\nb c" ) == 0 );
    // the mouse: click, drag, double click
    ui_set_text( &ui, 1, "hello world\nsecond" );
    click( colx( 3 ), 10 + 3 + 2 );
    check( "edit: a click puts the caret there", ed_caret() == 3 );
    mouse( colx( 1 ), 10 + 3 + 2, UIB_LEFT, 0 );
    mouse( colx( 4 ), 10 + 3 + 2, UIB_LEFT, 0 );
    mouse( colx( 4 ), 10 + 3 + 2, 0, 0 );
    check( "edit: dragging selects", ui_edit_query( &ui, 1, 3 ) == 3 && ed_caret() == 4 );
    click( colx( 8 ), 10 + 3 + 2 );
    now += 100;
    click( colx( 8 ), 10 + 3 + 2 );
    check( "edit: a double click selects the word", ui_edit_query( &ui, 1, 3 ) == 5 && ed_caret() == 11 );
    now += 100;
    click( colx( 8 ), 10 + 3 + 2 );
    check( "edit: a triple click selects the line", ui_edit_query( &ui, 1, 3 ) == 11 );
    now += 1000;
    // drawing: the selection is drawn in the highlight color, the caret in the text color
    ui_draw( &ui, true );
    check( "edit: the selection is drawn with the highlight", count_color( 12, 12, 200, 40, ui.theme.hilite ) > 100 );
    key( UIKEY_CTRL | 'a' );
    key( UIKEY_LEFT );
    check( "edit: Left drops the selection and goes to its start", ed_caret() == 0 && ui_edit_query( &ui, 1, 3 ) == 0 );
    // a long text scrolls: more lines than rows gives a scroll bar, the wheel and the caret move the view
    ui_set_text( &ui, 1, "1\n2\n3\n4\n5\n6\n7\n8\n9\n10" );
    ui_draw( &ui, true );
    ui_widget_t* ew = ui_find( &ui, 1 );
    check( "edit: the first line is at the top", ew->scroll == 0 );
    mouse( 100, 30, 0, -1 );
    check( "edit: the wheel scrolls", ew->scroll == 3 );
    key( UIKEY_CTRL | UIKEY_END );
    check( "edit: the caret is kept in view", ui_edit_query( &ui, 1, 1 ) == 9 && ew->scroll == 9 - 5 );
    key( UIKEY_CTRL | UIKEY_HOME );
    check( "edit: Ctrl+Home goes to the start and shows it", ed_caret() == 0 && ew->scroll == 0 );
    // a press on the scroll bar scrolls (the bar is at the right edge)
    click( 10 + 200 - 3 - 7, 10 + 80 - 3 - 3 );
    check( "edit: the scroll bar works (the down arrow)", ew->scroll == 1 );
    // Shift is the key itself for other widgets, Ctrl + a key is ignored by them
    ui_add( &ui, UIK_TEXT, 2, 10, 100, 100, 18, "abc", 0 );
    ui_set_focus( &ui, 2 );
    keymod( UIKEY_LEFT, UIKEY_SHIFT );
    check( "edit: other widgets take Shift + Left as Left", ui_find( &ui, 2 )->caret == 2 );
    key( UIKEY_CTRL | 'v' );
    check( "edit: other widgets ignore Ctrl + a key", strcmp( ui_get_text( &ui, 2 ), "abc" ) == 0 );
    // Tab leaves the editor
    ui_set_focus( &ui, 1 );
    key( UIKEY_TAB );
    check( "edit: Tab moves the focus on", ui.focus == 2 );
    // the length limit and the caret statement
    char* big = (char*) malloc( UI_EDITMAX + 100 );
    memset( big, 'x', UI_EDITMAX + 99 ); big[ UI_EDITMAX + 99 ] = '\0';
    check( "edit: a long text is cut to UI_EDITMAX", ui_set_text( &ui, 1, big ) && (int) strlen( ed_text() ) == UI_EDITMAX );
    ui_set_focus( &ui, 1 );
    typed( "y" );
    check( "edit: typing into a full editor does nothing", (int) strlen( ed_text() ) == UI_EDITMAX );
    free( big );
    ui_set_text( &ui, 1, "ab\ncd" );
    check( "edit: ui_edit_caret moves the caret", ui_edit_caret( &ui, 1, 4 ) && ui_edit_query( &ui, 1, 1 ) == 1 && ui_edit_query( &ui, 1, 2 ) == 1 && !ui_edit_caret( &ui, 2, 0 ) );
    check( "edit: the queries refuse other widgets", ui_edit_query( &ui, 2, 0 ) == -1 );
    check( "edit: a remove frees it", ui_remove( &ui, 1 ) );
    done();
}

// -- colors, themes, syntax highlighting -------------------------------------------------------------------------------------------------------------------

static int cell_count( int col, int line, int color ) {         // the pixels of a color in one character cell of the editor at (10, 10)
    int x = 10 + 3 + col * 8, y = 10 + 3 + line * 12;
    return count_color( x, y, x + 7, y + 11, color );
}

static void test_colors( void ) {
    setup();
    check( "color: an unknown widget is refused", !ui_set_color( &ui, 9, 20, 30 ) );
    ui_add( &ui, UIK_BUTTON, 1, 10, 10, 80, 24, "Go", 0 );
    check( "color: 0 and 256 are refused (0 is transparent)", !ui_set_color( &ui, 1, 0, -2 ) && !ui_set_color( &ui, 1, -2, 256 ) && !ui_set_color( &ui, 1, -3, -2 ) );
    check( "color: the theme's color is the default", ui_get_color( &ui, 1, 0 ) == -1 && ui_get_color( &ui, 1, 1 ) == -1 );
    check( "color: a color is set", ui_set_color( &ui, 1, 20, 30 ) && ui_get_color( &ui, 1, 0 ) == 20 && ui_get_color( &ui, 1, 1 ) == 30 );
    check( "color: -2 leaves one as it is", ui_set_color( &ui, 1, -2, 31 ) && ui_get_color( &ui, 1, 0 ) == 20 && ui_get_color( &ui, 1, 1 ) == 31 );
    ui_draw( &ui, true );
    check( "color: the button face has the color", gfx_point( &g, 14, 14 ) == 31 );
    check( "color: its caption has the color", count_color( 10, 10, 89, 33, 20 ) > 10 );
    check( "color: -1 goes back to the theme", ui_set_color( &ui, 1, -1, -1 ) && ui_get_color( &ui, 1, 1 ) == -1 );
    ui_draw( &ui, false );
    check( "color: the face is the theme's again", gfx_point( &g, 14, 14 ) == ui.theme.face );
    ui_add( &ui, UIK_LABEL, 2, 100, 10, 60, 14, "Hi", 0 );
    ui_set_color( &ui, 2, 40, 41 );
    ui_draw( &ui, false );
    check( "color: a label with a background fills it", gfx_point( &g, 158, 11 ) == 41 );
    ui_add( &ui, UIK_TEXT, 3, 10, 60, 100, 20, "abc", 0 );
    ui_set_color( &ui, 3, 50, 51 );
    ui_draw( &ui, false );
    check( "color: a text field has its colors", gfx_point( &g, 105, 62 ) == 51 && count_color( 10, 60, 109, 79, 50 ) > 5 );
    done();

    // the presets
    uint32_t a[ UI_PAL_COLORS ], b[ UI_PAL_COLORS ], s0[ UI_SYN_COLORS ], s1[ UI_SYN_COLORS ];
    ui_theme_preset( 0, a ); ui_theme_preset( 1, b );
    uint32_t classic[ UI_PAL_COLORS ];
    ui_theme_argb( classic );
    check( "theme: preset 0 is the classic theme", memcmp( a, classic, sizeof(a) ) == 0 );
    check( "theme: the dark preset differs", memcmp( a, b, sizeof(a) ) != 0 && ( b[4] & 0xFF ) > 0x80 );
    ui_theme_preset( 7, b );
    check( "theme: an unknown preset is the classic one", memcmp( a, b, sizeof(a) ) == 0 );
    ui_syntax_argb( 0, s0 ); ui_syntax_argb( 1, s1 );
    check( "theme: the syntax colors differ per preset and are opaque", memcmp( s0, s1, sizeof(s0) ) != 0 && ( s0[2] >> 24 ) == 0xFF && ( s1[9] >> 24 ) == 0xFF );

    // an editor with BASIC highlighting (the syntax slots are the palette entries 116 + slot)
    setup();
    ui_add( &ui, UIK_EDIT, 1, 10, 10, 300, 80, "10 PRINT \"A\"\nX=1", 0 );
    ui_draw( &ui, true );
    check( "syntax: plain text has no syntax colors", cell_count( 0, 0, 116 + HL_LINENO ) == 0 && cell_count( 3, 0, 116 + HL_STATEMENT ) == 0 );
    check( "syntax: a bad mode is refused", !ui_set_syntax( &ui, 1, 5 ) && !ui_set_syntax( &ui, 9, 1 ) && ui_get_syntax( &ui, 1 ) == 0 );
    check( "syntax: BASIC is set", ui_set_syntax( &ui, 1, 1 ) && ui_get_syntax( &ui, 1 ) == 1 );
    ui_draw( &ui, false );
    check( "syntax: the line number", cell_count( 1, 0, 116 + HL_LINENO ) > 3 );
    check( "syntax: the statement", cell_count( 3, 0, 116 + HL_STATEMENT ) > 3 && cell_count( 7, 0, 116 + HL_STATEMENT ) > 3 );
    check( "syntax: the string", cell_count( 9, 0, 116 + HL_STRING ) > 2 );
    check( "syntax: identifier and number on the next line", cell_count( 0, 1, 116 + HL_IDENT ) > 3 && cell_count( 2, 1, 116 + HL_NUMBER ) > 3 );
    check( "syntax: the operator", cell_count( 1, 1, 116 + HL_OPERATOR ) > 2 );
    check( "syntax: a slot gets another color", ui_syntax_color( &ui, HL_STATEMENT, 200 ) && !ui_syntax_color( &ui, 16, 5 ) && !ui_syntax_color( &ui, 2, 0 ) );
    ui_draw( &ui, false );
    check( "syntax: the new color is drawn", cell_count( 3, 0, 200 ) > 3 && cell_count( 3, 0, 116 + HL_STATEMENT ) == 0 );
    key( UIKEY_CTRL | UIKEY_HOME );
    typed( "'" );
    ui_draw( &ui, false );
    check( "syntax: typing recolors the line (a comment now)", cell_count( 5, 0, 116 + HL_COMMENT ) > 2 && cell_count( 5, 0, 200 ) == 0 );
    ui_set_enabled( &ui, 1, false );
    ui_draw( &ui, false );
    check( "syntax: a disabled editor is drawn plain", cell_count( 5, 0, 116 + HL_COMMENT ) == 0 );
    ui_set_enabled( &ui, 1, true );
    check( "syntax: mode 0 is plain again", ui_set_syntax( &ui, 1, 0 ) );
    ui_draw( &ui, false );
    check( "syntax: plain after all", cell_count( 3, 0, 116 + HL_COMMENT ) == 0 );
    done();

    // colors that the program gives (UI HILITE)
    setup();
    ui_add( &ui, UIK_EDIT, 1, 10, 10, 300, 80, "abc", 0 );
    uint8_t cl[3] = { 0, 13, 31 };
    check( "hilite: the classes are set (only the low 4 bits count)", ui_set_hilite( &ui, 1, cl, 3 ) && ui_get_syntax( &ui, 1 ) == 2 && !ui_set_hilite( &ui, 9, cl, 3 ) );
    ui_draw( &ui, true );
    check( "hilite: slot 13 and 15 are drawn, slot 0 is plain", cell_count( 1, 0, 116 + 13 ) > 3 && cell_count( 2, 0, 116 + 15 ) > 3 && cell_count( 0, 0, 116 + 13 ) == 0 );
    key( UIKEY_CTRL | UIKEY_HOME );
    typed( "X" );
    ui_draw( &ui, false );
    check( "hilite: the colors move with the text, new text is plain", cell_count( 0, 0, 116 + 13 ) == 0 && cell_count( 2, 0, 116 + 13 ) > 3 && cell_count( 3, 0, 116 + 15 ) > 3 );
    key( UIKEY_BACKSPACE );
    key( UIKEY_DELETE );
    key( UIKEY_END );
    ui_draw( &ui, false );
    check( "hilite: a deleted byte takes its color away", cell_count( 0, 0, 116 + 13 ) > 3 && cell_count( 1, 0, 116 + 15 ) > 3 );
    ui_set_text( &ui, 1, "zz" );
    key( UIKEY_END );
    ui_draw( &ui, false );
    check( "hilite: a new text has none", cell_count( 0, 0, 116 + 13 ) == 0 && cell_count( 1, 0, 116 + 15 ) == 0 );
    ui_add( &ui, UIK_LIST, 2, 10, 100, 50, 50, "", 0 );
    check( "hilite: a list is no editor", !ui_set_hilite( &ui, 2, cl, 3 ) && !ui_set_syntax( &ui, 2, 1 ) );
    ui_set_color( &ui, 1, 60, 61 );
    ui_draw( &ui, false );
    check( "color: an editor has its colors", gfx_point( &g, 200, 60 ) == 61 && cell_count( 0, 0, 60 ) > 3 );
    done();
}

static void test_caret( void ) {
    setup();
    ui_add( &ui, UIK_EDIT, 1, 10, 10, 300, 80, "abc", 0 );
    ui_draw( &ui, true );
    check( "caret: a block as wide as a character is drawn at the caret", cell_count( 0, 0, 100 + 4 ) > 50 );
    ui_input( &ui, 0, 0, 0, 0, 0, 0, 2600 );
    ui_draw( &ui, false );
    check( "caret: it blinks off after 500 ms without action", cell_count( 0, 0, 100 + 4 ) < 40 );
    ui_input( &ui, 0, 0, 0, 0, 0, 0, 3100 );
    ui_draw( &ui, false );
    check( "caret: and on again", cell_count( 0, 0, 100 + 4 ) > 50 );
    ui_input( &ui, 0, 0, 0, 0, 0, 0, 3700 );
    ui_draw( &ui, false );
    check( "caret: off", cell_count( 0, 0, 100 + 4 ) < 40 );
    int k = UIKEY_END;
    ui_input( &ui, 0, 0, 0, 0, &k, 1, 3800 );
    ui_draw( &ui, false );
    check( "caret: a key makes it solid and moves it", cell_count( 3, 0, 100 + 4 ) > 50 && cell_count( 0, 0, 100 + 4 ) < 40 );
    ui_set_focus( &ui, 0 );
    ui_add( &ui, UIK_BUTTON, 2, 10, 120, 50, 20, "x", 0 );
    ui_set_focus( &ui, 2 );
    ui_draw( &ui, true );
    check( "caret: an editor without the focus has none", cell_count( 3, 0, 100 + 4 ) < 40 );
    done();
}

int main( void ) {
    ui_theme_t th;
    ui_theme_default( &th, 100 );
    uint32_t argb[ UI_PAL_COLORS ];
    ui_theme_argb( argb );
    bool opaque = true;
    for ( int i = 0; i < UI_PAL_COLORS; ++i ) {
        if ( ( argb[i] >> 24 ) != 0xFF ) {
            opaque = false;
        }
    }
    check( "the theme colours are opaque and start at the base", opaque && th.desk == 100 && th.fill == 112 && th.font == FONT_SANS );

    // -- creating widgets ---------------------------------------------------------------------------------------------------------------------------------
    setup();
    check( "a button is created", ui_add( &ui, UIK_BUTTON, 1, 10, 10, 60, 20, "OK", 0 ) );
    check( "the same id twice is refused", !ui_add( &ui, UIK_BUTTON, 1, 80, 10, 60, 20, "No", 0 ) );
    check( "id 0 and negative ids are refused", !ui_add( &ui, UIK_LABEL, 0, 0, 0, 10, 10, "x", 0 ) && !ui_add( &ui, UIK_LABEL, -3, 0, 0, 10, 10, "x", 0 ) );
    check( "an invalid kind is refused", !ui_add( &ui, UIK_NONE, 2, 0, 0, 10, 10, "x", 0 ) && !ui_add( &ui, UIK_COUNT, 2, 0, 0, 10, 10, "x", 0 ) );
    check( "an empty size is refused", !ui_add( &ui, UIK_LABEL, 2, 0, 0, 0, 10, "x", 0 ) );
    check( "the first widget that can have the focus has it", ui.focus == 1 );
    check( "a widget is found", ui_find( &ui, 1 ) != 0 && ui_find( &ui, 99 ) == 0 );
    check( "the caption is kept as a copy", strcmp( ui_get_text( &ui, 1 ), "OK" ) == 0 && ui_get_text( &ui, 99 ) == 0 );
    char longtext[400];
    memset( longtext, 'x', sizeof(longtext) - 1 );
    longtext[ sizeof(longtext) - 1 ] = '\0';
    check( "a long text is cut to 255 characters", ui_add( &ui, UIK_TEXT, 3, 10, 200, 100, 16, longtext, 0 ) && strlen( ui_get_text( &ui, 3 ) ) == UI_TEXTMAX );
    check( "a remove works", ui_remove( &ui, 3 ) && ui_find( &ui, 3 ) == 0 && !ui_remove( &ui, 3 ) );
    done();

    setup();
    bool room = true;
    for ( int i = 1; i <= UI_MAXWIDGETS; ++i ) {
        room = room && ui_add( &ui, UIK_LABEL, i, 0, 0, 5, 5, "", 0 );
    }
    check( "the table takes UI_MAXWIDGETS widgets", room );
    check( "and refuses one more", !ui_add( &ui, UIK_LABEL, 1000, 0, 0, 5, 5, "", 0 ) );
    ui_clear( &ui );
    check( "clear empties it", ui.count == 0 && ui.focus == 0 && ui_add( &ui, UIK_LABEL, 1, 0, 0, 5, 5, "", 0 ) );
    done();

    // -- buttons and the focus ---------------------------------------------------------------------------------------------------------------------------
    setup();
    ui_add( &ui, UIK_LABEL, 10, 10, 5, 100, 14, "A form", 0 );
    ui_add( &ui, UIK_BUTTON, 1, 10, 30, 60, 22, "OK", 0 );
    ui_add( &ui, UIK_BUTTON, 2, 80, 30, 60, 22, "Cancel", 0 );
    ui_add( &ui, UIK_BUTTON, 3, 150, 30, 60, 22, "Help", 0 );
    check( "a label does not take the focus", ui.focus == 1 );
    click( 20, 40 );
    check( "a click on a button is an event", expect( 1, UIE_CLICK, 0 ) && none() );
    mouse( 20, 40, UIB_LEFT, 0 );
    mouse( 200, 100, 0, 0 );
    check( "press on a button, release elsewhere: no click", none() );
    click( 20, 100 );
    check( "a click on nothing is nothing", none() );
    click( 90, 40 );
    check( "a click gives the button the focus", expect( 2, UIE_CLICK, 0 ) && ui.focus == 2 );
    key( UIKEY_TAB );
    check( "Tab moves the focus on", ui.focus == 3 );
    key( UIKEY_TAB );
    check( "Tab goes round", ui.focus == 1 );
    key( UIKEY_BACKTAB );
    check( "Shift+Tab goes back", ui.focus == 3 );
    ui_set_enabled( &ui, 1, false );
    key( UIKEY_TAB );
    check( "Tab skips a disabled button", ui.focus == 2 );
    click( 20, 40 );
    check( "a disabled button does not click", none() );
    ui_set_visible( &ui, 3, false );
    key( UIKEY_TAB );
    check( "Tab skips a hidden button", ui.focus == 2 );
    ui_set_visible( &ui, 3, true );
    ui_set_enabled( &ui, 1, true );
    ui_set_focus( &ui, 3 );
    key( ' ' );
    check( "Space presses the button that has the focus", expect( 3, UIE_CLICK, 0 ) );
    key( UIKEY_ENTER );
    check( "Return does, too", expect( 3, UIE_CLICK, 0 ) );
    check( "a label cannot have the focus", !ui_set_focus( &ui, 10 ) && ui.focus == 3 );
    done();

    // -- check boxes and radio buttons -----------------------------------------------------------------------------------------------------------------
    setup();
    ui_add( &ui, UIK_CHECK, 1, 10, 10, 120, 16, "Sound", 0 );
    ui_add( &ui, UIK_RADIO, 2, 10, 40, 120, 16, "Low", 1 );
    ui_add( &ui, UIK_RADIO, 3, 10, 60, 120, 16, "Medium", 1 );
    ui_add( &ui, UIK_RADIO, 4, 10, 80, 120, 16, "High", 1 );
    ui_add( &ui, UIK_RADIO, 5, 10, 110, 120, 16, "Other group", 2 );
    int v;
    check( "a check box starts off", ui_get_value( &ui, 1, &v ) && v == 0 );
    click( 15, 18 );
    check( "a click switches a check box on", expect( 1, UIE_CHANGE, 1 ) && ui_get_value( &ui, 1, &v ) && v == 1 );
    click( 50, 18 );
    check( "a click on its text switches it off again", expect( 1, UIE_CHANGE, 0 ) );
    ui_set_focus( &ui, 1 );
    key( ' ' );
    check( "Space toggles it", expect( 1, UIE_CHANGE, 1 ) && none() );
    check( "set_value switches without an event", ui_set_value( &ui, 1, 0 ) && none() && ui_get_value( &ui, 1, &v ) && v == 0 );
    click( 14, 48 );
    check( "a click selects a radio button", expect( 2, UIE_CHANGE, 1 ) );
    click( 14, 68 );
    int a, b;
    ui_get_value( &ui, 2, &a );
    ui_get_value( &ui, 3, &b );
    check( "the others of the group go off", expect( 3, UIE_CHANGE, 1 ) && a == 0 && b == 1 );
    ui_set_value( &ui, 5, 1 );
    ui_get_value( &ui, 3, &b );
    check( "another group is independent", b == 1 && ui_get_value( &ui, 5, &a ) && a == 1 );
    click( 14, 68 );
    check( "a click on the selected radio button is no change", none() );
    ui_set_focus( &ui, 3 );
    key( UIKEY_DOWN );
    check( "Down selects the next radio button of the group", expect( 4, UIE_CHANGE, 1 ) && ui.focus == 4 );
    key( UIKEY_DOWN );
    check( "and goes round", expect( 2, UIE_CHANGE, 1 ) && ui.focus == 2 );
    key( UIKEY_UP );
    check( "Up goes back", expect( 4, UIE_CHANGE, 1 ) );
    ui_get_value( &ui, 5, &a );
    check( "the other group is still on", a == 1 );
    done();

    // -- text fields ---------------------------------------------------------------------------------------------------------------------------------------
    setup();
    ui_add( &ui, UIK_TEXT, 1, 10, 10, 100, 18, "", 0 );
    ui_add( &ui, UIK_BUTTON, 2, 10, 50, 60, 22, "OK", 0 );
    typed( "Hello" );
    check( "typing fills a text field", strcmp( ui_get_text( &ui, 1 ), "Hello" ) == 0 );
    drain();
    key( UIKEY_BACKSPACE );
    check( "Backspace", strcmp( ui_get_text( &ui, 1 ), "Hell" ) == 0 && expect( 1, UIE_CHANGE, 4 ) );
    key( UIKEY_LEFT );
    key( UIKEY_LEFT );
    typed( "X" );
    check( "the caret moves: insert in the middle", strcmp( ui_get_text( &ui, 1 ), "HeXll" ) == 0 );
    key( UIKEY_DELETE );
    check( "Delete", strcmp( ui_get_text( &ui, 1 ), "HeXl" ) == 0 );
    key( UIKEY_HOME );
    typed( ">" );
    key( UIKEY_END );
    typed( "<" );
    check( "Home and End", strcmp( ui_get_text( &ui, 1 ), ">HeXl<" ) == 0 );
    drain();
    key( UIKEY_ENTER );
    check( "Return in a text field is an event with the length", expect( 1, UIE_ENTER, 6 ) );
    key( UIKEY_HOME );
    key( UIKEY_BACKSPACE );
    key( UIKEY_LEFT );
    check( "Backspace at the start and Left at the start change nothing", strcmp( ui_get_text( &ui, 1 ), ">HeXl<" ) == 0 );
    drain();
    ui_set_text( &ui, 1, "abc" );
    click( 12, 18 );
    typed( "-" );
    check( "a click near the left edge puts the caret at the start", strcmp( ui_get_text( &ui, 1 ), "-abc" ) == 0 );
    click( 105, 18 );
    typed( "+" );
    check( "a click right of the text puts the caret at the end", strcmp( ui_get_text( &ui, 1 ), "-abc+" ) == 0 );
    char big[ 300 ];
    memset( big, 'a', sizeof(big) - 1 );
    big[ sizeof(big) - 1 ] = '\0';
    ui_set_text( &ui, 1, "" );
    for ( int i = 0; i < 300; ++i ) {
        key( 'a' );
    }
    check( "a text field takes 255 characters and no more", strlen( ui_get_text( &ui, 1 ) ) == UI_TEXTMAX );
    ui_widget_t* w = ui_find( &ui, 1 );
    check( "a long text scrolls so that the caret is shown", w->scroll > 0 );
    ui_set_enabled( &ui, 1, false );
    key( 'b' );
    check( "a disabled text field does not take keys", strlen( ui_get_text( &ui, 1 ) ) == UI_TEXTMAX );
    ui_set_enabled( &ui, 1, true );
    check( "a text field value is its length", ui_get_value( &ui, 1, &v ) && v == UI_TEXTMAX );
    done();

    // -- lists ---------------------------------------------------------------------------------------------------------------------------------------------
    setup();
    ui_add( &ui, UIK_LIST, 1, 10, 10, 120, 74, "", 0 );                 // (74 - 4) / 14 = 5 rows
    for ( int i = 0; i < 12; ++i ) {
        char s[16];
        snprintf( s, sizeof(s), "Item %d", i );
        ui_list_add( &ui, 1, s );
    }
    check( "a list has its items", strcmp( ui_list_get( &ui, 1, 3 ), "Item 3" ) == 0 && ui_list_get( &ui, 1, 12 ) == 0 );
    check( "nothing is selected at first", ui_get_value( &ui, 1, &v ) && v == -1 );
    click( 30, 10 + 2 + 14 * 2 + 5 );
    check( "a click selects the row", expect( 1, UIE_SELECT, 2 ) && ui_get_value( &ui, 1, &v ) && v == 2 );
    now += 100;
    click( 30, 10 + 2 + 14 * 2 + 5 );
    check( "a second click on the same row in time is a double click", expect( 1, UIE_DOUBLE, 2 ) && none() );
    now += 1000;
    click( 30, 10 + 2 + 14 * 2 + 5 );
    now += 1000;
    click( 30, 10 + 2 + 14 * 3 + 5 );
    check( "a click on another row selects it", expect( 1, UIE_SELECT, 3 ) && none() );
    key( UIKEY_DOWN );
    check( "Down selects the next row", expect( 1, UIE_SELECT, 4 ) );
    key( UIKEY_END );
    check( "End selects the last row and scrolls to it", expect( 1, UIE_SELECT, 11 ) && ui_find( &ui, 1 )->scroll == 7 );
    key( UIKEY_HOME );
    check( "Home selects the first row and scrolls back", expect( 1, UIE_SELECT, 0 ) && ui_find( &ui, 1 )->scroll == 0 );
    key( UIKEY_PGDN );
    check( "Page down moves by the rows shown (5)", expect( 1, UIE_SELECT, 5 ) );
    key( UIKEY_PGUP );
    key( UIKEY_UP );
    drain();
    key( UIKEY_ENTER );
    check( "Return activates the row (a double click)", expect( 1, UIE_DOUBLE, 0 ) );
    mouse( 30, 40, 0, 1 );
    check( "the wheel up at the top of the list changes nothing", ui_find( &ui, 1 )->scroll == 0 );
    mouse( 30, 40, 0, -1 );
    check( "the wheel down scrolls the list by 3 rows", ui_find( &ui, 1 )->scroll == 3 );
    mouse( 30, 40, 0, 1 );
    check( "the wheel up scrolls back", ui_find( &ui, 1 )->scroll == 0 );
    mouse( 30, 40, 0, -9 );
    check( "and stops at the end of the list", ui_find( &ui, 1 )->scroll == 7 );
    check( "set_value selects a row without an event and shows it", ui_set_value( &ui, 1, 1 ) && none() && ui_find( &ui, 1 )->scroll == 1 );
    check( "a row that does not exist selects none", ui_set_value( &ui, 1, 50 ) && ui_get_value( &ui, 1, &v ) && v == -1 );
    check( "list_clear", ui_list_clear( &ui, 1 ) && ui_list_get( &ui, 1, 0 ) == 0 );
    done();

    // -- sliders and progress bars ---------------------------------------------------------------------------------------------------------------------------
    setup();
    ui_add( &ui, UIK_SLIDER, 1, 10, 10, 110, 20, "", 0 );                // 0 .. 100, travel 100 pixels
    ui_add( &ui, UIK_PROGRESS, 2, 10, 50, 104, 14, "", 0 );
    check( "a slider starts at its minimum", ui_get_value( &ui, 1, &v ) && v == 0 );
    click( 10 + 5 + 50, 20 );
    check( "a click on the track sets the value", expect( 1, UIE_CHANGE, 50 ) );
    mouse( 15, 20, UIB_LEFT, 0 );
    drain();
    mouse( 15 + 25, 20, UIB_LEFT, 0 );
    mouse( 15 + 25, 20, 0, 0 );
    check( "dragging follows the mouse", ui_get_value( &ui, 1, &v ) && v == 25 );
    drain();
    ui_set_focus( &ui, 1 );
    key( UIKEY_RIGHT );
    check( "Right adds one", expect( 1, UIE_CHANGE, 26 ) );
    key( UIKEY_PGUP );
    check( "Page up adds a tenth of the range", expect( 1, UIE_CHANGE, 36 ) );
    key( UIKEY_END );
    check( "End goes to the maximum", expect( 1, UIE_CHANGE, 100 ) );
    key( UIKEY_RIGHT );
    check( "no event past the maximum", none() );
    check( "set_range changes and clamps", ui_set_range( &ui, 1, 10, 20 ) && ui_get_value( &ui, 1, &v ) && v == 20 );
    check( "a bad range is refused", !ui_set_range( &ui, 1, 5, 1 ) && !ui_set_range( &ui, 2, 5, 1 ) );
    check( "set_value on a slider clamps", ui_set_value( &ui, 1, 99 ) && ui_get_value( &ui, 1, &v ) && v == 20 && ui_set_value( &ui, 1, -4 ) && ui_get_value( &ui, 1, &v ) && v == 10 );
    check( "a progress bar can not have the focus or be clicked", !ui_set_focus( &ui, 2 ) );
    ui_set_value( &ui, 2, 50 );
    ui_draw( &ui, true );
    check( "a progress bar of 50 % is half filled", count_color( 12, 52, 12 + 51, 61, th.fill ) > 0 && gfx_point( &g, 12 + 40, 56 ) == th.fill && gfx_point( &g, 12 + 60, 56 ) == th.field );
    done();

    // -- a modal panel and a pop-up ------------------------------------------------------------------------------------------------------------------------
    setup();
    ui_add( &ui, UIK_BUTTON, 1, 10, 10, 60, 22, "Outside", 0 );
    ui_add( &ui, UIK_PANEL, 5, 100, 50, 200, 100, "Question", 0 );
    ui_add( &ui, UIK_BUTTON, 6, 110, 110, 60, 22, "Yes", 0 );
    ui_add( &ui, UIK_BUTTON, 7, 200, 110, 60, 22, "No", 0 );
    check( "a panel is a panel", ui_set_modal( &ui, 5 ) && ui.modal == 5 && !ui_set_modal( &ui, 1 ) && !ui_set_modal( &ui, 99 ) );
    check( "the focus goes into the panel", ui.focus == 6 );
    click( 20, 20 );
    check( "a widget outside of a modal panel does not react", none() );
    click( 120, 120 );
    check( "a widget inside does", expect( 6, UIE_CLICK, 0 ) );
    key( UIKEY_TAB );
    key( UIKEY_TAB );
    check( "Tab stays inside the panel", ui.focus == 6 );
    key( UIKEY_ESCAPE );
    check( "Escape in a modal panel is a cancel event for the panel", expect( 5, UIE_CANCEL, 0 ) );
    ui_set_modal( &ui, 0 );
    click( 20, 20 );
    check( "without the modal panel the outside button reacts again", expect( 1, UIE_CLICK, 0 ) );
    done();

    setup();
    ui_add( &ui, UIK_BUTTON, 1, 10, 10, 60, 22, "Menu", 0 );
    ui_add( &ui, UIK_LIST, 2, 0, 0, 80, 60, "", 0 );
    ui_list_add( &ui, 2, "New" );
    ui_list_add( &ui, 2, "Open" );
    ui_list_add( &ui, 2, "Quit" );
    ui_set_visible( &ui, 2, false );
    ui_draw( &ui, true );
    click( 20, 20 );
    drain();
    check( "a pop-up opens", ui_popup( &ui, 2, 10, 34 ) && ui.popup == 2 && ( ui_find( &ui, 2 )->flags & UIF_VISIBLE ) );
    click( 20, 20 );
    check( "a click outside closes it with a cancel", expect( 2, UIE_CANCEL, 0 ) && ui.popup == 0 && !( ui_find( &ui, 2 )->flags & UIF_VISIBLE ) && none() );
    ui_popup( &ui, 2, 10, 34 );
    click( 30, 34 + 2 + 14 + 3 );
    check( "a click on a row chooses it and closes the pop-up", expect( 2, UIE_SELECT, 1 ) && ui.popup == 0 && none() );
    ui_popup( &ui, 2, 10, 34 );
    key( UIKEY_DOWN );
    key( UIKEY_DOWN );
    drain();
    key( UIKEY_ENTER );
    check( "keys work in the pop-up (it starts at the row chosen last: row 1): Return chooses", expect( 2, UIE_SELECT, 2 ) && ui.popup == 0 );
    ui_popup( &ui, 2, 10, 34 );
    key( UIKEY_ESCAPE );
    check( "Escape closes the pop-up with a cancel", expect( 2, UIE_CANCEL, 0 ) && ui.popup == 0 );
    check( "the focus is back where it was", ui.focus == 1 );
    done();

    // -- drawing -------------------------------------------------------------------------------------------------------------------------------------------
    setup();
    ui_add( &ui, UIK_PANEL, 1, 20, 20, 200, 120, "Title", 0 );
    ui_add( &ui, UIK_LABEL, 2, 30, 50, 100, 14, "Name:", 0 );
    ui_add( &ui, UIK_BUTTON, 3, 30, 80, 70, 24, "OK", 0 );
    ui_add( &ui, UIK_CHECK, 4, 120, 80, 90, 16, "Fast", 0 );
    ui_draw( &ui, true );
    check( "the desk has the desk colour", gfx_point( &g, 5, 5 ) == th.desk && gfx_point( &g, 395, 235 ) == th.desk );
    check( "a panel is drawn with a border and the face colour", gfx_point( &g, 20, 20 ) == th.border && gfx_point( &g, 100, 130 ) == th.face && gfx_point( &g, 21, 21 ) == th.light );
    check( "the title bar has the title colour and the title text", gfx_point( &g, 40, 40 ) == th.title || gfx_point( &g, 24, 24 ) == th.title );
    check( "the title text is in the title text colour", count_color( 24, 23, 214, 38, th.titletext ) > 10 );
    check( "a label draws ink in the text colour on the face", count_color( 30, 50, 129, 63, th.text ) > 10 );
    check( "a button has a raised bevel: light top left", gfx_point( &g, 31, 81 ) == th.light && gfx_point( &g, 98, 102 ) == th.shadow );
    check( "a button has a face and a caption", gfx_point( &g, 36, 86 ) == th.face && count_color( 31, 82, 98, 101, th.text ) > 10 );
    check( "a check box is a white box with a border", gfx_point( &g, 120, 81 ) == th.border && gfx_point( &g, 124, 85 ) == th.field );
    ui_set_value( &ui, 4, 1 );
    ui_draw( &ui, false );
    check( "a check mark is drawn when it is on", count_color( 121, 82, 131, 92, th.text ) > 4 );
    mouse( 40, 90, UIB_LEFT, 0 );
    ui_draw( &ui, false );
    check( "a pressed button is sunken: shadow top left", gfx_point( &g, 31, 81 ) == th.shadow && gfx_point( &g, 98, 102 ) == th.light );
    mouse( 40, 90, 0, 0 );
    drain();
    ui_draw( &ui, false );
    check( "released again: raised", gfx_point( &g, 31, 81 ) == th.light );

    // changed parts only
    int ink0 = count_color( 30, 50, 129, 63, th.text );
    ui_set_text( &ui, 2, "Name: Hello" );
    ui_draw( &ui, false );
    int ink1 = count_color( 30, 50, 129, 63, th.text );
    check( "a changed label is drawn again with more ink", ink1 > ink0 );
    ui_set_text( &ui, 2, "Name:" );
    ui_draw( &ui, false );
    check( "and the old text is gone when it is set back", count_color( 30, 50, 129, 63, th.text ) == ink0 );
    gfx_pset( &g, 220, 200, 77 );
    ui_set_text( &ui, 2, "x" );
    ui_draw( &ui, false );
    check( "a partial redraw leaves the rest of the canvas alone", gfx_point( &g, 220, 200 ) == 77 );
    ui_set_visible( &ui, 3, false );
    ui_draw( &ui, false );
    check( "a hidden widget leaves the face of the panel", gfx_point( &g, 33, 83 ) == th.face && count_color( 30, 80, 99, 103, th.text ) == 0 && gfx_point( &g, 31, 81 ) == th.face );
    ui_set_visible( &ui, 3, true );
    ui_draw( &ui, false );
    check( "shown again", gfx_point( &g, 31, 81 ) == th.light );
    ui_remove( &ui, 3 );
    ui_draw( &ui, false );
    check( "a removed widget is erased by the next ui_draw", gfx_point( &g, 31, 81 ) == th.face );
    ui_set_enabled( &ui, 4, false );
    ui_draw( &ui, false );
    check( "a disabled widget is drawn in the disabled colour", count_color( 121, 82, 131, 92, th.disabled ) > 4 );
    // a button outside any panel has the desk behind it when it is hidden
    ui_add( &ui, UIK_BUTTON, 9, 300, 150, 60, 22, "Free", 0 );
    ui_draw( &ui, false );
    ui_set_visible( &ui, 9, false );
    ui_draw( &ui, false );
    check( "outside of a panel the background is the desk", gfx_point( &g, 330, 160 ) == th.desk );
    done();

    // -- scroll bars ---------------------------------------------------------------------------------------------------------------------------------
    setup();
    ui_add( &ui, UIK_SCROLL, 1, 10, 10, 15, 120, "", 10 );              // vertical: arrows 15 + 15, track 90, positions 0..100, page 10
    check( "a scroll bar starts at 0", ui_get_value( &ui, 1, &v ) && v == 0 );
    check( "a scroll bar can have the focus", ui.focus == 1 );
    check( "a page below 1 is refused", !ui_set_page( &ui, 1, 0 ) && ui_set_page( &ui, 1, 10 ) );
    ui_draw( &ui, true );
    check( "the knob is a tenth of... page / (range + page) of the track (90 * 10 / 110 = 8, at least 10)", gfx_point( &g, 17, 25 + 4 ) == th.face && gfx_point( &g, 17, 25 + 9 ) == th.border );
    check( "the arrow buttons and the knob are raised", gfx_point( &g, 11, 11 ) == th.light && gfx_point( &g, 11, 26 ) == th.light );
    click( 17, 14 );
    check( "the down arrow moves by one (the up arrow at the top does nothing)", none() );
    click( 17, 130 - 5 );
    check( "the lower arrow adds one", expect( 1, UIE_CHANGE, 1 ) && none() );
    now += 1000;
    click( 17, 100 );
    check( "a click in the track behind the knob moves by a page", expect( 1, UIE_CHANGE, 11 ) );
    now += 1000;
    click( 17, 12 + 15 );
    check( "a click in the track before the knob moves back by a page", expect( 1, UIE_CHANGE, 1 ) );
    // hold the lower arrow: repeats after 400 ms, then every 60 ms
    now += 1000;
    mouse( 17, 125, UIB_LEFT, 0 );
    drain();
    now += 100;
    mouse( 17, 125, UIB_LEFT, 0 );
    check( "holding an arrow does not repeat at once", none() );
    now += 400;
    mouse( 17, 125, UIB_LEFT, 0 );
    check( "but after 400 ms", expect( 1, UIE_CHANGE, 3 ) );
    now += 70;
    mouse( 17, 125, UIB_LEFT, 0 );
    check( "then every 60 ms", expect( 1, UIE_CHANGE, 4 ) );
    mouse( 17, 125, 0, 0 );
    drain();
    // drag the knob
    now += 1000;
    ui_set_value( &ui, 1, 0 );
    ui_draw( &ui, false );
    mouse( 17, 28, UIB_LEFT, 0 );                                       // on the knob (it starts at y = 25)
    mouse( 17, 28 + 40, UIB_LEFT, 0 );
    mouse( 17, 28 + 40, 0, 0 );
    ui_get_value( &ui, 1, &v );
    check( "dragging the knob follows the mouse", v > 30 && v < 70 );
    drain();
    ui_set_value( &ui, 1, 0 );
    mouse( 17, 28, UIB_LEFT, 0 );
    mouse( 17, 28 + 500, UIB_LEFT, 0 );
    mouse( 17, 28 + 500, 0, 0 );
    check( "dragging past the end gives the maximum", ui_get_value( &ui, 1, &v ) && v == 100 );
    drain();
    key( UIKEY_HOME );
    check( "Home", expect( 1, UIE_CHANGE, 0 ) );
    key( UIKEY_PGDN );
    check( "Page down adds the page", expect( 1, UIE_CHANGE, 10 ) );
    key( UIKEY_DOWN );
    check( "Down adds one", expect( 1, UIE_CHANGE, 11 ) );
    key( UIKEY_END );
    check( "End", expect( 1, UIE_CHANGE, 100 ) );
    key( UIKEY_DOWN );
    check( "no event past the end", none() );
    mouse( 17, 60, 0, 1 );
    check( "the wheel scrolls it by 3 (up is back)", expect( 1, UIE_CHANGE, 97 ) );
    check( "the range can change and clamps", ui_set_range( &ui, 1, 0, 20 ) && ui_get_value( &ui, 1, &v ) && v == 20 );
    done();

    setup();
    ui_add( &ui, UIK_SCROLL, 1, 10, 10, 120, 15, "", 50 );              // horizontal, page 50 of 0..100: the knob is a third of the track
    ui_draw( &ui, true );
    check( "a horizontal scroll bar is drawn: a knob after the left arrow", gfx_point( &g, 25 + 4, 17 ) == th.face && gfx_point( &g, 25 + 20, 17 ) == th.face );
    click( 125, 17 );
    check( "its right arrow adds one", expect( 1, UIE_CHANGE, 1 ) );
    done();

    // the bar of a list
    setup();
    ui_add( &ui, UIK_LIST, 1, 10, 10, 120, 74, "", 0 );                  // 5 rows
    for ( int i = 0; i < 12; ++i ) {
        ui_list_add( &ui, 1, "row" );
    }
    ui_draw( &ui, true );
    check( "a list with more rows than it shows has a bar at the right", gfx_point( &g, 10 + 120 - 3 - 7, 10 + 2 + 20 ) != th.field && count_color( 10 + 120 - 2 - 15, 12, 10 + 120 - 3, 80, th.border ) > 20 );
    click( 10 + 120 - 3 - 7, 10 + 74 - 6 );
    check( "its lower arrow scrolls the list by one row without a selection", none() && ui_find( &ui, 1 )->scroll == 1 && ui_find( &ui, 1 )->value == -1 );
    click( 10 + 120 - 3 - 7, 10 + 74 - 25 );
    check( "a click in its track scrolls a page", ui_find( &ui, 1 )->scroll == 6 && none() );
    click( 30, 12 + 3 );
    check( "a click left of the bar still selects a row", expect( 1, UIE_SELECT, 6 ) );
    ui_list_clear( &ui, 1 );
    ui_list_add( &ui, 1, "one" );
    ui_draw( &ui, false );
    check( "a list that shows all its rows has no bar", gfx_point( &g, 10 + 120 - 3 - 7, 10 + 2 + 20 ) == th.field );
    done();

    // a hidden widget that overlaps a panel must not erase the panel when the panel is drawn after it was hidden
    setup();
    ui_draw( &ui, true );
    ui_add( &ui, UIK_PANEL, 1, 20, 15, 200, 100, "P", 0 );
    ui_add( &ui, UIK_LIST, 2, 0, 0, 90, 66, "", 0 );
    ui_set_visible( &ui, 2, false );
    ui_draw( &ui, false );
    check( "a hidden widget over a panel leaves the panel intact", gfx_point( &g, 50, 50 ) == th.face && gfx_point( &g, 5, 5 ) == th.desk );
    done();

    // a radio button and a list are drawn
    setup();
    ui_add( &ui, UIK_RADIO, 1, 10, 10, 100, 16, "One", 1 );
    ui_add( &ui, UIK_LIST, 2, 10, 40, 100, 60, "", 0 );
    ui_list_add( &ui, 2, "Alpha" );
    ui_list_add( &ui, 2, "Beta" );
    ui_set_value( &ui, 2, 1 );
    ui_set_value( &ui, 1, 1 );
    ui_draw( &ui, true );
    check( "a selected radio button has a dot", gfx_point( &g, 16, 18 ) == th.text && gfx_point( &g, 12, 18 ) == th.field );
    check( "the selected list row is highlighted", gfx_point( &g, 14, 40 + 2 + 14 + 6 ) == th.hilite || count_color( 12, 56, 108, 70, th.hilite ) > 100 );
    check( "its text is in the highlight text colour", count_color( 12, 56, 108, 70, th.hilitetext ) > 8 );
    done();

    // the event queue does not overflow
    setup();
    ui_add( &ui, UIK_BUTTON, 1, 10, 10, 60, 22, "OK", 0 );
    for ( int i = 0; i < 200; ++i ) {
        key( ' ' );
    }
    check( "the queue holds UI_EVENTS events and drops the rest", nevents() == UI_EVENTS );
    drain();
    check( "and is empty afterwards", none() );
    done();

    // the counter that ON UI watches
    setup();
    ui_add( &ui, UIK_BUTTON, 1, 10, 10, 60, 22, "OK", 0 );
    uint32_t c0 = ui_event_count();
    key( ' ' );
    check( "ui_event_count counts a queued event", ui_event_count() == c0 + 1U );
    drain();
    check( "reading an event does not change the count", ui_event_count() == c0 + 1U );
    done();

    // arrows move the focus to the nearest widget in that direction
    setup();
    ui_add( &ui, UIK_BUTTON, 1, 10, 10, 60, 22, "A", 0 );
    ui_add( &ui, UIK_BUTTON, 2, 100, 10, 60, 22, "B", 0 );
    ui_add( &ui, UIK_BUTTON, 3, 10, 80, 60, 22, "C", 0 );
    ui_add( &ui, UIK_CHECK, 4, 100, 80, 80, 16, "D", 0 );
    check( "the first button has the focus", ui.focus == 1 );
    key( UIKEY_RIGHT );
    check( "right: the button on the right", ui.focus == 2 );
    key( UIKEY_DOWN );
    check( "down: the box below", ui.focus == 4 );
    key( UIKEY_LEFT );
    check( "left: the button on the left", ui.focus == 3 );
    key( UIKEY_UP );
    check( "up: the button above", ui.focus == 1 );
    key( UIKEY_UP );
    check( "up at the top stays", ui.focus == 1 );
    drain();
    done();

    // a game controller as keys
    setup();
    ui_add( &ui, UIK_BUTTON, 1, 10, 10, 60, 22, "A", 0 );
    ui_add( &ui, UIK_LIST, 2, 100, 10, 80, 60, "", 0 );
    int pk[16];
    int np = ui_pad_keys( &ui, 0, 0.0, 0.0, 1000, pk, 16 );
    check( "no buttons, no keys", np == 0 );
    np = ui_pad_keys( &ui, JOYB_A, 0.0, 0.0, 1000, pk, 16 );
    check( "A on a button is Space", np == 1 && pk[0] == ' ' );
    np = ui_pad_keys( &ui, JOYB_A, 0.0, 0.0, 1010, pk, 16 );
    check( "a held A does not repeat", np == 0 );
    np = ui_pad_keys( &ui, JOYB_RIGHT, 0.0, 0.0, 1100, pk, 16 );
    check( "the D-pad is an arrow key", np == 1 && pk[0] == UIKEY_RIGHT );
    np = ui_pad_keys( &ui, JOYB_RIGHT, 0.0, 0.0, 1300, pk, 16 );
    check( "no repeat before 400 ms", np == 0 );
    np = ui_pad_keys( &ui, JOYB_RIGHT, 0.0, 0.0, 1520, pk, 16 );
    check( "repeat after 400 ms", np == 1 && pk[0] == UIKEY_RIGHT );
    np = ui_pad_keys( &ui, JOYB_RIGHT, 0.0, 0.0, 1560, pk, 16 );
    check( "then every 60 ms", np == 0 );
    np = ui_pad_keys( &ui, JOYB_RIGHT, 0.0, 0.0, 1585, pk, 16 );
    check( "the second repeat", np == 1 );
    np = ui_pad_keys( &ui, 0, 0.0, -0.9, 1700, pk, 16 );
    check( "the left stick up is the Up key", np == 1 && pk[0] == UIKEY_UP );
    np = ui_pad_keys( &ui, 0, 0.3, 0.3, 1800, pk, 16 );
    check( "a small stick move is nothing", np == 0 );
    np = ui_pad_keys( &ui, JOYB_B | JOYB_LB | JOYB_RB | JOYB_START, 0.0, 0.0, 1900, pk, 16 );
    check( "B, LB, RB, Start", np == 4 && pk[0] == UIKEY_ENTER && pk[1] == UIKEY_ESCAPE && pk[2] == UIKEY_BACKTAB && pk[3] == UIKEY_TAB );
    ui_pad_keys( &ui, 0, 0.0, 0.0, 2000, pk, 16 );
    ui_set_focus( &ui, 2 );
    np = ui_pad_keys( &ui, JOYB_A, 0.0, 0.0, 2100, pk, 16 );
    check( "A in a list is Return", np == 1 && pk[0] == UIKEY_ENTER );
    done();

    // windows: raising, moving, ownership
    setup();
    ui_add( &ui, UIK_PANEL, 10, 10, 10, 150, 100, "One", 1 );
    ui_add( &ui, UIK_BUTTON, 11, 20, 40, 60, 22, "B1", 0 );
    ui_add( &ui, UIK_PANEL, 20, 60, 50, 150, 100, "Two", 1 );
    ui_add( &ui, UIK_BUTTON, 21, 75, 80, 60, 22, "B2", 0 );
    ui_add( &ui, UIK_BUTTON, 30, 300, 10, 60, 22, "Free", 0 );
    check( "a widget created inside a window is owned by it", ui_find( &ui, 11 )->owner == 10 && ui_find( &ui, 21 )->owner == 20 && ui_find( &ui, 30 )->owner == 0 );
    check( "the window is a window", ( ui_find( &ui, 10 )->flags & UIF_WINDOW ) != 0 );
    ui_draw( &ui, true );
    // window Two is in front: a press in the overlap (80, 60) hits Two, a press at (30, 30) hits One's body and raises One
    drain();
    click( 30, 60 );
    check( "a press in a window raises it: its panel is behind its widgets, after the others", ui.widgets[ ui.count - 2 ].id == 10 && ui.widgets[ ui.count - 1 ].id == 11 );
    check( "the other window is now behind", ui.widgets[0].id == 20 || ui.widgets[1].id == 20 );
    click( 30, 51 );
    check( "a click on the widget of the raised window", expect( 11, UIE_CLICK, 0 ) );
    drain();
    // the close box: x2 - 17 .. x2 - 5, y + 5 .. y + 17 of the window One (10, 10, 150 wide)
    click( 10 + 150 - 10, 10 + 11 );
    check( "the close box asks for cancel", expect( 10, UIE_CANCEL, 0 ) );
    // dragging by the title bar: press at (50, 15), move by (100, 50), release
    mouse( 50, 15, UIB_LEFT, 0 );
    mouse( 90, 45, UIB_LEFT, 0 );
    mouse( 150, 65, UIB_LEFT, 0 );
    mouse( 150, 65, 0, 0 );
    check( "the window moved with the mouse", ui_find( &ui, 10 )->x == 110 && ui_find( &ui, 10 )->y == 60 );
    check( "its widgets moved with it", ui_find( &ui, 11 )->x == 120 && ui_find( &ui, 11 )->y == 90 );
    check( "the other window did not", ui_find( &ui, 20 )->x == 60 && ui_find( &ui, 21 )->x == 75 );
    ui_draw( &ui, false );
    check( "the old place shows the desk again", gfx_point( &g, 12, 12 ) == ui.theme.desk );
    check( "the new place shows the window (its title bar)", gfx_point( &g, 150, 64 ) == ui.theme.title );
    check( "ui_move moves a window and its widgets", ui_move( &ui, 10, 10, 10 ) && ui_find( &ui, 11 )->x == 20 && ui_find( &ui, 11 )->y == 40 );
    check( "ui_move of an unknown id fails", !ui_move( &ui, 99, 0, 0 ) );
    check( "ui_raise brings Two to the front", ui_raise( &ui, 20 ) && ui.widgets[ ui.count - 1 ].id == 21 );
    mouse( 30, 15, UIB_LEFT, 0 );
    mouse( 5000, 5000, UIB_LEFT, 0 );
    mouse( 5000, 5000, 0, 0 );
    check( "a window cannot be dragged out of the canvas", ui_find( &ui, 10 )->x == 400 - 40 && ui_find( &ui, 10 )->y == 240 - 20 );
    drain();
    done();

    // damage lists: the part of a window that another window hides is not drawn, and is drawn when it is revealed
    setup();
    ui_add( &ui, UIK_PANEL, 10, 10, 10, 150, 100, "One", 1 );
    ui_add( &ui, UIK_LABEL, 11, 20, 40, 100, 14, "first", 0 );
    ui_add( &ui, UIK_PANEL, 20, 90, 50, 150, 100, "Two", 1 );
    ui_add( &ui, UIK_BUTTON, 21, 110, 80, 60, 22, "B2", 0 );
    painted = 0;
    ui_draw( &ui, true );
    long fullpaint = painted;
    check( "the window in front owns the overlap", gfx_point( &g, 100, 70 ) == ui.theme.face && gfx_point( &g, 95, 55 ) == ui.theme.title );
    check( "window One has the overlap on its damage list", ui_find( &ui, 10 )->dmg.n > 0 );
    // a change in the hidden part of One must not show through Two
    ui_set_text( &ui, 10, "One!" );
    ui_set_text( &ui, 11, "second" );
    int before = gfx_point( &g, 100, 70 );
    ui_draw( &ui, false );
    check( "changing a window under another leaves the one in front alone", gfx_point( &g, 100, 70 ) == before && gfx_point( &g, 95, 55 ) == ui.theme.title );
    check( "its obscured part is still on its damage list", ui_find( &ui, 10 )->dmg.n > 0 );
    // moving Two away reveals One: the revealed part is drawn again
    ui_move( &ui, 20, 300, 50 );
    painted = 0;
    ui_draw( &ui, false );
    check( "the revealed part of One is drawn (its face)", gfx_point( &g, 100, 70 ) == ui.theme.face && gfx_point( &g, 150, 100 ) == ui.theme.face );
    check( "and the desk where Two was", gfx_point( &g, 200, 150 ) == ui.theme.desk );
    check( "a window moved elsewhere is drawn there", gfx_point( &g, 305, 55 ) == ui.theme.shadow || gfx_point( &g, 305, 55 ) == ui.theme.title );
    check( "a redraw touches only the changed parts (less than the full draw)", painted < fullpaint );
    check( "the damage list of One is empty now", ui_find( &ui, 10 )->dmg.n == 0 );
    // raising One over Two: Two's part that One covers goes on Two's list
    ui_move( &ui, 20, 90, 50 );
    ui_draw( &ui, false );
    check( "Two is in front again, intact", gfx_point( &g, 95, 55 ) == ui.theme.title && gfx_point( &g, 100, 70 ) == ui.theme.face );
    ui_raise( &ui, 10 );
    ui_draw( &ui, false );
    check( "raising One shows it over Two (its title bar)", gfx_point( &g, 100, 15 ) != ui.theme.desk && gfx_point( &g, 95, 55 ) != ui.theme.title );
    check( "Two keeps the covered part on its damage list", ui_find( &ui, 20 )->dmg.n > 0 );
    ui_raise( &ui, 20 );
    ui_draw( &ui, false );
    check( "raising Two again redraws the covered part", gfx_point( &g, 95, 55 ) == ui.theme.title && ui_find( &ui, 20 )->dmg.n == 0 );
    // a window hidden / removed gives its place back
    ui_remove( &ui, 21 );
    ui_remove( &ui, 20 );
    ui_draw( &ui, false );
    check( "a removed window shows the one under it", gfx_point( &g, 100, 70 ) == ui.theme.face && gfx_point( &g, 200, 150 ) == ui.theme.desk );
    done();

    // the incremental redraw (damage lists) must give the picture of a full redraw: a modal panel over a form, added and removed
    setup();
    ui_add( &ui, UIK_PANEL, 1, 10, 10, 300, 200, "Form", 0 );
    ui_add( &ui, UIK_LABEL, 2, 20, 40, 60, 14, "Name", 0 );
    ui_add( &ui, UIK_TEXT, 3, 90, 36, 150, 18, "abc", 0 );
    ui_add( &ui, UIK_BUTTON, 4, 20, 150, 80, 22, "OK", 0 );
    ui_add( &ui, UIK_LIST, 5, 20, 70, 120, 60, "", 0 );
    ui_draw( &ui, true );
    ui_add( &ui, UIK_PANEL, 20, 60, 60, 200, 100, "Dialog", 0 );
    ui_add( &ui, UIK_LABEL, 23, 70, 90, 150, 14, "Sure?", 0 );
    ui_add( &ui, UIK_BUTTON, 21, 70, 120, 60, 22, "Yes", 0 );
    ui_set_modal( &ui, 20 );
    ui_draw( &ui, false );
    ui_set_modal( &ui, 0 );
    ui_remove( &ui, 21 );
    ui_remove( &ui, 23 );
    ui_remove( &ui, 20 );
    ui_draw( &ui, false );
    {
        static uint8_t keep[ 400 * 240 ];
        memcpy( keep, buf, sizeof(keep) );
        ui_draw( &ui, true );
        int diff = 0;
        for ( int i = 0; i < 400 * 240; ++i ) {
            diff += keep[i] != buf[i];
        }
        check( "after a modal panel came and went, the picture is the full redraw", diff == 0 );
    }
    done();

    // random windows: after any run of moves, raises, changes, hides and removals the incremental redraw gives the picture of a full redraw
    {
        setup();
        static uint8_t keep[ 400 * 240 ];
        int wid[4] = { 100, 200, 300, 400 };
        bool present[4] = { false, false, false, false };
        int bad = 0, steps = 0;
        for ( int step = 0; step < 400 && bad == 0; ++step ) {
            for ( int op = 0; op < 3; ++op ) {
                int k = RND( 4 ), w = wid[k], what = RND( 7 );
                if ( !present[k] ) {
                    int x = RND( 250 ), y = RND( 120 );
                    ui_add( &ui, UIK_PANEL, w, x, y, 140, 90, "Win", 1 );
                    ui_add( &ui, UIK_LABEL, w + 1, x + 10, y + 30, 80, 14, "label", 0 );
                    ui_add( &ui, UIK_BUTTON, w + 2, x + 10, y + 50, 60, 22, "Btn", 0 );
                    ui_add( &ui, UIK_CHECK, w + 3, x + 80, y + 50, 50, 16, "c", 0 );
                    present[k] = true;
                } else if ( what == 0 || what == 1 ) {
                    ui_move( &ui, w, RND( 300 ) - 20, RND( 200 ) );
                } else if ( what == 2 ) {
                    ui_raise( &ui, w );
                } else if ( what == 3 ) {
                    ui_set_text( &ui, w + 1, ( rnd_state & 1U ) ? "a longer label" : "x" );
                } else if ( what == 4 ) {
                    ui_set_visible( &ui, w + 2, ( rnd_state & 2U ) != 0 );
                } else if ( what == 5 ) {
                    ui_set_value( &ui, w + 3, (int)( rnd_state & 1U ) );
                } else {
                    ui_remove( &ui, w + 3 );
                    ui_remove( &ui, w + 2 );
                    ui_remove( &ui, w + 1 );
                    ui_remove( &ui, w );
                    present[k] = false;
                }
            }
            ui_draw( &ui, false );
            memcpy( keep, buf, sizeof(keep) );
            ui_draw( &ui, true );
            ++steps;
            for ( int i = 0; i < 400 * 240; ++i ) {
                if ( keep[i] != buf[i] ) {
                    ++bad;
                }
            }
            if ( bad ) {
                printf( "random windows: step %d differs in %d pixels\n", step, bad );
            }
        }
        check( "400 random steps of windows: every incremental redraw equals a full redraw", bad == 0 && steps == 400 );
        done();
    }

    // a canvas that is removed and made again with other cells: the redraw is complete (the sprite editor does it when its size changes)
    {
        setup();
        static uint8_t keep2[ 400 * 240 ];
        ui_add( &ui, UIK_PANEL, 99, 0, 0, 400, 240, "T", 0 );
        ui_add( &ui, UIK_CANVAS, 100, 10, 24, 120, 180, "", 0 );
        ui_set_canvas( &ui, 100, 16, 24 );
        ui_add( &ui, UIK_LABEL, 112, 10, 210, 300, 14, "status", 0 );
        ui_add( &ui, UIK_SLIDER, 126, 150, 40, 120, 18, "", 0 );
        ui_add( &ui, UIK_BUTTON, 114, 150, 80, 60, 22, "Clear", 0 );
        ui_add( &ui, UIK_BUTTON, 115, 220, 80, 60, 22, "Flip", 0 );
        ui_draw( &ui, false );
        ui_remove( &ui, 100 );
        ui_add( &ui, UIK_CANVAS, 100, 10, 24, 120, 180, "", 0 );
        ui_set_canvas( &ui, 100, 64, 24 );
        ui_set_text( &ui, 112, "other" );
        ui_draw( &ui, false );
        memcpy( keep2, buf, sizeof(keep2) );
        ui_draw( &ui, true );
        int bad = 0;
        for ( int i = 0; i < 400 * 240; ++i ) {
            if ( keep2[i] != buf[i] ) {
                ++bad;
            }
        }
        check( "a canvas removed and made again with other cells: the incremental redraw equals a full redraw", bad == 0 );
        done();
    }

    // canvas: a zoomed grid of cells
    setup();
    check( "a canvas is added", ui_add( &ui, UIK_CANVAS, 7, 10, 10, 100, 100, "", 0 ) );
    check( "a canvas without cells has no pixel", ui_canvas_get( &ui, 7, 0, 0 ) == -1 );
    check( "the cells are set", ui_set_canvas( &ui, 7, 8, 6 ) && ui_canvas_get( &ui, 7, 0, 0 ) == 0 );
    check( "bad cell counts are refused", !ui_set_canvas( &ui, 7, 0, 6 ) && !ui_set_canvas( &ui, 7, 8, 300 ) && !ui_set_canvas( &ui, 99, 8, 6 ) );
    check( "a pixel is set and read", ui_canvas_set( &ui, 7, 3, 2, 42 ) && ui_canvas_get( &ui, 7, 3, 2 ) == 42 );
    check( "a pixel out of the grid is refused", !ui_canvas_set( &ui, 7, 8, 0, 1 ) && !ui_canvas_set( &ui, 7, 0, 6, 1 ) && !ui_canvas_set( &ui, 7, 0, 0, 256 ) &&
        ui_canvas_get( &ui, 7, 0, 6 ) == -1 );
    ui_draw( &ui, true );
    // 100 - 4 = 96 pixels: a cell is 12 pixels (8 cells), the grid is 96 x 72 pixels, centered: x from 12, y from 10 + 2 + 12 = 24
    check( "the cell is drawn in its color", gfx_point( &g, 12 + 3 * 12 + 3, 24 + 2 * 12 + 3 ) == 42 );
    check( "an empty cell is checkered", gfx_point( &g, 12 + 3, 24 + 3 ) != 0 && gfx_point( &g, 12 + 3, 24 + 3 ) != gfx_point( &g, 12 + 12 + 3, 24 + 3 ) );
    drain();
    mouse( 12 + 5 * 12 + 4, 24 + 1 * 12 + 4, UIB_LEFT, 0 );
    check( "a left press reports the cell", expect( 7, UIE_PIXEL, 1 * 8 + 5 ) );
    mouse( 12 + 5 * 12 + 6, 24 + 1 * 12 + 6, UIB_LEFT, 0 );
    check( "moving inside the cell reports nothing", none() );
    mouse( 12 + 6 * 12 + 4, 24 + 1 * 12 + 4, UIB_LEFT, 0 );
    check( "dragging to the next cell reports it", expect( 7, UIE_PIXEL, 1 * 8 + 6 ) );
    mouse( 12 + 6 * 12 + 4, 24 + 1 * 12 + 4, 0, 0 );
    mouse( 12 + 2 * 12 + 4, 24 + 4 * 12 + 4, UIB_RIGHT, 0 );
    check( "a right press reports with the other kind", expect( 7, UIE_PIXEL2, 4 * 8 + 2 ) );
    mouse( 0, 0, 0, 0 );
    mouse( 11, 17, UIB_LEFT, 0 );
    check( "a press next to the grid (inside the widget) reports nothing", none() );
    mouse( 0, 0, 0, 0 );
    uint8_t img[ 48 ], back[ 64 ];
    for ( int i = 0; i < 48; ++i ) {
        img[i] = (uint8_t)( i + 1 );
    }
    size_t nb = 0;
    check( "an image is loaded", ui_canvas_load( &ui, 7, img, 48 ) && ui_canvas_get( &ui, 7, 7, 5 ) == 48 && ui_canvas_get( &ui, 7, 0, 0 ) == 1 );
    check( "a short image clears the rest", ui_canvas_load( &ui, 7, img, 10 ) && ui_canvas_get( &ui, 7, 2, 1 ) == 0 && ui_canvas_get( &ui, 7, 1, 0 ) == 2 );
    check( "the image is read back", ui_canvas_save( &ui, 7, back, sizeof(back), &nb ) && nb == 48U && memcmp( back, img, 10 ) == 0 && back[10] == 0 );
    check( "removing the canvas frees it", ui_remove( &ui, 7 ) );
    done();

    // a tab strip: the value is the selected tab, a click on a tab and Left / Right change it
    setup();
    ui_add( &ui, UIK_TAB, 1, 10, 10, 300, 22, "One|Two|Three", 0 );
    ui_add( &ui, UIK_BUTTON, 2, 10, 100, 50, 20, "Btn", 0 );
    int tv = -1;
    check( "a tab strip starts at tab 0", ui_get_value( &ui, 1, &tv ) && tv == 0 );
    click( 14, 20 );
    check( "a click on the selected tab is no change", none() );
    ui_set_focus( &ui, 1 );
    key( UIKEY_RIGHT );
    check( "Right selects the next tab", expect( 1, UIE_CHANGE, 1 ) && ui_get_value( &ui, 1, &tv ) && tv == 1 );
    key( UIKEY_RIGHT );
    key( UIKEY_RIGHT );
    check( "and stops at the last", expect( 1, UIE_CHANGE, 2 ) && none() && ui_get_value( &ui, 1, &tv ) && tv == 2 );
    key( UIKEY_LEFT );
    key( UIKEY_LEFT );
    key( UIKEY_LEFT );
    check( "Left goes back and stops at the first", expect( 1, UIE_CHANGE, 1 ) && expect( 1, UIE_CHANGE, 0 ) && none() );
    int hit2 = -1;
    for ( int x = 12; x < 300 && hit2 < 0; x += 2 ) {
        click( x, 20 );
        ui_get_value( &ui, 1, &tv );
        if ( tv == 2 ) {
            hit2 = x;
        }
        while ( ui.qcount > 0 ) {
            ui_event_t ev;
            ui_next_event( &ui, &ev );
        }
    }
    check( "clicks reach all three tabs (the third one by position)", hit2 > 40 && tv == 2 );
    check( "set_value selects without an event, clamped", ui_set_value( &ui, 1, 7 ) && none() && ui_get_value( &ui, 1, &tv ) && tv == 2 && ui_set_value( &ui, 1, -3 ) && ui_get_value( &ui, 1, &tv ) && tv == 0 );
    ui_draw( &ui, true );
    check( "a tab strip is drawn (the line under the tabs)", gfx_point( &g, 300, 31 ) != 0 );
    done();

    // a popup and drawing never write outside of the canvas (a widget partly outside)
    setup();
    ui_add( &ui, UIK_BUTTON, 1, 380, 230, 60, 22, "Edge", 0 );
    ui_add( &ui, UIK_TEXT, 2, -10, -5, 60, 22, "clip", 0 );
    ui_draw( &ui, true );
    check( "a widget half outside of the canvas is clipped", gfx_point( &g, 399, 239 ) != 0 );
    done();

    test_edit();
    test_colors();
    test_caret();

    printf( "%d checks, %d failed\n", checks, failures );
    return failures;
}
