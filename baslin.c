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

#include "baslin.h"
#include "bastok.h"

// -- linehdr_t -------------------------------------------------------------

void clear_linehdr( linehdr_t* hdr ) {
    hdr->lineno = LINENO_NONE;
    hdr->length = UINT16_C(0);
}

void emit_linehdr_raw( uint8_t** pp, const linehdr_t* source ) {
    emit_uint24( pp, source->lineno );
    emit_uint16( pp, source->length );
}

void read_linehdr_raw( const uint8_t** pp, linehdr_t* target ) {
    read_uint24( pp, &target->lineno );
    read_uint16( pp, &target->length );
}
