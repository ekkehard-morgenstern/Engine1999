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

#ifndef SDLUTIL_H
#define SDLUTIL_H   1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

uint64_t sdlutil_getnsec( struct timespec* pts );
int sdlutil_comparetime( const struct timespec* a, const struct timespec* b );
void sdlutil_projecttime( uint64_t nsec, const struct timespec* from, struct timespec* to );
void sdlutil_nanosleep( uint64_t nsec, const struct timespec* pts );

#endif
