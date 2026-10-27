
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

#include <unistd.h>
#include <fcntl.h>
#include "basic.h"
#include "vm.h"

/*
Automated test of the whole back end: BASIC text -> tokens -> syntax tree -> code -> VM -> output.

Every test case is a list of source lines. Either all lines are numbered (a program, which is stored,
compiled as a whole and run), or none is (direct mode: every line is compiled and run immediately,
variables persist from line to line). The text printed by the program, followed by any error messages,
is compared with the expected text. The exit status is the number of failed cases.

Usage: vmtest1 [-v]      -v: don't silence the compiler's and code generator's debug output
*/

typedef struct _testcase_t {
    const char* name;
    const char* source;     // lines separated by '\n'
    const char* expected;
} testcase_t;

static const testcase_t cases[] = {
    { "number literal",         "PRINT 42",                         "42\n" },
    { "arithmetic precedence",  "PRINT 1+2*3",                      "7\n" },
    { "parentheses",            "PRINT (1+2)*3",                    "9\n" },
    { "subtraction, division",  "PRINT 10-4/2",                     "8\n" },
    { "unary minus",            "PRINT -5+2",                       "-3\n" },
    { "power",                  "PRINT 2**10",                      "1024\n" },
    { "fraction",               "PRINT 1/4",                        "0.25\n" },
    { "comparison true",        "PRINT 1<2",                        "-1\n" },
    { "comparison false",       "PRINT 2<1",                        "0\n" },
    { "string literal",         "PRINT \"HELLO\"",                  "HELLO\n" },
    { "string concatenation",   "PRINT \"AB\"+\"CD\"",              "ABCD\n" },
    { "string comparison",      "PRINT \"A\"<\"B\"",                "-1\n" },
    { "semicolon separator",    "PRINT 1;2;3",                      "123\n" },
    { "comma separator",        "PRINT 1,2",                        "1         2\n" },
    { "trailing semicolon",     "PRINT 1;\nPRINT 2",                "12\n" },
    { "numeric variable",       "A=5\nPRINT A*2",                   "10\n" },
    { "LET",                    "LET A=3\nLET B=A+1\nPRINT B",      "4\n" },
    { "string variable",        "A$=\"HI\"+\"X\"\nPRINT A$",        "HIX\n" },
    { "integer variable",       "A%=300\nPRINT A%*2",               "600\n" },
    { "several statements",     "A=1:B=2:PRINT A+B",                "3\n" },
    { "program",                "10 A=2\n20 PRINT A**3",            "8\n" },
    { "program, line order",    "20 PRINT A\n10 A=7",               "7\n" },
    { "bitwise and",            "PRINT 7 AND 3",                    "3\n" },
    { "bitwise or",             "PRINT 4 OR 1",                     "5\n" },
    { "bitwise not",            "PRINT NOT 0",                      "-1\n" },
    { "left associativity",     "PRINT 10-3-2",                     "5\n" },
    { "string self-append",     "A$=\"X\"\nA$=A$+A$\nPRINT A$",     "XX\n" },
    { "empty PRINT",            "PRINT",                            "\n" },
    { "PRINT with channel only","PRINT #0,",                         "\n" },
    { "unimplemented statement","10 DIM A(3)",                       "? Internal error (feature not implemented yet) in line 10\n" },
    // control flow
    { "GOTO forward",           "10 GOTO 30\n20 PRINT 1\n30 PRINT 2",                 "2\n" },
    { "GOTO backward",          "10 A=0\n20 A=A+1\n30 PRINT A\n40 IF A<3 THEN 20\n50 PRINT 99", "1\n2\n3\n99\n" },
    { "GOTO undefined line",    "10 GOTO 50",                       "? Undefined line number 50 in line 10\n" },
    { "GOSUB and RETURN",       "10 GOSUB 100\n20 PRINT 2\n30 GOTO 200\n100 PRINT 1\n110 RETURN\n200 PRINT 3", "1\n2\n3\n" },
    { "nested GOSUB",           "10 GOSUB 100\n20 PRINT \"D\"\n30 GOTO 900\n100 PRINT \"A\"\n110 GOSUB 200\n120 PRINT \"C\"\n130 RETURN\n200 PRINT \"B\"\n210 RETURN\n900 PRINT \"E\"", "A\nB\nC\nD\nE\n" },
    { "RETURN without GOSUB",   "10 RETURN",                        "? Return stack underflow in line 10\n" },
    { "label GOTO",             "10 GOTO FIN\n20 PRINT 1\n30 LABEL FIN\n40 PRINT 2", "2\n" },
    { "label GOSUB",            "10 GOSUB SUBR\n20 PRINT 2\n30 GOTO 100\n40 LABEL SUBR\n50 PRINT 1\n60 RETURN\n100 PRINT 3", "1\n2\n3\n" },
    { "IF then line",           "10 A=1\n20 IF A=1 THEN 40\n30 PRINT 0\n40 PRINT 1",  "1\n" },
    { "IF false falls through", "10 A=2\n20 IF A=1 THEN 40\n30 PRINT 0\n40 PRINT 1",  "0\n1\n" },
    { "IF else line",           "10 A=2\n20 IF A=1 THEN 50 ELSE 40\n30 PRINT \"N\"\n40 PRINT \"E\"\n45 GOTO 60\n50 PRINT \"T\"\n60 PRINT \"X\"", "E\nX\n" },
    { "IF THEN statements",     "A=1\nIF A=1 THEN PRINT \"YES\":PRINT \"TWO\"",     "YES\nTWO\n" },
    { "IF THEN false",          "A=0\nIF A THEN PRINT \"YES\"\nPRINT \"AFTER\"",    "AFTER\n" },
    { "IF THEN ELSE statements","A=0\nIF A THEN PRINT \"T\" ELSE PRINT \"F\"",       "F\n" },
    { "UNLESS",                 "A=0\nUNLESS A THEN PRINT \"U\"",                    "U\n" },
    { "UNLESS true",            "A=5\nUNLESS A THEN PRINT \"U\" ELSE PRINT \"N\"",   "N\n" },
    { "IF GOSUB",               "10 A=1\n20 IF A GOSUB 100\n30 PRINT 2\n40 GOTO 200\n100 PRINT 1\n110 RETURN\n200 PRINT 3", "1\n2\n3\n" },
    { "multi-line IF",          "10 A=1\n20 IF A=1 THEN\n30 PRINT \"T\"\n40 PRINT \"U\"\n50 ENDIF\n60 PRINT \"END\"", "T\nU\nEND\n" },
    { "multi-line IF else",     "10 A=0\n20 IF A THEN\n30 PRINT \"T\"\n40 ELSE\n50 PRINT \"F\"\n60 ENDIF\n70 PRINT \"END\"", "F\nEND\n" },
    { "multi-line UNLESS",      "10 A=0\n20 UNLESS A THEN\n30 PRINT \"U\"\n40 ENDUNLESS\n50 PRINT \"END\"", "U\nEND\n" },
    { "FOR",                    "10 FOR I=1 TO 3\n20 PRINT I\n30 NEXT I",           "1\n2\n3\n" },
    { "FOR STEP",               "FOR I=10 TO 1 STEP -3:PRINT I:NEXT I",              "10\n7\n4\n1\n" },
    { "FOR fractional step",    "FOR X=0 TO 1 STEP 0.5:PRINT X:NEXT X",              "0\n0.5\n1\n" },
    { "FOR zero passes",        "FOR I=5 TO 1:PRINT I:NEXT I\nPRINT \"DONE\"",      "DONE\n" },
    { "FOR variable after",     "FOR I=1 TO 3:NEXT I\nPRINT I",                     "4\n" },
    { "nested FOR",             "FOR I=1 TO 2:FOR J=1 TO 2:PRINT I*10+J:NEXT J:NEXT I", "11\n12\n21\n22\n" },
    { "NEXT with list",         "FOR I=1 TO 2:FOR J=1 TO 2:PRINT I*10+J:NEXT J,I",   "11\n12\n21\n22\n" },
    { "NEXT without variable",  "FOR I=1 TO 2:PRINT I:NEXT",                         "1\n2\n" },
    { "FOR integer variable",   "FOR I%=1 TO 3:PRINT I%:NEXT I%",                    "1\n2\n3\n" },
    { "FOR with GOSUB",         "10 FOR I=1 TO 2\n20 GOSUB 100\n30 NEXT I\n40 GOTO 200\n100 PRINT I\n110 RETURN\n200 PRINT \"E\"", "1\n2\nE\n" },
    { "NEXT without FOR",       "10 NEXT I",                         "? NEXT without FOR in line 10\n" },
    { "FOR without NEXT",       "10 FOR I=1 TO 3",                   "? FOR without NEXT in line 10\n" },
    { "loop with IF",           "10 FOR I=1 TO 5\n20 IF I=3 THEN 40\n30 PRINT I\n40 NEXT I", "1\n2\n4\n5\n" },
    { "undefined label",        "10 GOTO NOWHERE",                  "? Undefined label in line 10\n" },
    { "endless loop guard",     "10 GOTO 10",                       "? Step limit exceeded in line 10\n" },
    { "nested multi-line IF",   "10 A=1:B=0\n20 IF A THEN\n30 IF B THEN\n40 PRINT \"AB\"\n50 ELSE\n60 PRINT \"A\"\n70 ENDIF\n80 ENDIF\n90 PRINT \"E\"", "A\nE\n" },
    { "FOR inside IF",          "10 A=1\n20 IF A THEN\n30 FOR I=1 TO 2\n40 PRINT I\n50 NEXT I\n60 ENDIF\n70 PRINT \"E\"", "1\n2\nE\n" },
    { "multi-line IF in loop",  "10 FOR I=1 TO 3\n20 IF I=2 THEN\n30 PRINT \"TWO\"\n40 ELSE\n50 PRINT I\n60 ENDIF\n70 NEXT I", "1\nTWO\n3\n" },
    { "division by zero",       "PRINT 1/0",                        "? Division by zero\n" },
    { "error line number",      "10 PRINT 1\n20 PRINT 1/0",         "1\n? Division by zero in line 20\n" },
};

