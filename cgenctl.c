
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
Code generation for control flow statements.

Layout of the generated code:
    - Every numbered line is recorded in a table (line number -> code offset). GOTO/GOSUB/IF to a line number
      emit a BRIA instruction that is patched in cgen_finish_control() once all lines are known.
    - GOSUB pushes the return address on the return stack (PHPA.C) before it branches, RETURN is a JUMP.
    - Labels are variables holding a code offset. A LABEL statement stores its offset at compile time, GOTO/GOSUB
      label read the variable onto the return stack (RRLV) and JUMP to it.
    - IF/UNLESS:
            <condition>  PHPA.C then  JPCC  <else part>  BRIA end  then: <then part>  end:
    - FOR/NEXT are matched at compile time (NEXT closes the innermost open FOR of the same variable). The limit
      and the step live in hidden variables named "@FOR<n>L" and "@FOR<n>S"; the test is done before the first pass:
            var=from  limit=to  step=step  top: <exit if past the limit>  <body>  NEXT: var=var+step  BRIA top  exit:
*/

// -- bookkeeping -------------------------------------------------------------

void cgen_begin_control( codegen_t* cgen ) {
    cgen->numlineaddrs = 0;
    cgen->numfixups    = 0;
    cgen->numfors      = 0;
    cgen->forserial    = 0;
}

static bool cc_grow( void** pitems, size_t* pnum, size_t* pmax, size_t itemsize ) {
    if ( *pnum < *pmax ) {
        return true;
    }
    size_t newmax = *pmax ? *pmax * 2U : 64U;
    void* p = realloc( *pitems, newmax * itemsize );
    if ( p == 0 ) {
        return false;
    }
    *pitems = p;
    *pmax = newmax;
    return true;
}

static bool cc_out_of_memory( compiler_t* comp ) {
    comp_error( comp, "Out of memory" );
    return false;
}

bool cgen_note_line( codegen_t* cgen, compiler_t* comp, uint32_t lineno ) {
    if ( !cc_grow( (void**) &cgen->lineaddrs, &cgen->numlineaddrs, &cgen->maxlineaddrs, sizeof(cgen_lineaddr_t) ) ) {
        return cc_out_of_memory( comp );
    }
    cgen_lineaddr_t* la = &cgen->lineaddrs[ cgen->numlineaddrs++ ];
    la->lineno = lineno;
    la->addr   = cgen->codesize;
    return true;
}

static bool cc_find_line( const codegen_t* cgen, uint32_t lineno, uint32_t* paddr ) {
    // the table is in ascending order
    size_t lo = 0, hi = cgen->numlineaddrs;
    while ( lo < hi ) {
        size_t mid = lo + ( hi - lo ) / 2U;
        if ( cgen->lineaddrs[mid].lineno < lineno ) {
            lo = mid + 1U;
        } else {
            hi = mid;
        }
    }
    if ( lo < cgen->numlineaddrs && cgen->lineaddrs[lo].lineno == lineno ) {
        *paddr = cgen->lineaddrs[lo].addr;
        return true;
    }
    return false;
}

// -- patching ----------------------------------------------------------------

// instruction layout of BRIA and PHPA (basic opcode, with parameter): <ins> <hi> <lo>

static void cc_patch_param( codegen_t* cgen, uint32_t insoffs, uint16_t value ) {
    cgen->code[ insoffs + 1U ] = (uint8_t)( value >> UINT8_C(8) );
    cgen->code[ insoffs + 2U ] = (uint8_t)  value;
}

static void cc_patch_bria( codegen_t* cgen, uint32_t insoffs, uint32_t addr ) {
    // the C bit holds the uppermost (17th) bit of the address
    uint8_t ins = cgen->code[ insoffs ] & (uint8_t) ~INSF_C;
    if ( addr & UINT32_C(0x10000) ) {
        ins |= INSF_C;
    }
    cgen->code[ insoffs ] = ins;
    cc_patch_param( cgen, insoffs, (uint16_t) addr );
}

