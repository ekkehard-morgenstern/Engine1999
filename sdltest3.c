
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


#include "sdlmain.h"
#include "sdlscreen.h"
#include "sdlevent.h"
#include "cursors.h"
#include "palette.h"
#include <strings.h>

/*
Visual test of the mouse pointer's pictures (cursors.c): every shape is shown on three backgrounds (light, dark, striped) and the animated ones play. They are
copied into sprites of the program for the show; the real pointer, the reserved sprite 255, is drawn in front of everything and can be moved and changed:

    N / P    next / previous shape of the pointer
    W A S D  move the pointer by 4 pixels (hold SHIFT for 16)
    Q        quit

The hot spot of a shape (the pixel that is "where the pointer is") is marked with a small red cross on the pointer's position.
*/

#define GALLERY_IMAGE   100             // the images of the program 100.. get a copy of the engine's pictures
#define GALLERY_SEQ     100             // the sequences 100.. play the animations

static int color_by_name( const char* name, int dflt ) {
    for ( int i = 0; i < 256; ++i ) {
        if ( strcasecmp( pal_title( i ), name ) == 0 && ( pal_argb( i ) >> 24 ) == 255U ) {
            return i;
        }
    }
    return dflt;
}

int main( int argc, char** argv ) {
    (void) argc; (void) argv;
    sdl_init();
    sdlscr_ansi_puts( "\033[97;44m Engine 1999 - mouse pointer shapes \033[0m   N / P: shape   W A S D: move   Q: quit\n" );
    // backgrounds on the background layer (below the text and the sprites): a light, a dark and a striped band
    int light = color_by_name( "Silver", 7 ), dark = color_by_name( "Black", 1 ), stripe = color_by_name( "DarkSlateBlue", 28 ), stripe2 = color_by_name( "Gold", 244 );
    sdlscr_gfx_box( 0, 0, 36, 639, 120, light, true );
    sdlscr_gfx_box( 0, 0, 121, 639, 205, dark, true );
    for ( int x = 0; x < 640; x += 8 ) {
        sdlscr_gfx_box( 0, x, 206, x + 3, 299, stripe, true );
        sdlscr_gfx_box( 0, x + 4, 206, x + 7, 299, stripe2, true );
    }
    for ( int s = 0; s < CUR_COUNT; ++s ) {
        int first, count;
        int speed = cursors_images( s, &first, &count );
        for ( int f = 0; f < count; ++f ) {
            uint8_t img[SPRITE_WIDTH * SPRITE_HEIGHT];
            if ( sdlscr_cursor_image( first + f, img ) ) {
                sdlscr_writesprite( GALLERY_IMAGE + first + f, img );
            }
        }
        // three copies of each shape: on the light, the dark and the striped band
        for ( int band = 0; band < 3; ++band ) {
            int spr = s * 3 + band;
            int x = 24 + s * 50;
            int y = band == 0 ? 60 : band == 1 ? 146 : 236;
            sdlscr_showsprite( spr, true );
            sdlscr_movesprite( spr, x, y );
            if ( count > 1 ) {
                uint8_t seq[16];
                for ( int f = 0; f < count; ++f ) {
                    seq[f] = (uint8_t)( GALLERY_IMAGE + first + f );
                }
                sdlscr_spriteanimdata( GALLERY_SEQ + s, seq, (size_t) count );
                sdlscr_spriteanimcfg( spr, GALLERY_SEQ + s, count, speed );
            } else {
                sdlscr_sprite_setimage( spr, GALLERY_IMAGE + first );
            }
        }
        // the name under the middle sprite (text screen: 8 x 12 pixels per character, 80 x 25)
        int col = ( 24 + s * 50 ) / 8, row = 9;
        sdlscr_printf( row, col, -1, -1, "%.5s", cursors_name( s ) );
    }
    sdlscr_cursor_show( true );
    int px = 320, py = 150;
    int shape = CUR_ARROW;
    sdlscr_cursor_shape( shape );
    sdlscr_cursor_move( px, py );
    sdlscr_pollkeys( true );
    while ( !sdlscr_term() ) {
        int ev = sdlev_wait( SDLEV_VBLANK | SDLEV_SCREENWORKERFINISHED );
        if ( ev & SDLEV_SCREENWORKERFINISHED ) {
            break;
        }
        int k;
        bool quit = false;
        while ( ( k = sdlscr_pollkey() ) != 0 ) {
            int step = k >= 'A' && k <= 'Z' ? 16 : 4;
            switch ( k | 32 ) {
                case 'n': shape = ( shape + 1 ) % CUR_COUNT; sdlscr_cursor_shape( shape ); break;
                case 'p': shape = ( shape + CUR_COUNT - 1 ) % CUR_COUNT; sdlscr_cursor_shape( shape ); break;
                case 'w': py -= step; break;
                case 's': py += step; break;
                case 'a': px -= step; break;
                case 'd': px += step; break;
                case 'q': quit = true; break;
                default: break;
            }
        }
        if ( quit ) {
            break;
        }
        if ( px < 0 ) px = 0;
        if ( px > 639 ) px = 639;
        if ( py < 0 ) py = 0;
        if ( py > 299 ) py = 299;
        sdlscr_cursor_move( px, py );
        sdlscr_printf( 24, 0, -1, -1, "pointer: %-6s at %3d,%3d ", cursors_name( shape ), px, py );
    }
    return EXIT_SUCCESS;
}
