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

#include "house.h"
#include "noise.h"

// -- the generator: a port of GENHOUSE of tools/house3d.bas (the order of the random numbers is the same) -----------------------------------------------------

typedef struct {
    house_t* h;
    int64_t  sd;
    int64_t  rc;
    bool     okp;
    int      pw;
    double   lq[4];
} gen_t;

#define TW ( (double) HOUSE_TW )

static double rn( gen_t* g ) {
    ++g->rc;
    return nz_hash( g->sd * 7919, g->rc, 17, 0 );
}

static int ri( gen_t* g, int a, int b ) {
    return a + (int) floor( rn( g ) * (double)( b - a + 1 ) );
}

// is the box free of furniture and of the clearances of the doors and stairs of floor f?
static bool freebox( const gen_t* g, int f, double x1, double y1, double x2, double y2 ) {
    const house_t* h = g->h;
    for ( int i = 0; i < h->nfu[f]; ++i ) {
        const hfurn_t* u = &h->fu[f][i];
        if ( x1 < u->x2 && u->x1 < x2 && y1 < u->y2 && u->y1 < y2 ) { return false; }
    }
    for ( int i = 0; i < h->ncl[f]; ++i ) {
        const double* c = h->cl[f][i];
        if ( x1 < c[2] && c[0] < x2 && y1 < c[3] && c[1] < y2 ) { return false; }
    }
    return true;
}

static void addroom( gen_t* g, int f, double x1, double y1, double x2, double y2, int t ) {
    house_t* h = g->h;
    int n = h->nrm[f];
    if ( n >= HOUSE_MAXROOMS ) { return; }
    hroom_t* r = &h->rm[f][n];
    r->x1 = x1; r->y1 = y1; r->x2 = x2; r->y2 = y2; r->type = t; r->color = 20 + ri( g, 0, 5 );
    h->nrm[f] = n + 1;
}

static void adddoor( gen_t* g, int f, int ax, double x, double y, double w ) {
    house_t* h = g->h;
    int n = h->ndor[f];
    if ( n < HOUSE_MAXDOORS ) {
        h->dor[f][n].axis = ax; h->dor[f][n].x = x; h->dor[f][n].y = y; h->dor[f][n].w = w;
        h->ndor[f] = n + 1;
    }
    int c = h->ncl[f];
    if ( c < HOUSE_MAXCLEAR ) {
        double* q = h->cl[f][c];
        if ( ax == 0 ) { q[0] = x - 9; q[1] = y - w / 2; q[2] = x + 10; q[3] = y + w / 2; }
        else           { q[0] = x - w / 2; q[1] = y - 9; q[2] = x + w / 2; q[3] = y + 10; }
        h->ncl[f] = c + 1;
    }
}

static void addwin( gen_t* g, int f, int side, double pp, double w, double sl, double hd, int k ) {
    house_t* h = g->h;
    if ( h->nwin >= HOUSE_MAXWINS ) { return; }
    hwin_t* x = &h->win[h->nwin++];
    x->floor = f; x->side = side; x->pos = pp; x->w = w; x->sill = sl; x->head = hd; x->kind = k;
}

static void wallwin( gen_t* g, int f, int side, double a1, double a2, double w, double sl, double hd, int k ) {
    int n = (int) floor( ( a2 - a1 ) / 32.0 );
    if ( n < 1 ) { n = 1; }
    double w2 = w, s2 = sl, h2 = hd;
    int k2 = k;
    if ( k2 == 1 ) {
        if ( a2 - a1 < 28.0 ) {
            k2 = 0;
        } else {
            n = 1; w2 = 20; s2 = 0; h2 = 22;
        }
    }
    for ( int p = 0; p < n; ++p ) {
        addwin( g, f, side, a1 + ( a2 - a1 ) * ( p + 0.5 ) / n, w2, s2, h2, k2 );
    }
}

static void roomwin( gen_t* g, int f, int i ) {
    const house_t* h = g->h;
    const hroom_t* r = &h->rm[f][i];
    double w = 12, sl = 9, hd = 21;
    if ( r->type == 2 || r->type == 9 ) { w = 6; sl = 14; hd = 22; }
    if ( r->y1 == TW ) { wallwin( g, f, 0, r->x1, r->x2, w, sl, hd, 0 ); }
    if ( r->y2 == h->dy - TW ) {
        int k = r->type == 3 ? 1 : 0;
        wallwin( g, f, 1, r->x1, r->x2, w, sl, hd, k );
    }
    if ( r->x1 == TW ) { wallwin( g, f, 2, r->y1, r->y2, w, sl, hd, 0 ); }
    if ( r->x2 == h->lx - TW ) { wallwin( g, f, 3, r->y1, r->y2, w, sl, hd, 0 ); }
}

static void addfu( gen_t* g, int f, double x1, double y1, double x2, double y2, int c ) {
    house_t* h = g->h;
    int n = h->nfu[f];
    if ( n >= HOUSE_MAXFURN ) { return; }
    hfurn_t* u = &h->fu[f][n];
    u->x1 = x1; u->y1 = y1; u->x2 = x2; u->y2 = y2; u->color = c;
    h->nfu[f] = n + 1;
}

// a piece of furniture against a wall of room i: w along the wall, hh deep; wall 0 south, 1 north, 2 west, 3 east, tried from wall on; pf 0 .. 1 where along the wall
static void setf( gen_t* g, int f, int i, double w, double hh, int wall, double pf, int c ) {
    const hroom_t* r = &g->h->rm[f][i];
    double rx1 = r->x1, ry1 = r->y1, rx2 = r->x2, ry2 = r->y2;
    g->okp = false;
    for ( int t = 0; t < 8; ++t ) {
        int wl = ( wall + t ) % 4;
        double lng = wl < 2 ? rx2 - rx1 - w : ry2 - ry1 - w;
        if ( lng >= 0 ) {
            double q = pf + t * 0.37;
            q = q - floor( q );
            double a = q * lng, x1 = 0, y1 = 0, x2 = 0, y2 = 0;
            if ( wl == 0 ) { x1 = rx1 + a; y1 = ry1; x2 = x1 + w; y2 = y1 + hh; }
            if ( wl == 1 ) { x1 = rx1 + a; y2 = ry2; x2 = x1 + w; y1 = y2 - hh; }
            if ( wl == 2 ) { y1 = ry1 + a; x1 = rx1; y2 = y1 + w; x2 = x1 + hh; }
            if ( wl == 3 ) { y1 = ry1 + a; x2 = rx2; y2 = y1 + w; x1 = x2 - hh; }
            if ( x1 >= rx1 && x2 <= rx2 && y1 >= ry1 && y2 <= ry2 ) {
                if ( freebox( g, f, x1, y1, x2, y2 ) ) {
                    addfu( g, f, x1, y1, x2, y2, c );
                    g->lq[0] = x1; g->lq[1] = y1; g->lq[2] = x2; g->lq[3] = y2;
                    g->pw = wl; g->okp = true;
                    return;
                }
            }
        }
    }
}

