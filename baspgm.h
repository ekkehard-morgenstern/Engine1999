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

// the program is a doubly linked list of heap-allocated lines, sorted by line number
typedef struct _program_t {
    line_t*     first;
    line_t*     last;
} program_t;

typedef struct _pgmiter_t {
    program_t*  pgm;
    line_t*     line;       // current line, 0 if none
    uint8_t*    tok;        // tokens of current line
    linehdr_t   hdr;        // header describing current line (lineno, length)
} pgmiter_t;

// -- program management ----------------------------------------------------

void init_program( program_t* pgm );
void free_program( program_t* pgm );
void clear_iter( pgmiter_t* iter, program_t* pgm );
bool iter_load_line( pgmiter_t* iter, line_t* line );
size_t iter_remaining( const pgmiter_t* iter, const uint8_t* tokp );   // token bytes left in the loaded line
bool begin_iterate_program( pgmiter_t* iter, program_t* pgm );
bool step_iterate_program( pgmiter_t* iter );

// finds the line with the given number; returns true if it exists (*pline), else *pline is 0.
// *pprev receives the last line with a smaller number (0 if none).
bool find_line( program_t* pgm, uint32_t lineno, line_t** pline, line_t** pprev );

line_t* create_line( uint32_t lineno, const uint8_t* tokens, uint16_t toklen );
void link_line( program_t* pgm, line_t* prev, line_t* line );   // inserts after prev (0 = at head)
void unlink_line( program_t* pgm, line_t* line );
void delete_line( program_t* pgm, line_t* line );               // unlinks and frees
bool enter_line( program_t* pgm, const uint8_t* tokline, bool (*directmode)( program_t*, const uint8_t*, size_t ) );
// binary program file: PGMFILE_HEADER_SIZE header bytes (magic 0x89 'E' '9' '9' + format version), then the encoded lines.
// The first magic byte is not ASCII, so a text listing can never be mistaken for a binary file.
#define PGMFILE_MAGIC_SIZE      4U
#define PGMFILE_HEADER_SIZE     5U
#define PGMFILE_VERSION         1U
// writes the program to a stream, either as text (one line per program line, as listed) or in binary form
// (the header, then the encoded lines, each a linehdr_t of LINEHDR_SIZE bytes followed by its tokens)
bool save_program( program_t* pgm, FILE* out, bool ascii );
// checks for the binary header at the current position of the stream and consumes it if present;
// returns false if the magic is missing (then the stream is rewound to where it was, so a text loader can read it)
// or the version is unknown; *pversion receives the version if the magic was found (0 otherwise)
bool read_program_header( FILE* in, uint8_t* pversion );
// finds the line that holds "LABEL <label>" (the first one), by scanning the program text; false if there is none
bool find_label_line( program_t* pgm, const char* label, uint32_t* plineno );
void list_program( program_t* pgm, uint32_t lineno_first, uint32_t lineno_last, void (*printer)( const char* ) );

#endif
