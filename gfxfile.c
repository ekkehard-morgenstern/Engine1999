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

#include "gfxfile.h"
#include "bmp.h"
#include "tilescreen.h"
#include "sprscreen.h"

#if TILE_WIDTH != GFXFILE_CELLW || TILE_HEIGHT != GFXFILE_CELLH || SPRITE_WIDTH != GFXFILE_CELLW || SPRITE_HEIGHT != GFXFILE_CELLH
#error "gfxfile.h: the cell size differs from the tiles and sprites"
#endif

static int kind_layer( int kind ) {
    return kind == GFXFILE_SPRITE ? 4 : 1;
}

static void read_palette( const gfxfile_host_t* h, int layer, uint32_t pal[256] ) {
    for ( int i=0; i < 256; ++i ) {
        pal[i] = h->palget( h->ud, layer, i );
    }
}

// The pixels of the picture as indices into the layer's palette: with setpal the palette of the file (or a quantized one) is installed first.
// Returns a malloc()ed w * h array, 0 if out of memory.
static uint8_t* map_image( const gfxfile_host_t* h, int layer, const bmp_image_t* img, bool setpal ) {
    size_t n = (size_t) img->w * (size_t) img->h;
    uint8_t* out = (uint8_t*) malloc( n );
    if ( out == 0 ) {
        return 0;
    }
    uint32_t pal[256];
    int first = 1;
    if ( setpal ) {
        if ( img->idx != 0 ) {
            for ( int i=0; i < img->npal; ++i ) {
                // (entry 0 stays transparent: it is "nothing" on every layer, an opaque color there would cover the whole layer)
                h->palset( h->ud, layer, i, i == 0 ? ( img->pal[i] & 0x00FFFFFFU ) : img->pal[i] );
            }
            memcpy( out, img->idx, n );
            return out;
        }
        memset( pal, 0, sizeof(pal) );
        first = 1;                                  // (entry 0 is never given a color: transparent, as on every layer)
        int cnt = bmp_quantize( img->argb, n, pal, first, 256 - first );
        for ( int i=0; i < cnt; ++i ) {
            h->palset( h->ud, layer, first + i, pal[first + i] );
        }
        if ( cnt == 0 ) {
            first = 1;
        }
    }
    read_palette( h, layer, pal );
    uint32_t lastc = 0; int lasti = 0; bool have = false;
    for ( size_t i=0; i < n; ++i ) {
        uint32_t c = img->argb[i];
        if ( !have || c != lastc ) {
            lastc = c; lasti = bmp_nearest( pal, first, c); have = true;
        }
        out[i] = (uint8_t) lasti;
    }
    return out;
}

bool gfxfile_load_layer( const gfxfile_host_t* h, int layer, const uint8_t* file, size_t len, int x, int y, bool setpal, const char** err ) {
    bmp_image_t img;
    if ( !bmp_decode( file, len, &img, err ) ) {
        return false;
    }
    uint8_t* px = map_image( h, layer, &img, setpal );
    if ( px == 0 ) {
        bmp_free( &img );
        *err = "Out of memory";
        return false;
    }
    for ( int j=0; j < img.h; ++j ) {
        int yy = y + j;
        if ( yy < 0 || yy >= GFXFILE_LH ) {
            continue;
        }
        for ( int i=0; i < img.w; ++i ) {
            int xx = x + i;
            if ( xx >= 0 && xx < GFXFILE_LW ) {
                h->pset( h->ud, layer, xx, yy, px[(size_t) j * (size_t) img.w + (size_t) i] );
            }
        }
    }
    free( px );
    bmp_free( &img );
    return true;
}

bool gfxfile_save_layer( const gfxfile_host_t* h, int layer, int x1, int y1, int x2, int y2, uint8_t** pout, size_t* plen, const char** err ) {
    if ( x1 > x2 ) { int t = x1; x1 = x2; x2 = t; }
    if ( y1 > y2 ) { int t = y1; y1 = y2; y2 = t; }
    if ( x1 < 0 ) x1 = 0;
    if ( y1 < 0 ) y1 = 0;
    if ( x2 >= GFXFILE_LW ) x2 = GFXFILE_LW - 1;
    if ( y2 >= GFXFILE_LH ) y2 = GFXFILE_LH - 1;
    if ( x1 > x2 || y1 > y2 ) {
        *err = "Illegal argument";
        return false;
    }
    int w = x2 - x1 + 1, hh = y2 - y1 + 1;
    uint8_t* px = (uint8_t*) malloc( (size_t) w * (size_t) hh );
    if ( px == 0 ) {
        *err = "Out of memory";
        return false;
    }
    for ( int j=0; j < hh; ++j ) {
        for ( int i=0; i < w; ++i ) {
            int c = h->point( h->ud, layer, x1 + i, y1 + j );
            px[(size_t) j * (size_t) w + (size_t) i] = (uint8_t)( c < 0 ? 0 : c );
        }
    }
    uint32_t pal[256];
    read_palette( h, layer, pal );
    bool ok = bmp_encode8( w, hh, px, pal, pout, plen );
    free( px );
    if ( !ok ) {
        *err = "Out of memory";
    }
    return ok;
}