// a piece of furniture in the open (the middle of the room, 5 dm from the walls)
static void setc( gen_t* g, int f, int i, double w, double hh, int c ) {
    const hroom_t* r = &g->h->rm[f][i];
    double rx1 = r->x1 + 5, ry1 = r->y1 + 5, rx2 = r->x2 - 5, ry2 = r->y2 - 5;
    g->okp = false;
    if ( rx2 - rx1 < w || ry2 - ry1 < hh ) { return; }
    double ax = rx2 - rx1 - w, ay = ry2 - ry1 - hh;
    for ( int t = 0; t < 8; ++t ) {
        double r1 = rn( g );
        double x1 = rx1 + ax * ( 0.5 + ( r1 - 0.5 ) * 0.5 );
        double r2 = rn( g );
        double y1 = ry1 + ay * ( 0.5 + ( r2 - 0.5 ) * 0.5 );
        double x2 = x1 + w, y2 = y1 + hh;
        if ( freebox( g, f, x1, y1, x2, y2 ) ) {
            addfu( g, f, x1, y1, x2, y2, c );
            g->lq[0] = x1; g->lq[1] = y1; g->lq[2] = x2; g->lq[3] = y2;
            g->okp = true;
            return;
        }
    }
}

// SETF with the random arguments of the BASIC calls evaluated in their order: the wall (RI(0,3)) and the place (RN())
static void setfr( gen_t* g, int f, int i, double w, double hh, int c ) {
    int wall = ri( g, 0, 3 );
    double pf = rn( g );
    setf( g, f, i, w, hh, wall, pf, c );
}

static void fitroom( gen_t* g, int f, int i ) {
    house_t* h = g->h;
    const hroom_t* r = &h->rm[f][i];
    int t = r->type;
    if ( t == 1 ) {
        setfr( g, f, i, 10, 3.5, 41 );
        setfr( g, f, i, 6, 2, 39 );
    }
    if ( t == 2 ) {
        setf( g, f, i, 4.5, 7, 1, 0.5, 54 );
        setfr( g, f, i, 4, 3.5, 54 );
    }
    if ( t == 3 ) {
        int c = 42;
        if ( rn( g ) < 0.4 ) { c = 53; }
        if ( rn( g ) < 0.25 ) { c = 58; }
        setfr( g, f, i, 20, 9, c );
        if ( g->okp ) {
            int op = g->pw + 1 - 2 * ( g->pw % 2 );
            setf( g, f, i, 15, 4.5, op, 0.5, 40 );
            setc( g, f, i, 10, 6, 41 );
        }
        setfr( g, f, i, 12, 3.5, 40 );
        setfr( g, f, i, 8, 8, c );
    }
    if ( t == 4 ) {
        setc( g, f, i, 16, 9, 40 );
        if ( g->okp ) {
            double tx1 = g->lq[0], ty1 = g->lq[1], tx2 = g->lq[2], ty2 = g->lq[3];
            for ( int n = 0; n <= 1; ++n ) {
                addfu( g, f, tx1 + 3 + n * 7, ty1 - 3.5, tx1 + 6 + n * 7, ty1 - 0.5, 41 );
                addfu( g, f, tx1 + 3 + n * 7, ty2 + 0.5, tx1 + 6 + n * 7, ty2 + 3.5, 41 );
            }
            if ( rn( g ) < 0.5 ) { addfu( g, f, tx1 - 3.5, ty1 + 3, tx1 - 0.5, ty1 + 6, 41 ); }
            if ( rn( g ) < 0.5 ) { addfu( g, f, tx2 + 0.5, ty1 + 3, tx2 + 3.5, ty1 + 6, 41 ); }
        }
        setfr( g, f, i, 14, 4, 41 );
    }
    if ( t == 5 ) {
        setf( g, f, i, r->x2 - r->x1 - 8, 6, ri( g, 0, 3 ), 0.5, 54 );
        int wl = g->pw;
        if ( g->okp ) {
            if ( wl < 2 ) { setf( g, f, i, 18, 6, 2 + ri( g, 0, 1 ), 0.5, 54 ); }
            else          { setf( g, f, i, 18, 6, ri( g, 0, 1 ), 0.5, 54 ); }
        }
        setfr( g, f, i, 6, 6, 54 );
        setc( g, f, i, 10, 8, 40 );
        if ( g->okp ) {
            double tx1 = g->lq[0], ty1 = g->lq[1], tx2 = g->lq[2], ty2 = g->lq[3];
            addfu( g, f, tx1 + 1, ty1 - 3.5, tx1 + 4, ty1 - 0.5, 41 ); addfu( g, f, tx2 - 4, ty2 + 0.5, tx2 - 1, ty2 + 3.5, 41 );
        }
    }
    if ( t == 6 ) {
        setfr( g, f, i, 14, 7, 40 );
        setfr( g, f, i, 12, 3.5, 41 );
        setfr( g, f, i, 12, 3.5, 41 );
        setfr( g, f, i, 16, 8, 42 );
    }
    if ( t == 7 ) {
        setfr( g, f, i, 18, 21, 53 );
        setfr( g, f, i, 4.5, 4.5, 40 );
        setfr( g, f, i, 4.5, 4.5, 40 );
        setfr( g, f, i, 20, 6.5, 39 );
        setfr( g, f, i, 12, 4.5, 41 );
    }
    if ( t == 8 ) {
        setfr( g, f, i, 9, 20, 42 );
        setfr( g, f, i, 12, 6, 39 );
        setfr( g, f, i, 12, 6, 40 );
        if ( r->y2 - r->y1 > 50 ) { setfr( g, f, i, 9, 20, 58 ); setfr( g, f, i, 16, 3.5, 41 ); }
    }
    if ( t == 9 ) {
        setf( g, f, i, 17, 7.5, ri( g, 0, 1 ), 0.5, 54 );
        setfr( g, f, i, 6, 5, 54 );
        setfr( g, f, i, 4.5, 7, 54 );
    }
    if ( t == 11 ) {
        setc( g, f, i, 16, 8, 40 );
        if ( g->okp ) {
            double tx1 = g->lq[0], ty1 = g->lq[1], tx2 = g->lq[2], ty2 = g->lq[3];
            addfu( g, f, tx1 + 2, ty1 - 3.5, tx1 + 5, ty1 - 0.5, 41 ); addfu( g, f, tx2 - 5, ty2 + 0.5, tx2 - 2, ty2 + 3.5, 41 );
        }
        setfr( g, f, i, 14, 3.5, 41 );
        setfr( g, f, i, 10, 6, 42 );
    }
    if ( t == 10 ) {
        setfr( g, f, i, 24, 6, 41 );
        setfr( g, f, i, 16, 3.5, 40 );
        setfr( g, f, i, 8, 8, 39 );
        setc( g, f, i, 6, 6, 40 );
    }
}

