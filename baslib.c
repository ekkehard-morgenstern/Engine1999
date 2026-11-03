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
#include <unistd.h>
#include <strings.h>

// -- the list -----------------------------------------------------------------------------

void baslib_init( liblist_t* list ) {
    list->libs = 0;
    list->count = 0;
    list->capacity = 0;
}

void baslib_free( liblist_t* list ) {
    for ( size_t i=0; i < list->count; ++i ) {
        free( list->libs[i].name );
        free( list->libs[i].prefix );
        free( list->libs[i].file );
        free_program( &list->libs[i].pgm );
    }
    free( list->libs );
    baslib_init( list );
}

static bool baslib_append( liblist_t* list, const baslib_t* lib ) {
    if ( list->count == list->capacity ) {
        size_t cap = list->capacity ? list->capacity * 2U : 8U;
        baslib_t* p = (baslib_t*) realloc( list->libs, cap * sizeof(baslib_t) );
        if ( p == 0 ) {
            return false;
        }
        memset( p + list->capacity, 0, ( cap - list->capacity ) * sizeof(baslib_t) );
        list->libs = p;
        list->capacity = cap;
    }
    list->libs[ list->count++ ] = *lib;
    return true;
}

// -- scanning the text of a program -----------------------------------------------------------

typedef struct _libref_t {
    uint32_t    lineno;
    char*       name;
} libref_t;

typedef struct _scan_t {
    libref_t*   refs;
    size_t      nrefs, maxrefs;
    bool        islib;          // the first statement is LIBRARY
    char*       name;
    char*       prefix;
    uint32_t    headline;
    bool        seenstmt;       // a statement has been seen (comments don't count)
    bool        inproc;         // inside PROC ... END PROC / FUNC ... END FUNC
    bool        bad;
    char        msg[240];
    uint32_t    badline;
} scan_t;

static bool is_word_char( char c ) {
    return ( c >= 'A' && c <= 'Z' ) || ( c >= 'a' && c <= 'z' ) || ( c >= '0' && c <= '9' ) || c == '_' || c == '.' ||
        c == '$' || c == '%' || c == '!';
}

static void skip_blanks( const char** pp ) {
    while ( **pp == ' ' || **pp == '\t' ) {
        ++*pp;
    }
}

static bool read_word( const char** pp, char* word, size_t size ) {
    skip_blanks( pp );
    const char* p = *pp;
    size_t n = 0;
    while ( is_word_char( *p ) ) {
        if ( n + 1U < size ) {
            word[ n++ ] = *p;
        }
        ++p;
    }
    word[ n ] = '\0';
    *pp = p;
    return n > 0;
}

// a string literal in quotes at *pp (after blanks): copies its text (malloc'ed); false if there is none
static bool read_quoted( const char** pp, char** ptext ) {
    skip_blanks( pp );
    const char* p = *pp;
    if ( *p != '"' ) {
        return false;
    }
    ++p;
    const char* start = p;
    while ( *p && *p != '"' ) {
        ++p;
    }
    if ( *p != '"' ) {
        return false;
    }
    size_t n = (size_t)( p - start );
    char* text = (char*) malloc( n + 1U );
    if ( text == 0 ) {
        return false;
    }
    memcpy( text, start, n );
    text[n] = '\0';
    *pp = p + 1;
    *ptext = text;
    return true;
}

static void scan_fail( scan_t* sc, uint32_t lineno, const char* msg ) {
    if ( !sc->bad ) {
        sc->bad = true;
        sc->badline = lineno;
        snprintf( sc->msg, sizeof(sc->msg), "%s", msg );
    }
}

static void scan_free( scan_t* sc ) {
    for ( size_t i=0; i < sc->nrefs; ++i ) {
        free( sc->refs[i].name );
    }
    free( sc->refs );
    free( sc->name );
    free( sc->prefix );
    memset( sc, 0, sizeof(*sc) );
}

