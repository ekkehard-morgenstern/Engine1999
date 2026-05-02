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

void init_compiler( compiler_t* comp, program_t* pgm ) {
    clear_iter( &comp->iter, pgm );
    comp->tokp = 0;
    comp->currtok = TOK_EOL;
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
    // <nodepos.16> <nextbranch.16>
    return false; // TBD
}

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
    uint8_t tok = comp->currtok = *comp->tokp++;
    if ( tok == TOK_EOL ) {
        --comp->tokp;
        return true;
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
