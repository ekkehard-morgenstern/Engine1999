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

#define NODE_NONE           ((node_t*) 0)

/*
The syntax tree is temporary for the compiler run. Every node is a separate heap allocation
with a variable number of branches (growable pointer array) and optional trailing data.
All nodes created by a compiler are chained together (node_t.chain), so they can be freed at once
(including nodes of abandoned parses) by cleanup_compiler().
*/

typedef struct _node_t {
    struct _node_t*     chain;          // next node allocated by the compiler (for freeing)
    struct _node_t**    branches;       // growable array of branch nodes, 0 if none
    size_t              numbranches;
    size_t              maxbranches;    // allocated size of the branches array
    uint16_t            datalen;
    uint8_t             type;           // NT_*
    uint8_t             data[];         // datalen bytes
} node_t;

#ifndef NODETYPE_H
#include "nodetype.h"
#endif

#ifndef VARS_H
#include "vars.h"
#endif

typedef struct _comp_ctxstk_t {
    struct _comp_ctxstk_t*  prev;
    pgmiter_t               iter;
    uint8_t*                tokp;
    const uint8_t*          tokend;
    uint8_t                 currtok;
    char*                   param;      // heap allocated, owned by the context
    double                  number;
} comp_ctxstk_t;

struct _runtime_t;

typedef struct _compiler_t {
    struct _runtime_t* rt;
    pgmiter_t       iter;
    uint8_t*        tokp;
    const uint8_t*  tokend;         // end of the current line's tokens (see comp_set_tokens)
    uint8_t         currtok;
    char*           param;          // heap allocated text of the current literal/identifier token
    double          number;         // value of the current numeric literal token
    char*           name;           // buffer for comp_node_name()
    node_t*         curfn;          // name node of the multi-line user function whose body is being parsed
    size_t          namealloc;
    comp_ctxstk_t*  ctxstk;
    void*           userdata;
    void            (*report)( struct _compiler_t*, void*, const char* );
    void            (*halt)( struct _compiler_t*, void* ) ATTR_NORETURN;
    node_t*         nodes;          // chain of all nodes allocated so far
    uint16_t        arraydim[MAXDIM];
    uint8_t         numdim;
    node_t*         syntree;
} compiler_t;

void init_compiler( compiler_t* comp, struct _runtime_t* rt, program_t* pgm, bool keepmemory );
void cleanup_compiler( compiler_t* comp );     // frees param and the context stack; call before init_compiler on a reused compiler
// returns the name stored in a node (after <skip> bytes of data) as a NUL-terminated string; valid until the next call
const char* comp_node_name( compiler_t* comp, const node_t* node, size_t skip );
// the same for a prefix followed by <len> bytes of a name
const char* comp_prefixed_name( compiler_t* comp, const char* prefix, const void* name, size_t len );
void comp_set_tokens( compiler_t* comp, const uint8_t* tokens, size_t size );
void comp_error( compiler_t* comp, const char* text ) ATTR_NORETURN;
void comp_push_context( compiler_t* comp );
void comp_commit_context( compiler_t* comp );
void comp_pop_context( compiler_t* comp );

void comp_free_tree( compiler_t* comp );       // frees all syntax tree nodes

// the variable arguments are numbranches node_t* pointers
bool comp_create_node( compiler_t* comp, node_t** pnode, uint8_t nodetype, uint8_t numbranches, uint16_t datalen,
    const void* pdata, ... );
bool comp_add_branch( compiler_t* comp, node_t* node, node_t* branch );
bool comp_node_iter_branches( compiler_t* comp, node_t* node, void* userdata, bool (*callback)( void*, node_t* ) );
double comp_extract_float( const uint8_t* mem, uint16_t offs );

typedef bool (*comp_eatfn_t)( compiler_t*, node_t** );

bool comp_eat_list( compiler_t* comp, node_t** pnode, uint8_t nodetype, comp_eatfn_t element_eater, uint8_t septok,
    const char* errortext );
bool comp_eat_list2( compiler_t* comp, node_t** pnode, uint8_t nodetype, comp_eatfn_t element_eater, const uint8_t* septoks,
    const char* errortext, bool oneoperator, bool forceexpr );

