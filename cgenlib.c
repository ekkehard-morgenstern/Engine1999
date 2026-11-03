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

    ADDITIONAL PERMISSION (GPLv3 section 7): the Engine1999 Runtime Exception
        applies. Programs written in the built-in programming language, and
        executables made from them with BUILD, do not fall under this license
        as the exception describes (see the file LICENSE-EXCEPTION).

    CONTACT INFO:
        E-Mail: ekkehard@ekkehardmorgenstern.de
        Mail: Ekkehard Morgenstern, Mozartstr. 1, D-76744 Woerth am Rhein,
              Germany, Europe
*/

#include "codegen.h"
#include "instset.h"
#include "runtime.h"
#include <strings.h>

/*
    Library mode of the code generator (docs/library-support.md, section 5): a library keeps out of the program's variables. Inside a PROC, FUNC or
    DEF FN the variables of a library may only be its parameters, its LOCAL variables and variables whose names start with the library's prefix; in a
    labelled code section (which runs in the global context) only the prefixed ones. Everything else is the compile error "Variable X is global in the
    library L".
*/

typedef struct _libvars_t {
    codegen_t*      cgen;
    compiler_t*     comp;
    const char**    allowed;        // the names (copies) of the parameters and LOCAL variables of the current function
    size_t          num, max;
    uint32_t        lineno;         // the line being checked (for the message)
    bool            ok;
} libvars_t;

static bool lv_add( libvars_t* lv, const char* name ) {
    if ( lv->num == lv->max ) {
        size_t cap = lv->max ? lv->max * 2U : 16U;
        const char** p = (const char**) realloc( lv->allowed, cap * sizeof(const char*) );
        if ( p == 0 ) {
            return false;
        }
        lv->allowed = p;
        lv->max = cap;
    }
    char* copy = strdup( name );            // (the names come from buffers that the next lookup overwrites)
    if ( copy == 0 ) {
        return false;
    }
    lv->allowed[ lv->num++ ] = copy;
    return true;
}

static bool is_varref_node( const node_t* n ) {
    return ( n->type == NT_NUMVARREF || n->type == NT_STRVARREF || n->type == NT_EMPTYARRAYREF || n->type == NT_ARRAYDECL ) && n->datalen == UINT16_C(3);
}

static const char* varref_name( libvars_t* lv, const node_t* n ) {
    uint16_t offs = (uint16_t)( ( ( (uint16_t) n->data[1] ) << UINT8_C(8) ) | n->data[2] );
    const char* name = 0;
    if ( !vmem_get_var_name( &lv->comp->rt->varmem, offs, &name ) ) {
        return 0;
    }
    return name;
}

// the names a LOCAL statement declares, anywhere below 'node'
static void lv_collect_locals( libvars_t* lv, node_t* node ) {
    if ( node == NODE_NONE ) {
        return;
    }
    if ( node->type == NT_LOCALSTMT ) {
        for ( size_t i=0; i < node->numbranches; ++i ) {
            node_t* b = node->branches[i];
            if ( b == NODE_NONE ) {
                continue;
            }
            if ( is_varref_node( b ) ) {
                const char* name = varref_name( lv, b );
                if ( name != 0 ) {
                    lv_add( lv, name );
                }
            } else if ( ( b->type == NT_NUMBASEVARREF || b->type == NT_STRBASEVARREF ) && b->datalen >= UINT16_C(2) ) {
                lv_add( lv, comp_node_name( lv->comp, b, 1U ) );        // LOCAL X: a base reference that has not been resolved
            }
        }
        return;
    }
    for ( size_t i=0; i < node->numbranches; ++i ) {
        lv_collect_locals( lv, node->branches[i] );
    }
}

static bool lv_name_ok( const libvars_t* lv, const char* name ) {
    if ( name[0] == '@' ) {
        return true;                        // the compiler's hidden variables (loops, function results)
    }
    const char* prefix = lv->cgen->libprefix;
    if ( prefix != 0 && prefix[0] != '\0' && strncasecmp( name, prefix, strlen( prefix ) ) == 0 ) {
        return true;
    }
    for ( size_t i=0; i < lv->num; ++i ) {
        if ( strcasecmp( lv->allowed[i], name ) == 0 ) {
            return true;
        }
    }
    return false;
}

