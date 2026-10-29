#pragma once
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

#ifndef NODETYPE_H
#define NODETYPE_H  1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

/*
Explanation of node types:

    NT_NUMEXLIST    numeric expression list
    NT_STREXLIST    string expression list
    NT_EXPRLIST     generic expression list
        data: none
        branches: point to expression objects in the list
        immediate processing: none

    [ NT_ARRAYINDEX - not generated ]

    NT_ARRAYSUB     array subscript
        data: none
        branches: 1
            - points to either NT_NUMEXLIST or string expression
        immediate processing: none

    NT_ARRAYDIMDECL array dimension declaration
        data:
            - 1 byte (TOK_DYNAMIC or TOK_ASSOC), if specified
            - 2 bytes per dimension of size information, if specified
        branches: none
        immediate processing:
            - if given, the numeric expressions are evaluated to
              see if they're constant. it's an error if they aren't.
            - the total size of the expected array is computed
              it's an error if it's too small or too large.
            - this computes the list of output dimensions

    NT_ARRAYDECL    array declaration
        data:
            - 1 byte of type indicator, 2 bytes of variable offset
        branches: none
        immediate processing:
            - the variable is looked up, to see if it exists
            - if it does, it's an error
            - if it doesn't, the variable is created, and the type and
              offset stored in the data field

    NT_ARRAYDECLLIST    array declaration list
        data: none
        branches: the NT_ARRAYDECL nodes

    NT_EMPTYARRAYREF    empty array reference, needed for ERASE statement
        data:
            - 1 byte of type indicator, 2 bytes of variable offset
        branches: none
        immediate processing:
            - the array is looked up, it's an error if it doesn't exist
            - the variable offset is encoded in the data field

    NT_EMPTYARRAYREFLIST    empty array reference list
        data: none
        branches: the NT_EMPTYARRAYREF nodes

    NT_NUMBASEVARREF    numeric base variable reference
        data:
            - 1 byte of type indicator, n bytes of name
        branches: none
        immediate processing: none (!)
        note:
            - because it's used in compound contexts, the variable reference
              cannot be resolved here.

    NT_NUMVARREF        numeric variable reference
        data:
            - 1 byte of type indicator
            - 2 bytes of variable offset
        branches:
            - list of array index expressions

    NT_STRBASEVARREF    string base variable reference
        data:
            - 1 byte of type indicator, n bytes of name
        branches: none
        immediate processing: none (!)
        note:
            - because it's used in compound contexts, the variable reference
              cannot be resolved here.

    NT_STRVARREF        string variable reference
        data:
            - 1 byte of type indicator
            - 2 bytes of variable offset
        branches:
            - list of array index expressions

    [ NT_ANYBASEVARREF - not generated ]

    NT_DECLIT       decimal literal
        data:
            - numeric value, 8 bytes in network byte order
        immediate processing:
            - consumes TOK_DECLIT or TOK_DEC0..DEC9, and generates a floating-point representation,
              which is then stored into the data field.

    NT_NUMLIT       numeric literal
        data:
            - numeric value, 8 bytes in network byte order
        immediate processing:
            - attempts NT_DECLIT first, and if successful, returns it as a NT_NUMLIT node.
            - otherwise, attempts the other number base literals (hex, oct, quad and bin)

    NT_STRLIT       string literal
        data:
            - 1 byte of type indicator (can be string, shell, bracket or brace literal)
            - n bytes of text
        branches: none
        note:
            - note that shell/bracket/brace literals aren't evaluated here, just gathered.

    NT_STRLITS      string literals
        data: none
        branches:
            - list of string literals (NT_STRLIT)

    NT_NUMUSRFNNAME     numeric user function name
    NT_STRUSRFNNAME     string  user function name
        data:
            - 1 byte of type indicator
            - n bytes of name

    NT_USRFNARG         user function argument
        data:
            - 1 byte of type indicator
            - n bytes of name (NUL-terminated)

    NT_USERFNARGLIST    user function argument list
        branches:
            - 2 or more branches of user function argument declarations
        immediate processing:
            - generated only if there are more than 2 arguments

    [ NT_ANYUSRFNNAME - not generated ]

    NT_SINGLELINEUSRFNBODY  single-line user function body
        branches:
            - 1 branch of expression

    NT_MULTILINEUSERFNBODY  multi-line user function body
        branches:
            - 0 or more branches of statement lines

    [ NT_USRFNBODY - not generated ]

    NT_USRFNDECL
        data:
            - 1 byte of function type
            - 2 bytes of variable offset
        branches:
            - 1 branch of expression or statement list

    NT_NUMUSRFNCALL     numeric user function call
    NT_STRUSRFNCALL     string user function call
        data:
            - 1 byte of type indicator
            - 2 bytes of variable offset
        branches:
            - argument expression list (can be empty)

    NT_SYSNUMFUNCARGCALL    numeric system function call with arguments
    NT_SYSSTRFUNCARGCALL    string system function call with arguments
        data:
            - 1 byte of function token (like TOK_VAL)
        branches:
            - 1 or more branches of expression list (depending on function)

    NT_SYSNOARGSTRCALL  system string function call without arguments
    NT_SYSNOARGNUMCALL  system number function call without arguments
        data:
            - 1 byte of function token (like TOK_TI)

    [ NT_NUMFUNCCALL - not generated ]
    [ NT_STRFUNCCALL - not generated ]
    [ NT_STRBASEXPR - not generated ]

    NT_STRADDEXPR   string addition expression
        branches:
            - at least 2 branches of string expressions
        immediate processing:
            - generated only if there is 2 or more nodes

    NT_STREXPR
        branches:
            - 1 branch of numeric expression
        immediate processing:
            - generated as a hint to the code generator

    [ NT_NUMSUBEXPR - not generated ]
    [ NT_NUMBASEEXPR - not generated ]

    NT_OPERATOR     operator
        data:
            - 1 byte of operator token
        branches:
            - 1 branch of operand

    NT_NUMUNARYEX   numeric unary expression
        data:
            - 1 byte of operator token
        branches:
            - 1 branch of operand
        immediate processing:
            - generated only if a unary expression was used

    NT_NUMMULTEX   numeric multiplication expression
        branches:
            - 1 branch of first operand
            - at least 1 branch of NT_OPERATOR
        immediate processing:
            - generated only if a multiplication operator was used

    NT_NUMADDEX   numeric addition expression
        branches:
            - 1 branch of first operand
            - at least 1 branch of NT_OPERATOR
        immediate processing:
            - generated only if an addition operator was used

    NT_NUMSHIFTEX   numeric shift expression
        branches:
            - 1 branch of first operand
            - at least 1 branch of NT_OPERATOR
        immediate processing:
            - generated only if a shift operator was used

    NT_NUMCMPEX   numeric comparison expression
        branches:
            - 1 branch of first operand
            - at least 1 branch of NT_OPERATOR
        immediate processing:
            - generated only if a comparison operator was used

    NT_STRCMPEX  string comparison expression
        branches:
            - 1 branch of first operand
            - at least 1 branch of NT_OPERATOR
        immediate processing:
            - generated as a hint to the code generator
            - generated only if a comparison operator was used

    NT_NUMANDEX   numeric AND expression
        branches:
            - 1 branch of first operand
            - at least 1 branch of NT_OPERATOR
        immediate processing:
            - generated only if a multiplication operator was used

    NT_NUMOREX   numeric OR expression
        branches:
            - 1 branch of first operand
            - at least 1 branch of NT_OPERATOR
        immediate processing:
            - generated only if a multiplication operator was used

    NT_NUMEXPR
        branches:
            - 1 branch of numeric expression
        immediate processing:
            - generated as a hint to the code generator

    [ NT_EXPR - not generated ]

    NT_SAVESTMT     SAVE statement
        data:
            - n bytes of save mode (optional)
        branches:
            - 1 branch of string expression
        immediate processing:
            - the SAVE statement is special b/c it uses an identifier as optional second parameter
              denoting save mode (A for ASCII, B (default) for binary)
            - the file name need not be a string literal

    NT_CHANSPEC     channel specifier
        branches:
            - 1 branch of numeric expression

    NT_PRINTSEP     print separator
        data:
            - 1 byte of separator token

    NT_PRINTARG     print argument
        branches:
            - 1 branch of expression
            - 1 optional branch of separator

    NT_PRINTARGLIST     print argument
        branches:
            - 2 or more branches of print arguments
        immediate processing:
            - generated only if there's more than one print argument

    NT_PRINTSTMT    print statement
        branches:
            - 1 optional branch of channel info
            - 1 optional branch of print argument list

    [ NT_IOSTMT - not generated ]

    NT_NUMASSIGN    numeric assignment
        branches:
            - 1 branch of numeric variable reference
            - 1 branch of numeric expression

    NT_STRASSIGN    string assignment
        branches:
            - 1 branch of string variable reference
            - 1 branch of string expression

    NT_SUBSTRASSIGN     substring assignment
        data:
            - 1 byte of substring operator
        branches:
            - 1 branch of expression list
            - 1 branch of string expression

    [ NT_ANYASSIGN - not generated ]

    NT_ASSIGNLIST       assignment list
        branches:
            - 2 or more branches of assignment expressions
        immediate processing:
            - not generated if there's only one assignment expression

    NT_LETSTMT      LET statement
        branches:
            - 1 branch of assignment list

    NT_DIMSTMT      DIM statement
        branches:
            - 1 branch of array declarator list

    NT_ERASESTMT    ERASE statement
        branches:
            - 1 branch of empty array reference list

    NT_DEFSTMT      DEF statement
        branches:
            - 1 branch of user function declaration

    [ NT_ASSIGNSTMT - not generated ]

    NT_FORSTMT      FOR statement
        branches:
            - 1 branch of variable reference
            - 1 branch of FROM expression
            - 1 branch of TO expression
            - 1 branch of STEP expression (may be empty)

    NT_NEXTSTMT     NEXT statement
        branches:
            - 1 branch of variable reference list

    NT_GOTOTARGET   GOTO/GOSUB target
        data:
            - 1 byte of type (integer or label)
            - 3 bytes of line number or variable offset (label!, high byte zero)

    NT_GOTOSTMT     GOTO/GOSUB statement
        data:
            - 1 byte of GOTO/GOSUB token
        branches:
            - 1 branch of goto/gosub target

    NT_RETURNSTMT   RETURN statement

    NT_LABELSTMT    LABEL statement
        data:
            - 2 bytes of label variable offset

    [ NT_ENDIFKW - not generated ]
    [ NT_ENDUNLESSKW - not generated ]
    [ NT_THENKW - not generated ]

    NT_SINGLELINEIFSTMT     Single-line IF statement
        data:
            - 1 token byte: IF/UNLESS
            - 1 token byte: THEN/GOTO/GOSUB
        branches:
            - 1 branch of IF numeric expression
            - 1 branch of THEN gosub/goto target or statement list
            - 1 optional branch of ELSE gosub/goto target or statement list
        immediate processing:
            - THEN/ELSE branches are either both gosub/goto targets or
              both statement lists

    NT_MULTILINEIFSTMT      Multi-line IF statement
        data:
            - 1 byte of IF/UNLESS token
        branches:
            - 1 branch of IF numeric expression
            - 1 branch of statement lines
            - 1 optional branch of ELSE statement lines

    [ NT_CONTROLFLOWSTMT - not generated ]
    [ NT_STMT - not generated ]

    NT_STMTLIST         Statement list
        branches:
            - 2 or more statements
        immediate processing:
            - generated only if more than 1 statement

    NT_STMTLINE         Statement line
        data:
            - 3 bytes of line number (not present in direct mode)
        branches:
            - 1 branch of statement list
            - this is the root node for direct mode

    NT_ENDSTMT          END statement (ends the program)

    NT_POPSTMT          POP statement (discards the latest GOSUB return address)

    NT_AGAINSTMT        AGAIN statement
        data:
            - 2 bytes of loop number: 1 = innermost loop, 2 = the one around it, ...

    NT_REPEATSTMT       REPEAT statement (the loop is closed by UNTIL)

    NT_UNTILSTMT        UNTIL statement
        branches:
            - 1 branch of numeric expression
        immediate processing:
            - none, the code generator decides: if the innermost open loop is a REPEAT, this statement
              closes it (the loop repeats until the expression is true), otherwise it opens a loop that
              runs until the expression is true and is closed by UEND

    NT_WHILESTMT        WHILE statement (opens a loop that runs while the expression is true, closed by WEND)
        branches:
            - 1 branch of numeric expression

    NT_WENDSTMT         WEND statement (closes a WHILE loop)

    NT_UENDSTMT         UEND statement (closes an UNTIL loop)

    NT_ONSTMT           ON statement
        data:
            - 1 byte of GOTO/GOSUB token
        branches:
            - 1 branch of numeric expression
            - 1 or more branches of goto/gosub targets

    NT_DATASTMT         DATA statement
        branches:
            - 1 or more branches of data items: NT_NUMLIT (a sign is folded into the value) or NT_STRLIT
              (string literals only, not shell or bracket literals)

    NT_READSTMT         READ statement
        branches:
            - 1 or more branches of variable references (NT_NUMVARREF or NT_STRVARREF, regular or array element)

    NT_RESTORESTMT      RESTORE statement
        branches:
            - none, or 1 branch of goto target (a line number or a label)

    NT_INPUTSTMT        INPUT statement
        data:
            - 1 byte of flags (INPUTF_*): channel present, prompt present, no "? " after the prompt
        branches:
            - optional channel info (NT_CHANSPEC), optional prompt (NT_STRLITS or NT_STRLIT)
            - 1 or more branches of variable references (NT_NUMVARREF or NT_STRVARREF)

    NT_GETSTMT          GET statement
        data:
            - 1 byte of flags (INPUTF_CHAN)
        branches:
            - optional channel info (NT_CHANSPEC)
            - 1 or more branches of variable references (NT_NUMVARREF or NT_STRVARREF)

    NT_FNASSIGNSTMT     Assignment of the result of a multi-line user function ("FN A = expr")
        data:
            - 1 byte of type indicator (VARTYPEV_FLOAT, VARTYPEV_INT or VARTYPEV_STR), n bytes of function name
        branches:
            - 1 branch of expression (numeric or string, depending on the function)
        immediate processing:
            - only allowed in the body of the function with this name

    NT_STMTLINES        Statement lines
        branches:
            - 2 or more statement lines
        immediate processing:
            - generated only if more than 1 statement
            - this is the root node for program mode
*/

