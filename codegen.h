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

#define CODEOFFS_NONE   UINT32_C(0XFFFFFFFF)
#define DATAOFFS_NONE   UINT32_C(0XFFFFFFFF)

// The code and data segments are heap buffers that grow on demand. The offsets are 32 bit (wide instruction parameters,
// see instset.h); these soft limits keep a runaway program from using all the memory. Code and data offsets must stay
// below CODEOFFS_NONE / DATAOFFS_NONE.
#define CODESIZE_MAX    UINT32_C(0X10000000)
#define DATASIZE_MAX    UINT32_C(0X10000000)

// a constant in the data segment, for sharing identical constants (see cgen_data_const())
typedef struct _cgen_const_t {
    uint32_t    offs;           // offset in the data segment, CGEN_CONST_FREE = unused slot
    uint32_t    size;           // bytes, including the terminating NUL of a string
    uint32_t    hash;
} cgen_const_t;

#define CGEN_CONST_FREE     UINT32_C(0XFFFFFFFF)

// control flow bookkeeping (see cgenctl.c)
typedef struct _cgen_lineaddr_t {
    uint32_t    lineno;
    uint32_t    addr;           // code offset of the line
    size_t      dataindex;      // number of DATA items generated before the line (for RESTORE)
} cgen_lineaddr_t;

typedef struct _cgen_restorefix_t {
    uint32_t    insoffs;        // code offset of the RSTR instruction to patch
    bool        islabel;
    uint32_t    lineno;         // target line number, or ...
    uint16_t    varoffs;        // ... target label variable
    uint32_t    srcline;
} cgen_restorefix_t;

typedef struct _cgen_labeldata_t {
    uint16_t    varoffs;        // label variable
    size_t      dataindex;      // number of DATA items generated before the label
} cgen_labeldata_t;

typedef struct _cgen_fixup_t {
    uint32_t    insoffs;        // code offset of the branch instruction to patch
    uint32_t    lineno;         // target line number
    uint32_t    srcline;        // line number of the statement that refers to it (for error messages)
} cgen_fixup_t;

#define CGEN_LOOP_FOR       UINT8_C(1)
#define CGEN_LOOP_REPEAT    UINT8_C(2)
#define CGEN_LOOP_WHILE     UINT8_C(3)
#define CGEN_LOOP_UNTIL     UINT8_C(4)

#define CGEN_FIXUP_END      UINT32_C(0XFFFFFFFF)    // fixup target: the end of the program

typedef struct _cgen_loopctx_t {
    uint8_t     kind;           // CGEN_LOOP_*
    uint8_t     vartype;        // FOR: type of the loop variable (VARTYPEV_FLOAT or VARTYPEV_INT)
    uint16_t    varoffs;        // FOR: loop variable
    uint16_t    stepvar;        // FOR: hidden variable holding the STEP value
    uint32_t    topaddr;        // where the closing statement branches to (the loop test, or the body for REPEAT)
    uint32_t    bodyaddr;       // start of the body (AGAIN branches here)
    uint32_t    exitins;        // code offset of the PHPA instruction holding the loop exit address (not REPEAT)
    uint32_t    srcline;        // line number of the statement that opened the loop
} cgen_loopctx_t;

typedef struct _codegen_t {
    void*           userdata;
    void            (*report)( struct _codegen_t*, void*, const char* );
    void            (*halt)( struct _codegen_t*, void* ) ATTR_NORETURN;
    uint8_t*        code;           // the code segment (heap; codesize bytes in use, codecap allocated)
    uint8_t*        data;           // the data segment (heap)
    uint32_t        codesize, datasize;
    size_t          codecap, datacap;
    uint32_t        endaddr;        // where END and running off the end of a program go: its HLT instruction (CODEOFFS_NONE: the end of the code)
    bool            writemode;      // (internal) generating the arguments of a WRITE statement
    uint32_t        persistent;     // counts what the code/data generated so far has stored that outlives it (see cgen_note_persistent)
    cgen_const_t*   consts;         // hash table of the shared constants in the data segment (size: power of two)
    size_t          numconsts, maxconsts;
    cgen_lineaddr_t*    lineaddrs;      // line number -> code address, in ascending order
    size_t              numlineaddrs, maxlineaddrs;
    cgen_fixup_t*       fixups;         // forward references to lines
    size_t              numfixups, maxfixups;
    cgen_loopctx_t*     loops;          // loops that are currently open (compile time), innermost last
    size_t              numloops, maxloops;
    size_t              loopbase;       // loops below this number belong to code around the current function body
    uint32_t*           scratch;        // temporary list of code offsets (ON .. GOSUB)
    size_t              numscratch, maxscratch;
    uint32_t            forserial;      // numbers the hidden variables of loops and ON statements
    uint32_t            datafirst, datalast;    // first and last DATA item in the data segment (DATAOFFS_NONE = none)
    uint32_t*           dataoffs;       // the data offsets of all DATA items, in program order
    size_t              numdataitems, maxdataitems;
    cgen_restorefix_t*  restorefix;     // RESTORE statements with a target (patched at the end)
    size_t              numrestorefix, maxrestorefix;
    cgen_labeldata_t*   labeldata;      // where in the DATA items the labels are
    size_t              numlabeldata, maxlabeldata;
} codegen_t;

