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

    NOTE: Programs created with a built-in programming language (if any),
          do not fall under this license.

    CONTACT INFO:
        E-Mail: ekkehard@ekkehardmorgenstern.de
        Mail: Ekkehard Morgenstern, Mozartstr. 1, D-76744 Woerth am Rhein,
              Germany, Europe
*/

#include "basrepl.h"
#include "basdebug.h"

/*
Automated test of the REPL (basrepl.[ch]) without any host: lines are fed to repl_enter() like a user would type
them, the output and the error messages are captured and compared with the expected text. The keyboard callbacks
read from the "input" text of a case (INPUT takes lines, GET takes characters). The exit status is the number of
failed cases.

Usage: repltest1 [-v]       -v: show all debug output (see basdebug.h)
*/

// the program files of the LOAD cases (in the current directory; removed at the end)
#define TESTFILE    "repltest1.tmp"

typedef struct _testcase_t {
    const char* name;
    const char* source;     // lines separated by '\n'
    const char* expected;
    const char* input;      // keyboard input (optional)
    unsigned    breakafter; // the interrupt callback says "break" from this poll on (0 = never)
} testcase_t;

static const testcase_t cases[] = {
    { "direct mode",            "PRINT 1+2",                                      "3\n" },
    { "variables persist",      "A=5\nPRINT A*2",                                 "10\n" },
    { "empty line",             "\n\nPRINT 7",                                    "7\n" },
    { "parse error",            "PRINT )",                                        "? End of line expected\n" },
    { "error doesn't stop the session", "PRINT 1/0\nPRINT 2",                     "? Division by zero\n2\n" },
    { "function persists",      "DEF FN A(X)=X*3\nPRINT FN A(4)",                 "12\n" },
    { "DATA pointer persists",  "DATA 1,2,3\nREAD A\nREAD B\nPRINT A;B",          "12\n" },
    { "store and RUN",          "10 PRINT \"HI\"\n20 PRINT 2\nRUN",               "HI\n2\n" },
    { "line order",             "20 PRINT 2\n10 PRINT 1\nRUN",                    "1\n2\n" },
    { "replace a line",         "10 PRINT 1\n10 PRINT 5\nRUN",                    "5\n" },
    { "delete a line",          "10 PRINT 1\n20 PRINT 2\n10\nRUN",                "2\n" },
    { "RUN clears variables",   "A=5\n10 PRINT A\nRUN",                           "0\n" },
    { "variables stay after RUN","10 A=9\nRUN\nPRINT A",                          "9\n" },
    { "RUN twice",             "10 A=A+1\n20 PRINT A\nRUN\nRUN",                  "1\n1\n" },
    { "RUN without program",    "RUN",                                            "? Empty program\n" },
    { "DATA added later",       "DATA 1\nREAD A\nDATA 2\nREAD B\nPRINT A;B",   "12\n" },
    { "RUN restarts DATA",      "10 READ A\n20 PRINT A\n30 DATA 4\nRUN\nRUN",     "4\n4\n" },
    { "run time error line",    "10 PRINT 1\n20 PRINT 1/0\nRUN",                  "1\n? Division by zero in line 20\n" },
    { "undefined line",         "10 GOTO 99\nRUN",                                "? Undefined line number 99 in line 10\n" },
    { "LIST",                   "20 PRINT 2\n10 A=1\nLIST",                       "10 A=1\n20 PRINT 2\n" },
    { "LIST range",             "10 A=1\n20 A=2\n30 A=3\nLIST 20-30",             "20 A=2\n30 A=3\n" },
    { "LIST one line",          "10 A=1\n20 A=2\nLIST 20",                        "20 A=2\n" },
    { "LIST up to",             "10 A=1\n20 A=2\nLIST -10",                       "10 A=1\n" },
    { "LIST undefined label",   "LIST X",                                         "? Undefined label X\n" },
    { "LIST syntax error",      "LIST 10 20",                                     "? End of line expected\n" },
    { "lower case",             "print 1+1\nlist",                                "2\n" },
    { "string keeps case",      "print \"Hello\"",                                "Hello\n" },
    // RUN, LIST and LOAD as statements
    { "RUN to a line",          "10 PRINT 1\n20 RUN 40\n30 PRINT 3\n40 PRINT 4\nRUN",     "1\n4\n" },
    { "RUN to a label",         "10 PRINT 1\n20 RUN L\n30 PRINT 3\n40 LABEL L\n50 PRINT 5\nRUN", "1\n5\n" },
    { "RUN in a program clears variables", "10 A=7\n20 RUN 40\n30 END\n40 PRINT A\nRUN",   "0\n" },
    { "RUN line in direct mode","10 PRINT 1\n20 PRINT 2\nRUN 20",                      "2\n" },
    { "RUN label in direct mode","10 PRINT 1\n20 LABEL X\n30 PRINT 3\nRUN X",         "3\n" },
    { "RUN undefined line",     "10 PRINT 1\nRUN 99",                                   "? Undefined line number 99\n" },
    { "RUN undefined label",    "10 PRINT 1\n20 RUN NOWHERE\nRUN",                      "1\n? Undefined label NOWHERE\n" },
    { "RUN a counted loop",     "10 N=N+1\n20 PRINT N\n30 IF N<3 THEN GOTO 10\n40 PRINT \"E\"\nRUN 10", "1\n2\n3\nE\n" },
    { "program restarts itself, Break stops it", "10 RUN\nRUN",                              "? Break in line 10\n", 0, 3 },
    { "RUN then direct mode",   "10 A=4\n20 RUN 30\n30 A=A+1\nRUN\nPRINT A",            "1\n" },
    { "LIST to a label",        "10 A=1\n20 LABEL L\n30 A=3\n40 A=4\nLIST -L",         "10 A=1\n20 LABEL L\n" },
    { "LIST from a label",      "10 A=1\n20 LABEL L\n30 A=3\n40 A=4\nLIST L-",         "20 LABEL L\n30 A=3\n40 A=4\n" },
    { "LIST a label",           "10 A=1\n20 LABEL L\n30 A=3\nLIST L",                  "20 LABEL L\n" },
    { "LIST label range",       "10 LABEL A\n20 A=1\n30 LABEL B\n40 A=4\nLIST A-B",    "10 LABEL A\n20 A=1\n30 LABEL B\n" },
    { "LIST label to line",     "10 LABEL A\n20 A=1\n30 A=3\nLIST A-20",               "10 LABEL A\n20 A=1\n" },
    { "LIST skips LABEL in a string", "10 PRINT \"LABEL X\"\n20 LABEL X\nLIST X",      "20 LABEL X\n" },
    { "LIST from a program",    "10 LIST 20\n20 PRINT 2\nRUN",                         "20 PRINT 2\n2\n" },
    { "LIST all from a program","10 LIST\n20 PRINT 2\nRUN",                            "10 LIST\n20 PRINT 2\n2\n" },
    { "LIST undefined label in a program", "10 LIST X\nRUN",                           "? Undefined label X in line 10\n" },
    { "LOAD a binary file",     "10 PRINT \"B\"\nSAVE \"" TESTFILE "\"\nNEW\nLOAD \"" TESTFILE "\"\nLIST", "B\n10 PRINT \"B\"\n" },
    { "LOAD a text file",       "10 PRINT \"B\"\n20 PRINT 2\nSAVE \"" TESTFILE "\",A\nNEW\nLOAD \"" TESTFILE "\"", "B\n2\n" },
    { "LOAD in a program replaces it and runs it", "10 PRINT \"B\"\nSAVE \"" TESTFILE "\"\nNEW\n10 PRINT \"A\"\n20 LOAD \"" TESTFILE "\"\n30 PRINT \"C\"\nRUN\nLIST", "A\nB\n10 PRINT \"B\"\n" },
    { "LOAD clears variables", "10 PRINT A\nSAVE \"" TESTFILE "\"\nNEW\nA=5\nLOAD \"" TESTFILE "\"", "0\n" },
    { "LOAD missing file",      "10 PRINT 1\nLOAD \"no/such/file\"\nLIST",             "? File not found\n10 PRINT 1\n" },
    { "LOAD keeps the program on a bad file", "10 PRINT 1\nA$=`printf 'garbage' >" TESTFILE "`\nLOAD \"" TESTFILE "\"\nLIST", "? Line number missing in program file\n10 PRINT 1\n" },
    { "LOAD a truncated binary file", "10 PRINT 1\nSAVE \"" TESTFILE "\"\nA$=`truncate -s -2 " TESTFILE "`\nLOAD \"" TESTFILE "\"\nLIST", "? Bad program file\n10 PRINT 1\n" },
    { "LOAD a future file version", "10 PRINT 1\nA$=`printf '\\211E99\\7' >" TESTFILE "`\nLOAD \"" TESTFILE "\"\nLIST", "? Unsupported program file version\n10 PRINT 1\n" },
    { "LOAD at the end of a chain", "10 PRINT \"B\"\nSAVE \"" TESTFILE "\"\nNEW\n10 PRINT \"A\"\n20 LOAD \"" TESTFILE "\"\nRUN", "A\nB\n" },
    { "NEW",                    "10 PRINT 1\nA=3\nNEW\nRUN\nPRINT A",             "? Empty program\n0\n" },
    { "program then direct",    "10 DEF FN A(X)=X+1\nRUN\nPRINT FN A(1)",         "2\n" },
    { "INPUT in direct mode",   "INPUT \"N\";A:PRINT A*2",                        "N? 21\n42\n", "21\n" },
    { "INPUT in a program",     "10 INPUT A$\n20 PRINT \"[\"+A$+\"]\"\nRUN",      "? ABC\n[ABC]\n", "ABC\n" },
    { "GET in a program",       "10 GET A$,B$\n20 PRINT B$;A$\nRUN",              "YX\n", "XY" },
    { "INKEY$ in direct mode",  "PRINT LEN(INKEY$)",                              "0\n" },
    { "end of input",           "INPUT A",                                        "? ? End of input\n" },
    { "Break stops a loop",     "10 GOTO 10\nRUN",                                "? Break in line 10\n", 0, 3 },
    { "Break at INPUT",         "10 INPUT A\nRUN",                                "? ? Break in line 10\n", 0, 1 },
    { "session continues after Break", "10 GOTO 10\nRUN\nPRINT 5",                "? Break in line 10\n5\n", 0, 3 },
};