#define NT_UNKNOWN              UINT8_C(0X00)   // unknown node type
#define NT_NUMEXLIST            UINT8_C(0X01)   // numeric expression list
#define NT_STREXLIST            UINT8_C(0X02)   // string expression list
#define NT_EXPRLIST             UINT8_C(0X03)   // generic expression list
#define NT_ARRAYSUB             UINT8_C(0X04)   // array subscript
#define NT_ARRAYDIMDECL         UINT8_C(0X05)   // array dimension declaration
#define NT_ARRAYDECL            UINT8_C(0X06)   // array declaration
#define NT_ARRAYDECLLIST        UINT8_C(0X07)   // array declaration list
#define NT_EMPTYARRAYREF        UINT8_C(0X08)   // empty array reference, needed for ERASE statement
#define NT_EMPTYARRAYREFLIST    UINT8_C(0X09)   // empty array reference list
#define NT_NUMBASEVARREF        UINT8_C(0X0A)   // numeric base variable reference
#define NT_NUMVARREF            UINT8_C(0X0B)   // numeric variable reference
#define NT_STRBASEVARREF        UINT8_C(0X0C)   // string base variable reference
#define NT_STRVARREF            UINT8_C(0X0D)   // string variable reference
#define NT_DECLIT               UINT8_C(0X0E)   // decimal literal
#define NT_NUMLIT               UINT8_C(0X0F)   // numeric literal
#define NT_STRLIT               UINT8_C(0X10)   // string literal
#define NT_STRLITS              UINT8_C(0X11)   // string literals
#define NT_NUMUSRFNNAME         UINT8_C(0X12)   // numeric user function name
#define NT_STRUSRFNNAME         UINT8_C(0X13)   // string  user function name
#define NT_NUMUSRFNCALL         UINT8_C(0X14)   // numeric user function call
#define NT_STRUSRFNCALL         UINT8_C(0X15)   // string user function call
#define NT_STRADDEXPR           UINT8_C(0X17)   // string addition expression
#define NT_NUMUNARYEX           UINT8_C(0X19)   // numeric unary expression
#define NT_NUMMULTEX            UINT8_C(0X1B)   // numeric multiplication expression
#define NT_NUMADDEX             UINT8_C(0X1D)   // numeric addition expression
#define NT_NUMSHIFTEX           UINT8_C(0X1F)   // numeric shift expression
#define NT_NUMCMPEX             UINT8_C(0X21)   // numeric shift expression
#define NT_NUMANDEX             UINT8_C(0X23)   // numeric AND expression
#define NT_NUMOREX              UINT8_C(0X25)   // numeric OR expression
#define NT_SAVESTMT             UINT8_C(0X26)   // SAVE statement
#define NT_CHANSPEC             UINT8_C(0X27)   // channel specifier
#define NT_PRINTSEP             UINT8_C(0X28)   // print separator
#define NT_PRINTARG             UINT8_C(0X29)   // print argument
#define NT_PRINTARGLIST         UINT8_C(0X2A)   // print argument
#define NT_PRINTSTMT            UINT8_C(0X2B)   // print statement
#define NT_NUMASSIGN            UINT8_C(0X2C)   // numeric assignment
#define NT_STRASSIGN            UINT8_C(0X2D)   // string assignment
#define NT_SUBSTROP             UINT8_C(0X2E)   // substring operator
#define NT_SUBSTRASSIGN         UINT8_C(0X2F)   // substring assignment
#define NT_ASSIGNLIST           UINT8_C(0X30)   // assignment list
#define NT_LETSTMT              UINT8_C(0X31)   // LET statement
#define NT_DIMSTMT              UINT8_C(0X32)   // DIM statement
#define NT_ERASESTMT            UINT8_C(0X33)   // ERASE statement
#define NT_FORSTMT              UINT8_C(0X34)   // FOR statement
#define NT_NEXTSTMT             UINT8_C(0X35)   // NEXT statement
#define NT_GOTOKW               UINT8_C(0X36)   // GOTO/GOSUB keyword
#define NT_GOTOTARGET           UINT8_C(0X37)   // GOTO/GOSUB target
#define NT_GOTOSTMT             UINT8_C(0X38)   // GOTO/GOSUB statement
#define NT_RETURNSTMT           UINT8_C(0X39)   // RETURN statement
#define NT_LABELSTMT            UINT8_C(0X3A)   // LABEL statement
#define NT_SINGLELINEIFSTMT     UINT8_C(0X3B)   // Single-line IF statement
#define NT_MULTILINEIFSTMT      UINT8_C(0X3C)   // Multi-line IF statement
#define NT_STMTLIST             UINT8_C(0X3D)   // Statement list
#define NT_STMTLINE             UINT8_C(0X3E)   // Statement line
#define NT_STMTLINES            UINT8_C(0X3F)   // Statement lines
#define NT_USERFNARGLIST        UINT8_C(0X40)   // User function argument list
#define NT_SINGLELINEUSERFNBODY UINT8_C(0X41)   // single-line user function body
#define NT_MULTILINEUSERFNBODY  UINT8_C(0X42)   // multi-line user function body
#define NT_USRFNDECL            UINT8_C(0X43)   // user function declaration
#define NT_DEFSTMT              UINT8_C(0X44)   // User function definition
#define NT_USRFNARG             UINT8_C(0X45)   // User function argument
#define NT_SYSNUMFUNC           UINT8_C(0X46)   // Numeric system function
#define NT_SYSSTRFUNC           UINT8_C(0X47)   // String system function
#define NT_SYSNUMFUNCARGCALL    UINT8_C(0X48)   // Numeric system function call with arguments
#define NT_SYSSTRFUNCARGCALL    UINT8_C(0X49)   // String system function call with arguments
#define NT_SYSNOARGSTRCALL      UINT8_C(0X16)   // system no-argument string function call
#define NT_SYSNOARGNUMCALL      UINT8_C(0X4A)   // system no-argument number function call
#define NT_OPERATOR             UINT8_C(0X4B)   // operator token
#define NT_NUMBASEVARREFLIST    UINT8_C(0X4C)   // numeric base variable reference list
#define NT_FNASSIGNSTMT         UINT8_C(0X4D)   // assignment of the result of a multi-line user function
#define NT_ENDSTMT              UINT8_C(0X4E)   // END statement
#define NT_POPSTMT              UINT8_C(0X4F)   // POP statement
#define NT_AGAINSTMT            UINT8_C(0X50)   // AGAIN statement
#define NT_REPEATSTMT           UINT8_C(0X51)   // REPEAT statement
#define NT_UNTILSTMT            UINT8_C(0X52)   // UNTIL statement
#define NT_WHILESTMT            UINT8_C(0X53)   // WHILE statement
#define NT_WENDSTMT             UINT8_C(0X54)   // WEND statement
#define NT_UENDSTMT             UINT8_C(0X55)   // UEND statement
#define NT_ONSTMT               UINT8_C(0X56)   // ON statement
#define NT_DATASTMT             UINT8_C(0X57)   // DATA statement
#define NT_READSTMT             UINT8_C(0X58)   // READ statement
#define NT_RESTORESTMT          UINT8_C(0X59)   // RESTORE statement
#define NT_INPUTSTMT            UINT8_C(0X5A)   // INPUT statement
#define NT_GETSTMT              UINT8_C(0X5B)   // GET statement
#define NT_LOADSTMT             UINT8_C(0X5C)   // LOAD statement
#define NT_RUNSTMT              UINT8_C(0X5D)   // RUN statement
#define NT_LISTSTMT             UINT8_C(0X5E)   // LIST statement
#define NT_NEWSTMT              UINT8_C(0X5F)   // NEW statement
#define NT_CLEARSTMT            UINT8_C(0X60)   // CLEAR statement
#define NT_DELETESTMT           UINT8_C(0X61)   // DELETE statement
#define NT_RENUMSTMT            UINT8_C(0X62)   // RENUM statement
#define NT_EDITSTMT             UINT8_C(0X63)   // EDIT statement
#define NT_MONITORSTMT          UINT8_C(0X64)   // MONITOR statement
#define NT_SOUNDSTMT            UINT8_C(0X65)   // SOUND, PLAY, WAVE, TEMPO statements (data: 1 byte of SNDK_*)

