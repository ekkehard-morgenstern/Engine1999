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


#include "w3d.h"
#include "gfxops.h"

// -- the world ----------------------------------------------------------------------------------------

typedef struct _wall_t {
    double  x1, y1, x2, y2, len;
    int     tex, upper, lower;          // texture numbers (middle, above and below a portal)
    int     next;                       // the sector behind it, -1 for a solid wall
    double  xoff, yoff;
    int     flags;
} wall_t;

typedef struct _sector_t {
    bool    used;
    double  fz, cz;                     // the heights at the sector's slope origin (sox, soy)
    double  fgx, fgy, cgx, cgy;         // slopes of floor and ceiling: rise per unit in x and y (all 0: flat)
    double  sox, soy;
    bool    sloped;
    int     ftex, ctex;                 // ctex -1 = sky
    int     light, tag;
    int     elight;                     // the light of the frame (light effects applied)
    int     fxmode;                     // 0 none, 1 flicker, 2 pulse, 3 strobe
    double  fxamt, fxrate;
    double  tfz, tcz, mspeed;           // a mover: the heights it moves to, units per second
    bool    moving;
    double  fsx, fsy, csx, csy, wsx, wsy;           // scrolling speeds (units per second) of the floor, ceiling and walls
    double  fox, foy, cox, coy, wox, woy;           // ... and the offsets they have reached
    wall_t* walls;
    int     nwalls, cap;
    double  minx, miny, maxx, maxy;
} sector_t;

typedef struct _tex_t {
    int         w, h;
    uint8_t*    col;                    // column by column: col[ u * h + v ], row 0 at the top
} tex_t;

typedef struct _thing_t {
    bool    used;
    bool    trans;
    double  x, y, zoff, w, h;
    int     tex, sector, light;         // light -1: the sector's
} thing_t;

typedef struct _lamp_t {
    bool    on;
    double  x, y, z, radius, amount;
} lamp_t;

#define W3D_MAXLAMPS    8
#define MAXTRANS        4

struct _w3d_t {
    sector_t*   sectors;
    int         nsectors, capsectors;   // nsectors = the highest number in use + 1
    tex_t*      texs;                   // W3D_MAXTEX entries
    thing_t*    things;                 // W3D_MAXTHINGS entries
    int         skytex;
    double      skyrep, skyshift;       // how often the sky picture goes around, and its vertical shift in rows
    double      farz;                   // draw distance (0 = unlimited)
    double      time;                   // the world's clock (W3D TICK)
    int         animfirst[W3D_MAXTEX], animcount[W3D_MAXTEX];   // animated textures: texture n shows frames first .. first + count - 1
    double      animfps[W3D_MAXTEX];
    lamp_t      lamps[W3D_MAXLAMPS];
    int         nlamps;                 // the highest lamp in use + 1
    uint32_t    pal[256];
    bool        havepal;
    bool        usetrans;               // something translucent exists (the blend table is made)
    uint8_t*    blend;                  // 256 x 256: the palette entry nearest to the mean of two (made when the first translucent thing is drawn)
    struct _tcol_t* trans;              // translucent walls of the frame: MAXTRANS per column, far to near
    int*        ntrans;
    int         transcap;
    int         stat_visited;           // sectors the last frame reached
    double      cx, cy, angle, pitch, lift, pz;
    int         csector;
    bool        pzvalid;
    double      pr, ph, peye, pstep;    // the player
    int         vx, vy, vw, vh, scale, bw, bh;
    double      fov, fog;
    uint8_t*    fb;
    float*      zb;
    size_t      fbsize;
    uint8_t     cmap[W3D_LEVELS][256];
    int         hit;
    int         shotkind;
    double      shotdist, shotx, shoty;
    int*        order;                  // scratch of the sprite sort
    uint8_t*    visited;                // per sector: did a column of this frame get into it? (things in other sectors are not drawn: they could show over the sky above a low wall)
    int         visitcap;
};

#define NONE_VAL(v)     ( (v) < -1.0e8 )
#define DEG2RAD         ( 3.14159265358979323846 / 180.0 )

static int imin( int a, int b ) { return a < b ? a : b; }
static int imax( int a, int b ) { return a > b ? a : b; }
static int iclamp( int v, int lo, int hi ) { return v < lo ? lo : ( v > hi ? hi : v ); }
static double dmin( double a, double b ) { return a < b ? a : b; }
static double dmax( double a, double b ) { return a > b ? a : b; }

// a rounded-up row index of a screen coordinate, kept inside a range that cannot overflow
// a double as an int inside lo .. hi (NaN and huge values land on the ends)
static int dclamp( double v, int lo, int hi ) {
    if ( !( v >= (double) lo ) ) {
        return lo;
    }
    return v >= (double) hi ? hi : (int) v;
}

static int ceilrow( double y ) {
    if ( y < -1.0e6 ) y = -1.0e6;
    if ( y > 1.0e6 ) y = 1.0e6;
    return (int) ceil( y );
}

static void default_colormap( w3d_t* w ) {
    for ( int l=0; l < W3D_LEVELS; ++l ) {
        for ( int i=0; i < 256; ++i ) {
            w->cmap[l][i] = (uint8_t) i;
        }
    }
}

static bool set_view( w3d_t* w, int x, int y, int vw, int vh, int scale, double fov ) {
    int bw = vw / scale, bh = vh / scale;
    size_t need = (size_t) bw * (size_t) bh;
    if ( need == 0 ) {
        return false;
    }
    if ( need > w->fbsize ) {
        uint8_t* nfb = (uint8_t*) realloc( w->fb, need );
        if ( nfb == 0 ) {
            return false;
        }
        w->fb = nfb;
        float* nzb = (float*) realloc( w->zb, need * sizeof(float) );
        if ( nzb == 0 ) {
            return false;
        }
        w->zb = nzb;
        w->fbsize = need;
    }
    memset( w->fb, 0, need );                   // (new memory is set before anything reads it)
    for ( size_t i=0; i < need; ++i ) {
        w->zb[i] = 1.0e30f;
    }
    w->vx = x; w->vy = y; w->vw = vw; w->vh = vh; w->scale = scale; w->bw = bw; w->bh = bh; w->fov = fov;
    return true;
}

static void free_world( w3d_t* w ) {
    for ( int i=0; i < w->nsectors; ++i ) {
        free( w->sectors[i].walls );
    }
    free( w->sectors );
    w->sectors = 0; w->nsectors = 0; w->capsectors = 0;
    for ( int i=0; i < W3D_MAXTEX; ++i ) {
        free( w->texs[i].col );
    }
    memset( w->texs, 0, sizeof(tex_t) * W3D_MAXTEX );
    memset( w->things, 0, sizeof(thing_t) * W3D_MAXTHINGS );
}

static void reset_state( w3d_t* w ) {
    w->usetrans = false;
    w->time = 0.0; memset( w->animcount, 0, sizeof(w->animcount) ); memset( w->lamps, 0, sizeof(w->lamps) ); w->nlamps = 0;
    w->skytex = 1; w->skyrep = 1.0; w->skyshift = 0.0; w->farz = 0.0; w->stat_visited = 0;
    w->cx = w->cy = w->angle = w->pitch = w->lift = w->pz = 0.0;
    w->csector = -1;
    w->pzvalid = false;
    w->pr = 14.0; w->ph = 56.0; w->peye = 46.0; w->pstep = 24.0;
    w->fog = 1200.0;
    w->hit = 0; w->shotkind = 0; w->shotdist = w->shotx = w->shoty = 0.0;
}

w3d_t* w3d_new( void ) {
    w3d_t* w = (w3d_t*) calloc( 1, sizeof(w3d_t) );
    if ( w == 0 ) {
        return 0;
    }
    w->texs = (tex_t*) calloc( W3D_MAXTEX, sizeof(tex_t) );
    w->things = (thing_t*) calloc( W3D_MAXTHINGS, sizeof(thing_t) );
    w->order = (int*) calloc( W3D_MAXTHINGS, sizeof(int) );
    reset_state( w );
    default_colormap( w );
    if ( w->texs == 0 || w->things == 0 || w->order == 0 || !set_view( w, 0, 0, 640, 300, 2, 90.0 ) ) {
        w3d_free( w );
        return 0;
    }
    return w;
}

void w3d_free( w3d_t* w ) {
    if ( w == 0 ) {
        return;
    }
    if ( w->texs != 0 && w->things != 0 ) {
        free_world( w );
    }
    free( w->texs ); free( w->things ); free( w->order ); free( w->fb ); free( w->zb ); free( w->visited ); free( w->blend ); free( w->trans ); free( w->ntrans );
    free( w );
}

static void w3d_reset( w3d_t* w ) {
    free_world( w );
    reset_state( w );
    default_colormap( w );
}

void w3d_getview( const w3d_t* w, int* x, int* y, int* width, int* height, int* scale, int* bmpw, int* bmph ) {
    *x = w->vx; *y = w->vy; *width = w->vw; *height = w->vh; *scale = w->scale; *bmpw = w->bw; *bmph = w->bh;
}

// -- colors ---------------------------------------------------------------------------------------------

void w3d_colormap( w3d_t* w, const uint32_t* argb ) {
    memcpy( w->pal, argb, sizeof(w->pal) );
    w->havepal = true;
    free( w->blend );
    w->blend = 0;
    for ( int l=0; l < W3D_LEVELS; ++l ) {
        double scale = (double) l / (double)( W3D_LEVELS - 1 );
        w->cmap[l][0] = 0;
        for ( int i=1; i < 256; ++i ) {
            if ( l == W3D_LEVELS - 1 ) {
                w->cmap[l][i] = (uint8_t) i;
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
                // (plus the difference in tint: a dark gray must not turn into a dark sand color just because the ramp of grays is coarse there)
                double cr = ( (double)( ( argb[j] >> 16 ) & 0xFFU ) - (double)( ( argb[j] >> 8 ) & 0xFFU ) ) - ( tr - tg );
                double cb = ( (double)( argb[j] & 0xFFU ) - (double)( ( argb[j] >> 8 ) & 0xFFU ) ) - ( tb - tg );
                double d = 2.0 * dr * dr + 4.0 * dg * dg + 3.0 * db * db + 6.0 * cr * cr + 6.0 * cb * cb;
                if ( d < best ) {
                    best = d;
                    bi = j;
                }
            }
            w->cmap[l][i] = (uint8_t) bi;
        }
    }
}

