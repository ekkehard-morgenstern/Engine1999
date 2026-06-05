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
    cgen->datasize = UINT16_C(0);
}

bool cgen_alloc_code( codegen_t* cgen, uint16_t size, uint16_t* poffs ) {
    if ( size > CODESIZE_MAX - cgen->codesize ) {
        return false;
    }
    *poffs = (uint16_t) cgen->codesize;
    cgen->codesize += size;
    return true;
}

bool cgen_alloc_data( codegen_t* cgen, uint16_t size, uint16_t* poffs ) {
    if ( size > DATASIZE_MAX - cgen->datasize ) {
        return false;
    }
    *poffs = (uint16_t) cgen->datasize;
    cgen->datasize += size;
    return true;
}

static const char* opcode_to_string( uint16_t opcode ) {
    switch ( opcode ) {
        case INS_BRK:       return "BRK";
        case INS_NOP:       return "NOP";
        case INS_PHPA:      return "PHPA";
        case INS_PHIM:      return "PHIM";
        case INS_BRIA:      return "BRIA";
        case INS_BRIR:      return "BRIR";
        case INS_JPCC:      return "JPCC";
        case INS_JUMP:      return "JUMP";
        case INS_DROP:      return "DROP";
        case INS_LINE:      return "LINE";
        case INS_LAN:       return "LAN";
        case INS_LIAN:      return "LIAN";
        case INS_LUAN:      return "LUAN";
        case INS_LAS:       return "LAS";
        case INS_FRES:      return "FRES";
        case INS_NEG:       return "NEG";
        case INS_NOT:       return "NOT";
        case INS_LSH:       return "LSH";
        case INS_RSH:       return "RSH";
        case INS_ADD:       return "ADD";
        case INS_SUB:       return "SUB";
        case INS_MUL:       return "MUL";
        case INS_DIV:       return "DIV";
        case INS_AND:       return "AND";
        case INS_NAND:      return "NAND";
        case INS_OR:        return "OR";
        case INS_NOR:       return "NOR";
        case INS_XOR:       return "XOR";
        case INS_XNOR:      return "XNOR";
        case INS_POW:       return "POW";
        case INS_CON:       return "CON";
        case INS_CNEQ:      return "CNEQ";
        case INS_CNNE:      return "CNNE";
        case INS_CNGE:      return "CNGE";
        case INS_CNLE:      return "CNLE";
        case INS_CNGT:      return "CNGT";
        case INS_CNLT:      return "CNLT";
        case INS_CSEQ:      return "CSEQ";
        case INS_CSNE:      return "CSNE";
        case INS_CSGE:      return "CSGE";
        case INS_CSLE:      return "CSLE";
        case INS_CSGT:      return "CSGT";
        case INS_CSLT:      return "CSLT";
        case INS_RRNV:      return "RRNV";
        case INS_RRIV:      return "RRIV";
        case INS_RRSV:      return "RRSV";
        case INS_RRLV:      return "RRLV";
        case INS_RNAE:      return "RNAE";
        case INS_RIAE:      return "RIAE";
        case INS_RSAE:      return "RSAE";
        case INS_WRNV:      return "WRNV";
        case INS_WRIV:      return "WRIV";
        case INS_WRSV:      return "WRSV";
        case INS_WRLV:      return "WRLV";
        case INS_WNAE:      return "WNAE";
        case INS_WIAE:      return "WIAE";
        case INS_WSAE:      return "WSAE";
        case INS_SHEX:      return "SHEX";
        case INS_CALF:      return "CALF";
        case INS_RETF:      return "RETF";
        case INS_GADC:      return "GADC";
        case INS_GFAC:      return "GFAC";
        case INS_TI:        return "TI";
        case INS_INKY:      return "INKY";
        case INS_ASC:       return "ASC";
        case INS_VAL:       return "VAL";
        case INS_FRE:       return "FRE";
        case INS_LEN:       return "LEN";
        case INS_BINS:      return "BINS";
        case INS_QUAS:      return "QUAS";
        case INS_OCTS:      return "OCTS";
        case INS_DECS:      return "DECS";
        case INS_HEXS:      return "HEXS";
        case INS_STRS:      return "STRS";
        case INS_LFTS:      return "LFTS";
        case INS_RGTS:      return "RGTS";
        case INS_MIDS:      return "MIDS";
        default:            break;
    }
    return "???";
}

bool cgen_has_cd( uint16_t opcode ) {
    switch ( opcode ) {
        case INS_PHPA: case INS_DROP: case INS_LAN: case INS_LIAN: case INS_LUAN: case INS_LAS:
            return true;
        default:
            break;
    }
    return false;
}

void cgen_disasm_ins( const uint8_t* area, uint16_t* poffs, char linebuf[80] ) {
    uint16_t offs = *poffs;       char offhex[5];
    snprintf( offhex, 5U, "%04" PRIX16, offs );

    uint8_t ins = area[ offs++ ]; char inshex[3];
    snprintf( inshex, 3U, "%02" PRIX8, ins );

    uint8_t ext = UINT8_C(0);     char exthex[3];
    uint16_t param = UINT16_C(0); char parhex[5];
    memset( exthex, ' ', 2U ); exthex[2] = '\0';
    memset( parhex, ' ', 4U ); parhex[4] = '\0';

    if ( ins & INSF_E ) {
        ext = area[ offs++ ];
        snprintf( exthex, 3U, "%02" PRIX8, ext );
    }
    if ( ins & INSF_P ) {
        param = ( ( (uint16_t) area[ offs               ] ) << UINT8_C(8) ) |
                               area[ offs + UINT16_C(1) ];
        offs += UINT16_C(2);
        snprintf( parhex, 5U, "%04" PRIX16, param );
    }
    bool code = ( ins & INSF_C ) ? true : false;
    uint16_t opcode = ins & INSM_I;

    if ( ins & INSF_E ) {
        opcode = ( opcode << UINT8_C(8) ) | ext;
    }
    const char* opstr = opcode_to_string( opcode );
    const char* option = "  ";
    if ( cgen_has_cd( opcode ) ) {
        option = code ? ".C" : ".D";
    }
    char opcstr[10]; snprintf( opcstr, 10U, "%s%s", opstr, option );

    // 0000000000111111111122222222223333333333
    // 0123456789012345678901234567890123456789
    // 0000: 00 00 0000    AAAA.O  0000
    snprintf( linebuf, 80U, "%s: %s %s %s    %-7s %s", offhex, inshex, exthex, parhex, opcstr, parhex );

    *poffs = offs;
}

void cgen_print_ins( const uint8_t* area, uint16_t* poffs ) {
    static char linebuf[80];
    cgen_disasm_ins( area, poffs, linebuf );
    printf( "%s\n", linebuf );
}

bool cgen_gen_ins( codegen_t* cgen, uint8_t ins, uint8_t ext, uint16_t param ) {
    uint8_t size = UINT8_C(1);
    if ( ins & INSF_E ) ++size;
    if ( ins & INSF_P ) size += 2U;
    uint16_t offs = INS_NODATA;
    if ( !cgen_alloc_code( cgen, size, &offs ) ) {
        return false;
    }
    uint16_t offs0 = offs;
    cgen->code[ offs++ ] = ins;
    if ( ins & INSF_E ) {
        cgen->code[ offs++ ] = ext;
    }
    if ( ins & INSF_P ) {
        cgen->code[ offs++ ] = (uint8_t)( param >> UINT8_C(8) );
        cgen->code[ offs++ ] = (uint8_t)  param;
    }
    offs = offs0;
cgen_print_ins( cgen->code, &offs );
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
    return cgen_gen_ins( cgen, INS_JPCC | INSF_C, UINT8_C(0), UINT16_C(0) );
}