#define SNDK_SOUND              UINT8_C(1)      // SOUND ch, freq, beats [, vol [, endfreq]]: 3 to 5 numeric branches
#define SNDK_PLAY               UINT8_C(2)      // PLAY a$ [, b$ ...]: 1..8 string branches
#define SNDK_WAVE               UINT8_C(3)      // WAVE ch, wave: 2 numeric branches
#define SNDK_TEMPO              UINT8_C(4)      // TEMPO bpm: 1 numeric branch
#define SNDK_OFF                UINT8_C(5)      // SOUND OFF [ch]: 0 or 1 numeric branch
#define SNDK_SYNC               UINT8_C(6)      // SOUND SYNC ch [, ch ...]: 1..8 numeric branches
#define SNDK_PITCH              UINT8_C(7)      // PITCH ch, freq: 2 numeric branches
#define SNDK_VOLUME             UINT8_C(8)      // VOLUME ch, vol: 2 numeric branches
#define SNDK_ENVELOPE           UINT8_C(9)      // ENVELOPE ch [, attack, decay, sustain, release]: 1 or 5 numeric branches
#define SNDK_VIBRATO            UINT8_C(10)     // VIBRATO ch, rate, depth: 3 numeric branches
#define SNDK_TREMOLO            UINT8_C(11)     // TREMOLO ch, rate, depth: 3 numeric branches
#define SNDK_SFXDEF             UINT8_C(12)     // SFX n, a$: a numeric and a string branch
#define SNDK_SFXPLAY            UINT8_C(13)     // SFX n [, ch]: 1 or 2 numeric branches
#define SND_MAXVOICES           8               // PLAY strings / SYNC channels per statement (= SYNTH_NCHAN)
#define SND_MAXSFX              64              // stored effects (SFX 1..64)

