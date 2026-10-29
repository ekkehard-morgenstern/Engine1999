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
#ifndef VMASM_H
#define VMASM_H     1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

/*
Assembler and instruction decoder for the instruction set of the VM (instset.h), used by the machine monitor. No
screen or REPL dependency.

Assembler syntax: one instruction per line, "MNEMONIC[.C|.D|.W] [operand]"; case doesn't matter, ';' starts a comment.
    - the operand is a number: decimal (-5, 17), hexadecimal ($1F40 or 0x1F40); it is the parameter field (a data or code
      offset, a variable number, a line number, a count) or, for PHIM and BRIR, a signed value
    - .C / .D set or clear the C flag, which selects the code or the data stack / segment where an instruction has both
      (PHPA, DROP, LAN, LIAN, LUAN, LAS); JPCC, JUMP, RRLV and WRLV are always code (C is set for you)
    - .W forces the wide form (32 bit parameter field); it is chosen by itself for values above 65535 and, like the
      code generator does, for code addresses (BRIA, BRIR, PHPA.C, RSTR with an operand); PHIM takes the short form for
      -65536..65535 (the C bit is the 17th bit)
    - PHPA, PHIM, BRIA, BRIR and LINE need an operand; other instructions take one optionally (it makes the P flag
      set), the VM complains if it doesn't fit
*/

#define VMASM_MAXLEN    6U      // the longest instruction: instruction byte, extension byte, 32 bit parameter

typedef struct _vminsn_t {
    uint32_t    offs;           // where the instruction starts
    uint32_t    size;           // length in bytes
    uint16_t    opcode;         // 4 bit (basic) or 12 bit (extended) instruction number
    bool        valid;          // false: the area ends inside the instruction (size is then what is left)
    bool        code;           // flag C
    bool        wide;           // flag W
    bool        hasparam;       // flag P
    uint32_t    param;          // the parameter field as it is
    int32_t     value;          // the parameter as the VM reads it: PHIM / BRIA with the C bit as 17th bit, sign extended for PHIM and BRIR
    const char* name;           // mnemonic, "???" if unknown
} vminsn_t;

// decodes the instruction at offs of area[0..size); false if offs >= size
bool vmasm_decode( const uint8_t* area, uint32_t size, uint32_t offs, vminsn_t* out );

// the parts of the instruction text: mnemonic with suffix ("PHPA.C"), operand ("$0010", "-5", "" if none) and a comment
// (the target of a relative branch, "" if none); the strings are cut off to their size
void vmasm_format( const vminsn_t* insn, char* mnem, size_t mnemsz, char* oper, size_t opersz, char* comment, size_t commentsz );

// assembles one line; the bytes go to out, their number to *plen. false: err says why (also for an empty line: not an instruction)
bool vmasm_assemble( const char* line, uint8_t out[VMASM_MAXLEN], uint32_t* plen, char err[80] );

// parses a number as the assembler does (decimal, $hex, 0xhex, optional sign); false if text isn't one
bool vmasm_parse_number( const char* text, int64_t* pvalue );

#endif
