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

#include "roadnet.h"
#include "noise.h"
#include "vecext.h"
#include <string.h>
#include <stdlib.h>

E99_NO_CONTRACT                                      // (the vector loop of rnet_near() and its scalar twin must give the same bits)
E99_NO_PSABI

#if E99_VEC
typedef double v4d __attribute__((vector_size(32)));
typedef long long v4l __attribute__((vector_size(32)));
#endif

static int rn_simd = -1;

void rnet_set_simd( int on ) {                             // 1 = vector loop, 0 = scalar twin, -1 = by the environment (ENGINE1999_NOSIMD) again (tests)
    rn_simd = on;
}

#define RNET_MAXPTS 700         // a road of more steps than this is not cached (computed again at every call)
#define RNET_CACHE  512

// the point of region (rx, ry) of a network (0 dirt: salts 1 / 2, 1 tarred: salts 11 / 12 and its own spacing)
static void net_point( const rnet_t* n, int net, int64_t rx, int64_t ry, double* x, double* y ) {
    double sp = net ? n->tspacing : n->spacing;
    *x = (double) rx * sp + nz_hash( rx, ry, net ? 11 : 1, 0 ) * sp;
    *y = (double) ry * sp + nz_hash( rx, ry, net ? 12 : 2, 0 ) * sp;
}

static double smooth01( double t ) {
    return t <= 0.0 ? 0.0 : t >= 1.0 ? 1.0 : t * t * ( 3.0 - 2.0 * t );
}

// the t of ridged_layer() in m3d.c (without the quiet box): the same numbers, so a road keeps out of exactly the places where mountains rise
double rnet_mountain( const rmask_t* m, double x, double y ) {
    if ( !m->on ) {
        return 0.0;
    }
    double t = smooth01( ( nz_fbm( m->seed + 1, x / ( m->scale * 4.0 ), y / ( m->scale * 4.0 ), 2, 0.5, 2.0 ) - m->lo ) / ( m->hi - m->lo ) );
    if ( m->r1 > 0.0 && t > 0.0 ) {
        t *= smooth01( ( sqrt( x * x + y * y ) - m->r0 ) / ( m->r1 - m->r0 ) );
    }
    return t;
}

// does the way (the chord between two nodes, widened by 'side' on both sides) touch the mountains? sampled every 500 units, on 5 lines
static bool strip_in_mountains( const rnet_t* n, double x1, double y1, double x2, double y2, double len, double side ) {
    const rmask_t* m = &n->mask;
    double ux = ( x2 - x1 ) / len, uy = ( y2 - y1 ) / len;
    int ns = (int)( len / 500.0 ) + 1;
    for ( int k = 0; k <= ns; ++k ) {
        double t = (double) k / (double) ns, x = x1 + ( x2 - x1 ) * t, y = y1 + ( y2 - y1 ) * t;
        for ( int s = -2; s <= 2; ++s ) {
            if ( rnet_mountain( m, x - uy * side * 0.5 * (double) s, y + ux * side * 0.5 * (double) s ) > m->limit ) {
                return true;
            }
        }
    }
    return false;
}

// does the road of network net from region (rx, ry) to its west (e = 0) / south (e = 1) neighbour exist? (the hash, a length, and no mountains: the way may bulge by a tenth of its length)
static bool link_exists_calc( const rnet_t* n, int net, int64_t rx, int64_t ry, int e ) {
    if ( !n->on || ( net ? n->tspacing <= 0.0 : n->spacing <= 0.0 ) || nz_hash( rx, ry, ( net ? 160 : 60 ) + e, 0 ) >= ( net ? n->tchance : n->chance ) ) {
        return false;
    }
    double x1, y1, x2, y2;
    net_point( n, net, rx, ry, &x1, &y1 );
    net_point( n, net, rx - ( e == 0 ), ry - ( e == 1 ), &x2, &y2 );
    double len = sqrt( ( x2 - x1 ) * ( x2 - x1 ) + ( y2 - y1 ) * ( y2 - y1 ) );
    if ( len <= 1.0e-9 ) {
        return false;
    }
    return !( n->mask.on && strip_in_mountains( n, x1, y1, x2, y2, len, ( net ? n->wtar : n->wdirt ) + n->mask.margin + 0.1 * len ) );
}

