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
#include "noise.h"

// -- small things ----------------------------------------------------------------------------------------------------------------

static bool num_ok( double v ) {
    return v > -1.0e8 && v < 1.0e8;
}

static bool whole( double v, int lo, int hi, int* out ) {
    if ( !( v >= (double) lo && v <= (double) hi ) ) {
        return false;
    }
    *out = (int) floor( v + 0.5 );
    return true;
}

int m3d_shade_index( const uint32_t* argb, int i, double scale ) {
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
    return bi;
}

// -- the height map (M3D MINIMAP) ---------------------------------------------------------------------------------------------

static int nearest_color( const uint32_t* pal, int r, int g, int b ) {
    double best = 1.0e30;
    int bi = 1;
    for ( int j=1; j < 256; ++j ) {
        double dr = (double)( (int)( ( pal[j] >> 16 ) & 0xFFU ) - r ), dg = (double)( (int)( ( pal[j] >> 8 ) & 0xFFU ) - g ), db = (double)( (int)( pal[j] & 0xFFU ) - b );
        double d = 2.0 * dr * dr + 4.0 * dg * dg + 3.0 * db * db;
        if ( d < best ) {
            best = d;
            bi = j;
        }
    }
    return bi;
}

// the color of a height: (height, red, green, blue) points, linear in between
static void height_color( double h, double* r, double* g, double* b ) {
    static const double ramp[6][4] = { { 0, 60, 130, 50 }, { 40, 110, 150, 60 }, { 90, 140, 110, 70 }, { 150, 130, 130, 130 }, { 230, 245, 245, 245 }, { 1.0e9, 245, 245, 245 } };
    if ( h <= ramp[0][0] ) {
        *r = ramp[0][1]; *g = ramp[0][2]; *b = ramp[0][3];
        return;
    }
    for ( int i=1; i < 6; ++i ) {
        if ( h <= ramp[i][0] ) {
            double t = ( h - ramp[i - 1][0] ) / ( ramp[i][0] - ramp[i - 1][0] );
            *r = ramp[i - 1][1] + t * ( ramp[i][1] - ramp[i - 1][1] );
            *g = ramp[i - 1][2] + t * ( ramp[i][2] - ramp[i - 1][2] );
            *b = ramp[i - 1][3] + t * ( ramp[i][3] - ramp[i - 1][3] );
            return;
        }
    }
}

bool m3d_minimap( const m3d_t* m, const uint32_t* pal, int w, int h, double wx, double wy, double span, double water, uint8_t* out ) {
    if ( w < 1 || h < 1 || !( span > 0.0 ) || out == 0 ) {
        return false;
    }
    double lx = -0.55, ly = -0.35, lz = 0.75;
    if ( m->sun.on ) {
        lx = m->sun.lx; ly = m->sun.ly; lz = m->sun.lz;
    } else {
        double len = sqrt( lx * lx + ly * ly + lz * lz );
        lx /= len; ly /= len; lz /= len;
    }
    double px = span / (double) w;                                  // the size of a pixel in world units (square pixels)
    int lr = -1, lg = -1, lb = -1, li = 1;
    for ( int j=0; j < h; ++j ) {
        for ( int i=0; i < w; ++i ) {
            double x = wx + ( (double) i + 0.5 - (double) w * 0.5 ) * px, y = wy - ( (double) j + 0.5 - (double) h * 0.5 ) * px;
            double z = m3d_ground( m, x, y ), r = 0.0, g = 0.0, b = 0.0;
            double gx = ( m3d_ground( m, x + px, y ) - m3d_ground( m, x - px, y ) ) / ( 2.0 * px );
            double gy = ( m3d_ground( m, x, y + px ) - m3d_ground( m, x, y - px ) ) / ( 2.0 * px );
            double lam = ( -gx * lx - gy * ly + lz ) / sqrt( gx * gx + gy * gy + 1.0 ) / lz;      // 1 on level ground, as in the shaded terrain
            double f = 0.6 + 0.4 * lam;
            f = f < 0.45 ? 0.45 : f > 1.2 ? 1.2 : f;
            if ( z < water ) {
                double d = water - z;
                d = d > 40.0 ? 40.0 : d;
                r = 40.0 - d * 0.5; g = 90.0 - d; b = 170.0 - d;     // deeper is darker
                f = 1.0;
            } else {
                height_color( z, &r, &g, &b );
            }
            int ir = (int) floor( r * f + 0.5 ), ig = (int) floor( g * f + 0.5 ), ib = (int) floor( b * f + 0.5 );
            ir = ir < 0 ? 0 : ir > 255 ? 255 : ir; ig = ig < 0 ? 0 : ig > 255 ? 255 : ig; ib = ib < 0 ? 0 : ib > 255 ? 255 : ib;
            if ( ir != lr || ig != lg || ib != lb ) {
                lr = ir; lg = ig; lb = ib;
                li = nearest_color( pal, ir, ig, ib );
            }
            out[ (size_t) j * (size_t) w + (size_t) i ] = (uint8_t) li;
        }
    }
    return true;
}

// -- shaded terrain pictures (M3D SUN) ------------------------------------------------------------------------------------------

static bool set_picture( m3d_t* m, int n, int w, int h, uint8_t* col ) {      // takes col (u * h + v)
    if ( m->texs[n].col == 0 ) {
        ++m->presepoch;
    }
    free( m->texs[n].col );
    m->texs[n].col = col; m->texs[n].w = w; m->texs[n].h = h; ++m->texs[n].gen;
    return true;
}

// (re)makes the shaded picture n = t + k * stride from the picture t as it is now: false when it cannot be made (no palette yet, out of memory, a flat color above 255)
static bool sun_make( m3d_t* m, int t, int k ) {
    int n = t + k * m->sun.stride;
    if ( n >= M3D_MAXTEX || !m->havepal ) {
        return false;
    }
    uint8_t tab[256];
    double scale = 1.0 - (double) k / (double)( m->sun.steps - 1 ) * ( 1.0 - m->sun.ambient );
    tab[0] = 0;
    for ( int i=1; i < 256; ++i ) {
        tab[i] = (uint8_t) m3d_shade_index( m->pal, i, scale );
    }
    int w = 1, h = 1;
    uint8_t* col;
    if ( m->texs[t].col != 0 ) {
        w = m->texs[t].w; h = m->texs[t].h;
        col = (uint8_t*) malloc( (size_t) w * (size_t) h );
        if ( col == 0 ) {
            return false;
        }
        for ( size_t i = 0; i < (size_t) w * (size_t) h; ++i ) {
            col[i] = tab[ m->texs[t].col[i] ];
        }
    } else if ( t < 256 ) {                                 // a flat color: a picture of one texel
        col = (uint8_t*) malloc( 1 );
        if ( col == 0 ) {
            return false;
        }
        col[0] = tab[t];
    } else {
        return false;
    }
    set_picture( m, n, w, h, col );
    m->sun.src[n] = (uint16_t) t; m->sun.lvl[n] = (uint8_t) k;
    return true;
}

// the shaded pictures that exist are made again (the sun, the palette or a source picture changed); those of a step that is gone are deleted
static void sun_remake( m3d_t* m ) {
    for ( int n = 0; n < M3D_MAXTEX; ++n ) {
        int k = m->sun.lvl[n];
        if ( k == 0 ) {
            continue;
        }
        int t = m->sun.src[n];
        if ( !m->sun.on || k >= m->sun.steps || n != t + k * m->sun.stride || !sun_make( m, t, k ) ) {
            free( m->texs[n].col );
            m->texs[n].col = 0;
            ++m->presepoch;
            m->sun.lvl[n] = 0;
        }
    }
}

static void default_colormap( m3d_t* m ) {
    uint32_t pal[256];
    for ( int i=0; i < 256; ++i ) {
        uint32_t g = (uint32_t) i;
        pal[i] = 0xFF000000U | ( g << 16 ) | ( g << 8 ) | g;
    }
    m3d_colormap( m, pal );
    m->havepal = false;
}

void m3d_colormap( m3d_t* m, const uint32_t* argb ) {
    memcpy( m->pal, argb, sizeof(m->pal) );
    m->havepal = true;
    sun_remake( m );
    if ( m->rend != 0 ) {
        m->rend->palette( m, m->rdata );
    }
}


// -- lifetime -------------------------------------------------------------------------------------------------------------------

static uint32_t gen_seq = 0;

static uint32_t next_gen( void ) {
    if ( ++gen_seq == 0 ) {
        ++gen_seq;
    }
    return gen_seq;
}

static void free_obj( obj_t* o ) {
    if ( o->mref == 0 || --*o->mref <= 0 ) {                // the last one who holds the mesh frees it
        free( o->v ); free( o->t ); free( o->mref );
    }
    free( o->wv );
    memset( o, 0, sizeof(*o) );
}

