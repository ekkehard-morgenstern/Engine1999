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

#ifndef BASCOMP_H
#define BASCOMP_H   1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

#ifndef BASTOK_H
#include "bastok.h"
#endif

#ifndef BASLIN_H
#include "baslin.h"
#endif

#ifndef BASPGM_H
#include "baspgm.h"
#endif

/*
See bascomp.ebnf for syntax definition.
*/

#define NODEOFFS_NONE       UINT16_C(0XFFFF)
#define NODEHDR_SIZE        UINT8_C(8)
#define BRANCHENT_SIZE      UINT8_C(4)

/*
The syntax tree is temporary for the compiler run and organized as follows:

    <nodetype.8> <numbranches.8> <datalen.16> <firstbranch.16> <lastbranch.16> <data...>

For every branch entry:

    <nodepos.16> <nextbranch.16>

16-bit values are stored in network byte order (big endian).

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

    NT_NUMUSRFNCALL     numeric user function call
    NT_STRUSRFNCALL     string user function call
        data:
            - 1 byte of type indicator
            - 2 bytes of variable offset
        branches:
            - argument expression list
        immediate processing:
            - looks up user-function variable and returns its offset
            - it's an error if it doesn't already exist
            - the expression list provided must match the parameters specified
              in the declaration
            - the variable must contain a code offset for calling

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

/*
The runtime system has two stacks:

    - a data stack for holding parameters and return values
    - a code stack for holding return addresses

An instruction removes its parameters from the stack.
We're using FORTH notation here to indicate stack usage.
For instance, ( n1 n2 -- n ) means "parameters are n1 and n2, in that order" and
"n" is the return value.

Instructions are encoded as follows:

    <ins> [ <exp> ] [ <hi> <lo> ]

First, an instruction byte.
Followed by an optional 8-bit instruction extension.
Followed by an optional 16-bit parameter field in network byte order (big endian).

If the instruction extension is requested, an additional byte follows, expanding
the instruction field from 4 bit to 12 bits, with the 4 bits from the instruction byte
being the most significant, and the 8 bits from the extension byte being the least
significant bits.

if the parameter is given, it specifies an offset either into the data segment, or
the code segment, depending on a flag in the instruction code.

the instruction code is organized as follows:

    +---+---+---+---++---+---+---+---+
    | P | C | E | r || I | I | I | I |
    +---+---+---+---++---+---+---+---+

    P - parameter flag (parameter field is present)
    C - code flag (code, not data offset)
    E - extended instruction
    r - reserved bits
    I - an instruction field

The 16 basic instructions are as follows:

    0000 - BRK          a breakpoint (bits C and P are ignored, and should be 0)
    0001 - NOP          no operation (bits C and P are ignored, and should be 0)
    0010 - PHPA         push parameter address onto stack
                        the C bit decides which stack (code or data)
                        the P bit must be set (and a parameter field supplied)
    0011 - PHIM         push 17 bit immediate value on stack (from parameter field)
                        the C bit becomes the uppermost (17th) bit.
                        the P bit must be set (and a parameter field supplied)
                        first, the value is sign-extended to 32 bits, and then
                        converted to a number before being pushed onto the stack.
                        this is to sacrifice code space over data space for small
                        integral numbers.
    0100 - BRIA          branch to immediate 17-bit address
                        the C bit becomes the uppermost (17th) bit.
                        the P bit must be set (and a parameter field supplied)
                        first, the value is sign-extended to 32 bits, and then
                        used as an index into code memory (when in range).
    0101 - BRIR         branch immediate relative, using 17-bit offset
                        the C bit becomes the uppermost (17th) bit.
                        the P bit must be set (and a parameter field supplied)
                        first, the value is sign-extended to 32 bits, and then
                        is added to the address of the following instruction.
                        if the result address is in range, a branch takes place.
    0110 - JPCC         quick conditional jump ( n -- ) R( a -- )
                        pulls one data item (numeric) and one return address item.
                        if the data item is nonzero, execution continues at the
                        specified code address.
                        the C bit must be set to 1, the P bit must be 0.
    0111 - JUMP         quick jump R( a -- )
                        pulls one return address and jumps to it.
                        the C bit must be set to 1, the P bit must be 0.
    1000 - DROP         drop stack item(s) ( n -- ) or R( a -- )
                        depending on C, a code or data item is dropped from the stack.
                        if a parameter field is also given, it specifies the number of
                        items to drop.
    1001 - LINE         set line number immediate
                        sets the current line number (must be the first instruction of a line)
                        C field must be 0, P must be set and a line number specified as parameter.

    1111 reserved

The 4096 extended instructions are as follows:

    0000 0000xxxx       same as basic instructions
    0000 00010000       NEG - numerical negation                    ( n -- n )      C=0 P=0
    0000 00010001       NOT - bitwise NOT                           ( n -- n )      C=0 P=0
    0000 00010010       LSH - left shift                            ( n1 n2 -- n )  C=0 P=0
    0000 00010011       RSH - right shift                           ( n1 n2 -- n )  C=0 P=0
    0000 00010100       ADD - numerical addition                    ( n1 n2 -- n )  C=0 P=0
    0000 00010101       SUB - numerical subtraction                 ( n1 n2 -- n )  C=0 P=0
    0000 00010110       MUL - numerical multiplication              ( n1 n2 -- n )  C=0 P=0
    0000 00010111       DIV - numerical division                    ( n1 n2 -- n )  C=0 P=0
    0000 00011000       AND - bitwise AND                           ( n1 n2 -- n )  C=0 P=0
    0000 00011001       NAND - bitwise NAND                         ( n1 n2 -- n )  C=0 P=0
    0000 00011010       OR  - bitwise OR                            ( n1 n2 -- n )  C=0 P=0
    0000 00011011       NOR - bitwise NOR                           ( n1 n2 -- n )  C=0 P=0
    0000 00011100       XOR - bitwise XOR                           ( n1 n2 -- n )  C=0 P=0
    0000 00011101       XNOR - bitwise XNOR                         ( n1 n2 -- n )  C=0 P=0
    0000 00011110       POW - power                                 ( n1 n2 -- n )  C=0 P=0
    0000 00011111       CON - concatenate strings                   ( s1 s2 -- s )  C=0 P=0
    0000 00100000       CNEQ - compare numerical equal              ( n1 n2 -- n )  C=0 P=0
    0000 00100001       CNNE - compare numerical not equal          ( n1 n2 -- n )  C=0 P=0
    0000 00100010       CNGE - compare numerical greater or equal   ( n1 n2 -- n )  C=0 P=0
    0000 00100011       CNLE - compare numerical less or equal      ( n1 n2 -- n )  C=0 P=0
    0000 00100100       CNGT - compare numerical greater than       ( n1 n2 -- n )  C=0 P=0
    0000 00100101       CNLT - compare numerical less than          ( n1 n2 -- n )  C=0 P=0
    0000 00100110       CSEQ - compare string equal                 ( s1 s2 -- n )  C=0 P=0
    0000 00100111       CSNE - compare string not equal             ( s1 s2 -- n )  C=0 P=0
    0000 00101000       CSGE - compare string greater or equal      ( s1 s2 -- n )  C=0 P=0
    0000 00101001       CSLE - compare string less or equal         ( s1 s2 -- n )  C=0 P=0
    0000 00101010       CSGT - compare string greater than          ( s1 s2 -- n )  C=0 P=0
    0000 00101011       CSLT - compare string less than             ( s1 s2 -- n )  C=0 P=0
        .
        .
        .

    0001 00000000       RRNV - read regular numeric variable (param=varoffs) ( -- n ) C=0 P=1
    0001 00000001       RRIV - read regular integer variable (param=varoffs) ( -- n ) C=0 P=1
    0001 00000010       RRSV - read regular string  variable (param=varoffs) ( -- s ) C=0 P=1
    0001 00000011       (reserved)
    0001 00000100       RNAE - read numeric array element (param=varoffs) ( inx ... ndim -- n ) C=0 P=1
    0001 00000101       RIAE - read integer array element (param=varoffs) ( inx ... ndim -- n ) C=0 P=1
    0001 00000110       RSAE - read string  array element (param=varoffs) ( inx ... ndim -- s ) C=0 P=1
    0001 00000111       (reserved)

    0001 00001000       WRNV - write regular numeric variable (param=varoffs) ( n -- ) C=0 P=1
    0001 00001001       WRIV - write regular integer variable (param=varoffs) ( n -- ) C=0 P=1
    0001 00001010       WRSV - write regular string  variable (param=varoffs) ( s -- ) C=0 P=1
    0001 00001011       (reserved)
    0001 00001100       WNAE - write numeric array element (param=varoffs) ( inx ... ndim n -- ) C=0 P=1
    0001 00001101       WIAE - write integer array element (param=varoffs) ( inx ... ndim n -- ) C=0 P=1
    0001 00001110       WSAE - write string  array element (param=varoffs) ( inx ... ndim s -- ) C=0 P=1
    0001 00001111       (reserved)



*/

