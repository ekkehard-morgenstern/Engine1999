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

bool cgen_gen_ins( codegen_t* cgen, uint8_t ins, uint8_t ext, uint16_t param ) {
    uint8_t size = UINT8_C(1);
    if ( ins & INSF_E ) ++size;
    if ( ins & INSF_P ) size += 2U;
    uint16_t offs = INS_NODATA;
    if ( !cgen_alloc_code( cgen, size, &offs ) ) {
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

bool cgen_gen_ins12( codegen_t* cgen, uint16_t ins12, bool code, bool hasparam, uint16_t param ) {
    uint8_t ins = UINT8_C(0), ext = UINT8_C(0);
    if ( ins12 < UINT16_C(16) ) {
        ins =   (uint8_t)  ins12;
    } else {
        ins = ( (uint8_t)( ins12 >> UINT8_C(8) ) & INSM_I ) | INSF_E;
        ext =   (uint8_t)  ins12;
    }
    if ( hasparam ) {
        ins |= INSF_P;
    }
    if ( code ) {
        ins |= INSF_C;
    }
    return cgen_gen_ins( cgen, ins, ext, param );
}

bool cgen_gen_ins12_imm17( codegen_t* cgen, uint16_t ins12, int32_t imm17 ) {
    uint16_t param = (uint16_t) imm17;
    bool     code  = imm17 & INT32_C(0X00010000) ? true : false;
    return cgen_gen_ins12( cgen, ins12, code, true, param );
}

bool cgen_gen_ins4_imm17( codegen_t* cgen, uint8_t ins, int32_t imm ) {
    return cgen_gen_ins12_imm17( cgen, ins & INSM_I, imm );
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

bool cgen_gen_phim( codegen_t* cgen, int32_t imm ) {
    return cgen_gen_ins4_imm17( cgen, INS_PHIM, imm );
}

bool cgen_gen_bria( codegen_t* cgen, int32_t abs_offs ) {
    return cgen_gen_ins4_imm17( cgen, INS_BRIA, abs_offs );
}

bool cgen_gen_brir( codegen_t* cgen, int32_t rel_offs ) {
    return cgen_gen_ins4_imm17( cgen, INS_BRIR, rel_offs );
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
