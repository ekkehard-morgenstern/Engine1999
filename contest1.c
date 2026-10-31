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

#include "stdtypes.h"
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

/*
Automated test of the console host (engine1999c.c): runs ./engine1999c in a temporary directory with a program file and / or text on
standard input and compares standard output, standard error and the exit status. No window, no SDL. Run it in the build directory.
Exit status = number of failed cases.
*/

typedef struct _testcase_t {
    const char* name;
    const char* prog;       // the contents of prog.bas in the work directory (0 = none)
    const char* args;       // the command line arguments
    const char* input;      // standard input
    const char* out;        // expected standard output (exact)
    const char* err;        // expected standard error (exact; if errpart: a part of it); 0 = not checked
    int         status;
    bool        errpart;
} testcase_t;

static const testcase_t cases[] = {
    // the REPL on standard input
    { "a direct line",              0, "", "PRINT 1+2\n",                               "3\n", "", 0, false },
    { "a stored program and RUN",   0, "", "10 PRINT \"HI\"\nRUN\n",                  "HI\n", "", 0, false },
    { "empty lines are skipped",    0, "", "\n  \nPRINT 1\n",                          "1\n", "", 0, false },
    { "LIST",                       0, "", "10 PRINT 1\n20 PRINT 2\nLIST\n",           "10 PRINT 1\n20 PRINT 2\n", "", 0, false },
    // keys from standard input: cursor and function keys are escape sequences in their canonical form, whatever the terminal sent
    { "GET: a cursor key is a whole sequence",  "10 GET A$:PRINT LEN(A$);MID$(A$,2)\n", "prog.bas", "\033[A",       "3[A\n", "", 0, false },
    { "GET: a function key (SS3 form)",         "10 GET A$:PRINT LEN(A$);MID$(A$,2)\n", "prog.bas", "\033OP",       "3OP\n", "", 0, false },
    { "GET: modifiers",                         "10 GET A$:PRINT LEN(A$);MID$(A$,2)\n", "prog.bas", "\033[1;5D",    "6[1;5D\n", "", 0, false },
    { "GET: another dialect becomes the canonical form (rxvt Home)", "10 GET A$:PRINT MID$(A$,2)\n", "prog.bas", "\033[7~", "[H\n", "", 0, false },
    { "GET: Shift+Tab",                         "10 GET A$:PRINT LEN(A$);MID$(A$,2)\n", "prog.bas", "\033[Z",       "3[Z\n", "", 0, false },
    { "GET: an unknown sequence is skipped",    "10 GET A$:PRINT A$\n", "prog.bas", "\033[Mx",                "x\n", "", 0, false },
    { "GET: a lone ESC at the end of the input", "10 GET A$:PRINT ASC(A$)\n", "prog.bas", "\033",                  "27\n", "", 0, false },
    { "GET: ESC and a letter are two keys",     "10 GET A$,B$:PRINT LEN(A$);B$\n", "prog.bas", "\033x",             "1x\n", "", 0, false },
    { "GET: bytes above 127 from a pipe stay as they are", "10 GET A$,B$:PRINT ASC(A$);ASC(B$)\n", "prog.bas", "\xc3\xa4", "195164\n", "", 0, false },
    // the key dump (--debug-keys): the bytes of standard input item by item ("[keys]" in the expected output: a part of it)
    { "key dump: a cursor key with modifiers",  0, "--debug-keys", "\033[1;2A",      "[keys]Shift+Up (canonical)\n", "", 0, false },
    { "key dump: a function key",               0, "--debug-keys", "\033[15~",       "[keys]F5 (canonical)\n", "", 0, false },
    { "key dump: another form of a key",        0, "--debug-keys", "\033[7~",        "[keys]Home (other form", "", 0, false },
    { "key dump: an unknown sequence",          0, "--debug-keys", "\033[M",         "[keys]unknown escape sequence\n", "", 0, false },
    { "key dump: ESC alone",                    0, "--debug-keys", "\033",           "[keys]ESC alone", "", 0, false },
    { "key dump: Alt+x",                        0, "--debug-keys", "\033x",          "[keys]ESC + 'x' (Alt+x", "", 0, false },
    { "key dump: UTF-8",                        0, "--debug-keys", "\xc3\xa4",       "[keys]U+00E4 (UTF-8)\n", "", 0, false },
    { "key dump: a byte that is no UTF-8",      0, "--debug-keys", "\xe4",           "[keys]byte >= 0x80", "", 0, false },
    { "key dump: control codes and DEL",        0, "--debug-keys", "\r\177\t",       "[keys]Enter (Ctrl-M, CR)\n    7f                       \"\\x7f\"                 DEL", "", 0, false },
    { "key dump: Ctrl-D stops it",              0, "--debug-keys", "a\004b",         "[keys]'a' (U+0061)\n    04                       \"\\x04\"                 Ctrl-D\n\nEnd of the key dump.\n", "", 0, false },
    { "key dump: the end of the input stops it", 0, "--debug-keys", "",              "[keys]End of the key dump.\n", "", 0, false },
    { "BYE ends the input",         0, "", "PRINT 1\nBYE\nPRINT 2\n",                  "1\n", "", 0, false },
    { "BYE with a code",            0, "", "PRINT 1\nBYE 4\nPRINT 2\n",                "1\n", "", 4, false },
    { "QUIT with a code",           0, "", "QUIT 9\n",                                  "", "", 9, false },
    { "an error goes to standard error and the input goes on", 0, "", "PRINT 1/0\nPRINT 5\n", "5\n", "? Division by zero\n", 0, false },
    { "a syntax error",             0, "", "PRINT )\nPRINT 6\n",                        "6\n", "?", 0, true },
    { "INPUT reads the next line",  0, "", "10 INPUT \"NAME\";A$\n20 PRINT \"[\";A$;\"]\"\nRUN\nbob\n", "NAME? [bob]\n", "", 0, false },
    { "INPUT numbers",              0, "", "10 INPUT A,B:PRINT A+B\nRUN\n3,4\n",       "? 7\n", "", 0, false },
    { "GET reads one key",          0, "", "10 GET K$:PRINT ASC(K$)\n20 INPUT A$:PRINT A$\nRUN\nxbob\n", "120\n? bob\n", "", 0, false },
    { "GET: the line end is RETURN", 0, "", "10 GET K$:PRINT ASC(K$)\nRUN\n\n",         "13\n", "", 0, false },
    { "GET at the end of input ends the program", 0, "", "10 GET K$\n20 PRINT 1\nRUN\n", "", "? End of input in line 10\n", 0, false },
    { "INKEY$ without input is empty", 0, "", "10 A$=INKEY$:PRINT LEN(A$)\nRUN\n",       "0\n", "", 0, false },
    { "INKEY$ sees a key",          0, "", "10 A$=INKEY$:PRINT A$\nRUN\nq",            "q\n", "", 0, false },
    { "INPUT at the end of input",  0, "", "10 INPUT A$\n20 PRINT 1\nRUN\n",           "? ", "? End of input in line 10\n", 0, false },
    { "WAIT works",                 0, "", "10 WAIT 6:PRINT 1\nRUN\n",                  "1\n", "", 0, false },
    { "TI counts",                  0, "", "WAIT 10:PRINT TI>5\n",                      "-1\n", "", 0, false },
    // display and sound statements
    { "CLS prints nothing without a terminal", 0, "", "CLS\nPRINT 1\n",                 "1\n", "", 0, false },
    { "LOCATE prints nothing without a terminal", 0, "", "LOCATE 5,5:PRINT 1\n",         "1\n", "", 0, false },
    { "COLOR is accepted",          0, "", "COLOR 3:PRINT 1\n",                         "1\n", "", 0, false },
    { "sound statements are silent", 0, "", "PLAY \"CDE\"\nSOUND 1,440,1\nPRINT PLAYING(1)\n", "0\n", "", 0, false },
    { "sound arguments are still checked", 0, "", "SOUND 9,440,1\n",                    "", "? Illegal argument\n", 0, false },
    { "a graphics statement fails", 0, "", "GFX LAYER 1,ON\nPRINT 2\n",                 "2\n", "? Not available in the console host\n", 0, false },
    { "a graphics statement in a program", 0, "", "10 PRINT 1\n20 SPRITE 1,ON\nRUN\n", "1\n", "? Not available in the console host in line 20\n", 0, true },
    { "LIST is plain in a pipe",    0, "", "10 PRINT 1\nLIST\n",                         "10 PRINT 1\n", "", 0, false },
    { "--color=auto is plain in a pipe", 0, "--color=auto", "10 PRINT 1\nLIST\n",       "10 PRINT 1\n", "", 0, false },
    { "--color=never",              0, "--color=never", "10 PRINT 1\nLIST\n",          "10 PRINT 1\n", "", 0, false },
    { "--color=always colors LIST", 0, "--color=always", "10 PRINT 1\nLIST\n",         "\033[33m10 \033[34mPRINT \033[31m1\033[0m\n", "", 0, false },
    { "--color colors LIST",        0, "--color", "10 PRINT 1\nLIST\n",                "\033[33m10 \033[34mPRINT \033[31m1\033[0m\n", "", 0, false },
    { "colors don't touch PRINT",   0, "--color=always", "PRINT \"x\"\n",              "x\n", "", 0, false },
    { "a bad --color value",        0, "--color=blue", "",                               "", "bad value 'blue' for --color", 2, true },
    { "EDIT and MONITOR need a screen", 0, "", "EDIT\nMONITOR\nPRINT 3\n",            "3\n", "? Not available in the console host\n? Not available in the console host\n", 0, false },
    // run mode
    { "run a file",                 "10 PRINT \"RUN MODE\"\n", "prog.bas", "", "RUN MODE\n", "", 0, false },
    { "a #! first line is skipped", "#!/usr/bin/env engine1999c\n10 PRINT 1\n20 PRINT 2\n", "prog.bas", "", "1\n2\n", "", 0, false },
    { "BYE code of a file",         "10 BYE 9\n", "prog.bas", "", "", "", 9, false },
    { "a program that just ends",   "10 PRINT 1\n", "prog.bas", "", "1\n", "", 0, false },
    { "an error in a file",         "10 PRINT 1\n20 PRINT 1/0\n", "prog.bas", "", "1\n", "? Division by zero in line 20\n", 1, false },
    { "the file is found without extension", "10 PRINT 3\n", "prog", "", "3\n", "", 0, false },
    { "a file that is not there",   0, "nothere.bas", "", "", "? File not found\n", 2, false },
    { "INPUT in a file reads standard input", "10 INPUT A$\n20 PRINT \"<\";A$;\">\"\n", "prog.bas", "hello\n", "? <hello>\n", "", 0, false },
    { "-i goes on with standard input", "10 PRINT 1\n", "-i prog.bas", "PRINT 7\n", "1\n7\n", "", 0, false },
    { "-i and BYE in the input",    "10 PRINT 1\n", "-i prog.bas", "BYE 3\n", "1\n", "", 3, false },
    { "- reads the lines from standard input", 0, "-", "PRINT 8\n",                      "8\n", "", 0, false },
    // program arguments, the environment, the standard channels
    { "ARGC and ARG$ of a file",    "10 PRINT ARGC;\"|\";ARG$(0);\"|\";ARG$(1);\"|\";ARG$(2)\n", "prog.bas x \"y z\"", "", "2|prog.bas|x|y z\n", "", 0, false },
    { "no arguments",               "10 PRINT ARGC\n", "prog.bas", "", "0\n", "", 0, false },
    { "options behind the file are the program's", "10 PRINT ARG$(1);ARG$(2)\n", "prog.bas --version -i", "", "--version-i\n", "", 0, false },
    { "ARGC with standard input",   0, "- a b", "PRINT ARGC;ARG$(0);ARG$(2)\n", "2-b\n", "", 0, false },
    { "ARGC without any",           0, "", "PRINT ARGC;\"[\";ARG$(0);\"]\"\n", "0[]\n", "", 0, false },
    { "ENV$",                       0, "", "PRINT ENV$(\"E1999_CON\")\n", "from the shell\n", "", 0, false },
    { "ENV$ of a variable that is not set", 0, "", "PRINT \"[\";ENV$(\"E1999_NOT_SET\");\"]\"\n", "[]\n", "", 0, false },
    { "a program that takes an argument", "10 IF ARGC<1 THEN PRINT \"usage\":BYE 2\n20 PRINT \"hello \";ARG$(1)\n", "prog.bas world", "", "hello world\n", "", 0, false },
    { "the same without the argument", "10 IF ARGC<1 THEN PRINT \"usage\":BYE 2\n20 PRINT \"hello \";ARG$(1)\n", "prog.bas", "", "usage\n", "", 2, false },
    { "stdout: and stderr: as channels", "10 PRINT \"a\"\n20 OPEN \"stdout:\" FOR OUTPUT AS #1:PRINT #1,\"b\"\n30 OPEN \"stderr:\" FOR OUTPUT AS #2:PRINT #2,\"e\":CLOSE\n40 PRINT \"c\"\n", "prog.bas", "", "a\nb\nc\n", "e\n", 0, false },
    { "OPEN \"I\", \"O\" forms",     "10 OPEN \"O\",#1,\"stderr:\":PRINT #1,\"warning\":CLOSE #1\n20 PRINT \"done\"\n", "prog.bas", "", "done\n", "warning\n", 0, false },
    { "stdin: as a channel",        "10 OPEN \"stdin:\" FOR INPUT AS #1\n20 LINE INPUT #1,A$:PRINT \"<\";A$;\">\"\n30 IF NOT EOF(1) THEN 20\n40 CLOSE #1\n", "prog.bas", "one\ntwo\nthree\n", "<one>\n<two>\n<three>\n", "", 0, false },
    { "stdin: and INPUT take turns", "10 OPEN \"stdin:\" FOR INPUT AS #1:LINE INPUT #1,A$:CLOSE #1\n20 INPUT B$\n30 PRINT A$;\"+\";B$\n", "prog.bas", "first\nsecond\n", "? first+second\n", "", 0, false },
    { "stdin: can't be opened for output", "10 OPEN \"stdin:\" FOR OUTPUT AS #1\n", "prog.bas", "", "", "? Bad file mode in line 10\n", 1, false },
    { "a name behind stdout: is refused", "10 OPEN \"stdout:x\" FOR OUTPUT AS #1\n", "prog.bas", "", "", "? Bad file name in line 10\n", 1, false },
    // the command line
    { "--version",                  0, "--version", "", "Engine 1999 v", 0, 0, false },
    { "--help",                     0, "--help", "", "Usage:", 0, 0, false },
    { "an unknown option",          0, "--nonsense", "", "", "unknown option", 2, true },
    { "--debug-tokenize",           0, "--debug-tokenize", "PRINT 1\n", "[tokenize]", 0, 0, false },
};

