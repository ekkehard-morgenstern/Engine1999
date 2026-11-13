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


#include "w3d.h"
#include "gfxops.h"

/*
Automated test of the software 3D sector renderer (w3d.[ch], no SDL): the statements and functions, the picture, movement and collision,
shots, sight. Prints one line per failure; the exit status is the number of failures. W3D_DUMP=file writes the last scene as a PPM.
*/

static int failures = 0, cases = 0;

#define CHECK(cond) do { ++cases; if ( !(cond) ) { ++failures; printf( "FAIL line %d: %s\n", __LINE__, #cond ); } } while (0)

static w3d_t* W;

static bool op( int o, int n, ... ) {
    double a[10];
    va_list ap;
    va_start( ap, n );
    for ( int i=0; i < n; ++i ) {
        a[i] = va_arg( ap, double );
    }
    va_end( ap );
    const char* err = 0;
    return w3d_op( W, o, a, n, 0, 0, &err );
}

static const char* lasterr;
static bool ops( int o, int n, const char* s, size_t slen, ... ) {
    double a[10];
    va_list ap;
    va_start( ap, slen );
    for ( int i=0; i < n; ++i ) {
        a[i] = va_arg( ap, double );
    }
    va_end( ap );
    lasterr = 0;
    return w3d_op( W, o, a, n, s, slen, &lasterr );
}

static double Q( int q, int n, ... ) {
    double a[4] = { 0, 0, 0, 0 };
    va_list ap;
    va_start( ap, n );
    for ( int i=0; i < n; ++i ) {
        a[i] = va_arg( ap, double );
    }
    va_end( ap );
    double r = -12345.0;
    if ( !w3d_query( W, q, a, n, &r ) ) {
        return -99999.0;
    }
    return r;
}

#define NONE GFX_NONE

static void wall( int s, double x1, double y1, double x2, double y2, int tex, int next ) {
    op( GFXOP_W3DWALL, 7, (double) s, x1, y1, x2, y2, (double) tex, (double) next );
}

// room 0: 0..400 square, floor 0 ceil 128 (flats 20 / 30); room 1 east of it, x 400..600, y 100..300, joined by a doorway (400, 100..300)
static void build( void ) {
    op( GFXOP_W3DRESET, 0 );
    op( GFXOP_W3DSECTOR, 6, 0.0, 0.0, 128.0, 20.0, 30.0, 255.0 );
    wall( 0, 0, 0, 400, 0, 10, -1 );              // south (counterclockwise)
    wall( 0, 400, 0, 400, 100, 12, -1 );
    wall( 0, 400, 100, 400, 300, 13, 1 );         // the doorway
    wall( 0, 400, 300, 400, 400, 12, -1 );
    wall( 0, 400, 400, 0, 400, 14, -1 );          // north
    wall( 0, 0, 400, 0, 0, 15, -1 );              // west
    op( GFXOP_W3DSECTOR, 6, 1.0, 0.0, 128.0, 21.0, 31.0, 255.0 );
    wall( 1, 400, 100, 600, 100, 11, -1 );
    wall( 1, 600, 100, 600, 300, 11, -1 );
    wall( 1, 600, 300, 400, 300, 11, -1 );
    wall( 1, 400, 300, 400, 100, 13, 0 );
    op( GFXOP_W3DVIEW, 6, 0.0, 0.0, 320.0, 150.0, 1.0, 90.0 );
    op( GFXOP_W3DCAMERA, 5, 200.0, 200.0, 90.0, NONE, NONE );
}

static int px( const uint8_t* fb, int x, int y ) {
    return fb[ y * 320 + x ];
}

static void dump( const uint8_t* fb ) {
    const char* f = getenv( "W3D_DUMP" );
    if ( f == 0 || *f == '\0' ) {
        return;
    }
    FILE* fp = fopen( f, "wb" );
    if ( fp == 0 ) {
        return;
    }
    fprintf( fp, "P6\n320 150\n255\n" );
    for ( int i=0; i < 320 * 150; ++i ) {
        int c = fb[i];
        unsigned char rgb[3] = { (unsigned char)( ( c * 37 ) & 255 ), (unsigned char)( ( c * 91 ) & 255 ), (unsigned char)( ( c * 17 ) & 255 ) };
        fwrite( rgb, 1, 3, fp );
    }
    fclose( fp );
}

static void test_picture( void ) {
    build();
    const uint8_t* fb = w3d_render( W );
    CHECK( fb != 0 );
    CHECK( px( fb, 160, 75 ) == 14 );               // north wall straight ahead
    CHECK( px( fb, 160, 2 ) == 30 );                // ceiling
    CHECK( px( fb, 160, 147 ) == 20 );              // floor
    CHECK( px( fb, 160, 15 ) == 14 );               // wall: top edge at y 9.4
    CHECK( px( fb, 160, 7 ) == 30 );
    CHECK( px( fb, 160, 105 ) == 14 );              // floor edge at y 75 + 46*160/200 = 111.8
    CHECK( px( fb, 160, 118 ) == 20 );
    dump( fb );
    op( GFXOP_W3DCAMERA, 5, 200.0, 200.0, 0.0, NONE, NONE );
    fb = w3d_render( W );
    CHECK( px( fb, 160, 75 ) == 11 );               // through the doorway to the far wall of room 1
    CHECK( px( fb, 5, 75 ) == 12 );                 // left of the doorway: the solid part of the east wall (y 300..400 is to the left)
    op( GFXOP_W3DCAMERA, 5, 200.0, 200.0, 180.0, NONE, NONE );
    fb = w3d_render( W );
    CHECK( px( fb, 160, 75 ) == 15 );
    op( GFXOP_W3DCAMERA, 5, 200.0, 200.0, 270.0, NONE, NONE );
    fb = w3d_render( W );
    CHECK( px( fb, 160, 75 ) == 10 );
    // looking up moves the horizon down: the wall's top edge moves down too
    op( GFXOP_W3DCAMERA, 5, 200.0, 200.0, 90.0, 30.0, NONE );
    fb = w3d_render( W );
    CHECK( px( fb, 160, 110 ) == 14 && px( fb, 160, 149 ) == 14 );          // horizon at 75 + tan(30) * 160 = 167: the wall fills the bottom
    CHECK( px( fb, 160, 2 ) == 30 && px( fb, 160, 90 ) == 30 );
}