#define LISTF_FIRST             UINT8_C(0X01)   // LIST: start target present (first branch)
#define LISTF_LAST              UINT8_C(0X02)   // LIST: end target present (next branch)
#define LISTF_RANGE             UINT8_C(0X04)   // LIST: a minus sign was given (otherwise a single target)
#define LISTF_STEP              UINT8_C(0X08)   // RENUM: a step (numeric expression) is the last branch

#define INPUTF_CHAN             UINT8_C(0X01)   // INPUT / GET: channel info present
#define INPUTF_PROMPT           UINT8_C(0X02)   // INPUT: prompt present
#define INPUTF_NOQUEST          UINT8_C(0X04)   // INPUT: no "? " after the prompt
#define NT_NUMEXPR              UINT8_C(0X18)   // numeric expression
#define NT_STREXPR              UINT8_C(0X1A)   // string expression
#define NT_STRCMPEX             UINT8_C(0X1C)   // string comparison expression
#define NT_ZZZZZZZZ4            UINT8_C(0X1E)   //
#define NT_ZZZZZZZZ5            UINT8_C(0X20)   //
#define NT_ZZZZZZZZ6            UINT8_C(0X22)   //
#define NT_ZZZZZZZZ7            UINT8_C(0X24)   //

const char* nodename( uint8_t nodetype );

#endif