#define INS_NODATA      UINT16_C(0XFFFF)
#define INSF_P          UINT8_C(0X80)
#define INSF_C          UINT8_C(0X40)
#define MKINS_C(x)      ( ((x) & UINT8_C(1)) << UINT8_C(6) )
#define INSF_E          UINT8_C(0X20)
#define INSF__r         UINT8_C(0X10)
#define INSM_I          UINT8_C(0X0F)
#define MKINS_I(x)      ((x) & INSM_I)
#define INS_BRK         ( MKINS_I(0) )
#define INS_NOP         ( MKINS_I(1) )
#define INS_PHPA_C      ( MKINS_I(2) | INSF_C )
#define INS_PHPA_D      ( MKINS_I(2)          )
#define INS_PHIM        MKINS_I(3)
#define INS_BRIA        MKINS_I(4)
#define INS_BRIR        MKINS_I(5)
#define INS_JPCC        MKINS_I(6)
#define INS_JUMP        MKINS_I(7)
#define INS_DROP        MKINS_I(8)
#define INS_LINE        MKINS_I(9)

#define INS_NEG         UINT16_C(0X010)
#define INS_NOT         UINT16_C(0X011)
#define INS_LSH         UINT16_C(0X012)
#define INS_RSH         UINT16_C(0X013)
#define INS_ADD         UINT16_C(0X014)
#define INS_SUB         UINT16_C(0X015)
#define INS_MUL         UINT16_C(0X016)
#define INS_DIV         UINT16_C(0X017)
#define INS_AND         UINT16_C(0X018)
#define INS_NAND        UINT16_C(0X019)
#define INS_OR          UINT16_C(0X01A)
#define INS_NOR         UINT16_C(0X01B)
#define INS_XOR         UINT16_C(0X01C)
#define INS_XNOR        UINT16_C(0X01D)
#define INS_POW         UINT16_C(0X01E)
#define INS_CON         UINT16_C(0X01F)
#define INS_CNEQ        UINT16_C(0X020)
#define INS_CNNE        UINT16_C(0X021)
#define INS_CNGE        UINT16_C(0X022)
#define INS_CNLE        UINT16_C(0X023)
#define INS_CNGT        UINT16_C(0X024)
#define INS_CNLT        UINT16_C(0X025)
#define INS_CSEQ        UINT16_C(0X026)
#define INS_CSNE        UINT16_C(0X027)
#define INS_CSGE        UINT16_C(0X028)
#define INS_CSLE        UINT16_C(0X029)
#define INS_CSGT        UINT16_C(0X02A)
#define INS_CSLT        UINT16_C(0X02B)
#define INS_RRNV        UINT16_C(0X100)
#define INS_RRIV        UINT16_C(0X101)
#define INS_RRSV        UINT16_C(0X102)
#define INS_RNAV        UINT16_C(0X104)
#define INS_RIAV        UINT16_C(0X105)
#define INS_RSAV        UINT16_C(0X106)
#define INS_WRNV        UINT16_C(0X108)
#define INS_WRIV        UINT16_C(0X109)
#define INS_WRSV        UINT16_C(0X10A)
#define INS_WNAV        UINT16_C(0X10C)
#define INS_WIAV        UINT16_C(0X10D)
#define INS_WSAV        UINT16_C(0X10E)