// emits a BRIA to be patched later; *pins receives its offset
static bool cc_emit_bria( codegen_t* cgen, compiler_t* comp, uint32_t* pins ) {
    *pins = cgen->codesize;
    return cgen_gen_bria( cgen, 0 ) || cgen_out_of_code_memory( comp );
}

// emits a PHPA.C to be patched later; *pins receives its offset
static bool cc_emit_phpa_c( codegen_t* cgen, compiler_t* comp, uint32_t* pins ) {
    *pins = cgen->codesize;
    return cgen_gen_phpa_c( cgen, 0 ) || cgen_out_of_code_memory( comp );
}

static bool cc_here16( codegen_t* cgen, compiler_t* comp, uint16_t* paddr ) {
    if ( cgen->codesize > UINT16_MAX ) {
        comp_error( comp, "Program too large" );
        return false;
    }
    *paddr = (uint16_t) cgen->codesize;
    return true;
}

bool cgen_finish_control( codegen_t* cgen, compiler_t* comp ) {
    for ( size_t i=0; i < cgen->numfixups; ++i ) {
        const cgen_fixup_t* fx = &cgen->fixups[i];
        uint32_t addr = 0;
        if ( !cc_find_line( cgen, fx->lineno, &addr ) ) {
            char msg[64];
            snprintf( msg, sizeof(msg), "Undefined line number %u", (unsigned) fx->lineno );
            comp->iter.hdr.lineno = fx->srcline;    // for the error message
            comp_error( comp, msg );
            return false;
        }
        cc_patch_bria( cgen, fx->insoffs, addr );
    }
    if ( cgen->numfors ) {
        comp->iter.hdr.lineno = cgen->fors[ cgen->numfors - 1U ].srcline;
        comp_error( comp, "FOR without NEXT" );
        return false;
    }
    return true;
}

// -- helpers -----------------------------------------------------------------

static uint32_t cc_srcline( const compiler_t* comp ) {
    return comp->iter.hdr.lineno;
}

// emits code to evaluate a branch that may be a statement, a statement list, or statement lines
static bool cc_block( codegen_t* cgen, compiler_t* comp, node_t* node ) {
    if ( node == NODE_NONE ) {
        return cgen_bad_node( comp );
    }
    switch ( node->type ) {
        case NT_STMTLINES:  return cgen_from_stmtlines( cgen, comp, node );
        case NT_STMTLINE:   return cgen_from_stmtline( cgen, comp, node );
        default:
            break;
    }
    return cgen_from_stmt( cgen, comp, node );
}

// -- GOTO, GOSUB, RETURN, LABEL ----------------------------------------------

// a goto target node: <type.8> <value.24>
static bool cc_jump_to_target( codegen_t* cgen, compiler_t* comp, node_t* target, bool gosub ) {
    if ( target == NODE_NONE || target->type != NT_GOTOTARGET || target->datalen != UINT16_C(4) ) {
        return cgen_bad_node( comp );
    }
    uint8_t  type  = target->data[0];
    uint32_t value = ( ( (uint32_t) target->data[1] ) << UINT8_C(16) ) | ( ( (uint32_t) target->data[2] ) << UINT8_C(8) ) |
        target->data[3];
    uint32_t retins = 0;
    if ( gosub && !cc_emit_phpa_c( cgen, comp, &retins ) ) {    // return address, patched below
        return false;
    }
    if ( type == VARTYPEV_INT ) {
        // line number: patched when all lines are known
        uint32_t ins = 0;
        if ( !cc_emit_bria( cgen, comp, &ins ) ) {
            return false;
        }
        if ( !cc_grow( (void**) &cgen->fixups, &cgen->numfixups, &cgen->maxfixups, sizeof(cgen_fixup_t) ) ) {
            return cc_out_of_memory( comp );
        }
        cgen_fixup_t* fx = &cgen->fixups[ cgen->numfixups++ ];
        fx->insoffs = ins;
        fx->lineno  = value;
        fx->srcline = cc_srcline( comp );
    } else if ( type == VARTYPEV_LABEL ) {
        // label variable: the code offset is read onto the return stack, then we jump there
        if ( !cgen_gen_rrlv( cgen, (uint16_t) value ) || !cgen_gen_jump( cgen ) ) {
            return cgen_out_of_code_memory( comp );
        }
    } else {
        return cgen_unexpected_node( comp );
    }
    if ( gosub ) {
        uint16_t here = 0;
        if ( !cc_here16( cgen, comp, &here ) ) {
            return false;
        }
        cc_patch_param( cgen, retins, here );
    }
    return true;
}

