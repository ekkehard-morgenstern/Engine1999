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

#include "bascomp.h"
#include "runtime.h"

void init_compiler( compiler_t* comp, struct _runtime_t* rt, program_t* pgm, bool keepmemory ) {
    comp->rt = rt;
    clear_iter( &comp->iter, pgm );
    comp->tokp     = 0;
    comp->tokend   = 0;
    comp->param    = 0;
    comp->number   = 0.0;
    comp->currtok  = TOK_NONE;
    comp->name     = 0;
    comp->namealloc = 0;
    comp->curfn    = NODE_NONE;
    comp->ctxstk   = 0;
    // CAUTION: Set the "keepmemory" flag only if you know what you're doing!
    if ( keepmemory ) {
        return;
    }
    comp->report   = 0;
    comp->halt     = 0;
    comp->userdata = 0;
    comp->nodes    = 0;
    comp->syntree  = NODE_NONE;
}

void cleanup_compiler( compiler_t* comp ) {
    while ( comp->ctxstk ) {
        comp_ctxstk_t* ctx = comp->ctxstk;
        comp->ctxstk = ctx->prev;
        free( ctx->param );
        free( ctx );
    }
    free( comp->param );
    comp->param = 0;
    free( comp->name );
    comp->name = 0;
    comp->namealloc = 0;
    comp_free_tree( comp );
}

const char* comp_prefixed_name( compiler_t* comp, const char* prefix, const void* name, size_t len ) {
    size_t plen = strlen( prefix );
    if ( plen + len + 1U > comp->namealloc ) {
        size_t alloc = plen + len + 1U < 64U ? 64U : plen + len + 1U;
        char* p = (char*) realloc( comp->name, alloc );
        if ( p == 0 ) {
            comp_error( comp, "Out of memory" );
        }
        comp->name = p;
        comp->namealloc = alloc;
    }
    memmove( comp->name + plen, name, len );    // the name may be in the buffer already
    memcpy( comp->name, prefix, plen );
    comp->name[ plen + len ] = '\0';
    return comp->name;
}

const char* comp_node_name( compiler_t* comp, const node_t* node, size_t skip ) {
    size_t len = node->datalen > skip ? node->datalen - skip : 0;
    return comp_prefixed_name( comp, "", node->data + skip, len );
}

void comp_set_tokens( compiler_t* comp, const uint8_t* tokens, size_t size ) {
    comp->tokp   = (uint8_t*) tokens;
    comp->tokend = tokens + size;
}

void comp_error( compiler_t* comp, const char* text ) {
    char buf[128];
    if ( comp->iter.hdr.lineno != LINENO_NONE ) {
        snprintf( buf, 128U, "? %s in line %u\n", text, (unsigned) comp->iter.hdr.lineno );
    } else {
        snprintf( buf, 128U, "? %s\n", text );
    }
    if ( comp->report ) {
        comp->report( comp, comp->userdata, buf );
    } else {
        fprintf( stderr, "%s", buf );
    }
    if ( comp->halt ) {
        comp->halt( comp, comp->userdata );
    } else {
        exit( EXIT_FAILURE );
    }
}

static void out_of_memory( compiler_t* comp ) ATTR_NORETURN;

static void out_of_memory( compiler_t* comp ) {
    comp_error( comp, "Out of memory" );
}

static char* dup_param( compiler_t* comp, const char* text ) {
    size_t len = strlen( text );
    char* copy = (char*) malloc( len + 1U );
    if ( copy == 0 ) {
        out_of_memory( comp );
    }
    memcpy( copy, text, len + 1U );
    return copy;
}

// builds node data from the current token's text: <type.8> <text...> (no terminator)
// the caller frees the result.
static uint8_t* comp_param_data( compiler_t* comp, uint8_t type, uint16_t* pdatalen ) {
    size_t len = comp->param ? strlen( comp->param ) : 0;
    if ( len > UINT16_MAX - 1U ) {
        comp_error( comp, "Name or literal too long" );
        return 0;
    }
    uint8_t* data = (uint8_t*) malloc( 1U + len );
    if ( data == 0 ) {
        out_of_memory( comp );
        return 0;
    }
    data[0] = type;
    if ( len ) {
        memcpy( &data[1], comp->param, len );
    }
    *pdatalen = (uint16_t)( 1U + len );
    return data;
}

void comp_push_context( compiler_t* comp ) {
    comp_ctxstk_t* ctx = (comp_ctxstk_t*) malloc( sizeof(comp_ctxstk_t) );
    if ( ctx == 0 ) {
        comp_error( comp, "Out of memory" );
        return;
    }
    ctx->prev = comp->ctxstk; comp->ctxstk = ctx;
    memcpy( &ctx->iter, &comp->iter, sizeof(pgmiter_t) );
    ctx->tokp = comp->tokp; ctx->tokend = comp->tokend; ctx->currtok = comp->currtok;
    ctx->param = 0; ctx->number = comp->number;
    switch ( ctx->currtok ) {
        case TOK_HEXLIT: case TOK_DECLIT: case TOK_OCTLIT: case TOK_QUALIT: case TOK_BINLIT:
            ctx->number = comp->number;
            break;
        case TOK_IDENT: case TOK_NUMIDENT: case TOK_STRIDENT: case TOK_INTIDENT:
        case TOK_STRLIT: case TOK_SHLLIT: case TOK_QUOLIT: case TOK_BRKLIT: case TOK_BRCLIT:
            if ( comp->param ) {
                ctx->param = dup_param( comp, comp->param );
            }
            break;
    }
}

void comp_commit_context( compiler_t* comp ) {
    comp_ctxstk_t* ctx = comp->ctxstk;
    if ( ctx == 0 ) {
        return;
    }
    comp->ctxstk = ctx->prev;
    free( ctx->param );
    free( ctx );
}

void comp_pop_context( compiler_t* comp ) {
    comp_ctxstk_t* ctx = comp->ctxstk;
    if ( ctx == 0 ) {
        return;
    }
    comp->ctxstk = ctx->prev;
    memcpy( &comp->iter, &ctx->iter, sizeof(pgmiter_t) );
    comp->tokp = ctx->tokp; comp->tokend = ctx->tokend; comp->currtok = ctx->currtok;
    switch ( comp->currtok ) {
        case TOK_HEXLIT: case TOK_DECLIT: case TOK_OCTLIT: case TOK_QUALIT: case TOK_BINLIT:
            comp->number = ctx->number;
            break;
        case TOK_IDENT: case TOK_NUMIDENT: case TOK_STRIDENT: case TOK_INTIDENT:
        case TOK_STRLIT: case TOK_SHLLIT: case TOK_QUOLIT: case TOK_BRKLIT: case TOK_BRCLIT:
            if ( ctx->param ) {
                free( comp->param );
                comp->param = ctx->param;   // ownership moves back to the compiler
                ctx->param = 0;
            }
            break;
    }
    free( ctx->param );
    free( ctx );
}

void comp_free_tree( compiler_t* comp ) {
    node_t* node = comp->nodes;
    while ( node ) {
        node_t* next = node->chain;
        free( node->branches );
        free( node );
        node = next;
    }
    comp->nodes = 0;
    comp->syntree = NODE_NONE;
}

bool comp_create_node( compiler_t* comp, node_t** pnode, uint8_t nodetype, uint8_t numbranches, uint16_t datalen,
    const void* pdata, ... ) {
    if ( comp == 0 || pnode == 0 || nodetype == NT_UNKNOWN || ( datalen != UINT16_C(0) && pdata == 0 ) ) {
        return false;
    }
    node_t* node = (node_t*) malloc( sizeof(node_t) + datalen );
    if ( node == 0 ) {
        return false;
    }
    node->branches = 0;
    node->numbranches = 0;
    node->maxbranches = 0;
    node->datalen = datalen;
    node->type = nodetype;
    if ( datalen ) {
        memcpy( node->data, pdata, datalen );
    }
    // chain the node, so that it is freed with the tree even if it gets abandoned
    node->chain = comp->nodes;
    comp->nodes = node;
    if ( numbranches ) {
        va_list ap; va_start( ap, pdata );
        bool ok = true;
        while ( numbranches-- ) {
            node_t* branch = va_arg( ap, node_t* );
            if ( !comp_add_branch( comp, node, branch ) ) {
                ok = false;
                break;
            }
        }
        va_end( ap );
        if ( !ok ) {
            return false;
        }
    }
    *pnode = node;
    return true;
}

bool comp_add_branch( compiler_t* comp, node_t* node, node_t* branch ) {
    // NOTE: a branch may be NODE_NONE, which stands for an optional branch that isn't present
    if ( node == NODE_NONE ) {
        return false;
    }
    if ( node->numbranches == node->maxbranches ) {
        size_t newmax = node->maxbranches ? node->maxbranches * 2U : 4U;
        node_t** newbranches = (node_t**) realloc( node->branches, newmax * sizeof(node_t*) );
        if ( newbranches == 0 ) {
            return false;
        }
        node->branches = newbranches;
        node->maxbranches = newmax;
    }
    node->branches[ node->numbranches++ ] = branch;
    return true;
}

bool comp_eat_list( compiler_t* comp, node_t** pnode, uint8_t nodetype, comp_eatfn_t element_eater, uint8_t septok,
    const char* errortext ) {
    // list := element { SEPTOK element } .  -- if SEPTOK is TOK_EOL, there's no separator token
    node_t* expr1 = NODE_NONE;
    if ( !element_eater( comp, &expr1 ) || expr1 == NODE_NONE ) {
        return false;
    }
    node_t* node = NODE_NONE;
    for (;;) {
        bool mandatory = false; node_t* expr2 = NODE_NONE;
        if ( septok != TOK_EOL && comp->currtok == septok ) {
            // read next token
            if ( !comp_fetchtok( comp ) ) {
                // stop processing
                break;
            }
            mandatory = true;   // the following expression is mandatory
        } else if ( septok != TOK_EOL ) {
            // have a separator, but it's not present
            break;
        }
        if ( !element_eater( comp, &expr2 ) || expr2 == NODE_NONE ) {
            if ( mandatory ) {  // mandatory expression missing: stop
                comp_error( comp, errortext );
            }
            // stop processing
            break;
        }
        // we have now a new branch; first see if we already have a node or need to create one
        if ( node == NODE_NONE ) {
            if ( !comp_create_node( comp, &node, nodetype, UINT8_C(2), UINT16_C(0), 0, expr1, expr2 ) ||
                 node == NODE_NONE ) {
                // failed to create node: cancel operation
OOM:            out_of_memory( comp );
                break;
            }
        } else {
            // the node already exists: add a new branch
            if ( !comp_add_branch( comp, node, expr2 ) ) {
                // something went wrong: cancel
                goto OOM;
            }
        }
        // successful, continue
    }
    // either return node with what we already have, or just the first branch
    *pnode = node != NODE_NONE ? node : expr1;
    return true;
}

bool comp_eat_numexlist( compiler_t* comp, node_t** pnode ) {
    // num-ex-list := num-expr { TOK_COMMA num-expr } .
    return comp_eat_list( comp, pnode, NT_NUMEXLIST, comp_eat_numexpr, TOK_COMMA, "Numeric expression expected" );
}

bool comp_eat_strexlist( compiler_t* comp, node_t** pnode ) {
    // str-ex-list := str-expr { TOK_COMMA str-expr } .
    return comp_eat_list( comp, pnode, NT_STREXLIST, comp_eat_strexpr, TOK_COMMA, "String expression expected" );
}

bool comp_eat_exprlist( compiler_t* comp, node_t** pnode ) {
    // expr-list := expr { TOK_COMMA expr } .
    return comp_eat_list( comp, pnode, NT_EXPRLIST, comp_eat_expr, TOK_COMMA, "Expression expected" );
}