#define TREESIZE_MAX    65535U
#define CODESIZE_MAX    65536U
#define DATASIZE_MAX    65536U

typedef struct _compiler_t {
    pgmiter_t       iter;
    uint8_t*        tokp;
    uint8_t         currtok;
    union {
        char        param[256];
        double      number;
    };
    jmp_buf         ready_jump, exit_jump;
    uint8_t         tree[TREESIZE_MAX];
    uint8_t         code[CODESIZE_MAX];
    uint8_t         data[DATASIZE_MAX];
    uint32_t        treesize;
    uint32_t        codesize;
    uint32_t        datasize;
} compiler_t;

void init_compiler( compiler_t* comp, program_t* pgm, bool keepmemory );

bool comp_alloc_tree( compiler_t* comp, uint16_t size, uint16_t* poffs );
bool comp_alloc_code( compiler_t* comp, uint16_t size, uint16_t* poffs );
bool comp_alloc_data( compiler_t* comp, uint16_t size, uint16_t* poffs );

bool comp_create_node( compiler_t* comp, uint16_t* pnodeoffs, uint8_t nodetype, uint8_t numbranches, uint16_t datalen,
    const void* pdata, ... );
bool comp_add_branch( compiler_t* comp, uint16_t nodeoffs, uint16_t branchoffs );

