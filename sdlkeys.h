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



#ifndef SDLKEYS_H
#define SDLKEYS_H   1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

#include <SDL2/SDL.h>
#include "keyseq.h"

/*
The window host's keys that are not one byte, from SDL key codes. Needs the SDL headers only (no SDL function is called), so it can be tested
without a display.
*/

// the key (KS_*) and the modifiers (KSMOD_*) of an SDL key press: false for keys that have no sequence. Keypad keys count as the cursor
// block when Num Lock is off. SDL's KMOD_GUI is KSMOD_META; Caps Lock, Num Lock and AltGr (KMOD_MODE) are not modifiers here.
bool sdlkeys_map( SDL_Keycode sym, Uint16 sdlmods, int* key, int* mods );

// the canonical sequence of an SDL key press in buf (at least KEYSEQ_MAXLEN bytes); returns its length, 0 if the key has none
size_t sdlkeys_sequence( SDL_Keycode sym, Uint16 sdlmods, char* buf );

#endif
