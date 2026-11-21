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


#include "world.h"
#include "randseed.h"
#include "noise.h"
#include "m3dpriv.h"
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

struct _world {
    int      fd;
    int      n;
    uint64_t seed;
    ground_t gnd;
    char     dir[1024];
};

static _Thread_local char world_err[1400];

const char* world_error( void ) {
    return world_err;
}

static void seterr( const char* what, const char* path ) {
    snprintf( world_err, sizeof(world_err), "%s %s: %s", what, path, errno ? strerror( errno ) : "failed" );
}

// -- numbers in the file (big endian) -------------------------------------------------------------------------------------------------------------------------------------
static void put32( uint8_t* p, uint32_t v ) { p[0] = (uint8_t)( v >> 24 ); p[1] = (uint8_t)( v >> 16 ); p[2] = (uint8_t)( v >> 8 ); p[3] = (uint8_t) v; }
static void put64( uint8_t* p, uint64_t v ) { put32( p, (uint32_t)( v >> 32 ) ); put32( p + 4, (uint32_t) v ); }
static uint32_t get32( const uint8_t* p ) { return ( (uint32_t) p[0] << 24 ) | ( (uint32_t) p[1] << 16 ) | ( (uint32_t) p[2] << 8 ) | (uint32_t) p[3]; }
static uint64_t get64( const uint8_t* p ) { return ( (uint64_t) get32( p ) << 32 ) | get32( p + 4 ); }

static void rec_put( uint8_t* p, const wchunk_t* c ) {
    memset( p, 0, WORLD_RECSIZE );
    put64( p, c->seed );
    put32( p + 8, (uint32_t) c->hmin ); put32( p + 12, (uint32_t) c->hmax ); put32( p + 16, (uint32_t) c->hmean );
    p[20] = c->kind; p[21] = c->flags; p[22] = c->ntar; p[23] = c->ndirt;
}

static void rec_get( const uint8_t* p, wchunk_t* c ) {
    c->seed = get64( p );
    c->hmin = (int32_t) get32( p + 8 ); c->hmax = (int32_t) get32( p + 12 ); c->hmean = (int32_t) get32( p + 16 );
    c->kind = p[20]; c->flags = p[21]; c->ntar = p[22]; c->ndirt = p[23];
}

static void hdr_put( uint8_t* h, int n, uint64_t seed, uint32_t state ) {
    memset( h, 0, WORLD_HDRSIZE );
    memcpy( h, "E99WORLD", 8 );
    put32( h + 8, WORLD_VERSION ); put32( h + 12, (uint32_t) n ); put64( h + 16, seed );
    put32( h + 24, (uint32_t) WORLD_CHUNK ); put32( h + 28, WORLD_RECSIZE ); put32( h + 32, state );
}

// -- the ground function of the world (the layers of the 1987 game: demos/1987.bas lines 3272 .. 3278, without the hand-made area) --------------------------------------------
static void ground_setup( ground_t* g, uint64_t seed ) {
    memset( g, 0, sizeof(*g) );
    // tuned for chunks of 500 m (2026-11-21): rolling ground of 150 m waves 5 m high that never dips below the water level (-12) by itself, hills of 80 m, mountain ranges of
    // 1.6 km with a mask of 6 km (peaks up to 1.9 km), lakes of about 1 km (water only where a lake is), dirt tracks every 290 m, tarred roads every 6.4 km
    g->on = true; g->seed = nz_mix( seed + 1 ); g->scale = 5000.0; g->amp = 150.0; g->oct = 4; g->base = 150.0; g->gain = 0.5; g->lac = 2.0;
    g->mnt[1].on = true; g->mnt[1].seed = nz_mix( seed + 2 ); g->mnt[1].scale = 2500.0; g->mnt[1].amp = 700.0; g->mnt[1].oct = 5; g->mnt[1].lo = 0.05; g->mnt[1].hi = 0.4;
    g->mnt[0].on = true; g->mnt[0].seed = nz_mix( seed + 3 ); g->mnt[0].scale = 50000.0; g->mnt[0].amp = 60000.0; g->mnt[0].oct = 4; g->mnt[0].lo = 0.22; g->mnt[0].hi = 0.55;
    g->mnt[0].r0 = 14000.0; g->mnt[0].r1 = 22000.0;
    g->lake.on = true; g->lake.seed = nz_mix( seed + 4 ); g->lake.scale = 40000.0; g->lake.depth = 600.0; g->lake.lo = 0.35; g->lake.hi = 0.55;
    g->net.on = true; g->net.spacing = 9000.0; g->net.chance = 0.5; g->net.wdirt = 400.0; g->net.tspacing = 200000.0; g->net.tchance = 0.85; g->net.wtar = 3000.0;
    g->quiet.on = true; g->quiet.cx = 0.0; g->quiet.cy = 0.0; g->quiet.rx = 3900.0; g->quiet.ry = 3900.0; g->quiet.margin = 1200.0;
    m3d_ground_sync_roads( g );
}

