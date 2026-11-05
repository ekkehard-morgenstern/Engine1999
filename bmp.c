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

#include "bmp.h"

// -- reading -----------------------------------------------------------------------------------------

static uint32_t rd16( const uint8_t* p ) { return (uint32_t) p[0] | ( (uint32_t) p[1] << 8 ); }
static uint32_t rd32( const uint8_t* p ) { return rd16( p ) | ( rd16( p + 2 ) << 16 ); }

// a channel under a bit mask scaled to 0..255
static uint32_t chan( uint32_t v, uint32_t mask ) {
    if ( mask == 0 ) {
        return 255;
    }
    int shift = 0;
    while ( ( mask & 1U ) == 0 ) {
        mask >>= 1; ++shift;
    }
    uint32_t x = ( v >> shift ) & mask;
    return mask == 255 ? x : ( x * 255U + mask / 2U ) / mask;
}

void bmp_free( bmp_image_t* img ) {
    free( img->argb );
    free( img->idx );
    img->argb = 0;
    img->idx = 0;
}

bool bmp_decode( const uint8_t* d, size_t len, bmp_image_t* img, const char** err ) {
    static const char* const bad = "Not a BMP file";
    memset( img, 0, sizeof(*img) );
    if ( len < 26U || d[0] != 'B' || d[1] != 'M' ) {
        *err = bad;
        return false;
    }
    uint32_t offbits = rd32( d + 10 ), dib = rd32( d + 14 );
    int64_t w, h;
    int bpp, comp = 0;
    uint32_t clrused = 0;
    if ( dib == 12U ) {
        w = rd16( d + 18 ); h = rd16( d + 20 ); bpp = (int) rd16( d + 24 );
    } else if ( dib >= 40U && 14U + dib <= len ) {
        w = (int32_t) rd32( d + 18 ); h = (int32_t) rd32( d + 22 );
        bpp = (int) rd16( d + 28 );
        comp = (int) rd32( d + 30 );
        clrused = rd32( d + 46 );
    } else {
        *err = bad;
        return false;
    }
    bool topdown = h < 0;
    if ( topdown ) {
        h = -h;
    }
    if ( w <= 0 || h <= 0 || w > BMP_MAXDIM || h > BMP_MAXDIM ) {
        *err = "Bad BMP size";
        return false;
    }
    if ( comp != 0 && comp != 3 ) {
        *err = "Compressed BMP files are not supported";
        return false;
    }
    if ( bpp != 1 && bpp != 4 && bpp != 8 && bpp != 16 && bpp != 24 && bpp != 32 ) {
        *err = "Unsupported BMP color depth";
        return false;
    }
    uint32_t mr = 0, mg = 0, mb = 0, ma = 0;
    if ( bpp == 16 && comp == 0 ) {
        mr = 0x7C00; mg = 0x3E0; mb = 0x1F;
    } else if ( bpp == 32 && comp == 0 ) {
        mr = 0xFF0000U; mg = 0xFF00U; mb = 0xFFU;
    } else if ( ( bpp == 16 || bpp == 32 ) && comp == 3 ) {
        if ( dib >= 52U && 14U + 52U <= len ) {
            mr = rd32( d + 54 ); mg = rd32( d + 58 ); mb = rd32( d + 62 );
            if ( dib >= 56U && 14U + 56U <= len ) {
                ma = rd32( d + 66 );
            }
        } else if ( dib == 40U && len >= 14U + 40U + 12U ) {
            mr = rd32( d + 54 ); mg = rd32( d + 58 ); mb = rd32( d + 62 );
        } else {
            *err = bad;
            return false;
        }
    }
    int entsz = dib == 12U ? 3 : 4;
    int npal = 0;
    if ( bpp <= 8 ) {
        npal = clrused != 0 ? (int) ( clrused > 256U ? 256U : clrused ) : 1 << bpp;
        if ( 14U + dib + (size_t) npal * (size_t) entsz > len ) {
            *err = bad;
            return false;
        }
    }
    size_t stride = ( ( (size_t) w * (size_t) bpp + 31U ) / 32U ) * 4U;
    if ( offbits > len || stride * (size_t) h > len - offbits ) {
        *err = "The BMP file is truncated";
        return false;
    }
    size_t n = (size_t) w * (size_t) h;
    img->w = (int) w; img->h = (int) h;
    img->argb = (uint32_t*) calloc( n, sizeof(uint32_t) );
    if ( bpp <= 8 ) {
        img->idx = (uint8_t*) calloc( n, 1 );
    }
    if ( img->argb == 0 || ( bpp <= 8 && img->idx == 0 ) ) {
        bmp_free( img );
        *err = "Out of memory";
        return false;
    }
    if ( npal > 0 ) {
        img->npal = npal;
        bool anyalpha = false;
        const uint8_t* p = d + 14 + dib;
        for ( int i=0; i < npal; ++i, p += entsz ) {
            if ( entsz == 4 && p[3] != 0 ) anyalpha = true;
        }
        p = d + 14 + dib;
        for ( int i=0; i < npal; ++i, p += entsz ) {
            uint32_t a = entsz == 4 && anyalpha ? p[3] : 255U;
            img->pal[i] = ( a << 24 ) | ( (uint32_t) p[2] << 16 ) | ( (uint32_t) p[1] << 8 ) | p[0];
        }
    }
    bool anya = false;
    for ( int y=0; y < img->h; ++y ) {
        const uint8_t* row = d + offbits + stride * (size_t)( topdown ? y : img->h - 1 - y );
        uint32_t* dst = img->argb + (size_t) y * (size_t) img->w;
        uint8_t* di = img->idx != 0 ? img->idx + (size_t) y * (size_t) img->w : 0;
        for ( int x=0; x < img->w; ++x ) {
            uint32_t c;
            if ( bpp <= 8 ) {
                int per = 8 / bpp;
                unsigned v = ( row[x / per] >> ( 8 - bpp - ( x % per ) * bpp ) ) & ( ( 1U << bpp ) - 1U );
                if ( (int) v >= npal ) {
                    v = 0;
                }
                di[x] = (uint8_t) v;
                c = img->pal[v];
            } else if ( bpp == 24 ) {
                const uint8_t* p = row + x * 3;
                c = 0xFF000000U | ( (uint32_t) p[2] << 16 ) | ( (uint32_t) p[1] << 8 ) | p[0];
            } else {
                uint32_t v = bpp == 16 ? rd16( row + x * 2 ) : rd32( row + x * 4 );
                uint32_t a = ma != 0 ? chan( v, ma ) : 255U;
                if ( bpp == 32 && comp == 0 ) {
                    a = v >> 24;
                    if ( a != 0 ) anya = true;
                }
                c = ( a << 24 ) | ( chan( v, mr ) << 16 ) | ( chan( v, mg ) << 8 ) | chan( v, mb );
            }
            dst[x] = c;
        }
    }
    if ( bpp == 32 && comp == 0 && !anya ) {            // (most writers leave the unused byte zero: opaque)
        for ( size_t i=0; i < n; ++i ) {
            img->argb[i] |= 0xFF000000U;
        }
    }
    return true;
}