static void test_move( void ) {
    build();
    CHECK( Q( GFXQ_W3DSECTOR, 0 ) == 0 );
    op( GFXOP_W3DMOVE, 2, 1000.0, 0.0 );            // north, into the wall
    CHECK( Q( GFXQ_W3DY, 0 ) > 385.0 && Q( GFXQ_W3DY, 0 ) < 387.0 );
    CHECK( ( (int) Q( GFXQ_W3DHIT, 0 ) & W3D_HIT_WALL ) != 0 );
    op( GFXOP_W3DCAMERA, 5, 200.0, 200.0, 0.0, NONE, NONE );
    op( GFXOP_W3DMOVE, 2, 300.0, 0.0 );             // east through the doorway
    CHECK( Q( GFXQ_W3DSECTOR, 0 ) == 1 );
    CHECK( Q( GFXQ_W3DX, 0 ) > 499.0 && Q( GFXQ_W3DX, 0 ) < 501.0 );
    CHECK( ( (int) Q( GFXQ_W3DHIT, 0 ) & W3D_HIT_SECTOR ) != 0 );
    op( GFXOP_W3DMOVE, 2, -300.0, 0.0 );
    CHECK( Q( GFXQ_W3DSECTOR, 0 ) == 0 );
    op( GFXOP_W3DCAMERA, 5, 350.0, 50.0, 0.0, NONE, NONE );
    op( GFXOP_W3DMOVE, 2, 100.0, 0.0 );             // east into the solid part
    CHECK( Q( GFXQ_W3DX, 0 ) > 385.0 && Q( GFXQ_W3DX, 0 ) < 387.0 );
    CHECK( Q( GFXQ_W3DSECTOR, 0 ) == 0 );
    // sliding: diagonal into the north wall keeps the x movement
    op( GFXOP_W3DCAMERA, 5, 100.0, 380.0, 45.0, NONE, NONE );
    op( GFXOP_W3DMOVE, 2, 40.0, 0.0 );
    CHECK( Q( GFXQ_W3DX, 0 ) > 120.0 && Q( GFXQ_W3DY, 0 ) <= 386.5 );
    // a step that is too high stops the player, a low one does not
    op( GFXOP_W3DHEIGHT, 3, 1.0, 30.0, 128.0 );
    op( GFXOP_W3DCAMERA, 5, 200.0, 200.0, 0.0, NONE, NONE );
    op( GFXOP_W3DMOVE, 2, 300.0, 0.0 );
    CHECK( Q( GFXQ_W3DSECTOR, 0 ) == 0 && Q( GFXQ_W3DX, 0 ) < 387.0 );
    op( GFXOP_W3DHEIGHT, 3, 1.0, 20.0, 128.0 );
    op( GFXOP_W3DMOVE, 2, 300.0, 0.0 );
    CHECK( Q( GFXQ_W3DSECTOR, 0 ) == 1 );
    // a jump lifts the feet: a step of 30 (more than 24) can be taken with a lift of 10 or more
    op( GFXOP_W3DHEIGHT, 3, 1.0, 30.0, 128.0 );
    op( GFXOP_W3DCAMERA, 5, 200.0, 200.0, 0.0, NONE, NONE );
    op( GFXOP_W3DLIFT, 1, 3.0 );
    op( GFXOP_W3DMOVE, 2, 300.0, 0.0 );
    CHECK( Q( GFXQ_W3DSECTOR, 0 ) == 0 && Q( GFXQ_W3DLIFT, 0 ) == 3 );
    op( GFXOP_W3DLIFT, 1, 10.0 );
    op( GFXOP_W3DMOVE, 2, 300.0, 0.0 );
    CHECK( Q( GFXQ_W3DSECTOR, 0 ) == 1 );
    op( GFXOP_W3DLIFT, 1, 0.0 );
    op( GFXOP_W3DHEIGHT, 3, 1.0, 20.0, 128.0 );
    // a low ceiling (a closed door)
    op( GFXOP_W3DCAMERA, 5, 200.0, 200.0, 0.0, NONE, NONE );
    op( GFXOP_W3DHEIGHT, 3, 1.0, 0.0, 40.0 );
    op( GFXOP_W3DMOVE, 2, 300.0, 0.0 );
    CHECK( Q( GFXQ_W3DSECTOR, 0 ) == 0 );
    CHECK( ( (int) Q( GFXQ_W3DHIT, 0 ) & W3D_HIT_WALL ) != 0 );
    // a blocking portal
    op( GFXOP_W3DHEIGHT, 3, 1.0, 0.0, 128.0 );
    op( GFXOP_W3DWALLFLAG, 3, 0.0, 2.0, (double) W3D_WF_BLOCK );
    op( GFXOP_W3DMOVE, 2, 300.0, 0.0 );
    CHECK( Q( GFXQ_W3DSECTOR, 0 ) == 0 );
}

