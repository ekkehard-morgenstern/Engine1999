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

#include "policy.h"

#include <limits.h>
#include <strings.h>
#include <unistd.h>
#include <sys/stat.h>

extern char* realpath( const char* path, char* resolved );    // (POSIX; not declared with only _POSIX_C_SOURCE)

// -- state ------------------------------------------------------------------------------------------------------------------------------------------

static bool         pol_enabled = false;
static bool         pol_running = false;
static policy_ask_t pol_ask = 0;
static void*        pol_askud = 0;
static char         pol_root[PATH_MAX];                 // the canonical folder of the running program
static char         pol_prog[PATH_MAX];                 // the canonical file of the program ("" = typed in)
static uint64_t     pol_key = 0;                        // path + content hash (0 = no key: nothing is stored)
static int8_t       pol_opt[POL_COUNT];                 // --allow (1) / --deny (-1) / unset (0): for all programs
static int8_t       pol_run[POL_COUNT];                 // this run: 1 allowed, -1 denied, 0 not decided
typedef struct _pol_sess_t { uint64_t key; int cat; } pol_sess_t;
#define POL_MAXSESS 64
static pol_sess_t   pol_sess[POL_MAXSESS];              // the session grants (this process)
static int          pol_nsess = 0;

static const char* const pol_names[POL_COUNT] = { "read", "write", "shell", "net" };

void policy_enable( bool on ) { pol_enabled = on; }
bool policy_enabled( void ) { return pol_enabled; }
void policy_set_asker( policy_ask_t ask, void* ud ) { pol_ask = ask; pol_askud = ud; }
bool policy_active( void ) { return pol_enabled && pol_running; }
const char* policy_root( void ) { return pol_root; }
const char* policy_category_name( int category ) { return category >= 0 && category < POL_COUNT ? pol_names[category] : "?"; }
void policy_forget_session( void ) { pol_nsess = 0; memset( pol_opt, 0, sizeof(pol_opt) ); }

bool policy_set_option( const char* list, bool allow, char* err, size_t errsize ) {
    char word[16];
    const char* p = list;
    while ( *p != '\0' ) {
        size_t n = 0;
        while ( p[n] != '\0' && p[n] != ',' && n < sizeof(word) - 1U ) {
            word[n] = (char)( p[n] >= 'A' && p[n] <= 'Z' ? p[n] - 'A' + 'a' : p[n] );
            ++n;
        }
        word[n] = '\0';
        if ( p[n] != '\0' && p[n] != ',' ) {
            snprintf( err, errsize, "bad permission word" );
            return false;
        }
        int cat = -1;
        if ( strcmp( word, "all" ) == 0 ) {
            cat = POL_COUNT;
        } else {
            for ( int i=0; i < POL_COUNT; ++i ) {
                if ( strcmp( word, pol_names[i] ) == 0 ) cat = i;
            }
        }
        if ( cat < 0 ) {
            snprintf( err, errsize, "unknown permission '%s' (read, write, shell, net, all)", word );
            return false;
        }
        for ( int i=0; i < POL_COUNT; ++i ) {
            if ( cat == POL_COUNT || cat == i ) pol_opt[i] = allow ? 1 : -1;
        }
        p += n;
        if ( *p == ',' ) ++p;
    }
    return true;
}

// -- paths ------------------------------------------------------------------------------------------------------------------------------------------

// lexical normal form of an absolute path (no "." and ".." parts, no double slashes)
static bool pol_lexical( const char* path, char* out, size_t outsize ) {
    char tmp[PATH_MAX];
    if ( path[0] == '/' ) {
        if ( strlen( path ) >= sizeof(tmp) ) return false;
        strcpy( tmp, path );
    } else {
        if ( getcwd( tmp, sizeof(tmp) ) == 0 ) return false;
        size_t n = strlen( tmp );
        if ( n + 1U + strlen( path ) + 1U > sizeof(tmp) ) return false;
        tmp[n] = '/';
        strcpy( tmp + n + 1U, path );
    }
    size_t o = 0;
    out[o++] = '/';
    out[o] = '\0';
    const char* s = tmp;
    while ( *s != '\0' ) {
        while ( *s == '/' ) ++s;
        const char* e = s;
        while ( *e != '\0' && *e != '/' ) ++e;
        size_t n = (size_t)( e - s );
        if ( n == 0 ) break;
        if ( n == 1 && s[0] == '.' ) {
            // nothing
        } else if ( n == 2 && s[0] == '.' && s[1] == '.' ) {
            while ( o > 1 && out[o-1] != '/' ) --o;     // (the last part is dropped, then its slash)
            if ( o > 1 ) --o;
            out[o] = '\0';
        } else {
            if ( o + n + 2U > outsize ) return false;
            if ( o > 1 ) out[o++] = '/';
            memcpy( out + o, s, n );
            o += n;
            out[o] = '\0';
        }
        s = e;
    }
    return true;
}

