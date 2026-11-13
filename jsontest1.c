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


#include "json.h"

/*
Automated test of the JSON module (json.[ch], no SDL): parser (valid and invalid texts, escapes, numbers), paths, making and deleting elements,
the writer (compact / pretty, round trip), value texts, and a fuzz run with random and mutated texts. The exit status is the number of failures.
*/

static int failures = 0, cases = 0;

#define CHECK(cond) do { ++cases; if ( !(cond) ) { ++failures; printf( "FAIL line %d: %s\n", __LINE__, #cond ); } } while (0)

static json_node_t* P( const char* text ) {
    json_node_t* n = 0;
    char err[100];
    if ( !json_parse( text, strlen( text ), &n, err, sizeof(err) ) ) {
        return 0;
    }
    return n;
}

static bool bad( const char* text ) {
    json_node_t* n = 0;
    char err[100];
    bool ok = json_parse( text, strlen( text ), &n, err, sizeof(err) );
    if ( ok ) {
        json_node_free( n );
    }
    return !ok && err[0] != '\0';
}

static json_node_t* F( json_node_t* r, const char* path ) {
    return json_find( r, path, strlen( path ) );
}

static const char* TS( json_node_t* r, const char* path ) {
    static char buf[2000];
    size_t len;
    char* v = json_valuetext( F( r, path ), &len );
    snprintf( buf, sizeof(buf), "%s", v != 0 ? v : "(null)" );
    free( v );
    return buf;
}