static inline int lightlevel( const w3d_t* w, int light, double dist ) {
    double f = 1.0;
    if ( w->fog > 0.0 ) {
        f = 1.0 - dist / w->fog;
        if ( f < 0.0 ) {
            f = 0.0;
        }
    }
    int lv = (int)( (double) light * f * (double)( W3D_LEVELS - 1 ) / 255.0 + 0.5 );
    return lv < 0 ? 0 : ( lv >= W3D_LEVELS ? W3D_LEVELS - 1 : lv );
}

// the light that the lamps add at a point (0 .. 255)
static int lamp_light( const w3d_t* w, double x, double y, double z ) {
    double sum = 0.0;
    for ( int i=0; i < w->nlamps; ++i ) {
        const lamp_t* l = &w->lamps[i];
        if ( !l->on ) {
            continue;
        }
        double dx = x - l->x, dy = y - l->y, dz = z - l->z;
        double d2 = dx * dx + dy * dy + dz * dz;
        if ( d2 < l->radius * l->radius ) {
            sum += l->amount * ( 1.0 - sqrt( d2 ) / l->radius );
        }
    }
    return sum > 255.0 ? 255 : (int) sum;
}

static const uint8_t* blend_get( const w3d_t* w ) {
    return w->blend;
}

// the table of mixes: blend[ a * 256 + b ] = the entry nearest to the mean color of entries a and b
static void make_blend( w3d_t* w ) {
    if ( w->blend != 0 || !w->havepal ) {
        return;
    }
    uint8_t* bl = (uint8_t*) malloc( 65536U );
    if ( bl == 0 ) {
        return;
    }
    for ( int a=0; a < 256; ++a ) {
        for ( int b=a; b < 256; ++b ) {
            int r = (int)( ( ( w->pal[a] >> 16 ) & 0xFFU ) + ( ( w->pal[b] >> 16 ) & 0xFFU ) ) / 2;
            int g = (int)( ( ( w->pal[a] >> 8 ) & 0xFFU ) + ( ( w->pal[b] >> 8 ) & 0xFFU ) ) / 2;
            int bb = (int)( ( w->pal[a] & 0xFFU ) + ( w->pal[b] & 0xFFU ) ) / 2;
            int best = 0, bd = 1 << 30;
            for ( int j=1; j < 256; ++j ) {
                int dr = (int)( ( w->pal[j] >> 16 ) & 0xFFU ) - r, dg = (int)( ( w->pal[j] >> 8 ) & 0xFFU ) - g, db = (int)( w->pal[j] & 0xFFU ) - bb;
                int d = 2 * dr * dr + 4 * dg * dg + 3 * db * db;
                if ( d < bd ) {
                    bd = d;
                    best = j;
                }
            }
            bl[ a * 256 + b ] = bl[ b * 256 + a ] = (uint8_t) best;
        }
    }
    w->blend = bl;
}

static inline int lightlevel_at( const w3d_t* w, int light, double dist, double x, double y, double z ) {
    if ( w->nlamps > 0 ) {
        light = imin( 255, light + lamp_light( w, x, y, z ) );
    }
    return lightlevel( w, light, dist );
}

// -- geometry ---------------------------------------------------------------------------------------------

static bool sector_valid( const w3d_t* w, int s ) {
    return s >= 0 && s < w->nsectors && w->sectors[s].used;
}

// the height of the floor / ceiling of a sector at a point (planes: a slope per axis around the sector's origin)
static inline double floor_at( const sector_t* s, double x, double y ) {
    return s->sloped ? s->fz + s->fgx * ( x - s->sox ) + s->fgy * ( y - s->soy ) : s->fz;
}

static inline double ceil_at( const sector_t* s, double x, double y ) {
    return s->sloped ? s->cz + s->cgx * ( x - s->sox ) + s->cgy * ( y - s->soy ) : s->cz;
}

static bool inside( const sector_t* s, double x, double y ) {
    if ( x < s->minx || x > s->maxx || y < s->miny || y > s->maxy ) {
        return false;
    }
    bool in = false;
    for ( int i=0; i < s->nwalls; ++i ) {
        const wall_t* wl = &s->walls[i];
        if ( ( wl->y1 > y ) != ( wl->y2 > y ) && x < ( wl->x2 - wl->x1 ) * ( y - wl->y1 ) / ( wl->y2 - wl->y1 ) + wl->x1 ) {
            in = !in;
        }
    }
    return in;
}

// sectors may lie over each other (room over room, a bridge and the road under it): with a height z (not NONE) the sector whose floor is the
// highest one at or below z (and whose ceiling is above it) wins, else the one with the nearest floor
static int find_sector_z( const w3d_t* w, double x, double y, double z, int hint ) {
    int best = -1;
    double bestd = 1.0e30;
    for ( int i=0; i < w->nsectors; ++i ) {
        const sector_t* s = &w->sectors[i];
        if ( !s->used || !inside( s, x, y ) ) {
            continue;
        }
        double f = floor_at( s, x, y ), c = ceil_at( s, x, y );
        double d;
        if ( z >= f - 1.0e-6 && z < c ) {
            d = z - f;                              // the nearest floor below z
        } else {
            d = 1.0e6 + ( z < f ? f - z : z - c );
        }
        if ( d < bestd || ( d == bestd && i == hint ) ) {
            bestd = d;
            best = i;
        }
    }
    return best;
}

static int find_sector( const w3d_t* w, double x, double y, int hint ) {
    if ( sector_valid( w, hint ) ) {
        const sector_t* h = &w->sectors[hint];
        if ( inside( h, x, y ) ) {
            return hint;
        }
        for ( int i=0; i < h->nwalls; ++i ) {
            int n = h->walls[i].next;
            if ( sector_valid( w, n ) && inside( &w->sectors[n], x, y ) ) {
                return n;
            }
        }
    }
    for ( int i=0; i < w->nsectors; ++i ) {
        if ( w->sectors[i].used && inside( &w->sectors[i], x, y ) ) {
            return i;
        }
    }
    return -1;
}

// can a body of this height and step-up ability walk through the wall of sector s?
static bool wall_passable( const w3d_t* w, const sector_t* s, const wall_t* wl, double px, double py, double hgt, double step ) {
    if ( !sector_valid( w, wl->next ) || ( wl->flags & W3D_WF_BLOCK ) != 0 ) {
        return false;
    }
    const sector_t* n = &w->sectors[wl->next];
    double sf = floor_at( s, px, py ), nf = floor_at( n, px, py );
    if ( dmin( ceil_at( s, px, py ), ceil_at( n, px, py ) ) - dmax( sf, nf ) < hgt ) {
        return false;
    }
    return nf - sf <= step;
}

// pushes (*nx, *ny) out of the walls of sector si closer than r; solidonly: only walls that nothing can pass (the neighbors' walls)
static bool push_walls( const w3d_t* w, int si, bool solidonly, double ox, double oy, double* nx, double* ny, double r, double hgt, double step ) {
    const sector_t* s = &w->sectors[si];
    bool pushed = false;
    for ( int i=0; i < s->nwalls; ++i ) {
        const wall_t* wl = &s->walls[i];
        bool blocking;
        if ( solidonly ) {
            blocking = !sector_valid( w, wl->next ) || ( wl->flags & W3D_WF_BLOCK ) != 0;
        } else {
            double ex0 = wl->x2 - wl->x1, ey0 = wl->y2 - wl->y1;
            double t0 = ( ( *nx - wl->x1 ) * ex0 + ( *ny - wl->y1 ) * ey0 ) / ( wl->len * wl->len );
            t0 = t0 < 0.0 ? 0.0 : ( t0 > 1.0 ? 1.0 : t0 );
            blocking = !wall_passable( w, s, wl, wl->x1 + t0 * ex0, wl->y1 + t0 * ey0, hgt, step );
        }
        if ( !blocking ) {
            continue;
        }
        double ex = wl->x2 - wl->x1, ey = wl->y2 - wl->y1;
        double t = ( ( *nx - wl->x1 ) * ex + ( *ny - wl->y1 ) * ey ) / ( wl->len * wl->len );
        t = t < 0.0 ? 0.0 : ( t > 1.0 ? 1.0 : t );
        double cpx = wl->x1 + t * ex, cpy = wl->y1 + t * ey;
        double ddx = *nx - cpx, ddy = *ny - cpy;
        double d2 = ddx * ddx + ddy * ddy;
        if ( d2 >= r * r ) {
            continue;
        }
        double d = sqrt( d2 );
        if ( d > 1.0e-9 ) {
            *nx = cpx + ddx / d * r;
            *ny = cpy + ddy / d * r;
        } else {
            double nxn = -ey / wl->len, nyn = ex / wl->len;
            if ( nxn * ( ox - wl->x1 ) + nyn * ( oy - wl->y1 ) < 0.0 ) {
                nxn = -nxn; nyn = -nyn;
            }
            *nx = cpx + nxn * r;
            *ny = cpy + nyn * r;
        }
        pushed = true;
    }
    return pushed;
}