// -- host ----------------------------------------------------------------------

static char*  outbuf = 0;
static size_t outsize = 0, outalloc = 0;

static void out_append( const char* text ) {
    size_t len = strlen( text );
    if ( outsize + len + 1U > outalloc ) {
        outalloc = ( outsize + len + 1U ) * 2U;
        outbuf = (char*) realloc( outbuf, outalloc );
        if ( outbuf == 0 ) {
            fprintf( stderr, "out of memory\n" );
            exit( EXIT_FAILURE );
        }
    }
    memcpy( outbuf + outsize, text, len + 1U );
    outsize += len;
}

static void out_reset( void ) {
    outsize = 0;
    out_append( "" );
}

static const char* keys = 0;
static unsigned    polls = 0, breakafter = 0;

static void host_print( struct _vm_t* vm, void* usrdata, int channel, const char* text ) {
    out_append( text );
}

static void host_report( struct _repl_t* repl, const char* text ) {
    out_append( text );
}

static int host_inkey( struct _vm_t* vm, void* usrdata ) {
    return keys && *keys ? (uint8_t) *keys++ : 0;
}

static int host_getkey( struct _vm_t* vm, void* usrdata, int channel ) {
    return keys && *keys ? (uint8_t) *keys++ : -1;
}

// the typed line is echoed, like the line editor of a real screen does
static char* host_input( struct _vm_t* vm, void* usrdata, int channel ) {
    if ( keys == 0 || *keys == '\0' ) {
        return 0;
    }
    const char* nl = strchr( keys, '\n' );
    size_t len = nl ? (size_t)( nl - keys ) : strlen( keys );
    char* line = (char*) malloc( len + 1U );
    memcpy( line, keys, len );
    line[ len ] = '\0';
    keys += nl ? len + 1U : len;
    out_append( line );
    out_append( "\n" );
    return line;
}

