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

#include "m3dpriv.h"
#include "gfxops.h"

/*
The software renderer of the mesh module: every triangle is rasterized into a palette bitmap with a depth buffer. Its state (the color map of
16 -> 32 light levels, the translucency table, the bitmap and the depth buffer) belongs to it, not to the model.
*/

typedef struct {
    uint8_t  cmap[M3D_LEVELS][256];
    uint8_t* blend;
    uint8_t* fb;
    float*   zb;
} soft_t;

static void make_cmap( m3d_t* m, soft_t* s ) {
    const uint32_t* argb = m->pal;
    free( s->blend );
    s->blend = 0;
    for ( int l=0; l < M3D_LEVELS; ++l ) {
        double scale = (double) l / (double)( M3D_LEVELS - 1 );
        s->cmap[l][0] = 0;
        for ( int i=1; i < 256; ++i ) {
            if ( l == M3D_LEVELS - 1 ) {
                s->cmap[l][i] = (uint8_t) i;
                continue;
            }
            double tr = (double)( ( argb[i] >> 16 ) & 0xFFU ) * scale;
            double tg = (double)( ( argb[i] >> 8 ) & 0xFFU ) * scale;
            double tb = (double)( argb[i] & 0xFFU ) * scale;
            double best = 1.0e30;
            int bi = i;
            for ( int j=1; j < 256; ++j ) {
                double dr = (double)( ( argb[j] >> 16 ) & 0xFFU ) - tr;
                double dg = (double)( ( argb[j] >> 8 ) & 0xFFU ) - tg;
                double db = (double)( argb[j] & 0xFFU ) - tb;
                double cr = ( (double)( ( argb[j] >> 16 ) & 0xFFU ) - (double)( ( argb[j] >> 8 ) & 0xFFU ) ) - ( tr - tg );
                double cb = ( (double)( argb[j] & 0xFFU ) - (double)( ( argb[j] >> 8 ) & 0xFFU ) ) - ( tb - tg );
                double d = 2.0 * dr * dr + 4.0 * dg * dg + 3.0 * db * db + 6.0 * cr * cr + 6.0 * cb * cb;
                if ( d < best ) {
                    best = d;
                    bi = j;
                }
            }
            s->cmap[l][i] = (uint8_t) bi;
        }
    }
}

static void make_blend( const m3d_t* m, soft_t* s ) {
    if ( s->blend != 0 || !m->havepal ) {
        return;
    }
    uint8_t* bl = (uint8_t*) malloc( 65536U );
    if ( bl == 0 ) {
        return;
    }
    for ( int a=0; a < 256; ++a ) {
        for ( int b=a; b < 256; ++b ) {
            int r = (int)( ( ( m->pal[a] >> 16 ) & 0xFFU ) + ( ( m->pal[b] >> 16 ) & 0xFFU ) ) / 2;
            int g = (int)( ( ( m->pal[a] >> 8 ) & 0xFFU ) + ( ( m->pal[b] >> 8 ) & 0xFFU ) ) / 2;
            int bb = (int)( ( m->pal[a] & 0xFFU ) + ( m->pal[b] & 0xFFU ) ) / 2;
            int best = 0, bd = 1 << 30;
            for ( int j=1; j < 256; ++j ) {
                int dr = (int)( ( m->pal[j] >> 16 ) & 0xFFU ) - r, dg = (int)( ( m->pal[j] >> 8 ) & 0xFFU ) - g, db = (int)( m->pal[j] & 0xFFU ) - bb;
                int d = 2 * dr * dr + 4 * dg * dg + 3 * db * db;
                if ( d < bd ) {
                    bd = d;
                    best = j;
                }
            }
            bl[ a * 256 + b ] = bl[ b * 256 + a ] = (uint8_t) best;
        }
    }
    s->blend = bl;
}

// -- rendering ------------------------------------------------------------------------------------------------------------------

typedef struct { double x, y, z, u, v; } cv_t;      // view space: x right, y up, z forward

typedef struct {
    m3d_t* m;
    soft_t* s;
    uint8_t* fb;
    float* zb;
    int W, H;
    double f, cx, cy;
    v3_t eye, r, up, fw;
    double horizon;
    int nlamps;
} rc_t;

