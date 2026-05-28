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

void init_compiler( compiler_t* comp, runtime_t* rt, program_t* pgm, bool keepmemory ) {
    comp->rt = rt;
    clear_iter( &comp->iter, pgm );
    comp->tokp     = 0;
    comp->currtok  = TOK_NONE;
    comp->treesize = UINT16_C(0);
    comp->ctxstk   = 0;
    // CAUTION: Set the "keepmemory" flag only if you know what you're doing!
    if ( keepmemory ) {
        return;
    }
    comp->report   = 0;
    comp->halt     = 0;
    comp->userdata = 0;
    comp->treesize = UINT16_C(0);
    comp->syntree  = NODEOFFS_NONE;
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

void comp_push_context( compiler_t* comp ) {
    comp_ctxstk_t* ctx = (comp_ctxstk_t*) malloc( sizeof(comp_ctxstk_t) );
    if ( ctx == 0 ) {
        comp_error( comp, "Out of memory" );
        return;
    }
    ctx->prev = comp->ctxstk; comp->ctxstk = ctx;
    memcpy( &ctx->iter, &comp->iter, sizeof(pgmiter_t) );
    ctx->tokp = comp->tokp; ctx->currtok = comp->currtok;
    switch ( ctx->currtok ) {
        case TOK_HEXLIT: case TOK_DECLIT: case TOK_OCTLIT: case TOK_QUALIT: case TOK_BINLIT:
            ctx->number = comp->number;
            break;
        case TOK_IDENT: case TOK_NUMIDENT: case TOK_STRIDENT: case TOK_INTIDENT:
        case TOK_STRLIT: case TOK_SHLLIT: case TOK_QUOLIT: case TOK_BRKLIT: case TOK_BRCLIT:
            snprintf( ctx->param, 256U, "%s", comp->param );
            break;
    }
}

void comp_commit_context( compiler_t* comp ) {
    comp_ctxstk_t* ctx = comp->ctxstk;
    if ( ctx == 0 ) {
        return;
    }
    comp->ctxstk = ctx->prev;
    free( ctx );
}

void comp_pop_context( compiler_t* comp ) {
    comp_ctxstk_t* ctx = comp->ctxstk;
    if ( ctx == 0 ) {
        return;
    }
    comp->ctxstk = ctx->prev;
    memcpy( &comp->iter, &ctx->iter, sizeof(pgmiter_t) );
    comp->tokp = ctx->tokp; comp->currtok = ctx->currtok;
    switch ( comp->currtok ) {
        case TOK_HEXLIT: case TOK_DECLIT: case TOK_OCTLIT: case TOK_QUALIT: case TOK_BINLIT:
            comp->number = ctx->number;
            break;
        case TOK_IDENT: case TOK_NUMIDENT: case TOK_STRIDENT: case TOK_INTIDENT:
        case TOK_STRLIT: case TOK_SHLLIT: case TOK_QUOLIT: case TOK_BRKLIT: case TOK_BRCLIT:
            snprintf( comp->param, 256U, "%s", ctx->param );
            break;
    }
    free( ctx );
}

bool comp_alloc_tree( compiler_t* comp, uint16_t size, uint16_t* poffs ) {
    if ( size > TREESIZE_MAX - comp->treesize ) {
        return false;
    }
    *poffs = (uint16_t) comp->treesize;
    comp->treesize += size;
    return true;
}

bool comp_create_node( compiler_t* comp, uint16_t* pnodeoffs, uint8_t nodetype, uint8_t numbranches, uint16_t datalen,
    const void* pdata, ... ) {
    if ( comp == 0 || pnodeoffs == 0 || nodetype == NT_UNKNOWN || ( datalen != UINT16_C(0) && pdata == 0 ) ) {
        return false;
    }
    uint16_t size = NODEHDR_SIZE + datalen + BRANCHENT_SIZE * numbranches;
    uint16_t nodepos = NODEOFFS_NONE;
    if ( !comp_alloc_tree( comp, size, &nodepos ) || nodepos == NODEOFFS_NONE ) {
        return false;
    }
    // <nodetype.8> <numbranches.8> <datalen.16> <firstbranch.16> <lastbranch.16> <data...>
    uint16_t offs = nodepos;
    comp->tree[ offs++ ] = nodetype;
    comp->tree[ offs++ ] = numbranches;
    WRITE16( comp, offs, datalen ); offs += UINT16_C(2);
    uint16_t linkoffs = offs;
    WRITE16( comp, offs, NODEOFFS_NONE ); offs += UINT16_C(2);
    WRITE16( comp, offs, NODEOFFS_NONE ); offs += UINT16_C(2);
    if ( datalen ) {
        memcpy( &comp->tree[ offs ], pdata, datalen );
        offs += datalen;
    }
    if ( numbranches ) {
        uint16_t firstbranch = NODEOFFS_NONE, lastbranch = NODEOFFS_NONE;
        va_list ap; va_start( ap, pdata );
        while ( numbranches-- ) {
            uint16_t branchoffs = (uint16_t) va_arg( ap, int );
            if ( firstbranch == NODEOFFS_NONE ) {
                firstbranch = offs;
            }
            if ( lastbranch != NODEOFFS_NONE ) {
                // <nodepos.16> <nextbranch.16>
                WRITE16( comp, lastbranch + 2U, offs );
            }
            lastbranch = offs;
            // <nodepos.16> <nextbranch.16>
            WRITE16( comp, offs, branchoffs    ); offs += UINT16_C(2);
            WRITE16( comp, offs, NODEOFFS_NONE ); offs += UINT16_C(2);
        }
        va_end( ap );
        WRITE16( comp, linkoffs     , firstbranch );
        WRITE16( comp, linkoffs + 2U, lastbranch  );
    }
    *pnodeoffs = nodepos;
    return true;
}

bool comp_add_branch( compiler_t* comp, uint16_t nodeoffs, uint16_t branchoffs ) {
    if ( nodeoffs == NODEOFFS_NONE || branchoffs == NODEOFFS_NONE ) {
        return false;
    }
    // <nodetype.8> <numbranches.8> <datalen.16> <firstbranch.16> <lastbranch.16> <data...>
    if ( comp->tree[ nodeoffs + 1U ] == UINT8_C(255) ) {    // numbranches too big
        return false;
    }
    // read last branch link from node
    uint16_t lastbranch = EXTRACT16( comp, nodeoffs + 6U );
    // allocate new branch entry
    uint16_t offs = NODEOFFS_NONE;
    if ( !comp_alloc_tree( comp, BRANCHENT_SIZE, &offs ) || offs == NODEOFFS_NONE ) {
        return false;
    }
    if ( lastbranch != NODEOFFS_NONE ) {
        // if there was a previous branch, link it to this one
        // <nodepos.16> <nextbranch.16>
        WRITE16( comp, lastbranch + 2U, offs );
    }
    lastbranch = offs;  // now this node is the last one in the list
    // update lastbranch link in node
    WRITE16( comp, nodeoffs + 6U, lastbranch );
    // increment number of branches
    comp->tree[ nodeoffs + 1U ] += UINT8_C(1);
    // store new branch info
    // <nodepos.16> <nextbranch.16>
    WRITE16( comp, offs, branchoffs    ); offs += UINT16_C(2);
    WRITE16( comp, offs, NODEOFFS_NONE );
    // done
    return true;
}

bool comp_eat_list( compiler_t* comp, uint16_t* pnodeoffs, uint8_t nodetype, comp_eatfn_t element_eater, uint8_t septok,
    const char* errortext ) {
    // list := element { SEPTOK element } .  -- if SEPTOK is TOK_EOL, there's no separator token
    uint16_t expr1 = NODEOFFS_NONE;
    if ( !element_eater( comp, &expr1 ) || expr1 == NODEOFFS_NONE ) {
        return false;
    }
    uint16_t nodeoffs = NODEOFFS_NONE;
    for (;;) {
        bool mandatory = false; uint16_t expr2 = NODEOFFS_NONE;
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
        if ( !element_eater( comp, &expr2 ) || expr2 == NODEOFFS_NONE ) {
            if ( mandatory ) {  // mandatory expression missing: stop
                comp_error( comp, errortext );
            }
            // stop processing
            break;
        }
        // we have now a new branch; first see if we already have a node or need to create one
        if ( nodeoffs == NODEOFFS_NONE ) {
            if ( !comp_create_node( comp, &nodeoffs, nodetype, UINT8_C(2), UINT16_C(0), 0, (int) expr1, (int) expr2 ) ||
                 nodeoffs == NODEOFFS_NONE ) {
                // failed to create node: cancel operation
OOM:            out_of_memory( comp );
                break;
            }
        } else {
            // the node already exists: add a new branch
            if ( !comp_add_branch( comp, nodeoffs, expr2 ) ) {
                // something went wrong: cancel
                goto OOM;
            }
        }
        // successful, continue
    }
    // either return node with what we already have, or just the first branch
    *pnodeoffs = nodeoffs != NODEOFFS_NONE ? nodeoffs : expr1;
    return true;
}

bool comp_eat_numexlist( compiler_t* comp, uint16_t* pnodeoffs ) {
    // num-ex-list := num-expr { TOK_COMMA num-expr } .
    return comp_eat_list( comp, pnodeoffs, NT_NUMEXLIST, comp_eat_numexpr, TOK_COMMA, "Numeric expression expected" );
}

bool comp_eat_strexlist( compiler_t* comp, uint16_t* pnodeoffs ) {
    // str-ex-list := str-expr { TOK_COMMA str-expr } .
    return comp_eat_list( comp, pnodeoffs, NT_STREXLIST, comp_eat_strexpr, TOK_COMMA, "String expression expected" );
}

bool comp_eat_exprlist( compiler_t* comp, uint16_t* pnodeoffs ) {
    // expr-list := expr { TOK_COMMA expr } .
    return comp_eat_list( comp, pnodeoffs, NT_EXPRLIST, comp_eat_expr, TOK_COMMA, "Expression expected" );
}

bool comp_eat_arrayindex( compiler_t* comp, uint16_t* pnodeoffs ) {
    // array-index := num-ex-list | str-expr .
    if ( comp_eat_numexlist( comp, pnodeoffs ) && *pnodeoffs != NODEOFFS_NONE ) {
        return true;
    }
    if ( comp_eat_strexpr( comp, pnodeoffs ) && *pnodeoffs != NODEOFFS_NONE ) {
        return true;
    }
    return false;
}

bool comp_eat_arraysub( compiler_t* comp, uint16_t* pnodeoffs ) {
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
        uint16_t arrayindexnode = NODEOFFS_NONE;
        if ( !comp_eat_arrayindex( comp, &arrayindexnode ) || arrayindexnode == NODEOFFS_NONE ) {
            goto ERROR;
        }
        if ( comp->currtok != TOK_RPAREN || !comp_fetchtok( comp ) ) {
            comp_error( comp, "Closing parenthesis ')' expected" );
            return false;
        }
        if ( !comp_create_node( comp, pnodeoffs, NT_ARRAYSUB, UINT8_C(1), UINT16_C(0), 0, (int) arrayindexnode ) ) {
            out_of_memory( comp );
            return false;
        }
        return true;
    }
    return false;
}

