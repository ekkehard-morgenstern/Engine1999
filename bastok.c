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

#include "bastok.h"
#include "baslin.h"

// -- single character tokens -----------------------------------------------

bool is_sngchrtok( char tok ) {
    switch ( tok ) {
        case TOK_SPACE:     // space
        case TOK_PLING:     // ! pling
        case TOK_LATTICE:   // # lattice
        case TOK_STRING:    // $ string type sigil
        case TOK_INTEGER:   // % integer type sigil
        case TOK_LPAREN:    // ( left parenthesis
        case TOK_RPAREN:    // ) right parenthesis
        case TOK_PLUS:      // + operator
        case TOK_COMMA:     // , comma
        case TOK_MINUS:     // - operator
        case TOK_DIV:       // / operator
        case TOK_COLON:     // : colon
        case TOK_SEMIC:     // ; semicolon
        case TOK_EQ:        // = operator
        case TOK_ADDROF:    // @ address-of operator
        case TOK_BACKSL:    // \ operator (integer division)
        case TOK_POW:       // ^ operator (**)
        case TOK_COLUMN:    // | column
        case TOK_TILDE:     // ~ tilde
            return true;
    }
    return false;
}

bool eat_sngchrtok( const char** pp, uint8_t* ptok ) {
    const char* p = *pp;
    if ( is_sngchrtok( *p ) ) {
        *ptok = *p++;
    } else {
        return false;
    }
    *pp = p;
    return true;
}

// -- uint16 ----------------------------------------------------------------

bool eat_uint16( const char** pp, uint16_t* target ) {
    int n = 0;
    if ( sscanf( *pp, "%" SCNu16 "%n", target, &n ) >= 1 ) {
        *pp += n;
        return true;
    }
    return false;
}

bool print_uint16( char** pp, size_t* premain, uint16_t source ) {
    int rv = snprintf( *pp, *premain, "%" PRIu16, source );
    if ( rv < 0 ) return false; // error
    if ( rv >= (int) (*premain) ) return false; // cut off
    *pp += rv; *premain -= rv;
    return true;
}

void emit_uint16( uint8_t** pp, uint16_t source ) {
    uint8_t* p = *pp;
    *p++ = (uint8_t)( source >> UINT8_C(8) );
    *p++ = (uint8_t) source;
    *pp = p;
}

void read_uint16( const uint8_t** pp, uint16_t* target ) {
    const uint8_t* p = *pp;
    *target = *p++;
    *target = ( ( *target ) << UINT8_C(8) ) | *p++;
    *pp = p;
}
// -- identifiers -----------------------------------------------------------

bool eat_ident( const char** pp, char target[256] ) {
    int n = 0; const char* p = *pp;
    while ( *p == ' ' ) ++p;
    if ( *p >= 'A' && *p <= 'Z' ) {
        if ( sscanf( p, "%255[A-Z0-9_]%n", target, &n ) >= 1 ) {
            p += n; *pp = p;
            return true;
        }
    }
    return false;
}

bool print_ident( char** pp, size_t* premain, const char source[256] ) {
    int rv = snprintf( *pp, *premain, "%s", source );
    if ( rv < 0 ) {
        return false; // error
    }
    if ( rv >= (int) (*premain) ) {
        return false; // cut off
    }
    *pp += rv; *premain -= rv;
    return true;
}

bool emit_ident( uint8_t** pp, const char source[256], size_t* premain ) {
    uint8_t* p = *pp; size_t len = strlen( source );
    if ( *premain <= len + 2U ) {
        return false;
    }
    *p++ = TOK_IDENT;
    *p++ = (uint8_t) len;
    if ( len ) {
        memcpy( p, source, len );
        p += len;
    }
    *premain -= p - *pp;
    *pp = p;
    return true;
}

bool read_ident( const uint8_t** pp, char target[256] ) {
    const uint8_t* p = *pp;
    if ( *p++ != TOK_IDENT ) return false;
    size_t len = *p++;
    if ( len ) {
        memcpy( target, p, len );
        p += len;
    }
    target[ len ] = '\0';
    *pp = p;
    return true;
}

// -- literals (general) ----------------------------------------------------

