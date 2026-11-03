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

#ifndef BASLIB_H
#define BASLIB_H        1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

#include "baspgm.h"

/*
    Library support, step 2 (docs/library-support.md): finding out which libraries a program needs.

    A library is a program whose first statement is LIBRARY "name" [, "prefix"]. A program (or library) names the libraries it needs with
    LOAD LIBRARY "name" statements. They are declarations: this module scans the lines of a program as text (the same way find_label_line()
    does: strings and comments are skipped), loads every named library through a callback (finding the file is the host's business: search
    path, drivers), scans that in turn, and returns the libraries in dependency order (a library comes before the ones that need it).
    No SDL, no VM, no compiler.
*/

#define BASLIB_MAXDEPTH     32      // libraries needing libraries needing ...

typedef struct _baslib_t {
    char*       name;       // the name the library declares: LIBRARY "name"
    char*       prefix;     // the variable prefix ("" if the library declares none)
    char*       file;       // the file it was read from (as the loader reported it)
    uint32_t    headline;   // the line number of its LIBRARY statement
    program_t   pgm;        // its lines (owned: free_program() by baslib_free())
} baslib_t;

typedef struct _liblist_t {
    baslib_t*   libs;       // in dependency order
    size_t      count;
    size_t      capacity;
} liblist_t;

// the first thing that is wrong: a message, the line it was found in, and the library whose line it is ("" = the program)
typedef struct _baslib_error_t {
    char        msg[240];
    uint32_t    line;
    char        lib[64];
} baslib_error_t;

// what a loader says about a library it was asked for
#define BASLIB_FOUND        0
#define BASLIB_NOTFOUND     1       // there is no such file
#define BASLIB_FAILED       2       // the file is there but can't be read as a program: the loader has put the reason into 'reason'

// A loader finds the library called 'name' (as written in LOAD LIBRARY) and reads it into *pgm (an initialized, empty program_t; the callee
// fills it). *pfile receives the file that was read (malloc'ed). 'reason' (a text of 'reasonsize' bytes) is set for BASLIB_FAILED.
typedef int (*baslib_loader_t)( void* usrdata, const char* name, program_t* pgm, char** pfile, char* reason, size_t reasonsize );

void baslib_init( liblist_t* list );
void baslib_free( liblist_t* list );

// What the first statement of a program is. true if it is LIBRARY "name" [, "prefix"] (comment lines may come before it; *pname and *pprefix are
// malloc'ed, the prefix is "" when there is none, *plineno is the line of the statement). false: an ordinary program (or an empty one).
bool baslib_header( program_t* pgm, char** pname, char** pprefix, uint32_t* plineno );

// Finds the libraries that 'pgm' needs, directly and indirectly. On success 'out' (initialized by the caller, emptied here) holds them in
// dependency order, each once. On failure false with *err filled and 'out' empty. Checks made on the way: a LOAD LIBRARY that stands behind
// IF / THEN / ELSE or inside a PROC / FUNC ("must be a plain statement"), a LIBRARY statement that is not the first statement of its
// file, a file that is not a library, a library whose declared name is not the name it was asked for, a bad or shared prefix, a cycle,
// nesting deeper than BASLIB_MAXDEPTH, a library that is not found.
bool baslib_resolve( program_t* pgm, baslib_loader_t load, void* usrdata, liblist_t* out, baslib_error_t* err );

// Is this a valid library prefix: a letter, then letters, digits and underscores (the empty string is valid: no prefix)?
bool baslib_valid_prefix( const char* prefix );

// The directories of the installed libraries (section 3 of docs/library-support.md, step 4): $ENGINE1999_HOME/lib, <program>/../lib/engine1999/lib and the usual
// system places. Returns how many were stored (malloc'ed strings in dirs[0..n-1]; free them with free()).
size_t baslib_system_dirs( char* dirs[], size_t maxdirs );

#endif
