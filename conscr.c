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


#include "conscr.h"
#include "textscreen.h"
#include "palette.h"
#include "ansi.h"
#include "keyseq.h"
#include <sys/ioctl.h>
#include <unistd.h>

static bool active = false, inited = false, truecolor = false;
static textcell_t* shadow = 0;              // what the terminal shows: the cells (0xFFFFFFFF = unknown)
static int shw = 0, shh = 0;
static int lastfg = -1, lastbg = -1;        // the colors the terminal is set to

bool conscr_active( void ) {
    return active;
}

// the terminal's size
void conscr_termsize( int* cols, int* rows ) {
    struct winsize ws;
    *cols = 80; *rows = 25;
    if ( ioctl( 1, TIOCGWINSZ, &ws ) == 0 && ws.ws_col > 0 && ws.ws_row > 0 ) {
        *cols = ws.ws_col; *rows = ws.ws_row;
    } else {
        const char* c = getenv( "COLUMNS" ); const char* r = getenv( "LINES" );
        if ( c != 0 && atoi( c ) > 0 ) *cols = atoi( c );
        if ( r != 0 && atoi( r ) > 0 ) *rows = atoi( r );
    }
    if ( *cols > TXTSCR_MAXW ) *cols = TXTSCR_MAXW;
    if ( *rows > TXTSCR_MAXH ) *rows = TXTSCR_MAXH;
}

static void conscr_forget( void ) {         // the terminal's contents are unknown: everything is painted again
    free( shadow );
    shadow = 0; shw = shh = 0;
    lastfg = lastbg = -1;
}

bool conscr_syncsize( void ) {
    int cols, rows, w, h;
    conscr_termsize( &cols, &rows );
    txtscr_getscreensize( &w, &h );
    if ( cols == w && rows == h ) {
        return false;
    }
    txtscr_resize( cols, rows );
    conscr_forget();
    if ( active ) {
        fputs( "\033[2J", stdout );
    }
    return true;
}

bool conscr_begin( void ) {
    if ( active ) {
        return true;
    }
    if ( !isatty( 1 ) ) {
        return false;
    }
    if ( !inited ) {
        txtscr_init();
        txtscr_enablecursor( false );       // the terminal's own cursor is used
        inited = true;
    }
    const char* ct = getenv( "COLORTERM" );
    truecolor = ct != 0 && ( strcmp( ct, "truecolor" ) == 0 || strcmp( ct, "24bit" ) == 0 );
    active = true;
    conscr_forget();
    fputs( "\033[?1049h\033[?7l\033[0m\033[2J", stdout );      // alternate screen, no automatic wrap, clear
    conscr_syncsize();
    fflush( stdout );
    return true;
}

void conscr_end( void ) {
    if ( !active ) {
        return;
    }
    fputs( "\033[0m\033[?7h\033[?25h\033[?1049l", stdout );
    fflush( stdout );
    active = false;
    conscr_forget();
}

// -- colors ----------------------------------------------------------------------------------

// the xterm 256 color number nearest to an RGB value: the 6x6x6 cube (16..231) or the gray ramp (232..255)
int conscr_rgb_to_256( int r, int g, int b ) {
    static const int level[6] = { 0, 95, 135, 175, 215, 255 };
    int ri = 0, gi = 0, bi = 0, best;
    for ( int i = 1; i < 6; ++i ) {
        if ( abs( r - level[i] ) < abs( r - level[ri] ) ) ri = i;
        if ( abs( g - level[i] ) < abs( g - level[gi] ) ) gi = i;
        if ( abs( b - level[i] ) < abs( b - level[bi] ) ) bi = i;
    }
    int cube = 16 + 36 * ri + 6 * gi + bi;
    int cd = ( r - level[ri] ) * ( r - level[ri] ) + ( g - level[gi] ) * ( g - level[gi] ) + ( b - level[bi] ) * ( b - level[bi] );
    int avg = ( r + g + b ) / 3;
    int gi2 = ( avg - 8 + 5 ) / 10;                 // gray ramp: 8, 18, ..., 238
    if ( gi2 < 0 ) gi2 = 0;
    if ( gi2 > 23 ) gi2 = 23;
    int gv = 8 + 10 * gi2;
    int gd = ( r - gv ) * ( r - gv ) + ( g - gv ) * ( g - gv ) + ( b - gv ) * ( b - gv );
    best = gd < cd ? 232 + gi2 : cube;
    return best;
}

