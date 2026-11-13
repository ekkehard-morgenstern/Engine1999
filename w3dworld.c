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



#include "stdtypes.h"
#include "gfxops.h"
#include "w3dworld.h"

/*
This is the generator's job of demos/levels/mkw3d.lib (rooms, links, holes, house walls, fence sectors) done by the engine: the document says what
the world is, the loader makes the sectors. Rooms are rectangles in the plan (outlines: not yet). Sector numbers are the rooms' "id" values; the
fence sectors the loader makes itself take the numbers from "environment.fence.first" (default 100) upwards.
*/

#define WW_FIRSTFENCE   100

typedef struct {
    int id, a, b, tex, up, low, fl, k;      // a / k: the room and its side (0 S = y1, 1 E = x2, 2 N = y2, 3 W = x1); b: the room behind (-2: the house)
    double lo, hi, xo, yo;
} wlink_t;

typedef struct {
    int host, b, tex, fl;                   // a hole b in the room host: the ring of walls round it
    double x1, y1, x2, y2;
} wloop_t;

typedef struct {
    bool ex, outdoor, ghost;
    int ft, ct, lt, tg, wt;
    double x1, y1, x2, y2, f, c;
    int nh[4];                              // house spans per side
    double hlo[4][4], hhi[4][4];
} wroom_t;

typedef struct {
    double x1, y1, x2, y2, xo, yo;
    int tex, next, fl, up, low;
} wwall_t;

typedef struct {
    w3d_t* w;
    wroom_t* rooms;                         // W3D_MAXSECTORS
    int* ord;                               // the outdoor rooms in the order they were made
    int nord;
    wlink_t* lk;  size_t nlk, caplk;
    wloop_t* lp;  size_t nlp, caplp;
    int* idx;                               // scratch: link indices of one side
    int nidx;
    wwall_t* ww;  size_t nww, capww;
} ww_t;

static char ww_err[240];
static char ww_ctx[64];                     // what wop() is working on, for its messages

static bool fail( const char* fmt, ... );
static bool fail( const char* fmt, ... ) {
    va_list ap;
    va_start( ap, fmt );
    vsnprintf( ww_err, sizeof(ww_err), fmt, ap );
    va_end( ap );
    return false;
}

// -- JSON access ----------------------------------------------------------------------------------------------------------------------------

static const json_node_t* member( const json_node_t* o, const char* key ) {
    if ( o == 0 || o->type != JSON_OBJ ) {
        return 0;
    }
    for ( size_t i=0; i < o->nkids; ++i ) {
        if ( o->kids[i]->key != 0 && strcmp( o->kids[i]->key, key ) == 0 ) {
            return o->kids[i];
        }
    }
    return 0;
}

// a number member; def when it is missing; false (with a message) when it is not a number
static bool num( const json_node_t* o, const char* key, const char* what, double def, double* v ) {
    const json_node_t* n = member( o, key );
    *v = def;
    if ( n == 0 ) {
        return true;
    }
    if ( n->type != JSON_NUM ) {
        return fail( "%s: \"%s\" is not a number", what, key );
    }
    *v = n->num;
    return true;
}

static bool need( const json_node_t* o, const char* key, const char* what, double* v ) {
    if ( member( o, key ) == 0 ) {
        return fail( "%s: \"%s\" is missing", what, key );
    }
    return num( o, key, what, 0.0, v );
}

static bool flag( const json_node_t* o, const char* key, const char* what, bool def, bool* v ) {
    const json_node_t* n = member( o, key );
    *v = def;
    if ( n == 0 ) {
        return true;
    }
    if ( n->type == JSON_BOOL || n->type == JSON_NUM ) {
        *v = n->num != 0.0;
        return true;
    }
    return fail( "%s: \"%s\" is not true or false", what, key );
}

// n numbers of an array member
static bool numarr( const json_node_t* o, const char* key, const char* what, size_t n, double* v ) {
    const json_node_t* a = member( o, key );
    if ( a == 0 ) {
        return fail( "%s: \"%s\" is missing", what, key );
    }
    if ( a->type != JSON_ARR || a->nkids != n ) {
        return fail( "%s: \"%s\" needs %u numbers", what, key, (unsigned) n );
    }
    for ( size_t i=0; i < n; ++i ) {
        if ( a->kids[i]->type != JSON_NUM ) {
            return fail( "%s: \"%s\" needs %u numbers", what, key, (unsigned) n );
        }
        v[i] = a->kids[i]->num;
    }
    return true;
}

