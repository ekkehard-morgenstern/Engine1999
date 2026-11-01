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

#define _XOPEN_SOURCE 700           // posix_openpt() and friends for the terminal tests
#include "stdtypes.h"
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <time.h>

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
    // the lines come through the same editor as the ones typed on a terminal (the keys are the bytes of the input)
    { "CR LF line ends in the input",               0, "", "PRINT 1\r\nPRINT 2\r\n",           "1\n2\n", "", 0, false },
    { "the last line without a line end",           0, "", "PRINT 1\nPRINT 2",                 "1\n2\n", "", 0, false },
    { "an empty input",                             0, "", "",                                   "", "", 0, false },
    { "INPUT reads the next line of the input",     0, "", "10 INPUT A$\n20 PRINT A$;LEN(A$)\nRUN\nhello\n", "? hello5\n", "", 0, false },
    { "INPUT at the end of the input",              0, "", "10 INPUT A$\nRUN\n",              "? ", "? End of input in line 10\n", 0, true },
    { "an escape sequence in the input is a key, not text", 0, "", "PRINT 1\033[A\n",         "1\n", "", 0, false },
    // EDIT n: the one-line editor on the keys of the input (the text of the line is there to be edited)
    { "EDIT n: the line is edited with keys from the input (End, a character, RETURN)", 0, "", "10 PRINT 1\nEDIT 10\n\033[F0\nLIST\n", "10 PRINT 10\n", "", 0, false },
    { "AUTO from a pipe: numbered lines are stored, no prompts are written", 0, "", "AUTO 100,5\nPRINT 1\nPRINT 2\n\nLIST\n", "100 PRINT 1\n105 PRINT 2\n", "", 0, false },
    { "AUTO: the next lines are entered after it, an empty line ends it", 0, "", "AUTO 10\nA=1\n\nPRINT 5\nLIST\n", "5\n10 A=1\n", "", 0, false },
    { "EDIT n: ESC cancels, the line stays",        0, "", "10 PRINT 1\nEDIT 10\n\033\nLIST\n", "10 PRINT 1\n", "", 0, false },
    { "EDIT n: Home and DELETE edit the number's line", 0, "", "10 PRINT 1\nEDIT 10\n\033[H\033[3~\033[3~\033[3~\033[3~2 \n", "", "", 0, false },
    { "EDIT of a line that does not exist",         0, "", "EDIT 99\nPRINT 5\n",             "5\n", "? Undefined line number 99\n", 0, false },
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
    { "KEY() is not available (a terminal can't tell when a key is let go)", 0, "", "PRINT KEY(\"A\")\nPRINT 2\n", "2\n", "? KEY is not available in this host\n", 0, false },
    { "KEY(): a wrong name is checked first", 0, "", "PRINT KEY(\"BANANA\")\n", "", "? Illegal argument\n", 0, false },
    { "a graphics statement in a program", 0, "", "10 PRINT 1\n20 SPRITE 1,ON\nRUN\n", "1\n", "? Not available in the console host in line 20\n", 0, true },
    { "LIST is plain in a pipe",    0, "", "10 PRINT 1\nLIST\n",                         "10 PRINT 1\n", "", 0, false },
    { "--color=auto is plain in a pipe", 0, "--color=auto", "10 PRINT 1\nLIST\n",       "10 PRINT 1\n", "", 0, false },
    { "--color=never",              0, "--color=never", "10 PRINT 1\nLIST\n",          "10 PRINT 1\n", "", 0, false },
    { "--color=always colors LIST", 0, "--color=always", "10 PRINT 1\nLIST\n",         "\033[33m10 \033[34mPRINT \033[31m1\033[0m\n", "", 0, false },
    { "--color colors LIST",        0, "--color", "10 PRINT 1\nLIST\n",                "\033[33m10 \033[34mPRINT \033[31m1\033[0m\n", "", 0, false },
    { "colors don't touch PRINT",   0, "--color=always", "PRINT \"x\"\n",              "x\n", "", 0, false },
    { "a bad --color value",        0, "--color=blue", "",                               "", "bad value 'blue' for --color", 2, true },
    { "EDIT and MONITOR need a terminal", 0, "", "EDIT\nMONITOR\nPRINT 3\n",            "3\n", "? EDIT and MONITOR need a terminal\n? EDIT and MONITOR need a terminal\n", 0, false },
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


// -- the line editor on a terminal: engine1999c runs on a pseudo terminal, keys are sent one step at a time -----------------------------------------

