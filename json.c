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


#include "json.h"

// -- nodes ---------------------------------------------------------------------------------------------------

json_node_t* json_node_new( json_type_t type ) {
    json_node_t* n = (json_node_t*) calloc( 1, sizeof(json_node_t) );
    if ( n != 0 ) {
        n->type = type;
    }
    return n;
}

static void node_clear( json_node_t* n ) {
    for ( size_t i=0; i < n->nkids; ++i ) {
        json_node_free( n->kids[i] );
    }
    free( n->kids );
    free( n->str );
    n->kids = 0; n->str = 0; n->nkids = n->maxkids = 0; n->slen = 0; n->num = 0.0;
}

void json_node_free( json_node_t* n ) {
    if ( n == 0 ) {
        return;
    }
    node_clear( n );
    free( n->key );
    free( n );
}

void json_node_reset( json_node_t* n, json_type_t type ) {
    node_clear( n );
    n->type = type;
}

bool json_node_setstr( json_node_t* n, const char* s, size_t len ) {
    char* copy = (char*) malloc( len + 1U );
    if ( copy == 0 ) {
        return false;
    }
    memcpy( copy, s, len );
    copy[len] = '\0';
    node_clear( n );
    n->type = JSON_STR;
    n->str = copy;
    n->slen = len;
    return true;
}

bool json_node_add( json_node_t* parent, json_node_t* kid ) {
    if ( parent->nkids == parent->maxkids ) {
        size_t nm = parent->maxkids == 0 ? 4U : parent->maxkids * 2U;
        json_node_t** nk = (json_node_t**) realloc( parent->kids, nm * sizeof(json_node_t*) );
        if ( nk == 0 ) {
            return false;
        }
        parent->kids = nk;
        parent->maxkids = nm;
    }
    parent->kids[parent->nkids++] = kid;
    return true;
}

static char* dupn( const char* s, size_t len ) {
    char* d = (char*) malloc( len + 1U );
    if ( d != 0 ) {
        memcpy( d, s, len );
        d[len] = '\0';
    }
    return d;
}

const char* json_typename( const json_node_t* n ) {
    if ( n == 0 ) {
        return "";
    }
    switch ( n->type ) {
        case JSON_NULL: return "null";
        case JSON_BOOL: return "boolean";
        case JSON_NUM:  return "number";
        case JSON_STR:  return "string";
        case JSON_ARR:  return "array";
        default:        return "object";
    }
}

// -- paths ---------------------------------------------------------------------------------------------------

// the next component of the path: skips a dot, treats [n] like .n; false at the end
static bool path_next( const char* path, size_t plen, size_t* pos, const char** comp, size_t* clen, bool* bad ) {
    size_t i = *pos;
    if ( i >= plen ) {
        return false;
    }
    if ( path[i] == '.' ) {
        ++i;
    } else if ( path[i] == '[' ) {
        ++i;
        size_t s = i;
        while ( i < plen && path[i] != ']' ) {
            ++i;
        }
        if ( i >= plen ) {
            *bad = true;
            return false;
        }
        *comp = path + s;
        *clen = i - s;
        *pos = i + 1U;
        return true;
    }
    size_t s = i;
    while ( i < plen && path[i] != '.' && path[i] != '[' ) {
        ++i;
    }
    *comp = path + s;
    *clen = i - s;
    *pos = i;
    return true;
}

static bool comp_index( const char* c, size_t len, size_t* idx ) {
    if ( len == 0 || len > 9U ) {
        return false;
    }
    size_t v = 0;
    for ( size_t i=0; i < len; ++i ) {
        if ( c[i] < '0' || c[i] > '9' ) {
            return false;
        }
        v = v * 10U + (size_t)( c[i] - '0' );
    }
    *idx = v;
    return true;
}

static json_node_t* kid_by_key( const json_node_t* n, const char* c, size_t len ) {
    for ( size_t i=0; i < n->nkids; ++i ) {
        const char* k = n->kids[i]->key;
        if ( k != 0 && strlen( k ) == len && memcmp( k, c, len ) == 0 ) {
            return n->kids[i];
        }
    }
    return 0;
}