static bool same_cfg( const rnet_t* a, const rnet_t* b );

// link_exists() is asked many times for the same link (the nodes at both ends ask for all their links), and each answer samples the mountains along a long strip: a small
// direct mapped memo per thread keeps the answers (the world creation asks for a million chunks)
typedef struct { bool valid, res; int net, e; int64_t rx, ry; rnet_t cfg; } lemo_t;
#define LEMO_SIZE 1024
static _Thread_local lemo_t lemo[LEMO_SIZE];

static bool link_exists( const rnet_t* n, int net, int64_t rx, int64_t ry, int e ) {
    unsigned i = (unsigned)( nz_mix( (uint64_t) rx * 0x9E3779B97F4A7C15ULL + (uint64_t) ry * 0xC2B2AE3D27D4EB4FULL + (uint64_t)( net * 2 + e ) ) % LEMO_SIZE );
    lemo_t* m = &lemo[i];
    if ( m->valid && m->net == net && m->e == e && m->rx == rx && m->ry == ry && same_cfg( &m->cfg, n ) ) {
        return m->res;
    }
    bool r = link_exists_calc( n, net, rx, ry, e );
    memset( m, 0, sizeof(*m) );
    m->valid = true; m->res = r; m->net = net; m->e = e; m->rx = rx; m->ry = ry; m->cfg = *n;
    return r;
}

enum { SIDE_W = 0, SIDE_S = 1, SIDE_E = 2, SIDE_N = 3 };

// what a node (the point of region (rx, ry)) is like as the end of the road that leaves it by 'side': the unit vector of the road's direction there and whether the node is an
// intersection. An intersection (3 or 4 tarred roads) has its arms square to its sides (axis aligned); a node of 2 roads has one tangent for both (Catmull-Rom: from the one
// partner to the other: the road goes on smoothly, a turn becomes a curve); a node of 1 road, and a node of a dirt track with 3 or 4 roads, points the road at its partner.
static void node_dir( const rnet_t* n, int net, int64_t rx, int64_t ry, int side, double* dx, double* dy, bool* hub ) {
    static const int sx[4] = { -1, 0, 1, 0 }, sy[4] = { 0, -1, 0, 1 };
    bool has[4];
    has[SIDE_W] = link_exists( n, net, rx, ry, 0 );
    has[SIDE_S] = link_exists( n, net, rx, ry, 1 );
    has[SIDE_E] = link_exists( n, net, rx + 1, ry, 0 );
    has[SIDE_N] = link_exists( n, net, rx, ry + 1, 1 );
    int deg = has[0] + has[1] + has[2] + has[3];
    double px, py, qx, qy;
    net_point( n, net, rx, ry, &px, &py );
    net_point( n, net, rx + sx[side], ry + sy[side], &qx, &qy );
    *hub = false;
    if ( net && deg >= 3 ) {
        *hub = true;
        *dx = sx[side]; *dy = sy[side];
        return;
    }
    double vx = qx - px, vy = qy - py;                      // (the chord to the partner)
    if ( deg == 2 ) {
        for ( int o = 0; o < 4; ++o ) {
            if ( o != side && has[o] ) {
                double ox, oy;
                net_point( n, net, rx + sx[o], ry + sy[o], &ox, &oy );
                if ( ( qx - ox ) * ( qx - ox ) + ( qy - oy ) * ( qy - oy ) > 1.0 ) { vx = qx - ox; vy = qy - oy; }
                break;
            }
        }
    }
    double l = sqrt( vx * vx + vy * vy );
    *dx = l > 1.0e-9 ? vx / l : 1.0; *dy = l > 1.0e-9 ? vy / l : 0.0;
}

// the curve without the wander, at u (0 .. 1) between the ends of the stubs
static void hermite( const rlink_t* l, double u, double* x, double* y ) {
    double u2 = u * u, u3 = u2 * u, h00 = 2.0 * u3 - 3.0 * u2 + 1.0, h10 = u3 - 2.0 * u2 + u, h01 = -2.0 * u3 + 3.0 * u2, h11 = u3 - u2;
    *x = h00 * l->sax + h10 * l->m0x + h01 * l->sbx + h11 * l->m1x;
    *y = h00 * l->say + h10 * l->m0y + h01 * l->sby + h11 * l->m1y;
}

