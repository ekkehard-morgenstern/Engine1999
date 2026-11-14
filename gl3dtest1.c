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
#include "shot.h"
#include "palette.h"

/*
Automated test of the 32 bit layer of the hardware 3D renderer in a real window (sdlscreen.c: sdlscr_3d_put() / sdlscr_3d_hide()). It needs a display;
without one it says so and succeeds (GLTEST_REQUIRE=1: fails). A picture of ARGB pixels is put into the layer and the composed screen is read back
with shot_grab(): the picture must be there, scaled, with everything around it transparent; what is drawn on the graphics target layer must stay in
front of it, the target layer's own pixels under the picture are gone; hiding the layer shows what was below. Exit status = failures.
*/

static int failures = 0, cases = 0;

#define CHECK(cond) do { ++cases; if ( !(cond) ) { ++failures; printf( "FAIL line %d: %s\n", __LINE__, #cond ); } } while (0)

static bool frames( int n ) {
    uint64_t f = sdlscr_getframecnt();
    while ( sdlscr_getframecnt() < f + (uint64_t) n ) {
        if ( sdlscr_term() ) {
            return false;
        }
        sdlscr_vsync();
    }
    return true;
}

#define PIXEL(img,x,y) ( (img)[ (size_t)(y) * SHOT_WIDTH + (size_t)(x) ] )

int main( void ) {
    if ( SDL_Init( SDL_INIT_VIDEO ) != 0 ) {
        printf( "gl3dtest1: skipped (no video)\n" );
        const char* req = getenv( "GLTEST_REQUIRE" );
        return req != 0 && req[0] != 0 && req[0] != '0' ? 1 : 0;
    }
    SDL_Quit();
    sdl_init();
    uint32_t* shot = (uint32_t*) malloc( sizeof(uint32_t) * SHOT_WIDTH * SHOT_HEIGHT );
    sdlscr_gfx_reset( true );
    sdlscr_gfx_settarget( 3 );
    CHECK( frames( 3 ) );
    CHECK( shot_grab( shot ) );
    uint32_t before = PIXEL( shot, 100, 100 );              // whatever the screen shows there now (the backdrop and the text layer)

    // a 4 x 2 picture, every pixel scaled 10 x 10, at (80, 60): 40 x 20 pixels; the columns are red, green, blue, white (opaque)
    uint32_t src[8];
    uint32_t col[4] = { 0xFFFF0000U, 0xFF00FF00U, 0xFF0000FFU, 0xFFFFFFFFU };
    for ( int y=0; y < 2; ++y ) {
        for ( int x=0; x < 4; ++x ) {
            src[y*4+x] = col[x] & 0x00FFFFFFU;              // (the alpha is not trusted: the layer makes the picture opaque)
        }
    }
    // graphics layer content: a magenta bar across the picture (stays in front), a cyan dot that lies under the picture (is cleared)
    int magenta = 0, cyan = 0;
    for ( int i=0; i < 256; ++i ) {
        uint32_t c = pal_argb( i );
        if ( c == 0xFFFF00FFU && !magenta ) { magenta = i; }
        if ( c == 0xFF00FFFFU && !cyan ) { cyan = i; }
    }
    if ( magenta == 0 || cyan == 0 ) {
        magenta = 1; cyan = 2;                              // (any two opaque colors will do)
    }
    sdlscr_gfx_box( 3, 90, 75, 110, 76, magenta, true );
    sdlscr_gfx_box( 3, 82, 62, 83, 63, cyan, true );
    sdlscr_3d_put( 80, 60, src, 4, 2, 10 );
    CHECK( frames( 3 ) );
    CHECK( shot_grab( shot ) );
    CHECK( PIXEL( shot, 85, 65 ) == 0xFFFF0000U );          // column 0
    CHECK( PIXEL( shot, 95, 65 ) == 0xFF00FF00U );
    CHECK( PIXEL( shot, 105, 79 ) == 0xFF0000FFU );
    CHECK( PIXEL( shot, 115, 70 ) == 0xFFFFFFFFU );
    CHECK( PIXEL( shot, 79, 65 ) != 0xFFFF0000U && PIXEL( shot, 120, 65 ) != 0xFFFFFFFFU );     // one outside is not the picture
    CHECK( PIXEL( shot, 85, 59 ) != 0xFFFF0000U && PIXEL( shot, 85, 80 ) != 0xFFFF0000U );
    CHECK( PIXEL( shot, 100, 75 ) != 0xFF00FF00U );         // the HUD bar (graphics layer) is in front of the picture ...
    CHECK( PIXEL( shot, 82, 62 ) == 0xFFFF0000U );          // ... the dot under the picture was cleared (the picture shows)
    CHECK( PIXEL( shot, 100, 100 ) == before );             // outside the region: as before

    // a new picture elsewhere: the old one is gone
    sdlscr_3d_put( 300, 100, src, 4, 2, 1 );
    CHECK( frames( 3 ) );
    CHECK( shot_grab( shot ) );
    CHECK( PIXEL( shot, 85, 65 ) != 0xFFFF0000U );
    CHECK( PIXEL( shot, 300, 100 ) == 0xFFFF0000U && PIXEL( shot, 303, 101 ) == 0xFFFFFFFFU );

    // hiding it shows what was below
    sdlscr_3d_hide();
    CHECK( frames( 3 ) );
    CHECK( shot_grab( shot ) );
    CHECK( PIXEL( shot, 300, 100 ) != 0xFFFF0000U );

    // GFX RESET hides it as well
    sdlscr_3d_put( 300, 100, src, 4, 2, 1 );
    CHECK( frames( 3 ) );
    CHECK( shot_grab( shot ) );
    CHECK( PIXEL( shot, 300, 100 ) == 0xFFFF0000U );
    sdlscr_gfx_reset( false );
    CHECK( frames( 3 ) );
    CHECK( shot_grab( shot ) );
    CHECK( PIXEL( shot, 300, 100 ) != 0xFFFF0000U );
    free( shot );
    printf( "gl3dtest1: %d cases, %d failures\n", cases, failures );
    return failures;
}