// moves a circle through the world, sliding along walls, up steps and through portals it fits through
static int move_circle( const w3d_t* w, double* px, double* py, int* psec, double dx, double dy, double r, double hgt, double step ) {
    if ( !sector_valid( w, *psec ) ) {
        return W3D_HIT_STUCK;
    }
    double startx = *px, starty = *py;
    int startsec = *psec;
    int flags = 0;
    double dist = sqrt( dx * dx + dy * dy );
    int n = (int) ceil( dist / ( r * 0.5 ) );
    n = n < 1 ? 1 : ( n > 64 ? 64 : n );
    double sx = dx / n, sy = dy / n;
    for ( int k=0; k < n; ++k ) {
        double ox = *px, oy = *py;
        double nx = ox + sx, ny = oy + sy;
        const sector_t* cs = &w->sectors[*psec];
        int neigh[64], nn = 0;
        for ( int i=0; i < cs->nwalls && nn < 64; ++i ) {
            int nb = cs->walls[i].next;
            if ( sector_valid( w, nb ) && nb != *psec ) {
                bool dup = false;
                for ( int j=0; j < nn; ++j ) {
                    dup = dup || neigh[j] == nb;
                }
                if ( !dup ) {
                    neigh[nn++] = nb;
                }
            }
        }
        for ( int iter=0; iter < 4; ++iter ) {
            bool pushed = push_walls( w, *psec, false, ox, oy, &nx, &ny, r, hgt, step );
            for ( int j=0; j < nn; ++j ) {
                pushed = push_walls( w, neigh[j], true, ox, oy, &nx, &ny, r, hgt, step ) || pushed;
            }
            if ( !pushed ) {
                break;
            }
            flags |= W3D_HIT_WALL;
        }
        int ns = *psec;
        if ( !inside( cs, nx, ny ) ) {
            ns = -1;
            for ( int i=0; i < cs->nwalls; ++i ) {
                const wall_t* wl = &cs->walls[i];
                if ( wall_passable( w, cs, wl, nx, ny, hgt, step ) && inside( &w->sectors[wl->next], nx, ny ) ) {
                    ns = wl->next;
                    break;
                }
            }
            if ( ns < 0 ) {
                break;                      // somewhere nobody may be: stay where the last step ended
            }
        }
        *px = nx; *py = ny; *psec = ns;
    }
    if ( *psec != startsec ) {
        flags |= W3D_HIT_SECTOR;
    }
    if ( dist > 1.0e-9 && fabs( *px - startx ) + fabs( *py - starty ) < 1.0e-6 ) {
        flags |= W3D_HIT_STUCK;
    }
    return flags;
}

// -- tracing a ray (shots, sight, use) -------------------------------------------------------------------------

typedef struct _shot_t {
    int     kind;                           // 0 nothing within range, 1 a wall, 2 a thing
    int     thing;
    double  dist, x, y;
} shot_t;

// the nearest wall of sector s along the ray (ox,oy) + t * (dx,dy) with tmin < t < tmax
static const wall_t* nearest_wall( const sector_t* s, double ox, double oy, double dx, double dy, double tmin, double tmax, double* pt ) {
    const wall_t* best = 0;
    double bt = tmax;
    for ( int i=0; i < s->nwalls; ++i ) {
        const wall_t* wl = &s->walls[i];
        double ex = wl->x2 - wl->x1, ey = wl->y2 - wl->y1;
        double den = dx * ey - dy * ex;
        if ( den > -1.0e-12 && den < 1.0e-12 ) {
            continue;
        }
        double wx = wl->x1 - ox, wy = wl->y1 - oy;
        double t = ( wx * ey - wy * ex ) / den;
        if ( t <= tmin || t >= bt ) {
            continue;
        }
        double u = ( wx * dy - wy * dx ) / den;
        if ( u < -1.0e-9 || u > 1.0 + 1.0e-9 ) {
            continue;
        }
        bt = t;
        best = wl;
    }
    *pt = bt;
    return best;
}

// (dx, dy) must be a unit vector; slope = rise per unit of distance
static void trace( const w3d_t* w, double ox, double oy, double oz, double dx, double dy, double slope, int sec, double maxd, bool wantthings, shot_t* out ) {
    out->kind = 0; out->thing = -1; out->dist = maxd; out->x = ox + dx * maxd; out->y = oy + dy * maxd;
    double tprev = 1.0e-6;
    for ( int depth=0; depth < 64 && sector_valid( w, sec ); ++depth ) {
        const sector_t* s = &w->sectors[sec];
        double t = maxd;
        const wall_t* wl = nearest_wall( s, ox, oy, dx, dy, tprev, maxd, &t );
        if ( wantthings ) {
            double bs = 1.0e30;
            int bi = -1;
            for ( int i=0; i < W3D_MAXTHINGS; ++i ) {
                const thing_t* th = &w->things[i];
                if ( !th->used || th->sector != sec ) {
                    continue;
                }
                double px = th->x - ox, py = th->y - oy;
                double sd = px * dx + py * dy, lat = px * dy - py * dx;
                double zb = floor_at( s, th->x, th->y ) + th->zoff, zr = oz + slope * sd;
                if ( sd > tprev - 1.0e-9 && sd < t && sd < bs && fabs( lat ) <= th->w * 0.4 && zr >= zb && zr <= zb + th->h ) {
                    bs = sd;
                    bi = i;
                }
            }
            if ( bi >= 0 ) {
                out->kind = 2; out->thing = bi; out->dist = bs; out->x = ox + dx * bs; out->y = oy + dy * bs;
                return;
            }
        }
        if ( wl == 0 ) {
            return;
        }
        double zr = oz + slope * t;
        bool pass = false;
        if ( sector_valid( w, wl->next ) && ( wl->flags & W3D_WF_SOLID ) == 0 ) {
            const sector_t* n = &w->sectors[wl->next];
            double hx = ox + dx * t, hy = oy + dy * t;
            pass = zr >= dmax( floor_at( s, hx, hy ), floor_at( n, hx, hy ) ) && zr <= dmin( ceil_at( s, hx, hy ), ceil_at( n, hx, hy ) );
        }
        if ( !pass ) {
            out->kind = 1; out->dist = t; out->x = ox + dx * t; out->y = oy + dy * t;
            return;
        }
        sec = wl->next;
        tprev = t + 1.0e-6;
    }
}

// -- rendering -------------------------------------------------------------------------------------------------

typedef struct _rc_t {
    const w3d_t*    w;
    uint8_t*        fb;
    float*          zb;
    int             W, H;
    double          f, horizon, ez, cx, cy, fx, fy, rx, ry;
    // the column being drawn
    double          skyu;
    bool            skyok;
    double          crx, cry;           // the ray of the column (forward component 1)
    uint8_t*        visited;
    double          wsx, wsy;           // the scroll offsets of the walls of the sector being drawn
    struct _tcol_t* trans;
    int*            ntrans;
} rc_t;

typedef struct _masked_t {
    const wall_t*   wall;
    const sector_t* sector;
    double          t, u, zbase;
    int             y0, y1, lv;
} masked_t;

typedef struct _tcol_t {
    const wall_t*   wall;
    const sector_t* sector;
    double          t, u, zbase;
    int             y0, y1, lv;
} tcol_t;

#define MAXMASKED   16

static const tex_t* tex_get( const w3d_t* w, int n ) {
    if ( n >= 0 && n < W3D_MAXTEX && w->animcount[n] > 0 ) {
        double fr = floor( w->time * w->animfps[n] );
        n = w->animfirst[n] + (int)( fmod( fr < 0.0 ? 0.0 : fr, (double) w->animcount[n] ) );
    }
    if ( n < 0 || n >= W3D_MAXTEX || w->texs[n].col == 0 ) {
        return 0;
    }
    return &w->texs[n];
}

// v modulo n as 0 .. n-1 (any value, however large)
static int wrapi( double v, int n ) {
    double r = fmod( floor( v ), (double) n );
    if ( r < 0.0 ) {
        r += (double) n;
    }
    return r >= 0.0 && r < (double) n ? (int) r : 0;
}

static int texel_u( const tex_t* tx, const wall_t* wl, double u, double extra ) {
    return wrapi( ( 1.0 - u ) * wl->len + wl->xoff + extra, tx->w );
}

static void draw_sky( rc_t* c, int x, int y0, int y1, double rx, double ry ) {
    const w3d_t* w = c->w;
    const tex_t* tx = tex_get( w, w->skytex );
    const uint8_t* cm = w->cmap[W3D_LEVELS - 1];
    uint8_t* fb = c->fb + x;
    if ( tx == 0 ) {
        uint8_t col = cm[ (uint8_t) w->skytex ];
        for ( int y = y0; y < y1; ++y ) {
            fb[ (size_t) y * c->W ] = col;
        }
        return;
    }
    if ( !c->skyok ) {
        double ang = atan2( ry, rx ) / ( 2.0 * 3.14159265358979323846 );
        ang -= floor( ang );
        c->skyu = ang * tx->w * w->skyrep;
        c->skyok = true;
    }
    int tu = (int) fmod( c->skyu, (double) tx->w );
    const uint8_t* colp = tx->col + (size_t) tu * (size_t) tx->h;
    for ( int y = y0; y < y1; ++y ) {
        int row = tx->h - 1 - dclamp( c->horizon - ( y + 0.5 ) + w->skyshift, 0, tx->h - 1 );
        fb[ (size_t) y * c->W ] = cm[ colp[row] ];
    }
}

// rows y0 .. y1-1 of the wall in column x; the texture is anchored to the world height (its bottom at multiples of its height)
static void draw_wall( rc_t* c, int x, int y0, int y1, const wall_t* wl, int texno, double u, double t, int lv ) {
    if ( y0 >= y1 ) {
        return;
    }
    const uint8_t* cm = c->w->cmap[lv];
    uint8_t* fb = c->fb + x;
    float* zb = c->zb + x;
    if ( ( wl->flags & W3D_WF_SKY ) != 0 ) {
        draw_sky( c, x, y0, y1, c->crx, c->cry );
        for ( int y = y0; y < y1; ++y ) {
            zb[ (size_t) y * c->W ] = 1.0e30f;
        }
        return;
    }
    const tex_t* tx = tex_get( c->w, texno );
    if ( tx == 0 ) {
        uint8_t col = cm[ (uint8_t) texno ];
        for ( int y = y0; y < y1; ++y ) {
            fb[ (size_t) y * c->W ] = col;
            zb[ (size_t) y * c->W ] = (float) t;
        }
        return;
    }
    const uint8_t* colp = tx->col + (size_t) texel_u( tx, wl, u, c->wsx ) * (size_t) tx->h;
    double dz = t / c->f;
    double z = c->ez + ( c->horizon - ( y0 + 0.5 ) ) * dz + wl->yoff + c->wsy;
    int th = tx->h;
    for ( int y = y0; y < y1; ++y ) {
        int zi = wrapi( z, th );
        fb[ (size_t) y * c->W ] = cm[ colp[ th - 1 - zi ] ];
        zb[ (size_t) y * c->W ] = (float) t;
        z -= dz;
    }
}