static bool host_save( struct _vm_t* vm, void* usrdata, const char* filename, bool ascii ) {
    return repl_save_program( (repl_t*) usrdata, filename, ascii );
}

static bool host_interrupt( struct _vm_t* vm, void* usrdata ) {
    return breakafter && ++polls >= breakafter;
}

// -- running -----------------------------------------------------------------

static bool run_case( const testcase_t* tc ) {
    out_reset();
    keys = tc->input; polls = 0; breakafter = tc->breakafter;
    repl_t repl;
    if ( !init_repl( &repl ) ) {
        return false;
    }
    repl.report    = host_report;
    repl.print     = host_print;
    repl.inkey     = host_inkey;
    repl.input     = host_input;
    repl.getkey    = host_getkey;
    repl.interrupt = host_interrupt;
    repl.save      = host_save;
    char* src = strdup( tc->source );
    for ( char* line = strtok( src, "\n" ); line; line = strtok( 0, "\n" ) ) {
        repl_enter( &repl, line );
    }
    free( src );
    cleanup_repl( &repl );
    return strcmp( outbuf, tc->expected ) == 0;
}

static void show( const char* label, const char* text ) {
    printf( "    %s: \"", label );
    for ( const char* p = text; *p; ++p ) {
        if ( *p == '\n' ) {
            printf( "\\n" );
        } else {
            putchar( *p );
        }
    }
    printf( "\"\n" );
}

int main( int argc, char** argv ) {
    if ( argc > 1 && strcmp( argv[1], "-v" ) == 0 ) {
        basdbg_flags = BASDBG_ALL;
    }
    size_t numcases = sizeof(cases) / sizeof(cases[0]), failed = 0;
    remove( TESTFILE );
    for ( size_t i=0; i < numcases; ++i ) {
        if ( run_case( &cases[i] ) ) {
            printf( "PASS  %s\n", cases[i].name );
        } else {
            printf( "FAIL  %s\n", cases[i].name );
            show( "expected", cases[i].expected );
            show( "got     ", outbuf );
            ++failed;
        }
    }
    remove( TESTFILE );
    printf( "%u of %u passed\n", (unsigned)( numcases - failed ), (unsigned) numcases );
    free( outbuf );
    return (int) failed;
}