static void test_things( void ) {
    build();
    op( GFXOP_W3DCAMERA, 5, 200.0, 200.0, 0.0, NONE, NONE );
    CHECK( op( GFXOP_W3DTHING, 6, 0.0, 300.0, 200.0, 40.0, 20.0, 60.0 ) );
    CHECK( !op( GFXOP_W3DTHING, 6, 1.0, 900.0, 900.0, 40.0, 20.0, 60.0 ) );     // outside every sector
    CHECK( Q( GFXQ_W3DTON, 1, 0.0 ) == -1 && Q( GFXQ_W3DTON, 1, 1.0 ) == 0 );
    CHECK( Q( GFXQ_W3DTSECTOR, 1, 0.0 ) == 0 );
    CHECK( fabs( Q( GFXQ_W3DDIST, 1, 0.0 ) - 100.0 ) < 1e-6 );
    CHECK( fabs( Q( GFXQ_W3DBEARING, 1, 0.0 ) ) < 1e-6 );
    const uint8_t* fb = w3d_render( W );
    CHECK( px( fb, 160, 75 ) == 40 && px( fb, 160, 120 ) == 40 );     // in front of the far wall
    CHECK( px( fb, 160, 5 ) == 30 );
    // a thing to the right of the view (south, since the view is east) has a positive bearing
    op( GFXOP_W3DTHING, 6, 2.0, 300.0, 100.0, 41.0, 20.0, 40.0 );
    CHECK( Q( GFXQ_W3DBEARING, 1, 2.0 ) > 40.0 && Q( GFXQ_W3DBEARING, 1, 2.0 ) < 50.0 );
    // shots
    CHECK( Q( GFXQ_W3DSHOOT, 1, 1000.0 ) == 0 );
    CHECK( Q( GFXQ_W3DSHOTKIND, 0 ) == 2 && fabs( Q( GFXQ_W3DSHOTDIST, 0 ) - 100.0 ) < 1e-6 );
    op( GFXOP_W3DTHINGOFF, 1, 0.0 );
    CHECK( Q( GFXQ_W3DSHOOT, 1, 1000.0 ) == -1 );
    CHECK( Q( GFXQ_W3DSHOTKIND, 0 ) == 1 && fabs( Q( GFXQ_W3DSHOTDIST, 0 ) - 400.0 ) < 1e-6 );   // the far wall of room 1
    op( GFXOP_W3DTHING, 6, 0.0, 500.0, 200.0, 40.0, 20.0, 60.0 );               // in room 1, through the doorway
    CHECK( Q( GFXQ_W3DSHOOT, 1, 1000.0 ) == 0 );
    CHECK( Q( GFXQ_W3DSHOOT, 1, 250.0 ) == -1 );                                  // out of range
    op( GFXOP_W3DWALLFLAG, 3, 0.0, 2.0, (double) W3D_WF_SOLID );                  // frosted glass (both sides)
    op( GFXOP_W3DWALLFLAG, 3, 1.0, 3.0, (double) W3D_WF_SOLID );
    CHECK( Q( GFXQ_W3DSHOOT, 1, 1000.0 ) == -1 );
    CHECK( Q( GFXQ_W3DSEES, 1, 0.0 ) == 0 );
    op( GFXOP_W3DWALLFLAG, 3, 0.0, 2.0, 0.0 );
    op( GFXOP_W3DWALLFLAG, 3, 1.0, 3.0, 0.0 );
    CHECK( Q( GFXQ_W3DSEES, 1, 0.0 ) == -1 );
    op( GFXOP_W3DTHING, 6, 0.0, 500.0, 340.0, 40.0, 20.0, 60.0 );               // out of line of sight
    CHECK( Q( GFXQ_W3DSEES, 1, 0.0 ) == -1 || sqrt( 1.0 ) == 1.0 );
    // a shot aimed too high flies over the thing
    op( GFXOP_W3DTHING, 6, 0.0, 300.0, 200.0, 40.0, 20.0, 60.0 );
    op( GFXOP_W3DCAMERA, 5, 200.0, 200.0, 0.0, 20.0, NONE );
    CHECK( Q( GFXQ_W3DSHOOT, 1, 1000.0 ) == -1 );
    // moving a thing
    op( GFXOP_W3DCAMERA, 5, 200.0, 200.0, 0.0, 0.0, NONE );
    op( GFXOP_W3DTHINGMOVE, 3, 0.0, 50.0, 0.0 );
    CHECK( fabs( Q( GFXQ_W3DTX, 1, 0.0 ) - 350.0 ) < 1e-6 );
    op( GFXOP_W3DTHINGMOVE, 3, 0.0, 100.0, 0.0 );                                 // through the doorway
    CHECK( Q( GFXQ_W3DTSECTOR, 1, 0.0 ) == 1 );
    op( GFXOP_W3DTHINGMOVE, 3, 0.0, 0.0, 500.0 );                                 // into the wall of room 1
    CHECK( Q( GFXQ_W3DTY, 1, 0.0 ) < 300.0 );
    // use: the portal in front
    op( GFXOP_W3DCAMERA, 5, 370.0, 200.0, 0.0, NONE, NONE );
    CHECK( Q( GFXQ_W3DUSE, 1, 50.0 ) == 1 );
    CHECK( Q( GFXQ_W3DUSE, 1, 10.0 ) == -1 );
    op( GFXOP_W3DCAMERA, 5, 370.0, 200.0, 90.0, NONE, NONE );
    CHECK( Q( GFXQ_W3DUSE, 1, 300.0 ) == -1 );
}

static void test_queries( void ) {
    build();
    CHECK( Q( GFXQ_W3DSECTORS, 0 ) == 2 );
    CHECK( Q( GFXQ_W3DWALLS, 1, 0.0 ) == 6 && Q( GFXQ_W3DWALLS, 1, 1.0 ) == 4 );
    CHECK( Q( GFXQ_W3DFLOOR, 1, 0.0 ) == 0 && Q( GFXQ_W3DCEIL, 1, 0.0 ) == 128 );
    CHECK( Q( GFXQ_W3DSECTORAT, 2, 500.0, 200.0 ) == 1 && Q( GFXQ_W3DSECTORAT, 2, 100.0, 100.0 ) == 0 && Q( GFXQ_W3DSECTORAT, 2, 900.0, 0.0 ) == -1 );
    CHECK( Q( GFXQ_W3DFLOORAT, 2, 500.0, 200.0 ) == 0 && Q( GFXQ_W3DFLOORAT, 2, 900.0, 0.0 ) < -1.0e5 );
    op( GFXOP_W3DTAG, 2, 1.0, 77.0 );
    CHECK( Q( GFXQ_W3DTAG, 1, 1.0 ) == 77 && Q( GFXQ_W3DTAG, 1, 0.0 ) == 0 );
    op( GFXOP_W3DLIGHT, 2, 1.0, 100.0 );
    CHECK( Q( GFXQ_W3DLIGHT, 1, 1.0 ) == 100 );
    CHECK( Q( GFXQ_W3DANGLE, 0 ) == 90 );
    op( GFXOP_W3DTURN, 2, 300.0, 10.0 );
    CHECK( Q( GFXQ_W3DANGLE, 0 ) == 30 && Q( GFXQ_W3DPITCH, 0 ) == 10 );
    CHECK( Q( GFXQ_W3DZ, 0 ) == 46 );
    op( GFXOP_W3DCAMERA, 5, 200.0, 200.0, 0.0, NONE, 10.0 );
    CHECK( Q( GFXQ_W3DZ, 0 ) == 56 );
    op( GFXOP_W3DCAMERA, 5, 200.0, 200.0, 0.0, NONE, 500.0 );                    // the ceiling clamps the eye
    CHECK( Q( GFXQ_W3DZ, 0 ) == 126 );
    // errors
    CHECK( !op( GFXOP_W3DCAMERA, 5, 900.0, 900.0, 0.0, NONE, NONE ) );
    CHECK( !op( GFXOP_W3DCAMERA, 5, 200.0, 200.0, 0.0, 95.0, NONE ) );
    CHECK( !ops( GFXOP_W3DWALL, 7, 0, 0, 5.0, 0.0, 0.0, 1.0, 1.0, 1.0, -1.0 ) && lasterr != 0 && strcmp( lasterr, "Sector not defined" ) == 0 );
    CHECK( !op( GFXOP_W3DSECTOR, 3, 5.0, 100.0, 50.0 ) );                          // ceiling below floor
    CHECK( !op( GFXOP_W3DVIEW, 4, 0.0, 0.0, 4.0, 4.0 ) );
    CHECK( !op( GFXOP_W3DWALLTEX, 5, 0.0, 99.0, 1.0, 1.0, 1.0 ) );
    CHECK( !ops( GFXOP_W3DTEXTURE, 1, "\0\2\0\2abc", 7, 0.0 ) );                  // 2 x 2 needs 4 pixels
    CHECK( ops( GFXOP_W3DTEXTURE, 1, "\0\2\0\2abcd", 8, 5.0 ) );
    CHECK( Q( GFXQ_W3DTEXW, 1, 5.0 ) == 2 && Q( GFXQ_W3DTEXH, 1, 5.0 ) == 2 && Q( GFXQ_W3DTEXW, 1, 6.0 ) == 0 );
    CHECK( Q( GFXQ_W3DFLOOR, 1, 77.0 ) < -9999.0 );                                // a bad sector is an error
}