static double smooth( double t ) {
    return t <= 0.0 ? 0.0 : t >= 1.0 ? 1.0 : t * t * ( 3.0 - 2.0 * t );
}

#define WALL_START  4.0                 // chunks from the edge where the wall rises
#define WALL_WIDTH  3.0
#define WALL_HEIGHT 90000.0             // units (2.9 km)

static double height_of( const ground_t* g, int n, double x, double y ) {
    double h = m3d_ground_eval( g, x, y );
    double half = 0.5 * n * WORLD_CHUNK, d = fmax( fabs( x ), fabs( y ) ) - ( half - WALL_START * WORLD_CHUNK );
    if ( d > 0.0 ) {                    // the wall of mountains round the world
        h += WALL_HEIGHT * smooth( d / ( WALL_WIDTH * WORLD_CHUNK ) );
    }
    return h;
}

double world_height( const world_t* w, double x, double y ) {
    return height_of( &w->gnd, w->n, x, y );
}

double world_x0( const world_t* w, int cx ) { return ( (double) cx - 0.5 * w->n ) * WORLD_CHUNK; }
double world_y0( const world_t* w, int cy ) { return ( (double) cy - 0.5 * w->n ) * WORLD_CHUNK; }

bool world_chunk_at( const world_t* w, double x, double y, int* cx, int* cy ) {
    double fx = floor( x / WORLD_CHUNK + 0.5 * w->n ), fy = floor( y / WORLD_CHUNK + 0.5 * w->n );
    if ( !( fx >= 0.0 && fx < w->n && fy >= 0.0 && fy < w->n ) ) {
        return false;
    }
    *cx = (int) fx; *cy = (int) fy;
    return true;
}

uint64_t world_field_seed( uint64_t seed, int cx, int cy ) {
    randseed_t s;
    rs_seed( &s, seed );
    for ( int i=0; i < cy; ++i ) { rs_south( &s ); }
    for ( int i=0; i < cx; ++i ) { rs_east( &s ); }
    return rs_fold( &s );
}

