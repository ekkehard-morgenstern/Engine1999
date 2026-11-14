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



#include "m3d.h"
#include "gfxops.h"

// -- types ----------------------------------------------------------------------------------------------------------------------

#define NONE_VAL(v)     ( (v) < -1.0e8 )
#define DEG2RAD         0.017453292519943295
#define NEARZ           2.0

typedef struct { double x, y, z; } v3_t;

typedef struct {
    double x, y, z;                         // local coordinates
    double u, v;                            // picture coordinates (texels); GFX_NONE: derived from the face
} mv_t;

typedef struct {
    int a, b, c, tex;
    double u[3], v[3];
} mt_t;

typedef struct {
    bool    used, shown, dirty;
    int     flags, light;
    mv_t*   v;  int nv, capv;
    mt_t*   t;  int nt, capt;
    double  px, py, pz, yaw, cs, sn;
    v3_t*   wv;                             // world coordinates of the vertices (valid when !dirty)
    double  minx, miny, minz, maxx, maxy, maxz;
    double  cx, cy, cz, rad;                // bounding sphere
} obj_t;

typedef struct {
    bool on, trans;
    int  tex, light;
    double x, y, z, w, h;
} thing_t;

typedef struct { int w, h; uint8_t* col; } tex_t;     // col[ u * h + v ]

typedef struct { double x, y, z, r, amount; } lamp_t;

typedef struct { int id, tag; double x1, y1, x2, y2, z1, z2; } zone_t;

struct _m3d_t {
    tex_t*   texs;
    obj_t*   objs;     int nobjs;
    thing_t* things;
    lamp_t   lamps[M3D_MAXLAMPS];
    zone_t*  zones;    int nzones;
    uint32_t pal[256];
    bool     havepal;
    uint8_t  cmap[M3D_LEVELS][256];
    uint8_t* blend;
    int      vx, vy, vw, vh, scale, bw, bh;
    double   fov, fog, far;
    uint8_t* fb;  float* zb;
    int      skytex;  double skyrep, skyshift;
    // the player
    double   x, y, z, yaw, pitch, lift;
    double   radius, height, eye, step;
    int      hit;
    // the last shot
    int      shotkind, shotobj;
    double   shotdist, shotx, shoty, shotz;
    int      drawn;
    // frame scratch
    int*     order;  double* odist;
};

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

static int imod( int a, int n ) {
    int r = a % n;
    return r < 0 ? r + n : r;
}

static int iclamp( int v, int lo, int hi ) {
    return v < lo ? lo : v > hi ? hi : v;
}