static const char* pty_screen = "0";            // E1999_SCREEN of the child: the old terminal tests are for the classic line mode
static const char* pty_args[8];                 // extra arguments of the child (0 terminated)

// runs engine1999c on a pty, writes the steps (with a pause behind each), collects everything it writes; returns the exit status (-1: it did not end)
static int run_pty( const char* const* steps, size_t nsteps, char* out, size_t outcap ) {
    int master = posix_openpt( O_RDWR | O_NOCTTY );
    if ( master < 0 || grantpt( master ) != 0 || unlockpt( master ) != 0 ) {
        return -2;
    }
    const char* slave = ptsname( master );
    struct winsize ws = { 24, 80, 0, 0 };
    ioctl( master, TIOCSWINSZ, &ws );
    pid_t pid = fork();
    if ( pid < 0 || slave == 0 ) {
        return -2;
    }
    if ( pid == 0 ) {
        setsid();
        int fd = open( slave, O_RDWR );
        if ( fd < 0 ) {
            _exit( 127 );
        }
        ioctl( fd, TIOCSCTTY, 0 );
        dup2( fd, 0 ); dup2( fd, 1 ); dup2( fd, 2 );
        if ( chdir( workdir ) != 0 ) {
            _exit( 127 );
        }
        setenv( "E1999_SCREEN", pty_screen, 1 );
        setenv( "TERM", "xterm-256color", 1 );
        const char* av[12];
        int ac = 0;
        av[ac++] = exe;
        for ( int k = 0; pty_args[k] != 0; ++k ) av[ac++] = pty_args[k];
        av[ac] = 0;
        execv( exe, (char* const*) av );
        _exit( 127 );
    }
    size_t n = 0, answered = 0;
    out[0] = '\0';
    for ( size_t i = 0; i <= nsteps + 1U; ++i ) {       // (step 0 is the start: wait for the banner, the terminal must be in its raw mode before keys arrive)
        if ( i >= 1U && i <= nsteps ) {
            const char* st = steps[i - 1U];
            unsigned rows, cols;
            if ( strncmp( st, "@size ", 6U ) == 0 && sscanf( st + 6, "%u %u", &rows, &cols ) == 2 ) {      // the terminal is resized (the process gets SIGWINCH)
                struct winsize nws = { (unsigned short) rows, (unsigned short) cols, 0, 0 };
                ioctl( master, TIOCSWINSZ, &nws );
            } else if ( write( master, st, strlen( st ) ) < 0 ) { }
        }
        int quiet = 0;
        for ( int ms = 0; ms < 5000 && quiet < 200; ms += 20 ) {       // collect until it has been quiet for 200 ms
            struct pollfd pfd = { master, POLLIN, 0 };
            int timeout = 20;
            bool got_any = false;
            while ( poll( &pfd, 1, timeout ) > 0 ) {                    // everything that is there
                timeout = 0;
                ssize_t got = read( master, out + n, outcap - 1U - n );
                if ( got <= 0 ) {
                    break;
                }
                got_any = true;
                n += (size_t) got;
                out[n] = '\0';
                // the terminal answers "where is the cursor?" (ESC [ 6 n) like a real one: row 20, column 1
                for ( const char* q = out + answered; ( q = strstr( q, "\033[6n" ) ) != 0; q += 4 ) {
                    if ( write( master, "\033[20;1R", 8U ) < 0 ) { }
                }
                answered = n >= 3U ? n - 3U : 0;
            }
            quiet = got_any ? 0 : quiet + 20;
        }
    }
    int st = -1;
    for ( int tries = 0; tries < 50; ++tries ) {
        int w;
        if ( waitpid( pid, &w, WNOHANG ) == pid ) {
            st = WIFEXITED( w ) ? WEXITSTATUS( w ) : WIFSIGNALED( w ) ? 128 + WTERMSIG( w ) : -1;
            break;
        }
        struct timespec ts = { 0, 20000000L };
        nanosleep( &ts, 0 );
    }
    if ( st == -1 ) {
        kill( pid, SIGKILL );
        waitpid( pid, 0, 0 );
    }
    close( master );
    return st;
}

// the output without the escape sequences (CSI ... final byte): the text the user sees, to look for parts in
static void strip_csi( const char* in, char* out ) {
    while ( *in ) {
        if ( in[0] == 033 && in[1] == '[' ) {
            in += 2;
            while ( *in && !( *in >= 0x40 && *in <= 0x7E ) ) {
                ++in;
            }
            if ( *in ) {
                ++in;
            }
        } else if ( in[0] == 033 ) {
            ++in;
        } else {
            *out++ = *in++;
        }
    }
    *out = '\0';
}

