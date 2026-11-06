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

#include "basbuild.h"

#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <glob.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/utsname.h>

// -- small helpers ---------------------------------------------------------------------------

typedef struct _strlist_t {
    char**  v;
    size_t  n, max;
} strlist_t;

static void sl_free( strlist_t* l ) {
    for ( size_t i=0; i < l->n; ++i ) {
        free( l->v[i] );
    }
    free( l->v );
    l->v = 0; l->n = l->max = 0;
}

// adds a copy of s (v always has room for a terminating 0 pointer)
static bool sl_add( strlist_t* l, const char* s ) {
    if ( l->n + 2U > l->max ) {
        size_t newmax = l->max ? l->max * 2U : 32U;
        char** p = (char**) realloc( l->v, newmax * sizeof(char*) );
        if ( p == 0 ) {
            return false;
        }
        l->v = p;
        l->max = newmax;
    }
    char* copy = strdup( s );
    if ( copy == 0 ) {
        return false;
    }
    l->v[ l->n++ ] = copy;
    l->v[ l->n ] = 0;
    return true;
}

static bool sl_addf( strlist_t* l, const char* fmt, ... ) {
    char buf[1100];
    va_list ap;
    va_start( ap, fmt );
    int n = vsnprintf( buf, sizeof(buf), fmt, ap );
    va_end( ap );
    return n >= 0 && (size_t) n < sizeof(buf) && sl_add( l, buf );
}

static bool sl_has( const strlist_t* l, const char* s ) {
    for ( size_t i=0; i < l->n; ++i ) {
        if ( strcmp( l->v[i], s ) == 0 ) {
            return true;
        }
    }
    return false;
}

static bool is_file( const char* path ) {
    struct stat st;
    return stat( path, &st ) == 0 && S_ISREG( st.st_mode );
}

static bool is_dir( const char* path ) {
    struct stat st;
    return stat( path, &st ) == 0 && S_ISDIR( st.st_mode );
}

static bool fail( char* err, size_t errsize, const char* fmt, ... ) {
    if ( err != 0 && errsize > 0 ) {
        va_list ap;
        va_start( ap, fmt );
        vsnprintf( err, errsize, fmt, ap );
        va_end( ap );
    }
    return false;
}

// compares version strings like 12.2.0 and 9 numerically (a longer number is a bigger one)
static int vercmp( const char* a, const char* b ) {
    while ( *a != '\0' && *b != '\0' ) {
        if ( isdigit( (unsigned char) *a ) && isdigit( (unsigned char) *b ) ) {
            char* pa; char* pb;
            unsigned long x = strtoul( a, &pa, 10 );
            unsigned long y = strtoul( b, &pb, 10 );
            if ( x != y ) {
                return x < y ? -1 : 1;
            }
            a = pa; b = pb;
        } else {
            if ( *a != *b ) {
                return (unsigned char) *a < (unsigned char) *b ? -1 : 1;
            }
            ++a; ++b;
        }
    }
    return ( *a != '\0' ) - ( *b != '\0' );
}

// the first directory of the list that has the file; the path in a malloc'ed string, 0 if there is none
static char* find_in_dirs( const strlist_t* dirs, const char* name ) {
    for ( size_t i=0; i < dirs->n; ++i ) {
        char path[1100];
        int n = snprintf( path, sizeof(path), "%s/%s", dirs->v[i], name );
        if ( n > 0 && (size_t) n < sizeof(path) && is_file( path ) ) {
            return strdup( path );
        }
    }
    return 0;
}

// -- the machine -------------------------------------------------------------------------------