void init_codegen( codegen_t* cgen );
void cleanup_codegen( codegen_t* cgen );    // frees the bookkeeping tables
void cgen_reset( codegen_t* cgen );         // discards all code and data (keeps the callbacks)
// DEF FN (the function variable gets a code offset), LABEL (same) and DATA (records chained in the data segment) store
// references to what they generate. A direct mode line that did none of this can be discarded after it has run:
// compare <persistent> before and after, then cgen_rollback() to the code and data size from before the line.
void cgen_note_persistent( codegen_t* cgen );
void cgen_rollback( codegen_t* cgen, uint32_t codesize, uint32_t datasize );
bool cgen_alloc_code( codegen_t* cgen, uint32_t size, uint32_t* poffs );
bool cgen_alloc_data( codegen_t* cgen, uint32_t size, uint32_t* poffs );
// Offset of a read-only constant in the data segment: <len> bytes, followed by a NUL if <nul> is set (a string).
// Identical constants are stored once. Don't use it for data that is changed later (DATA items).
bool cgen_data_const( codegen_t* cgen, const void* bytes, uint32_t len, bool nul, uint32_t* poffs );
// Pushes a number: a small integer with PHIM (3 bytes of code, no data), anything else as a shared constant (PHPA, LAN).
bool cgen_push_number( codegen_t* cgen, compiler_t* comp, double value );
// A parameter above 65535 makes the instruction wide (INSF_W) by itself; cgen_gen_ins12_wide() forces the wide form, for
// instructions whose parameter is patched later (cgen_patch_param())
bool cgen_gen_ins( codegen_t* cgen, uint8_t ins, uint8_t ext, uint32_t param );
bool cgen_gen_ins12( codegen_t* cgen, uint16_t ins12, bool code, bool hasparam, uint32_t param );
bool cgen_gen_ins12_wide( codegen_t* cgen, uint16_t ins12, bool code, uint32_t param );
void cgen_patch_param( codegen_t* cgen, uint32_t insoffs, uint32_t value );   // sets the parameter of a wide instruction
bool cgen_gen_ins12_imm17( codegen_t* cgen, uint16_t ins12, int32_t imm17 );
bool cgen_gen_ins4_imm17( codegen_t* cgen, uint8_t ins, int32_t imm );

// DATA item records in the data segment: <type.8> <next.32> <payload>, chained through "next" (DATAOFFS_NONE = last)
#define DATAREC_HEADER      5U
uint32_t cgen_datarec_next( const uint8_t* data, uint32_t pos );
void cgen_datarec_set_next( uint8_t* data, uint32_t pos, uint32_t next );

// special instructions
bool cgen_gen_brk( codegen_t* cgen );
bool cgen_gen_nop( codegen_t* cgen );
bool cgen_gen_phpa_c( codegen_t* cgen, uint32_t offs );     // always wide (the address is usually patched later)
bool cgen_gen_phpa_d( codegen_t* cgen, uint32_t offs );
bool cgen_gen_phim( codegen_t* cgen, int32_t imm );
bool cgen_gen_bria( codegen_t* cgen, int32_t abs_offs );    // always wide
bool cgen_gen_brir( codegen_t* cgen, int32_t rel_offs );    // always wide
bool cgen_gen_jpcc( codegen_t* cgen );
bool cgen_gen_jump( codegen_t* cgen );
bool cgen_gen_drop( codegen_t* cgen, uint16_t cnt );
bool cgen_gen_drop_c( codegen_t* cgen );    // drops the latest return address
bool cgen_gen_line( codegen_t* cgen, uint32_t line );
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
bool cgen_gen_len( codegen_t* cgen );
bool cgen_gen_bins( codegen_t* cgen );
bool cgen_gen_quas( codegen_t* cgen );
bool cgen_gen_octs( codegen_t* cgen );
bool cgen_gen_hexs( codegen_t* cgen );
bool cgen_gen_decs( codegen_t* cgen );
bool cgen_gen_strs( codegen_t* cgen );
bool cgen_gen_chrs( codegen_t* cgen );
bool cgen_gen_lfts( codegen_t* cgen );
bool cgen_gen_rgts( codegen_t* cgen );
bool cgen_gen_mids( codegen_t* cgen );
bool cgen_gen_slft( codegen_t* cgen, uint16_t varoffs );
bool cgen_gen_srgt( codegen_t* cgen, uint16_t varoffs );
bool cgen_gen_smid( codegen_t* cgen, uint16_t varoffs );
bool cgen_gen_eras( codegen_t* cgen, uint16_t varoffs );
bool cgen_gen_rdnm( codegen_t* cgen );
bool cgen_gen_rdst( codegen_t* cgen );
bool cgen_gen_inpl( codegen_t* cgen, uint32_t promptoffs );
bool cgen_gen_inpn( codegen_t* cgen );
bool cgen_gen_inps( codegen_t* cgen );
bool cgen_gen_inpe( codegen_t* cgen );
bool cgen_gen_getk( codegen_t* cgen );
bool cgen_gen_rstr( codegen_t* cgen, bool hasparam, uint32_t dataoffs );     // with a parameter: wide, patched later