// before an object's mesh is changed: if it is shared with other objects it gets a private copy (copy on write); false when out of memory
static bool obj_unshare( obj_t* o ) {
    if ( o->mref == 0 ) {
        return true;
    }
    if ( *o->mref > 1 ) {
        mv_t* v = o->nv > 0 ? (mv_t*) malloc( sizeof(mv_t) * (size_t) o->nv ) : 0;
        mt_t* t = o->nt > 0 ? (mt_t*) malloc( sizeof(mt_t) * (size_t) o->nt ) : 0;
        if ( ( o->nv > 0 && v == 0 ) || ( o->nt > 0 && t == 0 ) ) {
            free( v ); free( t );
            return false;
        }
        if ( v != 0 ) { memcpy( v, o->v, sizeof(mv_t) * (size_t) o->nv ); }
        if ( t != 0 ) { memcpy( t, o->t, sizeof(mt_t) * (size_t) o->nt ); }
        --*o->mref;
        o->v = v; o->t = t; o->capv = o->nv; o->capt = o->nt;
    } else {
        free( o->mref );                                    // the only holder left: the mesh is its own again (its capacities were not kept: 0 makes the next add grow it)
        o->capv = o->nv; o->capt = o->nt;
    }
    o->mref = 0;
    return true;
}

static void clear_world( m3d_t* m ) {
    for ( int i=0; i < M3D_MAXOBJECTS; ++i ) {
        free_obj( &m->objs[i] );
    }
    m->top = 0;
    memset( m->things, 0, sizeof(thing_t) * M3D_MAXTHINGS );
    memset( m->lamps, 0, sizeof(m->lamps) );
    m->nobjs = 0;
    m->nzones = 0;
    memset( &m->gnd, 0, sizeof(m->gnd) );
    memset( &m->sun, 0, sizeof(m->sun) );          // (shaded pictures stay, as plain pictures now)
}

static bool set_view( m3d_t* m, int x, int y, int w, int h, int sc, double fov ) {
    int bw = w / sc, bh = h / sc;
    if ( !m->rend->resize( m, m->rdata, bw, bh ) ) {
        return false;
    }
    m->vx = x; m->vy = y; m->vw = w; m->vh = h; m->scale = sc; m->bw = bw; m->bh = bh; m->fov = fov;
    return true;
}

// switches to another renderer: its state is made and sized first, so that nothing changes when that fails
static bool use_renderer( m3d_t* m, const m3drend_t* r, const char** err ) {
    if ( r == m->rend ) {
        return true;
    }
    void* rd = r->create( m );
    if ( rd == 0 ) {
        *err = r == &m3d_soft_renderer ? "Out of memory" : "Hardware renderer could not be started";
        return false;
    }
    if ( !r->resize( m, rd, m->bw, m->bh ) ) {
        r->destroy( m, rd );
        *err = "Out of memory";
        return false;
    }
    const m3drend_t* old = m->rend;
    void* oldrd = m->rdata;
    m->rend = r;
    m->rdata = rd;
    if ( m->havepal ) {
        r->palette( m, rd );
    }
    old->destroy( m, oldrd );
    return true;
}

void m3d_set_hardware( m3d_t* m, const m3drend_t* r ) {
    m->hw = r;
}

static void reset_state( m3d_t* m ) {
    m->fog = 0; m->far = 0; m->skytex = -1; m->skyrep = 1; m->skyshift = 0;
    m->x = m->y = m->z = 0; m->yaw = 0; m->pitch = 0; m->lift = 0;
    m->radius = 14; m->height = 56; m->eye = 46; m->step = 24;
    m->hit = 0; m->shotkind = 0; m->shotobj = -1; m->shotdist = 0; m->shotx = m->shoty = m->shotz = 0;
}

m3d_t* m3d_new( void ) {
    m3d_t* m = (m3d_t*) calloc( 1, sizeof(m3d_t) );
    if ( m == 0 ) {
        return 0;
    }
    m->texs = (tex_t*) calloc( M3D_MAXTEX, sizeof(tex_t) );
    m->objs = (obj_t*) calloc( M3D_MAXOBJECTS, sizeof(obj_t) );
    m->things = (thing_t*) calloc( M3D_MAXTHINGS, sizeof(thing_t) );
    m->zones = (zone_t*) calloc( M3D_MAXZONES, sizeof(zone_t) );
    m->order = (int*) calloc( M3D_MAXOBJECTS + M3D_MAXTHINGS, sizeof(int) );
    m->odist = (double*) calloc( M3D_MAXOBJECTS + M3D_MAXTHINGS, sizeof(double) );
    reset_state( m );
    m->rend = &m3d_soft_renderer;
    m->rdata = m->rend->create( m );
    if ( m->rdata == 0 ) {
        m3d_free( m );
        return 0;
    }
    default_colormap( m );
    if ( m->texs == 0 || m->objs == 0 || m->things == 0 || m->zones == 0 || m->order == 0 || m->odist == 0 || !set_view( m, 0, 0, 640, 300, 2, 90.0 ) ) {
        m3d_free( m );
        return 0;
    }
    return m;
}

void m3d_free( m3d_t* m ) {
    if ( m == 0 ) {
        return;
    }
    if ( m->objs != 0 ) {
        for ( int i=0; i < M3D_MAXOBJECTS; ++i ) {
            free_obj( &m->objs[i] );
        }
    }
    if ( m->texs != 0 ) {
        for ( int i=0; i < M3D_MAXTEX; ++i ) {
            free( m->texs[i].col );
        }
    }
    if ( m->rend != 0 ) {
        m->rend->destroy( m, m->rdata );
    }
    free( m->zones );
    free( m->texs ); free( m->objs ); free( m->things ); free( m->order ); free( m->odist );
    free( m );
}

int m3d_nverts( const m3d_t* m, int o ) {
    return o >= 0 && o < M3D_MAXOBJECTS && m->objs[o].used ? m->objs[o].nv : 0;
}

void m3d_getview( const m3d_t* m, int* x, int* y, int* width, int* height, int* scale, int* bmpw, int* bmph ) {
    *x = m->vx; *y = m->vy; *width = m->vw; *height = m->vh; *scale = m->scale; *bmpw = m->bw; *bmph = m->bh;
}

// -- objects --------------------------------------------------------------------------------------------------------------------

// -- the ground ----------------------------------------------------------------------------------------------------------------

// the ground's height at x,y; every flat box multiplies it by 1 - w, where w is 1 inside the box (centre cx,cy, half sizes rx,ry) and falls to 0 over the
// margin outside it (smooth: the curve of PW in demos/1987.bas)
static double smooth01( double t ) {
    return t <= 0.0 ? 0.0 : t >= 1.0 ? 1.0 : t * t * ( 3.0 - 2.0 * t );
}

// 1 inside the box (centre cx,cy, half sizes rx,ry), falling to 0 over the margin outside it (smooth: the curve of PW in demos/1987.bas)
static double box_weight( double x, double y, double cx, double cy, double rx, double ry, double margin ) {
    double dx = fabs( x - cx ) - rx, dy = fabs( y - cy ) - ry;
    if ( dx < 0 ) { dx = 0; }
    if ( dy < 0 ) { dy = 0; }
    double d = sqrt( dx * dx + dy * dy ) / margin;
    return d >= 1.0 ? 0.0 : ( 1.0 - d ) * ( 1.0 - d ) * ( 1.0 + 2.0 * d );
}

double m3d_ground( const m3d_t* m, double x, double y ) {
    const ground_t* g = &m->gnd;
    if ( !g->on ) {
        return 0.0;
    }
    double h = g->base + g->amp * nz_fbm( g->seed, x / g->scale, y / g->scale, g->oct, g->gain, g->lac );
    double quiet = g->quiet.on ? 1.0 - box_weight( x, y, g->quiet.cx, g->quiet.cy, g->quiet.rx, g->quiet.ry, g->quiet.margin ) : 1.0;
    if ( g->mnt.on && quiet > 0.0 ) {                   // ridged mountains where the (four times slower) noise of the range is high
        double t = quiet * smooth01( ( nz_fbm( g->mnt.seed + 1, x / ( g->mnt.scale * 4.0 ), y / ( g->mnt.scale * 4.0 ), 2, 0.5, 2.0 ) - g->mnt.lo ) / ( g->mnt.hi - g->mnt.lo ) );
        if ( t > 0.0 ) {
            h += g->mnt.amp * t * nz_ridged( g->mnt.seed, x / g->mnt.scale, y / g->mnt.scale, g->mnt.oct, 0.5, 2.0 );
        }
    }
    if ( g->lake.on && quiet > 0.0 ) {                  // basins
        double t = smooth01( ( nz_fbm( g->lake.seed, x / g->lake.scale, y / g->lake.scale, 2, 0.5, 2.0 ) - g->lake.lo ) / ( g->lake.hi - g->lake.lo ) );
        h -= g->lake.depth * t * quiet;
    }
    for ( int i=0; i < 8; ++i ) {
        if ( g->flat[i].on ) {
            h *= 1.0 - box_weight( x, y, g->flat[i].cx, g->flat[i].cy, g->flat[i].rx, g->flat[i].ry, g->flat[i].margin );
        }
    }
    return h;
}

static bool obj_valid( const m3d_t* m, int o ) {
    return o >= 0 && o < M3D_MAXOBJECTS && m->objs[o].used;
}

static bool obj_addv( obj_t* o, double x, double y, double z, double u, double v ) {
    if ( o->mref != 0 && !obj_unshare( o ) ) {
        return false;
    }
    if ( o->nv >= o->capv ) {
        int nc = o->capv == 0 ? 16 : o->capv * 2;
        mv_t* n = (mv_t*) realloc( o->v, sizeof(mv_t) * (size_t) nc );
        if ( n == 0 ) {
            return false;
        }
        o->v = n; o->capv = nc;
    }
    mv_t* p = &o->v[o->nv++];
    p->x = x; p->y = y; p->z = z; p->u = u; p->v = v;
    o->dirty = true;
    o->gen = next_gen();
    return true;
}