// each expected part must be in the output, either as it was written (escape sequences included) or in the text without them
static bool pty_case_st( const char* name, const char* const* steps, size_t nsteps, const char* const* expect, size_t nexpect, int status ) {
    static char out[262144], plain[262144];
    int st = run_pty( steps, nsteps, out, sizeof(out) );
    strip_csi( out, plain );
    bool ok = st == status;
    for ( size_t i = 0; i < nexpect && ok; ++i ) {
        ok = strstr( out, expect[i] ) != 0 || strstr( plain, expect[i] ) != 0;
    }
    ok = ok && strstr( out, "^[" ) == 0;        // no echo of an escape sequence by the terminal
    printf( "%s  %s\n", ok ? "PASS" : "FAIL", name );
    if ( !ok ) {
        printf( "    status %d (expected %d), text without escape sequences:\n", st, status );
        size_t len = strlen( plain );
        for ( const char* p = len > 1500U ? plain + len - 1500U : plain; *p; ++p ) {
            if ( *p == 033 ) printf( "\\e" ); else if ( *p == '\r' ) printf( "\\r" ); else if ( *p == '\n' ) printf( "\\n\n" ); else putchar( *p );
        }
        printf( "\n    expected parts:" );
        for ( size_t i = 0; i < nexpect; ++i ) {
            printf( " [%s]", expect[i] );
        }
        printf( "\n" );
    }
    return ok;
}

