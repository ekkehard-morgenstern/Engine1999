#pragma once
#ifndef PRINTUSING_H
#define PRINTUSING_H

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

    ADDITIONAL PERMISSION (GPLv3 section 7): the Engine1999 Runtime Exception
        applies. Programs written in the built-in programming language, and
        executables made from them with BUILD, do not fall under this license
        as the exception describes (see the file LICENSE-EXCEPTION).

    CONTACT INFO:
        E-Mail: ekkehard@ekkehardmorgenstern.de
        Mail: Ekkehard Morgenstern, Mozartstr. 1, D-76744 Woerth am Rhein,
              Germany, Europe
*/

#include "stdtypes.h"

/*
    PRINT USING: the format string of PRINT USING f$; a, b$, ...  (no VM, no screen; docs/basic-language.md "PRINT USING").

    A format is literal text with fields. The fields:
        string:  !  the first character     \  \ (backslash, n spaces, backslash) n + 2 characters     &  the whole string
        number:  # digit position, . decimal point, , thousands separators (anywhere among the digits), a leading + (sign always shown) or a
                 trailing + / - (sign at the end: + or -, a space instead of a missing minus), ** fill the leading blanks with *, $$ a dollar sign
                 in front of the number (uses a digit position), **$ both, ^^^^ exponent (E+nn)
        _  the next character is literal (also _! _# _& _\ _+ ...)
    A number that does not fit its field is printed in full with a % in front.
*/

#define PU_NONE     0       // no field (end of the format)
#define PU_NUMBER   1
#define PU_STRING   2

typedef struct {
    uint8_t kind;           // PU_*
    size_t start;           // where the field starts in the format
    uint16_t width;         // width of the field in the format text; PU_STRING: the characters it shows (0 for &: the whole string)
    // PU_NUMBER:
    bool leadsign;          // leading + : the sign is always shown in front
    uint8_t trail;          // trailing sign: 0 none, '+' or '-'
    bool stars;             // ** : fill with *
    bool dollar;            // $$ : a dollar sign
    bool comma;             // thousands separators
    bool expo;              // ^^^^ : exponent
    bool point;             // there is a decimal point (also without digits behind it)
    uint16_t nint;          // digit positions before the point (** and $$ count 2 and 1)
    uint16_t ndec;          // digit positions after the point
} pu_field_t;

// scans the format from pos: literal text is copied to lit (at most len bytes, no terminator; *litlen is its length) up to the next field,
// which is described by f (kind PU_NONE if the format ends first); returns the position behind the field (len if there is none)
size_t pu_next( const char* fmt, size_t len, size_t pos, char* lit, size_t* litlen, pu_field_t* f );

// is there any field in the format?
bool pu_has_field( const char* fmt, size_t len );

// formats a number into out (at least PU_NUMBER_MAX bytes); returns its length (no terminator)
#define PU_NUMBER_MAX   1100
size_t pu_number( const pu_field_t* f, double x, char* out );

// formats a string: PU_STRING fields only; out has room for max( f->width, len ) bytes; returns the length ('!' gives one character, a blank for an empty string)
size_t pu_string( const pu_field_t* f, const char* str, size_t slen, char* out );

#endif
