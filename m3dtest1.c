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
#include "m3dpriv.h"
#include "m3dworld.h"
#include "json.h"
#include "gfxops.h"
#include "noise.h"

/*
Automated test of the software 3D mesh renderer (m3d.[ch], no SDL): objects and their faces, the picture (depth, culling, clipping, masked / translucent
textures, fog, lamps, the sky, things), movement and collision (walls, steps, ceilings, sliding), floor following, shots, use and sight. Prints one line
per failure; the exit status is the number of failures. M3D_DUMP=file writes the last picture as a PPM.
*/

static int failures = 0, cases = 0;

#define NEAR(a,b,eps) do { ++cases; double _a = (a), _b = (b); if ( !( fabs( _a - _b ) <= (eps) ) ) { ++failures; printf( "FAIL line %d: %s = %.17g, expected %.17g\n", __LINE__, #a, _a, _b ); } } while (0)
#define CHECK(cond) do { ++cases; if ( !(cond) ) { ++failures; printf( "FAIL line %d: %s\n", __LINE__, #cond ); } } while (0)

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
    return m3d_op( M, o, a, n, 0, 0, &lasterr );
}

static bool ops( int o, int n, const char* s, ... ) {                  // an operation with a string argument (after the n numbers)
    double a[10];
    va_list ap;
    va_start( ap, s );
    for ( int i=0; i < n; ++i ) {
        a[i] = va_arg( ap, double );
    }
    va_end( ap );
    lasterr = 0;
    return m3d_op( M, o, a, n, s, s != 0 ? strlen( s ) : 0, &lasterr );
}

static double Q( int q, int n, ... ) {
    double a[4] = { 0, 0, 0, 0 }, r = 0;
    va_list ap;
    va_start( ap, n );
    for ( int i=0; i < n; ++i ) {
        a[i] = va_arg( ap, double );
    }
    va_end( ap );
    if ( !m3d_query( M, q, a, n, &r ) ) {
        return -7777777.0;
    }
    return r;
}

static void tex_flat( int n, int w, int h, int col ) {
    uint8_t* b = (uint8_t*) malloc( 4 + (size_t) w * (size_t) h );
    b[0] = (uint8_t)( w >> 8 ); b[1] = (uint8_t) w; b[2] = (uint8_t)( h >> 8 ); b[3] = (uint8_t) h;
    memset( b + 4, col, (size_t) w * (size_t) h );
    double a[1] = { (double) n };
    const char* err = 0;
    if ( !m3d_op( M, GFXOP_M3DTEXTURE, a, 1, (const char*) b, 4 + (size_t) w * (size_t) h, &err ) ) {
        printf( "texture failed: %s\n", err );
    }
    free( b );
}

static const uint8_t* shot( void ) {
    const uint8_t* fb = m3d_render( M );
    int x, y, w, h, sc, bw, bh;
    m3d_getview( M, &x, &y, &w, &h, &sc, &bw, &bh );
    const char* d = getenv( "M3D_DUMP" );
    if ( d != 0 && fb != 0 ) {
        FILE* f = fopen( d, "wb" );
        if ( f != 0 ) {
            fprintf( f, "P6\n%d %d\n255\n", bw, bh );
            for ( int i=0; i < bw * bh; ++i ) {
                fputc( fb[i], f ); fputc( fb[i], f ); fputc( fb[i], f );
            }
            fclose( f );
        }
    }
    return fb;
}

#define PX(fb,x,y) ( (fb)[ (size_t)(y) * 160U + (size_t)(x) ] )

static void fresh( void ) {
    CHECK( op( GFXOP_M3DRESET, 0 ) );
    CHECK( op( GFXOP_M3DVIEW, 6, 0.0, 0.0, 160.0, 100.0, 1.0, 90.0 ) );
    tex_flat( 1, 8, 8, 100 );
    tex_flat( 2, 8, 8, 200 );
    tex_flat( 3, 8, 8, 50 );
    // a floor 1000 x 1000
    // object numbers run 0 .. 16383
    CHECK( op( GFXOP_M3DOBJECT, 1, 16383.0 ) );
    CHECK( Q( GFXQ_M3DOBJON, 1, 16383.0 ) != 0 );
    CHECK( !op( GFXOP_M3DOBJECT, 1, 16384.0 ) );
    CHECK( op( GFXOP_M3DOBJECT, 1, 1.0 ) );
    CHECK( op( GFXOP_M3DBOX, 8, 1.0, -500.0, -500.0, -10.0, 500.0, 500.0, 0.0, 1.0 ) );
    CHECK( op( GFXOP_M3DCAMERA, 4, 0.0, 0.0, 0.0, 0.0 ) );
}


// -- switching the renderer: a stand-in for the hardware renderer ------------------------------------------------------------------

typedef struct { int created, destroyed, resized, palettes, rendered, bw, bh; uint8_t* fb; } fake_t;
static fake_t fk;

static void* fake_create( m3d_t* m ) { (void) m; ++fk.created; return &fk; }
static void fake_destroy( m3d_t* m, void* rd ) { (void) m; (void) rd; ++fk.destroyed; free( fk.fb ); fk.fb = 0; }
static bool fake_resize( m3d_t* m, void* rd, int bw, int bh ) {
    (void) m; (void) rd;
    uint8_t* fb = (uint8_t*) malloc( (size_t) bw * (size_t) bh );
    if ( fb == 0 ) { return false; }
    memset( fb, 77, (size_t) bw * (size_t) bh );
    free( fk.fb ); fk.fb = fb; fk.bw = bw; fk.bh = bh; ++fk.resized;
    return true;
}
static void fake_palette( m3d_t* m, void* rd ) { (void) m; (void) rd; ++fk.palettes; }
static const void* fake_render( m3d_t* m, void* rd ) { (void) m; (void) rd; ++fk.rendered; return fk.fb; }
static const m3drend_t fake_renderer = { "fake", false, fake_create, fake_destroy, fake_resize, fake_palette, fake_render };

static uint32_t fake32[160 * 100];
static const void* fake32_render( m3d_t* m, void* rd ) { (void) m; (void) rd; return fake32; }
static const m3drend_t fake32_renderer = { "fake32", true, fake_create, fake_destroy, fake_resize, fake_palette, fake32_render };

static void test_switch( void ) {
    fresh();
    memset( &fk, 0, sizeof(fk) );
    CHECK( Q( GFXQ_M3DRENDERER, 0 ) == 0 );
    CHECK( Q( GFXQ_M3DHWAVAIL, 0 ) == 0 );
    CHECK( !op( GFXOP_M3DHARDWARE, 0 ) );                       // none registered
    CHECK( lasterr != 0 && strstr( lasterr, "not available" ) != 0 );
    CHECK( Q( GFXQ_M3DRENDERER, 0 ) == 0 );
    CHECK( op( GFXOP_M3DSOFTWARE, 0 ) );                        // already in use: fine
    const uint8_t* soft = shot();
    CHECK( soft != 0 && PX( soft, 80, 50 ) != 77 );
    uint32_t pal[256];
    for ( int i=0; i < 256; ++i ) { pal[i] = 0xFF000000U | (uint32_t)( i * 0x010101U ); }
    m3d_colormap( M, pal );
    m3d_set_hardware( M, &fake_renderer );
    CHECK( Q( GFXQ_M3DHWAVAIL, 0 ) == 1 );
    CHECK( Q( GFXQ_M3DRENDERER, 0 ) == 0 );                     // registering does not switch
    CHECK( op( GFXOP_M3DHARDWARE, 0 ) );
    CHECK( Q( GFXQ_M3DRENDERER, 0 ) == 1 );
    CHECK( fk.created == 1 && fk.destroyed == 0 && fk.palettes == 1 );   // the palette was handed over
    CHECK( fk.bw == 160 && fk.bh == 100 );                      // sized like the view
    const uint8_t* hw = shot();
    CHECK( hw != 0 && PX( hw, 5, 5 ) == 77 && fk.rendered == 1 );
    CHECK( op( GFXOP_M3DVIEW, 6, 0.0, 0.0, 80.0, 40.0, 1.0, 90.0 ) );     // the view changes: the renderer follows
    CHECK( fk.bw == 80 && fk.bh == 40 );
    CHECK( !op( GFXOP_M3DVIEW, 6, 0.0, 0.0, 2.0, 2.0, 1.0, 90.0 ) );     // refused: nothing changes
    CHECK( fk.bw == 80 && fk.bh == 40 );
    m3d_colormap( M, pal );
    CHECK( fk.palettes == 2 );
    CHECK( op( GFXOP_M3DRESET, 0 ) );                           // RESET keeps the renderer
    CHECK( Q( GFXQ_M3DRENDERER, 0 ) == 1 );
    CHECK( op( GFXOP_M3DSOFTWARE, 0 ) );
    CHECK( Q( GFXQ_M3DRENDERER, 0 ) == 0 );
    CHECK( fk.destroyed == 1 );
    CHECK( op( GFXOP_M3DVIEW, 6, 0.0, 0.0, 160.0, 100.0, 1.0, 90.0 ) );
    const uint8_t* again = shot();
    CHECK( again != 0 && fk.rendered == 1 );                    // the fake is no longer asked
    CHECK( op( GFXOP_M3DHARDWARE, 0 ) );
    CHECK( fk.created == 2 );
    m3d_set_hardware( M, 0 );                                   // the host withdraws it: the one in use stays until switched
    CHECK( Q( GFXQ_M3DHWAVAIL, 0 ) == 0 );
    CHECK( op( GFXOP_M3DSOFTWARE, 0 ) );
    CHECK( !op( GFXOP_M3DHARDWARE, 0 ) );
    CHECK( fk.destroyed == 2 );
    m3d_set_hardware( M, &fake_renderer );
    CHECK( op( GFXOP_M3DHARDWARE, 0 ) );
    // a renderer with 32 bit pictures
    CHECK( op( GFXOP_M3DSOFTWARE, 0 ) );
    CHECK( !m3d_is_argb( M ) && m3d_render32( M ) == 0 && m3d_render( M ) != 0 );
    m3d_set_hardware( M, &fake32_renderer );
    CHECK( op( GFXOP_M3DHARDWARE, 0 ) );
    CHECK( m3d_is_argb( M ) );
    CHECK( m3d_render( M ) == 0 );                              // not a palette bitmap
    CHECK( m3d_render32( M ) == fake32 );
    CHECK( op( GFXOP_M3DSOFTWARE, 0 ) );
    CHECK( !m3d_is_argb( M ) && m3d_render32( M ) == 0 );
    m3d_set_hardware( M, &fake_renderer );
    CHECK( op( GFXOP_M3DHARDWARE, 0 ) );
    // m3d_free (below) destroys the renderer in use
}