void house_gen( house_t* h, int type, int64_t seed ) {
    gen_t gg, *g = &gg;
    memset( h, 0, sizeof(*h) );
    memset( g, 0, sizeof(*g) );
    g->h = h; g->sd = seed;
    h->type = type; h->seed = seed;
    h->flz[0] = 3; h->flz[1] = 32;
    int ht = type;
    h->lx = ri( g, 100, 120 ); h->dy = ri( g, 80, 100 );
    double hx = ri( g, 26, 30 ), iw = h->lx - 2 * TW;
    double lw = floor( ( iw - hx - 2 ) * ( 0.45 + rn( g ) * 0.1 ) );
    double fd = ri( g, 28, 36 );
    if ( fd > h->dy - 50 ) { fd = h->dy - 50; }
    double wcd = ri( g, 20, 26 );
    double xa1 = TW + lw, xa2 = xa1 + 1, xb1 = xa2 + hx, xb2 = xb1 + 1, xe = h->lx - TW, ye = h->dy - TW;
    double yf1 = TW + fd, yf2 = yf1 + 1;
    h->wy = ye - wcd - 1;
    double wy = h->wy;
    h->ss = ri( g, 0, 1 );
    if ( h->ss == 0 ) { h->sx1 = xa2; h->sx2 = xa2 + 10; } else { h->sx1 = xb1 - 10; h->sx2 = xb1; }
    h->sy1 = wy - 31; h->sy2 = wy;
    for ( int i = 0; i <= 1; ++i ) {
        int c = h->ncl[i];
        h->cl[i][c][0] = h->sx1 - 1; h->cl[i][c][1] = h->sy1 - 1; h->cl[i][c][2] = h->sx2 + 1; h->cl[i][c][3] = h->sy2;
        h->ncl[i] = c + 1;
    }
    double xc = ( xa2 + xb1 ) / 2, xd = xc + 4;
    if ( h->ss == 1 ) { xd = xc - 4; }
    // the ground floor
    addroom( g, 0, xa2, TW, xb1, wy, 1 ); addroom( g, 0, xa2, wy + 1, xb1, ye, 2 );
    if ( h->ss == 0 ) {
        addroom( g, 0, TW, TW, xa1, yf1, 4 ); addroom( g, 0, TW, yf2, xa1, ye, 3 );
        addroom( g, 0, xb2, TW, xe, yf1, 6 ); addroom( g, 0, xb2, yf2, xe, ye, 5 );
    } else {
        addroom( g, 0, TW, TW, xa1, yf1, 6 ); addroom( g, 0, TW, yf2, xa1, ye, 5 );
        addroom( g, 0, xb2, TW, xe, yf1, 4 ); addroom( g, 0, xb2, yf2, xe, ye, 3 );
    }
    addwin( g, 0, 0, xd, 10, 0, 21, 2 );
    adddoor( g, 0, 1, xd, wy, 8 );
    if ( h->ss == 0 ) {
        adddoor( g, 0, 0, xa1, ri( g, (int) TW + 5, (int) yf1 - 5 ), 9 );
        adddoor( g, 0, 1, ( TW + xa1 ) / 2, yf1, 16 );
        adddoor( g, 0, 0, xb1, ri( g, (int) TW + 5, (int) yf1 - 5 ), 9 );
        adddoor( g, 0, 0, xb1, ri( g, (int) yf2 + 5, (int) wy - 5 ), 9 );
    } else {
        adddoor( g, 0, 0, xb1, ri( g, (int) TW + 5, (int) yf1 - 5 ), 9 );
        adddoor( g, 0, 1, ( xb2 + xe ) / 2, yf1, 16 );
        adddoor( g, 0, 0, xa1, ri( g, (int) TW + 5, (int) yf1 - 5 ), 9 );
        adddoor( g, 0, 0, xa1, ri( g, (int) yf2 + 5, (int) wy - 5 ), 9 );
    }
    // the upper floor
    addroom( g, 1, xa2, TW, xb1, wy, 1 ); addroom( g, 1, xa2, wy + 1, xb1, ye, 9 );
    if ( h->ss == 0 ) {
        addroom( g, 1, TW, TW, xa1, ye, 8 );
        addroom( g, 1, xb2, TW, xe, yf1, 8 ); addroom( g, 1, xb2, yf2, xe, ye, 7 );
    } else {
        addroom( g, 1, xb2, TW, xe, ye, 8 );
        addroom( g, 1, TW, TW, xa1, yf1, 8 ); addroom( g, 1, TW, yf2, xa1, ye, 7 );
    }
    addwin( g, 1, 0, xc, 10, 9, 21, 0 );
    adddoor( g, 1, 1, xd, wy, 8 );
    if ( h->ss == 0 ) {
        adddoor( g, 1, 0, xa1, ri( g, (int) TW + 5, (int) h->sy1 - 5 ), 9 );
        adddoor( g, 1, 0, xb1, ri( g, (int) TW + 5, (int) yf1 - 5 ), 9 );
        adddoor( g, 1, 0, xb1, ri( g, (int) yf2 + 5, (int) wy - 5 ), 9 );
    } else {
        adddoor( g, 1, 0, xb1, ri( g, (int) TW + 5, (int) h->sy1 - 5 ), 9 );
        adddoor( g, 1, 0, xa1, ri( g, (int) TW + 5, (int) yf1 - 5 ), 9 );
        adddoor( g, 1, 0, xa1, ri( g, (int) yf2 + 5, (int) wy - 5 ), 9 );
    }
    // the windows (not for the first room of a floor, the hall)
    for ( int c = 0; c <= 1; ++c ) {
        for ( int i = 0; i < h->nrm[c]; ++i ) {
            if ( i > 0 ) { roomwin( g, c, i ); }
        }
    }
    // type 2: a single storey annex (the garage) at the east gable, with a door from the east room at the front
    if ( ht == 2 ) {
        h->ai = ri( g, 45, 55 ); h->ay = ri( g, 50, 65 );
        addroom( g, 0, h->lx, TW, h->lx + h->ai, h->ay, 10 );
        int j = 0;
        for ( int i = 0; i < h->nwin; ++i ) {
            if ( !( h->win[i].floor == 0 && h->win[i].side == 3 && h->win[i].pos - h->win[i].w / 2 < h->ay + TW ) ) {
                h->win[j++] = h->win[i];
            }
        }
        h->nwin = j;
        addwin( g, 0, 3, ( TW + yf1 ) / 2, 9, 0, 21, 3 );
        addwin( g, 0, 4, h->lx + h->ai / 2, 24, 0, 22, 4 );
        addwin( g, 0, 5, h->ay / 2 + 2, 10, 10, 20, 0 );
    }
    // type 5: a bay window (two storeys) in the front of the west front room and a flat roofed extension at the back, at the widest back room that is not the WC
    if ( ht == 5 ) {
        h->bc = h->lx / 4; h->bw = 16;
        for ( int i = 0; i < h->nrm[0]; ++i ) {
            if ( h->rm[0][i].x1 == TW && h->rm[0][i].y1 == TW ) {
                h->bc = ( h->rm[0][i].x1 + h->rm[0][i].x2 ) / 2;
                h->bw = fmin( 18.0, h->rm[0][i].x2 - h->rm[0][i].x1 - 8 );
            }
        }
        h->bx1 = h->bc - h->bw / 2; h->bx2 = h->bc + h->bw / 2;
        int j = 0;
        for ( int i = 0; i < h->nwin; ++i ) {
            if ( !( h->win[i].side == 0 && h->win[i].pos + h->win[i].w / 2 > h->bx1 - 1 && h->win[i].pos - h->win[i].w / 2 < h->bx2 + 1 ) ) {
                h->win[j++] = h->win[i];
            }
        }
        h->nwin = j;
        addwin( g, 0, 0, h->bc, h->bw, 0, 21, 5 ); addwin( g, 1, 0, h->bc, h->bw, 0, 21, 5 );
        double exd = 0, exc = h->lx / 2;
        for ( int i = 0; i < h->nrm[0]; ++i ) {
            const hroom_t* r = &h->rm[0][i];
            if ( r->y2 == ye && r->type != 2 && r->x2 - r->x1 > exd ) { exd = r->x2 - r->x1; exc = ( r->x1 + r->x2 ) / 2; }
        }
        h->exw = ri( g, 34, 46 ); h->exy = ri( g, 28, 38 );
        h->exx1 = exc - h->exw / 2; h->exx2 = h->exx1 + h->exw;
        if ( h->exx1 < TW + 4 ) { h->exx1 = TW + 4; h->exx2 = h->exx1 + h->exw; }
        if ( h->exx2 > h->lx - TW - 4 ) { h->exx2 = h->lx - TW - 4; h->exx1 = h->exx2 - h->exw; }
        if ( exc < h->exx1 + 6 ) { exc = h->exx1 + 6; }
        if ( exc > h->exx2 - 6 ) { exc = h->exx2 - 6; }
        addroom( g, 0, h->exx1, h->dy, h->exx2, h->dy + h->exy, 11 );
        j = 0;
        for ( int i = 0; i < h->nwin; ++i ) {
            if ( !( h->win[i].floor == 0 && h->win[i].side == 1 && h->win[i].pos + h->win[i].w / 2 > h->exx1 - 4 && h->win[i].pos - h->win[i].w / 2 < h->exx2 + 4 ) ) {
                h->win[j++] = h->win[i];
            }
        }
        h->nwin = j;
        addwin( g, 0, 1, exc, 9, 0, 21, 3 );
        addwin( g, 0, 9, ( h->exx1 + h->exx2 ) / 2, 20, 0, 22, 1 );
        addwin( g, 0, 7, h->dy + h->exy / 2, 12, 9, 21, 0 ); addwin( g, 0, 8, h->dy + h->exy / 2, 12, 9, 21, 0 );
    }
    // the roof: the eaves, the ridge 8.5 .. 10 m high with a pitch of 27 .. 43 degrees
    h->kn = ri( g, 0, 6 ); h->ez = h->flz[1] + 28 + h->kn;
    double zlo = fmax( 85.0, floor( h->ez + 0.52 * h->dy / 2 ) ), zhi = fmin( 100.0, floor( h->ez + 0.95 * h->dy / 2 ) );
    if ( zhi < zlo ) { zhi = zlo; }
    h->zr = ri( g, (int) zlo, (int) zhi );
    h->chx = ri( g, (int) floor( h->lx * 0.25 ), (int) floor( h->lx * 0.65 ) );
    h->plc = 8 + ri( g, 0, 5 ); h->rfc = 26 + ri( g, 0, 2 ); h->att = ri( g, 0, 1 );
    // the furniture
    for ( int c = 0; c <= 1; ++c ) {
        for ( int i = 0; i < h->nrm[c]; ++i ) {
            fitroom( g, c, i );
        }
    }
}