bool gfxfile_load_cells( const gfxfile_host_t* h, int kind, int first, int count, const uint8_t* file, size_t len, bool setpal, const char** err ) {
    int limit = kind == GFXFILE_SPRITE ? GFXFILE_SPRMAX : GFXFILE_TILEMAX;
    if ( first < 0 || first >= limit ) {
        *err = "Illegal argument";
        return false;
    }
    bmp_image_t img;
    if ( !bmp_decode( file, len, &img, err ) ) {
        return false;
    }
    int cols = img.w / GFXFILE_CELLW, rows = img.h / GFXFILE_CELLH;
    if ( cols < 1 || rows < 1 ) {
        bmp_free( &img );
        *err = "The picture is smaller than one image (16 x 24)";
        return false;
    }
    int avail = cols * rows;
    if ( count < 0 ) {
        count = avail < limit - first ? avail : limit - first;
    } else if ( count > avail || first + count > limit ) {
        bmp_free( &img );
        *err = "Illegal argument (the picture has fewer images or they do not fit)";
        return false;
    }
    int layer = kind_layer( kind );
    uint8_t* px = map_image( h, layer, &img, setpal );
    if ( px == 0 ) {
        bmp_free( &img );
        *err = "Out of memory";
        return false;
    }
    uint8_t cell[GFXFILE_CELLW * GFXFILE_CELLH];
    for ( int k=0; k < count; ++k ) {
        int cx = ( k % cols ) * GFXFILE_CELLW, cy = ( k / cols ) * GFXFILE_CELLH;
        for ( int j=0; j < GFXFILE_CELLH; ++j ) {
            memcpy( cell + j * GFXFILE_CELLW, px + (size_t)( cy + j ) * (size_t) img.w + (size_t) cx, GFXFILE_CELLW );
        }
        h->putcell( h->ud, kind, first + k, cell );
    }
    free( px );
    bmp_free( &img );
    return true;
}

bool gfxfile_save_cells( const gfxfile_host_t* h, int kind, int first, int count, int cols, uint8_t** pout, size_t* plen, const char** err ) {
    int limit = kind == GFXFILE_SPRITE ? GFXFILE_SPRMAX : GFXFILE_TILEMAX;
    if ( first < 0 || count < 1 || first + count > limit ) {
        *err = "Illegal argument";
        return false;
    }
    if ( cols < 1 ) {
        cols = count < 16 ? count : 16;
    }
    if ( cols > count ) {
        cols = count;
    }
    int rows = ( count + cols - 1 ) / cols, w = cols * GFXFILE_CELLW, hh = rows * GFXFILE_CELLH;
    if ( w > BMP_MAXDIM || hh > BMP_MAXDIM ) {
        *err = "Illegal argument";
        return false;
    }
    uint8_t* px = (uint8_t*) calloc( (size_t) w * (size_t) hh, 1 );
    if ( px == 0 ) {
        *err = "Out of memory";
        return false;
    }
    uint8_t cell[GFXFILE_CELLW * GFXFILE_CELLH];
    for ( int k=0; k < count; ++k ) {
        if ( !h->getcell( h->ud, kind, first + k, cell ) ) {
            memset( cell, 0, sizeof(cell) );
        }
        int cx = ( k % cols ) * GFXFILE_CELLW, cy = ( k / cols ) * GFXFILE_CELLH;
        for ( int j=0; j < GFXFILE_CELLH; ++j ) {
            memcpy( px + (size_t)( cy + j ) * (size_t) w + (size_t) cx, cell + j * GFXFILE_CELLW, GFXFILE_CELLW );
        }
    }
    uint32_t pal[256];
    read_palette( h, kind_layer( kind ), pal );
    bool ok = bmp_encode8( w, hh, px, pal, pout, plen );
    free( px );
    if ( !ok ) {
        *err = "Out of memory";
    }
    return ok;
}

bool gfxfile_load_palette( const gfxfile_host_t* h, int layer, const uint8_t* file, size_t len, const char** err ) {
    bmp_image_t img;
    if ( !bmp_decode( file, len, &img, err ) ) {
        return false;
    }
    if ( img.idx != 0 ) {
        for ( int i=0; i < img.npal; ++i ) {
            h->palset( h->ud, layer, i, img.pal[i] );
        }
    } else {
        uint32_t pal[256];
        memset( pal, 0, sizeof(pal) );
        size_t n = (size_t) img.w * (size_t) img.h;
        int cnt = bmp_quantize( img.argb, n, pal, 1, 255 );
        for ( int i=0; i < cnt; ++i ) {
            h->palset( h->ud, layer, 1 + i, pal[1 + i] );
        }
    }
    bmp_free( &img );
    return true;
}

bool gfxfile_save_palette( const gfxfile_host_t* h, int layer, uint8_t** pout, size_t* plen, const char** err ) {
    uint8_t px[256];
    uint32_t pal[256];
    for ( int i=0; i < 256; ++i ) {
        px[i] = (uint8_t) i;
    }
    read_palette( h, layer, pal );
    if ( !bmp_encode8( 16, 16, px, pal, pout, plen ) ) {
        *err = "Out of memory";
        return false;
    }
    return true;
}