int main( void ) {
    M = m3d_new();
    if ( M == 0 ) {
        printf( "no renderer\n" );
        return 1;
    }
    // -- a wall in front: the picture, the sky, the floor --------------------------------------------------------------------------
    fresh();
    CHECK( op( GFXOP_M3DOBJECT, 1, 2.0 ) );
    CHECK( op( GFXOP_M3DBOX, 8, 2.0, 100.0, -50.0, 0.0, 110.0, 50.0, 100.0, 2.0 ) );
    const uint8_t* fb = shot();
    CHECK( fb != 0 );
    CHECK( PX( fb, 80, 50 ) == 200 );                       // the wall in the middle
    CHECK( PX( fb, 80, 98 ) == 100 );                       // the floor in front, below the wall
    CHECK( PX( fb, 80, 2 ) == 0 );                          // no sky: black
    CHECK( PX( fb, 2, 50 ) != 200 );                        // beside the wall: floor or nothing
    CHECK( Q( GFXQ_M3DDRAWN, 0 ) > 0 );
    // the wall is closer when the player steps up to it, it grows
    int wide0 = 0, wide1 = 0;
    for ( int x=0; x < 160; ++x ) { if ( PX( fb, x, 40 ) == 200 ) { ++wide0; } }
    CHECK( op( GFXOP_M3DMOVE, 2, 50.0, 0.0 ) );
    fb = shot();
    for ( int x=0; x < 160; ++x ) { if ( PX( fb, x, 40 ) == 200 ) { ++wide1; } }
    CHECK( wide1 > wide0 );
    // -- depth order: a near box hides a far one -----------------------------------------------------------------------------------
    fresh();
    CHECK( op( GFXOP_M3DOBJECT, 1, 2.0 ) );
    CHECK( op( GFXOP_M3DBOX, 8, 2.0, 200.0, -60.0, 0.0, 210.0, 60.0, 100.0, 2.0 ) );          // far, tex 2 (200)
    CHECK( op( GFXOP_M3DOBJECT, 1, 3.0 ) );
    CHECK( op( GFXOP_M3DBOX, 8, 3.0, 60.0, -10.0, 0.0, 70.0, 10.0, 100.0, 3.0 ) );            // near, tex 3 (50), covers the middle
    fb = shot();
    CHECK( PX( fb, 80, 50 ) == 50 );
    CHECK( PX( fb, 60, 50 ) == 200 );
    // object order does not matter: add the far one last
    CHECK( op( GFXOP_M3DOBJECT, 1, 4.0 ) );
    CHECK( op( GFXOP_M3DBOX, 8, 4.0, 300.0, -80.0, 0.0, 310.0, 80.0, 100.0, 1.0 ) );
    fb = shot();
    CHECK( PX( fb, 80, 50 ) == 50 );
    // -- culling: a quad that faces away is not drawn, a two sided one is ------------------------------------------------------------
    fresh();
    CHECK( op( GFXOP_M3DOBJECT, 1, 2.0 ) );
    CHECK( op( GFXOP_M3DVERTEX, 4, 2.0, 100.0, 50.0, 0.0 ) );
    CHECK( op( GFXOP_M3DVERTEX, 4, 2.0, 100.0, -50.0, 0.0 ) );
    CHECK( op( GFXOP_M3DVERTEX, 4, 2.0, 100.0, -50.0, 100.0 ) );
    CHECK( op( GFXOP_M3DVERTEX, 4, 2.0, 100.0, 50.0, 100.0 ) );
    CHECK( op( GFXOP_M3DQUAD, 6, 2.0, 0.0, 1.0, 2.0, 3.0, 2.0 ) );                              // seen from -x: counterclockwise? (a, b, c, d as given)
    fb = shot();
    int front = PX( fb, 80, 50 ) == 200;
    CHECK( op( GFXOP_M3DOBJSHOW, 2, 2.0, 0.0 ) );
    CHECK( op( GFXOP_M3DOBJECT, 1, 3.0 ) );
    CHECK( op( GFXOP_M3DVERTEX, 4, 3.0, 100.0, 50.0, 0.0 ) );
    CHECK( op( GFXOP_M3DVERTEX, 4, 3.0, 100.0, 50.0, 100.0 ) );
    CHECK( op( GFXOP_M3DVERTEX, 4, 3.0, 100.0, -50.0, 100.0 ) );
    CHECK( op( GFXOP_M3DVERTEX, 4, 3.0, 100.0, -50.0, 0.0 ) );
    CHECK( op( GFXOP_M3DQUAD, 6, 3.0, 0.0, 1.0, 2.0, 3.0, 2.0 ) );                              // the same quad, the other way round
    fb = shot();
    int back = PX( fb, 80, 50 ) == 200;
    CHECK( front && !back );                                                                  // exactly one winding is visible
    CHECK( op( GFXOP_M3DOBJFLAGS, 2, 2.0, (double) M3D_OF_TWOSIDED ) );
    CHECK( op( GFXOP_M3DOBJFLAGS, 2, 3.0, (double) M3D_OF_TWOSIDED ) );
    fb = shot();
    CHECK( PX( fb, 80, 50 ) == 200 );
    // -- clipping: standing inside a big box (a room) and looking at its walls, nothing crashes ----------------------------------------
    fresh();
    CHECK( op( GFXOP_M3DOBJECT, 1, 2.0 ) );
    CHECK( op( GFXOP_M3DBOX, 8, 2.0, -200.0, -200.0, 0.0, 200.0, 200.0, 100.0, 2.0 ) );       // seen from inside: the faces point outward
    CHECK( op( GFXOP_M3DOBJFLAGS, 2, 2.0, (double) M3D_OF_TWOSIDED ) );
    CHECK( op( GFXOP_M3DOBJPOS, 4, 2.0, 0.0, 0.0, 0.0 ) );
    fb = shot();
    CHECK( PX( fb, 80, 50 ) == 200 );
    for ( int a=0; a < 360; a += 15 ) {
        CHECK( op( GFXOP_M3DCAMERA, 4, 0.0, 0.0, 0.0, (double) a, 20.0 ) );
        CHECK( shot() != 0 );
    }
    // -- masked and translucent ----------------------------------------------------------------------------------------------------
    fresh();
    {
        uint8_t b[4 + 4 * 4];
        b[0] = 0; b[1] = 4; b[2] = 0; b[3] = 4;
        for ( int i=0; i < 16; ++i ) { b[4 + i] = ( i % 2 ) ? 0 : 200; }
        double a[1] = { 5.0 };
        const char* err = 0;
        CHECK( m3d_op( M, GFXOP_M3DTEXTURE, a, 1, (const char*) b, sizeof(b), &err ) );
    }
    CHECK( op( GFXOP_M3DOBJECT, 1, 3.0 ) );
    CHECK( op( GFXOP_M3DBOX, 8, 3.0, 200.0, -80.0, 0.0, 210.0, 80.0, 100.0, 3.0 ) );          // behind: 50
    CHECK( op( GFXOP_M3DOBJECT, 2, 2.0, 1.0 ) );
    CHECK( op( GFXOP_M3DBOX, 8, 2.0, 100.0, -80.0, 0.0, 101.0, 80.0, 100.0, 5.0 ) );          // masked checker in front
    fb = shot();
    int c200 = 0, c50 = 0;
    for ( int x=60; x < 100; ++x ) {
        if ( PX( fb, x, 40 ) == 200 ) { ++c200; }
        if ( PX( fb, x, 40 ) == 50 ) { ++c50; }
    }
    CHECK( c200 > 5 && c50 > 5 );                                                            // both the masked texture and what is behind it
    CHECK( op( GFXOP_M3DOBJFLAGS, 2, 2.0, (double) M3D_OF_TRANS ) );
    fb = shot();
    CHECK( PX( fb, 80, 40 ) != 0 );
    {   // a thing in front of the camera must not disturb the list of translucent objects (the scratch arrays were shared: the glass panes vanished)
        int mixed = PX( fb, 80, 40 );
        CHECK( op( GFXOP_M3DTHING, 7, 0.0, 400.0, 300.0, 0.0, 2.0, 20.0, 20.0 ) );
        fb = shot();
        CHECK( PX( fb, 80, 40 ) == mixed );
    }
    {   // a translucent color with an alpha of its own in the palette (< 255) is mixed with that alpha, the others half and half
        uint32_t gp[256];
        for ( int i=0; i < 256; ++i ) { gp[i] = 0xFF000000U | (uint32_t)( i * 0x010101U ); }
        gp[150] = 0x40000000U | ( 150U * 0x010101U );               // alpha 64 of 255: a quarter
        gp[151] = 0x00000000U | ( 151U * 0x010101U );               // alpha 0: nothing
        m3d_colormap( M, gp );
        fresh();
        tex_flat( 4, 8, 8, 150 );
        tex_flat( 5, 8, 8, 151 );
        CHECK( op( GFXOP_M3DOBJECT, 1, 2.0, 255.0 ) );
        CHECK( op( GFXOP_M3DBOX, 8, 2.0, 100.0, -80.0, 0.0, 110.0, 80.0, 100.0, 3.0 ) );          // behind: 50
        CHECK( op( GFXOP_M3DOBJECT, 1, 3.0, 255.0 ) );
        CHECK( op( GFXOP_M3DOBJFLAGS, 2, 3.0, (double) M3D_OF_TRANS ) );
        CHECK( op( GFXOP_M3DBOX, 8, 3.0, 60.0, -80.0, 0.0, 61.0, 80.0, 100.0, 4.0 ) );             // glass in front: 150 at an alpha of 64
        fb = shot();
        CHECK( PX( fb, 80, 50 ) == 75 );                                                         // 50 * 3/4 + 150 / 4
        CHECK( op( GFXOP_M3DBOX, 8, 3.0, 60.0, -80.0, 0.0, 61.0, 80.0, 100.0, 2.0 ) );            // adds a half and half color (200 over 75)
        fb = shot();
        CHECK( PX( fb, 80, 50 ) != 75 && PX( fb, 80, 50 ) != 0 );
        fresh();
        tex_flat( 5, 8, 8, 151 );
        CHECK( op( GFXOP_M3DOBJECT, 1, 2.0, 255.0 ) );
        CHECK( op( GFXOP_M3DBOX, 8, 2.0, 100.0, -80.0, 0.0, 110.0, 80.0, 100.0, 3.0 ) );
        CHECK( op( GFXOP_M3DOBJECT, 1, 3.0, 255.0 ) );
        CHECK( op( GFXOP_M3DOBJFLAGS, 2, 3.0, (double) M3D_OF_TRANS ) );
        CHECK( op( GFXOP_M3DBOX, 8, 3.0, 60.0, -80.0, 0.0, 61.0, 80.0, 100.0, 5.0 ) );             // alpha 0: nothing is drawn
        fb = shot();
        CHECK( PX( fb, 80, 50 ) == 50 );
        for ( int i=0; i < 256; ++i ) { gp[i] = 0xFF000000U | (uint32_t)( i * 0x010101U ); }
        m3d_colormap( M, gp );                                                                     // later tests expect the plain ramp
    }
    // -- fog and lamps -------------------------------------------------------------------------------------------------------------
    fresh();
    CHECK( op( GFXOP_M3DOBJECT, 1, 2.0 ) );
    CHECK( op( GFXOP_M3DBOX, 8, 2.0, 100.0, -50.0, 0.0, 110.0, 50.0, 100.0, 2.0 ) );
    CHECK( op( GFXOP_M3DFOG, 1, 130.0 ) );
    fb = shot();
    CHECK( PX( fb, 80, 50 ) != 200 );                                                        // darkened by the fog (100 of 130 away)
    CHECK( op( GFXOP_M3DFOG, 1, 0.0 ) );
    CHECK( op( GFXOP_M3DOBJLIGHT, 2, 2.0, 100.0 ) );
    fb = shot();
    int dark = PX( fb, 80, 50 );
    CHECK( dark != 200 );
    CHECK( op( GFXOP_M3DLAMP, 6, 0.0, 90.0, 0.0, 50.0, 80.0, 255.0 ) );                       // a lamp in front of the wall
    fb = shot();
    CHECK( PX( fb, 80, 50 ) > dark );
    {   // a cone: the same lamp aimed at the wall lights only what is inside the cone, aimed away it lights nothing
        int omni_side = PX( fb, 50, 50 );
        CHECK( omni_side > dark );                                                                // (the omni lamp lights the wall's side too)
        CHECK( op( GFXOP_M3DLAMP, 10, 0.0, 90.0, 0.0, 50.0, 80.0, 255.0, 1.0, 0.0, 0.0, 30.0 ) );
        fb = shot();
        CHECK( PX( fb, 80, 50 ) > dark );                                                         // on the axis
        CHECK( PX( fb, 50, 50 ) == dark );                                                        // 76 degrees off the axis
        CHECK( op( GFXOP_M3DLAMP, 10, 0.0, 90.0, 0.0, 50.0, 80.0, 255.0, -1.0, 0.0, 0.0, 30.0 ) );
        fb = shot();
        CHECK( PX( fb, 80, 50 ) == dark );                                                        // aimed away
        CHECK( !op( GFXOP_M3DLAMP, 10, 0.0, 90.0, 0.0, 50.0, 80.0, 255.0, 0.0, 0.0, 0.0, 20.0 ) );   // no direction
        CHECK( !op( GFXOP_M3DLAMP, 10, 0.0, 90.0, 0.0, 50.0, 80.0, 255.0, 1.0, 0.0, 0.0, 0.0 ) );    // no angle
        CHECK( !op( GFXOP_M3DLAMP, 7, 0.0, 90.0, 0.0, 50.0, 80.0, 255.0, 1.0 ) );                    // a direction needs all four
        CHECK( op( GFXOP_M3DLAMP, 6, 0.0, 90.0, 0.0, 50.0, 80.0, 255.0 ) );                        // omni again
        fb = shot();
        CHECK( PX( fb, 80, 50 ) > dark );
    }
    // -- the sky ---------------------------------------------------------------------------------------------------------------------
    tex_flat( 9, 16, 16, 77 );
    CHECK( op( GFXOP_M3DSKY, 1, 9.0 ) );
    fb = shot();
    CHECK( PX( fb, 80, 2 ) == 77 );
    // -- things ------------------------------------------------------------------------------------------------------------------------
    fresh();
    CHECK( op( GFXOP_M3DTHING, 7, 0.0, 100.0, 0.0, 0.0, 2.0, 40.0, 60.0 ) );
    fb = shot();
    CHECK( PX( fb, 80, 50 ) == 200 );
    CHECK( Q( GFXQ_M3DTON, 1, 0.0 ) != 0 );
    CHECK( Q( GFXQ_M3DDIST, 1, 0.0 ) > 99.0 && Q( GFXQ_M3DDIST, 1, 0.0 ) < 101.0 );
    CHECK( fabs( Q( GFXQ_M3DBEARING, 1, 0.0 ) ) < 1.0 );
    CHECK( op( GFXOP_M3DOBJECT, 1, 3.0 ) );
    CHECK( op( GFXOP_M3DBOX, 8, 3.0, 50.0, -50.0, 0.0, 60.0, 50.0, 100.0, 3.0 ) );            // a wall in front of the thing
    fb = shot();
    CHECK( PX( fb, 80, 50 ) == 50 );                                                         // hidden by the wall (depth test)
    CHECK( Q( GFXQ_M3DSEES, 1, 0.0 ) == 0 );                                                 // no line of sight
    CHECK( op( GFXOP_M3DOBJFLAGS, 2, 3.0, (double) M3D_OF_SEETHRU ) );
    CHECK( Q( GFXQ_M3DSEES, 1, 0.0 ) == -1 );                                                // glass: sees
    // -- shots, use ----------------------------------------------------------------------------------------------------------------------
    CHECK( op( GFXOP_M3DOBJFLAGS, 2, 3.0, 0.0 ) );
    CHECK( op( GFXOP_M3DCAMERA, 4, 0.0, 0.0, 0.0, 0.0 ) );
    CHECK( Q( GFXQ_M3DSHOOT, 1, 500.0 ) == -1 );                                             // the wall hides the thing
    CHECK( Q( GFXQ_M3DSHOTKIND, 0 ) == 1 && Q( GFXQ_M3DSHOTOBJ, 0 ) == 3 );
    CHECK( fabs( Q( GFXQ_M3DSHOTDIST, 0 ) - 50.0 ) < 0.1 );
    CHECK( Q( GFXQ_M3DUSE, 1, 100.0 ) == 3 );
    CHECK( Q( GFXQ_M3DUSE, 1, 20.0 ) == -1 );
    CHECK( op( GFXOP_M3DOBJSHOW, 2, 3.0, 0.0 ) );
    CHECK( Q( GFXQ_M3DSHOOT, 1, 500.0 ) == 0 );                                              // now the thing is hit
    CHECK( Q( GFXQ_M3DSHOTKIND, 0 ) == 2 );
    CHECK( Q( GFXQ_M3DSEES, 1, 0.0 ) == -1 );
    // -- movement: walls, sliding, floor, steps, ceilings ---------------------------------------------------------------------------------
    fresh();
    CHECK( op( GFXOP_M3DOBJECT, 1, 2.0 ) );
    CHECK( op( GFXOP_M3DBOX, 8, 2.0, 100.0, -300.0, 0.0, 110.0, 300.0, 100.0, 2.0 ) );        // a long wall across x
    CHECK( op( GFXOP_M3DMOVE, 2, 500.0, 0.0 ) );
    CHECK( Q( GFXQ_M3DX, 0 ) < 100.0 && Q( GFXQ_M3DX, 0 ) > 70.0 );                          // stopped at the wall (radius 14)
    CHECK( ( (int) Q( GFXQ_M3DHIT, 0 ) & 1 ) != 0 );
    CHECK( op( GFXOP_M3DCAMERA, 4, 0.0, 0.0, 0.0, 45.0 ) );                                  // diagonal: slides along the wall
    CHECK( op( GFXOP_M3DMOVE, 2, 300.0, 0.0 ) );
    CHECK( Q( GFXQ_M3DX, 0 ) < 100.0 );
    CHECK( Q( GFXQ_M3DY, 0 ) > 100.0 );
    // steps: a 20 high step is climbed, a 40 high one is a wall
    fresh();
    CHECK( op( GFXOP_M3DOBJECT, 1, 2.0 ) );
    CHECK( op( GFXOP_M3DBOX, 8, 2.0, 50.0, -100.0, 0.0, 80.0, 100.0, 20.0, 2.0 ) );
    CHECK( op( GFXOP_M3DOBJECT, 1, 3.0 ) );
    CHECK( op( GFXOP_M3DBOX, 8, 3.0, 150.0, -100.0, 0.0, 180.0, 100.0, 40.0, 2.0 ) );
    CHECK( op( GFXOP_M3DMOVE, 2, 65.0, 0.0 ) );
    CHECK( fabs( Q( GFXQ_M3DZ, 0 ) - 20.0 ) < 0.01 );                                         // standing on the step
    CHECK( fabs( Q( GFXQ_M3DFLOORZ, 0 ) - 20.0 ) < 0.01 );
    CHECK( op( GFXOP_M3DMOVE, 2, 60.0, 0.0 ) );                                              // off the step again
    CHECK( op( GFXOP_M3DMOVE, 2, 100.0, 0.0 ) );
    CHECK( Q( GFXQ_M3DX, 0 ) < 150.0 );                                                      // the high step stops it
    CHECK( fabs( Q( GFXQ_M3DFLOORAT, 3, 60.0, 0.0, 100.0 ) - 20.0 ) < 0.01 );
    CHECK( Q( GFXQ_M3DFLOORAT, 3, 600.0, 0.0, 100.0 ) < -100.0 || fabs( Q( GFXQ_M3DFLOORAT, 3, 600.0, 0.0, 100.0 ) ) > 0 );
    // a low ceiling: no entering
    fresh();
    CHECK( op( GFXOP_M3DOBJECT, 1, 2.0 ) );
    CHECK( op( GFXOP_M3DBOX, 8, 2.0, 60.0, -100.0, 40.0, 200.0, 100.0, 50.0, 2.0 ) );         // a slab 40 above the floor: lower than the player
    CHECK( op( GFXOP_M3DMOVE, 2, 150.0, 0.0 ) );
    CHECK( Q( GFXQ_M3DX, 0 ) < 60.0 );
    CHECK( Q( GFXQ_M3DCEILAT, 3, 100.0, 0.0, 1.0 ) > 39.0 && Q( GFXQ_M3DCEILAT, 3, 100.0, 0.0, 1.0 ) < 41.0 );
    // jumping: LIFT lets the player over a wall of 30
    fresh();
    CHECK( op( GFXOP_M3DOBJECT, 1, 2.0 ) );
    CHECK( op( GFXOP_M3DBOX, 8, 2.0, 50.0, -100.0, 0.0, 70.0, 100.0, 30.0, 2.0 ) );
    CHECK( op( GFXOP_M3DMOVE, 2, 100.0, 0.0 ) );
    CHECK( Q( GFXQ_M3DX, 0 ) < 50.0 );
    CHECK( op( GFXOP_M3DLIFT, 1, 10.0 ) );
    CHECK( op( GFXOP_M3DMOVE, 2, 100.0, 0.0 ) );
    CHECK( Q( GFXQ_M3DX, 0 ) > 70.0 );
    // a passable object does not block
    CHECK( op( GFXOP_M3DCAMERA, 4, 0.0, 0.0, 0.0, 0.0 ) );
    CHECK( op( GFXOP_M3DOBJFLAGS, 2, 2.0, (double) M3D_OF_PASS ) );
    CHECK( op( GFXOP_M3DMOVE, 2, 100.0, 0.0 ) );
    CHECK( Q( GFXQ_M3DX, 0 ) > 90.0 );
    // a rotated, moved object (a door): its box follows
    fresh();
    CHECK( op( GFXOP_M3DOBJECT, 1, 2.0 ) );
    CHECK( op( GFXOP_M3DBOX, 8, 2.0, 0.0, -50.0, 0.0, 10.0, 50.0, 100.0, 2.0 ) );
    CHECK( op( GFXOP_M3DOBJPOS, 4, 2.0, 100.0, 0.0, 0.0 ) );
    CHECK( Q( GFXQ_M3DUSE, 1, 200.0 ) == 2 );
    CHECK( op( GFXOP_M3DOBJPOS, 5, 2.0, 100.0, 0.0, 0.0, 90.0 ) );                            // turned: now a slab along x, edge-on
    CHECK( Q( GFXQ_M3DUSE, 1, 200.0 ) == -1 || Q( GFXQ_M3DUSE, 1, 200.0 ) == 2 );
    CHECK( op( GFXOP_M3DOBJPOS, 5, 2.0, 100.0, 80.0, 0.0, 90.0 ) );
    CHECK( Q( GFXQ_M3DUSE, 1, 200.0 ) == -1 );
    CHECK( Q( GFXQ_M3DOBJYAW, 1, 2.0 ) == 90.0 );
    // -- zones, walking things, the world document ----------------------------------------------------------------------------------
    fresh();
    CHECK( op( GFXOP_M3DZONE, 8, 7.0, 0.0, 0.0, 100.0, 100.0, 0.0, 100.0, 5.0 ) );
    CHECK( op( GFXOP_M3DZONE, 7, 8.0, 0.0, 0.0, 300.0, 300.0, 0.0, 100.0 ) );
    CHECK( Q( GFXQ_M3DZONE, 3, 50.0, 50.0, 1.0 ) == 7 );                                    // the first zone that contains the point
    CHECK( Q( GFXQ_M3DZONETAG, 3, 50.0, 50.0, 1.0 ) == 5 );
    CHECK( Q( GFXQ_M3DZONE, 3, 200.0, 50.0, 1.0 ) == 8 );
    CHECK( Q( GFXQ_M3DZONE, 3, 200.0, 50.0, 150.0 ) == -1 );                                // above the zone
    CHECK( Q( GFXQ_M3DZONE, 3, 900.0, 50.0, 1.0 ) == -1 );
    CHECK( op( GFXOP_M3DOBJECT, 1, 2.0 ) );
    CHECK( op( GFXOP_M3DBOX, 8, 2.0, 100.0, -300.0, 0.0, 110.0, 300.0, 100.0, 2.0 ) );
    CHECK( op( GFXOP_M3DTHING, 7, 0.0, 0.0, 0.0, 0.0, 2.0, 28.0, 56.0 ) );
    CHECK( op( GFXOP_M3DTHINGMOVE, 3, 0.0, 300.0, 0.0 ) );
    CHECK( Q( GFXQ_M3DTX, 1, 0.0 ) < 100.0 && Q( GFXQ_M3DTX, 1, 0.0 ) > 60.0 );              // a thing is stopped by the wall
    CHECK( ( (int) Q( GFXQ_M3DHIT, 0 ) & 1 ) != 0 );
    CHECK( op( GFXOP_M3DTHINGMOVE, 3, 0.0, 0.0, 40.0 ) );
    CHECK( Q( GFXQ_M3DTY, 1, 0.0 ) > 39.0 );                                                // and slides along it
    CHECK( op( GFXOP_M3DTHINGOFF, 1, 5.0 ) );                                                // removing a thing that is not there is fine
    CHECK( !op( GFXOP_M3DTHINGMOVE, 3, 5.0, 1.0, 1.0 ) );
    {
        const char* doc = "{\"format\":\"e99-mesh-world\",\"objects\":[{\"id\":3,\"flags\":[\"masked\",\"seethru\"],\"pos\":[10,20,0],\"yaw\":90,"
            "\"boxes\":[[0,0,0,10,10,50,2]],\"quads\":[[0,0,60, 10,0,60, 10,10,60, 0,10,60, 3]]}],"
            "\"zones\":[{\"id\":4,\"box\":[0,0,100,100],\"z\":[0,100],\"tag\":9}],\"things\":[{\"id\":2,\"tex\":1,\"x\":5,\"y\":5,\"w\":10,\"h\":10}],"
            "\"environment\":{\"fog\":500},\"start\":{\"x\":1,\"y\":2,\"z\":0,\"angle\":45}}";
        json_node_t* root = 0;
        char jerr[100];
        const char* werr = 0;
        CHECK( json_parse( doc, strlen( doc ), &root, jerr, sizeof(jerr) ) );
        CHECK( m3dworld_load( M, root, &werr ) );
        CHECK( Q( GFXQ_M3DOBJON, 1, 3.0 ) != 0 && Q( GFXQ_M3DOBJON, 1, 1.0 ) == 0 );          // the old objects are gone
        CHECK( Q( GFXQ_M3DVERTS, 1, 3.0 ) == 28 && Q( GFXQ_M3DTRIS, 1, 3.0 ) == 14 );         // a box (24 / 12) and a quad (4 / 2)
        CHECK( Q( GFXQ_M3DOBJX, 1, 3.0 ) == 10 && Q( GFXQ_M3DOBJYAW, 1, 3.0 ) == 90 );
        CHECK( Q( GFXQ_M3DZONETAG, 3, 50.0, 50.0, 1.0 ) == 9 );
        CHECK( Q( GFXQ_M3DTON, 1, 2.0 ) != 0 );
        CHECK( Q( GFXQ_M3DYAW, 0 ) == 45.0 && Q( GFXQ_M3DX, 0 ) == 1.0 );
        json_node_free( root );
        // a box with a face mask (the 8th number): only the +y face (4) and the roof (16) are built
        const char* fm = "{\"format\":\"e99-mesh-world\",\"objects\":[{\"id\":5,\"boxes\":[[0,0,0,10,10,50,2,20],[0,0,0,10,10,50,2,0]]},{\"id\":6,\"boxes\":[[0,0,0,10,10,50,2]]}]}";
        CHECK( json_parse( fm, strlen( fm ), &root, jerr, sizeof(jerr) ) );
        CHECK( m3dworld_load( M, root, &werr ) );
        CHECK( Q( GFXQ_M3DVERTS, 1, 5.0 ) == 8 && Q( GFXQ_M3DTRIS, 1, 5.0 ) == 4 && Q( GFXQ_M3DTRIS, 1, 6.0 ) == 12 );
        json_node_free( root );
        const char* bad = "{\"format\":\"e99-mesh-world\",\"objects\":[{\"id\":3,\"boxes\":[[0,0,0,10]]}]}";
        CHECK( json_parse( bad, strlen( bad ), &root, jerr, sizeof(jerr) ) );
        CHECK( !m3dworld_load( M, root, &werr ) && strstr( werr, "objects[0].boxes[0]" ) != 0 );
        json_node_free( root );
        const char* nofmt = "{\"objects\":[]}";
        CHECK( json_parse( nofmt, strlen( nofmt ), &root, jerr, sizeof(jerr) ) );
        CHECK( !m3dworld_load( M, root, &werr ) && strstr( werr, "format" ) != 0 );
        json_node_free( root );
    }
    // -- the ground function and the terrain grid (M3D GROUND, GROUNDFLAT, TERRAIN, M3D("GROUND")) ------------------------------------------------------------
    fresh();
    CHECK( Q( GFXQ_M3DGROUND, 2, 100.0, 200.0 ) == 0.0 );                                       // no ground function defined: 0
    CHECK( op( GFXOP_M3DGROUND, 7, 7.0, 400.0, 60.0, 5.0, 10.0, 0.5, 2.0 ) );
    {
        double h = 10.0 + 60.0 * nz_fbm( 7, 100.0 / 400.0, 200.0 / 400.0, 5, 0.5, 2.0 );
        CHECK( Q( GFXQ_M3DGROUND, 2, 100.0, 200.0 ) == h );                                      // the function is base + amp * FBM
        CHECK( Q( GFXQ_M3DGROUND, 2, 100.0, 200.0 ) == Q( GFXQ_M3DGROUND, 2, 100.0, 200.0 ) );
        CHECK( Q( GFXQ_M3DGROUND, 2, -1234.5, 99999.25 ) == 10.0 + 60.0 * nz_fbm( 7, -1234.5 / 400.0, 99999.25 / 400.0, 5, 0.5, 2.0 ) );
    }
    CHECK( op( GFXOP_M3DGROUND, 4, 7.0, 400.0, 60.0, 5.0 ) );                                  // base 0, gain 0.5, lacunarity 2 by default
    CHECK( Q( GFXQ_M3DGROUND, 2, 100.0, 200.0 ) == 60.0 * nz_fbm( 7, 100.0 / 400.0, 200.0 / 400.0, 5, 0.5, 2.0 ) );
    {   // a flat box: 0 inside, the plain height beyond the margin, between them a smooth fall (monotonic)
        double plain = Q( GFXQ_M3DGROUND, 2, 700.0, 10.0 );
        CHECK( op( GFXOP_M3DGROUNDFLAT, 6, 0.0, 0.0, 0.0, 100.0, 50.0, 200.0 ) );
        CHECK( Q( GFXQ_M3DGROUND, 2, 0.0, 0.0 ) == 0.0 && Q( GFXQ_M3DGROUND, 2, 99.0, 49.0 ) == 0.0 );
        CHECK( Q( GFXQ_M3DGROUND, 2, 700.0, 10.0 ) == plain );                                  // 600 beyond the box: outside the margin
        double plain2 = 60.0 * nz_fbm( 7, 250.0 / 400.0, 0.0, 5, 0.5, 2.0 ), mid = Q( GFXQ_M3DGROUND, 2, 250.0, 0.0 );   // 150 beyond the box: d = 0.75, weight 0.0156 * ...
        CHECK( fabs( mid ) <= fabs( plain2 ) + 1e-9 );
        CHECK( fabs( mid - plain2 * ( 1.0 - ( 0.25 * 0.25 * 2.5 ) ) ) < 1e-9 );                 // 1 - (1-d)^2 (1+2d) with d = 0.75
        CHECK( op( GFXOP_M3DGROUNDFLAT, 6, 0.0, 0.0, 0.0, 100.0, 50.0, 0.0 ) );                 // margin 0 removes it
        CHECK( Q( GFXQ_M3DGROUND, 2, 0.0, 0.0 ) == 60.0 * nz_fbm( 7, 0.0, 0.0, 5, 0.5, 2.0 ) );
    }
    // the grid: (nx + 1) (ny + 1) vertices, 2 nx ny triangles, the heights are the ground's (the floor of fresh() is taken away: the ground is also below 0)
    CHECK( op( GFXOP_M3DOBJECT, 1, 1.0, 0.0 ) );
    CHECK( op( GFXOP_M3DOBJECT, 2, 20.0, 0.0 ) );
    CHECK( ops( GFXOP_M3DTERRAIN, 7, 0, 20.0, 0.0, 0.0, 9.0, 9.0, 100.0, 1.0 ) );
    CHECK( Q( GFXQ_M3DVERTS, 1, 20.0 ) == 100.0 && Q( GFXQ_M3DTRIS, 1, 20.0 ) == 162.0 );
    {   // you can walk on it: the floor at a grid point is the ground's height (and between the points it is a slope: its mean)
        CHECK( fabs( Q( GFXQ_M3DFLOORAT, 3, 300.0, 400.0, 1000.0 ) - Q( GFXQ_M3DGROUND, 2, 300.0, 400.0 ) ) < 1e-9 );
        double a = Q( GFXQ_M3DGROUND, 2, 300.0, 400.0 ), b = Q( GFXQ_M3DGROUND, 2, 400.0, 400.0 ), c = Q( GFXQ_M3DGROUND, 2, 300.0, 500.0 ), d = Q( GFXQ_M3DGROUND, 2, 400.0, 500.0 );
        double f = Q( GFXQ_M3DFLOORAT, 3, 350.0, 450.0, 1000.0 );                               // the middle of a cell: on the diagonal b .. c of the triangles (a b d) / (a d c)
        CHECK( fabs( f - ( a + d ) / 2.0 ) < 1e-9 );
        (void) b; (void) c;
    }
    // a coarse grid: 3 x 3 cells of 300 units covers the same square
    CHECK( op( GFXOP_M3DOBJECT, 2, 21.0, 0.0 ) );
    CHECK( ops( GFXOP_M3DTERRAIN, 7, 0, 21.0, 900.0, 0.0, 3.0, 3.0, 300.0, 1.0 ) );
    CHECK( Q( GFXQ_M3DVERTS, 1, 21.0 ) == 16.0 && Q( GFXQ_M3DTRIS, 1, 21.0 ) == 18.0 );
    // skirts: 2 vertices and 2 triangles for every cell along each of the four edges
    CHECK( op( GFXOP_M3DOBJECT, 2, 22.0, 0.0 ) );
    CHECK( ops( GFXOP_M3DTERRAIN, 8, 0, 22.0, 0.0, 0.0, 3.0, 2.0, 100.0, 1.0, 20.0 ) );
    CHECK( Q( GFXQ_M3DVERTS, 1, 22.0 ) == 12.0 + 2.0 * 10.0 && Q( GFXQ_M3DTRIS, 1, 22.0 ) == 12.0 + 2.0 * 10.0 );
    // pictures of the cells: a flat ground, two cells, the pictures 1 and 2 (colors 100 and 200), seen from above
    fresh();
    CHECK( op( GFXOP_M3DGROUND, 4, 1.0, 100.0, 0.0, 1.0 ) );
    CHECK( op( GFXOP_M3DOBJECT, 1, 1.0, 0.0 ) );                                                // (no floor box in the way)
    CHECK( op( GFXOP_M3DOBJECT, 3, 30.0, 0.0, 255.0 ) );
    {
        const char cells[2] = { 0, 1 };                                                          // the cell string is bytes (the first cell picture 1, the second 1 + 1)
        double a[9] = { 30.0, -100.0, 0.0, 2.0, 1.0, 100.0, 1.0, GFX_NONE, GFX_NONE };
        lasterr = 0;
        CHECK( m3d_op( M, GFXOP_M3DTERRAIN, a, 9, cells, 2, &lasterr ) );
    }
    CHECK( op( GFXOP_M3DCAMERA, 5, 0.0, 50.0, 150.0, 0.0, -89.0 ) );
    {
        const uint8_t* fb = shot();
        CHECK( PX( fb, 80, 70 ) == 100 && PX( fb, 80, 30 ) == 200 );                              // the camera looks along +x: ahead (up in the picture) is the second cell
    }
    // errors: the object, the cell string, the sizes
    CHECK( !ops( GFXOP_M3DTERRAIN, 7, 0, 99.0, 0.0, 0.0, 3.0, 3.0, 100.0, 1.0 ) && strstr( lasterr, "not defined" ) != 0 );
    CHECK( op( GFXOP_M3DOBJECT, 1, 23.0 ) );
    CHECK( !ops( GFXOP_M3DTERRAIN, 7, "abc", 23.0, 0.0, 0.0, 2.0, 2.0, 100.0, 1.0 ) && strstr( lasterr, "one character per cell" ) != 0 );
    CHECK( !ops( GFXOP_M3DTERRAIN, 7, "\2", 23.0, 0.0, 0.0, 1.0, 1.0, 100.0, (double)( M3D_MAXTEX - 1 ) ) );   // the last picture number + 2 is none
    CHECK( !ops( GFXOP_M3DTERRAIN, 7, 0, 23.0, 0.0, 0.0, 0.0, 3.0, 100.0, 1.0 ) );                  // no cells
    CHECK( !ops( GFXOP_M3DTERRAIN, 7, 0, 23.0, 0.0, 0.0, 257.0, 3.0, 100.0, 1.0 ) );                // more than 256
    CHECK( !ops( GFXOP_M3DTERRAIN, 7, 0, 23.0, 0.0, 0.0, 3.0, 3.0, 0.0, 1.0 ) );                    // a cell of no size
    CHECK( !op( GFXOP_M3DGROUND, 4, 1.0, 0.0, 10.0, 3.0 ) );                                         // scale 0
    CHECK( !op( GFXOP_M3DGROUND, 4, 1.0, 100.0, 10.0, 17.0 ) );                                      // 17 octaves
    CHECK( !op( GFXOP_M3DGROUND, 7, 1.0, 100.0, 10.0, 3.0, 0.0, 0.5, 0.5 ) );                        // lacunarity below 1
    CHECK( !op( GFXOP_M3DGROUNDFLAT, 6, 8.0, 0.0, 0.0, 1.0, 1.0, 1.0 ) );                            // slot 8
    CHECK( op( GFXOP_M3DCLEAR, 0 ) && Q( GFXQ_M3DGROUND, 2, 100.0, 200.0 ) == 0.0 );                 // M3D CLEAR forgets the ground function
    // -- M3D SUN: terrain cells shaded by their slope with pictures t + k * stride ----------------------------------------------------------------------------
    fresh();
    {
        uint32_t gp[256];
        for ( int i=0; i < 256; ++i ) { gp[i] = 0xFF000000U | (uint32_t) i << 16 | (uint32_t) i << 8 | (uint32_t) i; }
        m3d_colormap( M, gp );                                                                    // a grey ramp: the nearest darker color of index i at scale s is about i * s
    }
    tex_flat( 301, 4, 4, 100 );
    CHECK( op( GFXOP_M3DOBJECT, 1, 1.0, 0.0 ) );
    CHECK( !op( GFXOP_M3DSUN, 3, 0.0, 0.0, -1.0 ) && strstr( lasterr, "above the horizon" ) != 0 );
    CHECK( !op( GFXOP_M3DSUN, 4, 1.0, 0.0, 1.0, 1.5 ) && !op( GFXOP_M3DSUN, 5, 1.0, 0.0, 1.0, 0.5, 9.0 ) && !op( GFXOP_M3DSUN, 5, 1.0, 0.0, 1.0, 0.5, 1.0 ) );
    CHECK( !op( GFXOP_M3DSUN, 6, 1.0, 0.0, 1.0, 0.5, 4.0, 1024.0 ) );
    // level ground: nothing is shaded
    CHECK( op( GFXOP_M3DGROUND, 4, 5.0, 300.0, 0.0, 2.0 ) );
    CHECK( op( GFXOP_M3DSUN, 6, -0.5, -0.3, 0.8, 0.5, 4.0, 100.0 ) );
    CHECK( op( GFXOP_M3DOBJECT, 1, 30.0, 0.0 ) );
    CHECK( ops( GFXOP_M3DTERRAIN, 7, 0, 30.0, 0.0, 0.0, 6.0, 6.0, 100.0, 301.0 ) );
    CHECK( Q( GFXQ_M3DTEXW, 1, 401.0 ) == 0.0 && Q( GFXQ_M3DTEXW, 1, 501.0 ) == 0.0 && Q( GFXQ_M3DTEXW, 1, 601.0 ) == 0.0 );
    // steep hills: shaded pictures appear, darker by step (k = 1: scale 1 - 1/3 * 0.5, k = 3: 0.5), index 0 stays, the picture's size stays
    CHECK( op( GFXOP_M3DGROUND, 4, 5.0, 300.0, 400.0, 2.0 ) );
    CHECK( op( GFXOP_M3DOBJECT, 1, 31.0, 0.0 ) );
    CHECK( ops( GFXOP_M3DTERRAIN, 7, 0, 31.0, 0.0, 0.0, 12.0, 12.0, 100.0, 301.0 ) );
    {
        int made = 0, dark = 0;
        for ( int k = 1; k <= 3; ++k ) {
            if ( Q( GFXQ_M3DTEXW, 1, (double)( 301 + 100 * k ) ) == 4.0 && Q( GFXQ_M3DTEXH, 1, (double)( 301 + 100 * k ) ) == 4.0 ) {
                ++made;
                int v = M->texs[301 + 100 * k].col[0];
                double want = 100.0 * ( 1.0 - (double) k / 3.0 * 0.5 );
                if ( fabs( v - want ) <= 1.0 ) { ++dark; }
            }
        }
        CHECK( made >= 2 && dark == made );
    }
    CHECK( Q( GFXQ_M3DTRIS, 1, 31.0 ) == 288.0 );
    // a flat color above 255 is not shaded (no picture to make), a flat color below is a picture of one texel
    CHECK( op( GFXOP_M3DOBJECT, 1, 32.0, 0.0 ) );
    CHECK( ops( GFXOP_M3DTERRAIN, 7, 0, 32.0, 0.0, 0.0, 12.0, 12.0, 100.0, 150.0 ) );
    {
        int one = 0;
        for ( int k = 1; k <= 3; ++k ) {
            if ( Q( GFXQ_M3DTEXW, 1, (double)( 150 + 100 * k ) ) == 1.0 ) { ++one; }
        }
        CHECK( one >= 1 );
    }
    // a palette change makes the shaded pictures again; sun off deletes them
    {
        int n = -1;
        for ( int k = 3; k >= 1; --k ) { if ( M->texs[301 + 100 * k].col != 0 ) { n = 301 + 100 * k; } }
        uint32_t gp[256];
        for ( int i=0; i < 256; ++i ) { gp[i] = 0xFF000000U | (uint32_t)( 255 - i ) << 16 | (uint32_t)( 255 - i ) << 8 | (uint32_t)( 255 - i ); }
        gp[0] = 0xFF000000U;
        m3d_colormap( M, gp );
        CHECK( n > 0 && M->texs[n].col != 0 );
        CHECK( op( GFXOP_M3DSUN, 3, 0.0, 0.0, 0.0 ) );
        CHECK( n > 0 && M->texs[n].col == 0 && M->texs[301].col != 0 );                          // the picture itself stays
    }
    // -- M3D MINIMAP: a shaded height map of the ground function ------------------------------------------------------------------------------------------
    fresh();
    {
        uint32_t mp[256];
        memset( mp, 0, sizeof(mp) );
        mp[1] = 0xFF2A5AAAU;                                                                      // water
        mp[2] = 0xFF3C8232U;                                                                      // low green (60, 130, 50)
        mp[3] = 0xFFF5F5F5U;                                                                      // snow
        mp[4] = 0xFF828282U;                                                                      // rock
        m3d_colormap( M, mp );
        uint8_t px[32 * 32], px2[32 * 32];
        CHECK( op( GFXOP_M3DGROUND, 4, 1.0, 100.0, 0.0, 1.0 ) );                                  // no ground (flat 0): level
        CHECK( m3d_minimap( M, mp, 32, 32, 0.0, 0.0, 500.0, -1.0e9, px ) );
        int same = 1;
        for ( int i=1; i < 32 * 32; ++i ) { if ( px[i] != px[0] ) { same = 0; } }
        CHECK( same && px[0] == 2 );                                                              // level land at height 0 is the low green
        CHECK( m3d_minimap( M, mp, 32, 32, 0.0, 0.0, 500.0, 10.0, px ) && px[0] == 1 && px[32 * 32 - 1] == 1 );   // below the water level: water
        CHECK( op( GFXOP_M3DGROUND, 5, 1.0, 100.0, 0.001, 1.0, 400.0 ) );                         // a plateau at 400: snow
        CHECK( m3d_minimap( M, mp, 8, 8, 0.0, 0.0, 100.0, -1.0e9, px ) && px[0] == 3 );
        // real hills: pictures differ from place to place, the same call gives the same picture, the picture follows the centre
        CHECK( op( GFXOP_M3DGROUND, 4, 7.0, 200.0, 120.0, 4.0 ) );
        CHECK( m3d_minimap( M, mp, 32, 32, 0.0, 0.0, 800.0, 20.0, px ) && m3d_minimap( M, mp, 32, 32, 0.0, 0.0, 800.0, 20.0, px2 ) && memcmp( px, px2, sizeof(px) ) == 0 );
        int kinds[8] = { 0 };
        for ( int i=0; i < 32 * 32; ++i ) { if ( px[i] < 8 ) { kinds[px[i]] = 1; } }
        CHECK( kinds[1] + kinds[2] + kinds[3] + kinds[4] >= 2 );
        CHECK( m3d_minimap( M, mp, 32, 32, 100.0, 0.0, 800.0, 20.0, px2 ) && memcmp( px, px2, sizeof(px) ) != 0 );
        // north is up: the pixel at the top is world y larger: a ground that rises with y gives a higher color at the top
        CHECK( op( GFXOP_M3DGROUND, 4, 7.0, 200.0, 0.0, 1.0, 0.0 ) );
        CHECK( !m3d_minimap( M, mp, 0, 8, 0.0, 0.0, 100.0, 0.0, px ) && !m3d_minimap( M, mp, 8, 8, 0.0, 0.0, 0.0, 0.0, px ) );
        // the statement only checks (the host draws)
        CHECK( op( GFXOP_M3DMINIMAP, 7, 10.0, 20.0, 64.0, 64.0, 0.0, 0.0, 1000.0 ) && op( GFXOP_M3DMINIMAP, 8, 10.0, 20.0, 64.0, 64.0, 0.0, 0.0, 1000.0, -12.0 ) );
        CHECK( !op( GFXOP_M3DMINIMAP, 7, 10.0, 20.0, 0.0, 64.0, 0.0, 0.0, 1000.0 ) && !op( GFXOP_M3DMINIMAP, 7, 10.0, 20.0, 513.0, 64.0, 0.0, 0.0, 1000.0 ) &&
               !op( GFXOP_M3DMINIMAP, 7, 10.0, 20.0, 64.0, 64.0, 0.0, 0.0, 0.0 ) );
    }
    // -- instances: shared meshes (M3D INSTANCE, OBJSCALE, OBJFREE) ----------------------------------------------------------------------------------------
    fresh();
    CHECK( op( GFXOP_M3DOBJECT, 1, 1.0, 0.0 ) );                                              // the floor of fresh() is gone
    CHECK( op( GFXOP_M3DOBJECT, 2, 40.0, 0.0, 255.0 ) );
    CHECK( op( GFXOP_M3DBOX, 8, 40.0, -5.0, -5.0, 0.0, 5.0, 5.0, 20.0, 1.0 ) );                // the prototype: a box 10 x 10 x 20
    CHECK( op( GFXOP_M3DOBJPOS, 4, 40.0, -300.0, 0.0, 0.0 ) );
    CHECK( op( GFXOP_M3DOBJSHOW, 2, 40.0, 0.0 ) );                                              // hidden: it does not stand in the way
    CHECK( op( GFXOP_M3DINSTANCE, 7, 41.0, 40.0, 100.0, 0.0, 0.0, 0.0, 2.0 ) );                 // twice the size at (100, 0)
    CHECK( op( GFXOP_M3DINSTANCE, 5, 42.0, 40.0, 200.0, 50.0, 3.0 ) );                          // 1 x, yaw 0, raised 3 at (200, 50)
    CHECK( op( GFXOP_M3DINSTANCE, 6, 43.0, 40.0, 300.0, 0.0, 0.0, 90.0 ) );                     // turned by 90 degrees
    CHECK( Q( GFXQ_M3DVERTS, 1, 41.0 ) == 24.0 && Q( GFXQ_M3DTRIS, 1, 41.0 ) == 12.0 );
    CHECK( Q( GFXQ_M3DOBJX, 1, 41.0 ) == 100.0 && Q( GFXQ_M3DOBJYAW, 1, 43.0 ) == 90.0 );
    CHECK( Q( GFXQ_M3DOBJON, 1, 41.0 ) != 0.0 );
    CHECK( Q( GFXQ_M3DFLOORAT, 3, 100.0, 0.0, 1000.0 ) == 40.0 );                               // the top of the doubled box: 20 * 2
    CHECK( Q( GFXQ_M3DFLOORAT, 3, 109.0, 0.0, 1000.0 ) == 40.0 && Q( GFXQ_M3DFLOORAT, 3, 111.0, 0.0, 1000.0 ) < -1.0e8 );   // it is 20 wide now: 90 .. 110
    CHECK( Q( GFXQ_M3DFLOORAT, 3, 200.0, 50.0, 1000.0 ) == 23.0 );                              // raised by 3
    CHECK( Q( GFXQ_M3DFLOORAT, 3, -300.0, 0.0, 1000.0 ) < -1.0e8 );                            // the hidden prototype is not there
    // copy on write: changing the prototype does not change the instances, changing an instance does not change the prototype
    CHECK( op( GFXOP_M3DBOX, 8, 40.0, 50.0, 50.0, 0.0, 60.0, 60.0, 5.0, 1.0 ) );
    CHECK( Q( GFXQ_M3DVERTS, 1, 40.0 ) == 48.0 && Q( GFXQ_M3DVERTS, 1, 41.0 ) == 24.0 && Q( GFXQ_M3DVERTS, 1, 42.0 ) == 24.0 );
    CHECK( op( GFXOP_M3DBOX, 8, 42.0, 70.0, 70.0, 0.0, 80.0, 80.0, 5.0, 1.0 ) );
    CHECK( Q( GFXQ_M3DVERTS, 1, 42.0 ) == 48.0 && Q( GFXQ_M3DVERTS, 1, 43.0 ) == 24.0 && Q( GFXQ_M3DVERTS, 1, 40.0 ) == 48.0 );
    // an instance of an instance; the size changes later; the instance of a made-again object
    CHECK( op( GFXOP_M3DINSTANCE, 5, 44.0, 43.0, 0.0, 0.0, 0.0 ) );
    CHECK( Q( GFXQ_M3DVERTS, 1, 44.0 ) == 24.0 );
    CHECK( op( GFXOP_M3DOBJSCALE, 2, 44.0, 3.0 ) && Q( GFXQ_M3DFLOORAT, 3, 0.0, 0.0, 1000.0 ) == 60.0 );
    CHECK( op( GFXOP_M3DOBJECT, 1, 43.0, 0.0 ) );                                               // object 43 made again (empty): its instance keeps the mesh
    CHECK( Q( GFXQ_M3DVERTS, 1, 43.0 ) == 0.0 && Q( GFXQ_M3DVERTS, 1, 44.0 ) == 24.0 );
    CHECK( op( GFXOP_M3DINSTANCE, 5, 44.0, 43.0, 0.0, 0.0, 0.0 ) && Q( GFXQ_M3DVERTS, 1, 44.0 ) == 0.0 );   // an instance of an empty one is empty
    // a picture: an instance is drawn like the object it copies
    fresh();
    CHECK( op( GFXOP_M3DOBJECT, 1, 1.0, 0.0 ) );
    CHECK( op( GFXOP_M3DOBJECT, 2, 50.0, 0.0, 255.0 ) );
    CHECK( op( GFXOP_M3DBOX, 8, 50.0, 100.0, -10.0, 0.0, 110.0, 10.0, 40.0, 2.0 ) );
    CHECK( op( GFXOP_M3DINSTANCE, 5, 51.0, 50.0, 0.0, 0.0, 0.0 ) );
    CHECK( op( GFXOP_M3DOBJSHOW, 2, 50.0, 0.0 ) );
    CHECK( op( GFXOP_M3DCAMERA, 4, 0.0, 0.0, 0.0, 0.0 ) );
    {
        const uint8_t* fb = shot();
        CHECK( PX( fb, 80, 70 ) == 200 );                                                       // the instance at 100 in front of the camera is seen (the prototype is hidden)
    }
    // OBJFREE: objects are not defined any more (and the highest number in use falls)
    CHECK( op( GFXOP_M3DOBJFREE, 2, 40.0, 51.0 ) );
    CHECK( Q( GFXQ_M3DOBJON, 1, 50.0 ) == 0.0 && Q( GFXQ_M3DOBJON, 1, 51.0 ) == 0.0 && Q( GFXQ_M3DVERTS, 1, 50.0 ) == -7777777.0 );
    CHECK( !op( GFXOP_M3DOBJFREE, 2, 5.0, 4.0 ) );                                              // b below a
    CHECK( op( GFXOP_M3DOBJFREE, 2, 7000.0, 7003.0 ) );                                         // objects that are not there: fine
    // errors
    CHECK( !op( GFXOP_M3DINSTANCE, 2, 60.0, 99.0 ) && strstr( lasterr, "not defined" ) != 0 );
    CHECK( op( GFXOP_M3DOBJECT, 1, 61.0 ) );
    CHECK( !op( GFXOP_M3DINSTANCE, 2, 61.0, 61.0 ) && strstr( lasterr, "own instance" ) != 0 );
    CHECK( !op( GFXOP_M3DINSTANCE, 7, 62.0, 61.0, 0.0, 0.0, 0.0, 0.0, 0.0 ) );                  // scale 0
    CHECK( !op( GFXOP_M3DOBJSCALE, 2, 99.0, 2.0 ) );
    CHECK( !op( GFXOP_M3DOBJSCALE, 2, 61.0, 0.0 ) );
    // many instances and changes: nothing leaks or crashes (run under ASan / valgrind to see it)
    fresh();
    CHECK( op( GFXOP_M3DOBJECT, 1, 100.0, 0.0 ) );
    CHECK( op( GFXOP_M3DBOX, 8, 100.0, -5.0, -5.0, 0.0, 5.0, 5.0, 20.0, 1.0 ) );
    for ( int i=0; i < 300; ++i ) {
        (void) ( op( GFXOP_M3DINSTANCE, 7, (double)( 101 + i % 150 ), (double)( i % 3 == 0 ? 100 : 101 + ( i + 7 ) % 150 ), (double)( i * 13 % 400 - 200 ), (double)( i * 7 % 300 ), 0.0, (double)( i % 360 ), 1.0 + ( i % 4 ) * 0.5 ) );     // (some of the sources were freed: those fail, which is right)
        if ( i % 5 == 0 ) { (void) op( GFXOP_M3DBOX, 8, (double)( 101 + i % 150 ), 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0 ); }
        if ( i % 7 == 0 ) { CHECK( op( GFXOP_M3DOBJFREE, 2, (double)( 101 + i % 150 ), (double)( 101 + i % 150 ) ) ); }
    }
    CHECK( shot() != 0 );
    // -- mountains and lakes in the ground function ----------------------------------------------------------------------------------------------------
    fresh();
    CHECK( op( GFXOP_M3DGROUND, 4, 7.0, 400.0, 20.0, 3.0 ) );
    {
        double x = 1234.5, y = -987.25, plain = Q( GFXQ_M3DGROUND, 2, x, y );
        CHECK( op( GFXOP_M3DMOUNTAINS, 6, 5.0, 300.0, 100.0, 4.0, -1.0, -0.99 ) );                 // everywhere (the slow noise is above -0.99 nearly all over): t = 1
        double m = nz_fbm( 6, x / 1200.0, y / 1200.0, 2, 0.5, 2.0 ), t = ( m + 1.0 ) / 0.01;
        t = t <= 0 ? 0 : t >= 1 ? 1 : t * t * ( 3 - 2 * t );
        NEAR( Q( GFXQ_M3DGROUND, 2, x, y ), plain + 100.0 * t * nz_ridged( 5, x / 300.0, y / 300.0, 4, 0.5, 2.0 ), 1e-9 );
        CHECK( Q( GFXQ_M3DGROUND, 2, x, y ) >= plain - 1e-9 );
        CHECK( op( GFXOP_M3DMOUNTAINS, 6, 5.0, 300.0, 0.0, 4.0, -1.0, -0.99 ) );                   // amp 0 takes them away
        CHECK( Q( GFXQ_M3DGROUND, 2, x, y ) == plain );
        CHECK( op( GFXOP_M3DMOUNTAINS, 6, 5.0, 300.0, 100.0, 4.0, 0.95, 1.0 ) );                   // a mask that is (almost) never reached: nothing here
        CHECK( !op( GFXOP_M3DMOUNTAINS, 6, 5.0, 300.0, 100.0, 4.0, 0.5, 0.5 ) && !op( GFXOP_M3DMOUNTAINS, 6, 5.0, 0.0, 100.0, 4.0, 0.0, 0.5 ) );
        CHECK( Q( GFXQ_M3DGROUND, 2, x, y ) == plain );
        CHECK( op( GFXOP_M3DLAKES, 5, 9.0, 500.0, 30.0, -1.0, -0.99 ) );                           // basins everywhere
        {
            double lm = ( nz_fbm( 9, x / 500.0, y / 500.0, 2, 0.5, 2.0 ) + 1.0 ) / 0.01, lt = lm <= 0 ? 0 : lm >= 1 ? 1 : lm * lm * ( 3 - 2 * lm );
            NEAR( Q( GFXQ_M3DGROUND, 2, x, y ), plain - 30.0 * lt, 1e-9 );
        }
        CHECK( Q( GFXQ_M3DGROUND, 2, x, y ) <= plain + 1e-9 && Q( GFXQ_M3DGROUND, 2, x, y ) >= plain - 30.0 - 1e-9 );
        CHECK( op( GFXOP_M3DLAKES, 5, 9.0, 500.0, 0.0, -1.0, -0.99 ) );
        CHECK( Q( GFXQ_M3DGROUND, 2, x, y ) == plain );
        CHECK( !op( GFXOP_M3DLAKES, 5, 9.0, 0.0, 30.0, -1.0, 0.5 ) );
        // a quiet box: no mountains and no lakes inside it, the full effect beyond its margin
        CHECK( op( GFXOP_M3DMOUNTAINS, 6, 5.0, 300.0, 100.0, 4.0, -1.0, -0.99 ) && op( GFXOP_M3DLAKES, 5, 9.0, 500.0, 30.0, -1.0, -0.99 ) );
        double withboth = Q( GFXQ_M3DGROUND, 2, x, y );
        CHECK( withboth != plain );
        CHECK( op( GFXOP_M3DGROUNDQUIET, 5, x, y, 10.0, 10.0, 50.0 ) );
        CHECK( Q( GFXQ_M3DGROUND, 2, x, y ) == plain );                                          // inside: the plain hills
        double mid = Q( GFXQ_M3DGROUND, 2, x + 40.0, y ), plainmid;
        CHECK( op( GFXOP_M3DMOUNTAINS, 6, 5.0, 300.0, 0.0, 4.0, -1.0, -0.99 ) && op( GFXOP_M3DLAKES, 5, 9.0, 500.0, 0.0, -1.0, -0.99 ) );
        plainmid = Q( GFXQ_M3DGROUND, 2, x + 40.0, y );
        CHECK( mid != plainmid );                                                                // in the margin they are there, partly
        CHECK( op( GFXOP_M3DMOUNTAINS, 6, 5.0, 300.0, 100.0, 4.0, -1.0, -0.99 ) && op( GFXOP_M3DLAKES, 5, 9.0, 500.0, 30.0, -1.0, -0.99 ) );
        CHECK( op( GFXOP_M3DGROUNDQUIET, 5, x, y, 10.0, 10.0, 0.0 ) );                           // margin 0 removes it
        CHECK( Q( GFXQ_M3DGROUND, 2, x, y ) == withboth );
        CHECK( !op( GFXOP_M3DGROUNDQUIET, 5, x, y, -1.0, 10.0, 5.0 ) );
        CHECK( op( GFXOP_M3DMOUNTAINS, 6, 5.0, 300.0, 100.0, 4.0, -1.0, -0.99 ) && op( GFXOP_M3DCLEAR, 0 ) && Q( GFXQ_M3DGROUND, 2, x, y ) == 0.0 );   // CLEAR forgets them too
    }
    // -- errors -----------------------------------------------------------------------------------------------------------------------------
    CHECK( !op( GFXOP_M3DOBJECT, 1, 20000.0 ) );
    CHECK( !op( GFXOP_M3DBOX, 8, 99.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0 ) );                 // object not defined
    CHECK( !op( GFXOP_M3DFACE, 5, 2.0, 0.0, 1.0, 99.0, 1.0 ) );                                 // vertices missing
    CHECK( !op( GFXOP_M3DVIEW, 4, 0.0, 0.0, 2.0, 2.0 ) );
    CHECK( !op( GFXOP_M3DPLAYER, 4, -1.0, 56.0, 46.0, 24.0 ) );
    CHECK( !op( GFXOP_M3DTHING, 7, 5000.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0 ) );
    // -- random frames: no crash, no hang ------------------------------------------------------------------------------------------------
    fresh();
    for ( int i=0; i < 40; ++i ) {
        CHECK( op( GFXOP_M3DOBJECT, 1, (double)( 2 + i ) ) );
        double x = ( i * 73 ) % 400 - 200, y = ( i * 131 ) % 400 - 200;
        CHECK( op( GFXOP_M3DBOX, 8, (double)( 2 + i ), x, y, 0.0, x + 20 + i, y + 30, 20.0 + 3 * i, (double)( 1 + i % 3 ) ) );
    }
    for ( int i=0; i < 300; ++i ) {
        CHECK( op( GFXOP_M3DCAMERA, 5, ( i * 37 ) % 500 - 250.0, ( i * 53 ) % 500 - 250.0, 0.0, (double)( i * 29 % 360 ), (double)( i % 60 - 30 ) ) );
        CHECK( shot() != 0 );
        CHECK( op( GFXOP_M3DMOVE, 2, 7.0 * ( i % 5 ), 3.0 ) );
    }
    test_switch();
    m3d_free( M );
    CHECK( fk.destroyed == 5 );
    printf( "m3dtest1: %d cases, %d failures\n", cases, failures );
    return failures;
}
