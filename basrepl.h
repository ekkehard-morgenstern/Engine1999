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
#include "baslib.h"
#endif

/*
The read-eval-print loop of the system, without any host dependencies (no SDL, no terminal).

A repl_t owns the runtime (variables, code), the compiler, the program store and a persistent VM (so the DATA
pointer survives from one direct mode line to the next). The host feeds it text lines with repl_enter():

    - a line with a line number is stored in the program (a line number alone deletes the line)
    - a line without one is compiled and run at once (direct mode); the code is appended to the code segment, so
      the variables and the user functions defined by earlier lines stay valid. Afterwards the code and the constants of
      the line are discarded again (cgen_rollback()), unless the line defined a function, a label or DATA items: those
      point into the code / data, so such a line stays. A long session of ordinary lines doesn't grow the segments.
    - RUN [line|label] compiles the stored program and runs it, with all variables cleared first (afterwards the
      variables stay available in direct mode); LIST [a][-[b]] lists it; LOAD "file" replaces it by the program in
      the file (binary or text, see baspgm.h) and runs that. These three are language statements, usable in programs
      too (cgenprog.c); the VM can't carry out RUN and LOAD itself, so it stops and leaves a request (vm.h) that the
      REPL carries out once the run has ended (repl_finish(), iteratively: a program may end with RUN).
      NEW deletes the program and clears everything (a statement too: the VM stops and leaves a request); CLEAR empties
      the variables inside the VM.

The host sets the callbacks after init_repl(). Output, errors and keyboard access all go through them; the VM
callbacks have the signatures of vm.h and receive the repl_t* as their user data parameter (use repl_t.userdata
for host data). Error messages (compiler, code generator, variables, VM) go to report() and always end in a
newline.

Known limitations: a line that defines a function, label or DATA keeps its code (a function that is defined again leaves
the old code behind until NEW or RUN); direct mode can't GOTO into the stored program.
*/

// a library that is part of a built executable: its name and the image of its program file
typedef struct _repl_embedlib_t {
    char*       name;
    uint8_t*    data;
    size_t      len;
} repl_embedlib_t;

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
    void        (*printn)( struct _vm_t*, void*, int channel, const char* text, size_t len );   // optional, see vm.h
    void        (*breakesc)( struct _vm_t*, void*, bool on );   // BREAK ON / OFF was executed: ESC is the break key / an ordinary key (optional)
    char*       (*clipget)( struct _vm_t*, void*, size_t* len );        // CLIPBOARD$ (optional: without it the internal clipboard)
    void        (*clipset)( struct _vm_t*, void*, const char* text, size_t len );   // CLIPBOARD a$ (optional)
    bool        (*keydown)( struct _vm_t*, void*, int keyid );  // KEY("name"): is this key (KEYID_*, keystate.h) held down now? (optional: without it KEY() is a run-time error; the console host has none)
    int         (*inkey)( struct _vm_t*, void* );
    char*       (*input)( struct _vm_t*, void*, int channel );
    int         (*getkey)( struct _vm_t*, void*, int channel );
    bool        (*interrupt)( struct _vm_t*, void* );
    bool        (*save)( struct _vm_t*, void*, const char* filename, bool ascii );
    const vm_sound_t* sound;        // SOUND, PLAY, ...: the host's sound output (optional, see vm.h)
    const vm_gfx_t*   gfx;          // GFX, SPRITE, ...: the host's graphics output (optional, see vm.h)
    // syntax highlighting of LIST output (optional): returns the text (e.g. an escape sequence) to insert where text of
    // the class starts; HL_NONE = end of the line. Only used when LIST writes to channel 0, the screen (bastok.h)
    hlstyle_t   hlstyle;
    // line editor for EDIT (optional): gets the text of the line (with its number), lets the user change it and returns the
    // result in a malloc'ed string, or 0 if the user cancelled (the host reports its own problems, e.g. a line too long)
    char*       (*edit)( struct _repl_t*, const char* text );
    // FILES listings to the screen channel stop after pagesize - 1 lines (0 = never) and ask the host to continue: pause()
    // returns false when the user wants to stop (a stop by the Break key is reported as "Break" afterwards)
    int         pagesize;
    bool        (*pause)( struct _repl_t* );
    char*       pending;            // an edited line waiting to be entered (see repl_enter())
    // EDIT without a target asks the host for the full-screen editor: the REPL sets this flag and returns; the host
    // checks it after repl_enter() (not inside a callback: the editor enters lines itself), clears it and opens the editor
    bool        editwin;                // (with fulledit) EDIT WINDOW: the host opens the editor window (a UI widget) instead of the text screen editor, if it has one
    bool        fulledit;
    // MONITOR asks the host for the machine monitor the same way (the run ends, the host checks the flag after repl_enter())
    bool        monitor;
    // BYE [code] (or QUIT [code]) asks the host to end the process: quit is set, exitcode (0..255) is the code
    bool        quit;
    bool        buildconsole;       // BUILD without WINDOW / CONSOLE makes a console executable (the console host sets this)
    int         exitcode;
    // the arguments of the program for ARGC / ARG$ (argv[0] = the program name; the host owns the strings, which must stay valid)
    int         argc;
    const char* const* argv;
    // AUTO [start[,step]]: the host prompts with line numbers while autoline != LINENO_NONE (see AUTO in docs/basic-language.md);
    // it enters the lines itself and calls repl_auto_next() after each one
    uint32_t    autoline, autostep;
    bool        trace;              // TRACE ON: the VM prints [n] for every line, also in the next runs
    bool        started;            // internal: repl_execute() got as far as running the program (CHAIN keeps the old runtime if not)
    bool        linesstale;         // internal: the stored program was changed since it was compiled (its line table is out of date)
    runtime_t*  keeprt;             // internal (CHAIN): the previous runtime, whose variables the program being started takes over
    repl_embedlib_t* embedlibs;   // a built executable: the libraries that came with the program (BUILD embeds them); 0 otherwise
    size_t      nembedlibs;
    bool        embedded;           // the program is that of a built executable (repl_load_image()): LOAD LIBRARY looks only in embedlibs
    liblist_t   libs;               // the libraries the stored program names (LOAD LIBRARY), in dependency order: found by every RUN / compile (docs/library-support.md)
    bool        fileascii;          // the file named by filename is an ASCII listing (false: binary); what SAVE without a name and without an option writes
    char*       filename;           // name of the last file loaded or saved (malloc'ed), 0 = none or NEW; for the editor's title
    // the security policy (policy.h) asks who the running program is: the file it was loaded from, else (an executable made with BUILD) what the host says
    const char* policyfile;         // 0: filename; else the file of the program (the executable) and, for the hash, its embedded image
    const void* policyimage;
    size_t      policyimagelen;
    const char* policyroot;         // 0: the folder of the file (a typed in program: the current directory)
    // TOOL name$ [, arg$] (docs/basic-language.md): runs a tool of the tools folder in a repl_t of its own, then the caller goes on. The window host switches it on;
    // the console host leaves it off (the statement is the error "TOOL is not available in the console host"). The hooks (optional) let the host save what the
    // tool will disturb (the text screen) before it runs and put it back / clean up (close the UI) after it
    bool        toolsok;
    void        (*toolbegin)( struct _repl_t* outer );
    void        (*toolend)( struct _repl_t* outer );
    bool        istool;             // internal: this repl_t runs a tool (the security policy and the program name are left to the caller's)
    int         tooldepth;          // internal: how many tools are running above this one
    struct _repl_t* toolouter;      // internal: the repl_t that started this tool
    char*       toolerr;            // internal: the last message the tool reported (it is said again when the tool has ended and the screen is back)
} repl_t;

