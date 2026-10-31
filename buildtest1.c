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

#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "basbuild.h"
#include "basrepl.h"

/*
Automated test of the BUILD machinery (basbuild.c): finding the linker and the system files, embedding a program with ld -r -b binary,
linking a dynamic executable with the system linker only. No screen is needed: the executables made here are run, but the one
with the stand-in runtime (buildstub.o) just writes its embedded program out, and the one with the real runtime is only asked
for --engine1999-about.

Run it in the directory with buildstub.o, engine1999.o and engine.lib (the makefile does). Exit status = number of failed checks.
*/

static int checks = 0, failures = 0;

static void check( bool cond, const char* what ) {
    ++checks;
    if ( !cond ) {
        ++failures;
        printf( "FAIL: %s\n", what );
    }
}

static void checkf( bool cond, const char* what, const char* detail ) {
    ++checks;
    if ( !cond ) {
        ++failures;
        printf( "FAIL: %s (%s)\n", what, detail );
    }
}

// runs a command (no arguments with blanks), returns what it wrote in a malloc'ed buffer; *pstatus = its exit status
static unsigned char* run_capture( const char* cmd, size_t* plen, int* pstatus ) {
    FILE* p = popen( cmd, "r" );
    if ( p == 0 ) {
        *plen = 0; *pstatus = -1;
        return 0;
    }
    size_t cap = 4096U, len = 0;
    unsigned char* buf = (unsigned char*) malloc( cap );
    for (;;) {
        if ( len == cap ) {
            cap *= 2U;
            buf = (unsigned char*) realloc( buf, cap );
        }
        size_t got = fread( buf + len, 1U, cap - len, p );
        if ( got == 0 ) {
            break;
        }
        len += got;
    }
    int st = pclose( p );
    *plen = len;
    *pstatus = WIFEXITED( st ) ? WEXITSTATUS( st ) : -1;
    return buf;
}

static bool contains( const unsigned char* hay, size_t haylen, const char* needle ) {
    size_t n = strlen( needle );
    for ( size_t i=0; i + n <= haylen; ++i ) {
        if ( memcmp( hay + i, needle, n ) == 0 ) {
            return true;
        }
    }
    return false;
}

static unsigned char* read_file( const char* name, size_t* plen ) {
    FILE* f = fopen( name, "rb" );
    if ( f == 0 ) {
        *plen = 0;
        return 0;
    }
    fseek( f, 0, SEEK_END );
    long size = ftell( f );
    fseek( f, 0, SEEK_SET );
    unsigned char* buf = (unsigned char*) malloc( size > 0 ? (size_t) size : 1U );
    *plen = fread( buf, 1U, size > 0 ? (size_t) size : 0U, f );
    fclose( f );
    return buf;
}

static int count_entries( const char* dir ) {
    DIR* d = opendir( dir );
    int n = 0;
    if ( d == 0 ) {
        return -1;
    }
    struct dirent* e;
    while ( ( e = readdir( d ) ) != 0 ) {
        if ( strcmp( e->d_name, "." ) != 0 && strcmp( e->d_name, ".." ) != 0 ) {
            ++n;
        }
    }
    closedir( d );
    return n;
}