// picture coordinates from the position, by the direction of the face (the pictures sit still on the object, 1 texel = 1 unit)
static void auto_uv( double nx, double ny, double nz, double x, double y, double z, double* u, double* v ) {
    double ax = fabs( nx ), ay = fabs( ny ), az = fabs( nz );
    if ( az >= ax && az >= ay ) {
        *u = x; *v = nz >= 0 ? -y : y;
    } else if ( ax >= ay ) {
        *u = nx >= 0 ? y : -y; *v = -z;
    } else {
        *u = ny >= 0 ? -x : x; *v = -z;
    }
}

static bool obj_addt( obj_t* o, int a, int b, int c, int tex ) {
    if ( o->mref != 0 && !obj_unshare( o ) ) {
        return false;
    }
    if ( o->nt >= o->capt ) {
        int nc = o->capt == 0 ? 16 : o->capt * 2;
        mt_t* n = (mt_t*) realloc( o->t, sizeof(mt_t) * (size_t) nc );
        if ( n == 0 ) {
            return false;
        }
        o->t = n; o->capt = nc;
    }
    mt_t* t = &o->t[o->nt++];
    t->a = a; t->b = b; t->c = c; t->tex = tex;
    const mv_t* v[3] = { &o->v[a], &o->v[b], &o->v[c] };
    double ux = v[1]->x - v[0]->x, uy = v[1]->y - v[0]->y, uz = v[1]->z - v[0]->z;
    double wx = v[2]->x - v[0]->x, wy = v[2]->y - v[0]->y, wz = v[2]->z - v[0]->z;
    double nx = uy * wz - uz * wy, ny = uz * wx - ux * wz, nz = ux * wy - uy * wx;
    for ( int k=0; k < 3; ++k ) {
        if ( NONE_VAL( v[k]->u ) || NONE_VAL( v[k]->v ) ) {
            auto_uv( nx, ny, nz, v[k]->x, v[k]->y, v[k]->z, &t->u[k], &t->v[k] );
        } else {
            t->u[k] = v[k]->u; t->v[k] = v[k]->v;
        }
    }
    o->dirty = true;
    o->gen = next_gen();
    return true;
}

// nx x ny cells of the size `cell` from (x0,y0) on, the heights from the ground function; the picture of a cell is tex + the code of its character in cells (0 = tex for all);
// skirt > 0 hangs a wall of that depth under the four edges (it hides the cracks between a coarse grid and its finer neighbour)
// the picture of a terrain cell t shaded for the slope of the cell (heights z00 z10 z01 z11: x then y)
static int cell_picture( m3d_t* m, int t, double cell, double z00, double z10, double z01, double z11 ) {
    if ( !m->sun.on ) {
        return t;
    }
    double dzx = ( ( z10 + z11 ) - ( z00 + z01 ) ) / ( 2.0 * cell ), dzy = ( ( z01 + z11 ) - ( z00 + z10 ) ) / ( 2.0 * cell );
    double inv = 1.0 / sqrt( dzx * dzx + dzy * dzy + 1.0 );
    double lam = ( -dzx * m->sun.lx - dzy * m->sun.ly + m->sun.lz ) * inv / m->sun.lz;        // 1 on level ground
    lam = lam < 0.0 ? 0.0 : lam > 1.0 ? 1.0 : lam;
    int k = (int) floor( ( 1.0 - lam ) * (double)( m->sun.steps - 1 ) + 0.5 );
    if ( k <= 0 ) {
        return t;
    }
    int n = t + k * m->sun.stride;
    if ( n >= M3D_MAXTEX ) {
        return t;
    }
    if ( m->texs[n].col == 0 && !sun_make( m, t, k ) ) {
        return t;
    }
    return n;
}

static bool obj_terrain( m3d_t* m, obj_t* o, double x0, double y0, int nx, int ny, double cell, int tex, const char* cells, double skirt, double uvs ) {
    int base = o->nv, w = nx + 1;
    for ( int j = 0; j <= ny; ++j ) {
        for ( int i = 0; i <= nx; ++i ) {
            double x = x0 + i * cell, y = y0 + j * cell;
            if ( !obj_addv( o, x, y, m3d_ground( m, x, y ), x * uvs, y * uvs ) ) {
                return false;
            }
        }
    }
    for ( int j = 0; j < ny; ++j ) {
        for ( int i = 0; i < nx; ++i ) {
            int b = base + j * w + i, t = tex + ( cells != 0 ? (unsigned char) cells[ j * nx + i ] : 0 );
            t = cell_picture( m, t, cell, o->v[b].z, o->v[b + 1].z, o->v[b + w].z, o->v[b + w + 1].z );
            if ( !obj_addt( o, b, b + 1, b + w + 1, t ) || !obj_addt( o, b, b + w + 1, b + w, t ) ) {
                return false;
            }
        }
    }
    if ( skirt > 0.0 ) {
        // the four edges counterclockwise seen from above (south going east, east going north, north going west, west going south): a wall facing out is a, b, b up, a up
        for ( int side = 0; side < 4; ++side ) {
            int n = side % 2 == 0 ? nx : ny;
            for ( int k = 0; k < n; ++k ) {
                int ia, ib, cell_i, cell_j;
                switch ( side ) {
                    case 0:  ia = k;                  ib = k + 1;              cell_i = k;          cell_j = 0;          break;
                    case 1:  ia = nx + k * w;         ib = nx + ( k + 1 ) * w; cell_i = nx - 1;     cell_j = k;          break;
                    case 2:  ia = ny * w + nx - k;    ib = ny * w + nx - k - 1; cell_i = nx - 1 - k; cell_j = ny - 1;     break;
                    default: ia = ( ny - k ) * w;     ib = ( ny - k - 1 ) * w; cell_i = 0;          cell_j = ny - 1 - k; break;
                }
                int va = base + ia, vb = base + ib, nv = o->nv;
                const mv_t pa = o->v[va], pb = o->v[vb];
                int t = tex + ( cells != 0 ? (unsigned char) cells[ cell_j * nx + cell_i ] : 0 );
                {
                    int b0 = base + cell_j * w + cell_i;
                    t = cell_picture( m, t, cell, o->v[b0].z, o->v[b0 + 1].z, o->v[b0 + w].z, o->v[b0 + w + 1].z );
                }
                if ( !obj_addv( o, pa.x, pa.y, pa.z - skirt, pa.u, pa.v ) || !obj_addv( o, pb.x, pb.y, pb.z - skirt, pb.u, pb.v ) ||
                     !obj_addt( o, nv, nv + 1, vb, t ) || !obj_addt( o, nv, vb, va, t ) ) {
                    return false;
                }
            }
        }
    }
    return true;
}

void m3d_obj_update( obj_t* o ) {
    if ( !o->dirty ) {
        return;
    }
    o->dirty = false;
    if ( o->nv == 0 ) {
        o->minx = o->miny = o->minz = o->maxx = o->maxy = o->maxz = 0;
        o->cx = o->cy = o->cz = o->rad = 0;
        return;
    }
    v3_t* wv = (v3_t*) realloc( o->wv, sizeof(v3_t) * (size_t) o->nv );
    if ( wv == 0 ) {
        o->dirty = true;
        return;
    }
    o->wv = wv;
    const double s = o->scale > 0.0 ? o->scale : 1.0;
    o->minx = o->miny = o->minz = 1.0e30; o->maxx = o->maxy = o->maxz = -1.0e30;
    for ( int i=0; i < o->nv; ++i ) {
        const mv_t* p = &o->v[i];
        double px = p->x * s, py = p->y * s;
        double x = o->px + o->cs * px - o->sn * py, y = o->py + o->sn * px + o->cs * py, z = o->pz + p->z * s;
        wv[i].x = x; wv[i].y = y; wv[i].z = z;
        if ( x < o->minx ) { o->minx = x; }
        if ( x > o->maxx ) { o->maxx = x; }
        if ( y < o->miny ) { o->miny = y; }
        if ( y > o->maxy ) { o->maxy = y; }
        if ( z < o->minz ) { o->minz = z; }
        if ( z > o->maxz ) { o->maxz = z; }
    }
    o->cx = ( o->minx + o->maxx ) * 0.5; o->cy = ( o->miny + o->maxy ) * 0.5; o->cz = ( o->minz + o->maxz ) * 0.5;
    double dx = o->maxx - o->cx, dy = o->maxy - o->cy, dz = o->maxz - o->cz;
    o->rad = sqrt( dx * dx + dy * dy + dz * dz );
}