static bool scan_addref( scan_t* sc, uint32_t lineno, char* name ) {      // takes over name
    if ( sc->nrefs == sc->maxrefs ) {
        size_t cap = sc->maxrefs ? sc->maxrefs * 2U : 8U;
        libref_t* p = (libref_t*) realloc( sc->refs, cap * sizeof(libref_t) );
        if ( p == 0 ) {
            free( name );
            return false;
        }
        sc->refs = p;
        sc->maxrefs = cap;
    }
    sc->refs[ sc->nrefs ].lineno = lineno;
    sc->refs[ sc->nrefs ].name = name;
    ++sc->nrefs;
    return true;
}

// one line of listing text ("<number> <statements>"); cond: an IF is open on the line
static void scan_line( scan_t* sc, uint32_t lineno, const char* text ) {
    const char* p = text;
    while ( *p >= '0' && *p <= '9' ) {
        ++p;
    }
    bool stmtstart = true;
    bool cond = false;
    char word[64];
    while ( *p && !sc->bad ) {
        skip_blanks( &p );
        if ( *p == '\0' || *p == '\'' ) {
            break;                                  // a comment ends the line
        }
        if ( *p == '"' || *p == '`' ) {              // strings and shell literals
            char q = *p++;
            while ( *p && *p != q ) {
                ++p;
            }
            if ( *p ) {
                ++p;
            }
            stmtstart = false;
            continue;
        }
        if ( *p == ':' ) {
            ++p;
            stmtstart = true;
            continue;
        }
        if ( !is_word_char( *p ) ) {
            ++p;
            stmtstart = false;
            continue;
        }
        read_word( &p, word, sizeof(word) );
        if ( strcmp( word, "THEN" ) == 0 || strcmp( word, "ELSE" ) == 0 ) {
            stmtstart = true;
            continue;
        }
        if ( !stmtstart ) {
            continue;
        }
        stmtstart = false;
        if ( strcmp( word, "REM" ) == 0 ) {
            break;
        }
        bool first = !sc->seenstmt;
        sc->seenstmt = true;
        if ( strcmp( word, "IF" ) == 0 ) {
            cond = true;
            stmtstart = false;
        } else if ( strcmp( word, "LIBRARY" ) == 0 ) {
            char* name = 0; char* prefix = 0;
            if ( !first || cond ) {
                scan_fail( sc, lineno, "LIBRARY must be the first statement of the file" );
            } else if ( !read_quoted( &p, &name ) ) {
                scan_fail( sc, lineno, "Library name in quotes expected" );
            } else {
                skip_blanks( &p );
                if ( *p == ',' ) {
                    ++p;
                    if ( !read_quoted( &p, &prefix ) ) {
                        scan_fail( sc, lineno, "Library prefix in quotes expected" );
                    }
                }
                if ( !sc->bad ) {
                    sc->islib = true;
                    sc->name = name; name = 0;
                    sc->prefix = prefix ? prefix : strdup( "" ); prefix = 0;
                    sc->headline = lineno;
                }
            }
            free( name );
            free( prefix );
        } else if ( strcmp( word, "LOAD" ) == 0 ) {
            const char* save = p;
            if ( read_word( &p, word, sizeof(word) ) && strcmp( word, "LIBRARY" ) == 0 ) {
                char* name = 0;
                if ( cond ) {
                    scan_fail( sc, lineno, "LOAD LIBRARY must be a plain statement, not behind IF / THEN / ELSE" );
                } else if ( sc->inproc ) {
                    scan_fail( sc, lineno, "LOAD LIBRARY must be a plain statement, not inside a PROC or FUNC" );
                } else if ( !read_quoted( &p, &name ) ) {
                    scan_fail( sc, lineno, "Library name in quotes expected" );
                } else if ( name[0] == '\0' ) {
                    free( name );
                    scan_fail( sc, lineno, "Library name in quotes expected" );
                } else if ( !scan_addref( sc, lineno, name ) ) {
                    scan_fail( sc, lineno, "Out of memory" );
                }
            } else {
                p = save;
            }
        } else if ( strcmp( word, "PROC" ) == 0 || strcmp( word, "FUNC" ) == 0 ) {
            sc->inproc = true;
        } else if ( strcmp( word, "END" ) == 0 ) {
            const char* save = p;
            if ( read_word( &p, word, sizeof(word) ) && ( strcmp( word, "PROC" ) == 0 || strcmp( word, "FUNC" ) == 0 ) ) {
                sc->inproc = false;
            } else {
                p = save;
            }
        }
    }
}