static int flagbit( const char* s ) {
    if ( strcmp( s, "masked" ) == 0 ) { return W3D_WF_MASKED; }
    if ( strcmp( s, "block" ) == 0 ) { return W3D_WF_BLOCK; }
    if ( strcmp( s, "solid" ) == 0 ) { return W3D_WF_SOLID; }
    if ( strcmp( s, "translucent" ) == 0 ) { return W3D_WF_TRANS; }
    if ( strcmp( s, "sky" ) == 0 ) { return W3D_WF_SKY; }
    return -1;
}

// wall flags: a number, a name, or an array of names
static bool wallflags( const json_node_t* o, const char* key, const char* what, int def, int* v ) {
    const json_node_t* n = member( o, key );
    *v = def;
    if ( n == 0 ) {
        return true;
    }
    if ( n->type == JSON_NUM ) {
        *v = (int) n->num;
        return true;
    }
    int bits = 0;
    size_t cnt = n->type == JSON_ARR ? n->nkids : 1;
    for ( size_t i=0; i < cnt; ++i ) {
        const json_node_t* e = n->type == JSON_ARR ? n->kids[i] : n;
        int b;
        if ( e->type == JSON_NUM ) {
            b = (int) e->num;
        } else if ( e->type == JSON_STR && ( b = flagbit( e->str ) ) >= 0 ) {
        } else {
            return fail( "%s: \"%s\": unknown flag", what, key );
        }
        bits |= b;
    }
    *v = bits;
    return true;
}

// a texture: a number; "sky" or "none" = -1
static bool texref( const json_node_t* o, const char* key, const char* what, int def, int* v ) {
    const json_node_t* n = member( o, key );
    *v = def;
    if ( n == 0 ) {
        return true;
    }
    if ( n->type == JSON_NUM ) {
        *v = (int) n->num;
        return true;
    }
    if ( n->type == JSON_STR && ( strcmp( n->str, "sky" ) == 0 || strcmp( n->str, "none" ) == 0 ) ) {
        *v = -1;
        return true;
    }
    return fail( "%s: \"%s\" is not a texture number", what, key );
}

static bool sideof( const json_node_t* o, const char* what, int* k ) {
    const json_node_t* n = member( o, "side" );
    if ( n == 0 || n->type != JSON_STR || n->slen != 1 || strchr( "SENW", n->str[0] ) == 0 ) {
        return fail( "%s: \"side\" must be \"S\", \"E\", \"N\" or \"W\"", what );
    }
    *k = (int) ( strchr( "SENW", n->str[0] ) - "SENW" );
    return true;
}

// -- the world ------------------------------------------------------------------------------------------------------------------------------

static bool addlink( ww_t* c, int a, int k, double lo, double hi, int b, int tex, int up, int low, int fl, double xo, double yo ) {
    if ( c->nlk >= c->caplk ) {
        size_t nc = c->caplk == 0 ? 1024 : c->caplk * 2;
        wlink_t* n = (wlink_t*) realloc( c->lk, sizeof(wlink_t) * nc );
        if ( n == 0 ) {
            return fail( "Out of memory" );
        }
        c->lk = n; c->caplk = nc;
    }
    wlink_t* l = &c->lk[c->nlk++];
    memset( l, 0, sizeof(*l) );
    l->a = a; l->k = k; l->lo = lo; l->hi = hi; l->b = b; l->tex = tex; l->up = up; l->low = low; l->fl = fl; l->xo = xo; l->yo = yo;
    return true;
}

static bool addloop( ww_t* c, int host, const wroom_t* r, int b, int tex, int fl ) {
    if ( c->nlp >= c->caplp ) {
        size_t nc = c->caplp == 0 ? 256 : c->caplp * 2;
        wloop_t* n = (wloop_t*) realloc( c->lp, sizeof(wloop_t) * nc );
        if ( n == 0 ) {
            return fail( "Out of memory" );
        }
        c->lp = n; c->caplp = nc;
    }
    wloop_t* l = &c->lp[c->nlp++];
    l->host = host; l->b = b; l->tex = tex; l->fl = fl; l->x1 = r->x1; l->y1 = r->y1; l->x2 = r->x2; l->y2 = r->y2;
    return true;
}

static bool roomok( const ww_t* c, double id, const char* what, int* out ) {
    if ( id != floor( id ) || id < 0 || id >= W3D_MAXSECTORS || !c->rooms[(int) id].ex ) {
        return fail( "%s: no such room %g", what, id );
    }
    *out = (int) id;
    return true;
}