static bool obj_box( obj_t* o, double x1, double y1, double z1, double x2, double y2, double z2, int tex, int faces ) {
    double lx = x1 < x2 ? x1 : x2, hx = x1 < x2 ? x2 : x1, ly = y1 < y2 ? y1 : y2, hy = y1 < y2 ? y2 : y1, lz = z1 < z2 ? z1 : z2, hz = z1 < z2 ? z2 : z1;
    // each face has its own four vertices (the pictures differ); counterclockwise seen from outside
    const double q[6][4][3] = {
        { { hx, ly, lz }, { hx, hy, lz }, { hx, hy, hz }, { hx, ly, hz } },     // +x
        { { lx, hy, lz }, { lx, ly, lz }, { lx, ly, hz }, { lx, hy, hz } },     // -x
        { { hx, hy, lz }, { lx, hy, lz }, { lx, hy, hz }, { hx, hy, hz } },     // +y
        { { lx, ly, lz }, { hx, ly, lz }, { hx, ly, hz }, { lx, ly, hz } },     // -y
        { { lx, ly, hz }, { hx, ly, hz }, { hx, hy, hz }, { lx, hy, hz } },     // +z
        { { lx, hy, lz }, { hx, hy, lz }, { hx, ly, lz }, { lx, ly, lz } }      // -z
    };
    for ( int f=0; f < 6; ++f ) {
        if ( ( faces & ( 1 << f ) ) == 0 ) {
            continue;
        }
        int base = o->nv;
        for ( int k=0; k < 4; ++k ) {
            if ( !obj_addv( o, q[f][k][0], q[f][k][1], q[f][k][2], GFX_NONE, GFX_NONE ) ) {
                return false;
            }
        }
        if ( !obj_addt( o, base, base + 1, base + 2, tex ) || !obj_addt( o, base, base + 2, base + 3, tex ) ) {
            return false;
        }
    }
    return true;
}

// -- textures -------------------------------------------------------------------------------------------------------------------

static bool set_texture( m3d_t* m, int n, const char* s, size_t slen, const char** err ) {
    if ( slen < 4U ) {
        *err = "Bad picture string";
        return false;
    }
    const uint8_t* b = (const uint8_t*) s;
    int tw = ( b[0] << 8 ) | b[1], th = ( b[2] << 8 ) | b[3];
    if ( tw < 1 || th < 1 || tw > 1024 || th > 1024 || slen != 4U + (size_t) tw * (size_t) th ) {
        *err = "Bad picture string";
        return false;
    }
    uint8_t* col = (uint8_t*) malloc( (size_t) tw * (size_t) th );
    if ( col == 0 ) {
        *err = "Out of memory";
        return false;
    }
    for ( int v=0; v < th; ++v ) {
        for ( int u=0; u < tw; ++u ) {
            col[ (size_t) u * (size_t) th + (size_t) v ] = b[ 4 + (size_t) v * (size_t) tw + (size_t) u ];
        }
    }
    if ( m->texs[n].col == 0 ) {
        ++m->presepoch;
    }
    free( m->texs[n].col );
    m->texs[n].col = col; m->texs[n].w = tw; m->texs[n].h = th; ++m->texs[n].gen;
    return true;
}


// -- geometry: rays and closest points -----------------------------------------------------------------------------------------


// Moeller-Trumbore, both sides; the distance along the (unit) direction or -1
static double ray_tri( v3_t o, v3_t d, v3_t a, v3_t b, v3_t c ) {
    v3_t e1 = vsub( b, a ), e2 = vsub( c, a );
    v3_t p = vcross( d, e2 );
    double det = vdot( e1, p );
    if ( det > -1.0e-12 && det < 1.0e-12 ) {
        return -1;
    }
    double inv = 1.0 / det;
    v3_t t = vsub( o, a );
    double u = vdot( t, p ) * inv;
    if ( u < 0.0 || u > 1.0 ) {
        return -1;
    }
    v3_t q = vcross( t, e1 );
    double v = vdot( d, q ) * inv;
    if ( v < 0.0 || u + v > 1.0 ) {
        return -1;
    }
    double s = vdot( e2, q ) * inv;
    return s > 1.0e-9 ? s : -1;
}

// the nearest point of the triangle to p (Ericson, Real-Time Collision Detection)
static v3_t closest_tri( v3_t p, v3_t a, v3_t b, v3_t c ) {
    v3_t ab = vsub( b, a ), ac = vsub( c, a ), ap = vsub( p, a );
    double d1 = vdot( ab, ap ), d2 = vdot( ac, ap );
    if ( d1 <= 0 && d2 <= 0 ) { return a; }
    v3_t bp = vsub( p, b );
    double d3 = vdot( ab, bp ), d4 = vdot( ac, bp );
    if ( d3 >= 0 && d4 <= d3 ) { return b; }
    double vc = d1 * d4 - d3 * d2;
    if ( vc <= 0 && d1 >= 0 && d3 <= 0 ) { return vadd( a, vmul( ab, d1 / ( d1 - d3 ) ) ); }
    v3_t cp = vsub( p, c );
    double d5 = vdot( ab, cp ), d6 = vdot( ac, cp );
    if ( d6 >= 0 && d5 <= d6 ) { return c; }
    double vb = d5 * d2 - d1 * d6;
    if ( vb <= 0 && d2 >= 0 && d6 <= 0 ) { return vadd( a, vmul( ac, d2 / ( d2 - d6 ) ) ); }
    double va = d3 * d6 - d5 * d4;
    if ( va <= 0 && ( d4 - d3 ) >= 0 && ( d5 - d6 ) >= 0 ) { return vadd( b, vmul( vsub( c, b ), ( d4 - d3 ) / ( ( d4 - d3 ) + ( d5 - d6 ) ) ) ); }
    double den = 1.0 / ( va + vb + vc );
    return vadd( a, vadd( vmul( ab, vb * den ), vmul( ac, vc * den ) ) );
}

typedef struct { double t; int obj, tri; } rayhit_t;

// the nearest triangle on the ray (unit direction d, up to maxd); skip: objects with any of these flags are ignored
static bool trace( m3d_t* m, v3_t o, v3_t d, double maxd, int skip, rayhit_t* hit ) {
    double best = maxd;
    bool found = false;
    for ( int i=0; i < m->top; ++i ) {
        obj_t* ob = &m->objs[i];
        if ( !ob->used || !ob->shown || ob->nt == 0 || ( ob->flags & skip ) != 0 ) {
            continue;
        }
        m3d_obj_update( ob );
        if ( ob->dirty ) {
            continue;
        }
        // slab test against the box
        double t0 = 0, t1 = best;
        const double lo[3] = { ob->minx, ob->miny, ob->minz }, hi[3] = { ob->maxx, ob->maxy, ob->maxz };
        const double oo[3] = { o.x, o.y, o.z }, dd[3] = { d.x, d.y, d.z };
        bool miss = false;
        for ( int k=0; k < 3 && !miss; ++k ) {
            if ( fabs( dd[k] ) < 1.0e-12 ) {
                if ( oo[k] < lo[k] - 1.0e-6 || oo[k] > hi[k] + 1.0e-6 ) {
                    miss = true;
                }
            } else {
                double a = ( lo[k] - oo[k] ) / dd[k], b = ( hi[k] - oo[k] ) / dd[k];
                if ( a > b ) { double tmp = a; a = b; b = tmp; }
                if ( a > t0 ) { t0 = a; }
                if ( b < t1 ) { t1 = b; }
                if ( t0 > t1 ) { miss = true; }
            }
        }
        if ( miss ) {
            continue;
        }
        for ( int j=0; j < ob->nt; ++j ) {
            const mt_t* t = &ob->t[j];
            double s = ray_tri( o, d, ob->wv[t->a], ob->wv[t->b], ob->wv[t->c] );
            if ( s > 0 && s < best ) {
                best = s;
                hit->t = s; hit->obj = i; hit->tri = j;
                found = true;
            }
        }
    }
    return found;
}

// the highest surface at or below z at (x, y); -1e9 if none
static double floor_at( m3d_t* m, double x, double y, double z, rayhit_t* hit ) {
    v3_t o = { x, y, z }, d = { 0, 0, -1 };
    rayhit_t h;
    // the ray starts exactly on a floor sometimes: lift it a little
    o.z += 0.01;
    if ( trace( m, o, d, 20000.0, M3D_OF_PASS, &h ) ) {
        if ( hit != 0 ) {
            *hit = h;
        }
        return o.z - h.t;
    }
    return -1.0e9;
}

static double ceil_at( m3d_t* m, double x, double y, double z ) {
    v3_t o = { x, y, z }, d = { 0, 0, 1 };
    rayhit_t h;
    if ( trace( m, o, d, 20000.0, M3D_OF_PASS, &h ) ) {
        return o.z + h.t;
    }
    return 1.0e9;
}

// -- movement -------------------------------------------------------------------------------------------------------------------

// pushes the capsule (spheres at its middle heights) out of the solid triangles near (x, y); returns whether anything pushed
typedef struct { double x, y, z, lift, radius, height, step; int hit; } body_t;

