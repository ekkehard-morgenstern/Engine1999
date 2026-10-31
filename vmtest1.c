
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

#include "basic.h"
#include "vm.h"
#include "gfxops.h"
#include "basdebug.h"

/*
Automated test of the whole back end: BASIC text -> tokens -> syntax tree -> code -> VM -> output.

Every test case is a list of source lines. Either all lines are numbered (a program, which is stored,
compiled as a whole and run), or none is (direct mode: every line is compiled and run immediately,
variables persist from line to line). The text printed by the program, followed by any error messages,
is compared with the expected text. The exit status is the number of failed cases.

Usage: vmtest1 [-v]      -v: show the debug output of the compiler and the code generator (see basdebug.h)
*/

typedef struct _testcase_t {
    const char* name;
    const char* source;     // lines separated by '\n'
    const char* expected;
    const char* input;      // keys for INKEY$ and GET, lines (separated by '\n') for INPUT (optional)
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
    { "PRINT channel is not sticky", "OPEN \"null:\" FOR OUTPUT AS #3:PRINT #3,\"A\":PRINT \"B\"", "B\n" },
    { "PRINT with channel only","PRINT #0,",                         "\n" },
    // ERASE
    { "ERASE numeric array",    "DIM A(3):A(1)=5:ERASE A():PRINT A(1)",                 "0\n" },
    { "ERASE string array",     "DIM S$(2):S$(1)=\"X\":ERASE S$():PRINT \"[\"+S$(1)+\"]\"", "[]\n" },
    { "ERASE two arrays",       "DIM A(2),B(2):A(0)=1:B(0)=2:ERASE A(),B():PRINT A(0)+B(0)", "0\n" },
    { "ERASE two dimensions",   "DIM A(3,3):A(2,2)=4:A(0,1)=1:ERASE A():PRINT A(2,2)+A(0,1)", "0\n" },
    { "array usable after ERASE", "DIM A(3):A(1)=5:ERASE A():A(1)=7:PRINT A(1)",        "7\n" },
    { "ERASE keeps other arrays", "DIM A(2),B(2):B(1)=9:ERASE A():PRINT B(1)",          "9\n" },
    { "ERASE of undefined array", "ERASE Q()",                                          "? Undefined variable\n" },
    // substring assignment
    { "LEFT$ assignment",       "A$=\"HELLO\":LEFT$(A$,2)=\"XY\":PRINT A$",             "XYLLO\n" },
    { "LEFT$ short replacement", "A$=\"HELLO\":LEFT$(A$,3)=\"X\":PRINT A$",             "XELLO\n" },
    { "LEFT$ long replacement", "A$=\"HELLO\":LEFT$(A$,10)=\"ABCDEFGH\":PRINT A$",      "ABCDE\n" },
    { "LEFT$ zero",             "A$=\"HELLO\":LEFT$(A$,0)=\"X\":PRINT A$",             "HELLO\n" },
    { "LEFT$ negative",         "A$=\"HELLO\":LEFT$(A$,0-1)=\"X\"",                    "? Illegal argument\n" },
    { "RIGHT$ assignment",      "A$=\"HELLO\":RIGHT$(A$,2)=\"XY\":PRINT A$",            "HELXY\n" },
    { "RIGHT$ short replacement", "A$=\"HELLO\":RIGHT$(A$,3)=\"X\":PRINT A$",           "HELLX\n" },
    { "MID$ assignment",        "A$=\"HELLO\":MID$(A$,2,3)=\"XYZ\":PRINT A$",           "HXYZO\n" },
    { "MID$ without length",    "A$=\"HELLO\":MID$(A$,2)=\"XY\":PRINT A$",              "HXYLO\n" },
    { "MID$ replacement past the end", "A$=\"HELLO\":MID$(A$,4)=\"ABCDE\":PRINT A$",     "HELAB\n" },
    { "MID$ length limits",     "A$=\"HELLO\":MID$(A$,2,1)=\"XYZ\":PRINT A$",           "HXLLO\n" },
    { "MID$ start zero",        "A$=\"AB\":MID$(A$,0)=\"X\"",                          "? Illegal argument\n" },
    { "MID$ start beyond end",  "A$=\"AB\":MID$(A$,3)=\"X\"",                          "? Illegal argument\n" },
    { "substring of array element", "DIM S$(3):S$(2)=\"HELLO\":MID$(S$(2),1,2)=\"JE\":PRINT S$(2)", "JELLO\n" },
    { "substring with expressions", "A$=\"HELLO\":N=1:LEFT$(A$,N+1)=\"A\"+\"B\":PRINT A$", "ABLLO\n" },
    { "substring keeps length", "A$=\"HELLO\":LEFT$(A$,2)=\"\":PRINT LEN(A$);A$",         "5HELLO\n" },
    { "substring in loop",      "A$=\"AAAA\":FOR I=1 TO 4:MID$(A$,I,1)=MID$(\"BCDE\",I,1):NEXT I:PRINT A$", "BCDE\n" },
    { "substring target must be a variable", "LEFT$(\"AB\",1)=\"X\"",                  "? String variable expected in substring assignment\n" },
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
    // arrays
    { "array write and read",   "DIM A(3):A(0)=5:A(1)=7:PRINT A(0)+A(1)",           "12\n" },
    { "array initial value",    "DIM A(2):PRINT A(1)",                              "0\n" },
    { "array without DIM",      "A(5)=1:PRINT A(5)",                                "1\n" },
    { "two dimensions",         "DIM B(2,3):B(1,2)=9:B(0,0)=1:PRINT B(1,2)*10+B(0,0)", "91\n" },
    { "string array",           "DIM S$(3):S$(1)=\"HI\":S$(2)=\"YOU\":PRINT S$(1)+S$(2)", "HIYOU\n" },
    { "empty string element",   "DIM S$(2):PRINT \"[\"+S$(1)+\"]\"",                 "[]\n" },
    { "integer array",          "DIM I%(2):I%(1)=300:PRINT I%(1)*2",                "600\n" },
    { "array index expression", "DIM A(5):I=2:A(I+1)=4:PRINT A(3)",                 "4\n" },
    { "array in loop",          "10 DIM A(4)\n20 FOR I=0 TO 3:A(I)=I*I:NEXT I\n30 FOR I=0 TO 3:PRINT A(I):NEXT I", "0\n1\n4\n9\n" },
    { "subscript out of range", "DIM A(3):PRINT A(3)",                              "? Subscript out of range\n" },
    { "negative subscript",     "DIM A(3):PRINT A(0-1)",                            "? Subscript out of range\n" },
    { "array element as index", "DIM A(3),B(3):A(1)=2:B(2)=7:PRINT B(A(1))",        "7\n" },
    // user functions
    { "DEF FN",                 "DEF FN A(X)=X*2\nPRINT FN A(4)",                     "8\n" },
    { "DEF FN two arguments",   "DEF FN A(X,Y)=X-Y\nPRINT FN A(10,3)",               "7\n" },
    { "DEF FN no arguments",    "DEF FN P=3\nPRINT FN P+1",                          "4\n" },
    { "DEF FN restores params", "X=1\nDEF FN A(X)=X*2\nPRINT FN A(5):PRINT X",       "10\n1\n" },
    { "DEF FN string",          "DEF FN A$(S$)=S$+\"!\"\nPRINT FN A$(\"HI\")",        "HI!\n" },
    { "DEF FN calls DEF FN",    "DEF FN A(X)=X*2\nDEF FN B(Y)=FN A(Y)+1\nPRINT FN B(5)", "11\n" },
    { "DEF FN in expression",   "DEF FN A(X)=X+1\nPRINT FN A(1)*FN A(2)",              "6\n" },
    { "DEF FN nested call",     "DEF FN A(X)=X+1\nPRINT FN A(FN A(1))",               "3\n" },
    { "DEF FN in program",      "10 DEF FN A(X)=X*X\n20 PRINT FN A(3)",              "9\n" },
    { "call before DEF",        "10 PRINT FN A(3)\n20 DEF FN A(X)=X*X",              "? Undefined function in line 10\n" },
    { "DEF FN uses variable",   "K=10\nDEF FN A(X)=X+K\nK=20\nPRINT FN A(1)",       "21\n" },
    { "DEF FN in loop",         "DEF FN A(X)=X*X\nFOR I=1 TO 3:PRINT FN A(I):NEXT I", "1\n4\n9\n" },
    { "DEF FN with array",      "DIM A(3):A(2)=5\nDEF FN A(I)=A(I)*2\nPRINT FN A(2)", "10\n" },
    // multi-line functions
    { "multi-line DEF FN",      "10 DEF FN A(X)\n20 FN A = X*2\n30 END FN\n40 PRINT FN A(4)",                       "8\n" },
    { "multi-line FN with IF",  "10 DEF FN A(X)\n20 FN A = 0\n30 IF X>5 THEN FN A = 1\n40 END FN\n50 PRINT FN A(3);FN A(9)", "01\n" },
    { "multi-line string FN",   "10 DEF FN A$(S$)\n20 FN A$ = S$+\"!\"\n30 END FN\n40 PRINT FN A$(\"HI\")",          "HI!\n" },
    { "multi-line FN default result", "10 DEF FN A(X)\n20 Y=X\n30 END FN\n40 PRINT FN A(5)",                         "0\n" },
    { "multi-line FN default string", "10 DEF FN A$(X)\n20 Y=X\n30 END FN\n40 PRINT \"[\"+FN A$(5)+\"]\"",         "[]\n" },
    { "multi-line FN with loop", "10 DEF FN S(N)\n20 T=0\n30 FOR I=1 TO N\n40 T=T+I\n50 NEXT I\n60 FN S = T\n70 END FN\n80 PRINT FN S(4)", "10\n" },
    { "multi-line FN last assignment wins", "10 DEF FN A(X)\n20 FN A = 1\n30 FN A = 2\n40 END FN\n50 PRINT FN A(0)", "2\n" },
    { "multi-line FN calls FN", "10 DEF FN B(X)=X+1\n20 DEF FN A(X)\n30 FN A = FN B(X)*2\n40 END FN\n50 PRINT FN A(4)", "10\n" },
    { "multi-line FN restores params", "10 X=1\n20 DEF FN A(X)\n30 X=X+10\n40 FN A = X\n50 END FN\n60 PRINT FN A(5);X", "151\n" },
    { "two multi-line FNs",     "10 DEF FN A(X)\n20 FN A = X+1\n30 END FN\n40 DEF FN B(X)\n50 FN B = X*10\n60 END FN\n70 PRINT FN A(1);FN B(2)", "220\n" },
    { "multi-line FN in a loop", "10 DEF FN A(X)\n20 FN A = X*X\n30 END FN\n40 FOR I=1 TO 3:PRINT FN A(I):NEXT I", "1\n4\n9\n" },
    { "multi-line integer FN",  "10 DEF FN A%(X)\n20 FN A% = X+1\n30 END FN\n40 PRINT FN A%(41)",               "42\n" },
    { "FN assignment to another function", "10 DEF FN A(X)\n20 FN B = 1\n30 END FN", "? Only the function being defined can be assigned in line 20\n" },
    { "FN assignment outside a function", "10 FN A = 1",                                "? Statement lines expected in line 10\n" },
    { "multi-line FN without END FN", "10 DEF FN A(X)\n20 FN A = 1",                      "? END expected in line 20\n" },
    { "error inside DEF FN",    "DEF FN A(X)=1/X\nPRINT FN A(0)",                  "? Division by zero\n" },
    { "error inside DEF FN with string", "S$=\"KEEP\"\nDEF FN A(S$)=1/0\nPRINT FN A(\"X\")", "? Division by zero\n" },
    { "string param restored",  "S$=\"KEEP\"\nDEF FN A$(S$)=S$+S$\nPRINT FN A$(\"AB\"):PRINT S$", "ABAB\nKEEP\n" },
    { "DEF FN wrong argument count", "DEF FN A(X)=X\nPRINT FN A(1,2)",                "? Argument count mismatch\n" },
    // system functions
    { "LEN",                    "PRINT LEN(\"HELLO\")",                             "5\n" },
    { "LEN of variable",        "A$=\"XYZ\":PRINT LEN(A$)",                        "3\n" },
    { "LEN of empty string",    "PRINT LEN(\"\")",                                 "0\n" },
    { "ASC",                    "PRINT ASC(\"A\")",                                "65\n" },
    { "ASC of empty string",    "PRINT ASC(\"\")",                                 "? Illegal argument\n" },
    { "VAL",                    "PRINT VAL(\"12.5ABC\")",                          "12.5\n" },
    { "VAL without number",     "PRINT VAL(\"ABC\")",                              "0\n" },
    { "VAL negative",           "PRINT VAL(\"-3\")+1",                             "-2\n" },
    { "VAL exponent",           "PRINT VAL(\" 1E2X\")",                            "100\n" },
    { "VAL incomplete exponent","PRINT VAL(\"5E\")",                               "5\n" },
    { "STR$",                   "PRINT STR$(42)+\"!\"",                            "42!\n" },
    { "STR$ fraction",          "PRINT STR$(1/4)",                                "0.25\n" },
    { "LEFT$",                  "PRINT LEFT$(\"HELLO\",2)",                        "HE\n" },
    { "LEFT$ beyond length",    "PRINT LEFT$(\"AB\",10)",                          "AB\n" },
    { "LEFT$ zero",             "PRINT \"[\"+LEFT$(\"AB\",0)+\"]\"",              "[]\n" },
    { "LEFT$ negative",         "PRINT LEFT$(\"A\",0-1)",                          "? Illegal argument\n" },
    { "RIGHT$",                 "PRINT RIGHT$(\"HELLO\",3)",                       "LLO\n" },
    { "RIGHT$ beyond length",   "PRINT RIGHT$(\"AB\",10)",                         "AB\n" },
    { "MID$",                   "PRINT MID$(\"HELLO\",2,3)",                       "ELL\n" },
    { "MID$ without length",    "PRINT MID$(\"HELLO\",3)",                         "LLO\n" },
    { "MID$ length beyond end", "PRINT MID$(\"HELLO\",4,10)",                      "LO\n" },
    { "MID$ start beyond end",  "PRINT \"[\"+MID$(\"HELLO\",9)+\"]\"",            "[]\n" },
    { "MID$ start zero",        "PRINT MID$(\"HELLO\",0)",                         "? Illegal argument\n" },
    { "HEX$",                   "PRINT HEX$(255)",                                "FF\n" },
    { "HEX$ padded",            "PRINT HEX$(255,4)",                              "00FF\n" },
    { "HEX$ negative",          "PRINT HEX$(0-1)",                                "FFFFFFFF\n" },
    { "BIN$",                   "PRINT BIN$(5)",                                  "101\n" },
    { "BIN$ padded",            "PRINT BIN$(5,8)",                                "00000101\n" },
    { "BIN$ zero",              "PRINT BIN$(0)",                                  "0\n" },
    { "OCT$",                   "PRINT OCT$(8)",                                  "10\n" },
    { "QUA$",                   "PRINT QUA$(5)",                                  "11\n" },
    { "DEC$ padded",            "PRINT DEC$(7,3)",                                "007\n" },
    { "DEC$ negative",          "PRINT DEC$(0-5)",                                "-5\n" },
    { "radix digits range",     "PRINT HEX$(1,300)",                              "? Illegal argument\n" },
    { "TI",                     "PRINT TI>=0",                                    "-1\n" },
    { "TI does not go back",    "A=TI:B=TI:PRINT B>=A",                           "-1\n" },
    { "FRE",                    "PRINT FRE(0)>0",                                 "-1\n" },
    { "INKEY$",                 "PRINT INKEY$;INKEY$;\"[\"+INKEY$+\"]\"",          "AB[]\n", "AB" },
    // INPUT, GET
    { "INPUT number",           "INPUT A:PRINT A*2",                              "? 21\n42\n", "21" },
    { "INPUT string",           "INPUT A$:PRINT \"[\"+A$+\"]\"",                  "? HELLO WORLD\n[HELLO WORLD]\n", "HELLO WORLD" },
    { "INPUT prompt",           "INPUT \"NAME\";N$:PRINT N$",                     "NAME? BOB\nBOB\n", "BOB" },
    { "INPUT prompt, comma",    "INPUT \"AGE: \",A:PRINT A",                      "AGE: 7\n7\n", "7" },
    { "INPUT several variables","INPUT A,B$,C%:PRINT A;B$;C%",                    "? 1.5,XY,3\n1.5XY3\n", "1.5,XY,3" },
    { "INPUT quoted string",    "INPUT A$,B$:PRINT A$;\"|\";B$",                   "? \"A,B\",C\nA,B|C\n", "\"A,B\",C" },
    { "INPUT blanks trimmed",   "INPUT A,B$:PRINT A;\"|\";B$;\"|\"",              "?  4 ,  XY  \n4|XY|\n", " 4 ,  XY  " },
    { "INPUT empty line, string","INPUT A$:PRINT \"[\"+A$+\"]\"",                 "? \n[]\n", "\n" },
    { "INPUT empty line, number","INPUT A:PRINT A",                               "? \n0\n", "\n" },
    { "INPUT empty second field","INPUT A,B$:PRINT A;\"|\";B$;\"|\"",             "? 5,\n5||\n", "5," },
    { "INPUT exponent, sign",   "INPUT A,B:PRINT A+B",                            "? -1E2,+.5\n-99.5\n", "-1E2,+.5" },
    { "INPUT redo, not a number","INPUT A:PRINT A",                               "? X\n?Redo from start\n? 5\n5\n", "X\n5" },
    { "INPUT redo, too few fields","INPUT A,B:PRINT A+B",                         "? 1\n?Redo from start\n? 1,2\n3\n", "1\n1,2" },
    { "INPUT redo shows the prompt","INPUT \"N\";A:PRINT A",                      "N? Z\n?Redo from start\nN? 2\n2\n", "Z\n2" },
    { "INPUT extra ignored",    "INPUT A:PRINT A",                                "? 1,2,3\n?Extra ignored\n1\n", "1,2,3" },
    { "INPUT to array element", "DIM A(3),S$(2):INPUT A(2),S$(1):PRINT A(2);S$(1)", "? 9,Q\n9Q\n", "9,Q" },
    { "INPUT in a loop",        "FOR I=1 TO 3:INPUT A:S=S+A:NEXT I:PRINT S",       "? 1\n? 2\n? 3\n6\n", "1\n2\n3" },
    { "INPUT with channel",     "INPUT #0,A:PRINT A",                             "? 4\n4\n", "4" },
    { "INPUT at end of input",  "INPUT A",                                        "? ? End of input\n" },
    { "INPUT needs a variable", "INPUT \"X\";",                                    "? Variable expected\n" },
    { "INPUT prompt needs separator", "INPUT \"X\" A",                             "? Semicolon ';' or comma ',' expected after prompt\n" },
    { "GET string",             "GET A$:PRINT A$",                                "Q\n", "Q" },
    { "GET number",             "GET A:PRINT A",                                  "65\n", "A" },
    { "GET several",            "GET A$,B$,C:PRINT A$;B$;C",                      "XY48\n", "XY0" },
    { "GET array element",      "DIM K$(2):GET K$(1):PRINT K$(1)",                "Z\n", "Z" },
    { "GET with channel",       "GET #0,A$:PRINT A$",                             "K\n", "K" },
    { "GET at end of input",    "GET A$",                                         "? End of input\n" },
    { "CHR$",                   "PRINT CHR$(65)+CHR$(66.9)",                      "AB\n" },
    { "CHR$ and ASC",           "PRINT ASC(CHR$(200))",                           "200\n" },
    { "CHR$ length",            "PRINT LEN(CHR$(255))",                           "1\n" },
    { "CHR$ in assignment",     "A$=CHR$(72)+\"I\":PRINT A$",                    "HI\n" },
    { "CHR$ zero",              "PRINT CHR$(0)",                                  "<0>\n" },
    // graphics: the statements and functions of gfxops.h (the test host logs the operations)
    { "GFX PSET",               "GFX PSET 10,20",                                 "<gfx GFX PSET 10 20 ->\n" },
    { "GFX PSET with a color",  "GFX PSET 10,20,5",                               "<gfx GFX PSET 10 20 5>\n" },
    { "GFX LINE",               "GFX LINE 1,2,3,4",                               "<gfx GFX LINE 1 2 3 4 - ->\n" },
    { "GFX LINE box",           "GFX LINE 1,2,3,4,7,B",                           "<gfx GFX LINE 1 2 3 4 7 0>\n" },
    { "GFX LINE filled box without a color", "GFX LINE 1,2,3,4,,BF",              "<gfx GFX LINE 1 2 3 4 - 1>\n" },
    { "GFX CIRCLE",             "GFX CIRCLE 5,6,7",                               "<gfx GFX CIRCLE 5 6 7 - - - ->\n" },
    { "GFX CIRCLE arc ellipse", "GFX CIRCLE 5,6,7,2,0,90,0.5",                    "<gfx GFX CIRCLE 5 6 7 2 0 90 0.5>\n" },
    { "GFX PAINT",              "GFX PAINT 1,2,3",                                "<gfx GFX PAINT 1 2 3 ->\n" },
    { "GFX PAINT with a border", "GFX PAINT 1,2,3,4",                             "<gfx GFX PAINT 1 2 3 4>\n" },
    { "GFX RADIUS",             "GFX RADIUS 100,100,50,45",                       "<gfx GFX RADIUS 100 100 50 45 ->\n" },
    { "GFX CLEAR",              "GFX CLEAR",                                      "<gfx GFX CLEAR ->\n" },
    { "GFX CLEAR with a color", "GFX CLEAR 4",                                    "<gfx GFX CLEAR 4>\n" },
    { "GFX LAYER ON",           "GFX LAYER 3,ON",                                 "<gfx GFX LAYER 3 1>\n" },
    { "GFX LAYER OFF",          "GFX LAYER 3,OFF",                                "<gfx GFX LAYER 3 0>\n" },
    { "GFX LAYER bad word",     "GFX LAYER 3,MAYBE",                              "? Unknown word 'MAYBE' in GFX LAYER\n" },
    { "GFX LAYER bad number",   "GFX LAYER 5,ON",                                 "? Illegal argument\n" },
    { "GFX PRIORITY",           "GFX PRIORITY 2,4",                               "<gfx GFX PRIORITY 2 4>\n" },
    { "GFX PALETTE with RGB",   "GFX PALETTE 1,5,RGB(255,128,0)",                 "<gfx GFX PALETTE 1 5 4.29493e+09>\n" },
    { "GFX PALNAME",            "GFX PALNAME 1,5,\"Gold\"",                       "<gfx GFX PALNAME 1 5 s(4)=71,111,108,100>\n" },
    { "GFX PALRESET",           "GFX PALRESET",                                   "<gfx GFX PALRESET ->\n" },
    { "GFX PALRESET one layer", "GFX PALRESET 2",                                 "<gfx GFX PALRESET 2>\n" },
    { "GFX BACKDROP",           "GFX BACKDROP RGB(1,2,3)",                        "<gfx GFX BACKDROP 4.27826e+09>\n" },
    { "GFX TARGET",             "GFX TARGET 0",                                   "<gfx GFX TARGET 0>\n" },
    { "GFX TARGET bad layer",   "GFX TARGET 2",                                   "? Illegal argument\n" },
    { "GFX PEN",                "GFX PEN 12",                                     "<gfx GFX PEN 12>\n" },
    { "GFX MODE XOR",           "GFX MODE XOR",                                   "<gfx GFX MODE 2>\n" },
    { "GFX MODE AND",           "GFX MODE and",                                   "<gfx GFX MODE 4>\n" },
    { "GFX GRAB into a string", "GFX GRAB 1,2,3,4,A$:PRINT A$",                   "<gfx GFX GRAB 1 2 3 4>\nabcd\n" },
    { "GFX BLIT",               "GFX BLIT \"abcd\",1,2",                          "<gfx GFX BLIT 1 2 - s(4)=97,98,99,100>\n" },
    { "GFX BLIT transparent",   "GFX BLIT \"abcd\",1,2,TRANS",                    "<gfx GFX BLIT 1 2 0 s(4)=97,98,99,100>\n" },
    { "GFX BLIT too short",     "GFX BLIT \"ab\",1,2",                            "? Illegal argument\n" },
    { "GFX TEXT",               "GFX TEXT 10,20,\"Hi\",3,2,2",                    "<gfx GFX TEXT 10 20 3 2 2 s(2)=72,105>\n" },
    { "GFX TEXT defaults",      "GFX TEXT 10,20,\"Hi\"",                          "<gfx GFX TEXT 10 20 - - - s(2)=72,105>\n" },
    { "GFX DRAW",               "GFX DRAW \"U10R5\"",                             "<gfx GFX DRAW s(5)=85,49,48,82,53>\n" },
    { "GFX VSYNC",              "GFX VSYNC",                                      "<gfx GFX VSYNC>\n" },
    { "GFX RESET",              "GFX RESET",                                      "<gfx GFX RESET ->\n" },
    { "GFX RESET ALL",          "GFX RESET ALL",                                  "<gfx GFX RESET 0>\n" },
    { "GFX RESET with a bad word", "GFX RESET EVERYTHING",                        "? Unknown word 'EVERYTHING' in GFX RESET\n" },
    { "GFX unknown command",    "GFX FROB 1",                                     "? Unknown GFX command\n" },
    { "TXT FILL",               "TXT FILL 0,0,79,24",                             "<gfx TXT FILL 0 0 79 24 - - ->\n" },
    { "TXT FILL with all",      "TXT FILL 0,0,9,9,32,7,1",                        "<gfx TXT FILL 0 0 9 9 32 7 1>\n" },
    { "TXT COPY",               "TXT COPY 0,0,10,10,5,5",                         "<gfx TXT COPY 0 0 10 10 5 5>\n" },
    { "TXT INVERT",             "TXT INVERT 1,2,3,4",                             "<gfx TXT INVERT 1 2 3 4>\n" },
    { "TXT SAVE",               "TXT SAVE A$:PRINT LEN(A$)",                      "<gfx TXT SAVE>\n4\n" },
    { "TXT LOAD of a wrong size", "TXT LOAD \"x\"",                               "? Illegal argument\n" },
    { "CLS",                    "CLS",                                            "<gfx CLS>\n" },
    { "LOCATE",                 "LOCATE 5,6",                                     "<gfx LOCATE 5 6>\n" },
    { "LOCATE out of range",    "LOCATE 80,0",                                    "? Illegal argument\n" },
    { "COLOR",                  "COLOR 3",                                        "<gfx COLOR 3 ->\n" },
    { "COLOR with paper",       "COLOR 3,4",                                      "<gfx COLOR 3 4>\n" },
    { "SYMBOL",                 "SYMBOL 65,\"123456789012\"",                     "<gfx SYMBOL 65 s(12)=49,50,51,52,53,54,55,56,57,48,49,50>\n" },
    { "SYMBOL needs 12 bytes",  "SYMBOL 65,\"123\"",                              "? Illegal argument\n" },
    { "TILE DEF",               "A$=\"\":FOR I=1 TO 384:A$=A$+CHR$(I AND 255):NEXT I:TILE DEF 7,A$", "<gfx TILE DEF 7 s(384)=1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16>\n" },
    { "TILE DEF needs 384 bytes", "TILE DEF 7,\"abc\"",                           "? Illegal argument\n" },
    { "TILE GET",               "TILE GET 3,A$:PRINT LEN(A$)",                    "<gfx TILE GET 3>\n4\n" },
    { "TILE SET",               "TILE SET 1,2,3",                                 "<gfx TILE SET 1 2 3>\n" },
    { "TILE SET out of range",  "TILE SET 41,0,0",                                "? Illegal argument\n" },
    { "TILE FILL of a wrong size", "TILE FILL \"x\"",                             "? Illegal argument\n" },
    { "TILE SCROLL",            "TILE SCROLL 3,4",                                "<gfx TILE SCROLL 3 4>\n" },
    { "SPRITE ON",              "SPRITE 1,ON",                                    "<gfx SPRITE ON 1>\n" },
    { "SPRITE OFF",             "SPRITE 1,OFF",                                   "<gfx SPRITE OFF 1>\n" },
    { "SPRITE move",            "SPRITE 1,10,20",                                 "<gfx SPRITE MOVE 1 10 20 ->\n" },
    { "SPRITE move with an image", "SPRITE 1,10,20,5",                            "<gfx SPRITE MOVE 1 10 20 5>\n" },
    { "SPRITE number is an expression", "S=2:SPRITE S+1,-5,300",                  "<gfx SPRITE MOVE 3 -5 300 ->\n" },
    { "SPRITE PRIORITY",        "SPRITE 1,PRIORITY 3",                            "<gfx SPRITE PRIORITY 1 3>\n" },
    { "SPRITE IMAGE",           "SPRITE 1,IMAGE 9",                               "<gfx SPRITE IMAGE 1 9>\n" },
    { "SPRITE ANIM",            "SPRITE 1,ANIM 2,4,6",                            "<gfx SPRITE ANIM 1 2 4 6>\n" },
    { "SPRITE MOVETO",          "SPRITE 1,MOVETO 100,50,30",                      "<gfx SPRITE MOVETO 1 100 50 30>\n" },
    { "SPRITE COLLISION ON",    "SPRITE 1,COLLISION ON",                          "<gfx SPRITE COLLISION 1 1 ->\n" },
    { "SPRITE COLLISION ON, TILES", "SPRITE 1,COLLISION ON,TILES",                "<gfx SPRITE COLLISION 1 1 0>\n" },
    { "SPRITE COLLISION OFF",   "SPRITE 1,COLLISION OFF",                         "<gfx SPRITE COLLISION 1 0 ->\n" },
    { "SPRITE DEF",             "A$=\"\":FOR I=1 TO 384:A$=A$+\"x\":NEXT I:SPRITE DEF 2,A$", "<gfx SPRITE DEF 2 s(384)=120,120,120,120,120,120,120,120,120,120,120,120,120,120,120,120>\n" },
    { "SPRITE GET",             "SPRITE GET 2,A$:PRINT LEN(A$)",                  "<gfx SPRITE GET 2>\n4\n" },
    { "SPRITE SEQ",             "SPRITE SEQ 1,CHR$(1)+CHR$(2)",                   "<gfx SPRITE SEQ 1 s(2)=1,2>\n" },
    { "SPRITE SEQ is empty",    "SPRITE SEQ 1,\"\"",                              "? Illegal argument\n" },
    { "SPRITE number out of range", "SPRITE 300,ON",                              "? Illegal argument\n" },
    { "POINT",                  "PRINT POINT(3,4)",                               "7\n" },
    { "RGB",                    "PRINT RGB(1,2,3)",                               "4278256131\n" },
    { "RGB with alpha",         "PRINT RGB(1,2,3,4)",                             "67174915\n" },
    { "RGB out of range",       "PRINT RGB(256,0,0)",                             "? Illegal argument\n" },
    { "GFX() values",           "PRINT GFX(\"FRAME\");GFX(\"LAYER\",3);GFX(\"PALETTE\",1,5)", "036\n" },
    { "TXT() values",           "PRINT TXT(\"CHAR\",2,3);TXT(\"X\")",             "50\n" },
    { "SPRITE() values",        "PRINT SPRITE(\"X\",9);SPRITE(\"HITS\",2)",       "92\n" },
    { "TILE() value",           "PRINT TILE(4,5)",                                "9\n" },
    { "unknown value name",     "PRINT SPRITE(\"NOPE\",1)",                       "? Unknown SPRITE value 'NOPE'\n" },
    { "too few arguments",      "PRINT GFX(\"LAYER\")",                           "? Too few arguments\n" },
    { "sprite query out of range", "PRINT SPRITE(\"HITS\",300)",                  "? Illegal argument\n" },
    { "DESIGN CHAR",            "10 DESIGN CHAR 65\n20 FOR I=1 TO 12:@ \"o......o\":NEXT I", "<gfx SYMBOL 65 s(12)=129,129,129,129,129,129,129,129,129,129,129,129>\n" },
    { "DESIGN SPRITE",          "10 DESIGN SPRITE 3\n20 FOR I=1 TO 24:@ \"1.2.3.4.5.6.7.8.\":NEXT I", "<gfx SPRITE DEF 3 s(384)=1,0,2,0,3,0,4,0,5,0,6,0,7,0,8,0>\n" },
    { "DESIGN TILE with a base", "10 DESIGN TILE 7,32\n20 FOR I=1 TO 24:@ \"o.A.F...........\":NEXT I", "<gfx TILE DEF 7 s(384)=33,0,42,0,47,0,0,0,0,0,0,0,0,0,0,0>\n" },
    { "DESIGN rows come one by one", "10 DESIGN CHAR 1\n20 @ \"oooooooo\"\n30 PRINT \"mid\"\n40 FOR I=1 TO 11:@ \"........\":NEXT I", "mid\n<gfx SYMBOL 1 s(12)=255,0,0,0,0,0,0,0,0,0,0,0>\n" },
    { "DESIGN row of the wrong length", "10 DESIGN CHAR 1\n20 @ \"abc\"",             "? Bad design row in line 20\n" },
    { "DESIGN row with a bad character", "10 DESIGN CHAR 1\n20 @ \"ooooooox\"",     "? Bad design row in line 20\n" },
    { "DESIGN base overflow",   "10 DESIGN SPRITE 1,250\n20 @ \"9...............\"", "? Bad design row in line 20\n" },
    { "@ row without DESIGN",   "@ \"........\"",                                 "? Design without DESIGN\n" },
    { "DESIGN needs a kind",    "DESIGN FROB 1",                                  "? SPRITE, TILE or CHAR expected\n" },
    { "an unfinished DESIGN does nothing", "10 DESIGN CHAR 1\n20 @ \"oooooooo\"\n30 PRINT \"x\"", "x\n" },
    { "ON COLLISION GOSUB",     "10 ON COLLISION GOSUB 100\n20 FOR I=1 TO 3000:NEXT I\n30 PRINT \"done\";C\n40 END\n100 C=C+1:RETURN", "done1\n" },
    { "ON COLLISION OFF",       "10 ON COLLISION GOSUB 100\n15 ON COLLISION OFF\n20 FOR I=1 TO 3000:NEXT I\n30 PRINT \"done\";C\n40 END\n100 C=C+1:RETURN", "done0\n" },
    { "ON COLLISION needs GOTO, GOSUB or OFF", "ON COLLISION", "? GOTO, GOSUB or OFF expected\n" },
    // numeric functions, \ and MOD, RND
    { "INT rounds down",        "PRINT INT(3.7);INT(-3.2);INT(5)",                "3-45\n" },
    { "FIX cuts toward zero",   "PRINT FIX(3.7);FIX(-3.7)",                       "3-3\n" },
    { "ABS",                    "PRINT ABS(-5);ABS(5);ABS(0)",                    "550\n" },
    { "SGN",                    "PRINT SGN(-2);SGN(0);SGN(9)",                    "-101\n" },
    { "SQR",                    "PRINT SQR(16);SQR(2);SQR(0)",                    "41.41421356237310\n" },
    { "SQR of a negative number", "PRINT SQR(-1)",                                "? Illegal argument\n" },
    { "SIN COS TAN",            "PRINT SIN(0);COS(0);TAN(0)",                     "010\n" },
    { "PI",                     "PRINT PI",                                       "3.14159265358979\n" },
    { "SIN of a quarter turn",  "PRINT SIN(PI/2);COS(PI)",                        "1-1\n" },
    { "SIN is in radians",      "PRINT SIN(1)",                                   "0.841470984807897\n" },
    { "ATN",                    "PRINT ATN(1)*4",                                 "3.14159265358979\n" },
    { "EXP and LOG",            "PRINT EXP(1);LOG(EXP(2))",                       "2.718281828459052\n" },
    { "LOG of zero",            "PRINT LOG(0)",                                   "? Illegal argument\n" },
    { "LOG of a negative number", "PRINT LOG(-1)",                                "? Illegal argument\n" },
    { "EXP overflows",          "PRINT EXP(1000)",                                "? Overflow\n" },
    { "functions nest",         "PRINT INT(SQR(ABS(-50))+0.5)",                   "7\n" },
    { "function without parenthesis", "PRINT SQR 4",                              "? Opening parenthesis '(' expected\n" },
    { "RND is in range",        "A=RND(1):PRINT A>=0 AND A<1",                    "-1\n" },
    { "RND without an argument", "A=RND:PRINT A>=0 AND A<1",                      "-1\n" },
    { "RND(0) repeats the last number", "A=RND(1):PRINT RND(0)=A",                "-1\n" },
    { "RND(1) goes on",         "A=RND(1):B=RND(1):PRINT A<>B",                   "-1\n" },
    { "RND with a negative argument starts the sequence over", "A=RND(-5):B=RND(1):C=RND(-5):D=RND(1):PRINT A=C AND B=D", "-1\n" },
    { "different seeds, different numbers", "A=RND(-5):B=RND(-6):PRINT A<>B",     "-1\n" },
    { "RANDOMIZE with a seed",  "RANDOMIZE 7:A=RND(1):B=RND(1):RANDOMIZE 7:PRINT RND(1)=A:PRINT RND(1)=B:PRINT A<>B", "-1\n-1\n-1\n" },
    { "RANDOMIZE without a seed", "RANDOMIZE:A=RND(1):PRINT A>=0 AND A<1",         "-1\n" },
    { "RND dice stay in 1..6",  "10 FOR I=1 TO 2000\n20 D=INT(RND(1)*6)+1:IF D<1 OR D>6 THEN PRINT \"bad\";D\n30 NEXT I\n40 PRINT \"ok\"", "ok\n" },
    { "RND is spread out",      "10 RANDOMIZE 3:FOR I=1 TO 2000:S=S+RND(1):NEXT I\n20 PRINT S>900 AND S<1100", "-1\n" },
    { "integer division",       "PRINT 7\\2;100\\7;1\\2",                         "3140\n" },
    { "integer division truncates toward zero", "PRINT (0-7)\\2;7\\(0-2)",           "-3-3\n" },
    { "integer division truncates the operands", "PRINT 7.9\\2.9",                  "3\n" },
    { "MOD",                    "PRINT 7 MOD 3;10 MOD 5;2 MOD 5",                 "102\n" },
    { "MOD has the sign of the dividend", "PRINT (0-7) MOD 3;7 MOD (0-3)",         "-11\n" },
    { "MOD truncates the operands", "PRINT 7.9 MOD 3.2",                          "1\n" },
    { "integer division by zero", "PRINT 5\\0",                                   "? Division by zero\n" },
    { "MOD by zero",            "PRINT 5 MOD 0",                                  "? Division by zero\n" },
    { "MOD by a fraction below one", "PRINT 5 MOD 0.5",                           "? Division by zero\n" },
    { "\\ and MOD are left to right like *", "PRINT 2*7\\2;17 MOD 5*2",            "74\n" },
    { "MOD in a condition",     "10 FOR I=1 TO 10\n20 IF I MOD 3=0 THEN PRINT I;\n30 NEXT I", "369" },
    // DEG, RAD, GRAD
    { "the default angle unit is radians", "PRINT SIN(PI/2);SIN(90)",             "10.893996663600558\n" },
    { "DEG: SIN",               "DEG:PRINT SIN(30);SIN(90);SIN(180);SIN(270);SIN(360)", "0.51" "0-10\n" },
    { "DEG: COS",               "DEG:PRINT COS(0);COS(60);COS(90);COS(180)",     "10.50-1\n" },
    { "DEG: TAN",               "DEG:PRINT TAN(45);TAN(0);TAN(180)",             "100\n" },
    { "DEG: TAN of 90 overflows", "DEG:PRINT TAN(90)",                            "? Overflow\n" },
    { "DEG: ATN",               "DEG:PRINT ATN(1);ATN(0)",                        "450\n" },
    { "DEG: negative and big angles", "DEG:PRINT SIN(0-90);COS(0-180);SIN(450);SIN(720)", "-1-110\n" },
    { "GRAD: quarter turns",    "GRAD:PRINT SIN(100);COS(100);COS(200);SIN(300);SIN(400)", "10-1-10\n" },
    { "GRAD: ATN",              "GRAD:PRINT ATN(1)",                              "50\n" },
    { "GRAD: SIN(50) is SIN(45 degrees)", "GRAD:A=SIN(50):RAD:PRINT ABS(A-SIN(PI/4))<0.000001", "-1\n" },
    { "RAD returns to radians", "DEG:RAD:PRINT SIN(PI/2)",                        "1\n" },
    { "the unit changes inside a program", "10 DEG:PRINT SIN(90):RAD:PRINT SIN(PI/2):GRAD:PRINT SIN(100)", "1\n1\n1\n" },
    { "angles of the graphics statements stay in degrees", "DEG:GFX RADIUS 100,100,50,90",     "<gfx GFX RADIUS 100 100 50 90 ->\n" },
    // IF ... THEN with an assignment (an identifier after THEN is a label only if nothing else follows it)
    { "IF THEN assignment",     "IF 1 THEN A=2:PRINT A",                          "2\n" },
    { "IF THEN assignment, ELSE assignment", "IF 0 THEN A=1 ELSE A=7:PRINT A",    "7\n" },
    { "IF THEN array assignment", "DIM A(3):IF 1 THEN A(2)=5:PRINT A(2)",         "5\n" },
    { "IF THEN assignment is skipped when false", "A=1:IF 0 THEN A=2:PRINT 9\nPRINT A", "1\n" },
    { "IF THEN label is still a jump", "10 IF 1 THEN FIN\n20 PRINT 1\n30 LABEL FIN\n40 PRINT 2", "2\n" },
    { "IF THEN label ELSE label", "10 IF 0 THEN ONE ELSE TWO\n20 LABEL ONE\n30 PRINT 1:END\n40 LABEL TWO\n50 PRINT 2", "2\n" },
    // number packing (big endian)
    { "MKI$ is two bytes, big endian", "A$=MKI$(258):PRINT LEN(A$);ASC(A$);ASC(MID$(A$,2,1))", "212\n" },
    { "MKL$ is four bytes, big endian", "A$=MKL$(16909060):PRINT LEN(A$);ASC(A$);ASC(MID$(A$,2,1));ASC(MID$(A$,3,1));ASC(RIGHT$(A$,1))", "41234\n" },
    { "MKS$ is an IEEE single, big endian", "A$=MKS$(1.5):PRINT LEN(A$);ASC(A$);ASC(MID$(A$,2,1));ASC(MID$(A$,3,1));ASC(MID$(A$,4,1))", "463192" "00\n" },
    { "MKD$ is an IEEE double, big endian", "A$=MKD$(1):PRINT LEN(A$);ASC(A$);ASC(MID$(A$,2,1));ASC(RIGHT$(A$,1))", "863240" "0\n" },
    { "CVI of MKI$", "PRINT CVI(MKI$(-2));CVI(MKI$(1234));CVI(MKI$(-32768))", "-21234-32768\n" },
    { "MKI$ takes unsigned 16 bit values, CVI is signed", "PRINT CVI(MKI$(65535));CVI(MKI$(65535)) AND 65535;CVI(MKI$(32768))", "-165535-32768\n" },
    { "CVL of MKL$", "PRINT CVL(MKL$(-100000));CVL(MKL$(123456789));CVL(MKL$(4294967295))", "-100000123456789-1\n" },
    { "CVS of MKS$ (single precision)", "PRINT CVS(MKS$(0.5));CVS(MKS$(0.1))", "0.50.100000001490116\n" },
    { "CVD of MKD$ (double precision)", "PRINT CVD(MKD$(3.14159265358979));CVD(MKD$(-0.001))", "3.14159265358979-0.001\n" },
    { "extra bytes are ignored", "PRINT CVI(MKI$(5)+\"xyz\");CVL(MKL$(7)+CHR$(9))", "57\n" },
    { "CVI of a short string", "PRINT CVI(\"a\")", "? Illegal argument\n" },
    { "CVD of a short string", "PRINT CVD(MKL$(1))", "? Illegal argument\n" },
    { "MKI$ out of range", "PRINT LEN(MKI$(70000))", "? Illegal argument\n" },
    { "MKI$ negative out of range", "PRINT LEN(MKI$(-40000))", "? Illegal argument\n" },
    { "MKL$ out of range", "PRINT LEN(MKL$(5000000000))", "? Illegal argument\n" },
    { "MKI$ needs a sigil", "PRINT MKI(1)", "? String sigil '$' expected\n" },
    { "packed numbers in a random record", "10 OPEN \"mem:r\" FOR RANDOM AS #1 LEN=10\n20 FIELD #1,2 AS A$,8 AS B$\n30 LSET A$=MKI$(300):LSET B$=MKD$(2.5):PUTREC #1,1\n40 LSET A$=\"\":LSET B$=\"\":GETREC #1,1\n50 PRINT CVI(A$);CVD(B$)", "3002.5\n" },
    { "packed numbers through a file", "10 OPEN \"mem:p\" FOR OUTPUT AS #1:PRINT #1,MKI$(258);MKL$(-1);:CLOSE\n20 OPEN \"mem:p\" FOR INPUT AS #1:A$=INPUT$(2,#1):B$=INPUT$(4,#1):PRINT CVI(A$);CVL(B$);LOF(1)", "258-16\n" },
    { "packed numbers in a loop", "FOR I=-3 TO 3:PRINT CVI(MKI$(I*1000));:NEXT I", "-3000-2000-10000100020003000" },
    // binary strings: zero bytes are characters
    { "LEN of a zero byte",     "PRINT LEN(CHR$(0))",                             "1\n" },
    { "ASC of a zero byte",     "PRINT ASC(CHR$(0))",                             "0\n" },
    { "zero bytes in a string", "A$=\"a\"+CHR$(0)+\"b\":PRINT LEN(A$);ASC(MID$(A$,2,1));ASC(RIGHT$(A$,1))", "3098\n" },
    { "zero bytes are printed", "PRINT \"a\";CHR$(0);\"b\"",                        "a<0>b\n" },
    { "a zero byte sorts first", "PRINT (\"a\"+CHR$(0)>\"a\");(\"a\"<\"a\"+CHR$(0));(\"a\"+CHR$(0)=\"a\")", "-1-10\n" },
    { "compare behind a zero byte", "PRINT (\"a\"+CHR$(0)+\"b\"<\"a\"+CHR$(0)+\"c\");(\"a\"+CHR$(0)+\"b\"=\"a\"+CHR$(0)+\"b\")", "-1-1\n" },
    { "bytes above 127 compare as unsigned", "PRINT (CHR$(200)>CHR$(100))", "-1\n" },
    { "substrings across a zero byte", "A$=\"x\"+CHR$(0)+\"y\"+CHR$(0)+\"z\":PRINT LEN(LEFT$(A$,4));LEN(MID$(A$,3));ASC(MID$(A$,4,1))", "430\n" },
    { "substring assignment with a zero byte", "A$=\"abcde\":MID$(A$,2,2)=CHR$(0)+CHR$(1):PRINT LEN(A$);ASC(MID$(A$,2,1));ASC(MID$(A$,3,1))", "501\n" },
    { "LEFT$ assignment from a binary string", "A$=\"abcde\":LEFT$(A$,3)=\"x\"+CHR$(0)+\"y\":PRINT LEN(A$);ASC(MID$(A$,2,1));MID$(A$,3,1)", "50y\n" },
    { "zero bytes in an array element", "DIM A$(2):A$(1)=\"p\"+CHR$(0)+\"q\":PRINT LEN(A$(1));ASC(MID$(A$(1),2,1))", "30\n" },
    { "zero bytes through a function", "DEF FN L(S$)=LEN(S$)\nPRINT FN L(\"a\"+CHR$(0)+\"b\"+CHR$(0))", "4\n" },
    { "STR$ and VAL still work", "PRINT VAL(\"12\"+CHR$(0)+\"34\");LEN(STR$(5)+CHR$(0))", "122\n" },
    { "a zero byte in a name is refused", "OPEN \"mem:a\"+CHR$(0)+\"b\" FOR OUTPUT AS #1", "? Illegal argument (zero byte in string)\n" },
    { "a zero byte in a PLAY string is refused", "PLAY \"C\"+CHR$(0)", "? Illegal argument (zero byte in string)\n" },
    { "a binary file round trip: LINE INPUT", "10 OPEN \"mem:b\" FOR OUTPUT AS #1:PRINT #1,\"x\";CHR$(0);\"y\":CLOSE\n20 OPEN \"mem:b\" FOR INPUT AS #1:LINE INPUT #1,A$:PRINT LEN(A$);ASC(MID$(A$,2,1))\n30 PRINT LOF(1)", "30\n4\n" },
    { "a binary file round trip: INPUT$ and GET", "10 OPEN \"mem:b\" FOR OUTPUT AS #1:PRINT #1,CHR$(0);CHR$(1);CHR$(0);:CLOSE\n20 OPEN \"mem:b\" FOR INPUT AS #1:A$=INPUT$(2,#1):GET #1,B$:PRINT LEN(A$);ASC(A$);ASC(MID$(A$,2,1));LEN(B$);ASC(B$)", "20110\n" },
    { "INPUT # reads a binary field", "10 OPEN \"mem:b\" FOR OUTPUT AS #1:PRINT #1,CHR$(34);\"a\";CHR$(0);\"b\";CHR$(34);\",7\":CLOSE\n20 OPEN \"mem:b\" FOR INPUT AS #1:INPUT #1,A$,N:PRINT LEN(A$);N", "37\n" },
    { "a zero byte in a number field is bad data", "10 OPEN \"mem:b\" FOR OUTPUT AS #1:PRINT #1,\"1\";CHR$(0);\"2\":CLOSE\n20 OPEN \"mem:b\" FOR INPUT AS #1:INPUT #1,N", "? Bad data in file in line 20\n" },
    { "WRITE with a binary string", "10 OPEN \"mem:b\" FOR OUTPUT AS #1:WRITE #1,\"a\"+CHR$(0):CLOSE\n20 OPEN \"mem:b\" FOR INPUT AS #1:PRINT LOF(1)", "5\n" },
    { "random record with zero bytes", "10 OPEN \"mem:r\" FOR RANDOM AS #1 LEN=5\n20 FIELD #1,5 AS A$\n30 LSET A$=\"a\"+CHR$(0)+\"b\":PUTREC #1,1\n40 LSET A$=\"zzzzz\":GETREC #1,1\n50 PRINT LEN(A$);ASC(MID$(A$,2,1));ASC(MID$(A$,3,1));ASC(MID$(A$,4,1))", "509832\n" },
    { "CHR$ too large",         "PRINT CHR$(256)",                                "? Illegal argument\n" },
    { "CHR$ negative",          "PRINT CHR$(0-1)",                                "? Illegal argument\n" },
    { "CHR$ needs a sigil",     "PRINT CHR(65)",                                  "? String sigil '$' expected\n" },
    { "INKEY$ without input",   "PRINT LEN(INKEY$)",                              "0\n" },
    { "nested functions",       "PRINT LEFT$(STR$(12345),3)",                     "123\n" },
    { "function in expression", "PRINT LEN(HEX$(255,6))+ASC(\"A\")",               "71\n" },
    { "string function in DEF FN", "DEF FN A$(S$)=LEFT$(S$,1)\nPRINT FN A$(\"XYZ\")", "X\n" },
    { "functions in a loop",    "FOR I=1 TO 3:PRINT MID$(\"ABC\",I,1):NEXT I",     "A\nB\nC\n" },
    // SHEX and SAVE
    { "shell literal",          "PRINT `echo hi`",                                "hi\n" },
    { "shell result in variable", "A$=`printf abc`\nPRINT LEN(A$);A$",           "3abc\n" },
    { "shell strips newlines",  "PRINT \"[\"+`printf 'x\\n\\n'`+\"]\"",          "[x]\n" },
    { "shell no output",        "PRINT LEN(`true`)",                              "0\n" },
    { "shell after assignment", "C$=\"echo \"+\"ok\"\nPRINT `echo ok`",       "ok\n" },
    { "SAVE binary default",    "10 PRINT 1\n20 SAVE \"X\"",                       "1\n<save X binary>\n89 45 39 39 01 00 00 0A 00 0A 20 0B 20 31 00 00 00 14 00 0D 20 14 20 03 00 01 58 00 \n" },
    { "SAVE ascii",             "10 PRINT 1\n20 SAVE \"X\",A",                     "1\n<save X ascii>\n10 PRINT 1\n20 SAVE \"X\",A\n" },
    { "SAVE binary option",     "10 PRINT 1\n20 SAVE \"X\",B",                     "1\n<save X binary>\n89 45 39 39 01 00 00 0A 00 0A 20 0B 20 31 00 00 00 14 00 12 20 14 20 03 00 01 58 2C 8A 00 01 42 00 \n" },
    // LOAD, RUN, LIST (the VM only leaves requests / calls the host)
    { "RUN",                    "RUN",                                             "<request RUN \"\" -1>\n" },
    { "RUN line",               "RUN 100",                                         "<request RUN \"\" 100>\n" },
    { "RUN label",              "RUN START",                                       "<request RUN \"START\" -1>\n" },
    { "RUN ends the run",       "PRINT 1:RUN 20:PRINT 2",                          "1\n<request RUN \"\" 20>\n" },
    { "RUN in IF",              "IF 1 THEN RUN 5",                                 "<request RUN \"\" 5>\n" },
    { "RUN bad line number",    "RUN 1000000",                                     "? Invalid line number\n" },
    { "LOAD",                   "LOAD \"PROG\"",                                   "<request LOAD PROG>\n" },
    { "LOAD name expression",   "N$=\"AB\"\nLOAD N$+\"C\"",                          "<request LOAD ABC>\n" },
    { "LOAD ends the run",      "PRINT 1:LOAD \"X\":PRINT 2",                      "1\n<request LOAD X>\n" },
    { "LOAD empty name",        "LOAD \"\"",                                       "? Illegal argument\n" },
    { "LOAD without name",      "LOAD",                                            "? String expression expected\n" },
    { "EDIT line",              "EDIT 100",                                        "<request EDIT \"\" 100>\n" },
    { "EDIT label",             "EDIT START",                                      "<request EDIT \"START\" -1>\n" },
    { "EDIT without target",    "EDIT",                                            "<request EDIT \"\" -1>\n" },
    { "EDIT ends the run",      "PRINT 1:EDIT 20:PRINT 2",                         "1\n<request EDIT \"\" 20>\n" },
    { "MONITOR",                "MONITOR",                                         "<request MONITOR>\n" },
    { "MONITOR ends the run",   "PRINT 1:MONITOR:PRINT 2",                         "1\n<request MONITOR>\n" },
    { "MONITOR lower case",     "monitor",                                         "<request MONITOR>\n" },
    // the rest of the line after a jump target belongs to the THEN branch
    { "IF THEN GOSUB: colon statements, false", "10 A=0\n20 IF A THEN GOSUB 100:PRINT \"x\"\n30 PRINT \"after\"\n40 END\n100 PRINT \"sub\":RETURN", "after\n" },
    { "IF THEN GOSUB: colon statements, true", "10 A=1\n20 IF A THEN GOSUB 100:PRINT \"x\"\n30 PRINT \"after\"\n40 END\n100 PRINT \"sub\":RETURN", "sub\nx\nafter\n" },
    { "IF GOSUB without THEN: colon statements", "10 A=0\n20 IF A GOSUB 100:PRINT \"x\"\n30 A=1\n40 IF A GOSUB 100:PRINT \"y\"\n50 END\n100 PRINT \"sub\":RETURN", "sub\ny\n" },
    { "IF THEN GOTO: colon statements, false", "10 A=0\n20 IF A THEN GOTO 50:PRINT \"x\"\n30 PRINT \"after\"\n40 END\n50 PRINT \"jumped\"", "after\n" },
    { "IF THEN GOTO: colon statements, true", "10 A=1\n20 IF A THEN GOTO 50:PRINT \"x\"\n30 PRINT \"after\"\n40 END\n50 PRINT \"jumped\"", "jumped\n" },
    { "IF THEN line number: colon statements, false", "10 A=0\n20 IF A THEN 50:PRINT \"x\"\n30 PRINT \"after\"\n40 END\n50 PRINT \"jumped\"", "after\n" },
    { "IF THEN GOSUB: several statements after the colon", "10 A=1\n20 IF A THEN GOSUB 100:PRINT 1:PRINT 2\n30 END\n100 RETURN", "1\n2\n" },
    { "IF THEN GOSUB: colon statements and ELSE, true", "10 A=1\n20 IF A THEN GOSUB 100:PRINT \"x\" ELSE PRINT \"e\"\n30 END\n100 PRINT \"sub\":RETURN", "sub\nx\n" },
    { "IF THEN GOSUB: colon statements and ELSE, false", "10 A=0\n20 IF A THEN GOSUB 100:PRINT \"x\" ELSE PRINT \"e\"\n30 END\n100 PRINT \"sub\":RETURN", "e\n" },
    { "UNLESS THEN GOSUB: colon statements", "10 A=1\n20 UNLESS A THEN GOSUB 100:PRINT \"x\"\n30 PRINT \"after\"\n40 END\n100 PRINT \"sub\":RETURN", "after\n" },
    { "IF THEN GOSUB: only a comment after the colon", "10 A=1\n20 IF A THEN GOSUB 100: 'note\n30 PRINT \"after\"\n40 END\n100 PRINT \"sub\":RETURN", "sub\nafter\n" },
    { "IF THEN GOSUB, no colon: unchanged", "10 A=1\n20 IF A THEN GOSUB 100\n30 PRINT \"after\"\n40 END\n100 PRINT \"sub\":RETURN", "sub\nafter\n" },
    { "IF THEN line ELSE line: unchanged", "10 A=0\n20 IF A THEN 50 ELSE 60\n50 PRINT \"t\":END\n60 PRINT \"e\"", "e\n" },
    // WAIT
    { "WAIT 0",                 "WAIT 0:PRINT \"ok\"",                             "ok\n" },
    { "WAIT sleeps",            "T=TI:WAIT 6:PRINT TI-T>=5",                       "-1\n" },
    { "WAIT with an expression", "N=2:WAIT N*3:PRINT \"ok\"",                      "ok\n" },
    { "WAIT lower case",        "wait 1:print \"ok\"",                             "ok\n" },
    { "WAIT negative",          "WAIT 0-1",                                        "? Illegal argument\n" },
    { "WAIT too long",          "WAIT 216001",                                     "? Illegal argument\n" },
    { "WAIT needs a number",    "WAIT",                                            "? Numeric expression expected\n" },
    { "WAIT in a program",      "10 FOR I=1 TO 3\n20 WAIT 1\n30 PRINT I\n40 NEXT I", "1\n2\n3\n" },
    // sound (the test sound output logs: <tone chan freq vol beats>, channels 0..7 there)
    { "SOUND",                  "SOUND 1,440,2",                                   "<tone 0 440 0.5 2>\n" },
    { "SOUND with volume",      "SOUND 2,880,1,0.25",                              "<tone 1 880 0.25 1>\n" },
    { "SOUND rest",             "SOUND 3,0,1.5",                                   "<tone 2 0 0 1.5>\n" },
    { "SOUND without end",      "SOUND 1,440,0",                                   "<tone 0 440 0.5 0>\n" },
    { "SOUND with expressions", "A=3:SOUND A,110*4,A-2",                           "<tone 2 440 0.5 1>\n" },
    { "SOUND lower case",       "sound 8,20,1",                                    "<tone 7 20 0.5 1>\n" },
    { "SOUND channel 0",        "SOUND 0,440,1",                                   "? Illegal argument\n" },
    { "SOUND channel 9",        "SOUND 9,440,1",                                   "? Illegal argument\n" },
    { "SOUND frequency low",    "SOUND 1,19,1",                                    "? Illegal argument\n" },
    { "SOUND frequency high",   "SOUND 1,20001,1",                                 "? Illegal argument\n" },
    { "SOUND volume",           "SOUND 1,440,1,1.5",                               "? Illegal argument\n" },
    { "SOUND negative beats",   "SOUND 1,440,0-1",                                 "? Illegal argument\n" },
    { "SOUND needs 3 arguments", "SOUND 1,440",                                    "? Comma ',' expected\n" },
    { "SOUND in a program",     "10 FOR C=1 TO 2\n20 SOUND C,100*C,1\n30 NEXT C", "<tone 0 100 0.5 1>\n<tone 1 200 0.5 1>\n" },
    { "WAVE",                   "WAVE 2,1",                                        "<wave 1 1>\n" },
    { "WAVE noise",             "WAVE 1,3",                                        "<wave 0 3>\n" },
    { "WAVE bad type",          "WAVE 1,4",                                        "? Illegal argument\n" },
    { "WAVE bad channel",       "WAVE 0,1",                                        "? Illegal argument\n" },
    { "TEMPO",                  "TEMPO 120",                                       "<bpm 120>\n" },
    { "TEMPO 0",                "TEMPO 0",                                         "? Illegal argument\n" },
    { "TEMPO 1001",             "TEMPO 1001",                                      "? Illegal argument\n" },
    { "SOUND OFF",              "SOUND OFF",                                       "<silence -1>\n" },
    { "SOUND OFF channel",      "SOUND OFF 3",                                     "<silence 2>\n" },
    { "SOUND OFF 0 is all",     "SOUND OFF 0",                                     "<silence -1>\n" },
    { "SOUND OFF bad channel",  "SOUND OFF 9",                                     "? Illegal argument\n" },
    { "SOUND SYNC",             "SOUND SYNC 1,2,3",                                "<sync 7>\n" },
    { "SOUND SYNC one",         "SOUND SYNC 4",                                    "<sync 8>\n" },
    { "SOUND SYNC bad channel", "SOUND SYNC 1,9",                                  "? Illegal argument\n" },
    { "PLAYING",                "SOUND 1,440,1:PRINT PLAYING(1):PRINT PLAYING(2)", "<tone 0 440 0.5 1>\n-1\n0\n" },
    { "PLAYING after SOUND OFF", "SOUND 1,440,1:SOUND OFF:PRINT PLAYING(1)",       "<tone 0 440 0.5 1>\n<silence -1>\n0\n" },
    { "PLAYING in a condition", "SOUND 5,440,1\nIF PLAYING(5) THEN PRINT \"BUSY\"", "<tone 4 440 0.5 1>\nBUSY\n" },
    { "PLAYING bad channel",    "PRINT PLAYING(0)",                                "? Illegal argument\n" },
    { "PLAYING lower case",     "print playing(3)",                                "0\n" },
    { "PLAY",                   "PLAY \"C\"",                                      "<sync 1>\n<tone 0 261.626 0.5 0.4375>\n<tone 0 0 0 0.0625>\n<tone 0 0 0 0>\n" },
    { "PLAY legato",            "PLAY \"ML C\"",                                   "<sync 1>\n<tone 0 261.626 0.5 0.5>\n<tone 0 0 0 0>\n" },
    { "PLAY two voices",        "PLAY \"ML R\",\"ML R2\"",                         "<sync 3>\n<tone 0 0 0 0.5>\n<tone 0 0 0 0>\n<tone 1 0 0 1>\n<tone 1 0 0 0>\n" },
    { "PLAY wave and volume",   "PLAY \"W1 V0.25 ML A\"",                          "<sync 1>\n<wave 0 1>\n<tone 0 440 0.25 0.5>\n<tone 0 0 0 0>\n" },
    { "PLAY follows TEMPO",     "TEMPO 120:PLAY \"ML R\"",                         "<bpm 120>\n<sync 1>\n<tone 0 0 0 1>\n<tone 0 0 0 0>\n" },
    { "PLAY string expression", "A$=\"ML R\":PLAY A$+\"4\"",                       "<sync 1>\n<tone 0 0 0 0.5>\n<tone 0 0 0 0>\n" },
    { "PLAY error queues nothing", "PLAY \"C\",\"Z\"",                              "? Unknown music command 'Z'\n" },
    { "PLAY note out of range", "PLAY \"O0 C\"",                                   "? Note out of range\n" },
    { "PLAY too many voices",   "PLAY \"R\",\"R\",\"R\",\"R\",\"R\",\"R\",\"R\",\"R\",\"R\"", "? End of line expected\n" },
    { "PLAY needs a string",    "PLAY 5",                                          "? String expression expected\n" },
    // sound effects
    { "SOUND glide",            "SOUND 1,2000,0.5,0.6,200",                        "<tone 0 2000 0.6 0.5 glide 200>\n" },
    { "SOUND glide, default volume", "SOUND 2,100,1,0.5,400",                      "<tone 1 100 0.5 1 glide 400>\n" },
    { "SOUND glide needs a duration", "SOUND 1,2000,0,0.6,200",                    "? Illegal argument\n" },
    { "SOUND glide end frequency", "SOUND 1,2000,1,0.5,10",                        "? Illegal argument\n" },
    { "SOUND glide on a rest is ignored", "SOUND 1,0,1,0.5,200",                   "<tone 0 0 0 1>\n" },
    { "PITCH",                  "PITCH 3,880",                                     "<pitch 2 880>\n" },
    { "PITCH range",            "PITCH 3,19",                                      "? Illegal argument\n" },
    { "PITCH channel",          "PITCH 0,440",                                     "? Illegal argument\n" },
    { "VOLUME",                 "VOLUME 1,0.25",                                   "<volume 0 0.25>\n" },
    { "VOLUME 0",               "VOLUME 8,0",                                      "<volume 7 0>\n" },
    { "VOLUME range",           "VOLUME 1,1.1",                                    "? Illegal argument\n" },
    { "PITCH and VOLUME in a loop", "10 FOR I=1 TO 3\n20 PITCH 1,100*I\n30 VOLUME 1,I/4\n40 NEXT I", "<pitch 0 100>\n<volume 0 0.25>\n<pitch 0 200>\n<volume 0 0.5>\n<pitch 0 300>\n<volume 0 0.75>\n" },
    { "ENVELOPE",               "ENVELOPE 1,0.01,0.1,0.5,0.3",                     "<env 0 0.01 0.1 0.5 0.3>\n" },
    { "ENVELOPE default",       "ENVELOPE 2",                                      "<env 1 off>\n" },
    { "ENVELOPE sustain range", "ENVELOPE 1,0,0,1.5,0",                            "? Illegal argument\n" },
    { "ENVELOPE time range",    "ENVELOPE 1,61,0,1,0",                             "? Illegal argument\n" },
    { "ENVELOPE with 3 values", "ENVELOPE 1,0,0",                                  "? Comma ',' expected\n" },
    { "VIBRATO",                "VIBRATO 1,5,0.5",                                 "<vib 0 5 0.5>\n" },
    { "VIBRATO off",            "VIBRATO 4,0,0",                                   "<vib 3 0 0>\n" },
    { "VIBRATO depth",          "VIBRATO 1,5,25",                                  "? Illegal argument\n" },
    { "TREMOLO",                "TREMOLO 1,8,0.75",                                "<trem 0 8 0.75>\n" },
    { "TREMOLO depth",          "TREMOLO 1,8,2",                                   "? Illegal argument\n" },
    { "PLAY with H and S",      "PLAY \"ML H1000 S2000,200,2\"",                     "<sync 1>\n<tone 0 1000 0.5 0.5>\n<tone 0 2000 0.5 1 glide 200>\n<tone 0 0 0 0>\n" },
    { "PLAY H with a length",   "PLAY \"ML H440,8\"",                              "<sync 1>\n<tone 0 440 0.5 0.25>\n<tone 0 0 0 0>\n" },
    { "PLAY effect commands",   "PLAY \"X0.01,0.1,0.5,0.3 ~5,0.5 ^8,0.25 ML C XR ~0,0\"", "<sync 1>\n<env 0 0.01 0.1 0.5 0.3>\n<vib 0 5 0.5>\n<trem 0 8 0.25>\n<tone 0 261.626 0.5 0.5>\n<env 0 off>\n<vib 0 0 0>\n<tone 0 0 0 0>\n" },
    { "PLAY effect command errors", "PLAY \"X1,2\"",                                "? Value missing or out of range after 'X'\n" },
    { "PLAY H frequency range", "PLAY \"H10\"",                                    "? Value missing or out of range after 'H'\n" },
    { "SFX define and play",    "SFX 1,\"ML S1000,100\":SFX 1,3",                  "<sync 4>\n<wave 2 0>\n<env 2 off>\n<vib 2 0 0>\n<trem 2 0 0>\n<tone 2 1000 0.5 0.5 glide 100>\n<tone 2 0 0 0>\n" },
    { "SFX without a channel uses the highest free one", "SFX 2,\"ML R\":SFX 2", "<sync 128>\n<wave 7 0>\n<env 7 off>\n<vib 7 0 0>\n<trem 7 0 0>\n<tone 7 0 0 0.5>\n<tone 7 0 0 0>\n" },
    { "SFX skips busy channels", "SOUND 8,440,1:SFX 2,\"ML R\":SFX 2", "<tone 7 440 0.5 1>\n<sync 64>\n<wave 6 0>\n<env 6 off>\n<vib 6 0 0>\n<trem 6 0 0>\n<tone 6 0 0 0.5>\n<tone 6 0 0 0>\n" },
    { "SFX takes back its wave", "SFX 1,\"W1 ML R\":SFX 1,1", "<sync 1>\n<wave 0 0>\n<env 0 off>\n<vib 0 0 0>\n<trem 0 0 0>\n<wave 0 1>\n<tone 0 0 0 0.5>\n<tone 0 0 0 0>\n<wave 0 0>\n" },
    { "SFX takes back vibrato and tremolo", "SFX 1,\"ML ~5,1 ^4,0.5 R\":SFX 1,1", "<sync 1>\n<wave 0 0>\n<env 0 off>\n<vib 0 0 0>\n<trem 0 0 0>\n<vib 0 5 1>\n<trem 0 4 0.5>\n<tone 0 0 0 0.5>\n<tone 0 0 0 0>\n<vib 0 0 0>\n<trem 0 0 0>\n" },
    { "SFX waits for the release before it takes back the envelope", "SFX 1,\"ML X0,0.1,0,0.5 R\":SFX 1,1", "<sync 1>\n<wave 0 0>\n<env 0 off>\n<vib 0 0 0>\n<trem 0 0 0>\n<env 0 0 0.1 0 0.5>\n<tone 0 0 0 0.5>\n<tone 0 0 0 0>\n<tone 0 0 0 0.5>\n<env 0 off>\n" },
    { "SFX redefined",          "SFX 1,\"ML R\":SFX 1,\"ML R2\":SFX 1,1",         "<sync 1>\n<wave 0 0>\n<env 0 off>\n<vib 0 0 0>\n<trem 0 0 0>\n<tone 0 0 0 1>\n<tone 0 0 0 0>\n" },
    { "SFX not defined",        "SFX 5,1",                                         "? Undefined effect\n" },
    { "SFX number range",       "SFX 65,\"C\"",                                   "? Illegal argument\n" },
    { "SFX number 0",           "SFX 0,\"C\"",                                    "? Illegal argument\n" },
    { "SFX channel range",      "SFX 1,\"C\":SFX 1,9",                            "? Illegal argument\n" },
    { "SFX string is checked when defined", "SFX 1,\"Z\"",                         "? Unknown music command 'Z'\n" },
    { "SFX with a string expression", "A$=\"ML \":SFX 1,A$+\"R\":SFX 1,2",         "<sync 2>\n<wave 1 0>\n<env 1 off>\n<vib 1 0 0>\n<trem 1 0 0>\n<tone 1 0 0 0.5>\n<tone 1 0 0 0>\n" },
    { "SFX empty string",       "SFX 1,\"\":SFX 1,1",                            "<sync 1>\n<wave 0 0>\n<env 0 off>\n<vib 0 0 0>\n<trem 0 0 0>\n<tone 0 0 0 0>\n" },
    { "EDIT bad line number",   "EDIT 1000000",                                    "? Invalid line number\n" },
    { "NEW",                    "NEW",                                             "<request NEW>\n" },
    { "NEW ends the run",       "PRINT 1:NEW:PRINT 2",                             "1\n<request NEW>\n" },
    { "CLEAR numbers",          "A=5:N%=7:CLEAR:PRINT A;N%",                        "00\n" },
    { "CLEAR strings",          "A$=\"X\":CLEAR:PRINT LEN(A$)",                     "0\n" },
    { "CLEAR arrays",           "DIM B(2),S$(2):B(1)=7:S$(1)=\"Q\":CLEAR:PRINT B(1);LEN(S$(1))", "00\n" },
    { "CLEAR keeps array dimensions", "DIM B(2):CLEAR:B(1)=4:PRINT B(1)",           "4\n" },
    { "CLEAR keeps functions",  "DEF FN A(X)=X*2\nCLEAR\nPRINT FN A(4)",             "8\n" },
    { "CLEAR keeps labels",     "10 K=1\n20 CLEAR\n30 GOTO M\n40 PRINT 4\n50 LABEL M\n60 PRINT K",  "0\n" },
    { "CLEAR goes on",          "A=1:CLEAR:PRINT 2",                               "2\n" },
    { "CLEAR in a FOR loop's body keeps the loop data", "FOR I=1 TO 3:S=S+1:NEXT:CLEAR:PRINT S;I", "00\n" },
    { "DELETE line",            "DELETE 20",                                       "<delete 20 20>\n" },
    { "DELETE range",           "DELETE 10-30",                                    "<delete 10 30>\n" },
    { "DELETE up to",           "DELETE -30",                                      "<delete * 30>\n" },
    { "DELETE from",            "DELETE 10-",                                      "<delete 10 *>\n" },
    { "DELETE labels",          "DELETE A-B",                                      "<delete A B>\n" },
    { "DELETE ends the run",    "PRINT 1:DELETE 5:PRINT 2",                        "1\n<delete 5 5>\n" },
    { "DELETE needs a target",  "DELETE",                                          "? Line number or label expected\n" },
    { "DELETE minus alone",     "DELETE -",                                        "? Line number or label expected\n" },
    { "MERGE",                  "MERGE \"PROG\"",                                  "<merge PROG>\n" },
    { "MERGE APPEND",           "MERGE \"PROG\",APPEND",                           "<merge PROG append>\n" },
    { "MERGE name expression",  "N$=\"AB\"\nMERGE N$+\"C\"",                        "<merge ABC>\n" },
    { "MERGE ends the run",     "PRINT 1:MERGE \"X\":PRINT 2",                     "1\n<merge X>\n" },
    { "MERGE empty name",       "MERGE \"\"",                                      "? Illegal argument\n" },
    { "MERGE only APPEND is an option", "MERGE \"X\",FOO",                         "? APPEND expected\n" },
    { "MERGE without name",     "MERGE",                                           "? String expression expected\n" },
    // AFTER / EVERY (real time: a tick is 1/60 s; WAIT gives way to the handlers)
    { "AFTER GOSUB during WAIT", "10 AFTER 3 GOSUB 100\n20 WAIT 12\n30 PRINT \"end\":END\n100 PRINT \"fired\":RETURN", "fired\nend\n" },
    { "AFTER fires once", "10 AFTER 2 GOSUB 100\n20 WAIT 30\n30 PRINT N:END\n100 N=N+1:RETURN", "1\n" },
    { "AFTER in the middle of a busy loop", "10 AFTER 1 GOSUB 100\n20 WHILE F=0:WEND\n30 PRINT \"ok\":END\n100 F=1:RETURN", "ok\n" },
    { "the WAIT time is kept when a handler runs", "10 T=TI:AFTER 3 GOSUB 100\n20 WAIT 12\n30 PRINT TI-T>=11 AND TI-T<=16:END\n100 RETURN", "-1\n" },
    { "EVERY repeats", "10 EVERY 6 GOSUB 100\n20 WAIT 40\n30 EVERY OFF\n40 PRINT N>=3 AND N<=7:END\n100 N=N+1:RETURN", "-1\n" },
    { "AFTER OFF", "10 AFTER 3 GOSUB 100\n20 AFTER OFF\n30 WAIT 12\n40 PRINT N:END\n100 N=N+1:RETURN", "0\n" },
    { "EVERY OFF", "10 EVERY 3 GOSUB 100\n20 EVERY OFF\n30 WAIT 12\n40 PRINT N:END\n100 N=N+1:RETURN", "0\n" },
    { "AFTER OFF leaves EVERY alone", "10 EVERY 3,1 GOSUB 100\n20 AFTER OFF\n30 WAIT 12\n40 EVERY OFF\n50 PRINT N>=1:END\n100 N=N+1:RETURN", "-1\n" },
    { "EVERY OFF leaves AFTER alone", "10 AFTER 3,1 GOSUB 100\n20 EVERY OFF\n30 WAIT 12\n40 PRINT N:END\n100 N=N+1:RETURN", "1\n" },
    { "AFTER OFF with a timer number", "10 AFTER 3,1 GOSUB 100\n20 AFTER 3,2 GOSUB 200\n30 AFTER OFF 1\n40 WAIT 12\n50 PRINT N;M:END\n100 N=N+1:RETURN\n200 M=M+1:RETURN", "01\n" },
    { "AFTER GOTO", "10 AFTER 3 GOTO 100\n20 WAIT 12\n30 PRINT \"no\":END\n100 PRINT \"jumped\":END", "jumped\n" },
    { "AFTER GO SUB and a label", "10 AFTER 3 GO SUB HANDLER\n20 WAIT 12\n30 PRINT \"end\":END\n40 LABEL HANDLER\n50 PRINT \"fired\":RETURN", "fired\nend\n" },
    { "setting a timer again replaces it", "10 AFTER 3,1 GOSUB 100\n20 AFTER 3,1 GOSUB 200\n30 WAIT 12\n40 END\n100 PRINT \"first\":RETURN\n200 PRINT \"second\":RETURN", "second\n" },
    { "a higher timer interrupts a handler", "10 AFTER 2,0 GOSUB 100\n20 AFTER 6,3 GOSUB 200\n30 WAIT 40\n40 END\n100 PRINT \"A start\":WAIT 20:PRINT \"A end\":RETURN\n200 PRINT \"B\":RETURN", "A start\nB\nA end\n" },
    { "a lower timer waits for the handler", "10 AFTER 2,3 GOSUB 100\n20 AFTER 6,0 GOSUB 200\n30 WAIT 40\n40 END\n100 PRINT \"A start\":WAIT 20:PRINT \"A end\":RETURN\n200 PRINT \"B\":RETURN", "A start\nA end\nB\n" },
    { "a handler returns to where the program was", "10 AFTER 14 GOSUB 100\n20 FOR I=1 TO 3:WAIT 5:PRINT I:NEXT\n30 END\n100 PRINT \"H\":RETURN", "1\n2\nH\n3\n" },
    { "AFTER delay and timer are expressions", "10 D=3:T=2\n20 AFTER D*1,T-1 GOSUB 100\n30 WAIT 12\n40 END\n100 PRINT \"ok\":RETURN", "ok\n" },
    { "timers end with the program", "10 AFTER 5 GOSUB 100\n20 PRINT \"x\":END\n100 PRINT \"never\":RETURN", "x\n" },
    { "AFTER delay zero", "10 AFTER 0 GOSUB 100\n20 END\n100 RETURN", "? Illegal argument in line 10\n" },
    { "AFTER delay negative", "10 AFTER -1 GOSUB 100\n20 END\n100 RETURN", "? Illegal argument in line 10\n" },
    { "AFTER timer 4", "10 AFTER 5,4 GOSUB 100\n20 END\n100 RETURN", "? Illegal argument in line 10\n" },
    { "EVERY timer -1", "10 EVERY 5,-1 GOSUB 100\n20 END\n100 RETURN", "? Illegal argument in line 10\n" },
    { "AFTER OFF timer 4", "10 AFTER OFF 4", "? Illegal argument in line 10\n" },
    { "AFTER to a missing line", "10 AFTER 5 GOSUB 999", "? Undefined line number 999 in line 10\n" },
    { "AFTER to a missing label", "10 AFTER 5 GOSUB NOWHERE", "? Undefined label in line 10\n" },
    { "AFTER without GOSUB", "10 AFTER 5", "? GOTO or GOSUB expected in line 10\n" },
    { "AFTER without a delay", "10 AFTER GOSUB 100", "? Numeric expression expected in line 10\n" },
    { "AFTER OFF before ELSE", "10 IF 0 THEN AFTER OFF ELSE PRINT 3", "3\n" },
    { "REMAIN of a timer that is not set", "PRINT REMAIN(0);REMAIN(3)", "00\n" },
    { "REMAIN returns the time left and clears the timer", "10 AFTER 60,1 GOSUB 100\n20 R=REMAIN(1)\n30 PRINT R>=58 AND R<=60:PRINT REMAIN(1)\n40 WAIT 6:PRINT N:END\n100 N=1:RETURN", "-1\n0\n0\n" },
    { "REMAIN clears an EVERY timer", "10 EVERY 60,2 GOSUB 100\n20 R=REMAIN(2):PRINT R>0\n30 WAIT 6:PRINT N:END\n100 N=N+1:RETURN", "-1\n0\n" },
    { "REMAIN of a timer that has fired", "10 AFTER 2 GOSUB 100\n20 WAIT 12:PRINT REMAIN(0):END\n100 RETURN", "0\n" },
    { "REMAIN in an expression", "10 AFTER 60 GOSUB 100\n20 PRINT REMAIN(0)>0 AND REMAIN(0)=0:END\n100 RETURN", "-1\n" },
    { "REMAIN timer 4", "PRINT REMAIN(4)", "? Illegal argument\n" },
    { "REMAIN timer -1", "PRINT REMAIN(-1)", "? Illegal argument\n" },
    { "REMAIN without parentheses", "PRINT REMAIN", "? Opening parenthesis '(' expected\n" },
    // files and devices (docs/io-subsystem.md); the "mem:" driver keeps the files in memory
    { "OPEN FOR OUTPUT, PRINT #, CLOSE, read back", "10 OPEN \"mem:t\" FOR OUTPUT AS #1\n20 PRINT #1,\"hello\";1+2\n30 PRINT #1,\"x\"\n40 CLOSE #1\n50 OPEN \"mem:t\" FOR INPUT AS #1\n60 LINE INPUT #1,A$:LINE INPUT #1,B$\n70 PRINT A$;\"|\";B$:CLOSE #1", "hello3|x\n" },
    { "OPEN short form", "10 OPEN \"O\",#2,\"mem:t\"\n20 PRINT #2,\"abc\"\n30 CLOSE #2\n40 OPEN \"i\",#2,\"mem:t\"\n50 LINE INPUT #2,A$:PRINT A$", "abc\n" },
    { "OPEN FOR APPEND", "10 OPEN \"mem:t\" FOR OUTPUT AS #1:PRINT #1,\"one\":CLOSE #1\n20 OPEN \"mem:t\" FOR APPEND AS #1:PRINT #1,\"two\":CLOSE #1\n30 OPEN \"mem:t\" FOR INPUT AS #1\n40 WHILE NOT EOF(1):LINE INPUT #1,A$:PRINT A$:WEND", "one\ntwo\n" },
    { "OPEN short form append", "10 OPEN \"O\",#1,\"mem:t\":PRINT #1,\"a\":CLOSE\n20 OPEN \"A\",#1,\"mem:t\":PRINT #1,\"b\":CLOSE\n30 OPEN \"I\",#1,\"mem:t\":LINE INPUT #1,A$:LINE INPUT #1,B$:PRINT A$;B$", "ab\n" },
    { "INPUT # splits a line into fields", "10 OPEN \"mem:t\" FOR OUTPUT AS #1:PRINT #1,\"7,hi there,2.5\":CLOSE #1\n20 OPEN \"mem:t\" FOR INPUT AS #1\n30 INPUT #1,A,B$,C:PRINT A;B$;C", "7hi there2.5\n" },
    { "INPUT # goes on with the next line", "10 OPEN \"mem:t\" FOR OUTPUT AS #1:PRINT #1,\"1,2\":PRINT #1,\"3\":CLOSE #1\n20 OPEN \"mem:t\" FOR INPUT AS #1\n30 INPUT #1,A,B,C:PRINT A+B+C", "6\n" },
    { "INPUT # shows no prompt", "10 OPEN \"mem:t\" FOR OUTPUT AS #1:PRINT #1,\"5\":CLOSE #1\n20 OPEN \"mem:t\" FOR INPUT AS #1\n30 INPUT #1,A:PRINT A\n40 INPUT \"X\";B", "5\nX? ? End of input in line 40\n" },
    { "INPUT # with bad data", "10 OPEN \"mem:t\" FOR OUTPUT AS #1:PRINT #1,\"abc\":CLOSE #1\n20 OPEN \"mem:t\" FOR INPUT AS #1\n30 INPUT #1,A", "? Bad data in file in line 30\n" },
    { "INPUT # past the end", "10 OPEN \"mem:t\" FOR OUTPUT AS #1:CLOSE #1\n20 OPEN \"mem:t\" FOR INPUT AS #1\n30 INPUT #1,A", "? Input past end in line 30\n" },
    { "LINE INPUT # past the end", "10 OPEN \"mem:t\" FOR OUTPUT AS #1:CLOSE #1\n20 OPEN \"mem:t\" FOR INPUT AS #1\n30 LINE INPUT #1,A$", "? Input past end in line 30\n" },
    { "LINE INPUT keeps commas and quotes", "10 OPEN \"mem:t\" FOR OUTPUT AS #1:PRINT #1,\"a,b \";CHR$(34);\"c\";CHR$(34);\", d\":CLOSE #1\n20 OPEN \"mem:t\" FOR INPUT AS #1\n30 LINE INPUT #1,A$:PRINT A$", "a,b \"c\", d\n" },
    { "LINE INPUT needs a string variable", "LINE INPUT A", "? LINE INPUT needs one string variable\n" },
    { "LINE INPUT needs INPUT", "LINE PRINT", "? INPUT expected after LINE\n" },
    { "WRITE #", "10 OPEN \"mem:t\" FOR OUTPUT AS #1:WRITE #1,\"a\",5,\"b c\",-2.5:WRITE #1:WRITE #1,\"z\"\n20 CLOSE #1:OPEN \"mem:t\" FOR INPUT AS #1\n30 WHILE NOT EOF(1):LINE INPUT #1,A$:PRINT A$:WEND", "\"a\",5,\"b c\",-2.5\n\n\"z\"\n" },
    { "WRITE to the screen", "WRITE \"a\",1", "\"a\",1\n" },
    { "WRITE then INPUT # reads the strings back", "10 OPEN \"mem:t\" FOR OUTPUT AS #1:WRITE #1,\"x,y\",3:CLOSE #1\n20 OPEN \"mem:t\" FOR INPUT AS #1:INPUT #1,A$,B:PRINT A$;B", "x,y3\n" },
    { "EOF LOF LOC", "10 OPEN \"mem:t\" FOR OUTPUT AS #1:PRINT #1,\"abc\":PRINT LOF(1);LOC(1):CLOSE #1\n20 OPEN \"mem:t\" FOR INPUT AS #1\n30 PRINT EOF(1);LOF(1);LOC(1):GET #1,A$:PRINT A$;LOC(1)", "44\n040\na1\n" },
    { "EOF at the end", "10 OPEN \"mem:t\" FOR OUTPUT AS #1:PRINT #1,\"a\":CLOSE #1\n20 OPEN \"mem:t\" FOR INPUT AS #1\n30 LINE INPUT #1,A$:PRINT EOF(1)", "-1\n" },
    { "GET # reads bytes", "10 OPEN \"mem:t\" FOR OUTPUT AS #1:PRINT #1,\"AB\":CLOSE #1\n20 OPEN \"mem:t\" FOR INPUT AS #1\n30 GET #1,A$,B,C$:PRINT A$;B;ASC(C$)", "A6610\n" },
    { "GET # past the end", "10 OPEN \"mem:t\" FOR OUTPUT AS #1:CLOSE #1\n20 OPEN \"mem:t\" FOR INPUT AS #1\n30 GET #1,A$", "? Input past end in line 30\n" },
    { "INPUT$", "10 OPEN \"mem:t\" FOR OUTPUT AS #1:PRINT #1,\"abcdef\":CLOSE #1\n20 OPEN \"mem:t\" FOR INPUT AS #1\n30 PRINT INPUT$(3,#1);\"|\";INPUT$(2,#1);\"|\";LOC(1)", "abc|de|5\n" },
    { "INPUT$ past the end", "10 OPEN \"mem:t\" FOR OUTPUT AS #1:PRINT #1,\"ab\":CLOSE #1\n20 OPEN \"mem:t\" FOR INPUT AS #1\n30 A$=INPUT$(10,#1)", "? Input past end in line 30\n" },
    { "INPUT$ from the keyboard without input", "A$=INPUT$(2)\nPRINT A$", "? End of input\n\n" },
    { "OPEN an open channel", "10 OPEN \"mem:a\" FOR OUTPUT AS #1\n20 OPEN \"mem:b\" FOR OUTPUT AS #1", "? File already open in line 20\n" },
    { "OPEN channel 0", "OPEN \"mem:a\" FOR OUTPUT AS #0", "? Bad file number\n" },
    { "OPEN channel 256", "OPEN \"mem:a\" FOR OUTPUT AS #256", "? Bad file number\n" },
    { "OPEN channel 255", "OPEN \"mem:a\" FOR OUTPUT AS #255:PRINT #255,\"x\":CLOSE #255:PRINT \"ok\"", "ok\n" },
    { "OPEN a missing file", "OPEN \"mem:nothing\" FOR INPUT AS #1", "? File not found\n" },
    { "OPEN an unknown device", "OPEN \"nodev:x\" FOR INPUT AS #1", "? Device not found\n" },
    { "OPEN with a bad mode", "OPEN \"X\",#1,\"mem:a\"", "? Bad file mode\n" },
    { "OPEN with an empty name", "OPEN \"\" FOR OUTPUT AS #1", "? Bad file name\n" },
    { "OPEN with an empty path", "OPEN \"mem:\" FOR OUTPUT AS #1", "? Bad file name\n" },
    { "PRINT # on a closed channel", "PRINT #4,\"x\"", "? File not open\n" },
    { "INPUT # on a closed channel", "INPUT #4,A", "? File not open\n" },
    { "reading from an output file", "10 OPEN \"mem:a\" FOR OUTPUT AS #1\n20 LINE INPUT #1,A$", "? Bad file mode in line 20\n" },
    { "writing to an input file", "10 OPEN \"mem:a\" FOR OUTPUT AS #1:CLOSE\n20 OPEN \"mem:a\" FOR INPUT AS #1\n30 PRINT #1,\"x\"", "? Bad file mode in line 30\n" },
    { "CLOSE a channel that is not open", "CLOSE #7:PRINT \"ok\"", "ok\n" },
    { "CLOSE several channels", "10 OPEN \"mem:a\" FOR OUTPUT AS #1:OPEN \"mem:b\" FOR OUTPUT AS #2\n20 CLOSE #1,#2\n30 PRINT #1,\"x\"", "? File not open in line 30\n" },
    { "CLOSE without a channel closes all", "10 OPEN \"mem:a\" FOR OUTPUT AS #1:OPEN \"mem:b\" FOR OUTPUT AS #2\n20 CLOSE\n30 PRINT #2,\"x\"", "? File not open in line 30\n" },
    { "CLOSE with a channel expression", "10 N=3:OPEN \"mem:a\" FOR OUTPUT AS #N\n20 CLOSE #N+0\n30 PRINT #3,\"x\"", "? File not open in line 30\n" },
    { "channel expressions in OPEN", "10 N=2:OPEN \"mem:a\" FOR OUTPUT AS #N*2:PRINT #4,\"hi\":CLOSE:PRINT \"ok\"", "ok\n" },
    { "the file name is an expression", "10 N$=\"mem:\":OPEN N$+\"file\" FOR OUTPUT AS #1:PRINT #1,\"x\":CLOSE #1:OPEN \"mem:file\" FOR INPUT AS #1:LINE INPUT #1,A$:PRINT A$", "x\n" },
    { "OPEN OUTPUT truncates", "10 OPEN \"mem:t\" FOR OUTPUT AS #1:PRINT #1,\"long text\":CLOSE\n20 OPEN \"mem:t\" FOR OUTPUT AS #1:PRINT #1,\"s\":CLOSE\n30 OPEN \"mem:t\" FOR INPUT AS #1:PRINT LOF(1)", "2\n" },
    { "null device", "10 OPEN \"null:\" FOR OUTPUT AS #1:PRINT #1,\"anything\":CLOSE\n20 OPEN \"null:\" FOR INPUT AS #2:PRINT EOF(2):LINE INPUT #2,A$", "-1\n? Input past end in line 20\n" },
    { "a carriage return before the newline is dropped", "10 OPEN \"mem:t\" FOR OUTPUT AS #1:PRINT #1,\"ab\";CHR$(13)\n20 CLOSE:OPEN \"mem:t\" FOR INPUT AS #1:LINE INPUT #1,A$:PRINT LEN(A$)", "2\n" },
    { "OPEN FOR needs a mode", "OPEN \"a\" FOR FOO AS #1", "? INPUT, OUTPUT, APPEND, RANDOM or UPDATE expected\n" },
    { "OPEN FOR needs AS", "OPEN \"a\" FOR OUTPUT #1", "? AS expected\n" },
    { "OPEN needs a channel", "OPEN \"a\" FOR OUTPUT AS 1", "? Channel '#n' expected\n" },
    { "OPEN needs FOR or a comma", "OPEN \"a\"", "? FOR or comma expected\n" },
    { "ADDRESS$ at the start", "PRINT ADDRESS$", "file\n" },
    { "ADDRESS with a name", "ADDRESS MEM:PRINT ADDRESS$", "mem\n" },
    { "ADDRESS with a string", "ADDRESS \"MEM\":PRINT ADDRESS$", "mem\n" },
    { "ADDRESS with a string expression and a colon", "A$=\"nul\":ADDRESS A$+\"l:\":PRINT ADDRESS$", "null\n" },
    { "ADDRESS swaps with the previous one", "ADDRESS MEM:ADDRESS NULL:ADDRESS:PRINT ADDRESS$:ADDRESS:PRINT ADDRESS$", "mem\nnull\n" },
    { "ADDRESS to an unknown device", "ADDRESS NODEV", "? Device not found\n" },
    { "names without a prefix use the ADDRESS driver", "10 ADDRESS MEM\n20 OPEN \"t\" FOR OUTPUT AS #1:PRINT #1,\"x\":CLOSE\n30 ADDRESS FILE\n40 OPEN \"mem:t\" FOR INPUT AS #1:LINE INPUT #1,A$:PRINT A$", "x\n" },
    { "an explicit prefix wins over ADDRESS", "10 ADDRESS NULL\n20 OPEN \"mem:t\" FOR OUTPUT AS #1:PRINT #1,\"x\":CLOSE\n30 OPEN \"mem:t\" FOR INPUT AS #1:PRINT LOF(1)", "2\n" },
    { "file: forces a disk file", "10 ADDRESS MEM\n20 OPEN \"file:/nonexistent/x\" FOR INPUT AS #1", "? File not found in line 20\n" },
    { "ADDRESS before ELSE", "IF 0 THEN ADDRESS MEM ELSE PRINT 3", "3\n" },
    // phase 2: KILL, RENAME, MKDIR, CHDIR, SEEK and the sh / res / con drivers
    // ON ERROR, ERR$, ERL
    { "ON ERROR GOTO", "10 ON ERROR GOTO 100\n20 PRINT 1/0\n30 PRINT \"not here\"\n100 PRINT ERR$;ERL", "Division by zero20\n" },
    { "ON ERROR GOSUB resumes behind the line", "10 ON ERROR GOSUB 100\n20 PRINT 1/0\n30 PRINT \"after\"\n40 END\n100 PRINT ERR$:RETURN", "Division by zero\nafter\n" },
    { "ON ERROR OFF", "10 ON ERROR GOTO 100\n20 ON ERROR OFF\n30 PRINT 1/0\n100 PRINT \"caught\"", "? Division by zero in line 30\n" },
    { "the GOTO trap is off inside the handler", "10 ON ERROR GOTO 100\n20 PRINT 1/0\n100 PRINT \"h\":PRINT 1/0", "h\n? Division by zero in line 100\n" },
    { "the GOSUB trap is on again after RETURN", "10 ON ERROR GOSUB 100\n20 PRINT 1/0\n30 PRINT 1/0\n40 END\n100 PRINT ERR$;ERL:RETURN", "Division by zero20\nDivision by zero30\n" },
    { "an error in a GOSUB handler is not caught", "10 ON ERROR GOSUB 100\n20 PRINT 1/0\n30 END\n100 PRINT 1/0:RETURN", "? Division by zero in line 100\n" },
    { "the data stack is cleaned up", "10 ON ERROR GOSUB 100\n20 A=5+(1/0)\n30 PRINT A;\"ok\"\n40 END\n100 RETURN", "0ok\n" },
    { "an error inside a function gives the parameters back", "10 DEF FN F(X)=1/X\n20 X=7\n30 ON ERROR GOSUB 100\n40 PRINT FN F(0)\n50 PRINT X\n60 END\n100 PRINT ERR$:RETURN", "Division by zero\n7\n" },
    { "ERR$ and ERL without an error", "PRINT ERR$;ERL", "0\n" },
    { "ERL in an expression", "PRINT ERL+1", "1\n" },
    { "other errors are caught, too", "10 ON ERROR GOTO 100\n20 OPEN \"mem:nope\" FOR INPUT AS #1\n100 PRINT ERR$;ERL", "File not found20\n" },
    { "ON ERROR needs GOTO, GOSUB or OFF", "ON ERROR", "? GOTO, GOSUB or OFF expected\n" },
    { "ON ERROR in a loop of handlers", "10 ON ERROR GOSUB 100\n20 FOR I=1 TO 3\n30 PRINT 1/0\n40 NEXT I\n50 PRINT N\n60 END\n100 N=N+1:RETURN", "3\n" },
    { "ON x GOTO still works", "10 X=2:ON X GOTO 100,200\n100 PRINT \"a\":END\n200 PRINT \"b\"", "b\n" },
    // phase 3: random access, FOR UPDATE, the play and tcp drivers
    { "random file: PUTREC and GETREC", "10 OPEN \"mem:r\" FOR RANDOM AS #1 LEN=10\n20 FIELD #1,4 AS N$,6 AS C$\n30 LSET N$=\"Bob\":LSET C$=\"Paris\"\n40 PUTREC #1,1\n50 LSET N$=\"Eve\":RSET C$=\"Rome\"\n60 PUTREC #1,2\n70 GETREC #1,1:PRINT N$;\"|\";C$;\"|\"\n80 GETREC #1,2:PRINT N$;\"|\";C$;\"|\"\n90 PRINT LOF(1);LOC(1)", "Bob |Paris |\nEve |  Rome|\n202\n" },
    { "random file: records in any order", "10 OPEN \"R\",#1,\"mem:r\",4\n20 FIELD #1,4 AS A$\n30 LSET A$=\"c\":PUTREC #1,3\n40 LSET A$=\"a\":PUTREC #1,1\n50 GETREC #1,3:PRINT A$;\"|\":GETREC #1,1:PRINT A$;\"|\":PRINT LOF(1)", "c   |\na   |\n12\n" },
    { "random file: the next record", "10 OPEN \"mem:r\" FOR RANDOM AS #1 LEN=3\n20 FIELD #1,3 AS A$\n30 FOR I=1 TO 3:LSET A$=STR$(I):PUTREC #1:NEXT I\n40 GETREC #1,1:GETREC #1:PRINT A$;LOC(1):GETREC #1:PRINT A$;LOC(1)", "2  2\n3  3\n" },
    { "random file: GETREC behind the end", "10 OPEN \"mem:r\" FOR RANDOM AS #1 LEN=4\n20 GETREC #1,1", "? Input past end in line 20\n" },
    { "random file: the default record length is 128", "10 OPEN \"mem:r\" FOR RANDOM AS #1\n20 FIELD #1,100 AS A$,28 AS B$\n30 LSET A$=\"x\":PUTREC #1,1:PRINT LOF(1)", "128\n" },
    { "FIELD overflow", "10 OPEN \"mem:r\" FOR RANDOM AS #1 LEN=8\n20 FIELD #1,5 AS A$,4 AS B$", "? Field overflow in line 20\n" },
    { "FIELD needs a random file", "10 OPEN \"mem:r\" FOR OUTPUT AS #1\n20 FIELD #1,5 AS A$", "? Bad file mode in line 20\n" },
    { "GETREC needs a random file", "10 OPEN \"mem:r\" FOR OUTPUT AS #1\n20 GETREC #1,1", "? Bad file mode in line 20\n" },
    { "GETREC record 0 is the next one", "10 OPEN \"mem:r\" FOR RANDOM AS #1 LEN=2\n20 FIELD #1,2 AS A$\n30 LSET A$=\"ab\":PUTREC #1,1:LSET A$=\"cd\":PUTREC #1,2\n40 GETREC #1,1:GETREC #1,0:PRINT A$", "cd\n" },
    { "bad record number", "10 OPEN \"mem:r\" FOR RANDOM AS #1 LEN=2\n20 GETREC #1,-1", "? Bad record number in line 20\n" },
    { "LSET justifies, RSET too, longer text is cut", "10 OPEN \"mem:r\" FOR RANDOM AS #1 LEN=4\n20 FIELD #1,4 AS A$\n30 LSET A$=\"abcdefg\":PRINT A$;\"|\"\n40 RSET A$=\"xy\":PRINT A$;\"|\"\n50 LSET A$=\"z\":PRINT A$;\"|\"", "abcd|\n  xy|\nz   |\n"},
    { "LSET on a plain variable keeps its length", "10 A$=\"12345\":LSET A$=\"ab\":PRINT A$;\"|\":RSET A$=\"cd\":PRINT A$;\"|\"", "ab   |\n   cd|\n" },
    { "CLOSE drops the bindings", "10 OPEN \"mem:r\" FOR RANDOM AS #1 LEN=4\n20 FIELD #1,4 AS A$\n30 CLOSE #1\n40 A$=\"abcdef\":LSET A$=\"x\":PRINT A$;\"|\"", "x     |\n" },
    { "OPEN FOR UPDATE keeps the data", "10 OPEN \"mem:u\" FOR OUTPUT AS #1:PRINT #1,\"hello\":CLOSE\n20 OPEN \"mem:u\" FOR UPDATE AS #1\n30 LINE INPUT #1,A$:PRINT A$:PRINT LOF(1)", "hello\n6\n" },
    { "OPEN short form U", "10 OPEN \"O\",#1,\"mem:u\":PRINT #1,\"abc\":CLOSE\n20 OPEN \"U\",#1,\"mem:u\":SEEK #1,1:PRINT #1,\"X\";:SEEK #1,0:PRINT INPUT$(3,#1)", "aXc\n" },
    { "record length only for random files", "OPEN \"O\",#1,\"mem:u\",10", "? Illegal argument\n" },
    { "bad record length", "OPEN \"R\",#1,\"mem:u\",99999", "? Illegal argument\n" },
    { "FIELD with an array element is refused", "10 DIM A$(3)\n20 OPEN \"mem:r\" FOR RANDOM AS #1\n30 FIELD #1,5 AS A$(1)", "? Plain string variable expected in line 30\n" },
    { "play: plays a line of music", "10 OPEN \"play:\" FOR OUTPUT AS #1\n20 PRINT #1,\"T120 C D E\"\n30 CLOSE #1", "<sync 1>\n<tone 0 261.626 0.5 0.4375>\n<tone 0 0 0 0.0625>\n<tone 0 293.665 0.5 0.4375>\n<tone 0 0 0 0.0625>\n<tone 0 329.628 0.5 0.4375>\n<tone 0 0 0 0.0625>\n<tone 0 0 0 0>\n" },
    { "play: a bad music string is an error", "10 OPEN \"play:2\" FOR OUTPUT AS #1\n20 PRINT #1,\"Z\"", "? Unknown music command 'Z' in line 20\n" },
    { "play: voice number", "OPEN \"play:9\" FOR OUTPUT AS #1", "? Bad file name\n" },
    { "play: can't be read", "OPEN \"play:\" FOR INPUT AS #1", "? Bad file mode\n" },
    { "tcp: needs host and port", "OPEN \"tcp:localhost\" FOR UPDATE AS #1", "? Bad file name\n" },
    { "KILL deletes a file", "10 OPEN \"mem:k\" FOR OUTPUT AS #1:CLOSE\n20 KILL \"mem:k\"\n30 OPEN \"mem:k\" FOR INPUT AS #1", "? File not found in line 30\n" },
    { "KILL of a missing file", "KILL \"mem:nothing\"", "? File not found\n" },
    { "KILL uses ADDRESS", "10 OPEN \"mem:k\" FOR OUTPUT AS #1:CLOSE\n20 ADDRESS MEM:KILL \"k\"\n30 PRINT 1\n40 KILL \"k\"", "1\n? File not found in line 40\n" },
    { "RENAME ... TO", "10 OPEN \"mem:a\" FOR OUTPUT AS #1:PRINT #1,\"data\":CLOSE\n20 RENAME \"mem:a\" TO \"mem:b\"\n30 OPEN \"mem:b\" FOR INPUT AS #1:LINE INPUT #1,A$:PRINT A$", "data\n" },
    { "RENAME onto an existing file", "10 OPEN \"mem:a\" FOR OUTPUT AS #1:CLOSE\n20 OPEN \"mem:b\" FOR OUTPUT AS #1:CLOSE\n30 RENAME \"mem:a\" TO \"mem:b\"", "? File already exists in line 30\n" },
    { "RENAME across devices", "RENAME \"mem:a\" TO \"null:a\"", "? Not the same device\n" },
    { "RENAME needs TO", "RENAME \"a\",\"b\"", "? TO expected\n" },
    { "MKDIR on a device without directories", "MKDIR \"mem:d\"", "? Not supported by device\n" },
    { "CHDIR on a device without directories", "CHDIR \"mem:d\"", "? Not supported by device\n" },
    { "SEEK on a read channel", "10 OPEN \"mem:s\" FOR OUTPUT AS #1:PRINT #1,\"0123456789\";:CLOSE\n20 OPEN \"mem:s\" FOR INPUT AS #1\n30 SEEK #1,4\n40 PRINT INPUT$(3,#1);LOC(1)", "4567\n" },
    { "SEEK back and read again", "10 OPEN \"mem:s\" FOR OUTPUT AS #1:PRINT #1,\"abc\";:CLOSE\n20 OPEN \"mem:s\" FOR INPUT AS #1\n30 A$=INPUT$(3,#1):SEEK #1,0:B$=INPUT$(2,#1)\n40 PRINT A$;B$", "abcab\n" },
    { "SEEK past the end of a RAM file", "10 OPEN \"mem:s\" FOR OUTPUT AS #1:PRINT #1,\"abc\";:CLOSE\n20 OPEN \"mem:s\" FOR INPUT AS #1\n30 SEEK #1,99", "? Bad position in line 30\n" },
    { "SEEK on a closed channel", "SEEK #3,0", "? File not open\n" },
    { "SEEK with a negative position", "10 OPEN \"mem:s\" FOR OUTPUT AS #1\n20 SEEK #1,-1", "? Bad position in line 20\n" },
    { "SEEK with a bad channel", "SEEK #0,0", "? Bad file number\n" },
    { "sh: reads the output of a command", "10 OPEN \"sh:echo one;echo two\" FOR INPUT AS #1\n20 WHILE NOT EOF(1):LINE INPUT #1,A$:PRINT A$:WEND", "one\ntwo\n" },
    { "sh: with ADDRESS", "10 ADDRESS SH\n20 OPEN \"echo hi\" FOR INPUT AS #1:LINE INPUT #1,A$:PRINT A$", "hi\n" },
    { "sh: can't append", "OPEN \"sh:cat\" FOR APPEND AS #1", "? Bad file mode\n" },
    { "con: output goes to the screen", "10 OPEN \"con:\" FOR OUTPUT AS #1\n20 PRINT #1,\"to the screen\"\n30 CLOSE #1", "to the screen\n" },
    { "res: can't be written", "OPEN \"res:x\" FOR OUTPUT AS #1", "? Permission denied\n" },
    { "res: stays inside its directory", "OPEN \"res:../x\" FOR INPUT AS #1", "? Permission denied\n" },
    { "channels stay open between direct lines of a program run", "10 OPEN \"mem:a\" FOR OUTPUT AS #1\n20 PRINT #1,\"x\"\n30 CLOSE #1:OPEN \"mem:a\" FOR INPUT AS #1:PRINT LOF(1)", "2\n" },
    { "EOF on a channel that is not open", "PRINT EOF(5)", "? File not open\n" },
    { "LOF on channel 0", "PRINT LOF(0)", "? Bad file number\n" },
    { "FILES",                  "FILES",                                           "<files \"\">\n" },
    { "FILES pattern",          "FILES \"*.BAS\"",                                 "<files \"*.BAS\">\n" },
    { "FILES name expression",  "A$=\"X\"\nFILES A$+\"*\"",                         "<files \"X*\">\n" },
    { "FILES goes on",          "PRINT 1:FILES:PRINT 2",                           "1\n<files \"\">\n2\n" },
    { "FILES before ELSE",      "IF 0 THEN FILES ELSE PRINT 3",                    "3\n" },
    { "FILES after a colon",    "FILES:PRINT 5",                                   "<files \"\">\n5\n" },
    { "CHAIN",                  "CHAIN \"PROG\"",                                  "<request CHAIN PROG \"\" -1>\n" },
    { "CHAIN to a line",        "CHAIN \"PROG\",100",                              "<request CHAIN PROG \"\" 100>\n" },
    { "CHAIN to a label",       "CHAIN \"PROG\",START",                            "<request CHAIN PROG \"START\" -1>\n" },
    { "CHAIN MERGE",            "CHAIN MERGE \"PROG\",100",                        "<request CHAIN MERGE PROG \"\" 100>\n" },
    { "CHAIN ends the run",     "PRINT 1:CHAIN \"X\":PRINT 2",                     "1\n<request CHAIN X \"\" -1>\n" },
    { "CHAIN empty name",       "CHAIN \"\"",                                      "? Illegal argument\n" },
    { "CHAIN without name",     "CHAIN",                                           "? String expression expected\n" },
    { "CHAIN with a comma and no target", "CHAIN \"X\",",                          "? Line number or label expected\n" },
    { "RENUM default",          "RENUM",                                           "<renumber * * step 10>\n" },
    { "RENUM from a line",      "RENUM 100",                                       "<renumber 100 * step 10>\n" },
    { "RENUM from a line, step","RENUM 100,5",                                     "<renumber 100 * step 5>\n" },
    { "RENUM range",            "RENUM 10-50",                                     "<renumber 10 50 step 10>\n" },
    { "RENUM up to",            "RENUM -50,20",                                    "<renumber * 50 step 20>\n" },
    { "RENUM step only",        "RENUM ,5",                                        "<renumber * * step 5>\n" },
    { "RENUM labels, expression", "RENUM A-B,2*3",                                 "<renumber A B step 6>\n" },
    { "RENUM ends the run",     "PRINT 1:RENUM:PRINT 2",                           "1\n<renumber * * step 10>\n" },
    { "RENUM step 0",           "RENUM ,0",                                        "? Illegal argument\n" },
    { "RENUM step missing",     "RENUM 10,",                                       "? Numeric expression expected\n" },
    { "number immediates: limits", "PRINT 65535;-65536;65536;-65537;131071;0",       "65535-6553665536-655371310710\n" },
    { "number immediates: 32 bit",  "PRINT -100000;100000;2147483647;-2147483648;2147483648", "-1000001000002147483647-21474836482147483648\n" },
    { "number immediates: fractions", "PRINT 1.5;-2.25;3",                          "1.5-2.253\n" },
    { "same constants used twice", "A$=\"HELLO\":B$=\"HELLO\":C$=\"\":D$=\"\":PRINT A$+B$;LEN(C$+D$);1.5+1.5", "HELLOHELLO03\n" },
    { "LIST all",               "LIST",                                            "<list * * on channel 0>\n" },
    { "LIST one line",          "LIST 20",                                         "<list 20 20 on channel 0>\n" },
    { "LIST range",             "LIST 10-30",                                      "<list 10 30 on channel 0>\n" },
    { "LIST from",              "LIST 10-",                                        "<list 10 * on channel 0>\n" },
    { "LIST up to",             "LIST -30",                                        "<list * 30 on channel 0>\n" },
    { "LIST label",             "LIST A",                                          "<list A A on channel 0>\n" },
    { "LIST labels",            "LIST A-B",                                        "<list A B on channel 0>\n" },
    { "LIST label to line",     "LIST A-99",                                       "<list A 99 on channel 0>\n" },
    { "LIST line to label",     "LIST 5-B",                                        "<list 5 B on channel 0>\n" },
    { "LIST then more",         "LIST 1-2:PRINT 3",                                "<list 1 2 on channel 0>\n3\n" },
    { "LIST uses channel 0",    "OPEN \"null:\" FOR OUTPUT AS #2:PRINT #2,\"X\"\nLIST",       "<list * * on channel 0>\n" },
    { "LIST bad line number",   "LIST 1000000",                                    "? Invalid line number\n" },
    { "LIST minus alone",       "LIST -",                                          "<list * * on channel 0>\n" },
    { "SAVE of empty program",  "SAVE \"X\",A",                                    "<save X ascii>\n" },
    { "SAVE with name expression", "N$=\"AB\"\nSAVE N$+\"C\",A",                   "<save ABC ascii>\n" },
    { "SAVE bad mode",          "SAVE \"X\",Z",                                    "? Bad SAVE mode\n" },
    { "SAVE empty name",        "SAVE \"\"",                                       "? Illegal argument\n" },
    // loops: WHILE, UNTIL, REPEAT, AGAIN
    { "WHILE",                  "I=0:WHILE I<3:PRINT I:I=I+1:WEND",                        "0\n1\n2\n" },
    { "WHILE zero passes",      "I=5:WHILE I<3:PRINT I:WEND:PRINT \"D\"",                  "D\n" },
    { "WHILE over lines",       "10 I=0\n20 WHILE I<2\n30 PRINT I\n40 I=I+1\n50 WEND\n60 PRINT \"D\"", "0\n1\nD\n" },
    { "nested WHILE",           "I=0:WHILE I<2:J=0:WHILE J<2:PRINT I*10+J:J=J+1:WEND:I=I+1:WEND", "0\n1\n10\n11\n" },
    { "UNTIL loop",             "I=0:UNTIL I=3:PRINT I:I=I+1:UEND",                        "0\n1\n2\n" },
    { "UNTIL loop zero passes", "I=3:UNTIL I=3:PRINT I:UEND:PRINT \"D\"",                  "D\n" },
    { "UNTIL loop in WHILE",    "I=0:WHILE I<2:J=0:UNTIL J=2:PRINT I*10+J:J=J+1:UEND:I=I+1:WEND", "0\n1\n10\n11\n" },
    { "REPEAT",                 "I=0:REPEAT:PRINT I:I=I+1:UNTIL I=3",                      "0\n1\n2\n" },
    { "REPEAT runs once",       "I=9:REPEAT:PRINT I:UNTIL 1",                              "9\n" },
    { "REPEAT over lines",      "10 I=0\n20 REPEAT\n30 PRINT I\n40 I=I+1\n50 UNTIL I>=2\n60 PRINT \"D\"", "0\n1\nD\n" },
    { "nested REPEAT",          "I=0:REPEAT:J=0:REPEAT:PRINT I*10+J:J=J+1:UNTIL J=2:I=I+1:UNTIL I=2", "0\n1\n10\n11\n" },
    { "WHILE in REPEAT in FOR", "FOR K=1 TO 2:I=0:REPEAT:J=0:WHILE J<2:PRINT K*100+I*10+J:J=J+1:WEND:I=I+1:UNTIL I=2:NEXT K", "100\n101\n110\n111\n200\n201\n210\n211\n" },
    { "AGAIN in FOR",           "10 K=0\n20 FOR I=1 TO 2\n30 K=K+1\n40 IF K<3 THEN AGAIN\n50 PRINT I;K\n60 NEXT I", "13\n24\n" },
    { "AGAIN 2",                "10 N=0\n20 FOR I=1 TO 2\n30 FOR J=1 TO 2\n40 N=N+1\n50 IF N=2 THEN AGAIN 2\n60 PRINT I;J;N\n70 NEXT J\n80 NEXT I", "111\n113\n124\n215\n226\n" },
    { "AGAIN 1 is the innermost loop", "10 N=0\n20 FOR I=1 TO 2\n30 FOR J=1 TO 2\n40 N=N+1\n50 IF N=2 THEN AGAIN 1\n60 PRINT I;J;N\n70 NEXT J\n80 NEXT I", "111\n123\n214\n225\n" },
    { "AGAIN in WHILE",         "10 I=0:K=0\n20 WHILE I<2\n30 K=K+1\n40 IF K=2 THEN AGAIN\n50 I=I+1\n60 PRINT K\n70 WEND", "1\n3\n" },
    { "AGAIN in REPEAT",        "10 K=0\n20 REPEAT\n30 K=K+1\n40 IF K<3 THEN AGAIN\n50 UNTIL 1\n60 PRINT K",  "3\n" },
    { "AGAIN in UNTIL loop",    "10 I=0:K=0\n20 UNTIL I=1\n30 K=K+1\n40 IF K<3 THEN AGAIN\n50 I=1\n60 UEND\n70 PRINT K", "3\n" },
    { "AGAIN across loop kinds", "10 K=0\n20 WHILE K<5\n30 FOR I=1 TO 1\n40 K=K+1\n50 IF K=2 THEN AGAIN 2\n60 NEXT I\n70 WEND\n80 PRINT K", "5\n" },
    { "AGAIN without loop",     "10 AGAIN",                                                "? AGAIN without loop in line 10\n" },
    { "AGAIN with too big a number", "10 FOR I=1 TO 2\n20 AGAIN 2\n30 NEXT I",             "? AGAIN without loop in line 20\n" },
    { "AGAIN with number zero", "10 FOR I=1 TO 2\n20 AGAIN 0\n30 NEXT I",                  "? Invalid loop number in line 20\n" },
    { "loops around a function are not visible", "10 FOR I=1 TO 2\n20 DEF FN A(X)\n30 AGAIN\n40 FN A = 1\n50 END FN\n60 NEXT I", "? AGAIN without loop in line 30\n" },
    // EXIT / LEAVE / ITERATE, DOWNTO, FOR EACH
    { "EXIT in FOR",            "10 FOR I=1 TO 9\n20 IF I=4 THEN EXIT\n30 NEXT I\n40 PRINT I",           "4\n" },
    { "LEAVE is EXIT",          "10 FOR I=1 TO 9\n20 IF I=3 THEN LEAVE\n30 NEXT I\n40 PRINT I",          "3\n" },
    { "EXIT in WHILE",          "10 I=0\n20 WHILE 1\n30 I=I+1\n40 IF I=5 THEN EXIT\n50 WEND\n60 PRINT I", "5\n" },
    { "EXIT in UNTIL loop",     "10 I=0\n20 UNTIL I=100\n30 I=I+1\n40 IF I=7 THEN EXIT\n50 UEND\n60 PRINT I", "7\n" },
    { "EXIT in REPEAT",         "10 I=0\n20 REPEAT\n30 I=I+1\n40 IF I=6 THEN EXIT\n50 UNTIL 0\n60 PRINT I", "6\n" },
    { "EXIT 2 leaves the outer loop", "10 FOR I=1 TO 3\n20 FOR J=1 TO 3\n30 IF J=2 THEN EXIT 2\n40 PRINT I;J\n50 NEXT J\n60 NEXT I\n70 PRINT \"done\"", "11\ndone\n" },
    { "EXIT leaves only the inner loop", "10 FOR I=1 TO 2\n20 FOR J=1 TO 3\n30 IF J=2 THEN EXIT\n40 PRINT I;J\n50 NEXT J\n60 NEXT I", "11\n21\n" },
    { "two EXITs in one loop",  "10 FOR I=1 TO 9\n20 IF I=2 THEN EXIT\n30 IF I=1 THEN EXIT\n40 NEXT I\n50 PRINT I", "1\n" },
    { "EXIT in an inner and an outer pending state", "10 FOR I=1 TO 3\n20 FOR J=1 TO 3\n30 IF J=2 THEN EXIT\n40 NEXT J\n50 IF I=2 THEN EXIT\n60 NEXT I\n70 PRINT I;J", "22\n" },
    { "ITERATE in FOR",         "10 FOR I=1 TO 5\n20 IF I=3 THEN ITERATE\n30 PRINT I;\n40 NEXT I\n50 PRINT",   "1245\n" },
    { "ITERATE in WHILE",       "10 I=0\n20 WHILE I<5\n30 I=I+1\n40 IF I=3 THEN ITERATE\n50 PRINT I;\n60 WEND\n70 PRINT", "1245\n" },
    { "ITERATE in UNTIL loop",  "10 I=0\n20 UNTIL I=5\n30 I=I+1\n40 IF I=3 THEN ITERATE\n50 PRINT I;\n60 UEND\n70 PRINT", "1245\n" },
    { "ITERATE in REPEAT tests the condition", "10 I=0\n20 REPEAT\n30 I=I+1\n40 IF I<3 THEN ITERATE\n50 PRINT I;\n60 UNTIL I>=5\n70 PRINT", "345\n" },
    { "ITERATE 2 steps the outer loop", "10 FOR I=1 TO 3\n20 FOR J=1 TO 3\n30 IF J=2 THEN ITERATE 2\n40 PRINT I;J\n50 NEXT J\n60 NEXT I", "11\n21\n31\n" },
    { "EXIT without loop",      "10 EXIT",                                                     "? EXIT without loop in line 10\n" },
    { "ITERATE without loop",   "10 ITERATE",                                                  "? ITERATE without loop in line 10\n" },
    { "EXIT with too big a number", "10 FOR I=1 TO 2\n20 EXIT 2\n30 NEXT I",                   "? EXIT without loop in line 20\n" },
    { "EXIT with number zero",  "10 FOR I=1 TO 2\n20 EXIT 0\n30 NEXT I",                       "? Invalid loop number in line 20\n" },
    { "EXIT does not see loops around a function", "10 FOR I=1 TO 2\n20 DEF FN A(X)\n30 EXIT\n40 FN A = 1\n50 END FN\n60 NEXT I", "? EXIT without loop in line 30\n" },
    { "EXIT in a multi-line IF", "10 FOR I=1 TO 9\n20 IF I=2 THEN\n30 EXIT\n40 END IF\n50 NEXT I\n60 PRINT I", "2\n" },
    { "DOWNTO",                 "10 FOR I=3 DOWNTO 1\n20 PRINT I;\n30 NEXT I\n40 PRINT",         "321\n" },
    { "DOWNTO with STEP",       "10 FOR I=10 DOWNTO 1 STEP 3\n20 PRINT I;\n30 NEXT I\n40 PRINT", "10741\n" },
    { "DOWNTO does not run when the start is lower", "10 FOR I=1 DOWNTO 3\n20 PRINT I\n30 NEXT I\n40 PRINT \"x\"", "x\n" },
    { "DOWNTO with expressions", "10 A=2:B=5\n20 FOR I=B*2 DOWNTO A+1\n30 PRINT I;\n40 NEXT I\n50 PRINT", "109876543\n" },
    { "DOWNTO with EXIT",       "10 FOR I=9 DOWNTO 1\n20 IF I=6 THEN EXIT\n30 NEXT I\n40 PRINT I", "6\n" },
    { "FOR EACH over a numeric array", "10 DIM A(3)\n20 A(0)=5:A(1)=6:A(2)=7\n30 FOR EACH V IN A()\n40 PRINT V;\n50 NEXT\n60 PRINT", "567\n" },
    { "FOR EACH with a key",    "10 DIM A(3)\n20 A(0)=5:A(1)=6:A(2)=7\n30 FOR EACH K,V IN A()\n40 PRINT K;V;\n50 NEXT V\n60 PRINT", "051627\n" },
    { "FOR EACH over a string array", "10 DIM A$(3)\n20 A$(0)=\"X\":A$(1)=\"YY\":A$(2)=\"Z\"\n30 FOR EACH S$ IN A$()\n40 PRINT S$;\n50 NEXT\n60 PRINT", "XYYZ\n" },
    { "FOR EACH with a string key", "10 DIM A$(2)\n20 A$(0)=\"X\":A$(1)=\"Y\"\n30 FOR EACH K,S$ IN A$()\n40 PRINT K;S$\n50 NEXT", "0X\n1Y\n" },
    { "FOR EACH over an integer array", "10 DIM A%(3)\n20 A%(0)=1:A%(1)=2:A%(2)=3\n30 FOR EACH V IN A%()\n40 S=S+V\n50 NEXT\n60 PRINT S", "6\n" },
    { "FOR EACH over a 2-dimensional array", "10 DIM A(2,2)\n20 A(0,0)=1:A(0,1)=2:A(1,0)=3:A(1,1)=4\n30 FOR EACH K,V IN A()\n40 PRINT K;V;\n50 NEXT\n60 PRINT", "01122334\n" },
    { "FOR EACH into an integer variable", "10 DIM A(2)\n20 A(0)=1.9:A(1)=2.5\n30 FOR EACH V% IN A()\n40 PRINT V%;\n50 NEXT\n60 PRINT", "12\n" },
    { "FOR EACH with EXIT",     "10 DIM A(5)\n20 FOR EACH K,V IN A()\n30 IF K=2 THEN EXIT\n40 NEXT\n50 PRINT K", "2\n" },
    { "FOR EACH with ITERATE",  "10 DIM A(4)\n20 FOR EACH K,V IN A()\n30 IF K=1 THEN ITERATE\n40 PRINT K;\n50 NEXT\n60 PRINT", "023\n" },
    { "FOR EACH nested",        "10 DIM A(2):DIM B$(2)\n20 A(0)=1:A(1)=2:B$(0)=\"x\":B$(1)=\"y\"\n30 FOR EACH V IN A()\n40 FOR EACH S$ IN B$()\n50 PRINT V;S$;\n60 NEXT\n70 NEXT\n80 PRINT", "1x1y2x2y\n" },
    { "FOR EACH leaves the elements alone", "10 DIM A(2)\n20 A(0)=3:A(1)=4\n30 FOR EACH V IN A()\n40 V=V*2\n50 NEXT\n60 PRINT A(0);A(1);V", "348\n" },
    { "FOR EACH with a variable named EACH", "10 FOR EACH=1 TO 3\n20 PRINT EACH;\n30 NEXT EACH\n40 PRINT",   "123\n" },
    { "FOR EACH type mismatch", "10 DIM A(2)\n20 FOR EACH S$ IN A()\n30 NEXT",                 "? Type mismatch (FOR EACH variable and array) in line 20\n" },
    { "FOR EACH without IN",    "10 DIM A(2)\n20 FOR EACH V A()\n30 NEXT",                    "? IN expected in line 20\n" },
    { "FOR EACH of an unknown array", "10 FOR EACH V IN Q()\n20 NEXT",                          "? Undefined variable in line 10\n" },
    { "FOR EACH without NEXT",  "10 DIM A(2)\n20 FOR EACH V IN A()",                           "? FOR without NEXT in line 20\n" },
    // PEEK / POKE, STOP / CONT / TRACE
    { "POKE and PEEK a byte",   "10 POKE 100,65\n20 PRINT PEEK(100);PEEK(101)",              "650\n" },
    { "PEEK is unsigned for bytes and words", "10 POKE 0,-1,1\n20 POKE 10,-1,2\n30 PRINT PEEK(0);PEEK(10,2)", "25565535\n" },
    { "POKE and PEEK a word, big endian", "10 POKE 20,258,2\n20 PRINT PEEK(20);PEEK(21);PEEK(20,2)", "12258\n" },
    { "POKE and PEEK a long",   "10 POKE 20,-2,4\n20 PRINT PEEK(20,4);PEEK(20);PEEK(23)", "-2255254\n" },
    { "POKE and PEEK a double", "10 POKE 40,3.25,-8\n20 PRINT PEEK(40,-8)",                  "3.25\n" },
    { "POKE and PEEK a single", "10 POKE 40,0.5,-4\n20 PRINT PEEK(40,-4)",                   "0.5\n" },
    { "POKE a string",          "10 POKE 200,\"ABC\"\n20 PRINT PEEK(200);PEEK(202);PEEK$(200,3);PEEK$(201,1)", "6567ABCB\n" },
    { "PEEK$ of untouched RAM", "10 PRINT LEN(PEEK$(5,4));ASC(PEEK$(5,1))",                  "40\n" },
    { "PEEK$ with length 0",    "10 PRINT LEN(PEEK$(5,0))",                                  "0\n" },
    { "POKE a binary string",   "10 POKE 0,CHR$(0)+CHR$(255)+CHR$(7)\n20 PRINT PEEK(0);PEEK(1);PEEK(2);LEN(PEEK$(0,3))", "025573\n" },
    { "last bytes of the RAM",  "10 POKE 1048575,9\n20 PRINT PEEK(1048575)",                 "9\n" },
    { "POKE behind the RAM",    "10 POKE 1048576,1",                                         "? Illegal address in line 10\n" },
    { "a long that does not fit", "10 POKE 1048574,1,4",                                     "? Illegal address in line 10\n" },
    { "PEEK of a negative address", "10 PRINT PEEK(-1)",                                     "? Illegal address in line 10\n" },
    { "PEEK with a bad size",   "10 PRINT PEEK(0,3)",                                        "? Illegal argument in line 10\n" },
    { "POKE with a bad size",   "10 POKE 0,1,8",                                             "? Illegal argument in line 10\n" },
    { "POKE with a value out of range", "10 POKE 0,5000000000,4",                            "? Illegal argument in line 10\n" },
    { "PEEK$ with a bad length", "10 PRINT PEEK$(0,-1)",                                     "? Illegal argument in line 10\n" },
    { "PEEK$ with one argument", "10 PRINT PEEK$(0)",                                        "? Illegal argument in line 10\n" },
    { "PEEK in an expression",  "10 POKE 5,7\n20 A=PEEK(5)*2+PEEK(5,2)\n30 PRINT A",         "1806\n" },
    { "STOP",                  "10 PRINT 1\n20 STOP\n30 PRINT 2",                            "1\n? Break in line 20\n" },
    { "STOP inside a FUNC",    "10 FUNC F(X)\n20 STOP\n30 RETURN X\n40 END FUNC\n50 PRINT FN F(1)", "? Break in line 20\n" },
    { "CONT without STOP",     "10 CONT",                                                    "? Cannot continue in line 10\n" },
    { "TRACE ON and OFF",      "10 TRACE ON\n20 A=1\n30 TRACE OFF\n40 PRINT A",              "[20][30]1\n" },
    { "TRACE needs ON or OFF", "10 TRACE 1",                                                 "? ON or OFF expected in line 10\n" },
    // array parameters and LOCAL arrays
    { "array parameter of a FUNC", "10 DIM V(4):FOR I=0 TO 3:V(I)=I+1:NEXT I\n20 FUNC SUM(A())\n30 LOCAL S,K\n40 FOR K=0 TO 3:S=S+A(K):NEXT K\n50 RETURN S\n60 END FUNC\n70 PRINT FN SUM(V())", "10\n" },
    { "array parameter is by reference", "10 DIM V(3)\n20 PROC FILL(A(),X)\n30 LOCAL K\n40 FOR K=0 TO 2:A(K)=X*K:NEXT K\n50 END PROC\n60 CALL FILL(V(),5)\n70 PRINT V(0);V(1);V(2)", "0510\n" },
    { "string array parameter", "10 DIM W$(2):W$(0)=\"a\":W$(1)=\"b\"\n20 PROC SAY(S$())\n30 PRINT S$(0);S$(1)\n40 END PROC\n50 CALL SAY(W$())", "ab\n" },
    { "the same PROC with different arrays", "10 DIM P(2),Q(2):P(0)=1:Q(0)=2\n20 PROC SAY(A())\n30 PRINT A(0);\n40 END PROC\n50 CALL SAY(P()):CALL SAY(Q()):CALL SAY(P()):PRINT", "121\n" },
    { "array parameter name is given back", "10 DIM A(2):A(0)=7\n20 DIM B(2):B(0)=8\n30 PROC SAY(A())\n40 PRINT A(0);\n50 END PROC\n60 CALL SAY(B()):PRINT A(0)", "87\n" },
    { "array parameter passed on to a recursive call", "10 DIM V(2)\n20 PROC UP(A(),N)\n30 IF N>0 THEN CALL UP(A(),N-1)\n40 A(0)=A(0)+1\n50 END PROC\n60 CALL UP(V(),4):PRINT V(0)", "5\n" },
    { "array bounds of the argument count", "10 DIM V(2)\n20 PROC P(A())\n30 A(2)=1\n40 END PROC\n50 CALL P(V())", "? Subscript out of range in line 30\n" },
    { "multi-dimensional array argument", "10 DIM M(2,2):M(1,1)=5\n20 PROC P(A())\n30 PRINT A(1,1)\n40 END PROC\n50 CALL P(M())", "5\n" },
    { "scalar given for an array parameter", "10 PROC P(A())\n20 END PROC\n30 CALL P(5)",        "? Array expected as argument in line 30\n" },
    { "array given for a scalar parameter", "10 DIM V(2)\n20 PROC P(A)\n30 END PROC\n40 CALL P(V())", "? Array not allowed as this argument in line 40\n" },
    { "array type mismatch",    "10 DIM V(2)\n20 PROC P(A$())\n30 END PROC\n40 CALL P(V())",     "? Type mismatch (array argument) in line 40\n" },
    { "an error in a PROC gives the array parameter back", "10 DIM V(2),B(2):V(0)=3\n20 ON ERROR GOTO 90\n30 PROC P(B())\n40 LOCAL T(2)\n50 B(0)=1/0\n60 END PROC\n70 CALL P(V())\n80 END\n90 PRINT V(0);B(0)", "30\n" },
    { "LOCAL array",           "10 PROC R(N)\n20 LOCAL T(2)\n30 T(0)=N\n40 IF N>1 THEN CALL R(N-1)\n50 PRINT T(0);\n60 END PROC\n70 CALL R(3):PRINT", "123\n" },
    { "LOCAL array of strings", "10 PROC R(N)\n20 LOCAL T$(2)\n30 T$(0)=STR$(N)\n40 IF N>1 THEN CALL R(N-1)\n50 PRINT T$(0);\n60 END PROC\n70 CALL R(3):PRINT", "123\n" },
    { "LOCAL A() is a local copy",  "10 DIM D(2):D(0)=4\n20 PROC P\n30 LOCAL D()\n40 PRINT D(0);\n50 D(0)=9\n60 END PROC\n70 CALL P:PRINT D(0)", "04\n" },
    { "LOCAL array in a FUNC",  "10 FUNC F(N)\n20 LOCAL T(3)\n30 T(1)=N*2\n40 RETURN T(1)+T(2)\n50 END FUNC\n60 PRINT FN F(4);FN F(5)", "810\n" },
    { "LOCAL A() of an unknown array", "10 PROC P\n20 LOCAL Q()\n30 END PROC",                 "? Undefined variable in line 20\n" },
    // RESUME
    { "RESUME retries the statement", "10 ON ERROR GOTO 100\n20 D=0\n30 PRINT 10/D\n40 END\n100 D=2:RESUME",    "5\n" },
    { "RESUME NEXT",            "10 ON ERROR GOTO 100\n20 PRINT 1/0:PRINT \"next\"\n30 PRINT \"end\"\n40 END\n100 PRINT \"err\":RESUME NEXT", "err\nnext\nend\n" },
    { "RESUME with a line number", "10 ON ERROR GOTO 100\n20 PRINT 1/0\n30 PRINT \"no\"\n40 END\n50 PRINT \"here\"\n60 END\n100 RESUME 50", "here\n" },
    { "RESUME with a label",    "10 ON ERROR GOTO 100\n20 PRINT 1/0\n30 END\n40 LABEL FOO:PRINT \"fixed\"\n50 END\n100 RESUME FOO", "fixed\n" },
    { "RESUME arms the GOTO handler again", "10 ON ERROR GOTO 100\n20 X=1/0:PRINT \"a\"\n30 Y=1/0:PRINT \"b\"\n40 END\n100 RESUME NEXT", "a\nb\n" },
    { "RESUME from a GOSUB handler", "10 ON ERROR GOSUB 100\n20 X=1/0:PRINT \"a\"\n30 GOSUB 200\n40 PRINT \"b\"\n50 END\n100 RESUME NEXT\n200 RETURN", "a\nb\n" },
    { "RESUME from a GOSUB handler keeps the GOSUB stack", "10 ON ERROR GOSUB 100\n20 GOSUB 200\n30 PRINT \"back\"\n40 END\n100 RESUME NEXT\n200 X=1/0:PRINT \"in\"\n210 RETURN", "in\nback\n" },
    { "RESUME NEXT in the THEN part", "10 ON ERROR GOTO 100\n20 IF 1 THEN PRINT 1/0 ELSE PRINT \"e\"\n30 PRINT \"after\"\n40 END\n100 RESUME NEXT", "after\n" },
    { "RESUME NEXT in the ELSE part", "10 ON ERROR GOTO 100\n20 IF 0 THEN PRINT \"t\" ELSE PRINT 1/0\n30 PRINT \"after\"\n40 END\n100 RESUME NEXT", "after\n" },
    { "RESUME NEXT in a loop",  "10 ON ERROR GOTO 100\n20 FOR I=1 TO 3\n30 PRINT 6/(I-2)\n40 NEXT I\n50 END\n100 RESUME NEXT", "-6\n6\n" },
    { "RESUME NEXT with several statements on the line", "10 ON ERROR GOTO 100\n20 A=1:B=1/0:C=3:PRINT A;C;B\n30 END\n100 RESUME NEXT", "130\n" },
    { "ERL after RESUME is the failing line", "10 ON ERROR GOTO 100\n20 PRINT 1/0\n30 PRINT 2/0\n40 END\n100 PRINT ERL;\n110 RESUME NEXT", "2030" },
    { "RESUME without an error", "10 RESUME",                                                  "? RESUME without error in line 10\n" },
    { "RESUME twice",           "10 ON ERROR GOTO 100\n20 PRINT 1/0\n30 ON ERROR OFF:GOTO 110\n100 RESUME NEXT\n110 RESUME", "? RESUME without error in line 110\n" },
    { "RESUME after an error in a PROC", "10 PROC P\n20 PRINT 1/0\n30 END PROC\n40 ON ERROR GOTO 100\n50 CALL P\n60 END\n100 RESUME", "? Cannot RESUME (the error was in a PROC or FUNC) in line 100\n" },
    { "RESUME target after an error in a PROC", "10 PROC P\n20 PRINT 1/0\n30 END PROC\n40 ON ERROR GOTO 100\n50 CALL P\n60 PRINT \"after\"\n70 END\n100 PRINT \"h\":RESUME 60", "h\nafter\n" },
    { "RESUME NEXT after an error in a FUNC", "10 FUNC F(X)\n20 RETURN 1/X\n30 END FUNC\n40 ON ERROR GOTO 100\n50 PRINT FN F(0)\n60 END\n100 RESUME NEXT", "? Cannot RESUME (the error was in a PROC or FUNC) in line 100\n" },
    // PROC / FUNC / CALL / LOCAL
    { "PROC and CALL",          "10 CALL HI\n20 CALL HI\n30 END\n40 PROC HI\n50 PRINT \"hi\"\n60 END PROC",   "hi\nhi\n" },
    { "PROC with parameters",   "10 CALL ADD(2,3)\n20 CALL ADD(10,20)\n30 END\n40 PROC ADD(A,B)\n50 PRINT A+B\n60 END PROC", "5\n30\n" },
    { "PROC defined before use", "10 PROC SAY(S$)\n20 PRINT \"<\";S$;\">\"\n30 END PROC\n40 CALL SAY(\"x\")", "<x>\n" },
    { "parameters are given back", "10 A=7\n20 CALL P(1)\n30 PRINT A\n40 END\n50 PROC P(A)\n60 A=A+100\n70 END PROC", "7\n" },
    { "PROC without LOCAL changes globals", "10 G=1\n20 CALL P\n30 PRINT G\n40 END\n50 PROC P\n60 G=G+5\n70 END PROC", "6\n" },
    { "LOCAL variables are given back", "10 X=1:Y$=\"out\"\n20 CALL P\n30 PRINT X;Y$\n40 END\n50 PROC P\n60 LOCAL X,Y$\n70 PRINT X;\"[\";Y$;\"]\"\n80 X=99:Y$=\"in\"\n90 END PROC", "0[]\n1out\n" },
    { "LOCAL in a recursive PROC", "10 CALL DOWN(3)\n20 END\n30 PROC DOWN(N)\n40 LOCAL K\n50 K=N*10\n60 IF N>1 THEN CALL DOWN(N-1)\n70 PRINT K\n80 END PROC", "10\n20\n30\n" },
    { "EXIT PROC",             "10 CALL P(1)\n20 CALL P(2)\n30 END\n40 PROC P(N)\n50 IF N=1 THEN EXIT PROC\n60 PRINT \"two\"\n70 END PROC",  "two\n" },
    { "EXIT PROC from a loop",  "10 CALL P\n20 PRINT \"back\"\n30 END\n40 PROC P\n50 FOR I=1 TO 9\n60 IF I=3 THEN EXIT PROC\n70 NEXT I\n80 PRINT \"no\"\n90 END PROC", "back\n" },
    { "FUNC",                  "10 FUNC SQ(X)\n20 RETURN X*X\n30 END FUNC\n40 PRINT FN SQ(4)",  "16\n" },
    { "FUNC result by FN name =", "10 FUNC TWICE(X)\n20 FN TWICE = X*2\n30 END FUNC\n40 PRINT FN TWICE(21)", "42\n" },
    { "recursive FUNC",         "10 FUNC FACT(N)\n20 IF N<=1 THEN RETURN 1\n30 RETURN N*FN FACT(N-1)\n40 END FUNC\n50 PRINT FN FACT(5)", "120\n" },
    { "recursive FUNC with FN name = ", "10 FUNC FIB(N)\n20 IF N<2 THEN FN FIB=N ELSE FN FIB=FN FIB(N-1)+FN FIB(N-2)\n30 END FUNC\n40 PRINT FN FIB(10)", "55\n" },
    { "string FUNC",            "10 FUNC REV$(S$)\n20 LOCAL R$,I\n30 FOR I=LEN(S$) DOWNTO 1\n40 R$=R$+MID$(S$,I,1)\n50 NEXT I\n60 RETURN R$\n70 END FUNC\n80 PRINT FN REV$(\"abc\")", "cba\n" },
    { "integer FUNC",           "10 FUNC HALF%(N)\n20 RETURN N\\2\n30 END FUNC\n40 PRINT FN HALF%(9)", "4\n" },
    { "FUNC without RETURN is 0", "10 FUNC NOTHING(X)\n20 X=X+1\n30 END FUNC\n40 PRINT FN NOTHING(1)", "0\n" },
    { "EXIT FUNC returns the value so far", "10 FUNC F(X)\n20 FN F=5\n30 IF X=0 THEN EXIT FUNC\n40 FN F=6\n50 END FUNC\n60 PRINT FN F(0);FN F(1)", "56\n" },
    { "FUNC keeps the caller's variables", "10 FUNC F(X)\n20 R=100\n30 RETURN X\n40 END FUNC\n50 R=3\n60 PRINT FN F(2);R", "2100\n" },
    { "FUNC used inside an expression", "10 FUNC SQ(X)\n20 RETURN X*X\n30 END FUNC\n40 PRINT FN SQ(2)+FN SQ(3)*2", "22\n" },
    { "multi-line DEF FN with RETURN and LOCAL", "10 DEF FN A(X)\n20 RETURN X+1\n30 END FN\n40 PRINT FN A(3)", "4\n" },
    { "CALL of an unknown PROC", "10 CALL NOPE",                                             "? Undefined procedure in line 10\n" },
    { "CALL with a wrong argument count", "10 CALL P(1,2)\n20 END\n30 PROC P(A)\n40 END PROC",  "? Argument count mismatch in line 10\n" },
    { "LOCAL outside a procedure", "10 LOCAL X",                                              "? LOCAL outside a PROC or FUNC in line 10\n" },
    { "RETURN with a value in a PROC", "10 CALL P\n20 END\n30 PROC P\n40 RETURN 5\n50 END PROC", "? RETURN with a value outside a FUNC in line 40\n" },
    { "EXIT PROC outside a procedure", "10 EXIT PROC",                                        "? EXIT outside a PROC, FUNC or DEF FN in line 10\n" },
    { "PROC inside a PROC",     "10 PROC A\n20 PROC B\n30 END PROC\n40 END PROC",              "? PROC inside a PROC or FUNC in line 20\n" },
    { "GOSUB and RETURN inside a PROC", "10 CALL P\n20 END\n30 PROC P\n40 GOSUB 70\n50 PRINT \"after\"\n60 EXIT PROC\n70 PRINT \"sub\"\n80 RETURN\n90 END PROC", "sub\nafter\n" },
    { "an error in a PROC gives the parameters back", "10 A=5\n20 ON ERROR GOTO 60\n30 CALL P(1)\n40 END\n50 PROC P(A)\n55 LOCAL B:B=1/0\n57 END PROC\n60 PRINT A;ERR$", "5Division by zero\n" },
    // FLOOR CEIL LOG2 LOG10, IMP NIMP EQV NEQV, SHL SHR ASL ASR, TIME$ DATE$ DATETIME$
    { "FLOOR",                  "10 PRINT FLOOR(2.7);FLOOR(-2.5)",                             "2-3\n" },
    { "CEIL",                   "10 PRINT CEIL(2.1);CEIL(-2.1);CEIL(4)",                       "3-24\n" },
    { "LOG2",                   "10 PRINT LOG2(8);LOG2(1)",                                    "30\n" },
    { "LOG10",                  "10 PRINT LOG10(1000);LOG10(1)",                               "30\n" },
    { "LOG2 of zero",           "10 PRINT LOG2(0)",                                            "? Illegal argument in line 10\n" },
    { "LOG10 of a negative number", "10 PRINT LOG10(-1)",                                      "? Illegal argument in line 10\n" },
    { "IMP",                    "10 PRINT 0 IMP 0;0 IMP -1;-1 IMP 0;-1 IMP -1",                "-1-10-1\n" },
    { "NIMP",                   "10 PRINT 0 NIMP 0;0 NIMP -1;-1 NIMP 0;-1 NIMP -1",            "00-10\n" },
    { "EQV",                    "10 PRINT 5 EQV 5;6 EQV 3",                                    "-1-6\n" },
    { "NEQV",                   "10 PRINT 6 NEQV 3;5 NEQV 5",                                  "50\n" },
    { "IMP in a condition",     "10 IF 1 IMP 0 THEN PRINT \"a\" ELSE PRINT \"b\"",              "a\n" },
    { "SHL and ASL",            "10 PRINT 1 SHL 4;1 ASL 3",                                    "168\n" },
    { "ASR keeps the sign",     "10 PRINT -16 ASR 2;16 ASR 2",                                 "-44\n" },
    { "SHR fills with zeros",   "10 PRINT -16 SHR 28;16 SHR 2;1 SHR 40",                       "1540\n" },
    { "SHL is LSHIFT",          "10 PRINT (1 SHL 5)=(1 LSHIFT 5)",                             "-1\n" },
    { "TIME$ has 8 characters", "10 PRINT LEN(TIME$);MID$(TIME$,3,1)",                         "8:\n" },
    { "DATE$ has 10 characters", "10 PRINT LEN(DATE$);MID$(DATE$,5,1);MID$(DATE$,8,1)",        "10--\n" },
    { "DATETIME$ joins both",   "10 PRINT LEN(DATETIME$);MID$(DATETIME$,11,1);LEFT$(DATETIME$,10)=DATE$", "19 -1\n" },
    { "loop inside a function", "10 DEF FN A(N)\n20 T=0\n30 WHILE N>0\n40 T=T+N\n50 N=N-1\n60 WEND\n70 FN A = T\n80 END FN\n90 PRINT FN A(4)", "10\n" },
    { "WEND without WHILE",     "10 WEND",                                                 "? WEND without WHILE in line 10\n" },
    { "UEND without UNTIL",     "10 UEND",                                                 "? UEND without UNTIL in line 10\n" },
    { "WHILE without WEND",     "10 WHILE 1",                                              "? WHILE without WEND in line 10\n" },
    { "REPEAT without UNTIL",   "10 REPEAT",                                               "? REPEAT without UNTIL in line 10\n" },
    { "UNTIL without UEND",     "10 UNTIL 0",                                              "? UNTIL without UEND in line 10\n" },
    { "mismatched loop end",    "10 WHILE 1\n20 NEXT I",                                   "? NEXT without FOR in line 20\n" },
    { "WEND closes the wrong loop", "10 FOR I=1 TO 2\n20 WEND\n30 NEXT I",                 "? WEND without WHILE in line 20\n" },
    // END, POP, ON
    { "END",                    "PRINT 1:END:PRINT 2",                                     "1\n" },
    { "END in a program",       "10 PRINT 1\n20 END\n30 PRINT 2",                          "1\n" },
    { "END in IF",              "10 PRINT 1\n20 IF 1 THEN END\n30 PRINT 2",               "1\n" },
    { "END inside a subroutine", "10 GOSUB 100\n20 PRINT \"BACK\"\n30 END\n100 PRINT \"SUB\"\n110 END", "SUB\n" },
    { "END IF is not END",      "10 A=1\n20 IF A THEN\n30 PRINT 1\n40 END IF\n50 PRINT 2", "1\n2\n" },
    { "END UNLESS is not END",  "10 A=0\n20 UNLESS A THEN\n30 PRINT 1\n40 END UNLESS\n50 PRINT 2", "1\n2\n" },
    { "END FN is not END",      "10 DEF FN A(X)\n20 FN A = X\n30 END FN\n40 PRINT FN A(3)\n50 END", "3\n" },
    { "POP",                    "10 GOSUB 100\n20 PRINT \"NOT REACHED\"\n30 PRINT \"DONE\"\n40 END\n100 POP\n110 GOTO 30", "DONE\n" },
    { "POP without GOSUB",      "10 POP",                                                  "? Return stack underflow in line 10\n" },
    { "ON GOTO",                "10 N=2\n20 ON N GOTO 100,200,300\n30 PRINT \"NONE\"\n40 END\n100 PRINT \"A\":END\n200 PRINT \"B\":END\n300 PRINT \"C\":END", "B\n" },
    { "ON GOTO first",          "10 ON 1 GOTO 100,200\n20 PRINT \"NONE\"\n30 END\n100 PRINT \"A\":END\n200 PRINT \"B\":END", "A\n" },
    { "ON GOTO last",           "10 ON 3 GOTO 100,200,300\n20 PRINT \"NONE\"\n30 END\n100 PRINT \"A\":END\n200 PRINT \"B\":END\n300 PRINT \"C\":END", "C\n" },
    { "ON GOTO zero",           "10 ON 0 GOTO 100,200\n20 PRINT \"NONE\"\n30 END\n100 PRINT \"A\":END\n200 PRINT \"B\":END", "NONE\n" },
    { "ON GOTO too big",        "10 ON 3 GOTO 100,200\n20 PRINT \"NONE\"\n30 END\n100 PRINT \"A\":END\n200 PRINT \"B\":END", "NONE\n" },
    { "ON GOTO negative",       "10 ON 0-1 GOTO 100,200\n20 PRINT \"NONE\"\n30 END\n100 PRINT \"A\":END\n200 PRINT \"B\":END", "NONE\n" },
    { "ON GOTO fraction",       "10 ON 2.5 GOTO 100,200,300\n20 PRINT \"NONE\"\n30 END\n100 PRINT \"A\":END\n200 PRINT \"B\":END\n300 PRINT \"C\":END", "B\n" },
    { "ON GOTO one target",     "10 ON 1 GOTO 100\n20 PRINT \"NONE\"\n30 END\n100 PRINT \"A\"",  "A\n" },
    { "ON GOSUB",               "10 ON 2 GOSUB 100,200\n20 PRINT \"BACK\"\n30 END\n100 PRINT \"A\":RETURN\n200 PRINT \"B\":RETURN", "B\nBACK\n" },
    { "ON GOSUB out of range",  "10 ON 5 GOSUB 100,200\n20 PRINT \"BACK\"\n30 END\n100 PRINT \"A\":RETURN\n200 PRINT \"B\":RETURN", "BACK\n" },
    { "ON GO SUB",              "10 ON 1 GO SUB 100\n20 PRINT \"BACK\"\n30 END\n100 PRINT \"A\":RETURN", "A\nBACK\n" },
    { "ON with labels",         "10 ON 2 GOTO FIRST,SECOND\n20 END\n30 LABEL FIRST\n40 PRINT 1:END\n50 LABEL SECOND\n60 PRINT 2:END", "2\n" },
    { "ON in a loop",           "10 FOR I=1 TO 3\n20 ON I GOSUB 100,200,300\n30 NEXT I\n40 END\n100 PRINT \"A\":RETURN\n200 PRINT \"B\":RETURN\n300 PRINT \"C\":RETURN", "A\nB\nC\n" },
    { "ON with undefined line", "10 ON 1 GOTO 100",                                          "? Undefined line number 100 in line 10\n" },
    // DATA, READ, RESTORE
    { "DATA and READ",          "DATA 1,2,3:READ A,B,C:PRINT A+B+C",                        "6\n" },
    { "READ strings",           "DATA \"HI\",\"THERE\":READ A$,B$:PRINT A$+B$",             "HITHERE\n" },
    { "READ mixed types",       "DATA 5,\"X\",2.5:READ N,S$,M:PRINT N;S$;M",                "5X2.5\n" },
    { "DATA signs",             "DATA -3,+4,-0.5:READ A,B,C:PRINT A;B;C",                   "-34-0.5\n" },
    { "DATA string with comma", "DATA \"A,B\":READ S$:PRINT S$",                            "A,B\n" },
    { "DATA empty string",      "DATA \"\":READ A$:PRINT LEN(A$)",                          "0\n" },
    { "READ into array elements", "DIM A(3):DATA 7,8,9:READ A(0),A(2):PRINT A(0);A(2)",     "78\n" },
    { "READ into string array", "DIM S$(2):DATA \"A\",\"B\":READ S$(0),S$(1):PRINT S$(0)+S$(1)", "AB\n" },
    { "READ into integer",      "DATA 12:READ I%:PRINT I%",                                 "12\n" },
    { "READ with index expression", "DIM A(5):I=2:DATA 9:READ A(I+1):PRINT A(3)",           "9\n" },
    { "READ continues",         "DATA 1,2,3:READ A:READ B:READ C:PRINT A;B;C",              "123\n" },
    { "RESTORE",                "DATA 1,2:READ A:READ B:RESTORE:READ C:PRINT A;B;C",        "121\n" },
    { "DATA after READ",        "10 READ A,B\n20 PRINT A+B\n30 DATA 3,4",                   "7\n" },
    { "several DATA lines",     "10 DATA 1,2\n20 DATA 3\n30 READ A,B,C\n40 PRINT A;B;C",    "123\n" },
    { "READ in a loop",         "10 FOR I=1 TO 3\n20 READ A\n30 PRINT A*2\n40 NEXT I\n50 DATA 1,2,3", "2\n4\n6\n" },
    { "RESTORE line",           "10 DATA 1,2\n20 DATA 3,4\n30 DATA 5,6\n40 RESTORE 20\n50 READ A,B\n60 PRINT A;B", "34\n" },
    { "RESTORE line without DATA", "10 DATA 1,2\n20 DATA 3,4\n30 DATA 5,6\n40 RESTORE 25\n50 READ A,B\n60 PRINT A;B", "56\n" },
    { "RESTORE first line",     "10 DATA 1,2\n20 DATA 3,4\n40 RESTORE 5\n50 READ A\n60 PRINT A", "1\n" },
    { "RESTORE behind the last DATA", "10 DATA 1,2\n40 RESTORE 100\n50 READ A",              "? Out of DATA in line 50\n" },
    { "RESTORE label",          "10 DATA 1,2\n20 LABEL SECOND\n30 DATA 3,4\n40 RESTORE SECOND\n50 READ A\n60 PRINT A", "3\n" },
    { "RESTORE undefined label", "10 RESTORE NOWHERE",                                      "? Undefined label in line 10\n" },
    { "RESTORE forward line",   "10 RESTORE 30\n20 READ A\n25 PRINT A\n30 DATA 4,5\n40 DATA 6", "4\n" },
    { "Out of DATA",            "DATA 1:READ A,B",                                          "? Out of DATA\n" },
    { "Out of DATA without DATA", "READ A",                                                 "? Out of DATA\n" },
    { "Out of DATA with line",  "10 DATA 1\n20 READ A,B",                                   "? Out of DATA in line 20\n" },
    { "READ number from string item", "DATA \"X\":READ A",                                  "? Type mismatch (DATA item is not a number)\n" },
    { "READ string from number item", "DATA 5:READ A$",                                     "? Type mismatch (DATA item is not a string)\n" },
    { "DATA without items",     "DATA",                                                     "? Data item expected\n" },
    { "DATA with a variable",   "DATA A",                                                   "? Data item expected\n" },
    { "READ without variable",  "READ",                                                     "? Variable expected\n" },
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
    iodev_mem_clear();
    outsize = 0;
    out_append( "" );
}

