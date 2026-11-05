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

#include "sprpath.h"

typedef struct _parser_t {
    const char* s;
    size_t      len, pos;
    char*       err;
    size_t      errsize;
    size_t*     errpos;
    sprpath_cmd_t* cmd;
    uint32_t    n, cap;
    int         tdef;                   // T: frames of a move without *k
    uint32_t    open[SPRPATH_NEST];     // the commands of the open brackets
    int         depth;
} parser_t;

static bool fail( parser_t* p, const char* msg ) {
    snprintf( p->err, p->errsize, "%s", msg );
    *p->errpos = p->pos;
    return false;
}

static void skipblanks( parser_t* p ) {
    while ( p->pos < p->len && ( p->s[p->pos] == ' ' || p->s[p->pos] == '\t' ) ) {
        ++p->pos;
    }
}

// a decimal number with an optional sign (sign: 0 = none allowed, 1 = optional, 2 = required)
static bool number( parser_t* p, int sign, int32_t* out, bool* signed_ ) {
    skipblanks( p );
    bool neg = false, had = false;
    if ( p->pos < p->len && ( p->s[p->pos] == '+' || p->s[p->pos] == '-' ) ) {
        if ( sign == 0 ) {
            return fail( p, "Number without sign expected" );
        }
        neg = p->s[p->pos] == '-';
        had = true;
        ++p->pos;
        skipblanks( p );
    }
    if ( sign == 2 && !had ) {
        return fail( p, "+ or - expected" );
    }
    if ( p->pos >= p->len || p->s[p->pos] < '0' || p->s[p->pos] > '9' ) {
        return fail( p, "Number expected" );
    }
    int64_t v = 0;
    while ( p->pos < p->len && p->s[p->pos] >= '0' && p->s[p->pos] <= '9' ) {
        v = v * 10 + ( p->s[p->pos] - '0' );
        if ( v > 1000000 ) {
            return fail( p, "Number too big" );
        }
        ++p->pos;
    }
    *out = (int32_t)( neg ? -v : v );
    if ( signed_ ) {
        *signed_ = had;
    }
    return true;
}

static bool comma( parser_t* p ) {
    skipblanks( p );
    if ( p->pos >= p->len || p->s[p->pos] != ',' ) {
        return fail( p, "Comma expected" );
    }
    ++p->pos;
    return true;
}

// an optional "*k"
static bool times( parser_t* p, int32_t* k ) {
    skipblanks( p );
    *k = 0;
    if ( p->pos < p->len && p->s[p->pos] == '*' ) {
        ++p->pos;
        if ( !number( p, 0, k, 0 ) ) {
            return false;
        }
        if ( *k < 1 || *k > 65535 ) {
            return fail( p, "Count out of range" );
        }
    }
    return true;
}

static bool add( parser_t* p, uint8_t op, int32_t a, int32_t b, int32_t c ) {
    if ( p->n >= SPRPATH_MAX ) {
        return fail( p, "Path too long" );
    }
    if ( p->n >= p->cap ) {
        uint32_t nc = p->cap ? p->cap * 2 : 32;
        sprpath_cmd_t* nm = (sprpath_cmd_t*) realloc( p->cmd, nc * sizeof(sprpath_cmd_t) );
        if ( nm == 0 ) {
            return fail( p, "Out of memory" );
        }
        p->cmd = nm;
        p->cap = nc;
    }
    sprpath_cmd_t* m = &p->cmd[p->n++];
    m->op = op; m->a = a; m->b = b; m->c = c;
    return true;
}

static bool move( parser_t* p, int32_t dx, int32_t dy, int32_t k ) {
    if ( k > 0 ) {
        if ( (int64_t) dx * k > 1000000 || (int64_t) dx * k < -1000000 || (int64_t) dy * k > 1000000 || (int64_t) dy * k < -1000000 ) {
            return fail( p, "Distance too big" );
        }
        return add( p, SPO_MOVE, dx * k, dy * k, k );
    }
    return add( p, SPO_MOVE, dx, dy, p->tdef );
}