bool policy_normalize( const char* path, char* out, size_t outsize ) {
    char lex[PATH_MAX];
    if ( outsize < 2 || !pol_lexical( path, lex, sizeof(lex) ) ) return false;
    // symbolic links: resolve the longest existing part, keep the rest as it is
    char head[PATH_MAX], real[PATH_MAX], rest[PATH_MAX];
    strcpy( head, lex );
    rest[0] = '\0';
    for (;;) {
        if ( realpath( head, real ) != 0 ) {
            break;
        }
        char* s = strrchr( head, '/' );
        if ( s == 0 ) return false;
        size_t sl = strlen( s ), rl = strlen( rest );
        if ( sl + rl >= sizeof(rest) ) return false;
        memmove( rest + sl, rest, rl + 1U );
        memcpy( rest, s, sl );
        if ( s == head ) { head[1] = '\0'; } else { *s = '\0'; }
    }
    if ( strcmp( real, "/" ) == 0 ) {
        if ( strlen( rest ) + 1U > outsize ) return false;
        strcpy( out, rest[0] ? rest : "/" );
    } else {
        if ( strlen( real ) + strlen( rest ) + 1U > outsize ) return false;
        strcpy( out, real );
        strcat( out, rest );
    }
    return true;
}

bool policy_inside( const char* path, const char* root ) {
    char p[PATH_MAX], r[PATH_MAX];
    if ( !policy_normalize( path, p, sizeof(p) ) || !policy_normalize( root, r, sizeof(r) ) ) return false;
    size_t rn = strlen( r );
    if ( rn == 1 ) return true;                     // the root of the file system: everything
    return strncmp( p, r, rn ) == 0 && ( p[rn] == '\0' || p[rn] == '/' );
}

// -- the grants file --------------------------------------------------------------------------------------------------------------------------------

static uint64_t pol_hash( uint64_t h, const void* data, size_t len ) {
    const uint8_t* p = (const uint8_t*) data;
    for ( size_t i=0; i < len; ++i ) {
        h ^= p[i];
        h *= UINT64_C(1099511628211);
    }
    return h;
}

static bool pol_grants_path( char* out, size_t size ) {
    const char* e = getenv( "ENGINE1999_GRANTS" );
    if ( e != 0 && e[0] != '\0' ) {
        snprintf( out, size, "%s", e );
        return true;
    }
    const char* x = getenv( "XDG_CONFIG_HOME" );
    const char* h = getenv( "HOME" );
    if ( x != 0 && x[0] == '/' ) {
        snprintf( out, size, "%s/engine1999/grants", x );
    } else if ( h != 0 && h[0] == '/' ) {
        snprintf( out, size, "%s/.config/engine1999/grants", h );
    } else {
        return false;
    }
    return true;
}

// one line: KEY(16 hex) CATEGORY EXPIRY(seconds since 1970, 0 = always) PROGRAM
static bool pol_stored( uint64_t key, int cat ) {
    char path[PATH_MAX];
    if ( key == 0 || !pol_grants_path( path, sizeof(path) ) ) return false;
    FILE* f = fopen( path, "r" );
    if ( f == 0 ) return false;
    bool found = false;
    char line[PATH_MAX + 64];
    time_t now = time( 0 );
    while ( !found && fgets( line, sizeof(line), f ) != 0 ) {
        unsigned long long k = 0; char name[16]; long long exp = 0;
        if ( sscanf( line, "%llx %15s %lld", &k, name, &exp ) == 3 && (uint64_t) k == key && strcmp( name, pol_names[cat] ) == 0 &&
             ( exp == 0 || exp > (long long) now ) ) {
            found = true;
        }
    }
    fclose( f );
    return found;
}