// scans all lines of a program; false only if something could not even be read (out of memory)
static bool scan_program( program_t* pgm, scan_t* sc ) {
    memset( sc, 0, sizeof(*sc) );
    buf_t* buf = create_buffer();
    if ( buf == 0 ) {
        return false;
    }
    bool ok = true;
    for ( line_t* line = pgm->first; line != 0 && !sc->bad; line = line->next ) {
        pgmiter_t iter;
        clear_iter( &iter, pgm );
        iter_load_line( &iter, line );
        clear_buffer( buf );
        if ( !detokenize_line( buf, iter.tok, &iter.hdr ) ) {
            ok = false;
            break;
        }
        scan_line( sc, line->lineno, buf->buffer );
    }
    delete_buffer( buf );
    return ok;
}

bool baslib_header( program_t* pgm, char** pname, char** pprefix, uint32_t* plineno ) {
    scan_t sc;
    if ( !scan_program( pgm, &sc ) ) {
        scan_free( &sc );
        return false;
    }
    bool islib = sc.islib;      // (a later mistake in the file is reported by the resolver, which scans it again)
    if ( islib ) {
        *pname = sc.name; sc.name = 0;
        *pprefix = sc.prefix; sc.prefix = 0;
        if ( plineno ) {
            *plineno = sc.headline;
        }
    }
    scan_free( &sc );
    return islib;
}

bool baslib_valid_prefix( const char* prefix ) {
    if ( prefix[0] == '\0' ) {
        return true;
    }
    if ( !( ( prefix[0] >= 'A' && prefix[0] <= 'Z' ) || ( prefix[0] >= 'a' && prefix[0] <= 'z' ) ) ) {
        return false;
    }
    for ( const char* p = prefix; *p; ++p ) {
        if ( !( ( *p >= 'A' && *p <= 'Z' ) || ( *p >= 'a' && *p <= 'z' ) || ( *p >= '0' && *p <= '9' ) || *p == '_' ) ) {
            return false;
        }
    }
    return strlen( prefix ) <= 16U;
}

// -- resolving -------------------------------------------------------------------------------

typedef struct _resolve_t {
    baslib_loader_t load;
    void*           usrdata;
    liblist_t*      out;
    baslib_error_t* err;
    const char*     stack[BASLIB_MAXDEPTH + 1];     // names of the libraries being read (for cycles)
    size_t          depth;
    bool            failed;
} resolve_t;

static void resolve_fail( resolve_t* rs, const char* lib, uint32_t line, const char* fmt, ... ) {
    if ( rs->failed ) {
        return;
    }
    rs->failed = true;
    va_list ap;
    va_start( ap, fmt );
    vsnprintf( rs->err->msg, sizeof(rs->err->msg), fmt, ap );
    va_end( ap );
    rs->err->line = line;
    snprintf( rs->err->lib, sizeof(rs->err->lib), "%s", lib );
}

// the name a LOAD LIBRARY asks for, reduced to what the library has to call itself: no driver prefix, no directory, no extension
void baslib_request_key( const char* request, char* key, size_t size ) {
    const char* p = request;
    const char* colon = strchr( p, ':' );
    if ( colon != 0 && colon - p >= 2 ) {
        p = colon + 1;
    }
    const char* slash = strrchr( p, '/' );
    if ( slash != 0 ) {
        p = slash + 1;
    }
    snprintf( key, size, "%s", p );
    char* dot = strrchr( key, '.' );
    if ( dot != 0 && dot != key ) {
        static const char* const exts[] = { ".bas", ".e99", ".lib", ".l99" };
        for ( size_t i=0; i < sizeof(exts) / sizeof(exts[0]); ++i ) {
            if ( strcasecmp( dot, exts[i] ) == 0 ) {
                *dot = '\0';
                break;
            }
        }
    }
}

