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


#ifndef NOISE_H
#define NOISE_H     1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

/*
Hash and noise functions (roadmap item 95, docs/noise.md; no SDL, no global state: every function is a pure function of its arguments and gives the same
result on every machine; only integer arithmetic and + - * / of doubles are used, no sin / cos / pow from the C library).

nz_hash:    a number in [0, 1) from up to four integers (SplitMix64 finalizer, 53 bits)
nz_noise:   smooth gradient noise ("Perlin noise") in 1, 2 or 3 dimensions, in [-1, 1], 0 at every lattice point, period-free; the seed selects another noise
nz_ridged:  ridged fractal noise: 1 - |noise| squared per octave, the crests are sharp ridges (mountain ranges), 0 .. 1
nz_fbm:     fractal noise: the sum of octaves of 2 D noise, each with twice the frequency and half the amplitude (by default), normalized to [-1, 1]
*/

#define NZ_MAXOCTAVES   16

uint64_t nz_mix( uint64_t x );                                                  // the SplitMix64 finalizer (a bijection of 64 bit numbers)
uint64_t nz_hash64( uint64_t seed, int64_t a, int64_t b, int64_t c, int64_t d ); // 64 bit hash of the seed and four integers
double   nz_hash( int64_t a, int64_t b, int64_t c, int64_t d );                 // in [0, 1)
double   nz_noise( uint64_t seed, int dims, double x, double y, double z );     // dims 1 .. 3: the unused coordinates are ignored
double   nz_fbm( uint64_t seed, double x, double y, int octaves, double gain, double lacunarity );
double   nz_ridged( uint64_t seed, double x, double y, int octaves, double gain, double lacunarity );   // ridged noise: sharp crests, in [0, 1]

#endif