static bool pty_case( const char* name, const char* const* steps, size_t nsteps, const char* const* expect, size_t nexpect ) {
    return pty_case_st( name, steps, nsteps, expect, nexpect, 0 );
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

    // the line editor on a terminal (a pty): the one-line editor of EDIT n and the monitor, the same as in the window (scredit.c); the terminal answers
    // ESC [ 6 n; the terminal's own echo of an escape sequence ("^[[A") must not be seen
    {
        static const char* const steps1[] = { "RINT 5", "\033[H", "P", "\r", "\004" };
        static const char* const exp1[] = { "\r\n5\r\nReady.\r\n" };
        failed += !pty_case( "terminal: Home, a character in front, RETURN", steps1, 5U, exp1, 1U );
        static const char* const steps2[] = { "PRINT 1X5", "\033[D", "\177", "\r", "PRINT 3Z", "\033[D", "\033[3~", "\r", "\004" };
        static const char* const exp2[] = { "\r\n15\r\n", "\r\n3\r\n" };
        failed += !pty_case( "terminal: Left, Backspace and Delete edit in the middle of the line", steps2, 9U, exp2, 2U );
        static const char* const steps2b[] = { "PRINT 1", "\033OP", "\033[A", "2", "\r", "\004" };
        static const char* const exp2b[] = { "\r\n12\r\n" };
        failed += !pty_case( "terminal: function keys and Up / Down are ignored", steps2b, 6U, exp2b, 1U );
        static const char* const steps3[] = { "PRINT \"\xc3\xa4\";LEN(\"\xc3\xa4\")", "\r", "\004" };
        static const char* const exp3[] = { "\r\n\xc3\xa4" "1\r\n" };
        failed += !pty_case( "terminal: ä is typed, echoed and one character long", steps3, 3U, exp3, 1U );
        static const char* const steps4[] = { "PRINT 7\001\013\025\t", "8\r", "\004" };
        static const char* const exp4[] = { "\r\n78\r\n" };
        failed += !pty_case( "terminal: control codes and TAB are ignored; Ctrl-D ends on an empty line", steps4, 3U, exp4, 1U );
        static const char* const steps5[] = { "PRINT 5\004", "\r", "\004" };
        static const char* const exp5[] = { "\r\n5\r\n" };
        failed += !pty_case( "terminal: Ctrl-D does nothing in the middle of a line", steps5, 3U, exp5, 1U );
        // a line at the prompt is as long as the screen holds (the editor leaves one row free: 23 x 80 = 1840 on this 24 x 80 terminal)
        static char longline[2600], mediumline[1600];
        snprintf( longline, sizeof(longline), "PRINT 1%*s+2", 2500, "" );
        snprintf( mediumline, sizeof(mediumline), "PRINT 1%*s+2", 1500, "" );
        static const char* const steps6[] = { mediumline, "\r", longline, "\r", "\004" };
        static const char* const exp6[] = { "\r\n3\r\n", "\r\n1\r\n" };
        failed += !pty_case( "terminal: a 1500 character line is entered, a 2500 character line is cut where the screen ends", steps6, 5U, exp6, 2U );
        static const char* const steps7[] = { "10 INPUT A$\r", "RUN\r", "ab\033[D\xc3\xb6", "\r", "PRINT A$;LEN(A$)\r", "\004" };
        static const char* const exp7[] = { "a\xc3\xb6" "b3\r\n" };
        failed += !pty_case( "terminal: INPUT uses the same editor (a character in the middle)", steps7, 6U, exp7, 1U );
        // the full-screen editor and the monitor on the terminal (the same code as in the window; the escape sequences of the painter are what we see)
        static const char* const stepsE1[] = { "10 PRINT 1\r", "EDIT\r", "\033[F", "\r", "20 PRINT 2\r", "\033", "\033", "LIST\r", "\004" };
        static const char* const expE1[] = { "\033[?1049h", "Text Editor", "ESC Close", "\033[?1049l", "\r\n10 PRINT 1\r\n20 PRINT 2\r\n" };
        failed += !pty_case( "terminal: EDIT opens the editor on the alternate screen, a line is added, ESC leaves", stepsE1, 9U, expE1, 5U );
        static const char* const stepsE2[] = { "10 PRINT 1\r", "EDIT\r", "@size 12 50", "\033", "\004" };
        static const char* const expE2[] = { "\033[12;1H", "ESC Close" };
        failed += !pty_case( "terminal: EDIT follows a resize of the terminal (the help bar moves to row 12)", stepsE2, 5U, expE2, 2U );
        static const char* const stepsE3[] = { "10 PRINT 1\r", "EDIT\r", "@size 4 20", "\033", "\004" };
        static const char* const expE3[] = { "Too small" };
        failed += !pty_case( "terminal: EDIT on a terminal that is too small shows a message, ESC leaves", stepsE3, 5U, expE3, 1U );
        static const char* const stepsM1[] = { "10 PRINT 1\r", "MONITOR\r", "u\r", "q\r", "PRINT 5\r", "\004" };
        static const char* const expM1[] = { "\033[?1049h", "Machine Monitor", "LINE     10", "Qquit", "\033[?1049l", "\r\n5\r\n" };
        failed += !pty_case( "terminal: MONITOR shows a disassembly, Q leaves, BASIC goes on", stepsM1, 6U, expM1, 6U );
        static const char* const stepsM2[] = { "10 PRINT 1\r", "MONITOR\r", "@size 10 40", "g\r", "q\r", "\004" };
        static const char* const expM2[] = { "\033[10;1H", "Running from $0", "the program has ended" };
        failed += !pty_case( "terminal: MONITOR follows a resize and runs the program on its screen", stepsM2, 6U, expM2, 3U );

        static const char* const stepsB1[] = { "10 GOTO 10\r", "RUN\r", "\033", "PRINT 5\r", "\004" };
        static const char* const expB1[] = { "? Break in line 10", "\r\n5\r\n" };
        failed += !pty_case( "terminal: a lone ESC breaks a running program (as in the window)", stepsB1, 5U, expB1, 2U );
        static const char* const stepsB2[] = { "10 GOTO 10\r", "RUN\r", "ab", "\003", "10 GET A$:PRINT A$\r", "RUN\r", "x", "\004" };
        static const char* const expB2[] = { "? Break in line 10", "10 GET A$:PRINT A$", "\r\nx\r\n" };
        failed += !pty_case( "terminal: Ctrl-C breaks a running program; keys typed meanwhile are dropped at its end", stepsB2, 8U, expB2, 3U );
        static const char* const stepsB3[] = { "10 GOTO 10\r", "MONITOR\r", "g\r", "\033", "q\r", "\004" };
        static const char* const expB3[] = { "Running from $0", "Break" };
        failed += !pty_case( "terminal: a lone ESC breaks the program the monitor runs", stepsB3, 6U, expB3, 2U );
        static const char* const stepsN1[] = { "10 PRINT 1\r", "EDIT 10\r", "\033[F", "0", "\r", "LIST\r", "\004" };
        static const char* const expN1[] = { "10 PRINT 10" };
        failed += !pty_case( "terminal: EDIT n edits the line in the editor, RETURN enters it", stepsN1, 7U, expN1, 1U );
        static const char* const stepsN2[] = { "10 PRINT 1\r", "EDIT 10\r", "\033", "LIST\r", "\004" };
        static const char* const expN2[] = { "10 PRINT 1" };
        failed += !pty_case( "terminal: EDIT n: ESC cancels", stepsN2, 5U, expN2, 1U );
        static const char* const stepsB4[] = { "10 A$=INKEY$:GOTO 10\r", "RUN\r", "\033", "PRINT 5\r", "\004" };
        static const char* const expB4[] = { "? Break in line 10", "\r\n5\r\n" };
        failed += !pty_case( "terminal: a lone ESC breaks an INKEY$ loop", stepsB4, 5U, expB4, 2U );
        static const char* const stepsB5[] = { "10 GET A$\r", "RUN\r", "\033", "PRINT 5\r", "\004" };
        static const char* const expB5[] = { "? Break in line 10", "\r\n5\r\n" };
        failed += !pty_case( "terminal: a lone ESC breaks a program waiting in GET", stepsB5, 5U, expB5, 2U );
        static const char* const stepsB6[] = { "10 GET A$:PRINT ASC(A$)\r", "RUN\r", "\033[A", "\004" };
        static const char* const expB6[] = { "\r\n27\r\n" };
        failed += !pty_case( "terminal: a cursor key (ESC sequence) is a key for GET, not a break", stepsB6, 4U, expB6, 1U );
        static const char* const stepsK1[] = { "10 BREAK OFF\r", "20 A$=INKEY$:IF A$=\"\" THEN GOTO 20\r", "30 PRINT ASC(A$)\r", "RUN\r", "\033", "\004" };
        static const char* const expK1[] = { "\r\n27\r\n" };
        failed += !pty_case( "terminal: BREAK OFF makes ESC an ordinary key (INKEY$ gets 27)", stepsK1, 6U, expK1, 1U );
        static const char* const stepsK2[] = { "10 BREAK OFF:GOTO 10\r", "RUN\r", "\033", "\003", "PRINT 8\r", "\004" };
        static const char* const expK2[] = { "? Break in line 10", "\r\n8\r\n" };
        failed += !pty_case( "terminal: with BREAK OFF ESC does not stop the program, Ctrl-C does", stepsK2, 6U, expK2, 2U );
        static const char* const stepsK3[] = { "10 BREAK OFF:INPUT A$:PRINT LEN(A$)\r", "RUN\r", "ab\033", "\r", "\004" };
        static const char* const expK3[] = { "\r\n0\r\n" };
        failed += !pty_case( "terminal: with BREAK OFF ESC at INPUT is an empty line", stepsK3, 5U, expK3, 1U );
        static const char* const stepsK4[] = { "10 BREAK OFF:GOTO 10\r", "RUN\r", "\003", "20 GOTO 20\r", "10 PRINT 1\r", "RUN\r", "\033", "PRINT 9\r", "\004" };
        static const char* const expK4[] = { "? Break in line 20", "\r\n9\r\n" };
        failed += !pty_case( "terminal: the next run has BREAK ON again", stepsK4, 9U, expK4, 2U );
        static const char* const steps8[] = { "PRINT 1", "\033", "PRINT 5\r", "\004" };
        static const char* const exp8[] = { "\r\n5\r\n" };
        failed += !pty_case( "terminal: a lone ESC at the prompt cancels the line", steps8, 4U, exp8, 1U );
        static const char* const steps9[] = { "10 INPUT A$\r", "RUN\r", "ab\033", "PRINT 5\r", "\004" };
        static const char* const exp9[] = { "? Break in line 10", "\r\n5\r\n" };
        failed += !pty_case( "terminal: a lone ESC at INPUT is a break (Break in line 10)", steps9, 5U, exp9, 2U );
        static const char* const steps10[] = { "PRINT 1", "\003", "PRINT 6\r", "\004" };
        static const char* const exp10[] = { "\r\n6\r\n" };
        failed += !pty_case( "terminal: Ctrl-C at the prompt cancels the line", steps10, 4U, exp10, 1U );
    }
    // AUTO on a terminal in the classic line mode: the prompt shows the line numbers, an empty line ends it
    {
        static const char* const stepsA1[] = { "AUTO 100,5\r", "PRINT 1\r", "PRINT 2\r", "\r", "LIST\r", "\004" };
        static const char* const expA1[] = { "AUTO 100,5", "100 ", "PRINT 1", "105 ", "PRINT 2", "110 ", "Ready.", "\r\n100 PRINT 1\r\n105 PRINT 2\r\n" };
        failed += !pty_case( "terminal: AUTO prompts with line numbers, an empty line ends it", stepsA1, 6U, expA1, 8U );
        static const char* const stepsA2[] = { "AUTO 10\r", "A=1\r", "\033", "LIST\r", "\004" };
        static const char* const expA2[] = { "AUTO 10", "10 ", "A=1", "20 ", "Ready.", "\r\n10 A=1\r\n" };
        failed += !pty_case( "terminal: ESC ends AUTO, the lines entered stay", stepsA2, 5U, expA2, 6U );
    }
    // screen mode (the default on a terminal): the session runs on text screen 0 on the alternate screen; what is left of it is written to the normal
    // screen at the end (the "exit dump": the lines come with \r\n, the painting of the alternate screen moves the cursor instead)
    {
        pty_screen = "1";
        static const char* const sA[] = { "PRINT 1+1\r", "BYE 3\r" };
        static const char* const eA[] = { "\033[?1049h", "OVR  F12=INS", "\033[?1049l", "\r\nReady.\r\nPRINT 1+1\r\n2\r\nReady.\r\nBYE 3\r\n" };
        failed += !pty_case_st( "screen mode: the session is on the alternate screen, the status line shows, the screen is dumped at the end, BYE's status", sA, 2U, eA, 4U, 3 );
        static const char* const sB[] = { "10 PRINT 1\r", "\033[A", "\033[C\033[C\033[C\033[C\033[C\033[C\033[C\033[C\033[C", "7\r", "RUN\r", "BYE\r" };
        static const char* const eB[] = { "\r\n7\r\nReady.\r\nBYE\r\n" };
        failed += !pty_case( "screen mode: a line is picked up from the screen, edited in place and entered", sB, 6U, eB, 1U );
        static const char* const sC[] = { "PRINT 1", "\003", "+1\r", "\004", "BYE\r" };
        static const char* const eC[] = { "\r\n2\r\n" };
        failed += !pty_case( "screen mode: Ctrl-C and Ctrl-D at the prompt do nothing", sC, 5U, eC, 1U );
        static const char* const sAU[] = { "AUTO 100,5\r", "PRINT 1\r", "PRINT 2\r", "\r", "LIST\r", "BYE\r" };
        static const char* const eAU[] = { "\r\nReady.\r\nAUTO 100,5\r\nReady.\r\n100 PRINT 1\r\n105 PRINT 2\r\n110\r\nReady.\r\nLIST\r\n100 PRINT 1\r\n105 PRINT 2\r\n" };
        failed += !pty_case( "screen mode: AUTO prompts with line numbers on the screen, an empty line ends it", sAU, 6U, eAU, 1U );
        static const char* const sD[] = { "10 GOTO 10\r", "RUN\r", "\033", "BYE\r" };
        static const char* const eD[] = { "? Break in line 10" };
        failed += !pty_case( "screen mode: a lone ESC breaks a running program", sD, 4U, eD, 1U );
        static const char* const sE[] = { "10 INPUT \"x\";A$:PRINT \"[\";A$;\"]\"\r", "RUN\r", "ab\033[D\xc3\xb6\r", "BYE\r" };
        static const char* const eE[] = { "x? a\xc3\xb6" "b\r\n[a\xc3\xb6" "b]\r\n" };
        failed += !pty_case( "screen mode: INPUT uses the one-line editor on the screen (a character in the middle)", sE, 4U, eE, 1U );
        static const char* const sF[] = { "10 PRINT 1\r", "EDIT\r", "\033", "PRINT 8\r", "BYE\r" };
        static const char* const eF[] = { "\033[?1049h", " Text Editor", "\r\n8\r\nReady.\r\nBYE\r\n" };
        failed += !pty_case( "screen mode: EDIT opens the editor on its own screen and comes back to the BASIC screen", sF, 5U, eF, 3U );
        static const char* const sG[] = { "@size 12 50", "PRINT 3\r", "BYE\r" };
        static const char* const eG[] = { "\r\n3\r\nReady.\r\nBYE\r\n" };
        failed += !pty_case( "screen mode: a resize of the terminal is followed", sG, 3U, eG, 1U );
        static const char* const sH[] = { "10 CLS:LOCATE 5,2:COLOR 140,2:PRINT \"hi\":PRINT TXT(\"WIDTH\")\r", "RUN\r", "BYE\r" };
        static const char* const eH[] = { "\r\n\r\n     hi\r\n80\r\nReady.\r\nBYE\r\n" };
        failed += !pty_case( "screen mode: CLS, LOCATE, COLOR and TXT work on the text screen", sH, 3U, eH, 1U );
        // KEY("name") is an error on a terminal, too (the held-key approximation is disabled, see engine1999c.c: -DCON_KEYSTATE)
        static const char* const sK1[] = { "10 PRINT KEY(\"A\")\r", "RUN\r", "BYE\r" };
        static const char* const eK1[] = { "? KEY is not available in this host in line 10" };
        failed += !pty_case( "screen mode: KEY() is a run-time error in the console host", sK1, 3U, eK1, 1U );
        // indentation: the blanks behind the line number are kept by the prompt, EDIT n and the full-screen editor
        static const char* const sN1[] = { "10 FOR I=1 TO 2\r", "20     PRINT I\r", "30 NEXT\r", "RUN\r", "BYE\r" };
        static const char* const eN1[] = { "20     PRINT I", "\r\n1\r\n2\r\n" };
        failed += !pty_case( "screen mode: an indented line typed at the prompt keeps its indent (and runs)", sN1, 5U, eN1, 2U );
        static const char* const sN2[] = { "20     PRINT 5\r", "EDIT 20\r", "\r", "BYE\r" };
        static const char* const eN2[] = { "20     PRINT 5" };
        failed += !pty_case( "screen mode: EDIT n keeps the indent", sN2, 4U, eN2, 1U );
        static const char* const sN3[] = { "10 PRINT 1\r", "20     PRINT 5\r", "EDIT\r", "\033[B", "\033[F", "+0", "\033", "LIST\r", "BYE\r" };
        static const char* const eN3[] = { "20     PRINT 5+0" };
        failed += !pty_case( "screen mode: the full-screen editor keeps the indent of an edited line", sN3, 9U, eN3, 1U );
        // the mouse: the terminal reports it in the stream of the keys (xterm SGR mode); the cell (col 10, row 5) is the position 76,54 (8 x 12 pixels per cell)
        static const char* const sM1[] = { "10 IF MOUSE(\"B\")=0 THEN WAIT 1:GOTO 10\r", "20 PRINT MOUSE(\"X\");\",\";MOUSE(\"Y\");\",\";MOUSE(\"B\");\",\";MOUSE(\"VISIBLE\")\r", "RUN\r", "\033[<0;10;5M", "BYE\r" };
        static const char* const eM1[] = { "\033[?1003h\033[?1006h", "76,54,1,-1", "\033[5;10H", "\033[?1003l\033[?1006l\033]22;\033\\", "\033[?1049l" };
        failed += !pty_case( "screen mode: the mouse is reported (position, button, the pointer shows) and switched off at the end", sM1, 5U, eM1, 5U );
        // a report between keys is no key; a release, motion without a button and the wheel
        static const char* const sM2[] = { "10 GET A$,B$:PRINT \"[\";A$;B$;\"]\"\r", "20 PRINT MOUSE(\"X\");\",\";MOUSE(\"B\");\",\";MOUSE(\"PRESSED\");\",\";MOUSE(\"RELEASED\");\",\";MOUSE(\"WHEEL\")\r", "RUN\r",
            "a\033[<0;3;3M\033[<0;3;3m\033[<64;3;3M\033[<64;3;3M\033[<65;3;3M\033[<35;4;3Mb", "BYE\r" };
        static const char* const eM2[] = { "[ab]", "28,0,1,1,1" };
        failed += !pty_case( "screen mode: mouse reports inside typed keys are taken out; release, motion and the wheel", sM2, 5U, eM2, 2U );
        // the mouse in the full-screen editor (a 24 x 80 terminal: panes in rows 2..23, the scroll bar in column 80; a report is col;row from 1): the wheel and
        // the scroll bar scroll the view, the cursor is pulled to the first line shown; a marker typed afterwards shows where it is
        static const char* const mkprog = "OPEN \"mem:p\" FOR OUTPUT AS #1:FOR I=1 TO 60:PRINT #1,I*10;\" REM n\":NEXT:CLOSE #1:LOAD \"mem:p\"\r";
        static const char* const sW1[] = { mkprog, "EDIT\r", "\033[<65;5;5M\033[<65;5;5M", "Z", "\033", "LIST 60-80\r", "BYE\r" };
        static const char* const eW1[] = { "\033[?1049h", "70 ZREM" };
        failed += !pty_case( "screen mode: the wheel scrolls the full-screen editor (two notches: 6 lines)", sW1, 7U, eW1, 2U );
        static const char* const sW2[] = { mkprog, "EDIT\r", "\033[<0;80;23M\033[<0;80;23m", "Z", "\033", "LIST 10-30\r", "BYE\r" };
        static const char* const eW2[] = { "\033[?1049h", "20 ZREM" };
        failed += !pty_case( "screen mode: a click on the down arrow of the scroll bar scrolls one line", sW2, 7U, eW2, 2U );
        static const char* const sW3[] = { mkprog, "EDIT\r", "\033[<0;80;12M\033[<0;80;12m", "Z", "\033", "LIST 180-200\r", "BYE\r" };
        static const char* const eW3[] = { "\033[?1049h", "190 ZREM" };
        failed += !pty_case( "screen mode: a click on the track of the scroll bar puts the view there", sW3, 7U, eW3, 2U );
        static const char* const sW4[] = { mkprog, "EDIT\r", "\033[<0;80;4M\033[<32;80;12M\033[<32;80;23M\033[<0;80;23m", "Z", "\033", "LIST\r", "BYE\r" };
        static const char* const eW4[] = { "\033[?1049h", "ZREM" };
        failed += !pty_case( "screen mode: the thumb of the scroll bar can be dragged (a move with the button held)", sW4, 7U, eW4, 2U );
        // the shape is the terminal's own pointer (OSC 22), MOUSE HIDE hides the drawn one
        static const char* const sM3[] = { "10 MOUSE SHAPE CROSS:MOUSE POS 100,100:PRINT MOUSE(\"SHAPE\");MOUSE(\"VISIBLE\");MOUSE(\"X\");MOUSE(\"Y\")\r", "20 MOUSE HIDE:PRINT MOUSE(\"VISIBLE\")\r", "RUN\r", "BYE\r" };
        static const char* const eM3[] = { "\033]22;crosshair\033\\", "6-1100100", "\r\n0\r\n" };
        failed += !pty_case( "screen mode: MOUSE SHAPE sets the terminal's pointer shape (OSC 22), POS and HIDE", sM3, 4U, eM3, 3U );
        // no mouse reporting when it is switched off: the statements are an error
        setenv( "E1999_MOUSE", "0", 1 );
        static const char* const sM4[] = { "10 MOUSE SHOW\r", "RUN\r", "BYE\r" };
        static const char* const eM4[] = { "? Not available in the console host in line 10" };
        failed += !pty_case( "screen mode: E1999_MOUSE=0 switches the mouse off (an error, no reporting)", sM4, 3U, eM4, 1U );
        unsetenv( "E1999_MOUSE" );
        // run mode: an error waits for a key (the hint is an overlay, not in the dump); a normal end leaves at once
        put( "prog.bas", "10 PRINT \"before\"\n20 PRINT 1/0\n" );
        pty_args[0] = "prog.bas";
        static const char* const sI[] = { "@size 24 80", "x" };
        static const char* const eI[] = { "Press a key to leave", "before\r\n? Division by zero in line 20\r\n" };
        failed += !pty_case_st( "screen mode: a program that fails waits for a key, then the screen is dumped; status 1", sI, 2U, eI, 2U, 1 );
        put( "prog.bas", "10 PRINT \"fine\"\n" );
        static const char* const sJ[] = { "@size 24 80" };
        static const char* const eJ[] = { "\033[?1049l" "fine\r\n" };
        failed += !pty_case_st( "screen mode: a program that ends normally leaves at once (dumped)", sJ, 1U, eJ, 1U, 0 );
        pty_args[0] = "--no-dump"; pty_args[1] = "prog.bas";
        failed += !pty_case_st( "screen mode: --no-dump ends the session and leaves the alternate screen", sJ, 1U, (const char* const[]){ "\033[?1049l" }, 1U, 0 );
        pty_args[0] = "--line"; pty_args[1] = "prog.bas";
        failed += !pty_case_st( "--line: no alternate screen even on a terminal", sJ, 1U, (const char* const[]){ "fine\r\n" }, 1U, 0 );
        pty_args[0] = pty_args[1] = 0;
        pty_screen = "0";
    }
    {
        char *out, *err; size_t outlen;
        // the prompt takes lines of up to 4095 characters from a redirected input (the screen's size does not matter there)
        {
            static char big[6200];
            snprintf( big, sizeof(big), "PRINT 1%*s+2\n", 3000, "" );
            int st2 = run( "-", big, &out, &outlen, &err );
            bool ok2 = st2 == 0 && strcmp( out, "3\n" ) == 0;
            printf( "%s  a 3000 character line from a pipe is entered whole\n", ok2 ? "PASS" : "FAIL" );
            failed += !ok2;
            free( out ); free( err );
            snprintf( big, sizeof(big), "PRINT 1%*s+2\n", 5000, "" );
            st2 = run( "-", big, &out, &outlen, &err );
            ok2 = st2 == 0 && strcmp( out, "1\n" ) == 0;
            printf( "%s  a 5000 character line from a pipe is cut at 4095\n", ok2 ? "PASS" : "FAIL" );
            failed += !ok2;
            free( out ); free( err );
        }
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