static void test_parse( void ) {
    json_node_t* r = P( "  {\"a\": {\"b\": [1, 2.5, -3e2, true, false, null, \"x\"], \"c\": \"hi\"}, \"d\": []}  " );
    CHECK( r != 0 && r->type == JSON_OBJ );
    CHECK( F( r, "a" ) != 0 && F( r, "a" )->type == JSON_OBJ );
    CHECK( strcmp( TS( r, "a.b.0" ), "1" ) == 0 );
    CHECK( strcmp( TS( r, "a.b.1" ), "2.5" ) == 0 );
    CHECK( strcmp( TS( r, "a.b.2" ), "-300" ) == 0 );
    CHECK( strcmp( TS( r, "a.b.3" ), "true" ) == 0 );
    CHECK( strcmp( TS( r, "a.b.4" ), "false" ) == 0 );
    CHECK( F( r, "a.b.5" ) != 0 && F( r, "a.b.5" )->type == JSON_NULL );
    CHECK( strcmp( TS( r, "a.b.6" ), "x" ) == 0 );
    CHECK( strcmp( TS( r, "a.b[6]" ), "x" ) == 0 );
    CHECK( strcmp( TS( r, "a.c" ), "hi" ) == 0 );
    CHECK( F( r, "a.b.7" ) == 0 );
    CHECK( F( r, "a.x" ) == 0 );
    CHECK( F( r, "a.c.z" ) == 0 );
    CHECK( F( r, "d.0" ) == 0 );
    CHECK( F( r, "" ) == r );
    CHECK( F( r, "a.b.-1" ) == 0 );
    CHECK( F( r, "a.b[2" ) == 0 );
    CHECK( strcmp( json_typename( F( r, "a.b" ) ), "array" ) == 0 );
    CHECK( strcmp( json_typename( F( r, "a.b.3" ) ), "boolean" ) == 0 );
    CHECK( strcmp( json_typename( F( r, "nope" ) ), "" ) == 0 );
    CHECK( F( r, "a.b" )->nkids == 7 );
    CHECK( strcmp( F( r, "a" )->kids[1]->key, "c" ) == 0 );            // order kept
    json_node_free( r );
    // strings: escapes, \u, surrogates, UTF-8 passes through
    r = P( "[\"a\\n\\t\\\"\\\\\\/\\b\\f\\r\", \"\\u00e4\\u20ac\", \"\\ud83d\\ude00\", \"\xc3\xa4\", \"\"]" );
    CHECK( r != 0 );
    if ( r != 0 ) {
        CHECK( F( r, "0" )->slen == 9 && memcmp( F( r, "0" )->str, "a\n\t\"\\/\b\f\r", 9 ) == 0 );
        CHECK( strcmp( F( r, "1" )->str, "\xc3\xa4\xe2\x82\xac" ) == 0 );
        CHECK( strcmp( F( r, "2" )->str, "\xf0\x9f\x98\x80" ) == 0 );
        CHECK( strcmp( F( r, "3" )->str, "\xc3\xa4" ) == 0 );
        CHECK( F( r, "4" )->slen == 0 );
    }
    json_node_free( r );
    // a string with an embedded NUL (\u0000) keeps its length
    r = P( "[\"a\\u0000b\"]" );
    CHECK( r != 0 && F( r, "0" )->slen == 3 && F( r, "0" )->str[1] == '\0' );
    json_node_free( r );
    // numbers
    r = P( "[0, -0, 12345678901234, 0.1, 1E3, 1e-2, 123456789012345678]" );
    CHECK( r != 0 );
    if ( r != 0 ) {
        CHECK( strcmp( TS( r, "0" ), "0" ) == 0 && strcmp( TS( r, "1" ), "0" ) == 0 );
        CHECK( strcmp( TS( r, "2" ), "12345678901234" ) == 0 );
        CHECK( strcmp( TS( r, "3" ), "0.1" ) == 0 );
        CHECK( strcmp( TS( r, "4" ), "1000" ) == 0 );
        CHECK( strcmp( TS( r, "5" ), "0.01" ) == 0 );
        CHECK( fabs( F( r, "6" )->num - 123456789012345678.0 ) < 1.0e3 );
    }
    json_node_free( r );
    // duplicate keys: the last value wins, at the first place
    r = P( "{\"a\":1,\"b\":2,\"a\":3}" );
    CHECK( r != 0 && r->nkids == 2 && strcmp( TS( r, "a" ), "3" ) == 0 && strcmp( r->kids[0]->key, "a" ) == 0 );
    json_node_free( r );
    // a byte order mark, bare values
    r = P( "\xef\xbb\xbf" "42" );
    CHECK( r != 0 && r->type == JSON_NUM && r->num == 42.0 );
    json_node_free( r );
    r = P( "\"x\"" );
    CHECK( r != 0 && r->type == JSON_STR );
    json_node_free( r );
    // errors
    CHECK( bad( "" ) );
    CHECK( bad( "{" ) );
    CHECK( bad( "[1,]" ) );
    CHECK( bad( "{\"a\":1,}" ) );
    CHECK( bad( "{a:1}" ) );
    CHECK( bad( "{\"a\" 1}" ) );
    CHECK( bad( "[1 2]" ) );
    CHECK( bad( "[01]" ) );
    CHECK( bad( "[1.]" ) );
    CHECK( bad( "[.5]" ) );
    CHECK( bad( "[1e]" ) );
    CHECK( bad( "[+1]" ) );
    CHECK( bad( "[nul]" ) );
    CHECK( bad( "[\"abc]" ) );
    CHECK( bad( "[\"a\\x\"]" ) );
    CHECK( bad( "[\"\\u12\"]" ) );
    CHECK( bad( "[\"\\ud800\"]" ) );
    CHECK( bad( "[\"\\udc00\"]" ) );
    CHECK( bad( "[\"a\tb\"]" ) );
    CHECK( bad( "1 2" ) );
    CHECK( bad( "[1]]" ) );
    CHECK( bad( "'a'" ) );
    CHECK( bad( "[NaN]" ) );
    {
        char deep[400];
        memset( deep, '[', 150 );
        memcpy( deep + 150, "]", 1 );
        deep[151] = '\0';
        CHECK( bad( deep ) );
        memset( deep, '[', 50 );
        memset( deep + 50, ']', 50 );
        deep[100] = '\0';
        r = P( deep );
        CHECK( r != 0 );
        json_node_free( r );
    }
    // the message names the line
    {
        json_node_t* n = 0;
        char err[100];
        const char* t = "{\n\"a\": 1,\n\"b\": ?\n}";
        CHECK( !json_parse( t, strlen( t ), &n, err, sizeof(err) ) && strstr( err, "line 3" ) != 0 );
    }
}