bool cgen_from_gotostmt( codegen_t* cgen, compiler_t* comp, node_t* node ) {
    /*
        NT_GOTOSTMT     GOTO/GOSUB statement
            data:
                - 1 byte of GOTO/GOSUB token
            branches:
                - 1 branch of goto/gosub target
    */
    if ( node == NODE_NONE ) {
        return cgen_bad_node( comp );
    }
    if ( node->type != NT_GOTOSTMT || node->datalen != UINT16_C(1) || node->numbranches != 1U ) {
        return cgen_unexpected_node( comp );
    }
    uint8_t tok = node->data[0];
    if ( tok != TOK_GOTO && tok != TOK_GOSUB ) {
        return cgen_unexpected_node( comp );
    }
    return cc_jump_to_target( cgen, comp, node->branches[0], tok == TOK_GOSUB );
}

bool cgen_from_returnstmt( codegen_t* cgen, compiler_t* comp, node_t* node ) {
    if ( node == NODE_NONE ) {
        return cgen_bad_node( comp );
    }
    if ( node->type != NT_RETURNSTMT ) {
        return cgen_unexpected_node( comp );
    }
    if ( !cgen_gen_jump( cgen ) ) {     // the return address is on the return stack
        return cgen_out_of_code_memory( comp );
    }
    return true;
}

bool cgen_from_labelstmt( codegen_t* cgen, compiler_t* comp, node_t* node ) {
    /*
        NT_LABELSTMT    LABEL statement
            data:
                - 2 bytes of label variable offset
    */
    if ( node == NODE_NONE ) {
        return cgen_bad_node( comp );
    }
    if ( node->type != NT_LABELSTMT || node->datalen != UINT16_C(2) ) {
        return cgen_unexpected_node( comp );
    }
    uint16_t varoffs = (uint16_t)( ( ( (uint16_t) node->data[0] ) << UINT8_C(8) ) | node->data[1] );
    uint16_t here = 0;
    if ( !cc_here16( cgen, comp, &here ) ) {
        return false;
    }
    varvalue_t value;
    memset( &value, 0, sizeof(value) );
    value.lbloffs = here;
    if ( !vmem_set_var( &comp->rt->varmem, varoffs, VARTYPEV_LABEL, 0, 0, &value ) ) {
        comp_error( comp, "Internal error (label variable not found)" );
        return false;
    }
    return true;
}

// -- IF / UNLESS ---------------------------------------------------------------