static const struct {
    const char* machine;        // uname -m
    const char* multiarch;      // the name of the library directories of Debian-like systems
    const char* interps[5];     // where the dynamic linker usually is
} archs[] = {
    { "x86_64",  "x86_64-linux-gnu",       { "/lib64/ld-linux-x86-64.so.2", "/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2", "/usr/lib64/ld-linux-x86-64.so.2",
                                             "/lib/ld-musl-x86_64.so.1", 0 } },
    { "aarch64", "aarch64-linux-gnu",      { "/lib/ld-linux-aarch64.so.1", "/lib/aarch64-linux-gnu/ld-linux-aarch64.so.1", "/lib64/ld-linux-aarch64.so.1",
                                             "/lib/ld-musl-aarch64.so.1", 0 } },
    { "i686",    "i386-linux-gnu",         { "/lib/ld-linux.so.2", "/lib/i386-linux-gnu/ld-linux.so.2", "/lib32/ld-linux.so.2", "/lib/ld-musl-i386.so.1", 0 } },
    { "armv7l",  "arm-linux-gnueabihf",    { "/lib/ld-linux-armhf.so.3", "/lib/arm-linux-gnueabihf/ld-linux-armhf.so.3", "/lib/ld-musl-armhf.so.1", 0, 0 } },
    { "riscv64", "riscv64-linux-gnu",      { "/lib/ld-linux-riscv64-lp64d.so.1", "/lib/riscv64-linux-gnu/ld-linux-riscv64-lp64d.so.1",
                                             "/lib/ld-musl-riscv64.so.1", 0, 0 } },
    { "ppc64le", "powerpc64le-linux-gnu",  { "/lib64/ld64.so.2", "/lib/powerpc64le-linux-gnu/ld64.so.2", 0, 0, 0 } },
};

typedef struct _machine_t {
    char        name[65];
    const char* multiarch;
    const char* const* interps;
} machine_t;

static void machine_get( machine_t* m ) {
    struct utsname un;
    memset( m, 0, sizeof(*m) );
    if ( uname( &un ) == 0 ) {
        snprintf( m->name, sizeof(m->name), "%s", un.machine );
    }
    const char* key = m->name;
    if ( key[0] == 'i' && key[2] == '8' && key[3] == '6' ) {        // i386 .. i686
        key = "i686";
    }
    for ( size_t i=0; i < sizeof(archs) / sizeof(archs[0]); ++i ) {
        if ( strcmp( archs[i].machine, key ) == 0 ) {
            m->multiarch = archs[i].multiarch;
            m->interps = archs[i].interps;
            return;
        }
    }
}

// -- the places where things are -------------------------------------------------------------

// the directories with libraries and startup files that exist
static bool find_libdirs( const machine_t* m, strlist_t* dirs ) {
    const char* lp = getenv( "LIBRARY_PATH" );
    while ( lp != 0 && *lp != '\0' ) {
        const char* colon = strchr( lp, ':' );
        size_t n = colon ? (size_t)( colon - lp ) : strlen( lp );
        if ( n > 0 && n < 1000U ) {
            char dir[1024];
            memcpy( dir, lp, n );
            dir[n] = '\0';
            if ( is_dir( dir ) && !sl_has( dirs, dir ) && !sl_add( dirs, dir ) ) {
                return false;
            }
        }
        lp = colon ? colon + 1 : 0;
    }
    strlist_t cand = { 0, 0, 0 };
    bool ok = true;
    if ( m->multiarch != 0 ) {
        ok = sl_addf( &cand, "/usr/lib/%s", m->multiarch ) && sl_addf( &cand, "/lib/%s", m->multiarch ) &&
             sl_addf( &cand, "/usr/local/lib/%s", m->multiarch );
    }
    static const char* const plain[] = { "/usr/lib64", "/lib64", "/usr/lib", "/lib", "/usr/local/lib64", "/usr/local/lib", 0 };
    for ( int i=0; ok && plain[i] != 0; ++i ) {
        ok = sl_add( &cand, plain[i] );
    }
    for ( size_t i=0; ok && i < cand.n; ++i ) {
        if ( is_dir( cand.v[i] ) && !sl_has( dirs, cand.v[i] ) ) {
            ok = sl_add( dirs, cand.v[i] );
        }
    }
    sl_free( &cand );
    return ok;
}

