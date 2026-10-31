
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
#include "runtime.h"

/*
Code generation for declarations.

DIM:    nothing to generate. The compiler already created the array when it parsed the declaration
        (the dimensions are constants), so the array exists before the program runs.

DEF FN: the function body is generated inline, jumped over by the code flow:
            BRIA end   start: <expression>  RETF   end:
        The code offset of "start" is stored in the function variable at compile time, so a call (CALF) can
        be generated, and executed, regardless of where the definition appears in the program.
        The parameter variables are made sure to exist, because CALF assigns the arguments to them.
        Multi-line function bodies are not supported yet (it isn't defined how they return their value).
*/

bool cgen_from_dimstmt( codegen_t* cgen, compiler_t* comp, node_t* node ) {
    /*
        NT_DIMSTMT      DIM statement
            branches:
                - 1 branch of array declaration list
    */
    if ( node == NODE_NONE ) {
        return cgen_bad_node( comp );
    }
    if ( node->type != NT_DIMSTMT || node->numbranches != 1U ) {
        return cgen_unexpected_node( comp );
    }
    return true;
}

// the hidden variable that holds the result of a multi-line function: "@FN" + the name of the function
static bool cd_result_var( compiler_t* comp, uint8_t basetype, const char* prefixedname, uint16_t* pvaroffs ) {
    uint16_t varoffs = VAROFFS_NONE;
    if ( !vmem_lookup_var( &comp->rt->varmem, basetype, prefixedname, &varoffs ) || varoffs == VAROFFS_NONE ) {
        varoffs = VAROFFS_NONE;
        if ( !vmem_create_var( &comp->rt->varmem, basetype, prefixedname, UINT8_C(0), 0, UINT8_C(0), 0, &varoffs ) ||
             varoffs == VAROFFS_NONE ) {
            comp_error( comp, "Out of memory" );
            return false;
        }
    }
    *pvaroffs = varoffs;
    return true;
}

static bool cd_write_var( codegen_t* cgen, uint8_t basetype, uint16_t varoffs ) {
    switch ( basetype ) {
        case VARTYPEV_INT:  return cgen_gen_wriv( cgen, varoffs );
        case VARTYPEV_STR:  return cgen_gen_wrsv( cgen, varoffs );
        default:            return cgen_gen_wrnv( cgen, varoffs );
    }
}

bool cgen_from_fnassignstmt( codegen_t* cgen, compiler_t* comp, node_t* node ) {
    /*
        NT_FNASSIGNSTMT     assignment of the result of a multi-line user function
            data:
                - 1 byte of type indicator, n bytes of function name
            branches:
                - 1 branch of expression
    */
    if ( node == NODE_NONE ) {
        return cgen_bad_node( comp );
    }
    if ( node->type != NT_FNASSIGNSTMT || node->datalen < UINT16_C(2) || node->numbranches != 1U ) {
        return cgen_unexpected_node( comp );
    }
    uint8_t basetype = node->data[0] & VARTYPEM_BASE;
    uint16_t varoffs = VAROFFS_NONE;
    if ( !cd_result_var( comp, basetype, comp_prefixed_name( comp, "@FN", &node->data[1], node->datalen - 1U ), &varoffs ) ) {
        return false;
    }
    if ( !cgen_from_expr( cgen, comp, node->branches[0] ) ) {
        return false;
    }
    if ( !cd_write_var( cgen, basetype, varoffs ) ) {
        return cgen_out_of_code_memory( comp );
    }
    return true;
}