json_node_t* json_find( json_node_t* root, const char* path, size_t plen ) {
    json_node_t* n = root;
    size_t pos = 0;
    const char* c; size_t clen; bool bad = false;
    while ( n != 0 && path_next( path, plen, &pos, &c, &clen, &bad ) ) {
        if ( n->type == JSON_OBJ ) {
            n = kid_by_key( n, c, clen );
        } else if ( n->type == JSON_ARR ) {
            size_t idx;
            n = comp_index( c, clen, &idx ) && idx < n->nkids ? n->kids[idx] : 0;
        } else {
            n = 0;
        }
    }
    return bad ? 0 : n;
}

json_node_t* json_make( json_node_t** proot, const char* path, size_t plen, const char** err ) {
    const char* dummy;
    if ( err == 0 ) {
        err = &dummy;
    }
    *err = 0;
    if ( *proot == 0 ) {
        *proot = json_node_new( JSON_NULL );
        if ( *proot == 0 ) {
            *err = "Out of memory";
            return 0;
        }
    }
    json_node_t* n = *proot;
    size_t pos = 0;
    const char* c; size_t clen; bool bad = false;
    while ( path_next( path, plen, &pos, &c, &clen, &bad ) ) {
        if ( clen == 0 ) {
            *err = "Empty name in the path";
            return 0;
        }
        // what the next component (if any) needs
        size_t p2 = pos, idx2;
        const char* c2; size_t l2; bool bad2 = false;
        bool more = path_next( path, plen, &p2, &c2, &l2, &bad2 );
        bool nextnum = more && comp_index( c2, l2, &idx2 );
        if ( n->type == JSON_NULL ) {
            n->type = comp_index( c, clen, &idx2 ) ? JSON_ARR : JSON_OBJ;       // (a new element is made as the kind its first child needs)
        }
        json_node_t* kid = 0;
        if ( n->type == JSON_OBJ ) {
            kid = kid_by_key( n, c, clen );
            if ( kid == 0 ) {
                kid = json_node_new( JSON_NULL );
                if ( kid == 0 || ( kid->key = dupn( c, clen ) ) == 0 || !json_node_add( n, kid ) ) {
                    json_node_free( kid );
                    *err = "Out of memory";
                    return 0;
                }
            }
        } else if ( n->type == JSON_ARR ) {
            size_t idx;
            if ( !comp_index( c, clen, &idx ) ) {
                *err = "An array needs a number in the path";
                return 0;
            }
            if ( idx > n->nkids ) {
                *err = "Array index beyond the end";
                return 0;
            }
            if ( idx == n->nkids ) {
                kid = json_node_new( JSON_NULL );
                if ( kid == 0 || !json_node_add( n, kid ) ) {
                    json_node_free( kid );
                    *err = "Out of memory";
                    return 0;
                }
            } else {
                kid = n->kids[idx];
            }
        } else {
            *err = "The path goes through a value that is not an array or object";
            return 0;
        }
        (void) nextnum;
        n = kid;
    }
    if ( bad ) {
        *err = "Bad path";
        return 0;
    }
    return n;
}

bool json_delete( json_node_t** proot, const char* path, size_t plen ) {
    if ( *proot == 0 ) {
        return false;
    }
    if ( plen == 0 ) {
        json_node_free( *proot );
        *proot = 0;
        return true;
    }
    // the parent: the path without its last component
    size_t pos = 0, lastpos = 0, lastend = 0;
    const char* c; size_t clen; bool bad = false;
    const char* lc = 0; size_t llen = 0;
    while ( true ) {
        size_t before = pos;
        if ( !path_next( path, plen, &pos, &c, &clen, &bad ) ) {
            break;
        }
        lastpos = before; lastend = pos; lc = c; llen = clen;
    }
    (void) lastend;
    if ( lc == 0 || bad ) {
        return false;
    }
    json_node_t* parent = json_find( *proot, path, lastpos );
    if ( parent == 0 ) {
        return false;
    }
    size_t at = (size_t) -1;
    if ( parent->type == JSON_OBJ ) {
        for ( size_t i=0; i < parent->nkids; ++i ) {
            const char* k = parent->kids[i]->key;
            if ( k != 0 && strlen( k ) == llen && memcmp( k, lc, llen ) == 0 ) {
                at = i;
                break;
            }
        }
    } else if ( parent->type == JSON_ARR ) {
        size_t idx;
        if ( comp_index( lc, llen, &idx ) && idx < parent->nkids ) {
            at = idx;
        }
    }
    if ( at == (size_t) -1 ) {
        return false;
    }
    json_node_free( parent->kids[at] );
    memmove( parent->kids + at, parent->kids + at + 1U, ( parent->nkids - at - 1U ) * sizeof(json_node_t*) );
    --parent->nkids;
    return true;
}