static double eye_z( const m3d_t* m ) {
    return m->z + m->eye;
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
    free( m->blend );
    m->blend = 0;
    for ( int l=0; l < M3D_LEVELS; ++l ) {
        double scale = (double) l / (double)( M3D_LEVELS - 1 );
        m->cmap[l][0] = 0;
        for ( int i=1; i < 256; ++i ) {
            if ( l == M3D_LEVELS - 1 ) {
                m->cmap[l][i] = (uint8_t) i;
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
            m->cmap[l][i] = (uint8_t) bi;
        }
    }
}

static void make_blend( m3d_t* m ) {
    if ( m->blend != 0 || !m->havepal ) {
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
    m->blend = bl;
}

// -- lifetime -------------------------------------------------------------------------------------------------------------------

static void free_obj( obj_t* o ) {
    free( o->v ); free( o->t ); free( o->wv );
    memset( o, 0, sizeof(*o) );
}

static void clear_world( m3d_t* m ) {
    for ( int i=0; i < M3D_MAXOBJECTS; ++i ) {
        free_obj( &m->objs[i] );
    }
    memset( m->things, 0, sizeof(thing_t) * M3D_MAXTHINGS );
    memset( m->lamps, 0, sizeof(m->lamps) );
    m->nobjs = 0;
    m->nzones = 0;
}

static bool set_view( m3d_t* m, int x, int y, int w, int h, int sc, double fov ) {
    int bw = w / sc, bh = h / sc;
    uint8_t* fb = (uint8_t*) calloc( (size_t) bw * (size_t) bh, 1 );
    float* zb = (float*) calloc( (size_t) bw * (size_t) bh, sizeof(float) );
    if ( fb == 0 || zb == 0 ) {
        free( fb ); free( zb );
        return false;
    }
    free( m->fb ); free( m->zb );
    m->fb = fb; m->zb = zb;
    m->vx = x; m->vy = y; m->vw = w; m->vh = h; m->scale = sc; m->bw = bw; m->bh = bh; m->fov = fov;
    return true;
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
    free( m->zones );
    free( m->texs ); free( m->objs ); free( m->things ); free( m->order ); free( m->odist ); free( m->fb ); free( m->zb ); free( m->blend );
    free( m );
}

int m3d_nverts( const m3d_t* m, int o ) {
    return o >= 0 && o < M3D_MAXOBJECTS && m->objs[o].used ? m->objs[o].nv : 0;
}

void m3d_getview( const m3d_t* m, int* x, int* y, int* width, int* height, int* scale, int* bmpw, int* bmph ) {
    *x = m->vx; *y = m->vy; *width = m->vw; *height = m->vh; *scale = m->scale; *bmpw = m->bw; *bmph = m->bh;
}

// -- objects --------------------------------------------------------------------------------------------------------------------

static bool obj_valid( const m3d_t* m, int o ) {
    return o >= 0 && o < M3D_MAXOBJECTS && m->objs[o].used;
}

static bool obj_addv( obj_t* o, double x, double y, double z, double u, double v ) {
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
    return true;
}

static void obj_update( obj_t* o ) {
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
    o->minx = o->miny = o->minz = 1.0e30; o->maxx = o->maxy = o->maxz = -1.0e30;
    for ( int i=0; i < o->nv; ++i ) {
        const mv_t* p = &o->v[i];
        double x = o->px + o->cs * p->x - o->sn * p->y, y = o->py + o->sn * p->x + o->cs * p->y, z = o->pz + p->z;
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
    free( m->texs[n].col );
    m->texs[n].col = col; m->texs[n].w = tw; m->texs[n].h = th;
    return true;
}

static const tex_t* tex_get( const m3d_t* m, int n ) {
    return n >= 0 && n < M3D_MAXTEX && m->texs[n].col != 0 ? &m->texs[n] : 0;
}

// -- geometry: rays and closest points -----------------------------------------------------------------------------------------

static v3_t vsub( v3_t a, v3_t b ) { v3_t r = { a.x - b.x, a.y - b.y, a.z - b.z }; return r; }
static v3_t vadd( v3_t a, v3_t b ) { v3_t r = { a.x + b.x, a.y + b.y, a.z + b.z }; return r; }
static v3_t vmul( v3_t a, double s ) { v3_t r = { a.x * s, a.y * s, a.z * s }; return r; }
static double vdot( v3_t a, v3_t b ) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static v3_t vcross( v3_t a, v3_t b ) { v3_t r = { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; return r; }

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
    for ( int i=0; i < M3D_MAXOBJECTS; ++i ) {
        obj_t* ob = &m->objs[i];
        if ( !ob->used || !ob->shown || ob->nt == 0 || ( ob->flags & skip ) != 0 ) {
            continue;
        }
        obj_update( ob );
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
static double floor_at( m3d_t* m, double x, double y, double z ) {
    v3_t o = { x, y, z }, d = { 0, 0, -1 };
    rayhit_t h;
    // the ray starts exactly on a floor sometimes: lift it a little
    o.z += 0.01;
    if ( trace( m, o, d, 20000.0, M3D_OF_PASS, &h ) ) {
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
        for ( int i=0; i < M3D_MAXOBJECTS; ++i ) {
            obj_t* ob = &m->objs[i];
            if ( !ob->used || !ob->shown || ob->nt == 0 || ( ob->flags & M3D_OF_PASS ) != 0 ) {
                continue;
            }
            obj_update( ob );
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
    double fl = floor_at( m, nx, ny, feet + b->step );
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
    double fl = floor_at( m, b->x, b->y, b->z + b->step );
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
    double args[9];
    *err = "Illegal argument";
    for ( int i=0; i < 9; ++i ) {
        args[i] = i < na ? a[i] : GFX_NONE;             // arguments beyond na are omitted
        if ( i < na && !NONE_VAL( args[i] ) && !num_ok( args[i] ) && op != GFXOP_M3DTEXTURE ) {
            return false;
        }
    }
    a = args;
    switch ( op ) {
        case GFXOP_M3DRESET:
            clear_world( m );
            for ( int i=0; i < M3D_MAXTEX; ++i ) {
                free( m->texs[i].col );
                m->texs[i].col = 0;
            }
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
            o->used = true; o->shown = true; o->dirty = true; o->flags = fl; o->light = li;
            o->px = px; o->py = py; o->pz = pz; o->yaw = yaw; o->cs = cos( yaw * DEG2RAD ); o->sn = sin( yaw * DEG2RAD );
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
            double fl = floor_at( m, m->x, m->y, m->z - m->lift + m->step );
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
            l->x = a[1]; l->y = a[2]; l->z = a[3]; l->r = a[4]; l->amount = a[5];
            return true;
        }
        default:
            *err = "Not implemented";
            return false;
    }
}

// -- rendering ------------------------------------------------------------------------------------------------------------------

typedef struct { double x, y, z, u, v; } cv_t;      // view space: x right, y up, z forward

typedef struct {
    m3d_t* m;
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
    const uint8_t* cm = m->cmap[M3D_LEVELS - 1];
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
                        cm = m->cmap[li];
                        uint8_t out = cm[col];
                        if ( translucent ) {
                            out = m->blend != 0 ? m->blend[ (size_t) fb[x] * 256U + out ] : out;
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
        const uint8_t* cm = m->cmap[ iclamp( (int)( lv + 0.5 ), 0, M3D_LEVELS - 1 ) ];
        if ( t->trans ) { make_blend( m ); }
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
                if ( t->trans && m->blend != 0 ) {
                    out = m->blend[ (size_t) *p * 256U + out ];
                }
                *p = out;
            }
        }
    }
}

const uint8_t* m3d_render( m3d_t* m ) {
    if ( m->fb == 0 ) {
        return 0;
    }
    rc_t c;
    memset( &c, 0, sizeof(c) );
    c.m = m; c.fb = m->fb; c.zb = m->zb; c.W = m->bw; c.H = m->bh;
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
        m->zb[i] = 1.0e30f;
    }
    int nt = 0;
    for ( int i=0; i < M3D_MAXOBJECTS; ++i ) {
        obj_t* o = &m->objs[i];
        if ( !o->used || !o->shown || o->nt == 0 ) {
            continue;
        }
        obj_update( o );
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
        make_blend( m );
        sort_desc( m->order, m->odist, nt );
        for ( int k=0; k < nt; ++k ) {
            draw_object( &c, &m->objs[m->order[k]], true );
        }
    }
    return m->fb;
}

// -- queries --------------------------------------------------------------------------------------------------------------------

bool m3d_query( m3d_t* m, int q, const double* a, int na, double* r ) {
    (void) na;
    int i1;
    *r = 0;
    switch ( q ) {
        case GFXQ_M3DX: *r = m->x; return true;
        case GFXQ_M3DY: *r = m->y; return true;
        case GFXQ_M3DZ: *r = m->z; return true;
        case GFXQ_M3DEYE: *r = eye_z( m ); return true;
        case GFXQ_M3DYAW: *r = m->yaw; return true;
        case GFXQ_M3DPITCH: *r = m->pitch; return true;
        case GFXQ_M3DLIFT: *r = m->lift; return true;
        case GFXQ_M3DHIT: *r = m->hit; return true;
        case GFXQ_M3DFLOORZ: *r = floor_at( m, m->x, m->y, m->z + m->step ); return true;
        case GFXQ_M3DFLOORAT: *r = floor_at( m, a[0], a[1], a[2] ); return true;
        case GFXQ_M3DCEILAT: *r = ceil_at( m, a[0], a[1], a[2] ); return true;
        case GFXQ_M3DDRAWN: *r = m->drawn; return true;
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
