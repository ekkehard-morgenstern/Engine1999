
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
    uint8_t ftype = 0, numparams = 0; uint16_t oldcode = CODEOFFS_NONE;
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
    if ( cgen->codesize > UINT16_MAX ) {
        comp_error( comp, "Program too large" );
        return false;
    }
    uint16_t start = (uint16_t) cgen->codesize;
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
            uint16_t emptyoffs = DATAOFFS_NONE;
            if ( !cgen_alloc_data( cgen, UINT16_C(1), &emptyoffs ) || emptyoffs == DATAOFFS_NONE ) {
                return cgen_out_of_data_memory( comp );
            }
            cgen->data[ emptyoffs ] = '\0';
            if ( !cgen_gen_phpa_d( cgen, emptyoffs ) || !cgen_gen_las_d( cgen ) ) {
                return cgen_out_of_code_memory( comp );
            }
        } else if ( !cgen_gen_phim( cgen, 0 ) ) {
            return cgen_out_of_code_memory( comp );
        }
        if ( !cd_write_var( cgen, basetype, resoffs ) ) {
            return cgen_out_of_code_memory( comp );
        }
        if ( !cgen_from_block( cgen, comp, body->branches[0] ) ) {
            return false;
        }
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
    // patch the jump (BRIA <ins> <hi> <lo>, the C bit is bit 16 of the address)
    uint32_t end = cgen->codesize;
    cgen->code[ skipins ] = (uint8_t)( ( cgen->code[ skipins ] & (uint8_t) ~INSF_C ) | ( ( end & UINT32_C(0x10000) ) ? INSF_C : 0 ) );
    cgen->code[ skipins + 1U ] = (uint8_t)( end >> UINT8_C(8) );
    cgen->code[ skipins + 2U ] = (uint8_t)  end;
    // store the code offset in the function variable
    varvalue_t value;
    memset( &value, 0, sizeof(value) );
    value.codeoffs = start;
    if ( !vmem_set_var( &comp->rt->varmem, funcoffs, functype, 0, 0, &value ) ) {
        comp_error( comp, "Internal error (failed to store the function address)" );
        return false;
    }
    return true;
}