static void conscr_setcolors( int fg, int bg ) {
    if ( fg == lastfg && bg == lastbg ) {
        return;
    }
    uint32_t f = pal_argb( fg ), b = pal_argb( bg );
    if ( truecolor ) {
        printf( "\033[38;2;%u;%u;%u;48;2;%u;%u;%um", (unsigned)( ( f >> 16 ) & 255U ), (unsigned)( ( f >> 8 ) & 255U ), (unsigned)( f & 255U ),
            (unsigned)( ( b >> 16 ) & 255U ), (unsigned)( ( b >> 8 ) & 255U ), (unsigned)( b & 255U ) );
    } else {
        printf( "\033[38;5;%d;48;5;%dm", conscr_rgb_to_256( (int)( ( f >> 16 ) & 255U ), (int)( ( f >> 8 ) & 255U ), (int)( f & 255U ) ),
            conscr_rgb_to_256( (int)( ( b >> 16 ) & 255U ), (int)( ( b >> 8 ) & 255U ), (int)( b & 255U ) ) );
    }
    lastfg = fg; lastbg = bg;
}

// -- painting --------------------------------------------------------------------------------

static void conscr_putchar( int code ) {
    if ( code < 0x20 || code == 0x7F ) {
        fputc( '?', stdout );
        return;
    }
    char u[4];
    char c = (char) code;
    size_t n = keyseq_latin9_to_utf8( &c, 1U, u );
    fwrite( u, 1U, n, stdout );
}

void conscr_paint( void ) {
    if ( !active ) {
        return;
    }
    int w, h;
    txtscr_getscreensize( &w, &h );
    if ( shadow == 0 || shw != w || shh != h ) {
        conscr_forget();
        shadow = (textcell_t*) malloc( (size_t) w * (size_t) h * sizeof(textcell_t) );
        if ( shadow == 0 ) {
            return;
        }
        for ( int i = 0; i < w * h; ++i ) {
            shadow[i] = UINT32_C(0XFFFFFFFF);
        }
        shw = w; shh = h;
        fputs( "\033[2J", stdout );
    }
    fputs( "\033[?25l", stdout );
    for ( int y = 0; y < h; ++y ) {
        int x = 0;
        while ( x < w ) {
            textcell_t c = txtscr_getdisplaycell( y, x );
            if ( c == shadow[ y * w + x ] ) {
                ++x;
                continue;
            }
            printf( "\033[%d;%dH", y + 1, x + 1 );
            // a run of changed cells (a short gap of equal cells is cheaper to write than to skip)
            int gap = 0;
            while ( x < w ) {
                c = txtscr_getdisplaycell( y, x );
                if ( c == shadow[ y * w + x ] ) {
                    if ( ++gap > 8 ) {
                        break;
                    }
                } else {
                    gap = 0;
                }
                conscr_setcolors( TXTSCR_CELL_FG( c ), TXTSCR_CELL_BG( c ) );
                conscr_putchar( TXTSCR_CELL_CHR( c ) );
                shadow[ y * w + x ] = c;
                ++x;
            }
        }
    }
    int cx, cy;
    txtscr_getabscursor( &cx, &cy );
    if ( cx >= w ) cx = w - 1;
    if ( cy >= h ) cy = h - 1;
    printf( "\033[%d;%dH\033[?25h", cy + 1, cx + 1 );
    fflush( stdout );
}

// -- program output on the screen ------------------------------------------------------------

static ansi_t conscr_ansi[TXTSCR_NSCREENS];
static bool conscr_ansiok[TXTSCR_NSCREENS];

static void conscr_emit( void* usrdata, int bg, int fg, const char* text, size_t len ) {
    (void) usrdata;
    txtscr_print_n( -1, -1, bg, fg, text, len );
}

