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


#include "m3d.h"
#include "m3dgl.h"
#include "glload.h"
#include "gfxops.h"

/*
Automated test of the hardware (OpenGL) renderer of the mesh module (m3dgl.c) against the software renderer: the same scenes are drawn by both and
the pictures compared pixel by pixel. They are not identical (the software renderer rounds the light to 32 levels and then to the nearest palette
color; the hardware renderer scales the true color; the edges of triangles may differ by a pixel), so a scene passes when only a few pixels differ
by more than a tolerance. The palette is a ramp of grays, so that the software renderer's color map is exact. Needs a display and OpenGL 2: without
them the test says so and succeeds (GLTEST_REQUIRE=1: fails). M3DGL_DUMP=prefix writes prefix-<scene>-soft.ppm / -hard.ppm. Exit status = failures.
*/

static int failures = 0, cases = 0;

#define CHECK(cond) do { ++cases; if ( !(cond) ) { ++failures; printf( "FAIL line %d: %s\n", __LINE__, #cond ); } } while (0)

#define VW  160
#define VH  100

static m3d_t* M;
static const char* lasterr;

static bool op( int o, int n, ... ) {
    double a[10];
    va_list ap;
    va_start( ap, n );
    for ( int i=0; i < n; ++i ) {
        a[i] = va_arg( ap, double );
    }
    va_end( ap );
    lasterr = 0;
    bool ok = m3d_op( M, o, a, n, 0, 0, &lasterr );
    if ( !ok ) {
        printf( "op %d: %s\n", o, lasterr != 0 ? lasterr : "failed" );
    }
    return ok;
}

static void tex_pat( int n, int w, int h, int base, int step ) {
    uint8_t* b = (uint8_t*) malloc( 4 + (size_t) w * (size_t) h );
    b[0] = (uint8_t)( w >> 8 ); b[1] = (uint8_t) w; b[2] = (uint8_t)( h >> 8 ); b[3] = (uint8_t) h;
    for ( int y=0; y < h; ++y ) {
        for ( int x=0; x < w; ++x ) {
            b[4 + y * w + x] = (uint8_t)( ( ( x / 2 + y / 2 ) & 1 ) ? base : ( base + step ) );
        }
    }
    double a[1] = { (double) n };
    const char* err = 0;
    if ( !m3d_op( M, GFXOP_M3DTEXTURE, a, 1, (const char*) b, 4 + (size_t) w * (size_t) h, &err ) ) {
        printf( "texture failed: %s\n", err );
    }
    free( b );
}

static void ramp( void ) {
    uint32_t pal[256];
    for ( int i=0; i < 256; ++i ) {
        pal[i] = 0xFF000000U | (uint32_t) i << 16 | (uint32_t) i << 8 | (uint32_t) i;
    }
    m3d_colormap( M, pal );
}

static void scene_base( void ) {
    CHECK( op( GFXOP_M3DRESET, 0 ) );
    CHECK( op( GFXOP_M3DVIEW, 6, 0.0, 0.0, (double) VW, (double) VH, 1.0, 90.0 ) );
    ramp();
    tex_pat( 1, 16, 16, 90, 40 );
    tex_pat( 2, 8, 8, 180, 60 );
    tex_pat( 3, 8, 8, 30, 30 );
    tex_pat( 4, 8, 8, 0, 120 );                                   // index 0 in half of it: see-through when masked
    CHECK( op( GFXOP_M3DOBJECT, 1, 1.0 ) );                       // the floor
    CHECK( op( GFXOP_M3DBOX, 8, 1.0, -500.0, -500.0, -10.0, 500.0, 500.0, 0.0, 1.0 ) );
    CHECK( op( GFXOP_M3DOBJECT, 1, 2.0 ) );                       // a wall ahead
    CHECK( op( GFXOP_M3DBOX, 8, 2.0, -100.0, 200.0, 0.0, 100.0, 220.0, 120.0, 2.0 ) );
    CHECK( op( GFXOP_M3DOBJECT, 1, 3.0 ) );                       // a block to the side
    CHECK( op( GFXOP_M3DBOX, 8, 3.0, 40.0, 100.0, 0.0, 80.0, 140.0, 50.0, 3.0 ) );
    CHECK( op( GFXOP_M3DCAMERA, 5, 0.0, 0.0, 0.0, 90.0, 0.0 ) );  // looking along +y
}

