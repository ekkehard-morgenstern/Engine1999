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
#include "vecext.h"
#if E99_VEC && defined(__SSE__)
#include <xmmintrin.h>
#endif

E99_NO_CONTRACT
E99_NO_PSABI
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("no-math-errno")      // (sqrtf then is one instruction; no errno for a negative argument is needed)
#endif

/*
The software renderer of the mesh module: every triangle is rasterized into a palette bitmap with a depth buffer. Its state (the color map of
16 -> 32 light levels, the translucency table, the bitmap and the depth buffer) belongs to it, not to the model.
*/

typedef struct {
    uint8_t  cmap[M3D_LEVELS][256];
    uint8_t  haze[16][256];     // the fog of a haze color: haze[k][i] = the palette color nearest to color i mixed with the haze color by k / 15 (made when first needed)
    int      haze_for;          // the palette index of the haze color the table was made for (-1: none)
    uint8_t* blend;
    uint8_t* ablend[32];        // translucency tables for palette entries with an alpha of their own (index = alpha / 8, made when first needed)
    uint8_t* fb;
    float*   zb;
    float*   tz;                // occlusion: per tile of OCC_T x OCC_T pixels the farthest depth in the depth buffer (1e30: some pixel is not covered) ...
    uint8_t* td;                // ... valid when the tile is not dirty (something was drawn in it since)
    int      ntx, nty;
} soft_t;

static void make_haze( const m3d_t* m, soft_t* s ) {
    const uint32_t* argb = m->pal;
    double hr = (double)( ( argb[m->hazecol] >> 16 ) & 0xFFU ), hg = (double)( ( argb[m->hazecol] >> 8 ) & 0xFFU ), hb = (double)( argb[m->hazecol] & 0xFFU );
    for ( int k=0; k < 16; ++k ) {
        double t = (double) k / 15.0 * M3D_HAZEMAX;
        s->haze[k][0] = 0;
        for ( int i=1; i < 256; ++i ) {
            s->haze[k][i] = k == 0 ? (uint8_t) i : (uint8_t) m3d_nearest_rgb( argb, ( 1.0 - t ) * (double)( ( argb[i] >> 16 ) & 0xFFU ) + t * hr,
                ( 1.0 - t ) * (double)( ( argb[i] >> 8 ) & 0xFFU ) + t * hg, ( 1.0 - t ) * (double)( argb[i] & 0xFFU ) + t * hb );
        }
    }
    s->haze_for = m->hazecol;
}

