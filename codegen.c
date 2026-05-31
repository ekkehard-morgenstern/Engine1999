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

// special instructions

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

bool cgen_gen_lan_c( codegen_t* cgen ) {
    return cgen_gen_ins( cgen, INS_LAN | INSF_C, UINT8_C(0), UINT16_C(0) );
}

bool cgen_gen_lan_d( codegen_t* cgen ) {
    return cgen_gen_ins( cgen, INS_LAN, UINT8_C(0), UINT16_C(0) );
}

bool cgen_gen_lian_c( codegen_t* cgen ) {
    return cgen_gen_ins( cgen, INS_LIAN | INSF_C, UINT8_C(0), UINT16_C(0) );
}

bool cgen_gen_lian_d( codegen_t* cgen ) {
    return cgen_gen_ins( cgen, INS_LIAN, UINT8_C(0), UINT16_C(0) );
}

bool cgen_gen_luan_c( codegen_t* cgen ) {
    return cgen_gen_ins( cgen, INS_LUAN | INSF_C, UINT8_C(0), UINT16_C(0) );
}

bool cgen_gen_luan_d( codegen_t* cgen ) {
    return cgen_gen_ins( cgen, INS_LUAN, UINT8_C(0), UINT16_C(0) );
}

// arithmetical / logical instructions

bool cgen_gen_neg( codegen_t* cgen ) {
    return cgen_gen_ins12( cgen, INS_NEG, false, false, UINT16_C(0) );
}

bool cgen_gen_not( codegen_t* cgen ) {
    return cgen_gen_ins12( cgen, INS_NOT, false, false, UINT16_C(0) );
}

bool cgen_gen_lsh( codegen_t* cgen ) {
    return cgen_gen_ins12( cgen, INS_LSH, false, false, UINT16_C(0) );
}

bool cgen_gen_rsh( codegen_t* cgen ) {
    return cgen_gen_ins12( cgen, INS_RSH, false, false, UINT16_C(0) );
}

bool cgen_gen_add( codegen_t* cgen ) {
    return cgen_gen_ins12( cgen, INS_ADD, false, false, UINT16_C(0) );
}

bool cgen_gen_sub( codegen_t* cgen ) {
    return cgen_gen_ins12( cgen, INS_SUB, false, false, UINT16_C(0) );
}

bool cgen_gen_mul( codegen_t* cgen ) {
    return cgen_gen_ins12( cgen, INS_MUL, false, false, UINT16_C(0) );
}

bool cgen_gen_div( codegen_t* cgen ) {
    return cgen_gen_ins12( cgen, INS_DIV, false, false, UINT16_C(0) );
}

bool cgen_gen_and( codegen_t* cgen ) {
    return cgen_gen_ins12( cgen, INS_AND, false, false, UINT16_C(0) );
}

bool cgen_gen_nand( codegen_t* cgen ) {
    return cgen_gen_ins12( cgen, INS_NAND, false, false, UINT16_C(0) );
}

bool cgen_gen_or( codegen_t* cgen ) {
    return cgen_gen_ins12( cgen, INS_OR, false, false, UINT16_C(0) );
}

bool cgen_gen_nor( codegen_t* cgen ) {
    return cgen_gen_ins12( cgen, INS_NOR, false, false, UINT16_C(0) );
}

bool cgen_gen_xor( codegen_t* cgen ) {
    return cgen_gen_ins12( cgen, INS_XOR, false, false, UINT16_C(0) );
}

bool cgen_gen_xnor( codegen_t* cgen ) {
    return cgen_gen_ins12( cgen, INS_XNOR, false, false, UINT16_C(0) );
}

bool cgen_gen_pow( codegen_t* cgen ) {
    return cgen_gen_ins12( cgen, INS_POW, false, false, UINT16_C(0) );
}

bool cgen_gen_con( codegen_t* cgen ) {
    return cgen_gen_ins12( cgen, INS_CON, false, false, UINT16_C(0) );
}

bool cgen_gen_cneq( codegen_t* cgen ) {
    return cgen_gen_ins12( cgen, INS_CNEQ, false, false, UINT16_C(0) );
}

bool cgen_gen_cnne( codegen_t* cgen ) {
    return cgen_gen_ins12( cgen, INS_CNNE, false, false, UINT16_C(0) );
}

