
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
    - Loops are matched at compile time, with a stack of the open loops (innermost last): FOR/NEXT, REPEAT/UNTIL,
      WHILE/WEND and UNTIL/UEND. NEXT closes the innermost FOR of the same variable. An UNTIL closes the innermost
      loop if that is a REPEAT, otherwise it opens an UNTIL loop. AGAIN [n] branches to the start of the body of the
      n-th innermost loop. EXIT [n] / LEAVE [n] branch to the end of the n-th innermost loop, ITERATE [n] to its next pass
      (FOR: the NEXT step, WHILE/UNTIL: the test, REPEAT: the UNTIL test); both are BRIAs that wait in cgen->loopjumps until
      the loop is closed. FOR EACH [key,] v IN a() is a FOR over a hidden counter 0 .. ACNT(a)-1 with v = AFLT(a, counter)
      at the top of the body. Loops inside a function body are separate from those around the function.
      The limit and the step of a FOR live in hidden variables named "@FOR<n>L" and "@FOR<n>S"; the test is done
      before the first pass:
            var=from  limit=to  step=step  top: <exit if past the limit>  body: <body>  NEXT: var=var+step  BRIA top  exit:
            WHILE:   top: <cond>  exit if zero   body: <body>  WEND: BRIA top  exit:
            UNTIL:   top: <cond>  exit if not zero   body: <body>  UEND: BRIA top  exit:
            REPEAT:  body: <body>  UNTIL: <cond>  branch to body if zero
    - END branches to the end of the program (a fixup), POP drops the latest return address, and
      ON n GOTO/GOSUB t1,t2,..: <n -> hidden variable "@ON<n>O">  tests  stubs  that jump (GOSUB: return address = end)
*/

// -- bookkeeping -------------------------------------------------------------

void cgen_begin_control( codegen_t* cgen ) {
    cgen->numlineaddrs = 0;
    cgen->numfixups    = 0;
    cgen->numloops     = 0;
    cgen->numloopjumps = 0;
    cgen->numstmts     = 0;
    cgen->loopbase     = 0;
    cgen->numscratch   = 0;
    cgen->numrestorefix = 0;
    cgen->numlabeldata = 0;
    cgen->forserial    = 0;
}

bool cgen_grow( void** pitems, size_t* pnum, size_t* pmax, size_t itemsize ) {
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
    if ( !cgen_grow( (void**) &cgen->lineaddrs, &cgen->numlineaddrs, &cgen->maxlineaddrs, sizeof(cgen_lineaddr_t) ) ) {
        return cc_out_of_memory( comp );
    }
    cgen_lineaddr_t* la = &cgen->lineaddrs[ cgen->numlineaddrs++ ];
    la->lineno = lineno;
    la->addr   = cgen->codesize;
    la->dataindex = cgen->numdataitems;
    return true;
}

