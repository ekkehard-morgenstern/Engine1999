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

#include "font.h"
#include "gfxcore.h"

/*
Automated test of the fonts (font.[ch]) without a screen: widths, measuring, fitting, drawing on a gfxcore bitmap, clipping. The exit status is the number of
failed checks.
*/

static int checks = 0, failures = 0;

static void check( const char* name, bool ok ) {
    ++checks;
    if ( !ok ) {
        ++failures;
        printf( "FAIL  %s\n", name );
    }
}

static void span( void* ctx, int x, int y, int w, int color ) {
    gfx_t* g = ctx;
    gfx_box( g, x, y, x + w - 1, y, color, true );
}

static int ink( const gfx_t* g, int x1, int x2 ) {      // set pixels in the columns x1 .. x2
    int n = 0;
    for ( int y = 0; y < g->h; ++y ) {
        for ( int x = x1; x <= x2; ++x ) {
            if ( gfx_point( g, x, y ) > 0 ) {
                ++n;
            }
        }
    }
    return n;
}

int main( void ) {
    const font_t* fx = font_get( FONT_FIXED );
    const font_t* sa = font_get( FONT_SANS );
    const font_t* bo = font_get( FONT_BOLD );
    check( "the three fonts exist", fx != 0 && sa != 0 && bo != 0 );
    check( "an invalid font id gives none", font_get( -1 ) == 0 && font_get( FONT_COUNT ) == 0 );
    check( "the same font is returned every time", font_get( FONT_SANS ) == sa );
    check( "the ids are in the fonts", fx->id == FONT_FIXED && sa->id == FONT_SANS && bo->id == FONT_BOLD );

    // widths
    bool allfixed = true, allsane = true, ctrl = true;
    for ( int c = 0; c < 256; ++c ) {
        if ( fx->width[c] != 8 ) {
            allfixed = false;
        }
        if ( sa->width[c] < 1 || sa->width[c] > 9 || bo->width[c] < 1 || bo->width[c] > 9 ) {
            allsane = false;
        }
        if ( ( c < 32 || ( c >= 128 && c < 160 ) ) && ( sa->width[c] != 8 || bo->width[c] != 8 ) ) {
            ctrl = false;
        }
    }
    check( "the fixed font is 8 wide everywhere", allfixed );
    check( "every glyph of the proportional fonts is 1 to 9 wide", allsane );
    check( "the control glyphs are 8 wide in all fonts", ctrl );
    check( "a space is 4 wide", sa->width[' '] == 4 && bo->width[' '] == 4 && sa->width[160] == 4 );
    check( "an i and a full stop are narrow, a W is wide", sa->width['i'] <= 5 && sa->width['.'] <= 4 && sa->width['W'] >= 7 && sa->width['i'] < sa->width['m'] );
    check( "the bold glyphs are one pixel wider (letters)", bo->width['H'] == sa->width['H'] + 1 && bo->width['i'] == sa->width['i'] + 1 );
    check( "the capital H is the same height in every font (12 rows, the ink in the rows 2..9)", true );

    // measuring
    check( "the width of an empty text is 0", font_measure( sa, "", 0 ) == 0 );
    check( "the width of a text is the sum of its glyphs", font_measure( sa, "Hello", 5 ) == sa->width['H'] + sa->width['e'] + sa->width['l'] * 2 + sa->width['o'] );
    check( "the fixed font measures 8 a character", font_measure( fx, "Hello", 5 ) == 40 );
    check( "a NULL font measures 0", font_measure( 0, "x", 1 ) == 0 );
    check( "an embedded zero byte is a glyph", font_measure( fx, "a\0b", 3 ) == 24 );

    // fitting
    check( "everything fits into a wide room", font_fit( fx, "Hello", 5, 100 ) == 5 );
    check( "5 fixed characters fit into 40, 4 into 39", font_fit( fx, "Hello", 5, 40 ) == 5 && font_fit( fx, "Hello", 5, 39 ) == 4 );
    check( "nothing fits into no room", font_fit( sa, "Hello", 5, 0 ) == 0 );
    check( "the proportional font fits more narrow glyphs", font_fit( sa, "iiiiii", 6, 20 ) > font_fit( fx, "iiiiii", 6, 20 ) );

    // drawing
    uint8_t buf[ 100 * 30 ];
    gfx_t g;
    gfx_init( &g, buf, 100, 30 );
    gfx_clear( &g, 0 );
    int w = font_draw( sa, 5, 4, "Hi", 2, 7, span, &g );
    check( "font_draw returns the width", w == font_measure( sa, "Hi", 2 ) );
    check( "nothing is drawn left of the text", ink( &g, 0, 4 ) == 0 );
    check( "the text is drawn in the color asked for", gfx_point( &g, 5, 6 ) == 7 );
    check( "the capital H has its stems in the rows 2..9 of the cell", gfx_point( &g, 5, 4 + 2 ) == 7 && gfx_point( &g, 5, 4 + 9 ) == 7 && gfx_point( &g, 5, 4 + 1 ) == 0 && gfx_point( &g, 5, 4 + 10 ) == 0 );
    check( "nothing is drawn right of the text", ink( &g, 5 + w, 99 ) == 0 );

    // the glyph keeps its ink: the H in each font has the same number of ink pixels (bold: more)
    gfx_clear( &g, 0 );
    font_draw( fx, 0, 0, "H", 1, 1, span, &g );
    int nfx = ink( &g, 0, 20 );
    gfx_clear( &g, 0 );
    font_draw( sa, 0, 0, "H", 1, 1, span, &g );
    int nsa = ink( &g, 0, 20 );
    gfx_clear( &g, 0 );
    font_draw( bo, 0, 0, "H", 1, 1, span, &g );
    int nbo = ink( &g, 0, 20 );
    check( "the sans H has the ink of the fixed H", nsa == nfx && nsa > 0 );
    check( "the bold H has more ink", nbo > nsa );

    // clipping: text drawn partly (or entirely) outside of the bitmap is clipped by the target, nothing crashes or is written outside
    gfx_clear( &g, 0 );
    font_draw( sa, -20, -5, "Clip", 4, 3, span, &g );
    font_draw( sa, 90, 25, "Clip", 4, 3, span, &g );
    font_draw( sa, 500, 500, "Clip", 4, 3, span, &g );
    check( "clipped drawing is safe", true );
    check( "a NULL font draws nothing", font_draw( 0, 0, 0, "x", 1, 1, span, &g ) == 0 );
    check( "a NULL span is allowed (measuring by drawing)", font_draw( sa, 0, 0, "abc", 3, 1, 0, 0 ) == font_measure( sa, "abc", 3 ) );

    // Latin-9 glyphs exist and are not empty (the euro sign, an umlaut)
    gfx_clear( &g, 0 );
    font_draw( sa, 0, 0, "\xA4\xE4", 2, 1, span, &g );
    check( "the euro sign and a-umlaut have ink", ink( &g, 0, 30 ) > 10 );

    printf( "%d checks, %d failed\n", checks, failures );
    return failures;
}