static void sort_desc( int* idx, const double* key, int n ) {
    for ( int gap = n / 2; gap > 0; gap /= 2 ) {
        for ( int i = gap; i < n; ++i ) {
            int t = idx[i];
            int j = i;
            while ( j >= gap && key[idx[j-gap]] < key[t] ) {
                idx[j] = idx[j-gap];
                j -= gap;
            }
            idx[j] = t;
        }
    }
}

static void draw_sky_bg( rc_t* c ) {
    m3d_t* m = c->m;
    const tex_t* tx = tex_get( m, m->skytex );
    const uint8_t* cm = c->s->cmap[M3D_LEVELS - 1];
    if ( tx == 0 ) {
        memset( c->fb, m->skytex >= 0 ? cm[ (uint8_t) m->skytex ] : 0, (size_t) c->W * (size_t) c->H );
        return;
    }
    for ( int x=0; x < c->W; ++x ) {
        double ang = ( m->yaw - atan( ( x + 0.5 - c->cx ) / c->f ) / DEG2RAD ) / 360.0;
        ang -= floor( ang );
        int tu = (int) fmod( ang * tx->w * m->skyrep, (double) tx->w );
        const uint8_t* colp = tx->col + (size_t) tu * (size_t) tx->h;
        for ( int y=0; y < c->H; ++y ) {
            int row = tx->h - 1 - iclamp( (int)( c->horizon - ( y + 0.5 ) + m->skyshift ), 0, tx->h - 1 );
            c->fb[ (size_t) y * (size_t) c->W + (size_t) x ] = cm[ colp[row] ];
        }
    }
}

typedef struct {
    double sx[3], sy[3];
    double A0[7], gx[7], gy[7];             // iz, u/z, v/z, wx/z, wy/z, wz/z
    int na;
    const tex_t* tx;
    int tex, flags, light;
    bool lamps;
} tri_t;

static void raster( rc_t* c, const tri_t* t, bool translucent ) {
    m3d_t* m = c->m;
    int a = 0, b = 1, d = 2;
    // top to bottom
    if ( t->sy[a] > t->sy[b] ) { int x = a; a = b; b = x; }
    if ( t->sy[b] > t->sy[d] ) { int x = b; b = d; d = x; }
    if ( t->sy[a] > t->sy[b] ) { int x = a; a = b; b = x; }
    double y0 = t->sy[a], y1 = t->sy[b], y2 = t->sy[d];
    int ys = (int) ceil( y0 - 0.5 ), ye = (int) ceil( y2 - 0.5 );
    if ( ys < 0 ) { ys = 0; }
    if ( ye > c->H ) { ye = c->H; }
    if ( ys >= ye || y2 - y0 < 1.0e-9 ) {
        return;
    }
    const uint8_t* cm;
    double base = (double) t->light * ( (double)( M3D_LEVELS - 1 ) / 255.0 );
    double fogd = m->fog;
    bool masked = ( t->flags & M3D_OF_MASKED ) != 0 || translucent;
    for ( int y = ys; y < ye; ++y ) {
        double yc = y + 0.5;
        double xa = t->sx[a] + ( yc - y0 ) * ( t->sx[d] - t->sx[a] ) / ( y2 - y0 );
        double xb;
        if ( yc < y1 ) {
            xb = y1 - y0 > 1.0e-9 ? t->sx[a] + ( yc - y0 ) * ( t->sx[b] - t->sx[a] ) / ( y1 - y0 ) : t->sx[b];
        } else {
            xb = y2 - y1 > 1.0e-9 ? t->sx[b] + ( yc - y1 ) * ( t->sx[d] - t->sx[b] ) / ( y2 - y1 ) : t->sx[b];
        }
        double xl = xa < xb ? xa : xb, xr = xa < xb ? xb : xa;
        int xs = (int) ceil( xl - 0.5 ), xe = (int) ceil( xr - 0.5 );
        if ( xs < 0 ) { xs = 0; }
        if ( xe > c->W ) { xe = c->W; }
        if ( xs >= xe ) {
            continue;
        }
        double A[7];
        for ( int k=0; k < t->na; ++k ) {
            A[k] = t->A0[k] + t->gx[k] * ( xs + 0.5 ) + t->gy[k] * yc;
        }
        uint8_t* fb = c->fb + (size_t) y * (size_t) c->W;
        float* zb = c->zb + (size_t) y * (size_t) c->W;
        for ( int x = xs; x < xe; ++x ) {
            double iz = A[0];
            if ( iz > 1.0e-12 ) {
                double z = 1.0 / iz;
                if ( z < zb[x] && ( m->far <= 0 || z <= m->far ) ) {
                    uint8_t col;
                    if ( t->tx != 0 ) {
                        int tu = imod( (int) floor( A[1] * z ), t->tx->w ), tv = imod( (int) floor( A[2] * z ), t->tx->h );
                        col = t->tx->col[ (size_t) tu * (size_t) t->tx->h + (size_t) tv ];
                    } else {
                        col = (uint8_t) t->tex;
                    }
                    if ( !( masked && col == 0 ) ) {
                        double lv = base;
                        if ( t->lamps ) {
                            double wx = A[3] * z, wy = A[4] * z, wz = A[5] * z, add = 0;
                            for ( int k=0; k < M3D_MAXLAMPS; ++k ) {
                                const lamp_t* l = &m->lamps[k];
                                if ( l->r > 0 && l->amount > 0 ) {
                                    double dx = wx - l->x, dy = wy - l->y, dz = wz - l->z, dd = sqrt( dx * dx + dy * dy + dz * dz );
                                    if ( dd < l->r ) {
                                        add += l->amount * ( 1.0 - dd / l->r );
                                    }
                                }
                            }
                            lv += add * ( (double)( M3D_LEVELS - 1 ) / 255.0 );
                        }
                        if ( fogd > 0 ) {
                            double ff = 1.0 - z / fogd;
                            lv *= ff < 0 ? 0 : ff;
                        }
                        int li = iclamp( (int)( lv + 0.5 ), 0, M3D_LEVELS - 1 );
                        cm = c->s->cmap[li];
                        uint8_t out = cm[col];
                        if ( translucent ) {
                            out = c->s->blend != 0 ? c->s->blend[ (size_t) fb[x] * 256U + out ] : out;
                        } else {
                            zb[x] = (float) z;
                        }
                        fb[x] = out;
                    }
                }
            }
            for ( int k=0; k < t->na; ++k ) {
                A[k] += t->gx[k];
            }
        }
    }
}