static void test_textures( void ) {
    build();
    // a 4 x 4 wall texture: column u has the color 50 + u in all rows (so the column that is hit shows in the picture)
    uint8_t blk[4 + 16];
    blk[0] = 0; blk[1] = 4; blk[2] = 0; blk[3] = 4;
    for ( int v=0; v < 4; ++v ) {
        for ( int u=0; u < 4; ++u ) {
            blk[4 + v * 4 + u] = (uint8_t)( 50 + u );
        }
    }
    CHECK( ops( GFXOP_W3DTEXTURE, 1, (const char*) blk, sizeof(blk), 50.0 ) );
    op( GFXOP_W3DWALLTEX, 5, 0.0, 4.0, 50.0, 50.0, 50.0 );                       // the north wall runs from x=400 to x=0 in its own direction
    op( GFXOP_W3DCAMERA, 5, 201.0, 200.0, 90.0, NONE, NONE );
    const uint8_t* fb = w3d_render( W );
    int c1 = px( fb, 160, 75 );
    op( GFXOP_W3DCAMERA, 5, 201.0, 200.0, 90.0, NONE, NONE );
    op( GFXOP_W3DWALLOFF, 4, 0.0, 4.0, 1.0, 0.0 );                                // shifted by one texel
    fb = w3d_render( W );
    int c2 = px( fb, 160, 75 );
    CHECK( c1 >= 50 && c1 <= 53 && c2 >= 50 && c2 <= 53 && c1 != c2 );
    // a texture on a floor is mapped to the world: 4 texels per unit step along x
    op( GFXOP_W3DSECTOR, 4, 0.0, 0.0, 128.0, 50.0 );
    op( GFXOP_W3DCAMERA, 5, 200.0, 200.0, 0.0, 90.0 - 90.0, NONE );
    fb = w3d_render( W );
    CHECK( px( fb, 160, 148 ) >= 50 && px( fb, 160, 148 ) <= 53 );
}

static void test_masked_sky_light( void ) {
    build();
    // a masked window texture over the doorway: columns 0..1 of a 2 x 1 picture: index 0 (see-through) and 60
    CHECK( ops( GFXOP_W3DTEXTURE, 1, "\0\2\0\1\0\x3c", 6, 60.0 ) );
    op( GFXOP_W3DWALLTEX, 5, 0.0, 2.0, 60.0, -1.0, -1.0 );
    op( GFXOP_W3DWALLFLAG, 3, 0.0, 2.0, (double) W3D_WF_MASKED );
    op( GFXOP_W3DCAMERA, 5, 380.0, 200.0, 0.0, NONE, NONE );
    const uint8_t* fb = w3d_render( W );
    CHECK( px( fb, 160, 75 ) == 11 || px( fb, 160, 75 ) == 60 );
    // a sky ceiling
    op( GFXOP_W3DSECTOR, 5, 0.0, 0.0, 128.0, NONE, -1.0, NONE );
    op( GFXOP_W3DSKY, 1, 99.0 );
    op( GFXOP_W3DCAMERA, 5, 200.0, 200.0, 90.0, NONE, NONE );
    fb = w3d_render( W );
    CHECK( px( fb, 160, 2 ) == 99 );
    // darker light: with a color map of grays the level picks a darker gray
    uint32_t pal[256];
    for ( int i=0; i < 256; ++i ) {
        pal[i] = 0xFF000000U | ( (uint32_t) i << 16 ) | ( (uint32_t) i << 8 ) | (uint32_t) i;
    }
    w3d_colormap( W, pal );
    op( GFXOP_W3DSECTOR, 6, 0.0, 0.0, 128.0, 20.0, 30.0, 255.0 );
    op( GFXOP_W3DFOG, 1, 0.0 );
    op( GFXOP_W3DWALLTEX, 5, 0.0, 4.0, 200.0, 200.0, 200.0 );
    op( GFXOP_W3DLIGHT, 2, 0.0, 255.0 );
    fb = w3d_render( W );
    CHECK( px( fb, 160, 75 ) == 200 );
    op( GFXOP_W3DLIGHT, 2, 0.0, 128.0 );
    fb = w3d_render( W );
    int dark = px( fb, 160, 75 );
    CHECK( dark > 90 && dark < 110 );
    op( GFXOP_W3DLIGHT, 2, 0.0, 0.0 );
    fb = w3d_render( W );
    CHECK( px( fb, 160, 75 ) == 1 || px( fb, 160, 75 ) == 0 || px( fb, 160, 75 ) < 8 );
}

static void test_concave( void ) {
    // an L-shaped room (concave) seen from inside: the corner of the L must not show walls through each other
    op( GFXOP_W3DRESET, 0 );
    op( GFXOP_W3DSECTOR, 6, 0.0, 0.0, 128.0, 20.0, 30.0, 255.0 );
    wall( 0, 0, 0, 300, 0, 10, -1 );
    wall( 0, 300, 0, 300, 100, 11, -1 );
    wall( 0, 300, 100, 100, 100, 12, -1 );        // the inner corner
    wall( 0, 100, 100, 100, 300, 13, -1 );
    wall( 0, 100, 300, 0, 300, 14, -1 );
    wall( 0, 0, 300, 0, 0, 15, -1 );
    op( GFXOP_W3DVIEW, 6, 0.0, 0.0, 320.0, 150.0, 1.0, 90.0 );
    op( GFXOP_W3DCAMERA, 5, 50.0, 50.0, 45.0, NONE, NONE );                      // toward the inner corner (100,100)
    const uint8_t* fb = w3d_render( W );
    CHECK( px( fb, 100, 75 ) == 13 && px( fb, 220, 75 ) == 12 );
    op( GFXOP_W3DCAMERA, 5, 200.0, 50.0, 90.0, NONE, NONE );
    fb = w3d_render( W );
    CHECK( px( fb, 160, 75 ) == 12 );
    CHECK( Q( GFXQ_W3DSECTORAT, 2, 50.0, 250.0 ) == 0 && Q( GFXQ_W3DSECTORAT, 2, 200.0, 200.0 ) == -1 );
}