bool cgen_gen_jump( codegen_t* cgen ) {
    return cgen_gen_ins( cgen, INS_JUMP | INSF_C, UINT8_C(0), UINT16_C(0) );
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

bool cgen_gen_las_c( codegen_t* cgen ) {
    return cgen_gen_ins( cgen, INS_LAS | INSF_C, UINT8_C(0), UINT16_C(0) );
}

bool cgen_gen_las_d( codegen_t* cgen ) {
    return cgen_gen_ins( cgen, INS_LAS, UINT8_C(0), UINT16_C(0) );
}

bool cgen_gen_fres( codegen_t* cgen ) {
    return cgen_gen_ins( cgen, INS_FRES, UINT8_C(0), UINT16_C(0) );
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

bool cgen_gen_rrlv( codegen_t* cgen, uint16_t varoffs ) {
    return cgen_gen_ins12( cgen, INS_RRLV, true, true, varoffs );
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

bool cgen_gen_wrlv( codegen_t* cgen, uint16_t varoffs ) {
    return cgen_gen_ins12( cgen, INS_WRLV, true, true, varoffs );
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

bool cgen_gen_shex( codegen_t* cgen ) {
    return cgen_gen_ins12( cgen, INS_SHEX, false, false, UINT16_C(0) );
}

bool cgen_gen_calf( codegen_t* cgen, uint16_t varoffs ) {
    return cgen_gen_ins12_imm17( cgen, INS_CALF, (int32_t) varoffs );
}

bool cgen_gen_retf( codegen_t* cgen ) {
    return cgen_gen_ins12( cgen, INS_RETF, false, false, UINT16_C(0) );
}

bool cgen_gen_gadc( codegen_t* cgen, uint16_t varoffs ) {
    return cgen_gen_ins12_imm17( cgen, INS_GADC, (int32_t) varoffs );
}

bool cgen_gen_gfac( codegen_t* cgen, uint16_t varoffs ) {
    return cgen_gen_ins12_imm17( cgen, INS_GFAC, (int32_t) varoffs );
}

// BASIC system function implementations

bool cgen_gen_ti( codegen_t* cgen ) {
    return cgen_gen_ins12( cgen, INS_TI, false, false, UINT16_C(0) );
}

bool cgen_gen_inky( codegen_t* cgen ) {
    return cgen_gen_ins12( cgen, INS_INKY, false, false, UINT16_C(0) );
}

bool cgen_gen_asc( codegen_t* cgen ) {
    return cgen_gen_ins12( cgen, INS_ASC, false, false, UINT16_C(0) );
}

bool cgen_gen_val( codegen_t* cgen ) {
    return cgen_gen_ins12( cgen, INS_VAL, false, false, UINT16_C(0) );
}

bool cgen_gen_fre( codegen_t* cgen ) {
    return cgen_gen_ins12( cgen, INS_FRE, false, false, UINT16_C(0) );
}

bool cgen_gen_len( codegen_t* cgen ) {
    return cgen_gen_ins12( cgen, INS_LEN, false, false, UINT16_C(0) );
}

bool cgen_gen_bins( codegen_t* cgen ) {
    return cgen_gen_ins12( cgen, INS_BINS, false, false, UINT16_C(0) );
}

bool cgen_gen_quas( codegen_t* cgen ) {
    return cgen_gen_ins12( cgen, INS_QUAS, false, false, UINT16_C(0) );
}

bool cgen_gen_octs( codegen_t* cgen ) {
    return cgen_gen_ins12( cgen, INS_OCTS, false, false, UINT16_C(0) );
}

bool cgen_gen_hexs( codegen_t* cgen ) {
    return cgen_gen_ins12( cgen, INS_HEXS, false, false, UINT16_C(0) );
}

bool cgen_gen_decs( codegen_t* cgen ) {
    return cgen_gen_ins12( cgen, INS_DECS, false, false, UINT16_C(0) );
}

bool cgen_gen_strs( codegen_t* cgen ) {
    return cgen_gen_ins12( cgen, INS_STRS, false, false, UINT16_C(0) );
}

bool cgen_gen_lfts( codegen_t* cgen ) {
    return cgen_gen_ins12( cgen, INS_LFTS, false, false, UINT16_C(0) );
}

bool cgen_gen_rgts( codegen_t* cgen ) {
    return cgen_gen_ins12( cgen, INS_RGTS, false, false, UINT16_C(0) );
}

bool cgen_gen_mids( codegen_t* cgen ) {
    return cgen_gen_ins12( cgen, INS_MIDS, false, false, UINT16_C(0) );
}

// generate code from syntax tree nodes

static bool cgen_bad_node( compiler_t* comp ) {
    comp_error( comp, "Internal error (bad node)" );
    return false;
}

static bool cgen_unexpected_node( compiler_t* comp ) {
    comp_error( comp, "Internal error (unexpected node)" );
    return false;
}

static bool cgen_out_of_code_memory( compiler_t* comp ) {
    comp_error( comp, "Out of code memory" );
    return false;
}

static bool cgen_out_of_data_memory( compiler_t* comp ) {
    comp_error( comp, "Out of data memory" );
    return false;
}

static bool cgen_not_implemented_yet( compiler_t* comp ) {
    comp_error( comp, "Internal error (feature not implemented yet)" );
    return false;
}

bool cgen_from_numlit( codegen_t* cgen, compiler_t* comp, uint16_t nodeoffs ) {
    /*
        NT_NUMLIT       numeric literal
            data:
                - numeric value, 8 bytes in network byte order
    */
    //  <nodetype.8> <numbranches.8> <datalen.16> <firstbranch.16> <lastbranch.16> <data...>
    if ( nodeoffs == NODEOFFS_NONE ) {
        return cgen_bad_node( comp );
    }
    uint8_t nodetype = comp->tree[ nodeoffs ];
    if ( nodetype != NT_NUMLIT ) {
UNEXP:  return cgen_unexpected_node( comp );
    }
    uint16_t datalen = EXTRACT16( comp, nodeoffs + 2U );
    if ( datalen != UINT16_C(8) ) {
        goto UNEXP;
    }

    uint16_t dataoffs = DATAOFFS_NONE;
    if ( !cgen_alloc_data( cgen, UINT16_C(8), &dataoffs ) || dataoffs == DATAOFFS_NONE ) {
        return cgen_out_of_data_memory( comp );
    }
    memcpy( &cgen->data[ dataoffs ], &comp->tree[ nodeoffs + 8U ], 8U );

    // generate code to push the data address on the stack and then code to read the data field as a number
    if ( !cgen_gen_phpa_d( cgen, dataoffs ) || !cgen_gen_lan_d( cgen ) ) {
        return cgen_out_of_code_memory( comp );
    }
    return true;
}

bool cgen_from_strlit( codegen_t* cgen, compiler_t* comp, uint16_t nodeoffs ) {
    /*
        NT_STRLIT       string literal
            data:
                - 1 byte of type indicator (can be string, shell, bracket or brace literal)
                - n bytes of text
            branches: none
            note:
                - note that shell/bracket/brace literals aren't evaluated here, just gathered.
    */
    //  <nodetype.8> <numbranches.8> <datalen.16> <firstbranch.16> <lastbranch.16> <data...>
    if ( nodeoffs == NODEOFFS_NONE ) {
        return cgen_bad_node( comp );
    }
    uint8_t nodetype = comp->tree[ nodeoffs ];
    if ( nodetype != NT_STRLIT ) {
UNEXP:  return cgen_unexpected_node( comp );
    }
    uint16_t datalen = EXTRACT16( comp, nodeoffs + 2U );
    if ( datalen < 1U ) {
        goto UNEXP;
    }
    nodeoffs += 8U;
    uint8_t datatype = comp->tree[ nodeoffs++ ]; --datalen;
    uint16_t dataoffs = DATAOFFS_NONE;
    if ( !cgen_alloc_data( cgen, datalen + UINT16_C(1), &dataoffs ) || dataoffs == DATAOFFS_NONE ) {
        return cgen_out_of_data_memory( comp );
    }
    if ( datalen ) {
        memcpy( &cgen->data[ dataoffs ], &comp->tree[ nodeoffs ], datalen );
    }
    cgen->data[ dataoffs + datalen ] = '\0';
    switch ( datatype ) {
        case TOK_STRLIT:
            // generate code to push the data address on the stack then code to load it as string pointer
            if ( !cgen_gen_phpa_d( cgen, dataoffs ) || !cgen_gen_las_d( cgen ) ) {
                return cgen_out_of_code_memory( comp );
            }
            break;
        case TOK_SHLLIT:
            // generate code to push the data address on the stack then code to load it as string pointer and execute it
            if ( !cgen_gen_phpa_d( cgen, dataoffs ) || !cgen_gen_las_d( cgen ) || !cgen_gen_shex( cgen ) ) {
                return cgen_out_of_code_memory( comp );
            }
            break;
        default: // TOK_BRKLIT TOK_BRCLIT
            // NOTE: TOK_BRKLIT is supposed to be the inline assembler -- TODO make this work!
            //       TOK_BRCLIT doesn't have any purpose yet -- TODO define!
            return cgen_not_implemented_yet( comp );
    }
    return true;
}

typedef struct _cbdata_t {
    codegen_t*  cgen;
    compiler_t* comp;
    uint16_t    nodeoffs;   // effectively, the parent node
    uint16_t    count;      // branch counter
} cbdata_t;

static bool from_strlits_cb( void* param, uint16_t nodeoffs ) {
    cbdata_t* pdata = (cbdata_t*) param;
    // put the current string literal on the data stack
    if ( !cgen_from_strlit( pdata->cgen, pdata->comp, nodeoffs ) ) {
        return false;
    }
    if ( ++pdata->count >= UINT16_C(2) ) {
        // generate CON (concat) instructions after 2 string literals and every new string literal
        if ( !cgen_gen_con( pdata->cgen ) ) {
            return cgen_out_of_code_memory( pdata->comp );
        }
    }
    return true;
}

bool cgen_from_strlits( codegen_t* cgen, compiler_t* comp, uint16_t nodeoffs ) {
    //  <nodetype.8> <numbranches.8> <datalen.16> <firstbranch.16> <lastbranch.16> <data...>
    if ( nodeoffs == NODEOFFS_NONE ) {
        return cgen_bad_node( comp );
    }
    uint8_t nodetype = comp->tree[ nodeoffs ];
    if ( nodetype != NT_STRLITS ) {
        return cgen_unexpected_node( comp );
    }
    cbdata_t cbdata = { cgen, comp, nodeoffs, UINT16_C(0) };
    return comp_node_iter_branches( comp, nodeoffs, &cbdata, from_strlits_cb );
}

static bool from_arraysub_cb( void* param, uint16_t nodeoffs ) {
    cbdata_t* pdata = (cbdata_t*) param;
    //  <nodetype.8> <numbranches.8> <datalen.16> <firstbranch.16> <lastbranch.16> <data...>
    if ( nodeoffs == NODEOFFS_NONE ) {
        return cgen_bad_node( pdata->comp );
    }
    uint8_t nodetype = pdata->comp->tree[ nodeoffs ];
    if ( nodetype == NT_NUMEXLIST ) {
        return comp_node_iter_branches( pdata->comp, nodeoffs, pdata, from_arraysub_cb );
    }
    ++pdata->count;
    return cgen_from_numexpr( pdata->cgen, pdata->comp, nodeoffs );
}

static bool gen_from_arraysub( codegen_t* cgen, compiler_t* comp, uint16_t nodeoffs ) {
    cbdata_t cbdata = { cgen, comp, nodeoffs, UINT16_C(0) };
    // output code for index expressions
    if ( !comp_node_iter_branches( comp, nodeoffs, &cbdata, from_arraysub_cb ) ) {
        comp_error( comp, "Internal error (failed to generate code for array subscripts)" );
        return false;
    }
    // output code to put number of expressions on stack
    if ( !cgen_gen_phim( cgen, cbdata.count ) ) {
        return cgen_out_of_code_memory( comp );
    }
    return true;
}

static bool from_fncall_cb( void* param, uint16_t nodeoffs ) {
    cbdata_t* pdata = (cbdata_t*) param;
    //  <nodetype.8> <numbranches.8> <datalen.16> <firstbranch.16> <lastbranch.16> <data...>
    if ( nodeoffs == NODEOFFS_NONE ) {
        return cgen_bad_node( pdata->comp );
    }
    uint8_t nodetype = pdata->comp->tree[ nodeoffs ];
    if ( nodetype == NT_EXPRLIST ) {
        return comp_node_iter_branches( pdata->comp, nodeoffs, pdata, from_fncall_cb );
    }
    ++pdata->count;
    return cgen_from_expr( pdata->cgen, pdata->comp, nodeoffs );
}

static bool gen_from_usrfncall( codegen_t* cgen, compiler_t* comp, uint16_t nodeoffs ) {
    cbdata_t cbdata = { cgen, comp, nodeoffs, UINT16_C(0) };
    // output code for parameter expressions
    if ( !comp_node_iter_branches( comp, nodeoffs, &cbdata, from_fncall_cb ) ) {
        comp_error( comp, "Internal error (failed to generate code for function argument expressions)" );
        return false;
    }
    // output code to put number of expressions on stack
    if ( !cgen_gen_phim( cgen, cbdata.count ) ) {
        return cgen_out_of_code_memory( comp );
    }
    return true;
}

static bool gen_from_sysfncall( codegen_t* cgen, compiler_t* comp, uint16_t nodeoffs ) {
    cbdata_t cbdata = { cgen, comp, nodeoffs, UINT16_C(0) };
    // output code for parameter expressions
    if ( !comp_node_iter_branches( comp, nodeoffs, &cbdata, from_fncall_cb ) ) {
        comp_error( comp, "Internal error (failed to generate code for function argument expressions)" );
        return false;
    }
    // don't need argument count on stack since it is well-known
    return true;
}

bool cgen_from_varref_lvalue( codegen_t* cgen, compiler_t* comp, uint16_t nodeoffs ) {
    // numeric or string variable reference in an lvalue context
    /*
        NT_NUMVARREF        numeric variable reference
        NT_STRVARREF        string  variable reference
            data:
                - 1 byte of type indicator
                - 2 bytes of variable offset
            branches:
                - list of array index expressions
    */
    //  <nodetype.8> <numbranches.8> <datalen.16> <firstbranch.16> <lastbranch.16> <data...>
    if ( nodeoffs == NODEOFFS_NONE ) {
        return cgen_bad_node( comp );
    }
    uint8_t nodetype = comp->tree[ nodeoffs ];
    if ( nodetype != NT_NUMVARREF && nodetype != NT_STRVARREF ) {
UNEXP:  return cgen_unexpected_node( comp );
    }
    uint16_t datalen = EXTRACT16( comp, nodeoffs + 2U );
    if ( datalen != UINT16_C(3) ) {
        goto UNEXP;
    }
    uint16_t nodeoffs0 = nodeoffs;
    nodeoffs += 8U;
    uint8_t vartype = comp->tree[ nodeoffs++ ];
    uint16_t varoffs = EXTRACT16( comp, nodeoffs );
    nodeoffs = nodeoffs0;
    uint8_t basetype = vartype & VARTYPEM_BASE;
    if ( vartype & VARTYPEF_ARRAY ) {
        bool (*generator)( codegen_t*, uint16_t ) = 0;
        if ( !gen_from_arraysub( cgen, comp, nodeoffs ) ) {
            return false;
        }
        switch ( basetype ) {
            case VARTYPEV_FLOAT:
                generator = cgen_gen_rnae;
                break;
            case VARTYPEV_INT:
                generator = cgen_gen_riae;
                break;
            case VARTYPEV_STR:
                generator = cgen_gen_rsae;
                break;
            default:
                break;
        }
        if ( generator == 0 ) {
            goto UNEXP;
        }
        if ( !generator( cgen, varoffs ) ) {
            return cgen_out_of_code_memory( comp );
        }
    } else if ( vartype & VARTYPEF_FUNC ) {
        goto UNEXP;
    } else {
        bool (*generator)( codegen_t*, uint16_t ) = 0;
        switch ( basetype ) {
            case VARTYPEV_FLOAT:
                generator = cgen_gen_rrnv;
                break;
            case VARTYPEV_INT:
                generator = cgen_gen_rriv;
                break;
            case VARTYPEV_STR:
                generator = cgen_gen_rrsv;
                break;
            case VARTYPEV_LABEL:
                generator = cgen_gen_rrlv;
                break;
            default:
                break;
        }
        if ( generator == 0 ) {
            goto UNEXP;
        }
        if ( !generator( cgen, varoffs ) ) {
            return cgen_out_of_code_memory( comp );
        }
    }
    return true;
}

bool cgen_from_usrfncall( codegen_t* cgen, compiler_t* comp, uint16_t nodeoffs ) {
    // numeric or string user function call
    /*
        NT_NUMUSRFNCALL     numeric user function call
        NT_STRUSRFNCALL     string user function call
            data:
                - 1 byte of type indicator
                - 2 bytes of variable offset
            branches:
                - argument expression list (can be empty)
    */
    //  <nodetype.8> <numbranches.8> <datalen.16> <firstbranch.16> <lastbranch.16> <data...>
    if ( nodeoffs == NODEOFFS_NONE ) {
        return cgen_bad_node( comp );
    }
    uint8_t nodetype = comp->tree[ nodeoffs ];
    if ( nodetype != NT_NUMUSRFNCALL && nodetype != NT_STRUSRFNCALL ) {
UNEXP:  return cgen_unexpected_node( comp );
    }
    uint16_t datalen = EXTRACT16( comp, nodeoffs + 2U );
    if ( datalen != UINT16_C(3) ) {
        goto UNEXP;
    }
    uint16_t nodeoffs0 = nodeoffs;
    nodeoffs += 8U;
    uint8_t vartype = comp->tree[ nodeoffs++ ];
    uint16_t varoffs = EXTRACT16( comp, nodeoffs );
    nodeoffs = nodeoffs0;
    if ( ( vartype & VARTYPEF_FUNC ) == 0 ) {
        goto UNEXP;
    }
    if ( !gen_from_usrfncall( cgen, comp, nodeoffs ) ) {
        return false;
    }
    if ( !cgen_gen_calf( cgen, varoffs ) ) {
        return cgen_out_of_code_memory( comp );
    }
    return true;
}

bool cgen_from_sysfncall( codegen_t* cgen, compiler_t* comp, uint16_t nodeoffs ) {
    /*
        sys-num-fn-1-arg-call := ( TOK_ASC | TOK_VAL | TOK_LEN ) TOK_LPAREN str-expr TOK_RPAREN |
                                   TOK_FRE TOK_LPAREN num-expr TOK_RPAREN .

        sys-str-fn-2-arg-call := ( TOK_BIN | TOK_QUA | TOK_OCT | TOK_DEC | TOK_HEX ) TOK_STRING TOK_LPAREN num-expr
                                      [ TOK_COMMA num-expr ] TOK_RPAREN |
                                 TOK_STR TOK_STRING TOK_LPAREN num-expr TOK_RPAREN |
                                 ( TOK_LEFT | TOK_RIGHT ) TOK_STRING TOK_LPAREN str-expr TOK_COMMA num-expr TOK_RPAREN |
                                 TOK_MID TOK_STRING TOK_LPAREN str-expr TOK_COMMA num-expr [ TOK_COMMA num-expr ] TOK_RPAREN .

        sys-num-fn-arg-call := sys-num-fn-1-arg-call .
        sys-str-fn-arg-call := sys-str-fn-2-arg-call .

        sys-noarg-num-name := TOK_TI .
        sys-noarg-num := sys-noarg-num-name .
        sys-noarg-num-call := sys-no-arg-num .

        sys-noarg-str-name := TOK_INKEY .
        sys-noarg-str := sys-noarg-str-name TOK_STRING .
        sys-noarg-str-call := sys-no-arg-str .

        NT_SYSNUMFUNCARGCALL    numeric system function call with arguments
        NT_SYSSTRFUNCARGCALL    string system function call with arguments
            data:
                - 1 byte of function token (like TOK_VAL)
            branches:
                - 1 or more branches of expression list (depending on function)

        NT_SYSNOARGSTRCALL  system string function call without arguments
        NT_SYSNOARGNUMCALL  system number function call without arguments
            data:
                - 1 byte of function token (like TOK_TI)
    */
    if ( nodeoffs == NODEOFFS_NONE ) {
        return cgen_bad_node( comp );
    }
    //  <nodetype.8> <numbranches.8> <datalen.16> <firstbranch.16> <lastbranch.16> <data...>
    uint8_t nodetype = comp->tree[ nodeoffs ]; bool hasargs = false;
    switch ( nodetype ) {
        case NT_SYSNUMFUNCARGCALL:
        case NT_SYSSTRFUNCARGCALL:
            hasargs = true;
            break;
        case NT_SYSNOARGNUMCALL:
        case NT_SYSNOARGSTRCALL:
            break;
        default:
UNEXP:      return cgen_unexpected_node( comp );
    }
    uint16_t datalen = EXTRACT16( comp, nodeoffs + 2U );
    if ( datalen != UINT16_C(1) ) {
        goto UNEXP;
    }
    if ( hasargs && !gen_from_sysfncall( cgen, comp, nodeoffs ) ) {
        return false;
    }
    bool (*generator)( codegen_t* ) = 0;
    uint8_t functok = comp->tree[ nodeoffs + 8U ];
    switch ( functok ) {
        case TOK_TI:    generator = cgen_gen_ti; break;
        case TOK_INKEY: generator = cgen_gen_inky; break;
        case TOK_ASC:   generator = cgen_gen_asc; break;
        case TOK_VAL:   generator = cgen_gen_val; break;
        case TOK_FRE:   generator = cgen_gen_fre; break;
        case TOK_LEN:   generator = cgen_gen_len; break;
        case TOK_BIN:   generator = cgen_gen_bins; break;
        case TOK_QUA:   generator = cgen_gen_quas; break;
        case TOK_OCT:   generator = cgen_gen_octs; break;
        case TOK_HEX:   generator = cgen_gen_hexs; break;
        case TOK_DEC:   generator = cgen_gen_decs; break;
        case TOK_STR:   generator = cgen_gen_strs; break;
        case TOK_LEFT:  generator = cgen_gen_lfts; break;
        case TOK_RIGHT: generator = cgen_gen_rgts; break;
        case TOK_MID:   generator = cgen_gen_mids; break;
        default:
            goto UNEXP;
    }
    if ( !generator( cgen ) ) {
        return cgen_out_of_code_memory( comp );
    }
    return true;
}

bool cgen_from_numbaseexpr( codegen_t* cgen, compiler_t* comp, uint16_t nodeoffs ) {
    // num-base-expr := num-var-ref | num-lit | num-func-call | num-sub-expr .
    // num-func-call := num-usr-fn-call | sys-num-fn-arg-call | sys-noarg-num-call .
    if ( nodeoffs == NODEOFFS_NONE ) {
        return cgen_bad_node( comp );
    }
    uint8_t nodetype = comp->tree[ nodeoffs ];
    switch ( nodetype ) {
        case NT_NUMVARREF:
            return cgen_from_varref_lvalue( cgen, comp, nodeoffs );
        case NT_NUMLIT:
            return cgen_from_numlit( cgen, comp, nodeoffs );
        case NT_NUMUSRFNCALL:
            return cgen_from_usrfncall( cgen, comp, nodeoffs );
        case NT_SYSNUMFUNCARGCALL:
        case NT_SYSNOARGNUMCALL:
            return cgen_from_sysfncall( cgen, comp, nodeoffs );
        default:
            break;
    }
    return cgen_from_numexpr( cgen, comp, nodeoffs );   // ???
}

bool cgen_from_strbaseexpr( codegen_t* cgen, compiler_t* comp, uint16_t nodeoffs ) {
    // str-base-expr := str-var-ref | str-lits | str-func-call .
    // str-func-call := str-usr-fn-call | sys-str-fn-arg-call | sys-noarg-str-call .
    if ( nodeoffs == NODEOFFS_NONE ) {
        return cgen_bad_node( comp );
    }
    uint8_t nodetype = comp->tree[ nodeoffs ];
    switch ( nodetype ) {
        case NT_STRVARREF:
            return cgen_from_varref_lvalue( cgen, comp, nodeoffs );
        case NT_STRLIT:
            return cgen_from_strlit( cgen, comp, nodeoffs );
        case NT_STRLITS:
            return cgen_from_strlits( cgen, comp, nodeoffs );
        case NT_STRUSRFNCALL:
            return cgen_from_usrfncall( cgen, comp, nodeoffs );
        case NT_SYSSTRFUNCARGCALL:
        case NT_SYSNOARGSTRCALL:
            return cgen_from_sysfncall( cgen, comp, nodeoffs );
        default:
            break;
    }
    return cgen_unexpected_node( comp );
}

static bool from_stradd_cb( void* param, uint16_t nodeoffs ) {
    cbdata_t* pdata = (cbdata_t*) param;
    // put the current string expression on the data stack
    if ( !cgen_from_strbaseexpr( pdata->cgen, pdata->comp, nodeoffs ) ) {
        return false;
    }
    if ( ++pdata->count >= UINT16_C(2) ) {
        // generate CON (concat) instructions after 2 string literals and every new string literal
        if ( !cgen_gen_con( pdata->cgen ) ) {
            return cgen_out_of_code_memory( pdata->comp );
        }
    }
    return true;
}

bool cgen_from_straddexpr( codegen_t* cgen, compiler_t* comp, uint16_t nodeoffs ) {
    // str-add-expr := str-base-expr { TOK_PLUS str-base-expr } .
    if ( nodeoffs == NODEOFFS_NONE ) {
        return cgen_bad_node( comp );
    }
    uint8_t nodetype = comp->tree[ nodeoffs ];
    if ( nodetype != NT_STRADDEXPR ) {
        return cgen_from_strbaseexpr( cgen, comp, nodeoffs );
    }
    cbdata_t cbdata = { cgen, comp, nodeoffs, UINT16_C(0) };
    return comp_node_iter_branches( comp, nodeoffs, &cbdata, from_stradd_cb );
}

static bool from_strexpr_cb( void* param, uint16_t nodeoffs ) {
    cbdata_t* pdata = (cbdata_t*) param;
    // put the first string expression on the data stack
    if ( !cgen_from_straddexpr( pdata->cgen, pdata->comp, nodeoffs ) ) {
        return false;
    }
    return true;
}

bool cgen_from_strexpr( codegen_t* cgen, compiler_t* comp, uint16_t nodeoffs ) {
    // str-expr := str-add-expr .
    if ( nodeoffs == NODEOFFS_NONE ) {
        return cgen_bad_node( comp );
    }
    //  <nodetype.8> <numbranches.8> <datalen.16> <firstbranch.16> <lastbranch.16> <data...>
    uint8_t nodetype = comp->tree[ nodeoffs ];
    if ( nodetype != NT_STREXPR ) {
        return cgen_unexpected_node( comp );
    }
    cbdata_t cbdata = { cgen, comp, nodeoffs, UINT16_C(0) };
    return comp_node_iter_branches( comp, nodeoffs, &cbdata, from_strexpr_cb );
}

static bool from_numunary_cb( void* param, uint16_t nodeoffs ) {
    cbdata_t* pdata = (cbdata_t*) param;
    if ( ++pdata->count >= UINT16_C(2) ) {  // not expected to have more than 1 branch
        cgen_unexpected_node( pdata->comp );
        return false;
    }
    // put the current numeric expression on the data stack
    if ( !cgen_from_numbaseexpr( pdata->cgen, pdata->comp, nodeoffs ) ) {
        return false;
    }
    return true;
}

bool cgen_from_numunaryex( codegen_t* cgen, compiler_t* comp, uint16_t nodeoffs ) {
    // num-unary-ex := [ num-unary-op ] num-base-expr .
    // num-unary-op := TOK_MINUS | TOK_PLUS | TOK_NOT .
    if ( nodeoffs == NODEOFFS_NONE ) {
        return cgen_bad_node( comp );
    }
    //  <nodetype.8> <numbranches.8> <datalen.16> <firstbranch.16> <lastbranch.16> <data...>
    uint8_t nodetype = comp->tree[ nodeoffs ];
    if ( nodetype != NT_NUMUNARYEX ) {
        return cgen_from_numbaseexpr( cgen, comp, nodeoffs );
    }
    uint16_t datalen = EXTRACT16( comp, nodeoffs + 2U );
    if ( datalen != 1U ) {
UNEXP:  return cgen_unexpected_node( comp );
    }
    uint8_t tok = comp->tree[ nodeoffs + 8U ];
    if ( tok != TOK_MINUS && tok != TOK_PLUS && tok != TOK_NOT ) {
        goto UNEXP;
    }
    cbdata_t cbdata = { cgen, comp, nodeoffs, UINT16_C(0) };
    if ( !comp_node_iter_branches( comp, nodeoffs, &cbdata, from_numunary_cb ) ) {
        return false;
    }
    bool (*generator)( codegen_t* ) = 0;
    switch ( tok ) {
        case TOK_MINUS:     generator = cgen_gen_neg; break;
        case TOK_NOT:       generator = cgen_gen_not; break;
        default:            break;
    }
    if ( generator && !generator( cgen ) ) {
        return cgen_out_of_code_memory( comp );
    }
    return true;
}

static bool from_nummult_cb( void* param, uint16_t nodeoffs ) {
    cbdata_t* pdata = (cbdata_t*) param;
    ++pdata->count;
    if ( pdata->count == UINT16_C(1) ) {
        // put the first numeric expression on the data stack
        if ( !cgen_from_numunaryex( pdata->cgen, pdata->comp, nodeoffs ) ) {
            return false;
        }
    } else {    // NT_OPERATOR node
        //  <nodetype.8> <numbranches.8> <datalen.16> <firstbranch.16> <lastbranch.16> <data...>
        uint8_t nodetype = pdata->comp->tree[ nodeoffs ];
        if ( nodetype != NT_OPERATOR ) {
UNEXP:      return cgen_unexpected_node( pdata->comp );
        }
        uint16_t datalen = EXTRACT16( pdata->comp, nodeoffs + 2U );
        if ( datalen != 1U ) {
            goto UNEXP;
        }
        uint8_t tok = pdata->comp->tree[ nodeoffs + 8U ];
        bool (*generator)( codegen_t* ) = 0;
        // num-mult-op  := TOK_MULT | TOK_DIV | TOK_POW .
        switch ( tok ) {
            case TOK_MULT:  generator = cgen_gen_mul; break;
            case TOK_DIV:   generator = cgen_gen_div; break;
            case TOK_POW:   generator = cgen_gen_pow; break;
            default:
                break;
        }
        if ( generator == 0 ) {
            goto UNEXP;
        }
        // can safely call myself here since NT_OPERATOR has only one branch
        cbdata_t cbdata = { pdata->cgen, pdata->comp, nodeoffs, UINT16_C(0) };
        if ( !comp_node_iter_branches( pdata->comp, nodeoffs, &cbdata, from_nummult_cb ) ) {
            return false;
        }
        // generate the statement to handle the new value pair
        if ( !generator( pdata->cgen ) ) {
            cgen_out_of_code_memory( pdata->comp );
            return false;
        }
    }
    return true;
}

bool cgen_from_nummultex( codegen_t* cgen, compiler_t* comp, uint16_t nodeoffs ) {
    // num-mult-op  := TOK_MULT | TOK_DIV | TOK_POW .
    // num-mult-ex  := num-unary-ex { num-mult-op num-unary-ex } .
    if ( nodeoffs == NODEOFFS_NONE ) {
        return cgen_bad_node( comp );
    }
    //  <nodetype.8> <numbranches.8> <datalen.16> <firstbranch.16> <lastbranch.16> <data...>
    uint8_t nodetype = comp->tree[ nodeoffs ];
    if ( nodetype != NT_NUMMULTEX ) {
        return cgen_from_numunaryex( cgen, comp, nodeoffs );
    }
    cbdata_t cbdata = { cgen, comp, nodeoffs, UINT16_C(0) };
    return comp_node_iter_branches( comp, nodeoffs, &cbdata, from_nummult_cb );
}

static bool from_numadd_cb( void* param, uint16_t nodeoffs ) {
    cbdata_t* pdata = (cbdata_t*) param;
    ++pdata->count;
    if ( pdata->count == UINT16_C(1) ) {
        // put the first numeric expression on the data stack
        if ( !cgen_from_nummultex( pdata->cgen, pdata->comp, nodeoffs ) ) {
            return false;
        }
    } else {    // NT_OPERATOR node
        //  <nodetype.8> <numbranches.8> <datalen.16> <firstbranch.16> <lastbranch.16> <data...>
        uint8_t nodetype = pdata->comp->tree[ nodeoffs ];
        if ( nodetype != NT_OPERATOR ) {
UNEXP:      return cgen_unexpected_node( pdata->comp );
        }
        uint16_t datalen = EXTRACT16( pdata->comp, nodeoffs + 2U );
        if ( datalen != 1U ) {
            goto UNEXP;
        }
        uint8_t tok = pdata->comp->tree[ nodeoffs + 8U ];
        bool (*generator)( codegen_t* ) = 0;
        // num-add-op   := TOK_PLUS | TOK_MINUS .
        switch ( tok ) {
            case TOK_PLUS:  generator = cgen_gen_add; break;
            case TOK_MINUS: generator = cgen_gen_sub; break;
            default:
                break;
        }
        if ( generator == 0 ) {
            goto UNEXP;
        }
        // can safely call myself here since NT_OPERATOR has only one branch
        cbdata_t cbdata = { pdata->cgen, pdata->comp, nodeoffs, UINT16_C(0) };
        if ( !comp_node_iter_branches( pdata->comp, nodeoffs, &cbdata, from_numadd_cb ) ) {
            return false;
        }
        // generate the statement to handle the new value pair
        if ( !generator( pdata->cgen ) ) {
            cgen_out_of_code_memory( pdata->comp );
            return false;
        }
    }
    return true;
}

bool cgen_from_numaddex( codegen_t* cgen, compiler_t* comp, uint16_t nodeoffs ) {
    /*
    num-add-op   := TOK_PLUS | TOK_MINUS .
    num-add-ex   := num-mult-ex { num-add-op num-mult-ex } .
    */
    if ( nodeoffs == NODEOFFS_NONE ) {
        return cgen_bad_node( comp );
    }
    //  <nodetype.8> <numbranches.8> <datalen.16> <firstbranch.16> <lastbranch.16> <data...>
    uint8_t nodetype = comp->tree[ nodeoffs ];
    if ( nodetype != NT_NUMADDEX ) {
        return cgen_from_nummultex( cgen, comp, nodeoffs );
    }
    cbdata_t cbdata = { cgen, comp, nodeoffs, UINT16_C(0) };
    return comp_node_iter_branches( comp, nodeoffs, &cbdata, from_numadd_cb );
}

static bool from_numshift_cb( void* param, uint16_t nodeoffs ) {
    cbdata_t* pdata = (cbdata_t*) param;
    ++pdata->count;
    if ( pdata->count == UINT16_C(1) ) {
        // put the first numeric expression on the data stack
        if ( !cgen_from_numaddex( pdata->cgen, pdata->comp, nodeoffs ) ) {
            return false;
        }
    } else {    // NT_OPERATOR node
        //  <nodetype.8> <numbranches.8> <datalen.16> <firstbranch.16> <lastbranch.16> <data...>
        uint8_t nodetype = pdata->comp->tree[ nodeoffs ];
        if ( nodetype != NT_OPERATOR ) {
UNEXP:      return cgen_unexpected_node( pdata->comp );
        }
        uint16_t datalen = EXTRACT16( pdata->comp, nodeoffs + 2U );
        if ( datalen != 1U ) {
            goto UNEXP;
        }
        uint8_t tok = pdata->comp->tree[ nodeoffs + 8U ];
        bool (*generator)( codegen_t* ) = 0;
        // num-shift-op := TOK_LSHIFT | TOK_RSHIFT .
        switch ( tok ) {
            case TOK_LSHIFT:    generator = cgen_gen_lsh; break;
            case TOK_RSHIFT:    generator = cgen_gen_rsh; break;
            default:
                break;
        }
        if ( generator == 0 ) {
            goto UNEXP;
        }
        // can safely call myself here since NT_OPERATOR has only one branch
        cbdata_t cbdata = { pdata->cgen, pdata->comp, nodeoffs, UINT16_C(0) };
        if ( !comp_node_iter_branches( pdata->comp, nodeoffs, &cbdata, from_numshift_cb ) ) {
            return false;
        }
        // generate the statement to handle the new value pair
        if ( !generator( pdata->cgen ) ) {
            cgen_out_of_code_memory( pdata->comp );
            return false;
        }
    }
    return true;
}

bool cgen_from_numshiftex( codegen_t* cgen, compiler_t* comp, uint16_t nodeoffs ) {
    /*
        num-shift-op := TOK_LSHIFT | TOK_RSHIFT .
        num-shift-ex := num-add-ex [ num-shift-op num-add-ex ] .
    */
    if ( nodeoffs == NODEOFFS_NONE ) {
        return cgen_bad_node( comp );
    }
    //  <nodetype.8> <numbranches.8> <datalen.16> <firstbranch.16> <lastbranch.16> <data...>
    uint8_t nodetype = comp->tree[ nodeoffs ];
    if ( nodetype != NT_NUMSHIFTEX ) {
        return cgen_from_numaddex( cgen, comp, nodeoffs );
    }
    cbdata_t cbdata = { cgen, comp, nodeoffs, UINT16_C(0) };
    return comp_node_iter_branches( comp, nodeoffs, &cbdata, from_numshift_cb );
}

static bool from_strcmp_cb( void* param, uint16_t nodeoffs ) {
    cbdata_t* pdata = (cbdata_t*) param;
    ++pdata->count;
    if ( pdata->count == UINT16_C(1) ) {
        // put the first string expression on the data stack
        if ( !cgen_from_strexpr( pdata->cgen, pdata->comp, nodeoffs ) ) {
            return false;
        }
    } else {    // NT_OPERATOR node
        //  <nodetype.8> <numbranches.8> <datalen.16> <firstbranch.16> <lastbranch.16> <data...>
        uint8_t nodetype = pdata->comp->tree[ nodeoffs ];
        if ( nodetype != NT_OPERATOR ) {
UNEXP:      return cgen_unexpected_node( pdata->comp );
        }
        uint16_t datalen = EXTRACT16( pdata->comp, nodeoffs + 2U );
        if ( datalen != 1U ) {
            goto UNEXP;
        }
        uint8_t tok = pdata->comp->tree[ nodeoffs + 8U ];
        bool (*generator)( codegen_t* ) = 0;
        // num-cmp-op := TOK_EQ | TOK_NE | TOK_LE | TOK_GE | TOK_LT | TOK_GT .
        switch ( tok ) {
            case TOK_EQ:    generator = cgen_gen_cseq; break;
            case TOK_NE:    generator = cgen_gen_csne; break;
            case TOK_GE:    generator = cgen_gen_csge; break;
            case TOK_LE:    generator = cgen_gen_csle; break;
            case TOK_GT:    generator = cgen_gen_csgt; break;
            case TOK_LT:    generator = cgen_gen_cslt; break;
            default:
                break;
        }
        if ( generator == 0 ) {
            goto UNEXP;
        }
        // can safely call myself here since NT_OPERATOR has only one branch
        cbdata_t cbdata = { pdata->cgen, pdata->comp, nodeoffs, UINT16_C(0) };
        if ( !comp_node_iter_branches( pdata->comp, nodeoffs, &cbdata, from_strcmp_cb ) ) {
            return false;
        }
        // generate the statement to handle the new value pair
        if ( !generator( pdata->cgen ) ) {
            cgen_out_of_code_memory( pdata->comp );
            return false;
        }
    }
    return true;
}

static bool from_numcmp_cb( void* param, uint16_t nodeoffs ) {
    cbdata_t* pdata = (cbdata_t*) param;
    ++pdata->count;
    if ( pdata->count == UINT16_C(1) ) {
        // put the first numeric expression on the data stack
        if ( !cgen_from_numshiftex( pdata->cgen, pdata->comp, nodeoffs ) ) {
            return false;
        }
    } else {    // NT_OPERATOR node
        //  <nodetype.8> <numbranches.8> <datalen.16> <firstbranch.16> <lastbranch.16> <data...>
        uint8_t nodetype = pdata->comp->tree[ nodeoffs ];
        if ( nodetype != NT_OPERATOR ) {
UNEXP:      return cgen_unexpected_node( pdata->comp );
        }
        uint16_t datalen = EXTRACT16( pdata->comp, nodeoffs + 2U );
        if ( datalen != 1U ) {
            goto UNEXP;
        }
        uint8_t tok = pdata->comp->tree[ nodeoffs + 8U ];
        bool (*generator)( codegen_t* ) = 0;
        // num-cmp-op := TOK_EQ | TOK_NE | TOK_LE | TOK_GE | TOK_LT | TOK_GT .
        switch ( tok ) {
            case TOK_EQ:    generator = cgen_gen_cneq; break;
            case TOK_NE:    generator = cgen_gen_cnne; break;
            case TOK_GE:    generator = cgen_gen_cnge; break;
            case TOK_LE:    generator = cgen_gen_cnle; break;
            case TOK_GT:    generator = cgen_gen_cngt; break;
            case TOK_LT:    generator = cgen_gen_cnlt; break;
            default:
                break;
        }
        if ( generator == 0 ) {
            goto UNEXP;
        }
        // can safely call myself here since NT_OPERATOR has only one branch
        cbdata_t cbdata = { pdata->cgen, pdata->comp, nodeoffs, UINT16_C(0) };
        if ( !comp_node_iter_branches( pdata->comp, nodeoffs, &cbdata, from_numcmp_cb ) ) {
            return false;
        }
        // generate the statement to handle the new value pair
        if ( !generator( pdata->cgen ) ) {
            cgen_out_of_code_memory( pdata->comp );
            return false;
        }
    }
    return true;
}

bool cgen_from_numcmpex( codegen_t* cgen, compiler_t* comp, uint16_t nodeoffs ) {
    /*
        num-cmp-op   := TOK_EQ | TOK_NE | TOK_LE | TOK_GE | TOK_LT | TOK_GT .
        num-cmp-ex   := num-shift-ex [ num-cmp-op num-shift-ex ] |
                        str-expr num-cmp-op str-expr .
    */
    if ( nodeoffs == NODEOFFS_NONE ) {
        return cgen_bad_node( comp );
    }
    //  <nodetype.8> <numbranches.8> <datalen.16> <firstbranch.16> <lastbranch.16> <data...>
    uint8_t nodetype = comp->tree[ nodeoffs ];
    if ( nodetype != NT_NUMCMPEX && nodetype != NT_STRCMPEX ) {
        return cgen_from_numshiftex( cgen, comp, nodeoffs );
    }
    cbdata_t cbdata = { cgen, comp, nodeoffs, UINT16_C(0) };
    if ( nodetype == NT_STRCMPEX ) {
        return comp_node_iter_branches( comp, nodeoffs, &cbdata, from_strcmp_cb );
    }
    return comp_node_iter_branches( comp, nodeoffs, &cbdata, from_numcmp_cb );
}

static bool from_numand_cb( void* param, uint16_t nodeoffs ) {
    cbdata_t* pdata = (cbdata_t*) param;
    ++pdata->count;
    if ( pdata->count == UINT16_C(1) ) {
        // put the first numeric expression on the data stack
        if ( !cgen_from_numcmpex( pdata->cgen, pdata->comp, nodeoffs ) ) {
            return false;
        }
    } else {    // NT_OPERATOR node
        //  <nodetype.8> <numbranches.8> <datalen.16> <firstbranch.16> <lastbranch.16> <data...>
        uint8_t nodetype = pdata->comp->tree[ nodeoffs ];
        if ( nodetype != NT_OPERATOR ) {
UNEXP:      return cgen_unexpected_node( pdata->comp );
        }
        uint16_t datalen = EXTRACT16( pdata->comp, nodeoffs + 2U );
        if ( datalen != 1U ) {
            goto UNEXP;
        }
        uint8_t tok = pdata->comp->tree[ nodeoffs + 8U ];
        bool (*generator)( codegen_t* ) = 0;
        // num-and-op   := TOK_AND | TOK_NAND .
        switch ( tok ) {
            case TOK_AND:   generator = cgen_gen_and; break;
            case TOK_NAND:  generator = cgen_gen_nand; break;
            default:
                break;
        }
        if ( generator == 0 ) {
            goto UNEXP;
        }
        // can safely call myself here since NT_OPERATOR has only one branch
        cbdata_t cbdata = { pdata->cgen, pdata->comp, nodeoffs, UINT16_C(0) };
        if ( !comp_node_iter_branches( pdata->comp, nodeoffs, &cbdata, from_numand_cb ) ) {
            return false;
        }
        // generate the statement to handle the new value pair
        if ( !generator( pdata->cgen ) ) {
            cgen_out_of_code_memory( pdata->comp );
            return false;
        }
    }
    return true;
}

bool cgen_from_numandex( codegen_t* cgen, compiler_t* comp, uint16_t nodeoffs ) {
    /*
        num-and-op   := TOK_AND | TOK_NAND .
        num-and-ex   := num-cmp-ex { num-and-op num-cmp-ex } .
    */
    if ( nodeoffs == NODEOFFS_NONE ) {
        return cgen_bad_node( comp );
    }
    //  <nodetype.8> <numbranches.8> <datalen.16> <firstbranch.16> <lastbranch.16> <data...>
    uint8_t nodetype = comp->tree[ nodeoffs ];
    if ( nodetype != NT_NUMANDEX ) {
        return cgen_from_numcmpex( cgen, comp, nodeoffs );
    }
    cbdata_t cbdata = { cgen, comp, nodeoffs, UINT16_C(0) };
    return comp_node_iter_branches( comp, nodeoffs, &cbdata, from_numand_cb );
}

static bool from_numor_cb( void* param, uint16_t nodeoffs ) {
    cbdata_t* pdata = (cbdata_t*) param;
    ++pdata->count;
    if ( pdata->count == UINT16_C(1) ) {
        // put the first numeric expression on the data stack
        if ( !cgen_from_numandex( pdata->cgen, pdata->comp, nodeoffs ) ) {
            return false;
        }
    } else {    // NT_OPERATOR node
        //  <nodetype.8> <numbranches.8> <datalen.16> <firstbranch.16> <lastbranch.16> <data...>
        uint8_t nodetype = pdata->comp->tree[ nodeoffs ];
        if ( nodetype != NT_OPERATOR ) {
UNEXP:      return cgen_unexpected_node( pdata->comp );
        }
        uint16_t datalen = EXTRACT16( pdata->comp, nodeoffs + 2U );
        if ( datalen != 1U ) {
            goto UNEXP;
        }
        uint8_t tok = pdata->comp->tree[ nodeoffs + 8U ];
        bool (*generator)( codegen_t* ) = 0;
        // num-or-op := TOK_OR | TOK_XOR | TOK_NOR | TOK_XNOR .
        switch ( tok ) {
            case TOK_OR:   generator = cgen_gen_or; break;
            case TOK_XOR:  generator = cgen_gen_xor; break;
            case TOK_NOR:  generator = cgen_gen_nor; break;
            case TOK_XNOR: generator = cgen_gen_xnor; break;
            default:
                break;
        }
        if ( generator == 0 ) {
            goto UNEXP;
        }
        // can safely call myself here since NT_OPERATOR has only one branch
        cbdata_t cbdata = { pdata->cgen, pdata->comp, nodeoffs, UINT16_C(0) };
        if ( !comp_node_iter_branches( pdata->comp, nodeoffs, &cbdata, from_numor_cb ) ) {
            return false;
        }
        // generate the statement to handle the new value pair
        if ( !generator( pdata->cgen ) ) {
            cgen_out_of_code_memory( pdata->comp );
            return false;
        }
    }
    return true;
}

bool cgen_from_numorex( codegen_t* cgen, compiler_t* comp, uint16_t nodeoffs ) {
    /*
        num-or-op    := TOK_OR | TOK_XOR | TOK_NOR | TOK_XNOR .
        num-or-ex    := num-and-ex { num-or-op num-and-ex } .
    */
    if ( nodeoffs == NODEOFFS_NONE ) {
        return cgen_bad_node( comp );
    }
    //  <nodetype.8> <numbranches.8> <datalen.16> <firstbranch.16> <lastbranch.16> <data...>
    uint8_t nodetype = comp->tree[ nodeoffs ];
    if ( nodetype != NT_NUMOREX ) {
        return cgen_from_numandex( cgen, comp, nodeoffs );
    }
    cbdata_t cbdata = { cgen, comp, nodeoffs, UINT16_C(0) };
    return comp_node_iter_branches( comp, nodeoffs, &cbdata, from_numor_cb );
}

static bool from_numexpr_cb( void* param, uint16_t nodeoffs ) {
    cbdata_t* pdata = (cbdata_t*) param;
    // put the first numeric expression on the data stack
    if ( !cgen_from_numorex( pdata->cgen, pdata->comp, nodeoffs ) ) {
        return false;
    }
    return true;
}

bool cgen_from_numexpr( codegen_t* cgen, compiler_t* comp, uint16_t nodeoffs ) {
    // num-expr     := num-or-ex .
    if ( nodeoffs == NODEOFFS_NONE ) {
        return cgen_bad_node( comp );
    }
    //  <nodetype.8> <numbranches.8> <datalen.16> <firstbranch.16> <lastbranch.16> <data...>
    uint8_t nodetype = comp->tree[ nodeoffs ];
    if ( nodetype != NT_NUMEXPR ) {
        return cgen_unexpected_node( comp );
    }
    cbdata_t cbdata = { cgen, comp, nodeoffs, UINT16_C(0) };
    return comp_node_iter_branches( comp, nodeoffs, &cbdata, from_numexpr_cb );
}

bool cgen_from_expr( codegen_t* cgen, compiler_t* comp, uint16_t nodeoffs ) {
    // expr := num-expr | str-expr .
    if ( nodeoffs == NODEOFFS_NONE ) {
        return cgen_bad_node( comp );
    }
    //  <nodetype.8> <numbranches.8> <datalen.16> <firstbranch.16> <lastbranch.16> <data...>
    uint8_t nodetype = comp->tree[ nodeoffs ];
    if ( nodetype == NT_STREXPR ) {
        return cgen_from_strexpr( cgen, comp, nodeoffs );
    }
    if ( nodetype == NT_NUMEXPR ) {
        return cgen_from_numexpr( cgen, comp, nodeoffs );
    }
    return cgen_unexpected_node( comp );
}
