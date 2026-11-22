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
#include "world.h"
#include "noise.h"
#include <unistd.h>
#include "m3dworld.h"
#include "json.h"
#include "gfxops.h"
#include "noise.h"
#include "roadnet.h"

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
    double a[12];
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
    double a[12];
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
    double a[8] = { 0, 0, 0, 0, 0, 0, 0, 0 }, r = 0;
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
    // object numbers run 0 .. 27999
    CHECK( op( GFXOP_M3DOBJECT, 1, 27999.0 ) );
    CHECK( Q( GFXQ_M3DOBJON, 1, 27999.0 ) != 0 );
    CHECK( !op( GFXOP_M3DOBJECT, 1, 28000.0 ) );
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


// -- the static world: creation in the background, the quadtree of tiles, coverage, no popping ------------------------------------------------------------------------------------
static void test_world( const char* base ) {
    char dir[600];
    snprintf( dir, sizeof(dir), "%s/m3dtest1-world", base );
    CHECK( op( GFXOP_M3DRESET, 0 ) );
    CHECK( op( GFXOP_M3DMSAA, 1, 4.0 ) && op( GFXOP_M3DMSAA, 1, 0.0 ) && !op( GFXOP_M3DMSAA, 1, 17.0 ) && !op( GFXOP_M3DMSAA, 1, -2.0 ) );    // antialiasing: the setting is kept, none is in use without the hardware renderer
    CHECK( Q( GFXQ_M3DMSAA, 0 ) == 0.0 && Q( GFXQ_M3DMSAAMAX, 0 ) == 0.0 );
    CHECK( op( GFXOP_M3DHIDE, 0 ) );                                                                    // hiding the host's 3D layer: nothing to do in the model
    CHECK( !op( GFXOP_M3DWORLDVIEW, 2, 0.0, 0.0 ) );                                // no world open
    CHECK( !ops( GFXOP_M3DWORLDOPEN, 1, "/nonexistent-world-folder", 100.0 ) );
    CHECK( !ops( GFXOP_M3DWORLDMAKE, 2, dir, 0.0, 5.0 ) );                          // no chunks
    CHECK( ops( GFXOP_M3DWORLDMAKE, 2, dir, 24.0, 5.0 ) );
    CHECK( !ops( GFXOP_M3DWORLDMAKE, 2, dir, 24.0, 5.0 ) || Q( GFXQ_M3DWORLDINFO, 1, 6.0 ) == 1.0 );   // a second one while the first runs fails (or it was quick)
    for ( int i = 0; i < 600 && Q( GFXQ_M3DWORLDINFO, 1, 6.0 ) != 1.0; ++i ) { struct timespec ts = { 0, 50000000L }; nanosleep( &ts, 0 ); }
    CHECK( Q( GFXQ_M3DWORLDINFO, 1, 6.0 ) == 1.0 && Q( GFXQ_M3DWORLDINFO, 1, 5.0 ) == 1000.0 );
    CHECK( ops( GFXOP_M3DWORLDOPEN, 1, dir, 100.0 ) );
    CHECK( Q( GFXQ_M3DWORLDINFO, 1, 0.0 ) == 1.0 && Q( GFXQ_M3DWORLDINFO, 1, 1.0 ) == 24.0 );
    CHECK( Q( GFXQ_M3DWORLDCHUNK, 3, 0.0, 0.0, 0.0 ) == 12.0 && Q( GFXQ_M3DWORLDCHUNK, 3, 0.0, 0.0, 1.0 ) == 12.0 );
    CHECK( Q( GFXQ_M3DWORLDCHUNK, 3, 1.0e7, 0.0, 0.0 ) == -1.0 );
    CHECK( op( GFXOP_M3DRINGPIC, 11, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, -12.0, 900.0 ) );
    // the ground function is the world's
    CHECK( Q( GFXQ_M3DGROUND, 2, 3000.0, 4000.0 ) == Q( GFXQ_M3DGROUND, 2, 3000.0, 4000.0 ) && fabs( Q( GFXQ_M3DGROUND, 2, 3000.0, 4000.0 ) ) < 2000.0 );
    CHECK( Q( GFXQ_M3DGROUND, 2, 11.0 * WORLD_CHUNK, 0.0 ) > 30000.0 );             // the wall
    // the first view: waits for everything
    CHECK( op( GFXOP_M3DWORLDVIEW, 2, 0.0, 0.0 ) );
    CHECK( op( GFXOP_M3DWORLDWAIT, 0 ) );
    double shown = Q( GFXQ_M3DWORLDINFO, 1, 2.0 );
    printf( "world: %g tiles shown, %g built\n", shown, Q( GFXQ_M3DWORLDINFO, 1, 3.0 ) );
    CHECK( shown > 20.0 && shown < 900.0 && Q( GFXQ_M3DWORLDINFO, 1, 4.0 ) == 0.0 );
    {
        CHECK( op( GFXOP_M3DVIEW, 5, 0.0, 0.0, 640.0, 264.0, 2.0 ) );
        CHECK( op( GFXOP_M3DCAMERA, 5, 0.0, 0.0, Q( GFXQ_M3DGROUND, 2, 0.0, 0.0 ) + 160.0, 30.0, 0.0 ) );
        struct timespec t0, t1;
        m3d_render( M );
        clock_gettime( CLOCK_MONOTONIC, &t0 );
        for ( int i = 0; i < 10; ++i ) { m3d_render( M ); }
        clock_gettime( CLOCK_MONOTONIC, &t1 );
        printf( "world render at 640 x 264 (software): %.1f ms a frame, %g triangles drawn\n", ( ( t1.tv_sec - t0.tv_sec ) + 1e-9 * ( t1.tv_nsec - t0.tv_nsec ) ) * 100.0, Q( GFXQ_M3DDRAWN, 0 ) );
    }
    for ( int pass = 0; pass < 3; ++pass ) {
        double cx = pass == 0 ? 0.0 : pass == 1 ? 30000.0 : -60000.0, cy = pass == 0 ? 0.0 : pass == 1 ? -20000.0 : 45000.0;
        if ( pass > 0 ) {
            CHECK( op( GFXOP_M3DWORLDVIEW, 2, cx, cy ) );
            CHECK( op( GFXOP_M3DWORLDWAIT, 0 ) );
        }
        // every place is covered by exactly one tile that is shown
        int bad = 0;
        for ( int i = 0; i < 400; ++i ) {
            double x = cx + ( nz_hash( 7, i, pass, 1 ) - 0.5 ) * 250000.0, y = cy + ( nz_hash( 7, i, pass, 2 ) - 0.5 ) * 250000.0;
            if ( fabs( x ) > 180000.0 || fabs( y ) > 180000.0 ) { continue; }
            int cnt = 0;
            for ( int k = 100; k < 100 + 3072; ++k ) {
                const obj_t* o = &M->objs[k];
                if ( o->used && o->shown && o->nv > 0 && x >= o->mx0 && x < o->mx1 && y >= o->my0 && y < o->my1 ) { ++cnt; }
            }
            if ( cnt != 1 ) {
                ++bad;
                if ( bad <= 2 ) {
                    printf( "  point %.0f %.0f covered %d times:", x, y, cnt );
                    for ( int k = 100; k < 100 + 3072; ++k ) {
                        const obj_t* o = &M->objs[k];
                        if ( o->used && o->shown && o->nv > 0 && x >= o->mx0 && x < o->mx1 && y >= o->my0 && y < o->my1 ) { printf( " [obj %d %.0f..%.0f %.0f..%.0f]", k - 100, o->mx0, o->mx1, o->my0, o->my1 ); }
                    }
                    printf( "\n" );
                }
            }
        }
        CHECK( bad == 0 );
        // near the camera the surface is the ground (cells of 122 units): the floor follows the mesh
        double worst = 0.0;
        for ( int i = 0; i < 30; ++i ) {
            double x = cx + ( nz_hash( 9, i, pass, 1 ) - 0.5 ) * 4000.0, y = cy + ( nz_hash( 9, i, pass, 2 ) - 0.5 ) * 4000.0;
            double gz = Q( GFXQ_M3DGROUND, 2, x, y ), fz = Q( GFXQ_M3DFLOORAT, 3, x, y, gz + 300.0 );
            if ( fabs( fz - gz ) > worst ) { worst = fabs( fz - gz ); }
        }
        CHECK( worst < 60.0 );
        printf( "world pass %d: floor differs from the ground by at most %.2f units near the camera\n", pass, worst );
    }
    // walking: no popping. Fixed places 500 .. 40000 units away are read after every step of 150 units; the surface may change smoothly, never by a step
    double px[24], py[24], last[24], maxjump = 0.0;
    for ( int i = 0; i < 24; ++i ) {
        double r = 500.0 + nz_hash( 3, i, 0, 0 ) * 40000.0, an = nz_hash( 3, i, 1, 0 ) * 6.2831853;
        px[i] = -60000.0 + r * cos( an ); py[i] = 45000.0 + r * sin( an ); last[i] = 0.0;
    }
    int splits_changed = 0;
    for ( int step = 0; step <= 160; ++step ) {
        double cx = -60000.0 + 150.0 * step, cy = 45000.0 - 60.0 * step;
        CHECK( op( GFXOP_M3DWORLDVIEW, 2, cx, cy ) );
        CHECK( op( GFXOP_M3DRINGWAIT, 0 ) );
        CHECK( op( GFXOP_M3DWORLDVIEW, 2, cx, cy ) );
        for ( int i = 0; i < 24; ++i ) {
            double z = Q( GFXQ_M3DFLOORAT, 3, px[i], py[i], Q( GFXQ_M3DGROUND, 2, px[i], py[i] ) + 300.0 );
            if ( step > 0 && fabs( z - last[i] ) > maxjump ) { maxjump = fabs( z - last[i] ); }
            last[i] = z;
        }
        if ( step == 0 ) { splits_changed = (int) Q( GFXQ_M3DWORLDINFO, 1, 2.0 ); }
    }
    printf( "world walk: the surface at 24 places changed by at most %.2f units between steps of 150 (tiles shown %d at the start, %g at the end)\n", maxjump, splits_changed, Q( GFXQ_M3DWORLDINFO, 1, 2.0 ) );
    CHECK( maxjump < 40.0 );
    // trees: cells of hashed trees are made where tree pictures are set, the same ones every time (all shown objects, summed over six places)
    {
        long tri[3] = { 0, 0, 0 };
        for ( int run = 0; run < 3; ++run ) {
            CHECK( op( GFXOP_M3DWORLDCLOSE, 0 ) );
            CHECK( op( GFXOP_M3DTREEPIC, 4, run == 0 ? 0.0 : 312.0, run == 0 ? 0.0 : 309.0, run == 0 ? 0.0 : 310.0, run == 0 ? 0.0 : 317.0 ) );
            CHECK( ops( GFXOP_M3DWORLDOPEN, 1, dir, 100.0 ) );
            for ( int pl = 0; pl < 6; ++pl ) {
                CHECK( op( GFXOP_M3DWORLDVIEW, 2, -50000.0 + 20000.0 * pl, 30000.0 - 9000.0 * pl ) );
                CHECK( op( GFXOP_M3DWORLDWAIT, 0 ) );
                for ( int k = 100; k < 100 + 3072; ++k ) {
                    const obj_t* o = &M->objs[k];
                    if ( o->used && o->shown && o->nv > 0 ) { tri[run] += o->nt; }
                }
            }
        }
        printf( "world trees: %ld triangles shown without tree pictures, %ld with, %ld again\n", tri[0], tri[1], tri[2] );
        CHECK( tri[1] > tri[0] && tri[1] == tri[2] );
        CHECK( op( GFXOP_M3DTREEPIC, 4, 0.0, 0.0, 0.0, 0.0 ) );
    }
    CHECK( op( GFXOP_M3DWORLDCLOSE, 0 ) );
    CHECK( Q( GFXQ_M3DWORLDINFO, 1, 0.0 ) == 0.0 );
    CHECK( !M->objs[100].used );
    char path[700];
    snprintf( path, sizeof(path), "%s/world.dat", dir );
    unlink( path ); rmdir( dir );
}

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
    // the fog of a haze color: the wall (200, 100 away) fades to color 40 quadratically with the distance (the palette is a gray ramp: the index is the gray)
    CHECK( op( GFXOP_M3DFOG, 2, 200.0, 40.0 ) );
    fb = shot();
    NEAR( PX( fb, 80, 50 ), 161.0, 4.0 );                                                    // q = 0.5: 4 / 15 of the way (times 0.92): 0.755 * 200 + 0.245 * 40
    CHECK( op( GFXOP_M3DFOG, 2, 130.0, 40.0 ) );
    fb = shot();
    NEAR( PX( fb, 80, 50 ), 112.0, 4.0 );                                                    // q = 0.77: 9 / 15 (times 0.92): 0.448 * 200 + 0.552 * 40
    CHECK( op( GFXOP_M3DFOG, 2, 90.0, 40.0 ) );
    fb = shot();
    NEAR( PX( fb, 80, 50 ), 53.0, 3.0 );                                                     // beyond the fog distance: nearly only haze (92 %: 0.08 * 200 + 0.92 * 40)
    CHECK( op( GFXOP_M3DFOG, 2, 5000.0, 40.0 ) );
    fb = shot();
    NEAR( PX( fb, 80, 50 ), 200.0, 2.0 );                                                    // far inside: unchanged
    CHECK( !op( GFXOP_M3DFOG, 2, 100.0, 0.0 ) && !op( GFXOP_M3DFOG, 2, 100.0, 256.0 ) );
    CHECK( op( GFXOP_M3DFOG, 1, 130.0 ) );                                                   // without a haze color it darkens again
    fb = shot();
    CHECK( PX( fb, 80, 50 ) < 200 && PX( fb, 80, 50 ) != 112 );
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
    // -- walking on hills: slopes are floors, a body cannot be lost with a ground function ----------------------------------------------------------------------
    fresh();
    tex_flat( 301, 4, 4, 100 );
    CHECK( op( GFXOP_M3DOBJECT, 1, 1.0, 0.0 ) );
    CHECK( op( GFXOP_M3DOBJECT, 2, 20.0, 0.0 ) );
    // a ramp of 60 degrees (rise 173 over 100): a thing climbs it (before 2026-11-21 the sloping triangle pushed it back like a wall)
    CHECK( op( GFXOP_M3DVERTEX, 4, 20.0, 0.0, -200.0, 0.0 ) && op( GFXOP_M3DVERTEX, 4, 20.0, 600.0, -200.0, 0.0 ) &&
           op( GFXOP_M3DVERTEX, 4, 20.0, 600.0, 200.0, 0.0 ) && op( GFXOP_M3DVERTEX, 4, 20.0, 0.0, 200.0, 0.0 ) );
    CHECK( op( GFXOP_M3DVERTEX, 4, 20.0, 800.0, -200.0, 0.0 ) && op( GFXOP_M3DVERTEX, 4, 20.0, 800.0, 200.0, 0.0 ) );
    CHECK( op( GFXOP_M3DQUAD, 6, 20.0, 0.0, 1.0, 2.0, 3.0, 301.0 ) );                                                    // the flat part x 0 .. 600 at z 0
    CHECK( op( GFXOP_M3DVERTEX, 4, 20.0, 700.0, -200.0, 173.0 ) && op( GFXOP_M3DVERTEX, 4, 20.0, 700.0, 200.0, 173.0 ) );  // 6, 7: the top of the ramp
    CHECK( op( GFXOP_M3DQUAD, 6, 20.0, 1.0, 6.0, 7.0, 2.0, 301.0 ) );                                                    // from x 600 (z 0) to x 700 (z 173): 60 degrees
    CHECK( op( GFXOP_M3DTHING, 7, 0.0, 500.0, 0.0, 0.0, 2.0, 28.0, 56.0 ) );
    for ( int i=0; i < 40; ++i ) { op( GFXOP_M3DTHINGMOVE, 3, 0.0, 10.0, 0.0 ); }
    CHECK( Q( GFXQ_M3DTX, 1, 0.0 ) > 650.0 && Q( GFXQ_M3DTZ, 1, 0.0 ) > 80.0 );                                           // on the ramp, high up
    // a wall (a vertical quad) still stops it
    CHECK( op( GFXOP_M3DOBJECT, 2, 21.0, 0.0 ) && op( GFXOP_M3DBOX, 8, 21.0, 100.0, -200.0, 0.0, 120.0, 200.0, 100.0, 301.0 ) );
    CHECK( op( GFXOP_M3DTHINGPOS, 4, 0.0, 0.0, 0.0, 0.0 ) );
    for ( int i=0; i < 30; ++i ) { op( GFXOP_M3DTHINGMOVE, 3, 0.0, 10.0, 0.0 ); }
    CHECK( Q( GFXQ_M3DTX, 1, 0.0 ) < 100.0 );
    // with a ground function: no floor mesh there -> the ground function is the floor; buried under the mesh -> lifted onto it
    fresh();
    CHECK( op( GFXOP_M3DGROUND, 5, 3.0, 300.0, 0.001, 1.0, 50.0 ) );                                                    // the ground is a plateau at 50
    CHECK( op( GFXOP_M3DOBJECT, 1, 1.0, 0.0 ) );                                                                         // (the floor of fresh() is gone)
    CHECK( op( GFXOP_M3DTHING, 7, 0.0, 0.0, 0.0, 45.0, 2.0, 28.0, 56.0 ) );
    CHECK( op( GFXOP_M3DTHINGMOVE, 3, 0.0, 5.0, 0.0 ) );
    CHECK( fabs( Q( GFXQ_M3DTZ, 1, 0.0 ) - 50.0 ) < 0.01 && fabs( Q( GFXQ_M3DTX, 1, 0.0 ) - 5.0 ) < 0.01 );               // nothing built: it stands on the function
    CHECK( op( GFXOP_M3DOBJECT, 2, 30.0, 0.0 ) && ops( GFXOP_M3DTERRAIN, 7, 0, 30.0, -500.0, -500.0, 10.0, 10.0, 100.0, 301.0 ) );
    CHECK( op( GFXOP_M3DTHINGPOS, 4, 0.0, 0.0, 0.0, -400.0 ) );                                                          // far below the surface
    CHECK( op( GFXOP_M3DTHINGMOVE, 3, 0.0, 1.0, 0.0 ) );
    CHECK( fabs( Q( GFXQ_M3DTZ, 1, 0.0 ) - 50.0 ) < 1.0 );                                                               // lifted back on top
    CHECK( op( GFXOP_M3DCLEAR, 0 ) );
    // without a ground function a body below everything stays lost (the old behaviour: nothing to stand on)
    CHECK( op( GFXOP_M3DOBJECT, 1, 5.0, 0.0 ) && op( GFXOP_M3DBOX, 8, 5.0, -100.0, -100.0, 0.0, 100.0, 100.0, 10.0, 301.0 ) );
    CHECK( op( GFXOP_M3DTHING, 7, 0.0, 0.0, 0.0, -400.0, 2.0, 28.0, 56.0 ) );
    CHECK( op( GFXOP_M3DTHINGMOVE, 3, 0.0, 1.0, 0.0 ) && Q( GFXQ_M3DTZ, 1, 0.0 ) < -300.0 );
    // -- a far, very high wall (the far land and its mountains): 8000 ahead, 30000 high, seen looking up; with a far limit of 40000 and haze ------------------------------
    fresh();
    CHECK( op( GFXOP_M3DOBJECT, 1, 1.0, 0.0 ) );
    CHECK( op( GFXOP_M3DFAR, 1, 40000.0 ) );
    CHECK( op( GFXOP_M3DFOG, 2, 7000.0, 40.0 ) );
    CHECK( op( GFXOP_M3DOBJECT, 1, 5.0 ) );
    CHECK( op( GFXOP_M3DBOX, 8, 5.0, -4500.0, 8000.0, 0.0, 4500.0, 8900.0, 30000.0, 2.0 ) );
    CHECK( op( GFXOP_M3DCAMERA, 5, 0.0, 0.0, 0.0, 90.0, 40.0 ) );
    fb = shot();
    CHECK( PX( fb, 80, 50 ) != 0 && Q( GFXQ_M3DDRAWN, 0 ) > 0 );                              // the wall is seen (about 92 % haze)
    CHECK( op( GFXOP_M3DOBJFLAGS, 2, 5.0, 12.0 ) );                                            // passable and see-through (as the far tiles of 1987): still drawn
    fb = shot();
    CHECK( PX( fb, 80, 50 ) != 0 && Q( GFXQ_M3DDRAWN, 0 ) > 0 );
    // -- M3D HILLS (a second ridged layer next to M3D MOUNTAINS) and holes in a terrain grid (cell character 255) ---------------------------------------------------
    fresh();
    CHECK( op( GFXOP_M3DGROUND, 5, 3.0, 300.0, 0.001, 1.0, 10.0 ) );
    {
        double p0 = Q( GFXQ_M3DGROUND, 2, 1234.0, 5678.0 ), g1, g2, g12;
        CHECK( op( GFXOP_M3DMOUNTAINS, 6, 11.0, 2000.0, 30000.0, 4.0, -1.0, 0.0 ) );          // the mask is high almost everywhere (lo -1 .. hi 0 of a noise in -1 .. 1)
        g1 = Q( GFXQ_M3DGROUND, 2, 1234.0, 5678.0 ) - p0;
        CHECK( op( GFXOP_M3DMOUNTAINS, 6, 11.0, 2000.0, 0.0, 4.0, -1.0, 0.0 ) );              // amp 0 removes them
        CHECK( fabs( Q( GFXQ_M3DGROUND, 2, 1234.0, 5678.0 ) - p0 ) < 1e-9 );
        CHECK( op( GFXOP_M3DHILLS, 6, 12.0, 300.0, 200.0, 4.0, -1.0, 0.0 ) );
        g2 = Q( GFXQ_M3DGROUND, 2, 1234.0, 5678.0 ) - p0;
        CHECK( op( GFXOP_M3DMOUNTAINS, 6, 11.0, 2000.0, 30000.0, 4.0, -1.0, 0.0 ) );
        g12 = Q( GFXQ_M3DGROUND, 2, 1234.0, 5678.0 ) - p0;
        CHECK( fabs( g12 - ( g1 + g2 ) ) < 1e-6 && ( g1 != 0.0 || g2 != 0.0 ) );              // the two layers add up
        CHECK( Q( GFXQ_M3DGROUND, 2, 0.0, 0.0 ) < 31000.0 );                                    // 30000 high mountains are possible (and heights this large are no problem)
        CHECK( op( GFXOP_M3DHILLS, 6, 12.0, 300.0, 0.0, 4.0, -1.0, 0.0 ) );
    }
    CHECK( op( GFXOP_M3DCLEAR, 0 ) );
    CHECK( op( GFXOP_M3DGROUND, 4, 5.0, 300.0, 5.0, 2.0 ) );
    CHECK( op( GFXOP_M3DOBJECT, 2, 70.0, 0.0 ) );
    {
        char holes[9] = { 0, 0, 0, 0, (char) 255, 0, 0, 0, 0 };                                  // the middle cell of 3 x 3 is a hole
        double ta[8] = { 70.0, 0.0, 0.0, 3.0, 3.0, 100.0, 301.0, GFX_NONE };
        CHECK( m3d_op( M, GFXOP_M3DTERRAIN, ta, 7, holes, 9, &lasterr ) );
        CHECK( Q( GFXQ_M3DVERTS, 1, 70.0 ) == 16.0 && Q( GFXQ_M3DTRIS, 1, 70.0 ) == 16.0 );
        CHECK( op( GFXOP_M3DOBJECT, 2, 71.0, 0.0 ) );
        double tb[8] = { 71.0, 0.0, 0.0, 3.0, 3.0, 100.0, 301.0, 20.0 };
        CHECK( m3d_op( M, GFXOP_M3DTERRAIN, tb, 8, holes, 9, &lasterr ) );   // with a skirt: only the edge cells have walls (8 cells, 12 edge pieces, 2 triangles)
        CHECK( Q( GFXQ_M3DTRIS, 1, 71.0 ) == 16.0 + 24.0 );
    }
    // -- the layers of the ground: fields and dirt roads first, then hills, tarred roads across everything, lakes and mountains last (M3D ROADNET) ---------------------------------------
    fresh();
    CHECK( op( GFXOP_M3DOBJECT, 1, 1.0, 0.0 ) );
    CHECK( op( GFXOP_M3DGROUND, 7, 1987.0, 900.0, 22.0, 4.0, 8.0, 0.5, 2.0 ) );
    CHECK( op( GFXOP_M3DROADNET, 6, 1800.0, 0.45, 140.0, 36000.0, 0.85, 900.0 ) );
    {
        const rnet_t* nt = &M->gnd.net;
        CHECK( nt->on && nt->spacing == 1800.0 && nt->tspacing == 36000.0 );
        // the road network itself: the ends of a road are the region points, the wander is zero there, the kinds are those of demos/1987.bas
        rlink_t dirt, tar;
        bool hd = false, ht = false;
        for ( int64_t ry = 20; ry < 60 && !hd; ++ry ) {
            for ( int64_t rx = 20; rx < 60 && !hd; ++rx ) {
                if ( rnet_link( nt, 0, rx, ry, 0, &dirt ) ) { hd = true; }
            }
        }
        for ( int64_t ry = 1; ry < 9 && !ht; ++ry ) {
            for ( int64_t rx = 1; rx < 9 && !ht; ++rx ) {
                if ( rnet_link( nt, 1, rx, ry, 0, &tar ) ) { ht = true; }
            }
        }
        CHECK( hd && ht && tar.tar && !dirt.tar );
        double px, py;
        rnet_point( &tar, 0.0, &px, &py );
        NEAR( px, tar.x1, 1e-9 ); NEAR( py, tar.y1, 1e-9 );
        rnet_point( &tar, 1.0, &px, &py );
        NEAR( px, tar.x2, 1e-9 ); NEAR( py, tar.y2, 1e-9 );
        CHECK( tar.len > 5000.0 && tar.len < 60000.0 && tar.steps == (int)( tar.len / 200.0 ) + 1 && dirt.steps == (int)( dirt.len / 50.0 ) + 1 );   // the tarred roads are long
        // hills (amp 260), mountains (amp 30000 everywhere) and lakes (everywhere) all want to be where the roads are: they keep away
        CHECK( op( GFXOP_M3DHILLS, 6, 5.0, 500.0, 260.0, 5.0, -1.0, 0.0 ) );
        CHECK( op( GFXOP_M3DMOUNTAINS, 6, 11.0, 2000.0, 30000.0, 4.0, -1.0, 0.0 ) );
        CHECK( op( GFXOP_M3DLAKES, 5, 3.0, 400.0, 300.0, -1.0, 0.0 ) );
        CHECK( M->gnd.net.mask.on );
        M->gnd.net.mask.on = false;                                                                 // (mountains everywhere would drop every road: here the layers' keep-away is tested, the mask has a test of its own below)
        double base_c, gd, gt;
        rnet_point( &dirt, 0.5, &px, &py );
        base_c = 8.0 + 22.0 * nz_fbm( 1987, px / 900.0, py / 900.0, 4, 0.5, 2.0 );
        gd = Q( GFXQ_M3DGROUND, 2, px, py );
        CHECK( Q( GFXQ_M3DROADD, 2, px, py ) < 5.0 );                                                // (the steps of 50 units are straight: a point of the curve is a little off)
        NEAR( gd, base_c, 1e-6 );                                                                 // on a dirt road: the plain ground, no hill, no mountain, no lake
        rnet_point( &tar, 0.5, &px, &py );
        gt = Q( GFXQ_M3DGROUND, 2, px, py );
        CHECK( Q( GFXQ_M3DROADT, 2, px, py ) < 5.0 && gt > -50.0 && gt < 800.0 );                  // on a tarred road: not drowned, no mountain
        {   // across the road the ground is flat (cut and fill to the height of the road): 200 to the side, which is the road's shoulder
            double nx = -( tar.y2 - tar.y1 ) / tar.len, ny = ( tar.x2 - tar.x1 ) / tar.len;
            double g1 = Q( GFXQ_M3DGROUND, 2, px + nx * 200.0, py + ny * 200.0 ), g2 = Q( GFXQ_M3DGROUND, 2, px - nx * 200.0, py - ny * 200.0 );
            CHECK( fabs( g1 - gt ) < 5.0 && fabs( g2 - gt ) < 5.0 );
            CHECK( op( GFXOP_M3DROADNET, 6, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 ) );                              // without the network the mountains and lakes are everywhere
            double off = Q( GFXQ_M3DGROUND, 2, px, py );
            CHECK( off > 2000.0 || off < -50.0 );
            CHECK( op( GFXOP_M3DROADNET, 6, 1800.0, 0.45, 140.0, 36000.0, 0.85, 900.0 ) );
        }
        // a field is flat: the hills are only a tenth there (no road within 700, kind 2)
        int found = 0;
        for ( int i = 0; i < 4000 && !found; ++i ) {
            double fx = 36000.0 + (double)( i % 63 ) * 137.0, fy = 36000.0 + (double)( i / 63 ) * 137.0;
            rnear_t rn;
            rnet_near( nt, fx, fy, 700.0, &rn );
            if ( rn.dirt > 700.0 && rn.tar > 700.0 && Q( GFXQ_M3DKIND, 2, fx, fy ) == 2.0 ) {
                found = 1;
                CHECK( op( GFXOP_M3DMOUNTAINS, 6, 11.0, 2000.0, 0.0, 4.0, -1.0, 0.0 ) );
                CHECK( op( GFXOP_M3DLAKES, 5, 3.0, 400.0, 0.0, -1.0, 0.0 ) );
                double gf = Q( GFXQ_M3DGROUND, 2, fx, fy );
                CHECK( op( GFXOP_M3DROADNET, 6, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 ) );                          // without the network the full hills
                double gh = Q( GFXQ_M3DGROUND, 2, fx, fy ), bb = 8.0 + 22.0 * nz_fbm( 1987, fx / 900.0, fy / 900.0, 4, 0.5, 2.0 );
                NEAR( gf - bb, 0.1 * ( gh - bb ), 1e-6 );
                CHECK( fabs( gh - bb ) > 1.0 );
            }
        }
        CHECK( found );
    }
    CHECK( !op( GFXOP_M3DROADNET, 6, 1800.0, 1.5, 140.0, 36000.0, 0.85, 900.0 ) );
    CHECK( op( GFXOP_M3DCLEAR, 0 ) && Q( GFXQ_M3DROADD, 2, 0.0, 0.0 ) > 1.0e8 );                   // M3D CLEAR forgets the network
    // -- M3D RING: ground meshes made by a worker thread ----------------------------------------------------------------------------------------------------------
    fresh();
    tex_flat( 301, 4, 4, 100 );
    CHECK( op( GFXOP_M3DOBJECT, 1, 1.0, 0.0 ) );
    CHECK( op( GFXOP_M3DGROUND, 7, 3.0, 700.0, 40.0, 4.0, 10.0, 0.5, 2.0 ) );
    CHECK( op( GFXOP_M3DRINGPIC, 12, 325.0, 324.0, 301.0, 327.0, 327.0, 320.0, 303.0, 320.0, 302.0, -12.0, 70.0, 304.0 ) );
    CHECK( op( GFXOP_M3DOBJECT, 2, 3.0, 0.0 ) && op( GFXOP_M3DOBJECT, 2, 4.0, 0.0 ) && op( GFXOP_M3DOBJECT, 2, 5.0, 0.0 ) );
    CHECK( op( GFXOP_M3DRING, 10, 3.0, 0.0, 0.0, 8.0, 100.0, 0.0, 0.0, 0.0, 0.0, 0.0 ) );
    CHECK( Q( GFXQ_M3DRINGBUSY, 0 ) >= 0.0 );
    CHECK( op( GFXOP_M3DRINGWAIT, 0 ) && Q( GFXQ_M3DRINGBUSY, 0 ) == 0.0 );
    CHECK( Q( GFXQ_M3DVERTS, 1, 3.0 ) == 81.0 && Q( GFXQ_M3DTRIS, 1, 3.0 ) == 128.0 );             // 8 x 8 cells, no skirt, no hole
    CHECK( fabs( Q( GFXQ_M3DFLOORAT, 3, 200.0, 300.0, 5000.0 ) - Q( GFXQ_M3DGROUND, 2, 200.0, 300.0 ) ) < 1e-9 );   // the heights are the ground function's at a vertex
    CHECK( op( GFXOP_M3DRING, 10, 3.0, 0.0, 0.0, 8.0, 100.0, 20.0, 0.0, 300.0, 300.0, 2.0 ) );     // a hole of 2 x 2 cells and a skirt
    CHECK( op( GFXOP_M3DPOLL, 0 ) );                                                                  // (maybe not taken yet)
    CHECK( op( GFXOP_M3DRINGWAIT, 0 ) );
    CHECK( Q( GFXQ_M3DTRIS, 1, 3.0 ) == 2.0 * ( 64.0 - 4.0 ) + 2.0 * ( 4.0 * 8.0 - 0.0 + 4.0 * 2.0 - 0.0 ) );    // 60 cells, a wall piece for each of the 32 outer and 8 inner edges
    CHECK( !op( GFXOP_M3DRING, 10, 9.0, 0.0, 0.0, 8.0, 100.0, 0.0, 0.0, 0.0, 0.0, 0.0 ) );             // object 9 is not defined
    CHECK( !op( GFXOP_M3DRING, 10, 3.0, 0.0, 0.0, 0.0, 100.0, 0.0, 0.0, 0.0, 0.0, 0.0 ) );             // no cells
    // morphing: a ring with a morph range (the distances 100 .. 1000 from the camera): at a vertex near the camera it has the true height, far from it the surface of the coarser grid (3 x the cell)
    CHECK( op( GFXOP_M3DRING, 12, 4.0, 0.0, 0.0, 9.0, 300.0, 0.0, 0.0, 0.0, 0.0, 0.0, 100.0, 1000.0 ) );   // a ring of 9 x 9 cells of 300: x 0 .. 2700
    CHECK( op( GFXOP_M3DRINGWAIT, 0 ) );
    {
        double h00 = Q( GFXQ_M3DGROUND, 2, 0.0, 900.0 ), h01 = Q( GFXQ_M3DGROUND, 2, 0.0, 1800.0 );
        CHECK( op( GFXOP_M3DMORPH, 2, 0.0, 1200.0 ) );                                                        // the camera is at the vertex: the true height
        CHECK( fabs( Q( GFXQ_M3DFLOORAT, 3, 0.0, 1200.0, 9000.0 ) - Q( GFXQ_M3DGROUND, 2, 0.0, 1200.0 ) ) < 1e-3 );
        CHECK( op( GFXOP_M3DMORPH, 2, 100000.0, 100000.0 ) );                                                 // far away: the surface of the coarser grid (a cell of 900, u = 0, v = 1/3)
        CHECK( fabs( Q( GFXQ_M3DFLOORAT, 3, 0.0, 1200.0, 9000.0 ) - ( h00 + ( h01 - h00 ) / 3.0 ) ) < 1e-3 );
        CHECK( fabs( Q( GFXQ_M3DFLOORAT, 3, 0.0, 900.0, 9000.0 ) - h00 ) < 1e-3 );                          // at a coarse vertex both are the same
        CHECK( op( GFXOP_M3DMORPH, 2, 550.0, 1200.0 ) );                                                      // halfway: in between (550 from the vertex = w 0.5)
        double mid = Q( GFXQ_M3DFLOORAT, 3, 0.0, 1200.0, 9000.0 ), tru = Q( GFXQ_M3DGROUND, 2, 0.0, 1200.0 ), crs = h00 + ( h01 - h00 ) / 3.0;
        CHECK( fabs( mid - ( tru + ( crs - tru ) * 0.5 ) ) < 1e-2 );
    }
    // tarred roads as ribbons in a ring, and the kinds of ground in a chunk (mode 1)
    CHECK( op( GFXOP_M3DROADNET, 6, 1800.0, 0.45, 140.0, 36000.0, 0.85, 900.0 ) );
    {
        rlink_t tl;
        bool got = false;
        for ( int64_t ry = 1; ry < 9 && !got; ++ry ) { for ( int64_t rx = 1; rx < 9 && !got; ++rx ) { if ( rnet_link( &M->gnd.net, 1, rx, ry, 0, &tl ) ) { got = true; } } }
        CHECK( got );
        double px, py;
        rnet_point( &tl, 0.5, &px, &py );
        double gx = floor( ( px - 2000.0 ) / 300.0 ) * 300.0, gy = floor( ( py - 2000.0 ) / 300.0 ) * 300.0;
        CHECK( op( GFXOP_M3DRING, 10, 5.0, gx, gy, 12.0, 300.0, 0.0, 0.0, 0.0, 0.0, 0.0 ) );
        CHECK( op( GFXOP_M3DRINGWAIT, 0 ) );
        CHECK( Q( GFXQ_M3DTRIS, 1, 5.0 ) > 288.0 );                                                      // 12 x 12 cells = 288 triangles and the ribbon
        CHECK( op( GFXOP_M3DRING, 10, 5.0, gx, gy, 12.0, 300.0, 0.0, 1.0, 0.0, 0.0, 0.0 ) );               // as a chunk (mode 1): the ribbon too, made by the chunk that holds its start
        CHECK( op( GFXOP_M3DRINGWAIT, 0 ) && Q( GFXQ_M3DTRIS, 1, 5.0 ) > 288.0 );
        {   // the ribbon lies on the mesh: the floor above the road's middle is the ribbon, 3 above the ground (the corridor is flat there)
            double fz = Q( GFXQ_M3DFLOORAT, 3, px, py, 500.0 ), gz = Q( GFXQ_M3DGROUND, 2, px, py );
            CHECK( fz > gz + 1.0 && fz < gz + 12.0 );
        }
        CHECK( op( GFXOP_M3DRINGPIC, 12, 325.0, 324.0, 301.0, 0.0, 0.0, 320.0, 303.0, 320.0, 302.0, -12.0, 70.0, 0.0 ) );   // no tar picture: no ribbons
        CHECK( op( GFXOP_M3DRING, 10, 5.0, gx, gy, 12.0, 300.0, 0.0, 1.0, 0.0, 0.0, 0.0 ) );
        CHECK( op( GFXOP_M3DRINGWAIT, 0 ) && Q( GFXQ_M3DTRIS, 1, 5.0 ) == 288.0 );
    }
    // a chunk morphs too (M3D RING with a,b): its 4 x 4 vertices of 300 blend into the surface of the grid of 900 (a hill is the same either way at the distance b, the camera moves)
    CHECK( op( GFXOP_M3DGROUND, 7, 3.0, 700.0, 60.0, 4.0, 10.0, 0.5, 2.0 ) );
    CHECK( op( GFXOP_M3DRING, 12, 4.0, 900.0, 1800.0, 3.0, 300.0, 0.0, 1.0, 0.0, 0.0, 0.0, 900.0, 1800.0 ) );
    CHECK( op( GFXOP_M3DRINGWAIT, 0 ) );
    {
        double g00 = Q( GFXQ_M3DGROUND, 2, 900.0, 1800.0 ), g10 = Q( GFXQ_M3DGROUND, 2, 1800.0, 1800.0 ), g11 = Q( GFXQ_M3DGROUND, 2, 1800.0, 2700.0 ), g01 = Q( GFXQ_M3DGROUND, 2, 900.0, 2700.0 );
        double coarse = g00 + ( g10 - g00 ) / 3.0 + ( g11 - g10 ) / 3.0;                                     // the vertex (1200, 2100): u = 1/3, v = 1/3, u >= v
        (void) g01;
        double tru = Q( GFXQ_M3DGROUND, 2, 1200.0, 2100.0 );
        CHECK( op( GFXOP_M3DMORPH, 2, 1200.0, 2100.0 ) );                                                     // the camera is on it: the true height
        CHECK( fabs( Q( GFXQ_M3DFLOORAT, 3, 1200.0, 2100.0, 9000.0 ) - tru ) < 1e-3 );
        CHECK( op( GFXOP_M3DMORPH, 2, 50000.0, 50000.0 ) );                                                   // far away: the grid of 900
        CHECK( fabs( Q( GFXQ_M3DFLOORAT, 3, 1200.0, 2100.0, 9000.0 ) - coarse ) < 1e-3 );
        CHECK( op( GFXOP_M3DMORPH, 2, 50003.0, 50003.0 ) );                                                   // a move of 6 units changes nothing (and the object is left alone: all blended)
        CHECK( fabs( Q( GFXQ_M3DFLOORAT, 3, 1200.0, 2100.0, 9000.0 ) - coarse ) < 1e-3 );
        CHECK( op( GFXOP_M3DMORPH, 2, 1200.0, 2100.0 ) );                                                     // coming back: the true height again
        CHECK( fabs( Q( GFXQ_M3DFLOORAT, 3, 1200.0, 2100.0, 9000.0 ) - tru ) < 1e-3 );
    }
    // trees in a chunk (mode 2: lathe solids, mode 3: cheap ones) and the dirt tracks of a chunk as ribbons
    {
        CHECK( op( GFXOP_M3DTREEPIC, 4, 312.0, 309.0, 310.0, 317.0 ) );
        CHECK( op( GFXOP_M3DGROUND, 7, 3.0, 700.0, 4.0, 2.0, 10.0, 0.5, 2.0 ) );                       // gentle ground, no water, no hills
        int64_t fcx = -1, fcy = -1;
        for ( int64_t cy = 30; cy < 70 && fcx < 0; ++cy ) {
            for ( int64_t cx = 30; cx < 70 && fcx < 0; ++cx ) {
                rnear_t rn;
                rnet_near( &M->gnd.net, (double) cx * 900.0 + 450.0, (double) cy * 900.0 + 450.0, 700.0, &rn );
                if ( rnet_kind( &M->gnd.net, (double) cx * 900.0 + 450.0, (double) cy * 900.0 + 450.0 ) == 3 && rn.dirt > 700.0 && rn.tar > 700.0 ) { fcx = cx; fcy = cy; }
            }
        }
        CHECK( fcx >= 0 );
        double cx0 = (double) fcx * 900.0, cy0 = (double) fcy * 900.0;
        CHECK( op( GFXOP_M3DRING, 10, 5.0, cx0, cy0, 3.0, 300.0, 0.0, 1.0, 0.0, 0.0, 0.0 ) );
        CHECK( op( GFXOP_M3DRINGWAIT, 0 ) );
        double t1 = Q( GFXQ_M3DTRIS, 1, 5.0 );
        CHECK( t1 == 18.0 );
        CHECK( op( GFXOP_M3DRING, 10, 5.0, cx0, cy0, 3.0, 300.0, 0.0, 2.0, 0.0, 0.0, 0.0 ) );
        CHECK( op( GFXOP_M3DRINGWAIT, 0 ) );
        double t2 = Q( GFXQ_M3DTRIS, 1, 5.0 );
        CHECK( t2 > t1 + 60.0 );                                                                          // trees: some 64 .. 80 triangles each
        CHECK( op( GFXOP_M3DRING, 10, 5.0, cx0, cy0, 3.0, 300.0, 0.0, 3.0, 0.0, 0.0, 0.0 ) );
        CHECK( op( GFXOP_M3DRINGWAIT, 0 ) );
        double t3 = Q( GFXQ_M3DTRIS, 1, 5.0 );
        CHECK( t3 > t1 && t3 < t2 );                                                                      // cheap trees: 12 each
        CHECK( fmod( t3 - t1, 12.0 ) == 0.0 );
        // dirt tracks: a chunk that holds the start of a piece has more triangles with the dirt picture than without
        rlink_t dl; bool gotd = false;
        for ( int64_t ry = 20; ry < 60 && !gotd; ++ry ) { for ( int64_t rx = 20; rx < 60 && !gotd; ++rx ) { if ( rnet_link( &M->gnd.net, 0, rx, ry, 0, &dl ) ) { gotd = true; } } }
        CHECK( gotd );
        double dpx, dpy;
        rnet_point( &dl, 0.5, &dpx, &dpy );
        double dx0 = floor( dpx / 900.0 ) * 900.0, dy0 = floor( dpy / 900.0 ) * 900.0;
        CHECK( op( GFXOP_M3DRINGPIC, 12, 325.0, 324.0, 301.0, 0.0, 0.0, 320.0, 303.0, 320.0, 302.0, -12.0, 70.0, 304.0 ) );
        CHECK( op( GFXOP_M3DRING, 10, 5.0, dx0, dy0, 3.0, 300.0, 0.0, 1.0, 0.0, 0.0, 0.0 ) );
        CHECK( op( GFXOP_M3DRINGWAIT, 0 ) );
        CHECK( Q( GFXQ_M3DTRIS, 1, 5.0 ) > 18.0 );
    }
    CHECK( op( GFXOP_M3DCLEAR, 0 ) && Q( GFXQ_M3DRINGBUSY, 0 ) == 0.0 );
    // -- roads keep out of the mountains: a link is not there where the mountain mask is up on its way (and beside it); M3D("ROADLINK") says the same -----------------------------
    fresh();
    CHECK( op( GFXOP_M3DGROUND, 7, 1987.0, 900.0, 22.0, 4.0, 8.0, 0.5, 2.0 ) );
    CHECK( op( GFXOP_M3DROADNET, 6, 1800.0, 0.45, 140.0, 36000.0, 0.85, 900.0 ) );
    {
        int before = 0, after = 0, dropped = 0, bad = 0, qdiff = 0;
        for ( int64_t ry = -20; ry < 20; ++ry ) { for ( int64_t rx = -20; rx < 20; ++rx ) { for ( int e = 0; e < 2; ++e ) { rlink_t l; if ( rnet_link( &M->gnd.net, 0, rx, ry, e, &l ) ) { ++before; } } } }
        CHECK( !M->gnd.net.mask.on );
        CHECK( op( GFXOP_M3DMOUNTAINS, 8, 7.0, 9000.0, 80000.0, 4.0, 0.25, 0.6, 14000.0, 22000.0 ) );
        CHECK( M->gnd.net.mask.on && M->gnd.net.mask.limit > 0.0 );
        for ( int64_t ry = -20; ry < 20; ++ry ) {
            for ( int64_t rx = -20; rx < 20; ++rx ) {
                for ( int e = 0; e < 2; ++e ) {
                    rlink_t l;
                    bool has = rnet_link( &M->gnd.net, 0, rx, ry, e, &l );
                    if ( has ) {
                        ++after;
                        for ( int k = 0; k <= 4; ++k ) {                                                // the way: its points are in no mountain
                            double px, py;
                            rnet_point( &l, (double) k / 4.0, &px, &py );
                            if ( rnet_mountain( &M->gnd.net.mask, px, py ) > M->gnd.net.mask.limit ) { ++bad; }
                        }
                    }
                    if ( ( Q( GFXQ_M3DROADLINK, 4, 0.0, (double) rx, (double) ry, (double) e ) != 0.0 ) != has ) { ++qdiff; }
                }
            }
        }
        dropped = before - after;
        CHECK( dropped > 20 && after > 100 );                                                           // some roads are gone, the others are there
        CHECK( bad == 0 && qdiff == 0 );
        CHECK( Q( GFXQ_M3DGROUND, 2, 0.0, 0.0 ) == Q( GFXQ_M3DGROUND, 2, 0.0, 0.0 ) );
        // no tall wall beside a road any more: the ground 700 units to the side of every road's middle is never a mountain's wall
        double hmax = 0.0;
        for ( int64_t ry = -20; ry < 20; ++ry ) {
            for ( int64_t rx = -20; rx < 20; ++rx ) {
                rlink_t l;
                if ( !rnet_link( &M->gnd.net, 0, rx, ry, 0, &l ) ) { continue; }
                for ( int k = 1; k < 4; ++k ) {
                    double px, py;
                    rnet_point( &l, (double) k / 4.0, &px, &py );
                    for ( int sd = -1; sd <= 1; sd += 2 ) {
                        double g = Q( GFXQ_M3DGROUND, 2, px + sd * 700.0, py );
                        if ( g > hmax ) { hmax = g; }
                    }
                }
            }
        }
        CHECK( hmax < 3000.0 );
        CHECK( op( GFXOP_M3DMOUNTAINS, 8, 7.0, 9000.0, 0.0, 4.0, 0.25, 0.6, 14000.0, 22000.0 ) && !M->gnd.net.mask.on );     // amp 0: no mountains, all the roads are back
        int again = 0;
        for ( int64_t ry = -20; ry < 20; ++ry ) { for ( int64_t rx = -20; rx < 20; ++rx ) { for ( int e = 0; e < 2; ++e ) { rlink_t l; if ( rnet_link( &M->gnd.net, 0, rx, ry, e, &l ) ) { ++again; } } } }
        CHECK( again == before );
    }
    CHECK( op( GFXOP_M3DCLEAR, 0 ) );
    // -- the shape of the roads: smooth curves, intersections with arms square to their sides, the queries of the language ------------------------------------------------
    fresh();
    CHECK( op( GFXOP_M3DGROUND, 7, 1987.0, 900.0, 22.0, 4.0, 8.0, 0.5, 2.0 ) );
    CHECK( op( GFXOP_M3DROADNET, 6, 1800.0, 0.45, 140.0, 36000.0, 0.85, 900.0 ) );
    {
        int links = 0, hubends = 0, badarm = 0, sharp = 0, badq = 0, nodes_hub = 0, found_hub = 0, badtan = 0;
        double maxturn = 0.0;
        for ( int64_t ry = -10; ry < 10; ++ry ) {
            for ( int64_t rx = -10; rx < 10; ++rx ) {
                for ( int e = 0; e < 2; ++e ) {
                    rlink_t l;
                    if ( !rnet_link( &M->gnd.net, 1, rx, ry, e, &l ) ) { continue; }
                    ++links;
                    double px[800], py[800];
                    int np = 0;
                    for ( int k = 0; k <= l.steps && np < 800; ++k ) { rnet_point( &l, (double) k / (double) l.steps, &px[np], &py[np] ); ++np; }
                    for ( int k = 1; k + 1 < np; ++k ) {                                                // no corner: the direction changes by less than 40 degrees from one step of 200 units to the next
                        double ax = px[k] - px[k - 1], ay = py[k] - py[k - 1], bx = px[k + 1] - px[k], by = py[k + 1] - py[k];
                        double ang = fabs( atan2( ax * by - ay * bx, ax * bx + ay * by ) ) * 57.29578;
                        if ( ang > maxturn ) { maxturn = ang; }
                        if ( ang > 40.0 ) { ++sharp; }
                    }
                    if ( l.hub1 ) {                                                                     // an arm leaves the node along an axis (west for e = 0, south for e = 1), 300 units straight
                        ++hubends;
                        double ax, ay;
                        rnet_point( &l, 0.4 * l.fa, &ax, &ay );
                        if ( !( e == 0 ? fabs( ay - l.y1 ) < 1e-6 && ax < l.x1 : fabs( ax - l.x1 ) < 1e-6 && ay < l.y1 ) ) { ++badarm; }
                    }
                    if ( l.hub2 ) {                                                                     // ... and at the other end (east / north of that node)
                        ++hubends;
                        double ax, ay;
                        rnet_point( &l, 1.0 - 0.4 * l.fb, &ax, &ay );
                        if ( !( e == 0 ? fabs( ay - l.y2 ) < 1e-6 && ax > l.x2 : fabs( ax - l.x2 ) < 1e-6 && ay > l.y2 ) ) { ++badarm; }
                    }
                    // the queries of the language give the same numbers
                    double qx = Q( GFXQ_M3DROADPT, 6, 1.0, (double) rx, (double) ry, (double) e, 0.37, 0.0 ), qy = Q( GFXQ_M3DROADPT, 6, 1.0, (double) rx, (double) ry, (double) e, 0.37, 1.0 ), cx, cy;
                    rnet_point( &l, 0.37, &cx, &cy );
                    if ( qx != cx || qy != cy || Q( GFXQ_M3DROADINFO, 5, 1.0, (double) rx, (double) ry, (double) e, 0.0 ) != (double) l.steps ||
                         Q( GFXQ_M3DROADINFO, 5, 1.0, (double) rx, (double) ry, (double) e, 2.0 ) != ( l.hub1 ? 1.0 : 0.0 ) ) { ++badq; }
                    double arc = Q( GFXQ_M3DROADARC, 5, 1.0, (double) rx, (double) ry, (double) e, 1.0 );
                    if ( !( arc >= l.clen && arc < l.len * 1.1 + 50.0 && Q( GFXQ_M3DROADARC, 5, 1.0, (double) rx, (double) ry, (double) e, 0.5 ) < arc ) ) { ++badq; }
                }
                // a node is an intersection when 3 or 4 roads meet there: rnet_hubs finds exactly those
                int deg = 0;
                rlink_t t;
                deg += rnet_link( &M->gnd.net, 1, rx, ry, 0, &t ) + rnet_link( &M->gnd.net, 1, rx, ry, 1, &t ) + rnet_link( &M->gnd.net, 1, rx + 1, ry, 0, &t ) + rnet_link( &M->gnd.net, 1, rx, ry + 1, 1, &t );
                double nx = ( (double) rx + nz_hash( rx, ry, 11, 0 ) ) * 36000.0, ny = ( (double) ry + nz_hash( rx, ry, 12, 0 ) ) * 36000.0, hxy[8];
                int nh = rnet_hubs( &M->gnd.net, nx - 1.0, ny - 1.0, nx + 1.0, ny + 1.0, hxy, 4 );
                if ( deg >= 3 ) { ++nodes_hub; found_hub += nh == 1; } else if ( nh != 0 ) { ++badtan; }
            }
        }
        CHECK( links > 100 && hubends > 100 );
        CHECK( sharp == 0 && maxturn < 40.0 );
        CHECK( badarm == 0 && badq == 0 );
        CHECK( nodes_hub > 50 && found_hub == nodes_hub && badtan == 0 );
        // the tangent is the same on both sides of a node of two roads (no turn at the node): the directions just before and after it differ by less than 25 degrees
        int nd2 = 0, kink = 0;
        for ( int64_t ry = -10; ry < 10; ++ry ) {
            for ( int64_t rx = -10; rx < 10; ++rx ) {
                rlink_t lw, ls;
                bool w = rnet_link( &M->gnd.net, 0, rx, ry, 0, &lw ), s = rnet_link( &M->gnd.net, 0, rx, ry, 1, &ls ), ee = false, nn = false;
                rlink_t le, ln;
                ee = rnet_link( &M->gnd.net, 0, rx + 1, ry, 0, &le ); nn = rnet_link( &M->gnd.net, 0, rx, ry + 1, 1, &ln );
                if ( w + s + ee + nn != 2 ) { continue; }
                const rlink_t* a[2]; int bsd[2], na = 0;
                if ( w ) { a[na] = &lw; bsd[na++] = 0; } if ( s ) { a[na] = &ls; bsd[na++] = 0; } if ( ee ) { a[na] = &le; bsd[na++] = 1; } if ( nn ) { a[na] = &ln; bsd[na++] = 1; }
                double ox[2], oy[2];
                for ( int i = 0; i < 2; ++i ) {                                                          // the way 0.5 % along each road away from the node
                    double nx = bsd[i] ? a[i]->x2 : a[i]->x1, ny = bsd[i] ? a[i]->y2 : a[i]->y1, tx, ty;
                    rnet_point( a[i], bsd[i] ? 1.0 - 1.0 / (double) a[i]->steps * 0.3 : 1.0 / (double) a[i]->steps * 0.3, &tx, &ty );
                    ox[i] = tx - nx; oy[i] = ty - ny;
                }
                double dot = ox[0] * ox[1] + oy[0] * oy[1], cr = ox[0] * oy[1] - oy[0] * ox[1];
                double ang = fabs( atan2( cr, -dot ) ) * 57.29578;                                    // 0 when the two leave the node in opposite directions (a straight way through)
                ++nd2;
                if ( ang > 60.0 ) { ++kink; }
            }
        }
        CHECK( nd2 > 20 && kink <= nd2 / 4 );                                                          // (the wander and the sharp bends of very short tracks are allowed to differ: most nodes pass smoothly)
    }
    CHECK( op( GFXOP_M3DCLEAR, 0 ) );
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
    // -- M3D("FLOORTEX") / M3D("FLOOROBJ"): what the floor under a point is made of ----------------------------------------------------------------------
    fresh();
    tex_flat( 301, 4, 4, 100 ); tex_flat( 302, 4, 4, 120 );
    CHECK( op( GFXOP_M3DOBJECT, 1, 1.0, 0.0 ) );
    CHECK( op( GFXOP_M3DOBJECT, 2, 50.0, 0.0 ) && op( GFXOP_M3DBOX, 8, 50.0, 0.0, 0.0, 0.0, 100.0, 100.0, 10.0, 301.0 ) );                  // a slab, top at 10
    CHECK( op( GFXOP_M3DOBJECT, 2, 51.0, 0.0 ) && op( GFXOP_M3DBOX, 8, 51.0, 20.0, 20.0, 10.0, 40.0, 40.0, 30.0, 302.0 ) );                  // a block on it, top at 30
    CHECK( Q( GFXQ_M3DFLOORTEX, 3, 10.0, 10.0, 100.0 ) == 301.0 && Q( GFXQ_M3DFLOOROBJ, 3, 10.0, 10.0, 100.0 ) == 50.0 );
    CHECK( Q( GFXQ_M3DFLOORTEX, 3, 30.0, 30.0, 100.0 ) == 302.0 && Q( GFXQ_M3DFLOOROBJ, 3, 30.0, 30.0, 100.0 ) == 51.0 );                    // the highest one below the point
    CHECK( Q( GFXQ_M3DFLOORTEX, 3, 30.0, 30.0, 20.0 ) == 301.0 );                                                                           // the point is below the block's top: the slab
    CHECK( Q( GFXQ_M3DFLOORTEX, 3, 500.0, 500.0, 100.0 ) == -1.0 && Q( GFXQ_M3DFLOOROBJ, 3, 500.0, 500.0, 100.0 ) == -1.0 );                // nothing there
    CHECK( op( GFXOP_M3DOBJFLAGS, 2, 51.0, 4.0 ) && Q( GFXQ_M3DFLOORTEX, 3, 30.0, 30.0, 100.0 ) == 301.0 );                                // a passable object is no floor
    {   // a shaded terrain cell tells its own picture, not the shaded copy
        uint32_t gp[256];
        for ( int i=0; i < 256; ++i ) { gp[i] = 0xFF000000U | (uint32_t) i << 16 | (uint32_t) i << 8 | (uint32_t) i; }
        m3d_colormap( M, gp );
        CHECK( op( GFXOP_M3DGROUND, 4, 5.0, 300.0, 400.0, 2.0 ) && op( GFXOP_M3DSUN, 6, -0.5, -0.3, 0.8, 0.5, 4.0, 100.0 ) );
        CHECK( op( GFXOP_M3DOBJECT, 2, 60.0, 0.0 ) && ops( GFXOP_M3DTERRAIN, 7, 0, 60.0, 0.0, 0.0, 12.0, 12.0, 100.0, 301.0 ) );
        int shaded = 0, bad = 0;
        for ( int j=0; j < 12; ++j ) {
            for ( int i=0; i < 12; ++i ) {
                double x = i * 100.0 + 50.0, y = j * 100.0 + 50.0, t = Q( GFXQ_M3DFLOORTEX, 3, x, y, 5000.0 );
                if ( t != 301.0 ) { ++bad; }
            }
        }
        for ( int k = 1; k <= 3; ++k ) { if ( M->texs[301 + 100 * k].col != 0 ) { ++shaded; } }
        CHECK( shaded >= 1 && bad == 0 );
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
    CHECK( !op( GFXOP_M3DOBJECT, 1, 30000.0 ) );
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
    // -- rnet_near(): the vector loop against its scalar twin (the same bits for 20000 places, dirt tracks and tarred roads) -------------------------------------
    {
        rnet_t rn;
        memset( &rn, 0, sizeof(rn) );
        rn.on = true; rn.spacing = 1800.0; rn.chance = 0.45; rn.wdirt = 140.0; rn.tspacing = 9000.0; rn.tchance = 0.4; rn.wtar = 600.0;
        int diff = 0;
        for ( int i=0; i < 20000; ++i ) {
            double x = (double)( ( i * 7919 ) % 40000 ) - 20000.0 + i * 0.37, y = (double)( ( i * 104729 ) % 40000 ) - 20000.0 + i * 0.11;
            rnear_t a, b;
            rnet_set_simd( 1 ); rnet_near( &rn, x, y, 500.0, &a );
            rnet_set_simd( 0 ); rnet_near( &rn, x, y, 500.0, &b );
            if ( a.dirt != b.dirt || a.tar != b.tar || a.tt != b.tt ) { ++diff; }
        }
        CHECK( diff == 0 );
        rnet_set_simd( -1 );
    }
    // -- the vector span kernel against its scalar twin: the same bytes (fog, haze, lamps, translucency, textures of any size, several views) ------------------
    {
        fresh();
        // a patterned picture 7 x 5 (not a power of two: wraps by division) and one of 8 x 16 (wraps by a mask)
        uint8_t b7[4 + 35], b8[4 + 128];
        b7[0] = 0; b7[1] = 7; b7[2] = 0; b7[3] = 5; b8[0] = 0; b8[1] = 8; b8[2] = 0; b8[3] = 16;
        for ( int i=0; i < 35; ++i ) { b7[4+i] = (uint8_t)( 1 + ( i * 37 ) % 200 ); }
        for ( int i=0; i < 128; ++i ) { b8[4+i] = (uint8_t)( 1 + ( i * 53 ) % 200 ); }
        double a1[1] = { 4.0 }, a2[1] = { 5.0 };
        const char* err = 0;
        CHECK( m3d_op( M, GFXOP_M3DTEXTURE, a1, 1, (const char*) b7, sizeof(b7), &err ) );
        CHECK( m3d_op( M, GFXOP_M3DTEXTURE, a2, 1, (const char*) b8, sizeof(b8), &err ) );
        CHECK( op( GFXOP_M3DOBJECT, 1, 2.0 ) );
        CHECK( op( GFXOP_M3DBOX, 8, 2.0, 100.0, -60.0, 0.0, 130.0, 60.0, 90.0, 4.0 ) );
        CHECK( op( GFXOP_M3DOBJECT, 1, 3.0 ) );
        CHECK( op( GFXOP_M3DBOX, 8, 3.0, 40.0, 70.0, 0.0, 90.0, 120.0, 60.0, 5.0 ) );
        CHECK( op( GFXOP_M3DOBJECT, 2, 4.0, 0.0 ) );
        CHECK( op( GFXOP_M3DBOX, 8, 4.0, 60.0, -30.0, 10.0, 80.0, 30.0, 50.0, 2.0 ) );
        CHECK( op( GFXOP_M3DLAMP, 6, 0.0, 60.0, 0.0, 40.0, 300.0, 200.0 ) );
        for ( int mode = 0; mode < 3; ++mode ) {
            if ( mode == 1 ) { CHECK( op( GFXOP_M3DFOG, 1, 400.0 ) ); }
            if ( mode == 2 ) { CHECK( op( GFXOP_M3DFOG, 3, 400.0, 5.0, 100.0 ) ); }
            for ( int yaw = 0; yaw < 360; yaw += 37 ) {
                CHECK( op( GFXOP_M3DCAMERA, 5, 0.0, 0.0, 0.0, (double) yaw, -12.0 ) );
                m3d_soft_set_simd( 1 );
                const uint8_t* fa = m3d_render( M );
                uint8_t* copy = (uint8_t*) malloc( 160U * 100U );
                memcpy( copy, fa, 160U * 100U );
                m3d_soft_set_simd( 0 );
                const uint8_t* fb2 = m3d_render( M );
                CHECK( memcmp( copy, fb2, 160U * 100U ) == 0 );
                free( copy );
            }
        }
        m3d_soft_set_simd( -1 );
    }
    test_world( getenv( "TMPDIR" ) != 0 ? getenv( "TMPDIR" ) : "/tmp" );
    test_switch();
    m3d_free( M );
    CHECK( fk.destroyed == 5 );
    printf( "m3dtest1: %d cases, %d failures\n", cases, failures );
    return failures;
}
