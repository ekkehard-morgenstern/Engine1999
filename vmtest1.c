
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
    const char* input;      // keys for INKEY$ (optional)
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
    { "SAVE binary default",    "10 PRINT 1\n20 SAVE \"X\"",                       "1\n<save X binary>\n00 00 0A 00 0A 20 0B 20 31 00 00 00 14 00 0D 20 14 20 03 00 01 58 00 \n" },
    { "SAVE ascii",             "10 PRINT 1\n20 SAVE \"X\",A",                     "1\n<save X ascii>\n10 PRINT 1\n20 SAVE \"X\",A\n" },
    { "SAVE binary option",     "10 PRINT 1\n20 SAVE \"X\",B",                     "1\n<save X binary>\n00 00 0A 00 0A 20 0B 20 31 00 00 00 14 00 12 20 14 20 03 00 01 58 2C 8A 00 01 42 00 \n" },
    { "SAVE of empty program",  "SAVE \"X\",A",                                    "<save X ascii>\n" },
    { "SAVE with name expression", "N$=\"AB\"\nSAVE N$+\"C\",A",                   "<save ABC ascii>\n" },
    { "SAVE bad mode",          "SAVE \"X\",Z",                                    "? Bad SAVE mode\n" },
    { "SAVE empty name",        "SAVE \"\"",                                       "? Illegal argument\n" },
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

static int vm_inkey( struct _vm_t* vm, void* usrdata ) {
    return keys && *keys ? (uint8_t) *keys++ : 0;
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
    // direct mode lines are appended to the code, so that functions defined by earlier lines stay valid
    uint32_t start = rt.codegen.codesize;
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
    vm.inkey    = vm_inkey;
    vm.save     = vm_save;
    vm.maxsteps = UINT64_C(1000000);
    ok = vm_run_at( &vm, start );
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
    const testcase_t* dyn[] = { &tc1, &tc2, &tc3, &tc4 };
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
    return true;
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
    if ( !run_dynamic_cases( tokbuf, &numcases, &failed ) ) {
        ++failed;
    }
    cleanup_compiler( &comp );
    cleanup_runtime( &rt );
    delete_buffer( tokbuf );
    free( outbuf );
    printf( "%zu of %zu passed\n", numcases - failed, numcases );
    return failed > 255U ? 255 : (int) failed;
}
