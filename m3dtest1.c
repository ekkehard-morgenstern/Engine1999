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
#include "gfxops.h"

/*
Automated test of the software 3D mesh renderer (m3d.[ch], no SDL): objects and their faces, the picture (depth, culling, clipping, masked / translucent
textures, fog, lamps, the sky, things), movement and collision (walls, steps, ceilings, sliding), floor following, shots, use and sight. Prints one line
per failure; the exit status is the number of failures. M3D_DUMP=file writes the last picture as a PPM.
*/

static int failures = 0, cases = 0;

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
    CHECK( op( GFXOP_M3DOBJECT, 1, 1.0 ) );
    CHECK( op( GFXOP_M3DBOX, 8, 1.0, -500.0, -500.0, -10.0, 500.0, 500.0, 0.0, 1.0 ) );
    CHECK( op( GFXOP_M3DCAMERA, 4, 0.0, 0.0, 0.0, 0.0 ) );
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
    // -- errors -----------------------------------------------------------------------------------------------------------------------------
    CHECK( !op( GFXOP_M3DOBJECT, 1, 5000.0 ) );
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
    m3d_free( M );
    printf( "m3dtest1: %d cases, %d failures\n", cases, failures );
    return failures;
}
