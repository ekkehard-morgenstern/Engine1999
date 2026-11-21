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


#include "randseed.h"
#include "noise.h"

void rs_seed( randseed_t* tgt, uint64_t seed ) {
    uint64_t x = seed;
    for ( int i=0; i < RANDSEED_WORDS; ++i ) {
        x = nz_mix( x + UINT64_C(0x9e3779b97f4a7c15) );
        tgt->w[i] = x;
    }
    // the constants of the original (they are the starting points of the walks)
    tgt->sv[0] = UINT64_C(0xbb8dd8182615ddf3);
    tgt->sv[1] = UINT64_C(0x38d35226195ddb49);
    tgt->sv[2] = UINT64_C(0xadeb4f5fc2fe50ee);
    tgt->sv[3] = UINT64_C(0x096ed738a88b61c8);
    tgt->ev[0] = UINT64_C(0xe8497fc974fed9ea);
    tgt->ev[1] = UINT64_C(0x973672f11a2ddd91);
    tgt->ev[2] = UINT64_C(0xc19950208bc3a7cf);
    tgt->ev[3] = UINT64_C(0xd378563e9631242d);
}

void rs_south( randseed_t* tgt ) {
    uint64_t v1 = tgt->sv[0], v2 = tgt->sv[1], v3 = tgt->sv[2], v4 = tgt->sv[3];
    for ( int i=0; i < RANDSEED_WORDS; ++i ) {
        v1 = ( ( v2 + tgt->w[i] ) ^ v3 );
        v2 = ( ( v3 ^ v4 ) - v2 );
        v3 = ( ( v4 - v3 ) ^ v1 );
        v4 = ( ( v2 ^ v1 ) + v3 );
        tgt->w[i] = v1;
    }
    tgt->sv[0] = v1; tgt->sv[1] = v2; tgt->sv[2] = v3; tgt->sv[3] = v4;
}

void rs_east( randseed_t* tgt ) {
    uint64_t v1 = tgt->ev[0], v2 = tgt->ev[1], v3 = tgt->ev[2], v4 = tgt->ev[3];
    for ( int i=0; i < RANDSEED_WORDS; ++i ) {
        v1 = ( ( v2 ^ tgt->w[i] ) - v3 );
        v2 = ( ( v3 + v4 ) ^ v2 );
        v3 = ( ( v4 + v3 ) - v1 );
        v4 = ( ( v2 ^ v1 ) + v3 );
        tgt->w[i] = v1;
    }
    tgt->ev[0] = v1; tgt->ev[1] = v2; tgt->ev[2] = v3; tgt->ev[3] = v4;
}

uint64_t rs_fold( const randseed_t* src ) {
    uint64_t h = 0;
    for ( int i=0; i < RANDSEED_WORDS; ++i ) {
        h = nz_mix( h ^ src->w[i] );
    }
    return h;
}
