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
    list_program_hl( pgm, lineno_first, lineno_last, printer, 0, 0 );
}

void list_program_hl( program_t* pgm, uint32_t lineno_first, uint32_t lineno_last, void (*printer)( const char* ),
    hlstyle_t style, void* usrdata ) {
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
        if ( !detokenize_line_hl( buf, iter.tok, &iter.hdr, style, usrdata ) ) {
            break;
        }
        printer( buf->buffer );
    }
    delete_buffer( buf );
}

static const uint8_t pgmfile_header[PGMFILE_HEADER_SIZE] = { 0x89, 'E', '9', '9', PGMFILE_VERSION };

bool read_program_header( FILE* in, uint8_t* pversion ) {
    *pversion = 0;
    long start = ftell( in );
    uint8_t hdr[PGMFILE_HEADER_SIZE];
    size_t got = fread( hdr, 1U, PGMFILE_HEADER_SIZE, in );
    if ( got >= PGMFILE_MAGIC_SIZE && memcmp( hdr, pgmfile_header, PGMFILE_MAGIC_SIZE ) == 0 ) {
        if ( got == PGMFILE_HEADER_SIZE ) {
            *pversion = hdr[PGMFILE_MAGIC_SIZE];
            if ( *pversion == PGMFILE_VERSION ) {
                return true;
            }
        }
        return false;
    }
    if ( start >= 0 ) {
        fseek( in, start, SEEK_SET );
    }
    return false;
}

static bool is_word_char( char c ) {
    return ( c >= 'A' && c <= 'Z' ) || ( c >= 'a' && c <= 'z' ) || ( c >= '0' && c <= '9' ) || c == '_' || c == '.' ||
        c == '$' || c == '%' || c == '!';
}

// copies the word at *pp (after blanks) to the buffer; false if there is none
static bool next_word( const char** pp, char* word, size_t size ) {
    const char* p = *pp;
    while ( *p == ' ' ) {
        ++p;
    }
    size_t n = 0;
    while ( is_word_char( *p ) ) {
        if ( n + 1U < size ) {
            word[ n++ ] = *p;
        }
        ++p;
    }
    word[ n ] = '\0';
    *pp = p;
    return n > 0;
}

bool find_label_line( program_t* pgm, const char* label, uint32_t* plineno ) {
    buf_t* buf = create_buffer();
    if ( buf == 0 ) {
        return false;
    }
    bool found = false;
    size_t labellen = strlen( label );
    size_t wordsize = labellen + 2U < 8U ? 8U : labellen + 2U;      // longer words can't match: they are cut off behind the label
    char* word = (char*) malloc( wordsize );
    for ( line_t* line = pgm->first; line && word && !found; line = line->next ) {
        pgmiter_t iter; clear_iter( &iter, pgm );
        iter_load_line( &iter, line );
        clear_buffer( buf );
        if ( !detokenize_line( buf, iter.tok, &iter.hdr ) ) {
            break;
        }
        // the listing text starts with the line number; skip strings, shell literals and comments
        const char* p = buf->buffer;
        while ( *p && *p != '\'' && !found ) {
            if ( *p == '"' || *p == '`' ) {
                char quote = *p++;
                while ( *p && *p != quote ) {
                    ++p;
                }
                if ( *p ) {
                    ++p;
                }
            } else if ( is_word_char( *p ) ) {
                next_word( &p, word, wordsize );
                if ( strcmp( word, "LABEL" ) == 0 && next_word( &p, word, wordsize ) && strcmp( word, label ) == 0 ) {
                    *plineno = line->lineno;
                    found = true;
                }
            } else {
                ++p;
            }
        }
    }
    free( word );
    delete_buffer( buf );
    return found;
}

void merge_programs( program_t* dst, program_t* src ) {
    line_t* prev = 0;           // the last line of dst before the position of the line being moved
    line_t* cur = dst->first;   // the first line of dst that is not before it
    while ( src->first != 0 ) {
        line_t* line = src->first;
        unlink_line( src, line );
        while ( cur != 0 && cur->lineno < line->lineno ) {
            prev = cur;
            cur = cur->next;
        }
        if ( cur != 0 && cur->lineno == line->lineno ) {
            line_t* next = cur->next;
            delete_line( dst, cur );
            cur = next;
        }
        link_line( dst, prev, line );
        prev = line;
    }
}

