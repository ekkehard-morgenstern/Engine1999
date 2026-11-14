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

// Automated test of the screenshot module (shot.[ch]): compositing, the BMP encoder, names, the request slot between two threads.

#include "stdtypes.h"
#include "shot.h"
#include <pthread.h>

static int total = 0, failures = 0;

static void check( bool cond, const char* name ) {
    ++total;
    if ( !cond ) {
        printf( "FAIL  %s\n", name );
        ++failures;
    }
}

#define NPIX (SHOT_WIDTH * SHOT_HEIGHT)

static void test_compose( void ) {
    uint8_t* m0 = (uint8_t*) calloc( NPIX, 1 ); uint8_t* m1 = (uint8_t*) calloc( NPIX, 1 ); uint8_t* m2 = (uint8_t*) calloc( NPIX, 1 );
    uint32_t* out = (uint32_t*) malloc( sizeof(uint32_t) * NPIX );
    uint32_t p0[256], p1[256], p2[256];
    memset( p0, 0, sizeof(p0) ); memset( p1, 0, sizeof(p1) ); memset( p2, 0, sizeof(p2) );
    p0[1] = 0xFFFF0000U;                    // opaque red
    p1[1] = 0x8000FF00U;                    // green at about 50 %
    p2[1] = 0xFF0000FFU;                    // opaque blue
    m0[10] = 1; m1[10] = 1; m1[11] = 1; m2[12] = 1; m1[12] = 1;
    shot_layer_t l[3] = { { m0, p0, true, 0 }, { m1, p1, true, 1 }, { m2, p2, true, 2 } };
    shot_compose( l, 3, 0x00102030U, out );
    check( out[0] == 0xFF102030U, "the backdrop is opaque and shows where every layer is transparent" );
    check( out[10] == 0xFF7F8000U, "a half transparent green over opaque red blends (127, 128, 0)" );
    uint32_t g_over_bg = out[11];
    check( ( g_over_bg >> 8 & 255U ) > 0x80U && ( g_over_bg >> 16 & 255U ) < 0x20U + 0x10U, "a half transparent green over the backdrop" );
    check( out[12] == 0xFF0000FFU, "an opaque pixel of a higher layer covers everything below" );
    // priorities: swap the order of layer 0 and 2 (red on top of blue where both are set)
    m0[12] = 1;
    l[0].priority = 2; l[2].priority = 0;
    shot_compose( l, 3, 0, out );
    check( out[12] == 0xFFFF0000U, "the priority decides the order, not the array position" );
    l[0].enabled = false;
    shot_compose( l, 3, 0, out );
    check( out[10] == 0xFF008000U, "a disabled layer is not drawn (the green over the black backdrop)" );
    check( out[12] == 0xFF00807FU, "... and the layers below it show (green over blue)" );
    // a 32 bit layer between them: its ARGB pixels are used as they are, the palette and the indices are not looked at
    uint32_t* a1 = (uint32_t*) calloc( NPIX, sizeof(uint32_t) );
    a1[10] = 0xFF00FFFFU; a1[11] = 0x80FFFFFFU;
    shot_layer_t l32[3] = { { m0, p0, true, 0, 0 }, { 0, 0, true, 1, a1 }, { m2, p2, true, 2, 0 } };
    shot_compose( l32, 3, 0x00000000U, out );
    check( out[10] == 0xFF00FFFFU, "an opaque pixel of a 32 bit layer covers the layer below" );
    check( ( out[11] & 0xFFU ) == 0x80U && ( out[11] >> 16 & 255U ) == 0x80U, "a half transparent one blends with the backdrop (128)" );
    check( out[12] == 0xFF0000FFU, "a transparent pixel of a 32 bit layer shows the layers around it" );
    l32[1].enabled = false;
    shot_compose( l32, 3, 0, out );
    check( out[10] == 0xFFFF0000U, "a disabled 32 bit layer is not drawn" );
    free( a1 );
    free( m0 ); free( m1 ); free( m2 ); free( out );
}

static uint32_t rd32( const uint8_t* p ) { return (uint32_t) p[0] | (uint32_t) p[1] << 8 | (uint32_t) p[2] << 16 | (uint32_t) p[3] << 24; }