// -- parser --------------------------------------------------------------------------------------------------

typedef struct _jparse_t {
    const char* s;
    size_t      len, pos;
    int         line;
    char*       err;
    size_t      errsz;
    bool        failed;
} jparse_t;

static bool jfail( jparse_t* p, const char* msg ) {
    if ( !p->failed && p->err != 0 && p->errsz > 0 ) {
        snprintf( p->err, p->errsz, "%s at line %d", msg, p->line );
    }
    p->failed = true;
    return false;
}

static void skipws( jparse_t* p ) {
    while ( p->pos < p->len ) {
        char c = p->s[p->pos];
        if ( c == '\n' ) {
            ++p->line;
        } else if ( c != ' ' && c != '\t' && c != '\r' ) {
            break;
        }
        ++p->pos;
    }
}

typedef struct _jbuf_t { char* d; size_t n, max; bool oom; } jbuf_t;

static void jb_put( jbuf_t* b, const char* s, size_t len ) {
    if ( b->oom ) {
        return;
    }
    if ( b->n + len + 1U > b->max ) {
        size_t nm = b->max == 0 ? 256U : b->max;
        while ( nm < b->n + len + 1U ) {
            nm *= 2U;
        }
        char* nd = (char*) realloc( b->d, nm );
        if ( nd == 0 ) {
            b->oom = true;
            return;
        }
        b->d = nd;
        b->max = nm;
    }
    memcpy( b->d + b->n, s, len );
    b->n += len;
    b->d[b->n] = '\0';
}

static void jb_putc( jbuf_t* b, char c ) {
    jb_put( b, &c, 1U );
}

static void put_utf8( jbuf_t* b, uint32_t cp ) {
    char u[4];
    if ( cp < 0x80U ) {
        u[0] = (char) cp;
        jb_put( b, u, 1U );
    } else if ( cp < 0x800U ) {
        u[0] = (char)( 0xC0U | ( cp >> 6 ) ); u[1] = (char)( 0x80U | ( cp & 0x3FU ) );
        jb_put( b, u, 2U );
    } else if ( cp < 0x10000U ) {
        u[0] = (char)( 0xE0U | ( cp >> 12 ) ); u[1] = (char)( 0x80U | ( ( cp >> 6 ) & 0x3FU ) ); u[2] = (char)( 0x80U | ( cp & 0x3FU ) );
        jb_put( b, u, 3U );
    } else {
        u[0] = (char)( 0xF0U | ( cp >> 18 ) ); u[1] = (char)( 0x80U | ( ( cp >> 12 ) & 0x3FU ) );
        u[2] = (char)( 0x80U | ( ( cp >> 6 ) & 0x3FU ) ); u[3] = (char)( 0x80U | ( cp & 0x3FU ) );
        jb_put( b, u, 4U );
    }
}

static bool hex4( jparse_t* p, uint32_t* v ) {
    if ( p->pos + 4U > p->len ) {
        return false;
    }
    uint32_t r = 0;
    for ( int i=0; i < 4; ++i ) {
        char c = p->s[p->pos + (size_t) i];
        int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        if ( d < 0 ) {
            return false;
        }
        r = r * 16U + (uint32_t) d;
    }
    p->pos += 4U;
    *v = r;
    return true;
}