bool find_label_clash( program_t* a, program_t* b, char* name, size_t namesize ) {
    buf_t* buf = create_buffer();
    size_t wordsize = 65600U;
    char* word = (char*) malloc( wordsize );
    bool clash = false;
    if ( buf == 0 || word == 0 ) {
        delete_buffer( buf );
        free( word );
        return false;
    }
    for ( line_t* line = b->first; line && !clash; line = line->next ) {
        pgmiter_t iter; clear_iter( &iter, b );
        iter_load_line( &iter, line );
        clear_buffer( buf );
        if ( !detokenize_line( buf, iter.tok, &iter.hdr ) ) {
            break;
        }
        const char* p = buf->buffer;        // as in find_label_line()
        while ( *p && *p != '\'' && !clash ) {
            if ( *p == '"' || *p == '`' ) {
                char quote = *p++;
                while ( *p && *p != quote ) {
                    ++p;
                }
                if ( *p ) {
                    ++p;
                }
            } else if ( is_word_char( *p ) ) {
                next_word( &p, word, wordsize );
                if ( strcmp( word, "LABEL" ) == 0 && next_word( &p, word, wordsize ) ) {
                    uint32_t lineno = 0; line_t* found = 0;
                    if ( find_label_line( a, word, &lineno ) && !find_line( b, lineno, &found, 0 ) ) {
                        snprintf( name, namesize, "%s", word );
                        clash = true;
                    }
                }
            } else {
                ++p;
            }
        }
    }
    free( word );
    delete_buffer( buf );
    return clash;
}

size_t delete_lines( program_t* pgm, uint32_t first, uint32_t last ) {
    size_t count = 0;
    line_t* next;
    for ( line_t* line = pgm->first; line; line = next ) {
        next = line->next;
        if ( ( first == LINENO_NONE || line->lineno >= first ) && ( last == LINENO_NONE || line->lineno <= last ) ) {
            delete_line( pgm, line );
            ++count;
        }
    }
    return count;
}

// -- RENUM -------------------------------------------------------------------

typedef struct _renummap_t {
    const uint32_t* olds;       // ascending
    const uint32_t* news;
    size_t          count;
} renummap_t;

static uint32_t renum_map( const renummap_t* map, uint32_t lineno ) {
    size_t lo = 0, hi = map->count;
    while ( lo < hi ) {
        size_t mid = lo + ( hi - lo ) / 2U;
        if ( map->olds[mid] < lineno ) {
            lo = mid + 1U;
        } else {
            hi = mid;
        }
    }
    return lo < map->count && map->olds[lo] == lineno ? map->news[lo] : lineno;
}

// what the scanner expects next
#define EXP_NONE    0
#define EXP_NUM     1       // a line number (or a label) comes next
#define EXP_RANGE   2       // like EXP_NUM; LIST/DELETE: a minus and a second number may follow

