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
        case TOK_MULT:      // * operator
        case TOK_PLUS:      // + operator
        case TOK_COMMA:     // , comma
        case TOK_MINUS:     // - operator
        case TOK_DIV:       // / operator
        case TOK_COLON:     // : colon
        case TOK_SEMIC:     // ; semicolon
        case TOK_LT:        // < operator
        case TOK_EQ:        // = operator
        case TOK_GT:        // > operator
        case TOK_ADDROF:    // @ address-of operator
        case TOK_BACKSL:    // \ operator (integer division)
        case TOK_POW:       // ^ operator (**)
        case TOK_COLUMN:    // | column
        case TOK_TILDE:     // ~ tilde
            return true;
    }
    return false;
}

bool eat_sngchrtok( rbuf_t* rbuf, uint8_t* ptok ) {
    if ( rbuf->rpos < rbuf->size && is_sngchrtok( rbuf->buffer[ rbuf->rpos ] ) ) {
        *ptok = (uint8_t) rbuf->buffer[ rbuf->rpos++ ];
        return true;
    }
    return false;
}

// -- helpers ---------------------------------------------------------------

#define MAX_ITEMLEN     65535U      // maximum length of an identifier or literal

// character at the read position + offset, or NUL when beyond the end
static char rb_peek( const rbuf_t* rbuf, size_t offs ) {
    size_t pos = rbuf->rpos + offs;
    if ( pos >= rbuf->size ) {
        return '\0';
    }
    return rbuf->buffer[ pos ];
}

// replaces *ptarget with a NUL-terminated copy of the given data
static bool store_string( char** ptarget, const char* data, size_t len ) {
    char* copy = (char*) malloc( len + 1U );
    if ( copy == 0 ) {
        fprintf( stderr, "? out of memory\n" );
        return false;
    }
    if ( len ) {
        memcpy( copy, data, len );
    }
    copy[len] = '\0';
    free( *ptarget );
    *ptarget = copy;
    return true;
}

static bool emit_byte( buf_t* buf, uint8_t value ) {
    return writeto_buffer( buf, &value, 1U ) >= 0;
}

// emits <tok> <len.16> <data...>
static bool emit_tagged( buf_t* buf, uint8_t tok, const char* source, size_t len ) {
    uint8_t hdr[3];
    hdr[0] = tok;
    hdr[1] = (uint8_t)( len >> UINT8_C(8) );
    hdr[2] = (uint8_t) len;
    if ( anticipate_buffer( buf, 3U + len ) < 0 ) {
        return false;
    }
    writeto_buffer( buf, hdr, 3U );
    if ( len ) {
        writeto_buffer( buf, source, len );
    }
    return true;
}

// -- buffers ---------------------------------------------------------------

buf_t* create_buffer( void ) {
    buf_t* buf = (buf_t*) malloc( sizeof( buf_t ) );
    if ( buf == 0 ) {
ERR1:   fprintf( stderr, "? out of memory\n" );
        return 0;
    }
    buf->buffer = (char*) malloc( BUF_INIT_SIZE );
    if ( buf->buffer == 0 ) {
        free( buf );
        goto ERR1;
    }
    buf->alloc = BUF_INIT_SIZE;
    buf->fill  = 0;
    return buf;
}

void delete_buffer( buf_t* buf ) {
    if ( buf == 0 ) {
        return;
    }
    free( buf->buffer ); buf->buffer = 0;
    buf->alloc = 0; buf->fill = 0;
    free( buf );
}

void clear_buffer( buf_t* buf ) {
    buf->fill = 0;
}

bool grow_buffer( buf_t* buf, size_t requiredSize ) {
    if ( buf->alloc >= requiredSize ) {
        return true;
    }
    size_t newSize = buf->alloc ? buf->alloc : BUF_INIT_SIZE;
    while ( newSize < requiredSize ) {
        if ( newSize > SIZE_MAX / 2U ) {
            fprintf( stderr, "? cannot grow buffer\n" );
            return false;
        }
        newSize *= 2U;
    }
    char* newbuf = (char*) realloc( buf->buffer, newSize );
    if ( newbuf == 0 ) {
        fprintf( stderr, "? out of memory\n" );
        return false;
    }
    buf->buffer = newbuf;
    buf->alloc  = newSize;
    return true;
}

int printto_buffer( buf_t* buf, const char* fmt, ... ) {

    size_t oldFill = buf->fill;

    for (;;) {
        va_list ap;
        va_start( ap, fmt );
        size_t remain = buf->alloc - buf->fill;
        int ret = vsnprintf( &buf->buffer[buf->fill], remain, fmt, ap );
        va_end( ap );
        if ( ret < 0 ) { // error
            fprintf( stderr, "? buffer error\n" );
            return ret;
        }
        if ( (size_t) ret >= remain ) {    // cut off
            if ( (size_t) ret >= SIZE_MAX - buf->fill ) {
                fprintf( stderr, "? size request too large\n" );
                return -1;
            }
            if ( !grow_buffer( buf, buf->fill + (size_t) ret + 1U ) ) {
                return -1;
            }
            continue;   // try again
        }
        // success
        buf->fill += (size_t) ret;
        break;
    }

    size_t numWritten = buf->fill - oldFill;

    return (int) numWritten;
}