static bool cd_ensure_param_vars( compiler_t* comp, uint16_t funcoffs, uint8_t numparams ) {
    for ( uint8_t i=0; i < numparams; ++i ) {
        uint8_t ptype = 0; const char* name = 0; uint16_t poffs = VAROFFS_NONE;
        if ( !vmem_get_func_param( &comp->rt->varmem, funcoffs, i, &ptype, &name ) ) {
            comp_error( comp, "Internal error (bad function parameter)" );
            return false;
        }
        if ( ptype & VARTYPEF_ARRAY ) {
            comp_error( comp, "Array parameters are not supported yet" );
            return false;
        }
        if ( !vmem_lookup_var( &comp->rt->varmem, ptype, name, &poffs ) || poffs == VAROFFS_NONE ) {
            poffs = VAROFFS_NONE;
            if ( !vmem_create_var( &comp->rt->varmem, ptype, name, UINT8_C(0), 0, UINT8_C(0), 0, &poffs ) ||
                 poffs == VAROFFS_NONE ) {
                comp_error( comp, "Out of memory" );
                return false;
            }
        }
    }
    return true;
}

bool cgen_from_procstmt( codegen_t* cgen, compiler_t* comp, node_t* node ) {
    /*
        NT_PROCSTMT     PROC / FUNC definition
            data:
                - 1 byte of function type, 2 bytes of variable offset
            branches:
                - 1 branch of statement lines
        Code:   BRIA end   start: [ LOCL result ]  <body>  exits:  [ result ]  RETF   end:
        A FUNC keeps its value in the hidden variable "@FN<name>", which is local to the call (saved by LOCL, so a call of the
        function inside its own body doesn't clobber it); RETF leaves the value on the data stack and then gives the variable back.
    */
    if ( node == NODE_NONE ) {
        return cgen_bad_node( comp );
    }
    if ( node->type != NT_PROCSTMT || node->datalen != UINT16_C(3) || node->numbranches != 1U ) {
        return cgen_unexpected_node( comp );
    }
    uint8_t  functype = node->data[0];
    uint16_t funcoffs = (uint16_t)( ( ( (uint16_t) node->data[1] ) << UINT8_C(8) ) | node->data[2] );
    uint8_t  basetype = functype & VARTYPEM_BASE;
    bool     isfunc   = basetype != VARTYPEV_LABEL;
    if ( cgen->inproc ) {
        comp_error( comp, isfunc ? "FUNC inside a PROC or FUNC" : "PROC inside a PROC or FUNC" );
        return false;
    }
    uint8_t ftype = 0, numparams = 0; uint32_t oldcode = CODEOFFS_NONE;
    if ( !vmem_get_func_info( &comp->rt->varmem, funcoffs, &ftype, &numparams, &oldcode ) || ftype != functype ) {
        comp_error( comp, "Internal error (function variable not found)" );
        return false;
    }
    if ( !cd_ensure_param_vars( comp, funcoffs, numparams ) ) {
        return false;
    }
    uint16_t resoffs = VAROFFS_NONE;
    if ( isfunc ) {
        var_t* fvar = vmem_var( &comp->rt->varmem, funcoffs );
        if ( fvar == 0 || !cd_result_var( comp, basetype, comp_prefixed_name( comp, "@FN", fvar->name, strlen( fvar->name ) ), &resoffs ) ) {
            return false;
        }
    }
    uint32_t skipins = cgen->codesize;
    if ( !cgen_gen_bria( cgen, 0 ) ) {
        return cgen_out_of_code_memory( comp );
    }
    uint32_t start = cgen->codesize;
    size_t savedbase = cgen->loopbase;
    cgen->loopbase = cgen->numloops;
    cgen->inproc = true;
    cgen->procresult = isfunc;
    cgen->procbase = basetype;
    cgen->procresvar = resoffs;
    if ( isfunc && !cgen_gen_locl( cgen, resoffs ) ) {
        return cgen_out_of_code_memory( comp );
    }
    if ( node->branches[0] != NODE_NONE && !cgen_from_block( cgen, comp, node->branches[0] ) ) {
        return false;
    }
    if ( !cgen_loops_closed( cgen, comp, cgen->loopbase ) ) {
        return false;
    }
    cgen_resolve_procexits( cgen, cgen->codesize );
    cgen->inproc = false;
    cgen->procresult = false;
    cgen->loopbase = savedbase;
    if ( isfunc ) {
        bool ok = basetype == VARTYPEV_INT ? cgen_gen_rriv( cgen, resoffs ) :
            basetype == VARTYPEV_STR ? cgen_gen_rrsv( cgen, resoffs ) : cgen_gen_rrnv( cgen, resoffs );
        if ( !ok ) {
            return cgen_out_of_code_memory( comp );
        }
    }
    if ( !cgen_gen_retf( cgen ) ) {
        return cgen_out_of_code_memory( comp );
    }
    cgen_patch_param( cgen, skipins, cgen->codesize );
    varvalue_t value;
    memset( &value, 0, sizeof(value) );
    value.codeoffs = start;
    cgen_note_persistent( cgen );
    if ( !vmem_set_var( &comp->rt->varmem, funcoffs, functype, 0, 0, &value ) ) {
        comp_error( comp, "Internal error (failed to store the function address)" );
        return false;
    }
    return true;
}

