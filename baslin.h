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

#define LINENO_MIN      UINT16_C(0)         // minimum user-supplied line number
#define LINENO_MAX      UINT16_C(65534)     // maximum user-supplied line number
#define LINENO_DEL      UINT16_C(65535)     // deleted line or no line number
#define LINENO_NONE     UINT16_C(65535)

#define LINEOFFS_NONE   UINT16_C(65535)

#pragma pack(2)
typedef struct _linenode_t {
    uint16_t    nextoffs;   // from beginning of memory
    uint16_t    prevoffs;   // from beginning of memory
} linenode_t;
typedef struct _linelist_t {
    linenode_t  head;
    linenode_t  tail;
} linelist_t;
typedef struct _linehdr_t {
    // all values in network byte order (big endian)
    linenode_t  node;
    uint16_t    lineno;
    uint16_t    length;
    uint16_t    alloc;
    // followed by tokenized line data
} linehdr_t;
#pragma pack()

#define LINENODE_INIT { LINEOFFS_NONE, LINEOFFS_NONE }
#define LINELIST_INIT { LINENODE_INIT, LINENODE_INIT }

// -- linenode_t ------------------------------------------------------------

struct _program_t;

void init_linenode( linenode_t* node );
void emit_linenode_raw( uint8_t** pp, const linenode_t* source );
bool emit_linenode( struct _program_t* pgm, uint16_t offs, const linenode_t* source );
void read_linenode_raw( const uint8_t** pp, linenode_t* target );
bool read_linenode( struct _program_t* pgm, uint16_t offs, linenode_t* target );
bool remove_linenode( struct _program_t* pgm, uint16_t pos, linenode_t* outnode );
bool emplace_linenode( struct _program_t* pgm, uint16_t prevpos, uint16_t pos, uint16_t nextpos, linenode_t* outnode );

// -- linelist_t ------------------------------------------------------------

void init_linelist( linelist_t* list, uint16_t pos );
bool emit_linelist( struct _program_t* pgm, uint16_t offs, const linelist_t* source );
bool read_linelist( struct _program_t* pgm, uint16_t offs, linelist_t* target );
bool linelist_empty( struct _program_t* pgm, uint16_t offs, bool* pempty );
bool linelist_firstnode( struct _program_t* pgm, uint16_t listpos, uint16_t* pnodepos );
bool linelist_lastnode( struct _program_t* pgm, uint16_t listpos, uint16_t* pnodepos, bool wanthead );
bool linelist_nextnode( struct _program_t* pgm, uint16_t nodepos, uint16_t* pnextpos, linenode_t* outnode );
bool linelist_prevnode( struct _program_t* pgm, uint16_t nodepos, uint16_t* pprevpos, linenode_t* outnode );
bool linelist_addtail( struct _program_t* pgm, uint16_t listoffs, uint16_t nodeoffs, linenode_t* outnode );

// -- linehdr_t -------------------------------------------------------------

void clear_linehdr( linehdr_t* hdr );
void emit_linehdr_raw( uint8_t** pp, const linehdr_t* source );
bool emit_linehdr( struct _program_t* pgm, uint16_t pos, const linehdr_t* source, uint8_t** outp );
void read_linehdr_raw( const uint8_t** pp, linehdr_t* target );
bool read_linehdr( struct _program_t* pgm, uint16_t pos, linehdr_t* target, const uint8_t** outp );

#endif