int writeto_buffer( buf_t* buf, const void* data, size_t size ) {

    if ( size > (size_t) INT_MAX || anticipate_buffer( buf, size ) < 0 ) {
        return -1;
    }

    if ( size ) {
        memcpy( &buf->buffer[buf->fill], data, size );
        buf->fill += size;
    }

    return (int) size;
}

int anticipate_buffer( buf_t* buf, size_t size ) {

    if ( size > (size_t) INT_MAX ) {
        fprintf( stderr, "? size request too large\n" );
        return -1;
    }
    if ( size > SIZE_MAX - buf->fill ) {
        fprintf( stderr, "? size request too large\n" );
        return -1;
    }
    if ( !grow_buffer( buf, buf->fill + size ) ) {
        return -1;
    }

    return (int) size;
}

// -- read-only buffers -----------------------------------------------------

void init_rbuf( rbuf_t* rbuf, const void* data, size_t size ) {
    rbuf->buffer = (const char*) data;
    rbuf->size   = size;
    rbuf->rpos   = 0;
}

int readfrom_buffer( rbuf_t* rbuf, void* data, size_t size ) {
    size_t remain = rbuf->size - rbuf->rpos;
    if ( remain < size || size > (size_t) INT_MAX ) {
        return -1;
    }
    if ( size ) {
        memcpy( data, &rbuf->buffer[ rbuf->rpos ], size );
        rbuf->rpos += size;
    }
    return (int) size;
}

int anticipate_rbuffer( rbuf_t* rbuf, size_t size ) {
    size_t remain = rbuf->size - rbuf->rpos;
    if ( remain < size || size > (size_t) INT_MAX ) {
        return -1;
    }
    return (int) size;
}

// -- raw 16-bit values (used by line headers) ------------------------------

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

// -- raw 24-bit values (used for line numbers) -----------------------------

void emit_uint24( uint8_t** pp, uint32_t source ) {
    uint8_t* p = *pp;
    *p++ = (uint8_t)( source >> UINT8_C(16) );
    *p++ = (uint8_t)( source >> UINT8_C(8) );
    *p++ = (uint8_t) source;
    *pp = p;
}

void read_uint24( const uint8_t** pp, uint32_t* target ) {
    const uint8_t* p = *pp;
    uint32_t v = *p++;
    v = ( v << UINT8_C(8) ) | *p++;
    v = ( v << UINT8_C(8) ) | *p++;
    *target = v;
    *pp = p;
}

// -- uint32 ----------------------------------------------------------------

bool eat_uint32( rbuf_t* rbuf, uint32_t* target ) {
    size_t offs = 0;
    while ( rb_peek( rbuf, offs ) == ' ' ) ++offs;
    char c = rb_peek( rbuf, offs );
    if ( c < '0' || c > '9' ) {
        return false;
    }
    uint64_t value = 0;
    do {
        value = value * 10U + (uint64_t)( c - '0' );
        if ( value > UINT32_MAX ) {
            return false;
        }
        c = rb_peek( rbuf, ++offs );
    } while ( c >= '0' && c <= '9' );
    *target = (uint32_t) value;
    rbuf->rpos += offs;
    return true;
}

bool print_uint32( buf_t* buf, uint32_t source ) {
    int rv = printto_buffer( buf, "%" PRIu32, source );
    if ( rv < 0 ) return false; // error
    return true;
}

bool emit_uint32( buf_t* buf, uint32_t source ) {
    uint8_t data[4];
    data[0] = (uint8_t)( source >> UINT8_C(24) );
    data[1] = (uint8_t)( source >> UINT8_C(16) );
    data[2] = (uint8_t)( source >> UINT8_C( 8) );
    data[3] = (uint8_t) source;
    if ( writeto_buffer( buf, data, 4U ) < 0 ) {
        return false;
    }
    return true;
}

bool read_uint32( rbuf_t* rbuf, uint32_t* target ) {
    uint8_t data[4];
    if ( readfrom_buffer( rbuf, data, 4U ) < 0 ) {
        return false;
    }
    *target =
        ( ( (uint32_t) data[0] ) << UINT8_C(24) ) |
        ( ( (uint32_t) data[1] ) << UINT8_C(16) ) |
        ( ( (uint32_t) data[2] ) << UINT8_C( 8) ) |
                       data[3];
    return true;
}

// -- identifiers -----------------------------------------------------------

bool eat_ident( rbuf_t* rbuf, char** ptarget ) {
    size_t offs = 0;
    while ( rb_peek( rbuf, offs ) == ' ' ) ++offs;
    char c = rb_peek( rbuf, offs );
    if ( c < 'A' || c > 'Z' ) {
        return false;
    }
    size_t beg = offs;
    do {
        c = rb_peek( rbuf, ++offs );
    } while ( ( c >= 'A' && c <= 'Z' ) || ( c >= '0' && c <= '9' ) || c == '_' );
    size_t len = offs - beg;
    if ( len > MAX_ITEMLEN ) {
        fprintf( stderr, "? identifier too long\n" );
        return false;
    }
    if ( !store_string( ptarget, &rbuf->buffer[ rbuf->rpos + beg ], len ) ) {
        return false;
    }
    rbuf->rpos += offs;
    return true;
}

