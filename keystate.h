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

#ifndef KEYSTATE_H
#define KEYSTATE_H  1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

#ifndef KEYSEQ_H
#include "keyseq.h"
#endif

/*
The names of the keys for KEY("name") (no SDL, no terminal): "A".."Z", "0".."9" and the other printable keys as the character they type ("," "." "/" ...),
"SPACE", "RETURN" ("ENTER"), "ESC", "TAB", "BACKSPACE", "UP" "DOWN" "LEFT" "RIGHT", "HOME" "END" "INSERT" "DELETE" "PGUP" "PGDN", "F1".."F12", "SHIFT" "LSHIFT"
"RSHIFT", "CTRL" "LCTRL" "RCTRL", "ALT" "LALT" "RALT", the keypad "KP0".."KP9" "KPENTER" "KP+" "KP-" "KP*" "KP/" "KP.", and "ANY" (any key at all).
Case does not matter. A name becomes a key id; the hosts map the id to what they know (the window to an SDL scancode, the console host to the last time the
terminal sent the key).
*/

#define KEYID_ANY       1
// 32..126: a key that types this character (letters in lower case)
#define KEYID_ESC       256
#define KEYID_RETURN    257
#define KEYID_TAB       258
#define KEYID_BACKSPACE 259
#define KEYID_SHIFT     260
#define KEYID_LSHIFT    261
#define KEYID_RSHIFT    262
#define KEYID_CTRL      263
#define KEYID_LCTRL     264
#define KEYID_RCTRL     265
#define KEYID_ALT       266
#define KEYID_LALT      267
#define KEYID_RALT      268
#define KEYID_KP0       280     // .. KEYID_KP0 + 9
#define KEYID_KPENTER   290
#define KEYID_KPPLUS    291
#define KEYID_KPMINUS   292
#define KEYID_KPMUL     293
#define KEYID_KPDIV     294
#define KEYID_KPDOT     295
#define KEYID_KS        512     // + a keyseq_key_t: the cursor keys, HOME .. PGDN, F1 .. F12
#define KEYID_MAX       (KEYID_KS + KS_COUNT)

// The key events (ON KEY, KEY$("EVENT")): the host that knows when a key goes down and up (the window) pushes them, the program reads them. A ring of KEYEV_SIZE events,
// one producer (the screen thread) and one consumer (the program), atomic indices, no locks; when the ring is full the new event is dropped (a handler that
// reads them as they come never loses one).
#define KEYEV_SIZE      64
void     keystate_event_push( int id, bool down );      // the producer
bool     keystate_event_pop( int* id, bool* down );     // the consumer: the oldest event, false if there is none
uint32_t keystate_event_count( void );                  // the events pushed so far (wraps at 2^31; ON KEY looks for a change)
void     keystate_event_clear( void );                  // the consumer forgets what is waiting (a run starts)
const char* keystate_name( int id );                    // the canonical name of a key id ("A", "SPACE", "LEFT", "F5", "LSHIFT", "KP5" ...), "?" if there is none

int keystate_id( const char* name, size_t len );        // the id of a key name, -1 if it is none
int keystate_ks( int id );                              // the keyseq_key_t of an id of the KEYID_KS range, KS_NONE for any other

#endif