bool cgen_from_callstmt( codegen_t* cgen, compiler_t* comp, node_t* node ) {
    /*
        NT_CALLSTMT     CALL statement
            data: 1 byte of type indicator, n bytes of the procedure name
            branches: the argument expression list (can be empty)
        Code:   <arguments> <count> CALF(proc)
    */
    if ( node == NODE_NONE ) {
        return cgen_bad_node( comp );
    }
    if ( node->type != NT_CALLSTMT || node->datalen < UINT16_C(2) || node->numbranches != 1U ) {
        return cgen_unexpected_node( comp );
    }
    uint16_t funcoffs = VAROFFS_NONE;
    const char* name = comp_node_name( comp, node, 1U );
    if ( !vmem_lookup_var( &comp->rt->varmem, VARTYPEV_LABEL | VARTYPEF_FUNC, name, &funcoffs ) || funcoffs == VAROFFS_NONE ) {
        comp_error( comp, "Undefined procedure" );
        return false;
    }
    uint8_t ftype = 0, numparams = 0; uint32_t codeoffs = CODEOFFS_NONE;
    if ( !vmem_get_func_info( &comp->rt->varmem, funcoffs, &ftype, &numparams, &codeoffs ) ) {
        comp_error( comp, "Undefined procedure" );
        return false;
    }
    node_t* args = node->branches[0];
    size_t numargs = args == NODE_NONE ? 0U : args->type == NT_EXPRLIST ? args->numbranches : 1U;
    if ( numargs != numparams ) {
        comp_error( comp, "Argument count mismatch" );
        return false;
    }
    if ( !cgen_gen_call_args( cgen, comp, node ) ) {
        return false;
    }
    if ( !cgen_gen_calf( cgen, funcoffs ) ) {
        return cgen_out_of_code_memory( comp );
    }
    return true;
}

