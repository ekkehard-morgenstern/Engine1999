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
#define TOK_PRINT       UINT8_C(11)         // PRINT keyword
#define TOK_INPUT       UINT8_C(12)         // INPUT keyword
#define TOK_PUT         UINT8_C(13)         // PUT keyword
#define TOK_GET         UINT8_C(14)         // GET keyword
#define TOK_LIST        UINT8_C(15)         // LIST keyword
#define TOK_HEXLIT      UINT8_C(16)         // hexadecimal literal, <len> <digchr> ...
#define TOK_READ        UINT8_C(17)         // READ keyword
#define TOK_DATA        UINT8_C(18)         // DATA keyword
#define TOK_RESTORE     UINT8_C(19)         // RESTORE keyword
#define TOK_SAVE        UINT8_C(20)         // SAVE keyword
#define TOK_RUN         UINT8_C(21)         // RUN keyword
#define TOK_AUTO        UINT8_C(22)         // AUTO keyword
#define TOK_RENUM       UINT8_C(23)         // RENUM keyword
#define TOK_DELETE      UINT8_C(24)         // DELETE keyword
#define TOK_MERGE       UINT8_C(25)         // MERGE keyword
#define TOK_CHAIN       UINT8_C(26)         // CHAIN keyword
#define TOK_FILES       UINT8_C(27)         // FILES keyword
#define TOK_NEW         UINT8_C(28)         // NEW keyword
#define TOK_CLEAR       UINT8_C(29)         // CLEAR keyword
#define TOK_ERASE       UINT8_C(30)         // ERASE keyword
#define TOK_EDIT        UINT8_C(31)         // EDIT keyword
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
#define TOK_LOAD        UINT8_C(63)         // LOAD keyword
#define TOK_ADDROF      UINT8_C(64)         // @ address-of operator
#define TOK_LE          UINT8_C(65)         // <= operator
#define TOK_NE          UINT8_C(66)         // <> not-equal operator
#define TOK_GE          UINT8_C(67)         // >= operator
#define TOK_SHOW        UINT8_C(68)         // SHOW keyword
#define TOK_WARRANTY    UINT8_C(69)         // WARRANTY keyword
#define TOK_COPYING     UINT8_C(70)         // COPYING keyword
#define TOK_DIM         UINT8_C(71)         // DIM keyword
#define TOK_DEF         UINT8_C(72)         // DEF keyword
#define TOK_INT         UINT8_C(73)         // INT keyword
#define TOK_STR         UINT8_C(74)         // STR keyword
#define TOK_FLT         UINT8_C(75)         // FLT keyword
#define TOK_OPTION      UINT8_C(76)         // OPTION keyword
#define TOK_BASE        UINT8_C(77)         // BASE keyword
#define TOK_ASC         UINT8_C(78)         // ASC keyword
#define TOK_VAL         UINT8_C(79)         // VAL keyword
#define TOK_LEFT        UINT8_C(80)         // LEFT keyword
#define TOK_MID         UINT8_C(81)         // MID keyword
#define TOK_RIGHT       UINT8_C(82)         // RIGHT keyword
#define TOK_INKEY       UINT8_C(83)         // INKEY keyword
#define TOK_BIN         UINT8_C(84)         // BIN keyword
#define TOK_QUA         UINT8_C(85)         // QUA keyword
#define TOK_OCT         UINT8_C(86)         // OCT keyword
#define TOK_DEC         UINT8_C(87)         // DEC keyword
#define TOK_HEX         UINT8_C(88)         // HEX keyword
#define TOK_GOTO        UINT8_C(89)         // GOTO keyword
#define TOK_GOSUB       UINT8_C(90)         // GOSUB keyword
#define TOK_LBRACK      UINT8_C(91)         // [ left bracket
#define TOK_BACKSL      UINT8_C(92)         // \ operator (integer division)
#define TOK_RBRACK      UINT8_C(93)         // ] right bracket
#define TOK_POW         UINT8_C(94)         // ^ operator (*)
#define TOK_GO          UINT8_C(95)         // GO keyword
#define TOK_BACKTK      UINT8_C(96)         // ` back tick (shell operator)
#define TOK_TO          UINT8_C(97)         // TO keyword
#define TOK_SUB         UINT8_C(98)         // SUB keyword
#define TOK_RETURN      UINT8_C(99)         // RETURN keyword
#define TOK_IF          UINT8_C(100)        // IF keyword
#define TOK_UNLESS      UINT8_C(101)        // UNLESS keyword
#define TOK_THEN        UINT8_C(102)        // THEN keyword
#define TOK_ELSE        UINT8_C(103)        // ELSE keyword
#define TOK_ENDIF       UINT8_C(104)        // ENDIF keyword
#define TOK_ENDUNLESS   UINT8_C(105)        // ENDUNLESS keyword
#define TOK_END         UINT8_C(106)        // END keyword
#define TOK_FOR         UINT8_C(107)        // FOR keyword
#define TOK_STEP        UINT8_C(108)        // STEP keyword
#define TOK_NEXT        UINT8_C(109)        // NEXT keyword
#define TOK_REPEAT      UINT8_C(110)        // REPEAT keyword
#define TOK_WHILE       UINT8_C(111)        // WHILE keyword
#define TOK_UNTIL       UINT8_C(112)        // UNTIL keyword
#define TOK_WEND        UINT8_C(113)        // WEND keyword
#define TOK_UEND        UINT8_C(114)        // UEND keyword
#define TOK_POP         UINT8_C(115)        // POP keyword
#define TOK_AFTER       UINT8_C(116)        // AFTER keyword
#define TOK_EVERY       UINT8_C(117)        // EVERY keyword
#define TOK_ON          UINT8_C(118)        // ON keyword
#define TOK_OFF         UINT8_C(119)        // OFF keyword
#define TOK_SYMBOL      UINT8_C(120)        // SYMBOL keyword
#define TOK_FN          UINT8_C(121)        // FN keyword
#define TOK_LET         UINT8_C(122)        // LET keyword
#define TOK_LBRACE      UINT8_C(123)        // { left brace
#define TOK_COLUMN      UINT8_C(124)        // | column
#define TOK_RBRACE      UINT8_C(125)        // } right brace
#define TOK_TILDE       UINT8_C(126)        // ~ tilde