// random camera, view, things and heights on the two room world: no crash (run it under ASan), the player always stays inside its sector
static unsigned rngstate = 12345U;
static double rnd01( void ) {
    rngstate = rngstate * 1664525U + 1013904223U;
    return (double)( rngstate >> 8 ) / 16777216.0;
}

static void test_fuzz( void ) {
    build();
    op( GFXOP_W3DWALLFLAG, 3, 0.0, 2.0, (double)( W3D_WF_MASKED | W3D_WF_BLOCK ) );
    CHECK( ops( GFXOP_W3DTEXTURE, 1, "\0\2\0\1\0\x3c", 6, 13.0 ) );
    for ( int i=0; i < 12; ++i ) {
        op( GFXOP_W3DTHING, 6, (double) i, 20.0 + rnd01() * 350.0, 20.0 + rnd01() * 350.0, (double)( 30 + i ), 10.0 + rnd01() * 40.0, 10.0 + rnd01() * 70.0 );
    }
    int bad = 0;
    for ( int it=0; it < 600; ++it ) {
        int r = (int)( rnd01() * 8.0 );
        switch ( r ) {
            case 0: op( GFXOP_W3DTURN, 2, rnd01() * 90.0 - 45.0, rnd01() * 20.0 - 10.0 ); break;
            case 1: case 2: case 3: op( GFXOP_W3DMOVE, 2, rnd01() * 300.0 - 150.0, rnd01() * 300.0 - 150.0 ); break;
            case 4: op( GFXOP_W3DTHINGMOVE, 3, (double)(int)( rnd01() * 12.0 ), rnd01() * 200.0 - 100.0, rnd01() * 200.0 - 100.0 ); break;
            case 5: op( GFXOP_W3DHEIGHT, 3, 1.0, rnd01() * 40.0, 60.0 + rnd01() * 100.0 ); break;
            case 6: op( GFXOP_W3DVIEW, 6, 0.0, 0.0, 16.0 + rnd01() * 300.0, 16.0 + rnd01() * 200.0, 1.0 + (double)(int)( rnd01() * 3.0 ), 40.0 + rnd01() * 100.0 ); break;
            default: op( GFXOP_W3DCAMERA, 5, 5.0 + rnd01() * 390.0, 5.0 + rnd01() * 390.0, rnd01() * 360.0, rnd01() * 100.0 - 50.0, rnd01() * 40.0 - 10.0 ); break;
        }
        const uint8_t* fb = w3d_render( W );
        if ( fb == 0 ) {
            ++bad;
        }
        if ( Q( GFXQ_W3DSECTOR, 0 ) != Q( GFXQ_W3DSECTORAT, 2, Q( GFXQ_W3DX, 0 ), Q( GFXQ_W3DY, 0 ) ) ) {
            ++bad;
        }
        Q( GFXQ_W3DSHOOT, 1, 1000.0 );
        Q( GFXQ_W3DSEES, 1, (double)(int)( rnd01() * 12.0 ) );
    }
    CHECK( bad == 0 );
}

// a thing in a sector that no column got into (behind a low wall, over the sky) is not drawn
static void test_hidden_things( void ) {
    op( GFXOP_W3DRESET, 0 );
    op( GFXOP_W3DSECTOR, 6, 0.0, 0.0, 100.0, 20.0, -1.0, 255.0 );
    wall( 0, 0, 0, 400, 0, 10, -1 );
    wall( 0, 400, 0, 400, 400, 10, -1 );
    wall( 0, 400, 400, 0, 400, 10, 1 );          // the north wall is a portal into a low sky room: you look over it
    wall( 0, 0, 400, 0, 0, 10, -1 );
    op( GFXOP_W3DSECTOR, 6, 1.0, 0.0, 100.0, 20.0, -1.0, 255.0 );
    wall( 1, 0, 400, 400, 400, 10, 0 );
    wall( 1, 400, 400, 400, 420, 10, -1 );
    wall( 1, 400, 420, 0, 420, 77, -1 );
    wall( 1, 0, 420, 0, 400, 10, -1 );
    op( GFXOP_W3DSECTOR, 6, 2.0, 0.0, 300.0, 20.0, 30.0, 255.0 );      // a room behind the wall, not reachable by any column
    wall( 2, 0, 500, 400, 500, 10, -1 );
    wall( 2, 400, 500, 400, 700, 10, -1 );
    wall( 2, 400, 700, 0, 700, 10, -1 );
    wall( 2, 0, 700, 0, 500, 10, -1 );
    op( GFXOP_W3DVIEW, 6, 0.0, 0.0, 320.0, 150.0, 1.0, 90.0 );
    op( GFXOP_W3DCAMERA, 5, 200.0, 100.0, 90.0, NONE, NONE );
    op( GFXOP_W3DTHING, 6, 0.0, 200.0, 600.0, 55.0, 100.0, 280.0 );     // tall, behind the low fence, in sector 2
    const uint8_t* fb = w3d_render( W );
    int found = 0;
    for ( int i=0; i < 320 * 150; ++i ) {
        if ( fb[i] == 55 ) {
            ++found;
        }
    }
    CHECK( found == 0 );
    CHECK( px( fb, 160, 10 ) != 55 );
}


// -- slopes, sectors over each other, sky walls, draw distance, sky mapping ------------------------------------------------