// copies the program text after the line number to <out>, with the line number references mapped; sets *pchanged
static bool renum_text( const char* text, const renummap_t* map, buf_t* out, bool* pchanged ) {
    int expect = EXP_NONE;
    bool commalist = false;     // GOTO/GOSUB: a comma continues the list
    bool rangeopen = false;     // LIST/DELETE: a minus continues the range
    bool pendingminus = false;  // a number of a range was mapped; a minus and the second number may follow
    char prev[16] = "";
    const char* p = text;
    while ( *p ) {
        if ( *p == '"' || *p == '`' ) {
            char quote = *p;
            const char* q = p + 1;
            while ( *q && *q != quote ) {
                ++q;
            }
            if ( *q ) {
                ++q;
            }
            if ( writeto_buffer( out, p, (size_t)( q - p ) ) < 0 ) {
                return false;
            }
            p = q;
            expect = EXP_NONE;
        } else if ( *p == '\'' ) {
            return writeto_buffer( out, p, strlen( p ) ) >= 0;      // comment: the rest of the line
        } else if ( *p >= '0' && *p <= '9' ) {
            const char* q = p;
            while ( *q >= '0' && *q <= '9' ) {
                ++q;
            }
            bool plain = !is_word_char( *q ) && *q != '.' && ( p == text || !is_word_char( p[-1] ) );
            if ( expect != EXP_NONE && plain && q - p < 10 ) {
                char digits[16];
                memcpy( digits, p, (size_t)( q - p ) );
                digits[ q - p ] = '\0';
                uint32_t oldno = (uint32_t) strtoul( digits, 0, 10 );
                uint32_t newno = renum_map( map, oldno );
                if ( newno != oldno ) {
                    *pchanged = true;
                }
                if ( printto_buffer( out, "%u", (unsigned) newno ) < 0 ) {
                    return false;
                }
                // what may follow the number
                const char* r = q;
                while ( *r == ' ' ) {
                    ++r;
                }
                if ( commalist && *r == ',' ) {
                    expect = EXP_NUM;
                } else if ( rangeopen && expect == EXP_RANGE && *r == '-' ) {
                    expect = EXP_NONE;
                    pendingminus = true;
                    rangeopen = false;
                } else {
                    expect = EXP_NONE;
                }
            } else {
                if ( writeto_buffer( out, p, (size_t)( q - p ) ) < 0 ) {
                    return false;
                }
                expect = EXP_NONE;
            }
            p = q;
        } else if ( is_word_char( *p ) ) {
            const char* q = p;
            while ( is_word_char( *q ) ) {
                ++q;
            }
            size_t len = (size_t)( q - p );
            char word[16];
            snprintf( word, sizeof(word), "%.*s", len < sizeof(word) ? (int) len : (int) sizeof(word) - 1, p );
            bool gosub = strcmp( word, "GOTO" ) == 0 || strcmp( word, "GOSUB" ) == 0 ||
                ( strcmp( prev, "GO" ) == 0 && ( strcmp( word, "TO" ) == 0 || strcmp( word, "SUB" ) == 0 ) );
            bool range = strcmp( word, "LIST" ) == 0 || strcmp( word, "DELETE" ) == 0;
            bool single = strcmp( word, "THEN" ) == 0 || strcmp( word, "ELSE" ) == 0 || strcmp( word, "RESTORE" ) == 0 ||
                strcmp( word, "RUN" ) == 0;
            expect = gosub || single ? EXP_NUM : range ? EXP_RANGE : EXP_NONE;
            pendingminus = false;
            commalist = gosub;
            rangeopen = range;
            snprintf( prev, sizeof(prev), "%s", word );
            if ( writeto_buffer( out, p, len ) < 0 ) {
                return false;
            }
            p = q;
        } else {
            if ( *p == '-' && ( expect == EXP_RANGE || pendingminus ) ) {
                // "LIST -50" (the first number is missing) or the minus of "LIST 10-50"
                expect = EXP_NUM;
                rangeopen = false;
                pendingminus = false;
            } else if ( *p != ' ' && *p != ',' ) {
                expect = EXP_NONE;
                pendingminus = false;
            } else if ( *p == ',' && !commalist ) {
                expect = EXP_NONE;
                pendingminus = false;
            }
            if ( writeto_buffer( out, p, 1U ) < 0 ) {
                return false;
            }
            ++p;
        }
    }
    return true;
}

