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

#ifndef POLICY_H
#define POLICY_H        1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

/*
The security policy (roadmap item 54, owner's design of 2026-11-03; docs/security-policy.md): what a RUNNING PROGRAM may do with the machine. No SDL, no VM.

 * While a program runs (the REPL tells: policy_begin_program / policy_end_program), the file drivers may reach files below the program's folder only
   (the folder of the program file; for an executable made with BUILD: the folder of the executable; for a program that was typed in: the current
   directory). Never up (..), never absolute paths elsewhere, symbolic links are resolved. Commands typed at the prompt are never restricted.
 * The other powers are asked for: reading outside the folder (POL_READ), writing outside it (POL_WRITE), the shell (shell literals and sh:) (POL_SHELL)
   and the network (tcp:) (POL_NET). mem:, null:, con:, stdin:, stdout:, stderr:, res:, play: need no permission.
 * The question goes to the host (policy_set_asker): the window shows a dialog, the console host asks on the terminal. The answer is: no, yes for this
   session, yes for 24 hours, yes always. The last two are stored in the file ~/.config/engine1999/grants (ENGINE1999_GRANTS names another file), keyed by the
   path of the program and a hash of its content (and of its permission file), so a changed program has no consent.
 * A program may ask for permissions up front with a file next to it, NAME.perm (NAME = the program file without its extension): lines "request read",
   "request write", "request shell", "request net" or "request all" (# starts a comment). The user is asked once, when the program starts.
 * The command line options --allow=LIST and --deny=LIST (read, write, shell, net, all) decide without asking.
 The policy does nothing until the host switches it on (policy_enable): the tests of the other modules are not affected.
*/

#define POL_READ        0
#define POL_WRITE       1
#define POL_SHELL       2
#define POL_NET         3
#define POL_COUNT       4

#define POL_ANS_DENY    0           // the answers of the asker
#define POL_ANS_SESSION 1
#define POL_ANS_DAY     2
#define POL_ANS_ALWAYS  3

// asks the user whether the running program may use a power; what = a short description ("write the file /tmp/x"); returns POL_ANS_*
typedef int (*policy_ask_t)( void* ud, int category, const char* what );

void        policy_enable( bool on );
bool        policy_enabled( void );
void        policy_set_asker( policy_ask_t ask, void* ud );
const char* policy_category_name( int category );                       // "read", "write", "shell", "net"

// --allow / --deny: a comma separated list of read, write, shell, net, all; returns false (with a text in err) for a bad word
bool        policy_set_option( const char* list, bool allow, char* err, size_t errsize );

// a program starts to run: file = its file (0 or "" for a program that was typed in), image / len = its content for the hash (0: the file is read);
// root = its folder (0: the folder of file, or the current directory). It loads the stored grants and asks for what the permission file requests.
void        policy_begin_program( const char* file, const void* image, size_t len, const char* root );
void        policy_end_program( void );
bool        policy_active( void );                                      // enabled and a program runs

// the decision for an operation of a running program: operation "read", "write", "list", "shell", "net", "sound"; drv = the name of the driver (0 for
// the shell literal); path = what follows the driver prefix. Returns true when it is allowed (it may have asked).
bool        policy_check( const char* operation, const char* drv, const char* path );

// for the tests: the lexical + symbolic-link normal form of a path (absolute); false if too long
bool        policy_normalize( const char* path, char* out, size_t outsize );
bool        policy_inside( const char* path, const char* root );        // is the path below (or equal to) the folder
const char* policy_root( void );                                        // the root of the running program ("" if none)
void        policy_forget_session( void );                              // the tests: drops the session grants

#endif
