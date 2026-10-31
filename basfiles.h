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

#ifndef BASFILES_H
#define BASFILES_H  1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

#ifndef BASTOK_H
#include "bastok.h"
#endif

/*
The listing of the FILES statement (no SDL, no VM): the entries of a directory or of a file name pattern, oldest first
like "ls -ltr", one line each: size in bytes, modification time (YYYY-MM-DD HH:MM) and the name, followed by the "ls -F" type
indicator: "/" directory, "*" executable, "@" symbolic link (and " -> target"), "|" FIFO, "=" socket. There are no mode bits or
owners (use a shell literal for those). The text is colored through a hlstyle_t callback (classes HL_NUMBER for the size,
HL_COMMENT for the time, HL_FILEDIR, HL_FILELINK, HL_FILEEXEC, HL_FILEPROG for .bas / .e99 files, HL_IDENT for other names);
with style == 0 it is plain.
*/

// is called for every line (without a newline); returns false to stop the listing
typedef bool (*files_emit_t)( void* usrdata, const char* line );

/*
spec: "" = the current directory (names starting with '.' are left out, like ls); a directory = its entries; a name = that
file; a pattern with * ? [ ] = the matching names (as they are written in the pattern, with their directory).
Returns false with a message in err if the spec matches nothing or something fails; a listing that was stopped by the
callback is a success.
*/
// formats one entry as files_list() does (size right-aligned to sizewidth, time, name with its indicator and the target of a
// link, colored through style) and hands the line to emit; *pstop is set when emit asked to stop. false: out of memory (err).
bool files_format_entry( int sizewidth, long long size, time_t mtime, hlclass_t nameclass, const char* name, const char* indicator,
    const char* target, hlstyle_t style, void* styleud, files_emit_t emit, void* emitud, bool* pstop, char* err, size_t errsz );

bool files_list( const char* spec, hlstyle_t style, void* styleud, files_emit_t emit, void* emitud, char* err, size_t errsz );

#endif