static bool one( parser_t* p ) {
    skipblanks( p );
    char ch = p->s[p->pos];
    if ( ch >= 'a' && ch <= 'z' ) {
        ch = (char)( ch - 'a' + 'A' );
    }
    size_t at = p->pos;
    ++p->pos;
    int32_t v = 0, w = 0, u = 0, k = 0;
    switch ( ch ) {
        case 'X': case 'Y': {
            skipblanks( p );
            if ( p->pos < p->len && p->s[p->pos] == '=' ) {
                ++p->pos;
                if ( !number( p, 1, &v, 0 ) ) return false;
                return add( p, ch == 'X' ? SPO_SETX : SPO_SETY, v, 0, 0 );
            }
            if ( !number( p, 2, &v, 0 ) || !times( p, &k ) ) return false;
            return ch == 'X' ? move( p, v, 0, k ) : move( p, 0, v, k );
        }
        case 'M':
            if ( !number( p, 1, &v, 0 ) || !comma( p ) || !number( p, 1, &w, 0 ) || !times( p, &k ) ) return false;
            return move( p, v, w, k );
        case 'W':
            if ( !number( p, 0, &v, 0 ) ) return false;
            if ( v < 1 || v > 65535 ) { p->pos = at; return fail( p, "Frames out of range" ); }
            return add( p, SPO_WAIT, 0, 0, v );
        case 'T':
            if ( !number( p, 0, &v, 0 ) ) return false;
            if ( v < 1 || v > 65535 ) { p->pos = at; return fail( p, "Frames out of range" ); }
            p->tdef = v;
            return true;
        case 'A':
            if ( !number( p, 0, &v, 0 ) || !comma( p ) || !number( p, 0, &w, 0 ) || !comma( p ) || !number( p, 0, &u, 0 ) ) return false;
            if ( v > 255 || w > 15 || u > 31 ) { p->pos = at; return fail( p, "Animation out of range" ); }
            return add( p, SPO_ANIM, v, w, u );
        case 'D':
            if ( !number( p, 0, &v, 0 ) ) return false;
            if ( v > 255 ) { p->pos = at; return fail( p, "Image out of range" ); }
            return add( p, SPO_IMAGE, v, 0, 0 );
        case 'E':
            if ( !number( p, 0, &v, 0 ) ) return false;
            if ( v > 255 ) { p->pos = at; return fail( p, "Event out of range" ); }
            return add( p, SPO_EVENT, v, 0, 0 );
        case 'P':
            if ( !number( p, 0, &v, 0 ) ) return false;
            if ( v > 7 ) { p->pos = at; return fail( p, "Priority out of range" ); }
            return add( p, SPO_PRIO, v, 0, 0 );
        case 'H': return add( p, SPO_HIDE, 0, 0, 0 );
        case 'S': return add( p, SPO_SHOW, 0, 0, 0 );
        case 'R': return add( p, SPO_REPEAT, 0, 0, 0 );
        case '[':
            if ( p->depth >= SPRPATH_NEST ) { p->pos = at; return fail( p, "Brackets nested too deep" ); }
            p->open[p->depth] = p->n;
            if ( !add( p, SPO_LOOP, 0, p->depth, 0 ) ) return false;
            ++p->depth;
            return true;
        case ']':
            if ( p->depth <= 0 ) { p->pos = at; return fail( p, "] without [" ); }
            if ( !number( p, 0, &v, 0 ) ) return false;
            if ( v < 1 || v > 65535 ) { p->pos = at; return fail( p, "Count out of range" ); }
            --p->depth;
            return add( p, SPO_ENDLOOP, (int32_t) p->open[p->depth] + 1, p->depth, v );
        default:
            p->pos = at;
            return fail( p, "Unknown path command" );
    }
}

sprpath_t* sprpath_parse( const char* s, size_t len, char* err, size_t errsize, size_t* errpos ) {
    size_t dummy = 0;
    char dummyerr[2];
    parser_t p;
    memset( &p, 0, sizeof(p) );
    p.s = s; p.len = len;
    p.err = err ? err : dummyerr;
    p.errsize = err ? errsize : sizeof(dummyerr);
    p.errpos = errpos ? errpos : &dummy;
    p.tdef = 1;
    bool ok = true;
    for (;;) {
        skipblanks( &p );
        if ( p.pos >= p.len ) {
            break;
        }
        if ( !one( &p ) ) {
            ok = false;
            break;
        }
    }
    if ( ok && p.depth > 0 ) {
        ok = fail( &p, "[ without ]" );
    }
    if ( !ok ) {
        free( p.cmd );
        return 0;
    }
    sprpath_t* r = (sprpath_t*) calloc( 1, sizeof(sprpath_t) + ( p.n ? p.n - 1 : 0 ) * sizeof(sprpath_cmd_t) );
    if ( r == 0 ) {
        free( p.cmd );
        fail( &p, "Out of memory" );
        return 0;
    }
    r->n = p.n;
    if ( p.n ) {
        memcpy( r->cmd, p.cmd, p.n * sizeof(sprpath_cmd_t) );
    }
    free( p.cmd );
    return r;
}

void sprpath_free( sprpath_t* p ) {
    free( p );
}