int main( void ) {
    // a program image with every byte value in it (zero bytes included)
    unsigned char prog[1024];
    for ( size_t i=0; i < sizeof(prog); ++i ) {
        prog[i] = (unsigned char)( i * 7U + ( i >> 8 ) );
    }
    char err[300];
    static const char* const nolibs[] = { 0 };
    build_opts_t opts;
    build_default_opts( &opts );
    opts.runtime_obj = "buildstub.o";
    opts.runtime_lib = 0;
    opts.libs = nolibs;
    opts.optlibs = nolibs;
    opts.kitdir = ".";

    // the work files go to a directory of our own, so that we can see that nothing is left behind
    char tmpl[] = "/tmp/buildtest1.XXXXXX";
    char* tmpdir = mkdtemp( tmpl );
    check( tmpdir != 0, "temp dir" );
    if ( tmpdir == 0 ) {
        return 1;
    }
    setenv( "TMPDIR", tmpdir, 1 );

    // -- a stand-in runtime: the embedded program comes out as it went in
    unlink( "buildtest1.out" );
    err[0] = '\0';
    bool ok = build_executable( "buildtest1.out", prog, sizeof(prog), &opts, err, sizeof(err) );
    checkf( ok, "build with the stand-in runtime", err );
    check( count_entries( tmpdir ) == 0, "no work files left behind" );
    struct stat st;
    check( stat( "buildtest1.out", &st ) == 0 && ( st.st_mode & S_IXUSR ) != 0, "the executable is executable" );
    size_t len = 0; int status = 0;
    unsigned char* out = run_capture( "./buildtest1.out", &len, &status );
    check( status == 7, "the executable ran and returned 7" );
    check( len == sizeof(prog) && memcmp( out, prog, len ) == 0, "the embedded program is intact (all byte values)" );
    free( out );
    size_t exelen = 0;
    unsigned char* exe = read_file( "buildtest1.out", &exelen );
    check( exelen > 1000U && exe[0] == 0x7F && exe[1] == 'E' && exe[2] == 'L' && exe[3] == 'F', "an ELF file" );
    check( contains( exe, exelen, "ld-linux" ) || contains( exe, exelen, "ld-musl" ) || contains( exe, exelen, "ld64.so" ) ||
           contains( exe, exelen, "ld.so" ), "dynamically linked (names the dynamic linker)" );
    check( contains( exe, exelen, "libc.so" ), "needs the shared C library" );
    free( exe );

    // building again over the old file works; a one byte program too
    unsigned char one = 0x55;
    ok = build_executable( "buildtest1.out", &one, 1U, &opts, err, sizeof(err) );
    checkf( ok, "build again over the old file", err );
    out = run_capture( "./buildtest1.out", &len, &status );
    check( len == 1U && out[0] == 0x55, "a one byte program" );
    free( out );
    unlink( "buildtest1.out" );

    // -- errors
    ok = build_executable( "buildtest1.out", prog, sizeof(prog), &opts, err, sizeof(err) );
    check( ok, "(again)" );
    unlink( "buildtest1.out" );
    check( !build_executable( "", prog, sizeof(prog), &opts, err, sizeof(err) ) && strstr( err, "Illegal argument" ) != 0, "empty name" );
    check( !build_executable( "buildtest1.out", prog, 0U, &opts, err, sizeof(err) ) && strstr( err, "No program" ) != 0, "no program" );
    check( !build_executable( "buildtest1.out", 0, 5U, &opts, err, sizeof(err) ), "null program" );
    {
        build_opts_t o = opts;
        o.runtime_obj = "nosuchobject1999.o";
        ok = build_executable( "buildtest1.out", prog, sizeof(prog), &o, err, sizeof(err) );
        check( !ok && strstr( err, "nosuchobject1999.o not found" ) != 0, "missing runtime object" );
        o = opts;
        o.runtime_lib = "nosuchlib1999.lib";
        ok = build_executable( "buildtest1.out", prog, sizeof(prog), &o, err, sizeof(err) );
        check( !ok && strstr( err, "nosuchlib1999.lib not found" ) != 0, "missing runtime library" );
        static const char* const badlibs[] = { "nosuchlib1999", 0 };
        o = opts;
        o.libs = badlibs;
        ok = build_executable( "buildtest1.out", prog, sizeof(prog), &o, err, sizeof(err) );
        check( !ok && strstr( err, "Library nosuchlib1999 not found" ) != 0, "missing system library" );
        o = opts;
        o.kitdir = "/nonexistent-dir-1999";
        o.runtime_obj = "buildstub.o";
        ok = build_executable( "buildtest1.out", prog, sizeof(prog), &o, err, sizeof(err) );
        check( ok, "the kit is also found in the current directory" );
        unlink( "buildtest1.out" );
    }
    ok = build_executable( "/nonexistent-dir-1999/x", prog, sizeof(prog), &opts, err, sizeof(err) );
    check( !ok && strstr( err, "Link failed" ) != 0, "the output can't be written" );
    check( count_entries( tmpdir ) == 0, "no work files left behind after a failure" );
    setenv( "ENGINE1999_LD", "/nonexistent-dir-1999/ld", 1 );
    ok = build_executable( "buildtest1.out", prog, sizeof(prog), &opts, err, sizeof(err) );
    check( !ok && strstr( err, "Linker" ) != 0, "no linker" );
    setenv( "ENGINE1999_LD", "/bin/false", 1 );
    ok = build_executable( "buildtest1.out", prog, sizeof(prog), &opts, err, sizeof(err) );
    check( !ok && strstr( err, "Linker failed" ) != 0, "a linker that fails" );
    check( count_entries( tmpdir ) == 0, "no work files left behind after a failing linker" );
    unsetenv( "ENGINE1999_LD" );

    // -- the real runtime with SDL2 (only asked for its notice: that needs no screen)
    if ( access( "engine1999.o", R_OK ) == 0 && access( "engine.lib", R_OK ) == 0 ) {
        build_opts_t real;
        build_default_opts( &real );
        real.kitdir = ".";
        unlink( "buildtest1.full" );
        ok = build_executable( "buildtest1.full", prog, sizeof(prog), &real, err, sizeof(err) );
        checkf( ok, "build with the real runtime and SDL2", err );
        if ( ok ) {
            out = run_capture( "./buildtest1.full --engine1999-about", &len, &status );
            check( status == 0, "the real runtime shows its notice and ends with 0" );
            check( out != 0 && contains( out, len, "Runtime Exception" ) && contains( out, len, "Ekkehard Morgenstern" ), "the notice names the exception and the author" );
            free( out );
        }
        unlink( "buildtest1.full" );
    } else {
        printf( "(no engine1999.o / engine.lib here: the real runtime was not tried)\n" );
    }

    // -- the console kit: a real program built into an executable that needs no SDL, and run
    if ( access( "engine1999c.o", R_OK ) == 0 && access( "engine.lib", R_OK ) == 0 ) {
        static repl_t repl;
        check( init_repl( &repl ), "a REPL for the program" );
        repl_enter( &repl, "10 PRINT \"built \";ARGC;ARG$(1);ARG$(2)" );
        repl_enter( &repl, "20 INPUT N$:PRINT \"hi \";N$" );
        repl_enter( &repl, "30 BYE 3" );
        uint8_t* image = 0; size_t imagelen = 0;
        check( repl_program_image( &repl, &image, &imagelen ), "the program as a file image" );
        build_opts_t con;
        build_console_opts( &con );
        con.kitdir = ".";
        unlink( "buildtest1.con" );
        ok = image != 0 && build_executable( "buildtest1.con", image, imagelen, &con, err, sizeof(err) );
        checkf( ok, "build with the console runtime", err );
        if ( ok ) {
            FILE* f = fopen( "buildtest1.in", "wb" );
            if ( f ) { fputs( "bob\n", f ); fclose( f ); }
            out = run_capture( "./buildtest1.con x 'y z' < buildtest1.in", &len, &status );
            check( status == 3, "the console executable ends with the code of BYE" );
            check( out != 0 && contains( out, len, "built 2xy z\n" ) && contains( out, len, "hi bob\n" ),
                "the console executable runs the program with its arguments and its input" );
            free( out );
            unlink( "buildtest1.in" );
            out = run_capture( "./buildtest1.con --engine1999-about", &len, &status );
            check( status == 0 && out != 0 && contains( out, len, "Runtime Exception" ) && contains( out, len, "console runtime" ), "the console notice" );
            free( out );
            size_t clen = 0;
            unsigned char* cexe = read_file( "buildtest1.con", &clen );
            check( cexe != 0 && !contains( cexe, clen, "libSDL2" ) && contains( cexe, clen, "libm.so" ), "no SDL2 in the console executable" );
            free( cexe );
        }
        unlink( "buildtest1.con" );
        free( image );
        cleanup_repl( &repl );
    } else {
        printf( "(no engine1999c.o here: the console runtime was not tried)\n" );
    }

    rmdir( tmpdir );
    printf( "%d checks, %d failed\n", checks, failures );
    return failures;
}