// rows y0 .. y1-1 of a floor or ceiling: a0 = the height of the plane above the camera's position, gdr = how fast it rises along the column's ray
// (a flat plane: gdr 0); the depth of the point seen in a row follows from ez + vz * t = a0 + gdr * t
static void draw_flat( rc_t* c, int x, int y0, int y1, double a0, double gdr, int texno, int light, double rx, double ry, double offx, double offy ) {
    const w3d_t* w = c->w;
    const tex_t* tx = tex_get( w, texno );
    uint8_t* fb = c->fb + x;
    float* zb = c->zb + x;
    double dh = c->ez - a0;
    for ( int y = y0; y < y1; ++y ) {
        double dy = ( y + 0.5 ) - c->horizon + gdr * c->f;
        if ( dy > -1.0e-6 && dy < 1.0e-6 ) {
            continue;
        }
        double depth = dh * c->f / dy;
        if ( depth <= 0.0 || depth > 1.0e7 ) {
            continue;
        }
        const uint8_t* cm = w->cmap[ w->nlamps > 0 ? lightlevel_at( w, light, depth, c->cx + rx * depth, c->cy + ry * depth, a0 + gdr * depth ) : lightlevel( w, light, depth ) ];
        uint8_t px;
        if ( tx == 0 ) {
            px = (uint8_t) texno;
        } else {
            int u = wrapi( c->cx + rx * depth + offx, tx->w );
            int v = wrapi( c->cy + ry * depth + offy, tx->h );
            px = tx->col[ (size_t) u * (size_t) tx->h + (size_t) v ];
        }
        fb[ (size_t) y * c->W ] = cm[px];
        zb[ (size_t) y * c->W ] = (float) depth;
    }
}

static void render_column( rc_t* c, int x ) {
    const w3d_t* w = c->w;
    double sx = ( x + 0.5 - c->W * 0.5 ) / c->f;
    double rx = c->fx + c->rx * sx, ry = c->fy + c->ry * sx;
    int ytop = 0, ybot = c->H;
    int cur = w->csector;
    double tprev = 1.0e-4;
    masked_t mk[MAXMASKED];
    int nmk = 0;
    c->skyok = false;
    c->crx = rx; c->cry = ry;
    for ( int depth = 0; depth < 64 && ytop < ybot; ++depth ) {
        const sector_t* s = &w->sectors[cur];
        c->visited[cur] = 1;
        double t = 1.0e30;
        const wall_t* wl = nearest_wall( s, c->cx, c->cy, rx, ry, tprev, 1.0e30, &t );
        if ( wl == 0 ) {
            break;                          // a leak in the map: leave the rest as it is
        }
        double u = 0.0;
        {
            double ex = wl->x2 - wl->x1, ey = wl->y2 - wl->y1;
            double den = rx * ey - ry * ex;
            u = ( ( wl->x1 - c->cx ) * ry - ( wl->y1 - c->cy ) * rx ) / den;
            u = u < 0.0 ? 0.0 : ( u > 1.0 ? 1.0 : u );
        }
        if ( w->farz > 0.0 && t > w->farz ) {
            break;                          // beyond the draw distance
        }
        double inv = c->f / t;
        double hx = c->cx + rx * t, hy = c->cy + ry * t;
        double sfz = floor_at( s, hx, hy ), scz = ceil_at( s, hx, hy );
        int cy1 = iclamp( ceilrow( c->horizon - ( scz - c->ez ) * inv - 0.5 ), ytop, ybot );
        int fy0 = iclamp( ceilrow( c->horizon - ( sfz - c->ez ) * inv - 0.5 ), ytop, ybot );
        if ( fy0 < cy1 ) {
            fy0 = cy1;
        }
        if ( s->ctex < 0 ) {
            draw_sky( c, x, ytop, cy1, rx, ry );
        } else {
            draw_flat( c, x, ytop, cy1, ceil_at( s, c->cx, c->cy ), s->sloped ? s->cgx * rx + s->cgy * ry : 0.0, s->ctex, s->elight, rx, ry, s->cox, s->coy );
        }
        draw_flat( c, x, fy0, ybot, floor_at( s, c->cx, c->cy ), s->sloped ? s->fgx * rx + s->fgy * ry : 0.0, s->ftex, s->elight, rx, ry, s->fox, s->foy );
        int lv = lightlevel_at( w, s->elight, t, hx, hy, c->ez );
        c->wsx = s->wox; c->wsy = s->woy;
        if ( !sector_valid( w, wl->next ) ) {
            draw_wall( c, x, cy1, fy0, wl, wl->tex, u, t, lv );
            break;
        }
        const sector_t* n = &w->sectors[wl->next];
        int top = cy1, bot = fy0;
        double nfz = floor_at( n, hx, hy ), ncz = ceil_at( n, hx, hy );
        if ( ncz < scz && !( s->ctex < 0 && n->ctex < 0 ) ) {
            int e = iclamp( ceilrow( c->horizon - ( ncz - c->ez ) * inv - 0.5 ), cy1, fy0 );
            draw_wall( c, x, cy1, e, wl, wl->upper, u, t, lv );
            top = e;
        }
        if ( nfz > sfz ) {
            int e = iclamp( ceilrow( c->horizon - ( nfz - c->ez ) * inv - 0.5 ), top, fy0 );
            draw_wall( c, x, e, fy0, wl, wl->lower, u, t, lv );
            bot = e;
        }
        if ( ( wl->flags & W3D_WF_TRANS ) != 0 && top < bot && c->ntrans != 0 && c->ntrans[x] < MAXTRANS ) {
            tcol_t* m = &c->trans[ (size_t) x * MAXTRANS + (size_t) c->ntrans[x]++ ];
            m->wall = wl; m->sector = s; m->t = t; m->u = u; m->y0 = top; m->y1 = bot; m->lv = lv;
            m->zbase = dmax( sfz, nfz );
        } else if ( ( wl->flags & W3D_WF_MASKED ) != 0 && top < bot && nmk < MAXMASKED ) {
            masked_t* m = &mk[nmk++];
            m->wall = wl; m->sector = s; m->t = t; m->u = u; m->y0 = top; m->y1 = bot; m->lv = lv;
            m->zbase = dmax( sfz, nfz );
        }
        ytop = top;
        ybot = bot;
        cur = wl->next;
        tprev = t + 1.0e-5;
    }
    for ( int k = nmk - 1; k >= 0; --k ) {
        const masked_t* m = &mk[k];
        const tex_t* tx = tex_get( w, m->wall->tex );
        if ( tx == 0 ) {
            continue;
        }
        const uint8_t* cm = w->cmap[ m->lv ];
        const uint8_t* colp = tx->col + (size_t) texel_u( tx, m->wall, m->u, m->sector->wox ) * (size_t) tx->h;
        double dz = m->t / c->f;
        double z = c->ez + ( c->horizon - ( m->y0 + 0.5 ) ) * dz;
        for ( int y = m->y0; y < m->y1; ++y, z -= dz ) {
            double rel = z - m->zbase - m->wall->yoff - m->sector->woy;
            if ( rel < 0.0 || rel >= tx->h ) {
                continue;
            }
            uint8_t px = colp[ tx->h - 1 - (int) rel ];      // (rel is 0 .. h)
            size_t at = (size_t) y * c->W + (size_t) x;
            if ( px != 0 && (float) m->t < c->zb[at] ) {
                c->fb[at] = cm[px];
                c->zb[at] = (float) m->t;
            }
        }
    }
}

typedef struct _spr_t {
    int     thing;
    double  tz, sxc, half, ytop, ybot;
} spr_t;

static int spr_cmp( const void* a, const void* b ) {
    double d = ( (const spr_t*) b )->tz - ( (const spr_t*) a )->tz;      // far first
    return d < 0.0 ? -1 : ( d > 0.0 ? 1 : 0 );
}

static void render_things( rc_t* c ) {
    const w3d_t* w = c->w;
    spr_t* list = (spr_t*) malloc( sizeof(spr_t) * W3D_MAXTHINGS );
    if ( list == 0 ) {
        return;
    }
    int n = 0;
    for ( int i=0; i < W3D_MAXTHINGS; ++i ) {
        const thing_t* th = &w->things[i];
        if ( !th->used || !sector_valid( w, th->sector ) || !c->visited[ th->sector ] ) {
            continue;
        }
        double dx = th->x - c->cx, dy = th->y - c->cy;
        double tz = dx * c->fx + dy * c->fy;
        if ( tz < 2.0 ) {
            continue;
        }
        double lat = dx * c->rx + dy * c->ry;
        double half = th->w * 0.5 * c->f / tz;
        double sxc = c->W * 0.5 + lat * c->f / tz;
        double zb = floor_at( &w->sectors[th->sector], th->x, th->y ) + th->zoff;
        double ytop = c->horizon - ( zb + th->h - c->ez ) * c->f / tz;
        double ybot = c->horizon - ( zb - c->ez ) * c->f / tz;
        if ( sxc + half < 0.0 || sxc - half >= c->W || ybot < 0.0 || ytop >= c->H ) {
            continue;
        }
        list[n].thing = i; list[n].tz = tz; list[n].sxc = sxc; list[n].half = half; list[n].ytop = ytop; list[n].ybot = ybot;
        ++n;
    }
    qsort( list, (size_t) n, sizeof(spr_t), spr_cmp );
    for ( int k=0; k < n; ++k ) {
        const spr_t* sp = &list[k];
        const thing_t* th = &w->things[sp->thing];
        const tex_t* tx = tex_get( w, th->tex );
        const sector_t* tsec = &w->sectors[th->sector];
        double tzz = floor_at( tsec, th->x, th->y ) + th->zoff + th->h * 0.5;
        int lv = th->light >= 0 ? lightlevel_at( w, th->light, 0.0, th->x, th->y, tzz ) : lightlevel_at( w, tsec->elight, sp->tz, th->x, th->y, tzz );
        const uint8_t* bl = th->trans ? blend_get( w ) : 0;
        const uint8_t* cm = w->cmap[lv];
        double left = sp->sxc - sp->half, width = sp->half * 2.0, height = sp->ybot - sp->ytop;
        if ( width <= 0.0 || height <= 0.0 ) {
            continue;
        }
        int x0 = imax( 0, ceilrow( left - 0.5 ) ), x1 = imin( c->W, ceilrow( left + width - 0.5 ) );
        int y0 = imax( 0, ceilrow( sp->ytop - 0.5 ) ), y1 = imin( c->H, ceilrow( sp->ybot - 0.5 ) );
        for ( int x = x0; x < x1; ++x ) {
            int tu = tx ? dclamp( ( x + 0.5 - left ) / width * tx->w, 0, tx->w - 1 ) : 0;
            for ( int y = y0; y < y1; ++y ) {
                size_t at = (size_t) y * c->W + (size_t) x;
                if ( (float) sp->tz >= c->zb[at] ) {
                    continue;
                }
                uint8_t px;
                if ( tx == 0 ) {
                    px = (uint8_t) th->tex;
                } else {
                    int tv = dclamp( ( y + 0.5 - sp->ytop ) / height * tx->h, 0, tx->h - 1 );
                    px = tx->col[ (size_t) tu * (size_t) tx->h + (size_t) tv ];
                }
                if ( px != 0 ) {
                    if ( bl != 0 ) {
                        c->fb[at] = bl[ (size_t) cm[px] * 256U + c->fb[at] ];
                    } else {
                        c->fb[at] = cm[px];
                        c->zb[at] = (float) sp->tz;
                    }
                }
            }
        }
    }
    free( list );
}

