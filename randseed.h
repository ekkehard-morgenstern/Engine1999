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


#ifndef RANDSEED_H
#define RANDSEED_H  1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

/*
The two-dimensional 1024 bit random field of the owner's RANDPATH program (../randpath/randseed.[ch], Copyright (C) 1998, 2025 Ekkehard Morgenstern): a state of 1024 bits that
walks one step south (rs_south) or east (rs_east); the state at a place depends on the way there, so a whole field is made by walking row by row (docs/world-subsystem.md).
Changed from the original only by rs_seed() (a state made from a 64 bit number instead of /dev/urandom, so a world is reproducible) and rs_fold().
*/

#define RANDSEED_WORDS  16  /* 1024 bit */

typedef struct _randseed_t {
    uint64_t    w[RANDSEED_WORDS], sv[4], ev[4];
} randseed_t;

void     rs_seed( randseed_t* tgt, uint64_t seed );             // the state at the origin of the field of this seed
void     rs_south( randseed_t* tgt );
void     rs_east( randseed_t* tgt );
uint64_t rs_fold( const randseed_t* src );                      // the 1024 bits of a state folded into 64

#endif