// the room itself (rooms[] and solids[]): box, floor / ceiling, light, tag, default wall picture
static bool readroom( ww_t* c, const json_node_t* o, const char* what, int* id ) {
    double v, b[4];
    if ( !need( o, "id", what, &v ) ) {
        return false;
    }
    if ( v != floor( v ) || v < 0 || v >= W3D_MAXSECTORS ) {
        return fail( "%s: \"id\" must be a whole number 0 .. %d", what, W3D_MAXSECTORS - 1 );
    }
    *id = (int) v;
    if ( c->rooms[*id].ex ) {
        return fail( "%s: room %d is defined twice", what, *id );
    }
    if ( member( o, "outline" ) != 0 ) {
        return fail( "%s: \"outline\" rooms are not supported yet (use \"box\")", what );
    }
    if ( !numarr( o, "box", what, 4, b ) ) {
        return false;
    }
    if ( !( b[0] < b[2] && b[1] < b[3] ) ) {
        return fail( "%s: \"box\" must be x1, y1, x2, y2 with x1 < x2 and y1 < y2", what );
    }
    wroom_t* r = &c->rooms[*id];
    memset( r, 0, sizeof(*r) );
    r->x1 = b[0]; r->y1 = b[1]; r->x2 = b[2]; r->y2 = b[3];
    const json_node_t* f = member( o, "floor" ), * ce = member( o, "ceiling" );
    if ( f == 0 || ce == 0 ) {
        return fail( "%s: \"floor\" and \"ceiling\" are needed", what );
    }
    double l, t;
    if ( !need( f, "z", what, &r->f ) || !need( ce, "z", what, &r->c ) || !texref( f, "tex", what, 1, &r->ft ) || !texref( ce, "tex", what, -1, &r->ct ) ||
         !num( o, "light", what, 255, &l ) || !num( o, "tag", what, 0, &t ) || !texref( o, "walls", what, 1, &r->wt ) ) {
        return false;
    }
    if ( r->c < r->f ) {
        return fail( "%s: the ceiling is below the floor", what );
    }
    r->lt = (int) l; r->tg = (int) t;
    r->ex = true;
    return flag( o, "ghost", what, false, &r->ghost );
}

static bool readrooms( ww_t* c, const json_node_t* arr ) {
    for ( size_t i=0; i < arr->nkids; ++i ) {
        const json_node_t* o = arr->kids[i];
        char what[48];
        snprintf( what, sizeof(what), "rooms[%u]", (unsigned) i );
        int id = 0;
        if ( o->type != JSON_OBJ || !readroom( c, o, what, &id ) ) {
            return o->type != JSON_OBJ ? fail( "%s is not an object", what ) : false;
        }
        wroom_t* r = &c->rooms[id];
        if ( !flag( o, "outdoor", what, false, &r->outdoor ) ) {
            return false;
        }
        const json_node_t* h = member( o, "house" );
        for ( size_t j=0; h != 0 && j < h->nkids; ++j ) {
            double rg[2];
            int k = 0;
            if ( !sideof( h->kids[j], what, &k ) || !numarr( h->kids[j], "range", what, 2, rg ) ) {
                return false;
            }
            if ( r->nh[k] >= 4 ) {
                return fail( "%s: too many house spans on one side", what );
            }
            int p = r->nh[k]++;
            while ( p > 0 && r->hlo[k][p-1] > rg[0] ) {
                r->hlo[k][p] = r->hlo[k][p-1]; r->hhi[k][p] = r->hhi[k][p-1]; --p;
            }
            r->hlo[k][p] = rg[0]; r->hhi[k][p] = rg[1];
            r->outdoor = true;
        }
        if ( r->outdoor ) {
            c->ord[c->nord++] = id;
        }
    }
    return true;
}

// the geometry check of a two way opening, as the generator made it
static bool touches( const wroom_t* a, const wroom_t* b, int k, double lo, double hi, int ida, int idb ) {
    double c1, c2, r1, r2, r3, r4;
    switch ( k ) {
        case 0: c1 = a->y1; c2 = b->y2; r1 = a->x1; r2 = a->x2; r3 = b->x1; r4 = b->x2; break;
        case 2: c1 = a->y2; c2 = b->y1; r1 = a->x1; r2 = a->x2; r3 = b->x1; r4 = b->x2; break;
        case 3: c1 = a->x1; c2 = b->x2; r1 = a->y1; r2 = a->y2; r3 = b->y1; r4 = b->y2; break;
        default: c1 = a->x2; c2 = b->x1; r1 = a->y1; r2 = a->y2; r3 = b->y1; r4 = b->y2; break;
    }
    if ( c1 != c2 ) {
        return fail( "rooms %d and %d do not touch along that side", ida, idb );
    }
    if ( !( r1 <= lo && lo < hi && hi <= r2 && r3 <= lo && hi <= r4 ) ) {
        return fail( "opening from room %d to room %d: the span %g .. %g is outside the rooms", ida, idb, lo, hi );
    }
    return true;
}