bool print_ident( buf_t* buf, const char* source ) {
    size_t len = strlen( source );
    if ( len > (size_t) INT_MAX ) {
        return false;
    }
    int rv = writeto_buffer( buf, source, len );
    if ( rv < (int) len ) {
        return false; // error
    }
    return true;
}

bool emit_ident( buf_t* buf, const char* source, uint8_t tok ) {
    size_t len = strlen( source );
    if ( len > MAX_ITEMLEN ) {
        len = MAX_ITEMLEN;
    }
    if ( tok == TOK_STRIDENT || tok == TOK_INTIDENT ) {
        if ( len == MAX_ITEMLEN ) {
            --len;  // leave room for the sigil when reading back
        }
    }
    return emit_tagged( buf, tok, source, len );
}

bool ri_sigil = false;

bool read_ident( rbuf_t* rbuf, char** ptarget ) {
    if ( anticipate_rbuffer( rbuf, 3U ) < 0 ) {
        return false;
    }
    const uint8_t* p = (const uint8_t*)( &rbuf->buffer[ rbuf->rpos ] );
    uint8_t tok = p[0];
    switch ( tok ) {
        case TOK_IDENT: case TOK_NUMIDENT: case TOK_STRIDENT: case TOK_INTIDENT:
            break;
        default:
            return false;
    }
    size_t len = ( ( (size_t) p[1] ) << UINT8_C(8) ) | p[2];
    if ( anticipate_rbuffer( rbuf, 3U + len ) < 0 ) {
        return false;
    }
    char sigil = '\0';
    if ( ri_sigil ) {
        if ( tok == TOK_STRIDENT ) {
            sigil = '$';
        } else if ( tok == TOK_INTIDENT ) {
            sigil = '%';
        }
    }
    char* name = (char*) malloc( len + 2U );
    if ( name == 0 ) {
        fprintf( stderr, "? out of memory\n" );
        return false;
    }
    if ( len ) {
        memcpy( name, &p[3], len );
    }
    if ( sigil ) {
        name[ len++ ] = sigil;
    }
    name[ len ] = '\0';
    free( *ptarget );
    *ptarget = name;
    rbuf->rpos += 3U + ( len - ( sigil ? 1U : 0U ) );
    return true;
}

// -- literals (general) ----------------------------------------------------

bool eat_lit( rbuf_t* rbuf, char** ptarget, int beg, int end ) {
    size_t offs = 0;
    while ( rb_peek( rbuf, offs ) == ' ' ) ++offs;
    if ( rbuf->rpos + offs >= rbuf->size || rbuf->buffer[ rbuf->rpos + offs ] != (char) beg ) {
        return false;
    }
    ++offs;
    size_t beg_content = offs;
    for (;;) {
        size_t pos = rbuf->rpos + offs;
        if ( pos >= rbuf->size || rbuf->buffer[ pos ] == '\0' ) {
            if ( end ) {
                return false;   // unterminated
            }
            break;              // NUL-terminated means read to end of string
        }
        if ( end && rbuf->buffer[ pos ] == (char) end ) {
            break;
        }
        ++offs;
    }
    size_t len = offs - beg_content;
    if ( len > MAX_ITEMLEN ) {
        fprintf( stderr, "? literal too long\n" );
        return false;
    }
    if ( !store_string( ptarget, &rbuf->buffer[ rbuf->rpos + beg_content ], len ) ) {
        return false;
    }
    if ( end ) {
        ++offs;     // closing delimiter
    }
    rbuf->rpos += offs;
    return true;
}

bool print_lit( buf_t* buf, const char* source, int beg, int end ) {
    char fmt[16];
    int fp = 0;
    fmt[fp++] = beg;
    fmt[fp++] = '%';
    fmt[fp++] = 's';
    if ( end ) {
        fmt[fp++] = end;
    }
    fmt[fp] = '\0';
    int rv = printto_buffer( buf, fmt, source );
    if ( rv < 0 ) return false; // error
    return true;
}

bool emit_lit( buf_t* buf, const char* source, int tok ) {
    size_t len = strlen( source );
    if ( len > MAX_ITEMLEN ) len = MAX_ITEMLEN;
    return emit_tagged( buf, (uint8_t) tok, source, len );
}

bool read_lit( rbuf_t* rbuf, char** ptarget, int tok ) {
    if ( anticipate_rbuffer( rbuf, 3U ) < 0 ) {
        return false;
    }
    const uint8_t* p = (const uint8_t*)( &rbuf->buffer[ rbuf->rpos ] );
    if ( p[0] != tok ) {
        return false;
    }
    size_t len = ( ( (size_t) p[1] ) << UINT8_C(8) ) | p[2];
    if ( anticipate_rbuffer( rbuf, 3U + len ) < 0 ) {
        return false;
    }
    if ( !store_string( ptarget, (const char*) &p[3], len ) ) {
        return false;
    }
    rbuf->rpos += 3U + len;
    return true;
}

// -- numeric literals (general) --------------------------------------------