// the compiler's own directory (crtbegin.o, crtend.o, libgcc.a); the newest version wins. 0 = none (they are optional).
static char* find_gccdir( const machine_t* m ) {
    static const char* const patterns[] = { "/usr/lib/gcc/%s-*/*", "/usr/lib64/gcc/%s-*/*", "/usr/local/lib/gcc/%s-*/*", 0 };
    char* best = 0;
    for ( int i=0; patterns[i] != 0 && m->name[0] != '\0'; ++i ) {
        char pattern[256];
        snprintf( pattern, sizeof(pattern), patterns[i], m->name );
        glob_t g;
        if ( glob( pattern, GLOB_NOSORT, 0, &g ) != 0 ) {
            continue;
        }
        for ( size_t j=0; j < g.gl_pathc; ++j ) {
            char path[1100];
            snprintf( path, sizeof(path), "%s/crtbegin.o", g.gl_pathv[j] );
            if ( !is_file( path ) ) {
                continue;
            }
            snprintf( path, sizeof(path), "%s/libgcc.a", g.gl_pathv[j] );
            if ( !is_file( path ) ) {
                continue;
            }
            const char* ver = strrchr( g.gl_pathv[j], '/' );
            const char* bestver = best ? strrchr( best, '/' ) : 0;
            if ( best == 0 || vercmp( ver + 1, bestver + 1 ) > 0 ) {
                char* copy = strdup( g.gl_pathv[j] );
                if ( copy != 0 ) {
                    free( best );
                    best = copy;
                }
            }
        }
        globfree( &g );
    }
    return best;
}

static char* find_interp( const machine_t* m ) {
    for ( int i=0; m->interps != 0 && m->interps[i] != 0; ++i ) {
        if ( is_file( m->interps[i] ) ) {
            return strdup( m->interps[i] );
        }
    }
    static const char* const patterns[] = { "/lib*/ld-linux*.so*", "/lib/*/ld-linux*.so*", "/usr/lib*/ld-linux*.so*", 0 };
    for ( int i=0; patterns[i] != 0; ++i ) {
        glob_t g;
        if ( glob( patterns[i], 0, 0, &g ) == 0 ) {
            char* r = g.gl_pathc > 0 ? strdup( g.gl_pathv[0] ) : 0;
            globfree( &g );
            if ( r != 0 ) {
                return r;
            }
        }
    }
    return 0;
}

// the linker: $ENGINE1999_LD, then ld, ld.bfd, ld.gold in the PATH and the usual places
static char* find_linker( void ) {
    const char* env = getenv( "ENGINE1999_LD" );
    if ( env != 0 && env[0] != '\0' ) {
        return is_file( env ) ? strdup( env ) : 0;
    }
    strlist_t dirs = { 0, 0, 0 };
    const char* path = getenv( "PATH" );
    bool ok = true;
    while ( ok && path != 0 && *path != '\0' ) {
        const char* colon = strchr( path, ':' );
        size_t n = colon ? (size_t)( colon - path ) : strlen( path );
        if ( n > 0 && n < 1000U ) {
            char dir[1024];
            memcpy( dir, path, n );
            dir[n] = '\0';
            ok = sl_add( &dirs, dir );
        }
        path = colon ? colon + 1 : 0;
    }
    static const char* const fixed[] = { "/usr/bin", "/bin", "/usr/local/bin", "/usr/sbin", 0 };
    for ( int i=0; ok && fixed[i] != 0; ++i ) {
        ok = sl_add( &dirs, fixed[i] );
    }
    char* found = 0;
    static const char* const names[] = { "ld", "ld.bfd", "ld.gold", 0 };
    for ( int i=0; ok && found == 0 && names[i] != 0; ++i ) {
        found = find_in_dirs( &dirs, names[i] );
    }
    sl_free( &dirs );
    return found;
}

// a shared library: lib<name>.so (the development link), else lib<name>.so.* , else lib<name>-*.so* (SDL2: libSDL2-2.0.so.0)
static char* find_library( const strlist_t* dirs, const char* name ) {
    char file[300];
    snprintf( file, sizeof(file), "lib%s.so", name );
    char* r = find_in_dirs( dirs, file );
    if ( r != 0 ) {
        return r;
    }
    static const char* const forms[] = { "lib%s.so.*", "lib%s-*.so*", 0 };
    for ( int f=0; forms[f] != 0; ++f ) {
        for ( size_t i=0; i < dirs->n; ++i ) {
            char pattern[1200];
            char fname[300];
            snprintf( fname, sizeof(fname), forms[f], name );
            snprintf( pattern, sizeof(pattern), "%s/%s", dirs->v[i], fname );
            glob_t g;
            if ( glob( pattern, 0, 0, &g ) == 0 ) {
                r = g.gl_pathc > 0 ? strdup( g.gl_pathv[0] ) : 0;
                globfree( &g );
                if ( r != 0 ) {
                    return r;
                }
            }
        }
    }
    return 0;
}