// a string (the opening quote is at pos): the bytes go to b
static bool parse_string( jparse_t* p, jbuf_t* b ) {
    ++p->pos;
    while ( true ) {
        if ( p->pos >= p->len ) {
            return jfail( p, "Unterminated string" );
        }
        unsigned char c = (unsigned char) p->s[p->pos++];
        if ( c == '"' ) {
            return true;
        }
        if ( c < 0x20U ) {
            return jfail( p, "Control character in a string" );
        }
        if ( c != '\\' ) {
            jb_putc( b, (char) c );
            continue;
        }
        if ( p->pos >= p->len ) {
            return jfail( p, "Unterminated string" );
        }
        char e = p->s[p->pos++];
        switch ( e ) {
            case '"': jb_putc( b, '"' ); break;
            case '\\': jb_putc( b, '\\' ); break;
            case '/': jb_putc( b, '/' ); break;
            case 'b': jb_putc( b, '\b' ); break;
            case 'f': jb_putc( b, '\f' ); break;
            case 'n': jb_putc( b, '\n' ); break;
            case 'r': jb_putc( b, '\r' ); break;
            case 't': jb_putc( b, '\t' ); break;
            case 'u': {
                uint32_t cp;
                if ( !hex4( p, &cp ) ) {
                    return jfail( p, "Bad \\u escape" );
                }
                if ( cp >= 0xD800U && cp <= 0xDBFFU ) {
                    uint32_t lo;
                    if ( p->pos + 1U < p->len && p->s[p->pos] == '\\' && p->s[p->pos + 1U] == 'u' ) {
                        p->pos += 2U;
                        if ( !hex4( p, &lo ) || lo < 0xDC00U || lo > 0xDFFFU ) {
                            return jfail( p, "Bad surrogate pair" );
                        }
                        cp = 0x10000U + ( ( cp - 0xD800U ) << 10 ) + ( lo - 0xDC00U );
                    } else {
                        return jfail( p, "Lone surrogate" );
                    }
                } else if ( cp >= 0xDC00U && cp <= 0xDFFFU ) {
                    return jfail( p, "Lone surrogate" );
                }
                put_utf8( b, cp );
                break;
            }
            default:
                return jfail( p, "Bad escape" );
        }
    }
}

static json_node_t* parse_value( jparse_t* p, int depth );

static bool starts( jparse_t* p, const char* w ) {
    size_t l = strlen( w );
    if ( p->pos + l <= p->len && memcmp( p->s + p->pos, w, l ) == 0 ) {
        p->pos += l;
        return true;
    }
    return false;
}

