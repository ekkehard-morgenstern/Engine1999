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

#ifndef BASPGM_H
#define BASPGM_H    1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

#ifndef BASLIN_H
#include "baslin.h"
#endif

#define MAX_PROGRAMSIZE 65536

#define LINELIST_POS    UINT16_C(0)

typedef struct _program_t {
    uint8_t     memory[MAX_PROGRAMSIZE];
    uint16_t    fillpos;
} program_t;

typedef struct _pgmiter_t {
    program_t*  pgm;
    uint8_t*    tok;
    uint16_t    pos;
    linehdr_t   hdr;
} pgmiter_t;

// -- program management ----------------------------------------------------

void init_program( program_t* pgm );
void clear_iter( pgmiter_t* iter, program_t* pgm );
bool iter_load_line( pgmiter_t* iter, uint16_t lineoffs );
bool begin_iterate_program( pgmiter_t* iter, program_t* pgm );
bool step_iterate_program( pgmiter_t* iter );
bool get_prev_next_linenos( const pgmiter_t* iter, uint16_t* pprevlineno, uint16_t* pnextlineno );

#define FOUND_ERROR  -1
#define FOUND_NONE   0
#define FOUND_EXACT  1
#define FOUND_INSERT 2
#define FOUND_BEYOND 3
short find_line( program_t* pgm, uint16_t lineno, pgmiter_t* piter, uint16_t* pprevno, uint16_t* pnextno );

bool zero_line( pgmiter_t* iter );
bool unlink_line( pgmiter_t* iter );
bool quick_append_line( program_t* pgm, const linehdr_t* newhdr, const uint8_t* newtokens );
bool defrag_memory( program_t* pgm );
bool create_line( program_t* pgm, linehdr_t* newhdr, const uint8_t* newtokens, uint16_t* poffs );
bool emplace_line( program_t* pgm, uint16_t prevno, pgmiter_t* curr, uint16_t nextno );
bool update_line( pgmiter_t* iter, const linehdr_t* newhdr, const uint8_t* newtokens, uint16_t prevno, uint16_t nextno );
bool enter_line( program_t* pgm, const uint8_t* tokline );
void list_program( program_t* pgm, uint16_t lineno_first, uint16_t lineno_last );

#endif