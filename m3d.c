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
#include "roadnet.h"
#include "world.h"
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>

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

int m3d_nearest_rgb( const uint32_t* argb, double tr, double tg, double tb ) {
    double best = 1.0e30;
    int bi = 1;
    for ( int j=1; j < 256; ++j ) {
        double dr = (double)( ( argb[j] >> 16 ) & 0xFFU ) - tr, dg = (double)( ( argb[j] >> 8 ) & 0xFFU ) - tg, db = (double)( argb[j] & 0xFFU ) - tb;
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

const int* m3d_used( m3d_t* m, int* n ) {                      // the numbers of the objects in use, ascending (rebuilt after objects were made or deleted)
    if ( !m->ulok ) {
        int c = 0;
        for ( int i=0; i < m->top; ++i ) {
            if ( m->objs[i].used ) { m->ul[c++] = i; }
        }
        m->nul = c; m->ulok = true;
    }
    *n = m->nul;
    return m->ul;
}

static void free_obj( obj_t* o ) {
    if ( o->mref == 0 || --*o->mref <= 0 ) {                // the last one who holds the mesh frees it
        free( o->v ); free( o->t ); free( o->mref );
    }
    free( o->mf ); free( o->mc );
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

static void jobs_discard( m3d_t* m );
static void wl_close( m3d_t* m );
static double wl_height( const m3d_t* m, double x, double y );
static void wl_built( m3d_t* m, int obj );

static void clear_world( m3d_t* m ) {
    wl_close( m );
    jobs_discard( m );
    for ( int i=0; i < M3D_MAXOBJECTS; ++i ) {
        free_obj( &m->objs[i] );
    }
    m->top = 0; m->ulok = false;
    memset( m->things, 0, sizeof(thing_t) * M3D_MAXTHINGS );
    memset( m->lamps, 0, sizeof(m->lamps) );
    m->nobjs = 0;
    m->nzones = 0;
    memset( &m->gnd, 0, sizeof(m->gnd) );
    memset( &m->sun, 0, sizeof(m->sun) );          // (shaded pictures stay, as plain pictures now)
    memset( &m->rpic, 0, sizeof(m->rpic) );
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

void m3d_invalidate( m3d_t* m ) {
    ++m->presepoch;                                         // the kept meshes of the hardware renderer are built again
    if ( m->havepal && m->rend != 0 ) {
        m->rend->palette( m, m->rdata );                    // the palette texture / color map again
    }
    m->mvalid = false;
}

void m3d_set_hardware( m3d_t* m, const m3drend_t* r ) {
    m->hw = r;
}

static void reset_state( m3d_t* m ) {
    m->fog = 0; m->far = 0; m->hazecol = -1; m->fogstart = 0; m->skytex = -1; m->skyrep = 1; m->skyshift = 0;
    m->x = m->y = m->z = 0; m->yaw = 0; m->pitch = 0; m->lift = 0;
    m->radius = 14; m->height = 56; m->eye = 46; m->step = 24;
    m->hit = 0; m->shotkind = 0; m->shotobj = -1; m->shotdist = 0; m->shotx = m->shoty = m->shotz = 0;
    m->smallpx = 1.0; m->nearsort = true; m->occlude = true;
    memset( m->cull, 0, sizeof(m->cull) );
}

m3d_t* m3d_new( void ) {
    m3d_t* m = (m3d_t*) calloc( 1, sizeof(m3d_t) );
    if ( m == 0 ) {
        return 0;
    }
    m->texs = (tex_t*) calloc( M3D_MAXTEX, sizeof(tex_t) );
    m->objs = (obj_t*) calloc( M3D_MAXOBJECTS, sizeof(obj_t) );
    m->ul = (int*) calloc( M3D_MAXOBJECTS, sizeof(int) );
    m->things = (thing_t*) calloc( M3D_MAXTHINGS, sizeof(thing_t) );
    m->zones = (zone_t*) calloc( M3D_MAXZONES, sizeof(zone_t) );
    m->order = (int*) calloc( 2 * M3D_MAXOBJECTS + M3D_MAXTHINGS, sizeof(int) );        // (the translucent objects, the things, then the opaque objects of a frame)
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

static void jobs_stop( m3d_t* m );

void m3d_free( m3d_t* m ) {
    if ( m == 0 ) {
        return;
    }
    wl_close( m );
    if ( m->wmake != 0 ) { world_close( world_make_finish( m->wmake ) ); m->wmake = 0; }
    jobs_stop( m );
    rnet_flush();
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
    free( m->texs ); free( m->objs ); free( m->ul ); free( m->things ); free( m->order ); free( m->odist );
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

// the height one ridged layer adds at (x, y): amp * (a mask of the range) * ridged noise
static double ridged_layer( const mnt_t* mn, double x, double y, double quiet ) {
    if ( !mn->on || quiet <= 0.0 ) {
        return 0.0;
    }
    double t = quiet * smooth01( ( nz_fbm( mn->seed + 1, x / ( mn->scale * 4.0 ), y / ( mn->scale * 4.0 ), 2, 0.5, 2.0 ) - mn->lo ) / ( mn->hi - mn->lo ) );
    if ( mn->r1 > 0.0 && t > 0.0 ) {                    // faded in between the distances r0 and r1 from the origin (0, 0)
        t *= smooth01( ( sqrt( x * x + y * y ) - mn->r0 ) / ( mn->r1 - mn->r0 ) );
    }
    return t > 0.0 ? mn->amp * t * nz_ridged( mn->seed, x / mn->scale, y / mn->scale, mn->oct, 0.5, 2.0 ) : 0.0;
}

// the roads keep out of the mountains: the road network gets the mask of the mountain layer (the places where it is above 2 % and 1500 units round them)
void m3d_ground_sync_roads( ground_t* g ) {
    const mnt_t* mn = &g->mnt[0];
    rmask_t* k = &g->net.mask;
    memset( k, 0, sizeof(*k) );
    if ( mn->on && mn->amp != 0.0 ) {
        k->on = true; k->seed = mn->seed; k->scale = mn->scale; k->lo = mn->lo; k->hi = mn->hi; k->r0 = mn->r0; k->r1 = mn->r1;
        k->limit = 0.02; k->margin = 1500.0;
    }
}

static double fade01( double d, double inner, double outer ) {      // 0 at 'inner' or nearer, 1 at 'outer' or farther
    return smooth01( ( d - inner ) / ( outer - inner ) );
}

/*
The ground is made in layers, each one adapting to the ones before it (the order of the generation):
1. the basic hills of fractal noise, and the kinds of ground (meadows and fields: a field is flat);
2. dirt roads (M3D ROADNET): the hills keep away from them (they lie on the ground as it is);
3. the hills (M3D HILLS): none on a field, none on a dirt road;
4. tarred roads: they go across any terrain: the ground along a road (about 480 wide) is cut or filled to the road's height, the mean of the ground along the road;
5. lakes and mountains: they keep away from every road (a mountain's foot or a lake's shore never reaches a road);
6. the flat boxes (the house, the field) as the last word.
Everything the roads do fades out towards the quiet box.
*/
double m3d_ground_eval( const ground_t* g, double x, double y ) {
    if ( !g->on ) {
        return 0.0;
    }
    double h = g->base + g->amp * nz_fbm( g->seed, x / g->scale, y / g->scale, g->oct, g->gain, g->lac );
    double quiet = g->quiet.on ? 1.0 - box_weight( x, y, g->quiet.cx, g->quiet.cy, g->quiet.rx, g->quiet.ry, g->quiet.margin ) : 1.0;
    rnear_t rn;
    double hf = 1.0, rf = 1.0;                          // how much of the hills / of the lakes and mountains is left at this place
    bool net = g->net.on && quiet > 0.0;
    if ( net ) {
        rnet_near( &g->net, x, y, 700.0, &rn );
        hf = 1.0 - quiet * ( 1.0 - fade01( rn.dirt, 80.0, 280.0 ) );
        if ( rnet_kind( &g->net, x, y ) == 2 ) {
            hf *= 1.0 - 0.9 * quiet;
        }
        rf = 1.0 - quiet * ( 1.0 - fmin( fade01( rn.dirt, 120.0, 300.0 ), fade01( rn.tar, 300.0, 700.0 ) ) );
    }
    h += hf * ridged_layer( &g->mnt[1], x, y, quiet );
    if ( net && rn.tar < 520.0 ) {                      // a tarred road: cut and fill to the mean height of the ground along it (7 places 150 apart)
        double wt = quiet * ( 1.0 - fade01( rn.tar, 240.0, 520.0 ) ), hd = 0.0;
        for ( int j = -3; j <= 3; ++j ) {
            double t = rn.tt + (double) j * 150.0 / rn.tlink.len, px, py;
            t = t < 0.0 ? 0.0 : t > 1.0 ? 1.0 : t;
            rnet_point( &rn.tlink, t, &px, &py );
            hd += g->base + g->amp * nz_fbm( g->seed, px / g->scale, py / g->scale, g->oct, g->gain, g->lac ) + ridged_layer( &g->mnt[1], px, py, 1.0 );
        }
        h += ( hd / 7.0 - h ) * wt;
    }
    h += rf * ridged_layer( &g->mnt[0], x, y, quiet );
    if ( g->lake.on && quiet > 0.0 ) {                  // basins
        double t = smooth01( ( nz_fbm( g->lake.seed, x / g->lake.scale, y / g->lake.scale, 2, 0.5, 2.0 ) - g->lake.lo ) / ( g->lake.hi - g->lake.lo ) );
        h -= g->lake.depth * t * quiet * rf;
    }
    for ( int i=0; i < 8; ++i ) {
        if ( g->flat[i].on ) {
            h *= 1.0 - box_weight( x, y, g->flat[i].cx, g->flat[i].cy, g->flat[i].rx, g->flat[i].ry, g->flat[i].margin );
        }
    }
    return h;
}

double m3d_ground( const m3d_t* m, double x, double y ) {
    if ( m->wl != 0 ) { return wl_height( m, x, y ); }
    return m3d_ground_eval( &m->gnd, x, y );
}

// -- asynchronous ground jobs: M3D RING (a worker thread makes the ground meshes while the program goes on) ----------------------------------------------------------------
/*
A job makes the whole mesh of one object: a square grid of cells from the ground function (a hole of cells may be left out: the finer grid inside), pictures chosen by a rule in C
(mode 0, the rings: water, rock, grass; mode 1, the chunks round the player: the kinds of ground, mud, rock as FN OCELL of demos/1987.bas does), the slope shading of M3D SUN
(the shaded pictures are made when the result is taken), skirts under the edges, the vertices of the outer cells morphed to the coarser neighbour grid (M3D RINGMORPH: no step
where two rings meet) and, in the rings, the tarred roads as ribbons. The worker thread only reads a copy of the ground function; the result waits until M3D POLL (or RINGWAIT):
it replaces the object's mesh when no job is pending any more, so all rings change together. The ground function and the road network are pure (the road cache is thread local).
*/
#define TJOBS      512
#define TJOB_VARS  96

typedef struct {
    _Atomic int state;                      // 0 free, 1 queued, 2 working, 3 done
    long    seq;
    int     obj, n, mode, hn;
    double  x0, y0, cell, skirt, hx0, hy0, ma, mb;
    ground_t gnd;
    const world_t* world;                   // a tile of the static world (mode 4): the heights are the world's function (the wall included)
    bool    tcol; double tbase;             // mode 5 (the trees of a cell of the world): every vertex of a tree morphs down to the foot of the tree (tbase), so the tree grows out of the ground
    double  ratio;                          // the cell of the coarser grid the morph blends to, in cells (0: 3; 2 in the world: nested grids)
    bool    sunon;
    double  lx, ly, lz;
    int     steps, stride;
    ringpic_t pic;
    mv_t*   v;  int nv, capv;
    mt_t*   t;  int nt, capt;
    float*  mf; float* mc;                  // (rings) the heights as made and as the coarser ring has them
    int     nvar;
    int     vars[TJOB_VARS][2];             // (picture, shade step) pairs whose shaded pictures the result needs
} tjob_t;

typedef struct _tjobs {
    tjob_t  job[TJOBS];
    pthread_t th;
    bool    started;
    _Atomic int quit;
    long    seq;
} tjobs_t;

static int jv_addm( tjob_t* j, double x, double y, double z, double u, double v, double hc ) {
    if ( j->nv >= j->capv ) {
        int nc = j->capv == 0 ? 4096 : j->capv * 2;
        mv_t* nvv = (mv_t*) realloc( j->v, sizeof(mv_t) * (size_t) nc );
        if ( nvv == 0 ) { return -1; }
        j->v = nvv;
        if ( j->mb > j->ma ) {
            float* nf = (float*) realloc( j->mf, sizeof(float) * (size_t) nc );
            float* ncc = (float*) realloc( j->mc, sizeof(float) * (size_t) nc );
            if ( nf == 0 || ncc == 0 ) { return -1; }
            j->mf = nf; j->mc = ncc;
        }
        j->capv = nc;
    }
    mv_t* p = &j->v[j->nv];
    p->x = x; p->y = y; p->z = z; p->u = u; p->v = v;
    if ( j->mb > j->ma ) { j->mf[j->nv] = (float) z; j->mc[j->nv] = (float) hc; }
    return j->nv++;
}

static bool jt_add( tjob_t* j, int a, int b, int c, int tex ) {
    if ( j->nt >= j->capt ) {
        int nc = j->capt == 0 ? 8192 : j->capt * 2;
        mt_t* nt = (mt_t*) realloc( j->t, sizeof(mt_t) * (size_t) nc );
        if ( nt == 0 ) { return false; }
        j->t = nt; j->capt = nc;
    }
    mt_t* t = &j->t[j->nt++];
    t->a = a; t->b = b; t->c = c; t->tex = tex;
    const int ix[3] = { a, b, c };
    for ( int k=0; k < 3; ++k ) {
        t->u[k] = j->v[ix[k]].u; t->v[k] = j->v[ix[k]].v;
    }
    return true;
}

// the height of a vertex of the job: the ground function
static double job_height( const tjob_t* j, double x, double y ) {
    return j->world != 0 ? world_height( j->world, x, y ) : m3d_ground_eval( &j->gnd, x, y );
}

// the surface of the coarser grid (3 x the cell, on the world's grid: the triangles of the coarser ring) at (x, y)
static double job_coarse( const tjob_t* j, double x, double y ) {
    double c2 = j->cell * ( j->ratio > 0.0 ? j->ratio : 3.0 ), gx = floor( x / c2 ) * c2, gy = floor( y / c2 ) * c2;
    double h00 = job_height( j, gx, gy ), h10 = job_height( j, gx + c2, gy ), h11 = job_height( j, gx + c2, gy + c2 ), h01 = job_height( j, gx, gy + c2 );
    double u = ( x - gx ) / c2, v = ( y - gy ) / c2;
    return u >= v ? h00 + u * ( h10 - h00 ) + v * ( h11 - h10 ) : h00 + v * ( h01 - h00 ) + u * ( h11 - h01 );
}

static int job_cellpic( const tjob_t* j, double cx, double cy, double zc, double zmax, double zmin, double sl ) {
    const ringpic_t* p = &j->pic;
    if ( j->mode == 0 ) {
        if ( zc < p->level - 3.0 ) { return p->water; }
        if ( zc > p->rockh || ( zmax - zmin ) / j->cell > 0.9 ) { return p->rock; }
        return p->grass;
    }
    if ( j->mode == 4 ) {                                   // a tile of the static world: the kinds of ground of the road network, rock, mud, no hand-made area
        if ( zc > p->rockh || sl > 45.0 ) { return p->rock; }
        if ( zc < p->level + 1.5 ) { return p->mud; }
        int kw = rnet_kind( &j->gnd.net, cx, cy );
        return kw == 1 ? p->meadow : kw == 2 ? p->field : kw == 3 ? p->forest : p->grass;
    }
    // mode 1: FN OCELL
    int celltex = p->grass;
    double nz = sin( cx * 0.0023 + 1.0 ) * cos( cy * 0.0031 ) + sin( ( cx - cy ) * 0.0017 );
    celltex = nz > 0.55 ? p->grass + 2 : nz < -0.5 ? p->grass + 1 : p->grass;
    if ( cx >= -3600.0 && cx < 3600.0 && cy >= -3600.0 && cy < 3600.0 ) {
        if ( fabs( cx - 1100.0 ) < 700.0 && fabs( cy - 650.0 ) < 500.0 ) { return p->mud; }
        return celltex;
    }
    double s = fabs( m3d_ground_eval( &j->gnd, cx + 50.0, cy ) - m3d_ground_eval( &j->gnd, cx - 50.0, cy ) ) + fabs( m3d_ground_eval( &j->gnd, cx, cy + 50.0 ) - m3d_ground_eval( &j->gnd, cx, cy - 50.0 ) );
    if ( zc > p->rockh || s > 45.0 ) { return p->rock; }
    if ( zc < p->level + 1.5 ) { return p->mud; }
    int k = rnet_kind( &j->gnd.net, cx, cy );
    return k == 1 ? p->meadow : k == 2 ? p->field : k == 3 ? p->forest : celltex;
}

static int job_shade( const tjob_t* j, double cell, double z00, double z10, double z01, double z11 ) {
    if ( !j->sunon ) { return 0; }
    double dzx = ( ( z10 + z11 ) - ( z00 + z01 ) ) / ( 2.0 * cell ), dzy = ( ( z01 + z11 ) - ( z00 + z10 ) ) / ( 2.0 * cell );
    double inv = 1.0 / sqrt( dzx * dzx + dzy * dzy + 1.0 );
    double lam = ( -dzx * j->lx - dzy * j->ly + j->lz ) * inv / j->lz;
    lam = lam < 0.0 ? 0.0 : lam > 1.0 ? 1.0 : lam;
    return (int) floor( ( 1.0 - lam ) * (double)( j->steps - 1 ) + 0.5 );
}

static void job_var( tjob_t* j, int pic, int k ) {
    for ( int i=0; i < j->nvar; ++i ) {
        if ( j->vars[i][0] == pic && j->vars[i][1] == k ) { return; }
    }
    if ( j->nvar < TJOB_VARS ) {
        j->vars[j->nvar][0] = pic; j->vars[j->nvar][1] = k; ++j->nvar;
    }
}

// the height of the mesh of the job at (x, y) (the triangles of its cells), the ground function where there is none (outside, in the hole)
static double job_meshh( const tjob_t* j, double x, double y ) {
    int n = j->n, w = n + 1;
    int ci = (int) floor( ( x - j->x0 ) / j->cell ), cj = (int) floor( ( y - j->y0 ) / j->cell );
    int hi0 = j->hn > 0 ? (int) floor( ( j->hx0 - j->x0 ) / j->cell + 0.5 ) : 0, hj0 = j->hn > 0 ? (int) floor( ( j->hy0 - j->y0 ) / j->cell + 0.5 ) : 0;
    if ( ci < 0 || ci >= n || cj < 0 || cj >= n || ( j->hn > 0 && ci >= hi0 && ci < hi0 + j->hn && cj >= hj0 && cj < hj0 + j->hn ) ) {
        return m3d_ground_eval( &j->gnd, x, y );
    }
    int b = cj * w + ci;
    double z00 = j->v[b].z, z10 = j->v[b + 1].z, z01 = j->v[b + w].z, z11 = j->v[b + w + 1].z;
    double u = ( x - ( j->x0 + ci * j->cell ) ) / j->cell, v = ( y - ( j->y0 + cj * j->cell ) ) / j->cell;
    return u >= v ? z00 + u * ( z10 - z00 ) + v * ( z11 - z10 ) : z00 + v * ( z01 - z00 ) + u * ( z11 - z01 );
}

#define RIBBON_COLS 5

// a ribbon of road on the mesh: net 1 the tarred roads (440 wide, 5 columns of vertices, a piece every 100 units in a chunk or every cell in a ring), net 0 the dirt tracks (110 wide, 3 columns,
// pieces of 50, chunks only); the heights come from the mesh itself, the chunk that holds the start of a piece makes it
static void job_ribbons( tjob_t* j, int net ) {
    int n = j->n;
    rlink_t links[40];
    double ex = j->x0 + n * j->cell, ey = j->y0 + n * j->cell;
    int nl = rnet_links( &j->gnd.net, net, j->x0 - 500.0, j->y0 - 500.0, ex + 500.0, ey + 500.0, links, 40 );
    int cols = net ? RIBBON_COLS : 3;
    double half = net ? ( j->mode == 4 ? fmax( 220.0, j->cell * 0.2 ) : j->mode >= 1 ? 220.0 : fmax( 220.0, j->cell * 0.25 ) ) : 55.0, lift = net ? 3.0 + j->cell * 0.02 : 3.0, vsc = net ? 0.17 : 0.58;
    int tex = net ? ( ( j->mode == 0 || j->mode == 4 ) && j->cell >= 2700.0 ? j->pic.tarfar : j->pic.tar ) : j->pic.dirt;
    bool morph = j->mb > j->ma;
    static _Thread_local double px[1500], py[1500], pd[1500];
    static _Thread_local int row[1500];
    for ( int li = 0; li < nl; ++li ) {
        const rlink_t* l = &links[li];
        double sub = j->mode == 4 ? l->step / fmax( net ? 100.0 : 50.0, j->cell * 0.5 ) : ( net && j->mode >= 1 ) ? 2.0 : net ? 1.0 / fmax( 1.0, floor( j->cell / l->step ) ) : 1.0;
        double dt = 1.0 / ( (double) l->steps * sub );
        int np = 0;
        for ( double t = 0.0; t < 1.0 - 1.0e-9 && np < 1499; t += dt ) { rnet_point( l, t, &px[np], &py[np] ); ++np; }
        rnet_point( l, 1.0, &px[np], &py[np] ); ++np;
        pd[0] = 0.0; row[0] = -1;
        for ( int k = 1; k < np; ++k ) { pd[k] = pd[k - 1] + sqrt( ( px[k] - px[k - 1] ) * ( px[k] - px[k - 1] ) + ( py[k] - py[k - 1] ) * ( py[k] - py[k - 1] ) ); row[k] = -1; }
        for ( int k = 0; k + 1 < np; ++k ) {
            double sx = px[k], sy = py[k];
            bool mine;
            if ( j->mode >= 1 ) {                                        // a chunk makes the pieces that start in it
                mine = sx >= j->x0 && sx < ex && sy >= j->y0 && sy < ey;
            } else {                                                     // a ring the pieces in it, none over the finer ring
                double mx = ( sx + px[k + 1] ) * 0.5, my = ( sy + py[k + 1] ) * 0.5;
                mine = mx >= j->x0 && mx < ex && my >= j->y0 && my < ey;
                if ( mine && j->hn > 0 && mx >= j->hx0 && mx < j->hx0 + j->hn * j->cell && my >= j->hy0 && my < j->hy0 + j->hn * j->cell ) { mine = false; }
            }
            if ( !mine ) { continue; }
            double qw = 1.0;
            if ( j->gnd.quiet.on ) { qw = 1.0 - box_weight( sx, sy, j->gnd.quiet.cx, j->gnd.quiet.cy, j->gnd.quiet.rx, j->gnd.quiet.ry, j->gnd.quiet.margin ); }
            if ( qw < 0.99 ) { continue; }
            for ( int e = 0; e < 2; ++e ) {                              // the two ends: a row of vertices across the road on the mesh (the normal from the places before and after: shared with the neighbouring piece)
                int kk = k + e;
                if ( row[kk] >= 0 ) { continue; }
                int k0 = kk > 0 ? kk - 1 : 0, k1 = kk < np - 1 ? kk + 1 : np - 1;
                double tx = px[k1] - px[k0], ty = py[k1] - py[k0], tl = sqrt( tx * tx + ty * ty );
                if ( tl < 1.0e-9 ) { tl = 1.0; tx = 1.0; ty = 0.0; }
                double nx = ty / tl, ny = -tx / tl;
                int first = -1;
                for ( int c = 0; c < cols; ++c ) {
                    double off = half - 2.0 * half * (double) c / (double)( cols - 1 );
                    double vx = px[kk] + nx * off, vy = py[kk] + ny * off, vz = job_meshh( j, vx, vy ) + lift;
                    int vi = jv_addm( j, vx, vy, vz, 64.0 * ( 1.0 - (double) c / (double)( cols - 1 ) ), pd[kk] * vsc, morph ? job_coarse( j, vx, vy ) + lift : vz );
                    if ( vi < 0 ) { return; }
                    if ( c == 0 ) { first = vi; }
                }
                row[kk] = first;
            }
            for ( int c = 0; c + 1 < cols; ++c ) {
                int a0 = row[k] + c, a1 = row[k] + c + 1, b0 = row[k + 1] + c, b1 = row[k + 1] + c + 1;
                if ( !jt_add( j, a0, b0, b1, tex ) || !jt_add( j, a0, b1, a1, tex ) ) { return; }
            }
        }
    }
    if ( net ) {                                                                // the intersections: a square of plain tar over the roads' ends (a little above the arms), the arms leave it square to its sides
        double hxy[2 * 24];
        int nh = rnet_hubs( &j->gnd.net, j->x0 - 1.0, j->y0 - 1.0, ex + 1.0, ey + 1.0, hxy, 24 );
        int htex = j->pic.tarfar > 0 ? j->pic.tarfar : tex;
        for ( int h = 0; h < nh; ++h ) {
            double cx = hxy[2 * h], cy = hxy[2 * h + 1];
            if ( j->mode == 0 && j->hn > 0 && cx >= j->hx0 && cx < j->hx0 + j->hn * j->cell && cy >= j->hy0 && cy < j->hy0 + j->hn * j->cell ) { continue; }
            if ( j->gnd.quiet.on && 1.0 - box_weight( cx, cy, j->gnd.quiet.cx, j->gnd.quiet.cy, j->gnd.quiet.rx, j->gnd.quiet.ry, j->gnd.quiet.margin ) < 0.99 ) { continue; }
            int vi[9];
            for ( int q = 0; q < 9; ++q ) {                                     // 3 x 3 vertices, so the square follows the mesh
                double vx = cx + ( (double)( q % 3 ) - 1.0 ) * half, vy = cy + ( (double)( q / 3 ) - 1.0 ) * half, vz = job_meshh( j, vx, vy ) + lift + 1.5 + j->cell * 0.01;
                vi[q] = jv_addm( j, vx, vy, vz, (double)( q % 3 ) * 32.0, (double)( q / 3 ) * 32.0, morph ? job_coarse( j, vx, vy ) + lift + 1.5 + j->cell * 0.01 : vz );
                if ( vi[q] < 0 ) { return; }
            }
            for ( int cj = 0; cj < 2; ++cj ) {
                for ( int ci = 0; ci < 2; ++ci ) {
                    int b = cj * 3 + ci;
                    if ( !jt_add( j, vi[b], vi[b + 1], vi[b + 4], htex ) || !jt_add( j, vi[b], vi[b + 4], vi[b + 3], htex ) ) { return; }
                }
            }
        }
    }
}

// a vertex of a tree: it moves with the ground under the tree (dz: the coarser surface minus the true ground there)
static int jv_addtree( tjob_t* j, double dz, double x, double y, double z, double u, double v ) {
    return jv_addm( j, x, y, z, u, v, j->tcol ? j->tbase : z + dz );
}

// a tree: the lathe solids of demos/1987.bas (8 sides: a trunk and a crown of 2 .. 5 rings), or a cheap one for the far chunks (a cone of 4 sides on a square trunk: 12 triangles)
static bool job_tree( tjob_t* j, double x, double y, double z, double yaw, double sc, int kind, bool cheap, double dz ) {
    static const double crown[3][5][3] = {
        { { 14, 86, 63 }, { 46, 108, 50 }, { 54, 140, 32 }, { 38, 182, 14 }, { 0.6, 206, 0 } },
        { { 38, 74, 63 }, { 0.6, 134, 0 }, { 31, 112, 63 }, { 0.6, 172, 0 } },            // (the third cone follows below)
        { { 10, 74, 63 }, { 22, 120, 45 }, { 24, 180, 25 }, { 14, 240, 8 }, { 0.6, 285, 0 } } };
    double cs = cos( yaw * DEG2RAD ), sn = sin( yaw * DEG2RAD );
    j->tbase = z;
    int bark = j->pic.bark, leaf = j->pic.leaf[kind];
    if ( cheap ) {
        double br = kind == 0 ? 46.0 : kind == 1 ? 36.0 : 22.0, bz = kind == 0 ? 108.0 : kind == 1 ? 74.0 : 120.0, tz = kind == 0 ? 206.0 : kind == 1 ? 248.0 : 285.0;
        int base[2];
        for ( int r = 0; r < 2; ++r ) {                                   // the trunk (4 sides, 2 rings) and the cone's ring
            double rad = r == 0 ? 5.0 : br, zz = r == 0 ? 0.0 : bz, ztop = r == 0 ? 100.0 : 0.0;
            (void) ztop;
            base[r] = j->nv;
            for ( int k = 0; k < 5; ++k ) {
                double a = (double) k * 1.5707963, lx = rad * cos( a ), ly = rad * sin( a );
                if ( jv_addtree( j, dz, x + ( lx * cs - ly * sn ) * sc, y + ( lx * sn + ly * cs ) * sc, z + zz * sc, (double) k * 8.0, r == 0 ? 63.0 : 40.0 ) < 0 ) { return false; }
            }
        }
        int top = jv_addtree( j, dz, x, y, z + tz * sc, 16.0, 0.0 ), tt[5];
        if ( top < 0 ) { return false; }
        for ( int k = 0; k < 5; ++k ) {                                    // the top of the trunk: a ring at the cone's foot, narrow
            double a = (double) k * 1.5707963, lx = 3.2 * cos( a ), ly = 3.2 * sin( a );
            tt[k] = jv_addtree( j, dz, x + ( lx * cs - ly * sn ) * sc, y + ( lx * sn + ly * cs ) * sc, z + bz * sc, (double) k * 8.0, 0.0 );
            if ( tt[k] < 0 ) { return false; }
        }
        for ( int k = 0; k < 4; ++k ) {
            if ( !jt_add( j, base[0] + k, base[0] + k + 1, tt[k + 1], bark ) || !jt_add( j, base[0] + k, tt[k + 1], tt[k], bark ) ) { return false; }
            if ( !jt_add( j, base[1] + k, base[1] + k + 1, top, leaf ) ) { return false; }
        }
        return true;
    }
    // trunk: rings (5, 0, 63) and (3.2, 100 or 80, 0)
    double th = kind == 2 ? 80.0 : 100.0;
    int rb = j->nv;
    for ( int r = 0; r < 2; ++r ) {
        double rad = r == 0 ? 5.0 : 3.2, zz = r == 0 ? 0.0 : th;
        for ( int k = 0; k <= 8; ++k ) {
            double a = (double) k * 0.7853982, lx = rad * cos( a ), ly = rad * sin( a );
            if ( jv_addtree( j, dz, x + ( lx * cs - ly * sn ) * sc, y + ( lx * sn + ly * cs ) * sc, z + zz * sc, (double) k * 8.0, r == 0 ? 63.0 : 0.0 ) < 0 ) { return false; }
        }
    }
    for ( int k = 0; k < 8; ++k ) {
        int pp = rb + k;
        if ( !jt_add( j, pp, pp + 1, pp + 10, bark ) || !jt_add( j, pp, pp + 10, pp + 9, bark ) ) { return false; }
    }
    // crown: kind 0 and 2 five rings in one lathe, kind 1 three cones of two rings
    int ncone = kind == 1 ? 3 : 1, nr = kind == 1 ? 2 : 5;
    for ( int cn = 0; cn < ncone; ++cn ) {
        int cb = j->nv;
        for ( int r = 0; r < nr; ++r ) {
            double rad, zz, vv;
            if ( kind == 1 ) { rad = r == 0 ? 38.0 - cn * 7.0 : 0.6; zz = r == 0 ? 74.0 + cn * 38.0 : 134.0 + cn * 38.0; vv = r == 0 ? 63.0 : 0.0; }
            else { rad = crown[kind][r][0]; zz = crown[kind][r][1]; vv = crown[kind][r][2]; }
            if ( rad < 0.6 / sc ) { rad = 0.6 / sc; }
            for ( int k = 0; k <= 8; ++k ) {
                double a = (double) k * 0.7853982, lx = rad * cos( a ), ly = rad * sin( a );
                if ( jv_addtree( j, dz, x + ( lx * cs - ly * sn ) * sc, y + ( lx * sn + ly * cs ) * sc, z + zz * sc, (double) k * 8.0, vv ) < 0 ) { return false; }
            }
        }
        for ( int r = 0; r + 1 < nr; ++r ) {
            for ( int k = 0; k < 8; ++k ) {
                int pp = cb + r * 9 + k;
                if ( !jt_add( j, pp, pp + 1, pp + 10, leaf ) || !jt_add( j, pp, pp + 10, pp + 9, leaf ) ) { return false; }
            }
        }
    }
    return true;
}

// the trees of a chunk (the rules of CHTREES in demos/1987.bas): up to 24 at hashed places in forest, on ground between the water and a height of 70, not on or beside a road, 70 units apart
static void job_trees( tjob_t* j ) {
    int64_t cx = (int64_t) floor( j->x0 / 900.0 + 0.5 ), cy = (int64_t) floor( j->y0 / 900.0 + 0.5 );
    double tx[24], ty[24];
    int cnt = 0;
    for ( int i = 0; i < 60 && cnt < 24; ++i ) {
        double x = (double) cx * 900.0 + nz_hash( cx, cy, 100 + i, 0 ) * 900.0, y = (double) cy * 900.0 + nz_hash( cx, cy, 200 + i, 0 ) * 900.0;
        if ( rnet_kind( &j->gnd.net, x, y ) != 3 ) { continue; }
        double h = m3d_ground_eval( &j->gnd, x, y );
        if ( !( h > j->pic.level + 2.0 && h < 70.0 ) || nz_hash( cx, cy, 300 + i, 0 ) >= 0.7 ) { continue; }
        rnear_t rn;
        rnet_near( &j->gnd.net, x, y, 320.0, &rn );
        if ( rn.dirt < 100.0 || rn.tar < 310.0 ) { continue; }
        bool ok = true;
        for ( int q = 0; q < cnt; ++q ) { if ( ( tx[q] - x ) * ( tx[q] - x ) + ( ty[q] - y ) * ( ty[q] - y ) < 4900.0 ) { ok = false; } }
        if ( !ok ) { continue; }
        tx[cnt] = x; ty[cnt] = y; ++cnt;
        double kk = nz_hash( cx, cy, 400 + i, 0 );
        int kind = kk >= 0.8 ? 2 : kk >= 0.45 ? 1 : 0;
        if ( !job_tree( j, x, y, h - 2.0, nz_hash( cx, cy, 500 + i, 0 ) * 360.0, 0.9 + nz_hash( cx, cy, 600 + i, 0 ) * 0.5, kind, j->mode == 3, j->mb > j->ma ? job_coarse( j, x, y ) - h : 0.0 ) ) { return; }
    }
}

// the trees of a cell of the static world (mode 5: 3906 units = 125 m square, an object of its own, no ground): the cell is cut into squares of 162.8 units (5 m) on the world's grid,
// each holds at most one tree at a place hashed from the world seed and the square's number (so the same tree stands in the same place whatever cells are built), in forest mostly, now and
// then alone on a meadow or plain; not on or beside a road, not on steep or high ground, not in the yard of the house and its field; cheap trees (12 triangles). Every vertex morphs down to
// the foot of its tree (tcol), the ground job sets the distances: a tree grows out of the ground between a and b, nothing is added in the middle of the view.
static double job_wh( const tjob_t* j, int64_t a, int64_t b, int64_t c ) {
    return (double)( nz_hash64( world_seed( j->world ), a, b, c, 0 ) >> 11 ) * ( 1.0 / 9007199254740992.0 );
}

static void job_wtrees( tjob_t* j ) {
    const double sub = WORLD_CHUNK / 96.0;
    const int ns = (int) floor( j->cell * (double) j->n / sub + 0.5 );
    int64_t i0 = (int64_t) floor( j->x0 / sub + 0.5 ), k0 = (int64_t) floor( j->y0 / sub + 0.5 );
    for ( int q = 0; q < ns; ++q ) {
        for ( int p = 0; p < ns; ++p ) {
            int64_t ia = i0 + p, ka = k0 + q;
            double x = ( (double) ia + job_wh( j, ia, ka, 1 ) ) * sub, y = ( (double) ka + job_wh( j, ia, ka, 2 ) ) * sub;
            if ( fabs( x ) < 1500.0 && fabs( y ) < 1500.0 ) { continue; }                          // the house and its yard
            if ( fabs( x - 1100.0 ) < 900.0 && fabs( y - 650.0 ) < 700.0 ) { continue; }          // the wheat field
            int kw = rnet_kind( &j->gnd.net, x, y );
            double chance = kw == 3 ? 0.15 : 0.006;
            if ( job_wh( j, ia, ka, 3 ) >= chance ) { continue; }
            double h = job_height( j, x, y );
            if ( !( h > j->pic.level + 2.0 && h < j->pic.rockh - 60.0 ) ) { continue; }
            if ( fabs( job_height( j, x + 50.0, y ) - job_height( j, x - 50.0, y ) ) > 50.0 || fabs( job_height( j, x, y + 50.0 ) - job_height( j, x, y - 50.0 ) ) > 50.0 ) { continue; }
            rnear_t rn;
            rnet_near( &j->gnd.net, x, y, 320.0, &rn );
            if ( rn.dirt < 100.0 || rn.tar < 310.0 ) { continue; }
            double kk = job_wh( j, ia, ka, 4 );
            int kind = kk >= 0.8 ? 2 : kk >= 0.45 ? 1 : 0;
            if ( !job_tree( j, x, y, h - 2.0, job_wh( j, ia, ka, 5 ) * 360.0, 0.9 + job_wh( j, ia, ka, 6 ) * 0.5, kind, true, 0.0 ) ) { return; }
        }
    }
}

// a reflector post (the one of demos/1987.bas, 14 triangles): a square post 5 across and 36 high, on its -x face a white reflector (seen by the traffic that comes from behind), on its +x face
// a red one (the oncoming traffic's); the road's direction is the post's +x; the vertices morph to the foot like a tree's
static bool job_post( tjob_t* j, double x, double y, double z, double yaw ) {
    double cs = cos( yaw * DEG2RAD ), sn = sin( yaw * DEG2RAD );
    j->tbase = z;
    #define PV(lx,ly,lz) jv_addtree( j, 0.0, x + (lx) * cs - (ly) * sn, y + (lx) * sn + (ly) * cs, z + (lz), 0.0, 0.0 )
    int b[4], t[4];
    static const double cx[4] = { 2.5, 2.5, -2.5, -2.5 }, cy[4] = { -2.5, 2.5, 2.5, -2.5 };
    for ( int k = 0; k < 4; ++k ) { b[k] = PV( cx[k], cy[k], 0.0 ); t[k] = PV( cx[k], cy[k], 36.0 ); }
    // reflectors: a quad a little proud of the face, bottom left, bottom right, top right, top left as seen from outside
    int w[4] = { PV( -2.9, 2.0, 21.0 ), PV( -2.9, -2.0, 21.0 ), PV( -2.9, -2.0, 31.0 ), PV( -2.9, 2.0, 31.0 ) };
    int r[4] = { PV( 2.9, -2.0, 21.0 ), PV( 2.9, 2.0, 21.0 ), PV( 2.9, 2.0, 31.0 ), PV( 2.9, -2.0, 31.0 ) };
    #undef PV
    for ( int k = 0; k < 4; ++k ) {
        if ( b[k] < 0 || t[k] < 0 || w[k] < 0 || r[k] < 0 ) { return false; }
    }
    int tex = j->pic.post[0];
    for ( int k = 0; k < 4; ++k ) {
        int k1 = ( k + 1 ) & 3;
        if ( !jt_add( j, b[k], b[k1], t[k1], tex ) || !jt_add( j, b[k], t[k1], t[k], tex ) ) { return false; }
    }
    return jt_add( j, t[0], t[1], t[2], tex ) && jt_add( j, t[0], t[2], t[3], tex )
        && jt_add( j, w[0], w[1], w[2], j->pic.post[1] ) && jt_add( j, w[0], w[2], w[3], j->pic.post[1] )
        && jt_add( j, r[0], r[1], r[2], j->pic.post[2] ) && jt_add( j, r[0], r[2], r[3], j->pic.post[2] );
}

// the posts of a cell: every 50 m (1562.5 units) along each tarred road, measured on the way from its start, on both sides (232 from the middle, the right-hand one faces the traffic, the left-hand
// one is turned round); none within 520 of an intersection, none where the road is not made (the quiet box round the house)
static void job_wposts( tjob_t* j ) {
    const double S = 1562.5, ex = j->x0 + j->n * j->cell, ey = j->y0 + j->n * j->cell;
    rlink_t links[16];
    int nl = rnet_links( &j->gnd.net, 1, j->x0 - 300.0, j->y0 - 300.0, ex + 300.0, ey + 300.0, links, 16 );
    static _Thread_local double px[2000], py[2000];
    for ( int li = 0; li < nl; ++li ) {
        const rlink_t* l = &links[li];
        int np = 0, ns = l->steps * 4 < 1999 ? l->steps * 4 : 1999;
        for ( int k = 0; k <= ns; ++k ) { rnet_point( l, (double) k / (double) ns, &px[np], &py[np] ); ++np; }
        double total = 0.0;
        for ( int k = 1; k < np; ++k ) { total += hypot( px[k] - px[k - 1], py[k] - py[k - 1] ); }
        double d = 0.0, next = S;
        for ( int k = 1; k < np; ++k ) {
            double dx = px[k] - px[k - 1], dy = py[k] - py[k - 1], sl = hypot( dx, dy );
            while ( sl > 0.0 && d + sl >= next ) {
                double f = ( next - d ) / sl, mx = px[k - 1] + dx * f, my = py[k - 1] + dy * f, pd = next;
                next += S;
                if ( ( l->hub1 && pd < 520.0 ) || ( l->hub2 && pd > total - 520.0 ) ) { continue; }
                if ( j->gnd.quiet.on && 1.0 - box_weight( mx, my, j->gnd.quiet.cx, j->gnd.quiet.cy, j->gnd.quiet.rx, j->gnd.quiet.ry, j->gnd.quiet.margin ) < 0.99 ) { continue; }
                double ux = dx / sl, uy = dy / sl, yaw = atan2( uy, ux ) / DEG2RAD;
                for ( int side = 0; side < 2; ++side ) {
                    double sg = side == 0 ? 1.0 : -1.0, qx = mx + uy * 232.0 * sg, qy = my - ux * 232.0 * sg;
                    if ( qx < j->x0 || qx >= ex || qy < j->y0 || qy >= ey ) { continue; }
                    double h = job_height( j, qx, qy );
                    if ( !( h > j->pic.level + 1.0 ) ) { continue; }
                    if ( !job_post( j, qx, qy, h - 1.0, side == 0 ? yaw : yaw + 180.0 ) ) { return; }
                }
            }
            d += sl;
        }
    }
}

// a hay bale (a box of 44 x 28 x 22 units like the one of demos/1987.bas, 10 triangles: four sides and the top), scaled by sc, turned by yaw; the vertices morph to the foot
static bool job_bale( tjob_t* j, double x, double y, double z, double yaw, double sc ) {
    double cs = cos( yaw * DEG2RAD ), sn = sin( yaw * DEG2RAD );
    j->tbase = z;
    static const double cx[4] = { 22.0, 22.0, -22.0, -22.0 }, cy[4] = { -14.0, 14.0, 14.0, -14.0 };
    int b[4], t[4];
    for ( int k = 0; k < 4; ++k ) {
        double lx = cx[k] * sc, ly = cy[k] * sc;
        b[k] = jv_addtree( j, 0.0, x + lx * cs - ly * sn, y + lx * sn + ly * cs, z, (double) k * 16.0, 63.0 );
        t[k] = jv_addtree( j, 0.0, x + lx * cs - ly * sn, y + lx * sn + ly * cs, z + 22.0 * sc, (double) k * 16.0, 0.0 );
        if ( b[k] < 0 || t[k] < 0 ) { return false; }
    }
    int tex = j->pic.bale;
    for ( int k = 0; k < 4; ++k ) {
        int k1 = ( k + 1 ) & 3;
        if ( !jt_add( j, b[k], b[k1], t[k1], tex ) || !jt_add( j, b[k], t[k1], t[k], tex ) ) { return false; }
    }
    return jt_add( j, t[0], t[1], t[2], tex ) && jt_add( j, t[0], t[2], t[3], tex );
}

// the bales of a cell: on the same grid of squares as the trees (162.8 units), in a field (rnet_kind 2) a bale in 12 squares out of 100, hashed from the world seed (other hash
// channels than the trees), not on or beside a road, not in the yard or on the hand-made wheat field, not on rough ground or under the water
static void job_wbales( tjob_t* j ) {
    const double sub = WORLD_CHUNK / 96.0;
    const int ns = (int) floor( j->cell * (double) j->n / sub + 0.5 );
    int64_t i0 = (int64_t) floor( j->x0 / sub + 0.5 ), k0 = (int64_t) floor( j->y0 / sub + 0.5 );
    for ( int q = 0; q < ns; ++q ) {
        for ( int p = 0; p < ns; ++p ) {
            int64_t ia = i0 + p, ka = k0 + q;
            if ( job_wh( j, ia, ka, 13 ) >= 0.12 ) { continue; }
            double x = ( (double) ia + job_wh( j, ia, ka, 11 ) ) * sub, y = ( (double) ka + job_wh( j, ia, ka, 12 ) ) * sub;
            if ( fabs( x ) < 1500.0 && fabs( y ) < 1500.0 ) { continue; }
            if ( fabs( x - 1100.0 ) < 900.0 && fabs( y - 650.0 ) < 700.0 ) { continue; }
            if ( rnet_kind( &j->gnd.net, x, y ) != 2 ) { continue; }
            double h = job_height( j, x, y );
            if ( !( h > j->pic.level + 3.0 && h < j->pic.rockh - 60.0 ) ) { continue; }
            if ( fabs( job_height( j, x + 30.0, y ) - job_height( j, x - 30.0, y ) ) > 40.0 || fabs( job_height( j, x, y + 30.0 ) - job_height( j, x, y - 30.0 ) ) > 40.0 ) { continue; }
            rnear_t rn;
            rnet_near( &j->gnd.net, x, y, 320.0, &rn );
            if ( rn.dirt < 100.0 || rn.tar < 310.0 ) { continue; }
            if ( !job_bale( j, x, y, h - 1.0, job_wh( j, ia, ka, 14 ) * 360.0, 0.8 + job_wh( j, ia, ka, 15 ) * 0.5 ) ) { return; }
        }
    }
}

static void job_run( tjob_t* j ) {
    int n = j->n, w = n + 1;
    j->nv = j->nt = j->nvar = 0;
    if ( j->mode == 5 ) {                                          // the trees of a cell of the static world, no ground
        if ( j->world != 0 ) {
            j->tcol = true;
            if ( j->pic.bark > 0 ) { job_wtrees( j ); }
            if ( j->pic.posts ) { job_wposts( j ); }
            if ( j->pic.bales ) { job_wbales( j ); }
        }
        return;
    }
    double uvs = j->mode == 1 ? 0.5 : j->cell <= 300.0 ? 0.5 : j->cell <= 900.0 ? 0.1 : 0.02;
    int hi0 = j->hn > 0 ? (int) floor( ( j->hx0 - j->x0 ) / j->cell + 0.5 ) : 0, hj0 = j->hn > 0 ? (int) floor( ( j->hy0 - j->y0 ) / j->cell + 0.5 ) : 0;
    for ( int jy = 0; jy <= n; ++jy ) {
        for ( int jx = 0; jx <= n; ++jx ) {
            double x = j->x0 + jx * j->cell, y = j->y0 + jy * j->cell;
            double hz = job_height( j, x, y );
            if ( jv_addm( j, x, y, hz, x * uvs, y * uvs, j->mb > j->ma ? job_coarse( j, x, y ) : hz ) < 0 ) { return; }
        }
    }
    #define HOLE(ci,cj) ( j->hn > 0 && (ci) >= hi0 && (ci) < hi0 + j->hn && (cj) >= hj0 && (cj) < hj0 + j->hn )
    #define CELL_OFF(ci,cj) ( (ci) < 0 || (ci) >= n || (cj) < 0 || (cj) >= n || HOLE(ci,cj) )
    for ( int cj = 0; cj < n; ++cj ) {
        for ( int ci = 0; ci < n; ++ci ) {
            if ( HOLE( ci, cj ) ) { continue; }
            int b = cj * w + ci;
            double z00 = j->v[b].z, z10 = j->v[b + 1].z, z01 = j->v[b + w].z, z11 = j->v[b + w + 1].z;
            double zmax = fmax( fmax( z00, z10 ), fmax( z01, z11 ) ), zmin = fmin( fmin( z00, z10 ), fmin( z01, z11 ) );
            double cx = j->x0 + ( ci + 0.5 ) * j->cell, cy = j->y0 + ( cj + 0.5 ) * j->cell;
            double sl = 100.0 * ( fabs( ( z10 + z11 ) - ( z00 + z01 ) ) + fabs( ( z01 + z11 ) - ( z00 + z10 ) ) ) / ( 2.0 * j->cell );     // the slope, as FN OCELL measures it
            int p = job_cellpic( j, cx, cy, ( z00 + z10 + z01 + z11 ) * 0.25, zmax, zmin, sl );
            int k = job_shade( j, j->cell, z00, z10, z01, z11 );
            if ( k > 0 && p + k * j->stride < M3D_MAXTEX ) { job_var( j, p, k ); p += k * j->stride; }
            if ( !jt_add( j, b, b + 1, b + w + 1, p ) || !jt_add( j, b, b + w + 1, b + w, p ) ) { return; }
        }
    }
    if ( j->skirt > 0.0 ) {
        for ( int cj = 0; cj < n; ++cj ) {
            for ( int ci = 0; ci < n; ++ci ) {
                if ( HOLE( ci, cj ) ) { continue; }
                int b = cj * w + ci;
                // the four edges counterclockwise: south, east, north, west; a wall where the neighbour cell is off the grid or in the hole
                const int va[4] = { b, b + 1, b + w + 1, b + w }, vb[4] = { b + 1, b + w + 1, b + w, b };
                const bool off[4] = { CELL_OFF( ci, cj - 1 ), CELL_OFF( ci + 1, cj ), CELL_OFF( ci, cj + 1 ), CELL_OFF( ci - 1, cj ) };
                int pc = 0;
                if ( j->nt >= 2 ) { pc = j->t[ j->nt - 1 ].tex; }
                for ( int s = 0; s < 4; ++s ) {
                    if ( !off[s] ) { continue; }
                    mv_t pa = j->v[va[s]], pb = j->v[vb[s]];
                    double ca = j->mb > j->ma ? j->mc[va[s]] : pa.z, cb = j->mb > j->ma ? j->mc[vb[s]] : pb.z;
                    int na = jv_addm( j, pa.x, pa.y, pa.z - j->skirt, pa.u, pa.v, ca - j->skirt ), nb = jv_addm( j, pb.x, pb.y, pb.z - j->skirt, pb.u, pb.v, cb - j->skirt );
                    if ( na < 0 || nb < 0 || !jt_add( j, na, nb, vb[s], pc ) || !jt_add( j, na, vb[s], va[s], pc ) ) { return; }
                }
            }
        }
    }
    #undef HOLE
    #undef CELL_OFF
    // the tarred roads as ribbons on the mesh (about 440 wide; in the rings wider so that they are seen), a little above the mesh: the chunk that holds the start of a piece makes it
    if ( j->pic.tar > 0 && j->gnd.net.on ) {
        job_ribbons( j, 1 );
    }
    if ( j->mode >= 1 && j->pic.dirt > 0 && j->gnd.net.on && ( j->mode != 4 || j->cell <= 250.0 ) ) {
        job_ribbons( j, 0 );
    }
    if ( j->mode >= 2 && j->mode != 4 && j->pic.bark > 0 && j->gnd.net.on ) {
        job_trees( j );
    }
}

static void* job_worker( void* arg ) {
    tjobs_t* js = (tjobs_t*) arg;
    while ( !atomic_load( &js->quit ) ) {
        tjob_t* best = 0;
        for ( int i=0; i < TJOBS; ++i ) {                              // a chunk before a ring (the ground the player stands on first), the older before the newer
            if ( atomic_load( &js->job[i].state ) == 1 && ( best == 0 || ( js->job[i].mode != 0 && best->mode == 0 ) || ( ( js->job[i].mode != 0 ) == ( best->mode != 0 ) && js->job[i].seq < best->seq ) ) ) { best = &js->job[i]; }
        }
        if ( best == 0 ) {
            struct timespec ts = { 0, 1000000L };
            nanosleep( &ts, 0 );
            continue;
        }
        atomic_store( &best->state, 2 );
        job_run( best );
        atomic_store( &best->state, 3 );
    }
    rnet_flush();
    return 0;
}

static void job_free_result( tjob_t* j ) {
    free( j->v ); free( j->t ); free( j->mf ); free( j->mc );
    j->v = 0; j->t = 0; j->mf = j->mc = 0; j->nv = j->nt = j->capv = j->capt = 0;
}

static void jobs_stop( m3d_t* m ) {
    tjobs_t* js = m->jobs;
    if ( js == 0 ) { return; }
    atomic_store( &js->quit, 1 );
    if ( js->started ) { pthread_join( js->th, 0 ); }
    for ( int i=0; i < TJOBS; ++i ) { job_free_result( &js->job[i] ); }
    free( js );
    m->jobs = 0;
}

static int jobs_pending( const m3d_t* m ) {                     // jobs that are queued, working or done and not taken yet
    int c = 0;
    if ( m->jobs != 0 ) {
        for ( int i=0; i < TJOBS; ++i ) { if ( atomic_load( &m->jobs->job[i].state ) != 0 ) { ++c; } }
    }
    return c;
}

static int jobs_running_mode( const m3d_t* m, int mode ) {      // queued or working (mode -1: all)
    int c = 0;
    if ( m->jobs != 0 ) {
        for ( int i=0; i < TJOBS; ++i ) {
            int s = atomic_load( &m->jobs->job[i].state );
            if ( ( s == 1 || s == 2 ) && ( mode < 0 || m->jobs->job[i].mode == mode ) ) { ++c; }
        }
    }
    return c;
}

static int jobs_running( const m3d_t* m ) {                    // queued or working
    return jobs_running_mode( m, -1 );
}

static bool jobs_submit( m3d_t* m, tjob_t* proto ) {
    if ( m->jobs == 0 ) {
        m->jobs = (tjobs_t*) calloc( 1, sizeof(tjobs_t) );
        if ( m->jobs == 0 ) { return false; }
    }
    tjobs_t* js = m->jobs;
    if ( !js->started ) {
        if ( pthread_create( &js->th, 0, job_worker, js ) != 0 ) { return false; }
        js->started = true;
    }
    tjob_t* slot = 0;
    for ( int i=0; i < TJOBS; ++i ) {                           // a job for the same object that has not started is replaced
        if ( atomic_load( &js->job[i].state ) == 1 && js->job[i].obj == proto->obj ) { slot = &js->job[i]; break; }
    }
    if ( slot != 0 ) {
        if ( !atomic_compare_exchange_strong( &slot->state, &(int){ 1 }, 4 ) ) { slot = 0; }     // 4: being filled (the worker leaves it)
    }
    if ( slot == 0 ) {
        for ( int i=0; i < TJOBS; ++i ) {
            int zero = 0;
            if ( atomic_compare_exchange_strong( &js->job[i].state, &zero, 4 ) ) { slot = &js->job[i]; break; }
        }
    }
    if ( slot == 0 ) { return false; }
    job_free_result( slot );
    slot->seq = ++js->seq;
    slot->obj = proto->obj; slot->n = proto->n; slot->mode = proto->mode; slot->hn = proto->hn;
    slot->x0 = proto->x0; slot->y0 = proto->y0; slot->cell = proto->cell; slot->skirt = proto->skirt; slot->hx0 = proto->hx0; slot->hy0 = proto->hy0; slot->ma = proto->ma; slot->mb = proto->mb;
    slot->gnd = proto->gnd; slot->sunon = proto->sunon; slot->lx = proto->lx; slot->ly = proto->ly; slot->lz = proto->lz; slot->steps = proto->steps; slot->stride = proto->stride;
    slot->pic = proto->pic; slot->world = proto->world; slot->ratio = proto->ratio;
    atomic_store( &slot->state, 1 );
    return true;
}

// takes the finished jobs (a chunk (mode 1) at once, the rings (mode 0) only when none of them is queued or working any more: they change together): the object's mesh is replaced
static int jobs_apply( m3d_t* m ) {
    tjobs_t* js = m->jobs;
    int applied = 0;
    if ( js == 0 ) { return 0; }
    bool ringsok = jobs_running_mode( m, 0 ) == 0;                // the rings (mode 0) change together, the chunks (mode 1) one by one
    for ( ;; ) {
        tjob_t* best = 0;
        for ( int i=0; i < TJOBS; ++i ) {
            if ( atomic_load( &js->job[i].state ) == 3 && ( js->job[i].mode != 0 || ringsok ) && ( best == 0 || js->job[i].seq < best->seq ) ) { best = &js->job[i]; }
        }
        if ( best == 0 ) { break; }
        if ( best->obj >= 0 && best->obj < M3D_MAXOBJECTS && m->objs[best->obj].used && best->v != 0 ) {
            obj_t* o = &m->objs[best->obj];
            if ( o->mref == 0 || --*o->mref <= 0 ) { free( o->v ); free( o->t ); free( o->mref ); }
            free( o->mf ); free( o->mc );
            o->mref = 0;
            o->mf = best->mf; o->mc = best->mc; o->ma = best->ma; o->mb = best->mb; best->mf = best->mc = 0;
            o->mx0 = best->x0; o->my0 = best->y0; o->mx1 = best->x0 + best->n * best->cell; o->my1 = best->y0 + best->n * best->cell; o->msat = 0; m->mforce = true;
            o->v = best->v; o->nv = best->nv; o->capv = best->capv;
            o->t = best->t; o->nt = best->nt; o->capt = best->capt;
            best->v = 0; best->t = 0; best->nv = best->nt = best->capv = best->capt = 0;
            o->dirty = true; o->gen = next_gen();
            for ( int i=0; i < best->nvar; ++i ) {
                int t = best->vars[i][0], k = best->vars[i][1], nn = t + k * m->sun.stride;
                bool ok = m->sun.on && nn < M3D_MAXTEX && ( m->texs[nn].col != 0 || sun_make( m, t, k ) );
                if ( !ok ) {                                            // no shaded picture (no palette yet, the sun is off): the cells keep their own picture
                    for ( int q = 0; q < o->nt; ++q ) { if ( o->t[q].tex == nn ) { o->t[q].tex = t; } }
                }
            }
            ++applied;
        }
        wl_built( m, best->obj );
        job_free_result( best );
        atomic_store( &best->state, 0 );
    }
    return applied;
}

static void jobs_wait( m3d_t* m ) {
    while ( jobs_running( m ) > 0 ) {
        struct timespec ts = { 0, 500000L };
        nanosleep( &ts, 0 );
    }
    jobs_apply( m );
}

static void jobs_discard( m3d_t* m ) {                         // forget everything that is queued or done (M3D CLEAR / RESET)
    tjobs_t* js = m->jobs;
    if ( js == 0 ) { return; }
    while ( jobs_running( m ) > 0 ) {
        struct timespec ts = { 0, 500000L };
        nanosleep( &ts, 0 );
    }
    for ( int i=0; i < TJOBS; ++i ) {
        if ( atomic_load( &js->job[i].state ) == 3 ) { job_free_result( &js->job[i] ); atomic_store( &js->job[i].state, 0 ); }
    }
}


void m3d_morph( m3d_t* m, double cx, double cy ) {
    if ( m->mvalid && !m->mforce && fabs( cx - m->mlx ) + fabs( cy - m->mly ) < 8.0 ) {      // (the weights change by a fraction of a percent: not every frame)
        return;
    }
    m->mlx = cx; m->mly = cy; m->mvalid = true; m->mforce = false;
    int nul_; const int* ul_ = m3d_used( m, &nul_ );
    for ( int ui_=0; ui_ < nul_; ++ui_ ) {
        int i = ul_[ui_];
        obj_t* o = &m->objs[i];
        if ( !o->used || o->mc == 0 || o->mf == 0 || !( o->mb > o->ma ) ) {
            continue;
        }
        // the Chebyshev distances of the nearest and the farthest corner of the mesh's rectangle: an object that lies wholly in w = 0 or in w = 1 and is so already is left alone
        double dxn = fmax( fmax( o->mx0 - cx, cx - o->mx1 ), 0.0 ), dyn = fmax( fmax( o->my0 - cy, cy - o->my1 ), 0.0 );
        double dmin = fmax( dxn, dyn ), dmax = fmax( fmax( fabs( o->mx0 - cx ), fabs( o->mx1 - cx ) ), fmax( fabs( o->my0 - cy ), fabs( o->my1 - cy ) ) );
        if ( ( dmax <= o->ma && o->msat == 1 ) || ( dmin >= o->mb && o->msat == 2 ) ) {
            continue;
        }
        double inv = 1.0 / ( o->mb - o->ma );
        for ( int k=0; k < o->nv; ++k ) {
            mv_t* p = &o->v[k];
            double d = fmax( fabs( p->x - cx ), fabs( p->y - cy ) ), w = smooth01( ( d - o->ma ) * inv );
            p->z = (double) o->mf[k] + ( (double) o->mc[k] - (double) o->mf[k] ) * w;
        }
        o->msat = dmax <= o->ma ? 1 : dmin >= o->mb ? 2 : 0;
        o->dirty = true; o->gen = next_gen();
    }
}


// -- the static world: a quadtree of ground tiles (docs/world-subsystem.md, M3D WORLDOPEN) --------------------------------------------------------------------------------------
/*
The ground of a static world (world.[ch]) is drawn by tiles of 8 x 8 cells of every size s = S0 * 2^level (S0 = a sixteenth of a chunk = 976.6 units, 3.9 m cells; the top level 6 is
62500 units): a quadtree on the chunk grid. A tile is split into its four children when the camera comes nearer than R = 3 s (Chebyshev distance to the tile's box), merged again
beyond 1.1 R. The grids are nested (a coarse cell is two fine ones), every tile morphs from its own grid into the grid of its parent between the distances a = 4.42 s and b = 6 s
(the morph of M3D RING, ratio 2), so:
  * a tile is split at dmin < R, when all its vertices are nearer than a (blend 0: its own grid) and the children have all vertices farther than R = 6 s_child = b (blend 1: the
    parent's grid): the surface is the same before and after, nothing pops; merging is the same the other way round (the children are blended wholly, dmin >= 1.1 R > b);
  * the children are built beforehand (nearer than 1.6 R) and a split waits until all four are there, so a slow worker thread never leaves a gap or shows a half made tile;
  * cracks between neighbours of different sizes are closed by skirts.
Every tile is one object (slot i = object obj0 + i) made by a ground job of mode 4 on the worker thread; the heights are `world_height()`, pure, the same every time.
*/
#define WL_TOP      6
#define WL_S0       ( WORLD_CHUNK / 16.0 )
#define WL_RF       3.0
#define WL_SLOTS    3072
#define WL_HASH     4096
#define WL_REQMAX   384
#define WL_TREE     100                      // the "level" of a cell of trees (size 4 S0 = 3906 units); the trees of a cell grow out of the ground between WL_TB - WL_TBAND and WL_TB from the camera
#define WL_TB       7000.0
#define WL_TBAND    700.0

typedef struct { bool used, dead, built, queued, split, shown; int lvl; int64_t tx, ty; unsigned stamp; } wtile_t;

typedef struct { int slot; double d; } wreq_t;

typedef struct _wl {
    world_t* w;
    int      obj0, flags, light;
    double   half;                           // half the world's side, units
    wtile_t  t[WL_SLOTS];
    int16_t  hash[WL_HASH];                  // slot + 1; 0 empty, -1 removed
    unsigned frame;
    int      nshown, nbuilt, nqueued, changed;
    wreq_t   req[WL_REQMAX];
    int      nreq;
    double   cx, cy;
} wl_t;

static double wl_size( int lvl ) { return lvl == WL_TREE ? 4.0 * WL_S0 : WL_S0 * (double)( 1 << lvl ); }

static unsigned wl_key( int lvl, int64_t tx, int64_t ty ) {
    return (unsigned)( nz_mix( (uint64_t) tx * 0x9E3779B97F4A7C15ULL + (uint64_t) ty * 0xC2B2AE3D27D4EB4FULL + (uint64_t) lvl ) % WL_HASH );
}

static int wl_find( const wl_t* wl, int lvl, int64_t tx, int64_t ty ) {
    unsigned h = wl_key( lvl, tx, ty );
    for ( int k = 0; k < WL_HASH; ++k ) {
        int e = wl->hash[ ( h + (unsigned) k ) % WL_HASH ];
        if ( e == 0 ) { return -1; }
        if ( e > 0 ) {
            const wtile_t* t = &wl->t[e - 1];
            if ( !t->dead && t->lvl == lvl && t->tx == tx && t->ty == ty ) { return e - 1; }
        }
    }
    return -1;
}

static void wl_unhash( wl_t* wl, int slot ) {
    unsigned h = wl_key( wl->t[slot].lvl, wl->t[slot].tx, wl->t[slot].ty );
    for ( int k = 0; k < WL_HASH; ++k ) {
        unsigned i = ( h + (unsigned) k ) % WL_HASH;
        if ( wl->hash[i] == slot + 1 ) { wl->hash[i] = -1; return; }
        if ( wl->hash[i] == 0 ) { return; }
    }
}

static int wl_add( m3d_t* m, wl_t* wl, int lvl, int64_t tx, int64_t ty ) {
    int slot = -1;
    for ( int i = 0; i < WL_SLOTS; ++i ) {
        if ( !wl->t[i].used ) { slot = i; break; }
    }
    if ( slot < 0 ) { return -1; }
    unsigned h = wl_key( lvl, tx, ty );
    int at = -1;
    for ( int k = 0; k < WL_HASH; ++k ) {
        unsigned i = ( h + (unsigned) k ) % WL_HASH;
        if ( wl->hash[i] <= 0 ) { at = (int) i; break; }
    }
    if ( at < 0 ) { return -1; }
    wtile_t* t = &wl->t[slot];
    memset( t, 0, sizeof(*t) );
    t->used = true; t->lvl = lvl; t->tx = tx; t->ty = ty;
    wl->hash[at] = (int16_t)( slot + 1 );
    int o = wl->obj0 + slot;                    // the object, empty and hidden until the job's mesh is taken
    free_obj( &m->objs[o] );
    m->ulok = false;
    if ( o >= m->top ) { m->top = o + 1; }
    obj_t* ob = &m->objs[o];
    ob->used = true; ob->shown = false; ob->dirty = true; ob->flags = wl->flags; ob->light = wl->light; ob->gen = next_gen(); ob->scale = 1.0; ob->cs = 1.0;
    return slot;
}

static void wl_release( m3d_t* m, wl_t* wl, int slot ) {       // the slot is free again (the object is emptied)
    free_obj( &m->objs[wl->obj0 + slot] );
    m->ulok = false;
    memset( &wl->t[slot], 0, sizeof(wl->t[slot]) );
}

static void wl_remove( m3d_t* m, wl_t* wl, int slot ) {        // a tile nobody wants any more; one whose job is still out stays as a hidden husk until the job is done
    wtile_t* t = &wl->t[slot];
    wl_unhash( wl, slot );
    if ( t->shown ) { wl->changed++; }
    if ( t->queued ) {
        t->dead = true; t->shown = false; t->split = false;
        m->objs[wl->obj0 + slot].shown = false;
    } else {
        wl_release( m, wl, slot );
    }
}

static void wl_built( m3d_t* m, int obj ) {                    // jobs_apply took the mesh for object obj
    wl_t* wl = m->wl;
    if ( wl == 0 || obj < wl->obj0 || obj >= wl->obj0 + WL_SLOTS ) { return; }
    int slot = obj - wl->obj0;
    wtile_t* t = &wl->t[slot];
    if ( !t->used ) { return; }
    t->queued = false;
    if ( t->dead ) { wl_release( m, wl, slot ); return; }
    t->built = true;
    wl->changed++;
}

static bool wl_request( m3d_t* m, wl_t* wl, int slot ) {
    wtile_t* t = &wl->t[slot];
    double s = wl_size( t->lvl );
    tjob_t* pj = (tjob_t*) calloc( 1, sizeof(tjob_t) );
    if ( pj == 0 ) { return false; }
    pj->obj = wl->obj0 + slot; pj->n = 8; pj->x0 = (double) t->tx * s; pj->y0 = (double) t->ty * s; pj->cell = s / 8.0; pj->skirt = pj->cell * 0.75 + 10.0; pj->mode = t->lvl == WL_TREE ? 5 : 4;
    if ( t->lvl == WL_TREE ) { pj->ma = WL_TB - WL_TBAND; pj->mb = WL_TB; }
    else if ( t->lvl < WL_TOP ) { pj->ma = ( WL_RF + 1.42 ) * s; pj->mb = 2.0 * WL_RF * s; }
    pj->gnd = *world_ground( wl->w ); pj->world = wl->w; pj->ratio = 2.0; pj->pic = m->rpic;
    pj->sunon = m->sun.on; pj->lx = m->sun.lx; pj->ly = m->sun.ly; pj->lz = m->sun.lz; pj->steps = m->sun.steps; pj->stride = m->sun.stride;
    bool ok = jobs_submit( m, pj );
    free( pj );
    if ( ok ) { t->queued = true; }
    return ok;
}

static void wl_want( wl_t* wl, int slot, double d ) {          // a tile that must be built: remembered, the nearest ones are submitted first
    if ( wl->nreq < WL_REQMAX ) { wl->req[wl->nreq].slot = slot; wl->req[wl->nreq].d = d; ++wl->nreq; }
}

static void wl_show( m3d_t* m, wl_t* wl, int slot, bool on ) {
    wtile_t* t = &wl->t[slot];
    if ( t->shown != on ) { t->shown = on; m->objs[wl->obj0 + slot].shown = on; wl->changed++; }
}

static double wl_dist( const wl_t* wl, int lvl, int64_t tx, int64_t ty ) {      // the Chebyshev distance from the camera to the tile's box
    double s = wl_size( lvl ), x0 = (double) tx * s, y0 = (double) ty * s;
    double dx = fmax( fmax( x0 - wl->cx, wl->cx - ( x0 + s ) ), 0.0 ), dy = fmax( fmax( y0 - wl->cy, wl->cy - ( y0 + s ) ), 0.0 );
    return fmax( dx, dy );
}

static bool wl_inworld( const wl_t* wl, int lvl, int64_t tx, int64_t ty ) {
    double s = wl_size( lvl ), x0 = (double) tx * s, y0 = (double) ty * s;
    return x0 < wl->half && y0 < wl->half && x0 + s > -wl->half && y0 + s > -wl->half;
}

// the tile (lvl, tx, ty) is wanted: found or made (and stamped), asked for when it has no mesh; -1 outside the world or no slot
static int wl_need( m3d_t* m, wl_t* wl, int lvl, int64_t tx, int64_t ty ) {
    if ( !wl_inworld( wl, lvl, tx, ty ) ) { return -1; }
    int sl = wl_find( wl, lvl, tx, ty );
    if ( sl < 0 ) { sl = wl_add( m, wl, lvl, tx, ty ); }
    if ( sl < 0 ) { return -1; }
    wtile_t* t = &wl->t[sl];
    t->stamp = wl->frame;
    if ( !t->built && !t->queued ) { wl_want( wl, sl, wl_dist( wl, lvl, tx, ty ) ); }
    return sl;
}

static void wl_visit( m3d_t* m, wl_t* wl, int lvl, int64_t tx, int64_t ty ) {
    int sl = wl_need( m, wl, lvl, tx, ty );
    if ( sl < 0 ) { return; }
    wtile_t* t = &wl->t[sl];
    double dmin = wl_dist( wl, lvl, tx, ty ), R = WL_RF * wl_size( lvl );
    if ( t->split ) {
        bool ok = lvl > 0 && dmin < 1.1 * R;
        for ( int q = 0; q < 4 && ok; ++q ) {                      // (a child that is not there: back to the parent, no gap)
            int c = wl_find( wl, lvl - 1, 2 * tx + ( q & 1 ), 2 * ty + ( q >> 1 ) );
            if ( c < 0 ? wl_inworld( wl, lvl - 1, 2 * tx + ( q & 1 ), 2 * ty + ( q >> 1 ) ) : !wl->t[c].built ) { ok = false; }
        }
        if ( ok ) {
            wl_show( m, wl, sl, false );
            for ( int q = 0; q < 4; ++q ) { wl_visit( m, wl, lvl - 1, 2 * tx + ( q & 1 ), 2 * ty + ( q >> 1 ) ); }
            return;
        }
        t->split = false; wl->changed++;                          // merge: the children are not visited and so are swept
    }
    wl_show( m, wl, sl, t->built );
    if ( lvl > 0 && dmin < 1.6 * R ) {                             // the children are made beforehand
        bool all = true;
        int cs[4];
        for ( int q = 0; q < 4; ++q ) {
            cs[q] = wl_need( m, wl, lvl - 1, 2 * tx + ( q & 1 ), 2 * ty + ( q >> 1 ) );
            if ( cs[q] >= 0 && !t->split ) { wl_show( m, wl, cs[q], false ); wl->t[cs[q]].split = false; }      // (kept for later, hidden: a stale split would show grandchildren that are gone)
            if ( cs[q] >= 0 && !wl->t[cs[q]].built ) { all = false; }
            if ( cs[q] < 0 && wl_inworld( wl, lvl - 1, 2 * tx + ( q & 1 ), 2 * ty + ( q >> 1 ) ) ) { all = false; }     // (no slot: wait)
        }
        if ( dmin < R && all && t->built ) {
            t->split = true; wl->changed++;
            wl_show( m, wl, sl, false );
            for ( int q = 0; q < 4; ++q ) { wl_visit( m, wl, lvl - 1, 2 * tx + ( q & 1 ), 2 * ty + ( q >> 1 ) ); }
        }
    }
}

static int wl_reqcmp( const void* a, const void* b ) {
    double da = ( (const wreq_t*) a )->d, db = ( (const wreq_t*) b )->d;
    return da < db ? -1 : da > db ? 1 : 0;
}

// every frame (M3D WORLDVIEW x, y): takes finished meshes, decides splits and merges, submits the jobs for what is wanted (the nearest first), morphs
static void wl_view( m3d_t* m, double x, double y ) {
    wl_t* wl = m->wl;
    if ( wl == 0 ) { return; }
    jobs_apply( m );
    wl->cx = x; wl->cy = y; ++wl->frame; wl->nreq = 0; wl->changed = 0;
    double st = wl_size( WL_TOP ), rad = ( 10.5 * WORLD_CHUNK );
    int64_t ax = (int64_t) floor( ( x - rad ) / st ), bx = (int64_t) floor( ( x + rad ) / st ), ay = (int64_t) floor( ( y - rad ) / st ), by = (int64_t) floor( ( y + rad ) / st );
    for ( int64_t ty = ay; ty <= by; ++ty ) {
        for ( int64_t tx = ax; tx <= bx; ++tx ) { wl_visit( m, wl, WL_TOP, tx, ty ); }
    }
    // the cells of trees: made at the far end of their range (the trees are still flat on the ground there), shown from the range on, dropped when far again
    if ( m->rpic.bark > 0 || m->rpic.posts || m->rpic.bales ) {
        double ts = wl_size( WL_TREE ), reach = WL_TB + 4.5 * ts;
        int64_t ax2 = (int64_t) floor( ( x - reach ) / ts ), bx2 = (int64_t) floor( ( x + reach ) / ts ), ay2 = (int64_t) floor( ( y - reach ) / ts ), by2 = (int64_t) floor( ( y + reach ) / ts );
        for ( int64_t ty = ay2; ty <= by2; ++ty ) {
            for ( int64_t tx = ax2; tx <= bx2; ++tx ) {
                double d = wl_dist( wl, WL_TREE, tx, ty );
                if ( d >= reach || !wl_inworld( wl, WL_TREE, tx, ty ) ) { continue; }
                int sl = d < WL_TB + 3.5 * ts ? wl_need( m, wl, WL_TREE, tx, ty ) : wl_find( wl, WL_TREE, tx, ty );
                if ( sl < 0 ) { continue; }
                wl->t[sl].stamp = wl->frame;
                wl_show( m, wl, sl, wl->t[sl].built && d < WL_TB );
            }
        }
    }
    wl->nshown = wl->nbuilt = wl->nqueued = 0;
    for ( int i = 0; i < WL_SLOTS; ++i ) {                        // what nobody visited goes (the children of a merged tile, tiles far away)
        wtile_t* t = &wl->t[i];
        if ( t->used && !t->dead && t->stamp != wl->frame ) { wl_remove( m, wl, i ); continue; }
        if ( t->used && !t->dead ) { wl->nshown += t->shown; wl->nbuilt += t->built; wl->nqueued += t->queued; }
    }
    qsort( wl->req, (size_t) wl->nreq, sizeof(wl->req[0]), wl_reqcmp );
    int budget = 24;
    for ( int i = 0; i < wl->nreq && budget > 0 && jobs_pending( m ) < 300; ++i ) {
        wtile_t* t = &wl->t[wl->req[i].slot];
        if ( t->used && !t->dead && !t->built && !t->queued && wl_request( m, wl, wl->req[i].slot ) ) { --budget; ++wl->nqueued; }
    }
    m3d_morph( m, x, y );
}

// waits until every wanted tile is there and the splits are done (the start of the game, a jump)
static void wl_settle( m3d_t* m ) {
    wl_t* wl = m->wl;
    if ( wl == 0 ) { return; }
    for ( int pass = 0; pass < 400; ++pass ) {
        wl_view( m, wl->cx, wl->cy );
        if ( wl->nreq == 0 && wl->nqueued == 0 && wl->changed == 0 && jobs_pending( m ) == 0 ) { break; }
        jobs_wait( m );
    }
}

static double wl_height( const m3d_t* m, double x, double y ) {
    return world_height( m->wl->w, x, y );
}

static void wl_close( m3d_t* m ) {
    if ( m->wl != 0 ) {
        wl_t* wl = m->wl;
        jobs_discard( m );
        for ( int i = 0; i < WL_SLOTS; ++i ) {
            if ( wl->t[i].used ) { free_obj( &m->objs[wl->obj0 + i] ); }
        }
        m->ulok = false;
        world_close( wl->w );
        free( wl );
        m->wl = 0;
    }
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
            if ( cells != 0 && (unsigned char) cells[ j * nx + i ] == 255U ) {            // a character 255: no cell here (the grid has a hole)
                continue;
            }
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
                if ( cells != 0 && (unsigned char) cells[ cell_j * nx + cell_i ] == 255U ) {
                    continue;
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
    int nul_; const int* ul_ = m3d_used( m, &nul_ );
    for ( int ui_=0; ui_ < nul_; ++ui_ ) {
        int i = ul_[ui_];
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

#define M3D_WALKABLE_NZ 0.3             // the smallest |normal z| of a triangle that does not push a body sideways (a slope of up to 72.5 degrees)

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
        int nul_; const int* ul_ = m3d_used( m, &nul_ );
    for ( int ui_=0; ui_ < nul_; ++ui_ ) {
        int i = ul_[ui_];
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
                {   // a triangle that is more floor than wall (its normal is within about 72 degrees of vertical) is no wall: the floor under the body and the step height
                    // decide whether it can be climbed (a hillside is not a wall until it is steeper than that)
                    v3_t e1 = vsub( b, a ), e2 = vsub( c, a ), nn = vcross( e1, e2 );
                    double nl = sqrt( vdot( nn, nn ) );
                    if ( nl > 1.0e-12 && fabs( nn.z ) / nl >= M3D_WALKABLE_NZ ) {
                        continue;
                    }
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

// With a ground function (M3D GROUND) a body cannot be lost: when there is no floor under it (the mesh is not there yet) the ground function is the floor, and a body
// that is buried in the ground (the mesh was made again under it) is lifted onto the first surface above it (or onto the ground function's height)
static double body_floor( m3d_t* m, double x, double y, double from, double feet ) {
    double fl = floor_at( m, x, y, from, 0 );
    if ( fl > -1.0e8 || !m->gnd.on ) {
        return fl;
    }
    rayhit_t h;
    v3_t o = { x, y, feet + 0.01 }, d = { 0, 0, 1 };
    if ( trace( m, o, d, 20000.0, M3D_OF_PASS, &h ) ) {
        return o.z + h.t;
    }
    return m3d_ground( m, x, y );
}

static void unbury( m3d_t* m, body_t* b ) {
    if ( !m->gnd.on ) {
        return;
    }
    double g = m3d_ground( m, b->x, b->y );
    if ( b->z - b->lift >= g - 60.0 ) {
        return;
    }
    rayhit_t h;
    v3_t o = { b->x, b->y, b->z - b->lift + 0.01 }, d = { 0, 0, 1 };
    b->z = ( trace( m, o, d, 20000.0, M3D_OF_PASS, &h ) ? o.z + h.t : g ) + b->lift;
}

static bool try_move( m3d_t* m, body_t* b, double nx, double ny ) {
    double feet = b->z;
    bool pushed = push_out( m, b, &nx, &ny, feet );
    double fl = body_floor( m, nx, ny, feet + b->step, feet );
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
    unbury( m, b );
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
    double fl = body_floor( m, b->x, b->y, b->z + b->step, b->z );
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
    int i1 = 0, i2, i3;
    double args[12];
    *err = "Illegal argument";
    for ( int i=0; i < 12; ++i ) {
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
            if ( !( a[0] >= 0.0 && a[0] <= 1.0e6 ) || ( !NONE_VAL( a[1] ) && !whole( a[1], 1, 255, &i1 ) ) || ( !NONE_VAL( a[2] ) && !( a[2] >= 0.0 && a[2] < a[0] ) ) ) { return false; }
            m->fogstart = NONE_VAL( a[2] ) ? 0.0 : a[2];
            m->fog = a[0];
            m->hazecol = NONE_VAL( a[1] ) ? -1 : i1;
            return true;
        case GFXOP_M3DHIDE:                                   // the window host hides the hardware renderer's layer (host_m3d_op); the model has nothing to do
            return true;
        case GFXOP_M3DMSAA:
            if ( !( a[0] >= 0.0 && a[0] <= 16.0 ) ) { return false; }
            i1 = (int) floor( a[0] + 0.5 );
            m->msaa = i1 < 2 ? 0 : i1;
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
            m->ulok = false;
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
        case GFXOP_M3DMOUNTAINS: case GFXOP_M3DHILLS: {
            int oct;
            mnt_t* mn = &m->gnd.mnt[op == GFXOP_M3DHILLS ? 1 : 0];
            double r0 = NONE_VAL( a[6] ) ? 0.0 : a[6], r1 = NONE_VAL( a[7] ) ? 0.0 : a[7];
            if ( !( fabs( a[0] ) < 4.4e9 ) || !( a[1] > 0.0 && a[1] < 1.0e9 ) || !( fabs( a[2] ) < 1.0e9 ) || !whole( a[3], 1, NZ_MAXOCTAVES, &oct ) || !( a[4] >= -1.0 && a[5] <= 1.0 && a[5] > a[4] ) ||
                 !( r0 >= 0.0 && r0 < 1.0e9 && r1 >= 0.0 && r1 < 1.0e9 ) || ( r1 > 0.0 && r1 <= r0 ) ) {
                return false;
            }
            mn->r0 = r0; mn->r1 = r1;
            mn->on = a[2] != 0.0; mn->seed = (uint64_t)(int64_t) floor( a[0] ); mn->scale = a[1]; mn->amp = a[2]; mn->oct = oct;
            mn->lo = a[4]; mn->hi = a[5];
            m3d_ground_sync_roads( &m->gnd );
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
        case GFXOP_M3DRINGPIC: {
            int pv[9];
            int dirtp = 0;
            for ( int k = 0; k < 9; ++k ) {
                if ( !whole( a[k], 0, M3D_MAXTEX - 1, &pv[k] ) ) { return false; }
            }
            if ( !( fabs( a[9] ) < 1.0e9 ) || !( fabs( a[10] ) < 1.0e9 ) || ( !NONE_VAL( a[11] ) && !whole( a[11], 0, M3D_MAXTEX - 1, &dirtp ) ) ) { return false; }
            ringpic_t* rp = &m->rpic;
            rp->water = pv[0]; rp->rock = pv[1]; rp->grass = pv[2]; rp->tar = pv[3]; rp->tarfar = pv[4]; rp->mud = pv[5]; rp->meadow = pv[6]; rp->field = pv[7]; rp->forest = pv[8];
            rp->level = a[9]; rp->rockh = a[10]; rp->dirt = dirtp;
            return true;
        }
        case GFXOP_M3DTREEPIC: {
            int tv[4];
            for ( int k = 0; k < 4; ++k ) {
                if ( !whole( a[k], 0, M3D_MAXTEX - 1, &tv[k] ) ) { return false; }
            }
            m->rpic.bark = tv[0]; m->rpic.leaf[0] = tv[1]; m->rpic.leaf[1] = tv[2]; m->rpic.leaf[2] = tv[3];
            return true;
        }
        case GFXOP_M3DCULL:
            if ( !( a[0] >= 0.0 && a[0] <= 100.0 ) || ( !NONE_VAL( a[1] ) && !( a[1] == 0.0 || a[1] == 1.0 ) ) || ( !NONE_VAL( a[2] ) && !( a[2] == 0.0 || a[2] == 1.0 ) ) ) {
                return false;
            }
            m->smallpx = a[0];
            if ( !NONE_VAL( a[1] ) ) { m->nearsort = a[1] != 0.0; }
            if ( !NONE_VAL( a[2] ) ) { m->occlude = a[2] != 0.0; }
            return true;
        case GFXOP_M3DPOSTPIC: {
            int bv = -1;
            if ( !NONE_VAL( a[3] ) && a[3] >= 0.0 && !whole( a[3], 0, M3D_MAXTEX - 1, &bv ) ) { return false; }
            m->rpic.bales = bv >= 0; m->rpic.bale = bv;
            if ( a[0] < 0.0 && a[1] < 0.0 && a[2] < 0.0 ) { m->rpic.posts = false; return true; }
            int pv[3];
            for ( int k = 0; k < 3; ++k ) {
                if ( !whole( a[k], 0, M3D_MAXTEX - 1, &pv[k] ) ) { return false; }
            }
            m->rpic.posts = true; m->rpic.post[0] = pv[0]; m->rpic.post[1] = pv[1]; m->rpic.post[2] = pv[2];
            return true;
        }
        case GFXOP_M3DMORPH:
            m3d_morph( m, a[0], a[1] );
            return true;
        case GFXOP_M3DPOLL:
            jobs_apply( m );
            return true;
        case GFXOP_M3DWORLDMAKE: {                           // M3D WORLDMAKE dir$, n, seed: the world is made on threads of its own (M3D("WORLDINFO", 5) is the progress)
            int n;
            char dir[1000];
            if ( !whole( a[0], 1, WORLD_MAXN, &n ) || !( fabs( a[1] ) < 9.0e15 ) || slen == 0 || slen >= sizeof(dir) ) { return false; }
            if ( m->wmake != 0 ) {
                if ( !world_make_done( m->wmake ) ) { *err = "A world is being made"; return false; }
                world_close( world_make_finish( m->wmake ) );
                m->wmake = 0;
            }
            memcpy( dir, s, slen ); dir[slen] = 0;
            m->wmake = world_make_start( dir, n, (uint64_t)(int64_t) floor( a[1] ), 0 );
            if ( m->wmake == 0 ) { *err = "Out of memory"; return false; }
            return true;
        }
        case GFXOP_M3DWORLDOPEN: {                           // M3D WORLDOPEN dir$, obj0 [, flags [, light]]: the world's tiles are the objects obj0 .. obj0 + 3071
            int fl = 0, li = 255, o0;
            char dir[1000];
            if ( !whole( a[0], 0, M3D_MAXOBJECTS - WL_SLOTS, &o0 ) || ( !NONE_VAL( a[1] ) && !whole( a[1], 0, 255, &fl ) ) || ( !NONE_VAL( a[2] ) && !whole( a[2], 0, 255, &li ) ) || slen == 0 || slen >= sizeof(dir) ) { return false; }
            if ( m->wmake != 0 ) {
                if ( !world_make_done( m->wmake ) ) { *err = "A world is being made"; return false; }
                world_close( world_make_finish( m->wmake ) );
                m->wmake = 0;
            }
            memcpy( dir, s, slen ); dir[slen] = 0;
            wl_close( m );
            world_t* w = world_open( dir );
            if ( w == 0 ) { *err = world_error(); return false; }
            wl_t* wl = (wl_t*) calloc( 1, sizeof(wl_t) );
            if ( wl == 0 ) { world_close( w ); *err = "Out of memory"; return false; }
            wl->w = w; wl->obj0 = o0; wl->flags = fl; wl->light = li; wl->half = 0.5 * (double) world_size( w ) * WORLD_CHUNK;
            m->wl = wl;
            m->gnd = *world_ground( w );                     // the queries about the roads and kinds of ground (M3D("KIND"), ("ROADLINK"), ...) answer for this world
            return true;
        }
        case GFXOP_M3DWORLDCLOSE:
            wl_close( m );
            return true;
        case GFXOP_M3DWORLDVIEW:
            if ( m->wl == 0 ) { *err = "No world open"; return false; }
            if ( !( fabs( a[0] ) < 1.0e9 && fabs( a[1] ) < 1.0e9 ) ) { return false; }
            wl_view( m, a[0], a[1] );
            return true;
        case GFXOP_M3DWORLDWAIT:
            if ( m->wl == 0 ) { *err = "No world open"; return false; }
            wl_settle( m );
            return true;
        case GFXOP_M3DRINGWAIT:
            jobs_wait( m );
            return true;
        case GFXOP_M3DRING: {
            int n;
            if ( !whole( a[0], 0, M3D_MAXOBJECTS - 1, &i1 ) || !obj_valid( m, i1 ) ) {
                *err = "Object not defined";
                return false;
            }
            double ma = NONE_VAL( a[10] ) ? 0.0 : a[10], mb = NONE_VAL( a[11] ) ? 0.0 : a[11];
            if ( !whole( a[3], 1, 128, &n ) || !( a[4] >= 1.0 && a[4] < 1.0e7 ) || !( a[5] >= 0.0 && a[5] < 1.0e7 ) || !whole( a[6], 0, 3, &i2 ) || !whole( a[9], 0, 128, &i3 ) ||
                 !( fabs( a[1] ) < 1.0e9 && fabs( a[2] ) < 1.0e9 && fabs( a[7] ) < 1.0e9 && fabs( a[8] ) < 1.0e9 ) || !( ma >= 0.0 && mb >= 0.0 && mb < 1.0e9 ) || ( mb > 0.0 && mb <= ma ) ) {
                return false;
            }
            tjob_t* pj = (tjob_t*) calloc( 1, sizeof(tjob_t) );
            if ( pj == 0 ) { *err = "Out of memory"; return false; }
            pj->obj = i1; pj->n = n; pj->x0 = a[1]; pj->y0 = a[2]; pj->cell = a[4]; pj->skirt = a[5]; pj->mode = i2; pj->hx0 = a[7]; pj->hy0 = a[8]; pj->hn = i3;
            pj->ma = ma; pj->mb = mb;
            pj->gnd = m->gnd; pj->pic = m->rpic;
            pj->sunon = m->sun.on; pj->lx = m->sun.lx; pj->ly = m->sun.ly; pj->lz = m->sun.lz; pj->steps = m->sun.steps; pj->stride = m->sun.stride;
            bool ok = jobs_submit( m, pj );
            free( pj );
            if ( !ok ) { *err = "Too many ground jobs"; return false; }
            return true;
        }
        case GFXOP_M3DROADNET: {
            if ( !( a[0] >= 0.0 && a[0] < 1.0e7 ) || !( a[1] >= 0.0 && a[1] <= 1.0 ) || !( a[2] >= 0.0 && a[2] <= 1.0e4 ) || !( a[3] >= 0.0 && a[3] < 1.0e7 ) || !( a[4] >= 0.0 && a[4] <= 1.0 ) ||
                 !( a[5] >= 0.0 && a[5] <= 1.0e4 ) ) {
                return false;
            }
            rnet_t* nt = &m->gnd.net;
            nt->on = a[0] > 0.0 || a[3] > 0.0; nt->spacing = a[0]; nt->chance = a[1]; nt->wdirt = a[2]; nt->tspacing = a[3]; nt->tchance = a[4]; nt->wtar = a[5];
            m3d_ground_sync_roads( &m->gnd );
            return true;
        }
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
                    if ( (unsigned char) s[k] != 255U && tex + (unsigned char) s[k] >= M3D_MAXTEX ) {       // (255: no cell here)
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
            m->ulok = false;
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
                free_obj( &m->objs[k] ); m->ulok = false;
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
        case GFXQ_M3DROADD: case GFXQ_M3DROADT: {
            rnear_t rn;
            rnet_near( &m->gnd.net, a[0], a[1], 700.0, &rn );
            *r = q == GFXQ_M3DROADD ? rn.dirt : rn.tar;
            return true;
        }
        case GFXQ_M3DROADLINK: case GFXQ_M3DROADINFO: case GFXQ_M3DROADPT: case GFXQ_M3DROADARC: {          // the roads by region: the road of network net (0 dirt, 1 tarred) from region (rx, ry) to its west (e = 0) / south (e = 1) neighbour
            rlink_t lk;
            bool has = rnet_cached_link( &m->gnd.net, a[0] != 0.0 ? 1 : 0, (int64_t) floor( a[1] ), (int64_t) floor( a[2] ), a[3] != 0.0 ? 1 : 0, &lk );
            if ( q == GFXQ_M3DROADLINK || !has ) {
                *r = has ? 1.0 : 0.0;
            } else if ( q == GFXQ_M3DROADINFO ) {              // what: 0 steps, 1 length, 2 the start is an intersection, 3 the end is, 4 how far the way gets from the straight line between the nodes
                int w = (int) floor( a[4] );
                *r = w == 0 ? (double) lk.steps : w == 1 ? lk.len : w == 2 ? ( lk.hub1 ? 1.0 : 0.0 ) : w == 3 ? ( lk.hub2 ? 1.0 : 0.0 ) : lk.wa;
            } else if ( q == GFXQ_M3DROADPT ) {                // at t (0 .. 1): what 0 x, 1 y
                double t = a[4] < 0.0 ? 0.0 : a[4] > 1.0 ? 1.0 : a[4], px, py;
                rnet_point( &lk, t, &px, &py );
                *r = a[5] == 0.0 ? px : py;
            } else {                                           // the length of the way up to t
                *r = rnet_arc( &lk, a[4] < 0.0 ? 0.0 : a[4] > 1.0 ? 1.0 : a[4] );
            }
            return true;
        }
        case GFXQ_M3DKIND: *r = rnet_kind( &m->gnd.net, a[0], a[1] ); return true;
        case GFXQ_M3DWORLDINFO: {                           // M3D("WORLDINFO", k): 0 open, 1 size n, 2 tiles shown, 3 tiles built, 4 tiles being made, 5 progress of WORLDMAKE (0 .. 1000, -1 failed or none), 6 1 when WORLDMAKE has ended
            int k = (int) a[0];
            const wl_t* wl = m->wl;
            switch ( k ) {
                case 0: *r = wl != 0; return true;
                case 1: *r = wl != 0 ? world_size( wl->w ) : 0; return true;
                case 2: *r = wl != 0 ? wl->nshown : 0; return true;
                case 3: *r = wl != 0 ? wl->nbuilt : 0; return true;
                case 4: *r = wl != 0 ? wl->nqueued : 0; return true;
                case 5: {
                    int tot = 1, dn = 0;
                    if ( m->wmake == 0 ) { *r = -1; return true; }
                    dn = world_make_progress( m->wmake, &tot );
                    *r = dn < 0 ? -1 : world_make_done( m->wmake ) ? 1000 : floor( 1000.0 * dn / (double)( tot > 0 ? tot : 1 ) );
                    return true;
                }
                case 6: *r = m->wmake != 0 && world_make_done( m->wmake ); return true;
                default: *r = 0; return true;
            }
        }
        case GFXQ_M3DWORLDCHUNK: {                          // M3D("WORLDCHUNK", x, y, f): the record of the chunk that holds the place: f 0 cx, 1 cy, 2 lowest, 3 highest, 4 mean height, 5 kind, 6 flags, 7 tarred links; -1 outside / no world
            wchunk_t c;
            int cx, cy, f = (int) a[2];
            *r = -1.0;
            if ( m->wl == 0 || !world_chunk_at( m->wl->w, a[0], a[1], &cx, &cy ) || !world_chunk( m->wl->w, cx, cy, &c ) ) { return true; }
            *r = f == 0 ? cx : f == 1 ? cy : f == 2 ? c.hmin : f == 3 ? c.hmax : f == 4 ? c.hmean : f == 5 ? c.kind : f == 6 ? c.flags : f == 7 ? c.ntar : -1.0;
            return true;
        }
        case GFXQ_M3DRINGBUSY: *r = jobs_pending( m ); return true;
        case GFXQ_M3DQUIET:
            *r = m->gnd.quiet.on ? 1.0 - box_weight( a[0], a[1], m->gnd.quiet.cx, m->gnd.quiet.cy, m->gnd.quiet.rx, m->gnd.quiet.ry, m->gnd.quiet.margin ) : 1.0;
            return true;
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
        case GFXQ_M3DCULL: {
            int k = (int) a[0];
            if ( a[0] != k || k < 0 || k > 6 ) { return false; }
            if ( m->rend != 0 && m->rend->drawn != 0 ) {                      // (the hardware renderer counts when asked)
                m->drawn = (int) m->rend->drawn( m, m->rdata );
            }
            *r = m->cull[k];
            return true;
        }
        case GFXQ_M3DDRAWN:
            if ( m->rend != 0 && m->rend->drawn != 0 ) {
                m->drawn = (int) m->rend->drawn( m, m->rdata );
            }
            *r = m->drawn;
            return true;
        case GFXQ_M3DRENDERER: *r = m->rend == m->hw && m->hw != 0 ? 1 : 0; return true;
        case GFXQ_M3DHWAVAIL: *r = m->hw != 0 ? 1 : 0; return true;
        case GFXQ_M3DMSAAMAX: *r = m->hw != 0 && m->hw->msaamax != 0 ? m->hw->msaamax() : 0; return true;
        case GFXQ_M3DMSAA: {
            int mx = m->hw != 0 && m->hw->msaamax != 0 ? m->hw->msaamax() : 0;
            *r = m->rend == m->hw && m->hw != 0 && m->msaa >= 2 && mx >= 2 ? ( m->msaa < mx ? m->msaa : mx ) : 0;
            return true;
        }
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