// translucent walls of the frame, far to near, mixed with what is there (things included)
static void render_trans( rc_t* c ) {
    const w3d_t* w = c->w;
    const uint8_t* bl = blend_get( w );
    for ( int x=0; x < c->W; ++x ) {
        for ( int k = c->ntrans[x] - 1; k >= 0; --k ) {
            const tcol_t* m = &c->trans[ (size_t) x * MAXTRANS + (size_t) k ];
            const tex_t* tx = tex_get( w, m->wall->tex );
            if ( tx == 0 ) {
                continue;
            }
            const uint8_t* cm = w->cmap[ m->lv ];
            const uint8_t* colp = tx->col + (size_t) texel_u( tx, m->wall, m->u, m->sector->wox ) * (size_t) tx->h;
            double dz = m->t / c->f;
            double z = c->ez + ( c->horizon - ( m->y0 + 0.5 ) ) * dz;
            for ( int y = m->y0; y < m->y1; ++y, z -= dz ) {
                double rel = z - m->zbase - m->wall->yoff - m->sector->woy;
                if ( rel < 0.0 || rel >= tx->h ) {
                    continue;
                }
                uint8_t px = colp[ tx->h - 1 - (int) rel ];
                size_t at = (size_t) y * c->W + (size_t) x;
                if ( px != 0 && (float) m->t < c->zb[at] ) {
                    c->fb[at] = bl != 0 ? bl[ (size_t) cm[px] * 256U + c->fb[at] ] : cm[px];
                }
            }
        }
    }
}

// the light effects of the frame
static void update_effects( w3d_t* w ) {
    for ( int i=0; i < w->nsectors; ++i ) {
        sector_t* s = &w->sectors[i];
        if ( !s->used ) {
            continue;
        }
        double l = s->light;
        if ( s->fxmode == 1 ) {
            uint32_t h = (uint32_t) floor( w->time * s->fxrate ) * 2654435761U + (uint32_t) i * 40503U;
            h ^= h >> 15; h *= 2246822519U; h ^= h >> 13;
            l -= s->fxamt * (double)( h & 1023U ) / 1023.0;
        } else if ( s->fxmode == 2 ) {
            l += s->fxamt * sin( 2.0 * 3.14159265358979323846 * s->fxrate * w->time );
        } else if ( s->fxmode == 3 ) {
            if ( ( (long long) floor( w->time * s->fxrate * 2.0 ) & 1LL ) != 0 ) {
                l -= s->fxamt;
            }
        }
        s->elight = l < 0.0 ? 0 : ( l > 255.0 ? 255 : (int)( l + 0.5 ) );
    }
}

static double eye_z( const w3d_t* w ) {
    const sector_t* s = &w->sectors[w->csector];
    double ez = w->pz + w->peye + w->lift;
    double cz = ceil_at( s, w->cx, w->cy );
    if ( ez > cz - 2.0 ) {
        ez = cz - 2.0;
    }
    return ez;
}

const uint8_t* w3d_render( w3d_t* w ) {
    if ( w->fb == 0 || w->zb == 0 ) {
        return 0;
    }
    size_t n = (size_t) w->bw * (size_t) w->bh;
    memset( w->fb, 0, n );
    for ( size_t i=0; i < n; ++i ) {
        w->zb[i] = 1.0e30f;
    }
    if ( !sector_valid( w, w->csector ) ) {
        return w->fb;
    }
    double floor_ = floor_at( &w->sectors[w->csector], w->cx, w->cy );
    if ( !w->pzvalid ) {
        w->pz = floor_;
        w->pzvalid = true;
    } else if ( fabs( floor_ - w->pz ) < 0.5 ) {
        w->pz = floor_;
    } else {
        w->pz += ( floor_ - w->pz ) * 0.4;                  // stairs and lifts are followed smoothly
    }
    if ( w->visitcap < w->nsectors ) {
        uint8_t* nv = (uint8_t*) realloc( w->visited, (size_t) w->nsectors );
        if ( nv == 0 ) {
            return w->fb;
        }
        w->visited = nv;
        w->visitcap = w->nsectors;
    }
    memset( w->visited, 0, (size_t) w->nsectors );
    update_effects( w );
    if ( w->usetrans ) {
        make_blend( w );
        if ( w->trans == 0 || w->ntrans == 0 || w->transcap < w->bw ) {
            free( w->trans ); free( w->ntrans );
            w->trans = (struct _tcol_t*) calloc( (size_t) w->bw * MAXTRANS, sizeof(tcol_t) );
            w->ntrans = (int*) calloc( (size_t) w->bw, sizeof(int) );
            w->transcap = w->trans != 0 && w->ntrans != 0 ? w->bw : 0;
        }
        if ( w->transcap >= w->bw ) {
            memset( w->ntrans, 0, sizeof(int) * (size_t) w->bw );
        }
    }
    rc_t c;
    memset( &c, 0, sizeof(c) );
    if ( w->usetrans && w->transcap >= w->bw ) {
        c.trans = w->trans;
        c.ntrans = w->ntrans;
    }
    c.visited = w->visited;
    c.w = w; c.fb = w->fb; c.zb = w->zb; c.W = w->bw; c.H = w->bh;
    c.f = ( w->bw * 0.5 ) / tan( w->fov * 0.5 * DEG2RAD );
    c.horizon = w->bh * 0.5 + tan( w->pitch * DEG2RAD ) * c.f;
    c.ez = eye_z( w );
    c.cx = w->cx; c.cy = w->cy;
    c.fx = cos( w->angle * DEG2RAD ); c.fy = sin( w->angle * DEG2RAD );
    c.rx = c.fy; c.ry = -c.fx;
    for ( int x=0; x < c.W; ++x ) {
        render_column( &c, x );
    }
    {
        int cnt = 0;
        for ( int i=0; i < w->nsectors; ++i ) {
            cnt += w->visited[i] != 0;
        }
        w->stat_visited = cnt;
    }
    render_things( &c );
    if ( c.ntrans != 0 ) {
        render_trans( &c );
    }
    return w->fb;
}

// -- the statements -------------------------------------------------------------------------------------------

#define ARG_ERR     "Illegal argument"

static bool whole( double v, int lo, int hi, int* out ) {
    if ( !( v >= (double) lo && v <= (double) hi ) ) {
        return false;
    }
    *out = (int) floor( v + 0.5 );
    return true;
}

static bool num_ok( double v ) {
    return v > -1.0e7 && v < 1.0e7;
}

// makes room for sector number s and returns it, 0 if it cannot be
static sector_t* sector_make( w3d_t* w, int s ) {
    if ( s >= w->capsectors ) {
        int ncap = w->capsectors == 0 ? 16 : w->capsectors;
        while ( ncap <= s ) {
            ncap *= 2;
        }
        sector_t* ns = (sector_t*) realloc( w->sectors, sizeof(sector_t) * (size_t) ncap );
        if ( ns == 0 ) {
            return 0;
        }
        memset( ns + w->capsectors, 0, sizeof(sector_t) * (size_t)( ncap - w->capsectors ) );
        w->sectors = ns;
        w->capsectors = ncap;
    }
    if ( s >= w->nsectors ) {
        w->nsectors = s + 1;
    }
    return &w->sectors[s];
}

static bool set_texture( w3d_t* w, int n, const char* s, size_t slen, const char** err ) {
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
    free( w->texs[n].col );
    w->texs[n].col = col; w->texs[n].w = tw; w->texs[n].h = th;
    return true;
}

static wall_t* wall_get( w3d_t* w, double sv, double iv ) {
    int s, i;
    if ( !whole( sv, 0, W3D_MAXSECTORS - 1, &s ) || !sector_valid( w, s ) || !whole( iv, 0, W3D_MAXWALLS - 1, &i ) || i >= w->sectors[s].nwalls ) {
        return 0;
    }
    return &w->sectors[s].walls[i];
}

static thing_t* thing_get( w3d_t* w, double nv ) {
    int n;
    if ( !whole( nv, 0, W3D_MAXTHINGS - 1, &n ) ) {
        return 0;
    }
    return &w->things[n];
}

static thing_t* thing_live( w3d_t* w, double nv ) {
    thing_t* t = thing_get( w, nv );
    return t != 0 && t->used ? t : 0;
}

