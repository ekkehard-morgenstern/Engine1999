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

    ADDITIONAL PERMISSION (GPLv3 section 7): the Engine1999 Runtime Exception
        applies. Programs written in the built-in programming language, and
        executables made from them with BUILD, do not fall under this license
        as the exception describes (see the file LICENSE-EXCEPTION).

    CONTACT INFO:
        E-Mail: ekkehard@ekkehardmorgenstern.de
        Mail: Ekkehard Morgenstern, Mozartstr. 1, D-76744 Woerth am Rhein,
              Germany, Europe
*/

#include "synhl.h"

// the text the way preprocess_buffer() leaves it, but the same length (no blank collapsing), so the offsets stay: upper case outside strings, shell literals
// and comments; REM at the start of a statement is a comment (written as an apostrophe: its three letters stay in the class COMMENT, see below)
static void make_upper( const char* text, size_t len, char* out, size_t* remstart ) {
    int quote = 0;
    bool comment = false, stmt = true;
    *remstart = (size_t) -1;
    size_t i = 0;
    while ( i < len && ( text[i] == ' ' || text[i] == '\t' ) ) {      // the line number and the blanks behind it do not count as a statement
        out[i] = text[i];
        ++i;
    }
    if ( i < len && text[i] >= '0' && text[i] <= '9' ) {
        while ( i < len && ( ( text[i] >= '0' && text[i] <= '9' ) || text[i] == ' ' || text[i] == '\t' ) ) {
            out[i] = text[i];
            ++i;
        }
    } else {
        i = 0;
    }
    for ( ; i < len; ++i ) {
        char c = text[i];
        out[i] = c;
        if ( quote == 0 && !comment ) {
            if ( c == '\'' ) {
                comment = true;
            } else if ( stmt && ( c == 'R' || c == 'r' ) && i + 2 < len && ( text[i+1] == 'E' || text[i+1] == 'e' ) && ( text[i+2] == 'M' || text[i+2] == 'm' ) &&
                    ( i + 3 == len || text[i+3] == ' ' || text[i+3] == '\t' ) ) {
                *remstart = i;
                comment = true;
            } else if ( c >= 'a' && c <= 'z' ) {
                out[i] = (char)( c - 'a' + 'A' );
                stmt = false;
                continue;
            }
        }
        if ( !comment && quote == 0 && ( c == ' ' || c == '\t' ) ) {
            continue;
        }
        if ( !comment ) {
            if ( c == '"' ) {
                quote ^= 1;
            }
            if ( c == '`' ) {
                quote ^= 2;
            }
            if ( quote == 0 ) {
                stmt = ( c == ':' );
            }
        }
    }
}

static void fill( uint8_t* cls, size_t from, size_t to, hlclass_t c ) {
    for ( size_t i = from; i < to; ++i ) {
        cls[i] = (uint8_t) c;
    }
}

void synhl_line( const char* text, size_t len, uint8_t* cls ) {
    memset( cls, HL_NONE, len );
    if ( len == 0 ) {
        return;
    }
    char* buf = (char*) malloc( len + 1U );
    if ( buf == 0 ) {
        return;
    }
    size_t remstart;
    make_upper( text, len, buf, &remstart );
    buf[len] = '\0';
    if ( remstart != (size_t) -1 ) {                   // REM: the rest of the line is a comment (the tokenizer sees an apostrophe)
        fill( cls, remstart, len, HL_COMMENT );
        len = remstart;
        buf[len] = '\0';
    }
    rbuf_t src;
    init_rbuf( &src, buf, len );
    while ( src.rpos < src.size && src.buffer[ src.rpos ] == ' ' ) {
        ++src.rpos;
    }
    uint32_t lineno = 0;
    size_t p0 = src.rpos;
    if ( eat_uint32( &src, &lineno ) ) {
        fill( cls, p0, src.rpos, HL_LINENO );
    }
    char* item = 0;
    while ( src.rpos < src.size ) {
        size_t p = src.rpos;
        uint8_t tok = 0;
        hlclass_t c = HL_PUNCT;
        if ( src.buffer[p] == ' ' ) {
            ++src.rpos;
            continue;
        }
        if ( eat_dblchrtok( &src, &tok, "<=", TOK_LE ) || eat_dblchrtok( &src, &tok, "<>", TOK_NE ) || eat_dblchrtok( &src, &tok, "><", TOK_NE ) ||
             eat_dblchrtok( &src, &tok, ">=", TOK_GE ) || eat_dblchrtok( &src, &tok, "**", TOK_POW ) ) {
            c = HL_OPERATOR;
        } else if ( eat_sngchrtok( &src, &tok ) ) {
            c = sngchr_hlclass( tok );
        } else if ( ( free( item ), item = 0, eat_numlit( &src, &item, &(int){ 0 } ) ) ) {
            c = HL_NUMBER;
        } else if ( eat_ident( &src, &item ) ) {
            uint8_t t = 0, xid = 0;
            if ( is_keyword( item, &t ) ) {
                c = keyword_hlclass( t );
            } else if ( is_xkeyword( item, &xid ) ) {
                c = xkeyword_hlclass( xid );
            } else {
                c = HL_IDENT;
                char s = src.rpos < src.size ? src.buffer[ src.rpos ] : '\0';
                if ( s == '$' || s == '%' ) {
                    ++src.rpos;
                }
            }
        } else if ( eat_strlit( &src, &item ) ) {
            c = HL_STRING;
        } else if ( eat_shllit( &src, &item ) ) {
            c = HL_SHELL;
        } else if ( eat_quolit( &src, &item ) ) {
            c = HL_COMMENT;
        } else if ( eat_brklit( &src, &item ) || eat_brclit( &src, &item ) ) {
            c = HL_STRING;
        } else if ( src.buffer[p] == '"' || src.buffer[p] == '`' || src.buffer[p] == '[' || src.buffer[p] == '{' ) {      // unterminated: to the end of the line
            c = src.buffer[p] == '`' ? HL_SHELL : HL_STRING;
            src.rpos = src.size;
        } else if ( src.buffer[p] == '&' ) {
            ++src.rpos;
            c = HL_OPERATOR;
        } else if ( src.buffer[p] == '?' ) {
            ++src.rpos;
            c = HL_STATEMENT;
        } else {
            ++src.rpos;                                // not a token: punctuation, so the scan goes on
        }
        if ( src.rpos <= p ) {
            src.rpos = p + 1U;
        }
        fill( cls, p, src.rpos > len ? len : src.rpos, c );
    }
    free( item );
    free( buf );
}
