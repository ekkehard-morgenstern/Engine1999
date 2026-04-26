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

#include "baspgm.h"
#include "bastok.h"
#include "basintp.h"

// -- program management ----------------------------------------------------

void init_program( program_t* pgm ) {
    pgm->fillpos = sizeof(linelist_t);
    linelist_t list;
    init_linelist( &list, LINELIST_POS );
    emit_linelist( pgm, LINELIST_POS, &list );
}

void clear_iter( pgmiter_t* iter, program_t* pgm ) {
    iter->pgm = pgm;
    iter->tok = 0;
    iter->pos = LINEOFFS_NONE;
    clear_linehdr( &iter->hdr );
}

bool iter_load_line( pgmiter_t* iter, uint16_t lineoffs ) {
    if ( lineoffs == LINEOFFS_NONE || lineoffs < sizeof(linelist_t) || lineoffs >= iter->pgm->fillpos ) {
        return false;
    }
    linehdr_t hdr; clear_linehdr( &hdr ); const uint8_t* p = 0;
    if ( !read_linehdr( iter->pgm, lineoffs, &hdr, &p ) ) {
        return false;
    }
    if ( hdr.lineno == LINENO_NONE ) {
        return false;
    }
    iter->tok = (uint8_t*) p;
    iter->pos = lineoffs;
    iter->hdr = hdr;
    return true;
}

bool begin_iterate_program( pgmiter_t* iter, program_t* pgm ) {
    // starts iterating with the first line, if it exists.
    // returns true if a line was loaded
    clear_iter( iter, pgm );
    // load first line
    uint16_t nodepos = LINEOFFS_NONE;
    if ( !linelist_firstnode( iter->pgm, LINELIST_POS, &nodepos ) ) {
        return false;
    }
    if ( nodepos != LINEOFFS_NONE ) {
        if ( !iter_load_line( iter, nodepos ) ) {
            return false;
        }
        return true;
    }
    return false;
}

bool step_iterate_program( pgmiter_t* iter ) {
    // steps through the program by addressing the next logical line.
    if ( iter->hdr.lineno == LINENO_NONE ) {
        return false;
    }
    uint16_t nextpos = LINEOFFS_NONE;
    if ( !linelist_nextnode( iter->pgm, iter->pos, &nextpos, 0 ) ) {
        return false;
    }
    if ( nextpos != LINEOFFS_NONE ) {
        if ( !iter_load_line( iter, nextpos ) ) {
            return false;
        }
        return true;
    }
    return false;
}

bool get_prev_next_linenos( const pgmiter_t* iter, uint16_t* pprevlineno, uint16_t* pnextlineno ) {

    uint16_t prevpos = LINEOFFS_NONE, nextpos = LINEOFFS_NONE;
    if ( !linelist_prevnode( iter->pgm, iter->pos, &prevpos, 0 ) ) {
        return false;
    }
    if ( !linelist_nextnode( iter->pgm, iter->pos, &nextpos, 0 ) ) {
        return false;
    }

    uint16_t prev_lineno, next_lineno;

    if ( prevpos != LINEOFFS_NONE ) {
        pgmiter_t prev = *iter;
        if ( !iter_load_line( &prev, prevpos ) ) {
            return false;
        }
        prev_lineno = prev.hdr.lineno;
    } else {
        prev_lineno = LINENO_NONE;
    }

    if ( nextpos != LINEOFFS_NONE ) {
        pgmiter_t next = *iter;
        if ( !iter_load_line( &next, nextpos ) ) {
            return false;
        }
        next_lineno = next.hdr.lineno;
    } else {
        next_lineno = LINENO_NONE;
    }

    *pprevlineno = prev_lineno;
    *pnextlineno = next_lineno;

    return true;
}