// -- writing -----------------------------------------------------------------------------------------

static void wr16( uint8_t* p, uint32_t v ) { p[0] = (uint8_t) v; p[1] = (uint8_t)( v >> 8 ); }
static void wr32( uint8_t* p, uint32_t v ) { wr16( p, v ); wr16( p + 2, v >> 16 ); }

bool bmp_encode8( int w, int h, const uint8_t* idx, const uint32_t pal[256], uint8_t** pout, size_t* plen ) {
    if ( w < 1 || h < 1 || w > BMP_MAXDIM || h > BMP_MAXDIM ) {
        return false;
    }
    size_t stride = ( (size_t) w + 3U ) & ~(size_t) 3U, datasz = stride * (size_t) h, off = 14U + 40U + 1024U;
    uint8_t* o = (uint8_t*) calloc( 1, off + datasz );
    if ( o == 0 ) {
        return false;
    }
    o[0] = 'B'; o[1] = 'M';
    wr32( o + 2, (uint32_t)( off + datasz ) );
    wr32( o + 10, (uint32_t) off );
    wr32( o + 14, 40 );
    wr32( o + 18, (uint32_t) w ); wr32( o + 22, (uint32_t) h );
    wr16( o + 26, 1 ); wr16( o + 28, 8 );
    wr32( o + 34, (uint32_t) datasz );
    wr32( o + 38, 2835 ); wr32( o + 42, 2835 );
    wr32( o + 46, 256 ); wr32( o + 50, 256 );
    for ( int i=0; i < 256; ++i ) {
        uint8_t* p = o + 54 + i * 4;
        p[0] = (uint8_t) pal[i]; p[1] = (uint8_t)( pal[i] >> 8 ); p[2] = (uint8_t)( pal[i] >> 16 ); p[3] = (uint8_t)( pal[i] >> 24 );
    }
    for ( int y=0; y < h; ++y ) {
        memcpy( o + off + stride * (size_t)( h - 1 - y ), idx + (size_t) y * (size_t) w, (size_t) w );
    }
    *pout = o;
    *plen = off + datasz;
    return true;
}

// -- colors ------------------------------------------------------------------------------------------

