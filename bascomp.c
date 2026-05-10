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

void init_compiler( compiler_t* comp, program_t* pgm, bool keepmemory ) {
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
    comp->codesize = UINT16_C(0);
    comp->varssize = MAXVARS * 2U;
    comp->strssize = UINT16_C(0);
    comp->numvars  = UINT16_C(0);
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

bool comp_alloc_tree( compiler_t* comp, uint16_t size, uint16_t* poffs ) {
    if ( size > TREESIZE_MAX - comp->treesize ) {
        return false;
    }
    *poffs = (uint16_t) comp->treesize;
    comp->treesize += size;
    return true;
}

bool comp_alloc_code( compiler_t* comp, uint16_t size, uint16_t* poffs ) {
    if ( size > CODESIZE_MAX - comp->codesize ) {
        return false;
    }
    *poffs = (uint16_t) comp->codesize;
    comp->codesize += size;
    return true;
}

static void comp_compact_vars( compiler_t* comp ) {
    // algorithm:
    //  - by scanning the variable offset table, iterate over every variable (regular and array)
    //  - if it's still being used (not marked as deleted), copy it to temporary memory.
    //  - copy the resulting memory back, noting the new size
    static uint8_t tmp[VARSSIZE_MAX];
    uint16_t tmpoffs = MAXVARS * 2U;
    // iterate over the variables
    for ( uint16_t i=0; i < comp->numvars; ++i ) {
        uint16_t indexpos = i * 2U;
        // get the offset of the variable header stored in the offset cell
        uint16_t offs = VEXTRACT16( comp, indexpos );
        if ( offs == VAROFFS_NONE ) {
            // variable is unused; store that info
            VTWRITE16( tmp, indexpos, VAROFFS_NONE );
            continue;
        }
        // offset points to a variable header: check
        // <size.16> <type.8> <namelen.8> <name...> [ <arraydims...> | <numargs> <argdesc...> ] <data...>
        uint16_t size = VEXTRACT16( comp, offs );
        uint16_t begoffs = tmpoffs;
        // we need to trust that the size is correct.
        if ( size ) {
            memcpy( &tmp[ tmpoffs ], &comp->vars[ offs ], size );
            tmpoffs += size;
        }
        // store the new offset in the target area
        VTWRITE16( tmp, indexpos, begoffs );
    }
    // copy the results back
    memcpy( comp->vars, tmp, tmpoffs );
    comp->varssize = tmpoffs;
}

bool comp_alloc_vars( compiler_t* comp, uint16_t size, uint16_t* poffs ) {
    if ( size > VARSSIZE_MAX - comp->varssize ) {
        comp_compact_vars( comp );
        if ( size > VARSSIZE_MAX - comp->varssize ) {
            return false;
        }
    }
    *poffs = (uint16_t) comp->varssize;
    comp->varssize += size;
    return true;
}

static bool comp_create_var_offset( compiler_t* comp, uint16_t* poffs ) {
    if ( comp->numvars >= MAXVARS ) {
        for ( uint16_t i=UINT16_C(0); i < MAXVARS; ++i ) {
            uint16_t pos  = i * UINT16_C(2);
            uint16_t offs = VEXTRACT16( comp, pos );
            if ( offs != VAROFFS_NONE ) {
                continue;
            }
            *poffs = pos;
            return true;
        }
        return false;
    }
    uint16_t pos = comp->numvars * UINT16_C(2);
    VWRITE16( comp, pos, VAROFFS_NONE );
    *poffs = pos; ++comp->numvars;
    return true;
}

static void comp_get_array_num_elems( compiler_t* comp, uint16_t varoffs, uint16_t* pnumelem, uint16_t* pdataoffs ) {
    // call this ONLY for an array type!
    // <size.16> <type.8> <namelen.8> <name...> <numdims.8> <arraydims...>
    uint16_t numdimsfld = varoffs + UINT16_C(4) + comp->vars[ varoffs + 3U ];
    uint16_t arrdimsfld = numdimsfld + UINT16_C(1);
    uint16_t numelems   = UINT16_C(0);
    uint8_t  numdims    = comp->vars[ numdimsfld ];
    for ( uint8_t i = UINT8_C(0); i < numdims; ++i ) {
        uint16_t dim = VEXTRACT16( comp, arrdimsfld );
        if ( numelems ) {
            numelems *= dim;
        } else {
            numelems += dim;
        }
        arrdimsfld += UINT16_C(2);
    }
    *pnumelem  = numelems;
    *pdataoffs = arrdimsfld;
}

bool comp_lookup_var( compiler_t* comp, uint8_t vartype, const char* name, uint16_t* poutoffs ) {
    /*
    WARNING: The function return value doesn't indicate find success.
    If the function returns true with an offset of VAROFFS_NONE, the variable was not found.
    */
    size_t namelen = strlen( name );
    if ( namelen == 0 ) {
        // it's an error to have a zero-length name
        return false;
    }
    for ( uint16_t i=0; i < comp->numvars; ++i ) {
        uint16_t indexpos = i * 2U;
        // get the offset of the variable header stored in the offset cell
        uint16_t offs = VEXTRACT16( comp, indexpos );
        if ( offs == VAROFFS_NONE ) {
            // variable is unused; skip
            continue;
        }
        // offset points to a variable header: check
        // <size.16> <type.8> <namelen.8> <name...> [ <numdims.8> <arraydims...> | <numargs> <argdesc...> ] <data...>
        uint16_t var_type = VEXTRACT16( comp, offs + 2U );
        /*
            +---+---+---+---++---+---+---+---+
            | . | . | f | s || e | e | e | e |
            +---+---+---+---++---+---+---+---+
        */
        if ( var_type != vartype ) {
            // wrong variable type, skip ->
            // (in BASIC, it's permitted to have variables with same name but different type, like 'a$' and 'a' are distinct)
            continue;
        }
        uint8_t name_len = comp->vars[ offs + 3U ];
        if ( name_len != namelen ) {
            // different name length, skip ->
            continue;
        }
        if ( memcmp( &comp->vars[ offs + 4U ], name, name_len ) != 0 ) {
            // name mismatch, skip ->
            continue;
        }
        // exact match: return variable index offset
        *poutoffs = indexpos;
        return true;
    }
    // not found
    *poutoffs = VAROFFS_NONE;
    return true;
}

bool comp_create_var( compiler_t* comp, uint8_t vartype, const char* name, uint8_t numdims, const uint16_t* dims,
    uint8_t numparams, const usrparam_t* params, uint16_t* poutoffs ) {
    uint16_t indexpos = VAROFFS_NONE;
    size_t namelen = name ? strlen(name) : 0;
    // some sanity checks
    if ( vartype & VARTYPEM_INVAL || name == 0 || namelen == 0 || namelen > 255U || ( numdims != 0 && dims == 0 ) ||
        numdims > MAXDIM || ( numparams != 0 && params == 0 ) || numparams > MAXPARAM || poutoffs == 0 ) {
INTERR: comp_error( comp, "Internal error" );
        return false;
    }
    if ( ( vartype & ( VARTYPEF_ARRAY | VARTYPEF_FUNC ) ) == ( VARTYPEF_ARRAY | VARTYPEF_FUNC ) ) {
        // cannot be array and function at the same time
        goto INTERR;
    }
    size_t dimsize = 0;
    for ( uint8_t i=0; i < numdims; ++i ) {
        if ( i == 0 ) {
            dimsize += dims[i];
        } else {
            dimsize *= dims[i];
        }
    }
    if ( ( numdims && dimsize == 0 ) || dimsize > 65535U ) {
        if ( dimsize > 65535U ) {
TOOLARGE:   comp_error( comp, "Array too large" );
            return false;
        }
        goto INTERR;
    }
    for ( uint8_t i=0; i < numparams; ++i ) {
        if ( params[i].paramname == 0 || params[i].paramtype & VARTYPEM_INVAL ) {
            goto INTERR;
        }
        size_t nlen = strlen( params[i].paramname );
        if ( nlen == 0U || nlen > 255U ) {
            goto INTERR;
        }
    }
    // parameters seem OK, attempt to look up the variable
    if ( !comp_lookup_var( comp, vartype, name, &indexpos ) || indexpos != VAROFFS_NONE ) {
        // error or variable already exists: return error
        comp_error( comp, "Variable already exists" );
        return false;
    }
    // variable really does not exist: create
    if ( !comp_create_var_offset( comp, &indexpos ) || indexpos == VAROFFS_NONE ) {
        // most likely, variable index is full
        comp_error( comp, "Too many variables" );
        return false;
    }
    // We expect that everything concerning the variable parameters has already been checked.
    // Supplying nonsense here WILL result in undefined behavior.
    // I added some simple checks at the beginning to remedy that a bit, but they don't cover all of the cases.

    // <size.16> <type.8> <namelen.8> <name...> [ <numdims.8> <arraydims...> | <numargs> <argdesc...> ] <data...>
    //
    // A user-defined function variable has N argument descriptor fields. Each argument descriptor contains a type field
    // and a name field (with preceding length byte).

    size_t elemsize = 0U;
    switch ( vartype & VARTYPEM_BASE ) {
        case VARTYPEV_FLOAT:    elemsize = 8U; break;   // 64 bit float
        case VARTYPEV_INT:      elemsize = 2U; break;   // 16 bit integer
        case VARTYPEV_STR:      elemsize = 4U; break;   // offset in string space + length
        case VARTYPEV_LABEL:    elemsize = 2U; break;   // offset in code space
    }

    size_t varsize = 4U + namelen;
    if ( vartype & VARTYPEF_ARRAY ) {
        if ( numdims == 0U ) {
            goto INTERR;
        }
        varsize += 1U + numdims * 2U + dimsize * elemsize;
    } else if ( vartype & VARTYPEF_FUNC ) {
        varsize += 1U;
        for ( uint8_t i=UINT8_C(0); i < numparams; ++i ) {
            varsize += 2U + strlen( params[i].paramname );
        }
        varsize += 2U;  // offset in code segment
    } else {
        // regular variable
        varsize += elemsize;
    }

    if ( varsize > 65535U ) {
        goto TOOLARGE;
    }

    uint16_t memoffs = VAROFFS_NONE;
    if ( !comp_alloc_vars( comp, (uint16_t) varsize, &memoffs ) || memoffs == VAROFFS_NONE ) {
        comp_error( comp, "Out of memory" );
        return false;
    }

    // got the variable memory: set up variable header

    // <size.16> <type.8> <namelen.8> <name...> [ <numdims.8> <arraydims...> | <numargs> <argdesc...> ] <data...>
    //
    // A user-defined function variable has N argument descriptor fields. Each argument descriptor contains a type field
    // and a name field (with preceding length byte).

    uint16_t membeg = memoffs;
    VWRITE16( comp, memoffs, varsize );
    comp->vars[ memoffs + 2U ] = vartype;
    comp->vars[ memoffs + 3U ] = (uint8_t) namelen;
    memoffs += UINT16_C(4);
    memcpy( &comp->vars[ memoffs ], name, namelen );
    memoffs += (uint16_t) namelen;

    if ( vartype & VARTYPEF_ARRAY ) {
        comp->vars[ memoffs++ ] = numdims;
        for ( uint8_t i=0; i < numdims; ++i ) {
            VWRITE16( comp, memoffs, dims[i] );
            memoffs += UINT16_C(2);
        }
    } else if ( vartype & VARTYPEF_FUNC ) {
        comp->vars[ memoffs++ ] = numparams;
        for ( uint8_t i=0; i < numparams; ++i ) {
            uint8_t nlen = (uint8_t) strlen( params[i].paramname );
            comp->vars[ memoffs++ ] = params[i].paramtype;
            comp->vars[ memoffs++ ] = nlen;
            memcpy( &comp->vars[ memoffs ], params[i].paramname, nlen );
            memoffs += nlen;
        }
    }

    // initialize the data
    if ( vartype & VARTYPEF_ARRAY ) {
        uint16_t numwritten = memoffs - membeg;
        uint16_t numleft    = (uint16_t)( varsize - numwritten );
        switch ( vartype & VARTYPEM_BASE ) {
            case VARTYPEV_FLOAT:
            case VARTYPEV_INT:
                // float and int arrays are initialized to zero
                memset( &comp->vars[ memoffs ], 0, numleft );
                break;
            case VARTYPEV_STR:
                // string arrays are initialized to STROFFS_NONE (0xFFFF)
                memset( &comp->vars[ memoffs ], 0xFF, numleft );
                break;
            case VARTYPEV_LABEL:
                // label arrays do not exist
                goto INTERR;
        }
    } else if ( vartype & VARTYPEF_FUNC ) {
        // a function only has a single offset member
        VWRITE16( comp, memoffs, CODEOFFS_NONE );
    } else {
        // individual variables
        switch ( vartype & VARTYPEM_BASE ) {
            case VARTYPEV_FLOAT:
                // a float variable is set to zero
                memset( &comp->vars[ memoffs ], 0, 8U );
                break;
            case VARTYPEV_INT:
                // an int variable is set to zero
                memset( &comp->vars[ memoffs ], 0, 2U );
                break;
            case VARTYPEV_STR:
                // a string variable is set to STROFFS_NONE
                VWRITE16( comp, memoffs, STROFFS_NONE );
                break;
            case VARTYPEV_LABEL:
                // a label variable is set to CODEOFFS_NONE
                VWRITE16( comp, memoffs, CODEOFFS_NONE );
                break;
        }
    }

    // return the variable index
    *poutoffs = indexpos;

    return true;
}

static void comp_compact_strs( compiler_t* comp ) {
    // algorithm:
    //  - iterate over all string variables (incl. every cell of each string array)
    //  - if the variable is non-empty, copy the string it points to to temporary memory
    //  - copy the resulting memory back, noting the new size
    //
    static uint8_t tmp[STRSSIZE_MAX];
    uint16_t tmpoffs = UINT16_C(0);
    // iterate over the variables
    for ( uint16_t i=0; i < comp->numvars; ++i ) {
        uint16_t indexpos = i * 2U;
        // get the offset of the variable header stored in the offset cell
        uint16_t offs = VEXTRACT16( comp, indexpos );
        if ( offs == VAROFFS_NONE ) {
            // variable is unused; skip
            continue;
        }
        // offset points to a variable header: check
        // <size.16> <type.8> <namelen.8> <name...> [ <numdims.8> <arraydims...> | <numargs> <argdesc...> ] <data...>
        uint16_t vartype = VEXTRACT16( comp, offs + 2U );
        /*
            +---+---+---+---++---+---+---+---+
            | . | . | f | s || e | e | e | e |
            +---+---+---+---++---+---+---+---+
        */
        if ( vartype & VARTYPEF_ARRAY ) {
            if ( ( vartype & VARTYPEM_BASE ) != VARTYPEV_STR ) {
                // not string array: skip
                continue;
            }
            // an array
            uint16_t numelems = UINT16_C(0);
            uint16_t dataoffs = UINT16_C(0);
            comp_get_array_num_elems( comp, offs, &numelems, &dataoffs );
            // each element is comprised of <stroffs.16> <length.16>
            for ( uint16_t j=UINT16_C(0); j < numelems; ++j ) {
                uint16_t varfld  = dataoffs;
                uint16_t lenfld  = varfld + UINT16_C(2);
                uint16_t stroffs = VEXTRACT16( comp, varfld );
                uint16_t length  = VEXTRACT16( comp, lenfld );
                if ( stroffs == STROFFS_NONE ) {
                    // empty string, skip
                    dataoffs += UINT16_C(4);
                    continue;
                }
                // have offset: transfer string to temporary area
                uint16_t tmpbeg = tmpoffs;
                if ( length ) {
                    memcpy( &tmp[ tmpoffs ], &comp->strs[ stroffs ], length );
                    tmpoffs += length;
                }
                // write new offset
                VWRITE16( comp, stroffs, tmpbeg );
                // next element
                dataoffs += UINT16_C(4);
            }
        } else if ( ( vartype & VARTYPEM_BASE ) == VARTYPEV_STR ) {
            // <size.16> <type.8> <namelen.8> <name...> <stroffs.16> <length.16>
            uint16_t strfld  = offs + UINT16_C(4) + comp->vars[ offs + 3U ];
            uint16_t lenfld  = strfld + UINT16_C(2);
            uint16_t stroffs = VEXTRACT16( comp, strfld );
            uint16_t length  = VEXTRACT16( comp, lenfld );
            if ( stroffs == STROFFS_NONE ) {
                // empty string, skip
                continue;
            }
            // have offset: transfer string to temporary area
            uint16_t tmpbeg = tmpoffs;
            if ( length ) {
                memcpy( &tmp[ tmpoffs ], &comp->strs[ stroffs ], length );
                tmpoffs += length;
            }
            // write new offset
            VWRITE16( comp, stroffs, tmpbeg );
        }
    }
    // copy the results back
    memcpy( comp->strs, tmp, tmpoffs );
    comp->strssize = tmpoffs;
}

bool comp_alloc_strs( compiler_t* comp, uint16_t size, uint16_t* poffs ) {
    if ( size > STRSSIZE_MAX - comp->strssize ) {
        comp_compact_strs( comp );
        if ( size > STRSSIZE_MAX - comp->strssize ) {
            return false;
        }
    }
    *poffs = (uint16_t) comp->strssize;
    comp->strssize += size;
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
OOM:            comp_error( comp, "Out of memory" );
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
    // array-sub := TOK_LPAREN array-index TOK_RPAREN .
    if ( comp->currtok == TOK_LPAREN ) {
        if ( !comp_fetchtok( comp ) ) {
ERROR:      comp_error( comp, "Array index expected" );
            return false;
        }
        if ( !comp_eat_arrayindex( comp, pnodeoffs ) ) {
            goto ERROR;
        }
        if ( comp->currtok != TOK_RPAREN || !comp_fetchtok( comp ) ) {
            comp_error( comp, "Closing parenthesis ')' expected" );
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

static double comp_extract_float( compiler_t* comp, uint16_t offs ) {
    uint64_t val =
        ( ( (uint64_t) comp->tree[ offs      ] ) << UINT8_C(56) ) |
        ( ( (uint64_t) comp->tree[ offs + 1U ] ) << UINT8_C(48) ) |
        ( ( (uint64_t) comp->tree[ offs + 2U ] ) << UINT8_C(40) ) |
        ( ( (uint64_t) comp->tree[ offs + 3U ] ) << UINT8_C(32) ) |
        ( ( (uint32_t) comp->tree[ offs + 4U ] ) << UINT8_C(24) ) |
        ( ( (uint32_t) comp->tree[ offs + 5U ] ) << UINT8_C(16) ) |
        ( ( (uint16_t) comp->tree[ offs + 6U ] ) << UINT8_C( 8) ) |
                       comp->tree[ offs + 7U ]                    ;
    union {
        uint64_t ui64;
        double   dbl;
    } u;
    u.ui64 = val;
    return u.dbl;
}

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
    double val = comp_extract_float( comp, node + 8U );
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
OOM:        comp_error( comp, "Out of memory" );
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

    // TBD

    return false;
}

/*
bool comp_eat_arraydecllist( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_emptyarrayref( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_emptyarrayreflist( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numbasevarref( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numvarref( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_strbasevarref( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_strvarref( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_anybasevarref( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_declit( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_numlit( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_strlit( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_strlits( compiler_t* comp, uint16_t* pnodeoffs );
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

static bool comp_gen_ins( compiler_t* comp, uint8_t ins, uint8_t ext, uint16_t param ) {
    uint8_t size = UINT8_C(1);
    if ( ins & INSF_E ) ++size;
    if ( ins & INSF_P ) size += 2U;
    uint16_t offs = INS_NODATA;
    if ( !comp_alloc_code( comp, UINT16_C(1), &offs ) ) {
        return false;
    }
    comp->code[ offs++ ] = ins;
    if ( ins & INSF_E ) {
        comp->code[ offs++ ] = ext;
    }
    if ( ins & INSF_P ) {
        comp->code[ offs++ ] = (uint8_t)( param >> UINT8_C(8) );
        comp->code[ offs++ ] = (uint8_t)  param;
    }
    return true;
}

bool comp_gen_brk( compiler_t* comp ) {
    return comp_gen_ins( comp, INS_BRK, UINT8_C(0), UINT16_C(0) );
}

bool comp_gen_nop( compiler_t* comp ) {
    return comp_gen_ins( comp, INS_NOP, UINT8_C(0), UINT16_C(0) );
}

bool comp_gen_phpa_c( compiler_t* comp, uint16_t offs ) {
    return comp_gen_ins( comp, INS_PHPA_C | INSF_P, UINT8_C(0), offs );
}

bool comp_gen_phpa_d( compiler_t* comp, uint16_t offs ) {
    return comp_gen_ins( comp, INS_PHPA_D | INSF_P, UINT8_C(0), offs );
}

static bool comp_gen_imm_ins( compiler_t* comp, uint8_t ins, int32_t imm ) {
    uint8_t  c = (uint8_t)( ( imm & INT32_C(0X00010000) ) >> UINT8_C(16) );
    uint16_t o = (uint16_t) imm;
    return comp_gen_ins( comp, ins | INSF_P | MKINS_C(c), UINT8_C(0), o );
}

bool comp_gen_phim( compiler_t* comp, int32_t imm ) {
    return comp_gen_imm_ins( comp, INS_PHIM, imm );
}

bool comp_gen_bria( compiler_t* comp, int32_t abs_offs ) {
    return comp_gen_imm_ins( comp, INS_BRIA, abs_offs );
}

bool comp_gen_brir( compiler_t* comp, int32_t rel_offs ) {
    return comp_gen_imm_ins( comp, INS_BRIR, rel_offs );
}

bool comp_gen_jpcc( compiler_t* comp ) {
    return comp_gen_ins( comp, INS_JPCC, UINT8_C(0), UINT16_C(0) );
}

bool comp_gen_jump( compiler_t* comp ) {
    return comp_gen_ins( comp, INS_JUMP, UINT8_C(0), UINT16_C(0) );
}

bool comp_gen_drop( compiler_t* comp, uint16_t cnt ) {
    if ( cnt == UINT16_C(0) ) {
        return true;
    }
    if ( cnt == UINT16_C(1) ) {
        return comp_gen_ins( comp, INS_DROP, UINT8_C(0), UINT16_C(0) );
    }
    return comp_gen_ins( comp, INS_DROP | INSF_P, UINT8_C(0), cnt );
}

bool comp_gen_line( compiler_t* comp, uint16_t line ) {
    return comp_gen_ins( comp, INS_LINE | INSF_P, UINT8_C(0), line );
}

bool comp_gen_exp_ins( compiler_t* comp, uint16_t ins ) {
    uint8_t i = (uint8_t)( ( ins & UINT16_C(0X0F00) ) >> UINT8_C(8) );
    uint8_t e = (uint8_t) ins;
    return comp_gen_ins( comp, MKINS_I(i) | INSF_E, e, UINT16_C(0) );
}

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