// the way without the wander at t (0 .. 1)
static void base_point( const rlink_t* l, double t, double* x, double* y ) {
    if ( l->fa > 0.0 && t < l->fa ) {
        double s = t / l->fa;
        *x = l->x1 + ( l->sax - l->x1 ) * s; *y = l->y1 + ( l->say - l->y1 ) * s;
    } else if ( l->fb > 0.0 && t > 1.0 - l->fb ) {
        double s = ( t - ( 1.0 - l->fb ) ) / l->fb;
        *x = l->sbx + ( l->x2 - l->sbx ) * s; *y = l->sby + ( l->y2 - l->sby ) * s;
    } else {
        double span = 1.0 - l->fa - l->fb;
        hermite( l, span > 1.0e-12 ? ( t - l->fa ) / span : 0.0, x, y );
    }
}

#define STUB_LEN   700.0        // an intersection's arm runs straight (square to the intersection) for this long
#define WANDER_OFF 800.0        // no wander this near a tarred road's end

void rnet_point( const rlink_t* l, double t, double* x, double* y ) {
    base_point( l, t, x, y );
    if ( l->wamp != 0.0 ) {
        double d = ( t < 1.0 - t ? t : 1.0 - t ) * l->len - ( l->tar ? WANDER_OFF : 0.0 ), rl = l->len * 0.3 < 2500.0 ? l->len * 0.3 : 2500.0;
        double w = l->wamp * nz_noise( 77, 2, t * 3.0, l->key, 0.0 ) * smooth01( d / rl );
        *x -= ( l->y2 - l->y1 ) / l->clen * w;
        *y += ( l->x2 - l->x1 ) / l->clen * w;
    }
}

double rnet_arc( const rlink_t* l, double t ) {
    int ns = 8 + (int)( t * (double) l->steps );
    ns = ns > 400 ? 400 : ns;
    double px, py, ax, ay, sum = 0.0;
    rnet_point( l, 0.0, &ax, &ay );
    for ( int k = 1; k <= ns; ++k ) {
        rnet_point( l, t * (double) k / (double) ns, &px, &py );
        sum += sqrt( ( px - ax ) * ( px - ax ) + ( py - ay ) * ( py - ay ) );
        ax = px; ay = py;
    }
    return sum;
}

bool rnet_link( const rnet_t* n, int net, int64_t rx, int64_t ry, int e, rlink_t* l ) {
    if ( !link_exists( n, net, rx, ry, e ) ) {
        return false;
    }
    memset( l, 0, sizeof(*l) );
    net_point( n, net, rx, ry, &l->x1, &l->y1 );
    net_point( n, net, rx - ( e == 0 ), ry - ( e == 1 ), &l->x2, &l->y2 );
    double dx = l->x2 - l->x1, dy = l->y2 - l->y1;
    l->clen = sqrt( dx * dx + dy * dy );
    l->tar = net != 0;
    l->key = (double) rx * 31.0 + (double) ry * 57.0 + (double) e * 7.0 + ( net ? 7777.5 : 0.5 );
    l->step = net ? 200.0 : 50.0;
    double d1x, d1y, d2x, d2y;                              // the road's direction at the two nodes (away from the node; at the second one the travel direction is the opposite)
    node_dir( n, net, rx, ry, e == 0 ? SIDE_W : SIDE_S, &d1x, &d1y, &l->hub1 );
    node_dir( n, net, rx - ( e == 0 ), ry - ( e == 1 ), e == 0 ? SIDE_E : SIDE_N, &d2x, &d2y, &l->hub2 );
    double s1 = l->hub1 ? STUB_LEN : 0.0, s2 = l->hub2 ? STUB_LEN : 0.0;
    l->sax = l->x1 + d1x * s1; l->say = l->y1 + d1y * s1;
    l->sbx = l->x2 + d2x * s2; l->sby = l->y2 + d2y * s2;
    double k = sqrt( ( l->sbx - l->sax ) * ( l->sbx - l->sax ) + ( l->sby - l->say ) * ( l->sby - l->say ) );
    l->m0x = d1x * k; l->m0y = d1y * k;
    l->m1x = -d2x * k; l->m1y = -d2y * k;
    // the length: the stubs and the curve (24 pieces), then the shares of t
    double hl = 0.0, hx, hy, qx = l->sax, qy = l->say;
    for ( int i = 1; i <= 24; ++i ) {
        hermite( l, (double) i / 24.0, &hx, &hy );
        hl += sqrt( ( hx - qx ) * ( hx - qx ) + ( hy - qy ) * ( hy - qy ) );
        qx = hx; qy = hy;
    }
    l->len = hl + s1 + s2;
    l->wamp = net ? n->wtar : n->wdirt;                     // (a short road wanders less: the slope of the wander stays gentle, 4 % of the length at most)
    if ( l->wamp > 0.04 * l->len ) { l->wamp = 0.04 * l->len; }
    l->fa = s1 / l->len; l->fb = s2 / l->len;
    l->steps = (int)( l->len / l->step ) + 1;
    // how far the way gets from the straight line at the same t (the curve), and the wander
    double dev = 0.0;
    for ( int i = 0; i <= 64; ++i ) {
        double t = (double) i / 64.0, bx, by;
        base_point( l, t, &bx, &by );
        double ex = bx - ( l->x1 + dx * t ), ey = by - ( l->y1 + dy * t ), dd = sqrt( ex * ex + ey * ey );
        if ( dd > dev ) { dev = dd; }
    }
    l->wa = dev * 1.05 + l->wamp + 1.0;
    return true;
}