// BASIC system command implementations
bool cgen_gen_save( codegen_t* cgen );
bool cgen_gen_load( codegen_t* cgen );
bool cgen_gen_runp( codegen_t* cgen );
bool cgen_gen_edit( codegen_t* cgen );
bool cgen_gen_mrg( codegen_t* cgen, bool append );
bool cgen_gen_chn( codegen_t* cgen, bool merge );
bool cgen_gen_fils( codegen_t* cgen );
bool cgen_gen_tmr( codegen_t* cgen, uint16_t flags );
bool cgen_gen_tmro( codegen_t* cgen, uint16_t flags );
bool cgen_gen_mon( codegen_t* cgen );
bool cgen_gen_snd( codegen_t* cgen, uint16_t kind );
bool cgen_gen_plng( codegen_t* cgen );
bool cgen_gen_rmn( codegen_t* cgen );
bool cgen_gen_hlt( codegen_t* cgen );
bool cgen_gen_open( codegen_t* cgen, uint16_t form );
bool cgen_gen_clos( codegen_t* cgen );
bool cgen_gen_inpw( codegen_t* cgen );
bool cgen_gen_eofn( codegen_t* cgen );
bool cgen_gen_lofn( codegen_t* cgen );
bool cgen_gen_locn( codegen_t* cgen );
bool cgen_gen_inpd( codegen_t* cgen );
bool cgen_gen_addr( codegen_t* cgen, uint16_t swap );
bool cgen_gen_fop( codegen_t* cgen, uint16_t kind );
bool cgen_gen_errt( codegen_t* cgen, uint16_t param );
bool cgen_gen_erl( codegen_t* cgen );
bool cgen_gen_errs( codegen_t* cgen );
bool cgen_gen_gfxc( codegen_t* cgen, uint16_t op );
bool cgen_gen_gfxq( codegen_t* cgen, uint16_t q );
bool cgen_gen_dsgn( codegen_t* cgen, uint16_t param );
bool cgen_gen_drow( codegen_t* cgen );
bool cgen_gen_colt( codegen_t* cgen, uint16_t param );
bool cgen_gen_mki( codegen_t* cgen );
bool cgen_gen_mkl( codegen_t* cgen );
bool cgen_gen_mks( codegen_t* cgen );
bool cgen_gen_mkd( codegen_t* cgen );
bool cgen_gen_cvi( codegen_t* cgen );
bool cgen_gen_cvl( codegen_t* cgen );
bool cgen_gen_cvs( codegen_t* cgen );
bool cgen_gen_cvd( codegen_t* cgen );
bool cgen_gen_fld( codegen_t* cgen, uint16_t varoffs );
bool cgen_gen_lrs( codegen_t* cgen, uint16_t varoffs );
bool cgen_gen_rec( codegen_t* cgen, uint16_t put );
bool cgen_gen_adrs( codegen_t* cgen );
bool cgen_gen_wait( codegen_t* cgen );
bool cgen_gen_list( codegen_t* cgen );
bool cgen_gen_new( codegen_t* cgen );
bool cgen_gen_clr( codegen_t* cgen );
bool cgen_gen_dele( codegen_t* cgen );
bool cgen_gen_renm( codegen_t* cgen );
bool cgen_gen_pchn( codegen_t* cgen );
bool cgen_gen_pnum( codegen_t* cgen, bool hasparam, uint16_t param );
bool cgen_gen_pstr( codegen_t* cgen, bool hasparam, uint16_t param );