bool comp_eat_arrayindex( compiler_t* comp, node_t** pnode ) {
    // array-index := num-ex-list | str-expr .
    if ( comp_eat_numexlist( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    if ( comp_eat_strexpr( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    return false;
}

bool comp_eat_arraysub( compiler_t* comp, node_t** pnode ) {
    /*
        NT_ARRAYSUB     array subscript
            data: none
            branches: 1
                - points to either NT_NUMEXLIST or string expression
            immediate processing: none
    */
    // array-sub := TOK_LPAREN array-index TOK_RPAREN .
    if ( comp->currtok == TOK_LPAREN ) {
        if ( !comp_fetchtok( comp ) ) {
ERROR:      comp_error( comp, "Array index expected" );
            return false;
        }
        node_t* arrayindexnode = NODE_NONE;
        if ( !comp_eat_arrayindex( comp, &arrayindexnode ) || arrayindexnode == NODE_NONE ) {
            goto ERROR;
        }
        if ( comp->currtok != TOK_RPAREN || !comp_fetchtok( comp ) ) {
            comp_error( comp, "Closing parenthesis ')' expected" );
            return false;
        }
        if ( !comp_create_node( comp, pnode, NT_ARRAYSUB, UINT8_C(1), UINT16_C(0), 0, arrayindexnode ) ) {
            out_of_memory( comp );
            return false;
        }
        return true;
    }
    return false;
}

bool comp_node_iter_branches( compiler_t* comp, node_t* node, void* userdata, bool (*callback)( void*, node_t* ) ) {
    if ( node == NODE_NONE || node->numbranches == 0 || callback == 0 ) {
        return false;
    }
    for ( size_t i=0; i < node->numbranches; ++i ) {
        if ( !callback( userdata, node->branches[i] ) ) {
            return false;
        }
    }
    return true;
}

double comp_extract_float( const uint8_t* mem, uint16_t offs ) {
    uint64_t val =
        ( ( (uint64_t) mem[ offs      ] ) << UINT8_C(56) ) |
        ( ( (uint64_t) mem[ offs + 1U ] ) << UINT8_C(48) ) |
        ( ( (uint64_t) mem[ offs + 2U ] ) << UINT8_C(40) ) |
        ( ( (uint64_t) mem[ offs + 3U ] ) << UINT8_C(32) ) |
        ( ( (uint32_t) mem[ offs + 4U ] ) << UINT8_C(24) ) |
        ( ( (uint32_t) mem[ offs + 5U ] ) << UINT8_C(16) ) |
        ( ( (uint16_t) mem[ offs + 6U ] ) << UINT8_C( 8) ) |
                       mem[ offs + 7U ]                    ;
    union {
        uint64_t ui64;
        double   dbl;
    } u;
    u.ui64 = val;
    return u.dbl;
}

/*
static void comp_write_float( uint8_t* mem, uint16_t offs, double val ) {
    union {
        uint64_t ui64;
        double   dbl;
    } u;
    u.dbl = val;
    mem[ offs      ] = (uint8_t)( u.ui64 >> UINT8_C(56) );
    mem[ offs + 1U ] = (uint8_t)( u.ui64 >> UINT8_C(48) );
    mem[ offs + 2U ] = (uint8_t)( u.ui64 >> UINT8_C(40) );
    mem[ offs + 3U ] = (uint8_t)( u.ui64 >> UINT8_C(32) );
    mem[ offs + 4U ] = (uint8_t)( u.ui64 >> UINT8_C(24) );
    mem[ offs + 5U ] = (uint8_t)( u.ui64 >> UINT8_C(16) );
    mem[ offs + 6U ] = (uint8_t)( u.ui64 >> UINT8_C( 8) );
    mem[ offs + 7U ] = (uint8_t)( u.ui64                );
}
*/

static bool comp_gather_dims( void* userdata, node_t* node ) {
    compiler_t* comp = (compiler_t*) userdata;
    if ( comp->numdim >= MAXDIM ) {
        comp_error( comp, "Too many dimensions" );
        return false;
    }
    //  <nodetype.8> <numbranches.8> <datalen.16> <firstbranch.16> <lastbranch.16> <data...>
    uint8_t nodetype = node->type;
    if ( nodetype != NT_NUMLIT ) {
        if ( nodetype == NT_NUMEXPR ) {
            if ( !comp_node_iter_branches( comp, node, comp, comp_gather_dims ) ) {
                return false;
            }
            return true;
        }
        comp_error( comp, "Number expected" );
        return false;
    }
    uint16_t datalen = node->datalen;
    if ( datalen != UINT16_C(8) ) {
        comp_error( comp, "Bad number" );
        return false;
    }
    double val = comp_extract_float( node->data, 0 );
    if ( val < 1.0 || val >= 65536.0 ) {
        comp_error( comp, "Dimension out of range" );
        return false;
    }
    uint16_t v16 = (uint16_t)( (int64_t) val );
    comp->arraydim[ comp->numdim++ ] = v16;
    return true;
}

bool comp_eat_arraydimdecl( compiler_t* comp, node_t** pnode ) {
    /*
    NT_ARRAYDIMDECL array dimension declaration
        data:
            - 1 byte (TOK_DYNAMIC or TOK_ASSOC), if specified
            - 2 bytes per dimension of size information, if specified
        branches: none
        immediate processing:
            - if given, the numeric expressions are evaluated to
              see if they're constant. it's an error if they aren't.
            - the total size of the expected array is computed
              it's an error if it's too small or too large.
            - this computes the list of output dimensions
    */
    // array-dim-decl := num-expr-list | TOK_DYNAMIC | TOK_ASSOC .
    uint8_t data = TOK_EOL; uint16_t datalen = UINT16_C(0);
    if ( comp->currtok == TOK_DYNAMIC || comp->currtok == TOK_ASSOC ) {
        data = comp->currtok; ++datalen;
        if ( !comp_fetchtok( comp ) ) {
            return false;
        }
        if ( !comp_create_node( comp, pnode, NT_ARRAYDIMDECL, UINT8_C(0), datalen, &data ) || *pnode == NODE_NONE ) {
OOM:        out_of_memory( comp );
            return false;
        }
        return true;
    }
    node_t* numexpr = NODE_NONE;
    if ( !comp_eat_numexlist( comp, &numexpr ) || numexpr == NODE_NONE ) {
        comp_error( comp, "Numeric expression expected" );
        return false;
    }
    comp->numdim = UINT8_C(0);
    if ( numexpr->type == NT_NUMEXLIST ) {
        // multiple dimensions
        if ( !comp_node_iter_branches( comp, numexpr, comp, comp_gather_dims ) ) {
            return false;
        }
    } else if ( !comp_gather_dims( comp, numexpr ) ) { // single dimension
        return false;
    }
    // make sure we have at least one dimension
    if ( comp->numdim == UINT8_C(0) ) {
        comp_error( comp, "Internal error (zero dimensions)" );
        return false;
    }
    // finally, create the node
    uint16_t datalen2 = ( (uint16_t) comp->numdim ) * UINT16_C(2);
    uint8_t  data2[ MAXDIM * 2U ];
    size_t   totalsize = 0;
    for ( uint8_t i=UINT8_C(0); i < comp->numdim; ++i ) {
        if ( totalsize == 0 ) {
            totalsize += comp->arraydim[i];
        } else if ( comp->arraydim[i] && totalsize > SIZE_MAX / comp->arraydim[i] ) {
LARGE:      comp_error( comp, "Array too large" );
            return false;
        } else {
            totalsize *= comp->arraydim[i];
        }
        data2[ i * 2U      ] = (uint8_t)( comp->arraydim[i] >> UINT8_C(8) );
        data2[ i * 2U + 1U ] = (uint8_t)  comp->arraydim[i];
    }
    if ( totalsize == 0 || totalsize > MAXELEMS ) {
        goto LARGE;
    }
    if ( !comp_create_node( comp, pnode, NT_ARRAYDIMDECL, UINT8_C(0), datalen2, data2 ) || *pnode == NODE_NONE ) {
        goto OOM;
    }
    return true;
}

bool comp_eat_arraydecl( compiler_t* comp, node_t** pnode ) {
    /*
     NT_ARRAYDECL    array declaration
        data:
            - 1 byte of type indicator, 2 bytes of variable offset
        branches: none
        immediate processing:
            - the variable is looked up, to see if it exists
            - if it does, it's an error
            - if it doesn't, the variable is created, and the type and
              offset stored in the data field
    */
    // array-decl := any-base-var-ref TOK_LPAREN array-dim-decl TOK_RPAREN .
    node_t* varnode = NODE_NONE;
    if ( !comp_eat_anybasevarref( comp, &varnode ) || varnode == NODE_NONE ) {
        return false;
    }
    if ( comp->currtok != TOK_LPAREN || !comp_fetchtok( comp ) ) {
        comp_error( comp, "Opening parenthesis '(' expected" );
        return false;
    }
    node_t* declnode = NODE_NONE;
    if ( !comp_eat_arraydimdecl( comp, &declnode ) || declnode == NODE_NONE ) {
        comp_error( comp, "Array dimension declaration expected" );
        return false;
    }
    if ( comp->currtok != TOK_RPAREN || !comp_fetchtok( comp ) ) {
        comp_error( comp, "Closing parenthesis ')' expected" );
        return false;
    }

    // read dimension declaration
    //  <nodetype.8> <numbranches.8> <datalen.16> <firstbranch.16> <lastbranch.16> <data...>
    if ( declnode->type != NT_ARRAYDIMDECL || declnode->numbranches != 0 ) {
        comp_error( comp, "Internal error (NT_ARRAYDIMDECL expected)" );
        return false;
    }

    uint16_t datalen = declnode->datalen;
    if ( datalen <= UINT16_C(1) ) { // DYNAMIC or ASSOC
        comp_error( comp, "Feature not supported yet" );
        return false;
    }

    const uint8_t* datap = declnode->data;
    uint16_t numdims = datalen / UINT16_C(2);
    if ( numdims > MAXDIM ) {
        comp_error( comp, "Internal error (unexpected number of dimensions)" );
        return false;
    }
    uint16_t dims[MAXDIM];
    for ( uint16_t i=0; i < numdims; ++i ) {
        dims[i] = (uint16_t)( ( ( (uint16_t) datap[0] ) << UINT8_C(8) ) | datap[1] );
        datap += UINT16_C(2);
    }

    /*
        NT_NUMBASEVARREF    numeric base variable reference
        NT_STRBASEVARREF    string base variable reference
            data:
                - 1 byte of type indicator, n bytes of name
    */
    // read variable reference
    //  <nodetype.8> <numbranches.8> <datalen.16> <firstbranch.16> <lastbranch.16> <data...>
    datalen  = varnode->datalen;
    datap = varnode->data;
    const char* name = 0;
    uint8_t vartype = VARTYPEF_ARRAY;
    if ( varnode->type == NT_NUMBASEVARREF || varnode->type == NT_STRBASEVARREF ) {
        if ( datalen < UINT16_C(2) ) {
            comp_error( comp, "Internal error (fault in variable reference)" );
            return false;
        }
        // data:
        //    - 1 byte of type indicator, n bytes of name
        vartype |= datap[0] & VARTYPEM_BASE;
        name = comp_node_name( comp, varnode, 1U );
    } else {
        comp_error( comp, "Internal error (base variable reference expected)" );
        return false;
    }

    uint16_t varoffs = VAROFFS_NONE;
    bool ok = vmem_create_var( &comp->rt->varmem, vartype, name, numdims, dims, UINT8_C(0), 0, &varoffs );
    if ( !ok || varoffs == VAROFFS_NONE ) {
        comp_error( comp, "Failed to create variable" );
        return false;
    }

    uint8_t data[3];
    data[0] = vartype;
    data[1] = (uint8_t)( varoffs >> UINT8_C(8) );
    data[2] = (uint8_t)  varoffs;

    if ( !comp_create_node( comp, pnode, NT_ARRAYDECL, UINT8_C(0), UINT16_C(3), data ) || *pnode == NODE_NONE ) {
        out_of_memory( comp );
        return false;
    }

    return true;
}

bool comp_eat_arraydecllist( compiler_t* comp, node_t** pnode ) {
    /*
        NT_ARRAYDECLLIST    array declaration list
            data: none
            branches: the NT_ARRAYDECL nodes
    */
    // array-decl-list := array-decl { TOK_COMMA array-decl } .
    return comp_eat_list( comp, pnode, NT_ARRAYDECLLIST, comp_eat_arraydecl, TOK_COMMA, "Array declaration expected" );
}

bool comp_eat_emptyarrayref( compiler_t* comp, node_t** pnode ) {
    /*
        NT_EMPTYARRAYREF    empty array reference, needed for ERASE statement
            data:
                - 1 byte of type indicator, 2 bytes of variable offset
            branches: none
            immediate processing:
                - the array is looked up, it's an error if it doesn't exist
                - the variable offset is encoded in the data field

        ATTENTION:
            - there's a problem when generated code erases a variable that is subsequently used or redefined.
            - this needs to be solved TBD FIXME
    */
    // empty-array-ref := any-base-var-ref TOK_LPAREN TOK_RPAREN .
    node_t* varnode = NODE_NONE;
    if ( !comp_eat_anybasevarref( comp, &varnode ) || varnode == NODE_NONE ) {
        return false;
    }
    if ( comp->currtok != TOK_LPAREN || !comp_fetchtok( comp ) ) {
        comp_error( comp, "Opening parenthesis '(' expected" );
        return false;
    }
    if ( comp->currtok != TOK_RPAREN || !comp_fetchtok( comp ) ) {
        comp_error( comp, "Closing parenthesis ')' expected" );
        return false;
    }
    /*
        NT_NUMBASEVARREF    numeric base variable reference
        NT_STRBASEVARREF    string base variable reference
            data:
                - 1 byte of type indicator, n bytes of name
    */
    // read variable reference
    //  <nodetype.8> <numbranches.8> <datalen.16> <firstbranch.16> <lastbranch.16> <data...>
    uint16_t datalen  = varnode->datalen;
    const uint8_t* datap = varnode->data;
    const char* name = 0;
    uint8_t vartype = VARTYPEF_ARRAY;
    if ( varnode->type == NT_NUMBASEVARREF || varnode->type == NT_STRBASEVARREF ) {
        if ( datalen < UINT16_C(2) ) {
            comp_error( comp, "Internal error (fault in variable reference)" );
            return false;
        }
        // data:
        //    - 1 byte of type indicator, n bytes of name
        vartype |= datap[0] & VARTYPEM_BASE;
        name = comp_node_name( comp, varnode, 1U );
    } else {
        comp_error( comp, "Internal error (base variable reference expected)" );
        return false;
    }
    uint16_t varoffs = VAROFFS_NONE;
    bool ok = vmem_lookup_var( &comp->rt->varmem, vartype, name, &varoffs );
    if ( !ok || varoffs == VAROFFS_NONE ) {
        comp_error( comp, "Undefined variable" );
        return false;
    }
    /*
            data:
                - 1 byte of type indicator, 2 bytes of variable offset
            branches: none
            immediate processing:
                - the array is looked up, it's an error if it doesn't exist
                - the variable offset is encoded in the data field
    */
    uint8_t data[3];
    data[0] = vartype;
    data[1] = (uint8_t)( varoffs >> UINT8_C(8) );
    data[2] = (uint8_t)  varoffs;
    if ( !comp_create_node( comp, pnode, NT_EMPTYARRAYREF, UINT8_C(0), UINT16_C(3), data ) || *pnode == NODE_NONE ) {
        out_of_memory( comp );
        return false;
    }

    return true;
}

bool comp_eat_emptyarrayreflist( compiler_t* comp, node_t** pnode ) {
    /*
        NT_EMPTYARRAYREFLIST    empty array reference list
            data: none
            branches: the NT_EMPTYARRAYREF nodes
    */
    // empty-array-ref-list := empty-array-ref { TOK_COMMA empty-array-ref } .
    return comp_eat_list( comp, pnode, NT_EMPTYARRAYREFLIST, comp_eat_emptyarrayref, TOK_COMMA, "Array reference expected" );
}

bool comp_eat_numbasevarref( compiler_t* comp, node_t** pnode ) {
    /*
        NT_NUMBASEVARREF    numeric base variable reference
            data:
                - 1 byte of type indicator, n bytes of name
            branches: none
            immediate processing: none (!)
            note:
                - because it's used in compound contexts, the variable reference
                  cannot be resolved here.
    */
    // num-base-var-ref := TOK_NUMIDENT | TOK_INTIDENT .

    if ( comp->currtok != TOK_NUMIDENT && comp->currtok != TOK_INTIDENT ) {
        return false;
    }
    uint16_t datalen = 0;
    uint8_t* data = comp_param_data( comp, comp->currtok == TOK_NUMIDENT ? VARTYPEV_FLOAT : VARTYPEV_INT, &datalen );
    if ( data == 0 ) {
        return false;
    }
    if ( !comp_fetchtok( comp ) ) {
        free( data );
        return false;
    }

    bool ok = comp_create_node( comp, pnode, NT_NUMBASEVARREF, UINT8_C(0), datalen, data ) && *pnode != NODE_NONE;
    free( data );
    if ( !ok ) {
        out_of_memory( comp );
        return false;
    }

    return true;
}

static bool comp_eat_varref( compiler_t* comp, node_t** pnode, uint8_t basetype,
    bool (*baseeater)( compiler_t*, node_t** ), uint8_t nodetype ) {

    /*
        NT_NUMVARREF        numeric variable reference
        NT_STRVARREF        string variable reference
            data:
                - 1 byte of type indicator
                - 2 bytes of variable offset
            branches:
                - list of array index expressions
    */
    // num-var-ref      := num-base-var-ref [ array-sub ] .
    node_t* varnode = NODE_NONE;
    if ( !baseeater( comp, &varnode ) || varnode == NODE_NONE ) {
        return false;
    }
    node_t* arraysubnode = NODE_NONE; node_t* arrayindexnode = NODE_NONE;
    if ( !comp_eat_arraysub( comp, &arraysubnode ) || varnode == NODE_NONE ) {
        arrayindexnode = NODE_NONE;
    } else {    // NT_ARRAYSUB
        /*
            NT_ARRAYSUB     array subscript
                data: none
                branches: 1
                    - points to either NT_NUMEXLIST or string expression
                immediate processing: none
        */
        //  <nodetype.8> <numbranches.8> <datalen.16> <firstbranch.16> <lastbranch.16> <data...>
        //  <nodepos.16> <nextbranch.16>
        if ( arraysubnode->type != NT_ARRAYSUB ) {
NOSUB:      comp_error( comp, "Internal error (array subscript expected)" );
            return false;
        }
        if ( arraysubnode->numbranches == 0 ) {
            goto NOSUB;
        }
        arrayindexnode = arraysubnode->branches[0];
        if ( arrayindexnode == NODE_NONE ) {
            goto NOSUB;
        }
    }

    /*
        NT_NUMBASEVARREF    numeric base variable reference
        NT_STRBASEVARREF    string base variable reference
            data:
                - 1 byte of type indicator, n bytes of name
    */
    // read variable reference
    //  <nodetype.8> <numbranches.8> <datalen.16> <firstbranch.16> <lastbranch.16> <data...>
    uint16_t datalen  = varnode->datalen;
    const uint8_t* datap = varnode->data;
    const char* name = 0;
    uint8_t vartype = arraysubnode != NODE_NONE ? VARTYPEF_ARRAY : UINT8_C(0);
    if ( varnode->type == basetype ) {
        if ( datalen < UINT16_C(2) ) {
            comp_error( comp, "Internal error (fault in variable reference)" );
            return false;
        }
        // data:
        //    - 1 byte of type indicator, n bytes of name
        vartype |= datap[0] & VARTYPEM_BASE;
        name = comp_node_name( comp, varnode, 1U );
    } else {
        comp_error( comp, "Internal error (base variable reference expected)" );
        return false;
    }

    // lookup or create variable
    uint16_t varoffs = VAROFFS_NONE;
    bool ok = vmem_lookup_var( &comp->rt->varmem, vartype, name, &varoffs );
    if ( !ok || varoffs == VAROFFS_NONE ) {
        // variable not found: create it
        if ( vartype & VARTYPEF_ARRAY ) {
            // default array: single dimension, 10 elements
            uint16_t dim = UINT16_C(10);
            ok = vmem_create_var( &comp->rt->varmem, vartype, name, UINT8_C(1), &dim, UINT8_C(0), 0, &varoffs );
        } else {
            // default regular variable, initialized to 0 or empty string
            ok = vmem_create_var( &comp->rt->varmem, vartype, name, UINT8_C(0), 0, UINT8_C(0), 0, &varoffs );
        }
        if ( !ok || varoffs == VAROFFS_NONE ) {
            out_of_memory( comp );
            return false;
        }
    }

    // create result node
    /*
            data:
                - 1 byte of type indicator
                - 2 bytes of variable offset
            branches:
                - list of array index expressions
    */
    uint8_t data[3];
    data[0] = vartype;
    data[1] = (uint8_t)( varoffs >> UINT8_C(8) );
    data[2] = (uint8_t)  varoffs;

    if ( arrayindexnode != NODE_NONE ) {
        ok = comp_create_node( comp, pnode, nodetype, UINT8_C(1), UINT8_C(3), data, arrayindexnode );
    } else {
        ok = comp_create_node( comp, pnode, nodetype, UINT8_C(0), UINT8_C(3), data );
    }
    if ( !ok || *pnode == NODE_NONE ) {
        out_of_memory( comp );
        return false;
    }

    return true;
}

bool comp_eat_numvarref( compiler_t* comp, node_t** pnode ) {
    return comp_eat_varref( comp, pnode, NT_NUMBASEVARREF, comp_eat_numbasevarref, NT_NUMVARREF );
}

bool comp_eat_strbasevarref( compiler_t* comp, node_t** pnode ) {
    /*
        NT_STRBASEVARREF    string base variable reference
            data:
                - 1 byte of type indicator, n bytes of name
            branches: none
            immediate processing: none (!)
            note:
                - because it's used in compound contexts, the variable reference
                  cannot be resolved here.
    */
    // str-base-var-ref := TOK_STRIDENT .

    if ( comp->currtok != TOK_STRIDENT ) {
        return false;
    }
    uint16_t datalen = 0;
    uint8_t* data = comp_param_data( comp, VARTYPEV_STR, &datalen );
    if ( data == 0 ) {
        return false;
    }
    if ( !comp_fetchtok( comp ) ) {
        free( data );
        return false;
    }

    bool ok = comp_create_node( comp, pnode, NT_STRBASEVARREF, UINT8_C(0), datalen, data ) && *pnode != NODE_NONE;
    free( data );
    if ( !ok ) {
        out_of_memory( comp );
        return false;
    }

    return true;
}

bool comp_eat_strvarref( compiler_t* comp, node_t** pnode ) {
    return comp_eat_varref( comp, pnode, NT_STRBASEVARREF, comp_eat_strbasevarref, NT_STRVARREF );
}

bool comp_eat_anybasevarref( compiler_t* comp, node_t** pnode ) {
    /*
        NT_NUMBASEVARREF    numeric base variable reference
        NT_STRBASEVARREF    string base variable reference
            data:
                - 1 byte of type indicator, n bytes of name
            branches: none
            immediate processing: none (!)
            note:
                - because it's used in compound contexts, the variable reference
                  cannot be resolved here.
    */
    // any-base-var-ref := TOK_NUMIDENT | TOK_INTIDENT | TOK_STRIDENT .

    if ( comp->currtok != TOK_NUMIDENT && comp->currtok != TOK_INTIDENT && comp->currtok != TOK_STRIDENT ) {
        return false;
    }
    uint8_t vartype = VARTYPEV_FLOAT;
    if ( comp->currtok == TOK_STRIDENT ) {
        vartype = VARTYPEV_STR;
    } else if ( comp->currtok == TOK_INTIDENT ) {
        vartype = VARTYPEV_INT;
    }
    uint16_t datalen = 0;
    uint8_t* data = comp_param_data( comp, vartype, &datalen );
    if ( data == 0 ) {
        return false;
    }
    if ( !comp_fetchtok( comp ) ) {
        free( data );
        return false;
    }

    uint8_t nodetype = vartype == VARTYPEV_STR ? NT_STRBASEVARREF : NT_NUMBASEVARREF;

    bool ok = comp_create_node( comp, pnode, nodetype, UINT8_C(0), datalen, data ) && *pnode != NODE_NONE;
    free( data );
    if ( !ok ) {
        out_of_memory( comp );
        return false;
    }

    return true;
}

bool comp_eat_numlit( compiler_t* comp, node_t** pnode ) {
    // dec-lit := TOK_DECLIT | TOK_DEC0 .. TOK_DEC9 .
    // num-lit := dec-lit | TOK_HEXLIT | TOK_OCTLIT | TOK_QUALIT | TOK_BINLIT .
    double num = 0;
    uint8_t tok = comp->currtok;
    switch ( tok ) {
        case TOK_DECLIT: case TOK_HEXLIT: case TOK_OCTLIT: case TOK_QUALIT: case TOK_BINLIT:
            num = comp->number;
            break;
        case TOK_DEC0: case TOK_DEC1: case TOK_DEC2: case TOK_DEC3: case TOK_DEC4:
        case TOK_DEC5: case TOK_DEC6: case TOK_DEC7: case TOK_DEC8: case TOK_DEC9:
            num = tok - TOK_DEC0;
            break;
        default:
            return false;
    }
    if ( !comp_fetchtok( comp ) ) {
        return false;
    }
    /*
        NT_DECLIT       decimal literal
            data:
                - numeric value, 8 bytes in network byte order
            immediate processing:
                - consumes TOK_DECLIT or TOK_DEC0..DEC9, and generates a floating-point representation,
                  which is then stored into the data field.

        NT_NUMLIT       numeric literal
            data:
                - numeric value, 8 bytes in network byte order
            immediate processing:
                - attempts NT_DECLIT first, and if successful, returns it as a NT_NUMLIT node.
                - otherwise, attempts the other number base literals (hex, oct, quad and bin)
    */
    union {
        uint64_t    ui64;
        double      dbl;
    } u;
    u.dbl = num;
    uint8_t data[8];
    data[0] = (uint8_t)( u.ui64 >> UINT8_C(56) );
    data[1] = (uint8_t)( u.ui64 >> UINT8_C(48) );
    data[2] = (uint8_t)( u.ui64 >> UINT8_C(40) );
    data[3] = (uint8_t)( u.ui64 >> UINT8_C(32) );
    data[4] = (uint8_t)( u.ui64 >> UINT8_C(24) );
    data[5] = (uint8_t)( u.ui64 >> UINT8_C(16) );
    data[6] = (uint8_t)( u.ui64 >> UINT8_C( 8) );
    data[7] = (uint8_t)( u.ui64                );
    if ( !comp_create_node( comp, pnode, NT_NUMLIT, UINT8_C(0), UINT16_C(8), data ) || *pnode == NODE_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_strlit( compiler_t* comp, node_t** pnode ) {
    // str-lit := TOK_STRLIT | TOK_SHLLIT | TOK_BRKLIT | TOK_BRCLIT .
    /*
       NT_STRLIT       string literal
        data:
            - 1 byte of type indicator (can be string, shell, bracket or brace literal)
            - n bytes of text
        branches: none
        note:
            - note that shell/bracket/brace literals aren't evaluated here, just gathered.
    */
    switch ( comp->currtok ) {
        case TOK_STRLIT:
        case TOK_SHLLIT:
        case TOK_BRKLIT:
        case TOK_BRCLIT:
            break;
        default:
            return false;
    }
    uint16_t datalen = 0;
    uint8_t* data = comp_param_data( comp, comp->currtok, &datalen );
    if ( data == 0 ) {
        return false;
    }
    if ( !comp_fetchtok( comp ) ) {
        free( data );
        return false;
    }
    bool ok = comp_create_node( comp, pnode, NT_STRLIT, UINT8_C(0), datalen, data ) && *pnode != NODE_NONE;
    free( data );
    if ( !ok ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_strlits( compiler_t* comp, node_t** pnode ) {
    // str-lits := str-lit { str-lit } .
    /*
        NT_STRLITS      string literals
            data: none
            branches:
                - list of string literals (NT_STRLIT)
    */
    return comp_eat_list( comp, pnode, NT_STRLITS, comp_eat_strlit, TOK_EOL, "String literal expected" );
}

bool comp_eat_numusrfnname( compiler_t* comp, node_t** pnode ) {
    // num-usr-fn-name := TOK_FN ( TOK_NUMIDENT | TOK_INTIDENT ) .
    if ( comp->currtok != TOK_FN ) {
        return false;
    }
    comp_push_context( comp );
    uint8_t tok = TOK_EOL;
    if ( !comp_fetchtok( comp ) || ( tok = comp->currtok, ( tok != TOK_NUMIDENT && tok != TOK_INTIDENT ) ) ||
         !comp_fetchtok( comp ) ) {
        comp_pop_context( comp );
        return false;
    }
    comp_commit_context( comp );

    /*
        NT_NUMUSRFNNAME     numeric user function name
            data:
                - 1 byte of type indicator
                - n bytes of name
    */
    uint16_t datalen = 0;
    uint8_t* data = comp_param_data( comp, tok == TOK_NUMIDENT ? VARTYPEV_FLOAT : VARTYPEV_INT, &datalen );
    if ( data == 0 ) {
        return false;
    }

    bool ok = comp_create_node( comp, pnode, NT_NUMUSRFNNAME, UINT8_C(0), datalen, data ) && *pnode != NODE_NONE;
    free( data );
    if ( !ok ) {
        out_of_memory( comp );
        return false;
    }

    return true;
}

bool comp_eat_strusrfnname( compiler_t* comp, node_t** pnode ) {
    // str-usr-fn-name := TOK_FN TOK_STRIDENT .
    if ( comp->currtok != TOK_FN ) {
        return false;
    }
    comp_push_context( comp );
    uint8_t tok = TOK_EOL;
    if ( !comp_fetchtok( comp ) || ( tok = comp->currtok, tok != TOK_STRIDENT ) ||
         !comp_fetchtok( comp ) ) {
        comp_pop_context( comp );
        return false;
    }
    comp_commit_context( comp );

    /*
        NT_STRUSRFNNAME     string user function name
            data:
                - 1 byte of type indicator
                - n bytes of name
    */
    uint16_t datalen = 0;
    uint8_t* data = comp_param_data( comp, VARTYPEV_STR, &datalen );
    if ( data == 0 ) {
        return false;
    }

    bool ok = comp_create_node( comp, pnode, NT_STRUSRFNNAME, UINT8_C(0), datalen, data ) && *pnode != NODE_NONE;
    free( data );
    if ( !ok ) {
        out_of_memory( comp );
        return false;
    }

    return true;
}

bool comp_eat_usrfnarg( compiler_t* comp, node_t** pnode ) {
    // usr-fn-arg := any-base-var-ref [ TOK_LPAREN TOK_RPAREN ] .
    /*
        NT_USRFNARG         user function argument
            data:
                - 1 byte of type indicator
                - n bytes of name (NUL-terminated)
    */
    node_t* namenode = NODE_NONE;
    if ( !comp_eat_anybasevarref( comp, &namenode ) || namenode == NODE_NONE ) {
        return false;
    }
    bool is_array = false;
    if ( comp->currtok == TOK_LPAREN && comp_fetchtok( comp ) ) {
        if ( comp->currtok != TOK_RPAREN || !comp_fetchtok( comp ) ) {
            comp_error( comp, "Closing parenthesis ')' expected" );
        }
        is_array = true;
    }
    /*
        NT_NUMBASEVARREF    numeric base variable reference
        NT_STRBASEVARREF    numeric base variable reference
            data:
                - 1 byte of type indicator, n bytes of name
    */
    //  <nodetype.8> <numbranches.8> <datalen.16> <firstbranch.16> <lastbranch.16> <data...>
    uint8_t nodetype = namenode->type;
    if ( nodetype != NT_NUMBASEVARREF && nodetype != NT_STRBASEVARREF ) {
UNEXP:  comp_error( comp, "Internal error (unexpected node type)" );
        return false;
    }
    uint16_t datalen = namenode->datalen;
    if ( datalen < 2U || datalen == UINT16_MAX ) {
        goto UNEXP;
    }
    const uint8_t* datap = namenode->data;
    // data is the type indicator, the name and a NUL terminator
    uint8_t* data = (uint8_t*) malloc( (size_t) datalen + 1U );
    if ( data == 0 ) {
        out_of_memory( comp );
        return false;
    }
    data[0] = datap[0] & VARTYPEM_BASE;
    if ( is_array ) {
        data[0] |= VARTYPEF_ARRAY;
    }
    memcpy( &data[1], &datap[1], datalen - 1U );
    data[ datalen ] = '\0';
    bool ok = comp_create_node( comp, pnode, NT_USRFNARG, UINT8_C(0), (uint16_t)( datalen + 1U ), data ) && *pnode != NODE_NONE;
    free( data );
    if ( !ok ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_usrfnarglist( compiler_t* comp, node_t** pnode ) {
    // usr-fn-arg-list := usr-fn-arg { TOK_COMMA usr-fn-arg } .
    /*
    NT_USERFNARGLIST    user function argument list
        branches:
            - 2 or more branches of user function argument declarations
        immediate processing:
            - generated only if there are more than 2 arguments
    */
    return comp_eat_list( comp, pnode, NT_USERFNARGLIST, comp_eat_usrfnarg, TOK_COMMA, "User function argument expected" );
}

bool comp_eat_anyusrfnname( compiler_t* comp, node_t** pnode ) {
    // any-usr-fn-name := num-usr-fn-name | str-usr-fn-name .
    if ( comp_eat_numusrfnname( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    if ( comp_eat_strusrfnname( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    return false;
}

bool comp_eat_singlelineusrfnbody( compiler_t* comp, node_t** pnode ) {
    // singleline-usr-fn-body := TOK_EQ expr .
    if ( comp->currtok != TOK_EQ ) {
        return false;
    }
    node_t* exprnode = NODE_NONE;
    if ( !comp_fetchtok( comp ) || !comp_eat_expr( comp, &exprnode ) || exprnode == NODE_NONE ) {
        comp_error( comp, "Expression expected" );
        return false;
    }
    /*
        NT_SINGLELINEUSRFNBODY  single-line user function body
            branches:
                - 1 branch of expression
    */
    if ( !comp_create_node( comp, pnode, NT_SINGLELINEUSERFNBODY, UINT8_C(1), UINT8_C(0), 0, exprnode ) ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_multilineusrfnbody( compiler_t* comp, node_t** pnode ) {
    // multiline-usr-fn-body := TOK_EOL stmt-lines TOK_END TOK_FN .
    if ( comp->currtok != TOK_EOL ) {
        return false;
    }
    node_t* stmtlines = NODE_NONE;
    if ( !comp_fetchtok( comp ) || !comp_eat_stmtlines( comp, &stmtlines ) || stmtlines == NODE_NONE ) {
        comp_error( comp, "Function body expected" );
        return false;
    }
    if ( comp->currtok != TOK_END || !comp_fetchtok( comp ) ) {
        comp_error( comp, "END expected" );
        return false;
    }
    if ( comp->currtok != TOK_FN || !comp_fetchtok( comp ) ) {
        comp_error( comp, "FN expected" );
        return false;
    }
    /*
        NT_MULTILINEUSERFNBODY  multi-line user function body
            branches:
                - 0 or more branches of statement lines
    */
    if ( !comp_create_node( comp, pnode, NT_MULTILINEUSERFNBODY, UINT8_C(1), UINT8_C(0), 0, stmtlines ) ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_usrfnbody( compiler_t* comp, node_t** pnode ) {
    // usr-fn-body := singleline-usr-fn-body | multiline-usr-fn-body .
    // [ NT_USRFNBODY - not generated ]
    if ( comp_eat_singlelineusrfnbody( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    if ( comp_eat_multilineusrfnbody( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    return false;
}

typedef struct _cgbargs_t {
    compiler_t* comp;
    node_t**    branches;
    size_t      maxbranches;
    size_t      numbranches;
} cgbargs_t;

static bool cgb_worker( void* userdata, node_t* nodepos ) {
    cgbargs_t* args = (cgbargs_t*) userdata;
    if ( args->numbranches >= args->maxbranches ) {
        comp_error( args->comp, "Too many arguments" );
        return false;
    }
    args->branches[ args->numbranches++ ] = nodepos;
    return true;
}

static bool comp_gather_branches( compiler_t* comp, node_t* node, uint8_t listtype, node_t** branches, size_t maxbranches,
    size_t* pnumbranches ) {
    if ( node == NODE_NONE ) {  // no arguments at all
        *pnumbranches = 0U;
        return true;
    }
    if ( node->type == listtype ) {
        cgbargs_t args = { comp, branches, maxbranches, 0 };
        if ( !comp_node_iter_branches( comp, node, &args, cgb_worker ) ) {
            return false;
        }
        *pnumbranches = args.numbranches;
    } else if ( maxbranches >= 1U ) {
        branches[0] = node;
        *pnumbranches = 1U;
    } else {
        comp_error( comp, "Internal error (invalid argument)" );
        return false;
    }
    return true;
}

static bool comp_get_func_params( compiler_t* comp, node_t* paramoffs[MAXPARAM], size_t numparams, usrparam_t params[MAXPARAM] ) {
    // usr-fn-arg := any-base-var-ref [ TOK_LPAREN TOK_RPAREN ] .
    //
    /*
        NT_USRFNARG         user function argument
        data:
            - 1 byte of type indicator
            - n bytes of name (NUL-terminated)
    */
    //  <nodetype.8> <numbranches.8> <datalen.16> <firstbranch.16> <lastbranch.16> <data...>
    for ( size_t i=0; i < numparams && i < MAXPARAM; ++i ) {
        node_t* nodepos = paramoffs[i];
        if ( nodepos->type != NT_USRFNARG ) {
UNEXP:      comp_error( comp, "Internal error (unexpected parameter node)" );
            return false;
        }
        uint16_t datalen = nodepos->datalen;
        if ( datalen < 2U ) {
            goto UNEXP;
        }
        params[i].paramtype = nodepos->data[0];
        params[i].paramname = (char*)( &nodepos->data[1] );
    }
    return true;
}

bool comp_eat_usrfndecl( compiler_t* comp, node_t** pnode ) {
    // usr-fn-decl := any-usr-fn-name [ TOK_LPAREN [ usr-fn-arg-list ] TOK_RPAREN ] usr-fn-body .
    node_t* fnname = NODE_NONE;
    if ( !comp_eat_anyusrfnname( comp, &fnname ) || fnname == NODE_NONE ) {
        return false;
    }
    /*
        NT_NUMUSRFNNAME     numeric user function name
        NT_STRUSRFNNAME     string  user function name
            data:
                - 1 byte of type indicator
                - n bytes of name
    */
    //  <nodetype.8> <numbranches.8> <datalen.16> <firstbranch.16> <lastbranch.16> <data...>
    // get name
    if ( fnname->type != NT_NUMUSRFNNAME && fnname->type != NT_STRUSRFNNAME ) {
UNEXP:  comp_error( comp, "Internal error (unexpected name node)" );
        return false;
    }
    uint16_t datalen = fnname->datalen;
    if ( datalen < 2U ) {
        goto UNEXP;
    }
    const uint8_t* datap = fnname->data;
    uint8_t functype = ( datap[0] & VARTYPEM_BASE ) | VARTYPEF_FUNC;

    // get arguments
    node_t* fnarglist = NODE_NONE;
    node_t* paramoffs[MAXPARAM];
    if ( comp->currtok == TOK_LPAREN ) {
        if ( !comp_fetchtok( comp ) ) {
            comp_error( comp, "Function argument list expected" );
            return false;
        }
        comp_eat_usrfnarglist( comp, &fnarglist );
        if ( comp->currtok != TOK_RPAREN || !comp_fetchtok( comp ) ) {
            comp_error( comp, "Closing parenthesis ')' expected" );
            return false;
        }
    }
    usrparam_t params[MAXPARAM];
    size_t numparams = 0U;
    if ( !comp_gather_branches( comp, fnarglist, NT_USERFNARGLIST, paramoffs, MAXPARAM, &numparams ) ) {
        return false;
    }
    if ( numparams ) {
        if ( !comp_get_func_params( comp, paramoffs, numparams, params ) ) {
            return false;
        }
    }
    node_t* fnbody = NODE_NONE;
    node_t* outerfn = comp->curfn;
    comp->curfn = fnname;       // the body may assign the result of a multi-line function
    bool bodyok = comp_eat_usrfnbody( comp, &fnbody ) && fnbody != NODE_NONE;
    comp->curfn = outerfn;
    if ( !bodyok ) {
        comp_error( comp, "User function body expected" );
        return false;
    }

    // create variable
    uint16_t varoffs = VAROFFS_NONE;
    // (the name is made only now, since parsing the body uses the name buffer)
    if ( !vmem_create_var( &comp->rt->varmem, functype, comp_node_name( comp, fnname, 1U ), UINT8_C(0), 0, (uint8_t) numparams, params, &varoffs ) ||
        varoffs == VAROFFS_NONE ) {
        out_of_memory( comp );
        return false;
    }

    // create node
    /*
        NT_USRFNDECL
            data:
                - 1 byte of function type
                - 2 bytes of variable offset
            branches:
                - 1 branch of expression or statement list
    */

    uint8_t data[3];
    data[0] = functype;
    data[1] = (uint8_t)( varoffs >> UINT8_C(8) );
    data[2] = (uint8_t)  varoffs;

    if ( !comp_create_node( comp, pnode, NT_USRFNDECL, UINT8_C(1), UINT16_C(3), data, fnbody ) ||
        *pnode == NODE_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

static bool comp_eat_usrfncall( compiler_t* comp, node_t** pnode, uint8_t nodetype,
    uint8_t nametype, bool (*eater_func)( compiler_t*, node_t** ) ) {
    // num-usr-fn-call := num-usr-fn-name [ TOK_LPAREN [ expr-list ] TOK_RPAREN ] .
    // str-usr-fn-call := str-usr-fn-name [ TOK_LPAREN [ expr-list ] TOK_RPAREN ] .
    node_t* fnname = NODE_NONE;
    if ( !eater_func( comp, &fnname ) || fnname == NODE_NONE ) {
        return false;
    }
    node_t* exprlist = NODE_NONE;
    if ( comp->currtok == TOK_LPAREN && comp_fetchtok( comp ) ) {
        comp_eat_exprlist( comp, &exprlist );
        if ( comp->currtok != TOK_RPAREN || !comp_fetchtok( comp ) ) {
            comp_error( comp, "Closing parenthesis ')' expected" );
        }
    }

    //  <nodetype.8> <numbranches.8> <datalen.16> <firstbranch.16> <lastbranch.16> <data...>
    uint8_t nametype2 = fnname->type;
    if ( nametype2 != nametype ) {
UNEXP:  comp_error( comp, "Internal error (unexpected name type)" );
        return false;
    }
    uint16_t datalen  = fnname->datalen;
    const uint8_t* datap = fnname->data;
    if ( datalen < 2U ) {
        goto UNEXP;
    }
    // data is type + name
    uint8_t functype = ( datap[0] & VARTYPEM_BASE ) | VARTYPEF_FUNC;
    const char* name = comp_node_name( comp, fnname, 1U );

    // lookup variable
    uint16_t varoffs = VAROFFS_NONE;
    if ( !vmem_lookup_var( &comp->rt->varmem, functype, name, &varoffs ) || varoffs == VAROFFS_NONE ) {
UNDEF:  comp_error( comp, "Undefined function" );
        return false;
    }

    uint8_t ftype = 0, numargs = 0; uint16_t codeoffs = CODEOFFS_NONE;
    if ( !vmem_get_func_info( &comp->rt->varmem, varoffs, &ftype, &numargs, &codeoffs ) ) {
        goto UNDEF;
    }

    // check argument count
    if ( numargs == UINT8_C(0) && exprlist == NODE_NONE ) {
        // OK, no arguments
    } else if ( numargs == UINT8_C(0) || exprlist == NODE_NONE ) {
MISM:   comp_error( comp, "Argument count mismatch" );
        return false;
    } else {
        //  <nodetype.8> <numbranches.8> <datalen.16> <firstbranch.16> <lastbranch.16> <data...>
        if ( exprlist->type == NT_EXPRLIST ) {
            // several branches
            size_t numbranches = exprlist->numbranches;
            if ( numbranches != numargs ) {
                goto MISM;
            }
        } else {
            // 1 branch
            if ( numargs != 1U ) {
                goto MISM;
            }
        }
    }

    // not checking argument types at this point (TODO: Fix?)

    // we're all good: output node

    /*
        NT_NUMUSRFNCALL     numeric user function call
        NT_STRUSRFNCALL     string user function call
            data:
                - 1 byte of function type
                - 2 bytes of variable offset
            branches:
                - argument expression list (can be empty)
    */
    uint8_t data[3];
    data[0] = functype;
    data[1] = (uint8_t)( varoffs >> UINT8_C(8) );
    data[2] = (uint8_t)  varoffs;

    if ( !comp_create_node( comp, pnode, nodetype, UINT8_C(1), UINT16_C(3), data, exprlist ) ||
        *pnode == NODE_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_numusrfncall( compiler_t* comp, node_t** pnode ) {
    // num-usr-fn-call := num-usr-fn-name TOK_LPAREN expr-list TOK_RPAREN .
    return comp_eat_usrfncall( comp, pnode, NT_NUMUSRFNCALL, NT_NUMUSRFNNAME, comp_eat_numusrfnname );
}

bool comp_eat_strusrfncall( compiler_t* comp, node_t** pnode ) {
    // str-usr-fn-call := str-usr-fn-name TOK_LPAREN expr-list TOK_RPAREN .
    return comp_eat_usrfncall( comp, pnode, NT_STRUSRFNCALL, NT_STRUSRFNNAME, comp_eat_strusrfnname );
}

bool comp_eat_sysnumfn1argcall( compiler_t* comp, node_t** pnode ) {
    /*
        sys-num-fn-1-arg-call := ( TOK_ASC | TOK_VAL | TOK_LEN ) TOK_LPAREN str-expr TOK_RPAREN |
                                 TOK_FRE TOK_LPAREN num-expr TOK_RPAREN .
        NT_SYSNUMFUNCARGCALL    numeric system function call with arguments
            data:
                - 1 byte of function token (like TOK_VAL)
            branches:
                - 1 or more branches of expression list (depending on function)
    */
    uint8_t tok = comp->currtok;
    if ( tok == TOK_ASC || tok == TOK_VAL || tok == TOK_LEN ) {
        if ( !comp_fetchtok( comp ) ) {
            return false;
        }
        if ( comp->currtok != TOK_LPAREN || !comp_fetchtok( comp ) ) {
OPEN:       comp_error( comp, "Opening parenthesis '(' expected" );
            return false;
        }
        node_t* arg1 = NODE_NONE;
        if ( !comp_eat_strexpr( comp, &arg1 ) || arg1 == NODE_NONE ) {
            comp_error( comp, "String expression expected" );
            return false;
        }
        if ( comp->currtok != TOK_RPAREN || !comp_fetchtok( comp ) ) {
CLOSE:      comp_error( comp, "Closing parenthesis ')' expected" );
            return false;
        }
        if ( !comp_create_node( comp, pnode, NT_SYSNUMFUNCARGCALL, UINT8_C(1), UINT16_C(1), &tok, arg1 ) ||
            *pnode == NODE_NONE ) {
OOM:        out_of_memory( comp );
            return false;
        }
        return true;
    } else if ( tok == TOK_FRE ) {
        if ( !comp_fetchtok( comp ) ) {
            return false;
        }
        if ( comp->currtok != TOK_LPAREN || !comp_fetchtok( comp ) ) {
            goto OPEN;
        }
        node_t* arg1 = NODE_NONE;
        if ( !comp_eat_numexpr( comp, &arg1 ) || arg1 == NODE_NONE ) {
            comp_error( comp, "Numeric expression expected" );
            return false;
        }
        if ( comp->currtok != TOK_RPAREN || !comp_fetchtok( comp ) ) {
            goto CLOSE;
        }
        if ( !comp_create_node( comp, pnode, NT_SYSNUMFUNCARGCALL, UINT8_C(1), UINT16_C(1), &tok, arg1 ) ||
            *pnode == NODE_NONE ) {
            goto OOM;
        }
        return true;
    } else {
        return false;
    }
}

bool comp_eat_sysstrfn2argcall( compiler_t* comp, node_t** pnode ) {
    /*
        sys-str-fn-2-arg-call := ( TOK_BIN | TOK_QUA | TOK_OCT | TOK_DEC | TOK_HEX ) TOK_STRING TOK_LPAREN num-expr
                                    [ TOK_COMMA num-expr ] TOK_RPAREN |
                                 TOK_STR TOK_STRING TOK_LPAREN num-expr TOK_RPAREN |
                                 ( TOK_LEFT | TOK_RIGHT ) TOK_STRING TOK_LPAREN str-expr TOK_COMMA num-expr TOK_RPAREN |
                                 TOK_MID TOK_STRING TOK_LPAREN str-expr TOK_COMMA num-expr [ TOK_COMMA num-expr ] TOK_RPAREN .

        NT_SYSSTRFUNCARGCALL    string system function call with arguments
            data:
                - 1 byte of function token (like TOK_VAL)
            branches:
                - 1 or more branches of expression list (depending on function)
    */
    uint8_t tok = comp->currtok;
    if ( tok == TOK_BIN || tok == TOK_QUA || tok == TOK_OCT || tok == TOK_DEC || tok == TOK_HEX ) {
        // ( TOK_BIN | TOK_QUA | TOK_OCT | TOK_DEC | TOK_HEX ) TOK_STRING TOK_LPAREN num-expr [ TOK_COMMA num-expr ] TOK_RPAREN
        if ( !comp_fetchtok( comp ) ) {
            return false;
        }
        if ( comp->currtok != TOK_STRING || !comp_fetchtok( comp ) ) {
STRSIG:     comp_error( comp, "String sigil '$' expected" );
            return false;
        }
        if ( comp->currtok != TOK_LPAREN || !comp_fetchtok( comp ) ) {
OPEN:       comp_error( comp, "Opening parenthesis '(' expected" );
            return false;
        }
        node_t* arg1 = NODE_NONE, *arg2 = NODE_NONE;
        if ( !comp_eat_numexpr( comp, &arg1 ) || arg1 == NODE_NONE ) {
NUMEX:      comp_error( comp, "Numeric expression expected" );
            return false;
        }
        if ( comp->currtok == TOK_COMMA && comp_fetchtok( comp ) ) {
            if ( !comp_eat_numexpr( comp, &arg2 ) || arg2 == NODE_NONE ) {
                goto NUMEX;
            }
        } else {
            union {
                uint64_t    ui64;
                double      dbl;
            } u;
            u.dbl = -1.0;
            uint8_t data[8];
            data[0] = (uint8_t)( u.ui64 >> UINT8_C(56) );
            data[1] = (uint8_t)( u.ui64 >> UINT8_C(48) );
            data[2] = (uint8_t)( u.ui64 >> UINT8_C(40) );
            data[3] = (uint8_t)( u.ui64 >> UINT8_C(32) );
            data[4] = (uint8_t)( u.ui64 >> UINT8_C(24) );
            data[5] = (uint8_t)( u.ui64 >> UINT8_C(16) );
            data[6] = (uint8_t)( u.ui64 >> UINT8_C( 8) );
            data[7] = (uint8_t)( u.ui64                );
            if ( !comp_create_node( comp, &arg2, NT_NUMLIT, UINT8_C(0), UINT16_C(8), data ) || arg2 == NODE_NONE ) {
                goto OOM;
            }
        }
        if ( comp->currtok != TOK_RPAREN || !comp_fetchtok( comp ) ) {
CLOSE:      comp_error( comp, "Closing parenthesis ')' expected" );
            return false;
        }
        if ( !comp_create_node( comp, pnode, NT_SYSSTRFUNCARGCALL, UINT8_C(2), UINT16_C(1), &tok, arg1, arg2 ) ||
            *pnode == NODE_NONE ) {
OOM:        out_of_memory( comp );
            return false;
        }
        return true;
    } else if ( tok == TOK_STR ) {
        // TOK_STR TOK_STRING TOK_LPAREN num-expr TOK_RPAREN
        if ( !comp_fetchtok( comp ) ) {
            return false;
        }
        if ( comp->currtok != TOK_STRING || !comp_fetchtok( comp ) ) {
            goto STRSIG;
        }
        if ( comp->currtok != TOK_LPAREN || !comp_fetchtok( comp ) ) {
            goto OPEN;
        }
        node_t* arg1 = NODE_NONE;
        if ( !comp_eat_numexpr( comp, &arg1 ) || arg1 == NODE_NONE ) {
            goto NUMEX;
        }
        if ( comp->currtok != TOK_RPAREN || !comp_fetchtok( comp ) ) {
            goto CLOSE;
        }
        if ( !comp_create_node( comp, pnode, NT_SYSSTRFUNCARGCALL, UINT8_C(1), UINT16_C(1), &tok, arg1 ) ||
            *pnode == NODE_NONE ) {
            goto OOM;
        }
        return true;
    } else if ( tok == TOK_LEFT || tok == TOK_RIGHT ) {
        // ( TOK_LEFT | TOK_RIGHT ) TOK_STRING TOK_LPAREN str-expr TOK_COMMA num-expr TOK_RPAREN |
        if ( !comp_fetchtok( comp ) ) {
            return false;
        }
        if ( comp->currtok != TOK_STRING || !comp_fetchtok( comp ) ) {
            goto STRSIG;
        }
        if ( comp->currtok != TOK_LPAREN || !comp_fetchtok( comp ) ) {
            goto OPEN;
        }
        node_t* arg1 = NODE_NONE, *arg2 = NODE_NONE;
        if ( !comp_eat_strexpr( comp, &arg1 ) || arg1 == NODE_NONE ) {
STREX:      comp_error( comp, "String expression expected" );
            return false;
        }
        if ( comp->currtok != TOK_COMMA || !comp_fetchtok( comp ) ) {
COMMA:      comp_error( comp, "Comma ',' expected" );
            return false;
        }
        if ( !comp_eat_numexpr( comp, &arg2 ) || arg2 == NODE_NONE ) {
            goto NUMEX;
        }
        if ( comp->currtok != TOK_RPAREN || !comp_fetchtok( comp ) ) {
            goto CLOSE;
        }
        if ( !comp_create_node( comp, pnode, NT_SYSSTRFUNCARGCALL, UINT8_C(2), UINT16_C(1), &tok, arg1, arg2 ) ||
            *pnode == NODE_NONE ) {
            goto OOM;
        }
        return true;
    } else if ( tok == TOK_MID ) {
        // TOK_MID TOK_STRING TOK_LPAREN str-expr TOK_COMMA num-expr [ TOK_COMMA num-expr ] TOK_RPAREN .
        if ( !comp_fetchtok( comp ) ) {
            return false;
        }
        if ( comp->currtok != TOK_STRING || !comp_fetchtok( comp ) ) {
            goto STRSIG;
        }
        if ( comp->currtok != TOK_LPAREN || !comp_fetchtok( comp ) ) {
            goto OPEN;
        }
        node_t* arg1 = NODE_NONE, *arg2 = NODE_NONE, *arg3 = NODE_NONE;
        if ( !comp_eat_strexpr( comp, &arg1 ) || arg1 == NODE_NONE ) {
            goto STREX;
        }
        if ( comp->currtok != TOK_COMMA || !comp_fetchtok( comp ) ) {
            goto COMMA;
        }
        if ( !comp_eat_numexpr( comp, &arg2 ) || arg2 == NODE_NONE ) {
            goto NUMEX;
        }
        if ( comp->currtok == TOK_COMMA && comp_fetchtok( comp ) ) {
            if ( !comp_eat_numexpr( comp, &arg3 ) || arg3 == NODE_NONE ) {
                goto NUMEX;
            }
        } else {
            union {
                uint64_t    ui64;
                double      dbl;
            } u;
            u.dbl = -1.0;
            uint8_t data[8];
            data[0] = (uint8_t)( u.ui64 >> UINT8_C(56) );
            data[1] = (uint8_t)( u.ui64 >> UINT8_C(48) );
            data[2] = (uint8_t)( u.ui64 >> UINT8_C(40) );
            data[3] = (uint8_t)( u.ui64 >> UINT8_C(32) );
            data[4] = (uint8_t)( u.ui64 >> UINT8_C(24) );
            data[5] = (uint8_t)( u.ui64 >> UINT8_C(16) );
            data[6] = (uint8_t)( u.ui64 >> UINT8_C( 8) );
            data[7] = (uint8_t)( u.ui64                );
            if ( !comp_create_node( comp, &arg3, NT_NUMLIT, UINT8_C(0), UINT16_C(8), data ) || arg3 == NODE_NONE ) {
                goto OOM;
            }
        }
        if ( comp->currtok != TOK_RPAREN || !comp_fetchtok( comp ) ) {
            goto CLOSE;
        }
        if ( !comp_create_node( comp, pnode, NT_SYSSTRFUNCARGCALL, UINT8_C(3), UINT16_C(1), &tok, arg1, arg2,
            arg3 ) || *pnode == NODE_NONE ) {
            goto OOM;
        }
        return true;
    }
    return false;
}

bool comp_eat_sysnumfuncargcall( compiler_t* comp, node_t** pnode ) {
    // sys-num-fn-arg-call := sys-num-fn-1-arg-call .
    return comp_eat_sysnumfn1argcall( comp, pnode );
}

bool comp_eat_sysstrfuncargcall( compiler_t* comp, node_t** pnode ) {
    // sys-str-fn-arg-call := sys-str-fn-2-arg-call .
    return comp_eat_sysstrfn2argcall( comp, pnode );
}

bool comp_eat_sysnoargnumcall( compiler_t* comp, node_t** pnode ) {
    // sys-noarg-num-name := TOK_TI .
    // sys-noarg-num := sys-noarg-num-name .
    // sys-noarg-num-call := sys-no-arg-num .
    uint8_t tok = comp->currtok;
    if ( tok != TOK_TI || !comp_fetchtok( comp ) ) {
        return false;
    }
    if ( !comp_create_node( comp, pnode, NT_SYSNOARGNUMCALL, UINT8_C(0), UINT16_C(1), &tok ) || *pnode == NODE_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_sysnoargstrcall( compiler_t* comp, node_t** pnode ) {
    // sys-noarg-str-name := TOK_INKEY .
    // sys-noarg-str := sys-noarg-str-name TOK_STRING .
    // sys-noarg-str-call := sys-no-arg-str .
    uint8_t tok = comp->currtok;
    if ( tok != TOK_INKEY || !comp_fetchtok( comp ) ) {
        return false;
    }
    if ( comp->currtok != TOK_STRING || !comp_fetchtok( comp ) ) {
        comp_error( comp, "String sigil '$' expected" );
        return false;
    }
    if ( !comp_create_node( comp, pnode, NT_SYSNOARGSTRCALL, UINT8_C(0), UINT16_C(1), &tok ) || *pnode == NODE_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_numfunccall( compiler_t* comp, node_t** pnode ) {
    // num-func-call := num-usr-fn-call | sys-num-fn-arg-call | sys-noarg-num-call .
    if ( comp_eat_numusrfncall( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    if ( comp_eat_sysnumfuncargcall( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    if ( comp_eat_sysnoargnumcall( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    return false;
}

bool comp_eat_strfunccall( compiler_t* comp, node_t** pnode ) {
    // str-func-call := str-usr-fn-call | sys-str-fn-arg-call | sys-noarg-str-call .
    if ( comp_eat_strusrfncall( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    if ( comp_eat_sysstrfuncargcall( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    if ( comp_eat_sysnoargstrcall( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    return false;
}

bool comp_eat_strbaseexpr( compiler_t* comp, node_t** pnode ) {
    // str-base-expr := str-var-ref | str-lits | str-func-call .
    if ( comp_eat_strvarref( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    if ( comp_eat_strlits( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    if ( comp_eat_strfunccall( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    return false;
}

bool comp_eat_straddexpr( compiler_t* comp, node_t** pnode ) {
    // str-add-expr := str-base-expr { TOK_PLUS str-base-expr } .
    return comp_eat_list( comp, pnode, NT_STRADDEXPR, comp_eat_strbaseexpr, TOK_PLUS, "String expression expected" );
}

bool comp_eat_strexpr( compiler_t* comp, node_t** pnode ) {
    // str-expr := str-add-expr .
    /*
        NT_STREXPR
            branches:
                - 1 branch of string expression
            immediate processing:
                - generated as a hint to the code generator
    */
    node_t* straddex = NODE_NONE;
    if ( !comp_eat_straddexpr( comp, &straddex ) || straddex == NODE_NONE ) {
        return false;
    }
    if ( !comp_create_node( comp, pnode, NT_STREXPR, UINT8_C(1), UINT16_C(0), 0, straddex ) ||
        *pnode == NODE_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_numsubexpr( compiler_t* comp, node_t** pnode ) {
    // num-sub-expr := TOK_LPAREN num-expr TOK_RPAREN .
    if ( comp->currtok != TOK_LPAREN || !comp_fetchtok( comp ) ) {
        return false;
    }
    node_t* subexpr = NODE_NONE;
    if ( !comp_eat_numexpr( comp, &subexpr ) || subexpr == NODE_NONE ) {
        comp_error( comp, "Numeric expression expected" );
        return false;
    }
    if ( comp->currtok != TOK_RPAREN || !comp_fetchtok( comp ) ) {
        comp_error( comp, "Closing parenthesis ')' expected" );
        return false;
    }
    *pnode = subexpr;
    return true;
}

bool comp_eat_numbaseexpr( compiler_t* comp, node_t** pnode ) {
    // num-base-expr := num-var-ref | num-lit | num-func-call | num-sub-expr .
    if ( comp_eat_numvarref( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    if ( comp_eat_numlit( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    if ( comp_eat_numfunccall( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    if ( comp_eat_numsubexpr( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    return false;
}

bool comp_eat_list2( compiler_t* comp, node_t** pnode, uint8_t nodetype, comp_eatfn_t element_eater, const uint8_t* septoks,
    const char* errortext, bool oneoperator, bool forceexpr ) {
    /*
        The meaning of the "oneoperator" flag: Sometimes we wish that no more than one NT_OPERATOR node is present.
        The meaning of the "forceexpr" flag: Sometimes we wish to enforce that an expression with two operands is present.
        If only the first expression is present, this results in a rollback of state before the first expression was read.
    */
    // list := element { SEPTOK element } .  -- if SEPTOK is TOK_EOL, there's no separator token
    if ( forceexpr ) {
        // push current context
        comp_push_context( comp );
    }
    node_t* expr1 = NODE_NONE;
    if ( !element_eater( comp, &expr1 ) || expr1 == NODE_NONE ) {
        if ( forceexpr ) {
            // make sure first expression has not been read
            comp_pop_context( comp );
        }
        return false;
    }
    node_t* node = NODE_NONE;
    for (;;) {
        bool mandatory = false; node_t* expr2 = NODE_NONE;
        const uint8_t* sep = septoks; uint8_t septok = TOK_EOL;
        while ( *sep ) {
            if ( comp->currtok == *sep ) {
                septok = *sep;
                break;
            }
            ++sep;
        }
        if ( septok != TOK_EOL ) {
            // read next token
            if ( !comp_fetchtok( comp ) ) {
                // stop processing
                break;
            }
            mandatory = true;   // the following expression is mandatory
        } else {
            // operator not found: stop
            break;
        }
        if ( !element_eater( comp, &expr2 ) || expr2 == NODE_NONE ) {
            if ( mandatory ) {  // mandatory expression missing: stop
                if ( forceexpr ) {
                    // commit to changes before generating error
                    comp_commit_context( comp );
                }
                comp_error( comp, errortext );
            }
            // stop processing
            break;
        }
        // expr2 does contain a new branch now, but we need a node to tell what operation is to be done to it
        // create a new node that contains that token as data
        node_t* operand = NODE_NONE;
        if ( !comp_create_node( comp, &operand, NT_OPERATOR, UINT8_C(1), UINT16_C(1), &septok, expr2 ) ||
            operand == NODE_NONE ) {
OOM:        if ( forceexpr ) {
                // commit to changes before generating error
                comp_commit_context( comp );
            }
            out_of_memory( comp );
            return false;
        }
        // we have now a new branch; first see if we already have a node or need to create one
        if ( node == NODE_NONE ) {
            if ( !comp_create_node( comp, &node, nodetype, UINT8_C(2), UINT16_C(0), 0, expr1, operand ) ||
                 node == NODE_NONE ) {
                // failed to create node: cancel operation
                goto OOM;
            }
        } else {
            // the node already exists: add a new branch
            if ( !comp_add_branch( comp, node, operand ) ) {
                // something went wrong: cancel
                goto OOM;
            }
        }
        // successful, continue, unless only one additional operand is allowed
        if ( oneoperator ) {
            break;
        }
    }
    if ( node == NODE_NONE && forceexpr ) {
        // rewind so that the first expression has not been read
        comp_pop_context( comp );
        return false;
    } else if ( forceexpr ) {
        // all fine, commit to it
        comp_commit_context( comp );
    }
    // either return node with what we already have, or just the first branch
    *pnode = node != NODE_NONE ? node : expr1;
    return true;
}

bool comp_eat_numunaryex( compiler_t* comp, node_t** pnode ) {
    // num-unary-op := TOK_MINUS | TOK_PLUS | TOK_NOT .
    // num-unary-ex := [ num-unary-op ] num-base-expr .
    uint8_t tok = comp->currtok;
    switch ( tok ) {
        case TOK_MINUS: case TOK_PLUS: case TOK_NOT:
            break;
        default:
            tok = TOK_EOL;
            break;
    }
    if ( tok != TOK_EOL && !comp_fetchtok( comp ) ) {
        return false;
    }
    node_t* baseexpr = NODE_NONE;
    if ( !comp_eat_numbaseexpr( comp, &baseexpr ) || baseexpr == NODE_NONE ) {
        if ( tok != TOK_EOL ) { // mandatory expression when operator present
            comp_error( comp, "Numeric expression expected" );
        }
        return false;
    }
    /*
        NT_NUMUNARYEX   numeric unary expression
            data:
                - 1 byte of operator token
            branches:
                - 1 branch of operand
            immediate processing:
                - generated only if a unary expression was used
    */
    if ( tok == TOK_EOL ) {
        // no operator present: return expression directly
        *pnode = baseexpr;
        return true;
    }
    // operator present: create new node
    if ( !comp_create_node( comp, pnode, NT_NUMUNARYEX, UINT8_C(1), UINT16_C(1), &tok, baseexpr ) ||
        *pnode == NODE_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_nummultex( compiler_t* comp, node_t** pnode ) {
    // num-mult-op := TOK_MULT | TOK_DIV | TOK_POW .
    // num-mult-ex := num-unary-ex { num-mult-op num-unary-ex } .
    static const uint8_t septoks[] = { TOK_MULT, TOK_DIV, TOK_POW, 0 };
    /*
        NT_NUMMULTEX   numeric multiplication expression
            branches:
                - 1 branch of first operand
                - at least 1 branch of NT_OPERATOR
            immediate processing:
                - generated only if a multiplication operator was used
    */
    return comp_eat_list2( comp, pnode, NT_NUMMULTEX, comp_eat_numunaryex, septoks, "Multiplication expression expected",
        false, false );
}

bool comp_eat_numaddex( compiler_t* comp, node_t** pnode ) {
    // num-add-op := TOK_PLUS | TOK_MINUS .
    // num-add-ex := num-mult-ex { num-add-op num-mult-ex } .
    static const uint8_t septoks[] = { TOK_PLUS, TOK_MINUS, 0 };
    /*
        NT_NUMADDEX   numeric addition expression
            branches:
                - 1 branch of first operand
                - at least 1 branch of NT_OPERATOR
            immediate processing:
                - generated only if an addition operator was used
    */
    return comp_eat_list2( comp, pnode, NT_NUMADDEX, comp_eat_nummultex, septoks, "Add expression expected",
        false, false );
}

bool comp_eat_numshiftex( compiler_t* comp, node_t** pnode ) {
    // num-shift-op := TOK_LSHIFT | TOK_RSHIFT .
    // num-shift-ex := num-add-ex [ num-shift-op num-add-ex ] .
    static const uint8_t septoks[] = { TOK_LSHIFT, TOK_RSHIFT, 0 };
    /*
        NT_NUMSHIFTEX   numeric shift expression
            branches:
                - 1 branch of first operand
                - at least 1 branch of NT_OPERATOR
            immediate processing:
                - generated only if a shift operator was used
    */
    return comp_eat_list2( comp, pnode, NT_NUMSHIFTEX, comp_eat_numaddex, septoks, "Shift expression expected",
        true, false );
}

bool comp_eat_numcmpex( compiler_t* comp, node_t** pnode ) {
    // num-cmp-op := TOK_EQ | TOK_NE | TOK_LE | TOK_GE | TOK_LT | TOK_GT .
    // num-cmp-ex := num-shift-ex [ num-cmp-op num-shift-ex ] |
    //               str-expr num-cmp-op str-expr .
    static const uint8_t septoks[] = { TOK_EQ, TOK_NE, TOK_LE, TOK_GE, TOK_LT, TOK_GT, 0 };
    // numeric version
    /*
        NT_NUMCMPEX   numeric comparison expression
        NT_STRCMPEX   string  comparison expression
            branches:
                - 1 branch of first operand
                - at least 1 branch of NT_OPERATOR
            immediate processing:
                - generated only if a comparison operator was used
    */
    if ( comp_eat_list2( comp, pnode, NT_NUMCMPEX, comp_eat_numshiftex, septoks, "Comparison expression expected",
        true, false ) && *pnode != NODE_NONE ) {
        return true;
    }
    if ( comp_eat_list2( comp, pnode, NT_STRCMPEX, comp_eat_strexpr, septoks, "Comparison expression expected",
        true, true ) && *pnode != NODE_NONE ) {
        return true;
    }
    return false;
}

bool comp_eat_numandex( compiler_t* comp, node_t** pnode ) {
    // num-and-op := TOK_AND | TOK_NAND .
    // num-and-ex := num-cmp-ex { num-and-op num-cmp-ex } .
    static const uint8_t septoks[] = { TOK_AND, TOK_NAND, 0 };
    /*
        NT_NUMANDEX   numeric AND expression
            branches:
                - 1 branch of first operand
                - at least 1 branch of NT_OPERATOR
            immediate processing:
                - generated only if a multiplication operator was used
    */
    return comp_eat_list2( comp, pnode, NT_NUMANDEX, comp_eat_numcmpex, septoks, "AND expression expected",
        false, false );
}

bool comp_eat_numorex( compiler_t* comp, node_t** pnode ) {
    // num-or-op := TOK_OR | TOK_XOR | TOK_NOR | TOK_XNOR .
    // num-or-ex := num-and-ex { num-or-op num-and-ex } .
    static const uint8_t septoks[] = { TOK_OR, TOK_XOR, TOK_NOR, TOK_XNOR, 0 };
    /*
        NT_NUMOREX   numeric OR expression
            branches:
                - 1 branch of first operand
                - at least 1 branch of NT_OPERATOR
            immediate processing:
                - generated only if a multiplication operator was used
    */
    return comp_eat_list2( comp, pnode, NT_NUMOREX, comp_eat_numandex, septoks, "OR expression expected",
        false, false );
}

bool comp_eat_numexpr( compiler_t* comp, node_t** pnode ) {
    // num-expr := num-or-ex .
    /*
        NT_NUMEXPR
            branches:
                - 1 branch of numeric expression
            immediate processing:
                - generated as a hint to the code generator
    */
    node_t* numorex = NODE_NONE;
    if ( !comp_eat_numorex( comp, &numorex ) || numorex == NODE_NONE ) {
        return false;
    }
    if ( !comp_create_node( comp, pnode, NT_NUMEXPR, UINT8_C(1), UINT16_C(0), 0, numorex ) ||
        *pnode == NODE_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_expr( compiler_t* comp, node_t** pnode ) {
    // expr := num-expr | str-expr .
    // [ NT_EXPR - not generated ]
    if ( comp_eat_numexpr( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    if ( comp_eat_strexpr( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    return false;
}

bool comp_eat_savestmt( compiler_t* comp, node_t** pnode ) {
    // save-stmt := SAVE str-expr [ TOK_COMMA TOK_NUMIDENT ] .
    if ( comp->currtok != TOK_SAVE || !comp_fetchtok( comp ) ) {
        return false;
    }
    node_t* strexpr = NODE_NONE;
    if ( !comp_eat_strexpr( comp, &strexpr ) || strexpr == NODE_NONE ) {
        comp_error( comp, "String expression expected" );
        return false;
    }
    char* saveopt = 0;
    if ( comp->currtok == TOK_COMMA && comp_fetchtok( comp ) ) {
        if ( comp->currtok != TOK_NUMIDENT ) {
            comp_error( comp, "SAVE options expected" );
            return false;
        }
        saveopt = dup_param( comp, comp->param );   // copy before fetching the next token
        if ( !comp_fetchtok( comp ) ) {
            free( saveopt );
            comp_error( comp, "SAVE options expected" );
            return false;
        }
    }
    size_t saveoptlen = saveopt ? strlen( saveopt ) : 0;
    if ( saveoptlen > UINT16_MAX ) {
        free( saveopt );
        comp_error( comp, "SAVE options too long" );
        return false;
    }
    /*
        NT_SAVESTMT     SAVE statement
            data:
                - n bytes of save mode (optional)
            branches:
                - 1 branch of string expression
            immediate processing:
                - the SAVE statement is special b/c it uses an identifier as optional second parameter
                  denoting save mode (A for ASCII, B (default) for binary)
                - the file name need not be a string literal
    */
    bool ok = comp_create_node( comp, pnode, NT_SAVESTMT, UINT8_C(1), (uint16_t) saveoptlen,
        saveopt ? saveopt : "", strexpr ) && *pnode != NODE_NONE;
    free( saveopt );
    if ( !ok ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_chanspec( compiler_t* comp, node_t** pnode ) {
    // chan-spec := TOK_LATTICE num-expr .
    if ( comp->currtok != TOK_LATTICE || !comp_fetchtok( comp ) ) {
        return false;
    }
    node_t* numexpr = NODE_NONE;
    if ( !comp_eat_numexpr( comp, &numexpr ) || numexpr == NODE_NONE ) {
        comp_error( comp, "Numeric expression expected" );
        return false;
    }
    /*
        NT_CHANSPEC     channel specifier
            branches:
                - 1 branch of numeric expression
    */
    if ( !comp_create_node( comp, pnode, NT_CHANSPEC, UINT8_C(1), UINT16_C(0), 0, numexpr ) ||
        *pnode == NODE_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_printsep( compiler_t* comp, node_t** pnode ) {
    // print-sep := TOK_COMMA | TOK_SEMIC .
    uint8_t tok = comp->currtok;
    if ( tok != TOK_COMMA && tok != TOK_SEMIC ) {
        return false;
    }
    if ( !comp_fetchtok( comp ) ) {
        return false;
    }
    /*
        NT_PRINTSEP     print separator
            data:
                - 1 byte of separator token
    */
    if ( !comp_create_node( comp, pnode, NT_PRINTSEP, UINT8_C(0), UINT16_C(1), &tok ) || *pnode == NODE_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_printarg( compiler_t* comp, node_t** pnode ) {
    // print-arg := expr [ print-sep ] .
    node_t* expr = NODE_NONE;
    if ( !comp_eat_expr( comp, &expr ) || expr == NODE_NONE ) {
        return false;
    }
    node_t* sep = NODE_NONE;
    comp_eat_printsep( comp, &sep );
    /*
        NT_PRINTARG     print argument
            branches:
                - 1 branch of expression
                - 1 optional branch of separator
    */
    if ( !comp_create_node( comp, pnode, NT_PRINTARG, UINT8_C(2), UINT16_C(0), 0, expr, sep ) ||
        *pnode == NODE_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_printarglist( compiler_t* comp, node_t** pnode ) {
    // print-arg-list := print-arg { print-arg } .
    /*
        NT_PRINTARGLIST     print argument
            branches:
                - 2 or more branches of print arguments
            immediate processing:
                - generated only if there's more than one print argument
    */
    return comp_eat_list( comp, pnode, NT_PRINTARGLIST, comp_eat_printarg, TOK_EOL, "PRINT argument expected" );
}

bool comp_eat_printstmt( compiler_t* comp, node_t** pnode ) {
    // print-stmt := TOK_PRINT [ chan-spec TOK_COMMA ] [ print-arg-list ] .
    if ( comp->currtok != TOK_PRINT || !comp_fetchtok( comp ) ) {
        return false;
    }
    node_t* chan = NODE_NONE;
    if ( comp_eat_chanspec( comp, &chan ) && chan != NODE_NONE ) {
        if ( comp->currtok != TOK_COMMA || !comp_fetchtok( comp ) ) {
            comp_error( comp, "Comma ',' expected" );
            return false;
        }
    }
    node_t* args = NODE_NONE;
    comp_eat_printarglist( comp, &args );
    /*
        NT_PRINTSTMT    print statement
            branches:
                - 1 optional branch of channel info
                - 1 optional branch of print argument list
    */
    if ( !comp_create_node( comp, pnode, NT_PRINTSTMT, UINT8_C(2), UINT16_C(0), 0, chan, args ) ||
        *pnode == NODE_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

// stores a number in the 8 data bytes of a NT_NUMLIT node (big endian)
static void comp_set_numlit( uint8_t* data, double val ) {
    union {
        uint64_t    ui64;
        double      dbl;
    } u;
    u.dbl = val;
    for ( unsigned i=0; i < 8U; ++i ) {
        data[i] = (uint8_t)( u.ui64 >> ( 8U * ( 7U - i ) ) );
    }
}

// data-item := [ TOK_PLUS | TOK_MINUS ] num-lit | TOK_STRLIT .
static bool comp_eat_dataitem( compiler_t* comp, node_t** pnode ) {
    bool negate = false, signd = false;
    if ( comp->currtok == TOK_PLUS || comp->currtok == TOK_MINUS ) {
        negate = comp->currtok == TOK_MINUS;
        signd = true;
        if ( !comp_fetchtok( comp ) ) {
            return false;
        }
    }
    if ( comp_eat_numlit( comp, pnode ) && *pnode != NODE_NONE ) {
        if ( negate ) {
            comp_set_numlit( (*pnode)->data, -comp_extract_float( (*pnode)->data, 0 ) );
        }
        return true;
    }
    if ( signd ) {
        comp_error( comp, "Number expected after sign" );
        return false;
    }
    if ( comp->currtok != TOK_STRLIT ) {
        return false;
    }
    return comp_eat_strlit( comp, pnode ) && *pnode != NODE_NONE;
}

bool comp_eat_datastmt( compiler_t* comp, node_t** pnode ) {
    // data-stmt := TOK_DATA data-item { TOK_COMMA data-item } .
    if ( comp->currtok != TOK_DATA || !comp_fetchtok( comp ) ) {
        return false;
    }
    if ( !comp_create_node( comp, pnode, NT_DATASTMT, UINT8_C(0), UINT16_C(0), 0 ) || *pnode == NODE_NONE ) {
        out_of_memory( comp );
        return false;
    }
    for (;;) {
        node_t* item = NODE_NONE;
        if ( !comp_eat_dataitem( comp, &item ) || item == NODE_NONE ) {
            comp_error( comp, "Data item expected" );
            return false;
        }
        if ( !comp_add_branch( comp, *pnode, item ) ) {
            out_of_memory( comp );
            return false;
        }
        if ( comp->currtok != TOK_COMMA || !comp_fetchtok( comp ) ) {
            break;
        }
    }
    return true;
}

bool comp_eat_readstmt( compiler_t* comp, node_t** pnode ) {
    // read-stmt := TOK_READ ( num-var-ref | str-var-ref ) { TOK_COMMA ( num-var-ref | str-var-ref ) } .
    if ( comp->currtok != TOK_READ || !comp_fetchtok( comp ) ) {
        return false;
    }
    if ( !comp_create_node( comp, pnode, NT_READSTMT, UINT8_C(0), UINT16_C(0), 0 ) || *pnode == NODE_NONE ) {
        out_of_memory( comp );
        return false;
    }
    for (;;) {
        node_t* ref = NODE_NONE;
        if ( !( comp_eat_numvarref( comp, &ref ) && ref != NODE_NONE ) && !( comp_eat_strvarref( comp, &ref ) && ref != NODE_NONE ) ) {
            comp_error( comp, "Variable expected" );
            return false;
        }
        if ( !comp_add_branch( comp, *pnode, ref ) ) {
            out_of_memory( comp );
            return false;
        }
        if ( comp->currtok != TOK_COMMA || !comp_fetchtok( comp ) ) {
            break;
        }
    }
    return true;
}

bool comp_eat_restorestmt( compiler_t* comp, node_t** pnode ) {
    // restore-stmt := TOK_RESTORE [ goto-target ] .
    if ( comp->currtok != TOK_RESTORE || !comp_fetchtok( comp ) ) {
        return false;
    }
    node_t* target = NODE_NONE;
    bool hastarget = comp_eat_gototarget( comp, &target ) && target != NODE_NONE;
    if ( !( hastarget ? comp_create_node( comp, pnode, NT_RESTORESTMT, UINT8_C(1), UINT16_C(0), 0, target ) :
            comp_create_node( comp, pnode, NT_RESTORESTMT, UINT8_C(0), UINT16_C(0), 0 ) ) || *pnode == NODE_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

// input-var := num-var-ref | str-var-ref .
static bool comp_eat_inputvars( compiler_t* comp, node_t* stmt, const char* what ) {
    for (;;) {
        node_t* ref = NODE_NONE;
        if ( !( comp_eat_numvarref( comp, &ref ) && ref != NODE_NONE ) && !( comp_eat_strvarref( comp, &ref ) && ref != NODE_NONE ) ) {
            comp_error( comp, what );
            return false;
        }
        if ( !comp_add_branch( comp, stmt, ref ) ) {
            out_of_memory( comp );
            return false;
        }
        if ( comp->currtok != TOK_COMMA || !comp_fetchtok( comp ) ) {
            return true;
        }
    }
}

bool comp_eat_inputstmt( compiler_t* comp, node_t** pnode ) {
    // input-stmt := TOK_INPUT [ chan-spec TOK_COMMA ] [ str-lits ( TOK_SEMIC | TOK_COMMA ) ] input-var { TOK_COMMA input-var } .
    /*
        NT_INPUTSTMT        INPUT statement
            data:
                - 1 byte of flags: INPUTF_CHAN (channel branch present), INPUTF_PROMPT (prompt branch present),
                  INPUTF_NOQUEST (the prompt was followed by a comma: no "? " is shown)
            branches:
                - optional branch of channel info (NT_CHANSPEC)
                - optional branch of prompt (NT_STRLITS or NT_STRLIT)
                - 1 or more branches of variable references (NT_NUMVARREF or NT_STRVARREF)
    */
    if ( comp->currtok != TOK_INPUT || !comp_fetchtok( comp ) ) {
        return false;
    }
    uint8_t flags = 0;
    node_t* chan = NODE_NONE;
    if ( comp_eat_chanspec( comp, &chan ) && chan != NODE_NONE ) {
        if ( comp->currtok != TOK_COMMA || !comp_fetchtok( comp ) ) {
            comp_error( comp, "Comma ',' expected" );
            return false;
        }
        flags |= INPUTF_CHAN;
    }
    node_t* prompt = NODE_NONE;
    if ( comp_eat_strlits( comp, &prompt ) && prompt != NODE_NONE ) {
        if ( comp->currtok != TOK_SEMIC && comp->currtok != TOK_COMMA ) {
            comp_error( comp, "Semicolon ';' or comma ',' expected after prompt" );
            return false;
        }
        if ( comp->currtok == TOK_COMMA ) {
            flags |= INPUTF_NOQUEST;
        }
        flags |= INPUTF_PROMPT;
        if ( !comp_fetchtok( comp ) ) {
            return false;
        }
    }
    if ( !comp_create_node( comp, pnode, NT_INPUTSTMT, UINT8_C(0), UINT16_C(1), &flags ) || *pnode == NODE_NONE ) {
        out_of_memory( comp );
        return false;
    }
    if ( ( chan != NODE_NONE && !comp_add_branch( comp, *pnode, chan ) ) ||
         ( prompt != NODE_NONE && !comp_add_branch( comp, *pnode, prompt ) ) ) {
        out_of_memory( comp );
        return false;
    }
    return comp_eat_inputvars( comp, *pnode, "Variable expected" );
}

bool comp_eat_getstmt( compiler_t* comp, node_t** pnode ) {
    // get-stmt := TOK_GET [ chan-spec TOK_COMMA ] input-var { TOK_COMMA input-var } .
    /*
        NT_GETSTMT          GET statement
            data:
                - 1 byte of flags: INPUTF_CHAN (channel branch present)
            branches:
                - optional branch of channel info (NT_CHANSPEC)
                - 1 or more branches of variable references (NT_NUMVARREF or NT_STRVARREF)
    */
    if ( comp->currtok != TOK_GET || !comp_fetchtok( comp ) ) {
        return false;
    }
    uint8_t flags = 0;
    node_t* chan = NODE_NONE;
    if ( comp_eat_chanspec( comp, &chan ) && chan != NODE_NONE ) {
        if ( comp->currtok != TOK_COMMA || !comp_fetchtok( comp ) ) {
            comp_error( comp, "Comma ',' expected" );
            return false;
        }
        flags |= INPUTF_CHAN;
    }
    if ( !comp_create_node( comp, pnode, NT_GETSTMT, UINT8_C(0), UINT16_C(1), &flags ) || *pnode == NODE_NONE ) {
        out_of_memory( comp );
        return false;
    }
    if ( chan != NODE_NONE && !comp_add_branch( comp, *pnode, chan ) ) {
        out_of_memory( comp );
        return false;
    }
    return comp_eat_inputvars( comp, *pnode, "Variable expected" );
}

bool comp_eat_iostmt( compiler_t* comp, node_t** pnode ) {
    // io-stmt := save-stmt | print-stmt | input-stmt | get-stmt | data-stmt | read-stmt | restore-stmt .
    // [ NT_IOSTMT - not generated ]
    if ( comp_eat_savestmt( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    if ( comp_eat_printstmt( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    if ( comp_eat_inputstmt( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    if ( comp_eat_getstmt( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    if ( comp_eat_datastmt( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    if ( comp_eat_readstmt( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    if ( comp_eat_restorestmt( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    return false;
}

bool comp_eat_numassign( compiler_t* comp, node_t** pnode ) {
    // num-assign := num-var-ref TOK_EQ num-expr .
    node_t* varref = NODE_NONE;
    if ( !comp_eat_numvarref( comp, &varref ) || varref == NODE_NONE ) {
        return false;
    }
    if ( comp->currtok != TOK_EQ || !comp_fetchtok( comp ) ) {
        comp_error( comp, "Assignment operator '=' expected" );
        return false;
    }
    node_t* numexpr = NODE_NONE;
    if ( !comp_eat_numexpr( comp, &numexpr ) || numexpr == NODE_NONE ) {
        comp_error( comp, "Numeric expression expected" );
        return false;
    }
    /*
        NT_NUMASSIGN    numeric assignment
            branches:
                - 1 branch of numeric variable reference
                - 1 branch of numeric expression
    */
    if ( !comp_create_node( comp, pnode, NT_NUMASSIGN, UINT8_C(2), UINT16_C(0), 0, varref, numexpr ) ||
        *pnode == NODE_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_strassign( compiler_t* comp, node_t** pnode ) {
    // str-assign := str-var-ref TOK_EQ str-expr .
    node_t* varref = NODE_NONE;
    if ( !comp_eat_strvarref( comp, &varref ) || varref == NODE_NONE ) {
        return false;
    }
    if ( comp->currtok != TOK_EQ || !comp_fetchtok( comp ) ) {
        comp_error( comp, "Assignment operator '=' expected" );
        return false;
    }
    node_t* strexpr = NODE_NONE;
    if ( !comp_eat_strexpr( comp, &strexpr ) || strexpr == NODE_NONE ) {
        comp_error( comp, "String expression expected" );
        return false;
    }
    /*
        NT_STRASSIGN    string assignment
            branches:
                - 1 branch of string variable reference
                - 1 branch of string expression
    */
    if ( !comp_create_node( comp, pnode, NT_STRASSIGN, UINT8_C(2), UINT16_C(0), 0, varref, strexpr ) ||
        *pnode == NODE_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_substrassign( compiler_t* comp, node_t** pnode ) {
    // substr-op := TOK_LEFT | TOK_MID | TOK_RIGHT .
    // substr-assign := substr-op TOK_STRING TOK_LPAREN expr-list TOK_RPAREN TOK_EQ str-expr .
    uint8_t tok = comp->currtok;
    switch ( tok ) {
        case TOK_LEFT: case TOK_MID: case TOK_RIGHT:
            break;
        default:
            return false;
    }
    if ( !comp_fetchtok( comp ) ) {
        return false;
    }
    if ( comp->currtok != TOK_STRING || !comp_fetchtok( comp ) ) {
        comp_error( comp, "String sigil '$' expected" );
        return false;
    }
    if ( comp->currtok != TOK_LPAREN || !comp_fetchtok( comp ) ) {
        comp_error( comp, "Opening parenthesis '(' expected" );
        return false;
    }
    node_t* exprlist = NODE_NONE;
    if ( !comp_eat_exprlist( comp, &exprlist ) || exprlist == NODE_NONE ) {
        comp_error( comp, "Expression list expected" );
        return false;
    }
    if ( comp->currtok != TOK_RPAREN || !comp_fetchtok( comp ) ) {
        comp_error( comp, "Closing parenthesis ')' expected" );
        return false;
    }
    if ( comp->currtok != TOK_EQ || !comp_fetchtok( comp ) ) {
        comp_error( comp, "Assignment operator '=' expected" );
        return false;
    }
    node_t* strexpr = NODE_NONE;
    if ( !comp_eat_strexpr( comp, &strexpr ) || strexpr == NODE_NONE ) {
        comp_error( comp, "String expression expected after '='" );
        return false;
    }
    /*
        NT_SUBSTRASSIGN     substring assignment
            data:
                - 1 byte of substring operator
            branches:
                - 1 branch of expression list
                - 1 branch of string expression
    */
    if ( !comp_create_node( comp, pnode, NT_SUBSTRASSIGN, UINT8_C(2), UINT16_C(1), &tok, exprlist, strexpr ) ||
        *pnode == NODE_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_anyassign( compiler_t* comp, node_t** pnode ) {
    // any-assign := num-assign | substr-assign | str-assign .
    // [ NT_ANYASSIGN - not generated ]
    if ( comp_eat_numassign( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    if ( comp_eat_substrassign( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    if ( comp_eat_strassign( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    return false;
}

bool comp_eat_assignlist( compiler_t* comp, node_t** pnode ) {
    // assign-list := any-assign { TOK_COMMA any-assign } .
    /*
        NT_ASSIGNLIST       assignment list
            branches:
                - 2 or more branches of assignment expressions
            immediate processing:
                - not generated if there's only one assignment expression
    */
    return comp_eat_list( comp, pnode, NT_ASSIGNLIST, comp_eat_anyassign, TOK_COMMA, "Assignment expected" );
}

bool comp_eat_letstmt( compiler_t* comp, node_t** pnode ) {
    // let-stmt := [ TOK_LET ] assign-list .
    bool mandatory = false;
    if ( comp->currtok == TOK_LET ) {
        if ( !comp_fetchtok( comp ) ) {
            return false;
        }
        // if LET is given, assignment list must be too.
        mandatory = true;
    }
    node_t* assignlist = NODE_NONE;
    if ( !comp_eat_assignlist( comp, &assignlist ) || assignlist == NODE_NONE ) {
        if ( mandatory ) {
            comp_error( comp, "Assignment list expected after LET" );
        }
        return false;
    }
    /*
        NT_LETSTMT      LET statement
            branches:
                - 1 branch of assignment list
    */
    if ( !comp_create_node( comp, pnode, NT_LETSTMT, UINT8_C(1), UINT16_C(0), 0, assignlist ) ||
        *pnode == NODE_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_dimstmt( compiler_t* comp, node_t** pnode ) {
    // dim-stmt := TOK_DIM array-decl-list .
    if ( comp->currtok != TOK_DIM || !comp_fetchtok( comp ) ) {
        return false;
    }
    node_t* decllist = NODE_NONE;
    if ( !comp_eat_arraydecllist( comp, &decllist ) || decllist == NODE_NONE ) {
        comp_error( comp, "Array declaration list expected" );
        return false;
    }
    /*
        NT_DIMSTMT      DIM statement
            branches:
                - 1 branch of array declarator list
    */
    if ( !comp_create_node( comp, pnode, NT_DIMSTMT, UINT8_C(1), UINT16_C(0), 0, decllist ) ||
        *pnode == NODE_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_erasestmt( compiler_t* comp, node_t** pnode ) {
    // erase-stmt := TOK_ERASE empty-array-ref-list .
    if ( comp->currtok != TOK_ERASE || !comp_fetchtok( comp ) ) {
        return false;
    }
    node_t* reflist = NODE_NONE;
    if ( !comp_eat_emptyarrayreflist( comp, &reflist ) || reflist == NODE_NONE ) {
        comp_error( comp, "Empty array reference list expected" );
        return false;
    }
    /*
        NT_ERASESTMT    ERASE statement
            branches:
                - 1 branch of empty array reference list
    */
    if ( !comp_create_node( comp, pnode, NT_ERASESTMT, UINT8_C(1), UINT16_C(0), 0, reflist ) ||
        *pnode == NODE_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_defstmt( compiler_t* comp, node_t** pnode ) {
    // def-stmt := TOK_DEF usr-fn-decl .
    if ( comp->currtok != TOK_DEF || !comp_fetchtok( comp ) ) {
        return false;
    }
    node_t* fndecl = NODE_NONE;
    if ( !comp_eat_usrfndecl( comp, &fndecl ) || fndecl == NODE_NONE ) {
        comp_error( comp, "User function declaration expected" );
        return false;
    }
    /*
        NT_DEFSTMT      DEF statement
            branches:
                - 1 branch of user function declaration
    */
    if ( !comp_create_node( comp, pnode, NT_DEFSTMT, UINT8_C(1), UINT16_C(0), 0, fndecl ) ||
        *pnode == NODE_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_fnassignstmt( compiler_t* comp, node_t** pnode ) {
    // fn-assign-stmt := any-usr-fn-name TOK_EQ expr .
    /*
        NT_FNASSIGNSTMT     assignment of the result of a multi-line user function
            data:
                - 1 byte of type indicator, n bytes of function name
            branches:
                - 1 branch of expression
    */
    if ( comp->currtok != TOK_FN || comp->curfn == NODE_NONE ) {
        return false;
    }
    node_t* fnname = NODE_NONE;
    if ( !comp_eat_anyusrfnname( comp, &fnname ) || fnname == NODE_NONE ) {
        return false;
    }
    if ( fnname->type != comp->curfn->type || fnname->datalen != comp->curfn->datalen ||
         memcmp( fnname->data, comp->curfn->data, fnname->datalen ) != 0 ) {
        comp_error( comp, "Only the function being defined can be assigned" );
        return false;
    }
    if ( comp->currtok != TOK_EQ || !comp_fetchtok( comp ) ) {
        comp_error( comp, "Assignment operator '=' expected" );
        return false;
    }
    node_t* expr = NODE_NONE;
    bool isstr = ( fnname->data[0] & VARTYPEM_BASE ) == VARTYPEV_STR;
    if ( !( isstr ? comp_eat_strexpr( comp, &expr ) : comp_eat_numexpr( comp, &expr ) ) || expr == NODE_NONE ) {
        comp_error( comp, isstr ? "String expression expected" : "Numeric expression expected" );
        return false;
    }
    if ( !comp_create_node( comp, pnode, NT_FNASSIGNSTMT, UINT8_C(1), fnname->datalen, fnname->data, expr ) ||
        *pnode == NODE_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_assignstmt( compiler_t* comp, node_t** pnode ) {
    // assign-stmt := let-stmt | dim-stmt | erase-stmt | def-stmt | fn-assign-stmt .
    // [ NT_ASSIGNSTMT - not generated ]
    if ( comp_eat_letstmt( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    if ( comp_eat_dimstmt( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    if ( comp_eat_erasestmt( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    if ( comp_eat_defstmt( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    if ( comp_eat_fnassignstmt( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    return false;
}

bool comp_eat_forstmt( compiler_t* comp, node_t** pnode ) {
    // for-stmt := TOK_FOR num-base-var-ref TOK_EQ num-expr TOK_TO num-expr [ TOK_STEP num-expr ] .
    if ( comp->currtok != TOK_FOR || !comp_fetchtok( comp ) ) {
        return false;
    }
    // loop variable
    node_t* varref = NODE_NONE;
    if ( !comp_eat_numbasevarref( comp, &varref ) || varref == NODE_NONE ) {
        comp_error( comp, "Numeric variable expected" );
        return false;
    }
    if ( comp->currtok != TOK_EQ || !comp_fetchtok( comp ) ) {
        comp_error( comp, "Assignment operator '=' expected" );
        return false;
    }
    // from expression
    node_t* fromexp = NODE_NONE;
    if ( !comp_eat_numexpr( comp, &fromexp ) || fromexp == NODE_NONE ) {
        comp_error( comp, "Numeric expression expected" );
        return false;
    }
    if ( comp->currtok != TOK_TO || !comp_fetchtok( comp ) ) {
        comp_error( comp, "TO keyword expected" );
        return false;
    }
    // TO expression
    node_t* toexp = NODE_NONE;
    if ( !comp_eat_numexpr( comp, &toexp ) || toexp == NODE_NONE ) {
        comp_error( comp, "Numeric expression expected" );
        return false;
    }
    node_t* stepexp = NODE_NONE;
    if ( comp->currtok == TOK_STEP && comp_fetchtok( comp ) ) {
        // STEP expression
        if ( !comp_eat_numexpr( comp, &stepexp ) || stepexp == NODE_NONE ) {
            comp_error( comp, "Numeric expression expected" );
            return false;
        }
    }
    /*
        NT_FORSTMT      FOR statement
            branches:
                - 1 branch of variable reference
                - 1 branch of FROM expression
                - 1 branch of TO expression
                - 1 branch of STEP expression (may be empty)
    */
    // create node
    if ( !comp_create_node( comp, pnode, NT_FORSTMT, UINT8_C(4), UINT16_C(0), 0,
        varref, fromexp, toexp, stepexp ) || *pnode == NODE_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_nextstmt( compiler_t* comp, node_t** pnode ) {
    // next-stmt := TOK_NEXT [ num-base-var-ref { TOK_COMMA num-base-var-ref } ] .
    if ( comp->currtok != TOK_NEXT || !comp_fetchtok( comp ) ) {
        return false;
    }
    node_t* reflist = NODE_NONE;
    comp_eat_list( comp, &reflist, NT_NUMBASEVARREFLIST, comp_eat_numbasevarref, TOK_COMMA, "Numeric variable reference expected" );
    /*
        NT_NEXTSTMT     NEXT statement
            branches:
                - 1 branch of variable reference list
    */
    // create node
    if ( !comp_create_node( comp, pnode, NT_NEXTSTMT, UINT8_C(1), UINT16_C(0), 0, reflist ) ||
        *pnode == NODE_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_gototarget( compiler_t* comp, node_t** pnode ) {
    // dec-lit := TOK_DECLIT | TOK_DEC0 .. TOK_DEC9 .
    // goto-target := dec-lit | TOK_NUMIDENT .
    double num = 0; char* label = 0;
    uint8_t tok = comp->currtok;
    switch ( tok ) {
        case TOK_DECLIT:
            num = comp->number;
            break;
        case TOK_DEC0: case TOK_DEC1: case TOK_DEC2: case TOK_DEC3: case TOK_DEC4:
        case TOK_DEC5: case TOK_DEC6: case TOK_DEC7: case TOK_DEC8: case TOK_DEC9:
            num = tok - TOK_DEC0;
            break;
        case TOK_NUMIDENT:
            label = dup_param( comp, comp->param );
            break;
        default:
            return false;
    }
    if ( !comp_fetchtok( comp ) ) {
        free( label );
        return false;
    }
    uint8_t data[4];
    if ( label == 0 ) {
        // line number
        int64_t ival = (int64_t) num;
        if ( ival < (int64_t) LINENO_MIN || ival > (int64_t) LINENO_MAX ) {
            comp_error( comp, "Invalid line number" );
            return false;
        }
        uint32_t lnum = (uint32_t) ival;
        data[0] = VARTYPEV_INT;
        data[1] = (uint8_t)( lnum >> UINT8_C(16) );
        data[2] = (uint8_t)( lnum >> UINT8_C(8) );
        data[3] = (uint8_t)  lnum;
    } else {
        // label: lookup or create
        uint16_t lvar = VAROFFS_NONE;
        if ( !vmem_lookup_var( &comp->rt->varmem, VARTYPEV_LABEL, label, &lvar ) || lvar == VAROFFS_NONE ) {
            lvar = VAROFFS_NONE;
            if ( !vmem_create_var( &comp->rt->varmem, VARTYPEV_LABEL, label, UINT8_C(0), 0, UINT8_C(0), 0, &lvar ) ||
                lvar == VAROFFS_NONE ) {
                free( label );
                out_of_memory( comp );
                return false;
            }
        }
        free( label );
        // create node
        data[0] = VARTYPEV_LABEL;
        data[1] = UINT8_C(0);
        data[2] = (uint8_t)( lvar >> UINT8_C(8) );
        data[3] = (uint8_t)  lvar;
    }
    /*
        NT_GOTOTARGET   GOTO/GOSUB target
            data:
                - 1 byte of type (integer or label)
                - 3 bytes of line number or variable offset (label!, high byte zero)
    */
    if ( !comp_create_node( comp, pnode, NT_GOTOTARGET, UINT8_C(0), UINT16_C(4), data ) || *pnode == NODE_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_gotostmt( compiler_t* comp, node_t** pnode ) {
    // goto-kw := TOK_GOTO | TOK_GOSUB | TOK_GO ( TOK_TO | TOK_SUB ) .
    // goto-stmt := goto-kw goto-target .
    uint8_t tok = TOK_EOL;
    if ( comp->currtok == TOK_GO && comp_fetchtok( comp ) ) {
        if ( comp->currtok == TOK_TO ) {
            tok = TOK_GOTO;
        } else if ( comp->currtok == TOK_SUB ) {
            tok = TOK_GOSUB;
        } else {
UNEXP:      comp_error( comp, "TO or SUB expected after GO" );
            return false;
        }
        if ( !comp_fetchtok( comp ) ) {
            goto UNEXP;
        }
    } else if ( comp->currtok == TOK_GOTO || comp->currtok == TOK_GOSUB ) {
        tok = comp->currtok;
        if ( !comp_fetchtok( comp ) ) {
            return false;
        }
    } else {
        return false;
    }
    node_t* target = NODE_NONE;
    if ( !comp_eat_gototarget( comp, &target ) || target == NODE_NONE ) {
        comp_error( comp, "Jump target expected" );
        return false;
    }
    /*
        NT_GOTOSTMT     GOTO/GOSUB statement
            data:
                - 1 byte of GOTO/GOSUB token
            branches:
                - 1 branch of goto/gosub target
    */
    if ( !comp_create_node( comp, pnode, NT_GOTOSTMT, UINT8_C(1), UINT16_C(1), &tok, target ) ||
        *pnode == NODE_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_returnstmt( compiler_t* comp, node_t** pnode ) {
    // return-stmt := TOK_RETURN .
    if ( comp->currtok != TOK_RETURN || !comp_fetchtok( comp ) ) {
        return false;
    }
    if ( !comp_create_node( comp, pnode, NT_RETURNSTMT, UINT8_C(0), UINT16_C(0), 0 ) || *pnode == NODE_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_labelstmt( compiler_t* comp, node_t** pnode ) {
    // label-stmt := TOK_LABEL TOK_NUMIDENT .
    if ( comp->currtok != TOK_LABEL || !comp_fetchtok( comp ) ) {
        return false;
    }
    if ( comp->currtok != TOK_NUMIDENT ) {
UNEXP:  comp_error( comp, "Numeric identifier expected" );
        return false;
    }
    char* label = dup_param( comp, comp->param );
    if ( !comp_fetchtok( comp ) ) {
        free( label );
        goto UNEXP;
    }
    uint16_t labvar = VAROFFS_NONE;
    if ( !vmem_lookup_var( &comp->rt->varmem, VARTYPEV_LABEL, label, &labvar ) || labvar == VAROFFS_NONE ) {
        labvar = VAROFFS_NONE;
        if ( !vmem_create_var( &comp->rt->varmem, VARTYPEV_LABEL, label, UINT8_C(0), 0, UINT16_C(0), 0, &labvar ) ||
            labvar == VAROFFS_NONE ) {
            free( label );
            out_of_memory( comp );
            return false;
        }
    }
    free( label );
    /*
        NT_LABELSTMT    LABEL statement
            data:
                - 2 bytes of label variable offset
    */
    uint8_t data[2];
    data[0] = (uint8_t)( labvar >> UINT8_C(8) );
    data[1] = (uint8_t)  labvar;
    if ( !comp_create_node( comp, pnode, NT_LABELSTMT, UINT8_C(0), UINT16_C(2), data ) || *pnode == NODE_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_thenorgoto( compiler_t* comp, uint8_t* ptok ) {
    // goto-kw := TOK_GOTO | TOK_GOSUB | TOK_GO ( TOK_TO | TOK_SUB ) .
    // then-kw := TOK_THEN [ goto-kw ] .
    uint8_t tok = TOK_EOL;
    if ( comp->currtok == TOK_THEN && comp_fetchtok( comp ) ) {
        tok = TOK_THEN;
    }
    if ( comp->currtok == TOK_GO && comp_fetchtok( comp ) ) {
        if ( comp->currtok == TOK_TO ) {
            tok = TOK_GOTO;
        } else if ( comp->currtok == TOK_SUB ) {
            tok = TOK_GOSUB;
        } else {
UNEXP:      comp_error( comp, "TO or SUB expected after GO" );
            return false;
        }
        if ( !comp_fetchtok( comp ) ) {
            goto UNEXP;
        }
    } else if ( comp->currtok == TOK_GOTO || comp->currtok == TOK_GOSUB ) {
        tok = comp->currtok;
        if ( !comp_fetchtok( comp ) ) {
            return false;
        }
    } else if ( tok == TOK_EOL ) {
        return false;
    }
    *ptok = tok;
    return true;
}

bool comp_eat_singlelineifstmt( compiler_t* comp, node_t** pnode ) {
    // singleline-if-stmt := ( TOK_IF | TOK_UNLESS ) num-expr
    //                       ( then-kw  goto-target [ TOK_ELSE goto-target ] |
    //                         TOK_THEN stmt-list   [ TOK_ELSE stmt-list   ] ) .
    if ( comp->currtok != TOK_IF && comp->currtok != TOK_UNLESS ) {
        return false;
    }
    uint8_t iftok = comp->currtok;
    // -- first marker: at IF/UNLESS token
    comp_push_context( comp );
    if ( !comp_fetchtok( comp ) ) {
POP:    comp_pop_context( comp );
        return false;
    }
    node_t* numexpr = NODE_NONE;
    if ( !comp_eat_numexpr( comp, &numexpr ) || numexpr == NODE_NONE ) {
        goto POP;
    }
    uint8_t gotok = TOK_EOL;
    if ( !comp_eat_thenorgoto( comp, &gotok ) || gotok == TOK_EOL ) {
        goto POP;
    }
    // -- second marker: at THEN or GOTO keyword
    comp_push_context( comp );
    node_t* target1 = NODE_NONE, *target2 = NODE_NONE;
    if ( comp_eat_gototarget( comp, &target1 ) && target1 != NODE_NONE ) {
        // 1st variant, using one or two goto targets
        comp_commit_context( comp );
        comp_commit_context( comp );
        if ( comp->currtok == TOK_ELSE && comp_fetchtok( comp ) ) {
            // ELSE branch on 1st variant: Must also be GOTO target
            if ( !comp_eat_gototarget( comp, &target2 ) || target2 == NODE_NONE ) {
                comp_error( comp, "Jump target expected" );
                return false;
            }
        }
    } else if ( gotok == TOK_THEN ) {
        // 2nd variant, using two statement lists: make sure we're at the right position
        comp_pop_context( comp );
        target1 = NODE_NONE;
        if ( !comp_eat_stmtlist( comp, &target1 ) || target1 == NODE_NONE ) {
            // doesn't satisfy statement list, could be multiline IF/UNLESS, rewind
            goto POP;
        }
        // commit to the context
        comp_commit_context( comp );
        // check for ELSE branch
        if ( comp->currtok == TOK_ELSE && comp_fetchtok( comp ) ) {
            // ELSE branch on 1st variant: Must also be GOTO target
            if ( !comp_eat_stmtlist( comp, &target2 ) || target2 == NODE_NONE ) {
                comp_error( comp, "Statement list expected" );
                return false;
            }
        }
    } else {
        // unknown variant: rewind
        comp_pop_context( comp );
        goto POP;
    }
    /*
        NT_SINGLELINEIFSTMT     Single-line IF statement
            data:
                - 1 token byte: IF/UNLESS
                - 1 token byte: THEN/GOTO/GOSUB
            branches:
                - 1 branch of IF numeric expression
                - 1 branch of THEN gosub/goto target or statement list
                - 1 optional branch of ELSE gosub/goto target or statement list
            immediate processing:
                - THEN/ELSE branches are either both gosub/goto targets or
                  both statement lists
    */
    // at this point, we have our one or two targets
    uint8_t data[2];
    data[0] = iftok;
    data[1] = gotok;
    if ( !comp_create_node( comp, pnode, NT_SINGLELINEIFSTMT, UINT8_C(3), UINT16_C(2), data,
        numexpr, target1, target2 ) || *pnode == NODE_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

static bool comp_get_endiftoken( compiler_t* comp, uint8_t iftok, uint8_t* pendiftok ) {
    // endif-kw := TOK_END TOK_IF | TOK_ENDIF .
    // endunless-kw := TOK_END TOK_UNLESS | TOK_ENDUNLESS .
    uint8_t endtok = TOK_EOL;
    switch ( iftok ) {
        case TOK_IF:        endtok = TOK_ENDIF; break;
        case TOK_UNLESS:    endtok = TOK_ENDUNLESS; break;
        default:
            comp_error( comp, "Internal error (invalid IF token)" );
            return false;
    }
    if ( comp->currtok == TOK_END && comp_fetchtok( comp ) ) {
        // END IF/UNLESS
        if ( comp->currtok != iftok || !comp_fetchtok( comp ) ) {
            comp_error( comp, "END IF/UNLESS expected" );
            return false;
        }
OK:     if ( pendiftok ) {
            *pendiftok = endtok;
        }
        return true;
    }
    if ( comp->currtok == endtok && comp_fetchtok( comp ) ) {
        goto OK;
    }
    return false;
}

bool comp_eat_multilineifstmt( compiler_t* comp, node_t** pnode ) {
    /*
        endif-kw := TOK_END TOK_IF | TOK_ENDIF .
        endunless-kw := TOK_END TOK_UNLESS | TOK_ENDUNLESS .
        multiline-if-stmt := TOK_IF num-expr TOK_THEN TOK_EOL
                             stmt-lines
                             [ TOK_ELSE stmt-lines ]
                             endif-kw |
                             TOK_UNLESS num-expr TOK_THEN TOK_EOL
                             stmt-lines
                             [ TOK_ELSE stmt-lines ]
                             endunless-kw TOK_EOL .
    */
    if ( comp->currtok != TOK_IF && comp->currtok != TOK_UNLESS ) {
        return false;
    }
    uint8_t iftok = comp->currtok;
    if ( !comp_fetchtok( comp ) ) {
        return false;
    }
    node_t* numexpr = NODE_NONE;
    if ( !comp_eat_numexpr( comp, &numexpr ) || numexpr == NODE_NONE ) {
        comp_error( comp, "Numeric expression expected" );
        return false;
    }
    if ( comp->currtok != TOK_THEN || !comp_fetchtok( comp ) ) {
        comp_error( comp, "THEN expected" );
        return false;
    }
    if ( comp->currtok != TOK_EOL || !comp_fetchtok( comp ) ) {
        comp_error( comp, "End of line expected" );
        return false;
    }
    node_t* target1 = NODE_NONE, *target2 = NODE_NONE;
    if ( !comp_eat_stmtlines( comp, &target1 ) || target1 == NODE_NONE ) {
        comp_error( comp, "Statement list expected" );
    }
    if ( comp->currtok == TOK_ELSE && comp_fetchtok( comp ) ) {
        if ( comp->currtok != TOK_EOL || !comp_fetchtok( comp ) ) {
            comp_error( comp, "End of line expected" );
            return false;
        }
        if ( !comp_eat_stmtlines( comp, &target2 ) || target2 == NODE_NONE ) {
            comp_error( comp, "Statement list expected" );
        }
    }
    if ( !comp_get_endiftoken( comp, iftok, 0 ) ) {
        comp_error( comp, "END IF / END UNLESS / ENDIF / ENDUNLESS expected" );
        return false;
    }
    /*
        NT_MULTILINEIFSTMT      Multi-line IF statement
            data:
                - 1 byte of IF/UNLESS token
            branches:
                - 1 branch of IF numeric expression
                - 1 branch of statement lines
                - 1 optional branch of ELSE statement lines
    */
    if ( !comp_create_node( comp, pnode, NT_MULTILINEIFSTMT, UINT8_C(3), UINT16_C(1), &iftok,
        numexpr, target1, target2 ) || *pnode == NODE_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

// goto-kw := TOK_GOTO | TOK_GOSUB | TOK_GO ( TOK_TO | TOK_SUB ) .
static bool comp_eat_gotokw( compiler_t* comp, uint8_t* ptok ) {
    if ( comp->currtok == TOK_GO && comp_fetchtok( comp ) ) {
        if ( comp->currtok == TOK_TO ) {
            *ptok = TOK_GOTO;
        } else if ( comp->currtok == TOK_SUB ) {
            *ptok = TOK_GOSUB;
        } else {
            comp_error( comp, "TO or SUB expected after GO" );
            return false;
        }
        return comp_fetchtok( comp );
    }
    if ( comp->currtok == TOK_GOTO || comp->currtok == TOK_GOSUB ) {
        *ptok = comp->currtok;
        return comp_fetchtok( comp );
    }
    return false;
}

// creates a statement node without data and branches
static bool comp_make_simple_stmt( compiler_t* comp, node_t** pnode, uint8_t nodetype ) {
    if ( !comp_create_node( comp, pnode, nodetype, UINT8_C(0), UINT16_C(0), 0 ) || *pnode == NODE_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

// creates a statement node that has a numeric expression as its only branch
static bool comp_make_expr_stmt( compiler_t* comp, node_t** pnode, uint8_t nodetype, node_t* expr ) {
    if ( !comp_create_node( comp, pnode, nodetype, UINT8_C(1), UINT16_C(0), 0, expr ) || *pnode == NODE_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_endstmt( compiler_t* comp, node_t** pnode ) {
    // end-stmt := TOK_END .  -- not followed by IF, UNLESS or FN (those end a block, see comp_get_endiftoken())
    if ( comp->currtok != TOK_END ) {
        return false;
    }
    comp_push_context( comp );
    if ( !comp_fetchtok( comp ) || comp->currtok == TOK_IF || comp->currtok == TOK_UNLESS || comp->currtok == TOK_FN ) {
        comp_pop_context( comp );
        return false;
    }
    comp_commit_context( comp );
    return comp_make_simple_stmt( comp, pnode, NT_ENDSTMT );
}

bool comp_eat_popstmt( compiler_t* comp, node_t** pnode ) {
    // pop-stmt := TOK_POP .
    if ( comp->currtok != TOK_POP || !comp_fetchtok( comp ) ) {
        return false;
    }
    return comp_make_simple_stmt( comp, pnode, NT_POPSTMT );
}

bool comp_eat_againstmt( compiler_t* comp, node_t** pnode ) {
    // again-stmt := TOK_AGAIN [ dec-lit ] .
    if ( comp->currtok != TOK_AGAIN || !comp_fetchtok( comp ) ) {
        return false;
    }
    double num = 1.0;
    uint8_t tok = comp->currtok;
    if ( tok == TOK_DECLIT || ( tok >= TOK_DEC0 && tok <= TOK_DEC9 ) ) {
        num = tok == TOK_DECLIT ? comp->number : (double)( tok - TOK_DEC0 );
        if ( !comp_fetchtok( comp ) ) {
            return false;
        }
        if ( !( num >= 1.0 && num <= 65535.0 ) ) {
            comp_error( comp, "Invalid loop number" );
            return false;
        }
    }
    /*
        NT_AGAINSTMT        AGAIN statement
            data:
                - 2 bytes of loop number
    */
    uint16_t loopnum = (uint16_t) num;
    uint8_t data[2];
    data[0] = (uint8_t)( loopnum >> UINT8_C(8) );
    data[1] = (uint8_t)  loopnum;
    if ( !comp_create_node( comp, pnode, NT_AGAINSTMT, UINT8_C(0), UINT16_C(2), data ) || *pnode == NODE_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_repeatstmt( compiler_t* comp, node_t** pnode ) {
    // repeat-stmt := TOK_REPEAT .
    if ( comp->currtok != TOK_REPEAT || !comp_fetchtok( comp ) ) {
        return false;
    }
    return comp_make_simple_stmt( comp, pnode, NT_REPEATSTMT );
}

// until-stmt := TOK_UNTIL num-expr .   while-stmt := TOK_WHILE num-expr .
static bool comp_eat_condstmt( compiler_t* comp, node_t** pnode, uint8_t tok, uint8_t nodetype ) {
    if ( comp->currtok != tok || !comp_fetchtok( comp ) ) {
        return false;
    }
    node_t* expr = NODE_NONE;
    if ( !comp_eat_numexpr( comp, &expr ) || expr == NODE_NONE ) {
        comp_error( comp, "Numeric expression expected" );
        return false;
    }
    return comp_make_expr_stmt( comp, pnode, nodetype, expr );
}

bool comp_eat_untilstmt( compiler_t* comp, node_t** pnode ) {
    return comp_eat_condstmt( comp, pnode, TOK_UNTIL, NT_UNTILSTMT );
}

bool comp_eat_whilestmt( compiler_t* comp, node_t** pnode ) {
    return comp_eat_condstmt( comp, pnode, TOK_WHILE, NT_WHILESTMT );
}

bool comp_eat_wendstmt( compiler_t* comp, node_t** pnode ) {
    // wend-stmt := TOK_WEND .
    if ( comp->currtok != TOK_WEND || !comp_fetchtok( comp ) ) {
        return false;
    }
    return comp_make_simple_stmt( comp, pnode, NT_WENDSTMT );
}

bool comp_eat_uendstmt( compiler_t* comp, node_t** pnode ) {
    // uend-stmt := TOK_UEND .
    if ( comp->currtok != TOK_UEND || !comp_fetchtok( comp ) ) {
        return false;
    }
    return comp_make_simple_stmt( comp, pnode, NT_UENDSTMT );
}

bool comp_eat_onstmt( compiler_t* comp, node_t** pnode ) {
    // on-stmt := TOK_ON num-expr goto-kw goto-target { TOK_COMMA goto-target } .
    if ( comp->currtok != TOK_ON || !comp_fetchtok( comp ) ) {
        return false;
    }
    node_t* expr = NODE_NONE;
    if ( !comp_eat_numexpr( comp, &expr ) || expr == NODE_NONE ) {
        comp_error( comp, "Numeric expression expected" );
        return false;
    }
    uint8_t gotok = TOK_EOL;
    if ( !comp_eat_gotokw( comp, &gotok ) ) {
        comp_error( comp, "GOTO or GOSUB expected" );
        return false;
    }
    /*
        NT_ONSTMT           ON statement
            data:
                - 1 byte of GOTO/GOSUB token
            branches:
                - 1 branch of numeric expression
                - 1 or more branches of goto/gosub targets
    */
    if ( !comp_create_node( comp, pnode, NT_ONSTMT, UINT8_C(1), UINT16_C(1), &gotok, expr ) || *pnode == NODE_NONE ) {
        out_of_memory( comp );
        return false;
    }
    for (;;) {
        node_t* target = NODE_NONE;
        if ( !comp_eat_gototarget( comp, &target ) || target == NODE_NONE ) {
            comp_error( comp, "Jump target expected" );
            return false;
        }
        if ( !comp_add_branch( comp, *pnode, target ) ) {
            out_of_memory( comp );
            return false;
        }
        if ( comp->currtok != TOK_COMMA || !comp_fetchtok( comp ) ) {
            break;
        }
    }
    return true;
}

bool comp_eat_controlflowstmt( compiler_t* comp, node_t** pnode ) {
    // control-flow-stmt := for-stmt | next-stmt | goto-stmt | return-stmt | label-stmt | end-stmt | pop-stmt |
    //                      again-stmt | repeat-stmt | until-stmt | while-stmt | wend-stmt | uend-stmt | on-stmt |
    //                      singleline-if-stmt | multiline-if-stmt .
    // [ NT_CONTROLFLOWSTMT - not generated ]
    if ( comp_eat_forstmt( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    if ( comp_eat_nextstmt( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    if ( comp_eat_gotostmt( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    if ( comp_eat_returnstmt( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    if ( comp_eat_labelstmt( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    if ( comp_eat_endstmt( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    if ( comp_eat_popstmt( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    if ( comp_eat_againstmt( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    if ( comp_eat_repeatstmt( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    if ( comp_eat_untilstmt( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    if ( comp_eat_whilestmt( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    if ( comp_eat_wendstmt( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    if ( comp_eat_uendstmt( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    if ( comp_eat_onstmt( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    if ( comp_eat_singlelineifstmt( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    if ( comp_eat_multilineifstmt( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    return false;
}

bool comp_eat_stmt( compiler_t* comp, node_t** pnode ) {
    // stmt := io-stmt | assign-stmt | control-flow-stmt .
    // [ NT_STMT - not generated ]
    if ( comp_eat_iostmt( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    if ( comp_eat_assignstmt( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    if ( comp_eat_controlflowstmt( comp, pnode ) && *pnode != NODE_NONE ) {
        return true;
    }
    return false;
}

bool comp_eat_stmtlist( compiler_t* comp, node_t** pnode ) {
    // stmt-list := stmt { TOK_COLON stmt } .
    /*
        NT_STMTLIST         Statement list
            branches:
                - 2 or more statements
            immediate processing:
                - generated only if more than 1 statement
    */
    return comp_eat_list( comp, pnode, NT_STMTLIST, comp_eat_stmt, TOK_COLON, "Statement expected" );
}

bool comp_eat_stmtline( compiler_t* comp, node_t** pnode ) {
    // stmt-line := stmt-list [ TOK_QUOLIT ] TOK_EOL .
    uint32_t lnum = comp->iter.hdr.lineno;
    node_t* stmtlist = NODE_NONE;
    if ( !comp_eat_stmtlist( comp, &stmtlist ) || stmtlist == NODE_NONE ) {
        return false;
    }
    if ( comp->currtok == TOK_QUOLIT ) {
        comp_fetchtok( comp );
    }
    if ( comp->currtok != TOK_EOL ) {
        comp_error( comp, "End of line expected" );
        return false;
    }
    // ATTN: May return false at the end of text!
    comp_fetchtok( comp );
    /*
        NT_STMTLINE         Statement line
            data:
                - 3 bytes of line number (not present in direct mode)
            branches:
                - 1 branch of statement list
                - this is the root node for direct mode
    */
    uint8_t data[3];
    data[0] = (uint8_t)( lnum >> UINT8_C(16) );
    data[1] = (uint8_t)( lnum >> UINT8_C(8) );
    data[2] = (uint8_t)  lnum;
    uint16_t datalen = lnum == LINENO_DEL ? UINT16_C(0) : UINT16_C(3);
    // create node
    if ( !comp_create_node( comp, pnode, NT_STMTLINE, UINT8_C(1), datalen, data, stmtlist ) ||
        *pnode == NODE_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_stmtlines( compiler_t* comp, node_t** pnode ) {
    // stmt-lines := stmt-line { stmt-line } .
    /*
        NT_STMTLINES        Statement lines
            branches:
                - 2 or more statement lines
            immediate processing:
                - generated only if more than 1 statement
                - this is the root node for program mode
    */
    return comp_eat_list( comp, pnode, NT_STMTLINES, comp_eat_stmtline, TOK_EOL, "Statement line expected" );
}

bool comp_nextline( compiler_t* comp ) {
    if ( !step_iterate_program( &comp->iter ) ) {
        return false;
    }
    comp->tokp = comp->iter.tok;
    comp->tokend = comp->tokp + iter_remaining( &comp->iter, comp->tokp );
    comp->currtok = TOK_NONE;
    return true;
}

bool comp_fetchtok( compiler_t* comp ) {
NEXTTOK:
    if ( comp->tokp == 0 ) {
        return false;
    }
    if ( comp->currtok == TOK_EOL ) {
        if ( comp->iter.hdr.lineno != LINENO_NONE ) {
            if ( !comp_nextline( comp ) ) {
                return false;
            }
            goto NEXTTOK;
        }
        return false;
    }
SKIPTOK:
    uint8_t tok = comp->currtok = *comp->tokp++;
printf( "*** TOKEN: %02" PRIx8 "\n", tok );
    if ( tok == TOK_EOL ) {
        --comp->tokp;
        return true;
    }
    if ( tok == TOK_SPACE ) {
        goto SKIPTOK;
    }
    static const struct {
        uint8_t tok;
        bool (*read_fn)( rbuf_t*, char** );
    } littbl[] = {
        { TOK_IDENT, read_ident }, { TOK_NUMIDENT, read_ident }, { TOK_STRIDENT, read_ident }, { TOK_INTIDENT, read_ident },
        { TOK_STRLIT, read_strlit }, { TOK_HEXLIT, read_hexlit }, { TOK_DECLIT, read_declit }, { TOK_OCTLIT, read_octlit },
        { TOK_QUALIT, read_qualit }, { TOK_BINLIT, read_binlit }, { TOK_SHLLIT, read_shllit }, { TOK_QUOLIT, read_quolit },
        { TOK_BRKLIT, read_brklit }, { TOK_BRCLIT, read_brclit }, { 0, 0 }
    };
    ri_sigil = false;    // for identifiers, we DON'T want the sigil to be returned
    for ( int i=0; littbl[i].tok; ++i ) {
        if ( littbl[i].tok == tok ) {
            --comp->tokp;
            rbuf_t rbuf;
            init_rbuf( &rbuf, comp->tokp, comp->tokend > comp->tokp ? (size_t)( comp->tokend - comp->tokp ) : 0 );
            if ( !littbl[i].read_fn( &rbuf, &comp->param ) ) {
                return false;
            }
            comp->tokp += rbuf.rpos;
            int base = 0;
            switch ( tok ) {
                case TOK_HEXLIT: base = 16; goto NONDEC;
                case TOK_OCTLIT: base = 8; goto NONDEC;
                case TOK_QUALIT: base = 4; goto NONDEC;
                case TOK_BINLIT: base = 2; goto NONDEC;
                default:
                    break;
                case TOK_DECLIT:
                    comp->number = strtod( comp->param, 0 );
                    break;
                NONDEC:
                    comp->number = (double)((int64_t)(strtoull( comp->param, 0, base )));
                    break;
            }
            return true;
        }
    }
    if ( is_sngchrtok( tok ) || ( tok >= TOK_DEC0 && tok <= TOK_DEC9 ) ) {
        return true;
    } else if ( is_keyword2( tok ) ) {
        return true;
    }
    // unknown token, back up
    --comp->tokp;
    return false;
}

bool begin_comp( compiler_t* comp ) {

    if ( comp->syntree != NODE_NONE ) {
        comp_error( comp, "Internal error (syntax tree already exists)" );
        return false;
    }

    // fetch first token
    if ( !comp_fetchtok( comp ) ) {
printf( "*** empty line\n" );
        return false;
    }

    return true;
}

void run_compiler( compiler_t* comp ) {

    // begin compilation
    if ( !begin_comp( comp ) ) {
        return;
    }

    // compile statement lines
    if ( !comp_eat_stmtlines( comp, &comp->syntree ) || comp->syntree == NODE_NONE ) {
        comp_error( comp, "Statement lines expected" );
        return;
    }

    if ( comp->currtok != TOK_EOL ) {
        comp_error( comp, "Internal error (not at end of text)" );
        return;
    }
}