// one clipped polygon (view space) -> triangles
static void draw_poly( rc_t* c, const cv_t* p, int n, const v3_t* wp, int tex, int flags, int light, bool translucent, bool lamps ) {
    // wp: the world position of the polygon's vertices (parallel to p; only used when lamps)
    tri_t t;
    t.tx = tex_get( c->m, tex ); t.tex = tex; t.flags = flags; t.light = light; t.lamps = lamps; t.na = lamps ? 6 : 3;
    double sx[8], sy[8], iz[8];
    for ( int i=0; i < n; ++i ) {
        iz[i] = 1.0 / p[i].z;
        sx[i] = c->cx + c->f * p[i].x * iz[i];
        sy[i] = c->cy - c->f * p[i].y * iz[i];
    }
    for ( int i=1; i+1 < n; ++i ) {
        int k[3] = { 0, i, i + 1 };
        double x0 = sx[k[0]], y0 = sy[k[0]];
        double D = ( sx[k[1]] - x0 ) * ( sy[k[2]] - y0 ) - ( sx[k[2]] - x0 ) * ( sy[k[1]] - y0 );
        if ( fabs( D ) < 1.0e-9 ) {
            continue;
        }
        double at[3][6];
        for ( int q=0; q < 3; ++q ) {
            int j = k[q];
            at[q][0] = iz[j]; at[q][1] = p[j].u * iz[j]; at[q][2] = p[j].v * iz[j];
            if ( lamps ) {
                at[q][3] = wp[j].x * iz[j]; at[q][4] = wp[j].y * iz[j]; at[q][5] = wp[j].z * iz[j];
            }
            t.sx[q] = sx[j]; t.sy[q] = sy[j];
        }
        for ( int q=0; q < t.na; ++q ) {
            t.gx[q] = ( ( at[1][q] - at[0][q] ) * ( sy[k[2]] - y0 ) - ( at[2][q] - at[0][q] ) * ( sy[k[1]] - y0 ) ) / D;
            t.gy[q] = ( ( at[2][q] - at[0][q] ) * ( sx[k[1]] - x0 ) - ( at[1][q] - at[0][q] ) * ( sx[k[2]] - x0 ) ) / D;
            t.A0[q] = at[0][q] - t.gx[q] * x0 - t.gy[q] * y0;
        }
        raster( c, &t, translucent );
        ++c->m->drawn;
    }
}