int bmp_nearest( const uint32_t pal[256], int first, uint32_t argb ) {
    if ( ( argb >> 24 ) < 128U ) {
        return 0;
    }
    int r = (int)( ( argb >> 16 ) & 255U ), g = (int)( ( argb >> 8 ) & 255U ), b = (int)( argb & 255U );
    int best = first < 0 ? 0 : first, bestd = 0x7FFFFFFF;
    for ( int i = first < 0 ? 0 : first; i < 256; ++i ) {
        if ( i > 0 && ( pal[i] >> 24 ) == 0 ) {
            continue;
        }
        int dr = r - (int)( ( pal[i] >> 16 ) & 255U ), dg = g - (int)( ( pal[i] >> 8 ) & 255U ), db = b - (int)( pal[i] & 255U );
        int d = 3 * dr * dr + 4 * dg * dg + 2 * db * db;         // (the eye is most sensitive to green)
        if ( d < bestd ) {
            bestd = d; best = i;
            if ( d == 0 ) {
                break;
            }
        }
    }
    return best;
}

static int cmp_u32( const void* a, const void* b ) {
    uint32_t x = *(const uint32_t*) a, y = *(const uint32_t*) b;
    return x < y ? -1 : x > y ? 1 : 0;
}

static int sortchan;
static int cmp_chan( const void* a, const void* b ) {
    uint32_t x = *(const uint32_t*) a, y = *(const uint32_t*) b;
    int s = sortchan * 8;
    uint32_t cx = ( x >> s ) & 255U, cy = ( y >> s ) & 255U;
    return cx < cy ? -1 : cx > cy ? 1 : 0;
}

typedef struct { size_t lo, hi; } mcbox_t;

int bmp_quantize( const uint32_t* argb, size_t n, uint32_t pal[256], int first, int maxcolors ) {
    if ( first < 0 ) first = 0;
    if ( maxcolors > 256 - first ) maxcolors = 256 - first;
    if ( maxcolors < 1 ) return 0;
    // the opaque colors (a sample if there are very many pixels), without duplicates
    size_t step = n > 262144U ? n / 262144U : 1U, m = 0;
    uint32_t* c = (uint32_t*) malloc( ( n / step + 1U ) * sizeof(uint32_t) );
    if ( c == 0 ) {
        return 0;
    }
    for ( size_t i=0; i < n; i += step ) {
        if ( ( argb[i] >> 24 ) >= 128U ) {
            c[m++] = argb[i] & 0xFFFFFFU;
        }
    }
    qsort( c, m, sizeof(uint32_t), cmp_u32 );
    size_t u = 0;
    for ( size_t i=0; i < m; ++i ) {
        if ( u == 0 || c[u-1] != c[i] ) {
            c[u++] = c[i];
        }
    }
    int count = 0;
    if ( u <= (size_t) maxcolors ) {
        for ( size_t i=0; i < u; ++i ) {
            pal[first + count++] = 0xFF000000U | c[i];
        }
        free( c );
        return count;
    }
    mcbox_t boxes[256];
    int nb = 1;
    boxes[0].lo = 0; boxes[0].hi = u;
    while ( nb < maxcolors ) {
        // split the box with the largest extent in one channel (and more than one color)
        int bi = -1, bch = 0, bext = 0;
        for ( int i=0; i < nb; ++i ) {
            if ( boxes[i].hi - boxes[i].lo < 2U ) {
                continue;
            }
            for ( int ch=0; ch < 3; ++ch ) {
                int lo = 255, hi = 0;
                for ( size_t k = boxes[i].lo; k < boxes[i].hi; ++k ) {
                    int v = (int)( ( c[k] >> ( ch * 8 ) ) & 255U );
                    if ( v < lo ) lo = v;
                    if ( v > hi ) hi = v;
                }
                int ext = ( hi - lo ) * ( ch == 1 ? 4 : ch == 2 ? 2 : 3 );
                if ( ext > bext ) {
                    bext = ext; bi = i; bch = ch;
                }
            }
        }
        if ( bi < 0 ) {
            break;
        }
        sortchan = bch;
        qsort( c + boxes[bi].lo, boxes[bi].hi - boxes[bi].lo, sizeof(uint32_t), cmp_chan );
        size_t mid = boxes[bi].lo + ( boxes[bi].hi - boxes[bi].lo ) / 2U;
        boxes[nb].lo = mid; boxes[nb].hi = boxes[bi].hi;
        boxes[bi].hi = mid;
        ++nb;
    }
    for ( int i=0; i < nb; ++i ) {
        uint64_t sr = 0, sg = 0, sb = 0;
        size_t k = boxes[i].hi - boxes[i].lo;
        for ( size_t j = boxes[i].lo; j < boxes[i].hi; ++j ) {
            sr += ( c[j] >> 16 ) & 255U; sg += ( c[j] >> 8 ) & 255U; sb += c[j] & 255U;
        }
        pal[first + count++] = 0xFF000000U | ( (uint32_t)( sr / k ) << 16 ) | ( (uint32_t)( sg / k ) << 8 ) | (uint32_t)( sb / k );
    }
    free( c );
    return count;
}