static bool readsolids( ww_t* c, const json_node_t* arr ) {
    for ( size_t i=0; i < arr->nkids; ++i ) {
        const json_node_t* o = arr->kids[i];
        char what[48];
        snprintf( what, sizeof(what), "solids[%u]", (unsigned) i );
        int id = 0, host = 0;
        double hv = 0;
        if ( o->type != JSON_OBJ ) {
            return fail( "%s is not an object", what );
        }
        if ( !readroom( c, o, what, &id ) || !need( o, "in", what, &hv ) || !roomok( c, hv, what, &host ) ) {
            return false;
        }
        // "rim": the picture and the flags of the ring of walls round the hole (and of the portals back into the host)
        const json_node_t* rim = member( o, "rim" );
        int tex = c->rooms[id].wt, fl = 0;
        if ( rim != 0 && ( !texref( rim, "tex", what, tex, &tex ) || !wallflags( rim, "flags", what, 0, &fl ) ) ) {
            return false;
        }
        const wroom_t* r = &c->rooms[id];
        if ( !addloop( c, host, r, id, tex, fl ) ) {
            return false;
        }
        int ltex = fl == 0 ? -1 : tex;
        if ( !addlink( c, id, 0, r->x1, r->x2, host, ltex, -1, -1, fl, 0, 0 ) || !addlink( c, id, 1, r->y1, r->y2, host, ltex, -1, -1, fl, 0, 0 ) ||
             !addlink( c, id, 2, r->x1, r->x2, host, ltex, -1, -1, fl, 0, 0 ) || !addlink( c, id, 3, r->y1, r->y2, host, ltex, -1, -1, fl, 0, 0 ) ) {
            return false;
        }
    }
    return true;
}

static bool readopenings( ww_t* c, const json_node_t* arr ) {
    for ( size_t i=0; i < arr->nkids; ++i ) {
        const json_node_t* o = arr->kids[i];
        char what[48];
        snprintf( what, sizeof(what), "openings[%u]", (unsigned) i );
        double fv = 0, tv = 0, rg[2], xo[2] = { 0, 0 };
        int a = 0, b = 0, k = 0, tex = 0, up = 0, low = 0, fl = 0;
        bool oneway;
        if ( o->type != JSON_OBJ ) {
            return fail( "%s is not an object", what );
        }
        if ( !need( o, "from", what, &fv ) || !need( o, "to", what, &tv ) || !roomok( c, fv, what, &a ) || !roomok( c, tv, what, &b ) ||
             !sideof( o, what, &k ) || !numarr( o, "range", what, 2, rg ) || !texref( o, "tex", what, -1, &tex ) || !texref( o, "upper", what, -1, &up ) ||
             !texref( o, "lower", what, -1, &low ) || !wallflags( o, "flags", what, 0, &fl ) || !flag( o, "oneway", what, false, &oneway ) ) {
            return false;
        }
        if ( member( o, "offset" ) != 0 && !numarr( o, "offset", what, 2, xo ) ) {
            return false;
        }
        if ( !oneway ) {
            if ( !touches( &c->rooms[a], &c->rooms[b], k, rg[0], rg[1], a, b ) ) {
                return false;
            }
            if ( !addlink( c, a, k, rg[0], rg[1], b, tex, up, low, fl, xo[0], xo[1] ) ||
                 !addlink( c, b, ( k + 2 ) % 4, rg[0], rg[1], a, tex, up, low, fl, xo[0], xo[1] ) ) {
                return false;
            }
        } else if ( !addlink( c, a, k, rg[0], rg[1], b, tex, up, low, fl, xo[0], xo[1] ) ) {
            return false;
        }
    }
    return true;
}

// the links of room r on side k, ordered by their start (stable), as indices in c->idx
static bool sortside( ww_t* c, int r, int k ) {
    c->nidx = 0;
    for ( size_t i=0; i < c->nlk; ++i ) {
        if ( c->lk[i].a == r && c->lk[i].k == k ) {
            if ( c->idx == 0 ) {
                c->idx = (int*) malloc( sizeof(int) * 4096 );
                if ( c->idx == 0 ) {
                    return fail( "Out of memory" );
                }
            }
            if ( c->nidx >= 4096 ) {
                return fail( "room %d: too many openings on one side", r );
            }
            int j = c->nidx++;
            while ( j > 0 && c->lk[c->idx[j-1]].lo > c->lk[i].lo ) {
                c->idx[j] = c->idx[j-1]; --j;
            }
            c->idx[j] = (int) i;
        }
    }
    return true;
}

static bool putwall( ww_t* c, double x1, double y1, double x2, double y2, int tex, int next, int fl, int up, int low, double xo, double yo ) {
    if ( c->nww >= c->capww ) {
        size_t nc = c->capww == 0 ? 64 : c->capww * 2;
        wwall_t* n = (wwall_t*) realloc( c->ww, sizeof(wwall_t) * nc );
        if ( n == 0 ) {
            return fail( "Out of memory" );
        }
        c->ww = n; c->capww = nc;
    }
    wwall_t* w = &c->ww[c->nww++];
    w->x1 = x1; w->y1 = y1; w->x2 = x2; w->y2 = y2; w->tex = tex; w->next = next; w->fl = fl; w->up = up; w->low = low; w->xo = xo; w->yo = yo;
    return true;
}

