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

#include "basic.h"

#define STATIC

// -- single character tokens -----------------------------------------------

STATIC bool is_sngchrtok( char tok ) {
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
        case TOK_BACKSL:    // \ operator (integer division)
        case TOK_POW:       // ^ operator (**)
        case TOK_COLUMN:    // | column
        case TOK_TILDE:     // ~ tilde
            return true;
    }
    return false;
}

STATIC bool eat_sngchrtok( const char** pp, uint8_t* ptok ) {
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

STATIC bool eat_uint16( const char** pp, uint16_t* target ) {
    int n = 0;
    if ( sscanf( *pp, "%" SCNu16 "%n", target, &n ) >= 1 ) {
        *pp += n;
        return true;
    }
    return false;
}

STATIC bool print_uint16( char** pp, size_t* premain, uint16_t source ) {
    int rv = snprintf( *pp, *premain, "%" PRIu16, source );
    if ( rv < 0 ) return false; // error
    if ( rv >= (int) (*premain) ) return false; // cut off
    *pp += rv; *premain -= rv;
    return true;
}

STATIC void emit_uint16( uint8_t** pp, uint16_t source ) {
    uint8_t* p = *pp;
    *p++ = (uint8_t)( source >> UINT8_C(8) );
    *p++ = (uint8_t) source;
    *pp = p;
}

STATIC void read_uint16( const uint8_t** pp, uint16_t* target ) {
    const uint8_t* p = *pp;
    *target = *p++;
    *target = ( ( *target ) << UINT8_C(8) ) | *p++;
    *pp = p;
}

// -- linehdr_t -------------------------------------------------------------

STATIC void emit_linehdr( uint8_t** pp, const linehdr_t* source ) {
    emit_uint16( pp, source->nextoffs );
    emit_uint16( pp, source->prevoffs );
    emit_uint16( pp, source->lineno   );
    emit_uint16( pp, source->length   );
}

STATIC void read_linehdr( const uint8_t** pp, linehdr_t* target ) {
    read_uint16( pp, &target->nextoffs );
    read_uint16( pp, &target->prevoffs );
    read_uint16( pp, &target->lineno   );
    read_uint16( pp, &target->length   );
}

// -- identifiers -----------------------------------------------------------

STATIC bool eat_ident( const char** pp, char target[256] ) {
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

STATIC bool print_ident( char** pp, size_t* premain, const char source[256] ) {
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

STATIC bool emit_ident( uint8_t** pp, const char source[256], size_t* premain ) {
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

STATIC bool read_ident( const uint8_t** pp, char target[256] ) {
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

STATIC bool eat_lit( const char** pp, char target[256], int beg, int end ) {
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

STATIC bool print_lit( char** pp, size_t* premain, const char source[256], int beg, int end ) {
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

STATIC bool emit_lit( uint8_t** pp, const char source[256], int tok, size_t* premain ) {
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

STATIC bool read_lit( const uint8_t** pp, char target[256], int tok ) {
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

STATIC bool eat_numlit( const char** pp, char target[256], int* pbase ) {
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

STATIC bool print_numlit( char** pp, size_t* premain, const char source[256], int base ) {
    char fmt[16];
    int fp = 0;
    int bc = 0;
    switch ( base ) {
        case 16:    bc = 'X'; break;
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

STATIC bool emit_numlit( uint8_t** pp, const char source[256], int tok, size_t* premain ) {
    return emit_lit( pp, source, tok, premain );
}

STATIC bool read_numlit( const uint8_t** pp, char target[256], int tok ) {
    return read_lit( pp, target, tok );
}

// -- decimal literals ------------------------------------------------------

STATIC bool print_declit( char** pp, size_t* premain, const char source[256] ) {
    return print_numlit( pp, premain, source, 10 );
}

STATIC bool emit_declit( uint8_t** pp, const char source[256], size_t* premain ) {
    return emit_numlit( pp, source, TOK_DECLIT, premain );
}

STATIC bool read_declit( const uint8_t** pp, char target[256] ) {
    return read_numlit( pp, target, TOK_DECLIT );
}

// -- hexadecimal literals --------------------------------------------------

STATIC bool print_hexlit( char** pp, size_t* premain, const char source[256] ) {
    return print_numlit( pp, premain, source, 16 );
}

STATIC bool emit_hexlit( uint8_t** pp, const char source[256], size_t* premain ) {
    return emit_numlit( pp, source, TOK_HEXLIT, premain );
}

STATIC bool read_hexlit( const uint8_t** pp, char target[256] ) {
    return read_numlit( pp, target, TOK_HEXLIT );
}

// -- octal literals --------------------------------------------------------

STATIC bool print_octlit( char** pp, size_t* premain, const char source[256] ) {
    return print_numlit( pp, premain, source, 8 );
}

STATIC bool emit_octlit( uint8_t** pp, const char source[256], size_t* premain ) {
    return emit_numlit( pp, source, TOK_OCTLIT, premain );
}

STATIC bool read_octlit( const uint8_t** pp, char target[256] ) {
    return read_numlit( pp, target, TOK_OCTLIT );
}

// -- quaternary literals ---------------------------------------------------

STATIC bool print_qualit( char** pp, size_t* premain, const char source[256] ) {
    return print_numlit( pp, premain, source, 4 );
}

STATIC bool emit_qualit( uint8_t** pp, const char source[256], size_t* premain ) {
    return emit_numlit( pp, source, TOK_QUALIT, premain );
}

STATIC bool read_qualit( const uint8_t** pp, char target[256] ) {
    return read_numlit( pp, target, TOK_QUALIT );
}

// -- binary literals -------------------------------------------------------

STATIC bool print_binlit( char** pp, size_t* premain, const char source[256] ) {
    return print_numlit( pp, premain, source, 2 );
}

STATIC bool emit_binlit( uint8_t** pp, const char source[256], size_t* premain ) {
    return emit_numlit( pp, source, TOK_BINLIT, premain );
}

STATIC bool read_binlit( const uint8_t** pp, char target[256] ) {
    return read_numlit( pp, target, TOK_BINLIT );
}

// -- string literals -------------------------------------------------------

STATIC bool eat_strlit( const char** pp, char target[256] ) {
    return eat_lit( pp, target, '"', '"' );
}

STATIC bool print_strlit( char** pp, size_t* premain, const char source[256] ) {
    return print_lit( pp, premain, source, '"', '"' );
}

STATIC bool emit_strlit( uint8_t** pp, const char source[256], size_t* premain ) {
    return emit_lit( pp, source, TOK_STRLIT, premain );
}

STATIC bool read_strlit( const uint8_t** pp, char target[256] ) {
    return read_lit( pp, target, TOK_STRLIT );
}

// -- shell literals --------------------------------------------------------

STATIC bool eat_shllit( const char** pp, char target[256] ) {
    return eat_lit( pp, target, '`', '`' );
}

STATIC bool print_shllit( char** pp, size_t* premain, const char source[256] ) {
    return print_lit( pp, premain, source, '`', '`' );
}

STATIC bool emit_shllit( uint8_t** pp, const char source[256], size_t* premain ) {
    return emit_lit( pp, source, TOK_SHLLIT, premain );
}

STATIC bool read_shllit( const uint8_t** pp, char target[256] ) {
    return read_lit( pp, target, TOK_SHLLIT );
}

// -- quote literals (comments) ---------------------------------------------

STATIC bool eat_quolit( const char** pp, char target[256] ) {
    return eat_lit( pp, target, '\'', '\0' );
}

STATIC bool print_quolit( char** pp, size_t* premain, const char source[256] ) {
    return print_lit( pp, premain, source, '\'', '\0' );
}

STATIC bool emit_quolit( uint8_t** pp, const char source[256], size_t* premain ) {
    return emit_lit( pp, source, TOK_QUOLIT, premain );
}

STATIC bool read_quolit( const uint8_t** pp, char target[256] ) {
    return read_lit( pp, target, TOK_QUOLIT );
}

// -- bracket literals ------------------------------------------------------

STATIC bool eat_brklit( const char** pp, char target[256] ) {
    return eat_lit( pp, target, '[', ']' );
}

STATIC bool print_brklit( char** pp, size_t* premain, const char source[256] ) {
    return print_lit( pp, premain, source, '[', ']' );
}

STATIC bool emit_brklit( uint8_t** pp, const char source[256], size_t* premain ) {
    return emit_lit( pp, source, TOK_BRKLIT, premain );
}

STATIC bool read_brklit( const uint8_t** pp, char target[256] ) {
    return read_lit( pp, target, TOK_BRKLIT );
}

// -- brace literals --------------------------------------------------------

STATIC bool eat_brclit( const char** pp, char target[256] ) {
    return eat_lit( pp, target, '{', '}' );
}

STATIC bool print_brclit( char** pp, size_t* premain, const char source[256] ) {
    return print_lit( pp, premain, source, '{', '}' );
}

STATIC bool emit_brclit( uint8_t** pp, const char source[256], size_t* premain ) {
    return emit_lit( pp, source, TOK_BRCLIT, premain );
}

STATIC bool read_brclit( const uint8_t** pp, char target[256] ) {
    return read_lit( pp, target, TOK_BRCLIT );
}

// -- tokenization ----------------------------------------------------------

bool tokenize_line( const char* buf, uint8_t* whereto, size_t* premain, linehdr_t* phdr ) {
    const char* s = buf; uint8_t* d = whereto; size_t remain = *premain;
    if ( remain < sizeof(linehdr_t) ) return 0;
    linehdr_t hdr; memset( &hdr, 0, sizeof(hdr) ); remain -= sizeof(linehdr_t); d += sizeof(linehdr_t);
    // optional line number
    eat_uint16( &s, &hdr.lineno );
    // main line
    while ( *s != '\0' ) {
        char item[256]; int base = 0; uint8_t tok;
        if ( eat_sngchrtok( &s, &tok ) ) {
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
        static const struct {
            bool (*eat_fn)( const char**, char [256] );
            bool (*emit_fn)( uint8_t**, const char [256], size_t* );
        } littbl[] = {
            { eat_ident , emit_ident  }, { eat_strlit, emit_strlit }, { eat_shllit, emit_shllit },
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
            return false;
        }
        continue;
    }
    *d++ = TOK_EOL;
    *premain = --remain;
    hdr.length = d - whereto;
    d = whereto;
    emit_linehdr( &d, &hdr );
    *phdr = hdr;
    return true;
}

// -- detokenization --------------------------------------------------------

bool detokenize_line( char* buf, const uint8_t* wherefrom, size_t* premain, const linehdr_t* phdr ) {
    const uint8_t* s = wherefrom; size_t remain = *premain;
    char* d = buf;
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