// -- output capture --------------------------------------------------------

static char*  outbuf  = 0;
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

static void vm_print( struct _vm_t* vm, void* usrdata, int channel, const char* text ) {
    out_append( text );
}

static void vm_report( struct _vm_t* vm, void* usrdata, const char* text ) {
    out_append( text );
}

static void rt_report( struct _runtime_t* rt, void* usrdata, const char* text ) {
    out_append( text );
    if ( text[0] && text[ strlen( text ) - 1U ] != '\n' ) {
        out_append( "\n" );
    }
}

static void comp_report( struct _compiler_t* comp, void* usrdata, const char* text ) {
    out_append( text );
}

// -- silencing the debug output of the compiler and the code generator -------

static bool verbose = false;
static int  savedfd = -1;

static void quiet_begin( void ) {
    if ( verbose ) {
        return;
    }
    fflush( stdout );
    savedfd = dup( STDOUT_FILENO );
    int nullfd = open( "/dev/null", O_WRONLY );
    if ( nullfd >= 0 ) {
        dup2( nullfd, STDOUT_FILENO );
        close( nullfd );
    }
}

static void quiet_end( void ) {
    if ( verbose || savedfd < 0 ) {
        return;
    }
    fflush( stdout );
    dup2( savedfd, STDOUT_FILENO );
    close( savedfd );
    savedfd = -1;
}