bool eat_lit( const char** pp, char target[256], int beg, int end ) {
    int n = 0; const char* p = *pp; char fmt[16];
    if ( end ) {
        snprintf( fmt, 16U, "%c%%255[^%c]%c%%n", beg, end, end );
    } else {
        // NUL-terminated means read to end of string
        // we scan until the hopefully nonexistent character 255
        // this avoids having to guess whether the implementation supports [^\0]
        // this is hopefully enough for this tiny interpreter
        snprintf( fmt, 16U, "%c%%255[^\277]%%n", beg );
    }
    while ( *p == ' ' ) ++p;
    if ( sscanf( p, fmt, target, &n ) >= 1 ) {
        p += n; *pp = p;
        return true;
    }
    return false;
}

bool print_lit( char** pp, size_t* premain, const char source[256], int beg, int end ) {
    char fmt[16];
    int fp = 0;
    fmt[fp++] = beg;
    fmt[fp++] = '%';
    fmt[fp++] = 's';
    if ( end ) {
        fmt[fp++] = end;
    }
    fmt[fp] = '\0';
    int rv = snprintf( *pp, *premain, fmt, source );
    if ( rv < 0 ) return false; // error
    if ( rv >= (int) (*premain) ) return false; // cut off
    *pp += rv; *premain -= rv;
    return true;
}

bool emit_lit( uint8_t** pp, const char source[256], int tok, size_t* premain ) {
    uint8_t* p = *pp; size_t len = strlen( source );
    if ( *premain <= len + 2U ) {
        return false;
    }
    *p++ = tok;
    *p++ = (uint8_t) len;
    if ( len ) {
        memcpy( p, source, len );
        p += len;
    }
    *pp = p;
    return true;
}

bool read_lit( const uint8_t** pp, char target[256], int tok ) {
    const uint8_t* p = *pp;
    if ( *p++ != tok ) return false;
    size_t len = *p++;
    if ( len ) {
        memcpy( target, p, len );
        p += len;
    }
    target[ len ] = '\0';
    *pp = p;
    return true;
}

// -- numeric literals (general) --------------------------------------------

bool eat_numlit( const char** pp, char target[256], int* pbase ) {
    const char* p = *pp;
    while ( *p == ' ' ) ++p;
    int base = *pbase;
    if ( base == 0 && *p == '&' ) {
        ++p;
        switch ( *p ) {
            case 'H':   base = 16; break;
            case 'D':   base = 10; break;
            case 'O':   base = 8; break;
            case 'Q':   base = 4; break;
            case 'B':   base = 2; break;
            default:    return false;
        }
        ++p;
    }
    if ( base == 0 ) {
        base = 10;
    }
    if ( base < 2 || base > 36 ) return false;
    const char* p0 = p;
    int ndig = 0;
    for (;;) {
        char   c = *p;
        int8_t v = INT8_C(-1);
        if ( c >= '0' && c <= '9' ) {
            v = c - '0';
        } else if ( c >= 'A' && c <= 'Z' ) {
            v = c - 'A' + 10;
        } else if ( ndig ) {
            break;
        } else {
            return false;
        }
        if ( ++ndig > 255 ) {
            return false;
        }
        if ( v >= base ) {
            return false;
        }
        ++p;
    }
    if ( base == 10 ) { // floating-point support (decimal only)
        if ( *p == '.' ) {  // fraction
            ++p;
            if ( *p < '0' || *p > '9' ) {
                return false;
            }
            do {
                ++p;
            } while ( *p >= '0' && *p <= '9' );
        }
        if ( *p == 'E' ) {  // exponent
            ++p;
            if ( *p == '+' || *p == '-' ) {
                ++p;
            }
            if ( *p < '0' || *p > '9' ) {
                return false;
            }
            do {
                ++p;
            } while ( *p >= '0' && *p <= '9' );
        }
    }
    ndig = (int)( p - p0 );
    memcpy( target, p0, ndig );
    target[ndig] = '\0';
    *pbase = base;
    *pp = p;
    return true;
}

bool print_numlit( char** pp, size_t* premain, const char source[256], int base ) {
    char fmt[16];
    int fp = 0;
    int bc = 0;
    switch ( base ) {
        case 16:    bc = 'H'; break;
        case 10:    break;
        case 8:     bc = 'O'; break;
        case 4:     bc = 'Q'; break;
        case 2:     bc = 'B'; break;
    }
    if ( bc ) {
        fmt[fp++] = '&';
        fmt[fp++] = bc;
    }
    fmt[fp++] = '%';
    fmt[fp++] = 's';
    fmt[fp] = '\0';
    int rv = snprintf( *pp, *premain, fmt, source );
    if ( rv < 0 ) return false; // error
    if ( rv >= (int) (*premain) ) return false; // cut off
    *pp += rv; *premain -= rv;
    return true;
}

