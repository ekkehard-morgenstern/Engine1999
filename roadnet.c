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
#include <string.h>
#include <stdlib.h>

#define RNET_MAXPTS 320         // a road of more steps than this is not cached (computed again at every call)
#define RNET_CACHE  512

// the point of region (rx, ry) of a network (0 dirt: salts 1 / 2, 1 tarred: salts 11 / 12 and its own spacing)
static void net_point( const rnet_t* n, int net, int64_t rx, int64_t ry, double* x, double* y ) {
    double sp = net ? n->tspacing : n->spacing;
    *x = (double) rx * sp + nz_hash( rx, ry, net ? 11 : 1, 0 ) * sp;
    *y = (double) ry * sp + nz_hash( rx, ry, net ? 12 : 2, 0 ) * sp;
}

bool rnet_link( const rnet_t* n, int net, int64_t rx, int64_t ry, int e, rlink_t* l ) {
    if ( !n->on || ( net ? n->tspacing <= 0.0 : n->spacing <= 0.0 ) || nz_hash( rx, ry, ( net ? 160 : 60 ) + e, 0 ) >= ( net ? n->tchance : n->chance ) ) {
        return false;
    }
    net_point( n, net, rx, ry, &l->x1, &l->y1 );
    net_point( n, net, rx - ( e == 0 ), ry - ( e == 1 ), &l->x2, &l->y2 );
    double dx = l->x2 - l->x1, dy = l->y2 - l->y1;
    l->len = sqrt( dx * dx + dy * dy );
    l->tar = net != 0;
    l->key = (double) rx * 31.0 + (double) ry * 57.0 + (double) e * 7.0 + ( net ? 7777.5 : 0.5 );
    l->step = net ? 200.0 : 50.0;
    l->steps = (int)( l->len / l->step ) + 1;
    l->wa = net ? n->wtar : n->wdirt;
    return l->len > 1.0e-9;
}

void rnet_point( const rlink_t* l, double t, double* x, double* y ) {
    double w = l->wa * nz_noise( 77, 2, t * 3.0, l->key, 0.0 ) * sin( 3.14159265358979323846 * t );
    *x = l->x1 + ( l->x2 - l->x1 ) * t - ( l->y2 - l->y1 ) / l->len * w;
    *y = l->y1 + ( l->y2 - l->y1 ) * t + ( l->x2 - l->x1 ) / l->len * w;
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
    bool    has[2];
    rlink_t link[2];
    int     npts[2];                // 0: not cached (too long)
    double* pts[2];                 // npts x 2 numbers (malloc)
} rcache_t;

#define RNET_WAYS 4
static _Thread_local rcache_t cache[RNET_CACHE];          // (one cache for each thread: the ground function is also asked by the worker thread of m3d.c)
static _Thread_local unsigned cache_next[RNET_CACHE / RNET_WAYS];

static bool same_cfg( const rnet_t* a, const rnet_t* b ) {
    return a->on == b->on && a->spacing == b->spacing && a->chance == b->chance && a->wdirt == b->wdirt && a->tspacing == b->tspacing && a->tchance == b->tchance && a->wtar == b->wtar;
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
    double wa = net ? n->wtar : n->wdirt;
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
                    double cdx = l->x2 - l->x1, cdy = l->y2 - l->y1, t0 = ( ( x - l->x1 ) * cdx + ( y - l->y1 ) * cdy ) / ( l->len * l->len ), dt = wmax / l->len;
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
                    for ( int k = kmin; k <= kmax; ++k ) {
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
