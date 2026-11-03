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

#include "baslib.h"
#include "bastok.h"
#include <strings.h>

/*
Automated test of the library resolver (baslib.[ch]) without a REPL, a VM or a compiler: programs are built from text lines, the
libraries come from a table through a loader callback. The exit status is the number of failed checks.
*/

static int checks = 0, failures = 0;

static void check( const char* name, bool ok ) {
    ++checks;
    if ( !ok ) {
        ++failures;
        printf( "FAIL  %s\n", name );
    }
}

static void add( program_t* pgm, const char* text ) {
    char* copy = strdup( text );
    preprocess_buffer( copy );
    buf_t* tok = create_buffer();
    linehdr_t hdr;
    if ( !tokenize_line( copy, tok, &hdr ) || !enter_line( pgm, (const uint8_t*) tok->buffer, 0 ) ) {
        printf( "test setup failed on: %s\n", text );
        exit( 99 );
    }
    delete_buffer( tok );
    free( copy );
}

// a program from lines separated by '|'
static void build( program_t* pgm, const char* lines ) {
    init_program( pgm );
    char* copy = strdup( lines );
    for ( char* line = strtok( copy, "|" ); line != 0; line = strtok( 0, "|" ) ) {
        add( pgm, line );
    }
    free( copy );
}

typedef struct { const char* name; const char* lines; } libsrc_t;
static const libsrc_t* sources;
static int loads;           // how often the loader was called

// the loader of the tests: finds an entry by the name without a directory and extension
static int loader( void* ud, const char* name, program_t* pgm, char** pfile, char* reason, size_t reasonsize ) {
    (void) ud;
    ++loads;
    const char* base = strrchr( name, '/' );
    base = base ? base + 1 : name;
    char key[64];
    snprintf( key, sizeof(key), "%s", base );
    char* dot = strchr( key, '.' );
    if ( dot ) *dot = '\0';
    for ( const libsrc_t* s = sources; s->name; ++s ) {
        if ( strcasecmp( s->name, key ) == 0 ) {
            if ( strcmp( s->lines, "<damaged>" ) == 0 ) {
                snprintf( reason, reasonsize, "Bad program file" );
                return BASLIB_FAILED;
            }
            build( pgm, s->lines );
            *pfile = strdup( s->name );
            return BASLIB_FOUND;
        }
    }
    return BASLIB_NOTFOUND;
}

static bool resolve_text( const char* mainlines, const libsrc_t* srcs, liblist_t* out, baslib_error_t* err ) {
    program_t pgm;
    build( &pgm, mainlines );
    sources = srcs;
    loads = 0;
    bool ok = baslib_resolve( &pgm, loader, 0, out, err );
    free_program( &pgm );
    return ok;
}

static bool order_is( const liblist_t* l, const char* names ) {      // "c,a,b"
    char buf[256] = "";
    for ( size_t i=0; i < l->count; ++i ) {
        if ( i ) strcat( buf, "," );
        strcat( buf, l->libs[i].name );
    }
    return strcmp( buf, names ) == 0;
}

static bool msg_has( const baslib_error_t* e, const char* part ) {
    return strstr( e->msg, part ) != 0;
}