/*
A cache of what a region holds (its point and kind, its two roads with the places of all their steps): the ground function asks for the roads round a point at every sample, and
without the cache every one of them computed some hundred noise values. Direct mapped by the region; the configuration is kept with the entry (another one empties it). Every thread has a cache of its own (thread local), so there is no locking.
*/
typedef struct {
    bool    valid;
    int     net;
    int64_t rx, ry;
    rnet_t  cfg;
    double  px, py;
    int     kind;
    bool    hub;                    // an intersection (tarred roads only): 3 or 4 roads meet here
    bool    has[2];
    rlink_t link[2];
    int     npts[2];                // 0: not cached (too long)
    double* pts[2];                 // npts x 2 numbers (malloc)
} rcache_t;

#define RNET_WAYS 4
static _Thread_local rcache_t cache[RNET_CACHE];          // (one cache for each thread: the ground function is also asked by the worker thread of m3d.c)
static _Thread_local unsigned cache_next[RNET_CACHE / RNET_WAYS];

static bool same_cfg( const rnet_t* a, const rnet_t* b ) {
    return a->on == b->on && a->spacing == b->spacing && a->chance == b->chance && a->wdirt == b->wdirt && a->tspacing == b->tspacing && a->tchance == b->tchance && a->wtar == b->wtar &&
           a->mask.on == b->mask.on && a->mask.seed == b->mask.seed && a->mask.scale == b->mask.scale && a->mask.lo == b->mask.lo && a->mask.hi == b->mask.hi && a->mask.r0 == b->mask.r0 &&
           a->mask.r1 == b->mask.r1 && a->mask.limit == b->mask.limit && a->mask.margin == b->mask.margin;
}

void rnet_flush( void ) {                                  // frees the cache of this thread (a thread that goes away calls it)
    for ( int i = 0; i < RNET_CACHE; ++i ) {
        free( cache[i].pts[0] ); free( cache[i].pts[1] );
        memset( &cache[i], 0, sizeof(cache[i]) );
    }
}