static double norm_angle( double a ) {
    a = fmod( a, 360.0 );
    return a < 0.0 ? a + 360.0 : a;
}

bool w3d_op( w3d_t* w, int op, const double* args, int na, const char* s, size_t slen, const char** err ) {
    // arguments that were not passed are "omitted" (the VM passes the whole signature; a caller of this function may pass fewer)
    double a[12];
    for ( int i=0; i < 12; ++i ) {
        a[i] = i < na ? args[i] : GFX_NONE;
    }
    *err = ARG_ERR;
    int i1 = 0, i2 = 0, i3 = 0, i4 = 0;
    for ( int i=0; i < 12; ++i ) {
        if ( !NONE_VAL( a[i] ) && !num_ok( a[i] ) ) {
            return false;
        }
    }
    switch ( op ) {
        case GFXOP_W3DRESET:
            w3d_reset( w );
            return true;
        case GFXOP_W3DTEXTURE:
            return whole( a[0], 0, W3D_MAXTEX - 1, &i1 ) && set_texture( w, i1, s, slen, err );
        case GFXOP_W3DSECTOR: {
            if ( !whole( a[0], 0, W3D_MAXSECTORS - 1, &i1 ) || a[2] < a[1] ) {
                return false;
            }
            bool isnew = !( i1 < w->nsectors && w->sectors[i1].used );
            int ft = isnew ? 1 : w->sectors[i1].ftex, ct = isnew ? 1 : w->sectors[i1].ctex, lt = isnew ? 255 : w->sectors[i1].light;
            if ( ( !NONE_VAL( a[3] ) && !whole( a[3], 0, W3D_MAXTEX - 1, &ft ) ) || ( !NONE_VAL( a[4] ) && !whole( a[4], -1, W3D_MAXTEX - 1, &ct ) ) ||
                 ( !NONE_VAL( a[5] ) && !whole( a[5], 0, 255, &lt ) ) ) {
                return false;
            }
            sector_t* sec = sector_make( w, i1 );
            if ( sec == 0 ) {
                *err = "Out of memory";
                return false;
            }
            sec->used = true; sec->fz = a[1]; sec->cz = a[2]; sec->ftex = ft; sec->ctex = ct; sec->light = lt; sec->elight = lt;
            if ( isnew ) {
                sec->minx = sec->miny = 1.0e30; sec->maxx = sec->maxy = -1.0e30;
            }
            return true;
        }
        case GFXOP_W3DWALL: {
            int next = -1;
            if ( !whole( a[0], 0, W3D_MAXSECTORS - 1, &i1 ) ) {
                return false;
            }
            if ( !sector_valid( w, i1 ) ) {
                *err = "Sector not defined";
                return false;
            }
            if ( !whole( a[5], 0, W3D_MAXTEX - 1, &i2 ) || ( !NONE_VAL( a[6] ) && !whole( a[6], -1, W3D_MAXSECTORS - 1, &next ) ) ) {
                return false;
            }
            sector_t* sec = &w->sectors[i1];
            double len = sqrt( ( a[3] - a[1] ) * ( a[3] - a[1] ) + ( a[4] - a[2] ) * ( a[4] - a[2] ) );
            if ( len < 1.0e-6 || sec->nwalls >= W3D_MAXWALLS ) {
                *err = len < 1.0e-6 ? "A wall needs a length" : "Too many walls";
                return false;
            }
            if ( sec->nwalls >= sec->cap ) {
                int ncap = sec->cap == 0 ? 8 : sec->cap * 2;
                wall_t* nw = (wall_t*) realloc( sec->walls, sizeof(wall_t) * (size_t) ncap );
                if ( nw == 0 ) {
                    *err = "Out of memory";
                    return false;
                }
                sec->walls = nw;
                sec->cap = ncap;
            }
            wall_t* wl = &sec->walls[sec->nwalls++];
            memset( wl, 0, sizeof(*wl) );
            wl->x1 = a[1]; wl->y1 = a[2]; wl->x2 = a[3]; wl->y2 = a[4]; wl->len = len;
            wl->tex = wl->upper = wl->lower = i2;
            wl->next = next;
            sec->minx = dmin( sec->minx, dmin( wl->x1, wl->x2 ) ); sec->maxx = dmax( sec->maxx, dmax( wl->x1, wl->x2 ) );
            sec->miny = dmin( sec->miny, dmin( wl->y1, wl->y2 ) ); sec->maxy = dmax( sec->maxy, dmax( wl->y1, wl->y2 ) );
            return true;
        }
        case GFXOP_W3DWALLTEX: {
            wall_t* wl = wall_get( w, a[0], a[1] );
            int t[3] = { -1, -1, -1 };
            if ( wl == 0 ) {
                return false;
            }
            for ( int k=0; k < 3; ++k ) {
                if ( !whole( a[2 + k], -1, W3D_MAXTEX - 1, &t[k] ) ) {
                    return false;
                }
            }
            if ( t[0] >= 0 ) wl->tex = t[0];
            if ( t[1] >= 0 ) wl->upper = t[1];
            if ( t[2] >= 0 ) wl->lower = t[2];
            return true;
        }
        case GFXOP_W3DWALLOFF: {
            wall_t* wl = wall_get( w, a[0], a[1] );
            if ( wl == 0 ) {
                return false;
            }
            wl->xoff = a[2]; wl->yoff = a[3];
            return true;
        }
        case GFXOP_W3DWALLFLAG: {
            wall_t* wl = wall_get( w, a[0], a[1] );
            if ( wl == 0 || !whole( a[2], 0, 255, &i3 ) ) {
                return false;
            }
            wl->flags = i3;
            if ( ( i3 & W3D_WF_TRANS ) != 0 ) {
                w->usetrans = true;
            }
            return true;
        }
        case GFXOP_W3DHEIGHT:
            if ( !whole( a[0], 0, W3D_MAXSECTORS - 1, &i1 ) || !sector_valid( w, i1 ) || a[2] < a[1] ) {
                return false;
            }
            w->sectors[i1].fz = a[1]; w->sectors[i1].cz = a[2]; w->sectors[i1].moving = false;
            return true;
        case GFXOP_W3DLIGHT:
            if ( !whole( a[0], 0, W3D_MAXSECTORS - 1, &i1 ) || !sector_valid( w, i1 ) || !whole( a[1], 0, 255, &i2 ) ) {
                return false;
            }
            w->sectors[i1].light = i2; w->sectors[i1].elight = i2;
            return true;
        case GFXOP_W3DTAG:
            if ( !whole( a[0], 0, W3D_MAXSECTORS - 1, &i1 ) || !sector_valid( w, i1 ) || !whole( a[1], -1000000, 1000000, &i2 ) ) {
                return false;
            }
            w->sectors[i1].tag = i2;
            return true;
        case GFXOP_W3DSKY: {
            double rep = NONE_VAL( a[1] ) ? w->skyrep : a[1], shift = NONE_VAL( a[2] ) ? w->skyshift : a[2];
            if ( !whole( a[0], 0, W3D_MAXTEX - 1, &i1 ) || !( rep >= 0.1 && rep <= 64.0 ) || !( shift >= -10000.0 && shift <= 10000.0 ) ) {
                return false;
            }
            w->skytex = i1; w->skyrep = rep; w->skyshift = shift;
            return true;
        }
        case GFXOP_W3DCLEAR:
            for ( int k=0; k < w->nsectors; ++k ) {
                free( w->sectors[k].walls );
            }
            free( w->sectors );
            w->sectors = 0; w->nsectors = 0; w->capsectors = 0;
            memset( w->things, 0, sizeof(thing_t) * W3D_MAXTHINGS );
            w->csector = -1; w->pzvalid = false; w->hit = 0;
            return true;
        case GFXOP_W3DTICK: {
            if ( !( a[0] >= 0.0 && a[0] <= 3600.0 ) ) {
                return false;
            }
            w->time += a[0];
            if ( w->time > 1.0e7 ) {
                w->time = fmod( w->time, 1.0e6 );
            }
            for ( int i=0; i < w->nsectors; ++i ) {
                sector_t* s = &w->sectors[i];
                if ( !s->used ) {
                    continue;
                }
                if ( s->moving ) {
                    double step = s->mspeed * a[0];
                    double df = s->tfz - s->fz, dc = s->tcz - s->cz;
                    s->fz += df > step ? step : ( df < -step ? -step : df );
                    s->cz += dc > step ? step : ( dc < -step ? -step : dc );
                    if ( s->fz == s->tfz && s->cz == s->tcz ) {
                        s->moving = false;
                    }
                }
                s->fox = fmod( s->fox + s->fsx * a[0], 1.0e6 ); s->foy = fmod( s->foy + s->fsy * a[0], 1.0e6 );
                s->cox = fmod( s->cox + s->csx * a[0], 1.0e6 ); s->coy = fmod( s->coy + s->csy * a[0], 1.0e6 );
                s->wox = fmod( s->wox + s->wsx * a[0], 1.0e6 ); s->woy = fmod( s->woy + s->wsy * a[0], 1.0e6 );
            }
            return true;
        }
        case GFXOP_W3DSECMOVE: {
            if ( !whole( a[0], 0, W3D_MAXSECTORS - 1, &i1 ) || !sector_valid( w, i1 ) || !( a[1] > 0.0 && a[1] <= 100000.0 ) ) {
                return false;
            }
            sector_t* s = &w->sectors[i1];
            double tf = NONE_VAL( a[2] ) ? s->fz : a[2], tc = NONE_VAL( a[3] ) ? s->cz : a[3];
            if ( tc < tf ) {
                return false;
            }
            s->tfz = tf; s->tcz = tc; s->mspeed = a[1];
            s->moving = s->tfz != s->fz || s->tcz != s->cz;
            return true;
        }
        case GFXOP_W3DANIM: {
            if ( !whole( a[0], 0, W3D_MAXTEX - 1, &i1 ) || !whole( a[1], 0, W3D_MAXTEX - 1, &i2 ) || !whole( a[2], 0, W3D_MAXTEX, &i3 ) ||
                 !( a[3] > 0.0 && a[3] <= 1000.0 ) || i2 + i3 > W3D_MAXTEX ) {
                return false;
            }
            w->animfirst[i1] = i2; w->animcount[i1] = i3; w->animfps[i1] = a[3];
            return true;
        }
        case GFXOP_W3DSCROLL: {
            if ( !whole( a[0], 0, W3D_MAXSECTORS - 1, &i1 ) || !sector_valid( w, i1 ) || !whole( a[1], 0, 2, &i2 ) || !( fabs( a[2] ) <= 10000.0 ) || !( fabs( a[3] ) <= 10000.0 ) ) {
                return false;
            }
            sector_t* s = &w->sectors[i1];
            if ( i2 == 0 ) { s->fsx = a[2]; s->fsy = a[3]; }
            else if ( i2 == 1 ) { s->csx = a[2]; s->csy = a[3]; }
            else { s->wsx = a[2]; s->wsy = a[3]; }
            return true;
        }
        case GFXOP_W3DLIGHTFX: {
            double amt = NONE_VAL( a[2] ) ? 60.0 : a[2], rate = NONE_VAL( a[3] ) ? 8.0 : a[3];
            if ( !whole( a[0], 0, W3D_MAXSECTORS - 1, &i1 ) || !sector_valid( w, i1 ) || !whole( a[1], 0, 3, &i2 ) || !( amt >= 0.0 && amt <= 255.0 ) || !( rate > 0.0 && rate <= 1000.0 ) ) {
                return false;
            }
            sector_t* s = &w->sectors[i1];
            s->fxmode = i2; s->fxamt = amt; s->fxrate = rate;
            if ( i2 == 0 ) {
                s->elight = s->light;
            }
            return true;
        }
        case GFXOP_W3DLAMP: {
            if ( !whole( a[0], 0, W3D_MAXLAMPS - 1, &i1 ) || !( a[4] >= 0.0 && a[4] <= 100000.0 ) || !( a[5] >= 0.0 && a[5] <= 255.0 ) ) {
                return false;
            }
            lamp_t* l = &w->lamps[i1];
            l->x = a[1]; l->y = a[2]; l->z = a[3]; l->radius = a[4]; l->amount = a[5];
            l->on = a[4] > 0.0 && a[5] > 0.0;
            w->nlamps = 0;
            for ( int k=0; k < W3D_MAXLAMPS; ++k ) {
                if ( w->lamps[k].on ) {
                    w->nlamps = k + 1;
                }
            }
            return true;
        }
        case GFXOP_W3DTHINGTRANS: {
            thing_t* th = thing_live( w, a[0] );
            if ( th == 0 ) {
                return false;
            }
            th->trans = a[1] != 0.0;
            if ( th->trans ) {
                w->usetrans = true;
            }
            return true;
        }
        case GFXOP_W3DFAR:
            if ( !( a[0] >= 0.0 && a[0] <= 1.0e7 ) ) {
                return false;
            }
            w->farz = a[0];
            return true;
        case GFXOP_W3DSLOPE: {
            if ( !whole( a[0], 0, W3D_MAXSECTORS - 1, &i1 ) || !sector_valid( w, i1 ) || !whole( a[1], 0, 2, &i2 ) || !( fabs( a[2] ) <= 100.0 ) || !( fabs( a[3] ) <= 100.0 ) ) {
                return false;
            }
            sector_t* sec = &w->sectors[i1];
            if ( !NONE_VAL( a[4] ) && !NONE_VAL( a[5] ) ) {
                sec->sox = a[4]; sec->soy = a[5];
            } else if ( !sec->sloped ) {
                sec->sox = sec->nwalls > 0 ? ( sec->minx + sec->maxx ) * 0.5 : 0.0;
                sec->soy = sec->nwalls > 0 ? ( sec->miny + sec->maxy ) * 0.5 : 0.0;
            }
            if ( i2 == 0 || i2 == 2 ) {
                sec->fgx = a[2]; sec->fgy = a[3];
            }
            if ( i2 == 1 || i2 == 2 ) {
                sec->cgx = a[2]; sec->cgy = a[3];
            }
            sec->sloped = sec->fgx != 0.0 || sec->fgy != 0.0 || sec->cgx != 0.0 || sec->cgy != 0.0;
            return true;
        }
        case GFXOP_W3DCAMERA: {
            double pitch = NONE_VAL( a[3] ) ? w->pitch : a[3], lift = NONE_VAL( a[4] ) ? w->lift : a[4];
            if ( pitch < -80.0 || pitch > 80.0 ) {
                return false;
            }
            int sec = find_sector( w, a[0], a[1], w->csector );
            if ( !NONE_VAL( a[5] ) ) {
                // the sector is named (sectors can lie over each other)
                if ( !whole( a[5], 0, W3D_MAXSECTORS - 1, &i1 ) || !sector_valid( w, i1 ) || !inside( &w->sectors[i1], a[0], a[1] ) ) {
                    return false;
                }
                sec = i1;
            }
            if ( sec < 0 ) {
                *err = "Position outside of all sectors";
                return false;
            }
            w->cx = a[0]; w->cy = a[1]; w->angle = norm_angle( a[2] ); w->pitch = pitch; w->lift = lift;
            w->csector = sec;
            if ( !NONE_VAL( a[5] ) ) {
                w->pzvalid = false;                 // (a named sector is a jump to another floor: no easing)
            }
            if ( !w->pzvalid ) {
                w->pz = floor_at( &w->sectors[sec], a[0], a[1] );         // (the first position: no easing from nowhere)
                w->pzvalid = true;
            }
            return true;
        }
        case GFXOP_W3DTURN: {
            double p = w->pitch + ( NONE_VAL( a[1] ) ? 0.0 : a[1] );
            w->angle = norm_angle( w->angle + a[0] );
            w->pitch = p < -80.0 ? -80.0 : ( p > 80.0 ? 80.0 : p );
            return true;
        }
        case GFXOP_W3DPLAYER:
            if ( !( a[0] >= 1.0 && a[0] <= 1000.0 && a[1] >= 1.0 && a[1] <= 10000.0 && a[2] >= 1.0 && a[2] <= 10000.0 && a[3] >= 0.0 && a[3] <= 10000.0 ) ) {
                return false;
            }
            w->pr = a[0]; w->ph = a[1]; w->peye = a[2]; w->pstep = a[3];
            return true;
        case GFXOP_W3DVIEW: {
            int sc = 1;
            double fov = 90.0;
            if ( !whole( a[0], 0, 32767, &i1 ) || !whole( a[1], 0, 32767, &i2 ) || !whole( a[2], 8, W3D_MAXVIEWW, &i3 ) || !whole( a[3], 8, W3D_MAXVIEWH, &i4 ) ||
                 ( !NONE_VAL( a[4] ) && !whole( a[4], 1, 4, &sc ) ) || ( !NONE_VAL( a[5] ) && ( a[5] < 30.0 || a[5] > 150.0 ) ) ) {
                return false;
            }
            if ( !NONE_VAL( a[5] ) ) {
                fov = a[5];
            }
            if ( i3 / sc < 4 || i4 / sc < 4 || !set_view( w, i1, i2, i3, i4, sc, fov ) ) {
                return false;
            }
            return true;
        }
        case GFXOP_W3DFOG:
            if ( !( a[0] >= 0.0 && a[0] <= 1.0e6 ) ) {
                return false;
            }
            w->fog = a[0];
            return true;
        case GFXOP_W3DLIFT:
            if ( !( a[0] >= -1000.0 && a[0] <= 10000.0 ) ) {
                return false;
            }
            w->lift = a[0];
            return true;
        case GFXOP_W3DMOVE: {
            if ( !sector_valid( w, w->csector ) ) {
                *err = "No camera position set";
                return false;
            }
            double fx = cos( w->angle * DEG2RAD ), fy = sin( w->angle * DEG2RAD );
            double dx = fx * a[0] + fy * a[1], dy = fy * a[0] - fx * a[1];
            // a jump (lift > 0) lifts the feet: steps higher by that much can be taken (out of a pool)
            w->hit = move_circle( w, &w->cx, &w->cy, &w->csector, dx, dy, w->pr, w->ph, w->pstep + ( w->lift > 0.0 ? w->lift : 0.0 ) );
            return true;
        }
        case GFXOP_W3DTHING: {
            thing_t* t = thing_get( w, a[0] );
            if ( t == 0 || !whole( a[3], 0, W3D_MAXTEX - 1, &i1 ) || !( a[4] > 0.0 && a[4] <= 10000.0 ) || !( a[5] > 0.0 && a[5] <= 10000.0 ) ) {
                return false;
            }
            int sec = find_sector( w, a[1], a[2], t->used ? t->sector : -1 );
            if ( !NONE_VAL( a[6] ) ) {
                if ( !whole( a[6], 0, W3D_MAXSECTORS - 1, &i2 ) || !sector_valid( w, i2 ) || !inside( &w->sectors[i2], a[1], a[2] ) ) {
                    return false;
                }
                sec = i2;
            }
            if ( sec < 0 ) {
                *err = "Position outside of all sectors";
                return false;
            }
            bool isnew = !t->used;
            t->used = true; t->x = a[1]; t->y = a[2]; t->tex = i1; t->w = a[4]; t->h = a[5]; t->sector = sec;
            if ( isnew ) {
                t->zoff = 0.0; t->light = -1; t->trans = false;
            }
            return true;
        }
        case GFXOP_W3DTHINGZ: {
            thing_t* t = thing_live( w, a[0] );
            if ( t == 0 ) {
                return false;
            }
            t->zoff = a[1];
            return true;
        }
        case GFXOP_W3DTHINGTEX: {
            thing_t* t = thing_live( w, a[0] );
            if ( t == 0 || !whole( a[1], 0, W3D_MAXTEX - 1, &i1 ) ) {
                return false;
            }
            t->tex = i1;
            return true;
        }
        case GFXOP_W3DTHINGOFF: {
            thing_t* t = thing_get( w, a[0] );
            if ( t == 0 ) {
                return false;
            }
            t->used = false;
            return true;
        }
        case GFXOP_W3DTHINGMOVE: {
            thing_t* t = thing_live( w, a[0] );
            if ( t == 0 ) {
                return false;
            }
            w->hit = move_circle( w, &t->x, &t->y, &t->sector, a[1], a[2], dmax( 4.0, t->w * 0.35 ), t->h, 24.0 );
            return true;
        }
        case GFXOP_W3DTHINGLIGHT: {
            thing_t* t = thing_live( w, a[0] );
            if ( t == 0 || !whole( a[1], -1, 255, &i1 ) ) {
                return false;
            }
            t->light = i1;
            return true;
        }
        default:
            *err = "Not available here";
            return false;
    }
}

