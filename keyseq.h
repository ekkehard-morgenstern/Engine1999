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



#ifndef KEYSEQ_H
#define KEYSEQ_H    1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

/*
The keys that are not one byte, as the program sees them: ANSI escape sequences in the xterm form, the same in the window and in the
console host (docs/basic-language.md, "Keyboard: cursor keys, function keys, the character set"). This module has no dependencies; it makes
the canonical sequence of a key, and recognizes the sequences terminals send (several dialects) so that a host can turn them into the
canonical one.

    key                      canonical sequence                     with modifiers (m = 1 + shift 1 + alt 2 + ctrl 4 + meta 8)
    UP DOWN RIGHT LEFT       ESC [ A  B  C  D                       ESC [ 1 ; m A ...
    HOME END                 ESC [ H  F                             ESC [ 1 ; m H ...
    F1 .. F4                 ESC O P  Q  R  S                       ESC [ 1 ; m P ...
    INSERT DELETE PGUP PGDN  ESC [ 2 ~  3 ~  5 ~  6 ~               ESC [ 2 ; m ~ ...
    F5 .. F12                ESC [ 15 ~  17 ~  18 ~  19 ~  20 ~  21 ~  23 ~  24 ~
*/

typedef enum _keyseq_key_t {
    KS_NONE = 0,
    KS_UP, KS_DOWN, KS_RIGHT, KS_LEFT,
    KS_HOME, KS_END, KS_INSERT, KS_DELETE, KS_PGUP, KS_PGDN,
    KS_F1, KS_F2, KS_F3, KS_F4, KS_F5, KS_F6, KS_F7, KS_F8, KS_F9, KS_F10, KS_F11, KS_F12,
    KS_COUNT
} keyseq_key_t;

#define KSMOD_SHIFT     1
#define KSMOD_ALT       2
#define KSMOD_CTRL      4
#define KSMOD_META      8           // the "super" / Windows / Command key
#define KSMOD_ALL       15

#define KEYSEQ_MAXLEN   16          // the longest sequence keyseq_make() writes (with the final zero byte)
#define KEYSEQ_NAMELEN  32          // enough for keyseq_name( "Ctrl+Alt+Shift+Meta+PageDown" )

// writes the canonical sequence of key with the modifiers (KSMOD_*) to buf (at least KEYSEQ_MAXLEN bytes, zero terminated);
// returns its length, 0 for an invalid key or modifiers
size_t keyseq_make( int key, int mods, char* buf );

// recognizes an escape sequence at the start of s (len bytes) in one of the dialects below. Returns the number of bytes it takes
// and sets *key and *mods; 0 if s does not start with a known sequence; -1 if s is the beginning of one (more bytes needed).
//     ESC [ A B C D H F, ESC [ 1 ; m A ...            xterm, VT220, rxvt, linux console: cursor keys, Home, End
//     ESC O A B C D H F, ESC O m A ...                the same in application mode (SS3)
//     ESC [ 1 ~ 7 ~ (Home)  4 ~ 8 ~ (End)  2 ~  3 ~  5 ~  6 ~  with ";m"
//     ESC [ 11 ~ 12 ~ 13 ~ 14 ~ 15 ~ 17 ~ 18 ~ 19 ~ 20 ~ 21 ~ 23 ~ 24 ~   F1..F12 (11..14 = F1..F4 as the VT220 and rxvt send them)
//     ESC O P Q R S, ESC [ P Q R S, ESC [ 1 ; m P ...   F1..F4
//     ESC [ [ A B C D E                                 F1..F5 of the linux console
int keyseq_parse( const char* s, size_t len, int* key, int* mods );

// the name of a key: "Up", "PageDown", "F5" ("" for KS_NONE)
const char* keyseq_keyname( int key );

// "Ctrl+Shift+Up": the key with its modifiers in a fixed order (Ctrl, Alt, Shift, Meta); buf is at least KEYSEQ_NAMELEN bytes
void keyseq_name( int key, int mods, char* buf );

// for the key dump (--debug-keys): len bytes of s as hex ("1b 5b 41") and as a C style string with \e for ESC ("\e[A"); out gets at most cap bytes
void keyseq_hex( const char* s, size_t len, char* out, size_t cap );
void keyseq_escape( const char* s, size_t len, char* out, size_t cap );

#endif