static const rcache_t* region( const rnet_t* n, int net, int64_t rx, int64_t ry ) {
    uint64_t h = nz_mix( (uint64_t) rx * 0x9E3779B97F4A7C15ULL + (uint64_t) ry * 0xC2B2AE3D27D4EB4FULL + (uint64_t) net );
    unsigned set = (unsigned)( h % ( RNET_CACHE / RNET_WAYS ) );
    rcache_t* base = &cache[set * RNET_WAYS];
    for ( int w = 0; w < RNET_WAYS; ++w ) {
        rcache_t* c = &base[w];
        if ( c->valid && c->net == net && c->rx == rx && c->ry == ry && same_cfg( &c->cfg, n ) ) {
            return c;
        }
    }
    rcache_t* c = &base[ cache_next[set]++ % RNET_WAYS ];      // the oldest one of the set goes
    free( c->pts[0] ); free( c->pts[1] );
    memset( c, 0, sizeof(*c) );
    c->valid = true; c->net = net; c->rx = rx; c->ry = ry; c->cfg = *n;
    net_point( n, net, rx, ry, &c->px, &c->py );
    double u = nz_hash( rx, ry, 3, 0 );
    c->kind = u < 0.38 ? 3 : u < 0.66 ? 1 : u < 0.86 ? 2 : 0;
    c->hub = net && ( link_exists( n, net, rx, ry, 0 ) + link_exists( n, net, rx, ry, 1 ) + link_exists( n, net, rx + 1, ry, 0 ) + link_exists( n, net, rx, ry + 1, 1 ) ) >= 3;
    for ( int e = 0; e < 2; ++e ) {
        c->has[e] = rnet_link( n, net, rx, ry, e, &c->link[e] );
        if ( c->has[e] && c->link[e].steps <= RNET_MAXPTS ) {
            c->pts[e] = (double*) malloc( sizeof(double) * 2U * (size_t)( c->link[e].steps + 1 ) );
            if ( c->pts[e] != 0 ) {
                for ( int k = 0; k <= c->link[e].steps; ++k ) {
                    rnet_point( &c->link[e], (double) k / (double) c->link[e].steps, &c->pts[e][2 * k], &c->pts[e][2 * k + 1] );
                }
                c->npts[e] = c->link[e].steps + 1;
            }
        }
    }
    return c;
}

int rnet_links( const rnet_t* n, int net, double x0, double y0, double x1, double y1, rlink_t* out, int max ) {
    int cnt = 0;
    double sp = net ? n->tspacing : n->spacing;
    if ( !n->on || sp <= 0.0 ) {
        return 0;
    }
    double wa = ( net ? n->wtar : n->wdirt ) + 0.4 * sp;               // (a curve bulges by a tenth of its length, the nodes are a region away)
    int64_t ax = (int64_t) floor( ( x0 - wa ) / sp ) - 1, bx = (int64_t) floor( ( x1 + wa ) / sp ) + 1;
    int64_t ay = (int64_t) floor( ( y0 - wa ) / sp ) - 1, by = (int64_t) floor( ( y1 + wa ) / sp ) + 1;
    for ( int64_t ry = ay; ry <= by; ++ry ) {
        for ( int64_t rx = ax; rx <= bx; ++rx ) {
            const rcache_t* rc = region( n, net, rx, ry );
            for ( int e = 0; e < 2; ++e ) {
                if ( !rc->has[e] ) {
                    continue;
                }
                const rlink_t* l = &rc->link[e];
                if ( fmax( l->x1, l->x2 ) + l->wa < x0 || fmin( l->x1, l->x2 ) - l->wa > x1 || fmax( l->y1, l->y2 ) + l->wa < y0 || fmin( l->y1, l->y2 ) - l->wa > y1 ) {
                    continue;
                }
                if ( cnt < max ) {
                    out[cnt] = *l;
                }
                ++cnt;
            }
        }
    }
    return cnt < max ? cnt : max;
}

bool rnet_cached_link( const rnet_t* n, int net, int64_t rx, int64_t ry, int e, rlink_t* l ) {
    if ( !n->on || ( net ? n->tspacing <= 0.0 : n->spacing <= 0.0 ) ) {
        return false;
    }
    const rcache_t* c = region( n, net, rx, ry );
    if ( !c->has[e] ) {
        return false;
    }
    *l = c->link[e];
    return true;
}

int rnet_hubs( const rnet_t* n, double x0, double y0, double x1, double y1, double* xy, int max ) {
    int cnt = 0;
    double sp = n->tspacing;
    if ( !n->on || sp <= 0.0 ) {
        return 0;
    }
    int64_t ax = (int64_t) floor( x0 / sp ), bx = (int64_t) floor( x1 / sp ), ay = (int64_t) floor( y0 / sp ), by = (int64_t) floor( y1 / sp );
    for ( int64_t ry = ay; ry <= by; ++ry ) {
        for ( int64_t rx = ax; rx <= bx; ++rx ) {
            const rcache_t* c = region( n, 1, rx, ry );
            if ( c->hub && c->px >= x0 && c->px < x1 && c->py >= y0 && c->py < y1 ) {
                if ( cnt < max ) { xy[2 * cnt] = c->px; xy[2 * cnt + 1] = c->py; }
                ++cnt;
            }
        }
    }
    return cnt < max ? cnt : max;
}