// where the files of the runtime kit are: the directory of the running executable, the current directory, ...
static bool find_kitdirs( const build_opts_t* opts, strlist_t* dirs ) {
    bool ok = true;
    if ( opts->kitdir != 0 && opts->kitdir[0] != '\0' ) {
        ok = sl_add( dirs, opts->kitdir );
    }
    const char* home = getenv( "ENGINE1999_HOME" );
    if ( ok && home != 0 && home[0] != '\0' ) {
        ok = sl_add( dirs, home );
    }
    char exe[1024];
    ssize_t n = readlink( "/proc/self/exe", exe, sizeof(exe) - 1U );
    if ( ok && n > 0 ) {
        exe[n] = '\0';
        char* slash = strrchr( exe, '/' );
        if ( slash != 0 ) {
            *slash = '\0';
            ok = sl_add( dirs, slash == exe ? "/" : exe );
            // an installed Engine: bin/engine1999 with the kit in ../lib/engine1999 (whatever the prefix is)
            ok = ok && sl_addf( dirs, "%s/../lib/engine1999", slash == exe ? "" : exe );
        }
    }
    static const char* const fixed[] = { ".", "/usr/local/lib/engine1999", "/usr/lib/engine1999", "/usr/local/share/engine1999",
        "/usr/share/engine1999", 0 };
    for ( int i=0; ok && fixed[i] != 0; ++i ) {
        ok = sl_add( dirs, fixed[i] );
    }
    return ok;
}

// a kit file: a name with a '/' is a path, any other name is looked for in the kit directories
static char* find_kitfile( const strlist_t* kitdirs, const char* name ) {
    if ( strchr( name, '/' ) != 0 ) {
        return is_file( name ) ? strdup( name ) : 0;
    }
    return find_in_dirs( kitdirs, name );
}

// -- running the linker ---------------------------------------------------------------------

static bool trace_on( void ) {
    const char* t = getenv( "ENGINE1999_BUILD_TRACE" );
    return t != 0 && t[0] != '\0' && t[0] != '0';
}

// runs a program (argv[0] is its path, no shell) with its output going to logfile; returns its exit status, -1 if it could not be run
static int run_tool( const char* cwd, const char* logfile, char* const argv[] ) {
    if ( trace_on() ) {
        fprintf( stderr, "build:" );
        for ( int i=0; argv[i] != 0; ++i ) {
            fprintf( stderr, " %s", argv[i] );
        }
        fprintf( stderr, "\n" );
    }
    pid_t pid = fork();
    if ( pid < 0 ) {
        return -1;
    }
    if ( pid == 0 ) {
        // only async-signal-safe calls from here on (the engine has threads)
        if ( cwd != 0 && chdir( cwd ) != 0 ) {
            _exit( 126 );
        }
        int fd = open( logfile, O_WRONLY | O_CREAT | O_APPEND, 0600 );
        int nul = open( "/dev/null", O_RDONLY );
        if ( fd < 0 || nul < 0 ) {
            _exit( 126 );
        }
        dup2( nul, 0 );
        dup2( fd, 1 );
        dup2( fd, 2 );
        for ( int i=3; i < 256; ++i ) {
            close( i );
        }
        execv( argv[0], argv );
        _exit( 127 );
    }
    int status = 0;
    while ( waitpid( pid, &status, 0 ) < 0 ) {
        if ( errno != EINTR ) {
            return -1;
        }
    }
    return WIFEXITED( status ) ? WEXITSTATUS( status ) : -1;
}

