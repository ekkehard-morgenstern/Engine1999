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

// -- single character tokens -----------------------------------------------

static bool eat_sngchrtok( const char** pp, uint8_t* ptok ) {
    const char* p = *pp;
    switch ( *p ) {
        case TOK_PLING: // ! pling
        case TOK_LATTICE: // # lattice
        case TOK_LPAREN: // ( left parenthesis
        case TOK_RPAREN: // ) right parenthesis
        case TOK_PLUS: // + operator
        case TOK_COMMA: // , comma
        case TOK_MINUS: // - operator
        case TOK_DIV: // / operator
        case TOK_COLON: // : colon
        case TOK_SEMIC: // ; semicolon
        case TOK_EQ: // = operator
        case TOK_BACKSL: // \ operator (integer division)
        case TOK_POW: // ^ operator (**)
        case TOK_COLUMN: // | column
        case TOK_TILDE: // ~ tilde
            *ptok = *p++;
            break;
        default:
            return false;
    }
    *pp = p;
    return true;
}

// -- uint16 ----------------------------------------------------------------

static bool eat_uint16( const char** pp, uint16_t* target ) {
    int n = 0;
    if ( sscanf( *pp, "%" SCNu16 "%n", target, &n ) >= 1 ) {
        *pp += n;
        return true;
    }
    return false;
}

static bool print_uint16( char** pp, size_t* premain, uint16_t source ) {
    int rv = snprintf( *pp, *premain, "%" PRIu16, source );
    if ( rv < 0 ) return false; // error
    if ( rv >= (int) (*premain) ) return false; // cut off
    *pp += rv; *premain -= rv;
    return true;
}

static void emit_uint16( uint8_t** pp, uint16_t source ) {
    uint8_t* p = *pp;
    *p++ = (uint8_t)( source >> UINT8_C(8) );
    *p++ = (uint8_t) source;
    *pp = p;
}

static void read_uint16( const uint8_t** pp, uint16_t* target ) {
    const uint8_t* p = *pp;
    *target = *p++;
    *target = ( ( *target ) << UINT8_C(8) ) | *p++;
    *pp = p;
}

// -- linehdr_t -------------------------------------------------------------

static void emit_linehdr( uint8_t** pp, const linehdr_t* source ) {
    emit_uint16( pp, source->nextoffs );
    emit_uint16( pp, source->prevoffs );
    emit_uint16( pp, source->lineno   );
    emit_uint16( pp, source->length   );
}

static void read_linehdr( const uint8_t** pp, linehdr_t* target ) {
    read_uint16( pp, target->nextoffs );
    read_uint16( pp, target->prevoffs );
    read_uint16( pp, target->lineno   );
    read_uint16( pp, target->length   );
}

// -- identifiers -----------------------------------------------------------