void rnet_near( const rnet_t* n, double x, double y, double reach, rnear_t* out ) {
    out->dirt = out->tar = 1.0e9;
    out->tt = 0.0;
    if ( !n->on ) {
        return;
    }
    if ( reach > 700.0 ) {
        reach = 700.0;
    }
    for ( int net = 0; net < 2; ++net ) {
        double sp = net ? n->tspacing : n->spacing;
        if ( sp <= 0.0 ) {
            continue;
        }
        int64_t rx = (int64_t) floor( x / sp ), ry = (int64_t) floor( y / sp );
        for ( int j = -1; j <= 1; ++j ) {
            for ( int i = -1; i <= 1; ++i ) {
                const rcache_t* rc = region( n, net, rx + i, ry + j );
                for ( int e = 0; e < 2; ++e ) {
                    if ( !rc->has[e] ) {
                        continue;
                    }
                    const rlink_t* l = &rc->link[e];
                    double wmax = l->wa + reach;
                    if ( x < fmin( l->x1, l->x2 ) - wmax || x > fmax( l->x1, l->x2 ) + wmax || y < fmin( l->y1, l->y2 ) - wmax || y > fmax( l->y1, l->y2 ) + wmax ) {
                        continue;
                    }
                    double best = 1.0e9, bt = 0.0, px, py, ax, ay;
                    // only the steps whose place along the straight line from point to point is near that of (x, y): the road never leaves the line by more than wa
                    double cdx = l->x2 - l->x1, cdy = l->y2 - l->y1, t0 = ( ( x - l->x1 ) * cdx + ( y - l->y1 ) * cdy ) / ( l->clen * l->clen ), dt = wmax / l->clen;
                    int kmin = (int) floor( ( t0 - dt ) * (double) l->steps ), kmax = (int) ceil( ( t0 + dt ) * (double) l->steps );
                    kmin = kmin < 0 ? 0 : kmin; kmax = kmax > l->steps - 1 ? l->steps - 1 : kmax;
                    if ( kmin > kmax ) {
                        continue;
                    }
                    if ( rc->npts[e] > 0 ) {
                        ax = rc->pts[e][2 * kmin]; ay = rc->pts[e][2 * kmin + 1];
                    } else {
                        rnet_point( l, (double) kmin / (double) l->steps, &ax, &ay );
                    }
                    int kstart = kmin;
                    if ( rn_simd < 0 ) {
                        rn_simd = getenv( "ENGINE1999_NOSIMD" ) != 0 ? 0 : 1;
                    }
                    #if E99_VEC
                    if ( rn_simd && rc->npts[e] > 0 ) {
                        // four segments at a time: their distances to (x, y) are found together (the division and the clamp); the square root (and the choice of the
                        // first nearest one) is made only for a lane that can beat the best so far: d2 > best^2 (1 + 1e-9) cannot (so the result is the scalar loop's)
                        const double* P = rc->pts[e];
                        v4d vx = { x, x, x, x }, vy = { y, y, y, y }, zero = { 0, 0, 0, 0 }, one = { 1, 1, 1, 1 }, eps = { 1.0e-12, 1.0e-12, 1.0e-12, 1.0e-12 };
                        for ( ; kstart + 3 <= kmax; kstart += 4 ) {
                            const double* q = P + 2 * kstart;
                            v4d ax4 = { q[0], q[2], q[4], q[6] }, ay4 = { q[1], q[3], q[5], q[7] }, px4 = { q[2], q[4], q[6], q[8] }, py4 = { q[3], q[5], q[7], q[9] };
                            v4d ux4 = px4 - ax4, uy4 = py4 - ay4, len24 = ux4 * ux4 + uy4 * uy4;
                            v4d num = ( vx - ax4 ) * ux4 + ( vy - ay4 ) * uy4;
                            v4d u4 = num / len24;
                            v4l big = len24 > eps;
                            u4 = (v4d)( (v4l) u4 & big );                             // len2 <= 1e-12: u = 0
                            v4l lo = u4 < zero, hi = u4 > one;
                            u4 = (v4d)( ( (v4l) u4 & ~lo ) & ~hi ) + (v4d)( (v4l) one & hi );
                            v4d dx4 = ax4 + u4 * ux4 - vx, dy4 = ay4 + u4 * uy4 - vy, d24 = dx4 * dx4 + dy4 * dy4;
                            double bl = best * best * ( 1.0 + 1.0e-9 );
                            for ( int j = 0; j < 4; ++j ) {
                                if ( d24[j] <= bl ) {
                                    double d = sqrt( d24[j] );
                                    if ( d < best ) {
                                        best = d;
                                        bt = ( (double)( kstart + j ) + u4[j] ) / (double) l->steps;
                                        bl = best * best * ( 1.0 + 1.0e-9 );
                                    }
                                }
                            }
                        }
                        if ( kstart <= kmax ) {
                            ax = P[2 * kstart]; ay = P[2 * kstart + 1];
                        }
                    }
#endif
                    for ( int k = kstart; k <= kmax; ++k ) {
                        if ( rc->npts[e] > 0 ) {
                            px = rc->pts[e][2 * ( k + 1 )]; py = rc->pts[e][2 * ( k + 1 ) + 1];
                        } else {
                            rnet_point( l, (double)( k + 1 ) / (double) l->steps, &px, &py );
                        }
                        double ux = px - ax, uy = py - ay, len2 = ux * ux + uy * uy;
                        double u = len2 > 1.0e-12 ? ( ( x - ax ) * ux + ( y - ay ) * uy ) / len2 : 0.0;
                        u = u < 0.0 ? 0.0 : u > 1.0 ? 1.0 : u;
                        double ddx = ax + u * ux - x, ddy = ay + u * uy - y, d = sqrt( ddx * ddx + ddy * ddy );
                        if ( d < best ) {
                            best = d;
                            bt = ( (double) k + u ) / (double) l->steps;
                        }
                        ax = px; ay = py;
                    }
                    if ( l->tar ) {
                        if ( best < out->tar ) { out->tar = best; out->tlink = *l; out->tt = bt; }
                    } else if ( best < out->dirt ) {
                        out->dirt = best;
                    }
                }
            }
        }
    }
}