// -- the outside as a mesh ------------------------------------------------------------------------------------------------------------------------------------
// The same model as the BASIC builders show from the outside (the outer walls in columns of 10 dm that burn and fall by the hash, windows and doors as flat quads in front of the
// walls, the roof in cells, the gables, the chimney, the annex, the dormer, the porch, the bay window, the extension, the hole of the beam), no interior. Every piece has its own
// threshold from the hash (HR of the BASIC builder: HASH(seed * 7919 + 5, a, b)), so a piece that is gone at a stage is gone at the next.

#define UNIT 3.125

typedef struct {
    hmesh_t*       m;
    const house_t* h;
    double         d;                   // the damage 0 .. 1
    int            stage, level;
    bool           ok;
} mb_t;

static double hr( const mb_t* b, int64_t a, int64_t c ) {
    return nz_hash( b->h->seed * 7919 + 5, a, c, 0 );
}

static int mv( mb_t* b, double x, double y, double z ) {
    hmesh_t* m = b->m;
    if ( m->nv >= m->capv ) {
        int nc = m->capv == 0 ? 512 : m->capv * 2;
        hvert_t* nv = (hvert_t*) realloc( m->v, sizeof(hvert_t) * (size_t) nc );
        if ( nv == 0 ) { b->ok = false; return 0; }
        m->v = nv; m->capv = nc;
    }
    hvert_t* p = &m->v[m->nv];
    p->x = x * UNIT; p->y = y * UNIT; p->z = z * UNIT; p->u = 0; p->v = 0;
    return m->nv++;
}

static void mt( mb_t* b, int v0, int v1, int v2, int mat ) {
    hmesh_t* m = b->m;
    if ( !b->ok ) { return; }
    if ( m->nt >= m->capt ) {
        int nc = m->capt == 0 ? 1024 : m->capt * 2;
        htri_t* nt = (htri_t*) realloc( m->t, sizeof(htri_t) * (size_t) nc );
        if ( nt == 0 ) { b->ok = false; return; }
        m->t = nt; m->capt = nc;
    }
    m->t[m->nt].a = v0; m->t[m->nt].b = v1; m->t[m->nt].c = v2; m->t[m->nt].mat = mat;
    ++m->nt;
}

// a quad from 4 points (dm), turned so that it faces the direction (ox, oy, oz)
static void quad( mb_t* b, const double p[4][3], double ox, double oy, double oz, int mat ) {
    double e1[3] = { p[1][0] - p[0][0], p[1][1] - p[0][1], p[1][2] - p[0][2] }, e2[3] = { p[2][0] - p[0][0], p[2][1] - p[0][1], p[2][2] - p[0][2] };
    double nx = e1[1] * e2[2] - e1[2] * e2[1], ny = e1[2] * e2[0] - e1[0] * e2[2], nz = e1[0] * e2[1] - e1[1] * e2[0];
    bool flip = nx * ox + ny * oy + nz * oz < 0;
    int v[4];
    for ( int k = 0; k < 4; ++k ) {
        int s = flip ? ( 4 - k ) & 3 : k;
        v[k] = mv( b, p[s][0], p[s][1], p[s][2] );
    }
    mt( b, v[0], v[1], v[2], mat ); mt( b, v[0], v[2], v[3], mat );
}

