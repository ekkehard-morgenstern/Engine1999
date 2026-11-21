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


// worldtest1: the static world (world.[ch], randseed.[ch]): creation, reading, determinism, errors. Exit status = number of failures. Works in $TMPDIR / the folder given as argument 1.

#include "world.h"
#include "randseed.h"
#include <sys/stat.h>
#include <unistd.h>

static int fails = 0, cases = 0;
#define CHECK(c) do { ++cases; if ( !(c) ) { ++fails; printf( "FAIL line %d: %s\n", __LINE__, #c ); } } while ( 0 )

static int lastdone = 0;
static void prog( void* ud, int done, int total ) { (void) ud; (void) total; lastdone = done; }

static bool same_file( const char* a, const char* b ) {
    FILE* fa = fopen( a, "rb" ); FILE* fb = fopen( b, "rb" );
    bool ok = fa != 0 && fb != 0;
    while ( ok ) {
        int ca = fgetc( fa ), cb = fgetc( fb );
        if ( ca != cb ) { ok = false; }
        if ( ca == EOF || cb == EOF ) { break; }
    }
    if ( fa != 0 ) { fclose( fa ); }
    if ( fb != 0 ) { fclose( fb ); }
    return ok;
}

int main( int argc, char** argv ) {
    const char* base = argc > 1 ? argv[1] : "/tmp";
    char d1[512], d2[512], d3[512], p1[600], p2[600], p3[600];
    snprintf( d1, sizeof(d1), "%s/worldtest1-a", base ); snprintf( d2, sizeof(d2), "%s/worldtest1-b", base ); snprintf( d3, sizeof(d3), "%s/worldtest1-c", base );
    snprintf( p1, sizeof(p1), "%s/world.dat", d1 ); snprintf( p2, sizeof(p2), "%s/world.dat", d2 ); snprintf( p3, sizeof(p3), "%s/world.dat", d3 );

    // the random field: the same seed gives the same state, a step south and east do not commute with nothing, different seeds differ
    randseed_t a, b;
    rs_seed( &a, 5 ); rs_seed( &b, 5 );
    CHECK( rs_fold( &a ) == rs_fold( &b ) );
    rs_south( &a ); rs_south( &b );
    CHECK( rs_fold( &a ) == rs_fold( &b ) );
    rs_east( &a );
    CHECK( rs_fold( &a ) != rs_fold( &b ) );
    rs_seed( &b, 6 );
    CHECK( rs_fold( &a ) != rs_fold( &b ) );
    CHECK( world_field_seed( 5, 3, 4 ) == world_field_seed( 5, 3, 4 ) );
    CHECK( world_field_seed( 5, 3, 4 ) != world_field_seed( 5, 4, 3 ) );
    CHECK( world_field_seed( 5, 3, 4 ) != world_field_seed( 6, 3, 4 ) );

    // errors
    CHECK( world_create( d1, 0, 1, 1, 0, 0 ) == 0 );
    CHECK( world_create( d1, 1001, 1, 1, 0, 0 ) == 0 );
    CHECK( world_open( "/nonexistent-world-folder" ) == 0 );
    CHECK( world_error()[0] != 0 );

    // creation: 24 x 24 chunks (12 km x 12 km... 24 * 500 m), twice with different thread counts: identical files
    world_t* w = world_create( d1, 24, 1987, 1, prog, 0 );
    CHECK( w != 0 );
    CHECK( lastdone == 24 );
    world_t* w2 = world_create( d2, 24, 1987, 4, 0, 0 );
    CHECK( w2 != 0 );
    CHECK( same_file( p1, p2 ) );
    struct stat st;
    CHECK( stat( p1, &st ) == 0 && st.st_size == WORLD_HDRSIZE + 24 * 24 * WORLD_RECSIZE );
    if ( w == 0 || w2 == 0 ) { printf( "world error: %s\n", world_error() ); return 1; }
    CHECK( world_size( w ) == 24 && world_seed( w ) == 1987 );

    // the records: the seeds are the field's, the summary lies inside the heights of the chunk, the edge is marked
    int bad = 0, edge = 0, inner = 0, tar = 0, water = 0, mount = 0;
    for ( int cy = 0; cy < 24; ++cy ) {
        for ( int cx = 0; cx < 24; ++cx ) {
            wchunk_t c;
            if ( !world_chunk( w, cx, cy, &c ) ) { ++bad; continue; }
            if ( c.seed != world_field_seed( 1987, cx, cy ) ) { ++bad; }
            if ( c.hmin > c.hmean || c.hmean > c.hmax ) { ++bad; }
            if ( c.kind > 3 ) { ++bad; }
            int e = cx < 4 || cy < 4 || cx >= 20 || cy >= 20;
            if ( ( c.flags & WCF_EDGE ) != 0 ) { ++edge; } else { ++inner; }
            if ( ( c.flags & WCF_EDGE ) != 0 && !e && cx > 4 && cx < 19 && cy > 4 && cy < 19 ) { ++bad; }
            if ( c.flags & WCF_TAR ) { ++tar; }
            if ( c.flags & WCF_WATER ) { ++water; }
            if ( c.flags & WCF_MOUNTAIN ) { ++mount; }
            // the height at the chunk's middle lies within the sampled range (loosely: the range is from 81 points)
            double h = world_height( w, world_x0( w, cx ) + 0.5 * WORLD_CHUNK, world_y0( w, cy ) + 0.5 * WORLD_CHUNK );
            if ( h < c.hmin - 20000.0 || h > c.hmax + 20000.0 ) { ++bad; }
        }
    }
    CHECK( bad == 0 );
    CHECK( edge > 0 && inner > 0 );
    CHECK( tar > 0 );
    wchunk_t c;
    CHECK( !world_chunk( w, -1, 0, &c ) && !world_chunk( w, 0, 24, &c ) && !world_chunk( w, 24, 0, &c ) );
    // the place -> chunk mapping
    int cx, cy;
    CHECK( world_chunk_at( w, 0.0, 0.0, &cx, &cy ) && cx == 12 && cy == 12 );
    CHECK( world_chunk_at( w, -0.5, -0.5, &cx, &cy ) && cx == 11 && cy == 11 );
    CHECK( world_chunk_at( w, -12.0 * WORLD_CHUNK, -12.0 * WORLD_CHUNK, &cx, &cy ) && cx == 0 && cy == 0 );
    CHECK( !world_chunk_at( w, -12.0 * WORLD_CHUNK - 1.0, 0.0, &cx, &cy ) && !world_chunk_at( w, 0.0, 12.0 * WORLD_CHUNK, &cx, &cy ) );
    CHECK( world_x0( w, 12 ) == 0.0 && world_y0( w, 0 ) == -12.0 * WORLD_CHUNK );
    // the same ground in both worlds and after reopening; the wall at the edge is high
    world_close( w );
    w = world_open( d1 );
    CHECK( w != 0 );
    int diff = 0;
    for ( int i=0; i < 200; ++i ) {
        double x = ( i * 7919 % 12000 - 6000 ) * 1.0, y = ( i * 104729 % 12000 - 6000 ) * 1.0;
        if ( world_height( w, x, y ) != world_height( w2, x, y ) ) { ++diff; }
    }
    CHECK( diff == 0 );
    CHECK( world_height( w, 0.0, 0.0 ) == world_height( w, 0.0, 0.0 ) );
    CHECK( world_height( w, 11.0 * WORLD_CHUNK, 0.0 ) > 30000.0 );             // in the wall
    CHECK( world_height( w, 0.0, 0.0 ) < 500.0 );                              // the start is quiet
    // another seed: another world; same seed in another size: the same ground, other chunk numbers
    world_t* w3 = world_create( d3, 24, 1988, 2, 0, 0 );
    CHECK( w3 != 0 );
    CHECK( !same_file( p1, p3 ) );
    CHECK( world_height( w3, 3000.0, 4000.0 ) != world_height( w, 3000.0, 4000.0 ) );
    world_close( w3 );

    // an unfinished or damaged file is refused
    FILE* f = fopen( p3, "r+b" );
    CHECK( f != 0 );
    if ( f != 0 ) {
        fseek( f, 32, SEEK_SET ); fputc( 0, f ); fseek( f, 35, SEEK_SET ); fputc( 0, f );     // state 0 (the lowest byte of the state word at 32 .. 35)
        fclose( f );
        CHECK( world_open( d3 ) == 0 );
        f = fopen( p3, "r+b" );
        fseek( f, 35, SEEK_SET ); fputc( 1, f );
        fclose( f );
        CHECK( truncate( p3, WORLD_HDRSIZE + 100 ) == 0 );
        CHECK( world_open( d3 ) == 0 );
        f = fopen( p3, "wb" ); fputs( "not a world file at all, just text", f ); fclose( f );
        CHECK( world_open( d3 ) == 0 );
    }

    // a one chunk world works, and a speed figure for the owner (not a check): 100 x 100 chunks
    world_t* w1 = world_create( d3, 1, 7, 0, 0, 0 );
    CHECK( w1 != 0 && world_chunk( w1, 0, 0, &c ) );
    world_close( w1 );
    struct timespec t0, t1;
    clock_gettime( CLOCK_MONOTONIC, &t0 );
    world_t* w4 = world_create( d3, 100, 3, 0, 0, 0 );
    clock_gettime( CLOCK_MONOTONIC, &t1 );
    CHECK( w4 != 0 );
    printf( "100 x 100 chunks made in %.2f s\n", ( t1.tv_sec - t0.tv_sec ) + 1e-9 * ( t1.tv_nsec - t0.tv_nsec ) );
    world_close( w4 ); world_close( w ); world_close( w2 );
    unlink( p1 ); unlink( p2 ); unlink( p3 ); rmdir( d1 ); rmdir( d2 ); rmdir( d3 );
    printf( "worldtest1: %d cases, %d failures\n", cases, fails );
    return fails;
}