static bool lamp_near( const m3d_t* m, v3_t mn, v3_t mx ) {
    for ( int k=0; k < M3D_MAXLAMPS; ++k ) {
        const lamp_t* l = &m->lamps[k];
        if ( l->r > 0 && l->amount > 0 && l->x + l->r >= mn.x && l->x - l->r <= mx.x && l->y + l->r >= mn.y && l->y - l->r <= mx.y && l->z + l->r >= mn.z && l->z - l->r <= mx.z ) {
            return true;
        }
    }
    return false;
}

static void draw_object( rc_t* c, obj_t* o, bool translucent ) {
    m3d_t* m = c->m;
    cv_t* vv = (cv_t*) malloc( sizeof(cv_t) * (size_t) o->nv );
    if ( vv == 0 ) {
        return;
    }
    for ( int i=0; i < o->nv; ++i ) {
        v3_t d = vsub( o->wv[i], c->eye );
        vv[i].x = vdot( d, c->r ); vv[i].y = vdot( d, c->up ); vv[i].z = vdot( d, c->fw );
    }
    bool twosided = ( o->flags & M3D_OF_TWOSIDED ) != 0;
    for ( int j=0; j < o->nt; ++j ) {
        const mt_t* t = &o->t[j];
        v3_t wa = o->wv[t->a], wb = o->wv[t->b], wc = o->wv[t->c];
        v3_t nrm = vcross( vsub( wb, wa ), vsub( wc, wa ) );
        if ( !twosided && vdot( nrm, vsub( c->eye, wa ) ) <= 0 ) {
            continue;
        }
        cv_t in[3] = { vv[t->a], vv[t->b], vv[t->c] };
        v3_t win[3] = { wa, wb, wc };
        for ( int k=0; k < 3; ++k ) { in[k].u = t->u[k]; in[k].v = t->v[k]; }
        if ( in[0].z < NEARZ && in[1].z < NEARZ && in[2].z < NEARZ ) {
            continue;
        }
        // clip against the near plane
        cv_t out[4];
        v3_t wout[4];
        int n = 0;
        for ( int k=0; k < 3; ++k ) {
            int l = ( k + 1 ) % 3;
            bool ink = in[k].z >= NEARZ, inl = in[l].z >= NEARZ;
            if ( ink ) {
                out[n] = in[k]; wout[n] = win[k]; ++n;
            }
            if ( ink != inl ) {
                double s = ( NEARZ - in[k].z ) / ( in[l].z - in[k].z );
                out[n].x = in[k].x + ( in[l].x - in[k].x ) * s;
                out[n].y = in[k].y + ( in[l].y - in[k].y ) * s;
                out[n].z = NEARZ;
                out[n].u = in[k].u + ( in[l].u - in[k].u ) * s;
                out[n].v = in[k].v + ( in[l].v - in[k].v ) * s;
                wout[n] = vadd( win[k], vmul( vsub( win[l], win[k] ), s ) );
                ++n;
            }
        }
        if ( n < 3 ) {
            continue;
        }
        // off the screen on one side?
        bool offl = true, offr = true, offt = true, offb = true;
        for ( int k=0; k < n; ++k ) {
            double z = out[k].z;
            if ( out[k].x * c->f > -c->cx * z ) { offl = false; }
            if ( out[k].x * c->f < ( c->W - c->cx ) * z ) { offr = false; }
            if ( out[k].y * c->f >= ( c->cy - c->H ) * z ) { offb = false; }
            if ( out[k].y * c->f <= c->cy * z ) { offt = false; }
        }
        if ( offl || offr || offt || offb ) {
            continue;
        }
        bool lamps = false;
        if ( c->nlamps > 0 ) {
            v3_t mn = { fmin( wa.x, fmin( wb.x, wc.x ) ), fmin( wa.y, fmin( wb.y, wc.y ) ), fmin( wa.z, fmin( wb.z, wc.z ) ) };
            v3_t mx = { fmax( wa.x, fmax( wb.x, wc.x ) ), fmax( wa.y, fmax( wb.y, wc.y ) ), fmax( wa.z, fmax( wb.z, wc.z ) ) };
            lamps = lamp_near( m, mn, mx );
        }
        draw_poly( c, out, n, wout, t->tex, o->flags, o->light, translucent, lamps );
    }
    free( vv );
}