// -- the summary of one chunk ---------------------------------------------------------------------------------------------------------------------------------------------
// (the heights are those of the ground without the road network's cuts and keep-away rules: 20 times cheaper, and a summary does not need them; the dirt tracks cover every
// chunk, so only the tarred roads are counted)
static void summarize( const ground_t* g, const ground_t* gflat, int n, int cx, int cy, uint64_t cseed, wchunk_t* c ) {
    double x0 = ( (double) cx - 0.5 * n ) * WORLD_CHUNK, y0 = ( (double) cy - 0.5 * n ) * WORLD_CHUNK;
    double lo = 1.0e30, hi = -1.0e30, sum = 0.0;
    memset( c, 0, sizeof(*c) );
    c->seed = cseed;
    for ( int j=0; j < 5; ++j ) {
        for ( int i=0; i < 5; ++i ) {
            double h = height_of( gflat, n, x0 + i * ( WORLD_CHUNK / 4.0 ), y0 + j * ( WORLD_CHUNK / 4.0 ) );
            if ( h < lo ) { lo = h; }
            if ( h > hi ) { hi = h; }
            sum += h;
        }
    }
    c->hmin = (int32_t) floor( lo ); c->hmax = (int32_t) ceil( hi ); c->hmean = (int32_t) floor( sum / 25.0 );
    if ( lo < WORLD_WATERZ - 3.0 ) { c->flags |= WCF_WATER; }
    if ( hi > WORLD_MOUNTH ) { c->flags |= WCF_MOUNTAIN; }
    double half = 0.5 * n * WORLD_CHUNK;
    if ( fmax( fabs( x0 + 0.5 * WORLD_CHUNK ), fabs( y0 + 0.5 * WORLD_CHUNK ) ) > half - WALL_START * WORLD_CHUNK ) { c->flags |= WCF_EDGE; }
    c->kind = (uint8_t) rnet_kind_cold( &g->net, x0 + 0.5 * WORLD_CHUNK, y0 + 0.5 * WORLD_CHUNK );
    // tarred roads through the chunk: the links whose way comes within the chunk's half diagonal of its middle (checked on 32 points of the way)
    static _Thread_local rlink_t links[64];
    double mx = x0 + 0.5 * WORLD_CHUNK, my = y0 + 0.5 * WORLD_CHUNK, reach = 0.72 * WORLD_CHUNK;
    rnear_t rn;
    rnet_near( &g->net, mx, my, reach, &rn );
    if ( rn.tar < reach ) {
        int nl = rnet_links( &g->net, 1, mx - reach, my - reach, mx + reach, my + reach, links, 64 ), nt = 0;
        for ( int i=0; i < nl && i < 64; ++i ) {
            for ( int k=0; k <= 32; ++k ) {
                double px, py;
                rnet_point( &links[i], k / 32.0, &px, &py );
                if ( fmax( fabs( px - mx ), fabs( py - my ) ) < 0.5 * WORLD_CHUNK ) { ++nt; break; }
            }
        }
        c->ntar = (uint8_t) nt;
        if ( nt > 0 ) { c->flags |= WCF_TAR; }
    }
    if ( g->net.spacing > 0.0 ) { c->flags |= WCF_DIRT; }
}

// -- creation -------------------------------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    int             fd, n;
    const uint64_t* seeds;
    ground_t        gnd, gflat;
    _Atomic int     next;               // the next row to make
    _Atomic int     done;
    _Atomic int     fail;
} wjob_t;

static void* worker( void* arg ) {
    wjob_t* j = (wjob_t*) arg;
    uint8_t* row = (uint8_t*) malloc( (size_t) j->n * WORLD_RECSIZE );
    if ( row == 0 ) {
        j->fail = 1;
        return 0;
    }
    for ( ;; ) {
        int y = j->next++;
        if ( y >= j->n || j->fail ) {
            break;
        }
        for ( int x=0; x < j->n; ++x ) {
            wchunk_t c;
            summarize( &j->gnd, &j->gflat, j->n, x, y, j->seeds[ (size_t) y * (size_t) j->n + (size_t) x ], &c );
            rec_put( row + (size_t) x * WORLD_RECSIZE, &c );
        }
        off_t off = (off_t) WORLD_HDRSIZE + (off_t) y * (off_t) j->n * WORLD_RECSIZE;
        size_t len = (size_t) j->n * WORLD_RECSIZE, at = 0;
        while ( at < len ) {
            ssize_t r = pwrite( j->fd, row + at, len - at, off + (off_t) at );
            if ( r <= 0 ) { j->fail = 1; break; }
            at += (size_t) r;
        }
        ++j->done;
    }
    free( row );
    rnet_flush();                                       // (the road cache of this thread)
    return 0;
}