static void pol_store( uint64_t key, int cat, long long expiry ) {
    char path[PATH_MAX];
    if ( key == 0 || !pol_grants_path( path, sizeof(path) ) ) return;
    // the old lines (without the expired ones and the same grant) and the new one
    char* keep = 0; size_t keeplen = 0;
    FILE* f = fopen( path, "r" );
    time_t now = time( 0 );
    if ( f != 0 ) {
        char line[PATH_MAX + 64];
        while ( fgets( line, sizeof(line), f ) != 0 ) {
            unsigned long long k = 0; char name[16]; long long exp = 0;
            if ( sscanf( line, "%llx %15s %lld", &k, name, &exp ) != 3 ) continue;
            if ( exp != 0 && exp <= (long long) now ) continue;
            if ( (uint64_t) k == key && strcmp( name, pol_names[cat] ) == 0 ) continue;
            size_t n = strlen( line );
            char* p = (char*) realloc( keep, keeplen + n + 1U );
            if ( p == 0 ) { free( keep ); fclose( f ); return; }
            keep = p;
            memcpy( keep + keeplen, line, n );
            keeplen += n;
            keep[keeplen] = '\0';
        }
        fclose( f );
    }
    char dir[PATH_MAX];
    snprintf( dir, sizeof(dir), "%s", path );
    for ( char* s = dir + 1; *s != '\0'; ++s ) {        // (the directories of the file are made one after the other)
        if ( *s == '/' ) {
            *s = '\0';
            (void) mkdir( dir, 0700 );
            *s = '/';
        }
    }
    f = fopen( path, "w" );
    if ( f != 0 ) {
        if ( keep != 0 ) fputs( keep, f );
        fprintf( f, "%016llx %s %lld %s\n", (unsigned long long) key, pol_names[cat], expiry, pol_prog[0] ? pol_prog : "(typed in)" );
        fclose( f );
        (void) chmod( path, 0600 );
    }
    free( keep );
}

// -- grants and questions ---------------------------------------------------------------------------------------------------------------------------

static bool pol_session_has( uint64_t key, int cat ) {
    for ( int i=0; i < pol_nsess; ++i ) {
        if ( pol_sess[i].key == key && pol_sess[i].cat == cat ) return true;
    }
    return false;
}

// -1 denied, 0 not decided, 1 allowed
static int pol_state( int cat ) {
    if ( pol_opt[cat] != 0 ) return pol_opt[cat];
    if ( pol_run[cat] != 0 ) return pol_run[cat];
    if ( pol_key != 0 && ( pol_session_has( pol_key, cat ) || pol_stored( pol_key, cat ) ) ) return 1;
    return 0;
}

static bool pol_decide( int cat, const char* what ) {
    int s = pol_state( cat );
    if ( s != 0 ) return s > 0;
    int ans = pol_ask != 0 ? pol_ask( pol_askud, cat, what ) : POL_ANS_DENY;
    if ( ans == POL_ANS_DENY ) {
        pol_run[cat] = -1;
        return false;
    }
    pol_run[cat] = 1;
    if ( ans == POL_ANS_SESSION && pol_key != 0 && pol_nsess < POL_MAXSESS && !pol_session_has( pol_key, cat ) ) {
        pol_sess[pol_nsess].key = pol_key;
        pol_sess[pol_nsess].cat = cat;
        ++pol_nsess;
    } else if ( ans == POL_ANS_DAY && pol_key != 0 ) {
        pol_store( pol_key, cat, (long long) time( 0 ) + 86400LL );
    } else if ( ans == POL_ANS_ALWAYS && pol_key != 0 ) {
        pol_store( pol_key, cat, 0 );
    }
    return true;
}

bool policy_check( const char* operation, const char* drv, const char* path ) {
    if ( !policy_active() ) return true;
    int cat = -1;
    char what[PATH_MAX + 64];
    if ( strcmp( operation, "shell" ) == 0 || ( drv != 0 && strcasecmp( drv, "sh" ) == 0 ) ) {
        cat = POL_SHELL;
        snprintf( what, sizeof(what), "run the shell command: %.200s", path ? path : "" );
    } else if ( strcmp( operation, "net" ) == 0 || ( drv != 0 && strcasecmp( drv, "tcp" ) == 0 ) ) {
        cat = POL_NET;
        snprintf( what, sizeof(what), "use the network: %.200s", path ? path : "" );
    } else if ( drv != 0 && ( strcasecmp( drv, "file" ) == 0 || strcasecmp( drv, "dir" ) == 0 ) ) {
        if ( path == 0 || pol_root[0] == '\0' || policy_inside( path, pol_root ) ) return true;
        bool reading = strcmp( operation, "read" ) == 0 || strcmp( operation, "list" ) == 0;
        cat = reading ? POL_READ : POL_WRITE;
        snprintf( what, sizeof(what), "%s outside of its folder: %.200s", strcmp( operation, "list" ) == 0 ? "list the files" : reading ? "read the file" : "write the file", path );
    } else {
        return true;            // mem:, null:, con:, stdin:, stdout:, stderr:, res:, play: and drivers of the program's own
    }
    return pol_decide( cat, what );
}

