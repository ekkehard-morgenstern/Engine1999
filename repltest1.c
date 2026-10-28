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
    { "LOAD a binary file",     "10 PRINT \"B\"\nSAVE \"" TESTFILE "\"\nNEW\nLOAD \"" TESTFILE "\"\nLIST", "10 PRINT \"B\"\n" },
    { "LOAD in direct mode does not run", "10 PRINT \"B\"\nSAVE \"" TESTFILE "\"\nNEW\nLOAD \"" TESTFILE "\"\nPRINT 7\nRUN", "7\nB\n" },
    { "LOAD a text file",       "10 PRINT \"B\"\n20 PRINT 2\nSAVE \"" TESTFILE "\",A\nNEW\nLOAD \"" TESTFILE "\"\nRUN", "B\n2\n" },
    { "LOAD in a program replaces it and runs it", "10 PRINT \"B\"\nSAVE \"" TESTFILE "\"\nNEW\n10 PRINT \"A\"\n20 LOAD \"" TESTFILE "\"\n30 PRINT \"C\"\nRUN\nLIST", "A\nB\n10 PRINT \"B\"\n" },
    { "LOAD clears variables", "10 PRINT A\nSAVE \"" TESTFILE "\"\nNEW\nA=5\nLOAD \"" TESTFILE "\"\nPRINT A", "0\n" },
    { "LOAD missing file",      "10 PRINT 1\nLOAD \"no/such/file\"\nLIST",             "? File not found\n10 PRINT 1\n" },
    { "LOAD keeps the program on a bad file", "10 PRINT 1\nA$=`printf 'garbage' >" TESTFILE "`\nLOAD \"" TESTFILE "\"\nLIST", "? Line number missing in program file\n10 PRINT 1\n" },
    { "LOAD a truncated binary file", "10 PRINT 1\nSAVE \"" TESTFILE "\"\nA$=`truncate -s -2 " TESTFILE "`\nLOAD \"" TESTFILE "\"\nLIST", "? Bad program file\n10 PRINT 1\n" },
    { "LOAD a future file version", "10 PRINT 1\nA$=`printf '\\211E99\\7' >" TESTFILE "`\nLOAD \"" TESTFILE "\"\nLIST", "? Unsupported program file version\n10 PRINT 1\n" },
    { "LOAD at the end of a chain", "10 PRINT \"B\"\nSAVE \"" TESTFILE "\"\nNEW\n10 PRINT \"A\"\n20 LOAD \"" TESTFILE "\"\nRUN", "A\nB\n" },
    // DELETE and RENUM
    { "DELETE a range",         "10 A=1\n20 A=2\n30 A=3\n40 A=4\nDELETE 20-30\nLIST",  "10 A=1\n40 A=4\n" },
    { "DELETE one line",        "10 A=1\n20 A=2\n30 A=3\nDELETE 20\nLIST",             "10 A=1\n30 A=3\n" },
    { "DELETE up to",           "10 A=1\n20 A=2\n30 A=3\nDELETE -20\nLIST",            "30 A=3\n" },
    { "DELETE from",            "10 A=1\n20 A=2\n30 A=3\nDELETE 20-\nLIST",            "10 A=1\n" },
    { "DELETE a label's line",  "10 A=1\n20 LABEL L\n30 A=3\nDELETE L\nLIST",          "10 A=1\n30 A=3\n" },
    { "DELETE nothing",         "10 A=1\nDELETE 50-60\nLIST",                          "10 A=1\n" },
    { "DELETE without target",  "10 A=1\nDELETE\nLIST",                                "? Line number or label expected\n10 A=1\n" },
    { "DELETE undefined label", "10 A=1\nDELETE X\nLIST",                              "? Undefined label X\n10 A=1\n" },
    { "DELETE in a program ends the run", "10 PRINT 1\n20 DELETE 10\n30 PRINT 3\nRUN\nLIST", "1\n20 DELETE 10\n30 PRINT 3\n" },
    { "RENUM default",          "5 A=1\n7 A=2\n9 A=3\nRENUM\nLIST",                    "10 A=1\n20 A=2\n30 A=3\n" },
    { "RENUM step",             "5 A=1\n7 A=2\n9 A=3\nRENUM ,5\nLIST",                 "10 A=1\n15 A=2\n20 A=3\n" },
    { "RENUM adjusts GOTO",     "1 GOTO 3\n2 PRINT 2\n3 PRINT 3\nRENUM\nLIST\nRUN",     "10 GOTO 30\n20 PRINT 2\n30 PRINT 3\n3\n" },
    { "RENUM adjusts all reference kinds", "1 ON A+1 GOTO 3,5\n2 IF A THEN 3 ELSE 5\n3 RESTORE 5\n4 GOSUB 5\n5 GO TO 1\nRENUM\nLIST",
        "10 ON A+1 GOTO 30,50\n20 IF A THEN 30 ELSE 50\n30 RESTORE 50\n40 GOSUB 50\n50 GO TO 10\n" },
    { "RENUM adjusts THEN GOTO", "1 IF A THEN GOTO 2\n2 PRINT 2\nRENUM\nLIST",         "10 IF A THEN GOTO 20\n20 PRINT 2\n" },
    { "RENUM adjusts LIST, RUN, DELETE", "1 LIST 2-3\n2 RUN 3\n3 DELETE 2\nRENUM\nLIST", "10 LIST 20-30\n20 RUN 30\n30 DELETE 20\n" },
    { "RENUM leaves strings and comments alone", "1 PRINT \"GOTO 3\":GOTO 3 'GOTO 3\n3 PRINT 3\nRENUM\nLIST",
        "10 PRINT \"GOTO 3\":GOTO 20 'GOTO 3\n20 PRINT 3\n" },
    { "RENUM leaves numbers elsewhere alone", "1 A=3:B=A*3:GOTO 3\n3 PRINT 3\nRENUM\nLIST", "10 A=3:B=A*3:GOTO 20\n20 PRINT 3\n" },
    { "RENUM leaves labels alone", "1 GOTO L\n2 PRINT 2\n3 LABEL L\n4 PRINT 4\nRENUM\nLIST\nRUN", "10 GOTO L\n20 PRINT 2\n30 LABEL L\n40 PRINT 4\n4\n" },
    { "RENUM unknown targets stay", "1 GOTO 99\n2 A=1\nRENUM\nLIST",                    "10 GOTO 99\n20 A=1\n" },
    { "RENUM keeps the program runnable", "1 PRINT 1\n2 GOTO 4\n3 PRINT 3\n4 PRINT 4\nRENUM\nRUN", "1\n4\n" },
    { "RENUM a range with a start", "10 A=1\n20 A=2\n30 A=3\n40 A=4\n100 A=5\nRENUM 30-40,5\nLIST",
        "10 A=1\n20 A=2\n30 A=3\n35 A=4\n100 A=5\n" },
    { "RENUM adjusts references to a range", "10 GOTO 40\n20 A=2\n30 A=3\n40 A=4\n100 GOTO 30\nRENUM 30-40,5\nLIST",
        "10 GOTO 35\n20 A=2\n30 A=3\n35 A=4\n100 GOTO 30\n" },
    { "RENUM a single target means to the end", "10 A=1\n20 A=2\n30 A=3\nRENUM 20,2\nLIST", "10 A=1\n20 A=2\n22 A=3\n" },
    { "RENUM up to",            "1 A=1\n2 A=2\n3 A=3\n100 A=4\nRENUM -3\nLIST",     "10 A=1\n20 A=2\n30 A=3\n100 A=4\n" },
    { "RENUM label range",      "10 LABEL A\n20 A=1\n30 LABEL B\n40 A=2\nRENUM A-B,5\nLIST", "10 LABEL A\n15 A=1\n20 LABEL B\n40 A=2\n" },
    { "RENUM overlap",          "10 A=1\n11 A=2\n12 A=3\n13 A=4\nRENUM 11-12\nLIST", "? Renumbering would overlap other lines\n10 A=1\n11 A=2\n12 A=3\n13 A=4\n" },
    { "RENUM too large",        "999990 A=1\n999999 A=2\nRENUM 999990-,20\nLIST",    "? Line number too large\n999990 A=1\n999999 A=2\n" },
    { "RENUM step 0",           "10 A=1\nRENUM ,0\nLIST",                             "? Illegal argument\n10 A=1\n" },
    { "RENUM undefined label",  "10 A=1\nRENUM X\nLIST",                              "? Undefined label X\n10 A=1\n" },
    { "RENUM empty program",    "RENUM",                                              "" },
    { "RENUM in a program ends the run", "10 PRINT 1\n20 RENUM\n30 PRINT 3\n5 PRINT 0\nRUN\nLIST", "0\n1\n10 PRINT 0\n20 PRINT 1\n30 RENUM\n40 PRINT 3\n" },
    { "RENUM then SAVE and LOAD", "1 PRINT 1\n2 GOTO 4\n3 PRINT 3\n4 PRINT 4\nRENUM\nSAVE \"" TESTFILE "\",A\nNEW\nLOAD \"" TESTFILE "\"\nRUN", "1\n4\n" },
    { "NEW in a program",       "10 PRINT 1\n20 NEW\n30 PRINT 3\nRUN\nLIST\nRUN",        "1\n? Empty program\n" },
    { "NEW clears variables",   "A=4\nNEW\nPRINT A",                                   "0\n" },
    { "NEW then a new program", "10 PRINT 1\nNEW\n10 PRINT 2\nRUN",                    "2\n" },
    { "NEW from a loaded program", "10 PRINT \"B\"\n20 NEW\nSAVE \"" TESTFILE "\"\nNEW\nLOAD \"" TESTFILE "\"\nRUN\nLIST", "B\n" },
    { "CLEAR in direct mode",   "A=5\nB$=\"X\"\nCLEAR\nPRINT A;LEN(B$)",               "00\n" },
    { "CLEAR keeps the program","10 PRINT 1\nCLEAR\nRUN",                              "1\n" },
    { "CLEAR in a program",     "10 A=5:B$=\"X\"\n20 CLEAR\n30 PRINT A;LEN(B$)\nRUN", "00\n" },
    { "CLEAR keeps functions",  "DEF FN A(X)=X+1\nCLEAR\nPRINT FN A(1)",                "2\n" },
    { "CLEAR keeps labels",     "10 CLEAR\n20 GOTO M\n30 PRINT 3\n40 LABEL M\n50 PRINT 4\nRUN", "4\n" },
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