static void vm_print( struct _vm_t* vm, void* usrdata, int channel, const char* text ) {
    if ( channel != 0 ) {       // make output on other channels visible
        char tag[24];
        snprintf( tag, sizeof(tag), "[#%d]", channel );
        out_append( tag );
    }
    out_append( text );
}

// text with zero bytes: they are shown as <0>
static void vm_printn( struct _vm_t* vm, void* usrdata, int channel, const char* text, size_t len ) {
    (void) vm; (void) usrdata; (void) channel;
    for ( size_t i=0; i < len; ++i ) {
        char one[8];
        if ( text[i] == '\0' ) {
            snprintf( one, sizeof(one), "<0>" );
        } else {
            one[0] = text[i]; one[1] = '\0';
        }
        out_append( one );
    }
}

static void vm_report( struct _vm_t* vm, void* usrdata, const char* text ) {
    out_append( text );
}

static const char* keys = 0;
static program_t*  current_pgm = 0;

// the host's part of SAVE: the "file" is described in the output, the program is written to memory
static bool vm_save( struct _vm_t* vm, void* usrdata, const char* filename, bool ascii ) {
    char* text = 0; size_t size = 0;
    FILE* f = open_memstream( &text, &size );
    if ( f == 0 ) {
        return false;
    }
    bool ok = save_program( current_pgm, f, ascii );
    fclose( f );
    out_append( "<save " );
    out_append( filename );
    out_append( ascii ? " ascii>\n" : " binary>\n" );
    if ( ascii ) {
        out_append( text );
    } else {
        for ( size_t i=0; i < size; ++i ) {
            char hex[4];
            snprintf( hex, sizeof(hex), "%02X ", (unsigned)(uint8_t) text[i] );
            out_append( hex );
        }
        out_append( "\n" );
    }
    free( text );
    return ok;
}

