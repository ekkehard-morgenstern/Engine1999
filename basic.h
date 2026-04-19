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

#ifndef BASIC_H
#define BASIC_H 1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

/*

   The interpreter keeps only the tokenized form in special memory. Before running
   a program, it may produce a transformed version to avoid repeated parsing of the
   same text.

   Rationale behind the encoding of various literals:
    - Every literal should have a length byte, so it is easy to read back in.
    - Numbers should be encoded as strings, so when you save a program, it is saved
      in an architecture independent format (i.e. regardless of the numeric precision
      of the host interpreter). Thus, a number conversion error is a runtime error
      (which might be reported before actually running the program, see above).
    - Numerals 0..9 when appearing alone are encoded as separate tokens to speed up
      reading of one-digit numbers.

*/

#define TOK_EOL         UINT8_C(0)          // end of line
#define TOK_IDENT       UINT8_C(1)          // identifier, <len> <chr> ...
#define TOK_BINLIT      UINT8_C(2)          // binary literal, <len> <digchr> ...
#define TOK_STRLIT      UINT8_C(3)          // string literal, <len> <chr> ...
#define TOK_QUALIT      UINT8_C(4)          // quaternary literal, <len> <digchr> ...
#define TOK_SHLLIT      UINT8_C(5)          // shell literal, <len> <chr> ...
#define TOK_QUOLIT      UINT8_C(6)          // quote literal, <len> <chr> ...
#define TOK_BRKLIT      UINT8_C(7)          // bracket literal, <len> <chr> ...
#define TOK_OCTLIT      UINT8_C(8)          // octal literal, <len> <digchr> ...
#define TOK_BRCLIT      UINT8_C(9)          // brace literal <len> <chr> ...
#define TOK_DECLIT      UINT8_C(10)         // decimal literal, <len> <digchr> ...
#define TOK_HEXLIT      UINT8_C(16)         // hexadecimal literal, <len> <digchr> ...
#define TOK_SPACE       UINT8_C(32)         // space
#define TOK_PLING       UINT8_C(33)         // ! pling
#define TOK_LATTICE     UINT8_C(35)         // # lattice
#define TOK_STRING      UINT8_C(36)         // $ string type sigil
#define TOK_INTEGER     UINT8_C(37)         // % integer type sigil
#define TOK_AMP         UINT8_C(38)         // & ampersand
#define TOK_QUOTE       UINT8_C(39)         // ' quote (comment)
#define TOK_LPAREN      UINT8_C(40)         // ( left parenthesis
#define TOK_RPAREN      UINT8_C(41)         // ) right parenthesis
#define TOK_MULT        UINT8_C(42)         // * operator
#define TOK_PLUS        UINT8_C(43)         // + operator
#define TOK_COMMA       UINT8_C(44)         // , comma
#define TOK_MINUS       UINT8_C(45)         // - operator
#define TOK_DIV         UINT8_C(47)         // / operator
#define TOK_DEC0        UINT8_C(48)         // decimal 0
#define TOK_DEC1        UINT8_C(49)         // decimal 1
#define TOK_DEC2        UINT8_C(50)         // decimal 2
#define TOK_DEC3        UINT8_C(51)         // decimal 3
#define TOK_DEC4        UINT8_C(52)         // decimal 4
#define TOK_DEC5        UINT8_C(53)         // decimal 5
#define TOK_DEC6        UINT8_C(54)         // decimal 6
#define TOK_DEC7        UINT8_C(55)         // decimal 7
#define TOK_DEC8        UINT8_C(56)         // decimal 8
#define TOK_DEC9        UINT8_C(57)         // decimal 9
#define TOK_COLON       UINT8_C(58)         // : colon
#define TOK_SEMIC       UINT8_C(59)         // ; semicolon
#define TOK_LT          UINT8_C(60)         // < operator
#define TOK_EQ          UINT8_C(61)         // = operator
#define TOK_GT          UINT8_C(62)         // > operator
#define TOK_LBRACK      UINT8_C(91)         // [ left bracket
#define TOK_BACKSL      UINT8_C(92)         // \ operator (integer division)
#define TOK_RBRACK      UINT8_C(93)         // ] right bracket
#define TOK_POW         UINT8_C(94)         // ^ operator (**)
#define TOK_BACKTK      UINT8_C(96)         // ` back tick (shell operator)
#define TOK_LBRACE      UINT8_C(123)        // { left brace
#define TOK_COLUMN      UINT8_C(124)        // | column
#define TOK_RBRACE      UINT8_C(125)        // } right brace
#define TOK_TILDE       UINT8_C(126)        // ~ tilde

#define LINENO_MIN      UINT16_C(0)         // minimum user-supplied line number
#define LINENO_MAX      UINT16_C(65534)     // maximum user-supplied line number
#define LINENO_DEL      UINT16_C(65535)     // deleted line

#pragma pack(2)
typedef struct _linehdr_t {
    // all values in network byte order (big endian)
    uint16_t    nextoffs;   // from beginning of memory
    uint16_t    prevoffs;   // from beginning of memory
    uint16_t    lineno;
    uint16_t    length;
    // followed by tokenized line data
} linehdr_t;
#pragma pack()

typedef struct _program_t {
    uint8_t     memory[65536];
} program_t;

bool tokenize_line( const char* buf, uint8_t* whereto, size_t* premain, linehdr_t* phdr );
void preprocess_buffer( char* buf );

#endif
