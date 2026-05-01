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
    uint8_t         code[CODESIZE_MAX];
    uint8_t         data[DATASIZE_MAX];
    uint32_t        codesize;
    uint32_t        datasize;
} compiler_t;

void init_compiler( compiler_t* comp, program_t* pgm );
bool comp_alloc_code( compiler_t* comp, uint16_t size, uint16_t* poffs );
bool comp_alloc_data( compiler_t* comp, uint16_t size, uint16_t* poffs );
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
bool comp_gen_exp_ins( compiler_t* comp, uint16_t ins );

bool comp_nextline( compiler_t* comp );
bool comp_fetchtok( compiler_t* comp );
bool begin_comp( compiler_t* comp );
void run_compiler( compiler_t* comp );

#endif
