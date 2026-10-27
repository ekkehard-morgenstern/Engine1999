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

#ifndef BASLIN_H
#define BASLIN_H    1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

#define LINENO_MIN      UINT32_C(0)         // minimum user-supplied line number
#define LINENO_MAX      UINT32_C(999999)    // maximum user-supplied line number
#define LINENO_DEL      UINT32_C(16777215)  // deleted line or no line number (2^24-1, invalid)
#define LINENO_NONE     UINT32_C(16777215)

#define LINEHDR_SIZE    5U                  // encoded size of linehdr_t (lineno takes 3 bytes)

// -- linehdr_t -------------------------------------------------------------

// header of a tokenized line, as produced by tokenize_line() and consumed by enter_line()
typedef struct _linehdr_t {
    uint32_t    lineno;     // 24 bits when encoded
    uint16_t    length;     // total encoded bytes incl. header
    // followed by tokenized line data
} linehdr_t;

void clear_linehdr( linehdr_t* hdr );
void emit_linehdr_raw( uint8_t** pp, const linehdr_t* source );     // big endian, LINEHDR_SIZE bytes
void read_linehdr_raw( const uint8_t** pp, linehdr_t* target );

// -- line_t ----------------------------------------------------------------

// a stored program line; lines form a doubly linked list sorted by line number
typedef struct _line_t {
    struct _line_t* next;
    struct _line_t* prev;
    uint32_t        lineno;
    uint16_t        toklen;     // number of token bytes (incl. TOK_EOL)
    uint8_t         tokens[];   // tokenized line data
} line_t;

#endif
