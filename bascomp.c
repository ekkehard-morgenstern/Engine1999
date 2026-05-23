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
    comp->tokp = 0;
    comp->currtok = TOK_EOL;
    comp->treesize = UINT16_C(0);
    // CAUTION: Set the "keepmemory" flag only if you know what you're doing!
    if ( keepmemory ) {
        return;
    }
    comp->report   = 0;
    comp->halt     = 0;
    comp->userdata = 0;
    comp->treesize = UINT16_C(0);
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
    // num-base-var-ref := TOK_IDENT [ TOK_INTEGER ] .

    if ( comp->currtok != TOK_IDENT ) {
        return false;
    }
    static char data[257];
    data[0] = VARTYPEV_FLOAT;
    snprintf( &data[1], 256U, "%s", comp->param );
    uint16_t datalen = UINT16_C(1) + ( (uint16_t) strlen( &data[1] ) );
    if ( !comp_fetchtok( comp ) ) {
        return false;
    }

    if ( comp->currtok == TOK_INTEGER ) {
        data[0] = VARTYPEV_INT;
        if ( !comp_fetchtok( comp ) ) {
            return false;
        }
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
        ok = comp_create_node( comp, pnodeoffs, nodetype, UINT8_C(0), UINT8_C(3), data, (int) arrayindexnode );
    } else {
        ok = comp_create_node( comp, pnodeoffs, NT_NUMVARREF, UINT8_C(0), UINT8_C(3), data );
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
    // str-base-var-ref := TOK_IDENT TOK_DOLLAR .

    if ( comp->currtok != TOK_IDENT ) {
        return false;
    }
    static char data[257];
    data[0] = VARTYPEV_STR;
    snprintf( &data[1], 256U, "%s", comp->param );
    uint16_t datalen = UINT16_C(1) + ( (uint16_t) strlen( &data[1] ) );
    if ( !comp_fetchtok( comp ) ) {
        return false;
    }
    if ( comp->currtok != TOK_STRING || !comp_fetchtok( comp ) ) {
        comp_error( comp, "String sigil '$' expected" );
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
    // any-base-var-ref := TOK_IDENT [ TOK_INTEGER | TOK_STRING ] .

    if ( comp->currtok != TOK_IDENT ) {
        return false;
    }
    static char data[257];
    data[0] = VARTYPEV_FLOAT;
    snprintf( &data[1], 256U, "%s", comp->param );
    uint16_t datalen = UINT16_C(1) + ( (uint16_t) strlen( &data[1] ) );
    if ( !comp_fetchtok( comp ) ) {
        return false;
    }
    if ( comp->currtok == TOK_INTEGER ) {
        data[0] = VARTYPEV_INT;
        if ( !comp_fetchtok( comp ) ) {
            return false;
        }
    } else if ( comp->currtok == TOK_STRING ) {
        data[0] = VARTYPEV_STR;
        if ( !comp_fetchtok( comp ) ) {
            return false;
        }
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
    if ( !comp_create_node( comp, pnodeoffs, NT_DECLIT, UINT8_C(0), UINT16_C(8), data ) || *pnodeoffs == NODEOFFS_NONE ) {
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
    if ( !comp_create_node( comp, pnodeoffs, data[0], UINT8_C(0), (uint16_t)( 1U + len ), data ) || *pnodeoffs == NODEOFFS_NONE ) {
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
    return false; // TBD
}

/*
bool comp_eat_numusrfnname( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_strusrfnname( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numusrfncall( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_strusrfncall( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_sysnoargstrname( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_sysnoargstrcall( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numfunccall( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_strfunccall( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_strbaseexpr( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_straddexpr( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_strexpr( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numsubexpr( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numbaseexpr( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numunaryop( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numunaryex( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_nummultop( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_nummultex( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numaddop( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numaddex( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numshiftop( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numshiftex( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numcmpop( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numcmpex( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numandop( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numandex( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numorop( compiler_t* comp, uint16_t* pnodeoffs );
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
*/

bool comp_nextline( compiler_t* comp ) {
    if ( !step_iterate_program( &comp->iter ) ) {
        return false;
    }
    comp->tokp = comp->iter.tok;
    comp->currtok = TOK_EOL;
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
        { TOK_IDENT , read_ident  }, { TOK_STRLIT, read_strlit },
        { TOK_HEXLIT, read_hexlit }, { TOK_DECLIT, read_declit },
        { TOK_OCTLIT, read_octlit }, { TOK_QUALIT, read_qualit },
        { TOK_BINLIT, read_binlit }, { TOK_SHLLIT, read_shllit },
        { TOK_QUOLIT, read_quolit }, { TOK_BRKLIT, read_brklit },
        { TOK_BRCLIT, read_brclit }, { 0, 0 }
    };
    for ( int i=0; littbl[i].tok; ++i ) {
        if ( littbl[i].tok == tok ) {
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
    // fetch first token
    if ( !comp_fetchtok( comp ) ) {
        return false;
    }

    return true;
}

void run_compiler( compiler_t* comp ) {

    // begin compilation
    if ( !begin_comp( comp ) ) {
        return;
    }

    //

}
