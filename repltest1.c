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

#include <unistd.h>
#include <sys/stat.h>
#include "basrepl.h"
#include "basdebug.h"
#include "instset.h"

/*
Automated test of the REPL (basrepl.[ch]) without any host: lines are fed to repl_enter() like a user would type
them, the output and the error messages are captured and compared with the expected text. The keyboard callbacks
read from the "input" text of a case (INPUT takes lines, GET takes characters). The exit status is the number of
failed cases.

Usage: repltest1 [-v]       -v: show all debug output (see basdebug.h)
*/

// the program files of the LOAD cases (in the current directory; removed at the end)
#define TESTFILE    "repltest1.tmp"

static void cleanup_testfiles( void ) {
    static const char* const names[] = { TESTFILE, "repltest1x", "repltest1x.bas", "repltest1x.e99", "repltest1x.dat", "repltest1x.zzz" };
    for ( size_t i=0; i < sizeof(names) / sizeof(names[0]); ++i ) {
        remove( names[i] );
    }
}

typedef struct _testcase_t {
    const char* name;
    const char* source;     // lines separated by '\n'
    const char* expected;
    const char* input;      // keyboard input (optional)
    unsigned    breakafter; // the interrupt callback says "break" from this poll on (0 = never)
} testcase_t;

static const testcase_t cases[] = {
    { "RENUM adjusts a jump with colon statements", "10 A=0\n20 IF A THEN GOSUB 100:PRINT \"x\"\n100 PRINT \"s\":RETURN\nRENUM\nLIST", "10 A=0\n20 IF A THEN GOSUB 30:PRINT \"x\"\n30 PRINT \"s\":RETURN\n" },
    // indentation: the blanks behind the line number are kept (the compiler ignores them)
    { "indent is kept by LIST",   "10 FOR I=1 TO 2\n20     PRINT I\n30 NEXT\nLIST",    "10 FOR I=1 TO 2\n20     PRINT I\n30 NEXT\n" },
    { "an indented program runs", "10 FOR I=1 TO 2\n20     PRINT I\n30 NEXT\nRUN",     "1\n2\n" },
    { "no blank after the number: the listing has one", "10PRINT 1\n20PRINT 2\nLIST\nRUN", "10 PRINT 1\n20 PRINT 2\n1\n2\n" },
    { "a line number alone still deletes the line", "10 PRINT 1\n10\nLIST",        "" },
    { "indent: a tab after the number is the separator", "10\tPRINT 1\nLIST", "10 PRINT 1\n" },
    { "indent: every tab is one blank", "10\t\tPRINT 1\nLIST",                   "10  PRINT 1\n" },
    { "indent: blanks inside the line are still one", "10    PRINT   1  +  2\nLIST", "10    PRINT 1 + 2\n" },
    { "indent: blanks before the number are one", "   10   PRINT 1\nLIST",        "10   PRINT 1\n" },
    { "indent is kept by RENUM", "5   A=1\n7     GOTO 5\nRENUM\nLIST",           "10   A=1\n20     GOTO 10\n" },
    { "indent: blanks in a string are kept, in the rest of the line they are one", "10  PRINT \"a   b\"  : ' c   d\nLIST", "10  PRINT \"a   b\" : ' c d\n" },
    { "indent survives SAVE ,A, SAVE, LOAD and MERGE APPEND", "10 FOR I=1 TO 2\n20   PRINT I\n30 NEXT\nSAVE \"mem:t\",A\nSAVE \"mem:b\",B\nNEW\nLOAD \"mem:t\"\nLIST\nNEW\nLOAD \"mem:b\"\nMERGE \"mem:t\",APPEND\nLIST 40-", "10 FOR I=1 TO 2\n20   PRINT I\n30 NEXT\n40 FOR I=1 TO 2\n50   PRINT I\n60 NEXT\n" },
    { "indent in an IF block", "10 IF 1 THEN\n20   PRINT 7\n30 ENDIF\nRUN",     "7\n" },
    { "direct mode",            "PRINT 1+2",                                      "3\n" },
    { "KEY is an error in a host that can't tell",  "PRINT KEY(\"A\")",               "? KEY is not available in this host\n" },
    { "KEY: the name is checked first",             "PRINT KEY(\"BANANA\")",          "? Illegal argument\n" },
    { "SOUND without a sound output does nothing", "SOUND 1,440,1\nPRINT 1",     "1\n" },
    { "SFX persists between direct mode lines", "SFX 1,\"C\"\nSFX 1,1\nSFX 2,1",   "? Undefined effect\n" },
    { "SFX is checked when defined, without a sound output", "SFX 1,\"Z\"",         "? Unknown music command 'Z'\n" },
    { "INSTRUMENT is checked without a sound output", "INSTRUMENT 5,\"OSC SINE\"\nINSTRUMENT 5\nINSTRUMENT 5,\"OSC BOGUS\"", "? Wave name expected after OSC (SINE SQUARE TRIANGLE NOISE SAW)\n" },
    { "WAVE takes instruments, INSTRUMENT is listed", "10 WAVE 1,9\n20 INSTRUMENT 20,\"OSC SAW;FILTER 3\"\n30 INSTRUMENT 20\n40 PLAY \"I5 C\"\nLIST", "10 WAVE 1,9\n20 INSTRUMENT 20,\"OSC SAW;FILTER 3\"\n30 INSTRUMENT 20\n40 PLAY \"I5 C\"\n" },
    { "INSTRUMENT in a program without a sound output", "10 INSTRUMENT 20,\"OSC SAW;FILTER 3\":PLAY \"I20 C\":PRINT \"OK\"\nRUN", "OK\n" },
    { "PLAY I 32 is refused", "PLAY \"I32 C\"", "? Value missing or out of range after 'I'\n" },
    { "SFX is gone after RUN", "SFX 1,\"C\"\n10 SFX 1,1\nRUN",                     "? Undefined effect in line 10\n" },
    { "SFX defined in a program", "10 SFX 1,\"C\"\n20 SFX 1,1\n30 PRINT \"OK\"\nRUN", "OK\n" },
    { "effect statements without a sound output", "ENVELOPE 1,0,0,1,0:VIBRATO 1,5,1:TREMOLO 1,4,0.5:PITCH 1,440:VOLUME 1,0.5:SOUND 1,440,1,0.5,880:PRINT 1", "1\n" },
    { "effect statements are listed", "10 PITCH 1,440\n20 VOLUME 1,0.5\n30 ENVELOPE 1\n40 ENVELOPE 1,0,0.1,0.5,0.2\n50 VIBRATO 1,5,0.5\n60 TREMOLO 1,4,0.5\n70 SFX 1,\"S100,200\"\n80 SFX 1\n90 SFX 1,2\n100 SOUND 1,440,1,0.5,880\nLIST", "10 PITCH 1,440\n20 VOLUME 1,0.5\n30 ENVELOPE 1\n40 ENVELOPE 1,0,0.1,0.5,0.2\n50 VIBRATO 1,5,0.5\n60 TREMOLO 1,4,0.5\n70 SFX 1,\"S100,200\"\n80 SFX 1\n90 SFX 1,2\n100 SOUND 1,440,1,0.5,880\n" },
    { "file system statements are listed", "10 KILL \"a\"\n20 RENAME \"a\" TO \"b\"\n30 MKDIR \"d\"\n40 CHDIR \"d\"\n50 SEEK #1,2\nLIST", "10 KILL \"a\"\n20 RENAME \"a\" TO \"b\"\n30 MKDIR \"d\"\n40 CHDIR \"d\"\n50 SEEK #1,2\n" },
    { "RENAME and KILL on mem: in direct mode", "OPEN \"mem:q\" FOR OUTPUT AS #1:PRINT #1,\"ab\":CLOSE\nRENAME \"mem:q\" TO \"mem:r\"\nOPEN \"mem:r\" FOR INPUT AS #2:PRINT LOF(2):CLOSE\nKILL \"mem:r\"\nOPEN \"mem:r\" FOR INPUT AS #2", "3\n? File not found\n" },
    { "random access statements are listed", "10 OPEN \"r.dat\" FOR RANDOM AS #1 LEN=20\n20 FIELD #1,10 AS A$,10 AS B$\n30 LSET A$=\"x\":RSET B$=\"y\"\n40 PUTREC #1,2\n50 GETREC #1\n60 OPEN \"R\",#2,\"s.dat\",64\nLIST", "10 OPEN \"r.dat\" FOR RANDOM AS #1 LEN=20\n20 FIELD #1,10 AS A$,10 AS B$\n30 LSET A$=\"x\":RSET B$=\"y\"\n40 PUTREC #1,2\n50 GETREC #1\n60 OPEN \"R\",#2,\"s.dat\",64\n" },
    { "a random file in direct mode", "OPEN \"mem:q\" FOR RANDOM AS #1 LEN=4\nFIELD #1,4 AS A$\nLSET A$=\"abcd\":PUTREC #1,2\nGETREC #1,2:PRINT A$;LOF(1)", "abcd8\n" },
    { "ON ERROR statements are listed", "10 ON ERROR GOTO 100\n20 ON ERROR GOSUB 100\n30 ON ERROR OFF\n40 PRINT ERR$;ERL\n100 END\nLIST", "10 ON ERROR GOTO 100\n20 ON ERROR GOSUB 100\n30 ON ERROR OFF\n40 PRINT ERR$;ERL\n100 END\n" },
    { "extension words are listed", "10 PRINT MIN(1,2);MAX(3,4);ATAN2(1,1);SPACE$(2);STRING$(3,\"x\");STRING$(2,65)\nLIST", "10 PRINT MIN(1,2);MAX(3,4);ATAN2(1,1);SPACE$(2);STRING$(3,\"x\");STRING$(2,65)\n" },
    { "extension words in lower case", "10 print min(1,2);space$(1);string$(2,66)\nLIST", "10 PRINT MIN(1,2);SPACE$(1);STRING$(2,66)\n" },
    { "ON KEY and KEY$ are listed", "10 ON KEY GOSUB 100:ON KEY OFF:A$=KEY$(\"EVENT\"):B=KEY(\"A\")\nLIST", "10 ON KEY GOSUB 100:ON KEY OFF:A$=KEY$(\"EVENT\"):B=KEY(\"A\")\n" },
    { "JOY$ is listed", "10 A$=JOY$(\"NAME\",2)+JOY$(\"name\")\nLIST", "10 A$=JOY$(\"NAME\",2)+JOY$(\"name\")\n" },
    { "FRAME is listed", "10 FRAME:FRAME 3:IF X THEN FRAME ELSE FRAME 2\nLIST", "10 FRAME:FRAME 3:IF X THEN FRAME ELSE FRAME 2\n" },
    { "FRAME in lower case", "10 frame 4\nLIST", "10 FRAME 4\n" },
    { "JOY is listed", "10 PRINT JOY(\"A\",2):JOY DEADZONE 0.1:ON JOY GOSUB 100:ON JOY OFF:X=JOY(\"LX\")\nLIST", "10 PRINT JOY(\"A\",2):JOY DEADZONE 0.1:ON JOY GOSUB 100:ON JOY OFF:X=JOY(\"LX\")\n" },
    { "JOY in lower case", "10 print joy(\"b\"):joy deadzone 0.3:on joy goto 20\nLIST", "10 PRINT JOY(\"b\"):JOY DEADZONE 0.3:ON JOY GOTO 20\n" },
    { "ERROR and ERR are listed", "10 ON ERROR GOTO 0\n20 ERROR 5\n30 PRINT ERR;ERR$\nLIST", "10 ON ERROR GOTO 0\n20 ERROR 5\n30 PRINT ERR;ERR$\n" },
    { "number packing functions are listed", "10 A$=MKI$(1)+MKL$(2)+MKS$(3)+MKD$(4)\n20 PRINT CVI(A$);CVL(A$);CVS(A$);CVD(A$)\nLIST", "10 A$=MKI$(1)+MKL$(2)+MKS$(3)+MKD$(4)\n20 PRINT CVI(A$);CVL(A$);CVS(A$);CVD(A$)\n" },
    { "graphics statements are listed", "10 GFX LAYER 3,ON\n20 GFX PSET 1,2,3\n30 GFX LINE 1,2,3,4,5,BF\n40 TXT FILL 0,0,9,9,32\n50 TILE SET 1,2,3\n60 SPRITE 1,ON\n70 SPRITE 1,10,20,3\n80 SPRITE 1,MOVETO 5,6,7\n90 SPRITE 1,COLLISION ON,TILES\n100 CLS:LOCATE 1,2:COLOR 3,4\n110 GFX GRAB 0,0,9,9,A$\n120 PRINT POINT(1,2);RGB(1,2,3);SPRITE(\"X\",1);GFX(\"FRAME\")\n130 ON COLLISION GOSUB 10\n140 ON COLLISION OFF\n150 DESIGN SPRITE 3,16\n160 @ \"1.2.3.4.5.6.7.8.\"\nLIST", "10 GFX LAYER 3,ON\n20 GFX PSET 1,2,3\n30 GFX LINE 1,2,3,4,5,BF\n40 TXT FILL 0,0,9,9,32\n50 TILE SET 1,2,3\n60 SPRITE 1,ON\n70 SPRITE 1,10,20,3\n80 SPRITE 1,MOVETO 5,6,7\n90 SPRITE 1,COLLISION ON,TILES\n100 CLS:LOCATE 1,2:COLOR 3,4\n110 GFX GRAB 0,0,9,9,A$\n120 PRINT POINT(1,2);RGB(1,2,3);SPRITE(\"X\",1);GFX(\"FRAME\")\n130 ON COLLISION GOSUB 10\n140 ON COLLISION OFF\n150 DESIGN SPRITE 3,16\n160 @ \"1.2.3.4.5.6.7.8.\"\n" },
    { "graphics without a host does nothing", "GFX PSET 1,2:SPRITE 1,ON:PRINT POINT(1,2);\"ok\"", "0ok\n" },
    { "numeric functions and operators are listed", "10 A=INT(RND(1)*6)+1:B=SQR(ABS(SIN(PI)))\n20 C=7\\2+7 MOD 3:RANDOMIZE:RANDOMIZE 5\n30 D=FIX(EXP(LOG(2)))+SGN(ATN(COS(TAN(1))))\nLIST", "10 A=INT(RND(1)*6)+1:B=SQR(ABS(SIN(PI)))\n20 C=7\\2+7 MOD 3:RANDOMIZE:RANDOMIZE 5\n30 D=FIX(EXP(LOG(2)))+SGN(ATN(COS(TAN(1))))\n" },
    { "DEG RAD GRAD are listed", "10 DEG:PRINT SIN(90)\n20 RAD\n30 GRAD\nLIST", "10 DEG:PRINT SIN(90)\n20 RAD\n30 GRAD\n" },
    { "the angle unit lasts over direct mode lines", "DEG\nPRINT SIN(90)\nPRINT COS(180)", "1\n-1\n" },
    { "RUN returns to radians", "DEG\n10 PRINT SIN(90)\nRUN", "0.893996663600558\n" },
    { "PLAYING without a sound output is 0", "PRINT PLAYING(1)",                  "0\n" },
    { "PLAY checks its strings without a sound output", "PLAY \"Z\"",               "? Unknown music command 'Z'\n" },
    { "sound statements are listed", "10 SOUND OFF\n20 PLAY \"C\",\"E\"\n30 TEMPO 90\n40 SOUND SYNC 1,2\n50 WAVE 1,2\n60 SOUND 1,440,1,0.5\n70 PRINT PLAYING(1)\nLIST", "10 SOUND OFF\n20 PLAY \"C\",\"E\"\n30 TEMPO 90\n40 SOUND SYNC 1,2\n50 WAVE 1,2\n60 SOUND 1,440,1,0.5\n70 PRINT PLAYING(1)\n" },
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
    { "SAVE without extension makes .e99, LOAD finds it", "10 PRINT \"E\"\nSAVE \"repltest1x\",B\nNEW\nLOAD \"repltest1x.e99\"\nRUN\nNEW\nLOAD \"repltest1x\"\nRUN", "E\nE\n" },
    { "SAVE without a name and nothing loaded", "10 PRINT 1\nSAVE", "? No file name (LOAD or SAVE a file first, or give a name)\n? Save failed\n" },
    { "SAVE without a name writes the file of the last LOAD", "10 PRINT 1\nSAVE \"mem:t\"\nNEW\nLOAD \"mem:t\"\n20 PRINT 2\nSAVE\nNEW\nLOAD \"mem:t\"\nLIST", "10 PRINT 1\n20 PRINT 2\n" },
    { "SAVE without a name after a SAVE", "10 PRINT 1\nSAVE \"mem:t\"\n20 PRINT 2\nSAVE\nNEW\nLOAD \"mem:t\"\nLIST", "10 PRINT 1\n20 PRINT 2\n" },
    { "SAVE without a name keeps the format of the loaded file (binary)", "10 PRINT 3\nSAVE \"mem:u\",B\nNEW\nLOAD \"mem:u\"\n20 PRINT 4\nSAVE\nOPEN \"mem:u.e99\" FOR INPUT AS #1\nPRINT ASC(INPUT$(1,#1))\nCLOSE #1\nNEW\nLOAD \"mem:u.e99\"\nLIST", "137\n10 PRINT 3\n20 PRINT 4\n" },
    { "SAVE without a name keeps the format of the loaded file (ASCII)", "10 PRINT 3\nSAVE \"mem:u\"\nNEW\nLOAD \"mem:u\"\n20 PRINT 4\nSAVE\nOPEN \"mem:u.bas\" FOR INPUT AS #1\nPRINT ASC(INPUT$(1,#1))\nCLOSE #1", "49\n" },
    { "SAVE ,A without a name swaps .e99 for .bas", "10 PRINT 3\nSAVE \"mem:u\",B\nNEW\nLOAD \"mem:u\"\n20 PRINT 4\nSAVE ,A\nNEW\nLOAD \"mem:u.bas\"\nLIST", "10 PRINT 3\n20 PRINT 4\n" },
    { "SAVE without a name after a binary SAVE stays binary", "10 PRINT 3\nSAVE \"mem:u\",B\n20 PRINT 4\nSAVE\nOPEN \"mem:u.e99\" FOR INPUT AS #1\nPRINT ASC(INPUT$(1,#1))", "137\n" },
    { "SAVE ,B without a name swaps .bas for .e99", "10 PRINT 5\nSAVE \"mem:w\"\nNEW\nLOAD \"mem:w\"\n20 PRINT 6\nSAVE ,B\nNEW\nLOAD \"mem:w.e99\"\nLIST", "10 PRINT 5\n20 PRINT 6\n" },
    { "LIBRARY and LOAD LIBRARY lines are stored and listed", "10 LIBRARY \"x\",\"X_\"\n20 LOAD LIBRARY \"y\"\n30 PRINT 7\nLIST", "10 LIBRARY \"x\",\"X_\"\n20 LOAD LIBRARY \"y\"\n30 PRINT 7\n" },
    { "LOAD LIBRARY finds mem:m.lib and the program runs", "10 LIBRARY \"m\"\nSAVE \"mem:m.lib\"\nNEW\n10 LOAD LIBRARY \"mem:m\"\n20 PRINT 1\nRUN", "1\n" },
    { "LOAD LIBRARY finds .l99 and .bas, too", "10 LIBRARY \"m2\"\nSAVE \"mem:m2.l99\",B\nNEW\n10 LIBRARY \"m3\"\nSAVE \"mem:m3.bas\"\nNEW\n10 LOAD LIBRARY \"mem:m2\"\n20 LOAD LIBRARY \"mem:m3\"\n30 PRINT 2\nRUN", "2\n" },
    { "LOAD LIBRARY looks in lib/ below the directory", "10 LIBRARY \"q\"\nSAVE \"mem:lib/q.lib\"\nNEW\n10 LOAD LIBRARY \"mem:q\"\n20 PRINT 3\nRUN", "3\n" },
    { "LOAD LIBRARY with an explicit extension", "10 LIBRARY \"m\"\nSAVE \"mem:m.lib\"\nNEW\n10 LOAD LIBRARY \"mem:m.lib\"\n20 PRINT 4\nRUN", "4\n" },
    { "LOAD LIBRARY: a library that is not there", "10 LOAD LIBRARY \"mem:nothere\"\nRUN", "? Library not found: mem:nothere in line 10\n" },
    { "LOAD LIBRARY: an absolute name is refused", "10 LOAD LIBRARY \"/etc/x\"\nRUN", "? Library /etc/x: the name may only point down from the current directory (no absolute path, no ..) in line 10\n" },
    { "LOAD LIBRARY: .. is refused", "10 LOAD LIBRARY \"../x\"\nRUN", "? Library ../x: the name may only point down from the current directory (no absolute path, no ..) in line 10\n" },
    { "LOAD LIBRARY behind IF is refused", "10 IF 1 THEN LOAD LIBRARY \"mem:m\"\nRUN", "? LOAD LIBRARY must be a plain statement, not behind IF / THEN / ELSE in line 10\n" },
    { "LOAD LIBRARY of an ordinary program", "10 PRINT 5\nSAVE \"mem:p.bas\"\nNEW\n10 LOAD LIBRARY \"mem:p\"\nRUN", "? Not a library: mem:p (its first statement must be LIBRARY \"name\") in line 10\n" },
    { "LOAD LIBRARY: a cycle", "10 LIBRARY \"b\"\n20 LOAD LIBRARY \"mem:a\"\nSAVE \"mem:b.lib\"\nNEW\n10 LIBRARY \"a\"\n20 LOAD LIBRARY \"mem:b\"\nSAVE \"mem:a.lib\"\nNEW\n10 LOAD LIBRARY \"mem:a\"\nRUN", "? Library cycle (a, b, a) in line 20 of the library b\n" },
    { "FOR loops of a library and of the program do not share hidden variables", "10 LIBRARY \"lf\",\"LF_\"\n20 FUNC LF_F(X)\n30 LOCAL J,S\n40 FOR J=1 TO 2\n50 S=S+J\n60 NEXT J\n70 RETURN S\n80 END FUNC\nSAVE \"mem:lf.lib\"\nNEW\n10 LOAD LIBRARY \"mem:lf\"\n20 FOR I=1 TO 4\n30 X=FN LF_F(0)\n40 PRINT I;\n50 NEXT I\n60 PRINT\nRUN", "1234\n" },
    { "LOAD LIBRARY: a library that needs another", "10 LIBRARY \"in\"\nSAVE \"mem:in.lib\"\nNEW\n10 LIBRARY \"out\"\n20 LOAD LIBRARY \"mem:in\"\nSAVE \"mem:out.lib\"\nNEW\n10 LOAD LIBRARY \"mem:out\"\n20 PRINT 6\nRUN", "6\n" },
    { "LOAD LIBRARY: a library with a wrong declared name", "10 LIBRARY \"other\"\nSAVE \"mem:wr.lib\"\nNEW\n10 LOAD LIBRARY \"mem:wr\"\nRUN", "? The file mem:wr.lib holds the library other, not wr in line 10\n" },
    { "plain LOAD finds a .lib file for editing", "10 LIBRARY \"z\"\n20 PRINT 1\nSAVE \"mem:z.lib\"\nNEW\nLOAD \"mem:z\"\nLIST", "10 LIBRARY \"z\"\n20 PRINT 1\n" },
    { "library: a library FUNC and PROC are callable from the program", "10 LIBRARY \"m\",\"M_\"\n20 FUNC M_DOUBLE(X)\n30   RETURN X*2\n40 END FUNC\n50 PROC M_SHOW(A$)\n60   PRINT \"[\";A$;\"]\"\n70 END PROC\nSAVE \"mem:m.lib\"\nNEW\n10 LOAD LIBRARY \"mem:m\"\n20 PRINT FN M_DOUBLE(21)\n30 CALL M_SHOW(\"hi\")\nRUN", "42\n[hi]\n" },
    { "library: line numbers of a library are its own (GOTO inside it)", "10 LIBRARY \"m\",\"M_\"\n20 PROC M_P\n30   GOTO 50\n40   PRINT \"skipped\"\n50   PRINT \"lib 50\"\n60 END PROC\nSAVE \"mem:m.lib\"\nNEW\n10 LOAD LIBRARY \"mem:m\"\n20 CALL M_P\n30 GOTO 50\n40 PRINT \"skipped\"\n50 PRINT \"main 50\"\nRUN", "lib 50\nmain 50\n" },
    { "library: a run time error in a library names it", "10 LIBRARY \"m\",\"M_\"\n20 PROC M_BAD\n30   PRINT 1/0\n40 END PROC\nSAVE \"mem:m.lib\"\nNEW\n10 LOAD LIBRARY \"mem:m\"\n20 CALL M_BAD\nRUN", "? Division by zero in line 30 of the library m\n" },
    { "library: the library's procedures stay callable at the prompt after RUN", "10 LIBRARY \"m\",\"M_\"\n20 FUNC M_TWICE(X)\n30   RETURN X*2\n40 END FUNC\nSAVE \"mem:m.lib\"\nNEW\n10 LOAD LIBRARY \"mem:m\"\n20 PRINT 1\nRUN\nPRINT FN M_TWICE(4)", "1\n8\n" },
    { "library: code outside of a PROC is not allowed in a library", "10 LIBRARY \"m\",\"M_\"\n20 PROC M_A\n30 END PROC\n40 PRINT 1\nSAVE \"mem:m.lib\"\nNEW\n10 LOAD LIBRARY \"mem:m\"\nRUN", "? Not allowed in a library: code outside of a PROC, FUNC or labelled section in line 40 of the library m\n" },
    { "library: RETURN outside of a labelled section is not allowed in a library", "10 LIBRARY \"m\"\n20 RETURN\nSAVE \"mem:m.lib\"\nNEW\n10 LOAD LIBRARY \"mem:m\"\nRUN", "? Not allowed in a library: RETURN outside of a labelled section in line 20 of the library m\n" },
    { "library: a labelled section, DEF FN and DATA are allowed in a library", "10 LIBRARY \"m\",\"M_\"\n20 LABEL M_SUB\n30 M_N=M_N+1\n40 PRINT \"sub\";M_N\n50 RETURN\n60 DEF FN M_F(X)=X+1\n70 DATA 1,2\nSAVE \"mem:m.lib\"\nNEW\n10 LOAD LIBRARY \"mem:m\"\n20 GOSUB M_SUB\n30 GOSUB M_SUB\n40 PRINT FN M_F(1)\nRUN", "sub1\nsub2\n2\n" },
    { "library: a global variable in a library is an error", "10 LIBRARY \"m\",\"M_\"\n20 PROC M_A\n30   I=5\n40 END PROC\nSAVE \"mem:m.lib\"\nNEW\n10 LOAD LIBRARY \"mem:m\"\nRUN", "? Variable I is global in the library (declare it LOCAL, or start its name with M_) in line 30 of the library m\n" },
    { "library: LOCAL, parameters and the prefix are fine in a library", "10 LIBRARY \"m\",\"M_\"\n20 PROC M_A(A$)\n30   LOCAL I%,S$,T(3)\n40   FOR I%=1 TO 2: S$=S$+A$: NEXT\n50   M_TOTAL=M_TOTAL+LEN(S$)\n60   PRINT S$;M_TOTAL\n70 END PROC\nSAVE \"mem:m.lib\"\nNEW\n10 LOAD LIBRARY \"mem:m\"\n20 CALL M_A(\"ab\")\nRUN", "abab4\n" },
    { "library: a FOR variable of a library must be LOCAL or prefixed", "10 LIBRARY \"m\",\"M_\"\n20 PROC M_A\n30   FOR J=1 TO 2: NEXT\n40 END PROC\nSAVE \"mem:m.lib\"\nNEW\n10 LOAD LIBRARY \"mem:m\"\nRUN", "? Variable J is global in the library (declare it LOCAL, or start its name with M_) in line 30 of the library m\n" },
    { "library: a library without a prefix has only LOCAL variables", "10 LIBRARY \"m\"\n20 PROC MA\n30   STATE=1\n40 END PROC\nSAVE \"mem:m.lib\"\nNEW\n10 LOAD LIBRARY \"mem:m\"\nRUN", "? Variable STATE is global in the library (declare it LOCAL; the library has no prefix) in line 30 of the library m\n" },
    { "library: the same procedure name in two libraries is an error", "10 LIBRARY \"a\",\"A_\"\n20 PROC SAME\n30 END PROC\nSAVE \"mem:a.lib\"\nNEW\n10 LIBRARY \"b\",\"B_\"\n20 PROC SAME\n30 END PROC\nSAVE \"mem:b.lib\"\nNEW\n10 LOAD LIBRARY \"mem:a\"\n20 LOAD LIBRARY \"mem:b\"\nRUN", "? Duplicate name SAME (also defined in the library a) in line 20 of the library b\n" },
    { "library: a library cannot be run", "10 LIBRARY \"m\"\n20 PROC X\n30 END PROC\nRUN", "? Library cannot be run (m is a library: a program names it with LOAD LIBRARY)\n" },
    { "library: INIT procedures run before the program, in dependency order", "10 LIBRARY \"m\",\"M_\"\n20 PROC M_INIT\n30   M_COUNT=100\n40   PRINT \"init m\"\n50 END PROC\n60 PROC M_INC\n70   M_COUNT=M_COUNT+1\n80   PRINT M_COUNT\n90 END PROC\nSAVE \"mem:m.lib\"\nNEW\n10 LIBRARY \"n\",\"N_\"\n20 LOAD LIBRARY \"mem:m\"\n30 PROC N_INIT\n40   PRINT \"init n\"\n50 END PROC\nSAVE \"mem:n.lib\"\nNEW\n10 LOAD LIBRARY \"mem:n\"\n20 PRINT \"main\"\n30 CALL M_INC\nRUN", "init m\ninit n\nmain\n101\n" },
    { "library: INIT also runs for RUN with a line number", "10 LIBRARY \"m\",\"M_\"\n20 PROC M_INIT\n30   PRINT \"init\"\n40 END PROC\nSAVE \"mem:m.lib\"\nNEW\n10 LOAD LIBRARY \"mem:m\"\n20 PRINT \"a\"\n30 PRINT \"b\"\nRUN 30", "init\nb\n" },
    { "library: INIT with parameters is an error", "10 LIBRARY \"m\",\"M_\"\n20 PROC M_INIT(X)\n30 END PROC\nSAVE \"mem:m.lib\"\nNEW\n10 LOAD LIBRARY \"mem:m\"\nRUN", "? M_INIT must take no parameters (the library m)\n" },
    { "library: a library's unlabelled DATA is private, the program's READ is separate", "10 LIBRARY \"d\",\"D_\"\n20 PROC D_SHOW\n30   LOCAL A,B$\n40   READ A,B$\n50   PRINT \"lib\";A;B$\n60 END PROC\n70 PROC D_AGAIN\n80   RESTORE\n90 END PROC\n130 DATA 11,\"eleven\",22,\"two-two\"\n140 LABEL PUBTAB\n150 DATA 99,\"pub\"\nSAVE \"mem:d.lib\"\nNEW\n10 LOAD LIBRARY \"mem:d\"\n20 READ X,Y$\n30 PRINT \"main\";X;Y$\n40 CALL D_SHOW\n50 READ X,Y$\n60 PRINT \"main\";X;Y$\n70 CALL D_SHOW\n80 DATA 1,\"one\",2,\"two\"\nRUN", "main1one\nlib11eleven\nmain2two\nlib22two-two\n" },
    { "library: RESTORE in a library restores its own DATA", "10 LIBRARY \"d\",\"D_\"\n20 PROC D_SHOW\n30   LOCAL A,B$\n40   READ A,B$\n50   PRINT \"lib\";A;B$\n60 END PROC\n70 PROC D_AGAIN\n80   RESTORE\n90 END PROC\n130 DATA 11,\"eleven\",22,\"two-two\"\n140 LABEL PUBTAB\n150 DATA 99,\"pub\"\nSAVE \"mem:d.lib\"\nNEW\n10 LOAD LIBRARY \"mem:d\"\n20 CALL D_SHOW\n30 CALL D_AGAIN\n40 CALL D_SHOW\nRUN", "lib11eleven\nlib11eleven\n" },
    { "library: labelled DATA of a library can be named with RESTORE label", "10 LIBRARY \"d\",\"D_\"\n20 PROC D_SHOW\n30   LOCAL A,B$\n40   READ A,B$\n50   PRINT \"lib\";A;B$\n60 END PROC\n70 PROC D_AGAIN\n80   RESTORE\n90 END PROC\n130 DATA 11,\"eleven\",22,\"two-two\"\n140 LABEL PUBTAB\n150 DATA 99,\"pub\"\nSAVE \"mem:d.lib\"\nNEW\n10 LOAD LIBRARY \"mem:d\"\n20 RESTORE PUBTAB\n30 READ X,Y$\n40 PRINT X;Y$\nRUN", "99pub\n" },
    { "library: two runs read the library's DATA from its start again", "10 LIBRARY \"d\",\"D_\"\n20 PROC D_SHOW\n30   LOCAL A,B$\n40   READ A,B$\n50   PRINT \"lib\";A;B$\n60 END PROC\n70 PROC D_AGAIN\n80   RESTORE\n90 END PROC\n130 DATA 11,\"eleven\",22,\"two-two\"\n140 LABEL PUBTAB\n150 DATA 99,\"pub\"\nSAVE \"mem:d.lib\"\nNEW\n10 LOAD LIBRARY \"mem:d\"\n20 CALL D_SHOW\nRUN\nRUN", "lib11eleven\nlib11eleven\n" },
    { "library: a library needing a library: names and dependency order work through RUN", "10 LIBRARY \"in\",\"I_\"\n20 FUNC I_ONE%()\n30   RETURN 1\n40 END FUNC\nSAVE \"mem:in.lib\"\nNEW\n10 LIBRARY \"out\",\"O_\"\n20 LOAD LIBRARY \"mem:in\"\n30 FUNC O_TWO%()\n40   RETURN FN I_ONE%()+1\n50 END FUNC\nSAVE \"mem:out.lib\"\nNEW\n10 LOAD LIBRARY \"mem:out\"\n20 PRINT FN O_TWO%()\nRUN", "2\n" },
    { "library: an error in a library's compile names the library", "10 LIBRARY \"m\",\"M_\"\n20 PROC M_A\n30   PRINT FN NOSUCH(1)\n40 END PROC\nSAVE \"mem:m.lib\"\nNEW\n10 LOAD LIBRARY \"mem:m\"\nRUN", "? Undefined function in line 30 of the library m\n" },
    { "library: a program without libraries is not affected", "10 PRINT 1\n20 GOTO 40\n30 PRINT 2\n40 PRINT 3\nRUN", "1\n3\n" },
    { "library: SAVE LIBRARY writes .lib and the library loads again", "10 LIBRARY \"m\"\n20 PRINT 1\nSAVE LIBRARY \"mem:m\"\nNEW\nLOAD \"mem:m.lib\"\nLIST", "10 LIBRARY \"m\"\n20 PRINT 1\n" },
    { "library: SAVE LIBRARY, B writes .l99", "10 LIBRARY \"m\"\n20 PRINT 1\nSAVE LIBRARY \"mem:m\",B\nNEW\nLOAD \"mem:m.l99\"\nLIST", "10 LIBRARY \"m\"\n20 PRINT 1\n" },
    { "library: SAVE LIBRARY of a program that is not a library", "10 PRINT 1\nSAVE LIBRARY \"mem:m\"", "? Not a library (its first statement must be LIBRARY \"name\")\n? Save failed\n" },
    { "library: a plain SAVE of a library writes .bas", "10 LIBRARY \"m\"\n20 PRINT 1\nSAVE \"mem:m\"\nNEW\nLOAD \"mem:m.bas\"\nLIST", "10 LIBRARY \"m\"\n20 PRINT 1\n" },
    { "library: SAVE LIBRARY without a name after loading a .bas library writes .lib", "10 LIBRARY \"m\"\nSAVE \"mem:m\"\nNEW\nLOAD \"mem:m\"\n20 PRINT 2\nSAVE LIBRARY\nNEW\nLOAD \"mem:m.lib\"\nLIST", "10 LIBRARY \"m\"\n20 PRINT 2\n" },
    { "library: SAVE without a name keeps a .lib file's format and name", "10 LIBRARY \"m\"\nSAVE LIBRARY \"mem:m\",B\nNEW\nLOAD \"mem:m\"\n20 PRINT 2\nSAVE\nNEW\nLOAD \"mem:m.l99\"\nLIST", "10 LIBRARY \"m\"\n20 PRINT 2\n" },
    { "library: SAVE ,A without a name swaps .l99 for .lib", "10 LIBRARY \"m\"\nSAVE LIBRARY \"mem:m\",B\nNEW\nLOAD \"mem:m\"\n20 PRINT 2\nSAVE ,A\nNEW\nLOAD \"mem:m.lib\"\nLIST", "10 LIBRARY \"m\"\n20 PRINT 2\n" },
    { "library: LIBRARIES before any run", "LIBRARIES", "No libraries (a program names them with LOAD LIBRARY; RUN loads them)\n" },
    { "library: LIBRARIES lists the libraries of the last run", "10 LIBRARY \"gfx\",\"GX_\"\n20 PROC GX_INIT\n30   GX_N=0\n40 END PROC\n50 FUNC GX_DOUBLE(X,S$,A())\n60   RETURN X*2\n70 END FUNC\n80 FUNC GX_NAME$(N%)\n90   RETURN \"x\"\n100 END FUNC\n110 LABEL GX_SUB\n120 RETURN\n130 DEF FN GX_F(X)=X+1\nSAVE \"mem:gfx.lib\"\nNEW\n10 LOAD LIBRARY \"mem:gfx\"\n20 PRINT 1\nRUN\nLIBRARIES", "1\nNAME             PREFIX   FILE\ngfx              GX_      mem:gfx.lib\n" },
    { "library: LIBRARIES name lists what the library defines", "10 LIBRARY \"gfx\",\"GX_\"\n20 PROC GX_INIT\n30   GX_N=0\n40 END PROC\n50 FUNC GX_DOUBLE(X,S$,A())\n60   RETURN X*2\n70 END FUNC\n80 FUNC GX_NAME$(N%)\n90   RETURN \"x\"\n100 END FUNC\n110 LABEL GX_SUB\n120 RETURN\n130 DEF FN GX_F(X)=X+1\nSAVE \"mem:gfx.lib\"\nNEW\n10 LOAD LIBRARY \"mem:gfx\"\n20 PRINT 1\nRUN\nLIBRARIES \"gfx\"", "1\nLibrary gfx:\n  PROC GX_INIT()\n  FUNC GX_DOUBLE(X, S$, A())\n  FUNC GX_NAME$(N%)\n  LABEL GX_SUB\n  FUNC GX_F(X)\n" },
    { "library: LIBRARIES with an unknown name", "10 LIBRARY \"gfx\",\"GX_\"\n20 PROC GX_INIT\n30   GX_N=0\n40 END PROC\n50 FUNC GX_DOUBLE(X,S$,A())\n60   RETURN X*2\n70 END FUNC\n80 FUNC GX_NAME$(N%)\n90   RETURN \"x\"\n100 END FUNC\n110 LABEL GX_SUB\n120 RETURN\n130 DEF FN GX_F(X)=X+1\nSAVE \"mem:gfx.lib\"\nNEW\n10 LOAD LIBRARY \"mem:gfx\"\nRUN\nLIBRARIES \"nope\"", "? Library not loaded: nope\n" },
    { "library: LIBRARIES in a program", "10 LIBRARY \"gfx\",\"GX_\"\n20 PROC GX_INIT\n30   GX_N=0\n40 END PROC\n50 FUNC GX_DOUBLE(X,S$,A())\n60   RETURN X*2\n70 END FUNC\n80 FUNC GX_NAME$(N%)\n90   RETURN \"x\"\n100 END FUNC\n110 LABEL GX_SUB\n120 RETURN\n130 DEF FN GX_F(X)=X+1\nSAVE \"mem:gfx.lib\"\nNEW\n10 LOAD LIBRARY \"mem:gfx\"\n20 LIBRARIES\nRUN", "NAME             PREFIX   FILE\ngfx              GX_      mem:gfx.lib\n" },
    { "library: NEW forgets the libraries", "10 LIBRARY \"gfx\",\"GX_\"\n20 PROC GX_INIT\n30   GX_N=0\n40 END PROC\n50 FUNC GX_DOUBLE(X,S$,A())\n60   RETURN X*2\n70 END FUNC\n80 FUNC GX_NAME$(N%)\n90   RETURN \"x\"\n100 END FUNC\n110 LABEL GX_SUB\n120 RETURN\n130 DEF FN GX_F(X)=X+1\nSAVE \"mem:gfx.lib\"\nNEW\n10 LOAD LIBRARY \"mem:gfx\"\nRUN\nNEW\nLIBRARIES", "No libraries (a program names them with LOAD LIBRARY; RUN loads them)\n" },
    { "library: LOAD LIBRARY at the prompt is an error", "LOAD LIBRARY \"mem:x\"", "? LOAD LIBRARY belongs in a program (RUN loads the libraries a program names)\n" },
    { "library: LOAD LIBRARY in a program line is fine", "10 PRINT 1: LOAD LIBRARY \"mem:x\"\nLIST", "10 PRINT 1: LOAD LIBRARY \"mem:x\"\n" },
    { "SAVE without an option makes the ASCII .bas, LOAD finds it", "10 PRINT \"D\"\nSAVE \"repltest1x\"\nNEW\nLOAD \"repltest1x.bas\"\nRUN", "D\n" },
    { "SAVE ,A without extension makes .bas, LOAD finds it", "10 PRINT \"A\"\nSAVE \"repltest1x\",A\nNEW\nLOAD \"repltest1x\"\nRUN", "A\n" },
    { "LOAD without extension: .bas before .e99", "10 PRINT \"E\"\nSAVE \"repltest1x\",B\nNEW\n10 PRINT \"A\"\nSAVE \"repltest1x\",A\nNEW\nLOAD \"repltest1x\"\nRUN", "A\n" },
    { "LOAD without extension: the exact name wins", "10 PRINT \"X\"\nSAVE \"repltest1x\",A\nNEW\n10 PRINT \"N\"\nSAVE \"repltest1x.zzz\",A\nNEW\nA$=`mv repltest1x.zzz repltest1x`\nLOAD \"repltest1x\"\nRUN", "N\n" },
    { "SAVE with an extension keeps the name", "10 PRINT 1\nSAVE \"repltest1x.dat\",B\nNEW\nLOAD \"repltest1x.dat\"\nRUN", "1\n" },
    { "LOAD a missing name without extension", "LOAD \"repltest1nothing\"", "? File not found\n" },
    { "LOAD a binary file",     "10 PRINT \"B\"\nSAVE \"" TESTFILE "\",B\nNEW\nLOAD \"" TESTFILE "\"\nLIST", "10 PRINT \"B\"\n" },
    { "LOAD in direct mode does not run", "10 PRINT \"B\"\nSAVE \"" TESTFILE "\",B\nNEW\nLOAD \"" TESTFILE "\"\nPRINT 7\nRUN", "7\nB\n" },
    { "LOAD a text file",       "10 PRINT \"B\"\n20 PRINT 2\nSAVE \"" TESTFILE "\",A\nNEW\nLOAD \"" TESTFILE "\"\nRUN", "B\n2\n" },
    { "LOAD in a program replaces it and runs it", "10 PRINT \"B\"\nSAVE \"" TESTFILE "\",B\nNEW\n10 PRINT \"A\"\n20 LOAD \"" TESTFILE "\"\n30 PRINT \"C\"\nRUN\nLIST", "A\nB\n10 PRINT \"B\"\n" },
    { "LOAD clears variables", "10 PRINT A\nSAVE \"" TESTFILE "\",B\nNEW\nA=5\nLOAD \"" TESTFILE "\"\nPRINT A", "0\n" },
    { "LOAD missing file",      "10 PRINT 1\nLOAD \"no/such/file\"\nLIST",             "? File not found\n10 PRINT 1\n" },
    { "LOAD keeps the program on a bad file", "10 PRINT 1\nA$=`printf 'garbage' >" TESTFILE "`\nLOAD \"" TESTFILE "\"\nLIST", "? Line number missing in program file\n10 PRINT 1\n" },
    { "LOAD a truncated binary file", "10 PRINT 1\nSAVE \"" TESTFILE "\",B\nA$=`truncate -s -2 " TESTFILE "`\nLOAD \"" TESTFILE "\"\nLIST", "? Bad program file\n10 PRINT 1\n" },
    { "LOAD a future file version", "10 PRINT 1\nA$=`printf '\\211E99\\7' >" TESTFILE "`\nLOAD \"" TESTFILE "\"\nLIST", "? Unsupported program file version\n10 PRINT 1\n" },
    { "LOAD at the end of a chain", "10 PRINT \"B\"\nSAVE \"" TESTFILE "\",B\nNEW\n10 PRINT \"A\"\n20 LOAD \"" TESTFILE "\"\nRUN", "A\nB\n" },
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
    { "LIST # to a file",       "10 PRINT 1\n20 PRINT 2\n30 END\nOPEN \"mem:l1\" FOR OUTPUT AS #1\nLIST #1\nCLOSE #1\nOPEN \"mem:l1\" FOR INPUT AS #1\nLINE INPUT #1,A$:LINE INPUT #1,B$:PRINT A$;\"|\";B$", "10 PRINT 1|20 PRINT 2\n" },
    { "LIST # with a range",    "10 PRINT 1\n20 PRINT 2\n30 END\nOPEN \"mem:l2\" FOR OUTPUT AS #2\nLIST #2,20-30\nCLOSE #2\nOPEN \"mem:l2\" FOR INPUT AS #2\nLINE INPUT #2,A$:PRINT A$:PRINT EOF(2)", "20 PRINT 2\n0\n" },
    { "LIST # with a comma",    "10 PRINT 1\n20 PRINT 2\nOPEN \"mem:l3\" FOR OUTPUT AS #1\nLIST #1,10\nCLOSE #1\nOPEN \"mem:l3\" FOR INPUT AS #1\nLINE INPUT #1,A$:PRINT A$:PRINT EOF(1)", "10 PRINT 1\n-1\n" },
    { "LIST # of a closed channel", "10 PRINT 1\nLIST #3",                                  "? File not open\n" },
    { "LIST after LIST # is the screen again", "10 PRINT 1\nOPEN \"mem:l4\" FOR OUTPUT AS #1\nLIST #1\nLIST\nCLOSE #1", "10 PRINT 1\n" },
    { "a binary file with an unused token number (the former OPTION) is refused, with its line", "A$=\"\"\nA$=A$+CHR$(137)+CHR$(69)+CHR$(57)+CHR$(57)+CHR$(1)+CHR$(0)+CHR$(0)+CHR$(10)+CHR$(0)+CHR$(10)+CHR$(32)+CHR$(11)+CHR$(32)\nA$=A$+CHR$(49)+CHR$(0)+CHR$(0)+CHR$(0)+CHR$(20)+CHR$(0)+CHR$(12)+CHR$(32)+CHR$(76)+CHR$(32)+CHR$(77)+CHR$(32)+CHR$(49)\nA$=A$+CHR$(0)+CHR$(0)+CHR$(0)+CHR$(30)+CHR$(0)+CHR$(15)+CHR$(32)+CHR$(72)+CHR$(32)+CHR$(75)+CHR$(32)+CHR$(138)+CHR$(0)\nA$=A$+CHR$(1)+CHR$(65)+CHR$(0)+CHR$(0)+CHR$(0)+CHR$(40)+CHR$(0)+CHR$(10)+CHR$(32)+CHR$(11)+CHR$(32)+CHR$(50)+CHR$(0)\nOPEN \"mem:oldopt.e99\" FOR OUTPUT AS #1:PRINT #1,A$;:CLOSE #1\nLOAD \"mem:oldopt.e99\"\nLIST", "? Unknown token in line 20 of the program file\n" },
    { "OPTION, BASE and FLT are ordinary names now", "OPTION=3:BASE=4:FLT=5:PRINT OPTION+BASE+FLT", "12\n" },
    { "PUT is PUTREC",          "OPEN \"mem:pr\" FOR RANDOM AS #1 LEN=8\nFIELD #1,8 AS A$\nLSET A$=\"abc\"\nPUT #1,2\nLSET A$=\"zzz\"\nGETREC #1,2\nPRINT A$;LOC(1)", "abc     2\n" },
    { "PUT is listed as PUTREC", "10 PUT #1,3\nLIST",                                      "10 PUTREC #1,3\n" },
    { "RENUMBER is RENUM",      "5 A=1\n7 A=2\n9 A=3\nRENUMBER ,5\nLIST",              "10 A=1\n15 A=2\n20 A=3\n" },
    { "OLD is LOAD",            "10 PRINT 1\nSAVE \"mem:oldtest\"\nNEW\nOLD \"mem:oldtest\"\nLIST",  "10 PRINT 1\n" },
    { "CATALOG is FILES",       "CATALOG \"mem:nomatch*\"\nFILES \"mem:nomatch*\"",             "? File not found\n? File not found\n" },
    { "an alias is listed as the statement it stands for", "10 OLD \"x\"\n20 CATALOG\n30 RENUMBER\nLIST", "10 LOAD \"x\"\n20 FILES\n30 RENUM\n" },
    { "an alias is a reserved word", "OLD=1",                                              "? String expression expected\n" },
    { "RENUM step",             "5 A=1\n7 A=2\n9 A=3\nRENUM ,5\nLIST",                 "10 A=1\n15 A=2\n20 A=3\n" },
    { "comment after statement", "10 PRINT 1 'note\n20 PRINT 2\nRUN", "1\n2\n" },
    { "comment-only line", "10 PRINT 1\n20 'whole line\n30 PRINT 3\nRUN", "1\n3\n" },
    { "comment after colon", "10 PRINT 1: 'note\n20 PRINT 2\nRUN", "1\n2\n" },
    { "comment-only line is listed", "10 'only\nLIST", "10 'only\n" },
    { "REM is a comment: a line of its own", "10 PRINT 1\n20 REM this is a remark\n30 PRINT 3\nRUN", "1\n3\n" },
    { "REM after a colon", "10 PRINT 1: REM the rest : PRINT 9\n20 PRINT 2\nRUN", "1\n2\n" },
    { "REM in lower case keeps the text and is listed as an apostrophe", "10 rem Hello World\nLIST", "10 ' Hello World\n" },
    { "REMAIN and a variable REM_X are not comments", "10 REM_X=5:PRINT REM_X\n20 PRINT REMAIN(0)\nRUN", "5\n0\n" },
    { "REM inside a string is text", "10 PRINT \"REM not here\"\nRUN", "REM not here\n" },
    { "a comment keeps its case", "10 PRINT \"Ab\":REM_X=1 ' Ode to Joy: Bach\n20 x=2:'MiXed Case 'ok\nLIST", "10 PRINT \"Ab\":REM_X=1 ' Ode to Joy: Bach\n20 X=2:'MiXed Case 'ok\n" },
    { "comment in direct mode", "'just a note\nPRINT 5 'x", "5\n" },
    { "comment in IF branch", "10 IF 1 THEN PRINT 1 'x\n20 PRINT 2\nRUN", "1\n2\n" },
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
    { "BYE",                    "PRINT 1\nBYE\nPRINT 2",                           "1\n<quit 0>\n" },
    { "BYE with a code",        "BYE 7",                                           "<quit 7>\n" },
    { "QUIT",                   "PRINT 1\nQUIT\nPRINT 2",                          "1\n<quit 0>\n" },
    { "QUIT with a code",       "QUIT 3",                                          "<quit 3>\n" },
    { "quit lower case with leading blanks", "  quit 4",                           "<quit 4>\n" },
    { "QUIT expression",        "A=5\nQUIT A+1",                                   "<quit 6>\n" },
    { "BYE ends a program",     "10 PRINT 1\n20 BYE 2\n30 PRINT 3\nRUN\nPRINT 9",  "1\n<quit 2>\n" },
    { "BYE in a loop",          "10 FOR I=1 TO 5\n20 PRINT I\n30 IF I=2 THEN BYE 9\n40 NEXT\nRUN", "1\n2\n<quit 9>\n" },
    { "BYE in a procedure",     "10 CALL P\n20 PRINT 2\n30 PROC P\n40 PRINT 1:BYE 5\n50 END PROC\nRUN", "1\n<quit 5>\n" },
    { "BYE code out of range",  "BYE 256",                                         "? Illegal argument\n" },
    { "BYE negative code",      "BYE -1",                                          "? Illegal argument\n" },
    { "BYE fractional code",    "BYE 1.5",                                         "? Illegal argument\n" },
    { "EXIT is a loop word in a program", "10 FOR I=1 TO 3\n20 PRINT I\n30 EXIT\n40 NEXT\n50 PRINT 9\nRUN", "1\n9\n" },
    { "EXIT outside a loop in a program", "10 EXIT\nRUN",                        "? EXIT without loop in line 10\n" },
    { "ARGC without arguments", "PRINT ARGC",                                      "0\n" },
    { "ARG$(0) without arguments", "PRINT \"[\";ARG$(0);\"]\"",                    "[]\n" },
    { "ENV$ of PATH is not empty", "PRINT LEN(ENV$(\"PATH\"))>0",                 "-1\n" },
    { "BUILD without a program", "BUILD \"repltest1b\"",                             "? No program\n" },
    { "BUILD refuses a program file name", "10 PRINT 1\nBUILD \"repltest1b.BAS\"",  "? Bad name for an executable\n" },
    { "BUILD refuses .e99",     "10 PRINT 1\nBUILD \"repltest1b.e99\"",             "? Bad name for an executable\n" },
    { "BUILD refuses other drivers", "10 PRINT 1\nBUILD \"mem:b\"",                 "? Bad name for an executable\n" },
    { "BUILD unknown driver",   "10 PRINT 1\nBUILD \"zzz:b\"",                      "? Device not found\n" },
    { "BUILD error in a program", "10 PRINT 1\n20 BUILD \"b.e99\"\nRUN",           "1\n? Bad name for an executable in line 20\n" },
    { "NEW in a program",       "10 PRINT 1\n20 NEW\n30 PRINT 3\nRUN\nLIST\nRUN",        "1\n? Empty program\n" },
    { "NEW clears variables",   "A=4\nNEW\nPRINT A",                                   "0\n" },
    { "NEW then a new program", "10 PRINT 1\nNEW\n10 PRINT 2\nRUN",                    "2\n" },
    { "NEW from a loaded program", "10 PRINT \"B\"\n20 NEW\nSAVE \"" TESTFILE "\",B\nNEW\nLOAD \"" TESTFILE "\"\nRUN\nLIST", "B\n" },
    // MERGE, MERGE ... APPEND, CHAIN
    { "MERGE replaces and inserts", "10 PRINT 1\n20 PRINT 2\nSAVE \"" TESTFILE "\",A\nNEW\n10 PRINT 9\n15 PRINT 8\n30 PRINT 3\nMERGE \"" TESTFILE "\"\nLIST", "10 PRINT 1\n15 PRINT 8\n20 PRINT 2\n30 PRINT 3\n" },
    { "MERGE a binary file", "10 PRINT 1\nSAVE \"" TESTFILE "\",B\nNEW\n20 PRINT 2\nMERGE \"" TESTFILE "\"\nLIST", "10 PRINT 1\n20 PRINT 2\n" },
    { "MERGE into an empty program", "10 PRINT 1\nSAVE \"" TESTFILE "\",A\nNEW\nMERGE \"" TESTFILE "\"\nRUN", "1\n" },
    { "MERGE keeps the variables", "10 PRINT 1\nSAVE \"" TESTFILE "\",A\nNEW\nA=5\n20 PRINT A\nMERGE \"" TESTFILE "\"\nPRINT A", "5\n" },
    { "MERGE a missing file", "10 PRINT 1\nMERGE \"repltest1nothing\"\nLIST", "? File not found\n10 PRINT 1\n" },
    { "MERGE a bad file changes nothing", "10 PRINT 1\nA$=`printf '20 PRINT 2\\nPRINT 3\\n' >" TESTFILE "`\nMERGE \"" TESTFILE "\"\nLIST", "? Line number missing in program file\n10 PRINT 1\n" },
    { "MERGE APPEND numbers the new lines behind the last one", "5 PRINT 1\n7 GOTO 5\nSAVE \"" TESTFILE "\",A\nNEW\n10 PRINT 0\n25 PRINT 4\nMERGE \"" TESTFILE "\",APPEND\nLIST", "10 PRINT 0\n25 PRINT 4\n30 PRINT 1\n40 GOTO 30\n" },
    { "MERGE APPEND to an empty program", "5 PRINT 1\n7 GOTO 5\nSAVE \"" TESTFILE "\",A\nNEW\nMERGE \"" TESTFILE "\",APPEND\nLIST", "10 PRINT 1\n20 GOTO 10\n" },
    { "MERGE APPEND after a line that is a multiple of 10", "10 PRINT 0\n20 PRINT 4\nSAVE \"" TESTFILE "\",A\nMERGE \"" TESTFILE "\",APPEND\nLIST", "10 PRINT 0\n20 PRINT 4\n30 PRINT 0\n40 PRINT 4\n" },
    { "MERGE APPEND beyond the last line number", "999995 PRINT 1\n10 PRINT 2\nSAVE \"" TESTFILE "\",A\nMERGE \"" TESTFILE "\",APPEND\nLIST", "? Line number too large\n10 PRINT 2\n999995 PRINT 1\n" },
    { "MERGE APPEND adjusts all kinds of references", "1 ON A GOTO 1,2\n2 IF A THEN 1 ELSE 2\n3 RESTORE 2\nSAVE \"" TESTFILE "\",A\nNEW\n50 END\nMERGE \"" TESTFILE "\",APPEND\nLIST", "50 END\n60 ON A GOTO 60,70\n70 IF A THEN 60 ELSE 70\n80 RESTORE 70\n" },
    { "MERGE with a label defined twice", "20 LABEL X\nSAVE \"" TESTFILE "\",A\nNEW\n10 LABEL X\n20 PRINT 1\nMERGE \"" TESTFILE "\"\nLIST", "? Duplicate label X\n10 LABEL X\n20 PRINT 1\n" },
    { "MERGE APPEND with a label defined twice", "10 LABEL X\nSAVE \"" TESTFILE "\",A\nMERGE \"" TESTFILE "\",APPEND\nLIST", "? Duplicate label X\n10 LABEL X\n" },
    { "MERGE replacing the line of a label is fine", "10 LABEL X\n15 PRINT 2\nSAVE \"" TESTFILE "\",A\nNEW\n10 LABEL X\n20 PRINT 1\nMERGE \"" TESTFILE "\"\nLIST", "10 LABEL X\n15 PRINT 2\n20 PRINT 1\n" },
    { "MERGE in a program ends the run", "10 PRINT 1\nSAVE \"" TESTFILE "\",A\nNEW\n10 MERGE \"" TESTFILE "\"\n20 PRINT 2\nRUN\nLIST", "10 PRINT 1\n20 PRINT 2\n" },
    { "MERGE with an empty name", "MERGE \"\"", "? Illegal argument\n" },
    { "MERGE with a wrong option", "MERGE \"x\",FOO", "? APPEND expected\n" },
    { "CHAIN keeps the variables of both programs", "10 PRINT A;B$;C\nSAVE \"" TESTFILE "\",A\nNEW\n10 A=5:B$=\"x\":D=7\n20 CHAIN \"" TESTFILE "\"\nRUN", "5x0\n" },
    { "CHAIN does not run the old program on", "10 PRINT 1\nSAVE \"" TESTFILE "\",A\nNEW\n10 CHAIN \"" TESTFILE "\"\n20 PRINT 2\nRUN", "1\n" },
    { "CHAIN in direct mode runs the program", "10 PRINT A\nSAVE \"" TESTFILE "\",A\nNEW\nA=3\nCHAIN \"" TESTFILE "\"", "3\n" },
    { "CHAIN to a line", "10 PRINT 1\n20 PRINT 2\n30 PRINT 3\nSAVE \"" TESTFILE "\",A\nNEW\n10 CHAIN \"" TESTFILE "\",20\nRUN", "2\n3\n" },
    { "CHAIN to a label", "10 PRINT 1\n20 LABEL L\n30 PRINT 3\nSAVE \"" TESTFILE "\",A\nNEW\n10 CHAIN \"" TESTFILE "\",L\nRUN", "3\n" },
    { "CHAIN to a missing line", "10 PRINT 1\nSAVE \"" TESTFILE "\",A\nNEW\n10 CHAIN \"" TESTFILE "\",99\nRUN", "? Undefined line number 99\n" },
    { "CHAIN replaces the program", "10 PRINT 1\nSAVE \"" TESTFILE "\",A\nNEW\n10 CHAIN \"" TESTFILE "\"\n20 PRINT 2\nRUN\nLIST", "1\n10 PRINT 1\n" },
    { "CHAIN keeps an array with the same dimensions", "10 DIM X(3):PRINT X(2)\nSAVE \"" TESTFILE "\",A\nNEW\n10 DIM X(3):X(2)=9\n20 CHAIN \"" TESTFILE "\"\nRUN", "9\n" },
    { "CHAIN drops an array with other dimensions", "10 DIM X(4):PRINT X(2)\nSAVE \"" TESTFILE "\",A\nNEW\n10 DIM X(3):X(2)=9\n20 CHAIN \"" TESTFILE "\"\nRUN", "0\n" },
    { "CHAIN keeps string arrays", "10 DIM S$(2):PRINT S$(1)\nSAVE \"" TESTFILE "\",A\nNEW\n10 DIM S$(2):S$(1)=\"hi\"\n20 CHAIN \"" TESTFILE "\"\nRUN", "hi\n" },
    { "CHAIN keeps the sound effects", "10 SFX 1\nSAVE \"" TESTFILE "\",A\nNEW\n10 SFX 1,\"C\"\n20 CHAIN \"" TESTFILE "\"\n30 PRINT 1\nRUN", "" },
    { "CHAIN a missing file keeps everything", "10 A=4:CHAIN \"repltest1nothing\"\nRUN\nPRINT A", "? File not found\n4\n" },
    { "CHAIN MERGE merges and runs at the target", "100 PRINT A\n110 END\nSAVE \"" TESTFILE "\",A\nNEW\n10 A=7\n20 CHAIN MERGE \"" TESTFILE "\",100\nRUN\nLIST", "7\n10 A=7\n20 CHAIN MERGE \"" TESTFILE "\",100\n100 PRINT A\n110 END\n" },
    { "CHAIN MERGE without a target runs from the start", "20 PRINT 2\nSAVE \"" TESTFILE "\",A\nNEW\n10 PRINT 1\nCHAIN MERGE \"" TESTFILE "\"", "1\n2\n" },
    { "CHAIN from an IF passes the counter", "10 PRINT A:END\nSAVE \"" TESTFILE "\",A\nNEW\n10 A=A+1\n20 IF A<3 THEN CHAIN \"" TESTFILE "\"\nRUN", "1\n" },
    { "CHAIN keeps the variables when the new program does not compile", "10 PRINT (\nSAVE \"" TESTFILE "\",B\nNEW\n10 A=5:B$=\"x\":CHAIN \"" TESTFILE "\"\nRUN\nPRINT A;B$", "? Numeric expression expected in line 10\n5x\n" },
    { "CHAIN keeps the variables when the target is missing", "10 PRINT 1\nSAVE \"" TESTFILE "\",B\nNEW\n10 A=5:CHAIN \"" TESTFILE "\",99\nRUN\nPRINT A", "? Undefined line number 99\n5\n" },
    { "CHAIN keeps the variables when the file is empty", "A$=`: >" TESTFILE "`\n10 A=5:CHAIN \"" TESTFILE "\"\nRUN\nPRINT A", "? Empty program\n5\n" },
    { "CHAIN keeps functions and the sound effects when the new program does not compile", "10 PRINT (\nSAVE \"" TESTFILE "\",B\nNEW\n10 SFX 1,\"C\":DEF FN F(X)=X*2:CHAIN \"" TESTFILE "\"\nRUN\nPRINT FN F(4)\nSFX 1", "? Numeric expression expected in line 10\n8\n" },
    { "CHAIN does not keep the old variables after an error in the running new program", "10 PRINT A:A=7:PRINT 1/0\nSAVE \"" TESTFILE "\",B\nNEW\n10 A=5:CHAIN \"" TESTFILE "\"\nRUN\nPRINT A", "5\n? Division by zero in line 10\n7\n" },
    { "MERGE a binary file with APPEND", "5 PRINT 1\n7 GOTO 5\nSAVE \"" TESTFILE "\",B\nNEW\n10 PRINT 0\n25 PRINT 4\nMERGE \"" TESTFILE "\",APPEND\nLIST", "10 PRINT 0\n25 PRINT 4\n30 PRINT 1\n40 GOTO 30\n" },
    { "CHAIN a binary file", "10 PRINT A\nSAVE \"" TESTFILE "\",B\nNEW\n10 A=3:CHAIN \"" TESTFILE "\"\nRUN", "3\n" },
    { "CHAIN MERGE a binary file", "100 PRINT A\n110 END\nSAVE \"" TESTFILE "\",B\nNEW\n10 A=7\n20 CHAIN MERGE \"" TESTFILE "\",100\nRUN\nLIST", "7\n10 A=7\n20 CHAIN MERGE \"" TESTFILE "\",100\n100 PRINT A\n110 END\n" },
    { "AFTER in a direct line reaches program labels", "10 PRINT \"p\":END\n90 LABEL H\n100 PRINT \"h\":RETURN\nRUN\nAFTER 3 GOSUB H:WAIT 12", "p\nh\n" },
    { "timers end with the line or program that set them", "10 PRINT \"p\":END\n90 LABEL H\n100 PRINT \"h\":RETURN\nRUN\nAFTER 3 GOSUB H\nWAIT 12\nPRINT \"x\"", "p\nx\n" },
    { "a program starts without the timers of the one before", "10 AFTER 3 GOSUB 100:END\n100 PRINT \"h\":RETURN\nRUN\nWAIT 12\nRUN", "" },
    { "AFTER and EVERY are listed", "10 AFTER 5,1 GOSUB 100:EVERY 6 GOTO 200:AFTER OFF:EVERY OFF 2\n100 RETURN\n200 END\nLIST", "10 AFTER 5,1 GOSUB 100:EVERY 6 GOTO 200:AFTER OFF:EVERY OFF 2\n100 RETURN\n200 END\n" },
    { "GOTO a program line from direct mode", "10 PRINT 1:END\n100 PRINT 2:END\nRUN\nGOTO 100", "1\n2\n" },
    { "GOSUB a program line from direct mode", "10 END\n100 PRINT 7:RETURN\nRUN\nGOSUB 100\nPRINT 8", "7\n8\n" },
    { "a line number in a direct line without a compiled program", "GOTO 100", "? Undefined line number 100\n" },
    { "a line number in a direct line after the program was changed", "10 END\n100 PRINT 7:RETURN\nRUN\n100 PRINT 9:RETURN\nGOSUB 100\nRUN\nGOSUB 100", "? Undefined line number 100\n9\n" },
    { "a line number in a direct line after RENUM", "10 END\n100 PRINT 7:RETURN\nRUN\nRENUM\nGOSUB 100\nRUN\nGOSUB 20", "? Undefined line number 100\n7\n" },
    { "a line number in a direct line after DELETE", "10 PRINT 1\n20 END\n100 PRINT 7:RETURN\nRUN\nDELETE 10\nGOSUB 100\nRUN\nGOSUB 100", "1\n? Undefined line number 100\n7\n" },
    { "RESTORE a line from direct mode", "10 DATA 1,2\n20 DATA 3,4\n30 END\nRUN\nRESTORE 20:READ A:PRINT A", "3\n" },
    { "AFTER in a direct line reaches program line numbers", "10 PRINT \"p\":END\n100 PRINT \"h\":RETURN\nRUN\nAFTER 3 GOSUB 100:WAIT 12", "p\nh\n" },
    // files and devices through the drivers
    { "SAVE and LOAD through mem:", "10 PRINT 1\nSAVE \"mem:p\"\nNEW\nLOAD \"mem:p\"\nRUN", "1\n" },
    { "SAVE ,A and LOAD through mem:", "10 PRINT 2\nSAVE \"mem:q\",A\nNEW\nLOAD \"mem:q\"\nRUN", "2\n" },
    { "LOAD with the extension search on mem:", "10 PRINT 5\nSAVE \"mem:r\",B\nNEW\nLOAD \"mem:r.e99\"\nRUN\nNEW\nLOAD \"mem:r\"\nRUN", "5\n5\n" },
    { "LOAD uses the ADDRESS driver", "10 PRINT 3\nSAVE \"mem:p\"\nNEW\nADDRESS MEM\nLOAD \"p\"\nRUN", "3\n" },
    { "SAVE uses the ADDRESS driver", "10 PRINT 4\nADDRESS MEM\nSAVE \"p\"\nNEW\nLOAD \"mem:p\"\nRUN", "4\n" },
    { "MERGE through mem:", "10 PRINT 1\nSAVE \"mem:m\",A\nNEW\n20 PRINT 2\nMERGE \"mem:m\"\nRUN", "1\n2\n" },
    { "MERGE APPEND through mem:", "10 PRINT 1\nSAVE \"mem:m\"\nNEW\n10 PRINT 0\nMERGE \"mem:m\",APPEND\nLIST", "10 PRINT 0\n20 PRINT 1\n" },
    { "CHAIN through mem:", "10 PRINT A\nSAVE \"mem:c\"\nNEW\n10 A=4:CHAIN \"mem:c\"\nRUN", "4\n" },
    { "LOAD a missing file on mem:", "LOAD \"mem:nothing\"", "? File not found\n" },
    { "LOAD from an unknown device", "LOAD \"nodev:p\"", "? Device not found\n" },
    { "MERGE from an unknown device", "MERGE \"nodev:p\"", "? Device not found\n" },
    { "SAVE to an unknown device", "10 PRINT 1\nSAVE \"nodev:p\"", "? Device not found\n? Save failed\n" },
    { "SAVE to null:", "10 PRINT 1\nSAVE \"null:p\"\nPRINT \"ok\"", "ok\n" },
    { "LOAD from null: is an empty program", "10 PRINT 1\nLOAD \"null:p\"\nLIST", "" },
    { "EXIT and FOR EACH in direct mode lines", "FOR I=1 TO 9:IF I<3 THEN PRINT I;:NEXT ELSE EXIT\nPRINT I\nDIM A(2):A(0)=4:A(1)=5\nFOR EACH V IN A():PRINT V;:NEXT:PRINT", "123\n45\n" },
    { "STOP and CONT", "10 PRINT \"a\"\n20 STOP\n30 PRINT \"b\"\nRUN\nPRINT \"x\"\nCONT", "a\n? Break in line 20\nx\nb\n" },
    { "CONT goes on in a loop with a GOSUB", "10 FOR I=1 TO 3\n20 GOSUB 100\n30 NEXT I\n40 END\n100 PRINT I;\n110 IF I=2 THEN STOP\n120 RETURN\nRUN\nCONT", "12? Break in line 110\n3" },
    { "CONT inside a PROC keeps its LOCAL variables", "10 PROC P(N)\n20 LOCAL K:K=N*2\n30 STOP\n40 PRINT K;N\n50 END PROC\n60 CALL P(4)\n70 PRINT \"end\"\nRUN\nPRINT K\nCONT\nPRINT K", "? Break in line 30\n8\n84\nend\n0\n" },
    { "CONT without STOP",     "CONT",                                                      "? Cannot continue\n" },
    { "CONT after a change of the program", "10 STOP\n20 PRINT 1\nRUN\n30 PRINT 2\nCONT", "? Break in line 10\n? Cannot continue\n" },
    { "CONT after RUN again",  "10 STOP\n20 PRINT 1\nRUN\nRUN\nCONT\nCONT", "? Break in line 10\n? Break in line 10\n1\n? Cannot continue\n" },
    { "CONT twice",            "10 PRINT 1\n20 STOP\n30 PRINT 2\nRUN\nCONT\nCONT", "1\n? Break in line 20\n2\n? Cannot continue\n" },
    { "STOP in a direct mode line cannot be continued", "STOP\nCONT", "? Break\n? Cannot continue\n" },
    { "Break can be continued", "10 FOR I=1 TO 100000:NEXT I:PRINT \"done\"\nRUN\nCONT", "? Break in line 10\n? Break in line 10\n", 0, 3 },
    { "TRACE",                 "10 TRACE ON\n20 PRINT 1\n30 TRACE OFF\n40 PRINT 2\nRUN",     "[20]1\n[30]2\n" },
    { "TRACE lasts over RUN",  "10 PRINT 5\nTRACE ON\nRUN\nTRACE OFF\nRUN",                  "[10]5\n5\n" },
    { "STOP CONT TRACE AUTO are listed", "10 STOP\n20 CONT\n30 TRACE ON\n40 TRACE OFF\n50 AUTO\n60 AUTO 100\n70 AUTO 100,5\nLIST", "10 STOP\n20 CONT\n30 TRACE ON\n40 TRACE OFF\n50 AUTO\n60 AUTO 100\n70 AUTO 100,5\n" },
    { "files stay open between direct mode lines", "OPEN \"mem:s\" FOR OUTPUT AS #1\nPRINT #1,\"x\"\nCLOSE #1\nOPEN \"mem:s\" FOR INPUT AS #1\nLINE INPUT #1,A$\nPRINT A$", "x\n" },
    { "RUN closes all channels", "OPEN \"mem:s\" FOR OUTPUT AS #1\n10 PRINT #1,\"x\"\nRUN", "? File not open in line 10\n" },
    { "NEW closes all channels", "OPEN \"mem:s\" FOR OUTPUT AS #1\nNEW\nPRINT #1,\"x\"", "? File not open\n" },
    { "RUN resets ADDRESS", "ADDRESS MEM\n10 PRINT ADDRESS$\nRUN\nPRINT ADDRESS$", "file\nfile\n" },
    { "ADDRESS survives direct mode lines", "ADDRESS MEM\nPRINT ADDRESS$\nPRINT ADDRESS$", "mem\nmem\n" },
    { "a program may read a file written by an earlier program", "10 OPEN \"mem:d\" FOR OUTPUT AS #1:PRINT #1,\"data\":CLOSE\nRUN\n10 OPEN \"mem:d\" FOR INPUT AS #1:LINE INPUT #1,A$:PRINT A$\nRUN", "data\n" },
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
    { "Break ends a WAIT",      "10 WAIT 3000\nRUN\nPRINT 5",                     "? Break in line 10\n5\n", 0, 3 },
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
    iodev_mem_clear();
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
    (void) ascii;
    return repl_save_program_ex( (repl_t*) usrdata, filename, vm->savemode );
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
        if ( repl.quit ) {      // BYE: like the host, stop entering lines; the exit code is part of the output
            char text[40];
            snprintf( text, sizeof(text), "<quit %d>\n", repl.exitcode );
            out_append( text );
            break;
        }
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
    static const char* const marks[HL_COUNT] = { "<>", "<L>", "<S>", "<C>", "<M>", "<F>", "<O>", "<I>", "<N>", "<T>", "<H>", "<R>", "<P>", "<D>", "<K>", "<X>", "<B>" };
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

// -- FILES -----------------------------------------------------------------------------------------

#define FILESDIR    "repltest1.d"

// the files FILES lists: ages and sizes are fixed (the time zone is UTC); the directory has a size of its own, so cases that
// list it strip the size column
static void make_files_fixture( void ) {
    int rv = system(
        "rm -rf " FILESDIR " && mkdir " FILESDIR " " FILESDIR "/sub && "
        "printf abc > " FILESDIR "/old.txt && touch -d '2020-01-02 03:04:05' " FILESDIR "/old.txt && "
        "printf 0123456789 > " FILESDIR "/prog.bas && touch -d '2021-05-06 07:08:09' " FILESDIR "/prog.bas && "
        "printf echo > " FILESDIR "/run.sh && chmod +x " FILESDIR "/run.sh && touch -d '2022-01-01 00:00:00' " FILESDIR "/run.sh && "
        "ln -s old.txt " FILESDIR "/link && touch -h -d '2022-06-07 08:09:10' " FILESDIR "/link && "
        "printf x > " FILESDIR "/.hidden && "
        "touch -d '2023-03-03 12:00:00' " FILESDIR "/sub " FILESDIR );
    (void) rv;
}

static int files_pauses = 0, files_yes = 0;

static bool host_pause( struct _repl_t* repl ) {
    ++files_pauses;
    return files_yes < 0 || files_pauses <= files_yes;
}

typedef struct _filescase_t {
    const char* name;
    const char* source;
    bool        hl;             // colored (the marks of host_hlstyle())
    bool        inside;         // run in the fixture directory
    bool        strip;          // remove the size column of every line (the size of a directory depends on the file system)
    int         pagesize;       // 0 = no paging
    int         yes;            // number of pauses that are answered with "go on" (-1 = all)
    const char* expected;
    int         pauses;         // expected number of pauses
    bool        masktime;       // replace the time of every line by "T" (files that were just made)
} filescase_t;

static const filescase_t filescases[] = {
    { "FILES a pattern: oldest first, sizes aligned, executable marked", "FILES \"" FILESDIR "/*.*\"", false, false, false, 0, 0,
      " 3 2020-01-02 03:04 " FILESDIR "/old.txt\n10 2021-05-06 07:08 " FILESDIR "/prog.bas\n 4 2022-01-01 00:00 " FILESDIR "/run.sh*\n", 0 },
    { "FILES a directory: links, directories, hidden names left out", "FILES \"" FILESDIR "\"", false, false, true, 0, 0,
      "2020-01-02 03:04 old.txt\n2021-05-06 07:08 prog.bas\n2022-01-01 00:00 run.sh*\n2022-06-07 08:09 link@ -> old.txt\n2023-03-03 12:00 sub/\n", 0 },
    { "FILES one file", "FILES \"" FILESDIR "/old.txt\"", false, false, false, 0, 0, "3 2020-01-02 03:04 " FILESDIR "/old.txt\n", 0 },
    { "FILES without a name lists the current directory", "FILES", false, true, true, 0, 0,
      "2020-01-02 03:04 old.txt\n2021-05-06 07:08 prog.bas\n2022-01-01 00:00 run.sh*\n2022-06-07 08:09 link@ -> old.txt\n2023-03-03 12:00 sub/\n", 0 },
    { "FILES a pattern without a match", "FILES \"" FILESDIR "/nothing*\"", false, false, false, 0, 0, "? File not found\n", 0 },
    { "FILES a missing name", "FILES \"repltest1nothing\"", false, false, false, 0, 0, "? File not found\n", 0 },
    { "FILES a name expression", "A$=\"" FILESDIR "/\"\nFILES A$+\"o*\"", false, false, false, 0, 0, "3 2020-01-02 03:04 " FILESDIR "/old.txt\n", 0 },
    { "FILES in a program", "10 PRINT 1:FILES \"" FILESDIR "/o*\":PRINT 2\nRUN", false, false, false, 0, 0, "1\n3 2020-01-02 03:04 " FILESDIR "/old.txt\n2\n", 0 },
    { "FILES highlighting: names by type, size, time", "FILES \"" FILESDIR "/[opr]*\"", true, false, false, 0, 0,
      "<N> 3 <R>2020-01-02 03:04 <I>" FILESDIR "/old.txt<>\n<N>10 <R>2021-05-06 07:08 <B>" FILESDIR "/prog.bas<>\n"
      "<N> 4 <R>2022-01-01 00:00 <X>" FILESDIR "/run.sh*<>\n", 0 },
    { "FILES highlighting: link", "FILES \"" FILESDIR "/li*\"", true, false, false, 0, 0,
      "<N>7 <R>2022-06-07 08:09 <K>" FILESDIR "/link@ -> old.txt<>\n", 0 },
    { "FILES highlighting: directory", "FILES \"" FILESDIR "/su?\"", true, false, true, 0, 0, "<R>2023-03-03 12:00 <D>" FILESDIR "/sub/<>\n", 0 },
    { "FILES pages: the host is asked after each page", "FILES \"" FILESDIR "\"", false, false, true, 3, -1,
      "2020-01-02 03:04 old.txt\n2021-05-06 07:08 prog.bas\n2022-01-01 00:00 run.sh*\n2022-06-07 08:09 link@ -> old.txt\n2023-03-03 12:00 sub/\n", 2 },
    { "FILES stops when the host says so", "FILES \"" FILESDIR "\"\nPRINT \"X\"", false, false, true, 3, 1,
      "2020-01-02 03:04 old.txt\n2021-05-06 07:08 prog.bas\n2022-01-01 00:00 run.sh*\n2022-06-07 08:09 link@ -> old.txt\nX\n", 2 },
    { "FILES on mem: lists the RAM disk", "10 END\nSAVE \"mem:z\",B\nFILES \"mem:\"", false, false, true, 0, 0, "T z.e99\n", 0, true },
    { "FILES on mem: with a pattern", "10 END\nSAVE \"mem:z\",B\nSAVE \"mem:y\",A\nFILES \"mem:*.bas\"", false, false, true, 0, 0, "T y.bas\n", 0, true },
    { "FILES uses the ADDRESS driver", "10 END\nSAVE \"mem:z\",B\nADDRESS MEM\nFILES", false, false, true, 0, 0, "T z.e99\n", 0, true },
    { "FILES on mem: without a match", "FILES \"mem:*.zzz\"", false, false, false, 0, 0, "? File not found\n", 0 },
    { "FILES on an unknown device", "FILES \"nodev:\"", false, false, false, 0, 0, "? Device not found\n", 0 },
    { "FILES on null:", "FILES \"null:\"", false, false, false, 0, 0, "? Not supported by device\n", 0 },
    { "FILES does not ask when everything fits on a page", "FILES \"" FILESDIR "/o*\"", false, false, false, 24, -1, "3 2020-01-02 03:04 " FILESDIR "/old.txt\n", 0 },
};

// removes the size column (leading spaces, then digits) of every line, in place
static void strip_sizes( char* text ) {
    char* out = text;
    const char* p = text;
    while ( *p ) {
        while ( *p == ' ' ) ++p;
        if ( strncmp( p, "<N>", 3 ) == 0 ) p += 3;
        while ( *p >= '0' && *p <= '9' ) ++p;
        if ( *p == ' ' ) ++p;
        while ( *p && *p != '\n' ) *out++ = *p++;
        if ( *p == '\n' ) *out++ = *p++;
    }
    *out = '\0';
}

// replaces "YYYY-MM-DD HH:MM" by "T" (after the size column has been taken away)
static void mask_times( char* text ) {
    char* out = text;
    const char* p = text;
    while ( *p ) {
        const char* q = p;
        while ( *q >= '0' && *q <= '9' ) ++q;      // a possible marker like "<R>" is not handled: the cases that mask are plain
        if ( q - p == 4 && q[0] == '-' && q[3] == '-' && q[6] == ' ' && q[9] == ':' ) {
            *out++ = 'T';
            p += 16;
        } else {
            *out++ = *p++;
        }
    }
    *out = '\0';
}

static void run_files_tests( size_t* pnum, size_t* pfailed ) {
    setenv( "TZ", "UTC", 1 );
    tzset();
    make_files_fixture();
    for ( size_t i=0; i < sizeof(filescases) / sizeof(filescases[0]); ++i ) {
        const filescase_t* tc = &filescases[i];
        out_reset();
        keys = 0; polls = 0; breakafter = 0;
        files_pauses = 0; files_yes = tc->yes;
        repl_t repl;
        bool pass = false;
        char cwd[1024];
        bool moved = tc->inside && getcwd( cwd, sizeof(cwd) ) != 0 && chdir( FILESDIR ) == 0;
        if ( init_repl( &repl ) ) {
            repl.report   = host_report;
            repl.print    = host_print;
            repl.save     = host_save;
            repl.pagesize = tc->pagesize;
            repl.pause    = host_pause;
            if ( tc->hl ) repl.hlstyle = host_hlstyle;
            char* src = strdup( tc->source );
            for ( char* line = strtok( src, "\n" ); line; line = strtok( 0, "\n" ) ) {
                repl_enter( &repl, line );
            }
            free( src );
            cleanup_repl( &repl );
            if ( moved && chdir( cwd ) != 0 ) printf( "cannot go back\n" );
            if ( tc->strip ) strip_sizes( outbuf );
            if ( tc->masktime ) mask_times( outbuf );
            pass = strcmp( outbuf, tc->expected ) == 0 && files_pauses == tc->pauses;
        }
        printf( "%s  %s\n", pass ? "PASS" : "FAIL", tc->name );
        if ( !pass ) {
            show( "expected", tc->expected );
            show( "got     ", outbuf );
            printf( "    pauses %d, expected %d\n", files_pauses, tc->pauses );
            ++*pfailed;
        }
        ++*pnum;
    }
    int rv = system( "rm -rf " FILESDIR );
    (void) rv;
}

// -- EDIT ------------------------------------------------------------------------------------------

// the line editor callback shows the text it is given as "[text]" and answers with the next line of the case's reply
// (lines separated by '\n'); no lines left = the user pressed ESC
static const char* edit_reply = 0;

static char* host_edit( struct _repl_t* repl, const char* text ) {
    out_append( "[" );
    out_append( text );
    out_append( "]\n" );
    if ( edit_reply == 0 || *edit_reply == '\0' ) {
        return 0;
    }
    const char* nl = strchr( edit_reply, '\n' );
    size_t len = nl ? (size_t)( nl - edit_reply ) : strlen( edit_reply );
    char* line = (char*) malloc( len + 1U );
    memcpy( line, edit_reply, len );
    line[ len ] = '\0';
    edit_reply += nl ? len + 1U : len;
    return line;
}

typedef struct _editcase_t {
    const char* name;
    const char* source;
    const char* reply;      // lines the editor returns, one per call (0 = ESC)
    const char* expected;
} editcase_t;

static const editcase_t editcases[] = {
    { "EDIT and ESC: no change",    "10 PRINT 1\nEDIT 10\nLIST",           0,            "[10 PRINT 1]\n10 PRINT 1\n" },
    { "EDIT replaces the line",     "10 PRINT 1\nEDIT 10\nLIST\nRUN",      "10 PRINT 2", "[10 PRINT 1]\n10 PRINT 2\n2\n" },
    { "EDIT with a label",          "10 LABEL L\n20 PRINT 1\nEDIT L\nLIST", "20 PRINT 3",  "[10 LABEL L]\n10 LABEL L\n20 PRINT 3\n" },
    { "EDIT under a new number copies the line", "10 PRINT 1\nEDIT 10\nLIST", "20 PRINT 1", "[10 PRINT 1]\n10 PRINT 1\n20 PRINT 1\n" },
    { "EDIT, line number alone deletes the line", "10 PRINT 1\nEDIT 10\nLIST", "10",       "[10 PRINT 1]\n" },
    { "EDIT, a direct mode line is run", "10 PRINT 1\nEDIT 10",            "PRINT 42",   "[10 PRINT 1]\n42\n" },
    { "EDIT without a target asks for the full-screen editor", "EDIT",   "X",          "" },
    { "EDIT undefined line",        "10 PRINT 1\nEDIT 99",               "X",          "? Undefined line number 99\n" },
    { "EDIT undefined label",       "10 PRINT 1\nEDIT X",                "X",          "? Undefined label X\n" },
    { "EDIT in a program ends the run", "10 EDIT 20\n20 PRINT 1\n30 PRINT 3\nRUN", 0,     "[20 PRINT 1]\n" },
    { "EDIT in a program, the edited line is entered", "10 EDIT 20\n20 PRINT 1\nRUN\nLIST", "20 PRINT 7", "[20 PRINT 1]\n10 EDIT 20\n20 PRINT 7\n" },
    { "EDIT without a target in a program", "10 EDIT\n20 PRINT 1\nRUN",  "X",          "" },
    { "an edited EDIT line edits again", "10 PRINT 1\nEDIT 10",          "EDIT 10\nPRINT 5", "[10 PRINT 1]\n[10 PRINT 1]\n5\n" },
    { "EDIT in a program edits again, no nesting", "10 EDIT 10\nRUN",    "RUN\nRUN\n10 PRINT 9", "[10 EDIT 10]\n[10 EDIT 10]\n[10 EDIT 10]\n" },
};

static void run_edit_tests( size_t* pnum, size_t* pfailed ) {
    for ( size_t i=0; i < sizeof(editcases) / sizeof(editcases[0]); ++i ) {
        const editcase_t* tc = &editcases[i];
        out_reset();
        keys = 0; polls = 0; breakafter = 0;
        edit_reply = tc->reply;
        repl_t repl;
        bool pass = false;
        if ( init_repl( &repl ) ) {
            repl.report = host_report;
            repl.print  = host_print;
            repl.edit   = host_edit;
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
    edit_reply = 0;
}

// -- full-screen editor request, file name -------------------------------------------------------

static void run_fulledit_tests( size_t* pnum, size_t* pfailed ) {
    static const struct { const char* name; const char* source; bool fulledit; const char* filename; bool monitor; } tests[] = {
        { "EDIT without a target sets the full-screen editor flag", "EDIT",                         true,  0, false },
        { "MONITOR sets the monitor flag",         "MONITOR",                                      false, 0, true },
        { "MONITOR in a program ends the run",     "10 MONITOR\n20 PRINT 1\nRUN",                  false, 0, true },
        { "no monitor flag without MONITOR",       "10 PRINT 1\nRUN",                             false, 0, false },
        { "EDIT with a target doesn't",            "10 PRINT 1\nEDIT 10",                         false, 0, false },
        { "EDIT in a program sets the flag",       "10 EDIT\n20 PRINT 1\nRUN",                    true,  0, false },
        { "no flag without EDIT",                  "10 PRINT 1\nRUN",                             false, 0, false },
        { "SAVE remembers the file name",          "10 PRINT 1\nSAVE \"" TESTFILE "\",B",          false, TESTFILE, false },
        { "LOAD remembers the file name",          "LOAD \"" TESTFILE "\"",                      false, TESTFILE, false },
        { "NEW forgets the file name",             "LOAD \"" TESTFILE "\"\nNEW",                 false, 0, false },
    };
    for ( size_t i=0; i < sizeof(tests) / sizeof(tests[0]); ++i ) {
        out_reset();
        keys = 0; polls = 0; breakafter = 0;
        repl_t repl;
        bool pass = false;
        if ( init_repl( &repl ) ) {
            repl.report = host_report;
            repl.print  = host_print;
            repl.save   = host_save;
            char* src = strdup( tests[i].source );
            for ( char* line = strtok( src, "\n" ); line; line = strtok( 0, "\n" ) ) {
                repl_enter( &repl, line );
            }
            free( src );
            pass = repl.fulledit == tests[i].fulledit && repl.monitor == tests[i].monitor &&
                ( tests[i].filename == 0 ? repl.filename == 0 : repl.filename != 0 && strcmp( repl.filename, tests[i].filename ) == 0 );
            cleanup_repl( &repl );
        }
        printf( "%s  %s\n", pass ? "PASS" : "FAIL", tests[i].name );
        if ( !pass ) {
            ++*pfailed;
        }
        ++*pnum;
    }
}

// EDIT WINDOW, the program as text and the program replaced by text (the editor window of the window host)
static void run_editwindow_tests( size_t* pnum, size_t* pfailed ) {
    repl_t repl;
    if ( !init_repl( &repl ) ) {
        return;
    }
    repl.report = host_report;
    repl.print  = host_print;
    out_reset();
    struct { const char* name; bool ok; } r[16];
    int n = 0;
    repl_enter( &repl, "EDIT WINDOW" );
    r[n].name = "EDIT WINDOW sets the editor flag and the window flag"; r[n++].ok = repl.fulledit && repl.editwin;
    repl.fulledit = repl.editwin = false;
    repl_enter( &repl, "EDIT" );
    r[n].name = "EDIT alone sets the editor flag, not the window flag"; r[n++].ok = repl.fulledit && !repl.editwin;
    repl.fulledit = repl.editwin = false;
    repl_enter( &repl, "10 EDIT WINDOW" );
    repl_enter( &repl, "RUN" );
    r[n].name = "EDIT WINDOW in a program"; r[n++].ok = repl.fulledit && repl.editwin;
    repl.fulledit = repl.editwin = false;
    repl_enter( &repl, "NEW" );
    repl_enter( &repl, "10 PRINT \"A\"" );
    repl_enter( &repl, "20 FOR I=1 TO 3:PRINT I:NEXT" );
    size_t len = 0;
    char* text = repl_program_text( &repl, &len );
    r[n].name = "the program as text"; r[n++].ok = text != 0 && strcmp( text, "10 PRINT \"A\"\n20 FOR I=1 TO 3:PRINT I:NEXT\n" ) == 0 && len == strlen( text );
    free( text );
    char msg[200];
    size_t count = 0;
    r[n].name = "a text with a line without a number changes nothing";
    r[n++].ok = !repl_apply_text( &repl, "30 PRINT 3\nPRINT 4\n", msg, sizeof(msg), &count ) && strstr( msg, "Line 2" ) != 0 && repl.pgm.first != 0 && repl.pgm.first->lineno == 10;
    r[n].name = "a text replaces the program";
    r[n++].ok = repl_apply_text( &repl, "5 REM x\n15 PRINT 1\n\n  25 PRINT 2  \r\n35\n", msg, sizeof(msg), &count ) && msg[0] == '\0' && count == 3U &&
        repl.pgm.first != 0 && repl.pgm.first->lineno == 5 && repl.pgm.last->lineno == 25;
    text = repl_program_text( &repl, &len );
    r[n].name = "the lines are as entered"; r[n++].ok = text != 0 && strcmp( text, "5 ' x\n15 PRINT 1\n25 PRINT 2\n" ) == 0;
    free( text );
    repl.save = host_save;
    repl_enter( &repl, "SAVE \"" TESTFILE "\",B" );
    r[n].name = "the file name stays after a text was applied";
    r[n++].ok = repl_apply_text( &repl, "1 PRINT 1\n", msg, sizeof(msg), &count ) && count == 1U && repl.filename != 0 && strcmp( repl.filename, TESTFILE ) == 0;
    for ( int i = 0; i < n; ++i ) {
        printf( "%s  %s\n", r[i].ok ? "PASS" : "FAIL", r[i].name );
        if ( !r[i].ok ) {
            ++*pfailed;
        }
        ++*pnum;
    }
    cleanup_repl( &repl );
}

// -- debugging: breakpoints, BRK, single-step ----------------------------------------------------

static void run_debug_tests( size_t* pnum, size_t* pfailed ) {
    repl_t repl;
    bool pass;
    size_t n = 0;
#define DBGCHECK( name, cond ) do { pass = ( cond ); printf( "%s  %s\n", pass ? "PASS" : "FAIL", name ); if ( !pass ) ++*pfailed; ++*pnum; ++n; } while ( 0 )
    out_reset();
    keys = 0; polls = 0; breakafter = 0;
    if ( !init_repl( &repl ) ) {
        ++*pfailed; ++*pnum;
        return;
    }
    repl.report = host_report;
    repl.print  = host_print;
    repl_enter( &repl, "10 PRINT 1" );
    repl_enter( &repl, "20 PRINT 2" );
    repl_enter( &repl, "30 PRINT 3" );
    DBGCHECK( "debug: the program compiles", repl_compile( &repl ) );
    uint32_t at20 = 0, at30 = 0;
    DBGCHECK( "debug: line addresses", cgen_find_line( &repl.rt.codegen, 20, &at20 ) && cgen_find_line( &repl.rt.codegen, 30, &at30 ) && at20 < at30 );
    uint32_t bps[2] = { at20, at30 };
    DBGCHECK( "debug: start", repl_vm_start( &repl, 0 ) && repl.vm.suspended && repl.vm.pc == 0 );
    DBGCHECK( "debug: runs to the first breakpoint", repl_vm_resume( &repl, false, bps, 2 ) && repl.vm.suspended &&
        repl.vm.stopreason == VMSTOP_BREAKPOINT && repl.vm.pc == at20 && strcmp( outbuf, "1\n" ) == 0 );
    DBGCHECK( "debug: leaves a breakpoint and stops at the next", repl_vm_resume( &repl, false, bps, 2 ) && repl.vm.suspended &&
        repl.vm.pc == at30 && strcmp( outbuf, "1\n2\n" ) == 0 );
    DBGCHECK( "debug: runs to the end", repl_vm_resume( &repl, false, bps, 2 ) && !repl.vm.suspended &&
        repl.vm.stopreason == VMSTOP_NONE && strcmp( outbuf, "1\n2\n3\n" ) == 0 );
    // single-step through the whole program
    out_reset();
    unsigned steps = 0;
    bool ok = repl_vm_start( &repl, 0 );
    while ( ok && repl.vm.suspended && steps < 1000 ) {
        uint32_t before = repl.vm.pc;
        ok = repl_vm_resume( &repl, true, 0, 0 );
        if ( repl.vm.suspended && ( repl.vm.stopreason != VMSTOP_STEP || repl.vm.pc <= before ) ) {
            ok = false;
        }
        ++steps;
    }
    DBGCHECK( "debug: single-step runs the program one instruction at a time", ok && !repl.vm.suspended && steps > 6 &&
        strcmp( outbuf, "1\n2\n3\n" ) == 0 );
    // BRK stops with the debugger and is an error without it
    out_reset();
    repl.rt.codegen.code[ at20 ] = INS_BRK;         // LINE 20 becomes BRK NOP NOP
    repl.rt.codegen.code[ at20 + 1U ] = INS_NOP;
    repl.rt.codegen.code[ at20 + 2U ] = INS_NOP;
    DBGCHECK( "debug: start again", repl_vm_start( &repl, 0 ) );
    DBGCHECK( "debug: BRK stops the run", repl_vm_resume( &repl, false, 0, 0 ) && repl.vm.suspended &&
        repl.vm.stopreason == VMSTOP_BRK && repl.vm.pc == at20 + 1U && strcmp( outbuf, "1\n" ) == 0 );
    DBGCHECK( "debug: and the run continues behind it", repl_vm_resume( &repl, false, 0, 0 ) && !repl.vm.suspended &&
        strcmp( outbuf, "1\n2\n3\n" ) == 0 );
    out_reset();
    repl.vm.debug = false;
    DBGCHECK( "BRK without the debugger is an error", !vm_run_at( &repl.vm, 0 ) && strstr( outbuf, "Breakpoint" ) != 0 );
    DBGCHECK( "debug: a start address outside of the code is refused", !repl_vm_start( &repl, repl.rt.codegen.codesize ) );
#undef DBGCHECK
    cleanup_repl( &repl );
    (void) n;
}

// AUTO leaves its start and step in the REPL for the host, which prompts with the line numbers
static void run_auto_tests( size_t* pnum, size_t* pfailed ) {
    static const struct { const char* line; uint32_t start, step; } t[] = {
        { "AUTO", 10U, 10U }, { "AUTO 100", 100U, 10U }, { "AUTO 100,5", 100U, 5U }, { "AUTO 7,3", 7U, 3U },
    };
    for ( size_t i=0; i < sizeof(t) / sizeof(t[0]); ++i ) {
        repl_t repl;
        ++*pnum;
        if ( !setup_growth_repl( &repl ) ) { ++*pfailed; continue; }
        repl_enter( &repl, t[i].line );
        bool ok = repl.autoline == t[i].start && repl.autostep == t[i].step;
        printf( "%s  %s\n", ok ? "PASS" : "FAIL", t[i].line );
        if ( !ok ) {
            ++*pfailed;
            printf( "    autoline %u, autostep %u\n", (unsigned) repl.autoline, (unsigned) repl.autostep );
        }
        cleanup_repl( &repl );
    }
}

// TOOL: runs a BASIC program of the tools folder in a repl_t of its own; the caller's program and variables stay
static int tool_begins = 0, tool_ends = 0;
static void host_toolbegin( repl_t* outer ) { (void) outer; ++tool_begins; }
static void host_toolend( repl_t* outer ) { (void) outer; ++tool_ends; }

static void tool_write( const char* path, const char* text ) {
    FILE* f = fopen( path, "w" );
    if ( f != 0 ) {
        fputs( text, f );
        fclose( f );
    }
}

static bool tool_case( size_t* pnum, size_t* pfailed, const char* name, bool toolsok, const char* source, const char* expected ) {
    ++*pnum;
    out_reset();
    keys = 0; polls = 0; breakafter = 0;
    tool_begins = tool_ends = 0;
    repl_t repl;
    bool ok = false;
    if ( init_repl( &repl ) ) {
        repl.report = host_report;
        repl.print  = host_print;
        repl.input  = host_input;
        repl.inkey  = host_inkey;
        repl.interrupt = host_interrupt;
        repl.toolsok = toolsok;
        repl.toolbegin = host_toolbegin;
        repl.toolend = host_toolend;
        char* src = strdup( source );
        for ( char* line = strtok( src, "\n" ); line; line = strtok( 0, "\n" ) ) {
            repl_enter( &repl, line );
        }
        free( src );
        cleanup_repl( &repl );
        ok = strcmp( outbuf, expected ) == 0;
    }
    printf( "%s  %s\n", ok ? "PASS" : "FAIL", name );
    if ( !ok ) {
        ++*pfailed;
        show( "expected", expected );
        show( "got     ", outbuf );
    }
    return ok;
}

static void run_tool_tests( size_t* pnum, size_t* pfailed ) {
    char olddir[512], dir[300];
    const char* tmp = getenv( "TMPDIR" );
    snprintf( dir, sizeof(dir), "%s/repltest1-tools-XXXXXX", tmp != 0 && tmp[0] == '/' ? tmp : "/tmp" );
    if ( getcwd( olddir, sizeof(olddir) ) == 0 || mkdtemp( dir ) == 0 || chdir( dir ) != 0 ) {
        ++*pnum; ++*pfailed;
        printf( "FAIL  tools: scratch directory\n" );
        return;
    }
    mkdir( "tools", 0700 );
    mkdir( "lib", 0700 );
    tool_write( "tools/t1.bas", "10 PRINT \"tool \";ARGC;\" [\";ARG$(1);\"]\"\n20 A=9\n" );
    tool_write( "tools/t2.bas", "10 PRINT \"t2\"\n20 TOOL \"t1\",\"deep\"\n30 PRINT \"t2 back\"\n" );
    tool_write( "tools/t3.bas", "10 PRINT \"t3\"\n20 TOOL \"t3\"\n30 PRINT \"t3 end\"\n" );
    tool_write( "tools/t4.bas", "10 PRINT 1/0\n20 PRINT \"not reached\"\n" );
    tool_write( "tools/t5.bas", "10 OPEN \"mem:toolresult\" FOR OUTPUT AS #1:PRINT #1,\"done\":CLOSE #1\n" );
    tool_write( "tools/t6.bas", "10 LOAD LIBRARY \"tl\"\n20 PRINT FN TL_TWICE(21)\n" );
    tool_write( "lib/tl.lib", "10 LIBRARY \"tl\",\"TL_\"\n20 FUNC TL_TWICE(X)\n30   RETURN X*2\n40 END FUNC\n" );
    tool_write( "tools/t7.bas", "10 PRINT 1\n20 BYE 3\n" );
    tool_case( pnum, pfailed, "TOOL runs a tool and comes back", true, "TOOL \"t1\"\nPRINT \"after\"", "tool 0 []\nafter\n" );
    tool_case( pnum, pfailed, "TOOL: the argument is ARG$(1)", true, "TOOL \"t1\",\"file.txt\"", "tool 1 [file.txt]\n" );
    tool_case( pnum, pfailed, "TOOL: the stored program stays", true, "10 PRINT \"main\"\n20 PRINT \"two\"\nTOOL \"t1\"\nLIST\nRUN", "tool 0 []\n10 PRINT \"main\"\n20 PRINT \"two\"\nmain\ntwo\n" );
    tool_case( pnum, pfailed, "TOOL: the variables stay", true, "A=5\nTOOL \"t1\"\nPRINT A", "tool 0 []\n5\n" );
    tool_case( pnum, pfailed, "TOOL in a running program: it goes on afterwards", true, "10 A=1\n20 PRINT \"before\"\n30 TOOL \"t1\",\"x\"\n40 PRINT \"after\";A\nRUN", "before\ntool 1 [x]\nafter1\n" );
    tool_case( pnum, pfailed, "TOOL: the hooks are called once", true, "TOOL \"t1\"", "tool 0 []\n" );
    printf( "%s  TOOL: the hooks are called once (counters)\n", tool_begins == 1 && tool_ends == 1 ? "PASS" : "FAIL" );
    ++*pnum; if ( !( tool_begins == 1 && tool_ends == 1 ) ) ++*pfailed;
    tool_case( pnum, pfailed, "TOOL: a tool can start a tool", true, "TOOL \"t2\"\nPRINT \"main\"", "t2\ntool 1 [deep]\nt2 back\nmain\n" );
    tool_case( pnum, pfailed, "TOOL: nesting is limited", true, "TOOL \"t3\"\nPRINT \"main\"",
        "t3\nt3\nt3\nt3\n? Tools are nested too deeply in line 20\n? Tools are nested too deeply in line 20\nt3 end\n? Tools are nested too deeply in line 20\nt3 end\n? Tools are nested too deeply in line 20\nt3 end\n? Tools are nested too deeply in line 20\nmain\n" );
    tool_case( pnum, pfailed, "TOOL: an error in the tool ends the tool only", true, "TOOL \"t4\"\nPRINT \"main\"", "? Division by zero in line 10\n? Division by zero in line 10\nmain\n" );
    tool_case( pnum, pfailed, "TOOL: results through mem:", true, "TOOL \"t5\"\nOPEN \"mem:toolresult\" FOR INPUT AS #1:LINE INPUT #1,A$:CLOSE #1:PRINT A$", "done\n" );
    tool_case( pnum, pfailed, "TOOL: a tool finds its libraries next to its folder", true, "TOOL \"t6\"", "42\n" );
    tool_case( pnum, pfailed, "TOOL: BYE in a tool ends the tool", true, "TOOL \"t7\"\nPRINT \"main\"", "1\nmain\n" );
    tool_case( pnum, pfailed, "TOOL: an unknown tool", true, "TOOL \"nothere\"", "? Tool not found\n" );
    tool_case( pnum, pfailed, "TOOL: a name with a path is refused", true, "TOOL \"../t1\"", "? Bad tool name\n" );
    tool_case( pnum, pfailed, "TOOL: not available in the console host", false, "TOOL \"t1\"", "? TOOL is not available in the console host\n" );
    tool_case( pnum, pfailed, "TOOL is listed", true, "10 TOOL \"a\":TOOL \"b\",\"c\"+\"d\"\nLIST", "10 TOOL \"a\":TOOL \"b\",\"c\"+\"d\"\n" );
    tool_case( pnum, pfailed, "TOOL in lower case", true, "10 tool \"a\"\nLIST", "10 TOOL \"a\"\n" );
    if ( chdir( olddir ) != 0 ) { ++*pnum; ++*pfailed; }
    char cmd[400];
    snprintf( cmd, sizeof(cmd), "rm -rf '%s'", dir );
    if ( system( cmd ) != 0 ) { }
}

int main( int argc, char** argv ) {
    if ( argc > 1 && strcmp( argv[1], "-v" ) == 0 ) {
        basdbg_flags = BASDBG_ALL;
    }
    size_t numcases = sizeof(cases) / sizeof(cases[0]), failed = 0;
    cleanup_testfiles();
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
    run_files_tests( &numcases, &failed );
    run_edit_tests( &numcases, &failed );
    run_fulledit_tests( &numcases, &failed );
    run_editwindow_tests( &numcases, &failed );
    run_debug_tests( &numcases, &failed );
    run_auto_tests( &numcases, &failed );
    run_tool_tests( &numcases, &failed );
    cleanup_testfiles();
    printf( "%u of %u passed\n", (unsigned)( numcases - failed ), (unsigned) numcases );
    free( outbuf );
    return (int) failed;
}
