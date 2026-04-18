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

static bool eat_uint16( const char** pp, uint16_t* target ) {
    int n = 0;
    if ( sscanf( *pp, "%" SCNu16 "%n", target, &n ) >= 1 ) {
        *pp += n;
        return true;
    }
    return false;
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

static bool eat_lit( char** pp, char target[256], int beg, int end ) {
    int n = 0; const char* p = *pp; char fmt[16];
    if ( end ) {
        snprintf( fmt, 16U, "%c%%255[^%c]%c%%n", beg, end, end );
    } else {
        // NUL-terminated means read to end of string
        // we scan until the hopefully nonexistent character 255
        // this avoids having to guess whether the implementation supports [^\0]
        // this is hopefully enough for this tiny interpreter
        snprintf( fmt, 16U, "%c%%255[^\277]%%n", beg, end );
    }
    while ( *p == ' ' ) ++p;
    if ( sscanf( p, fmt, target, &n ) >= 1 ) {
        p += n; *pp = p;
        return true;
    }
    return false;
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

static bool eat_strlit( char** pp, char target[256] ) {
    return eat_lit( pp, target, '"', '"' );
}

static void emit_strlit( uint8_t** pp, const char source[256] ) {
    emit_lit( pp, source, TOK_STRLIT );
}

static bool read_strlit( const uint8_t** pp, char target[256] ) {
    return read_lit( pp, target, TOK_STRLIT );
}

static bool eat_shllit( char** pp, char target[256] ) {
    return eat_lit( pp, target, '`', '`' );
}

static void emit_shllit( uint8_t** pp, const char source[256] ) {
    emit_lit( pp, source, TOK_SHLLIT );
}

static bool read_shllit( const uint8_t** pp, char target[256] ) {
    return read_lit( pp, target, TOK_SHLLIT );
}

static bool eat_quolit( char** pp, char target[256] ) {
    return eat_lit( pp, target, '\'', '\0' );
}

static void emit_quolit( uint8_t** pp, const char source[256] ) {
    emit_lit( pp, source, TOK_QUOLIT );
}

static bool read_strlit( const uint8_t** pp, char target[256] ) {
    return read_lit( pp, target, TOK_QUOLIT );
}

static bool eat_brklit( char** pp, char target[256] ) {
    return eat_lit( pp, target, '[', ']' );
}

static void emit_brklit( uint8_t** pp, const char source[256] ) {
    emit_lit( pp, source, TOK_BRKLIT );
}

static bool read_brklit( const uint8_t** pp, char target[256] ) {
    return read_lit( pp, target, TOK_BRKLIT );
}

static bool eat_brclit( char** pp, char target[256] ) {
    return eat_lit( pp, target, '{', '}' );
}

static void emit_brclit( uint8_t** pp, const char source[256] ) {
    emit_lit( pp, source, TOK_BRCLIT );
}

static bool read_brclit( const uint8_t** pp, char target[256] ) {
    return read_lit( pp, target, TOK_BRCLIT );
}