static void tri3( mb_t* b, const double p[3][3], double ox, double oy, double oz, int mat ) {
    double e1[3] = { p[1][0] - p[0][0], p[1][1] - p[0][1], p[1][2] - p[0][2] }, e2[3] = { p[2][0] - p[0][0], p[2][1] - p[0][1], p[2][2] - p[0][2] };
    double nx = e1[1] * e2[2] - e1[2] * e2[1], ny = e1[2] * e2[0] - e1[0] * e2[2], nz = e1[0] * e2[1] - e1[1] * e2[0];
    bool flip = nx * ox + ny * oy + nz * oz < 0;
    int v[3];
    for ( int k = 0; k < 3; ++k ) {
        int s = flip ? ( 3 - k ) % 3 : k;
        v[k] = mv( b, p[s][0], p[s][1], p[s][2] );
    }
    mt( b, v[0], v[1], v[2], mat );
}

static const double sdx[4] = { 0, 0, -1, 1 }, sdy[4] = { -1, 1, 0, 0 };         // the outward direction of the sides 0 south, 1 north, 2 west, 3 east

// a vertical rectangle on a wall: the side, the plane's coordinate, the range along the wall, the heights, pushed off the wall by off (dm)
static void wface( mb_t* b, int side, double plane, double a1, double a2, double z1, double z2, int mat, double off ) {
    double pl = plane + off * ( side == 0 || side == 2 ? -1.0 : 1.0 ), p[4][3];
    for ( int k = 0; k < 4; ++k ) {
        double a = k == 0 || k == 3 ? a1 : a2, z = k < 2 ? z1 : z2;
        if ( side < 2 ) { p[k][0] = a; p[k][1] = pl; } else { p[k][0] = pl; p[k][1] = a; }
        p[k][2] = z;
    }
    quad( b, p, sdx[side], sdy[side], 0, mat );
}

// a horizontal rectangle at height z facing up (up = true) or down
static void hface( mb_t* b, double x1, double y1, double x2, double y2, double z, int mat, bool up ) {
    double p[4][3] = { { x1, y1, z }, { x2, y1, z }, { x2, y2, z }, { x1, y2, z } };
    quad( b, p, 0, 0, up ? 1.0 : -1.0, mat );
}

// a box without its bottom (the walls of the small parts): the sides facing outward and the top
static void hbox( mb_t* b, double x1, double y1, double z1, double x2, double y2, double z2, int mat, bool top ) {
    wface( b, 0, y1, x1, x2, z1, z2, mat, 0 ); wface( b, 1, y2, x1, x2, z1, z2, mat, 0 );
    wface( b, 2, x1, y1, y2, z1, z2, mat, 0 ); wface( b, 3, x2, y1, y2, z1, z2, mat, 0 );
    if ( top ) { hface( b, x1, y1, x2, y2, z2, mat, true ); }
}

// -- the walls: columns of 10 dm; the top and the picture of every column from the hash; neighbours of one picture and height become one run
typedef struct { double top; int mat; } col_t;

static void column( const mb_t* b, int pw, int k, double full, double fl1, col_t* c ) {
    double d = b->d, rr = hr( b, pw * 64 + k, 3 ), rr2 = hr( b, pw * 64 + k, 4 ), rr3 = hr( b, pw * 64 + k, 5 ), zt = full;
    if ( d > 0.6 ) {
        double kp = ( 0.97 - d ) * 2.6;
        if ( kp < 0 ) { kp = 0; }
        double fr = kp * ( 0.4 + 0.6 * rr );
        if ( fr > 1 ) { fr = 1; }
        zt = fl1 + ( full - fl1 ) * fr;
    }
    if ( d > 0.9 ) { zt = fmin( zt, fl1 * ( 0.74 + 0.26 * rr ) ); }
    if ( zt > full ) { zt = full; }
    if ( zt < full ) { zt = floor( zt / 6.0 ) * 6.0; }
    c->top = zt;
    c->mat = HM_PLASTER;
    if ( rr2 < d * 1.0 ) { c->mat = HM_CHAR; } else if ( rr3 < d * 0.6 ) { c->mat = HM_BRICK; }
}

// one outer wall (side 0 .. 3) of a box from lo to hi along the wall, to the height full, seen from outside; pw numbers the wall for the hash; the thickness 3.5 dm
static void wall_run( mb_t* b, int side, double plane, double lo, double hi, double full, double fl1, int pw ) {
    double d = b->d;
    int nc = (int) floor( ( hi - lo - 0.001 ) / 10.0 ) + 1;
    if ( nc > 40 ) { nc = 40; }
    col_t c[40];
    int single = d < 0.01 ? 1 : 0;
    for ( int k = 0; k < nc; ++k ) { column( b, pw, k, full, fl1, &c[k] ); }
    if ( single ) {
        c[0].top = full; c[0].mat = HM_PLASTER; nc = 1;
    }
    int k = 0;
    while ( k < nc ) {
        if ( c[k].top <= 3.0 ) { ++k; continue; }
        int j = k;
        while ( j + 1 < nc && c[j + 1].top == c[k].top && c[j + 1].mat == c[k].mat ) { ++j; }
        double a1 = single ? lo : lo + k * 10.0, a2 = single ? hi : fmin( hi, lo + ( j + 1 ) * 10.0 ), zt = c[k].top;
        int mat = c[k].mat;
        wface( b, side, plane, a1, a2, 0, zt, mat, 0 );
        if ( d > 0.3 ) {                                                       // seen from above and from inside once the roof is open
            double p[4][3];
            double t0 = plane, t1 = plane + ( side == 0 || side == 2 ? 3.5 : -3.5 );
            if ( side < 2 ) { double q[4][3] = { { a1, t0, zt }, { a2, t0, zt }, { a2, t1, zt }, { a1, t1, zt } }; memcpy( p, q, sizeof(p) ); }
            else            { double q[4][3] = { { t0, a1, zt }, { t0, a2, zt }, { t1, a2, zt }, { t1, a1, zt } }; memcpy( p, q, sizeof(p) ); }
            quad( b, p, 0, 0, 1, mat );
            wface( b, side ^ 1, plane + ( side == 0 || side == 2 ? 3.5 : -3.5 ), a1, a2, 0, zt, mat, 0 );
        }
        // the ends: only where the neighbour is lower
        if ( k > 0 && c[k - 1].top < zt ) {
            double z0 = c[k - 1].top > 3.0 ? c[k - 1].top : 0;
            double t1 = plane + ( side == 0 || side == 2 ? 3.5 : -3.5 ), p[4][3];
            if ( side < 2 ) { double q[4][3] = { { a1, plane, z0 }, { a1, t1, z0 }, { a1, t1, zt }, { a1, plane, zt } }; memcpy( p, q, sizeof(p) ); }
            else            { double q[4][3] = { { plane, a1, z0 }, { t1, a1, z0 }, { t1, a1, zt }, { plane, a1, zt } }; memcpy( p, q, sizeof(p) ); }
            if ( side < 2 ) { quad( b, p, -1, 0, 0, mat ); } else { quad( b, p, 0, -1, 0, mat ); }
        }
        if ( j + 1 < nc && c[j + 1].top < zt ) {
            double z0 = c[j + 1].top > 3.0 ? c[j + 1].top : 0;
            double t1 = plane + ( side == 0 || side == 2 ? 3.5 : -3.5 ), p[4][3];
            if ( side < 2 ) { double q[4][3] = { { a2, plane, z0 }, { a2, t1, z0 }, { a2, t1, zt }, { a2, plane, zt } }; memcpy( p, q, sizeof(p) ); }
            else            { double q[4][3] = { { plane, a2, z0 }, { t1, a2, z0 }, { t1, a2, zt }, { plane, a2, zt } }; memcpy( p, q, sizeof(p) ); }
            if ( side < 2 ) { quad( b, p, 1, 0, 0, mat ); } else { quad( b, p, 0, 1, 0, mat ); }
        }
        k = j + 1;
    }
}