// LIST: the call is described in the output
static void list_end( char* buf, size_t size, const char* label, int32_t line ) {
    if ( label[0] ) {
        snprintf( buf, size, "%s", label );
    } else if ( line >= 0 ) {
        snprintf( buf, size, "%d", (int) line );
    } else {
        snprintf( buf, size, "*" );
    }
}

static bool vm_list( struct _vm_t* vm, void* usrdata, const char* firstlabel, int32_t firstline,
    const char* lastlabel, int32_t lastline ) {
    char a[80], b[80], text[200];
    list_end( a, sizeof(a), firstlabel, firstline );
    list_end( b, sizeof(b), lastlabel, lastline );
    snprintf( text, sizeof(text), "<list %s %s on channel %d>\n", a, b, vm->channel );
    out_append( text );
    return true;
}

static bool vm_delete( struct _vm_t* vm, void* usrdata, const char* firstlabel, int32_t firstline,
    const char* lastlabel, int32_t lastline ) {
    char a[80], b[80], text[200];
    list_end( a, sizeof(a), firstlabel, firstline );
    list_end( b, sizeof(b), lastlabel, lastline );
    snprintf( text, sizeof(text), "<delete %s %s>\n", a, b );
    out_append( text );
    return true;
}

static bool vm_renumber( struct _vm_t* vm, void* usrdata, const char* firstlabel, int32_t firstline,
    const char* lastlabel, int32_t lastline, int32_t step ) {
    char a[80], b[80], text[200];
    list_end( a, sizeof(a), firstlabel, firstline );
    list_end( b, sizeof(b), lastlabel, lastline );
    snprintf( text, sizeof(text), "<renumber %s %s step %d>\n", a, b, (int) step );
    out_append( text );
    return true;
}

