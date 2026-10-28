
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

#include "sdlmain.h"
#include "sdlscreen.h"
#include "sdlevent.h"

/*
Visual test of sdlscr_ansi_puts(): colored text on the SDL text screen through ANSI SGR escape sequences. The program
prints a few demonstration lines, then repeats what you type (in color) until the window is closed.
*/

static void print_standard_colors( void ) {
    sdlscr_ansi_puts( "Foreground 30-37: " );
    for ( int i=0; i < 8; ++i ) {
        char buf[32];
        snprintf( buf, sizeof(buf), "\033[%dm%d ", 30 + i, 30 + i );
        sdlscr_ansi_puts( buf );
    }
    sdlscr_ansi_puts( "\033[0m\nBright 90-97:     " );
    for ( int i=0; i < 8; ++i ) {
        char buf[32];
        snprintf( buf, sizeof(buf), "\033[%dm%d ", 90 + i, 90 + i );
        sdlscr_ansi_puts( buf );
    }
    sdlscr_ansi_puts( "\033[0m\nBackground 40-47: " );
    for ( int i=0; i < 8; ++i ) {
        char buf[32];
        snprintf( buf, sizeof(buf), "\033[%d;%dm %d \033[0m ", 40 + i, i == 0 ? 97 : 30, 40 + i );
        sdlscr_ansi_puts( buf );
    }
    sdlscr_ansi_puts( "\n" );
}

static void print_palette_strip( void ) {
    // palette entries 0..255 as background colors, 64 per line
    sdlscr_ansi_puts( "Palette (48;5;n):\n" );
    for ( int n=0; n < 256; ++n ) {
        char buf[32];
        snprintf( buf, sizeof(buf), "\033[48;5;%dm ", n );
        sdlscr_ansi_puts( buf );
        if ( n % 64 == 63 ) {
            sdlscr_ansi_puts( "\033[0m\n" );
        }
    }
}

int main( int argc, char** argv ) {

    sdl_init();

    sdlscr_ansi_puts( "\033[97;44m Engine 1999 - ANSI color test \033[0m\n\n" );
    print_standard_colors();
    sdlscr_ansi_puts( "\n" );
    print_palette_strip();
    sdlscr_ansi_puts(
        "\n\033[38;5;120mOrange (38;5;120)\033[0m, "
        "\033[93;41m yellow on red \033[0m, "
        "\033[32mgreen\033[39m back to default, "
        "an escape sequence\033[31;1"
    );
    sdlscr_ansi_puts( "m cut in two calls\033[0m.\n\n" );

    // echo the typed lines in a changing color
    int color = 0;
    while ( !sdlscr_term() ) {
        char buf[128];
        int ev = sdlscr_lineinput( 0, buf, 128U, 0, 0 );
        if ( ev & SDLEV_SCREENWORKERFINISHED ) break;
        char head[16];
        snprintf( head, sizeof(head), "\033[%dm", 31 + color );
        color = ( color + 1 ) % 7;
        sdlscr_ansi_puts( head );
        sdlscr_ansi_puts( "You typed: " );
        sdlscr_ansi_puts( buf );
        sdlscr_ansi_puts( "\033[0m\n" );
    }

    return EXIT_SUCCESS;
}