static json_node_t* parse_value( jparse_t* p, int depth ) {
    skipws( p );
    if ( p->pos >= p->len ) {
        jfail( p, "Value expected" );
        return 0;
    }
    if ( depth > JSON_MAXDEPTH ) {
        jfail( p, "Nested too deeply" );
        return 0;
    }
    char c = p->s[p->pos];
    json_node_t* n = 0;
    if ( c == '{' || c == '[' ) {
        bool obj = c == '{';
        char close = obj ? '}' : ']';
        ++p->pos;
        n = json_node_new( obj ? JSON_OBJ : JSON_ARR );
        if ( n == 0 ) {
            jfail( p, "Out of memory" );
            return 0;
        }
        skipws( p );
        if ( p->pos < p->len && p->s[p->pos] == close ) {
            ++p->pos;
            return n;
        }
        while ( true ) {
            char* key = 0;
            if ( obj ) {
                skipws( p );
                if ( p->pos >= p->len || p->s[p->pos] != '"' ) {
                    jfail( p, "Key expected" );
                    goto bad;
                }
                jbuf_t kb = { 0, 0, 0, false };
                if ( !parse_string( p, &kb ) || kb.oom ) {
                    free( kb.d );
                    jfail( p, "Out of memory" );
                    goto bad;
                }
                key = kb.d != 0 ? kb.d : dupn( "", 0 );
                skipws( p );
                if ( p->pos >= p->len || p->s[p->pos] != ':' ) {
                    free( key );
                    jfail( p, "':' expected" );
                    goto bad;
                }
                ++p->pos;
            }
            json_node_t* v = parse_value( p, depth + 1 );
            if ( v == 0 ) {
                free( key );
                goto bad;
            }
            if ( obj ) {
                json_node_t* dup = kid_by_key( n, key, strlen( key ) );
                if ( dup != 0 ) {                           // a repeated key: the last value wins, at the place of the first
                    free( key );
                    free( v->key );
                    v->key = dup->key;
                    dup->key = 0;
                    for ( size_t i=0; i < n->nkids; ++i ) {
                        if ( n->kids[i] == dup ) {
                            n->kids[i] = v;
                        }
                    }
                    json_node_free( dup );
                } else {
                    v->key = key;
                    if ( !json_node_add( n, v ) ) {
                        json_node_free( v );
                        jfail( p, "Out of memory" );
                        goto bad;
                    }
                }
            } else if ( !json_node_add( n, v ) ) {
                json_node_free( v );
                jfail( p, "Out of memory" );
                goto bad;
            }
            skipws( p );
            if ( p->pos >= p->len ) {
                jfail( p, obj ? "'}' expected" : "']' expected" );
                goto bad;
            }
            if ( p->s[p->pos] == ',' ) {
                ++p->pos;
                continue;
            }
            if ( p->s[p->pos] == close ) {
                ++p->pos;
                return n;
            }
            jfail( p, obj ? "',' or '}' expected" : "',' or ']' expected" );
            goto bad;
        }
bad:
        json_node_free( n );
        return 0;
    }
    if ( c == '"' ) {
        jbuf_t b = { 0, 0, 0, false };
        if ( !parse_string( p, &b ) || b.oom ) {
            free( b.d );
            jfail( p, "Out of memory" );
            return 0;
        }
        n = json_node_new( JSON_STR );
        if ( n == 0 ) {
            free( b.d );
            jfail( p, "Out of memory" );
            return 0;
        }
        if ( b.d == 0 ) {
            b.d = dupn( "", 0 );
        }
        n->str = b.d;
        n->slen = b.n;
        return n;
    }
    if ( c == 't' || c == 'f' || c == 'n' ) {
        json_type_t t = c == 'n' ? JSON_NULL : JSON_BOOL;
        double v = c == 't' ? 1.0 : 0.0;
        if ( !starts( p, c == 't' ? "true" : c == 'f' ? "false" : "null" ) ) {
            jfail( p, "Unknown word" );
            return 0;
        }
        n = json_node_new( t );
        if ( n == 0 ) {
            jfail( p, "Out of memory" );
            return 0;
        }
        n->num = v;
        return n;
    }
    // a number: -? int frac? exp?
    size_t s = p->pos, i = s;
    if ( i < p->len && p->s[i] == '-' ) {
        ++i;
    }
    if ( i >= p->len || p->s[i] < '0' || p->s[i] > '9' ) {
        jfail( p, "Value expected" );
        return 0;
    }
    if ( p->s[i] == '0' ) {
        ++i;
    } else {
        while ( i < p->len && p->s[i] >= '0' && p->s[i] <= '9' ) {
            ++i;
        }
    }
    if ( i < p->len && p->s[i] == '.' ) {
        ++i;
        size_t f = i;
        while ( i < p->len && p->s[i] >= '0' && p->s[i] <= '9' ) {
            ++i;
        }
        if ( i == f ) {
            jfail( p, "Bad number" );
            return 0;
        }
    }
    if ( i < p->len && ( p->s[i] == 'e' || p->s[i] == 'E' ) ) {
        ++i;
        if ( i < p->len && ( p->s[i] == '+' || p->s[i] == '-' ) ) {
            ++i;
        }
        size_t f = i;
        while ( i < p->len && p->s[i] >= '0' && p->s[i] <= '9' ) {
            ++i;
        }
        if ( i == f ) {
            jfail( p, "Bad number" );
            return 0;
        }
    }
    char tmp[64];
    size_t nl = i - s;
    if ( nl >= sizeof(tmp) ) {
        jfail( p, "Number too long" );
        return 0;
    }
    memcpy( tmp, p->s + s, nl );
    tmp[nl] = '\0';
    n = json_node_new( JSON_NUM );
    if ( n == 0 ) {
        jfail( p, "Out of memory" );
        return 0;
    }
    n->num = strtod( tmp, 0 );
    p->pos = i;
    return n;
}

bool json_parse( const char* text, size_t len, json_node_t** out, char* err, size_t errsz ) {
    jparse_t p;
    memset( &p, 0, sizeof(p) );
    p.s = text; p.len = len; p.line = 1; p.err = err; p.errsz = errsz;
    if ( len >= 3U && (unsigned char) text[0] == 0xEFU && (unsigned char) text[1] == 0xBBU && (unsigned char) text[2] == 0xBFU ) {
        p.pos = 3U;                                         // a byte order mark
    }
    json_node_t* n = parse_value( &p, 0 );
    if ( n != 0 ) {
        skipws( &p );
        if ( p.pos < p.len ) {
            jfail( &p, "Text after the value" );
            json_node_free( n );
            n = 0;
        }
    }
    if ( n == 0 ) {
        return false;
    }
    *out = n;
    return true;
}

// -- writer --------------------------------------------------------------------------------------------------

static void put_number( jbuf_t* b, double v ) {
    char t[40];
    if ( !( v == v ) || v > 1.7e308 || v < -1.7e308 ) {
        jb_put( b, "null", 4U );
        return;
    }
    if ( v == floor( v ) && fabs( v ) < 1.0e15 ) {
        snprintf( t, sizeof(t), "%.0f", v );
        if ( strcmp( t, "-0" ) == 0 ) {
            strcpy( t, "0" );
        }
    } else {
        for ( int prec = 15; prec <= 17; ++prec ) {
            snprintf( t, sizeof(t), "%.*g", prec, v );
            if ( strtod( t, 0 ) == v ) {
                break;
            }
        }
    }
    jb_put( b, t, strlen( t ) );
}