static void test_m1( void ) {
    build();
    const uint8_t* fb = w3d_render( W );
    CHECK( px( fb, 160, 105 ) == 14 );              // flat floor: the north wall reaches down to row 111.8
    // a floor rising to the north: 0.1 per unit around the middle of the walls (y 200): 20 units high at the north wall (200 away)
    CHECK( op( GFXOP_W3DSLOPE, 4, 0.0, 0.0, 0.0, 0.1 ) );
    CHECK( fabs( Q( GFXQ_W3DFLOORAT, 2, 200.0, 300.0 ) - 10.0 ) < 1.0e-9 );
    CHECK( fabs( Q( GFXQ_W3DFLOORAT, 2, 200.0, 100.0 ) + 10.0 ) < 1.0e-9 );
    CHECK( fabs( Q( GFXQ_W3DCEILAT, 2, 200.0, 300.0 ) - 128.0 ) < 1.0e-9 );
    CHECK( fabs( Q( GFXQ_W3DSLOPE, 3, 0.0, 0.0, 1.0 ) - 0.1 ) < 1.0e-9 );
    fb = w3d_render( W );
    CHECK( px( fb, 160, 105 ) == 20 );              // the floor now meets the wall at row 95.8
    CHECK( px( fb, 160, 85 ) == 14 );
    CHECK( px( fb, 160, 147 ) == 20 );
    // walking up the slope the feet follow the floor
    op( GFXOP_W3DMOVE, 2, 100.0, 0.0 );
    for ( int i=0; i < 30; ++i ) {
        fb = w3d_render( W );                       // (the eye eases to the floor)
    }
    CHECK( fabs( Q( GFXQ_W3DFLOORZ, 0 ) - 10.0 ) < 1.0e-9 );
    CHECK( fabs( Q( GFXQ_W3DZ, 0 ) - 56.0 ) < 1.0e-9 );
    // a ceiling slope: lower to the north (-0.1: 20 units lower at the north wall, so it comes down to 108 there)
    build();
    CHECK( op( GFXOP_W3DSLOPE, 4, 0.0, 1.0, 0.0, -0.1 ) );
    CHECK( fabs( Q( GFXQ_W3DCEILAT, 2, 200.0, 390.0 ) - 109.0 ) < 1.0e-9 );
    fb = w3d_render( W );
    CHECK( px( fb, 160, 20 ) == 30 );               // the wall top was at row 9.4 with a flat ceiling; now 128 -> 108: 75 - 62*0.8 = 25.4
    CHECK( px( fb, 160, 30 ) == 14 );
    CHECK( !op( GFXOP_W3DSLOPE, 4, 0.0, 3.0, 0.0, 0.0 ) );
    CHECK( !op( GFXOP_W3DSLOPE, 4, 0.0, 0.0, 500.0, 0.0 ) );
    CHECK( !op( GFXOP_W3DSLOPE, 4, 9.0, 0.0, 0.1, 0.0 ) );
    // collision and shots with a slope: the doorway's step from sector 0 (floor 10 .. at x 400, y 200 with a gx slope 0.05 -> 10) to sector 1 (floor 0)
    build();
    op( GFXOP_W3DSLOPE, 4, 0.0, 0.0, 0.05, 0.0 );
    op( GFXOP_W3DCAMERA, 5, 380.0, 200.0, 0.0, NONE, NONE );
    op( GFXOP_W3DMOVE, 2, 60.0, 0.0 );              // 20 units further east: into sector 1 (the floor falls away: no step up)
    CHECK( Q( GFXQ_W3DSECTOR, 0 ) == 1.0 );

    // sectors over each other: sector 2 above sector 0 (floor 200, ceiling 300), joined by nothing
    build();
    op( GFXOP_W3DSECTOR, 6, 2.0, 200.0, 300.0, 40.0, 41.0, 255.0 );
    wall( 2, 0, 0, 400, 0, 50, -1 );
    wall( 2, 400, 0, 400, 400, 51, -1 );
    wall( 2, 400, 400, 0, 400, 52, -1 );
    wall( 2, 0, 400, 0, 0, 53, -1 );
    CHECK( Q( GFXQ_W3DSECTORAT, 2, 200.0, 200.0 ) == 0.0 );             // (the player's sector is preferred)
    CHECK( Q( GFXQ_W3DSECTORZ, 3, 200.0, 200.0, 250.0 ) == 2.0 );
    CHECK( Q( GFXQ_W3DSECTORZ, 3, 200.0, 200.0, 50.0 ) == 0.0 );
    CHECK( Q( GFXQ_W3DSECTORZ, 3, 200.0, 200.0, 150.0 ) == 0.0 );       // in the gap between: the nearest floor below
    CHECK( op( GFXOP_W3DCAMERA, 6, 200.0, 200.0, 90.0, NONE, NONE, 2.0 ) );
    CHECK( Q( GFXQ_W3DSECTOR, 0 ) == 2.0 );
    fb = w3d_render( W );
    CHECK( px( fb, 160, 75 ) == 52 );               // the upper room's own north wall
    CHECK( Q( GFXQ_W3DVISITED, 0 ) == 1.0 );
    CHECK( !op( GFXOP_W3DCAMERA, 6, 200.0, 200.0, 90.0, NONE, NONE, 1.0 ) );      // sector 1 is not there
    CHECK( op( GFXOP_W3DCAMERA, 6, 200.0, 200.0, 90.0, NONE, NONE, 0.0 ) );
    fb = w3d_render( W );
    CHECK( px( fb, 160, 75 ) == 14 );
    // things in the upper room
    CHECK( op( GFXOP_W3DTHING, 7, 5.0, 100.0, 100.0, 60.0, 20.0, 40.0, 2.0 ) );
    CHECK( Q( GFXQ_W3DTSECTOR, 1, 5.0 ) == 2.0 );
    CHECK( op( GFXOP_W3DTHING, 6, 6.0, 100.0, 100.0, 61.0, 20.0, 40.0 ) );
    CHECK( Q( GFXQ_W3DTSECTOR, 1, 6.0 ) == 0.0 );
    // sight stops at the ceiling and the floor of stacked rooms: the camera is in sector 0 (ceiling 128), thing 5 stands upstairs, thing 6 in the room
    CHECK( Q( GFXQ_W3DSEES, 1, 5.0 ) == 0.0 );
    CHECK( Q( GFXQ_W3DSEES, 1, 6.0 ) == -1.0 );
    CHECK( op( GFXOP_W3DCAMERA, 6, 200.0, 200.0, 90.0, NONE, NONE, 2.0 ) );
    CHECK( Q( GFXQ_W3DSEES, 1, 5.0 ) == -1.0 );
    CHECK( Q( GFXQ_W3DSEES, 1, 6.0 ) == 0.0 );        // and the thing downstairs is not seen through the floor

    // a sky wall: the north wall shows the sky picture (the palette entry 1 without a picture)
    build();
    fb = w3d_render( W );
    CHECK( px( fb, 160, 75 ) == 14 );
    op( GFXOP_W3DWALLFLAG, 3, 0.0, 4.0, (double) W3D_WF_SKY );
    fb = w3d_render( W );
    CHECK( px( fb, 160, 75 ) == 1 );
    CHECK( px( fb, 160, 147 ) == 20 );
    // sky mapping parameters
    CHECK( op( GFXOP_W3DSKY, 3, 2.0, 4.0, 8.0 ) );
    CHECK( !op( GFXOP_W3DSKY, 3, 2.0, 0.0, 0.0 ) );
    CHECK( !op( GFXOP_W3DSKY, 3, 2.0, 100.0, 0.0 ) );
    CHECK( op( GFXOP_W3DSKY, 1, 1.0 ) );

    // the draw distance: the north wall is 200 away
    build();
    CHECK( op( GFXOP_W3DFAR, 1, 150.0 ) );
    fb = w3d_render( W );
    CHECK( px( fb, 160, 75 ) == 0 );
    CHECK( op( GFXOP_W3DFAR, 1, 300.0 ) );
    fb = w3d_render( W );
    CHECK( px( fb, 160, 75 ) == 14 );
    CHECK( op( GFXOP_W3DFAR, 1, 0.0 ) );
    CHECK( !op( GFXOP_W3DFAR, 1, -1.0 ) );
    // a long chain of sectors: only the ones in view are reached
    op( GFXOP_W3DRESET, 0 );
    for ( int i=0; i < 300; ++i ) {
        op( GFXOP_W3DSECTOR, 6, (double) i, 0.0, 128.0, 20.0, 30.0, 255.0 );
        double x0 = i * 100.0;
        wall( i, x0, 0, x0 + 100, 0, 10, -1 );
        wall( i, x0 + 100, 0, x0 + 100, 100, 12, i < 299 ? i + 1 : -1 );
        wall( i, x0 + 100, 100, x0, 100, 14, -1 );
        wall( i, x0, 100, x0, 0, 15, i > 0 ? i - 1 : -1 );
    }
    op( GFXOP_W3DFOG, 1, 0.0 );
    op( GFXOP_W3DVIEW, 6, 0.0, 0.0, 320.0, 150.0, 1.0, 90.0 );
    op( GFXOP_W3DCAMERA, 5, 50.0, 50.0, 0.0, NONE, NONE );
    w3d_render( W );
    CHECK( Q( GFXQ_W3DVISITED, 0 ) >= 60.0 );       // (a column looks through at most 64 sectors)
    op( GFXOP_W3DFAR, 1, 1000.0 );
    w3d_render( W );
    CHECK( Q( GFXQ_W3DVISITED, 0 ) <= 12.0 );
    CHECK( Q( GFXQ_W3DVISITED, 0 ) >= 10.0 );
}