// the top of the column of the main house at the position pos on the side (for windows that must not hang in the air)
static double wall_top( const mb_t* b, int side, double pos ) {
    if ( b->d < 0.01 ) { return b->h->ez; }
    col_t c;
    int k = (int) floor( pos / 10.0 );
    column( b, 100 + side, k, b->h->ez, b->h->flz[1], &c );
    return c.top;
}

// the windows, doors and openings of the side planes as flat quads in front of the walls
static void openings( mb_t* b ) {
    const house_t* h = b->h;
    double d = b->d;
    for ( int i = 0; i < h->nwin; ++i ) {
        const hwin_t* w = &h->win[i];
        int ws, k = w->kind;
        double plane;
        switch ( w->side ) {
            case 0: ws = 0; plane = 0; break;
            case 1: ws = 1; plane = h->dy; break;
            case 2: ws = 2; plane = 0; break;
            case 3: continue;                                                  // (the east wall of the house is inside the garage or free: the doors there are seen from inside only)
            case 4: ws = 0; plane = 0; break;
            case 5: ws = 3; plane = h->lx + h->ai + 4; break;
            case 6: ws = 1; plane = h->ay + 4; break;
            case 7: ws = 2; plane = h->exx1 - 4; break;
            case 8: ws = 3; plane = h->exx2 + 4; break;
            default: ws = 1; plane = h->dy + h->exy + 4; break;
        }
        if ( w->side == 1 && k == 3 ) { continue; }                           // the door into the extension: inside
        if ( w->side == 1 && h->type == 5 && w->pos + w->w / 2 > h->exx1 - 4 && w->pos - w->w / 2 < h->exx2 + 4 ) { continue; }
        if ( k == 5 ) { continue; }                                            // the bay window has its own front (bay_window)
        double z1 = h->flz[w->floor] + w->sill, z2 = h->flz[w->floor] + w->head;
        if ( w->side < 4 && wall_top( b, w->side, w->pos ) < z2 ) { continue; }
        if ( w->side >= 4 && d > 0.6 && z2 > 26 && w->floor > 0 ) { continue; }
        double r = hr( b, 600, i );
        bool whole = d < 0.3 + 0.5 * r && d < 0.7;
        double a1 = w->pos - w->w / 2, a2 = w->pos + w->w / 2;
        if ( k == 4 ) {
            wface( b, ws, plane, a1, a2, z1, z2, whole ? HM_GARAGE : HM_GLASS, 0.3 );
        } else if ( k == 2 || k == 3 ) {
            wface( b, ws, plane, a1, a2, z1, z2, whole ? HM_DOOR : HM_GLASS, 0.3 );
        } else if ( whole ) {
            if ( b->level > 0 ) { wface( b, ws, plane, a1 - 0.6, a2 + 0.6, z1 - 0.6, z2 + 0.6, HM_FRAME, 0.15 ); }
            wface( b, ws, plane, a1, a2, z1, z2, HM_GLASS, 0.3 );
        } else {
            wface( b, ws, plane, a1, a2, z1, z2, HM_CHAR, 0.3 );               // a dark opening
        }
    }
}

// the round hole of the beam (D >= 0.5) on a side wall at the height of the ground floor: a dark octagon with a glassy rim
static void beam_hole( mb_t* b ) {
    const house_t* h = b->h;
    if ( b->d < 0.5 ) { return; }
    int hs = (int) floor( hr( b, 9, 1 ) * 4 );
    if ( hs > 3 ) { hs = 3; }
    if ( h->type == 2 && hs == 3 ) { hs = 1; }
    if ( h->type == 4 && hs == 0 ) { hs = 1; }
    if ( h->type == 5 ) { hs = 2 + ( hs & 1 ); }
    double hc = hs < 2 ? h->lx * ( 0.3 + 0.4 * hr( b, 9, 2 ) ) : h->dy * ( 0.3 + 0.4 * hr( b, 9, 2 ) );
    double rad = 8 + 2 * hr( b, 9, 3 ), zc = h->flz[0] + 13, plane = hs == 0 ? 0 : hs == 1 ? h->dy : hs == 2 ? 0 : h->lx;
    if ( hs == 3 && h->type == 2 ) { return; }
    for ( int ring = 0; ring < 2; ++ring ) {
        if ( ring == 1 && b->level == 0 ) { break; }
        double r1 = ring == 0 ? 0.0 : rad, r2 = ring == 0 ? rad : rad + 1.4, off = ring == 0 ? 0.35 : 0.3;
        for ( int k = 0; k < 8; ++k ) {
            double a0 = k * 0.7853982, a1 = ( k + 1 ) * 0.7853982;
            double pa[4][3];
            for ( int q = 0; q < 4; ++q ) {
                double rr = q == 0 || q == 3 ? r1 : r2, an = q == 0 || q == 1 ? a0 : a1;
                if ( ring == 0 ) { rr = q == 0 || q == 3 ? 0.0 : rad; }
                double al = hs < 2 ? hc + rr * cos( an ) : hc + rr * cos( an ), zz = zc + rr * sin( an );
                double pl = plane + off * ( hs == 0 || hs == 2 ? -1.0 : 1.0 );
                if ( hs < 2 ) { pa[q][0] = al; pa[q][1] = pl; } else { pa[q][0] = pl; pa[q][1] = al; }
                pa[q][2] = zz;
            }
            if ( ring == 0 ) {
                double tr[3][3] = { { pa[0][0], pa[0][1], pa[0][2] }, { pa[1][0], pa[1][1], pa[1][2] }, { pa[2][0], pa[2][1], pa[2][2] } };
                tri3( b, tr, sdx[hs], sdy[hs], 0, HM_GLASS );
            } else {
                quad( b, pa, sdx[hs], sdy[hs], 0, HM_RIM );
            }
        }
    }
}

