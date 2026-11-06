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

// automated test of the syntax highlighter (synhl.c): the classes it gives agree with the ones LIST uses (detokenize_line_hl) on every line of the demos

#include "synhl.h"
#include "baslin.h"
#include <glob.h>

static int checks = 0, failures = 0;

#define CHECK( cond, ... ) do { ++checks; if ( !( cond ) ) { ++failures; printf( "FAIL (line %d): ", __LINE__ ); printf( __VA_ARGS__ ); printf( "\n" ); } } while ( 0 )

// the sequence of classes of the non-blank characters, equal neighbours merged: "13,2,..." as text
static void runs_of_synhl( const char* line, char* out, size_t cap ) {
    size_t n = strlen( line );
    uint8_t* cls = malloc( n + 1U );
    synhl_line( line, n, cls );
    int last = -1;
    size_t o = 0;
    out[0] = '\0';
    for ( size_t i = 0; i < n; ++i ) {
        if ( line[i] == ' ' || line[i] == '\t' ) {
            continue;
        }
        if ( cls[i] != last && o + 4 < cap ) {
            o += (size_t) snprintf( out + o, cap - o, "%d,", cls[i] );
            last = cls[i];
        }
    }
    free( cls );
}

static const char* mark( hlclass_t c, void* u ) {
    static char s[8];
    (void) u;
    snprintf( s, sizeof(s), "\x01%d,", (int) c );
    return c == HL_NONE ? "\x01" "0," : s;
}

static void runs_of_list( const char* line, char* out, size_t cap ) {
    char* work = strdup( line );
    preprocess_buffer( work );
    buf_t* tb = create_buffer();
    linehdr_t hdr;
    out[0] = '\0';
    if ( tokenize_line( work, tb, &hdr ) ) {
        buf_t* ob = create_buffer();
        if ( detokenize_line_hl( ob, (const uint8_t*) tb->buffer + LINEHDR_SIZE, &hdr, mark, 0 ) ) {
            // markers: \x01<class>, ; the class is valid for the following text up to the next marker; a class with only blanks gives no run
            const char* s = ob->buffer;
            size_t o = 0;
            int cur = -1, last = -1;
            for ( ; *s != '\0'; ++s ) {
                if ( *s == '\x01' ) {
                    cur = atoi( s + 1 );
                    while ( *s != ',' ) ++s;
                    continue;
                }
                if ( *s == ' ' || cur == 0 ) {
                    continue;
                }
                if ( cur != last && o + 4 < cap ) {
                    o += (size_t) snprintf( out + o, cap - o, "%d,", cur );
                    last = cur;
                }
            }
        }
        delete_buffer( ob );
    } else {
        snprintf( out, cap, "FAIL" );
    }
    delete_buffer( tb );
    free( work );
}

static void compare_line( const char* line, const char* where ) {
    char a[2048], b[2048];
    runs_of_synhl( line, a, sizeof(a) );
    runs_of_list( line, b, sizeof(b) );
    if ( strcmp( b, "FAIL" ) == 0 ) {
        return;                                                     // not a tokenizable line (never mind)
    }
    CHECK( strcmp( a, b ) == 0, "%s: %s\n   synhl %s\n   list  %s", where, line, a, b );
}

static void one( const char* line, const char* expect ) {          // a spot check: the classes as a string of hex digits (0 = plain)
    size_t n = strlen( line );
    uint8_t* cls = malloc( n + 1U );
    char got[512];
    synhl_line( line, n, cls );
    for ( size_t i = 0; i < n; ++i ) {
        got[i] = (char)( cls[i] < 10 ? '0' + cls[i] : 'a' + cls[i] - 10 );
    }
    got[n] = '\0';
    CHECK( strcmp( got, expect ) == 0, "\"%s\": got %s, expected %s", line, got, expect );
    free( cls );
}

static void file( const char* path ) {
    FILE* f = fopen( path, "r" );
    if ( f == 0 ) {
        return;
    }
    char line[8192];
    int no = 0;
    while ( fgets( line, sizeof(line), f ) != 0 ) {
        size_t n = strlen( line );
        while ( n > 0 && ( line[n-1] == '\n' || line[n-1] == '\r' ) ) {
            line[--n] = '\0';
        }
        ++no;
        char where[300];
        snprintf( where, sizeof(where), "%s:%d", path, no );
        if ( strstr( path, "demos/" ) != 0 && line[0] != '\0' ) {            // a demo is a numbered listing: a line without a number would make LOAD fail ("Line number missing in program file")
            CHECK( line[0] >= '0' && line[0] <= '9', "%s: the line has no line number: %s", where, line );
        }
        compare_line( line, where );
    }
    fclose( f );
}

int main( int argc, char** argv ) {
    (void) argc; (void) argv;
    // classes: 0 plain, 1 line number, 2 statement, 3 control, 4 command, 5 function, 6 operator, 7 ident, 8 number, 9 string, a shell, b comment, c punctuation
    one( "", "" );
    one( "10 PRINT \"HI\"", "1102222209999" );
    one( "10 print x$", "11022222077" );
    one( "A=B+1", "76768" );
    one( "A = 12.5 ' note", "706088880bbbbbb" );
    one( "REM a comment", "bbbbbbbbbbbbb" );
    one( "X=1: REM tail", "768c0bbbbbbbb" );
    one( "IF A THEN GOTO 10", "33070333303333088" );
    one( "S$=\"it's\"", "776999999" );
    one( "S$=\"open", "77699999" );
    one( "X=&HFF", "768888" );
    one( "`ls -l`", "aaaaaaa" );
    one( "A<=B", "7667" );
    for ( int i = 1; i < argc; ++i ) {
        file( argv[i] );
    }
    if ( argc == 1 ) {                                              // no files given: the demos and their libraries (run in the build directory)
        const char* pats[] = { "demos/*.bas", "demos/lib/*.lib" };
        for ( int k = 0; k < 2; ++k ) {
            glob_t gl;
            if ( glob( pats[k], 0, 0, &gl ) == 0 ) {
                for ( size_t i = 0; i < gl.gl_pathc; ++i ) {
                    file( gl.gl_pathv[i] );
                }
            }
            globfree( &gl );
        }
    }
    printf( "synhltest1: %d checks, %d failures\n", checks, failures );
    return failures;
}