// the walls of room r: along each side, the openings in order with solid walls in the gaps; then the rings round the holes
static bool roomwalls( ww_t* c, int r ) {
    const wroom_t* rm = &c->rooms[r];
    c->nww = 0;
    for ( int k=0; k < 4; ++k ) {
        double fx, a0, a1;
        bool rv, hz;
        switch ( k ) {
            case 0: fx = rm->y1; a0 = rm->x1; a1 = rm->x2; rv = false; hz = true; break;
            case 1: fx = rm->x2; a0 = rm->y1; a1 = rm->y2; rv = false; hz = false; break;
            case 2: fx = rm->y2; a0 = rm->x1; a1 = rm->x2; rv = true; hz = true; break;
            default: fx = rm->x1; a0 = rm->y1; a1 = rm->y2; rv = true; hz = false; break;
        }
        if ( !sortside( c, r, k ) ) {
            return false;
        }
        // the pieces: (lo, hi, link or -1)
        size_t cap = (size_t) c->nidx * 2 + 1;
        double* sl = (double*) malloc( sizeof(double) * cap * 2 );
        int* si = (int*) malloc( sizeof(int) * cap );
        if ( sl == 0 || si == 0 ) {
            free( sl ); free( si );
            return fail( "Out of memory" );
        }
        double* sh = sl + cap;
        size_t n = 0;
        double pos = a0;
        for ( int i=0; i < c->nidx; ++i ) {
            const wlink_t* l = &c->lk[c->idx[i]];
            if ( l->lo < pos ) {
                free( sl ); free( si );
                return fail( "room %d: openings overlap on one side", r );
            }
            if ( l->lo > pos ) {
                sl[n] = pos; sh[n] = l->lo; si[n] = -1; ++n;
            }
            sl[n] = l->lo; sh[n] = l->hi; si[n] = c->idx[i]; ++n;
            pos = l->hi;
        }
        if ( pos < a1 ) {
            sl[n] = pos; sh[n] = a1; si[n] = -1; ++n;
        }
        bool ok = true;
        for ( size_t j=0; ok && j < n; ++j ) {
            size_t i = rv ? n - 1 - j : j;
            double lo = rv ? sh[i] : sl[i], hi = rv ? sl[i] : sh[i];
            double x1 = hz ? lo : fx, y1 = hz ? fx : lo, x2 = hz ? hi : fx, y2 = hz ? fx : hi;
            if ( si[i] < 0 ) {
                ok = putwall( c, x1, y1, x2, y2, rm->wt, -1, 0, -1, -1, 0, 0 );
            } else {
                const wlink_t* l = &c->lk[si[i]];
                int t = l->tex < 0 ? rm->wt : l->tex;
                if ( l->b == -2 ) {
                    ok = putwall( c, x1, y1, x2, y2, t, -1, 0, -1, -1, 0, 0 );
                } else {
                    ok = putwall( c, x1, y1, x2, y2, t, l->b, l->fl, l->up, l->low, l->xo, l->yo );
                }
            }
        }
        free( sl ); free( si );
        if ( !ok ) {
            return false;
        }
    }
    for ( size_t i=0; i < c->nlp; ++i ) {
        const wloop_t* l = &c->lp[i];
        if ( l->host == r ) {
            int t = l->tex < 0 ? rm->wt : l->tex;
            if ( !putwall( c, l->x1, l->y1, l->x1, l->y2, t, l->b, l->fl, -1, -1, 0, 0 ) || !putwall( c, l->x1, l->y2, l->x2, l->y2, t, l->b, l->fl, -1, -1, 0, 0 ) ||
                 !putwall( c, l->x2, l->y2, l->x2, l->y1, t, l->b, l->fl, -1, -1, 0, 0 ) || !putwall( c, l->x2, l->y1, l->x1, l->y1, t, l->b, l->fl, -1, -1, 0, 0 ) ) {
                return false;
            }
        }
    }
    return true;
}

// the outdoor rooms: a solid wall is the house (a tall picture) or a low fence: a thin sector behind it, open to the sky, that you see over and
// can't enter; its far wall and end caps are portals into the "ground" room, so no ray ends in the void
typedef struct { int tex, flags, height, wall, ground, house, first; } wfence_t;