bool cgen_from_defstmt( codegen_t* cgen, compiler_t* comp, node_t* node ) {
    /*
        NT_DEFSTMT      DEF statement
            branches:
                - 1 branch of user function declaration

        NT_USRFNDECL
            data:
                - 1 byte of function type
                - 2 bytes of variable offset
            branches:
                - 1 branch of expression or statement list (the function body)

        NT_SINGLELINEUSERFNBODY
            branches:
                - 1 branch of expression
    */
    if ( node == NODE_NONE ) {
        return cgen_bad_node( comp );
    }
    if ( node->type != NT_DEFSTMT || node->numbranches != 1U ) {
        return cgen_unexpected_node( comp );
    }
    node_t* decl = node->branches[0];
    if ( decl == NODE_NONE || decl->type != NT_USRFNDECL || decl->datalen != UINT16_C(3) || decl->numbranches != 1U ) {
        return cgen_bad_node( comp );
    }
    uint8_t  functype = decl->data[0];
    uint16_t funcoffs = (uint16_t)( ( ( (uint16_t) decl->data[1] ) << UINT8_C(8) ) | decl->data[2] );
    node_t*  body = decl->branches[0];
    if ( body == NODE_NONE ) {
        return cgen_bad_node( comp );
    }
    bool multiline = body->type == NT_MULTILINEUSERFNBODY;
    if ( ( !multiline && body->type != NT_SINGLELINEUSERFNBODY ) || body->numbranches != 1U ) {
        return cgen_unexpected_node( comp );
    }
    uint8_t ftype = 0, numparams = 0; uint32_t oldcode = CODEOFFS_NONE;
    if ( !vmem_get_func_info( &comp->rt->varmem, funcoffs, &ftype, &numparams, &oldcode ) || ftype != functype ) {
        comp_error( comp, "Internal error (function variable not found)" );
        return false;
    }
    if ( !cd_ensure_param_vars( comp, funcoffs, numparams ) ) {
        return false;
    }
    // jump over the function body
    uint32_t skipins = cgen->codesize;
    if ( !cgen_gen_bria( cgen, 0 ) ) {
        return cgen_out_of_code_memory( comp );
    }
    uint32_t start = cgen->codesize;
    // loops opened outside of the function are not visible inside (AGAIN, NEXT, WEND ...) and vice versa
    size_t savedbase = cgen->loopbase;
    cgen->loopbase = cgen->numloops;
    if ( !multiline ) {
        if ( !cgen_from_expr( cgen, comp, body->branches[0] ) ) {
            return false;
        }
    } else {
        // the result lives in a hidden variable; it starts as 0 or the empty string, the body assigns it
        uint8_t basetype = functype & VARTYPEM_BASE;
        uint16_t resoffs = VAROFFS_NONE;
        var_t* fvar = vmem_var( &comp->rt->varmem, funcoffs );
        if ( fvar == 0 || !cd_result_var( comp, basetype, comp_prefixed_name( comp, "@FN", fvar->name, strlen( fvar->name ) ), &resoffs ) ) {
            return false;
        }
        if ( basetype == VARTYPEV_STR ) {
            uint32_t emptyoffs = DATAOFFS_NONE;
            if ( !cgen_data_const( cgen, 0, 0, true, &emptyoffs ) || emptyoffs == DATAOFFS_NONE ) {
                return cgen_out_of_data_memory( comp );
            }
            if ( !cgen_gen_phpa_d( cgen, emptyoffs ) || !cgen_gen_las_d( cgen ) ) {
                return cgen_out_of_code_memory( comp );
            }
        } else if ( !cgen_gen_phim( cgen, 0 ) ) {
            return cgen_out_of_code_memory( comp );
        }
        if ( !cd_write_var( cgen, basetype, resoffs ) ) {
            return cgen_out_of_code_memory( comp );
        }
        // RETURN value, EXIT FN and LOCAL work in the body
        cgen->inproc = true;
        cgen->procresult = true;
        cgen->procbase = basetype;
        cgen->procresvar = resoffs;
        if ( !cgen_from_block( cgen, comp, body->branches[0] ) ) {
            return false;
        }
        cgen_resolve_procexits( cgen, cgen->codesize );
        cgen->inproc = false;
        cgen->procresult = false;
        bool ok = basetype == VARTYPEV_INT ? cgen_gen_rriv( cgen, resoffs ) :
            basetype == VARTYPEV_STR ? cgen_gen_rrsv( cgen, resoffs ) : cgen_gen_rrnv( cgen, resoffs );
        if ( !ok ) {
            return cgen_out_of_code_memory( comp );
        }
    }
    if ( !cgen_loops_closed( cgen, comp, cgen->loopbase ) ) {
        return false;
    }
    cgen->loopbase = savedbase;
    if ( !cgen_gen_retf( cgen ) ) {
        return cgen_out_of_code_memory( comp );
    }
    // patch the jump over the body (BRIA is wide)
    cgen_patch_param( cgen, skipins, cgen->codesize );
    // store the code offset in the function variable
    varvalue_t value;
    memset( &value, 0, sizeof(value) );
    value.codeoffs = start;
    cgen_note_persistent( cgen );       // the function variable now points into the code
    if ( !vmem_set_var( &comp->rt->varmem, funcoffs, functype, 0, 0, &value ) ) {
        comp_error( comp, "Internal error (failed to store the function address)" );
        return false;
    }
    return true;
}