bool emit_numlit( uint8_t** pp, const char source[256], int tok, size_t* premain ) {
    return emit_lit( pp, source, tok, premain );
}

bool read_numlit( const uint8_t** pp, char target[256], int tok ) {
    return read_lit( pp, target, tok );
}

// -- decimal literals ------------------------------------------------------

bool print_declit( char** pp, size_t* premain, const char source[256] ) {
    return print_numlit( pp, premain, source, 10 );
}

bool emit_declit( uint8_t** pp, const char source[256], size_t* premain ) {
    return emit_numlit( pp, source, TOK_DECLIT, premain );
}

bool read_declit( const uint8_t** pp, char target[256] ) {
    return read_numlit( pp, target, TOK_DECLIT );
}

// -- hexadecimal literals --------------------------------------------------

bool print_hexlit( char** pp, size_t* premain, const char source[256] ) {
    return print_numlit( pp, premain, source, 16 );
}

bool emit_hexlit( uint8_t** pp, const char source[256], size_t* premain ) {
    return emit_numlit( pp, source, TOK_HEXLIT, premain );
}

bool read_hexlit( const uint8_t** pp, char target[256] ) {
    return read_numlit( pp, target, TOK_HEXLIT );
}

// -- octal literals --------------------------------------------------------

bool print_octlit( char** pp, size_t* premain, const char source[256] ) {
    return print_numlit( pp, premain, source, 8 );
}

bool emit_octlit( uint8_t** pp, const char source[256], size_t* premain ) {
    return emit_numlit( pp, source, TOK_OCTLIT, premain );
}

bool read_octlit( const uint8_t** pp, char target[256] ) {
    return read_numlit( pp, target, TOK_OCTLIT );
}

// -- quaternary literals ---------------------------------------------------

bool print_qualit( char** pp, size_t* premain, const char source[256] ) {
    return print_numlit( pp, premain, source, 4 );
}

bool emit_qualit( uint8_t** pp, const char source[256], size_t* premain ) {
    return emit_numlit( pp, source, TOK_QUALIT, premain );
}

bool read_qualit( const uint8_t** pp, char target[256] ) {
    return read_numlit( pp, target, TOK_QUALIT );
}

// -- binary literals -------------------------------------------------------

bool print_binlit( char** pp, size_t* premain, const char source[256] ) {
    return print_numlit( pp, premain, source, 2 );
}

bool emit_binlit( uint8_t** pp, const char source[256], size_t* premain ) {
    return emit_numlit( pp, source, TOK_BINLIT, premain );
}

bool read_binlit( const uint8_t** pp, char target[256] ) {
    return read_numlit( pp, target, TOK_BINLIT );
}

// -- string literals -------------------------------------------------------

bool eat_strlit( const char** pp, char target[256] ) {
    return eat_lit( pp, target, '"', '"' );
}

bool print_strlit( char** pp, size_t* premain, const char source[256] ) {
    return print_lit( pp, premain, source, '"', '"' );
}

bool emit_strlit( uint8_t** pp, const char source[256], size_t* premain ) {
    return emit_lit( pp, source, TOK_STRLIT, premain );
}

bool read_strlit( const uint8_t** pp, char target[256] ) {
    return read_lit( pp, target, TOK_STRLIT );
}

// -- shell literals --------------------------------------------------------

bool eat_shllit( const char** pp, char target[256] ) {
    return eat_lit( pp, target, '`', '`' );
}

bool print_shllit( char** pp, size_t* premain, const char source[256] ) {
    return print_lit( pp, premain, source, '`', '`' );
}

bool emit_shllit( uint8_t** pp, const char source[256], size_t* premain ) {
    return emit_lit( pp, source, TOK_SHLLIT, premain );
}

bool read_shllit( const uint8_t** pp, char target[256] ) {
    return read_lit( pp, target, TOK_SHLLIT );
}

// -- quote literals (comments) ---------------------------------------------

bool eat_quolit( const char** pp, char target[256] ) {
    return eat_lit( pp, target, '\'', '\0' );
}

bool print_quolit( char** pp, size_t* premain, const char source[256] ) {
    return print_lit( pp, premain, source, '\'', '\0' );
}

bool emit_quolit( uint8_t** pp, const char source[256], size_t* premain ) {
    return emit_lit( pp, source, TOK_QUOLIT, premain );
}

bool read_quolit( const uint8_t** pp, char target[256] ) {
    return read_lit( pp, target, TOK_QUOLIT );
}

// -- bracket literals ------------------------------------------------------