static void put_string( jbuf_t* b, const char* s, size_t len ) {
    jb_putc( b, '"' );
    for ( size_t i=0; i < len; ++i ) {
        unsigned char c = (unsigned char) s[i];
        switch ( c ) {
            case '"': jb_put( b, "\\\"", 2U ); break;
            case '\\': jb_put( b, "\\\\", 2U ); break;
            case '\n': jb_put( b, "\\n", 2U ); break;
            case '\r': jb_put( b, "\\r", 2U ); break;
            case '\t': jb_put( b, "\\t", 2U ); break;
            case '\b': jb_put( b, "\\b", 2U ); break;
            case '\f': jb_put( b, "\\f", 2U ); break;
            default:
                if ( c < 0x20U ) {
                    char u[8];
                    snprintf( u, sizeof(u), "\\u%04x", (unsigned) c );
                    jb_put( b, u, 6U );
                } else {
                    jb_putc( b, (char) c );
                }
        }
    }
    jb_putc( b, '"' );
}

static void put_indent( jbuf_t* b, int level ) {
    jb_putc( b, '\n' );
    for ( int i=0; i < level; ++i ) {
        jb_put( b, "  ", 2U );
    }
}

// pretty printing keeps arrays of plain values (numbers, strings, ...) on one line: a map file stays readable
static bool all_plain( const json_node_t* n ) {
    for ( size_t i=0; i < n->nkids; ++i ) {
        if ( n->kids[i]->type == JSON_ARR || n->kids[i]->type == JSON_OBJ ) {
            return false;
        }
    }
    return true;
}

static void put_node( jbuf_t* b, const json_node_t* n, bool pretty, int level ) {
    switch ( n->type ) {
        case JSON_NULL: jb_put( b, "null", 4U ); break;
        case JSON_BOOL: if ( n->num != 0.0 ) jb_put( b, "true", 4U ); else jb_put( b, "false", 5U ); break;
        case JSON_NUM:  put_number( b, n->num ); break;
        case JSON_STR:  put_string( b, n->str != 0 ? n->str : "", n->slen ); break;
        default: {
            bool obj = n->type == JSON_OBJ;
            if ( n->nkids == 0 ) {
                jb_put( b, obj ? "{}" : "[]", 2U );
                break;
            }
            bool line = pretty && !obj && all_plain( n );
            jb_putc( b, obj ? '{' : '[' );
            for ( size_t i=0; i < n->nkids; ++i ) {
                if ( i > 0 ) {
                    jb_putc( b, ',' );
                    if ( line ) {
                        jb_putc( b, ' ' );
                    }
                }
                if ( pretty && !line ) {
                    put_indent( b, level + 1 );
                }
                if ( obj ) {
                    put_string( b, n->kids[i]->key != 0 ? n->kids[i]->key : "", n->kids[i]->key != 0 ? strlen( n->kids[i]->key ) : 0 );
                    jb_putc( b, ':' );
                    if ( pretty ) {
                        jb_putc( b, ' ' );
                    }
                }
                put_node( b, n->kids[i], pretty, level + 1 );
            }
            if ( pretty && !line ) {
                put_indent( b, level );
            }
            jb_putc( b, obj ? '}' : ']' );
        }
    }
}

char* json_print( const json_node_t* n, bool pretty, size_t* plen ) {
    jbuf_t b = { 0, 0, 0, false };
    jb_put( &b, "", 0 );
    put_node( &b, n, pretty, 0 );
    if ( pretty ) {
        jb_putc( &b, '\n' );
    }
    if ( b.oom ) {
        free( b.d );
        return 0;
    }
    if ( plen != 0 ) {
        *plen = b.n;
    }
    return b.d;
}

char* json_valuetext( const json_node_t* n, size_t* plen ) {
    if ( n == 0 || n->type == JSON_NULL ) {
        char* e = dupn( "", 0 );
        if ( plen != 0 ) {
            *plen = 0;
        }
        return e;
    }
    if ( n->type == JSON_STR ) {
        if ( plen != 0 ) {
            *plen = n->slen;
        }
        return dupn( n->str, n->slen );
    }
    return json_print( n, false, plen );
}