// -- DATA, READ, RESTORE -------------------------------------------------------------

/*
DATA generates no code. Every item becomes a record in the data segment: <type.8> <next.16> <payload>, chained in
program order (see instset.h, RDNM). READ is RDNM / RDST and the same store as an assignment; RESTORE is RSTR.
The target of a RESTORE is the first item at or behind a line (or label), found when all items are known.
*/

static bool cd_add_data_item( codegen_t* cgen, compiler_t* comp, uint8_t type, const void* payload, size_t paylen ) {
    if ( paylen > UINT16_MAX - 3U ) {
        comp_error( comp, "Data item too long" );
        return false;
    }
    uint32_t off = DATAOFFS_NONE;
    if ( !cgen_alloc_data( cgen, DATAREC_HEADER + (uint32_t) paylen, &off ) || off == DATAOFFS_NONE ) {
        return cgen_out_of_data_memory( comp );
    }
    cgen_note_persistent( cgen );       // the record is chained into the DATA items
    if ( !cgen_grow( (void**) &cgen->dataoffs, &cgen->numdataitems, &cgen->maxdataitems, sizeof(uint32_t) ) ) {
        comp_error( comp, "Out of memory" );
        return false;
    }
    cgen->data[ off ] = type;
    cgen_datarec_set_next( cgen->data, off, DATAOFFS_NONE );
    memcpy( &cgen->data[ off + DATAREC_HEADER ], payload, paylen );
    if ( cgen->datalast != DATAOFFS_NONE ) {
        cgen_datarec_set_next( cgen->data, cgen->datalast, off );
    } else {
        cgen->datafirst = off;
    }
    cgen->datalast = off;
    cgen->dataoffs[ cgen->numdataitems++ ] = off;
    return true;
}

bool cgen_from_datastmt( codegen_t* cgen, compiler_t* comp, node_t* node ) {
    /*
        NT_DATASTMT         DATA statement
            branches:
                - 1 or more branches of data items: NT_NUMLIT or NT_STRLIT
    */
    if ( node == NODE_NONE ) {
        return cgen_bad_node( comp );
    }
    if ( node->type != NT_DATASTMT ) {
        return cgen_unexpected_node( comp );
    }
    for ( size_t i=0; i < node->numbranches; ++i ) {
        node_t* item = node->branches[i];
        if ( item == NODE_NONE ) {
            return cgen_bad_node( comp );
        }
        if ( item->type == NT_NUMLIT && item->datalen == UINT16_C(8) ) {
            if ( !cd_add_data_item( cgen, comp, UINT8_C(0), item->data, 8U ) ) {
                return false;
            }
        } else if ( item->type == NT_STRLIT && item->datalen >= UINT16_C(1) && item->data[0] == TOK_STRLIT ) {
            // the text and its NUL terminator
            size_t len = (size_t) item->datalen - 1U;
            uint8_t* text = (uint8_t*) malloc( len + 1U );
            if ( text == 0 ) {
                comp_error( comp, "Out of memory" );
                return false;
            }
            memcpy( text, &item->data[1], len );
            text[ len ] = '\0';
            bool ok = cd_add_data_item( cgen, comp, UINT8_C(1), text, len + 1U );
            free( text );
            if ( !ok ) {
                return false;
            }
        } else {
            return cgen_unexpected_node( comp );
        }
    }
    return true;
}

