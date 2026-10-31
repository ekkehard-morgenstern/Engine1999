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

#ifndef BASBUILD_H
#define BASBUILD_H  1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

/*
The BUILD statement: makes a standalone executable from a program (docs/basic-language.md, "BUILD"; no SDL, no VM).

Only the system linker (ld) is used, no compiler. The executable consists of
    - engine1999.o   the host (main(), the screen, keyboard and sound glue) of the Engine; its main() runs the embedded program
                     when the symbol _binary_program_e99_start exists, otherwise it is the REPL
    - program.o      the program (the binary .e99 form), made by "ld -r -b binary" from a file called program.e99
    - engine.lib     all other modules of the Engine, statically
    - the system libraries (SDL2, the math library, the C library ...), dynamically,
    - the startup files crt1.o, crti.o, crtn.o of the C library and (if present) crtbegin.o / crtend.o / libgcc.a of the compiler.
The files are looked for in the usual places (build_find_* below); nothing has to be configured. The environment variables
ENGINE1999_HOME (where the runtime kit is: engine1999.o and engine.lib), ENGINE1999_LD (the linker) and LIBRARY_PATH (more
library directories) take precedence; ENGINE1999_BUILD_TRACE=1 prints the commands to the standard error output.

The runtime kit is the directory of the running executable, the current directory, $ENGINE1999_HOME, /usr/local/lib/engine1999,
/usr/lib/engine1999, /usr/local/share/engine1999 and /usr/share/engine1999 (first match wins).
*/

#define BUILD_PROGRAM_FILE      "program.e99"       // the name of the embedded file; it gives the symbols their names
#define BUILD_PROGRAM_SYMBOL    "_binary_program_e99_start"
#define BUILD_PROGRAM_END       "_binary_program_e99_end"

typedef struct _build_opts_t {
    const char*         runtime_obj;    // the object with main(): a name looked for in the kit, or a path (with a '/'); default "engine1999.o"
    const char*         runtime_lib;    // the library: the same; default "engine.lib"; 0 = none
    const char* const*  libs;           // libraries that must be found (name without "lib" and ".so"), 0-terminated; default SDL2, m
                                        // (the C library is always linked, last)
    const char* const*  optlibs;        // libraries that are used if they exist; default rt, pthread, dl
    const char*         kitdir;         // directory searched first for the runtime files (0 = only the usual places)
} build_opts_t;

void build_default_opts( build_opts_t* opts );          // the window kit: engine1999.o, SDL2
// the kit for programs without a window (engine1999c.o, no SDL2: the executable needs only the C and math libraries)
void build_console_opts( build_opts_t* opts );

// Links the program image <prog> (the binary program file) into the executable <outname>. opts may be 0 (the defaults).
// false after a message in err (one line, no newline at the end).
bool build_executable( const char* outname, const void* prog, size_t len, const build_opts_t* opts, char* err, size_t errsize );

#endif