static void make_cmap( m3d_t* m, soft_t* s ) {
    const uint32_t* argb = m->pal;
    s->haze_for = -1;
    free( s->blend );
    s->blend = 0;
    for ( int q=0; q < 32; ++q ) {
        free( s->ablend[q] );
        s->ablend[q] = 0;
    }
    for ( int l=0; l < M3D_LEVELS; ++l ) {
        double scale = (double) l / (double)( M3D_LEVELS - 1 );
        s->cmap[l][0] = 0;
        for ( int i=1; i < 256; ++i ) {
            if ( l == M3D_LEVELS - 1 ) {
                s->cmap[l][i] = (uint8_t) i;
                continue;
            }
            int bi = m3d_shade_index( argb, i, scale );
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

// the table for a texel color whose palette entry has the alpha q * 8 (1 .. 31): table[behind * 256 + front] = the palette entry nearest to
// behind * (1 - w) + front * w, w = q / 32; made when a translucent object with such a color is drawn, thrown away with a palette change
static const uint8_t* alpha_blend( const m3d_t* m, soft_t* s, int q ) {
    if ( s->ablend[q] != 0 ) {
        return s->ablend[q];
    }
    uint8_t* bl = (uint8_t*) malloc( 65536U );
    if ( bl == 0 ) {
        return 0;
    }
    double w = (double) q / 32.0;
    for ( int a=0; a < 256; ++a ) {
        for ( int b=0; b < 256; ++b ) {
            double r = ( 1.0 - w ) * (double)( ( m->pal[a] >> 16 ) & 0xFFU ) + w * (double)( ( m->pal[b] >> 16 ) & 0xFFU );
            double g = ( 1.0 - w ) * (double)( ( m->pal[a] >> 8 ) & 0xFFU ) + w * (double)( ( m->pal[b] >> 8 ) & 0xFFU );
            double bb = ( 1.0 - w ) * (double)( m->pal[a] & 0xFFU ) + w * (double)( m->pal[b] & 0xFFU );
            int best = 0;
            double bd = 1.0e30;
            for ( int j=1; j < 256; ++j ) {
                double dr = (double)( ( m->pal[j] >> 16 ) & 0xFFU ) - r, dg = (double)( ( m->pal[j] >> 8 ) & 0xFFU ) - g, db = (double)( m->pal[j] & 0xFFU ) - bb;
                double d = 2.0 * dr * dr + 4.0 * dg * dg + 3.0 * db * db;
                if ( d < bd ) {
                    bd = d;
                    best = j;
                }
            }
            bl[ a * 256 + b ] = (uint8_t) best;
        }
    }
    s->ablend[q] = bl;
    return bl;
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

#define OCC_T 8

// the screen rectangle of an object's bounding box (the 8 corners of its world box) and the smallest depth in view space; false when a corner is not in front of the near plane
static bool obj_rect( const rc_t* c, const obj_t* o, int* x0, int* y0, int* x1, int* y1, double* zmin ) {
    double sx0 = 1.0e30, sx1 = -1.0e30, sy0 = 1.0e30, sy1 = -1.0e30, zm = 1.0e30;
    for ( int k=0; k < 8; ++k ) {
        v3_t d = { ( k & 1 ? o->maxx : o->minx ) - c->eye.x, ( k & 2 ? o->maxy : o->miny ) - c->eye.y, ( k & 4 ? o->maxz : o->minz ) - c->eye.z };
        double zv = vdot( d, c->fw );
        if ( zv <= NEARZ ) {
            return false;
        }
        double sx = c->cx + c->f * vdot( d, c->r ) / zv, sy = c->cy - c->f * vdot( d, c->up ) / zv;
        if ( sx < sx0 ) { sx0 = sx; }
        if ( sx > sx1 ) { sx1 = sx; }
        if ( sy < sy0 ) { sy0 = sy; }
        if ( sy > sy1 ) { sy1 = sy; }
        if ( zv < zm ) { zm = zv; }
    }
    *x0 = iclamp( (int) floor( sx0 ), 0, c->W - 1 ); *x1 = iclamp( (int) floor( sx1 ), 0, c->W - 1 );
    *y0 = iclamp( (int) floor( sy0 ), 0, c->H - 1 ); *y1 = iclamp( (int) floor( sy1 ), 0, c->H - 1 );
    *zmin = zm;
    return true;
}

static void tile_refresh( rc_t* c, int tx, int ty ) {
    float mx = 0;
    int xe = ( tx + 1 ) * OCC_T < c->W ? ( tx + 1 ) * OCC_T : c->W, ye = ( ty + 1 ) * OCC_T < c->H ? ( ty + 1 ) * OCC_T : c->H;
    for ( int y = ty * OCC_T; y < ye; ++y ) {
        const float* p = c->zb + (size_t) y * (size_t) c->W;
        for ( int x = tx * OCC_T; x < xe; ++x ) {
            if ( p[x] > mx ) { mx = p[x]; }
        }
    }
    c->s->tz[ty * c->s->ntx + tx] = mx;
    c->s->td[ty * c->s->ntx + tx] = 0;
}

// is every pixel the object could cover already nearer than the object's nearest point? (then it is not drawn at all)
static bool obj_occluded( rc_t* c, const obj_t* o ) {
    int x0, y0, x1, y1;
    double zmin;
    if ( !obj_rect( c, o, &x0, &y0, &x1, &y1, &zmin ) ) {
        return false;
    }
    for ( int ty = y0 / OCC_T; ty <= y1 / OCC_T; ++ty ) {
        for ( int tx = x0 / OCC_T; tx <= x1 / OCC_T; ++tx ) {
            if ( c->s->td[ty * c->s->ntx + tx] ) {
                tile_refresh( c, tx, ty );
            }
            if ( !( zmin > (double) c->s->tz[ty * c->s->ntx + tx] * 1.00001 + 0.01 ) ) {
                return false;
            }
        }
    }
    return true;
}

static void tiles_dirty( rc_t* c, const obj_t* o ) {
    int x0, y0, x1, y1;
    double zmin;
    if ( !obj_rect( c, o, &x0, &y0, &x1, &y1, &zmin ) ) {
        memset( c->s->td, 1, (size_t) c->s->ntx * (size_t) c->s->nty );
        return;
    }
    for ( int ty = y0 / OCC_T; ty <= y1 / OCC_T; ++ty ) {
        memset( c->s->td + ty * c->s->ntx + x0 / OCC_T, 1, (size_t)( x1 / OCC_T - x0 / OCC_T + 1 ) );
    }
}

static void sort_asc( int* idx, const double* key, int n ) {
    for ( int gap = n / 2; gap > 0; gap /= 2 ) {
        for ( int i = gap; i < n; ++i ) {
            int t = idx[i];
            int j = i;
            while ( j >= gap && ( key[idx[j-gap]] > key[t] || ( key[idx[j-gap]] == key[t] && idx[j-gap] > t ) ) ) {
                idx[j] = idx[j-gap];
                j -= gap;
            }
            idx[j] = t;
        }
    }
}

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
    const uint8_t* cm = c->s->cmap[ iclamp( (int)( (double)( M3D_LEVELS - 1 ) * m->ambient + 0.5 ), 0, M3D_LEVELS - 1 ) ];       // the daylight darkens the sky too
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
    unsigned lampmask;                      // the lamps whose reach touches the triangle's bounding box (bit k = lamp k)
} tri_t;

// -- the span kernel: 4 pixels at a time ---------------------------------------------------------------------------------------
// For four neighbouring pixels of a row: the depth z = 1 / iz and the texel coordinates floor( u / z ), floor( v / z ). GCC's vector extensions (four doubles:
// one AVX register, two SSE2 ones on a plain x86-64, two NEON ones elsewhere) do the divisions and the floor together; no intrinsics, so any target builds.
// z of a lane is 0 when the pixel is not drawn (behind the eye, farther than the far limit, or not nearer than the depth buffer: zb is only read for the
// first nl lanes). The scalar twin gives the same numbers bit for bit (the file is compiled without fused multiply-add contraction: #pragma above); the
// environment variable ENGINE1999_NOSIMD selects it (tests compare the two).
#if E99_VEC
typedef double v4d __attribute__((vector_size(32)));
typedef long long v4l __attribute__((vector_size(32)));
typedef int v4i __attribute__((vector_size(16)));
#endif

static int soft_simd = -1;

typedef struct { double base, fogd, fogstart, far, ifog, iden; bool haze; const lamp_t* lamps; unsigned lampmask; double ir[M3D_MAXLAMPS], icn[M3D_MAXLAMPS]; } fogp_t;       // ir = 1 / radius, icn = 1 / ( 1 - cos of the cone angle ) of each lamp: multiplied, not divided, per pixel       // the light of the triangle, the fog (m3d_t) and the lamps that reach it

void m3d_soft_set_simd( int on ) {                                      // 1 = vector kernel, 0 = scalar twin, -1 = by the environment again (tests)
    soft_simd = on;
}

// the light the lamps add at a world point (brightening only: 0 .. much); a cone is full on its axis and nothing at the edge
static double lamp_add1( const fogp_t* fp, double wx, double wy, double wz ) {
    double add = 0;
    for ( int k=0; k < M3D_MAXLAMPS; ++k ) {
        const lamp_t* l = &fp->lamps[k];
        if ( ( fp->lampmask >> k ) & 1U ) {
            double dx = wx - l->x, dy = wy - l->y, dz = wz - l->z, d2 = dx * dx + dy * dy + dz * dz;
            if ( d2 < l->r * l->r ) {
                double sp = 1.0;
                if ( l->spot ) {
                    // a cone: full on the axis, nothing at the edge (linear in the cosine of the angle to the axis); outside the cone is
                    // found without a square root (the cosine is positive: dot > 0 and dot^2 < cc^2 * d2 is outside)
                    double dot = dx * l->dx + dy * l->dy + dz * l->dz;
                    if ( l->cc > 0.0 && ( dot <= 0.0 || dot * dot < l->cc * l->cc * d2 ) ) {
                        continue;
                    }
                    sp = d2 > 1.0e-18 ? ( dot / (double) sqrtf( (float) d2 ) - l->cc ) * fp->icn[k] : 1.0;
                    sp = sp < 0.0 ? 0.0 : sp > 1.0 ? 1.0 : sp;
                }
                add += l->amount * ( 1.0 - (double) sqrtf( (float) d2 ) * fp->ir[k] ) * sp;
            }
        }
    }
    return add;
}

static void span4_scalar( const tri_t* t, const double* B, int x0, int nl, const float* zb, const fogp_t* fp, double* lz, int* lu, int* lv, int* li, int* hk ) {
    double far = fp->far;
    for ( int j=0; j < 4; ++j ) {
        double xf = (double)( x0 + j ) + 0.5;
        double iz = B[0] + t->gx[0] * xf;
        lz[j] = 0.0; lu[j] = lv[j] = li[j] = hk[j] = 0;
        if ( j < nl && iz > 1.0e-12 ) {
            double z = 1.0 / iz;
            if ( z < (double) zb[x0 + j] && ( far <= 0 || z <= far ) ) {
                lz[j] = z;
                lu[j] = (int) floor( ( B[1] + t->gx[1] * xf ) * z );
                lv[j] = (int) floor( ( B[2] + t->gx[2] * xf ) * z );
                double l = fp->base;
                if ( fp->lampmask != 0 ) {
                    l += lamp_add1( fp, ( B[3] + t->gx[3] * xf ) * z, ( B[4] + t->gx[4] * xf ) * z, ( B[5] + t->gx[5] * xf ) * z ) * ( (double)( M3D_LEVELS - 1 ) / 255.0 );
                }
                int h = 0;
                if ( fp->fogd > 0 ) {
                    if ( fp->haze ) {
                        double q = ( z - fp->fogstart ) * fp->iden;
                        h = q <= 0.0 ? 0 : q >= 1.0 ? 15 : (int)( q * q * 15.0 + 0.5 );
                    } else {
                        double ff = 1.0 - z * fp->ifog;
                        l *= ff < 0 ? 0 : ff;
                    }
                }
                li[j] = (int)( l + 0.5 ); hk[j] = h;
            }
        }
    }
}

#if E99_VEC
typedef float v4f __attribute__((vector_size(16)));

// sqrtf( (float) x ) for four lanes, as doubles: the same value as the scalar expression (a float square root is correctly rounded: sqrtps = sqrtss)
static inline v4d sqrtf4( v4d x ) {
    v4f f = __builtin_convertvector( x, v4f );
#if defined(__SSE__)
    f = (v4f) _mm_sqrt_ps( (__m128) f );
#else
    for ( int j=0; j < 4; ++j ) { f[j] = sqrtf( f[j] ); }
#endif
    return __builtin_convertvector( f, v4d );
}

static inline v4d sel4( v4l m, v4d a, v4d b ) {                       // lane-wise m ? a : b
    return (v4d)( ( (v4l) a & m ) | ( (v4l) b & ~m ) );
}

static inline v4i floor4( v4d x ) {
    v4i ti = __builtin_convertvector( x, v4i );                             // truncates; one too high for a negative fraction
    v4d back = __builtin_convertvector( ti, v4d );
    v4l lt = x < back;                                                      // -1 where x < trunc( x )
    return ti + __builtin_convertvector( lt, v4i );
}

static void span4( const tri_t* t, const double* B, int x0, int W, const float* zb, const fogp_t* fp, double* lz, int* lu, int* lv, int* li, int* hk ) {
    double far = fp->far;
    int nl = W - x0 < 4 ? W - x0 : 4;
    if ( soft_simd < 0 ) {
        soft_simd = getenv( "ENGINE1999_NOSIMD" ) != 0 ? 0 : 1;
    }
    if ( !soft_simd ) {
        span4_scalar( t, B, x0, nl, zb, fp, lz, lu, lv, li, hk );
        return;
    }
    double xf0 = (double) x0 + 0.5;
    v4d xf = { xf0, xf0 + 1.0, xf0 + 2.0, xf0 + 3.0 };
    v4d iz = B[0] + t->gx[0] * xf;
    v4d zero = { 0, 0, 0, 0 };
    v4d one = { 1, 1, 1, 1 };
    v4d zbv = { zb[x0], nl > 1 ? zb[x0+1] : -1.0f, nl > 2 ? zb[x0+2] : -1.0f, nl > 3 ? zb[x0+3] : -1.0f };
    v4d eps = { 1.0e-12, 1.0e-12, 1.0e-12, 1.0e-12 };
    v4d z = one / iz;
    v4l ok = ( iz > eps ) & ( z < zbv );
    if ( far > 0 ) {
        v4d fv = { far, far, far, far };
        ok &= ( z <= fv );
    }
    if ( ok[0] == 0 && ok[1] == 0 && ok[2] == 0 && ok[3] == 0 ) {
        lz[0] = lz[1] = lz[2] = lz[3] = 0.0; lu[0] = lu[1] = lu[2] = lu[3] = 0; lv[0] = lv[1] = lv[2] = lv[3] = 0; li[0] = li[1] = li[2] = li[3] = 0; hk[0] = hk[1] = hk[2] = hk[3] = 0;
        return;
    }
    v4d u = ( B[1] + t->gx[1] * xf ) * z, v = ( B[2] + t->gx[2] * xf ) * z;
    v4l zi = (v4l) z & ok;
    v4d zz = (v4d) zi;
    v4i iu = floor4( u ), iv = floor4( v );
    v4d lvv = { fp->base, fp->base, fp->base, fp->base };
    v4i hkv = { 0, 0, 0, 0 };
    if ( fp->lampmask != 0 ) {
        v4d wx = ( B[3] + t->gx[3] * xf ) * z, wy = ( B[4] + t->gx[4] * xf ) * z, wz = ( B[5] + t->gx[5] * xf ) * z;
        v4d add = zero;
        for ( int k=0; k < M3D_MAXLAMPS; ++k ) {
            if ( !( ( fp->lampmask >> k ) & 1U ) ) {
                continue;
            }
            const lamp_t* l = &fp->lamps[k];
            v4d dx = wx - l->x, dy = wy - l->y, dz = wz - l->z, d2 = dx * dx + dy * dy + dz * dz;
            v4d rr = { l->r * l->r, l->r * l->r, l->r * l->r, l->r * l->r };
            v4l in = d2 < rr;
            v4d sqv = sqrtf4( d2 );
            v4d sp = one;
            if ( l->spot ) {
                v4d dot = dx * l->dx + dy * l->dy + dz * l->dz;
                if ( l->cc > 0.0 ) {
                    v4d cc2 = { l->cc * l->cc, l->cc * l->cc, l->cc * l->cc, l->cc * l->cc };
                    in &= ~( ( dot <= zero ) | ( dot * dot < cc2 * d2 ) );
                }
                v4d tiny = { 1.0e-18, 1.0e-18, 1.0e-18, 1.0e-18 };
                v4d s2 = ( dot / sqv - l->cc ) * fp->icn[k];
                s2 = sel4( d2 > tiny, s2, one );
                sp = sel4( s2 < zero, zero, sel4( s2 > one, one, s2 ) );
            }
            v4d term = l->amount * ( one - sqv * fp->ir[k] ) * sp;
            add = sel4( in, add + term, add );
        }
        lvv = lvv + add * ( (double)( M3D_LEVELS - 1 ) / 255.0 );
    }
    if ( fp->fogd > 0 ) {
        if ( fp->haze ) {
            v4d q = ( z - fp->fogstart ) * fp->iden;                              // clamped to 0 .. 1 first: the scalar rule gives the same steps
            v4d qc = sel4( q < zero, zero, sel4( q > one, one, q ) );
            v4d half = { 0.5, 0.5, 0.5, 0.5 };
            hkv = __builtin_convertvector( ( qc * qc * 15.0 ) + half, v4i );
        } else {
            v4d ff = one - z * fp->ifog;
            ff = sel4( ff < zero, zero, ff );
            lvv = lvv * ff;
        }
    }
    v4d half2 = { 0.5, 0.5, 0.5, 0.5 };
    v4i liv = __builtin_convertvector( lvv + half2, v4i );
    for ( int j=0; j < 4; ++j ) {
        lz[j] = zz[j]; lu[j] = ok[j] != 0 ? iu[j] : 0; lv[j] = ok[j] != 0 ? iv[j] : 0; li[j] = liv[j]; hk[j] = hkv[j];
    }
}
#else
// no vector extensions with this compiler (see vecext.h): the scalar twin does it all
static void span4( const tri_t* t, const double* B, int x0, int W, const float* zb, const fogp_t* fp, double* lz, int* lu, int* lv, int* li, int* hk ) {
    span4_scalar( t, B, x0, W - x0 < 4 ? W - x0 : 4, zb, fp, lz, lu, lv, li, hk );
}
#endif

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
    bool haze = m->hazecol >= 0;
    bool masked = ( t->flags & M3D_OF_MASKED ) != 0 || translucent;
    // a picture of a power of two size wraps with a mask (for a negative coordinate too: two's complement), the others with a division
    fogp_t fpar = { base, fogd, m->fogstart, m->far, fogd > 0 ? 1.0 / fogd : 0.0, fogd > 0 && haze ? 1.0 / ( fogd - m->fogstart ) : 0.0, haze, m->lamps, t->lamps ? t->lampmask : 0U, { 0 }, { 0 } };
    for ( int k=0; k < M3D_MAXLAMPS; ++k ) {
        if ( ( fpar.lampmask >> k ) & 1U ) {
            fpar.ir[k] = 1.0 / m->lamps[k].r;
            fpar.icn[k] = m->lamps[k].spot ? 1.0 / ( 1.0 - m->lamps[k].cc ) : 0.0;
        }
    }
    int wm = -1, hm = -1;
    if ( t->tx != 0 ) {
        wm = ( t->tx->w & ( t->tx->w - 1 ) ) == 0 ? t->tx->w - 1 : -1;
        hm = ( t->tx->h & ( t->tx->h - 1 ) ) == 0 ? t->tx->h - 1 : -1;
    }
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
        double B[6];                                                        // the plane values of this row at x = 0 (the pixel's own are B + gx * (x + 0.5): no running sums)
        for ( int k=0; k < t->na; ++k ) {
            B[k] = t->A0[k] + t->gy[k] * yc;
        }
        uint8_t* fb = c->fb + (size_t) y * (size_t) c->W;
        float* zb = c->zb + (size_t) y * (size_t) c->W;
        for ( int x0 = xs; x0 < xe; x0 += 4 ) {
            int nl = xe - x0 < 4 ? xe - x0 : 4;
            double lz[4];                                                   // per lane: 1 / iz (0 = not drawn), the texture coordinates
            int lu[4], lv_[4], lli[4], lhk[4];
            span4( t, B, x0, c->W, zb, &fpar, lz, lu, lv_, lli, lhk );
            for ( int j=0; j < nl; ++j ) {
                double z = lz[j];
                if ( z > 0.0 ) {
                    int x = x0 + j;
                    uint8_t col;
                    if ( t->tx != 0 ) {
                        int tu = wm >= 0 ? lu[j] & wm : imod( lu[j], t->tx->w ), tv = hm >= 0 ? lv_[j] & hm : imod( lv_[j], t->tx->h );
                        col = t->tx->col[ (size_t) tu * (size_t) t->tx->h + (size_t) tv ];
                    } else {
                        col = (uint8_t) t->tex;
                    }
                    if ( !( masked && col == 0 ) ) {
                        int hk = lhk[j];                                    // (the fog fades to the haze color, quadratically: gentle near, strong far; nothing before the start)
                        int li = iclamp( lli[j], 0, M3D_LEVELS - 1 );
                        cm = c->s->cmap[li];
                        uint8_t out = cm[col];
                        if ( hk > 0 ) {
                            out = c->s->haze[hk][out];
                        }
                        if ( translucent ) {
                            // a color with an alpha of its own in the palette (< 255) is mixed with that alpha (0: nothing), the others half and half
                            unsigned pa = m->pal[col] >> 24;
                            if ( pa == 0 ) {
                                out = fb[x];
                            } else if ( pa < 255U ) {
                                const uint8_t* at = alpha_blend( m, c->s, pa < 8U ? 1 : (int)( pa >> 3 ) );
                                out = at != 0 ? at[ (size_t) fb[x] * 256U + out ] : out;
                            } else {
                                out = c->s->blend != 0 ? c->s->blend[ (size_t) fb[x] * 256U + out ] : out;
                            }
                        } else {
                            zb[x] = (float) z;
                        }
                        fb[x] = out;
                    }
                }
            }
        }
    }
}

// one clipped polygon (view space) -> triangles
static void draw_poly( rc_t* c, const cv_t* p, int n, const v3_t* wp, int tex, int flags, int light, bool translucent, unsigned lampmask ) {
    bool lamps = lampmask != 0;
    // wp: the world position of the polygon's vertices (parallel to p; only used when lamps)
    tri_t t;
    t.tx = tex_get( c->m, tex ); t.tex = tex; t.flags = flags; t.light = light; t.lamps = lamps; t.lampmask = lampmask; t.na = lamps ? 6 : 3;
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

static unsigned lamp_near( const m3d_t* m, v3_t mn, v3_t mx ) {
    unsigned mask = 0;
    for ( int k=0; k < M3D_MAXLAMPS; ++k ) {
        const lamp_t* l = &m->lamps[k];
        if ( l->r > 0 && l->amount > 0 && l->x + l->r >= mn.x && l->x - l->r <= mx.x && l->y + l->r >= mn.y && l->y - l->r <= mx.y && l->z + l->r >= mn.z && l->z - l->r <= mx.z ) {
            mask |= 1U << k;
        }
    }
    return mask;
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
        unsigned lamps = 0;
        if ( c->nlamps > 0 ) {
            v3_t mn = { fmin( wa.x, fmin( wb.x, wc.x ) ), fmin( wa.y, fmin( wb.y, wc.y ) ), fmin( wa.z, fmin( wb.z, wc.z ) ) };
            v3_t mx = { fmax( wa.x, fmax( wb.x, wc.x ) ), fmax( wa.y, fmax( wb.y, wc.y ) ), fmax( wa.z, fmax( wb.z, wc.z ) ) };
            lamps = lamp_near( m, mn, mx );
        }
        draw_poly( c, out, n, wout, t->tex, o->flags, ( o->flags & M3D_OF_GLOW ) ? o->light : (int)( (double) o->light * m->ambient + 0.5 ), translucent, lamps );
    }
    free( vv );
}

// 0 visible, 1 behind the viewer or beyond FAR, 2 outside the sides of the view, 3 too small (the bounding sphere under M3D CULL's smallpx pixels, when the object does not reach the near plane)
static int object_visible( const rc_t* c, const obj_t* o, double* dist ) {
    v3_t d = { o->cx - c->eye.x, o->cy - c->eye.y, o->cz - c->eye.z };
    double zv = vdot( d, c->fw ), xv = vdot( d, c->r ), yv = vdot( d, c->up );
    *dist = zv;
    if ( zv + o->rad < NEARZ ) {
        return 1;
    }
    if ( c->m->far > 0 && zv - o->rad > c->m->far ) {
        return 1;
    }
    double zz = zv + o->rad;
    if ( fabs( xv ) - o->rad > zz * ( c->cx / c->f ) + 0 && zz > 0 && fabs( xv ) > zz * ( c->cx / c->f ) + o->rad * 1.5 ) {
        return 2;
    }
    if ( zz > 0 && fabs( yv ) > zz * ( c->cy / c->f ) + o->rad * 1.5 ) {
        return 2;
    }
    if ( c->m->smallpx > 0 && zv - o->rad > NEARZ && o->rad * c->f < c->m->smallpx * zv ) {
        return 3;
    }
    return 0;
}

static void draw_things( rc_t* c ) {
    m3d_t* m = c->m;
    int n = 0;
    // (the upper part of the scratch arrays: the translucent objects collected before keep the lower part)
    int* ord = m->order + M3D_MAXOBJECTS;
    double* od = m->odist + M3D_MAXOBJECTS;
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
        ord[n] = i; od[i] = zv; ++n;
    }
    sort_desc( ord, od, n );
    for ( int k=0; k < n; ++k ) {
        const thing_t* t = &m->things[ord[k]];
        double zv = od[ord[k]];
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
        double lv = (double) t->light * m->ambient * ( (double)( M3D_LEVELS - 1 ) / 255.0 );
        int hk = 0;
        if ( m->fog > 0 ) {
            if ( m->hazecol >= 0 ) {
                double q = ( zv - m->fogstart ) / ( m->fog - m->fogstart );
                hk = q <= 0.0 ? 0 : q >= 1.0 ? 15 : (int)( q * q * 15.0 + 0.5 );
            } else {
                double ff = 1.0 - zv / m->fog;
                lv *= ff < 0 ? 0 : ff;
            }
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
                if ( hk > 0 ) { out = c->s->haze[hk][out]; }
                uint8_t* p = &c->fb[ (size_t) y * (size_t) c->W + (size_t) x ];
                if ( t->trans && c->s->blend != 0 ) {
                    out = c->s->blend[ (size_t) *p * 256U + out ];
                }
                *p = out;
            }
        }
    }
}