static char workdir[] = "/tmp/contest1.XXXXXX";
static char exe[1024];

static void put( const char* name, const char* text ) {
    char path[1200];
    snprintf( path, sizeof(path), "%s/%s", workdir, name );
    FILE* f = fopen( path, "wb" );
    if ( f ) {
        fwrite( text, 1U, strlen( text ), f );
        fclose( f );
    }
}

static char* slurp( const char* name, size_t* plen ) {
    char path[1200];
    snprintf( path, sizeof(path), "%s/%s", workdir, name );
    FILE* f = fopen( path, "rb" );
    char* buf = (char*) malloc( 1U );
    size_t len = 0, cap = 1U;
    buf[0] = '\0';
    while ( f != 0 ) {
        if ( len + 4096U > cap ) {
            cap = len + 8192U;
            buf = (char*) realloc( buf, cap );
        }
        size_t got = fread( buf + len, 1U, 4096U, f );
        if ( got == 0 ) {
            break;
        }
        len += got;
    }
    if ( f ) {
        fclose( f );
    }
    buf[len] = '\0';
    *plen = len;
    return buf;
}

// runs engine1999c in the work directory; returns the exit status, out and err hold what it wrote
static int run( const char* args, const char* input, char** out, size_t* outlen, char** err ) {
    put( "in.txt", input ? input : "" );
    char cmd[3000];
    snprintf( cmd, sizeof(cmd), "cd %s && %s %s < in.txt > out.txt 2> err.txt", workdir, exe, args );
    int st = system( cmd );
    size_t errlen;
    *out = slurp( "out.txt", outlen );
    *err = slurp( "err.txt", &errlen );
    return WIFEXITED( st ) ? WEXITSTATUS( st ) : -1;
}