// the gable triangles and the roof
static void roof( mb_t* b ) {
    const house_t* h = b->h;
    double d = b->d, ov = 5, sl = ( h->zr - h->ez ) / ( h->dy / 2 ), ze = h->ez - ov * sl;
    if ( h->type != 3 && d <= 0.6 ) {
        for ( int e = 0; e < 2; ++e ) {
            double x = e == 0 ? 0.0 : h->lx;
            int mat = hr( b, 300, 1 + e ) < d * 1.4 ? HM_CHAR : HM_PLASTER;
            double t[3][3] = { { x, 0, h->ez }, { x, h->dy, h->ez }, { x, h->dy / 2, h->zr } };
            tri3( b, t, e == 0 ? -1 : 1, 0, 0, mat );
        }
    }
    if ( d >= 0.7 ) { return; }
    double cw = b->level > 0 ? 10.0 : 30.0;
    int rows = b->level > 0 ? 4 : 2;
    if ( h->type != 3 ) {
        int nx = (int) floor( ( h->lx + 6 ) / cw ) + 1;
        double wx = ( h->lx + 6 ) / nx;
        for ( int s = 0; s < 2; ++s ) {
            for ( int i = 0; i < nx; ++i ) {
                for ( int j = 0; j < rows; ++j ) {
                    int ix = s * 1000 + i * 10 + j;
                    if ( !( d < 0.3 + 0.5 * hr( b, 500, ix ) ) ) { continue; }
                    double x1 = -3 + i * wx, x2 = x1 + wx, t1 = (double) j / rows, t2 = (double) ( j + 1 ) / rows;
                    double y1 = -ov + ( h->dy / 2 + ov ) * t1, y2 = -ov + ( h->dy / 2 + ov ) * t2, z1 = ze + ( h->zr - ze ) * t1, z2 = ze + ( h->zr - ze ) * t2;
                    if ( s == 1 ) { y1 = h->dy - y1; y2 = h->dy - y2; }
                    double p[4][3] = { { x1, y1, z1 }, { x2, y1, z1 }, { x2, y2, z2 }, { x1, y2, z2 } };
                    quad( b, p, 0, s == 0 ? -1.0 : 1.0, 1, hr( b, 501, ix ) < d * 1.5 ? HM_CHAR : HM_ROOF );
                }
            }
        }
    } else {
        // the hip roof: z = eaves + slope * the distance to the nearest wall, the grid of 30 / 10 dm, the creases along the diagonals
        double xs[40], ys[40];
        int nx = 0, ny = 0;
        double step = b->level > 0 ? 10.0 : 30.0;
        xs[nx++] = -ov; xs[nx++] = 0;
        for ( double k = step; k < h->dy / 2 - 1; k += step ) { xs[nx++] = k; }
        xs[nx++] = h->dy / 2;
        int n = (int) floor( ( h->lx - h->dy ) / 12.0 ) + 1;
        for ( int i = 1; i < n; ++i ) { xs[nx++] = h->dy / 2 + ( h->lx - h->dy ) * i / n; }
        xs[nx++] = h->lx - h->dy / 2;
        double kk = step;
        while ( kk < h->dy / 2 - 1 ) { kk += step; }
        kk -= step;
        for ( ; kk >= step - 1e-9; kk -= step ) { xs[nx++] = h->lx - kk; }
        xs[nx++] = h->lx; xs[nx++] = h->lx + ov;
        ys[ny++] = -ov; ys[ny++] = 0;
        for ( double k = step; k < h->dy / 2 - 1; k += step ) { ys[ny++] = k; }
        ys[ny++] = h->dy / 2;
        kk = step;
        while ( kk < h->dy / 2 - 1 ) { kk += step; }
        kk -= step;
        for ( ; kk >= step - 1e-9; kk -= step ) { ys[ny++] = h->dy - kk; }
        ys[ny++] = h->dy; ys[ny++] = h->dy + ov;
        #define HZ(x,y) ( h->ez + sl * fmin( fmin( (x), h->lx - (x) ), fmin( (y), h->dy - (y) ) ) )
        for ( int i = 0; i + 1 < nx; ++i ) {
            for ( int j = 0; j + 1 < ny; ++j ) {
                if ( !( d < 0.3 + 0.5 * hr( b, 500, i * 40 + j ) ) ) { continue; }
                double x1 = xs[i], x2 = xs[i + 1], y1 = ys[j], y2 = ys[j + 1];
                int mat = hr( b, 501, i * 40 + j ) < d * 1.5 ? HM_CHAR : HM_ROOF;
                double ox = ( x1 + x2 ) / 2 - h->lx / 2, oy = ( y1 + y2 ) / 2 - h->dy / 2;
                bool same = ( ( x1 + x2 ) / 2 < h->lx / 2 ) == ( ( y1 + y2 ) / 2 < h->dy / 2 );
                double a[3] = { x1, y1, HZ( x1, y1 ) }, c[3] = { x2, y1, HZ( x2, y1 ) }, e[3] = { x2, y2, HZ( x2, y2 ) }, f[3] = { x1, y2, HZ( x1, y2 ) };
                double sxo = ox > 0 ? 0.5 : -0.5, syo = oy > 0 ? 0.5 : -0.5;
                if ( same ) {
                    double t1[3][3] = { { a[0], a[1], a[2] }, { c[0], c[1], c[2] }, { e[0], e[1], e[2] } }, t2[3][3] = { { a[0], a[1], a[2] }, { e[0], e[1], e[2] }, { f[0], f[1], f[2] } };
                    tri3( b, t1, sxo, syo, 1, mat ); tri3( b, t2, sxo, syo, 1, mat );
                } else {
                    double t1[3][3] = { { a[0], a[1], a[2] }, { c[0], c[1], c[2] }, { f[0], f[1], f[2] } }, t2[3][3] = { { c[0], c[1], c[2] }, { e[0], e[1], e[2] }, { f[0], f[1], f[2] } };
                    tri3( b, t1, sxo, syo, 1, mat ); tri3( b, t2, sxo, syo, 1, mat );
                }
            }
        }
        #undef HZ
    }
}