// emits the code common to both IF forms; thenpart/elsepart are branches (elsepart may be NODE_NONE)
static bool cc_if( codegen_t* cgen, compiler_t* comp, bool unless, node_t* cond, node_t* thenpart, node_t* elsepart,
    uint8_t gotok, bool block ) {
    if ( cond == NODE_NONE || thenpart == NODE_NONE ) {
        return cgen_bad_node( comp );
    }
    if ( !cgen_from_numexpr( cgen, comp, cond ) ) {
        return false;
    }
    if ( unless ) {
        // jump to the THEN part if the condition is zero
        if ( !cgen_gen_phim( cgen, 0 ) || !cgen_gen_cneq( cgen ) ) {
            return cgen_out_of_code_memory( comp );
        }
    }
    uint32_t thenins = 0;
    if ( !cc_emit_phpa_c( cgen, comp, &thenins ) || !cgen_gen_jpcc( cgen ) ) {
        return cgen_out_of_code_memory( comp );
    }
    // not taken: ELSE part, then skip the THEN part
    bool isgoto = thenpart->type == NT_GOTOTARGET;
    if ( elsepart != NODE_NONE ) {
        if ( isgoto ) {
            if ( !cc_jump_to_target( cgen, comp, elsepart, gotok == TOK_GOSUB ) ) {
                return false;
            }
        } else if ( !( block ? cc_block( cgen, comp, elsepart ) : cgen_from_stmt( cgen, comp, elsepart ) ) ) {
            return false;
        }
    }
    uint32_t endins = 0;
    if ( !cc_emit_bria( cgen, comp, &endins ) ) {
        return false;
    }
    uint16_t here = 0;
    if ( !cc_here16( cgen, comp, &here ) ) {
        return false;
    }
    cc_patch_param( cgen, thenins, here );
    if ( isgoto ) {
        if ( !cc_jump_to_target( cgen, comp, thenpart, gotok == TOK_GOSUB ) ) {
            return false;
        }
    } else if ( !( block ? cc_block( cgen, comp, thenpart ) : cgen_from_stmt( cgen, comp, thenpart ) ) ) {
        return false;
    }
    cc_patch_bria( cgen, endins, cgen->codesize );
    return true;
}

bool cgen_from_singlelineifstmt( codegen_t* cgen, compiler_t* comp, node_t* node ) {
    /*
        NT_SINGLELINEIFSTMT     Single-line IF statement
            data:
                - 1 token byte: IF/UNLESS
                - 1 token byte: THEN/GOTO/GOSUB
            branches:
                - 1 branch of IF numeric expression
                - 1 branch of THEN gosub/goto target or statement list
                - 1 optional branch of ELSE gosub/goto target or statement list
    */
    if ( node == NODE_NONE ) {
        return cgen_bad_node( comp );
    }
    if ( node->type != NT_SINGLELINEIFSTMT || node->datalen != UINT16_C(2) || node->numbranches != 3U ) {
        return cgen_unexpected_node( comp );
    }
    bool unless = node->data[0] == TOK_UNLESS;
    uint8_t gotok = node->data[1];
    // both branches are either jump targets or statements; THEN <line> is a GOTO
    if ( node->branches[2] != NODE_NONE && ( node->branches[1]->type == NT_GOTOTARGET ) !=
         ( node->branches[2]->type == NT_GOTOTARGET ) ) {
        return cgen_unexpected_node( comp );
    }
    return cc_if( cgen, comp, unless, node->branches[0], node->branches[1], node->branches[2], gotok, false );
}

bool cgen_from_multilineifstmt( codegen_t* cgen, compiler_t* comp, node_t* node ) {
    /*
        NT_MULTILINEIFSTMT      Multi-line IF statement
            data:
                - 1 byte of IF/UNLESS token
            branches:
                - 1 branch of IF numeric expression
                - 1 branch of statement lines
                - 1 optional branch of ELSE statement lines
    */
    if ( node == NODE_NONE ) {
        return cgen_bad_node( comp );
    }
    if ( node->type != NT_MULTILINEIFSTMT || node->datalen != UINT16_C(1) || node->numbranches != 3U ) {
        return cgen_unexpected_node( comp );
    }
    bool unless = node->data[0] == TOK_UNLESS;
    return cc_if( cgen, comp, unless, node->branches[0], node->branches[1], node->branches[2], TOK_THEN, true );
}

// -- FOR / NEXT ----------------------------------------------------------------

// finds or creates the numeric variable named by a base variable reference node
static bool cc_base_var( compiler_t* comp, node_t* node, uint8_t* pvartype, uint16_t* pvaroffs ) {
    if ( node == NODE_NONE ) {
        return cgen_bad_node( comp );
    }
    if ( node->type != NT_NUMBASEVARREF || node->datalen < UINT16_C(2) ) {
        return cgen_unexpected_node( comp );
    }
    uint8_t vartype = node->data[0] & VARTYPEM_BASE;
    const char* name = comp_node_name( comp, node, 1U );
    uint16_t varoffs = VAROFFS_NONE;
    if ( !vmem_lookup_var( &comp->rt->varmem, vartype, name, &varoffs ) || varoffs == VAROFFS_NONE ) {
        varoffs = VAROFFS_NONE;
        if ( !vmem_create_var( &comp->rt->varmem, vartype, name, UINT8_C(0), 0, UINT8_C(0), 0, &varoffs ) ||
             varoffs == VAROFFS_NONE ) {
            return cc_out_of_memory( comp );
        }
    }
    *pvartype = vartype;
    *pvaroffs = varoffs;
    return true;
}