#define LINENO_MIN      UINT16_C(0)         // minimum user-supplied line number
#define LINENO_MAX      UINT16_C(65534)     // maximum user-supplied line number
#define LINENO_DEL      UINT16_C(65535)     // deleted line or no line number
#define LINENO_NONE     UINT16_C(65535)

#pragma pack(2)
typedef struct _linehdr_t {
    // all values in network byte order (big endian)
    uint16_t    nextoffs;   // from beginning of memory
    uint16_t    prevoffs;   // from beginning of memory
    uint16_t    lineno;
    uint16_t    length;
    uint16_t    alloc;
    // followed by tokenized line data
} linehdr_t;
#pragma pack()

#define MAX_PROGRAMSIZE 65536

typedef struct _program_t {
    uint8_t     memory[MAX_PROGRAMSIZE];
    uint16_t    fillpos, firstlineoffs, lastlineoffs;
} program_t;

typedef struct _pgmiter_t {
    program_t*  pgm;
    uint8_t*    ptr;
    uint8_t*    end;
    uint8_t*    tok;
    uint16_t    offs;
    linehdr_t   hdr;
} pgmiter_t;

bool tokenize_line( const char* buf, uint8_t* whereto, size_t* premain, linehdr_t* phdr );
bool detokenize_line( char* buf, const uint8_t* wherefrom, size_t* premain, const linehdr_t* phdr );
void preprocess_buffer( char* buf );

#endif