// the first words of the linker's messages, in one line
static void read_log( const char* logfile, char* text, size_t size ) {
    text[0] = '\0';
    FILE* f = fopen( logfile, "r" );
    if ( f == 0 ) {
        return;
    }
    size_t n = fread( text, 1U, size - 1U, f );
    fclose( f );
    text[n] = '\0';
    char* w = text;
    bool space = true;
    for ( const char* r = text; *r != '\0'; ++r ) {
        if ( *r == '\n' || *r == '\r' || *r == '\t' || *r == ' ' ) {
            if ( !space ) {
                *w++ = ' ';
                space = true;
            }
        } else {
            *w++ = *r;
            space = false;
        }
    }
    if ( w > text && w[-1] == ' ' ) {
        --w;
    }
    *w = '\0';
}

static void remove_tmp( const char* tmpdir ) {
    static const char* const files[] = { BUILD_PROGRAM_FILE, "program.o", "link.log", 0 };
    for ( int i=0; files[i] != 0; ++i ) {
        char path[1100];
        snprintf( path, sizeof(path), "%s/%s", tmpdir, files[i] );
        unlink( path );
    }
    rmdir( tmpdir );
}

// -- BUILD ---------------------------------------------------------------------------------

void build_default_opts( build_opts_t* opts ) {
    static const char* const libs[] = { "SDL2", "m", 0 };
    static const char* const optlibs[] = { "rt", "pthread", "dl", 0 };
    memset( opts, 0, sizeof(*opts) );
    opts->runtime_obj = "engine1999.o";
    opts->runtime_lib = "engine.lib";
    opts->libs = libs;
    opts->optlibs = optlibs;
}

void build_console_opts( build_opts_t* opts ) {
    static const char* const libs[] = { "m", 0 };
    build_default_opts( opts );
    opts->runtime_obj = "engine1999c.o";
    opts->libs = libs;
}