int main( void ) {
    static const libsrc_t none[] = { { 0, 0 } };
    liblist_t out; baslib_init( &out );
    baslib_error_t err;

    // -- the header --
    {
        program_t p; char* name = 0; char* prefix = 0; uint32_t line = 0;
        build( &p, "1 REM license|2 ' more|10 LIBRARY \"gfx\",\"GX_\"|20 PROC GX_A|30 END PROC" );
        check( "header: comments before LIBRARY", baslib_header( &p, &name, &prefix, &line ) && strcmp( name, "gfx" ) == 0 && strcmp( prefix, "GX_" ) == 0 && line == 10 );
        free( name ); free( prefix ); free_program( &p );
        build( &p, "10 LIBRARY \"x\"" );
        check( "header: no prefix is empty", baslib_header( &p, &name, &prefix, &line ) && strcmp( name, "x" ) == 0 && prefix[0] == '\0' );
        free( name ); free( prefix ); free_program( &p );
        build( &p, "10 PRINT 1|20 LIBRARY \"x\"" );
        check( "header: not the first statement", !baslib_header( &p, &name, &prefix, &line ) );
        free_program( &p );
        build( &p, "10 PRINT \"LIBRARY\"" );
        check( "header: a string is no statement", !baslib_header( &p, &name, &prefix, &line ) );
        free_program( &p );
        init_program( &p );
        check( "header: empty program", !baslib_header( &p, &name, &prefix, &line ) );
        free_program( &p );
        build( &p, "10 LIBRARY \"x\":PRINT 1" );
        check( "header: LIBRARY then another statement on the line", baslib_header( &p, &name, &prefix, &line ) );
        free( name ); free( prefix ); free_program( &p );
    }

    // -- prefixes --
    check( "prefix: empty", baslib_valid_prefix( "" ) );
    check( "prefix: SP_", baslib_valid_prefix( "SP_" ) );
    check( "prefix: letters and digits", baslib_valid_prefix( "A1b2" ) );
    check( "prefix: starts with a digit", !baslib_valid_prefix( "1A" ) );
    check( "prefix: underscore first", !baslib_valid_prefix( "_A" ) );
    check( "prefix: a dollar sign", !baslib_valid_prefix( "A$" ) );
    check( "prefix: too long", !baslib_valid_prefix( "ABCDEFGHIJKLMNOPQ" ) );

    // -- a program without libraries --
    check( "no libraries", resolve_text( "10 PRINT 1", none, &out, &err ) && out.count == 0 && loads == 0 );

    // -- one library --
    {
        static const libsrc_t s[] = { { "a", "10 LIBRARY \"a\",\"A_\"|20 PROC A_X|30 END PROC" }, { 0, 0 } };
        check( "one library", resolve_text( "10 LOAD LIBRARY \"a\"|20 PRINT 1", s, &out, &err ) && order_is( &out, "a" ) && strcmp( out.libs[0].prefix, "A_" ) == 0 && out.libs[0].pgm.first != 0 );
        check( "one library: after a colon", resolve_text( "10 PRINT 1: LOAD LIBRARY \"a\"", s, &out, &err ) && order_is( &out, "a" ) );
        check( "one library: named twice, read once", resolve_text( "10 LOAD LIBRARY \"a\"|20 LOAD LIBRARY \"a\"", s, &out, &err ) && order_is( &out, "a" ) && loads == 1 );
        check( "one library: case and path and extension", resolve_text( "10 LOAD LIBRARY \"lib/A.bas\"|20 LOAD LIBRARY \"a\"", s, &out, &err ) && order_is( &out, "a" ) );
        check( "one library: a name in a string, a comment and REM is no reference", resolve_text( "10 PRINT \"LOAD LIBRARY \"\"a\"\"\"|20 REM LOAD LIBRARY \"a\"|30 ' LOAD LIBRARY \"a\"|40 PRINT 1 ' LOAD LIBRARY \"a\"", s, &out, &err ) && out.count == 0 );
    }

    // -- dependency order --
    {
        static const libsrc_t s[] = {
            { "a", "10 LIBRARY \"a\",\"A_\"|20 LOAD LIBRARY \"c\"" },
            { "b", "10 LIBRARY \"b\",\"B_\"|20 LOAD LIBRARY \"c\"" },
            { "c", "10 LIBRARY \"c\",\"C_\"" },
            { 0, 0 } };
        check( "order: a needs c", resolve_text( "10 LOAD LIBRARY \"a\"", s, &out, &err ) && order_is( &out, "c,a" ) );
        check( "order: a and b need c (diamond), c once", resolve_text( "10 LOAD LIBRARY \"a\"|20 LOAD LIBRARY \"b\"", s, &out, &err ) && order_is( &out, "c,a,b" ) && loads == 3 );
        check( "order: c first, then a", resolve_text( "10 LOAD LIBRARY \"c\"|20 LOAD LIBRARY \"a\"", s, &out, &err ) && order_is( &out, "c,a" ) );
    }

    // -- cycles --
    {
        static const libsrc_t s[] = {
            { "a", "10 LIBRARY \"a\"|20 LOAD LIBRARY \"b\"" },
            { "b", "10 LIBRARY \"b\"|20 LOAD LIBRARY \"a\"" },
            { "self", "10 LIBRARY \"self\"|20 LOAD LIBRARY \"self\"" },
            { 0, 0 } };
        check( "cycle: a, b, a", !resolve_text( "10 LOAD LIBRARY \"a\"", s, &out, &err ) && msg_has( &err, "Library cycle (a, b, a)" ) && out.count == 0 );
        check( "cycle: a library that needs itself", !resolve_text( "10 LOAD LIBRARY \"self\"", s, &out, &err ) && msg_has( &err, "Library cycle (self, self)" ) );
        check( "cycle: the line and the library of the error", err.line == 20 && strcmp( err.lib, "self" ) == 0 );
    }

    // -- errors --
    {
        static const libsrc_t s[] = {
            { "plain", "10 PRINT 1" },
            { "wrong", "10 LIBRARY \"other\"" },
            { "p1", "10 LIBRARY \"p1\",\"1X\"" },
            { "same1", "10 LIBRARY \"same1\",\"S_\"" },
            { "same2", "10 LIBRARY \"same2\",\"s_\"" },
            { "twice", "10 LIBRARY \"twice\"|20 PRINT 1|30 LIBRARY \"twice\"" },
            { "badhead", "10 LIBRARY twice" },
            { "damaged", "<damaged>" },
            { "inner", "10 LIBRARY \"inner\"|20 PROC X|30 LOAD LIBRARY \"plain\"|40 END PROC" },
            { 0, 0 } };
        check( "error: not found", !resolve_text( "10 PRINT 1|20 LOAD LIBRARY \"nothere\"", s, &out, &err ) && msg_has( &err, "Library not found: nothere" ) && err.line == 20 && err.lib[0] == '\0' );
        check( "error: not a library", !resolve_text( "10 LOAD LIBRARY \"plain\"", s, &out, &err ) && msg_has( &err, "Not a library: plain" ) );
        check( "error: wrong declared name", !resolve_text( "10 LOAD LIBRARY \"wrong\"", s, &out, &err ) && msg_has( &err, "holds the library other, not wrong" ) );
        check( "error: bad prefix", !resolve_text( "10 LOAD LIBRARY \"p1\"", s, &out, &err ) && msg_has( &err, "Bad library prefix \"1X\"" ) && strcmp( err.lib, "p1" ) == 0 );
        check( "error: the same prefix twice", !resolve_text( "10 LOAD LIBRARY \"same1\"|20 LOAD LIBRARY \"same2\"", s, &out, &err ) && msg_has( &err, "use the same prefix" ) );
        check( "error: LIBRARY twice in a library", !resolve_text( "10 LOAD LIBRARY \"twice\"", s, &out, &err ) && msg_has( &err, "LIBRARY must be the first statement" ) && err.line == 30 && strcmp( err.lib, "twice" ) == 0 );
        check( "error: a damaged LIBRARY statement is said so", !resolve_text( "10 LOAD LIBRARY \"badhead\"", s, &out, &err ) && msg_has( &err, "Library name in quotes expected" ) );
        check( "error: a file that can't be read", !resolve_text( "10 LOAD LIBRARY \"damaged\"", s, &out, &err ) && msg_has( &err, "Library damaged: Bad program file" ) );
        check( "error: LOAD LIBRARY inside a PROC of a library", !resolve_text( "10 LOAD LIBRARY \"inner\"", s, &out, &err ) && msg_has( &err, "not inside a PROC or FUNC" ) && strcmp( err.lib, "inner" ) == 0 );
        check( "error: LOAD LIBRARY behind IF", !resolve_text( "10 IF A THEN LOAD LIBRARY \"plain\"", s, &out, &err ) && msg_has( &err, "not behind IF" ) );
        check( "error: LOAD LIBRARY behind ELSE", !resolve_text( "10 IF A THEN 20 ELSE LOAD LIBRARY \"plain\"", s, &out, &err ) && msg_has( &err, "not behind IF" ) );
        check( "error: LOAD LIBRARY in a PROC of the program", !resolve_text( "10 PROC P|20 LOAD LIBRARY \"plain\"|30 END PROC", s, &out, &err ) && msg_has( &err, "not inside a PROC or FUNC" ) && err.line == 20 );
        check( "ok: LOAD LIBRARY after the END PROC", resolve_text( "10 PROC P|20 END PROC|30 FUNC F%()|40 END FUNC|50 LOAD LIBRARY \"same1\"", s, &out, &err ) && order_is( &out, "same1" ) );
        check( "error: LIBRARY in a program that is not at its start", !resolve_text( "10 PRINT 1|20 LIBRARY \"x\"", s, &out, &err ) && msg_has( &err, "first statement" ) && err.line == 20 );
        check( "error: LIBRARY behind IF", !resolve_text( "10 IF A THEN LIBRARY \"x\"", s, &out, &err ) && msg_has( &err, "first statement" ) );
        check( "error: an empty library name", !resolve_text( "10 LOAD LIBRARY \"\"", s, &out, &err ) && msg_has( &err, "Library name in quotes expected" ) );
    }

    // -- depth --
    {
        static libsrc_t deep[48]; static char texts[48][160]; static char names[48][16];
        for ( int i=0; i < 47; ++i ) {
            snprintf( names[i], sizeof(names[i]), "d%d", i );
            snprintf( texts[i], sizeof(texts[i]), "10 LIBRARY \"d%d\"|20 LOAD LIBRARY \"d%d\"", i, i + 1 );
            deep[i].name = names[i]; deep[i].lines = texts[i];
        }
        snprintf( names[47], sizeof(names[47]), "d47" );
        snprintf( texts[47], sizeof(texts[47]), "10 LIBRARY \"d47\"" );
        deep[47].name = names[47]; deep[47].lines = texts[47];
        deep[47].name = names[47];
        static libsrc_t table[49];
        memcpy( table, deep, sizeof(deep) );
        check( "depth: too deep", !resolve_text( "10 LOAD LIBRARY \"d0\"", table, &out, &err ) && msg_has( &err, "nested too deeply" ) );
        static libsrc_t shallow[13]; 
        memcpy( shallow, deep, 12 * sizeof(libsrc_t) );
        snprintf( texts[11], sizeof(texts[11]), "10 LIBRARY \"d11\"" );
        check( "depth: 12 levels are fine", resolve_text( "10 LOAD LIBRARY \"d0\"", shallow, &out, &err ) && out.count == 12 && strcmp( out.libs[0].name, "d11" ) == 0 );
    }

    // -- the list is reusable --
    {
        static const libsrc_t s[] = { { "a", "10 LIBRARY \"a\"" }, { 0, 0 } };
        check( "reuse: resolving again replaces the list", resolve_text( "10 LOAD LIBRARY \"a\"", s, &out, &err ) && resolve_text( "10 LOAD LIBRARY \"a\"", s, &out, &err ) && out.count == 1 );
    }
    baslib_free( &out );
    check( "free: empty again", out.count == 0 && out.libs == 0 );

    // -- the system directories --
    {
        char* dirs[8];
        size_t n = baslib_system_dirs( dirs, 8 );
        check( "system dirs: some", n >= 2 );
        bool absolute = true;
        for ( size_t i=0; i < n; ++i ) {
            absolute = absolute && dirs[i][0] == '/';
            free( dirs[i] );
        }
        check( "system dirs: absolute", absolute );
    }

    printf( "%d of %d checks passed\n", checks - failures, checks );
    return failures;
}