static void clean( void ) {
    char cmd[300];
    snprintf( cmd, sizeof(cmd), "rm -rf %s", workdir );
    if ( system( cmd ) != 0 ) {
        // nothing to do
    }
}

int main( void ) {
    char cwd[900];
    if ( getcwd( cwd, sizeof(cwd) ) == 0 ) {
        return 1;
    }
    snprintf( exe, sizeof(exe), "%s/engine1999c", cwd );
    setenv( "E1999_CON", "from the shell", 1 );
    unsetenv( "E1999_NOT_SET" );
    if ( access( exe, X_OK ) != 0 ) {
        printf( "engine1999c is not here: run this test in the build directory\n" );
        return 1;
    }
    if ( mkdtemp( workdir ) == 0 ) {
        return 1;
    }
    int failed = 0;
    size_t n = sizeof(cases) / sizeof(cases[0]);
    for ( size_t i=0; i < n; ++i ) {
        const testcase_t* tc = &cases[i];
        char cmd[300];
        snprintf( cmd, sizeof(cmd), "rm -f %s/prog.bas %s/prog", workdir, workdir );
        if ( system( cmd ) != 0 ) { }
        if ( tc->prog != 0 ) {
            put( "prog.bas", tc->prog );
        }
        char *out, *err; size_t outlen;
        int status = run( tc->args, tc->input, &out, &outlen, &err );
        bool okout = tc->out == 0 ? true : ( strncmp( tc->out, "Engine 1999 v", 13 ) == 0 || strcmp( tc->out, "Usage:" ) == 0 ||
            strcmp( tc->out, "[tokenize]" ) == 0 ) ? strstr( out, tc->out ) == out || strstr( out, tc->out ) != 0 :
            strncmp( tc->out, "[keys]", 6U ) == 0 ? strstr( out, tc->out + 6 ) != 0 : strcmp( out, tc->out ) == 0;
        bool okerr = tc->err == 0 ? true : tc->errpart ? strstr( err, tc->err ) != 0 : strcmp( err, tc->err ) == 0;
        bool ok = okout && okerr && status == tc->status;
        printf( "%s  %s\n", ok ? "PASS" : "FAIL", tc->name );
        if ( !ok ) {
            ++failed;
            printf( "    status %d (expected %d)\n    out: \"%s\"\n    err: \"%s\"\n", status, tc->status, out, err );
        }
        free( out ); free( err );
    }

    // cases that need more than a table row
    {
        char *out, *err; size_t outlen;
        // binary-safe output
        int st = run( "-", "PRINT CHR$(0);\"x\"\n", &out, &outlen, &err );
        bool ok = st == 0 && outlen == 3U && out[0] == '\0' && out[1] == 'x' && out[2] == '\n';
        printf( "%s  CHR$(0) reaches the output\n", ok ? "PASS" : "FAIL" );
        failed += !ok;
        free( out ); free( err );

        // a binary program file (SAVE writes .e99) runs
        st = run( "-", "10 PRINT 3\nSAVE \"binprog\"\n", &out, &outlen, &err );
        free( out ); free( err );
        st = run( "binprog.e99", "", &out, &outlen, &err );
        ok = st == 0 && strcmp( out, "3\n" ) == 0;
        printf( "%s  a binary program file (.e99) runs\n", ok ? "PASS" : "FAIL" );
        if ( !ok ) { printf( "    status %d out \"%s\" err \"%s\"\n", st, out, err ); }
        failed += !ok;
        free( out ); free( err );

        // BUILD: console executables run without SDL, WINDOW ones link it
        put( "prog.bas", "10 PRINT \"from built \";ARGC;ARG$(1)\n20 BYE 4\n" );
        char cmd0[3000];
        snprintf( cmd0, sizeof(cmd0), "rm -f %s/built %s/builtc %s/builtw", workdir, workdir, workdir );
        if ( system( cmd0 ) != 0 ) { }
        st = run( "-", "LOAD \"prog.bas\"\nBUILD \"built\"\nBUILD \"builtc\", CONSOLE\nBUILD \"builtw\", WINDOW\n", &out, &outlen, &err );
        ok = st == 0 && err[0] == '\0';
        printf( "%s  BUILD in the console host\n", ok ? "PASS" : "FAIL" );
        if ( !ok ) { printf( "    status %d err \"%s\"\n", st, err ); }
        failed += !ok;
        free( out ); free( err );
        for ( int k = 0; k < 2; ++k ) {
            char cmdk[3000];
            snprintf( cmdk, sizeof(cmdk), "cd %s && ./%s x < /dev/null > out.txt 2> err.txt", workdir, k == 0 ? "built" : "builtc" );
            st = system( cmdk );
            size_t ol;
            char* o = slurp( "out.txt", &ol );
            ok = WIFEXITED( st ) && WEXITSTATUS( st ) == 4 && strcmp( o, "from built 1x\n" ) == 0;
            printf( "%s  a built console executable runs (%s)\n", ok ? "PASS" : "FAIL", k == 0 ? "default" : "CONSOLE" );
            if ( !ok ) { printf( "    status %d out \"%s\"\n", st, o ); }
            failed += !ok;
            free( o );
        }
        char cmdg[3000];
        snprintf( cmdg, sizeof(cmdg), "grep -c libSDL2 %s/built %s/builtc %s/builtw > %s/g.txt", workdir, workdir, workdir, workdir );
        if ( system( cmdg ) != 0 ) { }
        size_t gl;
        char* g = slurp( "g.txt", &gl );
        ok = strstr( g, "builtw:1" ) != 0 && strstr( g, "built:0" ) != 0 && strstr( g, "builtc:0" ) != 0;
        printf( "%s  the console executables don't need libSDL2, the WINDOW one does\n", ok ? "PASS" : "FAIL" );
        if ( !ok ) { printf( "    grep: %s\n", g ); }
        failed += !ok;
        free( g );

        // Ctrl-C is Break: SIGINT to a program in an endless loop
        put( "prog.bas", "10 GOTO 10\n" );
        char cmd[3000];
        snprintf( cmd, sizeof(cmd), "cd %s && ( %s prog.bas < /dev/null > out.txt 2> err.txt & P=$!; sleep 1; kill -INT $P; wait $P; echo $? > st.txt )",
            workdir, exe );
        if ( system( cmd ) != 0 ) { }
        size_t l1, l2;
        char* stt = slurp( "st.txt", &l1 );
        char* e2 = slurp( "err.txt", &l2 );
        ok = atoi( stt ) == 1 && strstr( e2, "Break in line 10" ) != 0;
        printf( "%s  Ctrl-C is Break\n", ok ? "PASS" : "FAIL" );
        if ( !ok ) { printf( "    status %s err \"%s\"\n", stt, e2 ); }
        failed += !ok;
        free( stt ); free( e2 );
    }

    clean();
    printf( "%d failure(s)\n", failed );
    return failed;
}