short find_line( program_t* pgm, uint16_t lineno, pgmiter_t* piter, uint16_t* pprevno, uint16_t* pnextno ) {
    if ( pgm == 0 || lineno == LINENO_NONE ) {
        return FOUND_ERROR;
    }
    // scan through program line by line, comparing line numbers
    pgmiter_t iter;
    if ( !begin_iterate_program( &iter, pgm ) ) {
        // program is empty
        if ( pprevno ) {
            *pprevno = LINENO_NONE;
        }
        if ( pnextno ) {
            *pnextno = LINENO_NONE;
        }
        return FOUND_NONE;
    }
    do {
        if ( iter.hdr.lineno == lineno ) {
            // exact match
            uint16_t prevno = LINENO_NONE, nextno = LINENO_NONE;
            if ( !get_prev_next_linenos( &iter, &prevno, &nextno ) ) {
                return FOUND_ERROR;
            }
            if ( pprevno ) {
                *pprevno = prevno;
            }
            if ( pnextno ) {
                *pnextno = nextno;
            }
            if ( piter ) {
                *piter = iter;
            }
            return FOUND_EXACT;
        }
        if ( iter.hdr.lineno > lineno ) {
            // current line number greater than the requested line number:
            // ideal insertion point. If there was a previous line, stop
            // and return to previous line.
            uint16_t prevpos = LINEOFFS_NONE;
            if ( !linelist_prevnode( iter.pgm, iter.pos, &prevpos, 0 ) ) {
                return FOUND_ERROR;
            }
            uint16_t prevno = LINENO_NONE, nextno = LINENO_NONE;
            if ( prevpos != LINEOFFS_NONE ) {
                if ( !iter_load_line( &iter, prevpos ) ) {
                    return FOUND_ERROR;
                }
                if ( !get_prev_next_linenos( &iter, &prevno, &nextno ) ) {
                    return FOUND_ERROR;
                }
                prevno = iter.hdr.lineno;
            } else {
                // otherwise, we stay on the current line (insert before first line)
                nextno = iter.hdr.lineno;
            }
            if ( pprevno ) {
                *pprevno = prevno;
            }
            if ( pnextno ) {
                *pnextno = nextno;
            }
            return FOUND_INSERT;
        }
    } while ( step_iterate_program( &iter ) );
    return FOUND_BEYOND;
}

bool zero_line( pgmiter_t* iter ) {
    if ( iter->hdr.lineno == LINENO_NONE || iter->tok == 0 ) {
        return false;
    }
    uint16_t toklen = iter->hdr.length - sizeof(linehdr_t);
    clear_linehdr( &iter->hdr );
    iter->hdr.lineno = LINENO_NONE;
    if ( !emit_linehdr( iter->pgm, iter->pos, &iter->hdr, 0 ) ) {
        return false;
    }
    if ( toklen ) {
        memset( iter->tok, 0, toklen );
    }
    return true;
}

bool unlink_line( pgmiter_t* iter ) {
    // remove line from context
    return remove_linenode( iter->pgm, iter->pos, &iter->hdr.node );
}

bool quick_append_line( program_t* pgm, const linehdr_t* newhdr, const uint8_t* newtokens ) {

    if ( newhdr->length > MAX_PROGRAMSIZE - pgm->fillpos ) {
        return false;
    }

    // store line in memory
    linehdr_t hdr; clear_linehdr( &hdr );
    hdr.lineno = newhdr->lineno;
    hdr.length = newhdr->length;
    hdr.alloc  = newhdr->alloc;
    uint16_t newpos = pgm->fillpos;
    uint8_t* p = &pgm->memory[ newpos ];
    emit_linehdr_raw( &p, &hdr );
    uint16_t toklen = hdr.length - sizeof(linehdr_t);
    if ( toklen ) {
        memcpy( p, newtokens, toklen );
    }
    pgm->fillpos += hdr.length;

    // link line into line list
    return linelist_addtail( pgm, LINELIST_POS, newpos, 0 );
}