// the same without the region cache (no links are made: a place that is asked about once, as the world creation does for a million chunks, would only fill the cache)
int rnet_kind_cold( const rnet_t* n, double x, double y ) {
    if ( !n->on || n->spacing <= 0.0 ) {
        return 0;
    }
    int64_t rx = (int64_t) floor( x / n->spacing ), ry = (int64_t) floor( y / n->spacing );
    double bd = 1.0e30;
    int kind = 0;
    for ( int j = -1; j <= 1; ++j ) {
        for ( int i = -1; i <= 1; ++i ) {
            double px, py;
            net_point( n, 0, rx + i, ry + j, &px, &py );
            double d = ( px - x ) * ( px - x ) + ( py - y ) * ( py - y );
            if ( d < bd ) {
                bd = d;
                double u = nz_hash( rx + i, ry + j, 3, 0 );
                kind = u < 0.38 ? 3 : u < 0.66 ? 1 : u < 0.86 ? 2 : 0;
            }
        }
    }
    return kind;
}

int rnet_kind( const rnet_t* n, double x, double y ) {
    if ( !n->on || n->spacing <= 0.0 ) {
        return 0;
    }
    int64_t rx = (int64_t) floor( x / n->spacing ), ry = (int64_t) floor( y / n->spacing );
    double bd = 1.0e30;
    int kind = 0;
    for ( int j = -1; j <= 1; ++j ) {
        for ( int i = -1; i <= 1; ++i ) {
            const rcache_t* rc = region( n, 0, rx + i, ry + j );
            double d = ( rc->px - x ) * ( rc->px - x ) + ( rc->py - y ) * ( rc->py - y );
            if ( d < bd ) {
                bd = d;
                kind = rc->kind;
            }
        }
    }
    return kind;
}