bool build_executable( const char* outname, const void* prog, size_t len, const build_opts_t* userop, char* err, size_t errsize ) {
    build_opts_t opts;
    if ( userop != 0 ) {
        opts = *userop;
    } else {
        build_default_opts( &opts );
    }
    if ( outname == 0 || outname[0] == '\0' ) {
        return fail( err, errsize, "Illegal argument" );
    }
    if ( prog == 0 || len == 0 ) {
        return fail( err, errsize, "No program" );
    }

    machine_t m;
    machine_get( &m );
    strlist_t libdirs = { 0, 0, 0 }, kitdirs = { 0, 0, 0 }, args = { 0, 0, 0 };
    char *ld = 0, *interp = 0, *gccdir = 0, *robj = 0, *rlib = 0, *scrt1 = 0, *crt1 = 0, *crti = 0, *crtn = 0, *libc = 0;
    char tmpdir[1024]; tmpdir[0] = '\0';
    bool ok = false;
    strlist_t libpaths = { 0, 0, 0 };       // the libraries found, in link order
    char log[400];

    if ( !find_libdirs( &m, &libdirs ) || !find_kitdirs( &opts, &kitdirs ) ) {
        fail( err, errsize, "Out of memory" );
        goto DONE;
    }

    // the tools and files
    ld = find_linker();
    if ( ld == 0 ) {
        fail( err, errsize, "Linker (ld) not found; install binutils" );
        goto DONE;
    }
    robj = find_kitfile( &kitdirs, opts.runtime_obj ? opts.runtime_obj : "engine1999.o" );
    if ( robj == 0 ) {
        fail( err, errsize, "%s not found; it belongs next to the Engine (or set ENGINE1999_HOME)", opts.runtime_obj ? opts.runtime_obj : "engine1999.o" );
        goto DONE;
    }
    if ( opts.runtime_lib != 0 ) {
        rlib = find_kitfile( &kitdirs, opts.runtime_lib );
        if ( rlib == 0 ) {
            fail( err, errsize, "%s not found; it belongs next to the Engine (or set ENGINE1999_HOME)", opts.runtime_lib );
            goto DONE;
        }
    }
    interp = find_interp( &m );
    if ( interp == 0 ) {
        fail( err, errsize, "Dynamic linker (ld-linux) not found" );
        goto DONE;
    }
    crt1 = find_in_dirs( &libdirs, "crt1.o" );
    crti = find_in_dirs( &libdirs, "crti.o" );
    crtn = find_in_dirs( &libdirs, "crtn.o" );
    if ( crt1 == 0 || crti == 0 || crtn == 0 ) {
        fail( err, errsize, "%s not found; install the development files of the C library", crt1 == 0 ? "crt1.o" : crti == 0 ? "crti.o" : "crtn.o" );
        goto DONE;
    }
    gccdir = find_gccdir( &m );
    for ( int i=0; opts.libs != 0 && opts.libs[i] != 0; ++i ) {
        char* p = find_library( &libdirs, opts.libs[i] );
        if ( p == 0 ) {
            fail( err, errsize, "Library %s not found (install its development package)", opts.libs[i] );
            goto DONE;
        }
        if ( !sl_add( &libpaths, p ) ) {
            free( p );
            fail( err, errsize, "Out of memory" );
            goto DONE;
        }
        free( p );
    }
    for ( int i=0; opts.optlibs != 0 && opts.optlibs[i] != 0; ++i ) {
        char* p = find_library( &libdirs, opts.optlibs[i] );
        if ( p != 0 && !sl_add( &libpaths, p ) ) {
            free( p );
            fail( err, errsize, "Out of memory" );
            goto DONE;
        }
        free( p );
    }
    libc = find_library( &libdirs, "c" );
    if ( libc == 0 ) {
        fail( err, errsize, "The C library (libc.so) not found; install the development files of the C library" );
        goto DONE;
    }

    // a work directory with the program as a file named program.e99
    {
        const char* tmp = getenv( "TMPDIR" );
        snprintf( tmpdir, sizeof(tmpdir), "%s/e1999build.XXXXXX", tmp != 0 && tmp[0] != '\0' ? tmp : "/tmp" );
        if ( mkdtemp( tmpdir ) == 0 ) {
            tmpdir[0] = '\0';
            fail( err, errsize, "Can't create a work directory" );
            goto DONE;
        }
    }
    char progfile[1100], progobj[1100], logfile[1100];
    snprintf( progfile, sizeof(progfile), "%s/%s", tmpdir, BUILD_PROGRAM_FILE );
    snprintf( progobj, sizeof(progobj), "%s/program.o", tmpdir );
    snprintf( logfile, sizeof(logfile), "%s/link.log", tmpdir );
    {
        FILE* f = fopen( progfile, "wb" );
        bool wrote = f != 0 && fwrite( prog, 1U, len, f ) == len;
        if ( f != 0 && fclose( f ) != 0 ) {
            wrote = false;
        }
        if ( !wrote ) {
            fail( err, errsize, "Can't write the work files" );
            goto DONE;
        }
    }

    // step 1: the program as an object file (symbols _binary_program_e99_start / _end); in the work directory, so the symbol names don't
    // depend on where it is
    {
        char* argv1[] = { ld, "-r", "-b", "binary", "-o", "program.o", BUILD_PROGRAM_FILE, 0 };
        int st = run_tool( tmpdir, logfile, argv1 );
        if ( st != 0 || !is_file( progobj ) ) {
            read_log( logfile, log, sizeof(log) );
            fail( err, errsize, "Linker failed (%s)", log[0] != '\0' ? log : st < 0 ? "can't run it" : "program object" );
            goto DONE;
        }
    }

    // step 2: the executable. First as a position independent executable (PIE, the start files Scrt1.o / crtbeginS.o / crtendS.o; needs the
    // Engine's objects compiled position independent, which a compiler that makes PIE by default does), and if that is not possible, as a
    // plain executable. ENGINE1999_BUILD_PIE=0 asks for the plain one at once. Both are linked with "-z now" (no lazy binding) and "-z relro".
    {
        const char* pe = getenv( "ENGINE1999_BUILD_PIE" );
        bool wantpie = !( pe != 0 && pe[0] == '0' );
        char gccS[1100], gccE[1100];
        gccS[0] = gccE[0] = '\0';
        scrt1 = wantpie ? find_in_dirs( &libdirs, "Scrt1.o" ) : 0;
        if ( gccdir != 0 ) {
            snprintf( gccS, sizeof(gccS), "%s/crtbeginS.o", gccdir );
            snprintf( gccE, sizeof(gccE), "%s/crtendS.o", gccdir );
        }
        bool piepossible = scrt1 != 0 && ( gccdir == 0 || ( is_file( gccS ) && is_file( gccE ) ) );
        for ( int pass = piepossible ? 0 : 1; pass < 2; ++pass ) {
            bool pie = pass == 0;
            sl_free( &args );
            memset( &args, 0, sizeof(args) );
            ok = sl_add( &args, ld ) && sl_add( &args, "-o" ) && sl_add( &args, outname ) && sl_add( &args, "--eh-frame-hdr" ) &&
                 sl_add( &args, "-z" ) && sl_add( &args, "noexecstack" ) && sl_add( &args, "-z" ) && sl_add( &args, "relro" ) &&
                 sl_add( &args, "-z" ) && sl_add( &args, "now" ) &&
                 sl_add( &args, "--hash-style=gnu" ) && sl_add( &args, "-dynamic-linker" ) && sl_add( &args, interp );
            if ( ok && pie ) {
                ok = sl_add( &args, "-pie" );
            }
            for ( size_t i=0; ok && i < libdirs.n; ++i ) {      // the libraries the libraries need
                ok = sl_add( &args, "-rpath-link" ) && sl_add( &args, libdirs.v[i] );
            }
            ok = ok && sl_add( &args, pie ? scrt1 : crt1 ) && sl_add( &args, crti );
            char gccfile[1100];
            if ( gccdir != 0 ) {
                snprintf( gccfile, sizeof(gccfile), "%s/%s", gccdir, pie ? "crtbeginS.o" : "crtbegin.o" );
                ok = ok && sl_add( &args, gccfile );
            }
            ok = ok && sl_add( &args, robj ) && sl_add( &args, progobj );
            if ( rlib != 0 ) {
                ok = ok && sl_add( &args, rlib );
            }
            ok = ok && sl_add( &args, "--as-needed" );
            for ( size_t i=0; ok && i < libpaths.n; ++i ) {
                ok = sl_add( &args, libpaths.v[i] );
            }
            if ( gccdir != 0 ) {
                snprintf( gccfile, sizeof(gccfile), "%s/libgcc.a", gccdir );
                ok = ok && sl_add( &args, gccfile );
            }
            ok = ok && sl_add( &args, libc );
            if ( gccdir != 0 ) {
                snprintf( gccfile, sizeof(gccfile), "%s/libgcc.a", gccdir );
                ok = ok && sl_add( &args, gccfile );
                snprintf( gccfile, sizeof(gccfile), "%s/%s", gccdir, pie ? "crtendS.o" : "crtend.o" );
                ok = ok && sl_add( &args, gccfile );
            }
            ok = ok && sl_add( &args, crtn );
            if ( !ok ) {
                ok = fail( err, errsize, "Out of memory" );
                goto DONE;
            }
            unlink( logfile );
            int st = run_tool( 0, logfile, args.v );
            if ( st == 0 ) {
                ok = true;
                break;
            }
            if ( pass == 1 ) {          // the plain link failed, too: this is the error
                read_log( logfile, log, sizeof(log) );
                // the first message is the point; ld's own name in front of it is noise
                fail( err, errsize, "Link failed (%s)", log[0] != '\0' ? log : st < 0 ? "can't run the linker" : "see ENGINE1999_BUILD_TRACE" );
                ok = false;
                goto DONE;
            }
            unlink( outname );          // a failed PIE link: try the plain one
        }
    }
    ok = true;

DONE:
    if ( tmpdir[0] != '\0' ) {
        remove_tmp( tmpdir );
    }
    sl_free( &libdirs ); sl_free( &kitdirs ); sl_free( &args ); sl_free( &libpaths );
    free( ld ); free( interp ); free( gccdir ); free( robj ); free( rlib ); free( scrt1 ); free( crt1 ); free( crti ); free( crtn ); free( libc );
    return ok;
}