bool cgen_find_line( const codegen_t* cgen, uint32_t lineno, uint32_t* paddr ) {
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

// BRIA and PHPA.C are generated wide (<ins> <b3> <b2> <b1> <b0>), so any code address can be patched in

static void cc_patch_param( codegen_t* cgen, uint32_t insoffs, uint32_t value ) {
    cgen_patch_param( cgen, insoffs, value );
}

static void cc_patch_bria( codegen_t* cgen, uint32_t insoffs, uint32_t addr ) {
    cgen_patch_param( cgen, insoffs, addr );
}

// emits a BRIA to be patched later; *pins receives its offset
static bool cc_emit_bria( codegen_t* cgen, compiler_t* comp, uint32_t* pins ) {
    *pins = cgen->codesize;
    return cgen_gen_bria( cgen, 0 ) || cgen_out_of_code_memory( comp );
}

static bool cc_write_var( codegen_t* cgen, uint8_t vartype, uint16_t varoffs );
static bool cc_add_procexit( codegen_t* cgen, compiler_t* comp, uint32_t* pins );
static bool cc_base_var( compiler_t* comp, node_t* node, uint8_t* pvartype, uint16_t* pvaroffs );

// emits a PHPA.C to be patched later; *pins receives its offset
static bool cc_emit_phpa_c( codegen_t* cgen, compiler_t* comp, uint32_t* pins ) {
    *pins = cgen->codesize;
    return cgen_gen_phpa_c( cgen, 0 ) || cgen_out_of_code_memory( comp );
}

static bool cc_here( codegen_t* cgen, compiler_t* comp, uint32_t* paddr ) {
    *paddr = cgen->codesize;
    return true;
}

bool cgen_finish_control( codegen_t* cgen, compiler_t* comp ) {
    for ( size_t i=0; i < cgen->numfixups; ++i ) {
        const cgen_fixup_t* fx = &cgen->fixups[i];
        uint32_t addr = 0;
        if ( fx->lineno == CGEN_FIXUP_END ) {
            addr = cgen->endaddr != CODEOFFS_NONE ? cgen->endaddr : cgen->codesize;
        } else if ( !cgen_find_line( cgen, fx->lineno, &addr ) ) {
            char msg[64];
            snprintf( msg, sizeof(msg), "Undefined line number %u", (unsigned) fx->lineno );
            comp->iter.hdr.lineno = fx->srcline;    // for the error message
            comp_error( comp, msg );
            return false;
        }
        cc_patch_bria( cgen, fx->insoffs, addr );
    }
    return cgen_loops_closed( cgen, comp, 0 ) && cgen_finish_data( cgen, comp );
}

bool cgen_loops_closed( codegen_t* cgen, compiler_t* comp, size_t base ) {
    if ( cgen->numloops > base ) {
        const cgen_loopctx_t* lc = &cgen->loops[ cgen->numloops - 1U ];
        const char* msg = lc->kind == CGEN_LOOP_FOR ? "FOR without NEXT" : lc->kind == CGEN_LOOP_REPEAT ? "REPEAT without UNTIL" :
            lc->kind == CGEN_LOOP_WHILE ? "WHILE without WEND" : "UNTIL without UEND";
        comp->iter.hdr.lineno = lc->srcline;
        comp_error( comp, msg );
        return false;
    }
    return true;
}

// -- helpers -----------------------------------------------------------------

static uint32_t cc_srcline( const compiler_t* comp ) {
    return comp->iter.hdr.lineno;
}

// emits code to evaluate a branch that may be a statement, a statement list, or statement lines
bool cgen_from_block( codegen_t* cgen, compiler_t* comp, node_t* node ) {
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
// For a GOSUB the return address is the instruction after the jump, unless <pretins> is given: then the caller gets
// the offset of the PHPA instruction and patches the return address itself.
static bool cc_jump_to_target( codegen_t* cgen, compiler_t* comp, node_t* target, bool gosub, uint32_t* pretins ) {
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
        if ( !cgen_grow( (void**) &cgen->fixups, &cgen->numfixups, &cgen->maxfixups, sizeof(cgen_fixup_t) ) ) {
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
    if ( gosub && pretins ) {
        *pretins = retins;
    } else if ( gosub ) {
        uint32_t here = 0;
        if ( !cc_here( cgen, comp, &here ) ) {
            return false;
        }
        cc_patch_param( cgen, retins, here );
    }
    return true;
}

// pushes the code address of a goto target on the return stack (a line: PHPA.C patched when the lines are known; a label: the
// label variable is read onto the return stack)
static bool cc_push_target_address( codegen_t* cgen, compiler_t* comp, node_t* target ) {
    if ( target == NODE_NONE || target->type != NT_GOTOTARGET || target->datalen != UINT16_C(4) ) {
        return cgen_bad_node( comp );
    }
    uint8_t  type  = target->data[0];
    uint32_t value = ( ( (uint32_t) target->data[1] ) << UINT8_C(16) ) | ( ( (uint32_t) target->data[2] ) << UINT8_C(8) ) |
        target->data[3];
    if ( type == VARTYPEV_INT ) {
        uint32_t ins = 0;
        if ( !cc_emit_phpa_c( cgen, comp, &ins ) ) {
            return false;
        }
        if ( !cgen_grow( (void**) &cgen->fixups, &cgen->numfixups, &cgen->maxfixups, sizeof(cgen_fixup_t) ) ) {
            return cc_out_of_memory( comp );
        }
        cgen_fixup_t* fx = &cgen->fixups[ cgen->numfixups++ ];
        fx->insoffs = ins;
        fx->lineno  = value;
        fx->srcline = cc_srcline( comp );
        return true;
    }
    if ( type == VARTYPEV_LABEL ) {
        return cgen_gen_rrlv( cgen, (uint16_t) value ) || cgen_out_of_code_memory( comp );
    }
    return cgen_unexpected_node( comp );
}

bool cgen_from_timerstmt( codegen_t* cgen, compiler_t* comp, node_t* node ) {
    /*
        NT_TIMERSTMT    AFTER / EVERY statement
            data: 1 byte of TMRF_* flags
            branches: delay (not with TMRF_OFF), timer number (with TMRF_TIMER), goto target (not with TMRF_OFF)
        code: <delay> <timer> <target address on the return stack> TMR     (parameter: bit 0 EVERY, bit 1 GOTO)
              <timer or -1> TMRO                                           (parameter: bit 0 EVERY)
    */
    if ( node == NODE_NONE ) {
        return cgen_bad_node( comp );
    }
    if ( node->type != NT_TIMERSTMT || node->datalen != UINT16_C(1) ) {
        return cgen_unexpected_node( comp );
    }
    uint8_t flags = node->data[0];
    bool every = ( flags & TMRF_EVERY ) != 0;
    bool off = ( flags & TMRF_OFF ) != 0;
    bool hastimer = ( flags & TMRF_TIMER ) != 0;
    size_t expected = off ? ( hastimer ? 1U : 0U ) : ( hastimer ? 3U : 2U );
    if ( node->numbranches != expected ) {
        return cgen_unexpected_node( comp );
    }
    size_t k = 0;
    if ( off ) {
        if ( hastimer ? !cgen_from_numexpr( cgen, comp, node->branches[0] ) : !cgen_push_number( cgen, comp, -1.0 ) ) {
            return false;
        }
        return cgen_gen_tmro( cgen, every ? UINT16_C(1) : UINT16_C(0) ) || cgen_out_of_code_memory( comp );
    }
    if ( !cgen_from_numexpr( cgen, comp, node->branches[k++] ) ) {
        return false;
    }
    if ( hastimer ? !cgen_from_numexpr( cgen, comp, node->branches[k++] ) : !cgen_push_number( cgen, comp, 0.0 ) ) {
        return false;
    }
    if ( !cc_push_target_address( cgen, comp, node->branches[k] ) ) {
        return false;
    }
    uint16_t param = ( every ? UINT16_C(1) : UINT16_C(0) ) | ( ( flags & TMRF_GOTO ) ? UINT16_C(2) : UINT16_C(0) );
    return cgen_gen_tmr( cgen, param ) || cgen_out_of_code_memory( comp );
}

bool cgen_from_pokestmt( codegen_t* cgen, compiler_t* comp, node_t* node ) {
    // NT_POKESTMT: branches address, value (number or string), size (optional, 1): <address> <value> <size> POKE
    if ( node == NODE_NONE ) {
        return cgen_bad_node( comp );
    }
    if ( node->type != NT_POKESTMT || node->numbranches < 2U || node->numbranches > 3U ) {
        return cgen_unexpected_node( comp );
    }
    if ( !cgen_from_expr( cgen, comp, node->branches[0] ) || !cgen_from_expr( cgen, comp, node->branches[1] ) ) {
        return false;
    }
    if ( node->numbranches == 3U ? !cgen_from_expr( cgen, comp, node->branches[2] ) : !cgen_gen_phim( cgen, 1 ) ) {
        return node->numbranches == 3U ? false : cgen_out_of_code_memory( comp );
    }
    return cgen_gen_poke( cgen ) || cgen_out_of_code_memory( comp );
}

bool cgen_from_debugstmt( codegen_t* cgen, compiler_t* comp, node_t* node ) {
    /*
        NT_STOPSTMT / NT_CONTSTMT      STOP, CONT:  one instruction each
        NT_TRACESTMT                   data: 1 = ON, 0 = OFF:  TRON(on)
        NT_AUTOSTMT                    0..2 numeric branches (start, step):  <start or -1> <step or -1> AUTO
    */
    if ( node == NODE_NONE ) {
        return cgen_bad_node( comp );
    }
    switch ( node->type ) {
        case NT_STOPSTMT: return cgen_gen_stop( cgen ) || cgen_out_of_code_memory( comp );
        case NT_CONTSTMT: return cgen_gen_cont( cgen ) || cgen_out_of_code_memory( comp );
        case NT_TRACESTMT:
            if ( node->datalen != UINT16_C(1) ) {
                return cgen_unexpected_node( comp );
            }
            return cgen_gen_tron( cgen, node->data[0] ? UINT16_C(1) : UINT16_C(0) ) || cgen_out_of_code_memory( comp );
        case NT_AUTOSTMT:
            if ( node->numbranches > 2U ) {
                return cgen_unexpected_node( comp );
            }
            for ( size_t i=0; i < 2U; ++i ) {
                if ( i < node->numbranches ) {
                    if ( !cgen_from_numexpr( cgen, comp, node->branches[i] ) ) {
                        return false;
                    }
                } else if ( !cgen_gen_phim( cgen, -1 ) ) {
                    return cgen_out_of_code_memory( comp );
                }
            }
            return cgen_gen_auto( cgen ) || cgen_out_of_code_memory( comp );
        default:
            break;
    }
    return cgen_unexpected_node( comp );
}

bool cgen_from_resumestmt( codegen_t* cgen, compiler_t* comp, node_t* node ) {
    /*
        NT_RESUMESTMT   RESUME statement
            data: 1 byte, 0 = RESUME, 1 = RESUME NEXT, 2 = RESUME target     branches: the goto target (kind 2)
        code: [ <target address on the return stack> ] RSM(kind)
    */
    if ( node == NODE_NONE ) {
        return cgen_bad_node( comp );
    }
    if ( node->type != NT_RESUMESTMT || node->datalen != UINT16_C(1) || node->data[0] > 2 ||
         node->numbranches != ( node->data[0] == 2 ? 1U : 0U ) ) {
        return cgen_unexpected_node( comp );
    }
    if ( node->data[0] == 2 && !cc_push_target_address( cgen, comp, node->branches[0] ) ) {
        return false;
    }
    return cgen_gen_rsm( cgen, node->data[0] ) || cgen_out_of_code_memory( comp );
}

bool cgen_from_errtrapstmt( codegen_t* cgen, compiler_t* comp, node_t* node ) {
    /*
        NT_ERRTRAPSTMT  ON ERROR statement
            data: 1 byte, ERRT_* kind     branches: the goto target (not with ERRT_OFF)
        code: <target address on the return stack> ERRT(1 GOSUB / 2 GOTO)      ERRT(0)
    */
    if ( node == NODE_NONE ) {
        return cgen_bad_node( comp );
    }
    if ( node->type != NT_ERRTRAPSTMT || node->datalen != UINT16_C(1) ) {
        return cgen_unexpected_node( comp );
    }
    uint8_t kind = node->data[0];
    if ( kind == ERRT_OFF ) {
        return node->numbranches == 0U ? ( cgen_gen_errt( cgen, UINT16_C(0) ) || cgen_out_of_code_memory( comp ) ) : cgen_unexpected_node( comp );
    }
    if ( ( kind != ERRT_GOSUB && kind != ERRT_GOTO ) || node->numbranches != 1U ) {
        return cgen_unexpected_node( comp );
    }
    return cc_push_target_address( cgen, comp, node->branches[0] ) &&
        ( cgen_gen_errt( cgen, kind == ERRT_GOTO ? UINT16_C(2) : UINT16_C(1) ) || cgen_out_of_code_memory( comp ) );
}

bool cgen_from_oncollstmt( codegen_t* cgen, compiler_t* comp, node_t* node ) {
    // NT_ONCOLLSTMT: like ON ERROR; code: <target address on the return stack> COLT(1 GOSUB / 2 GOTO)      COLT(0)
    if ( node == NODE_NONE ) {
        return cgen_bad_node( comp );
    }
    if ( node->type != NT_ONCOLLSTMT || node->datalen != UINT16_C(1) ) {
        return cgen_unexpected_node( comp );
    }
    uint8_t kind = node->data[0];
    if ( kind == ERRT_OFF ) {
        return node->numbranches == 0U ? ( cgen_gen_colt( cgen, UINT16_C(0) ) || cgen_out_of_code_memory( comp ) ) : cgen_unexpected_node( comp );
    }
    if ( ( kind != ERRT_GOSUB && kind != ERRT_GOTO ) || node->numbranches != 1U ) {
        return cgen_unexpected_node( comp );
    }
    return cc_push_target_address( cgen, comp, node->branches[0] ) &&
        ( cgen_gen_colt( cgen, kind == ERRT_GOTO ? UINT16_C(2) : UINT16_C(1) ) || cgen_out_of_code_memory( comp ) );
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
    return cc_jump_to_target( cgen, comp, node->branches[0], tok == TOK_GOSUB, 0 );
}

bool cgen_from_returnstmt( codegen_t* cgen, compiler_t* comp, node_t* node ) {
    if ( node == NODE_NONE ) {
        return cgen_bad_node( comp );
    }
    if ( node->type != NT_RETURNSTMT || node->numbranches > 1U ) {
        return cgen_unexpected_node( comp );
    }
    if ( node->numbranches == 1U ) {
        // RETURN value: set the result of the FUNC and leave it
        if ( !cgen->inproc || !cgen->procresult ) {
            comp_error( comp, "RETURN with a value outside a FUNC" );
            return false;
        }
        uint32_t ins = 0;
        if ( !cgen_from_expr( cgen, comp, node->branches[0] ) ) {
            return false;
        }
        if ( !cc_write_var( cgen, cgen->procbase, cgen->procresvar ) ) {
            return cgen_out_of_code_memory( comp );
        }
        return cc_add_procexit( cgen, comp, &ins );
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
    uint32_t here = 0;
    if ( !cc_here( cgen, comp, &here ) ) {
        return false;
    }
    cgen_note_label_data( cgen, varoffs );
    varvalue_t value;
    memset( &value, 0, sizeof(value) );
    value.lbloffs = here;
    cgen_note_persistent( cgen );       // the label variable now points into the code
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
            if ( !cc_jump_to_target( cgen, comp, elsepart, gotok == TOK_GOSUB, 0 ) ) {
                return false;
            }
        } else if ( !( block ? cgen_from_block( cgen, comp, elsepart ) : cgen_from_stmt( cgen, comp, elsepart ) ) ) {
            return false;
        }
    }
    uint32_t endins = 0;
    if ( !cc_emit_bria( cgen, comp, &endins ) ) {
        return false;
    }
    uint32_t here = 0;
    if ( !cc_here( cgen, comp, &here ) ) {
        return false;
    }
    cc_patch_param( cgen, thenins, here );
    if ( isgoto ) {
        if ( !cc_jump_to_target( cgen, comp, thenpart, gotok == TOK_GOSUB, 0 ) ) {
            return false;
        }
    } else if ( !( block ? cgen_from_block( cgen, comp, thenpart ) : cgen_from_stmt( cgen, comp, thenpart ) ) ) {
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

// -- exits from a PROC / FUNC ---------------------------------------------------------

#define CGEN_PROCLEVEL  ((size_t) -1)       // the "loop level" of the jumps to the end of the procedure

// a jump to the end of the PROC / FUNC being generated; it waits in cgen->loopjumps until the end is known
static bool cc_add_procexit( codegen_t* cgen, compiler_t* comp, uint32_t* pins ) {
    if ( !cc_emit_bria( cgen, comp, pins ) ) {
        return false;
    }
    if ( !cgen_grow( (void**) &cgen->loopjumps, &cgen->numloopjumps, &cgen->maxloopjumps, sizeof(cgen_loopjump_t) ) ) {
        return cc_out_of_memory( comp );
    }
    cgen_loopjump_t* lj = &cgen->loopjumps[ cgen->numloopjumps++ ];
    lj->insoffs = *pins;
    lj->level   = CGEN_PROCLEVEL;
    lj->iterate = false;
    return true;
}

void cgen_resolve_procexits( codegen_t* cgen, uint32_t addr ) {
    size_t keep = 0;
    for ( size_t i=0; i < cgen->numloopjumps; ++i ) {
        cgen_loopjump_t lj = cgen->loopjumps[i];
        if ( lj.level == CGEN_PROCLEVEL ) {
            cc_patch_bria( cgen, lj.insoffs, addr );
        } else {
            cgen->loopjumps[ keep++ ] = lj;
        }
    }
    cgen->numloopjumps = keep;
}

bool cgen_from_localstmt( codegen_t* cgen, compiler_t* comp, node_t* node ) {
    /*
        NT_LOCALSTMT        LOCAL statement
            branches:
                - 1 or more variable references (numeric, integer or string base variables)
        Code:   LOCL(variable) for every variable: its value is saved and given back when the PROC / FUNC returns.
    */
    if ( node == NODE_NONE ) {
        return cgen_bad_node( comp );
    }
    if ( node->type != NT_LOCALSTMT || node->numbranches < 1U ) {
        return cgen_unexpected_node( comp );
    }
    if ( !cgen->inproc ) {
        comp_error( comp, "LOCAL outside a PROC or FUNC" );
        return false;
    }
    for ( size_t i=0; i < node->numbranches; ++i ) {
        uint8_t vartype = 0; uint16_t varoffs = VAROFFS_NONE;
        const node_t* item = node->branches[i];
        if ( item != NODE_NONE && ( item->type == NT_EMPTYARRAYREF || item->type == NT_ARRAYDECL ) && item->datalen == UINT16_C(3) ) {
            // a local array: LOCAL A() (an array that exists) or LOCAL A(10) (declared here)
            varoffs = (uint16_t)( ( ( (uint16_t) item->data[1] ) << UINT8_C(8) ) | item->data[2] );
        } else if ( !cc_base_var( comp, node->branches[i], &vartype, &varoffs ) ) {
            return false;
        }
        if ( !cgen_gen_locl( cgen, varoffs ) ) {
            return cgen_out_of_code_memory( comp );
        }
    }
    return true;
}

// -- loops -----------------------------------------------------------------------

static cgen_loopctx_t* cc_push_loop( codegen_t* cgen, compiler_t* comp, uint8_t kind ) {
    if ( !cgen_grow( (void**) &cgen->loops, &cgen->numloops, &cgen->maxloops, sizeof(cgen_loopctx_t) ) ) {
        cc_out_of_memory( comp );
        return 0;
    }
    cgen_loopctx_t* lc = &cgen->loops[ cgen->numloops++ ];
    memset( lc, 0, sizeof(*lc) );
    lc->kind = kind;
    lc->srcline = cc_srcline( comp );
    return lc;
}

// -- FOR / NEXT ----------------------------------------------------------------

// finds or creates the numeric variable named by a base variable reference node
static bool cc_base_var( compiler_t* comp, node_t* node, uint8_t* pvartype, uint16_t* pvaroffs ) {
    if ( node == NODE_NONE ) {
        return cgen_bad_node( comp );
    }
    if ( ( node->type != NT_NUMBASEVARREF && node->type != NT_STRBASEVARREF ) || node->datalen < UINT16_C(2) ) {
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

static bool cc_hidden_var( compiler_t* comp, const char* prefix, uint32_t serial, char which, uint16_t* pvaroffs ) {
    char name[32];
    snprintf( name, sizeof(name), "@%s%u%c", prefix, (unsigned) serial, which );
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
    return vartype == VARTYPEV_INT ? cgen_gen_wriv( cgen, varoffs ) :
        vartype == VARTYPEV_STR ? cgen_gen_wrsv( cgen, varoffs ) : cgen_gen_wrnv( cgen, varoffs );
}

// patches the EXIT / ITERATE jumps of the loop with the index "level" (just closed): ITERATE to contaddr, EXIT to exitaddr
static void cc_resolve_loopjumps( codegen_t* cgen, size_t level, uint32_t contaddr, uint32_t exitaddr ) {
    size_t keep = 0;
    for ( size_t i=0; i < cgen->numloopjumps; ++i ) {
        cgen_loopjump_t lj = cgen->loopjumps[i];
        if ( lj.level == level ) {
            cc_patch_bria( cgen, lj.insoffs, lj.iterate ? contaddr : exitaddr );
        } else {
            cgen->loopjumps[ keep++ ] = lj;
        }
    }
    cgen->numloopjumps = keep;
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
    if ( !cc_hidden_var( comp, "FOR", serial, 'L', &limvar ) || !cc_hidden_var( comp, "FOR", serial, 'S', &stepvar ) ) {
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
    // DOWNTO (data byte 1) counts down by the STEP value (default 1)
    bool downto = node->datalen == UINT16_C(1) && node->data[0] == UINT8_C(1);
    if ( node->branches[3] != NODE_NONE ) {
        if ( !cgen_from_numexpr( cgen, comp, node->branches[3] ) ) {
            return false;
        }
        if ( downto && !cgen_gen_neg( cgen ) ) {
            return cgen_out_of_code_memory( comp );
        }
    } else if ( !cgen_gen_phim( cgen, downto ? -1 : 1 ) ) {
        return cgen_out_of_code_memory( comp );
    }
    if ( !cgen_gen_wrnv( cgen, stepvar ) ) {
        return cgen_out_of_code_memory( comp );
    }
    // top: exit if ( step >= 0 AND var > limit ) OR ( step < 0 AND var < limit )
    uint32_t top = 0;
    if ( !cc_here( cgen, comp, &top ) ) {
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
    cgen_loopctx_t* fc = cc_push_loop( cgen, comp, CGEN_LOOP_FOR );
    if ( fc == 0 ) {
        return false;
    }
    fc->vartype = vartype;
    fc->varoffs = varoffs;
    fc->matchvar = varoffs;
    fc->matchvar2 = VAROFFS_NONE;
    fc->stepvar = stepvar;
    fc->topaddr = top;
    fc->bodyaddr = cgen->codesize;
    fc->exitins = exitins;
    return true;
}

// closes the innermost FOR loop, which must use the given variable
static bool cc_next( codegen_t* cgen, compiler_t* comp, bool anyvar, uint16_t varoffs ) {
    if ( cgen->numloops <= cgen->loopbase || cgen->loops[ cgen->numloops - 1U ].kind != CGEN_LOOP_FOR ||
         ( !anyvar && cgen->loops[ cgen->numloops - 1U ].matchvar != varoffs && cgen->loops[ cgen->numloops - 1U ].matchvar2 != varoffs ) ) {
        comp_error( comp, "NEXT without FOR" );
        return false;
    }
    cgen_loopctx_t fc = cgen->loops[ --cgen->numloops ];
    uint32_t contaddr = cgen->codesize;
    if ( !cc_read_var( cgen, fc.vartype, fc.varoffs ) || !cgen_gen_rrnv( cgen, fc.stepvar ) || !cgen_gen_add( cgen ) ||
         !cc_write_var( cgen, fc.vartype, fc.varoffs ) || !cgen_gen_bria( cgen, (int32_t) fc.topaddr ) ) {
        return cgen_out_of_code_memory( comp );
    }
    uint32_t here = 0;
    if ( !cc_here( cgen, comp, &here ) ) {
        return false;
    }
    cc_patch_param( cgen, fc.exitins, here );
    cc_resolve_loopjumps( cgen, cgen->numloops, contaddr, here );
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

// -- REPEAT / UNTIL / WHILE / WEND / UEND ------------------------------------------

static bool cc_check_simple( compiler_t* comp, node_t* node, uint8_t type ) {
    if ( node == NODE_NONE ) {
        return cgen_bad_node( comp );
    }
    if ( node->type != type ) {
        return cgen_unexpected_node( comp );
    }
    return true;
}

bool cgen_from_repeatstmt( codegen_t* cgen, compiler_t* comp, node_t* node ) {
    if ( !cc_check_simple( comp, node, NT_REPEATSTMT ) ) {
        return false;
    }
    uint32_t here = 0;
    if ( !cc_here( cgen, comp, &here ) ) {
        return false;
    }
    cgen_loopctx_t* lc = cc_push_loop( cgen, comp, CGEN_LOOP_REPEAT );
    if ( lc == 0 ) {
        return false;
    }
    lc->topaddr = lc->bodyaddr = here;
    return true;
}

// opens a loop that is left when the condition is false (WHILE) or true (UNTIL)
static bool cc_open_cond_loop( codegen_t* cgen, compiler_t* comp, node_t* cond, uint8_t kind ) {
    uint32_t top = 0;
    if ( !cc_here( cgen, comp, &top ) || !cgen_from_numexpr( cgen, comp, cond ) ) {
        return false;
    }
    if ( kind == CGEN_LOOP_WHILE && ( !cgen_gen_phim( cgen, 0 ) || !cgen_gen_cneq( cgen ) ) ) {
        return cgen_out_of_code_memory( comp );
    }
    uint32_t exitins = 0;
    if ( !cc_emit_phpa_c( cgen, comp, &exitins ) || !cgen_gen_jpcc( cgen ) ) {
        return cgen_out_of_code_memory( comp );
    }
    cgen_loopctx_t* lc = cc_push_loop( cgen, comp, kind );
    if ( lc == 0 ) {
        return false;
    }
    lc->topaddr  = top;
    lc->bodyaddr = cgen->codesize;
    lc->exitins  = exitins;
    return true;
}

bool cgen_from_whilestmt( codegen_t* cgen, compiler_t* comp, node_t* node ) {
    if ( !cc_check_simple( comp, node, NT_WHILESTMT ) ) {
        return false;
    }
    if ( node->numbranches != 1U ) {
        return cgen_unexpected_node( comp );
    }
    return cc_open_cond_loop( cgen, comp, node->branches[0], CGEN_LOOP_WHILE );
}

bool cgen_from_untilstmt( codegen_t* cgen, compiler_t* comp, node_t* node ) {
    if ( !cc_check_simple( comp, node, NT_UNTILSTMT ) ) {
        return false;
    }
    if ( node->numbranches != 1U ) {
        return cgen_unexpected_node( comp );
    }
    if ( cgen->numloops > cgen->loopbase && cgen->loops[ cgen->numloops - 1U ].kind == CGEN_LOOP_REPEAT ) {
        // closes the REPEAT loop: go back to the body while the condition is false
        cgen_loopctx_t lc = cgen->loops[ --cgen->numloops ];
        uint32_t contaddr = cgen->codesize;
        if ( !cgen_from_numexpr( cgen, comp, node->branches[0] ) ) {
            return false;
        }
        if ( !cgen_gen_phim( cgen, 0 ) || !cgen_gen_cneq( cgen ) || !cgen_gen_phpa_c( cgen, lc.bodyaddr ) ||
             !cgen_gen_jpcc( cgen ) ) {
            return cgen_out_of_code_memory( comp );
        }
        cc_resolve_loopjumps( cgen, cgen->numloops, contaddr, cgen->codesize );
        return true;
    }
    return cc_open_cond_loop( cgen, comp, node->branches[0], CGEN_LOOP_UNTIL );
}

// closes the innermost loop, which must be of the given kind
static bool cc_close_cond_loop( codegen_t* cgen, compiler_t* comp, uint8_t kind, const char* errortext ) {
    if ( cgen->numloops <= cgen->loopbase || cgen->loops[ cgen->numloops - 1U ].kind != kind ) {
        comp_error( comp, errortext );
        return false;
    }
    cgen_loopctx_t lc = cgen->loops[ --cgen->numloops ];
    if ( !cgen_gen_bria( cgen, (int32_t) lc.topaddr ) ) {
        return cgen_out_of_code_memory( comp );
    }
    uint32_t here = 0;
    if ( !cc_here( cgen, comp, &here ) ) {
        return false;
    }
    cc_patch_param( cgen, lc.exitins, here );
    cc_resolve_loopjumps( cgen, cgen->numloops, lc.topaddr, here );
    return true;
}

bool cgen_from_wendstmt( codegen_t* cgen, compiler_t* comp, node_t* node ) {
    return cc_check_simple( comp, node, NT_WENDSTMT ) && cc_close_cond_loop( cgen, comp, CGEN_LOOP_WHILE, "WEND without WHILE" );
}

bool cgen_from_uendstmt( codegen_t* cgen, compiler_t* comp, node_t* node ) {
    return cc_check_simple( comp, node, NT_UENDSTMT ) && cc_close_cond_loop( cgen, comp, CGEN_LOOP_UNTIL, "UEND without UNTIL" );
}

bool cgen_from_againstmt( codegen_t* cgen, compiler_t* comp, node_t* node ) {
    /*
        NT_AGAINSTMT        AGAIN statement
            data:
                - 2 bytes of loop number: 1 = innermost loop, 2 = the one around it, ...
    */
    if ( !cc_check_simple( comp, node, NT_AGAINSTMT ) ) {
        return false;
    }
    if ( node->datalen != UINT16_C(2) ) {
        return cgen_unexpected_node( comp );
    }
    size_t loopnum = ( ( (size_t) node->data[0] ) << UINT8_C(8) ) | node->data[1];
    if ( loopnum == 0 || loopnum > cgen->numloops - cgen->loopbase ) {
        comp_error( comp, "AGAIN without loop" );
        return false;
    }
    const cgen_loopctx_t* lc = &cgen->loops[ cgen->numloops - loopnum ];
    if ( !cgen_gen_bria( cgen, (int32_t) lc->bodyaddr ) ) {
        return cgen_out_of_code_memory( comp );
    }
    return true;
}

bool cgen_from_exitstmt( codegen_t* cgen, compiler_t* comp, node_t* node ) {
    /*
        NT_EXITSTMT         EXIT / LEAVE / ITERATE statement
            data:
                - 1 byte of kind: 1 = EXIT or LEAVE, 2 = ITERATE
                - 2 bytes of loop number: 1 = innermost loop, 2 = the one around it, ...
    */
    if ( !cc_check_simple( comp, node, NT_EXITSTMT ) ) {
        return false;
    }
    if ( node->datalen != UINT16_C(3) || node->data[0] < 1 || node->data[0] > 3 ) {
        return cgen_unexpected_node( comp );
    }
    if ( node->data[0] == 3 ) {
        // EXIT PROC / FUNC / FN
        if ( !cgen->inproc ) {
            comp_error( comp, "EXIT outside a PROC, FUNC or DEF FN" );
            return false;
        }
        uint32_t pins = 0;
        return cc_add_procexit( cgen, comp, &pins );
    }
    bool iterate = node->data[0] == 2;
    size_t loopnum = ( ( (size_t) node->data[1] ) << UINT8_C(8) ) | node->data[2];
    if ( loopnum == 0 || loopnum > cgen->numloops - cgen->loopbase ) {
        comp_error( comp, iterate ? "ITERATE without loop" : "EXIT without loop" );
        return false;
    }
    uint32_t ins = 0;
    if ( !cc_emit_bria( cgen, comp, &ins ) ) {
        return false;
    }
    if ( !cgen_grow( (void**) &cgen->loopjumps, &cgen->numloopjumps, &cgen->maxloopjumps, sizeof(cgen_loopjump_t) ) ) {
        return cc_out_of_memory( comp );
    }
    cgen_loopjump_t* lj = &cgen->loopjumps[ cgen->numloopjumps++ ];
    lj->insoffs = ins;
    lj->level   = cgen->numloops - loopnum;
    lj->iterate = iterate;
    return true;
}

bool cgen_from_foreachstmt( codegen_t* cgen, compiler_t* comp, node_t* node ) {
    /*
        NT_FOREACHSTMT      FOR EACH statement
            branches:
                - 1 branch of the value variable reference
                - 1 branch of the key variable reference (may be empty)
                - 1 branch of the array reference (NT_EMPTYARRAYREF)
        Code:   counter = 0  step = 1  top: ( counter >= ACNT(a) ) exit   value = AFLT(a, counter)  [ key = counter ]  body
                NEXT: counter = counter + step  BRIA top  exit:
    */
    if ( node == NODE_NONE ) {
        return cgen_bad_node( comp );
    }
    if ( node->type != NT_FOREACHSTMT || node->numbranches != 3U || node->branches[2] == NODE_NONE ) {
        return cgen_unexpected_node( comp );
    }
    const node_t* aref = node->branches[2];
    if ( aref->type != NT_EMPTYARRAYREF || aref->datalen != UINT16_C(3) ) {
        return cgen_unexpected_node( comp );
    }
    uint8_t arrtype = aref->data[0] & VARTYPEM_BASE;
    uint16_t arroffs = (uint16_t)( ( ( (uint16_t) aref->data[1] ) << UINT8_C(8) ) | aref->data[2] );
    uint8_t vartype = 0; uint16_t varoffs = VAROFFS_NONE;
    if ( !cc_base_var( comp, node->branches[0], &vartype, &varoffs ) ) {
        return false;
    }
    if ( ( vartype == VARTYPEV_STR ) != ( arrtype == VARTYPEV_STR ) ) {
        comp_error( comp, "Type mismatch (FOR EACH variable and array)" );
        return false;
    }
    uint8_t keytype = 0; uint16_t keyoffs = VAROFFS_NONE;
    if ( node->branches[1] != NODE_NONE ) {
        if ( !cc_base_var( comp, node->branches[1], &keytype, &keyoffs ) ) {
            return false;
        }
        if ( keytype == VARTYPEV_STR ) {
            comp_error( comp, "Numeric variable expected" );
            return false;
        }
    }
    uint32_t serial = cgen->forserial++;
    uint16_t cntvar = VAROFFS_NONE, stepvar = VAROFFS_NONE;
    if ( !cc_hidden_var( comp, "FOR", serial, 'L', &cntvar ) || !cc_hidden_var( comp, "FOR", serial, 'S', &stepvar ) ) {
        return false;
    }
    if ( !cgen_gen_phim( cgen, 0 ) || !cgen_gen_wrnv( cgen, cntvar ) || !cgen_gen_phim( cgen, 1 ) ||
         !cgen_gen_wrnv( cgen, stepvar ) ) {
        return cgen_out_of_code_memory( comp );
    }
    uint32_t top = 0;
    if ( !cc_here( cgen, comp, &top ) ) {
        return false;
    }
    if ( !cgen_gen_rrnv( cgen, cntvar ) || !cgen_gen_acnt( cgen, arroffs ) || !cgen_gen_cnge( cgen ) ) {
        return cgen_out_of_code_memory( comp );
    }
    uint32_t exitins = 0;
    if ( !cc_emit_phpa_c( cgen, comp, &exitins ) || !cgen_gen_jpcc( cgen ) ) {
        return cgen_out_of_code_memory( comp );
    }
    bool ok = cgen_gen_rrnv( cgen, cntvar ) && cgen_gen_aflt( cgen, arroffs ) && cc_write_var( cgen, vartype, varoffs );
    if ( ok && keyoffs != VAROFFS_NONE ) {
        ok = cgen_gen_rrnv( cgen, cntvar ) && cc_write_var( cgen, keytype, keyoffs );
    }
    if ( !ok ) {
        return cgen_out_of_code_memory( comp );
    }
    cgen_loopctx_t* fc = cc_push_loop( cgen, comp, CGEN_LOOP_FOR );
    if ( fc == 0 ) {
        return false;
    }
    fc->vartype = VARTYPEV_FLOAT;
    fc->varoffs = cntvar;
    fc->matchvar = vartype == VARTYPEV_STR ? VAROFFS_NONE : varoffs;
    fc->matchvar2 = keyoffs;
    fc->stepvar = stepvar;
    fc->topaddr = top;
    fc->bodyaddr = cgen->codesize;
    fc->exitins = exitins;
    return true;
}

// -- END, POP, ON --------------------------------------------------------------------

bool cgen_from_endstmt( codegen_t* cgen, compiler_t* comp, node_t* node ) {
    if ( !cc_check_simple( comp, node, NT_ENDSTMT ) ) {
        return false;
    }
    // branch to the end of the program; the address is known when everything has been generated
    uint32_t ins = 0;
    if ( !cc_emit_bria( cgen, comp, &ins ) ) {
        return false;
    }
    if ( !cgen_grow( (void**) &cgen->fixups, &cgen->numfixups, &cgen->maxfixups, sizeof(cgen_fixup_t) ) ) {
        return cc_out_of_memory( comp );
    }
    cgen_fixup_t* fx = &cgen->fixups[ cgen->numfixups++ ];
    fx->insoffs = ins;
    fx->lineno  = CGEN_FIXUP_END;
    fx->srcline = cc_srcline( comp );
    return true;
}

bool cgen_from_popstmt( codegen_t* cgen, compiler_t* comp, node_t* node ) {
    if ( !cc_check_simple( comp, node, NT_POPSTMT ) ) {
        return false;
    }
    if ( !cgen_gen_drop_c( cgen ) ) {
        return cgen_out_of_code_memory( comp );
    }
    return true;
}

bool cgen_from_onstmt( codegen_t* cgen, compiler_t* comp, node_t* node ) {
    /*
        NT_ONSTMT           ON statement
            data:
                - 1 byte of GOTO/GOSUB token
            branches:
                - 1 branch of numeric expression
                - 1 or more branches of goto/gosub targets
        Code:   <n> -> tmp   ( tmp >= i AND tmp < i+1 ) PHPA.C stub<i> JPCC   (for every target)   BRIA end
                stub<i>: [ PHPA.C end ] <jump to target i>   (for every target)   end:
    */
    if ( !cc_check_simple( comp, node, NT_ONSTMT ) ) {
        return false;
    }
    if ( node->datalen != UINT16_C(1) || node->numbranches < 2U ) {
        return cgen_unexpected_node( comp );
    }
    bool gosub = node->data[0] == TOK_GOSUB;
    if ( !gosub && node->data[0] != TOK_GOTO ) {
        return cgen_unexpected_node( comp );
    }
    size_t numtargets = node->numbranches - 1U;
    uint16_t tmp = VAROFFS_NONE;
    if ( !cc_hidden_var( comp, "ON", cgen->forserial++, 'O', &tmp ) ) {
        return false;
    }
    if ( !cgen_from_numexpr( cgen, comp, node->branches[0] ) || !cgen_gen_wrnv( cgen, tmp ) ) {
        return cgen_out_of_code_memory( comp );
    }
    // the tests: the offsets of the PHPA instructions that get the stub addresses go to the scratch list
    cgen->numscratch = 0;
    for ( size_t i=1; i <= numtargets; ++i ) {
        if ( !cgen_grow( (void**) &cgen->scratch, &cgen->numscratch, &cgen->maxscratch, sizeof(uint32_t) ) ) {
            return cc_out_of_memory( comp );
        }
        bool ok = cgen_gen_rrnv( cgen, tmp ) && cgen_gen_phim( cgen, (int32_t) i ) && cgen_gen_cnge( cgen ) &&
            cgen_gen_rrnv( cgen, tmp ) && cgen_gen_phim( cgen, (int32_t)( i + 1U ) ) && cgen_gen_cnlt( cgen ) && cgen_gen_and( cgen );
        if ( !ok ) {
            return cgen_out_of_code_memory( comp );
        }
        uint32_t phpains = 0;
        if ( !cc_emit_phpa_c( cgen, comp, &phpains ) || !cgen_gen_jpcc( cgen ) ) {
            return cgen_out_of_code_memory( comp );
        }
        cgen->scratch[ cgen->numscratch++ ] = phpains;
    }
    uint32_t endins = 0;
    if ( !cc_emit_bria( cgen, comp, &endins ) ) {
        return false;
    }
    // the stubs; a GOSUB returns to the end of the statement, so these return addresses are patched at the end
    size_t numtests = cgen->numscratch;
    for ( size_t i=0; i < numtargets; ++i ) {
        uint32_t here = 0;
        if ( !cc_here( cgen, comp, &here ) ) {
            return false;
        }
        cc_patch_param( cgen, cgen->scratch[i], here );
        uint32_t retins = 0;
        if ( !cc_jump_to_target( cgen, comp, node->branches[ i + 1U ], gosub, gosub ? &retins : 0 ) ) {
            return false;
        }
        if ( gosub ) {
            if ( !cgen_grow( (void**) &cgen->scratch, &cgen->numscratch, &cgen->maxscratch, sizeof(uint32_t) ) ) {
                return cc_out_of_memory( comp );
            }
            cgen->scratch[ cgen->numscratch++ ] = retins;
        }
    }
    uint32_t end = 0;
    if ( !cc_here( cgen, comp, &end ) ) {
        return false;
    }
    cc_patch_bria( cgen, endins, end );
    for ( size_t i = numtests; i < cgen->numscratch; ++i ) {
        cc_patch_param( cgen, cgen->scratch[i], end );
    }
    return true;
}