static void lv_walk( libvars_t* lv, node_t* node ) {
    if ( node == NODE_NONE || !lv->ok ) {
        return;
    }
    if ( node->type == NT_STMTLINE && node->datalen == UINT16_C(3) ) {
        lv->lineno = ( ( (uint32_t) node->data[0] ) << UINT8_C(16) ) | ( ( (uint32_t) node->data[1] ) << UINT8_C(8) ) | node->data[2];
    }
    if ( node->type == NT_LOCALSTMT ) {
        return;                             // the declaration itself is fine (its index expressions are not looked at: constants)
    }
    bool isbase = ( node->type == NT_NUMBASEVARREF || node->type == NT_STRBASEVARREF ) && node->datalen >= UINT16_C(2);
    if ( isbase || is_varref_node( node ) ) {
        const char* name = isbase ? comp_node_name( lv->comp, node, 1U ) : varref_name( lv, node );
        if ( name != 0 && !lv_name_ok( lv, name ) ) {
            char msg[200];
            const char* prefix = lv->cgen->libprefix;
            if ( prefix != 0 && prefix[0] != '\0' ) {
                snprintf( msg, sizeof(msg), "Variable %s is global in the library (declare it LOCAL, or start its name with %s)", name, prefix );
            } else {
                snprintf( msg, sizeof(msg), "Variable %s is global in the library (declare it LOCAL; the library has no prefix)", name );
            }
            lv->comp->iter.hdr.lineno = lv->lineno;
            lv->ok = false;
            comp_error( lv->comp, msg );
        }
    }
    for ( size_t i=0; i < node->numbranches && lv->ok; ++i ) {
        lv_walk( lv, node->branches[i] );
    }
}

static void lv_add_params( libvars_t* lv, uint16_t funcoffs ) {
    uint8_t ftype = 0, numparams = 0; uint32_t code = 0;
    if ( !vmem_get_func_info( &lv->comp->rt->varmem, funcoffs, &ftype, &numparams, &code ) ) {
        return;
    }
    for ( uint8_t i=0; i < numparams; ++i ) {
        uint8_t ptype = 0; const char* pname = 0;
        if ( vmem_get_func_param( &lv->comp->rt->varmem, funcoffs, i, &ptype, &pname ) && pname != 0 ) {
            lv_add( lv, pname );
        }
    }
}

// Checks the variables of one top-level statement of a library: a PROC / FUNC (parameters and LOCALs allowed), a DEF FN (its parameters and LOCALs)
// or any other statement of a labelled code section (prefixed variables only). comp_error() ends the compile (longjmp) on a violation.
bool cgen_check_library_vars( codegen_t* cgen, compiler_t* comp, node_t* stmt, uint32_t lineno ) {
    libvars_t lv;
    memset( &lv, 0, sizeof(lv) );
    lv.cgen = cgen;
    lv.comp = comp;
    lv.lineno = lineno;
    lv.ok = true;
    node_t* body = stmt;
    if ( stmt->type == NT_PROCSTMT && stmt->datalen == UINT16_C(3) ) {
        lv_add_params( &lv, (uint16_t)( ( ( (uint16_t) stmt->data[1] ) << UINT8_C(8) ) | stmt->data[2] ) );
        lv_collect_locals( &lv, stmt );
    } else if ( stmt->type == NT_DEFSTMT && stmt->numbranches == 1U && stmt->branches[0] != NODE_NONE &&
                stmt->branches[0]->type == NT_USRFNDECL && stmt->branches[0]->datalen == UINT16_C(3) ) {
        node_t* decl = stmt->branches[0];
        lv_add_params( &lv, (uint16_t)( ( ( (uint16_t) decl->data[1] ) << UINT8_C(8) ) | decl->data[2] ) );
        lv_collect_locals( &lv, decl );
    }
    lv_walk( &lv, body );
    for ( size_t i=0; i < lv.num; ++i ) {
        free( (char*) lv.allowed[i] );
    }
    free( lv.allowed );
    return lv.ok;
}
