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

#include "codegen.h"
#include "instset.h"

void init_codegen( codegen_t* cgen ) {
    cgen->report   = 0;
    cgen->halt     = 0;
    cgen->userdata = 0;
    cgen->codesize = UINT16_C(0);
}

bool cgen_alloc_code( codegen_t* cgen, uint16_t size, uint16_t* poffs ) {
    if ( size > CODESIZE_MAX - cgen->codesize ) {
        return false;
    }
    *poffs = (uint16_t) cgen->codesize;
    cgen->codesize += size;
    return true;
}

static bool cgen_gen_ins( codegen_t* cgen, uint8_t ins, uint8_t ext, uint16_t param ) {
    uint8_t size = UINT8_C(1);
    if ( ins & INSF_E ) ++size;
    if ( ins & INSF_P ) size += 2U;
    uint16_t offs = INS_NODATA;
    if ( !cgen_alloc_code( cgen, UINT16_C(1), &offs ) ) {
        return false;
    }
    cgen->code[ offs++ ] = ins;
    if ( ins & INSF_E ) {
        cgen->code[ offs++ ] = ext;
    }
    if ( ins & INSF_P ) {
        cgen->code[ offs++ ] = (uint8_t)( param >> UINT8_C(8) );
        cgen->code[ offs++ ] = (uint8_t)  param;
    }
    return true;
}

bool cgen_gen_brk( codegen_t* cgen ) {
    return cgen_gen_ins( cgen, INS_BRK, UINT8_C(0), UINT16_C(0) );
}

bool cgen_gen_nop( codegen_t* cgen ) {
    return cgen_gen_ins( cgen, INS_NOP, UINT8_C(0), UINT16_C(0) );
}

bool cgen_gen_phpa_c( codegen_t* cgen, uint16_t offs ) {
    return cgen_gen_ins( cgen, INS_PHPA_C | INSF_P, UINT8_C(0), offs );
}

bool cgen_gen_phpa_d( codegen_t* cgen, uint16_t offs ) {
    return cgen_gen_ins( cgen, INS_PHPA_D | INSF_P, UINT8_C(0), offs );
}

static bool cgen_gen_imm_ins( codegen_t* cgen, uint8_t ins, int32_t imm ) {
    uint8_t  c = (uint8_t)( ( imm & INT32_C(0X00010000) ) >> UINT8_C(16) );
    uint16_t o = (uint16_t) imm;
    return cgen_gen_ins( cgen, ins | INSF_P | MKINS_C(c), UINT8_C(0), o );
}

bool cgen_gen_phim( codegen_t* cgen, int32_t imm ) {
    return cgen_gen_imm_ins( cgen, INS_PHIM, imm );
}

bool cgen_gen_bria( codegen_t* cgen, int32_t abs_offs ) {
    return cgen_gen_imm_ins( cgen, INS_BRIA, abs_offs );
}

bool cgen_gen_brir( codegen_t* cgen, int32_t rel_offs ) {
    return cgen_gen_imm_ins( cgen, INS_BRIR, rel_offs );
}

bool cgen_gen_jpcc( codegen_t* cgen ) {
    return cgen_gen_ins( cgen, INS_JPCC, UINT8_C(0), UINT16_C(0) );
}

bool cgen_gen_jump( codegen_t* cgen ) {
    return cgen_gen_ins( cgen, INS_JUMP, UINT8_C(0), UINT16_C(0) );
}

bool cgen_gen_drop( codegen_t* cgen, uint16_t cnt ) {
    if ( cnt == UINT16_C(0) ) {
        return true;
    }
    if ( cnt == UINT16_C(1) ) {
        return cgen_gen_ins( cgen, INS_DROP, UINT8_C(0), UINT16_C(0) );
    }
    return cgen_gen_ins( cgen, INS_DROP | INSF_P, UINT8_C(0), cnt );
}

bool cgen_gen_line( codegen_t* cgen, uint16_t line ) {
    return cgen_gen_ins( cgen, INS_LINE | INSF_P, UINT8_C(0), line );
}

bool cgen_gen_exp_ins( codegen_t* cgen, uint16_t ins ) {
    uint8_t i = (uint8_t)( ( ins & UINT16_C(0X0F00) ) >> UINT8_C(8) );
    uint8_t e = (uint8_t) ins;
    return cgen_gen_ins( cgen, MKINS_I(i) | INSF_E, e, UINT16_C(0) );
}
