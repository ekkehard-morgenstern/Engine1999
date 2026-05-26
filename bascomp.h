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

#ifndef BASCOMP_H
#define BASCOMP_H   1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

#ifndef BASTOK_H
#include "bastok.h"
#endif

#ifndef BASLIN_H
#include "baslin.h"
#endif

#ifndef BASPGM_H
#include "baspgm.h"
#endif

/*
See bascomp.ebnf for syntax definition.
*/

#define NODEOFFS_NONE       UINT16_C(0XFFFF)
#define NODEHDR_SIZE        UINT8_C(8)
#define BRANCHENT_SIZE      UINT8_C(4)

#define EXTRACT16( comp, offs ) \
    ( ( ((uint16_t)( (comp)->tree[ offs ] )) << UINT8_C(8) ) | \
    ( (comp)->tree[ (offs) + 1U ] ) )

#define WRITE16( comp, offs, value ) \
    { \
        (comp)->tree[ offs ] = (uint8_t)( (value) >> UINT8_C(8) ); \
        (comp)->tree[ (offs) + 1U ] = (uint8_t) (value); \
    }


/*
The syntax tree is temporary for the compiler run and organized as follows:

    <nodetype.8> <numbranches.8> <datalen.16> <firstbranch.16> <lastbranch.16> <data...>

For every branch entry:

    <nodepos.16> <nextbranch.16>

16-bit values are stored in network byte order (big endian).
*/

#ifndef NODETYPE_H
#include "nodetype.h"
#endif

#ifndef VARS_H
#include "vars.h"
#endif

#ifndef RUNTIME_H
#include "runtime.h"
#endif

#define TREESIZE_MAX    65535U

typedef struct _comp_ctxstk_t {
    struct _comp_ctxstk_t*  prev;
    pgmiter_t               iter;
    uint8_t*                tokp;
    uint8_t                 currtok;
    union {
        char                param[256];
        double              number;
    };
} comp_ctxstk_t;

typedef struct _compiler_t {
    runtime_t*      rt;
    pgmiter_t       iter;
    uint8_t*        tokp;
    uint8_t         currtok;
    union {
        char        param[256];
        double      number;
    };
    comp_ctxstk_t*  ctxstk;
    void*           userdata;
    void            (*report)( struct _compiler_t*, void*, const char* );
    void            (*halt)( struct _compiler_t*, void* ) ATTR_NORETURN;
    uint8_t         tree[TREESIZE_MAX];
    uint32_t        treesize;
    uint16_t        arraydim[MAXDIM];
    uint8_t         numdim;
} compiler_t;

void init_compiler( compiler_t* comp, runtime_t* rt, program_t* pgm, bool keepmemory );
void comp_error( compiler_t* comp, const char* text ) ATTR_NORETURN;
void comp_push_context( compiler_t* comp );
void comp_commit_context( compiler_t* comp );
void comp_pop_context( compiler_t* comp );

bool comp_alloc_tree( compiler_t* comp, uint16_t size, uint16_t* poffs );

bool comp_create_node( compiler_t* comp, uint16_t* pnodeoffs, uint8_t nodetype, uint8_t numbranches, uint16_t datalen,
    const void* pdata, ... );
bool comp_add_branch( compiler_t* comp, uint16_t nodeoffs, uint16_t branchoffs );

typedef bool (*comp_eatfn_t)( compiler_t*, uint16_t* );

bool comp_eat_list( compiler_t* comp, uint16_t* pnodeoffs, uint8_t nodetype, comp_eatfn_t element_eater, uint8_t septok,
    const char* errortext );
bool comp_eat_list2( compiler_t* comp, uint16_t* pnodeoffs, uint8_t nodetype, comp_eatfn_t element_eater, const uint8_t* septoks,
    const char* errortext, bool oneoperator, bool forceexpr );

bool comp_eat_numexlist( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_strexlist( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_exprlist( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_arrayindex( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_arraysub( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_arraydimdecl( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_arraydecl( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_arraydecllist( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_emptyarrayref( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_emptyarrayreflist( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numbasevarref( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numvarref( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_strbasevarref( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_strvarref( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_anybasevarref( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numlit( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_strlit( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_strlits( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numusrfnname( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_strusrfnname( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_usrfnarg( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_usrfnarglist( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_anyusrfnname( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_singlelineusrfnbody( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_multilineusrfnbody( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_usrfnbody( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_usrfndecl( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numusrfncall( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_strusrfncall( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_sysnumfunc( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_sysstrfunc( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_sysnumfuncargcall( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_sysstrfuncargcall( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_sysnoargnumcall( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_sysnoargstrcall( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numfunccall( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_strfunccall( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_strbaseexpr( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_straddexpr( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_strexpr( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numsubexpr( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numbaseexpr( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numunaryex( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_nummultex( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numaddex( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numshiftex( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numcmpex( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numandex( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numorex( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numexpr( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_expr( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_savestmt( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_chanspec( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_printsep( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_printarg( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_printarglist( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_printstmt( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_iostmt( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numassign( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_strassign( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_substrop( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_substrassign( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_anyassign( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_assignlist( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_letstmt( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_dimstmt( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_erasestmt( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_defstmt( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_assignstmt( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_forstmt( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_nextstmt( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_gotokw( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_gototarget( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_gotostmt( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_returnstmt( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_labelstmt( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_singlelineifstmt( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_multilineifstmt( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_controlflowstmt( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_stmt( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_stmtlist( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_stmtline( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_stmtlines( compiler_t* comp, uint16_t* pnodeoffs );


bool comp_nextline( compiler_t* comp );
bool comp_fetchtok( compiler_t* comp );
bool begin_comp( compiler_t* comp );
void run_compiler( compiler_t* comp );

#endif