world_t* world_create( const char* dir, int n, uint64_t seed, int threads, world_progress_t cb, void* ud ) {
    world_err[0] = 0;
    if ( n < 1 || n > WORLD_MAXN ) {
        snprintf( world_err, sizeof(world_err), "world size %d: must be 1 .. %d", n, WORLD_MAXN );
        return 0;
    }
    if ( strlen( dir ) + 16 >= sizeof( ((world_t*) 0)->dir ) ) {
        snprintf( world_err, sizeof(world_err), "folder name too long" );
        return 0;
    }
    errno = 0;
    if ( mkdir( dir, 0777 ) != 0 && errno != EEXIST ) {
        seterr( "cannot make folder", dir );
        return 0;
    }
    char path[1100];
    snprintf( path, sizeof(path), "%s/world.dat", dir );
    int fd = open( path, O_RDWR | O_CREAT | O_TRUNC, 0666 );
    if ( fd < 0 ) {
        seterr( "cannot create", path );
        return 0;
    }
    uint8_t hdr[WORLD_HDRSIZE];
    hdr_put( hdr, n, seed, 0 );                         // state 0: not finished
    if ( pwrite( fd, hdr, WORLD_HDRSIZE, 0 ) != WORLD_HDRSIZE ) {
        seterr( "cannot write", path );
        close( fd );
        return 0;
    }
    // the seeds: the random field walked row by row (the state of a row's start is the previous row's, one step south)
    size_t cnt = (size_t) n * (size_t) n;
    uint64_t* seeds = (uint64_t*) malloc( cnt * sizeof(uint64_t) );
    wjob_t* j = (wjob_t*) calloc( 1, sizeof(wjob_t) );
    if ( seeds == 0 || j == 0 ) {
        snprintf( world_err, sizeof(world_err), "out of memory" );
        free( seeds ); free( j ); close( fd );
        return 0;
    }
    randseed_t rowst, cell;
    rs_seed( &rowst, seed );
    for ( int y=0; y < n; ++y ) {
        if ( y > 0 ) { rs_south( &rowst ); }
        cell = rowst;
        for ( int x=0; x < n; ++x ) {
            if ( x > 0 ) { rs_east( &cell ); }
            seeds[ (size_t) y * (size_t) n + (size_t) x ] = rs_fold( &cell );
        }
    }
    j->fd = fd; j->n = n; j->seeds = seeds;
    ground_setup( &j->gnd, seed );
    j->gflat = j->gnd; j->gflat.net.on = false;
    if ( threads <= 0 ) {
        long np = sysconf( _SC_NPROCESSORS_ONLN );
        threads = np < 1 ? 1 : np > 16 ? 16 : (int) np;
    }
    if ( threads > n ) { threads = n; }
    pthread_t th[16];
    int started = 0;
    if ( threads > 16 ) { threads = 16; }
    for ( int i=0; i < threads; ++i ) {
        if ( pthread_create( &th[started], 0, worker, j ) == 0 ) { ++started; }
    }
    if ( started == 0 ) {                               // no threads: do it here
        worker( j );
    }
    while ( j->done < n && !j->fail ) {
        if ( cb != 0 ) { cb( ud, j->done, n ); }
        struct timespec ts = { 0, 50 * 1000 * 1000 };
        nanosleep( &ts, 0 );
    }
    for ( int i=0; i < started; ++i ) {
        pthread_join( th[i], 0 );
    }
    if ( cb != 0 ) { cb( ud, j->done, n ); }
    bool ok = !j->fail && j->done == n;
    free( seeds ); free( j );
    if ( ok ) {
        hdr_put( hdr, n, seed, 1 );                     // finished
        ok = pwrite( fd, hdr, WORLD_HDRSIZE, 0 ) == WORLD_HDRSIZE && fsync( fd ) == 0;
    }
    close( fd );
    if ( !ok ) {
        seterr( "cannot write", path );
        return 0;
    }
    return world_open( dir );
}