static bool finish( ww_t* c, const wfence_t* fe ) {
    int fid = fe->first;
    int n0 = c->nord;
    for ( int oi=0; oi < n0; ++oi ) {
        int r = c->ord[oi];
        for ( int k=0; k < 4; ++k ) {
            wroom_t* rm = &c->rooms[r];
            double a0 = ( k == 0 || k == 2 ) ? rm->x1 : rm->y1, a1 = ( k == 0 || k == 2 ) ? rm->x2 : rm->y2;
            if ( !sortside( c, r, k ) ) {
                return false;
            }
            double gl[4097], gh[4097];
            int gn = 0;
            double pos = a0;
            for ( int i=0; i < c->nidx; ++i ) {
                const wlink_t* l = &c->lk[c->idx[i]];
                if ( l->lo > pos ) {
                    gl[gn] = pos; gh[gn] = l->lo; ++gn;
                }
                pos = l->hi;
            }
            if ( pos < a1 ) {
                gl[gn] = pos; gh[gn] = a1; ++gn;
            }
            for ( int g=0; g < gn; ++g ) {
                double g0 = gl[g], g1 = gh[g], cur = g0;
                int pk[10], np = 0;
                double pl[10], ph[10];
                for ( int h=0; h < rm->nh[k]; ++h ) {
                    double lo = rm->hlo[k][h], hi = rm->hhi[k][h];
                    if ( g0 > lo ) { lo = g0; }
                    if ( g1 < hi ) { hi = g1; }
                    if ( lo < hi ) {
                        if ( lo > cur ) { pk[np] = 0; pl[np] = cur; ph[np] = lo; ++np; }
                        pk[np] = 1; pl[np] = lo; ph[np] = hi; ++np;
                        cur = hi;
                    }
                }
                if ( cur < g1 ) { pk[np] = 0; pl[np] = cur; ph[np] = g1; ++np; }
                for ( int q=0; q < np; ++q ) {
                    double lo = pl[q], hi = ph[q];
                    if ( pk[q] == 1 ) {
                        if ( !addlink( c, r, k, lo, hi, -2, fe->house, -1, -1, 0, 0, 0 ) ) {
                            return false;
                        }
                        continue;
                    }
                    if ( fid >= W3D_MAXSECTORS || c->rooms[fid].ex ) {
                        return fail( "the fence sectors (from %d) run into room %d or past the last sector", fe->first, fid );
                    }
                    wroom_t* fr = &c->rooms[fid];
                    memset( fr, 0, sizeof(*fr) );
                    fr->ex = true; fr->f = 0; fr->c = fe->height; fr->ft = 10; fr->ct = -1; fr->lt = 255; fr->tg = 0; fr->wt = fe->wall;
                    switch ( k ) {
                        case 0: fr->x1 = lo; fr->y1 = rm->y1 - 20; fr->x2 = hi; fr->y2 = rm->y1; break;
                        case 2: fr->x1 = lo; fr->y1 = rm->y2; fr->x2 = hi; fr->y2 = rm->y2 + 20; break;
                        case 3: fr->x1 = rm->x1 - 20; fr->y1 = lo; fr->x2 = rm->x1; fr->y2 = hi; break;
                        default: fr->x1 = rm->x2; fr->y1 = lo; fr->x2 = rm->x2 + 20; fr->y2 = hi; break;
                    }
                    if ( !addlink( c, r, k, lo, hi, fid, fe->tex, -1, -1, fe->flags, 0, 0 ) || !addlink( c, fid, ( k + 2 ) % 4, lo, hi, r, fe->tex, -1, -1, fe->flags, 0, 0 ) ) {
                        return false;
                    }
                    if ( fe->ground >= 0 ) {
                        double s0 = ( k == 0 ) ? rm->y1 - 20 : ( k == 2 ) ? rm->y2 : ( k == 3 ) ? rm->x1 - 20 : rm->x2;
                        // k 0 / 2: the end caps run along y (sides 1 and 3); k 3 / 1: along x (sides 0 and 2)
                        if ( !addlink( c, fid, k, lo, hi, fe->ground, -1, -1, -1, 0, 0, 0 ) ) {
                            return false;
                        }
                        if ( k == 0 || k == 2 ) {
                            if ( !addlink( c, fid, 1, s0, s0 + 20, fe->ground, -1, -1, -1, 0, 0, 0 ) || !addlink( c, fid, 3, s0, s0 + 20, fe->ground, -1, -1, -1, 0, 0, 0 ) ) {
                                return false;
                            }
                        } else if ( !addlink( c, fid, 0, s0, s0 + 20, fe->ground, -1, -1, -1, 0, 0, 0 ) || !addlink( c, fid, 2, s0, s0 + 20, fe->ground, -1, -1, -1, 0, 0, 0 ) ) {
                            return false;
                        }
                    }
                    ++fid;
                }
            }
        }
    }
    return true;
}

// -- the renderer ---------------------------------------------------------------------------------------------------------------------------

