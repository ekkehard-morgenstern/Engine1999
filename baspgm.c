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

// -- program management ----------------------------------------------------

void init_program( program_t* pgm ) {
    pgm->first = 0;
    pgm->last  = 0;
}

void free_program( program_t* pgm ) {
    line_t* line = pgm->first;
    while ( line ) {
        line_t* next = line->next;
        free( line );
        line = next;
    }
    init_program( pgm );
}

void clear_iter( pgmiter_t* iter, program_t* pgm ) {
    iter->pgm  = pgm;
    iter->line = 0;
    iter->tok  = 0;
    clear_linehdr( &iter->hdr );
}

bool iter_load_line( pgmiter_t* iter, line_t* line ) {
    if ( line == 0 ) {
        return false;
    }
    iter->line = line;
    iter->tok  = line->tokens;
    iter->hdr.lineno = line->lineno;
    iter->hdr.length = (uint16_t)( line->toklen + LINEHDR_SIZE );
    return true;
}

size_t iter_remaining( const pgmiter_t* iter, const uint8_t* tokp ) {
    if ( iter->tok == 0 || tokp == 0 || iter->hdr.length < LINEHDR_SIZE ) {
        return 0;
    }
    const uint8_t* end = iter->tok + ( iter->hdr.length - LINEHDR_SIZE );
    if ( tokp < iter->tok || tokp > end ) {
        return 0;
    }
    return (size_t)( end - tokp );
}

bool begin_iterate_program( pgmiter_t* iter, program_t* pgm ) {
    // starts iterating with the first line, if it exists.
    // returns true if a line was loaded
    clear_iter( iter, pgm );
    return iter_load_line( iter, pgm->first );
}

bool step_iterate_program( pgmiter_t* iter ) {
    // steps through the program by addressing the next logical line.
    if ( iter->line == 0 ) {
        return false;
    }
    return iter_load_line( iter, iter->line->next );
}

bool find_line( program_t* pgm, uint32_t lineno, line_t** pline, line_t** pprev ) {
    line_t* prev = 0;
    line_t* line = pgm->first;
    while ( line && line->lineno < lineno ) {
        prev = line;
        line = line->next;
    }
    bool found = line && line->lineno == lineno;
    *pline = found ? line : 0;
    if ( pprev ) {
        *pprev = prev;
    }
    return found;
}

line_t* create_line( uint32_t lineno, const uint8_t* tokens, uint16_t toklen ) {
    line_t* line = (line_t*) malloc( sizeof(line_t) + toklen );
    if ( line == 0 ) {
        return 0;
    }
    line->next = 0;
    line->prev = 0;
    line->lineno = lineno;
    line->toklen = toklen;
    if ( toklen ) {
        memcpy( line->tokens, tokens, toklen );
    }
    return line;
}

void link_line( program_t* pgm, line_t* prev, line_t* line ) {
    line_t* next = prev ? prev->next : pgm->first;
    line->prev = prev;
    line->next = next;
    if ( prev ) {
        prev->next = line;
    } else {
        pgm->first = line;
    }
    if ( next ) {
        next->prev = line;
    } else {
        pgm->last = line;
    }
}

void unlink_line( program_t* pgm, line_t* line ) {
    if ( line->prev ) {
        line->prev->next = line->next;
    } else {
        pgm->first = line->next;
    }
    if ( line->next ) {
        line->next->prev = line->prev;
    } else {
        pgm->last = line->prev;
    }
    line->prev = 0;
    line->next = 0;
}

void delete_line( program_t* pgm, line_t* line ) {
    unlink_line( pgm, line );
    free( line );
}

bool enter_line( program_t* pgm, const uint8_t* tokline, bool (*directmode)( program_t*, const uint8_t*, size_t ) ) {
    linehdr_t hdr; const uint8_t* p = tokline;
    clear_linehdr( &hdr );
    read_linehdr_raw( &p, &hdr );
    if ( hdr.length < LINEHDR_SIZE ) {
        return false;
    }
    uint16_t toklen = (uint16_t)( hdr.length - LINEHDR_SIZE );
    if ( hdr.lineno == LINENO_NONE ) {
        // direct mode
        return directmode( pgm, p, toklen );
    }
    line_t* line = 0; line_t* prev = 0;
    bool found = find_line( pgm, hdr.lineno, &line, &prev );
    if ( toklen == 1U && *p == TOK_EOL ) {
        // line only contains line number: delete line (if it doesn't exist, nothing happens)
        if ( found ) {
            delete_line( pgm, line );
        }
        return true;
    }
    line_t* newline = create_line( hdr.lineno, p, toklen );
    if ( newline == 0 ) {
        return false;
    }
    if ( found ) {
        // replace existing line in place
        newline->prev = line->prev;
        newline->next = line->next;
        if ( line->prev ) {
            line->prev->next = newline;
        } else {
            pgm->first = newline;
        }
        if ( line->next ) {
            line->next->prev = newline;
        } else {
            pgm->last = newline;
        }
        free( line );
    } else {
        link_line( pgm, prev, newline );
    }
    return true;
}

void list_program( program_t* pgm, uint32_t lineno_first, uint32_t lineno_last, void (*printer)( const char* ) ) {
    buf_t* buf = create_buffer();
    if ( buf == 0 ) {
        return;
    }
    for ( line_t* line = pgm->first; line; line = line->next ) {
        if ( ( lineno_first != LINENO_NONE && line->lineno < lineno_first ) ||
             ( lineno_last  != LINENO_NONE && line->lineno > lineno_last  ) ) {
            continue;
        }
        pgmiter_t iter; clear_iter( &iter, pgm );
        iter_load_line( &iter, line );
        clear_buffer( buf );
        if ( !detokenize_line( buf, iter.tok, &iter.hdr ) ) {
            break;
        }
        printer( buf->buffer );
    }
    delete_buffer( buf );
}

bool save_program( program_t* pgm, FILE* out, bool ascii ) {
    buf_t* buf = ascii ? create_buffer() : 0;
    if ( ascii && buf == 0 ) {
        return false;
    }
    bool ok = true;
    for ( line_t* line = pgm->first; line && ok; line = line->next ) {
        pgmiter_t iter; clear_iter( &iter, pgm );
        iter_load_line( &iter, line );
        if ( ascii ) {
            clear_buffer( buf );
            ok = detokenize_line( buf, iter.tok, &iter.hdr ) && fputs( buf->buffer, out ) >= 0 && fputc( '\n', out ) != EOF;
        } else {
            uint8_t hdr[LINEHDR_SIZE]; uint8_t* p = hdr;
            emit_linehdr_raw( &p, &iter.hdr );
            ok = fwrite( hdr, 1U, LINEHDR_SIZE, out ) == LINEHDR_SIZE &&
                ( line->toklen == 0 || fwrite( line->tokens, 1U, line->toklen, out ) == line->toklen );
        }
    }
    if ( buf ) {
        delete_buffer( buf );
    }
    return ok && !ferror( out );
}