// -- time, movers, animated textures, scrolling, light effects, lamps, translucency ------------------------------------------------

static void gray_palette( void ) {
    uint32_t pal[256];
    for ( uint32_t i=0; i < 256; ++i ) {
        pal[i] = 0xFF000000U | ( i << 16 ) | ( i << 8 ) | i;
    }
    w3d_colormap( W, pal );
}

static void tex1x1( int n, int col, int w, int h ) {
    uint8_t s[ 4 + 512 ];
    s[0] = (uint8_t)( w >> 8 ); s[1] = (uint8_t) w; s[2] = (uint8_t)( h >> 8 ); s[3] = (uint8_t) h;
    for ( int i=0; i < w * h; ++i ) {
        s[4 + i] = (uint8_t) col;
    }
    ops( GFXOP_W3DTEXTURE, 1, (const char*) s, (size_t)( 4 + w * h ), (double) n );
}

static void test_m2( void ) {
    // movers and the clock
    build();
    CHECK( op( GFXOP_W3DSECMOVE, 4, 1.0, 20.0, 40.0, NONE ) );
    CHECK( Q( GFXQ_W3DMOVING, 1, 1.0 ) == -1.0 );
    CHECK( op( GFXOP_W3DTICK, 1, 1.0 ) );
    CHECK( Q( GFXQ_W3DFLOOR, 1, 1.0 ) == 20.0 );
    CHECK( Q( GFXQ_W3DTIME, 0 ) == 1.0 );
    CHECK( op( GFXOP_W3DTICK, 1, 1.5 ) );
    CHECK( Q( GFXQ_W3DFLOOR, 1, 1.0 ) == 40.0 );
    CHECK( Q( GFXQ_W3DMOVING, 1, 1.0 ) == 0.0 );
    CHECK( op( GFXOP_W3DSECMOVE, 4, 1.0, 100.0, 0.0, 60.0 ) );   // down again and the ceiling to 60
    CHECK( op( GFXOP_W3DTICK, 1, 0.1 ) );
    CHECK( fabs( Q( GFXQ_W3DFLOOR, 1, 1.0 ) - 30.0 ) < 1.0e-9 );
    CHECK( fabs( Q( GFXQ_W3DCEIL, 1, 1.0 ) - 118.0 ) < 1.0e-9 );  // 128 - 10
    CHECK( op( GFXOP_W3DHEIGHT, 3, 1.0, 0.0, 128.0 ) );           // a direct setting stops the mover
    CHECK( Q( GFXQ_W3DMOVING, 1, 1.0 ) == 0.0 );
    CHECK( !op( GFXOP_W3DSECMOVE, 4, 1.0, 0.0, 10.0, NONE ) );
    CHECK( !op( GFXOP_W3DSECMOVE, 4, 1.0, 10.0, 90.0, 80.0 ) );
    CHECK( !op( GFXOP_W3DTICK, 1, -1.0 ) );

    // animated textures: 72 shows 70 and 71 in turn, one per second
    build();
    tex1x1( 70, 100, 1, 1 );
    tex1x1( 71, 110, 1, 1 );
    CHECK( op( GFXOP_W3DANIM, 4, 72.0, 70.0, 2.0, 1.0 ) );
    op( GFXOP_W3DWALLTEX, 5, 0.0, 4.0, 72.0, -1.0, -1.0 );
    const uint8_t* fb = w3d_render( W );
    CHECK( px( fb, 160, 75 ) == 100 );
    op( GFXOP_W3DTICK, 1, 1.0 );
    fb = w3d_render( W );
    CHECK( px( fb, 160, 75 ) == 110 );
    op( GFXOP_W3DTICK, 1, 1.0 );
    fb = w3d_render( W );
    CHECK( px( fb, 160, 75 ) == 100 );
    CHECK( op( GFXOP_W3DANIM, 4, 72.0, 70.0, 0.0, 1.0 ) );        // off: the texture is 72 itself (not defined: its number is the color)
    fb = w3d_render( W );
    CHECK( px( fb, 160, 75 ) == 72 );
    CHECK( !op( GFXOP_W3DANIM, 4, 72.0, 1020.0, 10.0, 1.0 ) );
    CHECK( !op( GFXOP_W3DANIM, 4, 72.0, 70.0, 2.0, 0.0 ) );

    // scrolling walls: a texture of two columns (100, 110), the middle of the north wall shows column 0; one unit of scroll moves to column 1
    build();
    {
        uint8_t s[6] = { 0, 2, 0, 1, 100, 110 };
        ops( GFXOP_W3DTEXTURE, 1, (const char*) s, 6U, 73.0 );
    }
    op( GFXOP_W3DWALLTEX, 5, 0.0, 4.0, 73.0, -1.0, -1.0 );
    fb = w3d_render( W );
    CHECK( px( fb, 160, 75 ) == 100 );
    CHECK( op( GFXOP_W3DSCROLL, 4, 0.0, 2.0, 1.0, 0.0 ) );
    op( GFXOP_W3DTICK, 1, 1.0 );
    fb = w3d_render( W );
    CHECK( px( fb, 160, 75 ) == 110 );
    CHECK( !op( GFXOP_W3DSCROLL, 4, 0.0, 3.0, 1.0, 0.0 ) );

    // light effects
    build();
    CHECK( op( GFXOP_W3DLIGHTFX, 4, 0.0, 3.0, 255.0, 1.0 ) );     // strobe
    w3d_render( W );
    CHECK( Q( GFXQ_W3DLIGHTNOW, 1, 0.0 ) == 255.0 );
    op( GFXOP_W3DTICK, 1, 0.5 );
    w3d_render( W );
    CHECK( Q( GFXQ_W3DLIGHTNOW, 1, 0.0 ) == 0.0 );
    CHECK( op( GFXOP_W3DLIGHTFX, 4, 0.0, 2.0, 50.0, 1.0 ) );      // pulse around 255 (clamped), a quarter period later: +50, three quarters: -50
    op( GFXOP_W3DLIGHT, 2, 0.0, 150.0 );
    op( GFXOP_W3DTICK, 1, 0.75 );                                  // t = 1.25
    w3d_render( W );
    CHECK( Q( GFXQ_W3DLIGHTNOW, 1, 0.0 ) == 200.0 );
    op( GFXOP_W3DTICK, 1, 0.5 );                                   // t = 1.75
    w3d_render( W );
    CHECK( Q( GFXQ_W3DLIGHTNOW, 1, 0.0 ) == 100.0 );
    CHECK( op( GFXOP_W3DLIGHTFX, 4, 0.0, 1.0, 100.0, 10.0 ) );    // flicker: between 50 and 150, and not always the same
    int lo = 1000, hi = -1;
    for ( int i=0; i < 40; ++i ) {
        op( GFXOP_W3DTICK, 1, 0.1 );
        w3d_render( W );
        int l = (int) Q( GFXQ_W3DLIGHTNOW, 1, 0.0 );
        lo = l < lo ? l : lo;
        hi = l > hi ? l : hi;
    }
    CHECK( lo >= 50 && hi <= 150 && hi - lo > 20 );
    CHECK( op( GFXOP_W3DLIGHTFX, 4, 0.0, 0.0, NONE, NONE ) );
    w3d_render( W );
    CHECK( Q( GFXQ_W3DLIGHTNOW, 1, 0.0 ) == 150.0 );
    CHECK( !op( GFXOP_W3DLIGHTFX, 4, 0.0, 4.0, 10.0, 1.0 ) );

    // a lamp lights what is near it
    build();
    gray_palette();
    op( GFXOP_W3DFOG, 1, 0.0 );
    op( GFXOP_W3DLIGHT, 2, 0.0, 100.0 );
    op( GFXOP_W3DWALLTEX, 5, 0.0, 4.0, 200.0, -1.0, -1.0 );
    fb = w3d_render( W );
    int dark = px( fb, 160, 75 ), darkfloor = px( fb, 160, 147 );
    CHECK( dark > 0 && dark < 200 );
    CHECK( op( GFXOP_W3DLAMP, 6, 0.0, 200.0, 380.0, 46.0, 200.0, 155.0 ) );
    fb = w3d_render( W );
    CHECK( px( fb, 160, 75 ) > dark + 40 );
    CHECK( px( fb, 160, 100 ) > dark );                           // (the floor by the wall is lit as well)
    CHECK( px( fb, 160, 147 ) >= darkfloor );
    CHECK( op( GFXOP_W3DLAMP, 6, 0.0, 200.0, 380.0, 46.0, 0.0, 155.0 ) );      // off
    fb = w3d_render( W );
    CHECK( px( fb, 160, 75 ) == dark );
    CHECK( !op( GFXOP_W3DLAMP, 6, 8.0, 0.0, 0.0, 0.0, 10.0, 10.0 ) );

    // translucent walls: glass (100) in front of the east wall (200) gives 150, a masked wall gives the glass itself
    build();
    gray_palette();
    op( GFXOP_W3DFOG, 1, 0.0 );
    op( GFXOP_W3DCAMERA, 5, 200.0, 200.0, 0.0, NONE, NONE );
    tex1x1( 80, 100, 1, 128 );
    op( GFXOP_W3DWALLTEX, 5, 1.0, 1.0, 200.0, -1.0, -1.0 );
    op( GFXOP_W3DWALLTEX, 5, 0.0, 2.0, 80.0, -1.0, -1.0 );
    op( GFXOP_W3DWALLFLAG, 3, 0.0, 2.0, (double) W3D_WF_MASKED );
    fb = w3d_render( W );
    CHECK( px( fb, 160, 75 ) == 100 );
    op( GFXOP_W3DWALLFLAG, 3, 0.0, 2.0, (double) W3D_WF_TRANS );
    fb = w3d_render( W );
    CHECK( px( fb, 160, 75 ) == 150 );
    CHECK( px( fb, 160, 147 ) != 100 );
    // a translucent thing in front of the north wall
    build();
    gray_palette();
    op( GFXOP_W3DFOG, 1, 0.0 );
    op( GFXOP_W3DWALLTEX, 5, 0.0, 4.0, 200.0, -1.0, -1.0 );
    tex1x1( 81, 100, 1, 1 );
    op( GFXOP_W3DTHING, 6, 0.0, 200.0, 300.0, 81.0, 40.0, 60.0 );
    fb = w3d_render( W );
    CHECK( px( fb, 160, 75 ) == 100 );
    CHECK( op( GFXOP_W3DTHINGTRANS, 2, 0.0, 1.0 ) );
    fb = w3d_render( W );
    CHECK( px( fb, 160, 75 ) == 150 );
    CHECK( op( GFXOP_W3DTHINGTRANS, 2, 0.0, 0.0 ) );
    fb = w3d_render( W );
    CHECK( px( fb, 160, 75 ) == 100 );
    CHECK( !op( GFXOP_W3DTHINGTRANS, 2, 5.0, 1.0 ) );
}

int main( void ) {
    W = w3d_new();
    if ( W == 0 ) {
        printf( "cannot create the renderer\n" );
        return 1;
    }
    test_picture();
    test_move();
    test_things();
    test_queries();
    test_textures();
    test_masked_sky_light();
    test_concave();
    test_fuzz();
    test_hidden_things();
    test_m1();
    test_m2();
    w3d_free( W );
    printf( "w3dtest1: %d cases, %d failures\n", cases, failures );
    return failures;
}