// -- running ---------------------------------------------------------------

static jmp_buf jmp_fail;
static runtime_t rt;
static compiler_t comp;

static void rt_halt( struct _runtime_t* rt, void* usrdata ) ATTR_NORETURN;

static void rt_halt( struct _runtime_t* rt, void* usrdata ) {
    longjmp( jmp_fail, 1 );
}

static void comp_halt( struct _compiler_t* comp, void* usrdata ) ATTR_NORETURN;

static void comp_halt( struct _compiler_t* comp, void* usrdata ) {
    longjmp( jmp_fail, 1 );
}

// compiles the tokens set up in comp, generates the code and runs it; returns false if anything failed
static bool compile_and_run( void ) {
    bool ok = false;
    cgen_reset( &rt.codegen );
    quiet_begin();
    if ( setjmp( jmp_fail ) == 0 ) {
        run_compiler( &comp );
        ok = cgen_from_program( &rt.codegen, &comp );
    }
    quiet_end();
    if ( !ok ) {
        return false;
    }
    vm_t vm;
    init_vm( &vm, &rt.codegen, &rt.varmem );
    vm.print    = vm_print;
    vm.report   = vm_report;
    vm.maxsteps = UINT64_C(1000000);
    ok = vm_run( &vm );
    cleanup_vm( &vm );
    return ok;
}