bool eat_brklit( const char** pp, char target[256] ) {
    return eat_lit( pp, target, '[', ']' );
}

bool print_brklit( char** pp, size_t* premain, const char source[256] ) {
    return print_lit( pp, premain, source, '[', ']' );
}

bool emit_brklit( uint8_t** pp, const char source[256], size_t* premain ) {
    return emit_lit( pp, source, TOK_BRKLIT, premain );
}

bool read_brklit( const uint8_t** pp, char target[256] ) {
    return read_lit( pp, target, TOK_BRKLIT );
}

// -- brace literals --------------------------------------------------------

bool eat_brclit( const char** pp, char target[256] ) {
    return eat_lit( pp, target, '{', '}' );
}

bool print_brclit( char** pp, size_t* premain, const char source[256] ) {
    return print_lit( pp, premain, source, '{', '}' );
}

bool emit_brclit( uint8_t** pp, const char source[256], size_t* premain ) {
    return emit_lit( pp, source, TOK_BRCLIT, premain );
}

bool read_brclit( const uint8_t** pp, char target[256] ) {
    return read_lit( pp, target, TOK_BRCLIT );
}

// -- keywords --------------------------------------------------------------

static const struct {
    uint8_t     tok;
    const char* name;
} keywtbl[] = {
    { TOK_PRINT, "PRINT" }, { TOK_INPUT, "INPUT" }, { TOK_PUT, "PUT" }, { TOK_GET, "GET" }, { TOK_LIST, "LIST" },
    { TOK_READ, "READ" }, { TOK_DATA, "DATA" }, { TOK_RESTORE, "RESTORE" }, { TOK_SAVE, "SAVE" }, { TOK_RUN, "RUN" },
    { TOK_AUTO, "AUTO" }, { TOK_RENUM, "RENUM" }, { TOK_DELETE, "DELETE" }, { TOK_MERGE, "MERGE" }, { TOK_CHAIN, "CHAIN" },
    { TOK_FILES, "FILES" }, { TOK_NEW, "NEW" }, { TOK_CLEAR, "CLEAR" }, { TOK_ERASE, "ERASE" }, { TOK_EDIT, "EDIT" },
    { TOK_LOAD, "LOAD" }, { TOK_SHOW, "SHOW" }, { TOK_WARRANTY, "WARRANTY" }, { TOK_COPYING, "COPYING" },
    { TOK_DIM, "DIM" }, { TOK_DEF, "DEF" }, { TOK_INT, "INT" }, { TOK_STR, "STR" }, { TOK_FLT, "FLT" }, { TOK_OPTION, "OPTION" },
    { TOK_BASE, "BASE" }, { TOK_ASC, "ASC" }, { TOK_VAL, "VAL" }, { TOK_LEFT, "LEFT" }, { TOK_MID, "MID" },
    { TOK_RIGHT, "RIGHT" }, { TOK_INKEY, "INKEY" }, { TOK_BIN, "BIN" }, { TOK_QUA, "QUA" }, { TOK_OCT, "OCT" },
    { TOK_DEC, "DEC" }, { TOK_HEX, "HEX" }, { TOK_GOTO, "GOTO" }, { TOK_GOSUB, "GOSUB" }, { TOK_GO, "GO" },
    { TOK_TO, "TO" }, { TOK_SUB, "SUB" }, { TOK_RETURN, "RETURN" }, { TOK_IF, "IF" }, { TOK_UNLESS, "UNLESS" },
    { TOK_THEN, "THEN" }, { TOK_ELSE, "ELSE" }, { TOK_ENDIF, "ENDIF" }, { TOK_ENDUNLESS, "ENDUNLESS" }, { TOK_END, "END" },
    { TOK_FOR, "FOR" }, { TOK_STEP, "STEP" }, { TOK_NEXT, "NEXT" }, { TOK_REPEAT, "REPEAT" }, { TOK_WHILE, "WHILE" },
    { TOK_UNTIL, "UNTIL" }, { TOK_WEND, "WEND" }, { TOK_UEND, "UEND" }, { TOK_POP, "POP" }, { TOK_AFTER, "AFTER" },
    { TOK_EVERY, "EVERY" }, { TOK_ON, "ON" }, { TOK_OFF, "OFF" }, { TOK_SYMBOL, "SYMBOL" }, { TOK_FN, "FN" }, { TOK_LET, "LET" },
    { TOK_NOT, "NOT" }, { TOK_AND, "AND" }, { TOK_OR, "OR" }, { TOK_XOR, "XOR" }, { TOK_NAND, "NAND" }, { TOK_NOR, "NOR" },
    { TOK_XNOR, "XNOR" }, { TOK_LSHIFT, "LSHIFT" }, { TOK_RSHIFT, "RSHIFT" },
    { TOK_AMP, "&" }, { TOK_LE, "<=" }, { TOK_GE, ">=" }, { TOK_NE, "<>" }, { 0, 0 }
};