bool comp_node_iter_branches( compiler_t* comp, uint16_t nodeoffs, void* userdata, bool (*callback)( void*, uint16_t ) ) {
    //  <nodetype.8> <numbranches.8> <datalen.16> <firstbranch.16> <lastbranch.16> <data...>
    //  <nodepos.16> <nextbranch.16>
    if ( nodeoffs == NODEOFFS_NONE || comp->tree[ nodeoffs + 1U ] == UINT8_C(0) || callback == 0 ) {
        return false;
    }
    uint16_t branch = EXTRACT16( comp, nodeoffs + 4U );
    while ( branch != NODEOFFS_NONE ) {
        uint16_t brnode = EXTRACT16( comp, branch );
        if ( !callback( userdata, brnode ) ) {
            return false;
        }
        branch = EXTRACT16( comp, branch + 2U );
    }
    return true;
}

static double comp_extract_float( const uint8_t* mem, uint16_t offs ) {
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

static bool comp_gather_dims( void* userdata, uint16_t node ) {
    compiler_t* comp = (compiler_t*) userdata;
    if ( comp->numdim >= MAXDIM ) {
        comp_error( comp, "Too many dimensions" );
        return false;
    }
    //  <nodetype.8> <numbranches.8> <datalen.16> <firstbranch.16> <lastbranch.16> <data...>
    uint8_t nodetype = comp->tree[ node ];
    if ( nodetype != NT_NUMLIT ) {
        comp_error( comp, "Number expected" );
        return false;
    }
    uint16_t datalen = EXTRACT16( comp, node + 2U );
    if ( datalen != UINT16_C(8) ) {
        comp_error( comp, "Bad number" );
        return false;
    }
    double val = comp_extract_float( comp->tree, node + 8U );
    if ( val < 1.0 || val >= 65536.0 ) {
        comp_error( comp, "Dimension out of range" );
        return false;
    }
    uint16_t v16 = (uint16_t)( (int64_t) val );
    comp->arraydim[ comp->numdim++ ] = v16;
    return true;
}

bool comp_eat_arraydimdecl( compiler_t* comp, uint16_t* pnodeoffs ) {
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
        if ( !comp_create_node( comp, pnodeoffs, NT_ARRAYDIMDECL, UINT8_C(0), datalen, &data ) || *pnodeoffs == NODEOFFS_NONE ) {
OOM:        out_of_memory( comp );
            return false;
        }
        return true;
    }
    uint16_t numexpr = NODEOFFS_NONE;
    if ( !comp_eat_numexlist( comp, &numexpr ) || numexpr == NODEOFFS_NONE ) {
        comp_error( comp, "Numeric expression expected" );
        return false;
    }
    comp->numdim = UINT8_C(0);
    if ( comp->tree[ numexpr ] == NT_NUMLIT ) {
        // single dimension
        if ( !comp_gather_dims( comp, numexpr ) ) {
            return false;
        }
    } else if ( comp->tree[ numexpr ] == NT_NUMEXLIST ) {
        // multiple dimensions
        if ( !comp_node_iter_branches( comp, numexpr, comp, comp_gather_dims ) ) {
            return false;
        }
    } else {
        // unexpected node type
INTERR: comp_error( comp, "Internal error" );
        return false;
    }
    // make sure we have at least one dimension
    if ( comp->numdim == UINT8_C(0) ) {
        goto INTERR;
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
    if ( totalsize == 0 || totalsize > 65535U ) {
        goto LARGE;
    }
    if ( !comp_create_node( comp, pnodeoffs, NT_ARRAYDIMDECL, UINT8_C(0), datalen2, data2 ) || *pnodeoffs == NODEOFFS_NONE ) {
        goto OOM;
    }
    return true;
}

bool comp_eat_arraydecl( compiler_t* comp, uint16_t* pnodeoffs ) {
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
    uint16_t varnodeoffs = NODEOFFS_NONE;
    if ( !comp_eat_anybasevarref( comp, &varnodeoffs ) || varnodeoffs == NODEOFFS_NONE ) {
        return false;
    }
    if ( comp->currtok != TOK_LPAREN || !comp_fetchtok( comp ) ) {
        comp_error( comp, "Opening parenthesis '(' expected" );
        return false;
    }
    uint16_t declnodeoffs = NODEOFFS_NONE;
    if ( !comp_eat_arraydimdecl( comp, &declnodeoffs ) || declnodeoffs == NODEOFFS_NONE ) {
        comp_error( comp, "Array dimension declaration expected" );
        return false;
    }
    if ( comp->currtok != TOK_RPAREN || !comp_fetchtok( comp ) ) {
        comp_error( comp, "Closing parenthesis ')' expected" );
        return false;
    }

    // read dimension declaration
    //  <nodetype.8> <numbranches.8> <datalen.16> <firstbranch.16> <lastbranch.16> <data...>
    if ( comp->tree[ declnodeoffs ] != NT_ARRAYDIMDECL || comp->tree[ declnodeoffs + 1U ] != 0 ) {
        comp_error( comp, "Internal error (NT_ARRAYDIMDECL expected)" );
        return false;
    }

    uint16_t datalen = EXTRACT16( comp, declnodeoffs + 2U );
    if ( datalen <= UINT16_C(1) ) { // DYNAMIC or ASSOC
        comp_error( comp, "Feature not supported yet" );
        return false;
    }

    uint16_t dataoffs = declnodeoffs + UINT16_C(8);
    uint16_t numdims = datalen / UINT16_C(2);
    if ( numdims > MAXDIM ) {
        comp_error( comp, "Internal error (unexpected number of dimensions)" );
        return false;
    }
    uint16_t dims[MAXDIM];
    for ( uint16_t i=0; i < numdims; ++i ) {
        dims[i] = EXTRACT16( comp, dataoffs );
        dataoffs += UINT16_C(2);
    }

    /*
        NT_NUMBASEVARREF    numeric base variable reference
        NT_STRBASEVARREF    string base variable reference
            data:
                - 1 byte of type indicator, n bytes of name
    */
    // read variable reference
    //  <nodetype.8> <numbranches.8> <datalen.16> <firstbranch.16> <lastbranch.16> <data...>
    datalen  = EXTRACT16( comp, varnodeoffs + 2U );
    dataoffs = varnodeoffs + UINT16_C(8);
    static char name[256]; name[0] = '\0';
    uint8_t vartype = VARTYPEF_ARRAY;
    if ( comp->tree[ varnodeoffs ] == NT_NUMBASEVARREF || comp->tree[ varnodeoffs ] == NT_STRBASEVARREF ) {
        if ( datalen < UINT16_C(2) ) {
            comp_error( comp, "Internal error (fault in variable reference)" );
            return false;
        }
        // data:
        //    - 1 byte of type indicator, n bytes of name
        uint8_t typeind = comp->tree[ dataoffs++ ];
        uint8_t namelen = comp->tree[ dataoffs++ ];
        if ( namelen ) {
            memcpy( name, &comp->tree[ dataoffs ], namelen );
        }
        name[ namelen ] = '\0';
        vartype |= typeind & VARTYPEM_BASE;
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

    if ( !comp_create_node( comp, pnodeoffs, NT_ARRAYDECL, UINT8_C(0), UINT16_C(3), data ) || *pnodeoffs == NODEOFFS_NONE ) {
        out_of_memory( comp );
        return false;
    }

    return true;
}

bool comp_eat_arraydecllist( compiler_t* comp, uint16_t* pnodeoffs ) {
    /*
        NT_ARRAYDECLLIST    array declaration list
            data: none
            branches: the NT_ARRAYDECL nodes
    */
    // array-decl-list := array-decl { TOK_COMMA array-decl } .
    return comp_eat_list( comp, pnodeoffs, NT_ARRAYDECLLIST, comp_eat_arraydecl, TOK_COMMA, "Array declaration expected" );
}

bool comp_eat_emptyarrayref( compiler_t* comp, uint16_t* pnodeoffs ) {
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
    uint16_t varnodeoffs = NODEOFFS_NONE;
    if ( !comp_eat_anybasevarref( comp, &varnodeoffs ) || varnodeoffs == NODEOFFS_NONE ) {
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
    uint16_t datalen  = EXTRACT16( comp, varnodeoffs + 2U );
    uint16_t dataoffs = varnodeoffs + UINT16_C(8);
    static char name[256]; name[0] = '\0';
    uint8_t vartype = VARTYPEF_ARRAY;
    if ( comp->tree[ varnodeoffs ] == NT_NUMBASEVARREF || comp->tree[ varnodeoffs ] == NT_STRBASEVARREF ) {
        if ( datalen < UINT16_C(2) ) {
            comp_error( comp, "Internal error (fault in variable reference)" );
            return false;
        }
        // data:
        //    - 1 byte of type indicator, n bytes of name
        uint8_t typeind = comp->tree[ dataoffs++ ];
        uint8_t namelen = comp->tree[ dataoffs++ ];
        if ( namelen ) {
            memcpy( name, &comp->tree[ dataoffs ], namelen );
        }
        name[ namelen ] = '\0';
        vartype |= typeind & VARTYPEM_BASE;
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
    if ( !comp_create_node( comp, pnodeoffs, NT_EMPTYARRAYREF, UINT8_C(0), UINT16_C(3), data ) || *pnodeoffs == NODEOFFS_NONE ) {
        out_of_memory( comp );
        return false;
    }

    return true;
}

bool comp_eat_emptyarrayreflist( compiler_t* comp, uint16_t* pnodeoffs ) {
    /*
        NT_EMPTYARRAYREFLIST    empty array reference list
            data: none
            branches: the NT_EMPTYARRAYREF nodes
    */
    // empty-array-ref-list := empty-array-ref { TOK_COMMA empty-array-ref } .
    return comp_eat_list( comp, pnodeoffs, NT_EMPTYARRAYREFLIST, comp_eat_emptyarrayref, TOK_COMMA, "Array reference expected" );
}

bool comp_eat_numbasevarref( compiler_t* comp, uint16_t* pnodeoffs ) {
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
    static char data[257];
    data[0] = comp->currtok == TOK_NUMIDENT ? VARTYPEV_FLOAT : VARTYPEV_INT;
    snprintf( &data[1], 256U, "%s", comp->param );
    uint16_t datalen = UINT16_C(1) + ( (uint16_t) strlen( &data[1] ) );
    if ( !comp_fetchtok( comp ) ) {
        return false;
    }

    if ( !comp_create_node( comp, pnodeoffs, NT_NUMBASEVARREF, UINT8_C(0), datalen, data ) || *pnodeoffs == NODEOFFS_NONE ) {
        out_of_memory( comp );
        return false;
    }

    return true;
}

static bool comp_eat_varref( compiler_t* comp, uint16_t* pnodeoffs, uint8_t basetype,
    bool (*baseeater)( compiler_t*, uint16_t* ), uint8_t nodetype ) {

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
    uint16_t varnodeoffs = NODEOFFS_NONE;
    if ( !baseeater( comp, &varnodeoffs ) || varnodeoffs == NODEOFFS_NONE ) {
        return false;
    }
    uint16_t arraysubnode = NODEOFFS_NONE, arrayindexnode;
    if ( !comp_eat_arraysub( comp, &arraysubnode ) || varnodeoffs == NODEOFFS_NONE ) {
        arrayindexnode = NODEOFFS_NONE;
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
        if ( comp->tree[ arraysubnode ] != NT_ARRAYSUB ) {
NOSUB:      comp_error( comp, "Internal error (array subscript expected)" );
            return false;
        }
        uint16_t branch = EXTRACT16( comp, arraysubnode + 4U );
        if ( branch == NODEOFFS_NONE ) {
            goto NOSUB;
        }
        arrayindexnode = EXTRACT16( comp, branch );
        if ( arrayindexnode == NODEOFFS_NONE ) {
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
    uint16_t datalen  = EXTRACT16( comp, varnodeoffs + 2U );
    uint16_t dataoffs = varnodeoffs + UINT16_C(8);
    static char name[256]; name[0] = '\0';
    uint8_t vartype = arraysubnode != NODEOFFS_NONE ? VARTYPEF_ARRAY : UINT8_C(0);
    if ( comp->tree[ varnodeoffs ] == basetype ) {
        if ( datalen < UINT16_C(2) || datalen >= UINT16_C(256) ) {
            comp_error( comp, "Internal error (fault in variable reference)" );
            return false;
        }
        // data:
        //    - 1 byte of type indicator, n bytes of name
        uint8_t typeind = comp->tree[ dataoffs++ ];
        uint8_t namelen = datalen - UINT16_C(1);
        if ( namelen ) {
            memcpy( name, &comp->tree[ dataoffs ], namelen );
        }
        name[ namelen ] = '\0';
        vartype |= typeind & VARTYPEM_BASE;
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

    if ( arrayindexnode != NODEOFFS_NONE ) {
        ok = comp_create_node( comp, pnodeoffs, nodetype, UINT8_C(1), UINT8_C(3), data, (int) arrayindexnode );
    } else {
        ok = comp_create_node( comp, pnodeoffs, nodetype, UINT8_C(0), UINT8_C(3), data );
    }
    if ( !ok || *pnodeoffs == NODEOFFS_NONE ) {
        out_of_memory( comp );
        return false;
    }

    return true;
}

bool comp_eat_numvarref( compiler_t* comp, uint16_t* pnodeoffs ) {
    return comp_eat_varref( comp, pnodeoffs, NT_NUMBASEVARREF, comp_eat_numbasevarref, NT_NUMVARREF );
}

bool comp_eat_strbasevarref( compiler_t* comp, uint16_t* pnodeoffs ) {
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
    static char data[257];
    data[0] = VARTYPEV_STR;
    snprintf( &data[1], 256U, "%s", comp->param );
    uint16_t datalen = UINT16_C(1) + ( (uint16_t) strlen( &data[1] ) );
    if ( !comp_fetchtok( comp ) ) {
        return false;
    }

    if ( !comp_create_node( comp, pnodeoffs, NT_STRBASEVARREF, UINT8_C(0), datalen, data ) || *pnodeoffs == NODEOFFS_NONE ) {
        out_of_memory( comp );
        return false;
    }

    return true;
}

bool comp_eat_strvarref( compiler_t* comp, uint16_t* pnodeoffs ) {
    return comp_eat_varref( comp, pnodeoffs, NT_STRBASEVARREF, comp_eat_strbasevarref, NT_STRVARREF );
}

bool comp_eat_anybasevarref( compiler_t* comp, uint16_t* pnodeoffs ) {
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
    static char data[257];
    data[0] = VARTYPEV_FLOAT;
    if ( comp->currtok == TOK_STRIDENT ) {
        data[0] = VARTYPEV_STR;
    } else if ( comp->currtok == TOK_INTIDENT ) {
        data[0] = VARTYPEV_INT;
    }
    snprintf( &data[1], 256U, "%s", comp->param );
    uint16_t datalen = UINT16_C(1) + ( (uint16_t) strlen( &data[1] ) );
    if ( !comp_fetchtok( comp ) ) {
        return false;
    }

    uint8_t nodetype = data[0] == VARTYPEV_STR ? NT_STRBASEVARREF : NT_NUMBASEVARREF;

    if ( !comp_create_node( comp, pnodeoffs, nodetype, UINT8_C(0), datalen, data ) || *pnodeoffs == NODEOFFS_NONE ) {
        out_of_memory( comp );
        return false;
    }

    return true;
}

bool comp_eat_numlit( compiler_t* comp, uint16_t* pnodeoffs ) {
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
    if ( !comp_create_node( comp, pnodeoffs, NT_NUMLIT, UINT8_C(0), UINT16_C(8), data ) || *pnodeoffs == NODEOFFS_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_strlit( compiler_t* comp, uint16_t* pnodeoffs ) {
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
    static uint8_t data[257];
    data[0] = comp->currtok;
    size_t len = strlen( comp->param );
    if ( len ) {
        memcpy( &data[1], comp->param, len );
    }
    if ( !comp_fetchtok( comp ) ) {
        return false;
    }
    if ( !comp_create_node( comp, pnodeoffs, NT_STRLIT, UINT8_C(0), (uint16_t)( 1U + len ), data ) ||
        *pnodeoffs == NODEOFFS_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_strlits( compiler_t* comp, uint16_t* pnodeoffs ) {
    // str-lits := str-lit { str-lit } .
    /*
        NT_STRLITS      string literals
            data: none
            branches:
                - list of string literals (NT_STRLIT)
    */
    return comp_eat_list( comp, pnodeoffs, NT_STRLITS, comp_eat_strlit, TOK_EOL, "String literal expected" );
}

bool comp_eat_numusrfnname( compiler_t* comp, uint16_t* pnodeoffs ) {
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
    static char data[257];
    data[0] = tok == TOK_NUMIDENT ? VARTYPEV_FLOAT : VARTYPEV_INT;
    snprintf( &data[1], 256U, "%s", comp->param );
    uint16_t datalen = (uint16_t)( 1U + strlen( &data[1] ) );

    if ( !comp_create_node( comp, pnodeoffs, NT_NUMUSRFNNAME, UINT8_C(0), datalen, data ) || *pnodeoffs == NODEOFFS_NONE ) {
        out_of_memory( comp );
        return false;
    }

    return true;
}

bool comp_eat_strusrfnname( compiler_t* comp, uint16_t* pnodeoffs ) {
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
    static char data[257];
    data[0] = VARTYPEV_STR;
    snprintf( &data[1], 256U, "%s", comp->param );
    uint16_t datalen = (uint16_t)( 1U + strlen( &data[1] ) );

    if ( !comp_create_node( comp, pnodeoffs, NT_STRUSRFNNAME, UINT8_C(0), datalen, data ) || *pnodeoffs == NODEOFFS_NONE ) {
        out_of_memory( comp );
        return false;
    }

    return true;
}

bool comp_eat_usrfnarg( compiler_t* comp, uint16_t* pnodeoffs ) {
    // usr-fn-arg := any-base-var-ref [ TOK_LPAREN TOK_RPAREN ] .
    /*
        NT_USRFNARG         user function argument
            data:
                - 1 byte of type indicator
                - n bytes of name (NUL-terminated)
    */
    uint16_t namenode = NODEOFFS_NONE;
    if ( !comp_eat_anybasevarref( comp, &namenode ) || namenode == NODEOFFS_NONE ) {
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
    uint8_t nodetype = comp->tree[ namenode ];
    if ( nodetype != NT_NUMBASEVARREF && nodetype != NT_STRBASEVARREF ) {
UNEXP:  comp_error( comp, "Internal error (unexpected node type)" );
        return false;
    }
    uint16_t datalen = EXTRACT16( comp, namenode + 2U );
    if ( datalen < 2U || datalen >= 256U ) {
        goto UNEXP;
    }
    uint16_t dataoffs = namenode + UINT16_C(8);
    static char name[257];
    name[0] = comp->tree[ dataoffs ] & VARTYPEM_BASE;
    if ( is_array ) {
        name[0] |= VARTYPEF_ARRAY;
    }
    memcpy( &name[1], &comp->tree[ dataoffs + 1U ], datalen - 1U );
    name[ 1U + ( datalen - 1U ) ] = '\0';
    ++datalen;
    if ( !comp_create_node( comp, pnodeoffs, NT_USRFNARG, UINT8_C(0), datalen, name ) || *pnodeoffs == NODEOFFS_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_usrfnarglist( compiler_t* comp, uint16_t* pnodeoffs ) {
    // usr-fn-arg-list := usr-fn-arg { TOK_COMMA usr-fn-arg } .
    /*
    NT_USERFNARGLIST    user function argument list
        branches:
            - 2 or more branches of user function argument declarations
        immediate processing:
            - generated only if there are more than 2 arguments
    */
    return comp_eat_list( comp, pnodeoffs, NT_USERFNARGLIST, comp_eat_usrfnarg, TOK_COMMA, "User function argument expected" );
}

bool comp_eat_anyusrfnname( compiler_t* comp, uint16_t* pnodeoffs ) {
    // any-usr-fn-name := num-usr-fn-name | str-usr-fn-name .
    if ( comp_eat_numusrfnname( comp, pnodeoffs ) && *pnodeoffs != NODEOFFS_NONE ) {
        return true;
    }
    if ( comp_eat_strusrfnname( comp, pnodeoffs ) && *pnodeoffs != NODEOFFS_NONE ) {
        return true;
    }
    return false;
}

bool comp_eat_singlelineusrfnbody( compiler_t* comp, uint16_t* pnodeoffs ) {
    // singleline-usr-fn-body := TOK_EQ expr .
    if ( comp->currtok != TOK_EQ ) {
        return false;
    }
    uint16_t exprnode = NODEOFFS_NONE;
    if ( !comp_fetchtok( comp ) || !comp_eat_expr( comp, &exprnode ) || exprnode == NODEOFFS_NONE ) {
        comp_error( comp, "Expression expected" );
        return false;
    }
    /*
        NT_SINGLELINEUSRFNBODY  single-line user function body
            branches:
                - 1 branch of expression
    */
    if ( !comp_create_node( comp, pnodeoffs, NT_SINGLELINEUSERFNBODY, UINT8_C(1), UINT8_C(0), 0, (int) exprnode ) ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_multilineusrfnbody( compiler_t* comp, uint16_t* pnodeoffs ) {
    // multiline-usr-fn-body := TOK_EOL stmt-lines TOK_END TOK_FN .
    if ( comp->currtok != TOK_EOL ) {
        return false;
    }
    uint16_t stmtlines = NODEOFFS_NONE;
    if ( !comp_fetchtok( comp ) || !comp_eat_stmtlines( comp, &stmtlines ) || stmtlines == NODEOFFS_NONE ) {
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
    if ( !comp_create_node( comp, pnodeoffs, NT_MULTILINEUSERFNBODY, UINT8_C(1), UINT8_C(0), 0, (int) stmtlines ) ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_usrfnbody( compiler_t* comp, uint16_t* pnodeoffs ) {
    // usr-fn-body := singleline-usr-fn-body | multiline-usr-fn-body .
    // [ NT_USRFNBODY - not generated ]
    if ( comp_eat_singlelineusrfnbody( comp, pnodeoffs ) && *pnodeoffs != NODEOFFS_NONE ) {
        return true;
    }
    if ( comp_eat_multilineusrfnbody( comp, pnodeoffs ) && *pnodeoffs != NODEOFFS_NONE ) {
        return true;
    }
    return false;
}

typedef struct _cgbargs_t {
    compiler_t* comp;
    uint16_t*   branches;
    size_t      maxbranches;
    size_t      numbranches;
} cgbargs_t;

static bool cgb_worker( void* userdata, uint16_t nodepos ) {
    cgbargs_t* args = (cgbargs_t*) userdata;
    if ( args->numbranches >= args->maxbranches ) {
        comp_error( args->comp, "Too many arguments" );
        return false;
    }
    args->branches[ args->numbranches++ ] = nodepos;
    return true;
}

static bool comp_gather_branches( compiler_t* comp, uint16_t nodeoffs, uint8_t listtype, uint16_t* branches, size_t maxbranches,
    size_t* pnumbranches ) {
    //  <nodetype.8> <numbranches.8> <datalen.16> <firstbranch.16> <lastbranch.16> <data...>
    if ( comp->tree[ nodeoffs ] == listtype ) {
        cgbargs_t args = { comp, branches, maxbranches, 0 };
        if ( !comp_node_iter_branches( comp, nodeoffs, &args, cgb_worker ) ) {
            return false;
        }
        *pnumbranches = args.numbranches;
    } else if ( maxbranches >= 1U ) {
        branches[0] = nodeoffs;
        *pnumbranches = 1U;
    } else {
        comp_error( comp, "Internal error (invalid argument)" );
        return false;
    }
    return true;
}

static bool comp_get_func_params( compiler_t* comp, uint16_t paramoffs[MAXPARAM], size_t numparams, usrparam_t params[MAXPARAM] ) {
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
        uint16_t nodepos = paramoffs[i];
        if ( comp->tree[ nodepos ] != NT_USRFNARG ) {
UNEXP:      comp_error( comp, "Internal error (unexpected parameter node)" );
            return false;
        }
        uint16_t datalen = EXTRACT16( comp, nodepos + 2U );
        if ( datalen < 2U ) {
            goto UNEXP;
        }
        uint16_t datapos = nodepos + UINT16_C(8);
        params[i].paramtype = comp->tree[ datapos ];
        params[i].paramname = (char*)( &comp->tree[ datapos + 1U ] );
    }
    return true;
}

bool comp_eat_usrfndecl( compiler_t* comp, uint16_t* pnodeoffs ) {
    // usr-fn-decl := any-usr-fn-name [ TOK_LPAREN [ usr-fn-arg-list ] TOK_RPAREN ] usr-fn-body .
    uint16_t fnname = NODEOFFS_NONE;
    if ( !comp_eat_anyusrfnname( comp, &fnname ) || fnname == NODEOFFS_NONE ) {
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
    if ( comp->tree[ fnname ] != NT_NUMUSRFNNAME && comp->tree[ fnname ] != NT_STRUSRFNNAME ) {
UNEXP:  comp_error( comp, "Internal error (unexpected name node)" );
        return false;
    }
    uint16_t datalen = EXTRACT16( comp, fnname + 2U );
    if ( datalen < 2U || datalen > 256U ) {
        goto UNEXP;
    }
    uint16_t dataoffs = fnname + UINT16_C(8);
    uint8_t functype = ( comp->tree[ dataoffs ] & VARTYPEM_BASE ) | VARTYPEF_FUNC;
    static char name[ 256U ];
    memcpy( name, &comp->tree[ dataoffs + 1U ], datalen - 1U );
    name[ datalen - 1U ] = '\0';

    // get arguments
    uint16_t fnarglist = NODEOFFS_NONE;
    uint16_t paramoffs[MAXPARAM];
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
    uint16_t fnbody = NODEOFFS_NONE;
    if ( !comp_eat_usrfnbody( comp, &fnbody ) || fnbody == NODEOFFS_NONE ) {
        comp_error( comp, "User function body expected" );
        return false;
    }

    // create variable
    uint16_t varoffs = VAROFFS_NONE;
    if ( !vmem_create_var( &comp->rt->varmem, functype, name, UINT8_C(0), 0, (uint8_t) numparams, params, &varoffs ) ||
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

    if ( !comp_create_node( comp, pnodeoffs, NT_USRFNDECL, UINT8_C(1), UINT16_C(3), data, (int) fnbody ) ||
        *pnodeoffs == NODEOFFS_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

static bool comp_eat_usrfncall( compiler_t* comp, uint16_t* pnodeoffs, uint8_t nodetype,
    uint8_t nametype, bool (*eater_func)( compiler_t*, uint16_t* ) ) {
    // num-usr-fn-call := num-usr-fn-name [ TOK_LPAREN [ expr-list ] TOK_RPAREN ] .
    // str-usr-fn-call := str-usr-fn-name [ TOK_LPAREN [ expr-list ] TOK_RPAREN ] .
    uint16_t fnname = NODEOFFS_NONE;
    if ( !eater_func( comp, &fnname ) || fnname == NODEOFFS_NONE ) {
        return false;
    }
    uint16_t exprlist = NODEOFFS_NONE;
    if ( comp->currtok == TOK_LPAREN && comp_fetchtok( comp ) ) {
        comp_eat_exprlist( comp, &exprlist );
        if ( comp->currtok != TOK_RPAREN || !comp_fetchtok( comp ) ) {
            comp_error( comp, "Closing parenthesis ')' expected" );
        }
    }

    //  <nodetype.8> <numbranches.8> <datalen.16> <firstbranch.16> <lastbranch.16> <data...>
    uint8_t nametype2 = comp->tree[ fnname ];
    if ( nametype2 != nametype ) {
UNEXP:  comp_error( comp, "Internal error (unexpected name type)" );
        return false;
    }
    uint16_t datalen  = EXTRACT16( comp, fnname + 2U );
    uint16_t dataoffs = fnname + UINT16_C(8);
    if ( datalen < 2U || datalen > 256U ) {
        goto UNEXP;
    }
    // data is type + name
    uint8_t functype = ( comp->tree[ dataoffs ] & VARTYPEM_BASE ) | VARTYPEF_FUNC;
    static char name[256];
    memcpy( name, &comp->tree[ dataoffs + 1U ], datalen - 1U );
    name[ datalen - 1U ] = '\0';

    // lookup variable
    uint16_t varoffs = VAROFFS_NONE;
    if ( !vmem_lookup_var( &comp->rt->varmem, functype, name, &varoffs ) || varoffs == VAROFFS_NONE ) {
UNDEF:  comp_error( comp, "Undefined function" );
        return false;
    }

    // read true variable offset from index
    uint16_t varoffs2 = VEXTRACT16( &comp->rt->varmem, varoffs );
    if ( varoffs2 == VAROFFS_NONE ) {
        goto UNDEF;
    }

    // <size.16> <type.8> <namelen.8> <name...> [ <numdims.8> <arraydims...> | <numargs> <argdesc...> ] <data...>
    uint8_t  namelen = comp->rt->varmem.vars[ varoffs2 + 3U ];
    uint16_t argoffs = varoffs2 + UINT16_C(4) + namelen;
    uint8_t  numargs = comp->rt->varmem.vars[ argoffs++ ];

    // check argument count
    if ( numargs == UINT8_C(0) && exprlist == NODEOFFS_NONE ) {
        // OK, no arguments
    } else if ( numargs == UINT8_C(0) || exprlist == NODEOFFS_NONE ) {
MISM:   comp_error( comp, "Argument count mismatch" );
        return false;
    } else {
        //  <nodetype.8> <numbranches.8> <datalen.16> <firstbranch.16> <lastbranch.16> <data...>
        if ( comp->tree[ exprlist ] == NT_EXPRLIST ) {
            // several branches
            uint8_t numbranches = comp->tree[ exprlist + 1U ];
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

    if ( !comp_create_node( comp, pnodeoffs, nodetype, UINT8_C(1), UINT16_C(3), data, (int) exprlist ) ||
        *pnodeoffs == NODEOFFS_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_numusrfncall( compiler_t* comp, uint16_t* pnodeoffs ) {
    // num-usr-fn-call := num-usr-fn-name TOK_LPAREN expr-list TOK_RPAREN .
    return comp_eat_usrfncall( comp, pnodeoffs, NT_NUMUSRFNCALL, NT_NUMUSRFNNAME, comp_eat_numusrfnname );
}

bool comp_eat_strusrfncall( compiler_t* comp, uint16_t* pnodeoffs ) {
    // str-usr-fn-call := str-usr-fn-name TOK_LPAREN expr-list TOK_RPAREN .
    return comp_eat_usrfncall( comp, pnodeoffs, NT_STRUSRFNCALL, NT_STRUSRFNNAME, comp_eat_strusrfnname );
}

bool comp_eat_sysnumfunc( compiler_t* comp, uint16_t* pnodeoffs ) {
    // sys-num-func := TOK_ASC | TOK_BIN | TOK_QUA | TOK_OCT | TOK_DEC | TOK_HEX | TOK_VAL | TOK_FRE .

    switch ( comp->currtok ) {
        case TOK_ASC: case TOK_BIN: case TOK_QUA: case TOK_OCT: case TOK_DEC: case TOK_HEX: case TOK_VAL: case TOK_FRE:
            break;
        default:
            return false;
    }
    uint8_t tok = comp->currtok;
    if ( !comp_fetchtok( comp ) ) {
        return false;
    }

    /*
        NT_SYSNUMFUNC       numeric system function
            data:
                - 1 byte of function token (like TOK_VAL)
    */

    if ( !comp_create_node( comp, pnodeoffs, NT_SYSNUMFUNC, UINT8_C(0), UINT16_C(1), &tok ) ) {
        out_of_memory( comp );
        return false;
    }

    return true;
}

bool comp_eat_sysstrfunc( compiler_t* comp, uint16_t* pnodeoffs ) {
    // sys-str-func-name := TOK_LEFT | TOK_MID | TOK_RIGHT | TOK_STR .
    // sys-str-func := sys-str-func-name TOK_DOLLAR .
    switch ( comp->currtok ) {
        case TOK_LEFT: case TOK_MID: case TOK_RIGHT: case TOK_STR:
            break;
        default:
            return false;
    }
    uint8_t tok = comp->currtok;
    if ( !comp_fetchtok( comp ) ) {
        return false;
    }
    if ( comp->currtok != TOK_STRING || !comp_fetchtok( comp ) ) {
        comp_error( comp, "String sigil '$' expected" );
        return false;
    }

    /*
        NT_SYSSTRFUNC       string system function
            data:
                - 1 byte of function token (like TOK_VAL)
    */

    if ( !comp_create_node( comp, pnodeoffs, NT_SYSSTRFUNC, UINT8_C(0), UINT16_C(1), &tok ) ) {
        out_of_memory( comp );
        return false;
    }

    return true;
}

static bool comp_eat_sysfuncargcall( compiler_t* comp, uint16_t* pnodeoffs, bool (*eater)( compiler_t*, uint16_t* ),
    uint8_t nodetype, uint8_t eatentype ) {
    // sys-num-fn-arg-call := sys-num-func TOK_LPAREN expr-list TOK_RPAREN .
    // sys-str-fn-arg-call := sys-str-func TOK_LPAREN expr-list TOK_RPAREN .
    uint16_t eatenoffs = NODEOFFS_NONE;
    if ( !eater( comp, &eatenoffs ) || eatenoffs == NODEOFFS_NONE ) {
        return false;
    }
    //  <nodetype.8> <numbranches.8> <datalen.16> <firstbranch.16> <lastbranch.16> <data...>
    if ( comp->tree[ eatenoffs ] != eatentype ) {
UNEXP:  comp_error( comp, "Internal error (unexpected argument)" );
        return false;
    }
    uint16_t datalen = EXTRACT16( comp, eatenoffs + 2U );
    if ( datalen != 1U ) {
        goto UNEXP;
    }
    uint16_t dataoffs = eatenoffs + UINT16_C(8);
    uint8_t tok = comp->tree[ dataoffs ];

    // read argument list
    if ( comp->currtok != TOK_LPAREN || !comp_fetchtok( comp ) ) {
        comp_error( comp, "Opening parenthesis '(' expected" );
        return false;
    }
    uint16_t exprlist = NODEOFFS_NONE;
    if ( !comp_eat_exprlist( comp, &exprlist ) || exprlist == NODEOFFS_NONE ) {
        comp_error( comp, "Expression list expected" );
        return false;
    }
    if ( comp->currtok != TOK_RPAREN || !comp_fetchtok( comp ) ) {
        comp_error( comp, "Closing parenthesis ')' expected" );
        return false;
    }

    /*
        NT_SYSNUMFUNCARGCALL    numeric system function call with arguments
        NT_SYSSTRFUNCARGCALL    string system function call with arguments
            data:
                - 1 byte of function token (like TOK_VAL)
            branches:
                - 1 branch of expression list
    */

    // generate node
    if ( !comp_create_node( comp, pnodeoffs, nodetype, UINT8_C(1), UINT16_C(1), &tok, (int) exprlist ) ||
        *pnodeoffs == NODEOFFS_NONE ) {
        out_of_memory( comp );
        return false;
    }

    return true;
}

bool comp_eat_sysnumfuncargcall( compiler_t* comp, uint16_t* pnodeoffs ) {
    // sys-num-fn-arg-call := sys-num-func TOK_LPAREN expr-list TOK_RPAREN .
    return comp_eat_sysfuncargcall( comp, pnodeoffs, comp_eat_sysnumfunc, NT_SYSNUMFUNCARGCALL, NT_SYSNUMFUNC );
}

bool comp_eat_sysstrfuncargcall( compiler_t* comp, uint16_t* pnodeoffs ) {
    // sys-str-fn-arg-call := sys-str-func TOK_LPAREN expr-list TOK_RPAREN .
    return comp_eat_sysfuncargcall( comp, pnodeoffs, comp_eat_sysstrfunc, NT_SYSSTRFUNCARGCALL, NT_SYSSTRFUNC );
}

bool comp_eat_sysnoargnumcall( compiler_t* comp, uint16_t* pnodeoffs ) {
    // sys-noarg-num-name := TOK_TI .
    // sys-noarg-num := sys-noarg-num-name .
    // sys-noarg-num-call := sys-no-arg-num .
    uint8_t tok = comp->currtok;
    if ( tok != TOK_TI || !comp_fetchtok( comp ) ) {
        return false;
    }
    if ( !comp_create_node( comp, pnodeoffs, NT_SYSNOARGNUMCALL, UINT8_C(0), UINT16_C(1), &tok ) || *pnodeoffs == NODEOFFS_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_sysnoargstrcall( compiler_t* comp, uint16_t* pnodeoffs ) {
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
    if ( !comp_create_node( comp, pnodeoffs, NT_SYSNOARGSTRCALL, UINT8_C(0), UINT16_C(1), &tok ) || *pnodeoffs == NODEOFFS_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_numfunccall( compiler_t* comp, uint16_t* pnodeoffs ) {
    // num-func-call := num-usr-fn-call | sys-num-fn-arg-call | sys-noarg-num-call .
    if ( comp_eat_numusrfncall( comp, pnodeoffs ) && *pnodeoffs != NODEOFFS_NONE ) {
        return true;
    }
    if ( comp_eat_sysnumfuncargcall( comp, pnodeoffs ) && *pnodeoffs != NODEOFFS_NONE ) {
        return true;
    }
    if ( comp_eat_sysnoargnumcall( comp, pnodeoffs ) && *pnodeoffs != NODEOFFS_NONE ) {
        return true;
    }
    return false;
}

bool comp_eat_strfunccall( compiler_t* comp, uint16_t* pnodeoffs ) {
    // str-func-call := str-usr-fn-call | sys-str-fn-arg-call | sys-noarg-str-call .
    if ( comp_eat_strusrfncall( comp, pnodeoffs ) && *pnodeoffs != NODEOFFS_NONE ) {
        return true;
    }
    if ( comp_eat_sysstrfuncargcall( comp, pnodeoffs ) && *pnodeoffs != NODEOFFS_NONE ) {
        return true;
    }
    if ( comp_eat_sysnoargstrcall( comp, pnodeoffs ) && *pnodeoffs != NODEOFFS_NONE ) {
        return true;
    }
    return false;
}

bool comp_eat_strbaseexpr( compiler_t* comp, uint16_t* pnodeoffs ) {
    // str-base-expr := str-var-ref | str-lits | str-func-call .
    if ( comp_eat_strvarref( comp, pnodeoffs ) && *pnodeoffs != NODEOFFS_NONE ) {
        return true;
    }
    if ( comp_eat_strlits( comp, pnodeoffs ) && *pnodeoffs != NODEOFFS_NONE ) {
        return true;
    }
    if ( comp_eat_strfunccall( comp, pnodeoffs ) && *pnodeoffs != NODEOFFS_NONE ) {
        return true;
    }
    return false;
}

bool comp_eat_straddexpr( compiler_t* comp, uint16_t* pnodeoffs ) {
    // str-add-expr := str-base-expr { TOK_PLUS str-base-expr } .
    return comp_eat_list( comp, pnodeoffs, NT_STRADDEXPR, comp_eat_strbaseexpr, TOK_PLUS, "String expression expected" );
}

bool comp_eat_strexpr( compiler_t* comp, uint16_t* pnodeoffs ) {
    // str-expr := str-add-expr .
    return comp_eat_straddexpr( comp, pnodeoffs );
}

bool comp_eat_numsubexpr( compiler_t* comp, uint16_t* pnodeoffs ) {
    // num-sub-expr := TOK_LPAREN num-expr TOK_RPAREN .
    if ( comp->currtok != TOK_LPAREN || !comp_fetchtok( comp ) ) {
        return false;
    }
    uint16_t subexpr = NODEOFFS_NONE;
    if ( !comp_eat_numexpr( comp, &subexpr ) || subexpr == NODEOFFS_NONE ) {
        comp_error( comp, "Numeric expression expected" );
        return false;
    }
    if ( comp->currtok != TOK_RPAREN || !comp_fetchtok( comp ) ) {
        comp_error( comp, "Closing parenthesis ')' expected" );
        return false;
    }
    *pnodeoffs = subexpr;
    return true;
}

bool comp_eat_numbaseexpr( compiler_t* comp, uint16_t* pnodeoffs ) {
    // num-base-expr := num-var-ref | num-lit | num-func-call | num-sub-expr .
    if ( comp_eat_numvarref( comp, pnodeoffs ) && *pnodeoffs != NODEOFFS_NONE ) {
        return true;
    }
    if ( comp_eat_numlit( comp, pnodeoffs ) && *pnodeoffs != NODEOFFS_NONE ) {
        return true;
    }
    if ( comp_eat_numfunccall( comp, pnodeoffs ) && *pnodeoffs != NODEOFFS_NONE ) {
        return true;
    }
    if ( comp_eat_numsubexpr( comp, pnodeoffs ) && *pnodeoffs != NODEOFFS_NONE ) {
        return true;
    }
    return false;
}

bool comp_eat_list2( compiler_t* comp, uint16_t* pnodeoffs, uint8_t nodetype, comp_eatfn_t element_eater, const uint8_t* septoks,
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
    uint16_t expr1 = NODEOFFS_NONE;
    if ( !element_eater( comp, &expr1 ) || expr1 == NODEOFFS_NONE ) {
        if ( forceexpr ) {
            // make sure first expression has not been read
            comp_pop_context( comp );
        }
        return false;
    }
    uint16_t nodeoffs = NODEOFFS_NONE;
    for (;;) {
        bool mandatory = false; uint16_t expr2 = NODEOFFS_NONE;
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
        if ( !element_eater( comp, &expr2 ) || expr2 == NODEOFFS_NONE ) {
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
        uint16_t operand = NODEOFFS_NONE;
        if ( !comp_create_node( comp, &operand, NT_OPERATOR, UINT8_C(1), UINT16_C(1), &septok, (int) expr2 ) ||
            operand == NODEOFFS_NONE ) {
OOM:        if ( forceexpr ) {
                // commit to changes before generating error
                comp_commit_context( comp );
            }
            out_of_memory( comp );
            return false;
        }
        // we have now a new branch; first see if we already have a node or need to create one
        if ( nodeoffs == NODEOFFS_NONE ) {
            if ( !comp_create_node( comp, &nodeoffs, nodetype, UINT8_C(2), UINT16_C(0), 0, (int) expr1, (int) operand ) ||
                 nodeoffs == NODEOFFS_NONE ) {
                // failed to create node: cancel operation
                goto OOM;
            }
        } else {
            // the node already exists: add a new branch
            if ( !comp_add_branch( comp, nodeoffs, operand ) ) {
                // something went wrong: cancel
                goto OOM;
            }
        }
        // successful, continue, unless only one additional operand is allowed
        if ( oneoperator ) {
            break;
        }
    }
    if ( nodeoffs == NODEOFFS_NONE && forceexpr ) {
        // rewind so that the first expression has not been read
        comp_pop_context( comp );
        return false;
    } else if ( forceexpr ) {
        // all fine, commit to it
        comp_commit_context( comp );
    }
    // either return node with what we already have, or just the first branch
    *pnodeoffs = nodeoffs != NODEOFFS_NONE ? nodeoffs : expr1;
    return true;
}

bool comp_eat_numunaryex( compiler_t* comp, uint16_t* pnodeoffs ) {
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
    uint16_t baseexpr = NODEOFFS_NONE;
    if ( !comp_eat_numbaseexpr( comp, &baseexpr ) || baseexpr == NODEOFFS_NONE ) {
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
        *pnodeoffs = baseexpr;
        return true;
    }
    // operator present: create new node
    if ( !comp_create_node( comp, pnodeoffs, NT_NUMUNARYEX, UINT8_C(1), UINT16_C(1), &tok, (int) baseexpr ) ||
        *pnodeoffs == NODEOFFS_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_nummultex( compiler_t* comp, uint16_t* pnodeoffs ) {
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
    return comp_eat_list2( comp, pnodeoffs, NT_NUMMULTEX, comp_eat_numunaryex, septoks, "Multiplication expression expected",
        false, false );
}

bool comp_eat_numaddex( compiler_t* comp, uint16_t* pnodeoffs ) {
    // num-add-op := TOK_PLUS | TOK_MINUS .
    // num-add-ex := num-mult-ex { num-add-op num-mult-ex } .
    static const uint8_t septoks[] = { TOK_PLUS, TOK_MINUS, 0 };
    /*
        NT_NUMADDEX   numeric addition expression
            branches:
                - 1 branch of first operand
                - at least 1 branch of NT_OPERATOR
            immediate processing:
                - generated only if a multiplication operator was used
    */
    return comp_eat_list2( comp, pnodeoffs, NT_NUMADDEX, comp_eat_nummultex, septoks, "Add expression expected",
        false, false );
}

bool comp_eat_numshiftex( compiler_t* comp, uint16_t* pnodeoffs ) {
    // num-shift-op := TOK_LSHIFT | TOK_RSHIFT .
    // num-shift-ex := num-add-ex [ num-shift-op num-add-ex ] .
    static const uint8_t septoks[] = { TOK_LSHIFT, TOK_RSHIFT, 0 };
    /*
        NT_NUMSHIFTEX   numeric shift expression
            branches:
                - 1 branch of first operand
                - at least 1 branch of NT_OPERATOR
            immediate processing:
                - generated only if a multiplication operator was used
    */
    return comp_eat_list2( comp, pnodeoffs, NT_NUMSHIFTEX, comp_eat_numaddex, septoks, "Shift expression expected",
        true, false );
}

bool comp_eat_numcmpex( compiler_t* comp, uint16_t* pnodeoffs ) {
    // num-cmp-op := TOK_EQ | TOK_NE | TOK_LE | TOK_GE | TOK_LT | TOK_GT .
    // num-cmp-ex := num-shift-ex [ num-cmp-op num-shift-ex ] |
    //               str-expr num-cmp-op str-expr .
    static const uint8_t septoks[] = { TOK_EQ, TOK_NE, TOK_LE, TOK_GE, TOK_LT, TOK_GT, 0 };
    // numeric version
    /*
        NT_NUMCMPEX   numeric shift expression
            branches:
                - 1 branch of first operand
                - at least 1 branch of NT_OPERATOR
            immediate processing:
                - generated only if a multiplication operator was used
    */
    if ( comp_eat_list2( comp, pnodeoffs, NT_NUMCMPEX, comp_eat_numshiftex, septoks, "Comparison expression expected",
        true, false ) && *pnodeoffs != NODEOFFS_NONE ) {
        return true;
    }
    if ( comp_eat_list2( comp, pnodeoffs, NT_NUMCMPEX, comp_eat_strexpr, septoks, "Comparison expression expected",
        true, true ) && *pnodeoffs != NODEOFFS_NONE ) {
        return true;
    }
    return false;
}

bool comp_eat_numandex( compiler_t* comp, uint16_t* pnodeoffs ) {
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
    return comp_eat_list2( comp, pnodeoffs, NT_NUMANDEX, comp_eat_numcmpex, septoks, "AND expression expected",
        false, false );
}

bool comp_eat_numorex( compiler_t* comp, uint16_t* pnodeoffs ) {
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
    return comp_eat_list2( comp, pnodeoffs, NT_NUMOREX, comp_eat_numandex, septoks, "OR expression expected",
        false, false );
}

bool comp_eat_numexpr( compiler_t* comp, uint16_t* pnodeoffs ) {
    // num-expr := num-or-ex .
    // [ NT_NUMEXPR - not generated ]
    return comp_eat_numorex( comp, pnodeoffs );
}

bool comp_eat_expr( compiler_t* comp, uint16_t* pnodeoffs ) {
    // expr := num-expr | str-expr .
    // [ NT_EXPR - not generated ]
    if ( comp_eat_numexpr( comp, pnodeoffs ) && *pnodeoffs != NODEOFFS_NONE ) {
        return true;
    }
    if ( comp_eat_strexpr( comp, pnodeoffs ) && *pnodeoffs != NODEOFFS_NONE ) {
        return true;
    }
    return false;
}

bool comp_eat_savestmt( compiler_t* comp, uint16_t* pnodeoffs ) {
    // save-stmt := SAVE str-expr [ TOK_COMMA TOK_NUMIDENT ] .
    if ( comp->currtok != TOK_SAVE || !comp_fetchtok( comp ) ) {
        return false;
    }
    uint16_t strexpr = NODEOFFS_NONE;
    if ( !comp_eat_strexpr( comp, &strexpr ) || strexpr == NODEOFFS_NONE ) {
        comp_error( comp, "String expression expected" );
        return false;
    }
    static char saveopt[256];
    if ( comp->currtok == TOK_COMMA && comp_fetchtok( comp ) ) {
        if ( comp->currtok != TOK_NUMIDENT || !comp_fetchtok( comp ) ) {
            comp_error( comp, "SAVE options expected" );
            return false;
        }
        snprintf( saveopt, 256U, "%s", comp->param );
    } else {
        saveopt[0] = '\0';
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
    uint8_t saveoptlen = (uint8_t) strlen( saveopt );
    if ( !comp_create_node( comp, pnodeoffs, NT_SAVESTMT, UINT8_C(1), saveoptlen, saveopt, (int) strexpr ) ||
        *pnodeoffs == NODEOFFS_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_chanspec( compiler_t* comp, uint16_t* pnodeoffs ) {
    // chan-spec := TOK_LATTICE num-expr .
    if ( comp->currtok != TOK_LATTICE || !comp_fetchtok( comp ) ) {
        return false;
    }
    uint16_t numexpr = NODEOFFS_NONE;
    if ( !comp_eat_numexpr( comp, &numexpr ) || numexpr == NODEOFFS_NONE ) {
        comp_error( comp, "Numeric expression expected" );
        return false;
    }
    /*
        NT_CHANSPEC     channel specifier
            branches:
                - 1 branch of numeric expression
    */
    if ( !comp_create_node( comp, pnodeoffs, NT_CHANSPEC, UINT8_C(1), UINT16_C(0), 0, (int) numexpr ) ||
        *pnodeoffs == NODEOFFS_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_printsep( compiler_t* comp, uint16_t* pnodeoffs ) {
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
    if ( !comp_create_node( comp, pnodeoffs, NT_PRINTSEP, UINT8_C(0), UINT16_C(1), &tok ) || *pnodeoffs == NODEOFFS_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_printarg( compiler_t* comp, uint16_t* pnodeoffs ) {
    // print-arg := expr [ print-sep ] .
    uint16_t expr = NODEOFFS_NONE;
    if ( !comp_eat_expr( comp, &expr ) || expr == NODEOFFS_NONE ) {
        return false;
    }
    uint16_t sep = NODEOFFS_NONE;
    comp_eat_printsep( comp, &sep );
    /*
        NT_PRINTARG     print argument
            branches:
                - 1 branch of expression
                - 1 optional branch of separator
    */
    if ( !comp_create_node( comp, pnodeoffs, NT_PRINTARG, UINT8_C(2), UINT16_C(0), 0, (int) expr, (int) sep ) ||
        *pnodeoffs == NODEOFFS_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_printarglist( compiler_t* comp, uint16_t* pnodeoffs ) {
    // print-arg-list := print-arg { print-arg } .
    /*
        NT_PRINTARGLIST     print argument
            branches:
                - 2 or more branches of print arguments
            immediate processing:
                - generated only if there's more than one print argument
    */
    return comp_eat_list( comp, pnodeoffs, NT_PRINTARGLIST, comp_eat_printarg, TOK_EOL, "PRINT argument expected" );
}

bool comp_eat_printstmt( compiler_t* comp, uint16_t* pnodeoffs ) {
    // print-stmt := TOK_PRINT [ chan-spec TOK_COMMA ] [ print-arg-list ] .
    if ( comp->currtok != TOK_PRINT || !comp_fetchtok( comp ) ) {
        return false;
    }
    uint16_t chan = NODEOFFS_NONE;
    if ( comp_eat_chanspec( comp, &chan ) && chan != NODEOFFS_NONE ) {
        if ( comp->currtok != TOK_COMMA || !comp_fetchtok( comp ) ) {
            comp_error( comp, "Comma ',' expected" );
            return false;
        }
    }
    uint16_t args = NODEOFFS_NONE;
    comp_eat_printarglist( comp, &args );
    /*
        NT_PRINTSTMT    print statement
            branches:
                - 1 optional branch of channel info
                - 1 optional branch of print argument list
    */
    if ( !comp_create_node( comp, pnodeoffs, NT_PRINTSTMT, UINT8_C(2), UINT16_C(0), 0, (int) chan, (int) args ) ||
        *pnodeoffs == NODEOFFS_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_iostmt( compiler_t* comp, uint16_t* pnodeoffs ) {
    // io-stmt := save-stmt | print-stmt .
    // [ NT_IOSTMT - not generated ]
    if ( comp_eat_savestmt( comp, pnodeoffs ) && *pnodeoffs != NODEOFFS_NONE ) {
        return true;
    }
    if ( comp_eat_printstmt( comp, pnodeoffs ) && *pnodeoffs != NODEOFFS_NONE ) {
        return true;
    }
    return false;
}

bool comp_eat_numassign( compiler_t* comp, uint16_t* pnodeoffs ) {
    // num-assign := num-var-ref TOK_EQ num-expr .
    uint16_t varref = NODEOFFS_NONE;
    if ( !comp_eat_numvarref( comp, &varref ) || varref == NODEOFFS_NONE ) {
        return false;
    }
    if ( comp->currtok != TOK_EQ || !comp_fetchtok( comp ) ) {
        comp_error( comp, "Assignment operator '=' expected" );
        return false;
    }
    uint16_t numexpr = NODEOFFS_NONE;
    if ( !comp_eat_numexpr( comp, &numexpr ) || numexpr == NODEOFFS_NONE ) {
        comp_error( comp, "Numeric expression expected" );
        return false;
    }
    /*
        NT_NUMASSIGN    numeric assignment
            branches:
                - 1 branch of numeric variable reference
                - 1 branch of numeric expression
    */
    if ( !comp_create_node( comp, pnodeoffs, NT_NUMASSIGN, UINT8_C(2), UINT16_C(0), 0, (int) varref, (int) numexpr ) ||
        *pnodeoffs == NODEOFFS_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_strassign( compiler_t* comp, uint16_t* pnodeoffs ) {
    // str-assign := str-var-ref TOK_EQ str-expr .
    uint16_t varref = NODEOFFS_NONE;
    if ( !comp_eat_strvarref( comp, &varref ) || varref == NODEOFFS_NONE ) {
        return false;
    }
    if ( comp->currtok != TOK_EQ || !comp_fetchtok( comp ) ) {
        comp_error( comp, "Assignment operator '=' expected" );
        return false;
    }
    uint16_t strexpr = NODEOFFS_NONE;
    if ( !comp_eat_strexpr( comp, &strexpr ) || strexpr == NODEOFFS_NONE ) {
        comp_error( comp, "String expression expected" );
        return false;
    }
    /*
        NT_STRASSIGN    string assignment
            branches:
                - 1 branch of string variable reference
                - 1 branch of string expression
    */
    if ( !comp_create_node( comp, pnodeoffs, NT_STRASSIGN, UINT8_C(2), UINT16_C(0), 0, (int) varref, (int) strexpr ) ||
        *pnodeoffs == NODEOFFS_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_substrassign( compiler_t* comp, uint16_t* pnodeoffs ) {
    // substr-op := TOK_LEFT | TOK_MID | TOK_RIGHT .
    // substr-assign := substr-op TOK_STRING TOK_LPAREN expr-list TOK_RPAREN .
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
    uint16_t exprlist = NODEOFFS_NONE;
    if ( !comp_eat_exprlist( comp, &exprlist ) || exprlist == NODEOFFS_NONE ) {
        comp_error( comp, "Expression list expected" );
        return false;
    }
    if ( comp->currtok != TOK_RPAREN || !comp_fetchtok( comp ) ) {
        comp_error( comp, "Closing parenthesis ')' expected" );
        return false;
    }
    /*
        NT_SUBSTRASSIGN     substring assignment
            data:
                - 1 byte of substring operator
            branches:
                - 1 branch of expression list
    */
    if ( !comp_create_node( comp, pnodeoffs, NT_SUBSTRASSIGN, UINT8_C(1), UINT16_C(1), &tok, (int) exprlist ) ||
        *pnodeoffs == NODEOFFS_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_anyassign( compiler_t* comp, uint16_t* pnodeoffs ) {
    // any-assign := num-assign | substr-assign | str-assign .
    // [ NT_ANYASSIGN - not generated ]
    if ( comp_eat_numassign( comp, pnodeoffs ) && *pnodeoffs != NODEOFFS_NONE ) {
        return true;
    }
    if ( comp_eat_substrassign( comp, pnodeoffs ) && *pnodeoffs != NODEOFFS_NONE ) {
        return true;
    }
    if ( comp_eat_strassign( comp, pnodeoffs ) && *pnodeoffs != NODEOFFS_NONE ) {
        return true;
    }
    return false;
}

bool comp_eat_assignlist( compiler_t* comp, uint16_t* pnodeoffs ) {
    // assign-list := any-assign { TOK_COMMA any-assign } .
    /*
        NT_ASSIGNLIST       assignment list
            branches:
                - 2 or more branches of assignment expressions
            immediate processing:
                - not generated if there's only one assignment expression
    */
    return comp_eat_list( comp, pnodeoffs, NT_ASSIGNLIST, comp_eat_anyassign, TOK_COMMA, "Assignment expected" );
}

bool comp_eat_letstmt( compiler_t* comp, uint16_t* pnodeoffs ) {
    // let-stmt := [ TOK_LET ] assign-list .
    bool mandatory = false;
    if ( comp->currtok == TOK_LET ) {
        if ( !comp_fetchtok( comp ) ) {
            return false;
        }
        // if LET is given, assignment list must be too.
        mandatory = true;
    }
    uint16_t assignlist = NODEOFFS_NONE;
    if ( !comp_eat_assignlist( comp, &assignlist ) || assignlist == NODEOFFS_NONE ) {
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
    if ( !comp_create_node( comp, pnodeoffs, NT_LETSTMT, UINT8_C(1), UINT16_C(0), 0, (int) assignlist ) ||
        *pnodeoffs == NODEOFFS_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_dimstmt( compiler_t* comp, uint16_t* pnodeoffs ) {
    // dim-stmt := TOK_DIM array-decl-list .
    if ( comp->currtok != TOK_DIM || !comp_fetchtok( comp ) ) {
        return false;
    }
    uint16_t decllist = NODEOFFS_NONE;
    if ( !comp_eat_arraydecllist( comp, &decllist ) || decllist == NODEOFFS_NONE ) {
        comp_error( comp, "Array declaration list expected" );
        return false;
    }
    /*
        NT_DIMSTMT      DIM statement
            branches:
                - 1 branch of array declarator list
    */
    if ( !comp_create_node( comp, pnodeoffs, NT_DIMSTMT, UINT8_C(1), UINT16_C(0), 0, (int) decllist ) ||
        *pnodeoffs == NODEOFFS_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_erasestmt( compiler_t* comp, uint16_t* pnodeoffs ) {
    // erase-stmt := TOK_ERASE empty-array-ref-list .
    if ( comp->currtok != TOK_ERASE || !comp_fetchtok( comp ) ) {
        return false;
    }
    uint16_t reflist = NODEOFFS_NONE;
    if ( !comp_eat_emptyarrayreflist( comp, &reflist ) || reflist == NODEOFFS_NONE ) {
        comp_error( comp, "Empty array reference list expected" );
        return false;
    }
    /*
        NT_ERASESTMT    ERASE statement
            branches:
                - 1 branch of empty array reference list
    */
    if ( !comp_create_node( comp, pnodeoffs, NT_ERASESTMT, UINT8_C(1), UINT16_C(0), 0, (int) reflist ) ||
        *pnodeoffs == NODEOFFS_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_defstmt( compiler_t* comp, uint16_t* pnodeoffs ) {
    // def-stmt := TOK_DEF usr-fn-decl .
    if ( comp->currtok != TOK_DEF || !comp_fetchtok( comp ) ) {
        return false;
    }
    uint16_t fndecl = NODEOFFS_NONE;
    if ( !comp_eat_usrfndecl( comp, &fndecl ) || fndecl == NODEOFFS_NONE ) {
        comp_error( comp, "User function declaration expected" );
        return false;
    }
    /*
        NT_DEFSTMT      DEF statement
            branches:
                - 1 branch of user function declaration
    */
    if ( !comp_create_node( comp, pnodeoffs, NT_DEFSTMT, UINT8_C(1), UINT16_C(0), 0, (int) fndecl ) ||
        *pnodeoffs == NODEOFFS_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_assignstmt( compiler_t* comp, uint16_t* pnodeoffs ) {
    // assign-stmt := let-stmt | dim-stmt | erase-stmt | def-stmt .
    // [ NT_ASSIGNSTMT - not generated ]
    if ( comp_eat_letstmt( comp, pnodeoffs ) && *pnodeoffs != NODEOFFS_NONE ) {
        return true;
    }
    if ( comp_eat_dimstmt( comp, pnodeoffs ) && *pnodeoffs != NODEOFFS_NONE ) {
        return true;
    }
    if ( comp_eat_erasestmt( comp, pnodeoffs ) && *pnodeoffs != NODEOFFS_NONE ) {
        return true;
    }
    if ( comp_eat_defstmt( comp, pnodeoffs ) && *pnodeoffs != NODEOFFS_NONE ) {
        return true;
    }
    return false;
}

bool comp_eat_forstmt( compiler_t* comp, uint16_t* pnodeoffs ) {
    // for-stmt := TOK_FOR num-base-var-ref TOK_EQ num-expr TOK_TO num-expr [ TOK_STEP num-expr ] .
    if ( comp->currtok != TOK_FOR || !comp_fetchtok( comp ) ) {
        return false;
    }
    // loop variable
    uint16_t varref = NODEOFFS_NONE;
    if ( !comp_eat_numbasevarref( comp, &varref ) || varref == NODEOFFS_NONE ) {
        comp_error( comp, "Numeric variable expected" );
        return false;
    }
    if ( comp->currtok != TOK_EQ || !comp_fetchtok( comp ) ) {
        comp_error( comp, "Assignment operator '=' expected" );
        return false;
    }
    // from expression
    uint16_t fromexp = NODEOFFS_NONE;
    if ( !comp_eat_numexpr( comp, &fromexp ) || fromexp == NODEOFFS_NONE ) {
        comp_error( comp, "Numeric expression expected" );
        return false;
    }
    if ( comp->currtok != TOK_TO || !comp_fetchtok( comp ) ) {
        comp_error( comp, "TO keyword expected" );
        return false;
    }
    // TO expression
    uint16_t toexp = NODEOFFS_NONE;
    if ( !comp_eat_numexpr( comp, &toexp ) || toexp == NODEOFFS_NONE ) {
        comp_error( comp, "Numeric expression expected" );
        return false;
    }
    uint16_t stepexp = NODEOFFS_NONE;
    if ( comp->currtok == TOK_STEP && comp_fetchtok( comp ) ) {
        // STEP expression
        if ( !comp_eat_numexpr( comp, &stepexp ) || stepexp == NODEOFFS_NONE ) {
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
    if ( !comp_create_node( comp, pnodeoffs, NT_FORSTMT, UINT8_C(4), UINT16_C(0), 0,
        (int) varref, (int) fromexp, (int) toexp, (int) stepexp ) || *pnodeoffs == NODEOFFS_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_nextstmt( compiler_t* comp, uint16_t* pnodeoffs ) {
    // next-stmt := TOK_NEXT [ num-base-var-ref { TOK_COMMA num-base-var-ref } ] .
    if ( comp->currtok != TOK_NEXT || !comp_fetchtok( comp ) ) {
        return false;
    }
    uint16_t reflist = NODEOFFS_NONE;
    comp_eat_list( comp, &reflist, NT_NUMBASEVARREFLIST, comp_eat_numbasevarref, TOK_COMMA, "Numeric variable reference expected" );
    /*
        NT_NEXTSTMT     NEXT statement
            branches:
                - 1 branch of variable reference list
    */
    // create node
    if ( !comp_create_node( comp, pnodeoffs, NT_NEXTSTMT, UINT8_C(1), UINT16_C(0), 0, (int) reflist ) ||
        *pnodeoffs == NODEOFFS_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_gototarget( compiler_t* comp, uint16_t* pnodeoffs ) {
    // dec-lit := TOK_DECLIT | TOK_DEC0 .. TOK_DEC9 .
    // goto-target := dec-lit | TOK_NUMIDENT .
    double num = 0; static char label[256]; label[0] = '\0';
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
            snprintf( label, 256U, "%s", comp->param );
            break;
        default:
            return false;
    }
    if ( !comp_fetchtok( comp ) ) {
        return false;
    }
    uint8_t data[3];
    if ( label[0] == '\0' ) {
        // line number
        int64_t ival = (int64_t) num;
        if ( ival < (int64_t) LINENO_MIN || ival > (int64_t) LINENO_MAX ) {
            comp_error( comp, "Invalid line number" );
            return false;
        }
        uint16_t lnum = (uint16_t) ival;
        data[0] = VARTYPEV_INT;
        data[1] = (uint8_t)( lnum >> UINT8_C(8) );
        data[2] = (uint8_t)  lnum;
    } else {
        // label: lookup or create
        uint16_t lvar = VAROFFS_NONE;
        if ( !vmem_lookup_var( &comp->rt->varmem, VARTYPEV_LABEL, label, &lvar ) || lvar == VAROFFS_NONE ) {
            lvar = VAROFFS_NONE;
            if ( !vmem_create_var( &comp->rt->varmem, VARTYPEV_LABEL, label, UINT8_C(0), 0, UINT8_C(0), 0, &lvar ) ||
                lvar == VAROFFS_NONE ) {
                out_of_memory( comp );
                return false;
            }
        }
        // create node
        data[0] = VARTYPEV_LABEL;
        data[1] = (uint8_t)( lvar >> UINT8_C(8) );
        data[2] = (uint8_t)  lvar;
    }
    /*
        NT_GOTOTARGET   GOTO/GOSUB target
            data:
                - 1 byte of type (integer or label)
                - 2 bytes of line number or variable offset (label!)
    */
    if ( !comp_create_node( comp, pnodeoffs, NT_GOTOTARGET, UINT8_C(0), UINT16_C(3), data ) || *pnodeoffs == NODEOFFS_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_gotostmt( compiler_t* comp, uint16_t* pnodeoffs ) {
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
    uint16_t target = NODEOFFS_NONE;
    if ( !comp_eat_gototarget( comp, &target ) || target == NODEOFFS_NONE ) {
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
    if ( !comp_create_node( comp, pnodeoffs, NT_GOTOSTMT, UINT8_C(1), UINT16_C(1), &tok, (int) target ) ||
        *pnodeoffs == NODEOFFS_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_returnstmt( compiler_t* comp, uint16_t* pnodeoffs ) {
    // return-stmt := TOK_RETURN .
    if ( comp->currtok != TOK_RETURN || !comp_fetchtok( comp ) ) {
        return false;
    }
    if ( !comp_create_node( comp, pnodeoffs, NT_RETURNSTMT, UINT8_C(0), UINT16_C(0), 0 ) || *pnodeoffs == NODEOFFS_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_labelstmt( compiler_t* comp, uint16_t* pnodeoffs ) {
    // label-stmt := TOK_LABEL TOK_NUMIDENT .
    if ( comp->currtok != TOK_LABEL || !comp_fetchtok( comp ) ) {
        return false;
    }
    if ( comp->currtok != TOK_NUMIDENT ) {
UNEXP:  comp_error( comp, "Numeric identifier expected" );
        return false;
    }
    static char label[256];
    snprintf( label, 256U, "%s", comp->param );
    if ( !comp_fetchtok( comp ) ) {
        goto UNEXP;
    }
    uint16_t labvar = VAROFFS_NONE;
    if ( !vmem_lookup_var( &comp->rt->varmem, VARTYPEV_LABEL, label, &labvar ) || labvar == VAROFFS_NONE ) {
        labvar = VAROFFS_NONE;
        if ( !vmem_create_var( &comp->rt->varmem, VARTYPEV_LABEL, label, UINT8_C(0), 0, UINT16_C(0), 0, &labvar ) ||
            labvar == VAROFFS_NONE ) {
            out_of_memory( comp );
            return false;
        }
    }
    /*
        NT_LABELSTMT    LABEL statement
            data:
                - 2 bytes of label variable offset
    */
    uint8_t data[2];
    data[0] = (uint8_t)( labvar >> UINT8_C(8) );
    data[1] = (uint8_t)  labvar;
    if ( !comp_create_node( comp, pnodeoffs, NT_LABELSTMT, UINT8_C(0), UINT16_C(2), data ) || *pnodeoffs == NODEOFFS_NONE ) {
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

bool comp_eat_singlelineifstmt( compiler_t* comp, uint16_t* pnodeoffs ) {
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
    uint16_t numexpr = NODEOFFS_NONE;
    if ( !comp_eat_numexpr( comp, &numexpr ) || numexpr == NODEOFFS_NONE ) {
        goto POP;
    }
    uint8_t gotok = TOK_EOL;
    if ( !comp_eat_thenorgoto( comp, &gotok ) || gotok == TOK_EOL ) {
        goto POP;
    }
    // -- second marker: at THEN or GOTO keyword
    comp_push_context( comp );
    uint16_t target1 = NODEOFFS_NONE, target2 = NODEOFFS_NONE;
    if ( comp_eat_gototarget( comp, &target1 ) && target1 != NODEOFFS_NONE ) {
        // 1st variant, using one or two goto targets
        comp_commit_context( comp );
        comp_commit_context( comp );
        if ( comp->currtok == TOK_ELSE && comp_fetchtok( comp ) ) {
            // ELSE branch on 1st variant: Must also be GOTO target
            if ( !comp_eat_gototarget( comp, &target2 ) || target2 == NODEOFFS_NONE ) {
                comp_error( comp, "Jump target expected" );
                return false;
            }
        }
    } else if ( gotok == TOK_THEN ) {
        // 2nd variant, using two statement lists: make sure we're at the right position
        comp_pop_context( comp );
        target1 = NODEOFFS_NONE;
        if ( !comp_eat_stmtlist( comp, &target1 ) || target1 == NODEOFFS_NONE ) {
            // doesn't satisfy statement list, could be multiline IF/UNLESS, rewind
            goto POP;
        }
        // commit to the context
        comp_commit_context( comp );
        // check for ELSE branch
        if ( comp->currtok == TOK_ELSE && comp_fetchtok( comp ) ) {
            // ELSE branch on 1st variant: Must also be GOTO target
            if ( !comp_eat_stmtlist( comp, &target2 ) || target2 == NODEOFFS_NONE ) {
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
    if ( !comp_create_node( comp, pnodeoffs, NT_SINGLELINEIFSTMT, UINT8_C(2), UINT16_C(2), data,
        (int) target1, (int) target2 ) || *pnodeoffs == NODEOFFS_NONE ) {
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

bool comp_eat_multilineifstmt( compiler_t* comp, uint16_t* pnodeoffs ) {
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
    uint16_t numexpr = NODEOFFS_NONE;
    if ( !comp_eat_numexpr( comp, &numexpr ) || numexpr == NODEOFFS_NONE ) {
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
    uint16_t target1 = NODEOFFS_NONE, target2 = NODEOFFS_NONE;
    if ( !comp_eat_stmtlines( comp, &target1 ) || target1 == NODEOFFS_NONE ) {
        comp_error( comp, "Statement list expected" );
    }
    if ( comp->currtok == TOK_ELSE && comp_fetchtok( comp ) ) {
        if ( !comp_eat_stmtlines( comp, &target2 ) || target2 == NODEOFFS_NONE ) {
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
                - 1 branch of statement lines
                - 1 optional branch of ELSE statement lines
    */
    if ( !comp_create_node( comp, pnodeoffs, NT_MULTILINEIFSTMT, UINT8_C(2), UINT16_C(1), &iftok,
        (int) target1, (int) target2 ) || *pnodeoffs == NODEOFFS_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_controlflowstmt( compiler_t* comp, uint16_t* pnodeoffs ) {
    // control-flow-stmt := for-stmt | next-stmt | goto-stmt | return-stmt | label-stmt |
    //                      singleline-if-stmt | multiline-if-stmt .
    // [ NT_CONTROLFLOWSTMT - not generated ]
    if ( comp_eat_forstmt( comp, pnodeoffs ) && *pnodeoffs != NODEOFFS_NONE ) {
        return true;
    }
    if ( comp_eat_nextstmt( comp, pnodeoffs ) && *pnodeoffs != NODEOFFS_NONE ) {
        return true;
    }
    if ( comp_eat_gotostmt( comp, pnodeoffs ) && *pnodeoffs != NODEOFFS_NONE ) {
        return true;
    }
    if ( comp_eat_returnstmt( comp, pnodeoffs ) && *pnodeoffs != NODEOFFS_NONE ) {
        return true;
    }
    if ( comp_eat_labelstmt( comp, pnodeoffs ) && *pnodeoffs != NODEOFFS_NONE ) {
        return true;
    }
    if ( comp_eat_singlelineifstmt( comp, pnodeoffs ) && *pnodeoffs != NODEOFFS_NONE ) {
        return true;
    }
    if ( comp_eat_multilineifstmt( comp, pnodeoffs ) && *pnodeoffs != NODEOFFS_NONE ) {
        return true;
    }
    return false;
}

bool comp_eat_stmt( compiler_t* comp, uint16_t* pnodeoffs ) {
    // stmt := io-stmt | assign-stmt | control-flow-stmt .
    // [ NT_STMT - not generated ]
    if ( comp_eat_iostmt( comp, pnodeoffs ) && *pnodeoffs != NODEOFFS_NONE ) {
        return true;
    }
    if ( comp_eat_assignstmt( comp, pnodeoffs ) && *pnodeoffs != NODEOFFS_NONE ) {
        return true;
    }
    if ( comp_eat_controlflowstmt( comp, pnodeoffs ) && *pnodeoffs != NODEOFFS_NONE ) {
        return true;
    }
    return false;
}

bool comp_eat_stmtlist( compiler_t* comp, uint16_t* pnodeoffs ) {
    // stmt-list := stmt { TOK_COLON stmt } .
    /*
        NT_STMTLIST         Statement list
            branches:
                - 2 or more statements
            immediate processing:
                - generated only if more than 1 statement
    */
    return comp_eat_list( comp, pnodeoffs, NT_STMTLIST, comp_eat_stmt, TOK_COLON, "Statement expected" );
}

bool comp_eat_stmtline( compiler_t* comp, uint16_t* pnodeoffs ) {
    // stmt-line := stmt-list [ TOK_QUOLIT ] TOK_EOL .
    uint16_t lnum = comp->iter.hdr.lineno;
    uint16_t stmtlist = NODEOFFS_NONE;
    if ( !comp_eat_stmtlist( comp, &stmtlist ) || stmtlist == NODEOFFS_NONE ) {
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
                - 2 bytes of line number (not present in direct mode)
            branches:
                - 1 branch of statement list
                - this is the root node for direct mode
    */
    uint8_t data[2];
    data[0] = (uint8_t)( lnum >> UINT8_C(8) );
    data[1] = (uint8_t)  lnum;
    uint16_t datalen = lnum == LINENO_DEL ? UINT16_C(0) : UINT16_C(2);
    // create node
    if ( !comp_create_node( comp, pnodeoffs, NT_STMTLINE, UINT8_C(1), datalen, data, (int) stmtlist ) ||
        *pnodeoffs == NODEOFFS_NONE ) {
        out_of_memory( comp );
        return false;
    }
    return true;
}

bool comp_eat_stmtlines( compiler_t* comp, uint16_t* pnodeoffs ) {
    // stmt-lines := stmt-line { stmt-line } .
    /*
        NT_STMTLINES        Statement lines
            branches:
                - 2 or more statement lines
            immediate processing:
                - generated only if more than 1 statement
                - this is the root node for program mode
    */
    return comp_eat_list( comp, pnodeoffs, NT_STMTLINES, comp_eat_stmtline, TOK_EOL, "Statement line expected" );
}

bool comp_nextline( compiler_t* comp ) {
    if ( !step_iterate_program( &comp->iter ) ) {
        return false;
    }
    comp->tokp = comp->iter.tok;
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
        bool (*read_fn)( const uint8_t**, char [256] );
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
            if ( !littbl[i].read_fn( (const uint8_t**)(&comp->tokp), comp->param ) ) {
                return false;
            }
            return true;
        }
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
            NONDEC:
                comp->number = (double)((int64_t)(strtoull( comp->param, 0, base )));
                break;
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

    if ( comp->syntree != NODEOFFS_NONE ) {
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
    if ( !comp_eat_stmtlines( comp, &comp->syntree ) || comp->syntree == NODEOFFS_NONE ) {
        comp_error( comp, "Statement lines expected" );
        return;
    }

    if ( comp->currtok != TOK_EOL ) {
        comp_error( comp, "Internal error (not at end of text)" );
        return;
    }
}