static baslib_t* find_known( const liblist_t* list, const char* name ) {
    for ( size_t i=0; i < list->count; ++i ) {
        if ( strcasecmp( list->libs[i].name, name ) == 0 ) {
            return &list->libs[i];
        }
    }
    return 0;
}

static void resolve_program( resolve_t* rs, program_t* pgm, const char* ownername );

// reads the library named in 'ref' (a line of 'ownername') and everything it needs
static void resolve_ref( resolve_t* rs, const libref_t* ref, const char* ownername ) {
    char key[128];
    baslib_request_key( ref->name, key, sizeof(key) );
    if ( find_known( rs->out, key ) != 0 ) {
        return;                                         // already read (a library is loaded once)
    }
    for ( size_t i=0; i < rs->depth; ++i ) {
        if ( strcasecmp( rs->stack[i], key ) == 0 ) {      // a cycle: name the chain
            char chain[160] = "";
            for ( size_t j=i; j < rs->depth; ++j ) {
                size_t n = strlen( chain );
                snprintf( chain + n, sizeof(chain) - n, "%s, ", rs->stack[j] );
            }
            size_t n = strlen( chain );
            snprintf( chain + n, sizeof(chain) - n, "%s", key );
            resolve_fail( rs, ownername, ref->lineno, "Library cycle (%s)", chain );
            return;
        }
    }
    if ( rs->depth >= BASLIB_MAXDEPTH ) {
        resolve_fail( rs, ownername, ref->lineno, "Libraries are nested too deeply (more than %d)", BASLIB_MAXDEPTH );
        return;
    }
    baslib_t lib;
    memset( &lib, 0, sizeof(lib) );
    init_program( &lib.pgm );
    char reason[160] = "";
    int rc = rs->load( rs->usrdata, ref->name, &lib.pgm, &lib.file, reason, sizeof(reason) );
    if ( rc == BASLIB_NOTFOUND ) {
        resolve_fail( rs, ownername, ref->lineno, "Library not found: %s", ref->name );
        goto FAIL;
    }
    if ( rc != BASLIB_FOUND ) {
        resolve_fail( rs, ownername, ref->lineno, "Library %s: %s", ref->name, reason[0] ? reason : "can't be read" );
        goto FAIL;
    }
    if ( !baslib_header( &lib.pgm, &lib.name, &lib.prefix, &lib.headline ) ) {
        scan_t sc;
        bool scanned = scan_program( &lib.pgm, &sc );
        if ( scanned && sc.bad ) {                      // a damaged LIBRARY statement or the like: say what it is
            resolve_fail( rs, key, sc.badline, "%s", sc.msg );
            scan_free( &sc );
            goto FAIL;
        }
        scan_free( &sc );
        resolve_fail( rs, ownername, ref->lineno, "Not a library: %s (its first statement must be LIBRARY \"name\")", ref->name );
        goto FAIL;
    }
    if ( strcasecmp( lib.name, key ) != 0 ) {
        resolve_fail( rs, ownername, ref->lineno, "The file %s holds the library %s, not %s", lib.file ? lib.file : ref->name, lib.name, key );
        goto FAIL;
    }
    if ( find_known( rs->out, lib.name ) != 0 ) {      // (the request was spelled differently, the library is the same)
        goto FAIL;
    }
    if ( !baslib_valid_prefix( lib.prefix ) ) {
        resolve_fail( rs, lib.name, lib.headline, "Bad library prefix \"%s\" (a letter, then letters, digits and _; at most 16)", lib.prefix );
        goto FAIL;
    }
    if ( lib.prefix[0] != '\0' ) {
        for ( size_t i=0; i < rs->out->count; ++i ) {
            if ( strcasecmp( rs->out->libs[i].prefix, lib.prefix ) == 0 ) {
                resolve_fail( rs, lib.name, lib.headline, "The libraries %s and %s use the same prefix \"%s\"", rs->out->libs[i].name, lib.name, lib.prefix );
                goto FAIL;
            }
        }
    }
    rs->stack[ rs->depth++ ] = lib.name;
    resolve_program( rs, &lib.pgm, lib.name );
    --rs->depth;
    if ( rs->failed ) {
        goto FAIL;
    }
    if ( lib.prefix[0] != '\0' ) {      // (checked again: a library that was read while this one was open may have taken the prefix)
        for ( size_t i=0; i < rs->out->count; ++i ) {
            if ( strcasecmp( rs->out->libs[i].prefix, lib.prefix ) == 0 ) {
                resolve_fail( rs, lib.name, lib.headline, "The libraries %s and %s use the same prefix \"%s\"", rs->out->libs[i].name, lib.name, lib.prefix );
                goto FAIL;
            }
        }
    }
    if ( !baslib_append( rs->out, &lib ) ) {
        resolve_fail( rs, ownername, ref->lineno, "Out of memory" );
        goto FAIL;
    }
    return;
FAIL:
    free( lib.name );
    free( lib.prefix );
    free( lib.file );
    free_program( &lib.pgm );
}