// generate code from syntax tree nodes

bool cgen_from_numlit( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_strlit( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_strlits( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_varref_rvalue( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_usrfncall( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_sysfncall( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_numbaseexpr( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_strbaseexpr( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_straddexpr( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_strexpr( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_numunaryex( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_nummultex( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_numaddex( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_numshiftex( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_numcmpex( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_numandex( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_numorex( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_numexpr( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_expr( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_exprlist( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_savestmt( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_chanspec( codegen_t* cgen, compiler_t* comp, node_t* node, bool (*generator)( codegen_t* ) );
bool cgen_from_printarg( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_printarglist( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_printstmt( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_varref_lvalue( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_numassign( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_strassign( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_substrassign( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_anyassign( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_assignlist( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_letstmt( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_erasestmt( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_stmt( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_stmtlist( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_stmtline( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_stmtlines( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_program( codegen_t* cgen, compiler_t* comp );    // code for the whole syntax tree (comp->syntree)

// internal errors (report through comp_error and return false)
bool cgen_bad_node( compiler_t* comp );
bool cgen_unexpected_node( compiler_t* comp );
bool cgen_out_of_code_memory( compiler_t* comp );
bool cgen_out_of_data_memory( compiler_t* comp );
bool cgen_not_implemented_yet( compiler_t* comp );

// -- declarations (cgendecl.c) -----------------------------------------------

bool cgen_from_dimstmt( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_datastmt( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_readstmt( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_gen_channel0( codegen_t* cgen, compiler_t* comp );    // PCHN 0
bool cgen_from_restorestmt( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_inputstmt( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_getstmt( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_finish_data( codegen_t* cgen, compiler_t* comp );     // resolves the RESTORE targets
void cgen_note_label_data( codegen_t* cgen, uint16_t varoffs ); // a LABEL statement is generated (see RESTORE)
bool cgen_from_fnassignstmt( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_defstmt( codegen_t* cgen, compiler_t* comp, node_t* node );

// -- program commands (cgenprog.c) -------------------------------------------

bool cgen_from_loadstmt( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_runstmt( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_editstmt( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_monitorstmt( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_soundstmt( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_waitstmt( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_mergestmt( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_chainstmt( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_filesstmt( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_errtrapstmt( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_oncollstmt( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_gfxstmt( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_gfxquery( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_designstmt( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_designrow( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_timerstmt( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_errtrapstmt( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_openstmt( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_closestmt( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_addressstmt( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_fileopstmt( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_recordstmt( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_liststmt( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_newstmt( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_clearstmt( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_deletestmt( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_renumstmt( codegen_t* cgen, compiler_t* comp, node_t* node );

// finds the code offset of a numbered line of the program compiled last; false if it doesn't exist
const char* cgen_opcode_name( uint16_t opcode );     // mnemonic of a 12 bit opcode, "???" if there is none
bool cgen_has_cd( uint16_t opcode );                // the instruction shows C as the suffix .C / .D
bool cgen_find_line( const codegen_t* cgen, uint32_t lineno, uint32_t* paddr );

// -- control flow (cgenctl.c) ------------------------------------------------

// makes room for one more item in a growable table (items, number in use, allocated, item size)
bool cgen_grow( void** pitems, size_t* pnum, size_t* pmax, size_t itemsize );
void cgen_begin_control( codegen_t* cgen );     // clears the line table, fixups and FOR stack
bool cgen_note_line( codegen_t* cgen, compiler_t* comp, uint32_t lineno );  // line starts at the current code offset
bool cgen_finish_control( codegen_t* cgen, compiler_t* comp );              // resolves line references, checks FOR/NEXT

// code for a branch that is a statement, a statement list, a statement line, or statement lines
bool cgen_from_block( codegen_t* cgen, compiler_t* comp, node_t* node );
// checks that all loops opened since <base> have been closed (reports the innermost open one)
bool cgen_loops_closed( codegen_t* cgen, compiler_t* comp, size_t base );
bool cgen_from_gotostmt( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_returnstmt( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_labelstmt( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_forstmt( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_nextstmt( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_endstmt( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_popstmt( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_againstmt( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_repeatstmt( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_untilstmt( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_whilestmt( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_wendstmt( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_uendstmt( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_onstmt( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_singlelineifstmt( codegen_t* cgen, compiler_t* comp, node_t* node );
bool cgen_from_multilineifstmt( codegen_t* cgen, compiler_t* comp, node_t* node );

#endif
