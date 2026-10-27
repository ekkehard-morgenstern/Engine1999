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

#ifndef BASDEBUG_H
#define BASDEBUG_H  1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

#ifndef BASTOK_H
#include "bastok.h"
#endif

#ifndef BASLIN_H
#include "baslin.h"
#endif

#ifndef BASCOMP_H
#include "bascomp.h"
#endif

/*
Debug output of the BASIC system. Everything is off by default; a host (or a test program) turns parts on by setting
bits in basdbg_flags. The output goes to stdout, one line per item, prefixed with the stage in brackets.

    BASDBG_TOKENIZE     [tokenize]    the token bytes of every line the tokenizer produced (hex and printable characters)
    BASDBG_DETOKENIZE   [detokenize]  the text rebuilt from those tokens (shows what the tokenizer understood)
    BASDBG_PARSE        [parse]       every token as the compiler fetches it
    BASDBG_TREE         [tree]        the syntax tree after parsing
    BASDBG_CODE         [code]        every instruction as the code generator emits it (offset, bytes, mnemonic)

The tokenize / detokenize / tree dumps are produced by the hooks the REPL (basrepl.c) calls; the parse and code
output come from the compiler and the code generator themselves.
*/

#define BASDBG_TOKENIZE     UINT32_C(0X01)
#define BASDBG_DETOKENIZE   UINT32_C(0X02)
#define BASDBG_PARSE        UINT32_C(0X04)
#define BASDBG_TREE         UINT32_C(0X08)
#define BASDBG_CODE         UINT32_C(0X10)
#define BASDBG_ALL          UINT32_C(0X1F)

extern uint32_t basdbg_flags;

#define basdbg_on(f)        ( ( basdbg_flags & (f) ) != 0 )

// tokline: a tokenized line as produced by tokenize_line() (linehdr_t followed by the tokens)
void basdbg_print_tokens( const uint8_t* tokline );
void basdbg_print_detokenized( const uint8_t* tokline );
void basdbg_print_tree( compiler_t* comp );

#endif