static void test_make( void ) {
    json_node_t* r = 0;
    const char* err;
    json_node_t* n = json_make( &r, "a.b.c", 5, &err );
    CHECK( n != 0 && r != 0 && r->type == JSON_OBJ );
    CHECK( n != 0 && n->type == JSON_NULL );
    if ( n != 0 ) {
        n->type = JSON_NUM;
        n->num = 7.0;
    }
    CHECK( strcmp( TS( r, "a.b.c" ), "7" ) == 0 );
    // an array made by a number component, append at the end
    n = json_make( &r, "list.0.x", 8, &err );
    CHECK( n != 0 && F( r, "list" ) != 0 && F( r, "list" )->type == JSON_ARR && F( r, "list.0" )->type == JSON_OBJ );
    n = json_make( &r, "list.1", 6, &err );
    CHECK( n != 0 && F( r, "list" )->nkids == 2 );
    n = json_make( &r, "list.5", 6, &err );
    CHECK( n == 0 && err != 0 );
    n = json_make( &r, "list.name", 9, &err );
    CHECK( n == 0 && err != 0 );
    n = json_make( &r, "a.b.c.d", 7, &err );             // through a number
    CHECK( n == 0 && err != 0 );
    n = json_make( &r, "a..b", 4, &err );
    CHECK( n == 0 );
    n = json_make( &r, "a.b.c", 5, &err );              // the same element again
    CHECK( n != 0 && n->num == 7.0 );
    n = json_make( &r, "", 0, &err );
    CHECK( n == r );
    // [n] forms
    n = json_make( &r, "m[0][0]", 7, &err );
    CHECK( n != 0 && F( r, "m" )->type == JSON_ARR );
    // a string
    n = json_make( &r, "s", 1, &err );
    CHECK( n != 0 && json_node_setstr( n, "hello", 5 ) && strcmp( TS( r, "s" ), "hello" ) == 0 );
    // delete
    CHECK( json_delete( &r, "list.0", 6 ) && F( r, "list" )->nkids == 1 );
    CHECK( json_delete( &r, "a.b", 3 ) && F( r, "a.b" ) == 0 && F( r, "a" ) != 0 );
    CHECK( !json_delete( &r, "a.b", 3 ) );
    CHECK( !json_delete( &r, "zz.y", 4 ) );
    CHECK( !json_delete( &r, "list.9", 6 ) );
    CHECK( json_delete( &r, "", 0 ) && r == 0 );
    CHECK( !json_delete( &r, "x", 1 ) );
}