bool cgen_gen_cnge( codegen_t* cgen ) {
    return cgen_gen_ins12( cgen, INS_CNGE, false, false, UINT16_C(0) );
}

bool cgen_gen_cnle( codegen_t* cgen ) {
    return cgen_gen_ins12( cgen, INS_CNLE, false, false, UINT16_C(0) );
}

bool cgen_gen_cngt( codegen_t* cgen ) {
    return cgen_gen_ins12( cgen, INS_CNGT, false, false, UINT16_C(0) );
}

bool cgen_gen_cnlt( codegen_t* cgen ) {
    return cgen_gen_ins12( cgen, INS_CNLT, false, false, UINT16_C(0) );
}

bool cgen_gen_cseq( codegen_t* cgen ) {
    return cgen_gen_ins12( cgen, INS_CSEQ, false, false, UINT16_C(0) );
}

bool cgen_gen_csne( codegen_t* cgen ) {
    return cgen_gen_ins12( cgen, INS_CSNE, false, false, UINT16_C(0) );
}

bool cgen_gen_csge( codegen_t* cgen ) {
    return cgen_gen_ins12( cgen, INS_CSGE, false, false, UINT16_C(0) );
}

bool cgen_gen_csle( codegen_t* cgen ) {
    return cgen_gen_ins12( cgen, INS_CSLE, false, false, UINT16_C(0) );
}

bool cgen_gen_csgt( codegen_t* cgen ) {
    return cgen_gen_ins12( cgen, INS_CSGT, false, false, UINT16_C(0) );
}

bool cgen_gen_cslt( codegen_t* cgen ) {
    return cgen_gen_ins12( cgen, INS_CSLT, false, false, UINT16_C(0) );
}

// variable access instructions

bool cgen_gen_rrnv( codegen_t* cgen, uint16_t varoffs ) {
    return cgen_gen_ins12_imm17( cgen, INS_RRNV, (int32_t) varoffs );
}

bool cgen_gen_rriv( codegen_t* cgen, uint16_t varoffs ) {
    return cgen_gen_ins12_imm17( cgen, INS_RRIV, (int32_t) varoffs );
}

bool cgen_gen_rrsv( codegen_t* cgen, uint16_t varoffs ) {
    return cgen_gen_ins12_imm17( cgen, INS_RRSV, (int32_t) varoffs );
}

bool cgen_gen_rnae( codegen_t* cgen, uint16_t varoffs ) {
    return cgen_gen_ins12_imm17( cgen, INS_RNAE, (int32_t) varoffs );
}

bool cgen_gen_riae( codegen_t* cgen, uint16_t varoffs ) {
    return cgen_gen_ins12_imm17( cgen, INS_RIAE, (int32_t) varoffs );
}

bool cgen_gen_rsae( codegen_t* cgen, uint16_t varoffs ) {
    return cgen_gen_ins12_imm17( cgen, INS_RSAE, (int32_t) varoffs );
}

bool cgen_gen_wrnv( codegen_t* cgen, uint16_t varoffs ) {
    return cgen_gen_ins12_imm17( cgen, INS_WRNV, (int32_t) varoffs );
}

bool cgen_gen_wriv( codegen_t* cgen, uint16_t varoffs ) {
    return cgen_gen_ins12_imm17( cgen, INS_WRIV, (int32_t) varoffs );
}

bool cgen_gen_wrsv( codegen_t* cgen, uint16_t varoffs ) {
    return cgen_gen_ins12_imm17( cgen, INS_WRSV, (int32_t) varoffs );
}

bool cgen_gen_wnae( codegen_t* cgen, uint16_t varoffs ) {
    return cgen_gen_ins12_imm17( cgen, INS_WNAE, (int32_t) varoffs );
}

bool cgen_gen_wiae( codegen_t* cgen, uint16_t varoffs ) {
    return cgen_gen_ins12_imm17( cgen, INS_WIAE, (int32_t) varoffs );
}

bool cgen_gen_wsae( codegen_t* cgen, uint16_t varoffs ) {
    return cgen_gen_ins12_imm17( cgen, INS_WSAE, (int32_t) varoffs );
}
