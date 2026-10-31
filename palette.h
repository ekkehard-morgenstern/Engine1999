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



#ifndef PALETTE_H
#define PALETTE_H   1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

/*
The engine's 256 color palette as it is at the start (no SDL): the layers of the window start with these values (and `GFX PALETTE` changes
them there), the console host paints with them.
*/

uint32_t pal_argb( int index );                         // 0xAARRGGBB of an entry (0 outside 0..255)
const char* pal_title( int index );                     // its name ("Navy")
bool pal_find( const char* name, uint32_t* pargb );     // an entry by name (case does not matter)

#endif