static int tol( int a, int b ) { return a > b ? a - b : b - a; }

// compares the two renderers on the scene as it is now; at most maxbad percent of the pixels may differ by more than `limit` (of 255)
static void compare( const char* name, int limit, double maxbad ) {
    CHECK( op( GFXOP_M3DSOFTWARE, 0 ) );
    const uint8_t* sf = m3d_render( M );
    CHECK( sf != 0 );
    if ( sf == 0 ) { return; }
    uint32_t* soft = (uint32_t*) malloc( sizeof(uint32_t) * VW * VH );
    for ( int i=0; i < VW * VH; ++i ) { soft[i] = sf[i] | (uint32_t) sf[i] << 8 | (uint32_t) sf[i] << 16; }
    CHECK( op( GFXOP_M3DHARDWARE, 0 ) );
    const uint32_t* hard = m3d_render32( M );
    CHECK( hard != 0 );
    if ( hard == 0 ) { free( soft ); return; }
    long bad = 0, sumd = 0;
    for ( int i=0; i < VW * VH; ++i ) {
        int d = tol( (int)( hard[i] & 255U ), (int)( soft[i] & 255U ) );
        int dg = tol( (int)( hard[i] >> 8 & 255U ), (int)( hard[i] & 255U ) );        // the hardware picture is gray too
        if ( d > limit || dg > 1 ) { ++bad; }
        sumd += d;
    }
    double pct = 100.0 * (double) bad / (double)( VW * VH );
    printf( "  %-22s %5.2f %% of the pixels differ by more than %d (mean difference %.2f)\n", name, pct, limit, (double) sumd / (VW * VH) );
    CHECK( pct <= maxbad );
    const char* dump = getenv( "M3DGL_DUMP" );
    if ( dump != 0 ) {
        for ( int k=0; k < 2; ++k ) {
            char fn[300];
            snprintf( fn, sizeof(fn), "%s-%s-%s.ppm", dump, name, k == 0 ? "soft" : "hard" );
            FILE* f = fopen( fn, "wb" );
            if ( f != 0 ) {
                fprintf( f, "P6\n%d %d\n255\n", VW, VH );
                for ( int i=0; i < VW * VH; ++i ) {
                    uint32_t c = k == 0 ? soft[i] : hard[i];
                    fputc( (int)( c >> 16 & 255U ), f ); fputc( (int)( c >> 8 & 255U ), f ); fputc( (int)( c & 255U ), f );
                }
                fclose( f );
            }
        }
    }
    free( soft );
    CHECK( op( GFXOP_M3DSOFTWARE, 0 ) );
}