bool eat_numlit( rbuf_t* rbuf, char** ptarget, int* pbase ) {
    size_t offs = 0;
    while ( rb_peek( rbuf, offs ) == ' ' ) ++offs;
    int base = *pbase;
    if ( base == 0 && rb_peek( rbuf, offs ) == '&' ) {
        ++offs;
        switch ( rb_peek( rbuf, offs ) ) {
            case 'H':   base = 16; break;
            case 'D':   base = 10; break;
            case 'O':   base = 8; break;
            case 'Q':   base = 4; break;
            case 'B':   base = 2; break;
            default:    return false;
        }
        ++offs;
    }
    if ( base == 0 ) {
        base = 10;
    }
    if ( base < 2 || base > 36 ) return false;
    size_t beg = offs;
    size_t ndig = 0;
    for (;;) {
        char   c = rb_peek( rbuf, offs );
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
        if ( ++ndig > MAX_ITEMLEN ) {
            return false;
        }
        if ( v >= base ) {
            return false;
        }
        ++offs;
    }
    if ( base == 10 ) { // floating-point support (decimal only)
        if ( rb_peek( rbuf, offs ) == '.' ) {  // fraction
            ++offs;
            char c = rb_peek( rbuf, offs );
            if ( c < '0' || c > '9' ) {
                return false;
            }
            do {
                c = rb_peek( rbuf, ++offs );
            } while ( c >= '0' && c <= '9' );
        }
        if ( rb_peek( rbuf, offs ) == 'E' ) {  // exponent
            ++offs;
            char c = rb_peek( rbuf, offs );
            if ( c == '+' || c == '-' ) {
                c = rb_peek( rbuf, ++offs );
            }
            if ( c < '0' || c > '9' ) {
                return false;
            }
            do {
                c = rb_peek( rbuf, ++offs );
            } while ( c >= '0' && c <= '9' );
        }
    }
    if ( offs - beg > MAX_ITEMLEN ) {
        fprintf( stderr, "? numeric literal too long\n" );
        return false;
    }
    if ( !store_string( ptarget, &rbuf->buffer[ rbuf->rpos + beg ], offs - beg ) ) {
        return false;
    }
    *pbase = base;
    rbuf->rpos += offs;
    return true;
}

bool print_numlit( buf_t* buf, const char* source, int base ) {
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
    int rv = printto_buffer( buf, fmt, source );
    if ( rv < 0 ) return false; // error
    return true;
}

bool emit_numlit( buf_t* buf, const char* source, int tok ) {
    return emit_lit( buf, source, tok );
}

bool read_numlit( rbuf_t* rbuf, char** ptarget, int tok ) {
    return read_lit( rbuf, ptarget, tok );
}

// -- decimal literals ------------------------------------------------------

bool print_declit( buf_t* buf, const char* source ) {
    return print_numlit( buf, source, 10 );
}

bool emit_declit( buf_t* buf, const char* source ) {
    return emit_numlit( buf, source, TOK_DECLIT );
}

bool read_declit( rbuf_t* rbuf, char** ptarget ) {
    return read_numlit( rbuf, ptarget, TOK_DECLIT );
}

// -- hexadecimal literals --------------------------------------------------

bool print_hexlit( buf_t* buf, const char* source ) {
    return print_numlit( buf, source, 16 );
}

bool emit_hexlit( buf_t* buf, const char* source ) {
    return emit_numlit( buf, source, TOK_HEXLIT );
}

bool read_hexlit( rbuf_t* rbuf, char** ptarget ) {
    return read_numlit( rbuf, ptarget, TOK_HEXLIT );
}

// -- octal literals --------------------------------------------------------

bool print_octlit( buf_t* buf, const char* source ) {
    return print_numlit( buf, source, 8 );
}

bool emit_octlit( buf_t* buf, const char* source ) {
    return emit_numlit( buf, source, TOK_OCTLIT );
}

bool read_octlit( rbuf_t* rbuf, char** ptarget ) {
    return read_numlit( rbuf, ptarget, TOK_OCTLIT );
}

// -- quaternary literals ---------------------------------------------------

bool print_qualit( buf_t* buf, const char* source ) {
    return print_numlit( buf, source, 4 );
}

bool emit_qualit( buf_t* buf, const char* source ) {
    return emit_numlit( buf, source, TOK_QUALIT );
}

bool read_qualit( rbuf_t* rbuf, char** ptarget ) {
    return read_numlit( rbuf, ptarget, TOK_QUALIT );
}

// -- binary literals -------------------------------------------------------

bool print_binlit( buf_t* buf, const char* source ) {
    return print_numlit( buf, source, 2 );
}

bool emit_binlit( buf_t* buf, const char* source ) {
    return emit_numlit( buf, source, TOK_BINLIT );
}

bool read_binlit( rbuf_t* rbuf, char** ptarget ) {
    return read_numlit( rbuf, ptarget, TOK_BINLIT );
}

// -- string literals -------------------------------------------------------

bool eat_strlit( rbuf_t* rbuf, char** ptarget ) {
    return eat_lit( rbuf, ptarget, '"', '"' );
}

bool print_strlit( buf_t* buf, const char* source ) {
    return print_lit( buf, source, '"', '"' );
}

bool emit_strlit( buf_t* buf, const char* source ) {
    return emit_lit( buf, source, TOK_STRLIT );
}

bool read_strlit( rbuf_t* rbuf, char** ptarget ) {
    return read_lit( rbuf, ptarget, TOK_STRLIT );
}

// -- shell literals --------------------------------------------------------

