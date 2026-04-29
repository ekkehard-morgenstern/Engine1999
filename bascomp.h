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

Language syntax (using tokens)
------------------------------



num-ex-list := num-expr { TOK_COMMA num-expr } .
str-ex-list := str-expr { TOK_COMMA str-expr } .
expr-list   := expr { TOK_COMMA expr } .

array-index := num-ex-list | str-expr .
array-sub := TOK_LPAREN array-index TOK_RPAREN .

array-dim-decl := num-expr-list | TOK_DYNAMIC .
array-decl := any-base-var-ref TOK_LPAREN array-dim-decl TOK_RPAREN .
array-decl-list := array-decl { TOK_COMMA array-decl } .

empty-array-ref := any-base-var-ref TOK_LPAREN TOK_RPAREN .
empty-array-ref-list := empty-array-ref { TOK_COMMA empty-array-ref } .

num-base-var-ref := TOK_IDENT [ TOK_PERCENT ] .
num-var-ref      := num-base-var-ref [ array-sub ] .

str-base-var-ref := TOK_IDENT TOK_DOLLAR .
str-var-ref      := str-base-var-ref [ array-sub ] .

any-base-var-ref := num-base-var-ref | str-base-var-ref .

dec-lit := TOK_DECLIT | TOK_DEC0 .. TOK_DEC9 .
num-lit := dec-lit | TOK_HEXLIT | TOK_OCTLIT | TOK_QUALIT | TOK_BINLIT .

str-lit := TOK_STRLIT | TOK_SHLLIT .
str-lits := str-lit { str-lit } .

num-usr-fn-name := TOK_FN TOK_IDENT [ TOK_PERCENT ] .
str-usr-fn-name := TOK_FN TOK_IDENT TOK_DOLLAR .

num-usr-fn-call := num-usr-fn-name TOK_LPAREN expr-list TOK_RPAREN .
str-usr-fn-call := str-usr-fn-name TOK_LPAREN expr-list TOK_RPAREN .

sys-num-func := TOK_ASC | TOK_BIN | TOK_QUA | TOK_OCT | TOK_DEC | TOK_HEX .
sys-str-func-name := TOK_LEFT | TOK_MID | TOK_RIGHT | TOK_STR .
sys-str-func := sys-str-func-name TOK_DOLLAR .

sys-num-fn-arg-call := sys-num-func TOK_LPAREN expr-list TOK_RPAREN .
sys-str-fn-arg-call := sys-str-func TOK_LPAREN expr-list TOK_RPAREN .

sys-noarg-str-name := TOK_INKEY .
sys-noarg-str := sys-noarg-str-name TOK_DOLLAR .
sys-noarg-str-call := sys-no-arg-str .

num-func-call := num-usr-fn-call | sys-num-fn-arg-call .
str-func-call := str-usr-fn-call | sys-str-fn-arg-call | sys-noarg-str-call .

str-base-expr := str-var-ref | str-lits | str-func-call .
str-add-expr := str-base-expr { TOK_PLUS str-base-expr } .
str-expr := str-add-expr .

num-sub-expr := TOK_LPAREN num-expr TOK_RPAREN .
num-base-expr := num-var-ref | num-lit | num-func-call | num-sub-expr .

num-unary-op := TOK_MINUS | TOK_PLUS | TOK_NOT .
num-unary-ex := [ num-unary-op ] num-base-expr .

num-mult-op  := TOK_MULT | TOK_DIV | TOK_POW .
num-mult-ex  := num-unary-ex { num-mult-op num-unary-ex } .

num-add-op   := TOK_PLUS | TOK_MINUS .
num-add-ex   := num-mult-ex { num-add-op num-mult-ex } .

num-shift-op := TOK_LSHIFT | TOK_RSHIFT .
num-shift-ex := num-add-ex [ num-shift-op num-add-ex ] .

num-cmp-op   := TOK_EQ | TOK_NE | TOK_LE | TOK_GE | TOK_LT | TOK_GT .
num-cmp-ex   := num-shift-ex [ num-cmp-op num-shift-ex ] .

num-and-op   := TOK_AND | TOK_NAND .
num-and-ex   := num-cmp-ex { num-and-op num-cmp-ex } .

num-or-op    := TOK_OR | TOK_XOR | TOK_NOR | TOK_XNOR .
num-or-ex    := num-and-ex { num-or-op num-and-ex } .

num-expr     := num-or-ex .

expr         := num-expr | str-expr .

save-stmt := SAVE str-expr [ TOK_COMMA TOK_IDENT ] .

print-sep := TOK_COMMA | TOK_SEMIC .
print-arg := expr [ print-sep ] .
print-arg-list := print-arg { print-arg } .
print-stmt := TOK_PRINT [ print-arg-list ] .

io-stmt := save-stmt | print-stmt .

num-assign := num-var-ref TOK_EQ num-expr .
str-assign := str-var-ref TOK_EQ str-expr .
substr-op := TOK_LEFT | TOK_MID | TOK_RIGHT .
substr-assign := substr-op TOK_DOLLAR TOK_LPAREN expr-list TOK_RPAREN .
any-assign := num-assign | substr-assign | str-assign .
let-stmt := [ TOK_LET ] any-assign .

dim-stmt := TOK_DIM array-decl-list .
erase-stmt := TOK_ERASE empty-array-ref-list .

assign-stmt := let-stmt | dim-stmt | erase-stmt .

for-stmt := num-base-var-ref TOK_EQ num-expr TOK_TO num-expr [ TOK_STEP num-expr ] .
next-stmt := TOK_NEXT [ num-base-var-ref { TOK_COMMA num-base-var-ref } ] .

goto-kw := TOK_GO TOK_TO | TOK_GOTO | TOK_GO TOK_SUB | TOK_GOSUB .
goto-target := TOK_DECNUM | TOK_IDENT .
goto-stmt := goto-kw goto-target .

return-stmt := TOK_RETURN .

label-stmt := TOK_LABEL TOK_IDENT .

endif-kw := TOK_END TOK_IF | TOK_ENDIF .
endunless-kw := TOK_END TOK_UNLESS | TOK_ENDUNLESS .

then-kw := TOK_THEN | goto-kw .

singleline-if-stmt := ( TOK_IF | TOK_UNLESS ) num-expr
           then-kw ( goto-target [ TOK_ELSE goto-target ] |
                     stmt-list   [ TOK_ELSE stmt-list   ] ) .
-- this requires that the line number does not change during
-- the processing of this production.

multiline-if-stmt := TOK_IF num-expr TOK_THEN TOK_EOL
                        stmt-lines
                        [ TOK_ELSE stmt-lines ]
                        endif-kw |
                     TOK_UNLESS num-expr TOK_THEN TOK_EOL
                        stmt-lines
                        [ TOK_ELSE stmt-lines ]
                        endunless-kw TOK_EOL .

control-flow-stmt := for-stmt | next-stmt | goto-stmt | return-stmt | label-stmt |
                     singleline-if-stmt | multiline-if-stmt .

stmt := io-stmt | assign-stmt | control-flow-stmt .
stmt-list := stmt { TOK_COLON stmt } .
stmt-line := stmt-list TOK_EOL .
stmt-lines := stmt-line { stmt-line } .





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