static bool eat_ident( const char** pp, char target[256] ) {
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

static bool print_ident( char** pp, size_t* premain, const char source[256] ) {
    int rv = snprintf( *pp, *premain, "%s", source );
    if ( rv < 0 ) return false; // error
    if ( rv >= (int) (*premain) ) return false; // cut off
    *pp += rv; *premain -= rv;
    return true;
}

static void emit_ident( uint8_t** pp, const char source[256] ) {
    uint8_t* p = *pp; size_t len = strlen( source );
    *p++ = TOK_IDENT;
    *p++ = (uint8_t) len;
    if ( len ) {
        memcpy( p, source, len );
        p += len;
    }
    *pp = p;
}

static bool read_ident( const uint8_t** pp, char target[256] ) {
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

static bool eat_lit( const char** pp, char target[256], int beg, int end ) {
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

static bool print_lit( char** pp, size_t* premain, const char source[256], int beg, int end ) {
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

static void emit_lit( uint8_t** pp, const char source[256], int tok ) {
    uint8_t* p = *pp; size_t len = strlen( source );
    *p++ = tok;
    *p++ = (uint8_t) len;
    if ( len ) {
        memcpy( p, source, len );
        p += len;
    }
    *pp = p;
}

static bool read_lit( const uint8_t** pp, char target[256], int tok ) {
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

static bool eat_numlit( const char** pp, char target[256], int* pbase ) {
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
    if ( base < 2 || base > 36 ) return false;
    const char* p0 = p;
    int ndig = 0;
    for (;;) {
        char   c = *p++;
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
    memcpy( target, p0, ndig );
    target[ndig] = '\0';
    *pbase = base;
    *pp = p;
    return true;
}

static bool print_numlit( char** pp, size_t* premain, const char source[256], int base ) {
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

static void emit_numlit( uint8_t** pp, const char source[256], int tok ) {
    emit_lit( pp, source, tok );
}

static bool read_numlit( const uint8_t** pp, char target[256], int tok ) {
    return read_lit( pp, target, tok );
}

// -- decimal literals ------------------------------------------------------

static bool print_declit( char** pp, size_t* premain, const char source[256] ) {
    return print_numlit( pp, premain, source, 10 );
}

static void emit_declit( uint8_t** pp, const char source[256], int tok ) {
    emit_numlit( pp, source, TOK_DECLIT );
}

static bool read_declit( const uint8_t** pp, char target[256], int tok ) {
    return read_numlit( pp, target, TOK_DECLIT );
}

// -- hexadecimal literals --------------------------------------------------

static bool print_hexlit( char** pp, size_t* premain, const char source[256] ) {
    return print_numlit( pp, premain, source, 16 );
}

static void emit_hexlit( uint8_t** pp, const char source[256], int tok ) {
    emit_numlit( pp, source, TOK_HEXLIT );
}

static bool read_hexlit( const uint8_t** pp, char target[256], int tok ) {
    return read_numlit( pp, target, TOK_HEXLIT );
}

// -- octal literals --------------------------------------------------------

static bool print_octlit( char** pp, size_t* premain, const char source[256] ) {
    return print_numlit( pp, premain, source, 8 );
}

static void emit_octlit( uint8_t** pp, const char source[256], int tok ) {
    emit_numlit( pp, source, TOK_OCTLIT );
}

static bool read_octlit( const uint8_t** pp, char target[256], int tok ) {
    return read_numlit( pp, target, TOK_OCTLIT );
}

// -- quaternary literals ---------------------------------------------------

static bool print_qualit( char** pp, size_t* premain, const char source[256] ) {
    return print_numlit( pp, premain, source, 4 );
}

static void emit_qualit( uint8_t** pp, const char source[256], int tok ) {
    emit_numlit( pp, source, TOK_QUALIT );
}

static bool read_qualit( const uint8_t** pp, char target[256], int tok ) {
    return read_numlit( pp, target, TOK_QUALIT );
}

// -- binary literals -------------------------------------------------------

static bool print_binlit( char** pp, size_t* premain, const char source[256] ) {
    return print_numlit( pp, premain, source, 2 );
}

static void emit_binlit( uint8_t** pp, const char source[256], int tok ) {
    emit_numlit( pp, source, TOK_BINLIT );
}

static bool read_binlit( const uint8_t** pp, char target[256], int tok ) {
    return read_numlit( pp, target, TOK_BINLIT );
}

// -- string literals -------------------------------------------------------

static bool eat_strlit( const char** pp, char target[256] ) {
    return eat_lit( pp, target, '"', '"' );
}

static bool print_strlit( char** pp, size_t* premain, const char source[256] ) {
    return print_lit( pp, premain, source, '"', '"' );
}

static void emit_strlit( uint8_t** pp, const char source[256] ) {
    emit_lit( pp, source, TOK_STRLIT );
}

static bool read_strlit( const uint8_t** pp, char target[256] ) {
    return read_lit( pp, target, TOK_STRLIT );
}

// -- shell literals --------------------------------------------------------

static bool eat_shllit( const char** pp, char target[256] ) {
    return eat_lit( pp, target, '`', '`' );
}

static bool print_shllit( char** pp, size_t* premain, const char source[256] ) {
    return print_lit( pp, premain, source, '`', '`' );
}

static void emit_shllit( uint8_t** pp, const char source[256] ) {
    emit_lit( pp, source, TOK_SHLLIT );
}

static bool read_shllit( const uint8_t** pp, char target[256] ) {
    return read_lit( pp, target, TOK_SHLLIT );
}

// -- quote literals (comments) ---------------------------------------------

static bool eat_quolit( const char** pp, char target[256] ) {
    return eat_lit( pp, target, '\'', '\0' );
}

static bool print_quolit( char** pp, size_t* premain, const char source[256] ) {
    return print_lit( pp, premain, source, '\'', '\0' );
}

static void emit_quolit( uint8_t** pp, const char source[256] ) {
    emit_lit( pp, source, TOK_QUOLIT );
}

static bool read_quolit( const uint8_t** pp, char target[256] ) {
    return read_lit( pp, target, TOK_QUOLIT );
}

// -- bracket literals ------------------------------------------------------

static bool eat_brklit( const char** pp, char target[256] ) {
    return eat_lit( pp, target, '[', ']' );
}

static bool print_brklit( char** pp, size_t* premain, const char source[256] ) {
    return print_lit( pp, premain, source, '[', ']' );
}

static void emit_brklit( uint8_t** pp, const char source[256] ) {
    emit_lit( pp, source, TOK_BRKLIT );
}

static bool read_brklit( const uint8_t** pp, char target[256] ) {
    return read_lit( pp, target, TOK_BRKLIT );
}

// -- brace literals --------------------------------------------------------

static bool eat_brclit( const char** pp, char target[256] ) {
    return eat_lit( pp, target, '{', '}' );
}

static bool print_brclit( char** pp, size_t* premain, const char source[256] ) {
    return print_lit( pp, premain, source, '{', '}' );
}

static void emit_brclit( uint8_t** pp, const char source[256] ) {
    emit_lit( pp, source, TOK_BRCLIT );
}

static bool read_brclit( const uint8_t** pp, char target[256] ) {
    return read_lit( pp, target, TOK_BRCLIT );
}

// -- buffer preprocessing --------------------------------------------------

static void preprocess_buffer( char* buf ) {
    // Replace series of space and tab characters to single spaces.
    // Remove carriage return characters.
    // Convert lower case characters to upper case.
    // (All take place everywhere except within double quotes or shell quotes.)
    const char* s = buf;
    char*       d = buf;
    bool        quote = false;
    while ( *s != '\0' ) {
        if ( !quote ) {
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
            quote = !quote;
        }
        *d++ = *s++;
    }
    *d = '\0';
}