static bool object_visible( const rc_t* c, const obj_t* o, double* dist ) {
    v3_t d = { o->cx - c->eye.x, o->cy - c->eye.y, o->cz - c->eye.z };
    double zv = vdot( d, c->fw ), xv = vdot( d, c->r ), yv = vdot( d, c->up );
    *dist = zv;
    if ( zv + o->rad < NEARZ ) {
        return false;
    }
    if ( c->m->far > 0 && zv - o->rad > c->m->far ) {
        return false;
    }
    double zz = zv + o->rad;
    if ( fabs( xv ) - o->rad > zz * ( c->cx / c->f ) + 0 && zz > 0 && fabs( xv ) > zz * ( c->cx / c->f ) + o->rad * 1.5 ) {
        return false;
    }
    if ( zz > 0 && fabs( yv ) > zz * ( c->cy / c->f ) + o->rad * 1.5 ) {
        return false;
    }
    return true;
}

static void draw_things( rc_t* c ) {
    m3d_t* m = c->m;
    int n = 0;
    for ( int i=0; i < M3D_MAXTHINGS; ++i ) {
        const thing_t* t = &m->things[i];
        if ( !t->on ) {
            continue;
        }
        v3_t d = { t->x - c->eye.x, t->y - c->eye.y, t->z + t->h * 0.5 - c->eye.z };
        double zv = vdot( d, c->fw );
        if ( zv < NEARZ ) {
            continue;
        }
        m->order[n] = i; m->odist[i] = zv; ++n;
    }
    sort_desc( m->order, m->odist, n );
    for ( int k=0; k < n; ++k ) {
        const thing_t* t = &m->things[m->order[k]];
        double zv = m->odist[m->order[k]];
        v3_t pb = { t->x - c->eye.x, t->y - c->eye.y, t->z - c->eye.z }, pt = { pb.x, pb.y, pb.z + t->h };
        double xv = vdot( pb, c->r );
        double ybot = vdot( pb, c->up ), ytop = vdot( pt, c->up );
        double sxc = c->cx + c->f * xv / zv, hw = t->w * 0.5 * c->f / zv;
        double sy0 = c->cy - c->f * ytop / zv, sy1 = c->cy - c->f * ybot / zv;
        const tex_t* tx = tex_get( m, t->tex );
        int xs = (int) ceil( sxc - hw - 0.5 ), xe = (int) ceil( sxc + hw - 0.5 ), ys = (int) ceil( sy0 - 0.5 ), ye = (int) ceil( sy1 - 0.5 );
        if ( hw <= 0 || sy1 - sy0 <= 0 ) {
            continue;
        }
        double lv = (double) t->light * ( (double)( M3D_LEVELS - 1 ) / 255.0 );
        if ( m->fog > 0 ) {
            double ff = 1.0 - zv / m->fog;
            lv *= ff < 0 ? 0 : ff;
        }
        const uint8_t* cm = c->s->cmap[ iclamp( (int)( lv + 0.5 ), 0, M3D_LEVELS - 1 ) ];
        if ( t->trans ) { make_blend( m, c->s ); }
        for ( int x = xs < 0 ? 0 : xs; x < xe && x < c->W; ++x ) {
            double u = ( x + 0.5 - ( sxc - hw ) ) / ( 2.0 * hw );
            for ( int y = ys < 0 ? 0 : ys; y < ye && y < c->H; ++y ) {
                if ( zv >= c->zb[ (size_t) y * (size_t) c->W + (size_t) x ] ) {
                    continue;
                }
                uint8_t col;
                if ( tx != 0 ) {
                    int tu = iclamp( (int)( u * tx->w ), 0, tx->w - 1 ), tv = iclamp( (int)( ( y + 0.5 - sy0 ) / ( sy1 - sy0 ) * tx->h ), 0, tx->h - 1 );
                    col = tx->col[ (size_t) tu * (size_t) tx->h + (size_t) tv ];
                } else {
                    col = (uint8_t) t->tex;
                }
                if ( col == 0 ) {
                    continue;
                }
                uint8_t out = cm[col];
                uint8_t* p = &c->fb[ (size_t) y * (size_t) c->W + (size_t) x ];
                if ( t->trans && c->s->blend != 0 ) {
                    out = c->s->blend[ (size_t) *p * 256U + out ];
                }
                *p = out;
            }
        }
    }
}

