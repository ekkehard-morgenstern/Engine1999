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

    ADDITIONAL PERMISSION (GPLv3 section 7): the Engine1999 Runtime Exception
        applies. Programs written in the built-in programming language, and
        executables made from them with BUILD, do not fall under this license
        as the exception describes (see the file LICENSE-EXCEPTION).

    CONTACT INFO:
        E-Mail: ekkehard@ekkehardmorgenstern.de
        Mail: Ekkehard Morgenstern, Mozartstr. 1, D-76744 Woerth am Rhein,
              Germany, Europe
*/

#ifndef SYNHL_H
#define SYNHL_H     1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

#include "bastok.h"

/*
Syntax highlighting of one line of BASIC source text (no UI, no VM). synhl_line() writes one class per byte of the line: the value is an hlclass_t
(HL_NONE = plain text, blanks; HL_LINENO, HL_STATEMENT, HL_CONTROL, HL_COMMAND, HL_FUNCTION, HL_OPERATOR, HL_IDENT, HL_NUMBER, HL_STRING, HL_SHELL,
HL_COMMENT, HL_PUNCT). The rules are the tokenizer's (the same eat_* functions, the same case rules: upper case outside strings and comments, REM
at the start of a statement is a comment) and the classes of the keywords are those of LIST (keyword_hlclass()), so every colored listing agrees.
Lines are independent in BASIC (a string or a comment ends with its line). text is not NUL-terminated; len may be 0.
*/
void synhl_line( const char* text, size_t len, uint8_t* cls );

#define SYN_SLOTS       16              // the colors a syntax highlighted text can use: the values 0 .. 15 (the first 13 are the hlclass_t values above)

#endif