int renumber_program( program_t* pgm, uint32_t first, uint32_t last, uint32_t start, uint32_t step ) {
    // the selected lines form a contiguous part of the list
    size_t count = 0;
    line_t* firstsel = 0; line_t* lastsel = 0;
    for ( line_t* line = pgm->first; line; line = line->next ) {
        if ( ( first == LINENO_NONE || line->lineno >= first ) && ( last == LINENO_NONE || line->lineno <= last ) ) {
            if ( firstsel == 0 ) {
                firstsel = line;
            }
            lastsel = line;
            ++count;
        }
    }
    if ( count == 0 ) {
        return RENUM_OK;
    }
    if ( step == 0 || (uint64_t) start + (uint64_t)( count - 1U ) * step > (uint64_t) LINENO_MAX ) {
        return RENUM_OVERFLOW;
    }
    uint32_t lastnew = (uint32_t)( start + ( count - 1U ) * step );
    if ( ( firstsel->prev && firstsel->prev->lineno >= start ) || ( lastsel->next && lastsel->next->lineno <= lastnew ) ) {
        return RENUM_OVERLAP;
    }
    uint32_t* olds = (uint32_t*) malloc( 2U * count * sizeof(uint32_t) );
    if ( olds == 0 ) {
        return RENUM_MEMORY;
    }
    uint32_t* news = olds + count;
    size_t n = 0;
    for ( line_t* line = firstsel; n < count; line = line->next, ++n ) {
        olds[n] = line->lineno;
        news[n] = (uint32_t)( start + n * step );
    }
    renummap_t map = { olds, news, count };

    program_t result; init_program( &result );
    buf_t* text = create_buffer(); buf_t* rest = create_buffer(); buf_t* tokbuf = create_buffer();
    int status = RENUM_OK;
    if ( text == 0 || rest == 0 || tokbuf == 0 ) {
        status = RENUM_MEMORY;
    }
    for ( line_t* line = pgm->first; line && status == RENUM_OK; line = line->next ) {
        uint32_t newno = renum_map( &map, line->lineno );
        pgmiter_t iter; clear_iter( &iter, pgm );
        iter_load_line( &iter, line );
        clear_buffer( text ); clear_buffer( rest );
        bool changed = false;
        line_t* copy = 0;
        if ( !detokenize_line( text, iter.tok, &iter.hdr ) ) {
            status = RENUM_FAILED;
            break;
        }
        // the listing starts with the line number and a blank
        const char* body = text->buffer;
        while ( *body >= '0' && *body <= '9' ) {
            ++body;
        }
        if ( !renum_text( body, &map, rest, &changed ) ) {
            status = RENUM_MEMORY;
            break;
        }
        if ( !changed ) {
            copy = create_line( newno, line->tokens, line->toklen );        // the tokens stay as they are
        } else {
            char* full = (char*) malloc( rest->fill + 16U );
            if ( full == 0 ) {
                status = RENUM_MEMORY;
                break;
            }
            snprintf( full, rest->fill + 16U, "%u%.*s", (unsigned) newno, (int) rest->fill, rest->buffer );     // rest is not NUL-terminated
            preprocess_buffer( full );
            linehdr_t hdr;
            clear_buffer( tokbuf );
            if ( !tokenize_line( full, tokbuf, &hdr ) || hdr.lineno != newno ) {
                status = RENUM_FAILED;
            } else {
                copy = create_line( newno, (const uint8_t*) tokbuf->buffer + LINEHDR_SIZE, (uint16_t)( hdr.length - LINEHDR_SIZE ) );
            }
            free( full );
        }
        if ( status == RENUM_OK ) {
            if ( copy == 0 ) {
                status = RENUM_MEMORY;
            } else {
                link_line( &result, result.last, copy );
            }
        }
    }
    free( olds );
    if ( text ) { delete_buffer( text ); }
    if ( rest ) { delete_buffer( rest ); }
    if ( tokbuf ) { delete_buffer( tokbuf ); }
    if ( status != RENUM_OK ) {
        free_program( &result );
        return status;
    }
    free_program( pgm );
    *pgm = result;
    return RENUM_OK;
}

bool save_program( program_t* pgm, FILE* out, bool ascii ) {
    buf_t* buf = ascii ? create_buffer() : 0;
    if ( ascii && buf == 0 ) {
        return false;
    }
    bool ok = true;
    if ( !ascii ) {
        ok = fwrite( pgmfile_header, 1U, PGMFILE_HEADER_SIZE, out ) == PGMFILE_HEADER_SIZE;
    }
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
