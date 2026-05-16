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

#ifndef RUNTIME_H
#define RUNTIME_H   1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

#ifndef VARS_H
#include "vars.h"
#endif

#ifndef CODEGEN_H
#include "codegen.h"
#endif

typedef struct _runtime_t {
    varmem_t    varmem;
    codegen_t   codegen;
    void*       userdata;
    void        (*report)( struct _runtime_t*, void*, const char* );
    void        (*halt)( struct _runtime_t*, void* ) ATTR_NORETURN;
} runtime_t;

void init_runtime( runtime_t* rt );

#endif