bool eat_shllit( rbuf_t* rbuf, char** ptarget ) {
    return eat_lit( rbuf, ptarget, '`', '`' );
}

bool print_shllit( buf_t* buf, const char* source ) {
    return print_lit( buf, source, '`', '`' );
}

bool emit_shllit( buf_t* buf, const char* source ) {
    return emit_lit( buf, source, TOK_SHLLIT );
}

bool read_shllit( rbuf_t* rbuf, char** ptarget ) {
    return read_lit( rbuf, ptarget, TOK_SHLLIT );
}

// -- quote literals (comments) ---------------------------------------------

bool eat_quolit( rbuf_t* rbuf, char** ptarget ) {
    return eat_lit( rbuf, ptarget, '\'', '\0' );
}

bool print_quolit( buf_t* buf, const char* source ) {
    return print_lit( buf, source, '\'', '\0' );
}

bool emit_quolit( buf_t* buf, const char* source ) {
    return emit_lit( buf, source, TOK_QUOLIT );
}

bool read_quolit( rbuf_t* rbuf, char** ptarget ) {
    return read_lit( rbuf, ptarget, TOK_QUOLIT );
}

// -- bracket literals ------------------------------------------------------

bool eat_brklit( rbuf_t* rbuf, char** ptarget ) {
    return eat_lit( rbuf, ptarget, '[', ']' );
}

bool print_brklit( buf_t* buf, const char* source ) {
    return print_lit( buf, source, '[', ']' );
}

bool emit_brklit( buf_t* buf, const char* source ) {
    return emit_lit( buf, source, TOK_BRKLIT );
}

bool read_brklit( rbuf_t* rbuf, char** ptarget ) {
    return read_lit( rbuf, ptarget, TOK_BRKLIT );
}

// -- brace literals --------------------------------------------------------

bool eat_brclit( rbuf_t* rbuf, char** ptarget ) {
    return eat_lit( rbuf, ptarget, '{', '}' );
}

bool print_brclit( buf_t* buf, const char* source ) {
    return print_lit( buf, source, '{', '}' );
}

bool emit_brclit( buf_t* buf, const char* source ) {
    return emit_lit( buf, source, TOK_BRCLIT );
}

