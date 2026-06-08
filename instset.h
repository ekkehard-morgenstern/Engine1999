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

#ifndef INSTSET_H
#define INSTSET_H   1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

/*
The runtime system has two stacks:

    - a data stack for holding parameters and return values
      every item is a 64 bit floating-point number

    - a code stack for holding return addresses
      every item is a 16 bit unsigned address offset

An instruction removes its parameters from the stack.
We're using FORTH notation here to indicate stack usage.
For instance,
    ( n1 n2 -- n )  means "parameters are n1 and n2, in that order" and
                        "n" is the return value.
    R( a -- )       means an address offset is pulled from the return stack.

    n -- a 64-bit double (floating-point) value (data stack)
    a -- a 16-bit unsigned address offset (return stack only)
    i -- a 16-bit signed integer (not on stack)
    u -- a 16-bit unsigned integer (not on stack)
    s -- a 64-bit string pointer (data stack)

ATTN: String pointers point to heap memory and must be freed after use.
      Instructions with string arguments free their arguments automatically.

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
                        C=0 data stack, item is converted to floating-point
                        C=1 return stack, item is stored as an integer
                        the P bit must be set (and a parameter field supplied)

    0011 - PHIM         push 17 bit immediate value on stack (from parameter field)
                        the C bit becomes the uppermost (17th) bit.
                        the P bit must be set (and a parameter field supplied)
                        first, the value is sign-extended to 32 bits, and then
                        converted to a number before being pushed onto the stack.
                        this is to sacrifice code space over data space for small
                        integral numbers.

    0100 - BRIA         branch to immediate 17-bit address
                        the C bit becomes the uppermost (17th) bit.
                        the P bit must be set (and a parameter field supplied)
                        first, the value is zero-extended to 32 bits, and then
                        used as an index into code memory (when in range).
                        When out of range, a runtime exception occurs.

    0101 - BRIR         branch immediate relative, using 17-bit offset
                        the C bit becomes the uppermost (17th) bit.
                        the P bit must be set (and a parameter field supplied)
                        first, the value is sign-extended to 32 bits, and then
                        is added to the address of the following instruction.
                        if the result address is in range, a branch takes place.
                        When out of range, a runtime exception occurs.

    0110 - JPCC         quick conditional jump ( n -- ) R( a -- )
                        pulls one data item (numeric) and one return address item.
                        if the data item cast to an integer is nonzero, execution
                        continues at the specified code address.
                        the C bit must be set to 1, the P bit must be 0.
                        When out of range, a runtime exception occurs.

    0111 - JUMP         quick jump R( a -- )
                        pulls one return address and jumps to it.
                        the C bit must be set to 1, the P bit must be 0.
                        When out of range, a runtime exception occurs.

    1000 - DROP         drop stack item(s) ( n -- ) or R( a -- )
                        If C is 0, operates on the data stack.   (item size: 8 bytes)
                        If C is 1, operates on the return stack. (item size: 2 bytes)
                        if a parameter field is also given, it specifies the number of
                        items to drop.

    1001 - LINE         set line number immediate
                        sets the current line number (must be the first instruction of a line)
                        C field must be 0, P must be set and a line number specified as parameter.

    1010 - LAN          load as number ( n -- n )
                        loads value pointed to by code or data address on stack (flag C)
                        onto the stack
                        note that the address must be given as a floating-point number

    1011 - LIAN         load integer as number ( n -- n )
                        loads value pointed to by code or data address on stack (flag C)
                        onto the stack (after converting it to a number - signed)
                        note that the address must be given as a floating-point number

    1100 - LUAN         load unsigned as number ( n -- n )
                        loads value pointed to by code or data address on stack (flag C)
                        onto the stack (after converting it to a number - unsigned)
                        note that the address must be given as a floating-point number

    1101 - LAS          load as string ( n -- s )
                        loads value pointed to by code or data address on stack (flag C)
                        onto the stack (after converting it to a string pointer)
                        note that the address must be given as a floating-point number

    1110 - FRES         free string ( s -- )
                        frees string pointer from data stack (flag C must be 0)

    1111 - reserved

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
    0001 00000011       RRLV - read regular label   variable (param=varoffs) R( -- a ) C=1 P=1
    0001 00000100       RNAE - read numeric array element (param=varoffs) ( inx ... ndim -- n ) C=0 P=1
    0001 00000101       RIAE - read integer array element (param=varoffs) ( inx ... ndim -- n ) C=0 P=1
    0001 00000110       RSAE - read string  array element (param=varoffs) ( inx ... ndim -- s ) C=0 P=1
    0001 00000111       (reserved)

    0001 00001000       WRNV - write regular numeric variable (param=varoffs) ( n -- ) C=0 P=1
    0001 00001001       WRIV - write regular integer variable (param=varoffs) ( n -- ) C=0 P=1
    0001 00001010       WRSV - write regular string  variable (param=varoffs) ( s -- ) C=0 P=1
    0001 00001011       WRLV - write regular label   variable (param=varoffs) R( a -- ) C=1 P=1
    0001 00001100       WNAE - write numeric array element (param=varoffs) ( inx ... ndim n -- ) C=0 P=1
    0001 00001101       WIAE - write integer array element (param=varoffs) ( inx ... ndim n -- ) C=0 P=1
    0001 00001110       WSAE - write string  array element (param=varoffs) ( inx ... ndim s -- ) C=0 P=1
    0001 00001111       (reserved)

    0001 00010000       SHEX - shell execute ( s -- s ) C=0 P=0
    0001 00010001       (reserved)
    0001 00010010       CALF - call function (param=varoffs) ( ... args n -- ) R( -- retoffs context... ) C=0 P=1
                        Operation:
                            - the return address is put on the return stack
                            - the arguments are pulled from the stack and written into a temporary space
                            - the context for the call is created and pushed onto the return stack (pointer):
                                - contains the varoffs and content of each regular parameter variable before the call
                                  (saving array state isn't necessary)
                            - each regular parameter variable is set to one of the values from the temporary space
                            - code execution continues with the code of the function
    0001 00010011       RETF - return from function call ( result -- result ) R( retoffs context... -- ) C=0 P=0
                            - the context is pulled from the return stack, and variable contents are restored
                            - execution resumes with the instruction after the CALF instruction.
                            - the data stack is not affected (a result placed there will still exist afterwards)
    0001 00010100       GADC - get array    dimension count (param=varoffs) ( -- n ) C=0 P=1
    0001 00010101       GFAC - get function argument  count (param=varoffs) ( -- n ) C=0 P=1
    0001 00010110       (reserved)
    0001 00010111       (reserved)
    0001 00011000       TI   - get system   timer ( -- n ) C=0 P=0
    0001 00011001       INKY - get keyboard input ( -- s ) C=0 P=0
    0001 00011010       ASC  - ASCII code of first character        ( s -- n ) C=0 P=0
    0001 00011011       VAL  - numeric value of string              ( s -- n ) C=0 P=0
    0001 00011100       FRE  - return amount of free memory         ( n -- n ) C=0 P=0
    0001 00011101       LEN  - return length of string              ( s -- n ) C=0 P=0
    0001 00011110       (reserved)
    0001 00011111       (reserved)
    0001 00100000       BINS - convert number to binary      string ( n n -- s ) C=0 P=0
    0001 00100001       QUAS - convert number to quaternary  string ( n n -- s ) C=0 P=0
    0001 00100010       OCTS - convert number to octal       string ( n n -- s ) C=0 P=0
    0001 00100011       HEXS - convert number to hexadecimal string ( n n -- s ) C=0 P=0
    0001 00100100       DECS - convert number to decimal     string ( n n -- s ) C=0 P=0
    0001 00100101       STRS - convert number to             string ( n   -- s ) C=0 P=0
    0001 00100110       (reserved)
    0001 00100111       (reserved)
    0001 00101000       LFTS - get left part of string ( s n -- s ) C=0 P=0
    0001 00101001       RGTS - get right part of string ( s n -- s ) C=0 P=0
    0001 00101010       MIDS - get mid part of string ( s n -- s ) C=0 P=0
    0001 00101011       (reserved)
    0001 00101100       (reserved)
    0001 00101101       (reserved)
    0001 00101110       (reserved)
    0001 00101111       (reserved)
    0001 00110000       SAVE - save program ( s s -- ) C=0 P=0

*/