static const void* soft_render( m3d_t* m, void* rd ) {
    soft_t* s = (soft_t*) rd;
    if ( s->fb == 0 ) {
        return 0;
    }
    if ( m->hazecol >= 0 && s->haze_for != m->hazecol ) {
        make_haze( m, s );
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
    memset( m->cull, 0, sizeof(m->cull) );
    draw_sky_bg( &c );
    size_t np = (size_t) m->bw * (size_t) m->bh;
    for ( size_t i=0; i < np; ++i ) {
        s->zb[i] = 1.0e30f;
    }
    int nt = 0, no = 0;
    int* opq = m->order + M3D_MAXOBJECTS + M3D_MAXTHINGS;                  // the opaque objects of the frame, nearest first (M3D CULL ..., 1)
    int nul_; const int* ul_ = m3d_used( m, &nul_ );
    for ( int ui_=0; ui_ < nul_; ++ui_ ) {
        int i = ul_[ui_];
        obj_t* o = &m->objs[i];
        if ( !o->used || !o->shown || o->nt == 0 ) {
            continue;
        }
        m3d_obj_update( o );
        if ( o->dirty ) {
            continue;
        }
        ++m->cull[0];
        double dist;
        int why = object_visible( &c, o, &dist );
        if ( why != 0 ) {
            ++m->cull[why + 1];
            continue;
        }
        if ( ( o->flags & M3D_OF_TRANS ) != 0 ) {
            ++m->cull[1]; m->cull[6] += o->nt;
            m->order[nt] = i; m->odist[i] = dist; ++nt;
            continue;
        }
        if ( m->nearsort ) {
            opq[no] = i; m->odist[i] = dist - o->rad; ++no;                // (the nearest point of the sphere: big objects come first, they hide most)
            continue;
        }
        ++m->cull[1]; m->cull[6] += o->nt;
        draw_object( &c, o, false );
    }
    if ( no > 0 ) {
        sort_asc( opq, m->odist, no );
        bool occ = m->occlude && s->tz != 0;
        if ( occ ) {
            for ( int i=0; i < s->ntx * s->nty; ++i ) { s->tz[i] = 1.0e30f; }
            memset( s->td, 0, (size_t) s->ntx * (size_t) s->nty );
        }
        for ( int k=0; k < no; ++k ) {
            const obj_t* o = &m->objs[opq[k]];
            if ( occ && obj_occluded( &c, o ) ) {
                ++m->cull[5];
                continue;
            }
            ++m->cull[1]; m->cull[6] += o->nt;
            draw_object( &c, &m->objs[opq[k]], false );
            if ( occ ) { tiles_dirty( &c, o ); }
        }
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
        free( s->fb ); free( s->zb ); free( s->blend ); free( s->tz ); free( s->td );
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
    free( s->fb ); free( s->zb ); free( s->tz ); free( s->td );
    s->fb = fb; s->zb = zb;
    s->ntx = ( bw + OCC_T - 1 ) / OCC_T; s->nty = ( bh + OCC_T - 1 ) / OCC_T;
    s->tz = (float*) calloc( (size_t) s->ntx * (size_t) s->nty, sizeof(float) );
    s->td = (uint8_t*) calloc( (size_t) s->ntx * (size_t) s->nty, 1 );
    return true;
}

static void soft_palette( m3d_t* m, void* rd ) {
    make_cmap( m, (soft_t*) rd );
}

const m3drend_t m3d_soft_renderer = { "software", false, soft_create, soft_destroy, soft_resize, soft_palette, soft_render, 0 };
