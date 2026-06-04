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

#ifndef CODEGEN_H
#define CODEGEN_H   1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

#ifndef BASCOMP_H
#include "bascomp.h"
#endif

#define CODEOFFS_NONE   UINT16_C(0XFFFF)
#define DATAOFFS_NONE   UINT16_C(0XFFFF)

#define CODESIZE_MAX    65536U
#define DATASIZE_MAX    65536U

typedef struct _codegen_t {
    void*           userdata;
    void            (*report)( struct _codegen_t*, void*, const char* );
    void            (*halt)( struct _codegen_t*, void* ) ATTR_NORETURN;
    uint8_t         code[CODESIZE_MAX], data[DATASIZE_MAX];
    uint32_t        codesize, datasize;
} codegen_t;

void init_codegen( codegen_t* cgen );
bool cgen_alloc_code( codegen_t* cgen, uint16_t size, uint16_t* poffs );
bool cgen_alloc_data( codegen_t* cgen, uint16_t size, uint16_t* poffs );
bool cgen_gen_ins( codegen_t* cgen, uint8_t ins, uint8_t ext, uint16_t param );
bool cgen_gen_ins12( codegen_t* cgen, uint16_t ins12, bool code, bool hasparam, uint16_t param );
bool cgen_gen_ins12_imm17( codegen_t* cgen, uint16_t ins12, int32_t imm17 );
bool cgen_gen_ins4_imm17( codegen_t* cgen, uint8_t ins, int32_t imm );

// special instructions
bool cgen_gen_brk( codegen_t* cgen );
bool cgen_gen_nop( codegen_t* cgen );
bool cgen_gen_phpa_c( codegen_t* cgen, uint16_t offs );
bool cgen_gen_phpa_d( codegen_t* cgen, uint16_t offs );
bool cgen_gen_phim( codegen_t* cgen, int32_t imm );
bool cgen_gen_bria( codegen_t* cgen, int32_t abs_offs );
bool cgen_gen_brir( codegen_t* cgen, int32_t rel_offs );
bool cgen_gen_jpcc( codegen_t* cgen );
bool cgen_gen_jump( codegen_t* cgen );
bool cgen_gen_drop( codegen_t* cgen, uint16_t cnt );
bool cgen_gen_line( codegen_t* cgen, uint16_t line );
bool cgen_gen_lan_c( codegen_t* cgen );
bool cgen_gen_lan_d( codegen_t* cgen );
bool cgen_gen_lian_c( codegen_t* cgen );
bool cgen_gen_lian_d( codegen_t* cgen );
bool cgen_gen_luan_c( codegen_t* cgen );
bool cgen_gen_luan_d( codegen_t* cgen );
bool cgen_gen_las_c( codegen_t* cgen );
bool cgen_gen_las_d( codegen_t* cgen );
bool cgen_gen_fres( codegen_t* cgen );

// arithmetical / logical instructions
bool cgen_gen_neg( codegen_t* cgen );
bool cgen_gen_not( codegen_t* cgen );
bool cgen_gen_lsh( codegen_t* cgen );
bool cgen_gen_rsh( codegen_t* cgen );
bool cgen_gen_add( codegen_t* cgen );
bool cgen_gen_sub( codegen_t* cgen );
bool cgen_gen_mul( codegen_t* cgen );
bool cgen_gen_div( codegen_t* cgen );
bool cgen_gen_and( codegen_t* cgen );
bool cgen_gen_nand( codegen_t* cgen );
bool cgen_gen_or( codegen_t* cgen );
bool cgen_gen_nor( codegen_t* cgen );
bool cgen_gen_xor( codegen_t* cgen );
bool cgen_gen_xnor( codegen_t* cgen );
bool cgen_gen_pow( codegen_t* cgen );
bool cgen_gen_con( codegen_t* cgen );
bool cgen_gen_cneq( codegen_t* cgen );
bool cgen_gen_cnne( codegen_t* cgen );
bool cgen_gen_cnge( codegen_t* cgen );
bool cgen_gen_cnle( codegen_t* cgen );
bool cgen_gen_cngt( codegen_t* cgen );
bool cgen_gen_cnlt( codegen_t* cgen );
bool cgen_gen_cseq( codegen_t* cgen );
bool cgen_gen_csne( codegen_t* cgen );
bool cgen_gen_csge( codegen_t* cgen );
bool cgen_gen_csle( codegen_t* cgen );
bool cgen_gen_csgt( codegen_t* cgen );
bool cgen_gen_cslt( codegen_t* cgen );

// variable access instructions
bool cgen_gen_rrnv( codegen_t* cgen, uint16_t varoffs );
bool cgen_gen_rriv( codegen_t* cgen, uint16_t varoffs );
bool cgen_gen_rrsv( codegen_t* cgen, uint16_t varoffs );
bool cgen_gen_rrlv( codegen_t* cgen, uint16_t varoffs );
bool cgen_gen_rnae( codegen_t* cgen, uint16_t varoffs );
bool cgen_gen_riae( codegen_t* cgen, uint16_t varoffs );
bool cgen_gen_rsae( codegen_t* cgen, uint16_t varoffs );
bool cgen_gen_wrnv( codegen_t* cgen, uint16_t varoffs );
bool cgen_gen_wriv( codegen_t* cgen, uint16_t varoffs );
bool cgen_gen_wrsv( codegen_t* cgen, uint16_t varoffs );
bool cgen_gen_wrlv( codegen_t* cgen, uint16_t varoffs );
bool cgen_gen_wnae( codegen_t* cgen, uint16_t varoffs );
bool cgen_gen_wiae( codegen_t* cgen, uint16_t varoffs );
bool cgen_gen_wsae( codegen_t* cgen, uint16_t varoffs );

// other instructions
bool cgen_gen_shex( codegen_t* cgen );
bool cgen_gen_calf( codegen_t* cgen, uint16_t varoffs );
bool cgen_gen_retf( codegen_t* cgen );
bool cgen_gen_gadc( codegen_t* cgen, uint16_t varoffs );
bool cgen_gen_gfac( codegen_t* cgen, uint16_t varoffs );

// BASIC system function implementations
bool cgen_gen_ti( codegen_t* cgen );
bool cgen_gen_inky( codegen_t* cgen );
bool cgen_gen_asc( codegen_t* cgen );
bool cgen_gen_val( codegen_t* cgen );
bool cgen_gen_fre( codegen_t* cgen );
bool cgen_gen_bins( codegen_t* cgen );
bool cgen_gen_quas( codegen_t* cgen );
bool cgen_gen_octs( codegen_t* cgen );
bool cgen_gen_hexs( codegen_t* cgen );
bool cgen_gen_decs( codegen_t* cgen );
bool cgen_gen_strs( codegen_t* cgen );

// generate code from syntax tree nodes

bool cgen_from_numlit( codegen_t* cgen, compiler_t* comp, uint16_t nodeoffs );
bool cgen_from_strlit( codegen_t* cgen, compiler_t* comp, uint16_t nodeoffs );
bool cgen_from_strlits( codegen_t* cgen, compiler_t* comp, uint16_t nodeoffs );
bool cgen_from_varref_lvalue( codegen_t* cgen, compiler_t* comp, uint16_t nodeoffs );
bool cgen_from_usrfncall( codegen_t* cgen, compiler_t* comp, uint16_t nodeoffs );
bool cgen_from_numexpr( codegen_t* cgen, compiler_t* comp, uint16_t nodeoffs );

#endif