static bool push_out( m3d_t* m, const body_t* b, double* px, double* py, double feet ) {
    bool pushed = false;
    double r = b->radius;
    double zc[3];
    int nz = 0;
    double z0 = feet + b->step + r, z1 = feet + b->height - r;
    if ( z1 < z0 ) { z1 = z0; }
    zc[nz++] = z0;
    if ( z1 - z0 > r ) { zc[nz++] = ( z0 + z1 ) * 0.5; }
    if ( z1 > z0 ) { zc[nz++] = z1; }
    for ( int it=0; it < 4; ++it ) {
        bool any = false;
        for ( int i=0; i < m->top; ++i ) {
            obj_t* ob = &m->objs[i];
            if ( !ob->used || !ob->shown || ob->nt == 0 || ( ob->flags & M3D_OF_PASS ) != 0 ) {
                continue;
            }
            m3d_obj_update( ob );
            if ( ob->dirty || *px + r < ob->minx || *px - r > ob->maxx || *py + r < ob->miny || *py - r > ob->maxy || feet + b->height < ob->minz || feet + b->step > ob->maxz ) {
                continue;
            }
            for ( int j=0; j < ob->nt; ++j ) {
                const mt_t* t = &ob->t[j];
                v3_t a = ob->wv[t->a], b = ob->wv[t->b], c = ob->wv[t->c];
                double mnx = fmin( a.x, fmin( b.x, c.x ) ), mxx = fmax( a.x, fmax( b.x, c.x ) ), mny = fmin( a.y, fmin( b.y, c.y ) ), mxy = fmax( a.y, fmax( b.y, c.y ) );
                if ( *px + r < mnx || *px - r > mxx || *py + r < mny || *py - r > mxy ) {
                    continue;
                }
                for ( int k=0; k < nz; ++k ) {
                    v3_t cen = { *px, *py, zc[k] };
                    v3_t q = closest_tri( cen, a, b, c );
                    v3_t d = vsub( cen, q );
                    double len = sqrt( vdot( d, d ) );
                    if ( len >= r || len < 1.0e-9 ) {
                        continue;
                    }
                    double push = r - len + 1.0e-4;
                    double hx = d.x / len * push, hy = d.y / len * push;
                    if ( hx * hx + hy * hy < 1.0e-12 ) {
                        continue;
                    }
                    *px += hx; *py += hy;
                    any = pushed = true;
                }
            }
        }
        if ( !any ) {
            break;
        }
    }
    return pushed;
}

static bool try_move( m3d_t* m, body_t* b, double nx, double ny ) {
    double feet = b->z;
    bool pushed = push_out( m, b, &nx, &ny, feet );
    double fl = floor_at( m, nx, ny, feet + b->step, 0 );
    if ( fl < -1.0e8 ) {
        return false;
    }
    double nfeet = fl + b->lift;                    // the lift: the feet's height above the floor (a jump)
    if ( fl - feet > b->step + 1.0e-6 ) {                // (a jump: steps up to step + lift are taken)
        return false;
    }
    double ce = ceil_at( m, nx, ny, fl + 0.5 );
    if ( ce < nfeet + b->height ) {
        return false;
    }
    b->x = nx; b->y = ny; b->z = nfeet;
    if ( pushed ) {
        b->hit |= 1;
    }
    return true;
}

// walks a body by (dx, dy) in small pieces: pushed out of walls, sliding along an axis, following the floor; b->hit: 1 pushed, 2 no movement at all
static void body_move( m3d_t* m, body_t* b, double dx, double dy ) {
    b->hit = 0;
    double len = sqrt( dx * dx + dy * dy );
    int pieces = len > b->radius * 0.5 ? (int) ceil( len / ( b->radius * 0.5 ) ) : 1;
    bool moved = false;
    for ( int k=0; k < pieces; ++k ) {
        double sx = dx / pieces, sy = dy / pieces;
        if ( try_move( m, b, b->x + sx, b->y + sy ) ) { moved = true; continue; }
        bool mx = sx != 0 && try_move( m, b, b->x + sx, b->y );
        bool my = sy != 0 && try_move( m, b, b->x, b->y + sy );
        b->hit |= 1;
        if ( mx || my ) { moved = true; }
    }
    if ( !moved && len > 1.0e-9 ) {
        b->hit |= 2;
    }
    double fl = floor_at( m, b->x, b->y, b->z + b->step, 0 );
    if ( fl > -1.0e8 ) {
        b->z = fl + b->lift;
    }
}

static body_t player_body( const m3d_t* m ) {
    body_t b = { m->x, m->y, m->z, m->lift, m->radius, m->height, m->step, 0 };
    return b;
}

// -- operations -----------------------------------------------------------------------------------------------------------------

static bool thing_get( m3d_t* m, double nv, thing_t** t ) {
    int n;
    if ( !whole( nv, 0, M3D_MAXTHINGS - 1, &n ) ) {
        return false;
    }
    *t = &m->things[n];
    return true;
}

