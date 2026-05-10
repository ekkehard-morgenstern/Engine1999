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

#include "bascomp.h"
#include "stdtypes.h"
#include "unxtypes.h"
#include "sdlutil.h"

/*
TEST for the variable and strings subsystems
*/

static FILE* fprand = 0;

static int32_t getrand( int32_t modulo ) {
    int32_t val = INT32_C(0);
    if ( fprand == 0 ) {
        fprand = fopen( "/dev/urandom", "rb" );
        if ( fprand == 0 ) {
            perror( "fopen(3) /dev/urandom" );
            exit( 1 );
        }
    }
    if ( fread( &val, sizeof(int32_t), 1U, fprand ) != 1 ) {
        fprintf( stderr, "/dev/urandom: I/O error\n" );
        exit( 1 );
    }
    return ( val & INT32_MAX ) % modulo;
}

static void getrandstr( char buf[256], uint8_t* plen ) {
    uint8_t len = (uint8_t) getrand( INT32_C(256) );
    if ( len ) {
        if ( fread( buf, len, 1U, fprand ) != 1 ) {
            fprintf( stderr, "/dev/urandom: I/O error\n" );
            exit( 1 );
        }
    }
    buf[ len ] = '\0';
    for ( uint8_t i=0; i < len; ++i ) {
        uint8_t b = buf[i];
        if ( b < UINT8_C(32) || b > UINT8_C(126) ) {
            // non-printable character: try again
            b &= UINT8_C(127);
            if ( b < UINT8_C(32) ) {
                b = UINT8_C(32);
            } else if ( b > UINT8_C(126) ) {
                b = UINT8_C(126);
            }
            buf[i] = b;
        }
    }
    if ( plen ) {
        *plen = len;
    }
}

static jmp_buf errorjmp;

static void fatal_error( compiler_t* comp, void* usr ) {
    longjmp( errorjmp, 1 );
}

static void report_error( compiler_t* comp, void* user, const char* text ) {
    fprintf( stderr, "? %s\n", text );
}

// book keeping info for verification purposes
typedef union _keepvar_t {
    double  dval;
    int16_t ival;
    char*   sval;
} keepvar_t;

static void init_keepvar( uint8_t vartype, keepvar_t* var ) {
    switch ( vartype & VARTYPEM_BASE ) {
        case VARTYPEV_FLOAT:
            var->dval = 0;
            break;
        case VARTYPEV_INT:
            var->ival = 0;
            break;
        case VARTYPEV_STR:
            var->sval = 0;
            break;
        default:
            fprintf( stderr, "? invalid var type\n" );
            exit( 1 );
    }
}

static void setrand_keepvar( uint8_t vartype, keepvar_t* var ) {
    char buf[256];
    switch ( vartype & VARTYPEM_BASE ) {
        case VARTYPEV_FLOAT:
            var->dval = (double) getrand( INT32_MAX );
            break;
        case VARTYPEV_INT:
            var->ival = (int16_t) getrand( INT16_MAX );
            break;
        case VARTYPEV_STR:
            getrandstr( buf, 0 );
            if ( var->sval ) {
                free( var->sval );
            }
            var->sval = strdup( buf );
            if ( var->sval == 0 ) {
                fprintf( stderr, "? out of memory\n" );
                exit( 1 );
            }
            break;
        default:
            fprintf( stderr, "? invalid var type\n" );
            exit( 1 );
    }
}

typedef struct _keep_t {
    int eff;

} keep_t;

int main( int argc, char** argv ) {

    if ( setjmp( errorjmp ) != 0 ) {
        fprintf( stderr, "*** FATAL, stop\n" );
        return 1;
    }

    compiler_t comp;
    init_compiler( &comp, 0, false );
    comp.halt = fatal_error;
    comp.report = report_error;

    for (;;) {

        int32_t v = getrand( 10 );

        if ( v < 3 ) {  // allocate or remove a variable

        } else if ( v < 7 ) {   // manipulate a string


        } else {    // do nothing
            sdlutil_nanosleep( UINT64_C(1000000000) / 100, 0 );
        }

    }



    return 0;
}