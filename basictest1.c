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

    NOTE: Programs created with a built-in programming language (if any),
          do not fall under this license.

    CONTACT INFO:
        E-Mail: ekkehard@ekkehardmorgenstern.de
        Mail: Ekkehard Morgenstern, Mozartstr. 1, D-76744 Woerth am Rhein,
              Germany, Europe
*/

#include "basic.h"

#define _CSI "\033["
#define _BLK "0"
#define _RED "1"
#define _GRN "2"
#define _YEL "3"
#define _BLU "4"
#define _MAG "5"
#define _CYA "6"
#define _WHI "7"
#define _FG "3"
#define _BG "4"
#define _SEP ";"
#define _SGR "m"
#define RED _CSI _FG _RED _SGR
#define GRN _CSI _FG _GRN _SGR
#define YEL _CSI _FG _YEL _SGR
#define BLU _CSI _FG _BLU _SGR
#define MAG _CSI _FG _MAG _SGR
#define CYA _CSI _FG _CYA _SGR
#define WHI _CSI _FG _WHI _SGR
#define NRM _CSI _SGR

static void pgm_printer( const char* text ) {
    printf( YEL "%s" NRM "\n", text );
}

static bool directmode( program_t* pgm, const uint8_t* tokens, size_t size ) {
    printf( "direct mode\n" );
    return false;
}

int main( int argc, char** argv ) {

    program_t pgm;
    init_program( &pgm );

    static char*  buf = 0;
    static size_t bufsize = 0;
    static buf_t* tokbuf = 0;
    static buf_t* txtbuf = 0;
    tokbuf = create_buffer(); txtbuf = create_buffer();
    if ( tokbuf == 0 || txtbuf == 0 ) {
        return EXIT_FAILURE;
    }

    for (;;) {
        ssize_t rd = getline( &buf, &bufsize, stdin );
        if ( rd < 0 ) {
            break;
        }
        size_t len = (size_t) rd;
        if ( len > 0 && buf[len-1U] == '\n' ) {    // remove LF
            buf[--len] = '\0';
        }
        preprocess_buffer( buf );
        printf( ">>%s<<\n", buf );
        linehdr_t hdr;
        clear_buffer( tokbuf );
        if ( !tokenize_line( buf, tokbuf, &hdr ) ) {
            printf( "? Syntax error\n" );
            continue;
        }

        printf( YEL "[" BLU "%u %u %u %u %u" YEL "]" NRM,
            (unsigned) hdr.node.nextoffs,
            (unsigned) hdr.node.prevoffs,
            (unsigned) hdr.lineno,
            (unsigned) hdr.length,
            (unsigned) hdr.alloc
        );

        const uint8_t* tokens = (const uint8_t*) tokbuf->buffer;
        len = hdr.length; const uint8_t* p = &tokens[sizeof(linehdr_t)];
        const uint8_t* e = &tokens[len];
        while ( p < e ) {
            uint8_t b = *p++;
            uint8_t c = b >= UINT8_C(32) && b <= UINT8_C(126) ? b : UINT8_C(46);
            printf( " " YEL "[" GRN "%02X " MAG "%c" YEL "]" NRM, (int) b, (int) c );
        }

        printf( "\n" );
        clear_buffer( txtbuf ); p = &tokens[sizeof(linehdr_t)];
        if ( !detokenize_line( txtbuf, p, &hdr ) ) {
            printf( "? Detok error\n" );
            continue;
        }
        printf( CYA "%s" NRM "\n", txtbuf->buffer );

        if ( !enter_line( &pgm, &tokens[0], directmode ) ) {
            fprintf( stderr, "enter failed\n" );
        }

        list_program( &pgm, LINENO_NONE, LINENO_NONE, pgm_printer );
    }

    free( buf );
    delete_buffer( tokbuf );
    delete_buffer( txtbuf );

    return 0;
}