bool m3d_op( m3d_t* m, int op, const double* a, int na, const char* s, size_t slen, const char** err ) {
    int i1, i2, i3;
    double args[10];
    *err = "Illegal argument";
    for ( int i=0; i < 10; ++i ) {
        args[i] = i < na ? a[i] : GFX_NONE;             // arguments beyond na are omitted
        if ( i < na && !NONE_VAL( args[i] ) && !num_ok( args[i] ) && op != GFXOP_M3DTEXTURE ) {
            return false;
        }
    }
    a = args;
    switch ( op ) {
        case GFXOP_M3DSOFTWARE:
            return use_renderer( m, &m3d_soft_renderer, err );
        case GFXOP_M3DHARDWARE:
            if ( m->hw == 0 ) {
                *err = "Hardware renderer not available";
                return false;
            }
            return use_renderer( m, m->hw, err );
        case GFXOP_M3DRESET:
            clear_world( m );
            for ( int i=0; i < M3D_MAXTEX; ++i ) {
                free( m->texs[i].col );
                m->texs[i].col = 0;
            }
            ++m->presepoch;
            reset_state( m );
            return set_view( m, 0, 0, 640, 300, 2, 90.0 );
        case GFXOP_M3DCLEAR:
            clear_world( m );
            return true;
        case GFXOP_M3DTEXTURE:
            return whole( a[0], 0, M3D_MAXTEX - 1, &i1 ) && set_texture( m, i1, s, slen, err );
        case GFXOP_M3DVIEW: {
            int sc = 1;
            double fov = 90.0;
            if ( !whole( a[0], 0, 32767, &i1 ) || !whole( a[1], 0, 32767, &i2 ) || !whole( a[2], 8, M3D_MAXVIEWW, &i3 ) || !( a[3] >= 8 && a[3] <= M3D_MAXVIEWH ) ||
                 ( !NONE_VAL( a[4] ) && !whole( a[4], 1, 4, &sc ) ) || ( !NONE_VAL( a[5] ) && ( a[5] < 30.0 || a[5] > 150.0 ) ) ) {
                return false;
            }
            if ( !NONE_VAL( a[5] ) ) {
                fov = a[5];
            }
            int h = (int) a[3];
            return i3 / sc >= 4 && h / sc >= 4 && set_view( m, i1, i2, i3, h, sc, fov );
        }
        case GFXOP_M3DFOG:
            if ( !( a[0] >= 0.0 && a[0] <= 1.0e6 ) ) { return false; }
            m->fog = a[0];
            return true;
        case GFXOP_M3DFAR:
            if ( !( a[0] >= 0.0 && a[0] <= 1.0e6 ) ) { return false; }
            m->far = a[0];
            return true;
        case GFXOP_M3DSKY:
            if ( !whole( a[0], -1, M3D_MAXTEX - 1, &i1 ) ) { return false; }
            m->skytex = i1;
            m->skyrep = NONE_VAL( a[1] ) ? 1.0 : a[1];
            m->skyshift = NONE_VAL( a[2] ) ? 0.0 : a[2];
            return m->skyrep >= 0.1 && m->skyrep <= 64.0;
        case GFXOP_M3DOBJECT: {
            int fl = 0, li = 255;
            if ( !whole( a[0], 0, M3D_MAXOBJECTS - 1, &i1 ) || ( !NONE_VAL( a[1] ) && !whole( a[1], 0, 255, &fl ) ) || ( !NONE_VAL( a[2] ) && !whole( a[2], 0, 255, &li ) ) ) {
                return false;
            }
            obj_t* o = &m->objs[i1];
            double px = 0, py = 0, pz = 0, yaw = 0;
            bool keep = o->used;
            if ( keep ) { px = o->px; py = o->py; pz = o->pz; yaw = o->yaw; }
            free_obj( o );
            if ( i1 >= m->top ) { m->top = i1 + 1; }
            o->used = true; o->shown = true; o->dirty = true; o->flags = fl; o->light = li; o->gen = next_gen(); o->scale = 1.0;
            o->px = px; o->py = py; o->pz = pz; o->yaw = yaw; o->cs = cos( yaw * DEG2RAD ); o->sn = sin( yaw * DEG2RAD );
            return true;
        }
        case GFXOP_M3DGROUND: {
            ground_t* g = &m->gnd;
            int oct = 1;
            double base = NONE_VAL( a[4] ) ? 0.0 : a[4], gain = NONE_VAL( a[5] ) ? 0.5 : a[5], lac = NONE_VAL( a[6] ) ? 2.0 : a[6];
            if ( !( a[0] > -4.4e9 && a[0] < 4.4e9 ) || !( a[1] > 0.0 && a[1] < 1.0e9 ) || !( a[2] > -1.0e9 && a[2] < 1.0e9 ) || !whole( a[3], 1, NZ_MAXOCTAVES, &oct ) ||
                 !( base > -1.0e9 && base < 1.0e9 ) || !( gain >= 0.0 && gain <= 4.0 ) || !( lac >= 1.0 && lac <= 16.0 ) ) {
                return false;
            }
            g->seed = (uint64_t)(int64_t) floor( a[0] ); g->scale = a[1]; g->amp = a[2]; g->oct = oct; g->base = base; g->gain = gain; g->lac = lac;
            g->on = true;
            return true;
        }
        case GFXOP_M3DMOUNTAINS: {
            int oct;
            if ( !( fabs( a[0] ) < 4.4e9 ) || !( a[1] > 0.0 && a[1] < 1.0e9 ) || !( fabs( a[2] ) < 1.0e9 ) || !whole( a[3], 1, NZ_MAXOCTAVES, &oct ) || !( a[4] >= -1.0 && a[5] <= 1.0 && a[5] > a[4] ) ) {
                return false;
            }
            m->gnd.mnt.on = a[2] != 0.0; m->gnd.mnt.seed = (uint64_t)(int64_t) floor( a[0] ); m->gnd.mnt.scale = a[1]; m->gnd.mnt.amp = a[2]; m->gnd.mnt.oct = oct;
            m->gnd.mnt.lo = a[4]; m->gnd.mnt.hi = a[5];
            return true;
        }
        case GFXOP_M3DLAKES: {
            if ( !( fabs( a[0] ) < 4.4e9 ) || !( a[1] > 0.0 && a[1] < 1.0e9 ) || !( fabs( a[2] ) < 1.0e9 ) || !( a[3] >= -1.0 && a[4] <= 1.0 && a[4] > a[3] ) ) {
                return false;
            }
            m->gnd.lake.on = a[2] != 0.0; m->gnd.lake.seed = (uint64_t)(int64_t) floor( a[0] ); m->gnd.lake.scale = a[1]; m->gnd.lake.depth = a[2];
            m->gnd.lake.lo = a[3]; m->gnd.lake.hi = a[4];
            return true;
        }
        case GFXOP_M3DMINIMAP:                              // (the picture is drawn by the host with m3d_minimap(); this only checks the numbers)
            return whole( a[0], -32768, 32767, &i1 ) && whole( a[1], -32768, 32767, &i2 ) && whole( a[2], 1, 512, &i3 ) && a[3] >= 1.0 && a[3] <= 512.0 &&
                   fabs( a[4] ) < 1.0e9 && fabs( a[5] ) < 1.0e9 && a[6] > 0.0 && a[6] < 1.0e9 && ( NONE_VAL( a[7] ) || fabs( a[7] ) < 1.0e9 );
        case GFXOP_M3DSUN: {
            double amb = NONE_VAL( a[3] ) ? 0.5 : a[3];
            int steps = 4, stride = 100;
            if ( !( fabs( a[0] ) < 1.0e6 && fabs( a[1] ) < 1.0e6 && fabs( a[2] ) < 1.0e6 ) || !( amb >= 0.0 && amb <= 1.0 ) ||
                 ( !NONE_VAL( a[4] ) && !whole( a[4], 2, 8, &steps ) ) || ( !NONE_VAL( a[5] ) && !whole( a[5], 1, M3D_MAXTEX - 1, &stride ) ) ) {
                return false;
            }
            if ( a[0] == 0.0 && a[1] == 0.0 && a[2] == 0.0 ) {
                m->sun.on = false;                          // off: the shaded pictures made so far are deleted
                sun_remake( m );
                return true;
            }
            if ( !( a[2] > 0.0 ) ) {
                *err = "The sun must be above the horizon";
                return false;
            }
            double len = sqrt( a[0] * a[0] + a[1] * a[1] + a[2] * a[2] );
            m->sun.lx = a[0] / len; m->sun.ly = a[1] / len; m->sun.lz = a[2] / len;
            m->sun.ambient = amb; m->sun.steps = steps; m->sun.stride = stride; m->sun.on = true;
            sun_remake( m );
            return true;
        }
        case GFXOP_M3DGROUNDQUIET: {
            if ( !( a[4] >= 0.0 && a[4] < 1.0e9 ) || !( a[2] >= 0.0 && a[3] >= 0.0 && a[2] < 1.0e9 && a[3] < 1.0e9 ) || !( fabs( a[0] ) < 1.0e9 && fabs( a[1] ) < 1.0e9 ) ) {
                return false;
            }
            m->gnd.quiet.on = a[4] > 0.0; m->gnd.quiet.cx = a[0]; m->gnd.quiet.cy = a[1]; m->gnd.quiet.rx = a[2]; m->gnd.quiet.ry = a[3]; m->gnd.quiet.margin = a[4];
            return true;
        }
        case GFXOP_M3DGROUNDFLAT: {
            if ( !whole( a[0], 0, 7, &i1 ) || !( a[5] >= 0.0 && a[5] < 1.0e9 ) || !( a[3] >= 0.0 && a[4] >= 0.0 && a[3] < 1.0e9 && a[4] < 1.0e9 ) ||
                 !( fabs( a[1] ) < 1.0e9 && fabs( a[2] ) < 1.0e9 ) ) {
                return false;
            }
            m->gnd.flat[i1].on = a[5] > 0.0;
            m->gnd.flat[i1].cx = a[1]; m->gnd.flat[i1].cy = a[2]; m->gnd.flat[i1].rx = a[3]; m->gnd.flat[i1].ry = a[4]; m->gnd.flat[i1].margin = a[5];
            return true;
        }
        case GFXOP_M3DTERRAIN: {
            int nx, ny, tex;
            double skirt = NONE_VAL( a[7] ) ? 0.0 : a[7], uvs = NONE_VAL( a[8] ) ? 0.5 : a[8];
            if ( !whole( a[0], 0, M3D_MAXOBJECTS - 1, &i1 ) || !obj_valid( m, i1 ) ) {
                *err = "Object not defined";
                return false;
            }
            if ( !whole( a[3], 1, 256, &nx ) || !whole( a[4], 1, 256, &ny ) || !( a[5] > 0.0 && a[5] < 1.0e9 ) || !whole( a[6], 0, M3D_MAXTEX - 1, &tex ) ||
                 !( fabs( a[1] ) < 1.0e9 && fabs( a[2] ) < 1.0e9 ) || !( skirt >= 0.0 && skirt < 1.0e9 ) || !( fabs( uvs ) < 1.0e6 ) ) {
                return false;
            }
            if ( slen != 0 ) {
                if ( slen != (size_t) nx * (size_t) ny ) {
                    *err = "The cell string needs one character per cell";
                    return false;
                }
                for ( size_t k = 0; k < slen; ++k ) {
                    if ( tex + (unsigned char) s[k] >= M3D_MAXTEX ) {
                        *err = "Picture number out of range";
                        return false;
                    }
                }
            }
            if ( !obj_terrain( m, &m->objs[i1], a[1], a[2], nx, ny, a[5], tex, slen != 0 ? s : 0, skirt, uvs ) ) {
                *err = "Out of memory";
                return false;
            }
            return true;
        }
        case GFXOP_M3DINSTANCE: {
            int src;
            double sc = NONE_VAL( a[6] ) ? 1.0 : a[6];
            if ( !whole( a[0], 0, M3D_MAXOBJECTS - 1, &i1 ) || !whole( a[1], 0, M3D_MAXOBJECTS - 1, &src ) || !( sc > 0.0 && sc < 1.0e6 ) ) {
                return false;
            }
            if ( !obj_valid( m, src ) ) {
                *err = "Object not defined";
                return false;
            }
            if ( i1 == src ) {
                *err = "An object cannot be its own instance";
                return false;
            }
            obj_t* s = &m->objs[src];
            if ( s->mref == 0 && s->nv + s->nt > 0 ) {
                s->mref = (int*) malloc( sizeof(int) );
                if ( s->mref == 0 ) {
                    *err = "Out of memory";
                    return false;
                }
                *s->mref = 1;
            }
            obj_t* o = &m->objs[i1];
            free_obj( o );
            if ( i1 >= m->top ) { m->top = i1 + 1; }
            o->used = true; o->shown = true; o->dirty = true; o->flags = s->flags; o->light = s->light; o->gen = next_gen(); o->scale = sc;
            o->v = s->v; o->nv = s->nv; o->t = s->t; o->nt = s->nt; o->capv = s->nv; o->capt = s->nt;
            if ( s->mref != 0 ) {
                o->mref = s->mref;
                ++*o->mref;
            }
            o->px = NONE_VAL( a[2] ) ? 0.0 : a[2]; o->py = NONE_VAL( a[3] ) ? 0.0 : a[3]; o->pz = NONE_VAL( a[4] ) ? 0.0 : a[4];
            o->yaw = NONE_VAL( a[5] ) ? 0.0 : a[5]; o->cs = cos( o->yaw * DEG2RAD ); o->sn = sin( o->yaw * DEG2RAD );
            return true;
        }
        case GFXOP_M3DOBJSCALE: {
            if ( !whole( a[0], 0, M3D_MAXOBJECTS - 1, &i1 ) || !obj_valid( m, i1 ) ) {
                *err = "Object not defined";
                return false;
            }
            if ( !( a[1] > 0.0 && a[1] < 1.0e6 ) ) {
                return false;
            }
            m->objs[i1].scale = a[1]; m->objs[i1].dirty = true; m->objs[i1].gen = next_gen();
            return true;
        }
        case GFXOP_M3DOBJFREE: {
            if ( !whole( a[0], 0, M3D_MAXOBJECTS - 1, &i1 ) || !whole( a[1], 0, M3D_MAXOBJECTS - 1, &i2 ) || i2 < i1 ) {
                return false;
            }
            for ( int k = i1; k <= i2; ++k ) {
                free_obj( &m->objs[k] );
            }
            while ( m->top > 0 && !m->objs[m->top - 1].used ) {
                --m->top;
            }
            return true;
        }
        case GFXOP_M3DVERTEX: {
            if ( !whole( a[0], 0, M3D_MAXOBJECTS - 1, &i1 ) || !obj_valid( m, i1 ) ) {
                *err = "Object not defined";
                return false;
            }
            double u = a[4], v = a[5];
            if ( NONE_VAL( u ) || NONE_VAL( v ) ) { u = v = GFX_NONE; }
            if ( !obj_addv( &m->objs[i1], a[1], a[2], a[3], u, v ) ) {
                *err = "Out of memory";
                return false;
            }
            return true;
        }
        case GFXOP_M3DFACE: case GFXOP_M3DQUAD: {
            int q = op == GFXOP_M3DQUAD ? 1 : 0, ia, ib, ic, id = 0, tex;
            if ( !whole( a[0], 0, M3D_MAXOBJECTS - 1, &i1 ) || !obj_valid( m, i1 ) ) {
                *err = "Object not defined";
                return false;
            }
            obj_t* o = &m->objs[i1];
            if ( !whole( a[1], 0, o->nv - 1, &ia ) || !whole( a[2], 0, o->nv - 1, &ib ) || !whole( a[3], 0, o->nv - 1, &ic ) || ( q && !whole( a[4], 0, o->nv - 1, &id ) ) ||
                 !whole( a[4 + q], 0, M3D_MAXTEX - 1, &tex ) ) {
                *err = "Vertex or picture number out of range";
                return false;
            }
            if ( !obj_addt( o, ia, ib, ic, tex ) || ( q && !obj_addt( o, ia, ic, id, tex ) ) ) {
                *err = "Out of memory";
                return false;
            }
            return true;
        }
        case GFXOP_M3DBOX: {
            if ( !whole( a[0], 0, M3D_MAXOBJECTS - 1, &i1 ) || !obj_valid( m, i1 ) ) {
                *err = "Object not defined";
                return false;
            }
            int faces = 63;
            if ( !whole( a[7], 0, M3D_MAXTEX - 1, &i2 ) || ( !NONE_VAL( a[8] ) && !whole( a[8], 0, 63, &faces ) ) ) {
                return false;
            }
            if ( !obj_box( &m->objs[i1], a[1], a[2], a[3], a[4], a[5], a[6], i2, faces ) ) {
                *err = "Out of memory";
                return false;
            }
            return true;
        }
        case GFXOP_M3DOBJPOS: {
            if ( !whole( a[0], 0, M3D_MAXOBJECTS - 1, &i1 ) || !obj_valid( m, i1 ) ) {
                *err = "Object not defined";
                return false;
            }
            obj_t* o = &m->objs[i1];
            o->px = a[1]; o->py = a[2]; o->pz = a[3];
            if ( !NONE_VAL( a[4] ) ) {
                o->yaw = a[4]; o->cs = cos( a[4] * DEG2RAD ); o->sn = sin( a[4] * DEG2RAD );
            }
            o->dirty = true;
            return true;
        }
        case GFXOP_M3DOBJSHOW: case GFXOP_M3DOBJLIGHT: case GFXOP_M3DOBJFLAGS: {
            if ( !whole( a[0], 0, M3D_MAXOBJECTS - 1, &i1 ) || !obj_valid( m, i1 ) ) {
                *err = "Object not defined";
                return false;
            }
            if ( op == GFXOP_M3DOBJSHOW ) {
                m->objs[i1].shown = a[1] != 0.0;
            } else if ( !whole( a[1], 0, 255, &i2 ) ) {
                return false;
            } else if ( op == GFXOP_M3DOBJLIGHT ) {
                m->objs[i1].light = i2;
            } else {
                m->objs[i1].flags = i2;
            }
            return true;
        }
        case GFXOP_M3DTHING: {
            int tex;
            if ( !whole( a[0], 0, M3D_MAXTHINGS - 1, &i1 ) || !whole( a[4], 0, M3D_MAXTEX - 1, &tex ) || !( a[5] > 0 ) || !( a[6] > 0 ) ) {
                return false;
            }
            thing_t* t = &m->things[i1];
            memset( t, 0, sizeof(*t) );
            t->on = true; t->x = a[1]; t->y = a[2]; t->z = a[3]; t->tex = tex; t->w = a[5]; t->h = a[6]; t->light = 255;
            return true;
        }
        case GFXOP_M3DTHINGPOS: case GFXOP_M3DTHINGTEX: case GFXOP_M3DTHINGOFF: case GFXOP_M3DTHINGLIGHT: case GFXOP_M3DTHINGTRANS: {
            thing_t* t;
            if ( op == GFXOP_M3DTHINGOFF && thing_get( m, a[0], &t ) && !t->on ) {
                return true;                            // (removing a thing that is not there is fine)
            }
            if ( !thing_get( m, a[0], &t ) || !t->on ) {
                *err = "Thing not defined";
                return false;
            }
            if ( op == GFXOP_M3DTHINGPOS ) {
                t->x = a[1]; t->y = a[2]; t->z = a[3];
            } else if ( op == GFXOP_M3DTHINGOFF ) {
                t->on = false;
            } else if ( op == GFXOP_M3DTHINGTRANS ) {
                t->trans = a[1] != 0.0;
            } else if ( op == GFXOP_M3DTHINGTEX ) {
                if ( !whole( a[1], 0, M3D_MAXTEX - 1, &i2 ) ) { return false; }
                t->tex = i2;
            } else {
                if ( !whole( a[1], 0, 255, &i2 ) ) { return false; }
                t->light = i2;
            }
            return true;
        }
        case GFXOP_M3DCAMERA:
            m->x = a[0]; m->y = a[1]; m->z = a[2]; m->yaw = a[3];
            if ( !NONE_VAL( a[4] ) ) {
                m->pitch = a[4] < -89 ? -89 : a[4] > 89 ? 89 : a[4];
            }
            m->lift = 0;
            return true;
        case GFXOP_M3DTURN:
            m->yaw += a[0];
            if ( !NONE_VAL( a[1] ) ) {
                m->pitch += a[1];
                m->pitch = m->pitch < -89 ? -89 : m->pitch > 89 ? 89 : m->pitch;
            }
            m->yaw = fmod( m->yaw, 360.0 );
            if ( m->yaw < 0 ) { m->yaw += 360.0; }
            return true;
        case GFXOP_M3DPLAYER:
            if ( !( a[0] > 0 && a[1] > 0 && a[2] > 0 && a[2] <= a[1] && a[3] >= 0 ) ) {
                return false;
            }
            m->radius = a[0]; m->height = a[1]; m->eye = a[2]; m->step = a[3];
            return true;
        case GFXOP_M3DLIFT:
            if ( !( a[0] >= -1000.0 && a[0] <= 10000.0 ) ) { return false; }
        {
            double nl = a[0] < 0 ? 0 : a[0];
            double fl = floor_at( m, m->x, m->y, m->z - m->lift + m->step, 0 );
            if ( fl > -1.0e8 ) {
                m->z = fl + nl;
            } else {
                m->z += nl - m->lift;
            }
            m->lift = nl;
            return true;
        }
        case GFXOP_M3DMOVE: {
            double c = cos( m->yaw * DEG2RAD ), sn = sin( m->yaw * DEG2RAD );
            body_t b = player_body( m );
            body_move( m, &b, a[0] * c + a[1] * sn, a[0] * sn - a[1] * c );
            m->x = b.x; m->y = b.y; m->z = b.z; m->hit = b.hit;
            return true;
        }
        case GFXOP_M3DTHINGMOVE: {
            thing_t* t;
            if ( !thing_get( m, a[0], &t ) || !t->on ) {
                *err = "Thing not defined";
                return false;
            }
            body_t b = { t->x, t->y, t->z, 0, t->w * 0.4 > 14 ? 14 : t->w * 0.4, t->h, m->step, 0 };
            body_move( m, &b, a[1], a[2] );
            t->x = b.x; t->y = b.y; t->z = b.z; m->hit = b.hit;
            return true;
        }
        case GFXOP_M3DZONE: {
            int id, tg = 0;
            if ( !whole( a[0], 0, 65535, &id ) || m->nzones >= M3D_MAXZONES || ( !NONE_VAL( a[7] ) && !whole( a[7], 0, 65535, &tg ) ) ) {
                return false;
            }
            zone_t* z = &m->zones[m->nzones++];
            z->id = id; z->tag = tg; z->x1 = fmin( a[1], a[3] ); z->x2 = fmax( a[1], a[3] ); z->y1 = fmin( a[2], a[4] ); z->y2 = fmax( a[2], a[4] );
            z->z1 = fmin( a[5], a[6] ); z->z2 = fmax( a[5], a[6] );
            return true;
        }
        case GFXOP_M3DLAMP: {
            if ( !whole( a[0], 0, M3D_MAXLAMPS - 1, &i1 ) || a[4] < 0 || a[5] < 0 || a[5] > 255 ) {
                return false;
            }
            lamp_t* l = &m->lamps[i1];
            bool spot = !NONE_VAL( a[6] );
            if ( spot ) {
                double len = sqrt( a[6] * a[6] + a[7] * a[7] + a[8] * a[8] );
                if ( NONE_VAL( a[7] ) || NONE_VAL( a[8] ) || NONE_VAL( a[9] ) || len < 1.0e-9 || a[9] < 1.0 || a[9] > 179.0 ) {
                    *err = "M3D LAMP needs dx, dy, dz (not all 0) and an angle of 1 to 179 degrees for a cone";
                    return false;
                }
                l->dx = a[6] / len; l->dy = a[7] / len; l->dz = a[8] / len;
                l->cc = cos( a[9] * 3.14159265358979323846 / 180.0 );
            }
            l->spot = spot;
            l->x = a[1]; l->y = a[2]; l->z = a[3]; l->r = a[4]; l->amount = a[5];
            return true;
        }
        default:
            *err = "Not implemented";
            return false;
    }
}