bool cgen_from_readstmt( codegen_t* cgen, compiler_t* comp, node_t* node ) {
    /*
        NT_READSTMT         READ statement
            branches:
                - 1 or more branches of variable references (NT_NUMVARREF or NT_STRVARREF)
    */
    if ( node == NODE_NONE ) {
        return cgen_bad_node( comp );
    }
    if ( node->type != NT_READSTMT ) {
        return cgen_unexpected_node( comp );
    }
    for ( size_t i=0; i < node->numbranches; ++i ) {
        node_t* ref = node->branches[i];
        if ( ref == NODE_NONE ) {
            return cgen_bad_node( comp );
        }
        if ( ref->type != NT_NUMVARREF && ref->type != NT_STRVARREF ) {
            return cgen_unexpected_node( comp );
        }
        // the item is read first, then it is stored like in an assignment
        if ( !( ref->type == NT_STRVARREF ? cgen_gen_rdst( cgen ) : cgen_gen_rdnm( cgen ) ) ) {
            return cgen_out_of_code_memory( comp );
        }
        if ( !cgen_from_varref_lvalue( cgen, comp, ref ) ) {
            return false;
        }
    }
    return true;
}

void cgen_note_label_data( codegen_t* cgen, uint16_t varoffs ) {
    if ( !cgen_grow( (void**) &cgen->labeldata, &cgen->numlabeldata, &cgen->maxlabeldata, sizeof(cgen_labeldata_t) ) ) {
        return;     // RESTORE to this label will report an undefined label
    }
    cgen_labeldata_t* ld = &cgen->labeldata[ cgen->numlabeldata++ ];
    ld->varoffs   = varoffs;
    ld->dataindex = cgen->numdataitems;
}

bool cgen_from_restorestmt( codegen_t* cgen, compiler_t* comp, node_t* node ) {
    /*
        NT_RESTORESTMT      RESTORE statement
            branches:
                - none, or 1 branch of goto target (NT_GOTOTARGET: a line number or a label)
    */
    if ( node == NODE_NONE ) {
        return cgen_bad_node( comp );
    }
    if ( node->type != NT_RESTORESTMT || node->numbranches > 1U ) {
        return cgen_unexpected_node( comp );
    }
    if ( node->numbranches == 0 ) {
        if ( !cgen_gen_rstr( cgen, false, UINT16_C(0) ) ) {
            return cgen_out_of_code_memory( comp );
        }
        return true;
    }
    node_t* target = node->branches[0];
    if ( target == NODE_NONE || target->type != NT_GOTOTARGET || target->datalen != UINT16_C(4) ) {
        return cgen_bad_node( comp );
    }
    uint32_t value = ( ( (uint32_t) target->data[1] ) << UINT8_C(16) ) | ( ( (uint32_t) target->data[2] ) << UINT8_C(8) ) |
        target->data[3];
    if ( target->data[0] != VARTYPEV_INT && target->data[0] != VARTYPEV_LABEL ) {
        return cgen_unexpected_node( comp );
    }
    if ( !cgen_grow( (void**) &cgen->restorefix, &cgen->numrestorefix, &cgen->maxrestorefix, sizeof(cgen_restorefix_t) ) ) {
        comp_error( comp, "Out of memory" );
        return false;
    }
    cgen_restorefix_t* fx = &cgen->restorefix[ cgen->numrestorefix++ ];
    fx->insoffs = cgen->codesize;
    fx->islabel = target->data[0] == VARTYPEV_LABEL;
    fx->lineno  = fx->islabel ? 0 : value;
    fx->varoffs = fx->islabel ? (uint16_t) value : UINT16_C(0);
    fx->srcline = comp->iter.hdr.lineno;
    if ( !cgen_gen_rstr( cgen, true, UINT16_C(0) ) ) {      // the data offset is patched in cgen_finish_data()
        return cgen_out_of_code_memory( comp );
    }
    return true;
}

