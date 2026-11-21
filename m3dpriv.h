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
    bool argb;                                                    // the picture is 32 bit ARGB (m3d_render32()), not a palette bitmap (m3d_render())
    void* (*create)( m3d_t* m );                                  // the renderer's own state (0: out of memory)
    void  (*destroy)( m3d_t* m, void* rd );
    bool  (*resize)( m3d_t* m, void* rd, int bw, int bh );        // the picture becomes bw x bh (before m->bw / m->bh change); false: out of memory, nothing changed
    void  (*palette)( m3d_t* m, void* rd );                       // m->pal changed
    const void* (*render)( m3d_t* m, void* rd );                  // draws one frame; the picture (m->bw x m->bh pixels: bytes of palette indices, or uint32_t ARGB when `argb`) or 0
    long  (*drawn)( m3d_t* m, void* rd );                         // the number of triangles of the last frame, when the renderer does not count them while drawing (0: m->drawn is kept by render)
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
    uint32_t gen;                           // the generation of the mesh: a new number whenever the vertices or triangles change (a moved object keeps it): a renderer that keeps
                                            // the mesh in its own memory (m3dgl.c: a vertex buffer) builds it again only when this differs from the number it saw
    int     flags, light;
    mv_t*   v;  int nv, capv;
    mt_t*   t;  int nt, capt;
    int*    mref;                           // not 0: v and t are shared with other objects (M3D INSTANCE); *mref counts the sharers, the last one frees them (copy on write)
    double  scale;                          // the size (1 = as the mesh is; the picture coordinates do not change)
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

typedef struct { int w, h; uint8_t* col; unsigned gen; } tex_t;     // col[ u * h + v ]; gen: counts the times the picture was set (a hardware renderer uploads again when it changed)

typedef struct { double x, y, z, r, amount, dx, dy, dz, cc; bool spot; } lamp_t;     // spot: a cone along (dx,dy,dz) (unit length) with the cosine cc of its half angle

typedef struct { int id, tag; double x1, y1, x2, y2, z1, z2; } zone_t;

typedef struct { bool on; uint64_t seed; double scale, amp, lo, hi, r0, r1; int oct; } mnt_t;      // ridged noise where a slow noise is above lo .. hi

// the ground height function (M3D GROUND, GROUNDFLAT): base + amp * FBM(seed, x / scale, y / scale), times (1 - flatten) for every flat box
typedef struct {
    bool     on;
    uint64_t seed;
    double   scale, amp, base, gain, lac;
    int      oct;
    mnt_t    mnt[2];                                                                // mountains [0] and hills [1]
    struct { bool on; uint64_t seed; double scale, depth, lo, hi; } lake;           // lakes: the ground sinks by depth where a noise is above lo .. hi
    struct { bool on; double cx, cy, rx, ry, margin; } flat[8];
    struct { bool on; double cx, cy, rx, ry, margin; } quiet;                       // mountains and lakes fade out towards this box
} ground_t;

// M3D SUN: terrain cells get a picture shaded by their slope. A shaded picture is made from the cell's own picture t (palette indices mapped to the nearest
// darker color) and has the number t + k * stride for the shade step k = 1 .. steps - 1; lvl[n] is that k for a picture the engine made (0: not made), src[n] the t.
typedef struct {
    bool     on;
    double   lx, ly, lz, ambient;           // the direction to the sun (unit vector, lz > 0) and the brightness of the darkest step (0 .. 1)
    int      steps, stride;
    uint16_t src[M3D_MAXTEX];
    uint8_t  lvl[M3D_MAXTEX];
} sun_t;

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
    int      hazecol;                       // M3D FOG d, haze: the palette index the fog fades to (-1: the fog darkens)
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
    ground_t gnd;
    sun_t    sun;
    int      top;                           // one above the highest object number in use (the loops stop there)
    unsigned presepoch;                     // counts the times a picture number got or lost its picture (the renderer's kept meshes group the triangles by that)
};

// the palette index nearest to color i of the palette darkened to 'scale' (0 .. 1); used by the software color map and by the shaded terrain pictures
int m3d_shade_index( const uint32_t* argb, int i, double scale );
// the palette index (1 .. 255) nearest to the color (r, g, b: 0 .. 255) with the weights of the shading; used by the fog's haze table
int m3d_nearest_rgb( const uint32_t* argb, double r, double g, double b );

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