bool is_keyword( const char* name, uint8_t* ptok ) {
    for ( int i=0; keywtbl[i].tok; ++i ) {
        if ( strcmp( keywtbl[i].name, name ) == 0 ) {
            *ptok = keywtbl[i].tok;
            return true;
        }
    }
    return false;
}

bool is_keyword2( uint8_t tok ) {
    for ( int i=0; keywtbl[i].tok; ++i ) {
        if ( keywtbl[i].tok == tok ) {
            return true;
        }
    }
    return false;
}

bool print_keyword( uint8_t tok, char** whereto, size_t* premain ) {
    for ( int i=0; keywtbl[i].tok; ++i ) {
        if ( keywtbl[i].tok == tok ) {
            size_t len = strlen( keywtbl[i].name );
            if ( *premain <= len ) {
                return false;
            }
            memcpy( *whereto, keywtbl[i].name, len );
            *whereto += len; *premain -= len;
            return true;
        }
    }
    return false;
}

// -- double-character tokens -----------------------------------------------

bool eat_dblchrtok( const char** pp, uint8_t* ptok, const char* match, uint8_t tok ) {
    const char* p = *pp;
    if ( p[0] == match[0] && p[1] == match[1] ) {
        p += 2; *pp = p; *ptok = tok;
        return true;
    }
    return false;
}

// -- tokenization ----------------------------------------------------------

bool tokenize_line( const char* buf, uint8_t* whereto, size_t* premain, struct _linehdr_t* phdr ) {
    const char* s = buf; uint8_t* d = whereto; size_t remain = *premain;
    if ( remain < sizeof(linehdr_t) ) return 0;
    linehdr_t hdr; clear_linehdr( &hdr );
    remain -= sizeof(linehdr_t); d += sizeof(linehdr_t);
    // optional line number
    if ( eat_uint16( &s, &hdr.lineno ) ) {
        if ( hdr.lineno > LINENO_MAX ) {
            return false;
        }
    }
    // main line
    while ( *s != '\0' ) {
        char item[256]; int base = 0; uint8_t tok;
        if ( eat_sngchrtok( &s, &tok ) || eat_dblchrtok( &s, &tok, "<=", TOK_LE ) ||
             eat_dblchrtok( &s, &tok, "<>", TOK_NE ) || eat_dblchrtok( &s, &tok, "><", TOK_NE ) ||
             eat_dblchrtok( &s, &tok, ">=", TOK_GE ) ) {
            if ( remain <= 1U ) {
                return false;
            }
            *d++ = tok;
            continue;
        }
        if ( eat_numlit( &s, item, &base ) ) {
            if ( base == 10 && item[1] == '\0' ) {
                if ( remain <= 1U ) {
                    return false;
                }
                *d++ = item[0]; // '0'..'9' = TOK_DEC0..9
                continue;
            }
            static const struct {
                int base;
                bool (*emit_fn)( uint8_t**, const char [256], size_t* );
            } numemittbl[] = {
                { 16, emit_hexlit }, { 10, emit_declit }, { 8, emit_octlit },
                {  4, emit_qualit }, {  2, emit_binlit }, { 0, 0 }
            };
            bool found = false;
            for ( int i=0; numemittbl[i].base; ++i ) {
                if ( numemittbl[i].base == base ) {
                    if ( !numemittbl[i].emit_fn( &d, item, &remain ) ) {
                        return false;
                    }
                    found = true;
                    break;
                }
            }
            if ( !found ) {
                return false;
            }
            continue;
        }
        if ( eat_ident( &s, item ) ) {
            if ( is_keyword( item, &tok ) ) {
                if ( remain <= 1U ) {
                    return false;
                }
                *d++ = tok;
                continue;
            }
            if ( !emit_ident( &d, item, &remain ) ) {
                return false;
            }
            continue;
        }
        static const struct {
            bool (*eat_fn)( const char**, char [256] );
            bool (*emit_fn)( uint8_t**, const char [256], size_t* );
        } littbl[] = {
            { eat_strlit, emit_strlit }, { eat_shllit, emit_shllit },
            { eat_quolit, emit_quolit }, { eat_brklit, emit_brklit }, { eat_brclit, emit_brclit },
            { 0, 0 }
        };
        bool found = false;
        for ( int i=0; littbl[i].eat_fn; ++i ) {
            if ( littbl[i].eat_fn( &s, item ) ) {
                if ( !littbl[i].emit_fn( &d, item, &remain ) ) {
                    return false;
                }
                found = true;
                break;
            }
        }
        if ( !found ) {
            if ( *s == '&' ) {
                if ( remain <= 1U ) {
                    return false;
                }
                *d++ = TOK_AMP; --remain; ++s;
                continue;
            } else if ( *s == '?' ) {
                if ( remain <= 2U ) {
                    return false;
                }
                *d++ = TOK_PRINT; *d++ = TOK_SPACE; remain -= 2; ++s;
                continue;
            }
            return false;
        }
        continue;
    }
    *d++ = TOK_EOL;
    *premain = --remain;
    hdr.length = d - whereto;
    hdr.alloc  = d - whereto;
    d = whereto;
    emit_linehdr_raw( &d, &hdr );
    *phdr = hdr;
    return true;
}