// the small parts: the chimney on the west wall, the garage, the extension, the bay window, the porch and the dormer
static void features( mb_t* b ) {
    const house_t* h = b->h;
    double d = b->d, fl1 = h->flz[1];
    hbox( b, -6, h->dy / 2 - 4, 0, 0, h->dy / 2 + 4, h->zr + 5, HM_BRICK, true );
    double hw = h->ez;
    if ( b->stage == 3 ) { hw = fl1 + ( h->ez - fl1 ) * 0.35; }
    if ( b->stage == 4 ) { hw = fl1 * 0.82; }
    double ha = fmin( fl1, hw );
    int mat = hr( b, 190, 1 ) < d * 1.0 ? HM_CHAR : HM_PLASTER;
    if ( h->type == 2 ) {
        double x1 = h->lx, x2 = h->lx + h->ai + 4, y2 = h->ay + 4;
        wface( b, 0, 0, x1, x2, 0, ha, mat, 0 ); wface( b, 1, y2, x1, x2, 0, ha, mat, 0 ); wface( b, 3, x2, 0, y2, 0, ha, mat, 0 );
        if ( d < 0.3 + 0.5 * hr( b, 3000, 1 ) && d < 0.7 ) { hface( b, x1, 0, x2, y2, ha, d > 0.3 && hr( b, 3500, 1 ) < d * 1.5 ? HM_CHAR : HM_CONCRETE, true ); }
        else if ( d >= 0.7 ) { hface( b, x1, 0, x2, y2, h->flz[0], HM_CONCRETE, true ); }
    }
    if ( h->type == 5 ) {
        double x1 = h->exx1 - 4, x2 = h->exx2 + 4, y1 = h->dy, y2 = h->dy + h->exy + 4;
        wface( b, 2, x1, y1, y2, 0, ha, mat, 0 ); wface( b, 3, x2, y1, y2, 0, ha, mat, 0 ); wface( b, 1, y2, x1, x2, 0, ha, mat, 0 );
        if ( d < 0.3 + 0.5 * hr( b, 3000, 2 ) && d < 0.7 ) { hface( b, x1, y1, x2, y2, ha, d > 0.3 && hr( b, 3500, 2 ) < d * 1.5 ? HM_CHAR : HM_CONCRETE, true ); }
        else if ( d >= 0.7 ) { hface( b, x1, y1, x2, y2, h->flz[0], HM_CONCRETE, true ); }
        // the bay window: two storeys in front of the west front room, cheeks, a glass front, a flat roof
        double bx1 = h->bx1, bx2 = h->bx2;
        for ( int f = 0; f < 2; ++f ) {
            double z1 = f == 0 ? 0 : fl1, z2 = f == 0 ? fl1 : fl1 + 26;
            if ( f == 1 && d > 0.6 ) { continue; }
            wface( b, 2, bx1 - 1.5, -9, 0, z1, z2, mat, 0 ); wface( b, 3, bx2 + 1.5, -9, 0, z1, z2, mat, 0 );
            wface( b, 0, -9, bx1 - 1.5, bx2 + 1.5, z1, h->flz[f] + 3, mat, 0 );
            wface( b, 0, -9, bx1 - 1.5, bx2 + 1.5, h->flz[f] + 22, z2, mat, 0 );
            bool whole = d < 0.3 + 0.5 * hr( b, 180, f ) && d < 0.7;
            wface( b, 0, -9, bx1, bx2, h->flz[f] + 3, h->flz[f] + 22, whole ? HM_GLASS : HM_CHAR, 0.2 );
        }
        if ( d < 0.5 + 0.3 * hr( b, 181, 0 ) && d < 0.7 ) { hface( b, bx1 - 2.5, -10.5, bx2 + 2.5, 0.5, fl1 + 28.6, hr( b, 182, 0 ) < d * 1.5 ? HM_CHAR : HM_ROOF, true ); }
    }
    if ( h->type == 4 ) {
        double p0 = h->win[0].pos;
        for ( int i = 0; i < h->nwin; ++i ) { if ( h->win[i].floor == 0 && h->win[i].side == 0 && h->win[i].kind == 2 ) { p0 = h->win[i].pos; } }
        hface( b, p0 - 14, -14, p0 + 14, 0.5, h->flz[0] - 0.4, HM_CONCRETE, true );
        for ( int i = 0; i < 2; ++i ) {
            if ( hr( b, 195, i ) > d * 0.5 ) {
                double x = p0 - 13 + i * 24;
                hbox( b, x, -13, h->flz[0] - 0.4, x + 2, -11, h->flz[0] + 22.5, hr( b, 196, i ) < d * 1.4 ? HM_CHAR : HM_PLASTER, false );
            }
        }
        if ( d < 0.5 + 0.3 * hr( b, 197, 1 ) && d < 0.7 ) { hface( b, p0 - 15, -15, p0 + 15, 1, h->flz[0] + 24.3, hr( b, 198, 1 ) < d * 1.5 ? HM_CHAR : HM_ROOF, true ); }
        if ( d <= 0.6 ) {                                                       // the dormer on the south slope
            double sl = ( h->zr - h->ez ) / ( h->dy / 2 ), dx = h->lx * ( 0.4 + 0.2 * hr( b, 190, 1 ) ), dw = 18, yf = 9;
            double zr0 = h->ez + sl * yf, zt = fmin( zr0 + 14, h->zr - 6 ), yb = ( zt - h->ez ) / sl;
            int dm = hr( b, 191, 1 ) < d * 1.4 ? HM_CHAR : HM_PLASTER;
            wface( b, 0, yf, dx - dw / 2, dx + dw / 2, zr0 - 1, zt, dm, 0 );
            wface( b, 2, dx - dw / 2, yf, yb + 1, zr0 - 1, zt, dm, 0 ); wface( b, 3, dx + dw / 2, yf, yb + 1, zr0 - 1, zt, dm, 0 );
            wface( b, 0, yf, dx - dw / 2 + 2, dx + dw / 2 - 2, zr0 + 3, zt - 3, ( d < 0.3 + 0.5 * hr( b, 194, 1 ) ) ? HM_GLASS : HM_CHAR, 0.3 );
            if ( d < 0.5 + 0.3 * hr( b, 193, 1 ) ) { hface( b, dx - dw / 2 - 1.5, yf - 1.5, dx + dw / 2 + 1.5, yb + 1.5, zt + 1.2, hr( b, 192, 1 ) < d * 1.5 ? HM_CHAR : HM_ROOF, true ); }
        }
    }
    // an open top: the floor of the ground floor shows when the ceilings are gone
    if ( d >= 0.5 ) { hface( b, 3.5, 3.5, h->lx - 3.5, h->dy - 3.5, h->flz[0], HM_CONCRETE, true ); }
}

bool house_mesh( const house_t* h, int stage, int level, hmesh_t* m ) {
    static const double dmg[5] = { 0, 0.25, 0.5, 0.75, 1.0 };
    memset( m, 0, sizeof(*m) );
    if ( stage < 0 ) { stage = 0; }
    if ( stage > 4 ) { stage = 4; }
    mb_t b = { m, h, dmg[stage], stage, level > 0 ? 1 : 0, true };
    {
        // the colours of the plaster and of the roof from the seed, as in tools/house3d.bas
        m->plaster = (int) floor( hr( &b, 40, 1 ) * 6 );
        if ( m->plaster > 5 ) { m->plaster = 5; }
        m->roofc = (int) floor( hr( &b, 40, 2 ) * 3 );
        if ( m->roofc > 2 ) { m->roofc = 2; }
    }
    double lx = h->lx, dy = h->dy, ez = h->ez, fl1 = h->flz[1];
    wall_run( &b, 0, 0, 0, lx, ez, fl1, 100 );
    wall_run( &b, 1, dy, 0, lx, ez, fl1, 101 );
    wall_run( &b, 2, 0, 0, dy, ez, fl1, 102 );
    wall_run( &b, 3, lx, 0, dy, ez, fl1, 103 );
    openings( &b );
    beam_hole( &b );
    roof( &b );
    features( &b );
    m->w = lx * UNIT; m->d = dy * UNIT; m->h = ( h->zr + 5 ) * UNIT;
    if ( !b.ok ) {
        house_mesh_free( m );
        return false;
    }
    return true;
}

void house_mesh_free( hmesh_t* m ) {
    free( m->v ); free( m->t );
    memset( m, 0, sizeof(*m) );
}