bool defrag_memory( program_t* pgm ) {
    static program_t tmp;
    init_program( &tmp );
    pgmiter_t iter;
    if ( !begin_iterate_program( &iter, pgm ) ) {
        return false;
    }
    do {
        if ( !quick_append_line( &tmp, &iter.hdr, iter.tok ) ) {
            return false;
        }
    } while ( step_iterate_program( &iter ) );
    memcpy( pgm, &tmp, sizeof(program_t) );
    // at this point, all line positions in pgm might have changed
    return true;
}

bool create_line( program_t* pgm, linehdr_t* newhdr, const uint8_t* newtokens, uint16_t* poffs ) {
    // to create a new line, the following steps are undertaken:
    // - First, it is checked if there is enough memory to store the line.
    // - If there isn't, a defragmentation is attempted, and storing the line is retried.
    // NOTE that the line is NOT linked into the list just yet!

    if ( newhdr->length > MAX_PROGRAMSIZE - pgm->fillpos ) {
        // no room: attempt to defragment memory first
        defrag_memory( pgm );
        if ( newhdr->length > MAX_PROGRAMSIZE - pgm->fillpos ) {
            // still no room, give up
            return false;
        }
    }

    // in newhdr, only the length field needs to be correct at this point
    init_linenode( &newhdr->node );
    newhdr->alloc = newhdr->length;
    // write header
    uint16_t newpos = pgm->fillpos;
    uint8_t* p = &pgm->memory[ newpos ];
    emit_linehdr_raw( &p, newhdr );
    // write tokens
    uint16_t toklen = newhdr->length - sizeof(linehdr_t);
    if ( toklen ) {
        memcpy( p, newtokens, toklen );
    }
    *poffs = newpos;
    pgm->fillpos += newhdr->length;

    return true;
}

bool emplace_line( program_t* pgm, uint16_t prevno, pgmiter_t* curr, uint16_t nextno ) {
    // NOTE that the line numbers provided must be accurate.
    // If prevno is LINENO_NONE, currno is regarded as the first line of the program.
    // If nextno is LINENO_NONE, currno is regarded as the last line of the program.

    uint16_t prevpos = LINEOFFS_NONE, nextpos = LINEOFFS_NONE;

    if ( prevno != LINENO_NONE ) {
        pgmiter_t prev; clear_iter( &prev, pgm );
        short res = find_line( pgm, prevno, &prev, 0, 0 );
        if ( res != FOUND_EXACT ) {
            return false;
        }
        prevpos = prev.pos;
    }

    if ( nextno != LINENO_NONE ) {
        pgmiter_t next; clear_iter( &next, pgm );
        short res = find_line( pgm, nextno, &next, 0, 0 );
        if ( res != FOUND_EXACT ) {
            return false;
        }
        nextpos = next.pos;
    }

    if ( prevpos == LINEOFFS_NONE ) {
        prevpos = LINELIST_POS;     // head node
    }

    if ( nextpos == LINEOFFS_NONE ) {
        nextpos = LINELIST_POS + sizeof(linenode_t);    // tail node
    }

    return emplace_linenode( pgm, prevpos, curr->pos, nextpos, &curr->hdr.node );
}

