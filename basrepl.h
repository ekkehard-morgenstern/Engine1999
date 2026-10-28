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

#ifndef BASREPL_H
#define BASREPL_H   1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

#ifndef BASIC_H
#include "basic.h"
#endif

#ifndef VM_H
#include "vm.h"
#endif

/*
The read-eval-print loop of the system, without any host dependencies (no SDL, no terminal).

A repl_t owns the runtime (variables, code), the compiler, the program store and a persistent VM (so the DATA
pointer survives from one direct mode line to the next). The host feeds it text lines with repl_enter():

    - a line with a line number is stored in the program (a line number alone deletes the line)
    - a line without one is compiled and run at once (direct mode); the code is appended to the code segment, so
      the variables and the user functions defined by earlier lines stay valid
    - RUN [line|label] compiles the stored program and runs it, with all variables cleared first (afterwards the
      variables stay available in direct mode); LIST [a][-[b]] lists it; LOAD "file" replaces it by the program in
      the file (binary or text, see baspgm.h) and runs that. These three are language statements, usable in programs
      too (cgenprog.c); the VM can't carry out RUN and LOAD itself, so it stops and leaves a request (vm.h) that the
      REPL carries out once the run has ended (repl_finish(), iteratively: a program may end with RUN).
      NEW deletes the program and clears everything; it is the one host level command left.

The host sets the callbacks after init_repl(). Output, errors and keyboard access all go through them; the VM
callbacks have the signatures of vm.h and receive the repl_t* as their user data parameter (use repl_t.userdata
for host data). Error messages (compiler, code generator, variables, VM) go to report() and always end in a
newline.

Known limitations: every direct mode line appends to the 64 KiB code segment until NEW or RUN; direct mode can't
GOTO into the stored program.
*/

typedef struct _repl_t {
    runtime_t   rt;
    compiler_t  comp;
    program_t   pgm;
    vm_t        vm;
    buf_t*      tokbuf;
    jmp_buf     jmp;                // compiler / variable store / code generator errors jump here
    bool        vmready;
    void*       userdata;           // for the host
    // host callbacks (all optional; see vm.h for the VM ones)
    void        (*report)( struct _repl_t*, const char* text );
    void        (*print)( struct _vm_t*, void*, int channel, const char* text );
    int         (*inkey)( struct _vm_t*, void* );
    char*       (*input)( struct _vm_t*, void*, int channel );
    int         (*getkey)( struct _vm_t*, void*, int channel );
    bool        (*interrupt)( struct _vm_t*, void* );
    bool        (*save)( struct _vm_t*, void*, const char* filename, bool ascii );
} repl_t;

bool init_repl( repl_t* repl );             // false if out of memory
void cleanup_repl( repl_t* repl );

bool repl_enter( repl_t* repl, const char* text );  // one line of input; false if it was rejected (already reported)
bool repl_run( repl_t* repl );              // RUN (from the beginning)
void repl_new( repl_t* repl );              // NEW
void repl_list( repl_t* repl, uint32_t first, uint32_t last );  // LIST (LINENO_NONE = unbounded)
bool repl_save_program( repl_t* repl, const char* filename, bool ascii );   // for the host's SAVE callback

#endif
