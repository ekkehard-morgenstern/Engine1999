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
#include <unistd.h>
#include <sys/stat.h>

/*
Automated test of the security policy (policy.[ch]) without a REPL or a host: paths and symbolic links, the folder of a program, the questions (a fake
asker), the options, the grants file, the permission file. The exit status is the number of failed checks.
*/

static int checks = 0, failures = 0;

static void check( bool ok, const char* name ) {
    ++checks;
    if ( !ok ) {
        ++failures;
        printf( "FAIL  %s\n", name );
    }
}

static int asked = 0, answer = POL_ANS_DENY, askedcat = -1;
static char askedwhat[400];

static int fake_ask( void* ud, int category, const char* what ) {
    (void) ud;
    ++asked;
    askedcat = category;
    snprintf( askedwhat, sizeof(askedwhat), "%s", what );
    return answer;
}

static void put( const char* path, const char* text ) {
    FILE* f = fopen( path, "w" );
    if ( f != 0 ) { fputs( text, f ); fclose( f ); }
}

static char D[PATH_MAX];

static bool prog_start( const char* file, const char* content ) {
    put( file, content );
    policy_begin_program( file, 0, 0, 0 );
    return policy_active();
}

int main( void ) {
    strcpy( D, "/tmp/poltest1-XXXXXX" );
    if ( mkdtemp( D ) == 0 ) { printf( "no temp dir\n" ); return 99; }
    char p[PATH_MAX], q[PATH_MAX], g[PATH_MAX];
    snprintf( g, sizeof(g), "%s/grants", D );
    setenv( "ENGINE1999_GRANTS", g, 1 );
    snprintf( p, sizeof(p), "%s/app", D );   mkdir( p, 0700 );
    snprintf( p, sizeof(p), "%s/other", D ); mkdir( p, 0700 );
    snprintf( p, sizeof(p), "%s/app/sub", D ); mkdir( p, 0700 );
    snprintf( p, sizeof(p), "%s/app/link", D );
    snprintf( q, sizeof(q), "%s/other", D );
    if ( symlink( q, p ) != 0 ) { printf( "no symlink\n" ); return 99; }

    // -- normal forms and "inside"
    char out[PATH_MAX], exp[PATH_MAX + 8];
    check( policy_normalize( "/a/b/../c", out, sizeof(out) ) && strcmp( out, "/a/c" ) == 0, "normalize: .." );
    check( policy_normalize( "/a//b/./c/", out, sizeof(out) ) && strcmp( out, "/a/b/c" ) == 0, "normalize: // and ." );
    check( policy_normalize( "/../..", out, sizeof(out) ) && strcmp( out, "/" ) == 0, "normalize: above the root stays at the root" );
    char cwd[PATH_MAX];
    check( getcwd( cwd, sizeof(cwd) ) != 0 && policy_normalize( "x/y", out, sizeof(out) ), "normalize: a relative path" );
    strcpy( exp, strcmp( cwd, "/" ) == 0 ? "" : cwd );
    strcat( exp, "/x/y" );
    check( strcmp( out, exp ) == 0, "normalize: relative to the current directory" );
    snprintf( p, sizeof(p), "%s/app/link/f.txt", D );
    check( policy_normalize( p, out, sizeof(out) ) && strstr( out, "/other/f.txt" ) != 0 && strstr( out, "link" ) == 0, "normalize: a symbolic link is resolved, the missing file is kept" );
    snprintf( q, sizeof(q), "%s/app", D );
    snprintf( p, sizeof(p), "%s/app/sub/new/file", D );
    check( policy_inside( p, q ), "inside: a file that does not exist yet below the folder" );
    snprintf( p, sizeof(p), "%s/app/../other/f", D );
    check( !policy_inside( p, q ), "inside: .. leaves the folder" );
    snprintf( p, sizeof(p), "%s/app/link/f", D );
    check( !policy_inside( p, q ), "inside: a link that points out of the folder" );
    snprintf( p, sizeof(p), "%s/app", D );
    check( policy_inside( p, q ) && !policy_inside( D, q ), "inside: the folder itself, not its parent" );
    snprintf( p, sizeof(p), "%s/application/f", D );
    check( !policy_inside( p, q ), "inside: a folder with the same beginning is another folder" );

    // -- off by default
    check( policy_check( "write", "file", "/etc/x" ) && !policy_active(), "disabled: everything is allowed" );
    policy_enable( true );
    check( policy_check( "write", "file", "/etc/x" ), "enabled but no program runs: allowed (commands typed at the prompt)" );
    policy_set_asker( fake_ask, 0 );

    // -- a program: the folder, the questions
    char prog[PATH_MAX];
    snprintf( prog, sizeof(prog), "%s/app/game.bas", D );
    check( prog_start( prog, "10 PRINT 1\n" ), "a program starts" );
    snprintf( p, sizeof(p), "%s/app", D );
    char rt[PATH_MAX];
    check( policy_normalize( p, rt, sizeof(rt) ) && strcmp( policy_root(), rt ) == 0, "the folder is the folder of the program file" );
    asked = 0;
    snprintf( p, sizeof(p), "%s/app/sub/score.hi", D );
    check( policy_check( "write", "file", p ) && policy_check( "read", "file", p ) && policy_check( "list", "dir", p ) && asked == 0, "below the folder: no question" );
    char home[PATH_MAX];
    if ( getcwd( home, sizeof(home) ) != 0 && chdir( rt ) == 0 ) {
        check( policy_check( "write", "file", "relative.txt" ) && policy_check( "write", "file", "sub/x" ) && asked == 0, "a relative name is checked against the current directory (inside)" );
        if ( chdir( home ) != 0 ) {}
    }
    check( policy_check( "write", "mem", "/etc/x" ) && policy_check( "read", "null", "x" ) && policy_check( "read", "res", "x" ) && policy_check( "write", "play", "x" ) &&
           policy_check( "write", "con", "x" ) && policy_check( "read", "stdin", "" ) && asked == 0, "mem:, null:, res:, play:, con:, stdin: need nothing" );
    answer = POL_ANS_DENY;
    check( !policy_check( "read", "file", "/etc/hostname" ) && asked == 1 && askedcat == POL_READ && strstr( askedwhat, "/etc/hostname" ) != 0, "reading outside asks (read) and a no is a no" );
    check( !policy_check( "read", "file", "/etc/passwd" ) && asked == 1, "the no holds for the rest of the run (no second question)" );
    check( !policy_check( "list", "dir", "/etc" ) && asked == 1, "listing is reading" );
    check( !policy_check( "write", "file", "/tmp/x" ) && asked == 2 && askedcat == POL_WRITE, "writing is a power of its own" );
    snprintf( p, sizeof(p), "%s/app/../other/x", D );
    check( !policy_check( "write", "file", p ) && asked == 2, ".. out of the folder is outside (denied, decided already)" );
    answer = POL_ANS_SESSION;
    check( policy_check( "shell", 0, "ls -l" ) && asked == 3 && askedcat == POL_SHELL && strstr( askedwhat, "ls -l" ) != 0, "a shell literal asks (shell), yes for the session" );
    check( policy_check( "write", "sh", "ls" ) && asked == 3, "sh: is the same power" );
    check( policy_check( "read", "tcp", "host:80" ) && asked == 4 && askedcat == POL_NET, "tcp: asks (net)" );
    policy_end_program();
    check( !policy_active() && policy_check( "write", "file", "/etc/x" ), "after the program ended nothing is checked" );

    // -- the session grant belongs to this program (path + content)
    asked = 0;
    check( prog_start( prog, "10 PRINT 1\n" ) && policy_check( "shell", 0, "ls" ) && policy_check( "tcp", "tcp", "h:1" ) && asked == 0, "the same program again: the session grants are still there" );
    policy_end_program();
    check( prog_start( prog, "10 PRINT 2\n" ) && policy_check( "shell", 0, "ls" ) && asked == 1, "the program changed: no consent" );
    policy_end_program();

    // -- the grants file
    policy_forget_session();
    answer = POL_ANS_ALWAYS;
    asked = 0;
    check( prog_start( prog, "10 PRINT 3\n" ) && policy_check( "write", "file", "/tmp/x" ) && asked == 1, "always: asked once" );
    policy_end_program();
    struct stat st;
    check( stat( g, &st ) == 0 && ( st.st_mode & 077 ) == 0, "the grants file exists and is private" );
    policy_forget_session();
    asked = 0;
    check( prog_start( prog, "10 PRINT 3\n" ) && policy_check( "write", "file", "/tmp/y" ) && asked == 0, "always: a new process reads the file, no question" );
    check( !policy_check( "shell", 0, "ls" ) || asked == 1, "(another power is still asked)" );
    policy_end_program();
    asked = 0;
    check( prog_start( prog, "10 PRINT 4\n" ) && policy_check( "write", "file", "/tmp/y" ) && asked == 1, "always: the content changed, asked again" );
    policy_end_program();
    // 24 hours
    policy_forget_session();
    answer = POL_ANS_DAY;
    asked = 0;
    check( prog_start( prog, "10 PRINT 5\n" ) && policy_check( "read", "file", "/etc/hostname" ) && asked == 1, "a day: asked" );
    policy_end_program();
    FILE* f = fopen( g, "r" );
    char line[PATH_MAX + 64]; bool dayline = false;
    while ( f != 0 && fgets( line, sizeof(line), f ) != 0 ) {
        unsigned long long k; char name[16]; long long e = 0;
        if ( sscanf( line, "%llx %15s %lld", &k, name, &e ) == 3 && strcmp( name, "read" ) == 0 && e > (long long) time( 0 ) && e <= (long long) time( 0 ) + 86400LL ) dayline = true;
    }
    if ( f ) fclose( f );
    check( dayline, "a day: stored with an expiry within 24 hours" );
    policy_forget_session();
    asked = 0;
    check( prog_start( prog, "10 PRINT 5\n" ) && policy_check( "read", "file", "/etc/hostname" ) && asked == 0, "a day: later the same day no question" );
    policy_end_program();
    // an expired line is not honored: write one by hand with the key of this program ... (all lines are cut to the expired one)
    f = fopen( g, "r" );
    char all[8192]; size_t n = f ? fread( all, 1, sizeof(all) - 1U, f ) : 0; all[n] = '\0';
    if ( f ) fclose( f );
    char* sp = strstr( all, " read " );
    check( sp != 0, "(the read line is in the file)" );
    if ( sp != 0 ) {
        char* e = sp + 6;
        char* sp2 = strchr( e, ' ' );
        char rebuilt[8192];
        snprintf( rebuilt, sizeof(rebuilt), "%.*s 1000 %s", (int)( e - all ), all, sp2 + 1 );
        put( g, rebuilt );
    }
    policy_forget_session();
    asked = 0;
    check( prog_start( prog, "10 PRINT 5\n" ) && policy_check( "read", "file", "/etc/hostname" ) && asked == 1, "an expired line is ignored" );
    policy_end_program();

    // -- options
    policy_forget_session();
    char err[80];
    check( !policy_set_option( "read,bogus", true, err, sizeof(err) ) && strstr( err, "bogus" ) != 0, "an unknown word is an error" );
    policy_forget_session();
    check( policy_set_option( "read,Write", true, err, sizeof(err) ) && policy_set_option( "shell", false, err, sizeof(err) ), "--allow=read,write --deny=shell" );
    asked = 0;
    check( prog_start( prog, "10 PRINT 6\n" ) && policy_check( "read", "file", "/etc/x" ) && policy_check( "write", "file", "/etc/x" ) &&
           !policy_check( "shell", 0, "ls" ) && asked == 0, "the options decide without a question" );
    answer = POL_ANS_ALWAYS;
    check( policy_check( "tcp", "tcp", "h:1" ) && asked == 1, "(what the options leave open is asked)" );
    policy_end_program();
    policy_forget_session();
    check( policy_set_option( "all", true, err, sizeof(err) ), "--allow=all" );
    asked = 0;
    check( prog_start( prog, "10 PRINT 7\n" ) && policy_check( "tcp", "tcp", "h:1" ) && policy_check( "shell", 0, "ls" ) && asked == 0, "all: nothing is asked" );
    policy_end_program();
    policy_forget_session();

    // -- the permission file
    snprintf( p, sizeof(p), "%s/app/perm.bas", D );
    snprintf( q, sizeof(q), "%s/app/perm.perm", D );
    put( q, "# what this game needs\nrequest shell   # for the sound card tool\nrequest NET\nbogus line\n" );
    answer = POL_ANS_SESSION;
    asked = 0;
    check( prog_start( p, "10 PRINT 8\n" ) && asked == 2 && strstr( askedwhat, "net" ) != 0, "the permission file: one question for each power at the start" );
    check( policy_check( "shell", 0, "x" ) && policy_check( "tcp", "tcp", "x" ) && asked == 2, "...and they are granted" );
    check( policy_check( "write", "file", "/tmp/zz" ) && asked == 3, "(a power that was not requested is asked when it is needed)" );
    policy_end_program();
    answer = POL_ANS_DENY;
    policy_forget_session();
    asked = 0;
    check( prog_start( p, "10 PRINT 8\n" ) && asked == 2 && !policy_check( "shell", 0, "x" ) && asked == 2, "the permission file: a no at the start is a no for the run" );
    policy_end_program();
    put( q, "request shell\n" );
    policy_forget_session();
    answer = POL_ANS_SESSION;
    check( prog_start( p, "10 PRINT 8\n" ) && policy_check( "shell", 0, "x" ), "(the permission file changed: a new key)" );
    policy_end_program();

    // -- a program that was typed in
    policy_begin_program( 0, 0, 0, 0 );
    check( strcmp( policy_root(), cwd ) == 0, "typed in: the folder is the current directory" );
    answer = POL_ANS_ALWAYS;
    asked = 0;
    check( policy_check( "shell", 0, "ls" ) && asked == 1, "typed in: asked" );
    policy_end_program();
    policy_begin_program( 0, 0, 0, 0 );
    check( policy_check( "shell", 0, "ls" ) && asked == 2, "typed in: no key, so nothing is remembered" );
    policy_end_program();
    policy_begin_program( prog, "x", 1, "/tmp" );
    check( strcmp( policy_root(), "/tmp" ) == 0, "a root can be given (executables made with BUILD)" );
    policy_end_program();

    // clean up
    char cmd[PATH_MAX + 16];
    snprintf( cmd, sizeof(cmd), "rm -rf %s", D );
    if ( system( cmd ) != 0 ) {}
    printf( "%d checks, %d failure(s)\n", checks, failures );
    return failures;
}
