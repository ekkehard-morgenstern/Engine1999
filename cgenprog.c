
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
Code generation for the program commands LOAD, RUN, LIST, NEW, CLEAR, DELETE and RENUM.

    LOAD name        <name>  LOAD
    RUN [target]     <label name or "">  <line number or -1>  RUNP
    DELETE a[-[b]]   <label a or "">  <line a or -1>  <label b or "">  <line b or -1>  DELE
    RENUM [a][-[b]][,step]   (the same four operands)  <step, 10 if omitted>  RENM
    NEW              NEW
    CLEAR            CLR
    LIST [a][-[b]]   <label a or "">  <line a or -1>  <label b or "">  <line b or -1>  LIST

A target is a line number or a label. The label is passed by name, because the instruction is carried out by the host
on a program that is compiled anew (RUN, LOAD) or looked up in the source text (LIST). A single LIST target means
that line only (both ends are the same), "LIST a-" lists up to the end and "LIST -b" from the beginning.
*/

// pushes a constant string (from the data segment)
static bool cp_push_string( codegen_t* cgen, compiler_t* comp, const char* text ) {
    size_t len = strlen( text );
    uint16_t off = DATAOFFS_NONE;
    if ( len >= UINT16_MAX || !cgen_alloc_data( cgen, (uint16_t)( len + 1U ), &off ) || off == DATAOFFS_NONE ) {
        return cgen_out_of_data_memory( comp );
    }
    memcpy( &cgen->data[ off ], text, len + 1U );
    if ( !cgen_gen_phim( cgen, (int32_t) off ) || !cgen_gen_las_d( cgen ) ) {
        return cgen_out_of_code_memory( comp );
    }
    return true;
}

// pushes a constant number as a double in the data segment (big endian, like the NT_NUMLIT node data)
static bool cp_push_number( codegen_t* cgen, compiler_t* comp, double val ) {
    union {
        uint64_t    ui64;
        double      dbl;
    } u;
    u.dbl = val;
    uint16_t off = DATAOFFS_NONE;
    if ( !cgen_alloc_data( cgen, UINT16_C(8), &off ) || off == DATAOFFS_NONE ) {
        return cgen_out_of_data_memory( comp );
    }
    for ( unsigned i=0; i < 8U; ++i ) {
        cgen->data[ off + i ] = (uint8_t)( u.ui64 >> ( 8U * ( 7U - i ) ) );
    }
    if ( !cgen_gen_phpa_d( cgen, off ) || !cgen_gen_lan_d( cgen ) ) {
        return cgen_out_of_code_memory( comp );
    }
    return true;
}

// pushes <label name or ""> and <line number or -1> for a goto target node (NODE_NONE = neither)
static bool cp_push_target( codegen_t* cgen, compiler_t* comp, node_t* target ) {
    const char* label = "";
    double lineno = -1.0;
    if ( target != NODE_NONE ) {
        if ( target->type != NT_GOTOTARGET || target->datalen != UINT16_C(4) ) {
            return cgen_bad_node( comp );
        }
        uint32_t value = ( ( (uint32_t) target->data[1] ) << UINT8_C(16) ) | ( ( (uint32_t) target->data[2] ) << UINT8_C(8) ) |
            target->data[3];
        if ( target->data[0] == VARTYPEV_INT ) {
            lineno = (double) value;
        } else if ( target->data[0] == VARTYPEV_LABEL ) {
            if ( !vmem_get_var_name( &comp->rt->varmem, (uint16_t) value, &label ) ) {
                comp_error( comp, "Internal error (label variable not found)" );
                return false;
            }
        } else {
            return cgen_unexpected_node( comp );
        }
    }
    return cp_push_string( cgen, comp, label ) && cp_push_number( cgen, comp, lineno );
}

bool cgen_from_loadstmt( codegen_t* cgen, compiler_t* comp, node_t* node ) {
    /*
        NT_LOADSTMT     LOAD statement
            branches:
                - 1 branch of string expression (file name)
    */
    if ( node == NODE_NONE ) {
        return cgen_bad_node( comp );
    }
    if ( node->type != NT_LOADSTMT || node->numbranches != 1U ) {
        return cgen_unexpected_node( comp );
    }
    if ( !cgen_from_strexpr( cgen, comp, node->branches[0] ) ) {
        return false;
    }
    if ( !cgen_gen_load( cgen ) ) {
        return cgen_out_of_code_memory( comp );
    }
    return true;
}

bool cgen_from_runstmt( codegen_t* cgen, compiler_t* comp, node_t* node ) {
    /*
        NT_RUNSTMT      RUN statement
            branches:
                - none, or 1 branch of goto target (NT_GOTOTARGET: a line number or a label)
    */
    if ( node == NODE_NONE ) {
        return cgen_bad_node( comp );
    }
    if ( node->type != NT_RUNSTMT || node->numbranches > 1U ) {
        return cgen_unexpected_node( comp );
    }
    if ( !cp_push_target( cgen, comp, node->numbranches ? node->branches[0] : NODE_NONE ) ) {
        return false;
    }
    if ( !cgen_gen_runp( cgen ) ) {
        return cgen_out_of_code_memory( comp );
    }
    return true;
}

