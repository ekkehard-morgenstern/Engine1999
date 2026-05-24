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
        branches:
            - 1 branch of name
            - 1 optional branch of argument list
            - 1 branch of expression or statement list

    NT_NUMUSRFNCALL     numeric user function call
    NT_STRUSRFNCALL     string user function call
        branches:
            - name
            - argument expression list (can be empty)

    NT_SYSNOARGSTRNAME  system no-argument string name
        data:
            - 1 byte of function token (like TOK_INKEY)

    [ NT_SYSNOARGSTR - not generated ]
    [ NT_SYSNOARGSTRCALL - not generated ]
    [ NT_NUMFUNCCALL - not generated ]
    [ NT_STRFUNCCALL - not generated ]
    [ NT_STRBASEXPR - not generated ]

    NT_STRADDEXPR   string addition expression
        branches:
            - at least 2 branches of string expressions
        immediate processing:
            - generated only if there is 2 or more nodes

    [ NT_STREXPR - not generated ]
    [ NT_NUMSUBEXPR - not generated ]
    [ NT_NUMBASEEXPR - not generated ]

    NT_NUMUNARYOP   numeric unary operator
        data:
            - 1 byte of operator token

    NT_NUMUNARYEX   numeric unary expression
        branches:
            - 1 branch of NT_NUMUNARYOP
            - 1 branch of expression
        immediate processing:
            - generated only if a unary operator was used and the result is not constant
            - if the result is a constant expression, return NT_NUMLIT instead

    NT_NUMMULTOP   numeric multiplication operator
        data:
            - 1 byte of operator token

    NT_NUMMULTEX   numeric multiplication expression
        branches:
            - 1 branch of NT_NUMMULTOP
            - at least 2 branches of expressions
        immediate processing:
            - generated only if a multiplication operator was used and the result is not constant
            - if the result is a constant expression, return NT_NUMLIT instead
            - if a division by zero would occur, generate error

    NT_NUMADDOP   numeric addition operator
        data:
            - 1 byte of operator token

    NT_NUMADDEX   numeric addition expression
        branches:
            - 1 branch of NT_NUMADDOP
            - at least 2 branches of expressions
        immediate processing:
            - generated only if an addition operator was used and the result is not constant
            - if the result is a constant expression, return NT_NUMLIT instead

    NT_NUMSHIFTOP   numeric shift operator
        data:
            - 1 byte of operator token

    NT_NUMSHIFTEX   numeric shift expression
        branches:
            - 1 branch of NT_NUMSHIFTOP
            - 2 branches of expressions
        immediate processing:
            - generated only if a shift operator was used and the result is not constant
            - if the result is a constant expression, return NT_NUMLIT instead

    NT_NUMCMPOP   numeric comparison operator
        data:
            - 1 byte of operator token

    NT_NUMCMPEX   numeric shift expression
        branches:
            - 1 branch of NT_NUMCMPOP
            - 2 branches of expressions
        immediate processing:
            - generated only if a comparison operator was used and the result is not constant
            - if the result is a constant expression, return NT_NUMLIT instead
            - if two string expressions are compared, the result will be numeric

    NT_NUMANDOP     numeric AND operator
        data:
            - 1 byte of operator token

    NT_NUMANDEX   numeric AND expression
        branches:
            - 1 branch of NT_NUMANDOP
            - at least 2 branches of expressions
        immediate processing:
            - generated only if an AND operator was used and the result is not constant
            - if the result is a constant expression, return NT_NUMLIT instead

    NT_NUMOROP     numeric OR operator
        data:
            - 1 byte of operator token

    NT_NUMOREX   numeric OR expression
        branches:
            - 1 branch of NT_NUMOROP
            - at least 2 branches of expressions
        immediate processing:
            - generated only if an OR operator was used and the result is not constant
            - if the result is a constant expression, return NT_NUMLIT instead

    [ NT_NUMEXPR - not generated ]
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

    NT_SUBSTROP     substring operator
        data:
            - 1 byte of substring operator

    NT_SUBSTRASSIGN     substring assignment
        branches:
            - 1 branch of substring operator
            - 1 branch of expression list

    [ NT_ANYASSIGN - not generated ]

    NT_ASSIGNLIST       assignment list
        branches:
            - 1 or more branches of assignment expressions

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
        data:
            - 2 bytes of numeric variable offset
            - 8 bytes of starting value
            - 8 bytes of ending value
            - optionally, 8 bytes of step value
        immediate processing:
            - stores base numeric variable offset in data
            - the expressions are evaluated and must be constant
            - their computed values are stored in the data field
            - NOTE there's no code block associated with the FOR statement
              the reason for that is the NEXT statement can be located anywhere
              and has a variable list associated with it. Thus, all context
              resolution has to happen at runtime through a loop context stack.

    NT_NEXTSTMT     NEXT statement
        data:
            - at least 2 bytes of numeric variable offset (can be multiple)

    NT_GOTOKW       GOTO/GOSUB keyword
        data:
            - 1 byte of keyword token (even if originally written apart)

    NT_GOTOTARGET   GOTO/GOSUB target
        data:
            - 2 bytes of variable offset (label!), or
            - 8 bytes of line number

    NT_GOTOSTMT     GOTO/GOSUB statement
        branches:
            - 1 branch of goto/gosub keyword
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
            - 1 optional byte of TOK_GOTO if THEN/ELSE are goto targets
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
            - 2 bytes of line number (not present in direct mode)
        branches:
            - 1 branch of statement list
            - this is the root node for direct mode

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
#define NT_SYSNOARGSTRNAME      UINT8_C(0X16)   // system no-argument string name
#define NT_STRADDEXPR           UINT8_C(0X17)   // string addition expression
#define NT_NUMUNARYOP           UINT8_C(0X18)   // numeric unary operator
#define NT_NUMUNARYEX           UINT8_C(0X19)   // numeric unary expression
#define NT_NUMMULTOP            UINT8_C(0X1A)   // numeric multiplication operator
#define NT_NUMMULTEX            UINT8_C(0X1B)   // numeric multiplication expression
#define NT_NUMADDOP             UINT8_C(0X1C)   // numeric addition operator
#define NT_NUMADDEX             UINT8_C(0X1D)   // numeric addition expression
#define NT_NUMSHIFTOP           UINT8_C(0X1E)   // numeric shift operator
#define NT_NUMSHIFTEX           UINT8_C(0X1F)   // numeric shift expression
#define NT_NUMCMPOP             UINT8_C(0X20)   // numeric comparison operator
#define NT_NUMCMPEX             UINT8_C(0X21)   // numeric shift expression
#define NT_NUMANDOP             UINT8_C(0X22)   // numeric AND operator
#define NT_NUMANDEX             UINT8_C(0X23)   // numeric AND expression
#define NT_NUMOROP              UINT8_C(0X24)   // numeric OR operator
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

#endif