int main( void ) {
    if ( SDL_Init( SDL_INIT_VIDEO ) != 0 ) {
        printf( "m3dgltest1: skipped (no video)\n" );
        const char* req = getenv( "GLTEST_REQUIRE" );
        return req != 0 && req[0] != 0 && req[0] != '0' ? 1 : 0;
    }
    if ( glload_probe() != GLLOAD_YES ) {
        glinfo_t info;
        glload_info( &info );
        printf( "m3dgltest1: skipped (no OpenGL 2: %s)\n", info.why );
        SDL_Quit();
        const char* req = getenv( "GLTEST_REQUIRE" );
        return req != 0 && req[0] != 0 && req[0] != '0' ? 1 : 0;
    }
    M = m3d_new();
    m3d_set_hardware( M, m3d_gl_renderer() );
    CHECK( m3d_gl_renderer() != 0 );
    CHECK( op( GFXOP_M3DHARDWARE, 0 ) );
    CHECK( m3d_is_argb( M ) );

    scene_base();
    compare( "plain", 24, 3.0 );

    scene_base();
    CHECK( op( GFXOP_M3DOBJLIGHT, 2, 2.0, 120.0 ) );              // dimmer wall
    CHECK( op( GFXOP_M3DOBJLIGHT, 2, 3.0, 200.0 ) );
    compare( "light", 24, 3.0 );

    scene_base();
    CHECK( op( GFXOP_M3DFOG, 1, 400.0 ) );
    compare( "fog", 24, 3.0 );

    scene_base();
    CHECK( op( GFXOP_M3DLAMP, 6, 0.0, 60.0, 90.0, 40.0, 120.0, 250.0 ) );
    CHECK( op( GFXOP_M3DOBJLIGHT, 2, 2.0, 60.0 ) );
    CHECK( op( GFXOP_M3DOBJLIGHT, 2, 1.0, 60.0 ) );
    compare( "lamp", 28, 4.0 );

    scene_base();
    CHECK( op( GFXOP_M3DSKY, 1, 150.0 ) );                        // a flat sky color
    compare( "sky-flat", 24, 3.0 );

    scene_base();
    CHECK( op( GFXOP_M3DSKY, 1, 3.0 ) );                          // a picture
    CHECK( op( GFXOP_M3DCAMERA, 5, 0.0, 0.0, 0.0, 30.0, 10.0 ) );
    compare( "sky-picture", 24, 6.0 );

    scene_base();
    CHECK( op( GFXOP_M3DOBJECT, 3, 4.0, 1.0, 255.0 ) );           // a masked object (flag 1)
    CHECK( op( GFXOP_M3DBOX, 8, 4.0, -60.0, 120.0, 0.0, -20.0, 124.0, 60.0, 4.0 ) );
    compare( "masked", 24, 4.0 );

    scene_base();
    CHECK( op( GFXOP_M3DOBJECT, 3, 5.0, 2.0, 255.0 ) );           // a translucent object (flag 2)
    CHECK( op( GFXOP_M3DBOX, 8, 5.0, -50.0, 60.0, 0.0, 10.0, 64.0, 60.0, 2.0 ) );
    compare( "translucent", 40, 6.0 );

    scene_base();
    CHECK( op( GFXOP_M3DTHING, 7, 0.0, 0.0, 100.0, 0.0, 2.0, 30.0, 50.0 ) );
    CHECK( op( GFXOP_M3DTHING, 7, 1.0, 20.0, 60.0, 0.0, 4.0, 20.0, 30.0 ) );
    compare( "things", 28, 5.0 );

    scene_base();
    CHECK( op( GFXOP_M3DCAMERA, 5, 20.0, -50.0, 10.0, 70.0, -15.0 ) );
    compare( "camera", 24, 4.0 );

    // the hardware renderer follows a change of the view and the pictures
    scene_base();
    CHECK( op( GFXOP_M3DVIEW, 6, 0.0, 0.0, 80.0, 50.0, 1.0, 90.0 ) );
    CHECK( op( GFXOP_M3DHARDWARE, 0 ) );
    int x, y, w, h, sc, bw, bh;
    m3d_getview( M, &x, &y, &w, &h, &sc, &bw, &bh );
    const uint32_t* px = m3d_render32( M );
    CHECK( px != 0 && bw == 80 && bh == 50 );
    uint32_t a = px != 0 ? px[ 25 * 80 + 40 ] : 0;
    tex_pat( 2, 8, 8, 20, 10 );                                   // the wall's picture changes: the next frame shows it
    px = m3d_render32( M );
    CHECK( px != 0 && px[ 25 * 80 + 40 ] != a );

    m3d_free( M );
    SDL_Quit();
    printf( "m3dgltest1: %d cases, %d failures\n", cases, failures );
    return failures;
}
