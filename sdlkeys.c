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



#include "sdlkeys.h"

bool sdlkeys_map( SDL_Keycode sym, Uint16 sdlmods, int* key, int* mods ) {
    int k = KS_NONE;
    bool numlock = ( sdlmods & KMOD_NUM ) != 0;
    switch ( sym ) {
        case SDLK_UP:           k = KS_UP;      break;
        case SDLK_DOWN:         k = KS_DOWN;    break;
        case SDLK_RIGHT:        k = KS_RIGHT;   break;
        case SDLK_LEFT:         k = KS_LEFT;    break;
        case SDLK_HOME:         k = KS_HOME;    break;
        case SDLK_END:          k = KS_END;     break;
        case SDLK_INSERT:       k = KS_INSERT;  break;
        case SDLK_DELETE:       k = KS_DELETE;  break;
        case SDLK_PAGEUP:       k = KS_PGUP;    break;
        case SDLK_PAGEDOWN:     k = KS_PGDN;    break;
        case SDLK_F1:           k = KS_F1;      break;
        case SDLK_F2:           k = KS_F2;      break;
        case SDLK_F3:           k = KS_F3;      break;
        case SDLK_F4:           k = KS_F4;      break;
        case SDLK_F5:           k = KS_F5;      break;
        case SDLK_F6:           k = KS_F6;      break;
        case SDLK_F7:           k = KS_F7;      break;
        case SDLK_F8:           k = KS_F8;      break;
        case SDLK_F9:           k = KS_F9;      break;
        case SDLK_F10:          k = KS_F10;     break;
        case SDLK_F11:          k = KS_F11;     break;
        case SDLK_F12:          k = KS_F12;     break;
        case SDLK_TAB:          k = ( sdlmods & KMOD_SHIFT ) != 0 ? KS_BACKTAB : KS_NONE; break;      // Tab itself is the one byte 9
        case SDLK_KP_8:         k = numlock ? KS_NONE : KS_UP;      break;
        case SDLK_KP_2:         k = numlock ? KS_NONE : KS_DOWN;    break;
        case SDLK_KP_6:         k = numlock ? KS_NONE : KS_RIGHT;   break;
        case SDLK_KP_4:         k = numlock ? KS_NONE : KS_LEFT;    break;
        case SDLK_KP_7:         k = numlock ? KS_NONE : KS_HOME;    break;
        case SDLK_KP_1:         k = numlock ? KS_NONE : KS_END;     break;
        case SDLK_KP_0:         k = numlock ? KS_NONE : KS_INSERT;  break;
        case SDLK_KP_PERIOD:    k = numlock ? KS_NONE : KS_DELETE;  break;
        case SDLK_KP_9:         k = numlock ? KS_NONE : KS_PGUP;    break;
        case SDLK_KP_3:         k = numlock ? KS_NONE : KS_PGDN;    break;
        default:
            break;
    }
    if ( k == KS_NONE ) {
        return false;
    }
    int m = 0;
    if ( ( sdlmods & KMOD_SHIFT ) != 0 && k != KS_BACKTAB ) m |= KSMOD_SHIFT;       // (the Shift is in the key)
    if ( ( sdlmods & KMOD_ALT ) != 0 )   m |= KSMOD_ALT;
    if ( ( sdlmods & KMOD_CTRL ) != 0 )  m |= KSMOD_CTRL;
    if ( ( sdlmods & KMOD_GUI ) != 0 )   m |= KSMOD_META;
    *key = k; *mods = m;
    return true;
}

size_t sdlkeys_sequence( SDL_Keycode sym, Uint16 sdlmods, char* buf ) {
    int key, mods;
    if ( !sdlkeys_map( sym, sdlmods, &key, &mods ) ) {
        buf[0] = '\0';
        return 0;
    }
    return keyseq_make( key, mods, buf );
}