bool comp_eat_numexlist( compiler_t* comp, node_t** pnode );
bool comp_eat_strexlist( compiler_t* comp, node_t** pnode );
bool comp_eat_exprlist( compiler_t* comp, node_t** pnode );
bool comp_eat_arrayindex( compiler_t* comp, node_t** pnode );
bool comp_eat_arraysub( compiler_t* comp, node_t** pnode );
bool comp_eat_arraydimdecl( compiler_t* comp, node_t** pnode );
bool comp_eat_arraydecl( compiler_t* comp, node_t** pnode );
bool comp_eat_arraydecllist( compiler_t* comp, node_t** pnode );
bool comp_eat_emptyarrayref( compiler_t* comp, node_t** pnode );
bool comp_eat_emptyarrayreflist( compiler_t* comp, node_t** pnode );
bool comp_eat_numbasevarref( compiler_t* comp, node_t** pnode );
bool comp_eat_numvarref( compiler_t* comp, node_t** pnode );
bool comp_eat_strbasevarref( compiler_t* comp, node_t** pnode );
bool comp_eat_strvarref( compiler_t* comp, node_t** pnode );
bool comp_eat_anybasevarref( compiler_t* comp, node_t** pnode );
bool comp_eat_numlit( compiler_t* comp, node_t** pnode );
bool comp_eat_strlit( compiler_t* comp, node_t** pnode );
bool comp_eat_strlits( compiler_t* comp, node_t** pnode );
bool comp_eat_numusrfnname( compiler_t* comp, node_t** pnode );
bool comp_eat_strusrfnname( compiler_t* comp, node_t** pnode );
bool comp_eat_usrfnarg( compiler_t* comp, node_t** pnode );
bool comp_eat_usrfnarglist( compiler_t* comp, node_t** pnode );
bool comp_eat_anyusrfnname( compiler_t* comp, node_t** pnode );
bool comp_eat_singlelineusrfnbody( compiler_t* comp, node_t** pnode );
bool comp_eat_multilineusrfnbody( compiler_t* comp, node_t** pnode );
bool comp_eat_usrfnbody( compiler_t* comp, node_t** pnode );
bool comp_eat_usrfndecl( compiler_t* comp, node_t** pnode );
bool comp_eat_numusrfncall( compiler_t* comp, node_t** pnode );
bool comp_eat_strusrfncall( compiler_t* comp, node_t** pnode );
bool comp_eat_sysnumfn1argcall( compiler_t* comp, node_t** pnode );
bool comp_eat_sysstrfn2argcall( compiler_t* comp, node_t** pnode );
bool comp_eat_sysnumfuncargcall( compiler_t* comp, node_t** pnode );
bool comp_eat_sysstrfuncargcall( compiler_t* comp, node_t** pnode );
bool comp_eat_sysnoargnumcall( compiler_t* comp, node_t** pnode );
bool comp_eat_sysnoargstrcall( compiler_t* comp, node_t** pnode );
bool comp_eat_numfunccall( compiler_t* comp, node_t** pnode );
bool comp_eat_strfunccall( compiler_t* comp, node_t** pnode );
bool comp_eat_strbaseexpr( compiler_t* comp, node_t** pnode );
bool comp_eat_straddexpr( compiler_t* comp, node_t** pnode );
bool comp_eat_strexpr( compiler_t* comp, node_t** pnode );
bool comp_eat_numsubexpr( compiler_t* comp, node_t** pnode );
bool comp_eat_numbaseexpr( compiler_t* comp, node_t** pnode );
bool comp_eat_numunaryex( compiler_t* comp, node_t** pnode );
bool comp_eat_nummultex( compiler_t* comp, node_t** pnode );
bool comp_eat_numaddex( compiler_t* comp, node_t** pnode );
bool comp_eat_numshiftex( compiler_t* comp, node_t** pnode );
bool comp_eat_numcmpex( compiler_t* comp, node_t** pnode );
bool comp_eat_numandex( compiler_t* comp, node_t** pnode );
bool comp_eat_numorex( compiler_t* comp, node_t** pnode );
bool comp_eat_numexpr( compiler_t* comp, node_t** pnode );
bool comp_eat_expr( compiler_t* comp, node_t** pnode );
bool comp_eat_savestmt( compiler_t* comp, node_t** pnode );
bool comp_eat_chanspec( compiler_t* comp, node_t** pnode );
bool comp_eat_printsep( compiler_t* comp, node_t** pnode );
bool comp_eat_printarg( compiler_t* comp, node_t** pnode );
bool comp_eat_printarglist( compiler_t* comp, node_t** pnode );
bool comp_eat_printstmt( compiler_t* comp, node_t** pnode );
bool comp_eat_datastmt( compiler_t* comp, node_t** pnode );
bool comp_eat_inputstmt( compiler_t* comp, node_t** pnode );
bool comp_eat_getstmt( compiler_t* comp, node_t** pnode );
bool comp_eat_readstmt( compiler_t* comp, node_t** pnode );
bool comp_eat_restorestmt( compiler_t* comp, node_t** pnode );
bool comp_eat_iostmt( compiler_t* comp, node_t** pnode );
bool comp_eat_loadstmt( compiler_t* comp, node_t** pnode );
bool comp_eat_runstmt( compiler_t* comp, node_t** pnode );
bool comp_eat_editstmt( compiler_t* comp, node_t** pnode );
bool comp_eat_monitorstmt( compiler_t* comp, node_t** pnode );
bool comp_eat_soundstmt( compiler_t* comp, node_t** pnode );
bool comp_eat_waitstmt( compiler_t* comp, node_t** pnode );
bool comp_eat_mergestmt( compiler_t* comp, node_t** pnode );
bool comp_eat_chainstmt( compiler_t* comp, node_t** pnode );
bool comp_eat_filesstmt( compiler_t* comp, node_t** pnode );
bool comp_eat_timerstmt( compiler_t* comp, node_t** pnode );
bool comp_eat_openstmt( compiler_t* comp, node_t** pnode );
bool comp_eat_closestmt( compiler_t* comp, node_t** pnode );
bool comp_eat_addressstmt( compiler_t* comp, node_t** pnode );
bool comp_eat_fileopstmt( compiler_t* comp, node_t** pnode );
bool comp_eat_recordstmt( compiler_t* comp, node_t** pnode );
bool comp_eat_gfxstmt( compiler_t* comp, node_t** pnode );
bool comp_eat_gfxquerycall( compiler_t* comp, node_t** pnode );
bool comp_eat_designstmt( compiler_t* comp, node_t** pnode );
bool comp_eat_randomizestmt( compiler_t* comp, node_t** pnode );
bool comp_eat_anglestmt( compiler_t* comp, node_t** pnode );
bool comp_eat_liststmt( compiler_t* comp, node_t** pnode );
bool comp_eat_newstmt( compiler_t* comp, node_t** pnode );
bool comp_eat_clearstmt( compiler_t* comp, node_t** pnode );
bool comp_eat_deletestmt( compiler_t* comp, node_t** pnode );
bool comp_eat_renumstmt( compiler_t* comp, node_t** pnode );
bool comp_eat_progstmt( compiler_t* comp, node_t** pnode );
bool comp_eat_numassign( compiler_t* comp, node_t** pnode );
bool comp_eat_strassign( compiler_t* comp, node_t** pnode );
bool comp_eat_substrassign( compiler_t* comp, node_t** pnode );
bool comp_eat_anyassign( compiler_t* comp, node_t** pnode );
bool comp_eat_assignlist( compiler_t* comp, node_t** pnode );
bool comp_eat_letstmt( compiler_t* comp, node_t** pnode );
bool comp_eat_dimstmt( compiler_t* comp, node_t** pnode );
bool comp_eat_erasestmt( compiler_t* comp, node_t** pnode );
bool comp_eat_defstmt( compiler_t* comp, node_t** pnode );
bool comp_eat_fnassignstmt( compiler_t* comp, node_t** pnode );
bool comp_eat_assignstmt( compiler_t* comp, node_t** pnode );
bool comp_eat_forstmt( compiler_t* comp, node_t** pnode );
bool comp_eat_nextstmt( compiler_t* comp, node_t** pnode );
bool comp_eat_gototarget( compiler_t* comp, node_t** pnode );
bool comp_eat_gotostmt( compiler_t* comp, node_t** pnode );
bool comp_eat_returnstmt( compiler_t* comp, node_t** pnode );
bool comp_eat_labelstmt( compiler_t* comp, node_t** pnode );
bool comp_eat_singlelineifstmt( compiler_t* comp, node_t** pnode );
bool comp_eat_multilineifstmt( compiler_t* comp, node_t** pnode );
bool comp_eat_endstmt( compiler_t* comp, node_t** pnode );
bool comp_eat_popstmt( compiler_t* comp, node_t** pnode );
bool comp_eat_againstmt( compiler_t* comp, node_t** pnode );
bool comp_eat_exitstmt( compiler_t* comp, node_t** pnode );
bool comp_eat_resumestmt( compiler_t* comp, node_t** pnode );
bool comp_eat_procstmt( compiler_t* comp, node_t** pnode );
bool comp_eat_callstmt( compiler_t* comp, node_t** pnode );
bool comp_eat_localstmt( compiler_t* comp, node_t** pnode );
bool comp_eat_repeatstmt( compiler_t* comp, node_t** pnode );
bool comp_eat_untilstmt( compiler_t* comp, node_t** pnode );
bool comp_eat_whilestmt( compiler_t* comp, node_t** pnode );
bool comp_eat_wendstmt( compiler_t* comp, node_t** pnode );
bool comp_eat_uendstmt( compiler_t* comp, node_t** pnode );
bool comp_eat_onstmt( compiler_t* comp, node_t** pnode );
bool comp_eat_controlflowstmt( compiler_t* comp, node_t** pnode );
bool comp_eat_fnassignstmt( compiler_t* comp, node_t** pnode );
bool comp_eat_stmt( compiler_t* comp, node_t** pnode );
bool comp_eat_stmtlist( compiler_t* comp, node_t** pnode );
bool comp_eat_stmtline( compiler_t* comp, node_t** pnode );
bool comp_eat_stmtlines( compiler_t* comp, node_t** pnode );

bool comp_nextline( compiler_t* comp );
bool comp_fetchtok( compiler_t* comp );
bool begin_comp( compiler_t* comp );
void run_compiler( compiler_t* comp );

#endif
