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
    uint8_t tok = comp->currtok = *comp->tokp++;
    if ( tok == TOK_EOL ) {
        --comp->tokp;
        if ( comp->iter.hdr.lineno != LINENO_NONE ) {
            if ( !comp_nextline( comp ) ) {
                return false;
            }
            goto NEXTTOK;
        }
        return false;
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