static void resolve_program( resolve_t* rs, program_t* pgm, const char* ownername ) {
    scan_t sc;
    if ( !scan_program( pgm, &sc ) ) {
        resolve_fail( rs, ownername, 0, "Out of memory" );
        scan_free( &sc );
        return;
    }
    if ( sc.bad ) {
        resolve_fail( rs, ownername, sc.badline, "%s", sc.msg );
    }
    for ( size_t i=0; i < sc.nrefs && !rs->failed; ++i ) {
        resolve_ref( rs, &sc.refs[i], ownername );
    }
    scan_free( &sc );
}

bool baslib_resolve( program_t* pgm, baslib_loader_t load, void* usrdata, liblist_t* out, baslib_error_t* err ) {
    baslib_free( out );
    memset( err, 0, sizeof(*err) );
    resolve_t rs;
    memset( &rs, 0, sizeof(rs) );
    rs.load = load;
    rs.usrdata = usrdata;
    rs.out = out;
    rs.err = err;
    char* name = 0; char* prefix = 0;
    if ( baslib_header( pgm, &name, &prefix, 0 ) ) {        // a library by itself can't be run
        resolve_fail( &rs, "", 0, "Library cannot be run (%s is a library: a program names it with LOAD LIBRARY)", name );
        free( name );
        free( prefix );
        baslib_free( out );
        return false;
    }
    resolve_program( &rs, pgm, "" );
    if ( rs.failed ) {
        baslib_free( out );
        return false;
    }
    return true;
}

// -- where installed libraries are ----------------------------------------------------------

size_t baslib_system_dirs( char* dirs[], size_t maxdirs ) {
    size_t n = 0;
    char buf[8192];
    const char* home = getenv( "ENGINE1999_HOME" );
    if ( home != 0 && home[0] != '\0' && n < maxdirs ) {
        snprintf( buf, sizeof(buf), "%s/lib", home );
        dirs[n] = strdup( buf );
        if ( dirs[n] != 0 ) ++n;
    }
    char exe[4096];
    ssize_t len = readlink( "/proc/self/exe", exe, sizeof(exe) - 1U );
    if ( len > 0 && n < maxdirs ) {
        exe[len] = '\0';
        char* slash = strrchr( exe, '/' );
        if ( slash != 0 ) {
            *slash = '\0';
            snprintf( buf, sizeof(buf), "%s/../lib/engine1999/lib", exe );
            dirs[n] = strdup( buf );
            if ( dirs[n] != 0 ) ++n;
        }
    }
    static const char* const fixed[] = { "/usr/local/lib/engine1999/lib", "/usr/lib/engine1999/lib" };
    for ( size_t i=0; i < sizeof(fixed) / sizeof(fixed[0]) && n < maxdirs; ++i ) {
        dirs[n] = strdup( fixed[i] );
        if ( dirs[n] != 0 ) ++n;
    }
    return n;
}
