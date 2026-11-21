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

#pragma once
#ifndef VECEXT_H
#define VECEXT_H    1

/*
Compiler support for the vector code (m3dsoft.c: the span kernel of the software rasteriser, roadnet.c: rnet_near()). The vector code is written with the GCC /
clang vector extensions (vector_size, __builtin_convertvector), no intrinsics, so it builds for any target; where the compiler cannot do that, the scalar twin
of every vector function (which gives the same numbers bit for bit) is used and the vector code is not compiled.

    E99_VEC         1 when the vector code is compiled: clang 5 or newer, GCC 9 or newer (__builtin_convertvector); 0 otherwise.
                    -DE99_NOVEC forces 0 (to test the fallback).
    E99_NO_CONTRACT at the top of a file: no fused multiply-add contraction (so that the vector and the scalar code round alike on a CPU with FMA)
    E99_NO_PSABI    at the top of a file: no warning about 32 byte vectors in a build without AVX (the functions are static inline: the ABI does not matter)
*/

#if defined(E99_NOVEC)
#define E99_VEC 0
#elif defined(__clang__) && __clang_major__ >= 5
#define E99_VEC 1
#elif defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 9
#define E99_VEC 1
#else
#define E99_VEC 0
#endif

#if defined(__clang__)
#define E99_NO_CONTRACT _Pragma( "clang fp contract(off)" )
#define E99_NO_PSABI _Pragma( "clang diagnostic ignored \"-Wpsabi\"" )
#elif defined(__GNUC__)
#define E99_NO_CONTRACT _Pragma( "GCC optimize (\"fp-contract=off\")" )
#define E99_NO_PSABI _Pragma( "GCC diagnostic ignored \"-Wpsabi\"" )
#else
#define E99_NO_CONTRACT
#define E99_NO_PSABI
#endif

#endif
