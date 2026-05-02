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
    comp->codesize = UINT16_C(0);
    comp->datasize = UINT16_C(0);
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

bool comp_alloc_data( compiler_t* comp, uint16_t size, uint16_t* poffs ) {
    if ( size > DATASIZE_MAX - comp->datasize ) {
        return false;
    }
    *poffs = (uint16_t) comp->datasize;
    comp->datasize += size;
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
    comp->tree[ offs++ ] = (uint8_t) ( datalen >> UINT8_C(8) );
    comp->tree[ offs++ ] = (uint8_t)   datalen;
    uint16_t linkoffs = offs;
    comp->tree[ offs++ ] = (uint8_t) ( NODEOFFS_NONE >> UINT8_C(8) );
    comp->tree[ offs++ ] = (uint8_t)   NODEOFFS_NONE;
    comp->tree[ offs++ ] = (uint8_t) ( NODEOFFS_NONE >> UINT8_C(8) );
    comp->tree[ offs++ ] = (uint8_t)   NODEOFFS_NONE;
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
                comp->tree[ lastbranch + 2U ] = (uint8_t) ( offs >> UINT8_C(8) );
                comp->tree[ lastbranch + 3U ] = (uint8_t)   offs;
            }
            lastbranch = offs;
            // <nodepos.16> <nextbranch.16>
            comp->tree[ offs++ ] = (uint8_t) ( branchoffs >> UINT8_C(8) );
            comp->tree[ offs++ ] = (uint8_t)   branchoffs;
            comp->tree[ offs++ ] = (uint8_t) ( NODEOFFS_NONE >> UINT8_C(8) );
            comp->tree[ offs++ ] = (uint8_t)   NODEOFFS_NONE;
        }
        va_end( ap );
        comp->tree[ linkoffs      ] = (uint8_t) ( firstbranch >> UINT8_C(8) );
        comp->tree[ linkoffs + 1U ] = (uint8_t)   firstbranch;
        comp->tree[ linkoffs + 2U ] = (uint8_t) ( lastbranch >> UINT8_C(8) );
        comp->tree[ linkoffs + 3U ] = (uint8_t)   lastbranch;
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
    uint16_t lastbranch =
        ( ( (uint16_t) comp->tree[ nodeoffs + 6U ] ) << UINT8_C(8) ) |
                       comp->tree[ nodeoffs + 7U ];
    // allocate new branch entry
    uint16_t offs = NODEOFFS_NONE;
    if ( !comp_alloc_tree( comp, BRANCHENT_SIZE, &offs ) || offs == NODEOFFS_NONE ) {
        return false;
    }
    if ( lastbranch != NODEOFFS_NONE ) {
        // if there was a previous branch, link it to this one
        // <nodepos.16> <nextbranch.16>
        comp->tree[ lastbranch + 2U ] = (uint8_t) ( offs >> UINT8_C(8) );
        comp->tree[ lastbranch + 3U ] = (uint8_t)   offs;
    }
    lastbranch = offs;  // now this node is the last one in the list
    // update lastbranch link in node
    comp->tree[ nodeoffs + 6U ] = (uint8_t) ( lastbranch >> UINT8_C(8) );
    comp->tree[ nodeoffs + 7U ] = (uint8_t)   lastbranch;
    // increment number of branches
    comp->tree[ nodeoffs + 1U ] += UINT8_C(1);
    // store new branch info
    // <nodepos.16> <nextbranch.16>
    comp->tree[ offs++ ] = (uint8_t) ( branchoffs >> UINT8_C(8) );
    comp->tree[ offs++ ] = (uint8_t)   branchoffs;
    comp->tree[ offs++ ] = (uint8_t) ( NODEOFFS_NONE >> UINT8_C(8) );
    comp->tree[ offs   ] = (uint8_t)   NODEOFFS_NONE;
    // done
    return true;
}

bool comp_eat_list( compiler_t* comp, uint16_t* pnodeoffs, uint8_t nodetype, comp_eatfn_t element_eater, uint8_t septok,
    const char* errortext ) {
    // list := element { SEPTOK element } .  -- if SEPTOK is TOK_EOL, there's no separator token
    uint16_t expr1 = NODEOFFS_NONE;
    if ( !element_eater( comp, &expr1 ) ) {
        return false;
    }
    if ( expr1 == NODEOFFS_NONE ) {
        return false;
    }
    uint16_t nodeoffs = NODEOFFS_NONE;
    for (;;) {
        uint8_t* backup = comp->tokp;
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
                // in case the function returns (which it should not)
                // rewind token pointer
CANCEL:         comp->tokp = backup;
                // re-fetch the separator token (if any)
                comp_fetchtok( comp );
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
                goto CANCEL;
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

bool comp_eat_arraysub( compiler_t* comp, uint16_t* pnodeoffs ) {
    // array-index := num-ex-list | str-expr .
    // array-sub := TOK_LPAREN array-index TOK_RPAREN .
    return false; // TBD
}

/*
bool comp_eat_arraydimdecl( compiler_t* comp, uint16_t* pnodeoffs );
bool comp_eat_arraydecl( compiler_t* comp, uint16_t* pnodeoffs );
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
