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

static bool is_sngchrtok( char tok ) {
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

static bool eat_sngchrtok( const char** pp, uint8_t* ptok ) {
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

// -- linenode_t ------------------------------------------------------------

static void init_linenode( linenode_t* node ) {
    node->nextoffs = LINEOFFS_NONE;
    node->prevoffs = LINEOFFS_NONE;
}

static void emit_linenode_raw( uint8_t** pp, const linenode_t* source ) {
    uint8_t* p = *pp;
    emit_uint16( &p, source->nextoffs );
    emit_uint16( &p, source->prevoffs );
    *pp = p;
}

static bool emit_linenode( program_t* pgm, uint16_t offs, const linenode_t* source ) {
    if ( offs == LINEOFFS_NONE || offs >= pgm->fillpos ) {
        return false;
    }
    uint8_t* p = &pgm->memory[ offs ];
    emit_linenode_raw( &p, source );
    return true;
}

static void read_linenode_raw( const uint8_t** pp, linenode_t* target ) {
    const uint8_t* p = *pp;
    read_uint16( &p, &target->nextoffs );
    read_uint16( &p, &target->prevoffs );
    *pp = p;
}

static bool read_linenode( program_t* pgm, uint16_t offs, linenode_t* target ) {
    if ( offs == LINEOFFS_NONE || offs >= pgm->fillpos ) {
        return false;
    }
    const uint8_t* p = &pgm->memory[ offs ];
    read_linenode_raw( &p, target );
    return true;
}

static bool remove_linenode( program_t* pgm, uint16_t pos, linenode_t* outnode ) {
    // don't remove already removed node, and also don't remove head or tail nodes
    linenode_t node = LINENODE_INIT;
    if ( !read_linenode( pgm, pos, &node ) ) {
        return false;
    }
    uint16_t prevpos = node.prevoffs, nextpos = node.nextoffs;
    linenode_t prev = LINENODE_INIT, next = LINENODE_INIT;
    if ( !read_linenode( pgm, prevpos, &prev ) || !read_linenode( pgm, nextpos, &next ) ) {
        return false;
    }
    prev.nextoffs = node.nextoffs;
    next.prevoffs = node.prevoffs;
    node.nextoffs = LINEOFFS_NONE;
    node.prevoffs = LINEOFFS_NONE;
    if ( !emit_linenode( pgm, prevpos, &prev ) || !emit_linenode( pgm, nextpos, &next ) ) {
        return false;
    }
    if ( !emit_linenode( pgm, pos, &node ) ) {
        return false;
    }
    if ( outnode ) {
        *outnode = node;
    }
    return true;
}

static bool emplace_linenode( program_t* pgm, uint16_t prevpos, uint16_t pos, uint16_t nextpos, linenode_t* outnode ) {
    linenode_t node = LINENODE_INIT;
    if ( !read_linenode( pgm, pos, &node ) ) {
        return false;
    }
    linenode_t prev = LINENODE_INIT, next = LINENODE_INIT;
    if ( !read_linenode( pgm, prevpos, &prev ) || !read_linenode( pgm, nextpos, &next ) ) {
        return false;
    }
    prev.nextoffs = pos;
    node.prevoffs = prevpos;
    node.nextoffs = nextpos;
    next.prevoffs = pos;
    if ( !emit_linenode( pgm, prevpos, &prev ) || !emit_linenode( pgm, nextpos, &next ) ) {
        return false;
    }
    if ( !emit_linenode( pgm, pos, &node ) ) {
        return false;
    }
    if ( outnode ) {
        *outnode = node;
    }
    return true;
}

// -- linelist_t ------------------------------------------------------------

static void init_linelist( linelist_t* list, uint16_t pos ) {
    list->head.nextoffs = pos + sizeof(linenode_t);
    list->head.prevoffs = LINEOFFS_NONE;
    list->tail.nextoffs = LINEOFFS_NONE;
    list->tail.prevoffs = pos;
}

static bool emit_linelist( program_t* pgm, uint16_t offs, const linelist_t* source ) {
    if ( offs == LINEOFFS_NONE || offs >= pgm->fillpos ) {
        return false;
    }
    if ( !emit_linenode( pgm, offs, &source->head ) ) {
        return false;
    }
    if ( !emit_linenode( pgm, offs + sizeof(linenode_t), &source->tail ) ) {
        return false;
    }
    return true;
}

static bool read_linelist( program_t* pgm, uint16_t offs, linelist_t* target ) {
    if ( offs == LINEOFFS_NONE || offs >= pgm->fillpos ) {
        return false;
    }
    if ( !read_linenode( pgm, offs, &target->head ) ) {
        return false;
    }
    if ( !read_linenode( pgm, offs + sizeof(linenode_t), &target->tail ) ) {
        return false;
    }
    return true;
}

/* static bool linelist_empty( program_t* pgm, uint16_t offs, bool* pempty ) {
    // NOTE return value indicates success/error not state
    linelist_t list = LINELIST_INIT;
    if ( !read_linelist( pgm, offs, &list ) ) {
        return false;
    }
    if ( list.head.nextoffs == offs + sizeof(linenode_t) ) {    // head points to tail => list empty
        *pempty = true;
    } else {
        *pempty = false;
    }
    return true;
} */

static bool linelist_firstnode( program_t* pgm, uint16_t listpos, uint16_t* pnodepos ) {
    linelist_t list = LINELIST_INIT;
    if ( !read_linelist( pgm, listpos, &list ) ) {
        return false;
    }
    if ( list.head.nextoffs == listpos + sizeof(linenode_t) ) {    // head points to tail => list empty
        *pnodepos = LINEOFFS_NONE;
    } else {
        *pnodepos = list.head.nextoffs;
    }
    return true;
}

static bool linelist_lastnode( program_t* pgm, uint16_t listpos, uint16_t* pnodepos, bool wanthead ) {
    linelist_t list = LINELIST_INIT;
    if ( !read_linelist( pgm, listpos, &list ) ) {
        return false;
    }
    if ( wanthead ) {   // gimme the head node
        *pnodepos = list.tail.prevoffs;
    } else if ( list.tail.prevoffs == listpos ) {    // tail points to head => list empty
        *pnodepos = LINEOFFS_NONE;
    } else {
        *pnodepos = list.head.prevoffs;
    }
    return true;
}

static bool linelist_nextnode( program_t* pgm, uint16_t nodepos, uint16_t* pnextpos, linenode_t* outnode ) {
    linenode_t node = LINENODE_INIT;
    if ( !read_linenode( pgm, nodepos, &node ) ) {
        return false;
    }
    uint16_t nextpos = node.nextoffs;
    if ( nextpos == LINEOFFS_NONE ) {   // tail node??
        return false;
    }
    linenode_t next = LINENODE_INIT;
    if ( !read_linenode( pgm, nextpos, &next ) ) {
        return false;
    }
    if ( next.nextoffs == LINEOFFS_NONE ) { // next node is the tail node
        *pnextpos = LINEOFFS_NONE;
    } else {
        *pnextpos = nextpos;
    }
    if ( outnode ) {
        *outnode = node;
    }
    return true;
}

static bool linelist_prevnode( program_t* pgm, uint16_t nodepos, uint16_t* pprevpos, linenode_t* outnode ) {
    linenode_t node = LINENODE_INIT;
    if ( !read_linenode( pgm, nodepos, &node ) ) {
        return false;
    }
    uint16_t prevpos = node.prevoffs;
    if ( prevpos == LINEOFFS_NONE ) {   // head node??
        return false;
    }
    linenode_t prev = LINENODE_INIT;
    if ( !read_linenode( pgm, prevpos, &prev ) ) {
        return false;
    }
    if ( prev.prevoffs == LINEOFFS_NONE ) { // previous node is the head node
        *pprevpos = LINEOFFS_NONE;
    } else {
        *pprevpos = prevpos;
    }
    if ( outnode ) {
        *outnode = node;
    }
    return true;
}

static bool linelist_addtail( program_t* pgm, uint16_t listoffs, uint16_t nodeoffs, linenode_t* outnode ) {
    uint16_t lastpos = LINEOFFS_NONE;
    if ( !linelist_lastnode( pgm, listoffs, &lastpos, true ) ) {
        return false;
    }
    // the previous position is the last node in the list, or the head node of the list
    uint16_t prevpos = lastpos;
    // the next position is always the tail node of the list
    uint16_t nextpos = listoffs + sizeof(linenode_t);
    // emplace the current node between these two nodes
    return emplace_linenode( pgm, prevpos, nodeoffs, nextpos, outnode );
}

// -- linehdr_t -------------------------------------------------------------

static void clear_linehdr( linehdr_t* hdr ) {
    init_linenode( &hdr->node );
    hdr->lineno = LINENO_NONE;
    hdr->length = UINT16_C(0);
    hdr->alloc  = UINT16_C(0);
}

static void emit_linehdr_raw( uint8_t** pp, const linehdr_t* source ) {
    emit_linenode_raw( pp, &source->node );
    emit_uint16( pp, source->lineno );
    emit_uint16( pp, source->length );
    emit_uint16( pp, source->alloc  );
}

static bool emit_linehdr( program_t* pgm, uint16_t pos, const linehdr_t* source, uint8_t** outp ) {
    if ( pos == LINEOFFS_NONE || pos < sizeof(linelist_t) || pos >= pgm->fillpos ) {
        return false;
    }
    uint8_t* p = &pgm->memory[ pos ];
    emit_linehdr_raw( &p, source );
    if ( outp ) {
        *outp = p;
    }
    return true;
}

static void read_linehdr_raw( const uint8_t** pp, linehdr_t* target ) {
    read_linenode_raw( pp, &target->node );
    read_uint16( pp, &target->lineno );
    read_uint16( pp, &target->length );
    read_uint16( pp, &target->alloc  );
}

static bool read_linehdr( program_t* pgm, uint16_t pos, linehdr_t* target, const uint8_t** outp ) {
    if ( pos == LINEOFFS_NONE || pos < sizeof(linelist_t) || pos >= pgm->fillpos ) {
        return false;
    }
    const uint8_t* p = &pgm->memory[ pos ];
    read_linehdr_raw( &p, target );
    if ( outp ) {
        *outp = p;
    }
    return true;
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
    if ( rv < 0 ) {
        return false; // error
    }
    if ( rv >= (int) (*premain) ) {
        return false; // cut off
    }
    *pp += rv; *premain -= rv;
    return true;
}

static bool emit_ident( uint8_t** pp, const char source[256], size_t* premain ) {
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

static bool emit_lit( uint8_t** pp, const char source[256], int tok, size_t* premain ) {
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

static bool print_numlit( char** pp, size_t* premain, const char source[256], int base ) {
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

static bool emit_numlit( uint8_t** pp, const char source[256], int tok, size_t* premain ) {
    return emit_lit( pp, source, tok, premain );
}

static bool read_numlit( const uint8_t** pp, char target[256], int tok ) {
    return read_lit( pp, target, tok );
}

// -- decimal literals ------------------------------------------------------

static bool print_declit( char** pp, size_t* premain, const char source[256] ) {
    return print_numlit( pp, premain, source, 10 );
}

static bool emit_declit( uint8_t** pp, const char source[256], size_t* premain ) {
    return emit_numlit( pp, source, TOK_DECLIT, premain );
}

static bool read_declit( const uint8_t** pp, char target[256] ) {
    return read_numlit( pp, target, TOK_DECLIT );
}

// -- hexadecimal literals --------------------------------------------------

static bool print_hexlit( char** pp, size_t* premain, const char source[256] ) {
    return print_numlit( pp, premain, source, 16 );
}

static bool emit_hexlit( uint8_t** pp, const char source[256], size_t* premain ) {
    return emit_numlit( pp, source, TOK_HEXLIT, premain );
}

static bool read_hexlit( const uint8_t** pp, char target[256] ) {
    return read_numlit( pp, target, TOK_HEXLIT );
}

// -- octal literals --------------------------------------------------------

static bool print_octlit( char** pp, size_t* premain, const char source[256] ) {
    return print_numlit( pp, premain, source, 8 );
}

static bool emit_octlit( uint8_t** pp, const char source[256], size_t* premain ) {
    return emit_numlit( pp, source, TOK_OCTLIT, premain );
}

static bool read_octlit( const uint8_t** pp, char target[256] ) {
    return read_numlit( pp, target, TOK_OCTLIT );
}

// -- quaternary literals ---------------------------------------------------

static bool print_qualit( char** pp, size_t* premain, const char source[256] ) {
    return print_numlit( pp, premain, source, 4 );
}

static bool emit_qualit( uint8_t** pp, const char source[256], size_t* premain ) {
    return emit_numlit( pp, source, TOK_QUALIT, premain );
}

static bool read_qualit( const uint8_t** pp, char target[256] ) {
    return read_numlit( pp, target, TOK_QUALIT );
}

// -- binary literals -------------------------------------------------------

static bool print_binlit( char** pp, size_t* premain, const char source[256] ) {
    return print_numlit( pp, premain, source, 2 );
}

static bool emit_binlit( uint8_t** pp, const char source[256], size_t* premain ) {
    return emit_numlit( pp, source, TOK_BINLIT, premain );
}

static bool read_binlit( const uint8_t** pp, char target[256] ) {
    return read_numlit( pp, target, TOK_BINLIT );
}

// -- string literals -------------------------------------------------------

static bool eat_strlit( const char** pp, char target[256] ) {
    return eat_lit( pp, target, '"', '"' );
}

static bool print_strlit( char** pp, size_t* premain, const char source[256] ) {
    return print_lit( pp, premain, source, '"', '"' );
}

static bool emit_strlit( uint8_t** pp, const char source[256], size_t* premain ) {
    return emit_lit( pp, source, TOK_STRLIT, premain );
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

static bool emit_shllit( uint8_t** pp, const char source[256], size_t* premain ) {
    return emit_lit( pp, source, TOK_SHLLIT, premain );
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

static bool emit_quolit( uint8_t** pp, const char source[256], size_t* premain ) {
    return emit_lit( pp, source, TOK_QUOLIT, premain );
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

static bool emit_brklit( uint8_t** pp, const char source[256], size_t* premain ) {
    return emit_lit( pp, source, TOK_BRKLIT, premain );
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

static bool emit_brclit( uint8_t** pp, const char source[256], size_t* premain ) {
    return emit_lit( pp, source, TOK_BRCLIT, premain );
}

static bool read_brclit( const uint8_t** pp, char target[256] ) {
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
    { TOK_AMP, "&" }, { TOK_LE, "<=" }, { TOK_GE, ">=" }, { TOK_NE, "<>" }, { 0, 0 }
};

static bool is_keyword( const char* name, uint8_t* ptok ) {
    for ( int i=0; keywtbl[i].tok; ++i ) {
        if ( strcmp( keywtbl[i].name, name ) == 0 ) {
            *ptok = keywtbl[i].tok;
            return true;
        }
    }
    return false;
}

static bool is_keyword2( uint8_t tok ) {
    for ( int i=0; keywtbl[i].tok; ++i ) {
        if ( keywtbl[i].tok == tok ) {
            return true;
        }
    }
    return false;
}

static bool print_keyword( uint8_t tok, char** whereto, size_t* premain ) {
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

static bool eat_dblchrtok( const char** pp, uint8_t* ptok, const char* match, uint8_t tok ) {
    const char* p = *pp;
    if ( p[0] == match[0] && p[1] == match[1] ) {
        p += 2; *pp = p; *ptok = tok;
        return true;
    }
    return false;
}

// -- tokenization ----------------------------------------------------------

bool tokenize_line( const char* buf, uint8_t* whereto, size_t* premain, linehdr_t* phdr ) {
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

bool detokenize_line( char* buf, const uint8_t* wherefrom, size_t* premain, const linehdr_t* phdr ) {
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

// -- direct mode -----------------------------------------------------------

bool direct_mode( program_t* pgm, const uint8_t* tokens ) {
    fprintf( stderr, "direct mode called\n" );
    return false;
}

// -- program management ----------------------------------------------------

void init_program( program_t* pgm ) {
    pgm->fillpos = sizeof(linelist_t);
    linelist_t list;
    init_linelist( &list, LINELIST_POS );
    emit_linelist( pgm, LINELIST_POS, &list );
}

static void clear_iter( pgmiter_t* iter, program_t* pgm ) {
    iter->pgm = pgm;
    iter->tok = 0;
    iter->pos = LINEOFFS_NONE;
    clear_linehdr( &iter->hdr );
}

static bool iter_load_line( pgmiter_t* iter, uint16_t lineoffs ) {
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

static bool begin_iterate_program( pgmiter_t* iter, program_t* pgm ) {
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

static bool step_iterate_program( pgmiter_t* iter ) {
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

static bool get_prev_next_linenos( const pgmiter_t* iter, uint16_t* pprevlineno, uint16_t* pnextlineno ) {

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

#define FOUND_ERROR  -1
#define FOUND_NONE   0
#define FOUND_EXACT  1
#define FOUND_INSERT 2
#define FOUND_BEYOND 3

static short find_line( program_t* pgm, uint16_t lineno, pgmiter_t* piter, uint16_t* pprevno, uint16_t* pnextno ) {
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

static bool zero_line( pgmiter_t* iter ) {
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

static bool unlink_line( pgmiter_t* iter ) {
    // remove line from context
    return remove_linenode( iter->pgm, iter->pos, &iter->hdr.node );
}

static bool quick_append_line( program_t* pgm, const linehdr_t* newhdr, const uint8_t* newtokens ) {

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

static bool defrag_memory( program_t* pgm ) {
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

static bool create_line( program_t* pgm, linehdr_t* newhdr, const uint8_t* newtokens, uint16_t* poffs ) {
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

static bool emplace_line( program_t* pgm, uint16_t prevno, pgmiter_t* curr, uint16_t nextno ) {
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

static bool update_line( pgmiter_t* iter, const linehdr_t* newhdr, const uint8_t* newtokens, uint16_t prevno, uint16_t nextno ) {
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
