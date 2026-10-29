
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
    { "PRINT channel is not sticky", "PRINT #3,\"A\":PRINT \"B\"",              "[#3]A[#3]\nB\n" },
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
    { "CHR$ zero",              "PRINT CHR$(0)",                                  "? Illegal argument\n" },
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
    { "PLAY error queues nothing", "PLAY \"C\",\"X\"",                              "? Unknown music command 'X'\n" },
    { "PLAY note out of range", "PLAY \"O0 C\"",                                   "? Note out of range\n" },
    { "PLAY too many voices",   "PLAY \"R\",\"R\",\"R\",\"R\",\"R\",\"R\",\"R\",\"R\",\"R\"", "? End of line expected\n" },
    { "PLAY needs a string",    "PLAY 5",                                          "? String expression expected\n" },
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
    { "LIST uses channel 0",    "PRINT #2,\"X\"\nLIST",                              "[#2]X[#2]\n<list * * on channel 0>\n" },
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

static void snd_tone( void* usrdata, int chan, float freq, float vol, float beats ) {
    char text[100];
    snprintf( text, sizeof(text), "<tone %d %g %g %g>\n", chan, (double) freq, (double) vol, (double) beats );
    out_append( text );
    if ( freq > 0.0f ) snd_busy[chan] = true;
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

static const vm_sound_t test_sound = { snd_tone, snd_setwave, snd_setbpm, snd_getbpm, snd_sync, snd_silence, snd_isbusy };

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
    vm.report   = vm_report;
    vm.inkey    = vm_inkey;
    vm.input    = vm_input;
    vm.getkey   = vm_getkey;
    vm.save     = vm_save;
    vm.list     = vm_list;
    vm.deletelines = vm_delete;
    vm.renumber = vm_renumber;
    vm.sound    = &test_sound;
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