world_t* world_open( const char* dir ) {
    world_err[0] = 0;
    world_t* w = (world_t*) calloc( 1, sizeof(world_t) );
    if ( w == 0 ) { snprintf( world_err, sizeof(world_err), "out of memory" ); return 0; }
    if ( strlen( dir ) + 16 >= sizeof(w->dir) ) { snprintf( world_err, sizeof(world_err), "folder name too long" ); free( w ); return 0; }
    snprintf( w->dir, sizeof(w->dir), "%s", dir );
    char path[1100];
    snprintf( path, sizeof(path), "%s/world.dat", dir );
    errno = 0;
    w->fd = open( path, O_RDONLY );
    if ( w->fd < 0 ) {
        seterr( "cannot open", path );
        free( w );
        return 0;
    }
    uint8_t hdr[WORLD_HDRSIZE];
    if ( pread( w->fd, hdr, WORLD_HDRSIZE, 0 ) != WORLD_HDRSIZE || memcmp( hdr, "E99WORLD", 8 ) != 0 ) {
        snprintf( world_err, sizeof(world_err), "%s is not a world file", path );
        close( w->fd ); free( w );
        return 0;
    }
    uint32_t ver = get32( hdr + 8 ), n = get32( hdr + 12 );
    if ( ver != WORLD_VERSION || n < 1 || n > WORLD_MAXN || get32( hdr + 24 ) != (uint32_t) WORLD_CHUNK || get32( hdr + 28 ) != WORLD_RECSIZE ) {
        snprintf( world_err, sizeof(world_err), "%s: unsupported world version", path );
        close( w->fd ); free( w );
        return 0;
    }
    if ( get32( hdr + 32 ) != 1 ) {
        snprintf( world_err, sizeof(world_err), "%s: the world was not finished (make a new one)", path );
        close( w->fd ); free( w );
        return 0;
    }
    struct stat st;
    if ( fstat( w->fd, &st ) != 0 || (uint64_t) st.st_size < (uint64_t) WORLD_HDRSIZE + (uint64_t) n * n * WORLD_RECSIZE ) {
        snprintf( world_err, sizeof(world_err), "%s is truncated", path );
        close( w->fd ); free( w );
        return 0;
    }
    w->n = (int) n; w->seed = get64( hdr + 16 );
    ground_setup( &w->gnd, w->seed );
    return w;
}

void world_close( world_t* w ) {
    if ( w != 0 ) {
        close( w->fd );
        free( w );
    }
}

int world_size( const world_t* w ) { return w->n; }
uint64_t world_seed( const world_t* w ) { return w->seed; }

bool world_chunk( const world_t* w, int cx, int cy, wchunk_t* out ) {
    if ( cx < 0 || cy < 0 || cx >= w->n || cy >= w->n ) {
        return false;
    }
    uint8_t rec[WORLD_RECSIZE];
    off_t off = (off_t) WORLD_HDRSIZE + ( (off_t) cy * w->n + cx ) * WORLD_RECSIZE;
    if ( pread( w->fd, rec, WORLD_RECSIZE, off ) != WORLD_RECSIZE ) {
        return false;
    }
    rec_get( rec, out );
    return true;
}

const ground_t* world_ground( const world_t* w ) {
    return &w->gnd;
}

// -- creation in the background -------------------------------------------------------------------------------------------------------------------------------------------
struct _wmake {
    pthread_t   th;
    char        dir[1024];
    int         n, threads;
    uint64_t    seed;
    _Atomic int done, total, ended;
    world_t*    result;
    char        err[1400];
};

static void make_cb( void* ud, int done, int total ) {
    wmake_t* m = (wmake_t*) ud;
    m->done = done; m->total = total;
}

static void* make_main( void* arg ) {
    wmake_t* m = (wmake_t*) arg;
    m->result = world_create( m->dir, m->n, m->seed, m->threads, make_cb, m );
    if ( m->result == 0 ) {
        snprintf( m->err, sizeof(m->err), "%s", world_err );
    }
    m->ended = 1;
    return 0;
}

wmake_t* world_make_start( const char* dir, int n, uint64_t seed, int threads ) {
    wmake_t* m = (wmake_t*) calloc( 1, sizeof(wmake_t) );
    if ( m == 0 ) { return 0; }
    snprintf( m->dir, sizeof(m->dir), "%s", dir );
    m->n = n; m->seed = seed; m->threads = threads; m->total = n;
    if ( pthread_create( &m->th, 0, make_main, m ) != 0 ) {
        free( m );
        return 0;
    }
    return m;
}

int world_make_progress( wmake_t* m, int* total ) {
    *total = m->total;
    return ( m->ended && m->result == 0 ) ? -1 : m->done;
}

bool world_make_done( wmake_t* m ) {
    return m->ended != 0;
}

world_t* world_make_finish( wmake_t* m ) {
    pthread_join( m->th, 0 );
    world_t* w = m->result;
    if ( w == 0 ) {
        snprintf( world_err, sizeof(world_err), "%s", m->err );
    }
    free( m );
    return w;
}
