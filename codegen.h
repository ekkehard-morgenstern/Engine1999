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

#ifndef CODEGEN_H
#define CODEGEN_H   1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

#define CODEOFFS_NONE   UINT16_C(0XFFFF)

/*
The runtime system has two stacks:

    - a data stack for holding parameters and return values
    - a code stack for holding return addresses

An instruction removes its parameters from the stack.
We're using FORTH notation here to indicate stack usage.
For instance, ( n1 n2 -- n ) means "parameters are n1 and n2, in that order" and
"n" is the return value.
*/

#define CODESIZE_MAX    65536U

typedef struct _codegen_t {
    void*           userdata;
    void            (*report)( struct _codegen_t*, void*, const char* );
    void            (*halt)( struct _codegen_t*, void* ) ATTR_NORETURN;
    uint8_t         code[CODESIZE_MAX];
    uint32_t        codesize;
} codegen_t;

void init_codegen( codegen_t* cgen );

bool cgen_gen_brk( codegen_t* cgen );
bool cgen_gen_nop( codegen_t* cgen );
bool cgen_gen_phpa_c( codegen_t* cgen, uint16_t offs );
bool cgen_gen_phpa_d( codegen_t* cgen, uint16_t offs );
bool cgen_gen_phim( codegen_t* cgen, int32_t imm );
bool cgen_gen_bria( codegen_t* cgen, int32_t abs_offs );
bool cgen_gen_brir( codegen_t* cgen, int32_t rel_offs );
bool cgen_gen_jpcc( codegen_t* cgen );
bool cgen_gen_jump( codegen_t* cgen );
bool cgen_gen_drop( codegen_t* cgen, uint16_t cnt );
bool cgen_gen_line( codegen_t* cgen, uint16_t line );
bool cgen_gen_exp_ins( codegen_t* cgen, uint16_t ins );

#endif