// -- the functions ---------------------------------------------------------------------------------------------

bool w3d_query( w3d_t* w, int q, const double* a, int na, double* result ) {
    (void) na;
    int i1 = 0, i2 = 0;
    *result = 0.0;
    for ( int i=0; i < na; ++i ) {
        if ( !num_ok( a[i] ) ) {
            return false;
        }
    }
    switch ( q ) {
        case GFXQ_W3DX:         *result = w->cx; return true;
        case GFXQ_W3DY:         *result = w->cy; return true;
        case GFXQ_W3DZ:         *result = sector_valid( w, w->csector ) ? eye_z( w ) : 0.0; return true;
        case GFXQ_W3DANGLE:     *result = w->angle; return true;
        case GFXQ_W3DPITCH:     *result = w->pitch; return true;
        case GFXQ_W3DSECTOR:    *result = w->csector; return true;
        case GFXQ_W3DFLOORZ:    *result = sector_valid( w, w->csector ) ? floor_at( &w->sectors[w->csector], w->cx, w->cy ) : 0.0; return true;
        case GFXQ_W3DSECTORAT:  *result = find_sector( w, a[0], a[1], w->csector ); return true;
        case GFXQ_W3DSECTORZ:   *result = find_sector_z( w, a[0], a[1], a[2], w->csector ); return true;
        case GFXQ_W3DFLOORAT: case GFXQ_W3DCEILAT: {
            int s = find_sector( w, a[0], a[1], w->csector );
            *result = s < 0 ? -1.0e6 : ( q == GFXQ_W3DFLOORAT ? floor_at( &w->sectors[s], a[0], a[1] ) : ceil_at( &w->sectors[s], a[0], a[1] ) );
            return true;
        }
        case GFXQ_W3DVISITED:   *result = w->stat_visited; return true;
        case GFXQ_W3DTIME:      *result = w->time; return true;
        case GFXQ_W3DWALLINFO: {
            const wall_t* wl = wall_get( w, a[0], a[1] );
            if ( wl == 0 || !whole( a[2], 0, 10, &i1 ) ) {
                return false;
            }
            const double v[11] = { wl->x1, wl->y1, wl->x2, wl->y2, wl->tex, wl->upper, wl->lower, wl->next, wl->xoff, wl->yoff, wl->flags };
            *result = v[i1];
            return true;
        }
        case GFXQ_W3DSECINFO: {
            if ( !whole( a[0], 0, W3D_MAXSECTORS - 1, &i1 ) || !whole( a[1], 0, 12, &i2 ) ) {
                return false;
            }
            if ( i2 == 12 ) {
                *result = sector_valid( w, i1 ) ? -1.0 : 0.0;
                return true;
            }
            if ( !sector_valid( w, i1 ) ) {
                return false;
            }
            const sector_t* s = &w->sectors[i1];
            const double v[12] = { s->fz, s->cz, s->ftex, s->ctex, s->light, s->tag, s->fgx, s->fgy, s->cgx, s->cgy, s->sox, s->soy };
            *result = v[i2];
            return true;
        }
        case GFXQ_W3DTHINGINFO: {
            const thing_t* th = thing_live( w, a[0] );
            if ( th == 0 || !whole( a[1], 0, 8, &i2 ) ) {
                return false;
            }
            const double v[9] = { th->x, th->y, th->tex, th->w, th->h, th->zoff, th->light, th->trans ? -1.0 : 0.0, th->sector };
            *result = v[i2];
            return true;
        }
        case GFXQ_W3DLIGHTNOW:
            if ( !whole( a[0], 0, W3D_MAXSECTORS - 1, &i1 ) || !sector_valid( w, i1 ) ) {
                return false;
            }
            *result = w->sectors[i1].elight;
            return true;
        case GFXQ_W3DMOVING:
            if ( !whole( a[0], 0, W3D_MAXSECTORS - 1, &i1 ) || !sector_valid( w, i1 ) ) {
                return false;
            }
            *result = w->sectors[i1].moving ? -1.0 : 0.0;
            return true;
        case GFXQ_W3DSLOPE: {
            if ( !whole( a[0], 0, W3D_MAXSECTORS - 1, &i1 ) || !sector_valid( w, i1 ) ) {
                return false;
            }
            const sector_t* s = &w->sectors[i1];
            *result = a[1] < 0.5 ? ( a[2] < 0.5 ? s->fgx : s->fgy ) : ( a[2] < 0.5 ? s->cgx : s->cgy );
            return true;
        }
        case GFXQ_W3DSECTORS:   *result = w->nsectors; return true;
        case GFXQ_W3DHIT:       *result = w->hit; return true;
        case GFXQ_W3DSHOTDIST:  *result = w->shotdist; return true;
        case GFXQ_W3DSHOTX:     *result = w->shotx; return true;
        case GFXQ_W3DSHOTY:     *result = w->shoty; return true;
        case GFXQ_W3DSHOTKIND:  *result = w->shotkind; return true;
        case GFXQ_W3DLIFT:      *result = w->lift; return true;
        case GFXQ_W3DFLOOR: case GFXQ_W3DCEIL: case GFXQ_W3DLIGHT: case GFXQ_W3DTAG: case GFXQ_W3DWALLS:
            if ( !whole( a[0], 0, W3D_MAXSECTORS - 1, &i1 ) || !sector_valid( w, i1 ) ) {
                return false;
            }
            switch ( q ) {
                case GFXQ_W3DFLOOR: *result = w->sectors[i1].fz; break;
                case GFXQ_W3DCEIL:  *result = w->sectors[i1].cz; break;
                case GFXQ_W3DLIGHT: *result = w->sectors[i1].light; break;
                case GFXQ_W3DTAG:   *result = w->sectors[i1].tag; break;
                default:            *result = w->sectors[i1].nwalls; break;
            }
            return true;
        case GFXQ_W3DTEXW: case GFXQ_W3DTEXH:
            if ( !whole( a[0], 0, W3D_MAXTEX - 1, &i1 ) ) {
                return false;
            }
            *result = w->texs[i1].col == 0 ? 0.0 : ( q == GFXQ_W3DTEXW ? w->texs[i1].w : w->texs[i1].h );
            return true;
        case GFXQ_W3DTON: {
            const thing_t* t = thing_get( w, a[0] );
            if ( t == 0 ) {
                return false;
            }
            *result = t->used ? -1.0 : 0.0;
            return true;
        }
        case GFXQ_W3DTX: case GFXQ_W3DTY: case GFXQ_W3DTSECTOR: case GFXQ_W3DDIST: case GFXQ_W3DBEARING: case GFXQ_W3DTZ: case GFXQ_W3DSEES: {
            const thing_t* t = thing_live( w, a[0] );
            if ( t == 0 ) {
                return false;
            }
            double dx = t->x - w->cx, dy = t->y - w->cy;
            switch ( q ) {
                case GFXQ_W3DTX:        *result = t->x; break;
                case GFXQ_W3DTY:        *result = t->y; break;
                case GFXQ_W3DTSECTOR:   *result = t->sector; break;
                case GFXQ_W3DTZ:        *result = t->zoff; break;
                case GFXQ_W3DDIST:      *result = sqrt( dx * dx + dy * dy ); break;
                case GFXQ_W3DBEARING: {
                    double fx = cos( w->angle * DEG2RAD ), fy = sin( w->angle * DEG2RAD );
                    *result = atan2( dx * fy - dy * fx, dx * fx + dy * fy ) / DEG2RAD;
                    break;
                }
                default: {
                    double dist = sqrt( dx * dx + dy * dy );
                    if ( !sector_valid( w, w->csector ) || !sector_valid( w, t->sector ) ) {
                        *result = 0.0;
                    } else if ( dist < 1.0e-6 ) {
                        *result = -1.0;
                    } else {
                        double oz = w->sectors[t->sector].fz + t->zoff + t->h * 0.7;
                        shot_t sh;
                        trace( w, t->x, t->y, oz, -dx / dist, -dy / dist, ( eye_z( w ) - oz ) / dist, t->sector, dist - 0.5, false, &sh );
                        *result = sh.kind == 0 ? -1.0 : 0.0;
                    }
                    break;
                }
            }
            return true;
        }
        case GFXQ_W3DSHOOT: {
            if ( !sector_valid( w, w->csector ) || !( a[0] > 0.0 && a[0] <= 1.0e6 ) ) {
                return false;
            }
            shot_t sh;
            trace( w, w->cx, w->cy, eye_z( w ), cos( w->angle * DEG2RAD ), sin( w->angle * DEG2RAD ), tan( w->pitch * DEG2RAD ), w->csector, a[0], true, &sh );
            w->shotkind = sh.kind; w->shotdist = sh.dist; w->shotx = sh.x; w->shoty = sh.y;
            *result = sh.kind == 2 ? sh.thing : -1;
            return true;
        }
        case GFXQ_W3DUSE: {
            if ( !sector_valid( w, w->csector ) || !( a[0] > 0.0 && a[0] <= 1.0e6 ) ) {
                return false;
            }
            double t = 0.0;
            const wall_t* wl = nearest_wall( &w->sectors[w->csector], w->cx, w->cy, cos( w->angle * DEG2RAD ), sin( w->angle * DEG2RAD ), 0.0, a[0], &t );
            *result = wl != 0 && sector_valid( w, wl->next ) ? wl->next : -1;
            return true;
        }
        default:
            return false;
    }
}