void conscr_write( const char* text, size_t len ) {
    int n = txtscr_selected();
    if ( !conscr_ansiok[n] ) {
        int bg = TXTSCR_BGCOL, fg = TXTSCR_FGCOL;
        txtscr_getcolors( &bg, &fg );
        ansi_init( &conscr_ansi[n], fg, bg );
        conscr_ansiok[n] = true;
    }
    ansi_feed_n( &conscr_ansi[n], text, len, conscr_emit, 0 );
}

// -- line mode ---------------------------------------------------------------------------------

static int lcols = 80, lrows = 25, lx = 0, ly = 0;      // the model of the terminal: its size and the cursor (lx < lcols)
static bool lsilent = false;                            // nothing is written (input or output is not a terminal): only the model runs

void conscr_line_setsilent( bool silent ) {
    lsilent = silent;
}

void conscr_line_init( int cols, int rows, int cx, int cy ) {
    lcols = cols < 1 ? 1 : cols; lrows = rows < 1 ? 1 : rows;
    lx = cx < 0 ? 0 : cx >= lcols ? lcols - 1 : cx;
    ly = cy < 0 ? 0 : cy >= lrows ? lrows - 1 : cy;
}

void conscr_line_getsize( int* cols, int* rows ) { *cols = lcols; *rows = lrows; }
void conscr_line_getviewport( int* x, int* y, int* w, int* h ) { *x = 0; *y = 0; *w = lcols; *h = lrows; }
void conscr_line_getcursor( int* x, int* y ) { *x = lx; *y = ly; }

void conscr_line_locate( int x, int y ) {
    if ( x < 0 ) x = 0;
    if ( y < 0 ) y = 0;
    if ( x >= lcols ) x = lcols - 1;
    if ( y >= lrows ) y = lrows - 1;
    if ( !lsilent ) {
        printf( "\033[%d;%dH", y + 1, x + 1 );
    }
    lx = x; ly = y;
}

static void conscr_line_newline( void ) {       // the cursor to the start of the next row; at the bottom the terminal scrolls
    if ( !lsilent ) {
        fputs( "\r\n", stdout );
    }
    lx = 0;
    if ( ly < lrows - 1 ) {
        ++ly;
    }
}

int conscr_line_write( int y, int x, int bg, int fg, const char* text, int len ) {
    (void) bg; (void) fg;
    if ( len < 0 ) {
        len = (int) strlen( text );
    }
    if ( y >= 0 && x >= 0 ) {
        conscr_line_locate( x, y );
    }
    int n = len;
    if ( lx + n > lcols ) {
        n = lcols - lx;
    }
    for ( int i = 0; i < n && !lsilent; ++i ) {
        conscr_putchar( (unsigned char) text[i] );
    }
    lx += n;
    if ( lx >= lcols ) {
        lx = lcols - 1;                         // (stays on the last column: nothing wraps here)
        if ( !lsilent ) {
            printf( "\033[%d;%dH", ly + 1, lx + 1 );
        }
    }
    return n;
}

void conscr_line_print( int y, int x, int bg, int fg, const char* text ) {
    (void) bg; (void) fg;
    if ( y >= 0 && x >= 0 ) {
        conscr_line_locate( x, y );
    }
    for ( ; *text; ++text ) {
        unsigned char c = (unsigned char) *text;
        if ( c == '\n' ) {
            conscr_line_newline();
            continue;
        }
        if ( !lsilent ) {
            conscr_putchar( c );
        }
        ++lx;
        if ( lx >= lcols ) {                    // a full row: wrap at once (the terminal would wait for the next character)
            conscr_line_newline();
        }
    }
}

void conscr_line_changed( void ) {
    if ( !lsilent ) {
        fflush( stdout );
    }
}

int conscr_line_restart( int absy, int startx, size_t pos ) {
    (void) absy;
    // the terminal may have reflowed the text: the line starts where the cursor, less what is before it, is
    long lin = (long) ly * lcols + lx - (long) pos - startx;
    if ( lin < 0 ) {
        lin = 0;
    }
    return (int)( lin / lcols );
}