bool update_line( pgmiter_t* iter, const linehdr_t* newhdr, const uint8_t* newtokens, uint16_t prevno, uint16_t nextno ) {
    if ( iter->hdr.lineno == LINENO_NONE || iter->tok == 0 || newhdr == 0 || newtokens == 0 ||
         newhdr->lineno != iter->hdr.lineno ) {
        return false;
    }
    if ( newhdr->length == sizeof(linehdr_t) + 1U && *newtokens == TOK_EOL ) {
        // new line only contains line number: delete line
        if ( !unlink_line( iter ) ) {
            return false;
        }
        zero_line( iter );
        return true;
    }
    if ( newhdr->length > iter->hdr.alloc ) {
        // unlink and discard old version of line
        if ( !unlink_line( iter ) ) {
            return false;
        }
        zero_line( iter );
        // create new line
        // WARNING:
        //      This is kind of hairy, because the program memory might get reorganized during a call of create_line()!
        //      So, we need to read the line numbers of the previous and next lines and get their new locations after
        //      create_line() returns to relink the line. For this reason, create_line() deletes all the link info from
        //      the supplied line header.
        iter->hdr.length = newhdr->length; uint16_t newpos = LINEOFFS_NONE;
        if ( !create_line( iter->pgm, &iter->hdr, newtokens, &newpos ) ) {
            return false;
        }
        iter->pos = newpos;
        if ( !emplace_line( iter->pgm, prevno, iter, nextno ) ) {
            return false;
        }
        return true;
    }
    // new line same length or shorter than allocated area: replace data
    iter->hdr.length = newhdr->length;
    uint16_t toksize = newhdr->length - sizeof(linehdr_t);
    if ( toksize ) {
        memcpy( iter->tok, newtokens, toksize );
    }
    return true;
}

bool enter_line( program_t* pgm, const uint8_t* tokline ) {
    linehdr_t hdr; const uint8_t* p = tokline;
    memset( &hdr, 0, sizeof(linehdr_t) );
    read_linehdr_raw( &p, &hdr );
    if ( hdr.lineno == LINENO_NONE ) {
        // direct mode
        return direct_mode( pgm, p );
    }
    pgmiter_t iter; clear_iter( &iter, pgm ); uint16_t prevno = LINENO_NONE, nextno = LINENO_NONE;
    uint16_t newpos = LINEOFFS_NONE;
    short res = find_line( pgm, hdr.lineno, &iter, &prevno, &nextno );
    switch ( res ) {
        case FOUND_ERROR:
            return false;
        case FOUND_EXACT:
            if ( !update_line( &iter, &hdr, p, prevno, nextno ) ) {
                return false;
            }
            return true;
        case FOUND_INSERT:
            iter.hdr = hdr;
            if ( !create_line( pgm, &iter.hdr, p, &newpos ) ) {
                return false;
            }
            iter.pos = newpos;
            if ( !emplace_line( pgm, prevno, &iter, nextno ) ) {
                return false;
            }
            return true;
        case FOUND_NONE:
        case FOUND_BEYOND:
            if ( hdr.length == sizeof(linehdr_t) + 1U && *tokline == TOK_EOL ) {
                // deleting non-existent line: nothing happens
                return true;
            }
            if ( !quick_append_line( pgm, &hdr, p ) ) {
                return false;
            }
            return true;
        default:
            return false;
    }
}

void list_program( program_t* pgm, uint16_t lineno_first, uint16_t lineno_last ) {

    bool isEmpty = true;
    if ( !linelist_empty( pgm, LINELIST_POS, &isEmpty ) || isEmpty ) {
        return;
    }

    uint16_t line_pos = LINEOFFS_NONE;
    if ( !linelist_firstnode( pgm, LINELIST_POS, &line_pos ) ) {
        return;
    }

    do {
        pgmiter_t iter; clear_iter( &iter, pgm );
        if ( !iter_load_line( &iter, line_pos ) ) {
            break;
        }

        uint16_t lineno = iter.hdr.lineno; bool display = false;
        if ( ( lineno_first == LINENO_NONE || lineno >= lineno_first ) &&
             ( lineno_last  == LINENO_NONE || lineno <= lineno_last  ) ) {
            display = true;
        }

        if ( !display ) {
            continue;
        }

        static char buf[1024]; size_t remain = 1024U;
        if ( !detokenize_line( buf, iter.tok, &remain, &iter.hdr ) ) {
            break;
        }

        printf( "%s\n", buf );

    } while ( linelist_nextnode( pgm, line_pos, &line_pos, 0 ) && line_pos != LINEOFFS_NONE );
}