// -- the program starts and ends --------------------------------------------------------------------------------------------------------------------

static void pol_read_perm( const char* file, char* out, size_t outsize, bool* req ) {
    // NAME.perm for NAME.ext (or NAME if there is no extension)
    char perm[PATH_MAX];
    snprintf( perm, sizeof(perm), "%s", file );
    char* slash = strrchr( perm, '/' );
    char* dot = strrchr( perm, '.' );
    if ( dot != 0 && ( slash == 0 || dot > slash + 1 ) ) *dot = '\0';
    if ( strlen( perm ) + 6U > sizeof(perm) ) { out[0] = '\0'; return; }
    strcat( perm, ".perm" );
    out[0] = '\0';
    FILE* f = fopen( perm, "r" );
    if ( f == 0 ) return;
    size_t n = fread( out, 1, outsize - 1U, f );
    fclose( f );
    out[n] = '\0';
    char* line = out;
    while ( *line != '\0' ) {
        char* end = strchr( line, '\n' );
        if ( end != 0 ) *end = '\0';
        char* hash = strchr( line, '#' );
        if ( hash != 0 ) *hash = '\0';
        char verb[16], what[16];
        if ( sscanf( line, "%15s %15s", verb, what ) == 2 && strcasecmp( verb, "request" ) == 0 ) {
            for ( int c=0; c < POL_COUNT; ++c ) {
                if ( strcasecmp( what, pol_names[c] ) == 0 || strcasecmp( what, "all" ) == 0 ) req[c] = true;
            }
        }
        if ( end == 0 ) break;
        // (the text was cut: it is read again for the hash below, from the file)
        line = end + 1;
    }
}

void policy_begin_program( const char* file, const void* image, size_t len, const char* root ) {
    pol_running = true;
    memset( pol_run, 0, sizeof(pol_run) );
    pol_root[0] = '\0';
    pol_prog[0] = '\0';
    pol_key = 0;
    if ( !pol_enabled ) return;
    char tmp[PATH_MAX];
    bool havefile = file != 0 && file[0] != '\0';
    if ( havefile && policy_normalize( file, pol_prog, sizeof(pol_prog) ) ) {
        // the content for the hash: the image, else the file
        uint64_t h = pol_hash( UINT64_C(14695981039346656037), pol_prog, strlen( pol_prog ) + 1U );
        if ( image != 0 ) {
            h = pol_hash( h, image, len );
        } else {
            FILE* f = fopen( pol_prog, "rb" );
            if ( f != 0 ) {
                uint8_t buf[4096];
                size_t n;
                while ( ( n = fread( buf, 1, sizeof(buf), f ) ) > 0 ) h = pol_hash( h, buf, n );
                fclose( f );
            }
        }
        // the permission file counts, too (a program that asks for more has a new key)
        static char permtext[4096];
        bool req[POL_COUNT] = { false, false, false, false };
        pol_read_perm( pol_prog, permtext, sizeof(permtext), req );
        h = pol_hash( h, permtext, strlen( permtext ) );
        pol_key = h ? h : 1U;
        // the folder
        if ( root != 0 && root[0] != '\0' ) {
            snprintf( pol_root, sizeof(pol_root), "%s", root );
        } else {
            snprintf( tmp, sizeof(tmp), "%s", pol_prog );
            char* slash = strrchr( tmp, '/' );
            if ( slash != 0 ) { if ( slash == tmp ) tmp[1] = '\0'; else *slash = '\0'; }
            snprintf( pol_root, sizeof(pol_root), "%s", tmp );
        }
        // what the program asked for up front: one question for each power that is not decided yet
        for ( int c=0; c < POL_COUNT; ++c ) {
            if ( req[c] && pol_state( c ) == 0 ) {
                char what[96];
                snprintf( what, sizeof(what), "use the power '%s' (the program asks for it up front)", pol_names[c] );
                (void) pol_decide( c, what );
            }
        }
        return;
    }
    // typed in (or unknown file): the current directory, no key (nothing is stored; a session grant is not possible either)
    if ( root != 0 && root[0] != '\0' ) {
        snprintf( pol_root, sizeof(pol_root), "%s", root );
    } else if ( getcwd( pol_root, sizeof(pol_root) ) == 0 ) {
        pol_root[0] = '\0';
    }
}

void policy_end_program( void ) {
    pol_running = false;
}