static bool vm_merge( struct _vm_t* vm, void* usrdata, const char* name, bool append ) {
    char text[200];
    snprintf( text, sizeof(text), "<merge %s%s>\n", name, append ? " append" : "" );
    out_append( text );
    return true;
}

static bool vm_files( struct _vm_t* vm, void* usrdata, const char* spec ) {
    char text[200];
    snprintf( text, sizeof(text), "<files \"%s\">\n", spec );
    out_append( text );
    return true;
}

static int vm_inkey( struct _vm_t* vm, void* usrdata ) {
    return keys && *keys ? (uint8_t) *keys++ : 0;
}

// INPUT: the next line of the test input; like a terminal, the typed line is echoed
static char* vm_input( struct _vm_t* vm, void* usrdata, int channel ) {
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

// GET: the next character of the test input
static int vm_getkey( struct _vm_t* vm, void* usrdata, int channel ) {
    return keys && *keys ? (uint8_t) *keys++ : -1;
}

// the sound output: every call is logged as text; PLAYING( ch ) is true from a tone until SOUND OFF
static bool snd_busy[8];
static int snd_bpm = 60;

static void snd_reset( void ) {
    memset( snd_busy, 0, sizeof(snd_busy) );
    snd_bpm = 60;
}

static void snd_tone( void* usrdata, int chan, float freq, float freq2, float vol, float beats ) {
    char text[100];
    if ( freq2 != 0.0f ) {
        snprintf( text, sizeof(text), "<tone %d %g %g %g glide %g>\n", chan, (double) freq, (double) vol, (double) beats, (double) freq2 );
    } else {
        snprintf( text, sizeof(text), "<tone %d %g %g %g>\n", chan, (double) freq, (double) vol, (double) beats );
    }
    out_append( text );
    if ( freq > 0.0f ) snd_busy[chan] = true;
}

static void snd_setenv( void* usrdata, int chan, float a, float d, float s, float r ) {
    char text[100];
    if ( a < 0.0f ) {
        snprintf( text, sizeof(text), "<env %d off>\n", chan );
    } else {
        snprintf( text, sizeof(text), "<env %d %g %g %g %g>\n", chan, (double) a, (double) d, (double) s, (double) r );
    }
    out_append( text );
}

static void snd_setvib( void* usrdata, int chan, float rate, float depth ) {
    char text[60];
    snprintf( text, sizeof(text), "<vib %d %g %g>\n", chan, (double) rate, (double) depth );
    out_append( text );
}

static void snd_settrem( void* usrdata, int chan, float rate, float depth ) {
    char text[60];
    snprintf( text, sizeof(text), "<trem %d %g %g>\n", chan, (double) rate, (double) depth );
    out_append( text );
}

static void snd_setpitch( void* usrdata, int chan, float freq ) {
    char text[60];
    snprintf( text, sizeof(text), "<pitch %d %g>\n", chan, (double) freq );
    out_append( text );
}

static void snd_setvolume( void* usrdata, int chan, float vol ) {
    char text[60];
    snprintf( text, sizeof(text), "<volume %d %g>\n", chan, (double) vol );
    out_append( text );
}

static void snd_setwave( void* usrdata, int chan, int wave ) {
    char text[40];
    snprintf( text, sizeof(text), "<wave %d %d>\n", chan, wave );
    out_append( text );
}

static void snd_setbpm( void* usrdata, int bpm ) {
    char text[40];
    snprintf( text, sizeof(text), "<bpm %d>\n", bpm );
    out_append( text );
    snd_bpm = bpm;
}

static int snd_getbpm( void* usrdata ) {
    return snd_bpm;
}

static void snd_sync( void* usrdata, unsigned mask ) {
    char text[40];
    snprintf( text, sizeof(text), "<sync %u>\n", mask );
    out_append( text );
}

static void snd_silence( void* usrdata, int chan ) {
    char text[40];
    snprintf( text, sizeof(text), "<silence %d>\n", chan );
    out_append( text );
    for ( int i=0; i < 8; ++i ) {
        if ( chan < 0 || chan == i ) snd_busy[i] = false;
    }
}

static bool snd_isbusy( void* usrdata, int chan ) {
    return snd_busy[chan];
}

static const vm_sound_t test_sound = { snd_tone, snd_setwave, snd_setenv, snd_setvib, snd_settrem, snd_setpitch, snd_setvolume, snd_setbpm, snd_getbpm, snd_sync, snd_silence, snd_isbusy };

// a graphics host that logs every operation as "<gfx NAME args>" (an omitted argument is -); queries return the sum of their arguments
static int gfx_polls = 0;

static bool gfx_op( void* usrdata, int op, const double* a, int na, const char* s, size_t slen, uint8_t* out, size_t outsz, size_t* outlen,
    const char** err ) {
    (void) usrdata; (void) outsz; (void) err;
    char buf[64];
    out_append( "<gfx " );
    out_append( gfxops_opname( (uint16_t) op ) );
    for ( int i=0; i < na; ++i ) {
        if ( a[i] < -1.0e8 ) {
            out_append( " -" );
        } else {
            snprintf( buf, sizeof(buf), " %g", a[i] );
            out_append( buf );
        }
    }
    if ( s != 0 ) {
        snprintf( buf, sizeof(buf), " s(%zu)=", slen );
        out_append( buf );
        for ( size_t i=0; i < slen && i < 16U; ++i ) {
            snprintf( buf, sizeof(buf), "%s%u", i ? "," : "", (unsigned)(uint8_t) s[i] );
            out_append( buf );
        }
    }
    out_append( ">\n" );
    if ( out != 0 ) {
        memcpy( out, "abcd", 4 );
        *outlen = 4;
    }
    return true;
}

static bool gfx_query( void* usrdata, int q, const double* a, int na, double* result ) {
    (void) usrdata;
    if ( q == GFXQ_EVENT ) {
        *result = ++gfx_polls == 3 ? 3.0 : -1.0;        // a collision is reported at the third look
        return true;
    }
    double sum = 0.0;
    for ( int i=0; i < na; ++i ) {
        sum += a[i];
    }
    *result = sum;
    return true;
}

static const vm_gfx_t test_gfx = { gfx_op, gfx_query };

static void rt_report( struct _runtime_t* rt, void* usrdata, const char* text ) {
    out_append( text );
    if ( text[0] && text[ strlen( text ) - 1U ] != '\n' ) {
        out_append( "\n" );
    }
}

static void comp_report( struct _compiler_t* comp, void* usrdata, const char* text ) {
    out_append( text );
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
    // direct mode lines are appended to the code, so that functions defined by earlier lines stay valid
    uint32_t start = rt.codegen.codesize;
    if ( setjmp( jmp_fail ) == 0 ) {
        run_compiler( &comp );
        ok = cgen_from_program( &rt.codegen, &comp );
    }
    if ( !ok ) {
        return false;
    }
    vm_t vm;
    init_vm( &vm, &rt.codegen, &rt.varmem );
    vm.print    = vm_print;
    vm.printn   = vm_printn;
    vm.report   = vm_report;
    vm.inkey    = vm_inkey;
    vm.input    = vm_input;
    vm.getkey   = vm_getkey;
    vm.save     = vm_save;
    vm.list     = vm_list;
    vm.deletelines = vm_delete;
    vm.renumber = vm_renumber;
    vm.merge    = vm_merge;
    vm.files    = vm_files;
    vm.sound    = &test_sound;
    vm.gfx      = &test_gfx;
    gfx_polls   = 0;
    vm.maxsteps = UINT64_C(1000000);
    ok = vm_run_at( &vm, start );
    if ( ok && vm.request != VMREQ_NONE ) {
        // LOAD / RUN end the run and leave a request; the host (here: the test) would carry it out
        char text[200];
        if ( vm.request == VMREQ_LOAD ) {
            snprintf( text, sizeof(text), "<request LOAD %s>\n", vm.reqname );
        } else if ( vm.request == VMREQ_NEW ) {
            snprintf( text, sizeof(text), "<request NEW>\n" );
        } else if ( vm.request == VMREQ_MONITOR ) {
            snprintf( text, sizeof(text), "<request MONITOR>\n" );
        } else if ( vm.request == VMREQ_CHAIN ) {
            snprintf( text, sizeof(text), "<request CHAIN%s %s \"%s\" %d>\n", vm.reqmerge ? " MERGE" : "", vm.reqname,
                vm.reqlabel, (int) vm.reqline );
        } else if ( vm.request == VMREQ_EDIT ) {
            snprintf( text, sizeof(text), "<request EDIT \"%s\" %d>\n", vm.reqname, (int) vm.reqline );
        } else {
            snprintf( text, sizeof(text), "<request RUN \"%s\" %d>\n", vm.reqname, (int) vm.reqline );
        }
        out_append( text );
    }
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
    snd_reset();
    keys = tc->input;
    cleanup_runtime( &rt );
    init_runtime( &rt );
    rt.report = rt_report;
    rt.halt   = rt_halt;
    program_t pgm; init_program( &pgm );
    current_pgm = &pgm;
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

// cases that are built at run time, because they are too big to write down: the limits that the variable
// store used to have (name length 255, 1024 variables, 65535 array elements in total) are gone
static bool run_dynamic_cases( buf_t* tokbuf, size_t* pnum, size_t* pfailed ) {
    // 1. a variable with a 3000 character name
    char* longname = (char*) malloc( 3001U );
    char* src = (char*) malloc( 8000U );
    char* src2 = (char*) malloc( 200000U );
    if ( longname == 0 || src == 0 || src2 == 0 ) {
        return false;
    }
    memset( longname, 'N', 3000U );
    longname[3000] = '\0';
    snprintf( src, 8000U, "%s=5\nPRINT %s*2", longname, longname );
    testcase_t tc1 = { "variable with a 3000 character name", src, "10\n", 0 };
    // 2. 2000 variables
    size_t n = 0;
    for ( int i=0; i < 2000; ++i ) {
        char nm[8] = { 'V', (char)( 'A' + ( i / 17576 ) % 26 ), (char)( 'A' + ( i / 676 ) % 26 ), (char)( 'A' + ( i / 26 ) % 26 ),
            (char)( 'A' + i % 26 ), '\0' };
        n += (size_t) snprintf( src2 + n, 200000U - n, "%s%s=%d", i ? ":" : "", nm, i );
    }
    n += (size_t) snprintf( src2 + n, 200000U - n, "\nPRINT VAAAA;VABOC;VACYX" );
    testcase_t tc2 = { "2000 variables", src2, "010421999\n", 0 };
    // 3. the remaining limit: a dimension is a 16-bit value
    testcase_t tc3 = { "one dimension is limited to 65535", "DIM A(70000)", "? Dimension out of range\n", 0 };
    // 4. an array with several big dimensions
    testcase_t tc4 = { "array of 300 x 300 elements", "DIM A(300,300):A(299,299)=3:A(1,0)=2:A(0,1)=5:PRINT A(299,299);A(1,0);A(0,1)", "325\n", 0 };
    // 5. identical constants are stored once: 600 lines with the same 200 character string need 120000 bytes of data
    //    without sharing (the data segment holds 65536), and the same for one decimal number
    char* src5 = (char*) malloc( 400000U );
    size_t n5 = 0;
    for ( int i=1; i <= 600; ++i ) {
        n5 += (size_t) snprintf( src5 + n5, 400000U - n5, "%d A$=\"", i * 10 );
        for ( int j=0; j < 200; ++j ) {
            src5[ n5++ ] = (char)( 'A' + j % 26 );
        }
        n5 += (size_t) snprintf( src5 + n5, 400000U - n5, "\":B=B+%d.5\n", 1 );
    }
    snprintf( src5 + n5, 400000U - n5, "6010 PRINT LEN(A$);B" );
    testcase_t tc5 = { "constants are shared", src5, "200900\n", 0 };
    // 6. code and data beyond 64 KiB: 2500 filler lines with distinct strings (about 125 KB of code, 100 KB of data) come
    //    first, and then everything that needs a far address: jumps, GOSUB, FOR, ON, a function, a label, DATA with
    //    RESTORE, and line numbers above 65535 (also in an error message)
    char* src6 = (char*) malloc( 400000U );
    size_t n6 = 0;
    n6 += (size_t) snprintf( src6 + n6, 400000U - n6, "10 GOTO 50000\n" );
    for ( int i=0; i < 2500; ++i ) {
        n6 += (size_t) snprintf( src6 + n6, 400000U - n6, "%d A=A+1.5:B$=\"FILLER-LINE-%06d-0123456789ABCDEFGHIJ\":C=C*2+A\n", 100 + i, i );
    }
    n6 += (size_t) snprintf( src6 + n6, 400000U - n6,
        "50000 PRINT \"FAR\"\n50010 GOSUB 60000\n50020 FOR I=1 TO 3:PRINT I;:NEXT:PRINT\n50030 RESTORE 55000\n"
        "50040 READ X,Y$:PRINT X;Y$\n50050 DEF FN F(X)=X*2\n50060 PRINT FN F(21)\n50070 GOTO FARL\n50080 PRINT \"SKIPPED\"\n"
        "50090 LABEL FARL\n50100 ON 2 GOTO 50110,50130\n50110 PRINT \"BAD\"\n50120 END\n50130 PRINT \"ON OK\"\n"
        "50140 GOTO 70000\n55000 DATA 7,\"SEVEN\"\n60000 PRINT \"SUB\":RETURN\n70000 PRINT \"L70000\"\n70001 PRINT 1/0" );
    testcase_t tc6 = { "code and data beyond 64 KiB", src6, "FAR\nSUB\n123\n7SEVEN\n42\nON OK\nL70000\n? Division by zero in line 70001\n", 0 };
    const testcase_t* dyn[] = { &tc1, &tc2, &tc3, &tc4, &tc5, &tc6 };
    for ( size_t i=0; i < sizeof(dyn) / sizeof(dyn[0]); ++i ) {
        ++*pnum;
        if ( run_case( dyn[i], tokbuf ) ) {
            printf( "PASS  %s\n", dyn[i]->name );
        } else {
            printf( "FAIL  %s\n", dyn[i]->name );
            show( "expected", dyn[i]->expected );
            show( "got     ", outbuf );
            ++*pfailed;
        }
    }
    free( longname );
    free( src );
    free( src2 );
    free( src5 );
    free( src6 );
    return true;
}

// read_program_header(): binary file accepted and consumed, text rewound, unknown version / truncated header rejected
static bool check_one_header( const char* name, const uint8_t* data, size_t size, bool expect, uint8_t expectver, long expectpos ) {
    FILE* f = fmemopen( (void*) data, size, "rb" );
    if ( f == 0 ) {
        return false;
    }
    uint8_t ver = 99;
    bool ok = read_program_header( f, &ver );
    long pos = ftell( f );
    fclose( f );
    bool pass = ok == expect && ver == expectver && pos == expectpos;
    printf( "%s  %s\n", pass ? "PASS" : "FAIL", name );
    return pass;
}

static void run_header_cases( size_t* pnum, size_t* pfailed ) {
    static const uint8_t good[]   = { 0x89, 'E', '9', '9', 1, 0, 0, 10 };
    static const uint8_t future[] = { 0x89, 'E', '9', '9', 7, 0, 0, 10 };
    static const uint8_t cut[]    = { 0x89, 'E', '9', '9' };
    static const uint8_t text[]   = { '1', '0', ' ', 'P', 'R', 'I', 'N', 'T' };
    bool r[5] = {
        check_one_header( "program header: binary file", good, sizeof(good), true, 1, 5 ),
        check_one_header( "program header: unknown version", future, sizeof(future), false, 7, 5 ),
        check_one_header( "program header: truncated", cut, sizeof(cut), false, 0, 4 ),
        check_one_header( "program header: text file is rewound", text, sizeof(text), false, 0, 0 ),
        check_one_header( "program header: empty file", text, 0, false, 0, 0 ),
    };
    for ( size_t i=0; i < 5; ++i ) {
        ++*pnum;
        if ( !r[i] ) {
            ++*pfailed;
        }
    }
}

int main( int argc, char** argv ) {
    if ( argc > 1 && strcmp( argv[1], "-v" ) == 0 ) {
        basdbg_flags = BASDBG_PARSE | BASDBG_CODE;
    }
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
    if ( !run_dynamic_cases( tokbuf, &numcases, &failed ) ) {
        ++failed;
    }
    run_header_cases( &numcases, &failed );
    cleanup_compiler( &comp );
    cleanup_runtime( &rt );
    delete_buffer( tokbuf );
    free( outbuf );
    printf( "%zu of %zu passed\n", numcases - failed, numcases );
    return failed > 255U ? 255 : (int) failed;
}