static bool wop( ww_t* c, int op, int na, double a0, double a1, double a2, double a3, double a4, double a5, double a6 ) {
    const double a[7] = { a0, a1, a2, a3, a4, a5, a6 };
    const char* e = 0;
    if ( !w3d_op( c->w, op, a, na, 0, 0, &e ) ) {
        return fail( "%s: the renderer refused (operation %d): %s", ww_ctx, op, e != 0 ? e : "bad argument" );
    }
    return true;
}

static bool emit( ww_t* c ) {
    for ( int r=0; r < W3D_MAXSECTORS; ++r ) {
        const wroom_t* rm = &c->rooms[r];
        if ( !rm->ex ) {
            continue;
        }
        snprintf( ww_ctx, sizeof(ww_ctx), "room %d", r );
        if ( !roomwalls( c, r ) ) {
            return false;
        }
        if ( !wop( c, GFXOP_W3DSECTOR, 6, r, rm->f, rm->c, rm->ft, rm->ct, rm->lt, 0 ) || !wop( c, GFXOP_W3DTAG, 2, r, rm->tg, 0, 0, 0, 0, 0 ) ) {
            return false;
        }
        if ( rm->ghost && !wop( c, GFXOP_W3DGHOST, 2, r, 1, 0, 0, 0, 0, 0 ) ) {
            return false;
        }
        for ( size_t j=0; j < c->nww; ++j ) {
            const wwall_t* w = &c->ww[j];
            bool ok = wop( c, GFXOP_W3DWALL, 7, r, w->x1, w->y1, w->x2, w->y2, w->tex, w->next );
            if ( ok && ( w->up >= 0 || w->low >= 0 ) ) {
                ok = wop( c, GFXOP_W3DWALLTEX, 5, r, (double) j, -1, w->up, w->low, 0, 0 );
            }
            if ( ok && w->fl != 0 ) {
                ok = wop( c, GFXOP_W3DWALLFLAG, 3, r, (double) j, w->fl, 0, 0, 0, 0 );
            }
            if ( ok && ( w->xo != 0 || w->yo != 0 ) ) {
                ok = wop( c, GFXOP_W3DWALLOFF, 4, r, (double) j, w->xo, w->yo, 0, 0, 0 );
            }
            if ( !ok ) {
                return false;
            }
        }
    }
    return true;
}

static bool readthings( ww_t* c, const json_node_t* arr ) {
    for ( size_t i=0; i < arr->nkids; ++i ) {
        const json_node_t* o = arr->kids[i];
        char what[48];
        snprintf( what, sizeof(what), "things[%u]", (unsigned) i );
        double id, x, y, w, h, z, ro, tv;
        int tex;
        if ( o->type != JSON_OBJ ) {
            return fail( "%s is not an object", what );
        }
        if ( !need( o, "id", what, &id ) || !need( o, "x", what, &x ) || !need( o, "y", what, &y ) || !need( o, "w", what, &w ) || !need( o, "h", what, &h ) ||
             !num( o, "z", what, 0, &z ) || !need( o, "tex", what, &tv ) || !num( o, "room", what, GFX_NONE, &ro ) ) {
            return false;
        }
        tex = (int) tv;
        snprintf( ww_ctx, sizeof(ww_ctx), "%s", what );
        double sec = ro;
        if ( ro < -1.0e8 ) {
            const double q[3] = { x, y, 1.0 };      // the room the thing stands in: the one at its feet, as the demos did
            double res = -1;
            if ( !w3d_query( c->w, GFXQ_W3DSECTORZ, q, 3, &res ) ) {
                return fail( "%s: could not find the room", what );
            }
            sec = res;
        }
        if ( !wop( c, GFXOP_W3DTHING, 7, id, x, y, tex, w, h, sec ) || ( z != 0 && !wop( c, GFXOP_W3DTHINGZ, 2, id, z, 0, 0, 0, 0, 0 ) ) ) {
            return false;
        }
    }
    return true;
}