static const uint8_t* soft_render( m3d_t* m, void* rd ) {
    soft_t* s = (soft_t*) rd;
    if ( s->fb == 0 ) {
        return 0;
    }
    rc_t c;
    memset( &c, 0, sizeof(c) );
    c.m = m; c.s = s; c.fb = s->fb; c.zb = s->zb; c.W = m->bw; c.H = m->bh;
    c.f = ( m->bw * 0.5 ) / tan( m->fov * 0.5 * DEG2RAD );
    c.cx = m->bw * 0.5; c.cy = m->bh * 0.5;
    double yr = m->yaw * DEG2RAD, pr = m->pitch * DEG2RAD;
    c.eye.x = m->x; c.eye.y = m->y; c.eye.z = eye_z( m );
    c.fw.x = cos( yr ) * cos( pr ); c.fw.y = sin( yr ) * cos( pr ); c.fw.z = sin( pr );
    c.r.x = sin( yr ); c.r.y = -cos( yr ); c.r.z = 0;
    c.up.x = -sin( pr ) * cos( yr ); c.up.y = -sin( pr ) * sin( yr ); c.up.z = cos( pr );
    c.horizon = m->bh * 0.5 + tan( pr ) * c.f;
    for ( int k=0; k < M3D_MAXLAMPS; ++k ) {
        if ( m->lamps[k].r > 0 && m->lamps[k].amount > 0 ) { ++c.nlamps; }
    }
    m->drawn = 0;
    draw_sky_bg( &c );
    size_t np = (size_t) m->bw * (size_t) m->bh;
    for ( size_t i=0; i < np; ++i ) {
        s->zb[i] = 1.0e30f;
    }
    int nt = 0;
    for ( int i=0; i < m->top; ++i ) {
        obj_t* o = &m->objs[i];
        if ( !o->used || !o->shown || o->nt == 0 ) {
            continue;
        }
        m3d_obj_update( o );
        double dist;
        if ( o->dirty || !object_visible( &c, o, &dist ) ) {
            continue;
        }
        if ( ( o->flags & M3D_OF_TRANS ) != 0 ) {
            m->order[nt] = i; m->odist[i] = dist; ++nt;
            continue;
        }
        draw_object( &c, o, false );
    }
    draw_things( &c );
    if ( nt > 0 ) {
        make_blend( m, s );
        sort_desc( m->order, m->odist, nt );
        for ( int k=0; k < nt; ++k ) {
            draw_object( &c, &m->objs[m->order[k]], true );
        }
    }
    return s->fb;
}

// -- the renderer interface -----------------------------------------------------------------------------------------------------

static void* soft_create( m3d_t* m ) {
    (void) m;
    return calloc( 1, sizeof(soft_t) );
}

static void soft_destroy( m3d_t* m, void* rd ) {
    (void) m;
    soft_t* s = (soft_t*) rd;
    if ( s != 0 ) {
        free( s->fb ); free( s->zb ); free( s->blend );
        free( s );
    }
}

static bool soft_resize( m3d_t* m, void* rd, int bw, int bh ) {
    (void) m;
    soft_t* s = (soft_t*) rd;
    uint8_t* fb = (uint8_t*) calloc( (size_t) bw * (size_t) bh, 1 );
    float* zb = (float*) calloc( (size_t) bw * (size_t) bh, sizeof(float) );
    if ( fb == 0 || zb == 0 ) {
        free( fb ); free( zb );
        return false;
    }
    free( s->fb ); free( s->zb );
    s->fb = fb; s->zb = zb;
    return true;
}

static void soft_palette( m3d_t* m, void* rd ) {
    make_cmap( m, (soft_t*) rd );
}

const m3drend_t m3d_soft_renderer = { "software", soft_create, soft_destroy, soft_resize, soft_palette, soft_render };