// -- detokenization --------------------------------------------------------

bool detokenize_line( char* buf, const uint8_t* wherefrom, size_t* premain, const struct _linehdr_t* phdr ) {
    const uint8_t* s = wherefrom; size_t remain = *premain;
    char* d = buf;
    if ( phdr->lineno != LINENO_NONE ) {
        if ( !print_uint16( &d, &remain, phdr->lineno ) ) {
            return false;
        }
    }
    while ( *s != TOK_EOL ) {
        uint8_t tok = *s; char item[256];
        static const struct {
            uint8_t tok;
            bool (*read_fn)( const uint8_t**, char [256] );
            bool (*print_fn)( char**, size_t*, const char* );
        } littbl[] = {
            { TOK_IDENT , read_ident , print_ident  }, { TOK_STRLIT, read_strlit, print_strlit },
            { TOK_HEXLIT, read_hexlit, print_hexlit }, { TOK_DECLIT, read_declit, print_declit },
            { TOK_OCTLIT, read_octlit, print_octlit }, { TOK_QUALIT, read_qualit, print_qualit },
            { TOK_BINLIT, read_binlit, print_binlit }, { TOK_SHLLIT, read_shllit, print_shllit },
            { TOK_QUOLIT, read_quolit, print_quolit }, { TOK_BRKLIT, read_brklit, print_brklit },
            { TOK_BRCLIT, read_brclit, print_brclit }, { 0, 0, 0 }
        };
        bool found = false;
        for ( int i=0; littbl[i].tok; ++i ) {
            if ( littbl[i].tok == tok ) {
                if ( !littbl[i].read_fn( &s, item ) ) {
                    return false;
                }
                if ( !littbl[i].print_fn( &d, &remain, item ) ) {
                    return false;
                }
                found = true;
                break;
            }
        }
        if ( !found ) {
            if ( is_sngchrtok( tok ) || ( tok >= TOK_DEC0 && tok <= TOK_DEC9 ) ) {
                if ( remain <= 1U ) {
                    return false;
                }
                *d++ = tok; ++s; --remain;
            } else if ( is_keyword2( tok ) ) {
                if ( !print_keyword( tok, &d, &remain ) ) {
                    return false;
                }
                ++s;
            } else {
                return false;
            }
        }
    }
    *d = '\0'; --remain;
    *premain = remain;
    return true;
}

// -- buffer preprocessing --------------------------------------------------

void preprocess_buffer( char* buf ) {
    // Replace series of space and tab characters to single spaces.
    // Remove carriage return characters.
    // Convert lower case characters to upper case.
    // (All take place everywhere except within double quotes or shell quotes.)
    const char* s = buf;
    char*       d = buf;
    int         quote = 0;
    while ( *s != '\0' ) {
        if ( quote == 0 ) {
            if ( *s == ' ' || *s == '\t' ) {
                do {
                    ++s;
                } while ( *s == ' ' || *s == '\t' );
                *d++ = ' ';
                continue;
            }
            if ( *s == '\r' ) {
                ++s;
                continue;
            }
            if ( *s >= 'a' && *s <= 'z' ) {
                *d++ = ( *s++ - 'a' ) + 'A';
                continue;
            }
        }
        if ( *s == '"' ) {
            quote ^= 1;
        }
        if ( *s == '`' ) {
            quote ^= 2;
        }
        *d++ = *s++;
    }
    *d = '\0';
}