bool cgen_from_liststmt( codegen_t* cgen, compiler_t* comp, node_t* node ) {
    /*
        NT_LISTSTMT     LIST statement
            data:
                - 1 byte of flags: LISTF_FIRST, LISTF_LAST (the targets that are present), LISTF_RANGE (minus given)
            branches:
                - the start target if LISTF_FIRST, then the end target if LISTF_LAST (NT_GOTOTARGET)
    */
    if ( node == NODE_NONE ) {
        return cgen_bad_node( comp );
    }
    if ( node->type != NT_LISTSTMT || node->datalen != UINT16_C(1) ) {
        return cgen_unexpected_node( comp );
    }
    uint8_t flags = node->data[0];
    size_t expected = ( ( flags & LISTF_FIRST ) ? 1U : 0U ) + ( ( flags & LISTF_LAST ) ? 1U : 0U );
    if ( node->numbranches != expected ) {
        return cgen_unexpected_node( comp );
    }
    node_t* first = ( flags & LISTF_FIRST ) ? node->branches[0] : NODE_NONE;
    node_t* last  = ( flags & LISTF_LAST ) ? node->branches[ expected - 1U ] : NODE_NONE;
    if ( ( flags & LISTF_RANGE ) == 0 && first != NODE_NONE ) {
        last = first;       // a single target lists that line only
    }
    if ( !cgen_gen_channel0( cgen, comp ) || !cp_push_target( cgen, comp, first ) || !cp_push_target( cgen, comp, last ) ) {
        return false;
    }
    if ( !cgen_gen_list( cgen ) ) {
        return cgen_out_of_code_memory( comp );
    }
    return true;
}

bool cgen_from_newstmt( codegen_t* cgen, compiler_t* comp, node_t* node ) {
    // NT_NEWSTMT      NEW statement (no data, no branches)
    if ( node == NODE_NONE ) {
        return cgen_bad_node( comp );
    }
    if ( node->type != NT_NEWSTMT ) {
        return cgen_unexpected_node( comp );
    }
    return cgen_gen_new( cgen ) || cgen_out_of_code_memory( comp );
}

bool cgen_from_clearstmt( codegen_t* cgen, compiler_t* comp, node_t* node ) {
    // NT_CLEARSTMT    CLEAR statement (no data, no branches)
    if ( node == NODE_NONE ) {
        return cgen_bad_node( comp );
    }
    if ( node->type != NT_CLEARSTMT ) {
        return cgen_unexpected_node( comp );
    }
    return cgen_gen_clr( cgen ) || cgen_out_of_code_memory( comp );
}

// the start and end targets of a range statement (LIST, DELETE, RENUM) from the node; <single> = what a single target means:
// true: that line only (both ends), false: from there to the end
static bool cp_range_targets( compiler_t* comp, node_t* node, bool single, node_t** pfirst, node_t** plast, node_t** pextra ) {
    if ( node->datalen != UINT16_C(1) ) {
        return cgen_unexpected_node( comp );
    }
    uint8_t flags = node->data[0];
    size_t expected = ( ( flags & LISTF_FIRST ) ? 1U : 0U ) + ( ( flags & LISTF_LAST ) ? 1U : 0U ) +
        ( ( flags & LISTF_STEP ) ? 1U : 0U );
    if ( node->numbranches != expected ) {
        return cgen_unexpected_node( comp );
    }
    size_t at = 0;
    *pfirst = ( flags & LISTF_FIRST ) ? node->branches[ at++ ] : NODE_NONE;
    *plast  = ( flags & LISTF_LAST )  ? node->branches[ at++ ] : NODE_NONE;
    *pextra = ( flags & LISTF_STEP )  ? node->branches[ at++ ] : NODE_NONE;
    if ( single && ( flags & LISTF_RANGE ) == 0 && *pfirst != NODE_NONE ) {
        *plast = *pfirst;
    }
    return true;
}

bool cgen_from_deletestmt( codegen_t* cgen, compiler_t* comp, node_t* node ) {
    // NT_DELETESTMT   DELETE statement, same layout as NT_LISTSTMT (at least one target)
    if ( node == NODE_NONE ) {
        return cgen_bad_node( comp );
    }
    if ( node->type != NT_DELETESTMT ) {
        return cgen_unexpected_node( comp );
    }
    node_t* first = NODE_NONE; node_t* last = NODE_NONE; node_t* extra = NODE_NONE;
    if ( !cp_range_targets( comp, node, true, &first, &last, &extra ) ||
         !cp_push_target( cgen, comp, first ) || !cp_push_target( cgen, comp, last ) ) {
        return false;
    }
    return cgen_gen_dele( cgen ) || cgen_out_of_code_memory( comp );
}

bool cgen_from_renumstmt( codegen_t* cgen, compiler_t* comp, node_t* node ) {
    /*
        NT_RENUMSTMT    RENUM statement
            data:
                - 1 byte of flags: LISTF_FIRST, LISTF_LAST, LISTF_RANGE as for LIST, LISTF_STEP
            branches:
                - the start target, the end target (as flagged), then the step (numeric expression)
        A single target means "from there to the end" (renumbering one line is pointless).
    */
    if ( node == NODE_NONE ) {
        return cgen_bad_node( comp );
    }
    if ( node->type != NT_RENUMSTMT ) {
        return cgen_unexpected_node( comp );
    }
    node_t* first = NODE_NONE; node_t* last = NODE_NONE; node_t* step = NODE_NONE;
    if ( !cp_range_targets( comp, node, false, &first, &last, &step ) ||
         !cp_push_target( cgen, comp, first ) || !cp_push_target( cgen, comp, last ) ) {
        return false;
    }
    if ( step != NODE_NONE ? !cgen_from_numexpr( cgen, comp, step ) : !cp_push_number( cgen, comp, 10.0 ) ) {
        return false;
    }
    return cgen_gen_renm( cgen ) || cgen_out_of_code_memory( comp );
}