static bool cc_hidden_var( compiler_t* comp, uint32_t serial, char which, uint16_t* pvaroffs ) {
    char name[32];
    snprintf( name, sizeof(name), "@FOR%u%c", (unsigned) serial, which );
    uint16_t varoffs = VAROFFS_NONE;
    if ( !vmem_lookup_var( &comp->rt->varmem, VARTYPEV_FLOAT, name, &varoffs ) || varoffs == VAROFFS_NONE ) {
        varoffs = VAROFFS_NONE;
        if ( !vmem_create_var( &comp->rt->varmem, VARTYPEV_FLOAT, name, UINT8_C(0), 0, UINT8_C(0), 0, &varoffs ) ||
             varoffs == VAROFFS_NONE ) {
            return cc_out_of_memory( comp );
        }
    }
    *pvaroffs = varoffs;
    return true;
}

static bool cc_read_var( codegen_t* cgen, uint8_t vartype, uint16_t varoffs ) {
    return vartype == VARTYPEV_INT ? cgen_gen_rriv( cgen, varoffs ) : cgen_gen_rrnv( cgen, varoffs );
}

static bool cc_write_var( codegen_t* cgen, uint8_t vartype, uint16_t varoffs ) {
    return vartype == VARTYPEV_INT ? cgen_gen_wriv( cgen, varoffs ) : cgen_gen_wrnv( cgen, varoffs );
}

bool cgen_from_forstmt( codegen_t* cgen, compiler_t* comp, node_t* node ) {
    /*
        NT_FORSTMT      FOR statement
            branches:
                - 1 branch of variable reference
                - 1 branch of FROM expression
                - 1 branch of TO expression
                - 1 branch of STEP expression (may be empty)
    */
    if ( node == NODE_NONE ) {
        return cgen_bad_node( comp );
    }
    if ( node->type != NT_FORSTMT || node->numbranches != 4U ) {
        return cgen_unexpected_node( comp );
    }
    uint8_t vartype = 0; uint16_t varoffs = VAROFFS_NONE;
    if ( !cc_base_var( comp, node->branches[0], &vartype, &varoffs ) ) {
        return false;
    }
    uint32_t serial = cgen->forserial++;
    uint16_t limvar = VAROFFS_NONE, stepvar = VAROFFS_NONE;
    if ( !cc_hidden_var( comp, serial, 'L', &limvar ) || !cc_hidden_var( comp, serial, 'S', &stepvar ) ) {
        return false;
    }
    // var = from, limit = to, step = step
    if ( !cgen_from_numexpr( cgen, comp, node->branches[1] ) ) {
        return false;
    }
    if ( !cc_write_var( cgen, vartype, varoffs ) ) {
        return cgen_out_of_code_memory( comp );
    }
    if ( !cgen_from_numexpr( cgen, comp, node->branches[2] ) ) {
        return false;
    }
    if ( !cgen_gen_wrnv( cgen, limvar ) ) {
        return cgen_out_of_code_memory( comp );
    }
    if ( node->branches[3] != NODE_NONE ) {
        if ( !cgen_from_numexpr( cgen, comp, node->branches[3] ) ) {
            return false;
        }
    } else if ( !cgen_gen_phim( cgen, 1 ) ) {
        return cgen_out_of_code_memory( comp );
    }
    if ( !cgen_gen_wrnv( cgen, stepvar ) ) {
        return cgen_out_of_code_memory( comp );
    }
    // top: exit if ( step >= 0 AND var > limit ) OR ( step < 0 AND var < limit )
    uint16_t top = 0;
    if ( !cc_here16( cgen, comp, &top ) ) {
        return false;
    }
    bool ok =
        cgen_gen_rrnv( cgen, stepvar ) && cgen_gen_phim( cgen, 0 ) && cgen_gen_cnge( cgen ) &&
        cc_read_var( cgen, vartype, varoffs ) && cgen_gen_rrnv( cgen, limvar ) && cgen_gen_cngt( cgen ) &&
        cgen_gen_and( cgen ) &&
        cgen_gen_rrnv( cgen, stepvar ) && cgen_gen_phim( cgen, 0 ) && cgen_gen_cnlt( cgen ) &&
        cc_read_var( cgen, vartype, varoffs ) && cgen_gen_rrnv( cgen, limvar ) && cgen_gen_cnlt( cgen ) &&
        cgen_gen_and( cgen ) && cgen_gen_or( cgen );
    if ( !ok ) {
        return cgen_out_of_code_memory( comp );
    }
    uint32_t exitins = 0;
    if ( !cc_emit_phpa_c( cgen, comp, &exitins ) || !cgen_gen_jpcc( cgen ) ) {
        return cgen_out_of_code_memory( comp );
    }
    // remember the loop for NEXT
    if ( !cc_grow( (void**) &cgen->fors, &cgen->numfors, &cgen->maxfors, sizeof(cgen_forctx_t) ) ) {
        return cc_out_of_memory( comp );
    }
    cgen_forctx_t* fc = &cgen->fors[ cgen->numfors++ ];
    fc->vartype = vartype;
    fc->varoffs = varoffs;
    fc->stepvar = stepvar;
    fc->topaddr = top;
    fc->exitins = exitins;
    fc->srcline = cc_srcline( comp );
    return true;
}