#define INS_NODATA      UINT16_C(0XFFFF)
#define INSF_P          UINT8_C(0X80)
#define INSF_C          UINT8_C(0X40)
#define MKINS_C(x)      ( ((x) & UINT8_C(1)) << UINT8_C(6) )
#define INSF_E          UINT8_C(0X20)
#define INSF__r         UINT8_C(0X10)
#define INSM_I          UINT8_C(0X0F)
#define MKINS_I(x)      ((x) & INSM_I)
#define INS_BRK         MKINS_I(0)
#define INS_NOP         MKINS_I(1)
#define INS_PHPA        MKINS_I(2)
#define INS_PHPA_C      ( INS_PHPA | INSF_C )
#define INS_PHPA_D      INS_PHPA
#define INS_PHIM        MKINS_I(3)
#define INS_BRIA        MKINS_I(4)
#define INS_BRIR        MKINS_I(5)
#define INS_JPCC        MKINS_I(6)
#define INS_JUMP        MKINS_I(7)
#define INS_DROP        MKINS_I(8)
#define INS_LINE        MKINS_I(9)
#define INS_LAN         MKINS_I(10)
#define INS_LIAN        MKINS_I(11)
#define INS_LUAN        MKINS_I(12)
#define INS_LAS         MKINS_I(13)
#define INS_FRES        MKINS_I(14)

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
#define INS_RRLV        UINT16_C(0X103)
#define INS_RNAE        UINT16_C(0X104)
#define INS_RIAE        UINT16_C(0X105)
#define INS_RSAE        UINT16_C(0X106)
#define INS_WRNV        UINT16_C(0X108)
#define INS_WRIV        UINT16_C(0X109)
#define INS_WRSV        UINT16_C(0X10A)
#define INS_WRLV        UINT16_C(0X10B)
#define INS_WNAE        UINT16_C(0X10C)
#define INS_WIAE        UINT16_C(0X10D)
#define INS_WSAE        UINT16_C(0X10E)
#define INS_SHEX        UINT16_C(0X110)
#define INS_CALF        UINT16_C(0X112)
#define INS_RETF        UINT16_C(0X113)
#define INS_GADC        UINT16_C(0X114)
#define INS_GFAC        UINT16_C(0X115)
#define INS_TI          UINT16_C(0X118)
#define INS_INKY        UINT16_C(0X119)
#define INS_ASC         UINT16_C(0X11A)
#define INS_VAL         UINT16_C(0X11B)
#define INS_FRE         UINT16_C(0X11C)
#define INS_LEN         UINT16_C(0X11D)
#define INS_BINS        UINT16_C(0X120)
#define INS_QUAS        UINT16_C(0X121)
#define INS_OCTS        UINT16_C(0X122)
#define INS_HEXS        UINT16_C(0X123)
#define INS_DECS        UINT16_C(0X124)
#define INS_STRS        UINT16_C(0X125)
#define INS_LFTS        UINT16_C(0X128)
#define INS_RGTS        UINT16_C(0X129)
#define INS_MIDS        UINT16_C(0X12A)
#define INS_SAVE        UINT16_C(0X130)

#endif