static void test_write( void ) {
    const char* src = "{\"name\":\"Deqq\\n\",\"pos\":[1,2.5,-3],\"nest\":{\"a\":[{\"b\":true},{\"c\":null}],\"e\":{},\"f\":[]},\"q\":\"\\u0001\\\"\"}";
    json_node_t* r = P( src );
    CHECK( r != 0 );
    size_t len;
    char* compact = json_print( r, false, &len );
    CHECK( compact != 0 && strcmp( compact, "{\"name\":\"Deqq\\n\",\"pos\":[1,2.5,-3],\"nest\":{\"a\":[{\"b\":true},{\"c\":null}],\"e\":{},\"f\":[]},\"q\":\"\\u0001\\\"\"}" ) == 0 );
    char* pretty = json_print( r, true, &len );
    CHECK( pretty != 0 && strstr( pretty, "\"pos\": [1, 2.5, -3]" ) != 0 && strstr( pretty, "\n    \"a\": [" ) != 0 );
    // both read back to the same tree
    json_node_t* r2 = P( pretty );
    CHECK( r2 != 0 );
    char* again = r2 != 0 ? json_print( r2, false, &len ) : 0;
    CHECK( again != 0 && compact != 0 && strcmp( again, compact ) == 0 );
    free( again );
    free( pretty );
    free( compact );
    json_node_free( r2 );
    json_node_free( r );
    // numbers survive the round trip exactly
    r = P( "[0.1, 1e300, 3.141592653589793, 123456.789, 1e-7, 100, -2.5e-3]" );
    char* t1 = json_print( r, false, &len );
    r2 = P( t1 );
    CHECK( r != 0 && r2 != 0 );
    for ( size_t i=0; r != 0 && r2 != 0 && i < r->nkids; ++i ) {
        CHECK( r->kids[i]->num == r2->kids[i]->num );
    }
    free( t1 );
    json_node_free( r );
    json_node_free( r2 );
    // the value text of containers is JSON, of a null is empty
    r = P( "{\"a\":[1,{\"b\":2}],\"n\":null}" );
    CHECK( strcmp( TS( r, "a" ), "[1,{\"b\":2}]" ) == 0 );
    CHECK( strcmp( TS( r, "n" ), "" ) == 0 );
    json_node_free( r );
}

// random text and mutations of a valid document: never crash; a result that parses must print and parse again to the same text
static unsigned long seed = 12345UL;
static unsigned rnd( unsigned m ) {
    seed = seed * 1103515245UL + 12345UL;
    return (unsigned)( ( seed >> 16 ) & 0x7FFFU ) % m;
}

static void test_fuzz( void ) {
    const char* base = "{\"a\":[1,2,{\"b\":\"x\\u00e4\\n\"}],\"c\":{\"d\":null,\"e\":true,\"f\":-1.5e3},\"g\":\"\"}";
    size_t bl = strlen( base );
    int parsed = 0;
    for ( int it = 0; it < 20000; ++it ) {
        char buf[300];
        size_t len;
        if ( it % 4 == 0 ) {
            len = rnd( 60 );
            for ( size_t i=0; i < len; ++i ) {
                buf[i] = "{}[],:\"\\0123456789.-+eEtrufalsn xyz\n"[rnd( 37 )];
            }
        } else {
            memcpy( buf, base, bl );
            len = bl;
            int m = 1 + (int) rnd( 4 );
            for ( int k=0; k < m; ++k ) {
                size_t at = rnd( (unsigned) len );
                switch ( rnd( 3 ) ) {
                    case 0: buf[at] = "{}[],:\"\\0123456789.-+eE xyz"[rnd( 27 )]; break;
                    case 1: memmove( buf + at, buf + at + 1U, len - at - 1U ); --len; break;
                    default: if ( len + 1U < sizeof(buf) ) { memmove( buf + at + 1U, buf + at, len - at ); buf[at] = (char)( 32 + rnd( 90 ) ); ++len; }
                }
                if ( len == 0 ) {
                    len = 1;
                    buf[0] = '1';
                }
            }
        }
        json_node_t* n = 0;
        char err[100];
        if ( json_parse( buf, len, &n, err, sizeof(err) ) ) {
            ++parsed;
            size_t l1, l2;
            char* t1 = json_print( n, false, &l1 );
            json_node_t* n2 = 0;
            bool ok = t1 != 0 && json_parse( t1, l1, &n2, err, sizeof(err) );
            char* t2 = ok ? json_print( n2, false, &l2 ) : 0;
            if ( !( ok && t2 != 0 && l1 == l2 && memcmp( t1, t2, l1 ) == 0 ) ) {
                ++failures;
                printf( "FAIL fuzz round trip at %d\n", it );
            }
            ++cases;
            free( t1 ); free( t2 );
            json_node_free( n2 );
            json_node_free( n );
        }
    }
    CHECK( parsed > 100 );
}

int main( void ) {
    test_parse();
    test_make();
    test_write();
    test_fuzz();
    printf( "jsontest1: %d cases, %d failures\n", cases, failures );
    return failures;
}