// closes the innermost FOR loop, which must use the given variable
static bool cc_next( codegen_t* cgen, compiler_t* comp, bool anyvar, uint16_t varoffs ) {
    if ( cgen->numfors == 0 || ( !anyvar && cgen->fors[ cgen->numfors - 1U ].varoffs != varoffs ) ) {
        comp_error( comp, "NEXT without FOR" );
        return false;
    }
    cgen_forctx_t fc = cgen->fors[ --cgen->numfors ];
    if ( !cc_read_var( cgen, fc.vartype, fc.varoffs ) || !cgen_gen_rrnv( cgen, fc.stepvar ) || !cgen_gen_add( cgen ) ||
         !cc_write_var( cgen, fc.vartype, fc.varoffs ) || !cgen_gen_bria( cgen, (int32_t) fc.topaddr ) ) {
        return cgen_out_of_code_memory( comp );
    }
    uint16_t here = 0;
    if ( !cc_here16( cgen, comp, &here ) ) {
        return false;
    }
    cc_patch_param( cgen, fc.exitins, here );
    return true;
}

bool cgen_from_nextstmt( codegen_t* cgen, compiler_t* comp, node_t* node ) {
    /*
        NT_NEXTSTMT     NEXT statement
            branches:
                - 1 branch of variable reference list (may be empty, a single reference or a list)
    */
    if ( node == NODE_NONE ) {
        return cgen_bad_node( comp );
    }
    if ( node->type != NT_NEXTSTMT || node->numbranches != 1U ) {
        return cgen_unexpected_node( comp );
    }
    node_t* refs = node->branches[0];
    if ( refs == NODE_NONE ) {
        return cc_next( cgen, comp, true, VAROFFS_NONE );
    }
    uint8_t vartype = 0; uint16_t varoffs = VAROFFS_NONE;
    if ( refs->type == NT_NUMBASEVARREFLIST ) {
        for ( size_t i=0; i < refs->numbranches; ++i ) {
            if ( !cc_base_var( comp, refs->branches[i], &vartype, &varoffs ) || !cc_next( cgen, comp, false, varoffs ) ) {
                return false;
            }
        }
        return true;
    }
    return cc_base_var( comp, refs, &vartype, &varoffs ) && cc_next( cgen, comp, false, varoffs );
}