typedef bool (*comp_eatfn_t)( compiler_t*, uint16_t* );

bool comp_eat_list( compiler_t* comp, uint16_t* pnodeoffs, uint8_t nodetype, comp_eatfn_t element_eater, uint8_t septok );

bool comp_eat_numexlist( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_strexlist( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_exprlist( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_arraysub( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_arraydimdecl( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_arraydecl( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_arraydecllist( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_emptyarrayref( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_emptyarrayreflist( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numbasevarref( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numvarref( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_strbasevarref( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_strvarref( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_anybasevarref( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_declit( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numlit( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_strlit( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_strlits( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numusrfnname( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_strusrfnname( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numusrfncall( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_strusrfncall( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_sysnoargstrname( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_sysnoargstrcall( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numfunccall( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_strfunccall( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_strbaseexpr( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_straddexpr( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_strexpr( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numsubexpr( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numbaseexpr( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numunaryop( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numunaryex( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_nummultop( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_nummultex( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numaddop( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numaddex( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numshiftop( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numshiftex( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numcmpop( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numcmpex( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numandop( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numandex( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numorop( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numorex( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numexpr( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_expr( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_savestmt( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_chanspec( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_printsep( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_printarg( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_printarglist( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_printstmt( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_iostmt( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numassign( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_strassign( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_substrop( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_substrassign( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_anyassign( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_assignlist( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_letstmt( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_dimstmt( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_erasestmt( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_assignstmt( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_forstmt( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_nextstmt( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_gotokw( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_gototarget( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_gotostmt( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_returnstmt( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_labelstmt( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_singlelineifstmt( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_multilineifstmt( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_controlflowstmt( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_stmt( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_stmtlist( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_stmtline( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_stmtlines( compiler_t* comp, uint16_t* pnodeoffs );

bool comp_gen_brk( compiler_t* comp );
bool comp_gen_nop( compiler_t* comp );
bool comp_gen_phpa_c( compiler_t* comp, uint16_t offs );
bool comp_gen_phpa_d( compiler_t* comp, uint16_t offs );
bool comp_gen_phim( compiler_t* comp, int32_t imm );
bool comp_gen_bria( compiler_t* comp, int32_t abs_offs );
bool comp_gen_brir( compiler_t* comp, int32_t rel_offs );
bool comp_gen_jpcc( compiler_t* comp );
bool comp_gen_jump( compiler_t* comp );
bool comp_gen_drop( compiler_t* comp, uint16_t cnt );
bool comp_gen_line( compiler_t* comp, uint16_t line );
bool comp_gen_exp_ins( compiler_t* comp, uint16_t ins );

bool comp_nextline( compiler_t* comp );
bool comp_fetchtok( compiler_t* comp );
bool begin_comp( compiler_t* comp );
void run_compiler( compiler_t* comp );

#endif