// -- direct mode lines and the size of the code and data segments -----------------------------

static bool setup_growth_repl( repl_t* repl ) {
    out_reset();
    keys = 0; polls = 0; breakafter = 0;
    if ( !init_repl( repl ) ) {
        return false;
    }
    repl->report = host_report;
    repl->print  = host_print;
    return true;
}

static bool growth_pass( const char* name, bool pass, const char* detail ) {
    printf( "%s  %s\n", pass ? "PASS" : "FAIL", name );
    if ( !pass ) {
        printf( "    %s\n", detail );
    }
    return pass;
}

// every line of the text is entered <times> times
static void enter_times( repl_t* repl, const char* line, unsigned times ) {
    for ( unsigned i=0; i < times; ++i ) {
        repl_enter( repl, line );
    }
}

static void run_growth_tests( size_t* pnum, size_t* pfailed ) {
    bool r[6]; size_t n = 0;
    char detail[200];
    repl_t repl;

    // lines that define nothing leave nothing behind
    if ( !setup_growth_repl( &repl ) ) { ++*pfailed; return; }
    enter_times( &repl, "A=A+1:PRINT \"HELLO\";A;1.5;100000", 3000U );
    repl_enter( &repl, "PRINT A" );
    snprintf( detail, sizeof(detail), "code %u, data %u (expected 0 and 0), output ends \"%s\"", (unsigned) repl.rt.codegen.codesize,
        (unsigned) repl.rt.codegen.datasize, strlen( outbuf ) > 6U ? outbuf + strlen( outbuf ) - 6U : outbuf );
    r[n++] = growth_pass( "3000 direct lines leave no code or data", repl.rt.codegen.codesize == 0 && repl.rt.codegen.datasize == 0 &&
        strstr( outbuf, "3000\n" ) != 0, detail );
    cleanup_repl( &repl );

    // errors, too (a parse error, a run time error, an error at the end of code generation)
    if ( !setup_growth_repl( &repl ) ) { ++*pfailed; return; }
    for ( unsigned i=0; i < 100U; ++i ) {
        repl_enter( &repl, "PRINT )" );
        repl_enter( &repl, "PRINT \"X\";1/0" );
        repl_enter( &repl, "PRINT \"Y\":GOTO 99" );
    }
    snprintf( detail, sizeof(detail), "code %u, data %u (expected 0 and 0)", (unsigned) repl.rt.codegen.codesize,
        (unsigned) repl.rt.codegen.datasize );
    r[n++] = growth_pass( "failed direct lines leave no code or data", repl.rt.codegen.codesize == 0 && repl.rt.codegen.datasize == 0, detail );
    cleanup_repl( &repl );

    // a function stays, everything after it is discarded again
    if ( !setup_growth_repl( &repl ) ) { ++*pfailed; return; }
    repl_enter( &repl, "DEF FN A(X)=X*2" );
    uint32_t c1 = repl.rt.codegen.codesize, d1 = repl.rt.codegen.datasize;
    out_reset();
    enter_times( &repl, "PRINT FN A(21)", 1000U );
    bool same = repl.rt.codegen.codesize == c1 && repl.rt.codegen.datasize == d1;
    bool right = true;
    for ( unsigned i=0; i < 1000U && right; ++i ) {
        right = strncmp( outbuf + 3U * i, "42\n", 3U ) == 0;
    }
    snprintf( detail, sizeof(detail), "code %u (was %u), data %u (was %u), all 1000 results right: %d", (unsigned) repl.rt.codegen.codesize,
        (unsigned) c1, (unsigned) repl.rt.codegen.datasize, (unsigned) d1, (int) right );
    r[n++] = growth_pass( "a function defined directly keeps its code, later lines don't add any", c1 > 0 && same && right && strlen( outbuf ) == 3000U, detail );
    cleanup_repl( &repl );

    // a line that fails behind a DEF keeps the function
    if ( !setup_growth_repl( &repl ) ) { ++*pfailed; return; }
    repl_enter( &repl, "DEF FN B(X)=X+1:GOTO 99" );
    enter_times( &repl, "PRINT 5", 20U );
    repl_enter( &repl, "PRINT FN B(1)" );
    r[n++] = growth_pass( "a function stays when the line fails behind the DEF", strstr( outbuf, "Undefined line number 99" ) != 0 &&
        strlen( outbuf ) > 2U && strcmp( outbuf + strlen( outbuf ) - 2U, "2\n" ) == 0, outbuf );
    cleanup_repl( &repl );

    // DATA items stay and can be read after many other lines
    if ( !setup_growth_repl( &repl ) ) { ++*pfailed; return; }
    repl_enter( &repl, "DATA 1,2,\"THREE\"" );
    out_reset();
    enter_times( &repl, "PRINT 7", 500U );
    repl_enter( &repl, "READ A,B,C$:PRINT A;B;C$" );
    r[n++] = growth_pass( "DATA items survive other direct lines", strlen( outbuf ) == 1000U + strlen( "12THREE\n" ) &&
        strcmp( outbuf + 1000U, "12THREE\n" ) == 0, outbuf + ( strlen( outbuf ) > 40U ? strlen( outbuf ) - 40U : 0U ) );
    cleanup_repl( &repl );

    // a stored program works after a long session, and its data and code are not mixed up with the lines before
    if ( !setup_growth_repl( &repl ) ) { ++*pfailed; return; }
    enter_times( &repl, "PRINT \"SOME TEXT\";3.25", 200U );
    repl_enter( &repl, "10 PRINT \"PROGRAM\";1.5:GOTO 30" );
    repl_enter( &repl, "20 PRINT \"NOT HERE\"" );
    repl_enter( &repl, "30 PRINT 2" );
    out_reset();
    repl_enter( &repl, "RUN" );
    r[n++] = growth_pass( "RUN after a long session", strcmp( outbuf, "PROGRAM1.5\n2\n" ) == 0, outbuf );
    cleanup_repl( &repl );

    for ( size_t i=0; i < n; ++i ) {
        ++*pnum;
        if ( !r[i] ) {
            ++*pfailed;
        }
    }
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

// -- syntax highlighting of LIST ---------------------------------------------------------------------

// the style callback inserts a marker per class: <L>ine number <S>tatement <C>ontrol <M>command <F>unction <O>perator
// <I>dentifier <N>umber s<T>ring shell(<H>) comment(<R>emark) <P>unctuation, <> = end of the line
static const char* host_hlstyle( hlclass_t cls, void* usrdata ) {
    static const char* const marks[HL_COUNT] = { "<>", "<L>", "<S>", "<C>", "<M>", "<F>", "<O>", "<I>", "<N>", "<T>", "<H>", "<R>", "<P>" };
    return (unsigned) cls < HL_COUNT ? marks[cls] : 0;
}

static const testcase_t hlcases[] = {
    { "highlight: statement, string, identifier, numbers", "10 PRINT \"HI\";A$;12;&H1F\nLIST",
      "<L>10 <S>PRINT <T>\"HI\"<P>;<I>A$<P>;<N>12<P>;<N>&H1F<>\n" },
    { "highlight: control flow and operators", "20 FOR I=1 TO 10 STEP 2:NEXT I\nLIST",
      "<L>20 <C>FOR <I>I<O>=<N>1 <C>TO <N>10 <C>STEP <N>2<P>:<C>NEXT <I>I<>\n" },
    { "highlight: word operators and functions", "30 IF X>1 AND Y THEN GOTO 10 ELSE A=LEN(B$)+CHR(65)\nLIST",
      "<L>30 <C>IF <I>X<O>><N>1 <O>AND <I>Y <C>THEN GOTO <N>10 <C>ELSE <I>A<O>=<F>LEN<P>(<I>B$<P>)<O>+<F>CHR<P>(<N>65<P>)<>\n" },
    { "highlight: comment", "40 'A COMMENT\nLIST", "<L>40 <R>'A COMMENT<>\n" },
    { "highlight: label and shell literal", "50 LABEL L: A=`ls`\nLIST", "<L>50 <S>LABEL <I>L<P>: <I>A<O>=<H>`ls`<>\n" },
    { "highlight: commands", "60 LOAD \"X\":RUN:NEW\nLIST", "<L>60 <M>LOAD <T>\"X\"<P>:<M>RUN<P>:<M>NEW<>\n" },
    { "highlight: a range of lines", "10 A=1\n20 B=2\n30 C=3\nLIST 20-30",
      "<L>20 <I>B<O>=<N>2<>\n<L>30 <I>C<O>=<N>3<>\n" },
    { "highlight: only LIST is colored (SAVE ASCII is plain)", "10 PRINT 1\nSAVE \"" TESTFILE "\",A\nNEW\nLOAD \"" TESTFILE "\"\nLIST",
      "<L>10 <S>PRINT <N>1<>\n" },
};

static void run_highlight_tests( size_t* pnum, size_t* pfailed ) {
    for ( size_t i=0; i < sizeof(hlcases) / sizeof(hlcases[0]); ++i ) {
        const testcase_t* tc = &hlcases[i];
        out_reset();
        keys = 0; polls = 0; breakafter = 0;
        repl_t repl;
        bool pass = false;
        if ( init_repl( &repl ) ) {
            repl.report  = host_report;
            repl.print   = host_print;
            repl.save    = host_save;
            repl.hlstyle = host_hlstyle;
            char* src = strdup( tc->source );
            for ( char* line = strtok( src, "\n" ); line; line = strtok( 0, "\n" ) ) {
                repl_enter( &repl, line );
            }
            free( src );
            cleanup_repl( &repl );
            pass = strcmp( outbuf, tc->expected ) == 0;
        }
        printf( "%s  %s\n", pass ? "PASS" : "FAIL", tc->name );
        if ( !pass ) {
            show( "expected", tc->expected );
            show( "got     ", outbuf );
            ++*pfailed;
        }
        ++*pnum;
    }
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
    run_growth_tests( &numcases, &failed );
    run_highlight_tests( &numcases, &failed );
    remove( TESTFILE );
    printf( "%u of %u passed\n", (unsigned)( numcases - failed ), (unsigned) numcases );
    free( outbuf );
    return (int) failed;
}