bool read_brclit( rbuf_t* rbuf, char** ptarget ) {
    return read_lit( rbuf, ptarget, TOK_BRCLIT );
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
    { TOK_LOAD, "LOAD" }, { TOK_SHOW, "SHOW" }, { TOK_WARRANTY, "WARRANTY" }, { TOK_COPYING, "COPYING" }, { TOK_LEN, "LEN" },
    { TOK_DIM, "DIM" }, { TOK_DEF, "DEF" }, { TOK_INT, "INT" }, { TOK_STR, "STR" }, { TOK_FLT, "FLT" }, { TOK_OPTION, "OPTION" },
    { TOK_BASE, "BASE" }, { TOK_ASC, "ASC" }, { TOK_VAL, "VAL" }, { TOK_FRE, "FRE" }, { TOK_LEFT, "LEFT" }, { TOK_MID, "MID" },
    { TOK_RIGHT, "RIGHT" }, { TOK_INKEY, "INKEY" }, { TOK_BIN, "BIN" }, { TOK_QUA, "QUA" }, { TOK_OCT, "OCT" },
    { TOK_DEC, "DEC" }, { TOK_HEX, "HEX" }, { TOK_GOTO, "GOTO" }, { TOK_GOSUB, "GOSUB" }, { TOK_GO, "GO" }, { TOK_TI, "TI" },
    { TOK_TO, "TO" }, { TOK_SUB, "SUB" }, { TOK_RETURN, "RETURN" }, { TOK_IF, "IF" }, { TOK_UNLESS, "UNLESS" },
    { TOK_THEN, "THEN" }, { TOK_ELSE, "ELSE" }, { TOK_ENDIF, "ENDIF" }, { TOK_ENDUNLESS, "ENDUNLESS" }, { TOK_END, "END" },
    { TOK_FOR, "FOR" }, { TOK_STEP, "STEP" }, { TOK_NEXT, "NEXT" }, { TOK_REPEAT, "REPEAT" }, { TOK_WHILE, "WHILE" },
    { TOK_UNTIL, "UNTIL" }, { TOK_WEND, "WEND" }, { TOK_UEND, "UEND" }, { TOK_POP, "POP" }, { TOK_AFTER, "AFTER" },
    { TOK_EVERY, "EVERY" }, { TOK_ON, "ON" }, { TOK_OFF, "OFF" }, { TOK_SYMBOL, "SYMBOL" }, { TOK_FN, "FN" }, { TOK_LET, "LET" },
    { TOK_NOT, "NOT" }, { TOK_AND, "AND" }, { TOK_OR, "OR" }, { TOK_XOR, "XOR" }, { TOK_NAND, "NAND" }, { TOK_NOR, "NOR" },
    { TOK_XNOR, "XNOR" }, { TOK_LSHIFT, "LSHIFT" }, { TOK_RSHIFT, "RSHIFT" }, { TOK_DYNAMIC, "DYNAMIC" }, { TOK_ASSOC, "ASSOC" },
    { TOK_LABEL, "LABEL" }, { TOK_AGAIN, "AGAIN" }, { TOK_CHR, "CHR" }, { TOK_MONITOR, "MONITOR" }, { TOK_SOUND, "SOUND" }, { TOK_PLAY, "PLAY" },
    { TOK_WAVE, "WAVE" }, { TOK_TEMPO, "TEMPO" }, { TOK_PLAYING, "PLAYING" }, { TOK_SYNC, "SYNC" },
    { TOK_PITCH, "PITCH" }, { TOK_VOLUME, "VOLUME" }, { TOK_ENVELOPE, "ENVELOPE" }, { TOK_VIBRATO, "VIBRATO" },
    { TOK_TREMOLO, "TREMOLO" }, { TOK_SFX, "SFX" }, { TOK_WAIT, "WAIT" }, { TOK_REMAIN, "REMAIN" }, { TOK_OPEN, "OPEN" }, { TOK_CLOSE, "CLOSE" }, { TOK_LINE, "LINE" }, { TOK_WRITE, "WRITE" },
    { TOK_ADDRESS, "ADDRESS" }, { TOK_EOF, "EOF" }, { TOK_LOF, "LOF" }, { TOK_LOC, "LOC" },
    { TOK_KILL, "KILL" }, { TOK_RENAME, "RENAME" }, { TOK_MKDIR, "MKDIR" }, { TOK_CHDIR, "CHDIR" }, { TOK_SEEK, "SEEK" },
    { TOK_FIELD, "FIELD" }, { TOK_LSET, "LSET" }, { TOK_RSET, "RSET" }, { TOK_GETREC, "GETREC" }, { TOK_PUTREC, "PUTREC" }, { TOK_ERR, "ERR" }, { TOK_ERL, "ERL" },
    { TOK_MKI, "MKI" }, { TOK_MKL, "MKL" }, { TOK_MKS, "MKS" }, { TOK_MKD, "MKD" },
    { TOK_CVI, "CVI" }, { TOK_CVL, "CVL" }, { TOK_CVS, "CVS" }, { TOK_CVD, "CVD" }, { TOK_AMP, "&" }, { TOK_LE, "<=" }, { TOK_GE, ">=" }, { TOK_NE, "<>" }, { 0, 0 }
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

bool print_keyword( uint8_t tok, buf_t* buf ) {
    for ( int i=0; keywtbl[i].tok; ++i ) {
        if ( keywtbl[i].tok == tok ) {
            size_t len = strlen( keywtbl[i].name );
            return writeto_buffer( buf, keywtbl[i].name, len ) >= 0;
        }
    }
    return false;
}

// -- double-character tokens -----------------------------------------------

bool eat_dblchrtok( rbuf_t* rbuf, uint8_t* ptok, const char* match, uint8_t tok ) {
    if ( rb_peek( rbuf, 0 ) == match[0] && rb_peek( rbuf, 1 ) == match[1] ) {
        rbuf->rpos += 2U;
        *ptok = tok;
        return true;
    }
    return false;
}

// -- tokenization ----------------------------------------------------------

bool tokenize_line( const char* text, buf_t* out, struct _linehdr_t* phdr ) {
    rbuf_t src; init_rbuf( &src, text, strlen( text ) );
    size_t start = out->fill;
    char*  item  = 0;
    linehdr_t hdr; clear_linehdr( &hdr );
    // reserve room for the header, it is filled in at the end
    uint8_t zeros[LINEHDR_SIZE];
    memset( zeros, 0, sizeof(zeros) );
    if ( writeto_buffer( out, zeros, sizeof(zeros) ) < 0 ) {
        return false;
    }
    // optional line number
    uint32_t lineno = 0;
    if ( eat_uint32( &src, &lineno ) ) {
        if ( lineno > LINENO_MAX ) {
            goto FAIL;
        }
        hdr.lineno = lineno;
    }
    // main line
    while ( src.rpos < src.size && src.buffer[ src.rpos ] != '\0' ) {
        int base = 0; uint8_t tok;
        if ( eat_dblchrtok( &src, &tok, "<=", TOK_LE ) || eat_dblchrtok( &src, &tok, "<>", TOK_NE ) ||
             eat_dblchrtok( &src, &tok, "><", TOK_NE ) || eat_dblchrtok( &src, &tok, ">=", TOK_GE ) ||
             eat_dblchrtok( &src, &tok, "**", TOK_POW ) || eat_sngchrtok( &src, &tok ) ) {
            if ( !emit_byte( out, tok ) ) {
                goto FAIL;
            }
            continue;
        }
        if ( eat_numlit( &src, &item, &base ) ) {
            if ( base == 10 && item[1] == '\0' ) {
                if ( !emit_byte( out, (uint8_t) item[0] ) ) { // '0'..'9' = TOK_DEC0..9
                    goto FAIL;
                }
                continue;
            }
            static const struct {
                int base;
                bool (*emit_fn)( buf_t*, const char* );
            } numemittbl[] = {
                { 16, emit_hexlit }, { 10, emit_declit }, { 8, emit_octlit },
                {  4, emit_qualit }, {  2, emit_binlit }, { 0, 0 }
            };
            bool found = false;
            for ( int i=0; numemittbl[i].base; ++i ) {
                if ( numemittbl[i].base == base ) {
                    if ( !numemittbl[i].emit_fn( out, item ) ) {
                        goto FAIL;
                    }
                    found = true;
                    break;
                }
            }
            if ( !found ) {
                goto FAIL;
            }
            continue;
        }
        if ( eat_ident( &src, &item ) ) {
            if ( is_keyword( item, &tok ) ) {
                if ( !emit_byte( out, tok ) ) {
                    goto FAIL;
                }
                continue;
            }
            uint8_t idtok = TOK_NUMIDENT;
            if ( rb_peek( &src, 0 ) == '$' ) {
                ++src.rpos; idtok = TOK_STRIDENT;
            } else if ( rb_peek( &src, 0 ) == '%' ) {
                ++src.rpos; idtok = TOK_INTIDENT;
            }
            if ( !emit_ident( out, item, idtok ) ) {
                goto FAIL;
            }
            continue;
        }
        static const struct {
            bool (*eat_fn)( rbuf_t*, char** );
            bool (*emit_fn)( buf_t*, const char* );
        } littbl[] = {
            { eat_strlit, emit_strlit }, { eat_shllit, emit_shllit },
            { eat_quolit, emit_quolit }, { eat_brklit, emit_brklit }, { eat_brclit, emit_brclit },
            { 0, 0 }
        };
        bool found = false;
        for ( int i=0; littbl[i].eat_fn; ++i ) {
            if ( littbl[i].eat_fn( &src, &item ) ) {
                if ( !littbl[i].emit_fn( out, item ) ) {
                    goto FAIL;
                }
                found = true;
                break;
            }
        }
        if ( !found ) {
            char c = rb_peek( &src, 0 );
            if ( c == '&' ) {
                if ( !emit_byte( out, TOK_AMP ) ) {
                    goto FAIL;
                }
                ++src.rpos;
                continue;
            } else if ( c == '?' ) {
                if ( !emit_byte( out, TOK_PRINT ) || !emit_byte( out, TOK_SPACE ) ) {
                    goto FAIL;
                }
                ++src.rpos;
                continue;
            }
            goto FAIL;
        }
    }
    if ( !emit_byte( out, TOK_EOL ) ) {
        goto FAIL;
    }
    size_t total = out->fill - start;
    if ( total > UINT16_MAX ) {
        fprintf( stderr, "? line too long\n" );
        goto FAIL;
    }
    hdr.length = (uint16_t) total;
    uint8_t* hp = (uint8_t*)( &out->buffer[ start ] );
    emit_linehdr_raw( &hp, &hdr );
    *phdr = hdr;
    free( item );
    return true;

FAIL:
    free( item );
    out->fill = start;
    return false;
}

// -- detokenization --------------------------------------------------------

// the highlight class of a keyword token
static hlclass_t keyword_hlclass( uint8_t tok ) {
    switch ( tok ) {
        case TOK_LIST: case TOK_SAVE: case TOK_RUN: case TOK_AUTO: case TOK_RENUM: case TOK_DELETE: case TOK_MERGE:
        case TOK_CHAIN: case TOK_FILES: case TOK_NEW: case TOK_CLEAR: case TOK_EDIT: case TOK_MONITOR: case TOK_LOAD: case TOK_SHOW:
        case TOK_WARRANTY: case TOK_COPYING:
            return HL_COMMAND;
        case TOK_GOTO: case TOK_GOSUB: case TOK_GO: case TOK_TO: case TOK_SUB: case TOK_RETURN: case TOK_IF:
        case TOK_UNLESS: case TOK_THEN: case TOK_ELSE: case TOK_ENDIF: case TOK_ENDUNLESS: case TOK_END: case TOK_FOR:
        case TOK_STEP: case TOK_NEXT: case TOK_REPEAT: case TOK_WHILE: case TOK_UNTIL: case TOK_WEND: case TOK_UEND:
        case TOK_POP: case TOK_ON: case TOK_AGAIN: case TOK_AFTER: case TOK_EVERY:
            return HL_CONTROL;
        case TOK_ASC: case TOK_VAL: case TOK_LEFT: case TOK_MID: case TOK_RIGHT: case TOK_INKEY: case TOK_BIN:
        case TOK_QUA: case TOK_OCT: case TOK_DEC: case TOK_HEX: case TOK_FRE: case TOK_TI: case TOK_LEN: case TOK_CHR:
        case TOK_PLAYING: case TOK_REMAIN: case TOK_EOF: case TOK_LOF: case TOK_LOC: case TOK_ERR: case TOK_ERL: case TOK_MKI: case TOK_MKL: case TOK_MKS: case TOK_MKD:
        case TOK_CVI: case TOK_CVL: case TOK_CVS: case TOK_CVD: case TOK_FN:
            return HL_FUNCTION;
        case TOK_NOT: case TOK_AND: case TOK_OR: case TOK_XOR: case TOK_NAND: case TOK_NOR: case TOK_XNOR:
        case TOK_LSHIFT: case TOK_RSHIFT:
            return HL_OPERATOR;
    }
    return HL_STATEMENT;
}

// the highlight class of a single character token
static hlclass_t sngchr_hlclass( uint8_t tok ) {
    switch ( tok ) {
        case TOK_MULT: case TOK_PLUS: case TOK_MINUS: case TOK_DIV: case TOK_LT: case TOK_EQ: case TOK_GT:
        case TOK_ADDROF: case TOK_BACKSL: case TOK_POW: case TOK_TILDE: case TOK_PLING:
            return HL_OPERATOR;
    }
    if ( tok >= TOK_DEC0 && tok <= TOK_DEC9 ) {
        return HL_NUMBER;
    }
    return HL_PUNCT;
}

// switches to the class "cls" of the next text (spaces: HL_NONE = keep the current one)
static bool hl_switch( buf_t* out, hlstyle_t style, void* usrdata, hlclass_t* pcur, hlclass_t cls ) {
    if ( style == 0 || cls == HL_NONE || cls == *pcur ) {
        return true;
    }
    *pcur = cls;
    const char* text = style( cls, usrdata );
    return text == 0 || writeto_buffer( out, text, strlen( text ) ) >= 0;
}

bool detokenize_line_hl( buf_t* out, const uint8_t* wherefrom, const struct _linehdr_t* phdr,
    hlstyle_t style, void* usrdata ) {
    if ( phdr->length < LINEHDR_SIZE ) {
        return false;
    }
    rbuf_t src; init_rbuf( &src, wherefrom, phdr->length - LINEHDR_SIZE );
    size_t start = out->fill;
    char*  item  = 0;
    bool   old_sigil = ri_sigil;
    hlclass_t cur = HL_NONE;
    if ( phdr->lineno != LINENO_NONE ) {
        if ( !hl_switch( out, style, usrdata, &cur, HL_LINENO ) || !print_uint32( out, phdr->lineno ) ) {
            goto FAIL;
        }
    }
    ri_sigil = true;    // for identifiers, we want the sigil to be returned
    while ( src.rpos < src.size && (uint8_t) src.buffer[ src.rpos ] != TOK_EOL ) {
        uint8_t tok = (uint8_t) src.buffer[ src.rpos];
        static const struct {
            uint8_t tok;
            bool (*read_fn)( rbuf_t*, char** );
            bool (*print_fn)( buf_t*, const char* );
            hlclass_t cls;
        } littbl[] = {
            { TOK_IDENT, read_ident, print_ident, HL_IDENT }, { TOK_NUMIDENT, read_ident, print_ident, HL_IDENT },
            { TOK_STRIDENT, read_ident, print_ident, HL_IDENT }, { TOK_INTIDENT, read_ident, print_ident, HL_IDENT },
            { TOK_STRLIT, read_strlit, print_strlit, HL_STRING },
            { TOK_HEXLIT, read_hexlit, print_hexlit, HL_NUMBER }, { TOK_DECLIT, read_declit, print_declit, HL_NUMBER },
            { TOK_OCTLIT, read_octlit, print_octlit, HL_NUMBER }, { TOK_QUALIT, read_qualit, print_qualit, HL_NUMBER },
            { TOK_BINLIT, read_binlit, print_binlit, HL_NUMBER }, { TOK_SHLLIT, read_shllit, print_shllit, HL_SHELL },
            { TOK_QUOLIT, read_quolit, print_quolit, HL_COMMENT }, { TOK_BRKLIT, read_brklit, print_brklit, HL_STRING },
            { TOK_BRCLIT, read_brclit, print_brclit, HL_STRING },
            { 0, 0, 0, HL_NONE }
        };
        bool found = false;
        for ( int i=0; littbl[i].tok; ++i ) {
            if ( littbl[i].tok == tok ) {
                if ( !littbl[i].read_fn( &src, &item ) ) {
                    goto FAIL;
                }
                if ( !hl_switch( out, style, usrdata, &cur, littbl[i].cls ) ) {
                    goto FAIL;
                }
                if ( !littbl[i].print_fn( out, item ) ) {
                    goto FAIL;
                }
                found = true;
                break;
            }
        }
        if ( !found ) {
            if ( is_sngchrtok( tok ) || ( tok >= TOK_DEC0 && tok <= TOK_DEC9 ) ) {
                if ( !hl_switch( out, style, usrdata, &cur, tok == TOK_SPACE ? HL_NONE : sngchr_hlclass( tok ) ) ||
                     !emit_byte( out, tok ) ) {
                    goto FAIL;
                }
                ++src.rpos;
            } else if ( is_keyword2( tok ) ) {
                if ( !hl_switch( out, style, usrdata, &cur, keyword_hlclass( tok ) ) || !print_keyword( tok, out ) ) {
                    goto FAIL;
                }
                ++src.rpos;
            } else {
                goto FAIL;
            }
        }
    }
    if ( cur != HL_NONE ) {
        const char* text = style( HL_NONE, usrdata );
        if ( text != 0 && writeto_buffer( out, text, strlen( text ) ) < 0 ) {
            goto FAIL;
        }
    }
    // NUL-terminate, but don't count the terminator as content
    if ( anticipate_buffer( out, 1U ) < 0 ) {
        goto FAIL;
    }
    out->buffer[ out->fill ] = '\0';
    ri_sigil = old_sigil;
    free( item );
    return true;

FAIL:
    ri_sigil = old_sigil;
    free( item );
    out->fill = start;
    return false;
}

bool detokenize_line( buf_t* out, const uint8_t* wherefrom, const struct _linehdr_t* phdr ) {
    return detokenize_line_hl( out, wherefrom, phdr, 0, 0 );
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