static void test_encode( void ) {
    uint32_t* px = (uint32_t*) malloc( sizeof(uint32_t) * NPIX );
    for ( int i=0; i < NPIX; ++i ) px[i] = 0xFF000000U | ( (uint32_t)( i % 251 ) << 16 ) | ( (uint32_t)( i % 241 ) << 8 ) | (uint32_t)( i % 239 );
    px[0] = 0xFF112233U;                                    // the top left pixel
    px[(SHOT_HEIGHT-1) * SHOT_WIDTH + SHOT_WIDTH - 1] = 0xFF445566U;     // the bottom right pixel
    uint8_t* d = 0; size_t len = 0;
    check( shot_encode( px, &d, &len ), "shot_encode" );
    size_t stride = ( SHOT_WIDTH * 3 + 3 ) & ~3U;
    check( len == 54 + stride * SHOT_HEIGHT, "the file size: header 54 + padded rows" );
    check( d[0] == 'B' && d[1] == 'M' && rd32( d + 2 ) == len && rd32( d + 10 ) == 54 && rd32( d + 14 ) == 40, "the BMP header" );
    check( rd32( d + 18 ) == SHOT_WIDTH && rd32( d + 22 ) == SHOT_HEIGHT && d[26] == 1 && d[28] == 24 && rd32( d + 30 ) == 0, "640 x 300, 24 bits, uncompressed" );
    const uint8_t* last = d + 54 + stride * ( SHOT_HEIGHT - 1 );    // the first row of the file is the bottom of the picture, the last row the top
    check( last[0] == 0x33 && last[1] == 0x22 && last[2] == 0x11, "the top left pixel is in the last file row, as B G R" );
    const uint8_t* first = d + 54 + 3 * ( SHOT_WIDTH - 1 );
    check( first[0] == 0x66 && first[1] == 0x55 && first[2] == 0x44, "the bottom right pixel is in the first file row" );
    free( d ); free( px );
}

static void test_names( void ) {
    char n[80];
    struct tm tmv; memset( &tmv, 0, sizeof(tmv) );
    tmv.tm_year = 2026 - 1900; tmv.tm_mon = 10; tmv.tm_mday = 9; tmv.tm_hour = 7; tmv.tm_min = 5; tmv.tm_sec = 3; tmv.tm_isdst = -1;
    time_t t = mktime( &tmv );
    shot_autoname( n, sizeof(n), t, 7 );
    check( strcmp( n, "shot-20261109-070503-7.bmp" ) == 0, "shot-YYYYMMDD-HHMMSS-n.bmp" );
    unsigned a = shot_nextnumber(), b = shot_nextnumber();
    check( a >= 1 && b == a + 1, "the running number counts" );
    shot_autoname( n, 12, t, 1 );
    check( strlen( n ) == 11, "a short buffer is cut, not overrun" );
}

static void test_file( void ) {
    uint32_t* px = (uint32_t*) calloc( NPIX, sizeof(uint32_t) );
    const char* err = 0;
    check( shot_write_file( "shottest1.tmp.bmp", px, &err ), "shot_write_file writes a file" );
    FILE* f = fopen( "shottest1.tmp.bmp", "rb" );
    uint8_t h[2] = { 0, 0 };
    check( f != 0 && fread( h, 1, 2, f ) == 2 && h[0] == 'B' && h[1] == 'M', "the file starts with BM" );
    if ( f ) fclose( f );
    remove( "shottest1.tmp.bmp" );
    check( !shot_write_file( "/nonexistent-dir/x.bmp", px, &err ) && err != 0, "an unwritable name fails with a message" );
    free( px );
}

static volatile int screen_run = 0;
static void* screen_thread( void* arg ) {
    (void) arg;
    uint32_t* pic = (uint32_t*) malloc( sizeof(uint32_t) * NPIX );
    uint32_t frame = 0;
    while ( __atomic_load_n( &screen_run, __ATOMIC_ACQUIRE ) ) {
        if ( shot_wanted() ) {
            for ( int i=0; i < NPIX; ++i ) pic[i] = 0xFF000000U | ( frame + (uint32_t) i );
            shot_serve( pic );
            ++frame;
        }
        struct timespec ts = { 0, 1000000L };
        nanosleep( &ts, 0 );
    }
    free( pic );
    return 0;
}

static void test_slot( void ) {
    uint32_t* out = (uint32_t*) malloc( sizeof(uint32_t) * NPIX );
    // nobody serves: the request is withdrawn after about a second
    check( !shot_grab( out ), "without a screen thread the request fails" );
    check( !shot_wanted(), "... and is withdrawn" );
    pthread_t th;
    __atomic_store_n( &screen_run, 1, __ATOMIC_RELEASE );
    pthread_create( &th, 0, screen_thread, 0 );
    bool ok = true; int bad = 0;
    for ( int k=0; k < 200; ++k ) {
        if ( !shot_grab( out ) ) { ok = false; break; }
        // each picture is whole: pixel i = 0xFF000000 | ( frame + i ) with the frame of that grab
        uint32_t base = ( out[0] & 0x00FFFFFFU );
        for ( int i=1; i < NPIX; i += 997 ) if ( out[i] != ( 0xFF000000U | ( base + (uint32_t) i ) ) ) { ++bad; break; }
    }
    check( ok, "200 requests are all served" );
    check( bad == 0, "the pictures are not torn" );
    __atomic_store_n( &screen_run, 0, __ATOMIC_RELEASE );
    pthread_join( th, 0 );
    free( out );
}

int main( void ) {
    test_compose();
    test_encode();
    test_names();
    test_file();
    test_slot();
    printf( "shottest1: %d checks, %d failure(s)\n", total, failures );
    return failures;
}