static void setup_compiler( void ) {
    cleanup_compiler( &comp );
    init_compiler( &comp, &rt, 0, false );
    comp.halt   = comp_halt;
    comp.report = comp_report;
}

static bool run_direct( const uint8_t* tokens, size_t size ) {
    setup_compiler();
    comp.iter.hdr.lineno = LINENO_NONE;
    comp_set_tokens( &comp, tokens, size );
    return compile_and_run();
}

static bool run_program( program_t* pgm ) {
    setup_compiler();
    if ( !begin_iterate_program( &comp.iter, pgm ) ) {
        out_append( "? Empty program\n" );
        return false;
    }
    comp_set_tokens( &comp, comp.iter.tok, comp.iter.hdr.length - LINEHDR_SIZE );
    return compile_and_run();
}

static bool no_directmode( program_t* pgm, const uint8_t* tokens, size_t size ) {
    return false;
}

// returns false if the line couldn't be processed at all (the result is then in the output buffer)
static bool enter_source( const char* source, program_t* pgm, buf_t* tokbuf, bool* pprogram ) {
    char* text = strdup( source );
    if ( text == 0 ) {
        return false;
    }
    bool ok = true;
    for ( char* line = strtok( text, "\n" ); line && ok; line = strtok( 0, "\n" ) ) {
        char* copy = strdup( line );
        preprocess_buffer( copy );
        linehdr_t hdr;
        clear_buffer( tokbuf );
        if ( !tokenize_line( copy, tokbuf, &hdr ) ) {
            out_append( "? Syntax error\n" );
            ok = false;
        } else if ( hdr.lineno == LINENO_NONE ) {
            // direct mode: run now (errors are part of the output, but don't stop the sequence)
            const uint8_t* tokens = (const uint8_t*) tokbuf->buffer;
            run_direct( tokens + LINEHDR_SIZE, hdr.length - LINEHDR_SIZE );
        } else {
            *pprogram = true;
            ok = enter_line( pgm, (const uint8_t*) tokbuf->buffer, no_directmode );
        }
        free( copy );
    }
    free( text );
    return ok;
}

static bool run_case( const testcase_t* tc, buf_t* tokbuf ) {
    out_reset();
    cleanup_runtime( &rt );
    init_runtime( &rt );
    rt.report = rt_report;
    rt.halt   = rt_halt;
    program_t pgm; init_program( &pgm );
    bool isprogram = false;
    if ( enter_source( tc->source, &pgm, tokbuf, &isprogram ) && isprogram ) {
        run_program( &pgm );
    }
    free_program( &pgm );
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
    verbose = argc > 1 && strcmp( argv[1], "-v" ) == 0;
    buf_t* tokbuf = create_buffer();
    if ( tokbuf == 0 ) {
        return EXIT_FAILURE;
    }
    size_t numcases = sizeof(cases) / sizeof(cases[0]), failed = 0;
    for ( size_t i=0; i < numcases; ++i ) {
        if ( run_case( &cases[i], tokbuf ) ) {
            printf( "PASS  %s\n", cases[i].name );
        } else {
            printf( "FAIL  %s\n", cases[i].name );
            show( "expected", cases[i].expected );
            show( "got     ", outbuf );
            ++failed;
        }
    }
    cleanup_compiler( &comp );
    cleanup_runtime( &rt );
    delete_buffer( tokbuf );
    free( outbuf );
    printf( "%zu of %zu passed\n", numcases - failed, numcases );
    return failed > 255U ? 255 : (int) failed;
}