bool cgen_finish_data( codegen_t* cgen, compiler_t* comp ) {
    for ( size_t i=0; i < cgen->numrestorefix; ++i ) {
        const cgen_restorefix_t* fx = &cgen->restorefix[i];
        size_t index = cgen->numdataitems;      // default: behind the last item
        if ( fx->islabel ) {
            bool found = false;
            for ( size_t j=0; j < cgen->numlabeldata; ++j ) {
                if ( cgen->labeldata[j].varoffs == fx->varoffs ) {
                    index = cgen->labeldata[j].dataindex;
                    found = true;
                    break;
                }
            }
            if ( !found ) {
                comp->iter.hdr.lineno = fx->srcline;
                comp_error( comp, "Undefined label" );
                return false;
            }
        } else {
            // the first line at or behind the target line (the line table is in ascending order)
            for ( size_t j=0; j < cgen->numlineaddrs; ++j ) {
                if ( cgen->lineaddrs[j].lineno >= fx->lineno ) {
                    index = cgen->lineaddrs[j].dataindex;
                    break;
                }
            }
        }
        uint32_t off = index < cgen->numdataitems ? cgen->dataoffs[ index ] : DATAOFFS_NONE;
        cgen_patch_param( cgen, fx->insoffs, off );     // RSTR was generated wide
    }
    return true;
}

// -- INPUT, GET --------------------------------------------------------------------

/*
INPUT [#ch,] ["prompt" (;|,)] v1, v2, ...  becomes
    <channel> PCHN  INPL(prompt)  { INPN | INPS, store }...  INPE
The prompt text (with "? " appended unless the prompt was followed by a comma) is a constant in the data segment.
GET [#ch,] v1, v2, ...  becomes  <channel> PCHN  { GETK [ASC], store }...
Without a channel specifier the statement uses channel 0 (the channel of PCHN is otherwise sticky; PRINT does the same).
*/

// generates the channel selection: the channel expression of the optional first branch, or 0
static bool cd_gen_channel( codegen_t* cgen, compiler_t* comp, node_t* node, uint8_t flags ) {
    if ( flags & INPUTF_CHAN ) {
        if ( node->numbranches < 1U ) {
            return cgen_bad_node( comp );
        }
        return cgen_from_chanspec( cgen, comp, node->branches[0], cgen_gen_pchn );
    }
    return cgen_gen_channel0( cgen, comp );
}

// appends the text of a string literal node to a heap buffer
static bool cd_append_literal( compiler_t* comp, node_t* lit, char** ptext, size_t* plen ) {
    if ( lit == NODE_NONE || lit->type != NT_STRLIT || lit->datalen < UINT16_C(1) ) {
        return cgen_unexpected_node( comp );
    }
    if ( lit->data[0] != TOK_STRLIT ) {
        comp_error( comp, "Plain string literal expected as prompt" );
        return false;
    }
    size_t n = (size_t) lit->datalen - 1U;
    char* p = (char*) realloc( *ptext, *plen + n + 1U );
    if ( p == 0 ) {
        comp_error( comp, "Out of memory" );
        return false;
    }
    memcpy( p + *plen, &lit->data[1], n );
    *plen += n;
    p[ *plen ] = '\0';
    *ptext = p;
    return true;
}

