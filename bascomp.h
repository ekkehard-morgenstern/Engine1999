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

struct _compiler_t;

typedef void (*cpcallbk_t)( struct _compiler_t*, void* );

typedef struct _cpprintparam_t {
    void*       userdata;
    const char* text;
} cpprintparam_t;

typedef struct _cpcalltbl_t {
    void*       userdata;
    cpcallbk_t  yield;  // called once after every instruction
    cpcallbk_t  print;
} cpcalltbl_t;

typedef struct _cpcodeins_t {
    cpcallbk_t  callback;
    union {
        double  number;
        char*   string;
    };
} cpcodeins_t;

#define CODEBUF_INS     1024U

typedef struct _cpcodebuf_t {
    struct _cpcodebuf_t*    next;
    cpcodeins_t             code[CODEBUF_INS];
    size_t                  fill;
} cpcodebuf_t;

typedef struct _compiler_t {
    pgmiter_t       iter;
    uint8_t*        tokp;
    uint8_t         currtok;
    union {
        char        param[256];
        double      number;
    };
    jmp_buf         ready_jump, exit_jump;
    cpcalltbl_t     calltable;
    cpcodebuf_t*    codelist;
    cpcodebuf_t*    current;
} compiler_t;

void init_compiler( compiler_t* comp, program_t* pgm );
bool comp_nextline( compiler_t* comp );
bool comp_fetchtok( compiler_t* comp );
bool begin_comp( compiler_t* comp );
void run_compiler( compiler_t* comp );

#endif