bool w3dworld_load( w3d_t* w, const json_node_t* root, const char** err ) {
    ww_t c;
    memset( &c, 0, sizeof(c) );
    c.w = w;
    *err = ww_err;
    ww_err[0] = '\0'; ww_ctx[0] = '\0';
    if ( root == 0 || root->type != JSON_OBJ ) {
        fail( "there is no world document (JSON LOAD first)" );
        return false;
    }
    const json_node_t* fmt = member( root, "format" );
    if ( fmt == 0 || fmt->type != JSON_STR || strcmp( fmt->str, "e99-world" ) != 0 ) {
        fail( "not a world document: \"format\" must be \"e99-world\"" );
        return false;
    }
    double ver;
    if ( !num( root, "version", "world", 1, &ver ) ) {
        return false;
    }
    if ( ver != 1 ) {
        fail( "unknown world version %g", ver );
        return false;
    }
    c.rooms = (wroom_t*) calloc( W3D_MAXSECTORS, sizeof(wroom_t) );
    c.ord = (int*) calloc( W3D_MAXSECTORS + 8, sizeof(int) );
    if ( c.rooms == 0 || c.ord == 0 ) {
        free( c.rooms ); free( c.ord );
        fail( "Out of memory" );
        return false;
    }
    bool ok = true;
    const json_node_t* rooms = member( root, "rooms" ), * solids = member( root, "solids" ), * ops = member( root, "openings" ), * things = member( root, "things" );
    const json_node_t* env = member( root, "environment" );
    if ( ( rooms != 0 && rooms->type != JSON_ARR ) || ( solids != 0 && solids->type != JSON_ARR ) || ( ops != 0 && ops->type != JSON_ARR ) ||
         ( things != 0 && things->type != JSON_ARR ) ) {
        ok = fail( "\"rooms\", \"solids\", \"openings\" and \"things\" must be arrays" );
    }
    if ( ok && rooms != 0 ) { ok = readrooms( &c, rooms ); }
    if ( ok && solids != 0 ) { ok = readsolids( &c, solids ); }
    if ( ok && ops != 0 ) { ok = readopenings( &c, ops ); }
    // the fence and the house walls of the outdoor rooms
    const json_node_t* fj = member( env, "fence" );
    if ( ok && c.nord > 0 ) {
        wfence_t fe = { -1, W3D_WF_BLOCK | W3D_WF_SOLID, 100, 1, -1, -1, WW_FIRSTFENCE };
        double h = 100, first = WW_FIRSTFENCE, ground = -1;
        if ( fj != 0 ) {
            ok = texref( fj, "tex", "environment.fence", -1, &fe.tex ) && wallflags( fj, "flags", "environment.fence", fe.flags, &fe.flags ) &&
                 num( fj, "height", "environment.fence", 100, &h ) && texref( fj, "wall", "environment.fence", 1, &fe.wall ) &&
                 num( fj, "ground", "environment.fence", -1, &ground ) && texref( fj, "house", "environment.fence", -1, &fe.house ) &&
                 num( fj, "first", "environment.fence", WW_FIRSTFENCE, &first );
            fe.height = (int) h; fe.ground = (int) ground; fe.first = (int) first;
            if ( ok && fe.ground >= 0 ) {
                int g;
                ok = roomok( &c, ground, "environment.fence.ground", &g );
            }
        }
        if ( ok ) { ok = finish( &c, &fe ); }
    }
    if ( ok ) {
        // a world replaces the old one (the pictures and the settings stay)
        ok = wop( &c, GFXOP_W3DCLEAR, 0, 0, 0, 0, 0, 0, 0, 0 );
    }
    if ( ok ) { ok = emit( &c ); }
    if ( ok && things != 0 ) { ok = readthings( &c, things ); }
    if ( ok && env != 0 ) {
        double v;
        const json_node_t* sky = member( env, "sky" );
        ok = num( env, "fog", "environment", GFX_NONE, &v );
        if ( ok && v > -1.0e8 ) { ok = wop( &c, GFXOP_W3DFOG, 1, v, 0, 0, 0, 0, 0, 0 ); }
        if ( ok ) { ok = num( env, "far", "environment", GFX_NONE, &v ); }
        if ( ok && v > -1.0e8 ) { ok = wop( &c, GFXOP_W3DFAR, 1, v, 0, 0, 0, 0, 0, 0 ); }
        if ( ok && sky != 0 ) {
            double rep = GFX_NONE, shift = GFX_NONE;
            int tex = 0;
            ok = sky->type == JSON_NUM ? ( tex = (int) sky->num, rep = shift = GFX_NONE, true ) :
                 ( texref( sky, "tex", "environment.sky", 0, &tex ) && num( sky, "repeat", "environment.sky", GFX_NONE, &rep ) && num( sky, "shift", "environment.sky", GFX_NONE, &shift ) );
            if ( ok ) { ok = wop( &c, GFXOP_W3DSKY, 3, tex, rep, shift, 0, 0, 0, 0 ); }
        }
    }
    const json_node_t* st = member( root, "start" );
    if ( ok && st != 0 ) {
        double x, y, ang;
        ok = need( st, "x", "start", &x ) && need( st, "y", "start", &y ) && num( st, "angle", "start", 0, &ang );
        if ( ok ) { ok = wop( &c, GFXOP_W3DCAMERA, 3, x, y, ang, 0, 0, 0, 0 ); }
    }
    free( c.rooms ); free( c.ord ); free( c.lk ); free( c.lp ); free( c.idx ); free( c.ww );
    return ok;
}