bool init_repl( repl_t* repl );             // false if out of memory
void cleanup_repl( repl_t* repl );

bool repl_enter( repl_t* repl, const char* text );  // one line of input; false if it was rejected (already reported)
bool repl_run( repl_t* repl );              // RUN (from the beginning)
void repl_new( repl_t* repl );              // NEW
void repl_list( repl_t* repl, uint32_t first, uint32_t last );  // LIST (LINENO_NONE = unbounded)
bool repl_compile( repl_t* repl );          // compiles the stored program without running it (variables are cleared); for the monitor
bool repl_vm_start( repl_t* repl, uint32_t addr );    // monitor: suspend the VM at addr (code segment as it is, empty stacks, variables kept)
bool repl_vm_resume( repl_t* repl, bool step, const uint32_t* breakpoints, size_t numbreakpoints );   // continue / single-step it; see repl_vm_resume()
bool repl_save_program( repl_t* repl, const char* filename, bool ascii );
bool repl_save_program_ex( repl_t* repl, const char* filename, int mode );   // mode: 0 none given, 1 A, 2 B; a null filename = the file of the last LOAD / SAVE (with mode 0: in the format it had)   // for the host's SAVE callback
bool repl_program_image( repl_t* repl, uint8_t** pdata, size_t* plen );     // the stored program as a binary program file (malloc'ed); BUILD
bool repl_load_file( repl_t* repl, const char* name );                    // LOAD without running (the extension search, drivers); reports errors
// the file of the tool <name> (malloc'ed), found in tools/ of the current directory, tools/ next to the stored program's folder, ENGINE1999_HOME/tools and the installed
// lib/engine1999/tools; 0 if there is none or the name is not a plain name
char* repl_find_tool_file( const repl_t* repl, const char* name );
bool repl_load_image( repl_t* repl, const void* data, size_t len );         // replaces the stored program by a program file in memory

// the stored program as text (lines "10 PRINT ..." separated by LF, no highlighting), malloc'ed; 0 if out of memory. *plen is its length.
char* repl_program_text( repl_t* repl, size_t* plen );
// replaces the stored program by the lines of text (every non-blank line must start with a line number and have text behind it; a line number alone is ignored):
// NEW, then every line is entered like a typed one; the name of the file stays. false and msg: nothing was changed (a line without a number). Otherwise true;
// *pcount lines were accepted, msg is "" or the first message of a rejected line (rejected lines are dropped).
bool repl_apply_text( repl_t* repl, const char* text, char* msg, size_t msgsz, size_t* pcount );

#endif