bool cgen_from_inputstmt( codegen_t* cgen, compiler_t* comp, node_t* node ) {
    /*
        NT_INPUTSTMT        INPUT statement
            data:
                - 1 byte of flags (INPUTF_*)
            branches:
                - optional channel info (NT_CHANSPEC), optional prompt (NT_STRLITS or NT_STRLIT)
                - 1 or more branches of variable references (NT_NUMVARREF or NT_STRVARREF)
    */
    if ( node == NODE_NONE ) {
        return cgen_bad_node( comp );
    }
    if ( node->type != NT_INPUTSTMT || node->datalen != UINT16_C(1) ) {
        return cgen_unexpected_node( comp );
    }
    uint8_t flags = node->data[0];
    size_t first = ( ( flags & INPUTF_CHAN ) ? 1U : 0U ) + ( ( flags & INPUTF_PROMPT ) ? 1U : 0U );
    if ( node->numbranches <= first ) {
        return cgen_bad_node( comp );
    }
    // the prompt text
    char* text = (char*) calloc( 1U, 1U );
    size_t len = 0;
    if ( text == 0 ) {
        comp_error( comp, "Out of memory" );
        return false;
    }
    if ( flags & INPUTF_PROMPT ) {
        node_t* prompt = node->branches[ ( flags & INPUTF_CHAN ) ? 1U : 0U ];
        bool ok = true;
        if ( prompt != NODE_NONE && prompt->type == NT_STRLITS ) {
            for ( size_t i=0; ok && i < prompt->numbranches; ++i ) {
                ok = cd_append_literal( comp, prompt->branches[i], &text, &len );
            }
        } else {
            ok = cd_append_literal( comp, prompt, &text, &len );
        }
        if ( !ok ) {
            free( text );
            return false;
        }
    }
    if ( !( flags & ( INPUTF_NOQUEST | INPUTF_LINE ) ) ) {
        char* p = (char*) realloc( text, len + 3U );
        if ( p == 0 ) {
            free( text );
            comp_error( comp, "Out of memory" );
            return false;
        }
        text = p;
        memcpy( text + len, "? ", 3U );
        len += 2U;
    }
    uint32_t promptoffs = DATAOFFS_NONE;
    if ( len >= UINT16_MAX - 1U || !cgen_data_const( cgen, text, (uint16_t) len, true, &promptoffs ) || promptoffs == DATAOFFS_NONE ) {
        free( text );
        return cgen_out_of_data_memory( comp );
    }
    free( text );
    if ( !cd_gen_channel( cgen, comp, node, flags ) ) {
        return false;
    }
    if ( !cgen_gen_inpl( cgen, promptoffs ) ) {
        return cgen_out_of_code_memory( comp );
    }
    for ( size_t i=first; i < node->numbranches; ++i ) {
        node_t* ref = node->branches[i];
        if ( ref == NODE_NONE ) {
            return cgen_bad_node( comp );
        }
        if ( ref->type != NT_NUMVARREF && ref->type != NT_STRVARREF ) {
            return cgen_unexpected_node( comp );
        }
        if ( !( ( flags & INPUTF_LINE ) ? cgen_gen_inpw( cgen ) : ref->type == NT_STRVARREF ? cgen_gen_inps( cgen ) : cgen_gen_inpn( cgen ) ) ) {
            return cgen_out_of_code_memory( comp );
        }
        if ( !cgen_from_varref_lvalue( cgen, comp, ref ) ) {
            return false;
        }
    }
    if ( !cgen_gen_inpe( cgen ) ) {
        return cgen_out_of_code_memory( comp );
    }
    return true;
}

bool cgen_from_getstmt( codegen_t* cgen, compiler_t* comp, node_t* node ) {
    /*
        NT_GETSTMT          GET statement
            data:
                - 1 byte of flags (INPUTF_CHAN)
            branches:
                - optional channel info (NT_CHANSPEC)
                - 1 or more branches of variable references (NT_NUMVARREF or NT_STRVARREF)
    */
    if ( node == NODE_NONE ) {
        return cgen_bad_node( comp );
    }
    if ( node->type != NT_GETSTMT || node->datalen != UINT16_C(1) ) {
        return cgen_unexpected_node( comp );
    }
    uint8_t flags = node->data[0];
    size_t first = ( flags & INPUTF_CHAN ) ? 1U : 0U;
    if ( node->numbranches <= first ) {
        return cgen_bad_node( comp );
    }
    if ( !cd_gen_channel( cgen, comp, node, flags ) ) {
        return false;
    }
    for ( size_t i=first; i < node->numbranches; ++i ) {
        node_t* ref = node->branches[i];
        if ( ref == NODE_NONE ) {
            return cgen_bad_node( comp );
        }
        if ( ref->type != NT_NUMVARREF && ref->type != NT_STRVARREF ) {
            return cgen_unexpected_node( comp );
        }
        if ( !cgen_gen_getk( cgen ) || ( ref->type == NT_NUMVARREF && !cgen_gen_asc( cgen ) ) ) {
            return cgen_out_of_code_memory( comp );
        }
        if ( !cgen_from_varref_lvalue( cgen, comp, ref ) ) {
            return false;
        }
    }
    return true;
}