// -- queries --------------------------------------------------------------------------------------------------------------------

bool m3d_query( m3d_t* m, int q, const double* a, int na, double* r ) {
    (void) na;
    int i1;
    *r = 0;
    switch ( q ) {
        case GFXQ_M3DGROUND: *r = m3d_ground( m, a[0], a[1] ); return true;
        case GFXQ_M3DX: *r = m->x; return true;
        case GFXQ_M3DY: *r = m->y; return true;
        case GFXQ_M3DZ: *r = m->z; return true;
        case GFXQ_M3DEYE: *r = eye_z( m ); return true;
        case GFXQ_M3DYAW: *r = m->yaw; return true;
        case GFXQ_M3DPITCH: *r = m->pitch; return true;
        case GFXQ_M3DLIFT: *r = m->lift; return true;
        case GFXQ_M3DHIT: *r = m->hit; return true;
        case GFXQ_M3DFLOORZ: *r = floor_at( m, m->x, m->y, m->z + m->step, 0 ); return true;
        case GFXQ_M3DFLOORAT: *r = floor_at( m, a[0], a[1], a[2], 0 ); return true;
        case GFXQ_M3DFLOORTEX: case GFXQ_M3DFLOOROBJ: {         // what the floor under the point is made of: the surface M3D("FLOORAT") finds
            rayhit_t h;
            if ( floor_at( m, a[0], a[1], a[2], &h ) < -1.0e8 ) {
                *r = -1;
            } else if ( q == GFXQ_M3DFLOOROBJ ) {
                *r = h.obj;
            } else {
                int t = m->objs[h.obj].t[h.tri].tex;
                *r = t >= 0 && t < M3D_MAXTEX && m->sun.lvl[t] != 0 ? m->sun.src[t] : t;      // a shaded copy tells its cell's own picture
            }
            return true;
        }
        case GFXQ_M3DCEILAT: *r = ceil_at( m, a[0], a[1], a[2] ); return true;
        case GFXQ_M3DDRAWN:
            if ( m->rend != 0 && m->rend->drawn != 0 ) {
                m->drawn = (int) m->rend->drawn( m, m->rdata );
            }
            *r = m->drawn;
            return true;
        case GFXQ_M3DRENDERER: *r = m->rend == m->hw && m->hw != 0 ? 1 : 0; return true;
        case GFXQ_M3DHWAVAIL: *r = m->hw != 0 ? 1 : 0; return true;
        case GFXQ_M3DZONE: case GFXQ_M3DZONETAG: {
            *r = -1;
            for ( int i=0; i < m->nzones; ++i ) {
                const zone_t* z = &m->zones[i];
                if ( a[0] >= z->x1 && a[0] < z->x2 && a[1] >= z->y1 && a[1] < z->y2 && a[2] >= z->z1 - 2.0 && a[2] < z->z2 ) {
                    *r = q == GFXQ_M3DZONE ? z->id : z->tag;
                    break;
                }
            }
            return true;
        }
        case GFXQ_M3DSHOTKIND: *r = m->shotkind; return true;
        case GFXQ_M3DSHOTOBJ: *r = m->shotobj; return true;
        case GFXQ_M3DSHOTDIST: *r = m->shotdist; return true;
        case GFXQ_M3DSHOTX: *r = m->shotx; return true;
        case GFXQ_M3DSHOTY: *r = m->shoty; return true;
        case GFXQ_M3DSHOTZ: *r = m->shotz; return true;
        case GFXQ_M3DTON: case GFXQ_M3DTX: case GFXQ_M3DTY: case GFXQ_M3DTZ: case GFXQ_M3DDIST: case GFXQ_M3DBEARING: case GFXQ_M3DSEES: {
            if ( !whole( a[0], 0, M3D_MAXTHINGS - 1, &i1 ) ) { return false; }
            const thing_t* t = &m->things[i1];
            if ( q == GFXQ_M3DTON ) { *r = t->on ? -1 : 0; return true; }
            if ( !t->on ) { return false; }
            double dx = t->x - m->x, dy = t->y - m->y;
            switch ( q ) {
                case GFXQ_M3DTX: *r = t->x; return true;
                case GFXQ_M3DTY: *r = t->y; return true;
                case GFXQ_M3DTZ: *r = t->z; return true;
                case GFXQ_M3DDIST: *r = sqrt( dx * dx + dy * dy ); return true;
                case GFXQ_M3DBEARING: {
                    double ang = atan2( dy, dx ) / DEG2RAD - m->yaw;
                    ang = fmod( ang + 540.0, 360.0 ) - 180.0;
                    *r = -ang;
                    return true;
                }
                default: {
                    v3_t from = { t->x, t->y, t->z + t->h * 0.5 }, to = { m->x, m->y, eye_z( m ) };
                    v3_t d = vsub( to, from );
                    double len = sqrt( vdot( d, d ) );
                    rayhit_t h;
                    *r = len < 1.0e-6 || !trace( m, from, vmul( d, 1.0 / len ), len - 0.5, M3D_OF_SEETHRU, &h ) ? -1 : 0;
                    return true;
                }
            }
        }
        case GFXQ_M3DSHOOT: case GFXQ_M3DUSE: {
            double range = a[0];
            if ( !( range > 0 && range < 1.0e7 ) ) { return false; }
            double yr = m->yaw * DEG2RAD, pr = m->pitch * DEG2RAD;
            v3_t o = { m->x, m->y, eye_z( m ) };
            v3_t d = { cos( yr ) * cos( pr ), sin( yr ) * cos( pr ), sin( pr ) };
            rayhit_t h;
            bool oh = trace( m, o, d, range, q == GFXQ_M3DSHOOT ? M3D_OF_SEETHRU : 0, &h );
            if ( q == GFXQ_M3DUSE ) {
                *r = oh ? h.obj : -1;
                return true;
            }
            double best = oh ? h.t : range;
            int thing = -1;
            for ( int i=0; i < M3D_MAXTHINGS; ++i ) {
                const thing_t* t = &m->things[i];
                if ( !t->on ) { continue; }
                // the closest approach of the ray to the thing's vertical axis, in the plane
                double dx = t->x - o.x, dy = t->y - o.y, dh = d.x * d.x + d.y * d.y;
                if ( dh < 1.0e-12 ) { continue; }
                double s = ( dx * d.x + dy * d.y ) / dh;
                if ( s <= 0 || s >= best ) { continue; }
                double px = o.x + d.x * s - t->x, py = o.y + d.y * s - t->y, pz = o.z + d.z * s;
                if ( px * px + py * py <= t->w * t->w * 0.25 && pz >= t->z && pz <= t->z + t->h ) {
                    best = s; thing = i;
                }
            }
            m->shotkind = thing >= 0 ? 2 : oh ? 1 : 0;
            m->shotobj = thing < 0 && oh ? h.obj : -1;
            m->shotdist = thing >= 0 || oh ? best : 0;
            m->shotx = o.x + d.x * best; m->shoty = o.y + d.y * best; m->shotz = o.z + d.z * best;
            *r = thing;
            return true;
        }
        case GFXQ_M3DTEXW: case GFXQ_M3DTEXH: {
            if ( !whole( a[0], 0, M3D_MAXTEX - 1, &i1 ) ) { return false; }
            *r = m->texs[i1].col == 0 ? 0 : q == GFXQ_M3DTEXW ? m->texs[i1].w : m->texs[i1].h;
            return true;
        }
        case GFXQ_M3DOBJON: case GFXQ_M3DOBJX: case GFXQ_M3DOBJY: case GFXQ_M3DOBJZ: case GFXQ_M3DOBJYAW: case GFXQ_M3DVERTS: case GFXQ_M3DTRIS: {
            if ( !whole( a[0], 0, M3D_MAXOBJECTS - 1, &i1 ) ) { return false; }
            const obj_t* o = &m->objs[i1];
            if ( q == GFXQ_M3DOBJON ) { *r = o->used ? -1 : 0; return true; }
            if ( !o->used ) { return false; }
            *r = q == GFXQ_M3DOBJX ? o->px : q == GFXQ_M3DOBJY ? o->py : q == GFXQ_M3DOBJZ ? o->pz : q == GFXQ_M3DOBJYAW ? o->yaw : q == GFXQ_M3DVERTS ? o->nv : o->nt;
            return true;
        }
        default:
            return false;
    }
}

// -- the renderer in use --------------------------------------------------------------------------------------------------------

const uint8_t* m3d_render( m3d_t* m ) {
    return m->rend->argb ? 0 : (const uint8_t*) m->rend->render( m, m->rdata );
}

bool m3d_is_argb( const m3d_t* m ) {
    return m->rend->argb;
}

const uint32_t* m3d_render32( m3d_t* m ) {
    return m->rend->argb ? (const uint32_t*) m->rend->render( m, m->rdata ) : 0;
}
