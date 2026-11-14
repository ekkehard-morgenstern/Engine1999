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

#ifndef M3DPRIV_H
#define M3DPRIV_H   1

#include "m3d.h"

/*
Private to m3d.c and the renderers (m3dsoft.c, later m3dgl.c): the world model and the interface between the model and a renderer. A renderer draws
the model's current state (objects, things, lamps, camera, fog, sky, textures, palette) into a picture; the model keeps everything that behavior
depends on (collision, queries), so it is the same whatever renderer is in use.
*/

typedef struct _m3drend_t {
    const char* name;
    void* (*create)( m3d_t* m );                                  // the renderer's own state (0: out of memory)
    void  (*destroy)( m3d_t* m, void* rd );
    bool  (*resize)( m3d_t* m, void* rd, int bw, int bh );        // the picture becomes bw x bh (before m->bw / m->bh change); false: out of memory, nothing changed
    void  (*palette)( m3d_t* m, void* rd );                       // m->pal changed
    const uint8_t* (*render)( m3d_t* m, void* rd );               // draws one frame; the bitmap (m->bw x m->bh palette indices) or 0
} m3drend_t;

extern const m3drend_t m3d_soft_renderer;

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
    int      vx, vy, vw, vh, scale, bw, bh;
    double   fov, fog, far;
    const m3drend_t* hw;                       // the hardware renderer the host offered, or 0
    const m3drend_t* rend;  void* rdata;       // the renderer in use (m3dsoft.c: the software one)
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
    int      top;                           // one above the highest object number in use (the loops stop there)
};

// -- shared helpers -------------------------------------------------------------------------------------------------------------

static inline int imod( int a, int n ) {
    int r = a % n;
    return r < 0 ? r + n : r;
}

static inline int iclamp( int v, int lo, int hi ) {
    return v < lo ? lo : v > hi ? hi : v;
}

static inline double eye_z( const m3d_t* m ) {
    return m->z + m->eye;
}

static inline const tex_t* tex_get( const m3d_t* m, int n ) {
    return n >= 0 && n < M3D_MAXTEX && m->texs[n].col != 0 ? &m->texs[n] : 0;
}

static inline v3_t vsub( v3_t a, v3_t b ) { v3_t r = { a.x - b.x, a.y - b.y, a.z - b.z }; return r; }
static inline v3_t vadd( v3_t a, v3_t b ) { v3_t r = { a.x + b.x, a.y + b.y, a.z + b.z }; return r; }
static inline v3_t vmul( v3_t a, double s ) { v3_t r = { a.x * s, a.y * s, a.z * s }; return r; }
static inline double vdot( v3_t a, v3_t b ) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline v3_t vcross( v3_t a, v3_t b ) { v3_t r = { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; return r; }

void m3d_obj_update( obj_t* o );                                  // world coordinates of the vertices, bounds (m3d.c)

#endif
