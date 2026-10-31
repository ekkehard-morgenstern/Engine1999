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

    NOTE: Programs created with a built-in programming language (if any),
          do not fall under this license.

    CONTACT INFO:
        E-Mail: ekkehard@ekkehardmorgenstern.de
        Mail: Ekkehard Morgenstern, Mozartstr. 1, D-76744 Woerth am Rhein,
              Germany, Europe
*/


#include <math.h>
#include "gfxcore.h"

void gfx_init( gfx_t* g, uint8_t* buf, int w, int h ) {
    memset( g, 0, sizeof(*g) );
    g->buf = buf; g->w = w; g->h = h;
    g->mode = GFXM_SET;
    g->pen = 1;
    g->drawcolor = -1;
    g->drawscale = 4;
}

void gfx_clear( gfx_t* g, int color ) {
    memset( g->buf, color < 0 ? g->pen : color & 0xFF, (size_t) g->w * (size_t) g->h );
}

static void gfx_put( gfx_t* g, int x, int y, int c ) {
    if ( x < 0 || y < 0 || x >= g->w || y >= g->h ) {
        return;
    }
    uint8_t* p = &g->buf[ (size_t) y * (size_t) g->w + (size_t) x ];
    switch ( g->mode ) {
        case GFXM_CLEAR:    *p = 0; break;
        case GFXM_XOR:      *p ^= (uint8_t) c; break;
        case GFXM_OR:       *p |= (uint8_t) c; break;
        case GFXM_AND:      *p &= (uint8_t) c; break;
        default:            *p = (uint8_t) c; break;
    }
}

void gfx_pset( gfx_t* g, int x, int y, int color ) {
    gfx_put( g, x, y, color < 0 ? g->pen : color & 0xFF );
}

int gfx_point( const gfx_t* g, int x, int y ) {
    if ( x < 0 || y < 0 || x >= g->w || y >= g->h ) {
        return -1;
    }
    return g->buf[ (size_t) y * (size_t) g->w + (size_t) x ];
}

void gfx_line( gfx_t* g, int x1, int y1, int x2, int y2, int color ) {
    int c = color < 0 ? g->pen : color & 0xFF;
    int dx = x2 > x1 ? x2 - x1 : x1 - x2, sx = x1 < x2 ? 1 : -1;
    int dy = y2 > y1 ? y1 - y2 : y2 - y1, sy = y1 < y2 ? 1 : -1;     // dy is negative
    int err = dx + dy;
    // a line far outside of the bitmap would take long to walk: limit it to a sane range first
    if ( dx > 100000 || -dy > 100000 ) {
        return;
    }
    for (;;) {
        gfx_put( g, x1, y1, c );
        if ( x1 == x2 && y1 == y2 ) {
            break;
        }
        int e2 = 2 * err;
        if ( e2 >= dy ) { err += dy; x1 += sx; }
        if ( e2 <= dx ) { err += dx; y1 += sy; }
    }
}

void gfx_box( gfx_t* g, int x1, int y1, int x2, int y2, int color, bool fill ) {
    int c = color < 0 ? g->pen : color & 0xFF;
    if ( x1 > x2 ) { int t = x1; x1 = x2; x2 = t; }
    if ( y1 > y2 ) { int t = y1; y1 = y2; y2 = t; }
    if ( fill ) {
        int ya = y1 < 0 ? 0 : y1, yb = y2 >= g->h ? g->h - 1 : y2;
        int xa = x1 < 0 ? 0 : x1, xb = x2 >= g->w ? g->w - 1 : x2;
        for ( int y = ya; y <= yb; ++y ) {
            for ( int x = xa; x <= xb; ++x ) {
                gfx_put( g, x, y, c );
            }
        }
        return;
    }
    gfx_line( g, x1, y1, x2, y1, c );
    gfx_line( g, x1, y2, x2, y2, c );
    if ( y2 - y1 > 1 ) {
        gfx_line( g, x1, y1 + 1, x1, y2 - 1, c );
        gfx_line( g, x2, y1 + 1, x2, y2 - 1, c );
    }
}

void gfx_circle( gfx_t* g, int x, int y, int r, int color, int a1, int a2, double ratio ) {
    int c = color < 0 ? g->pen : color & 0xFF;
    if ( r < 0 || r > 20000 ) {
        return;
    }
    if ( !( ratio > 0.0 ) || ratio > 1000.0 ) {
        ratio = 1.0;
    }
    bool whole = a1 == a2 || ( a1 < 0 && a2 < 0 );
    double start = whole ? 0.0 : (double) a1, end = whole ? 360.0 : (double) a2;
    if ( !whole && end <= start ) {
        end += 360.0;
    }
    if ( r == 0 ) {
        gfx_put( g, x, y, c );
        return;
    }
    double rx = (double) r, ry = (double) r * ratio;
    double big = rx > ry ? rx : ry;
    int steps = (int)( big * 6.2832 * ( ( end - start ) / 360.0 ) ) + 8;       // about one step per pixel of arc, a few more to close it
    if ( steps > 200000 ) {
        steps = 200000;
    }
    int px = 0, py = 0;
    for ( int i=0; i <= steps; ++i ) {
        double a = ( start + ( end - start ) * (double) i / (double) steps ) * ( 3.14159265358979323846 / 180.0 );
        int cx = x + (int) lround( rx * cos( a ) ), cy = y - (int) lround( ry * sin( a ) );
        if ( i == 0 ) {
            gfx_put( g, cx, cy, c );
        } else if ( cx != px || cy != py ) {
            gfx_line( g, px, py, cx, cy, c );
        }
        px = cx; py = cy;
    }
}

void gfx_radius( gfx_t* g, int x, int y, int r, double angle, int color ) {
    double a = angle * ( 3.14159265358979323846 / 180.0 );
    gfx_line( g, x, y, x + (int) lround( (double) r * cos( a ) ), y - (int) lround( (double) r * sin( a ) ), color );
}

typedef struct _gfxspan_t { int x1, x2, y; } gfxspan_t;

void gfx_paint( gfx_t* g, int x, int y, int color, int border ) {
    int c = color < 0 ? g->pen : color & 0xFF;
    int seed = gfx_point( g, x, y );
    if ( seed < 0 ) {
        return;
    }
    bool byborder = border >= 0;
    int b = border & 0xFF;
    if ( !byborder && seed == c ) {
        return;
    }
    if ( byborder && ( seed == b || seed == c ) ) {
        return;
    }
    // a pixel can be filled if it is of the seed color (area mode) or neither the border nor the fill color (border mode)
    #define GFX_INSIDE(px) ( byborder ? ( (px) != b && (px) != c ) : (px) == seed )
    size_t cap = 256, n = 0;
    gfxspan_t* stack = (gfxspan_t*) malloc( cap * sizeof(*stack) );
    if ( stack == 0 ) {
        return;
    }
    stack[n++] = (gfxspan_t){ x, x, y };
    while ( n > 0 ) {
        gfxspan_t s = stack[--n];
        int xl = s.x1;
        if ( !GFX_INSIDE( g->buf[ (size_t) s.y * (size_t) g->w + (size_t) xl ] ) ) {
            continue;
        }
        while ( xl > 0 && GFX_INSIDE( g->buf[ (size_t) s.y * (size_t) g->w + (size_t)( xl - 1 ) ] ) ) {
            --xl;
        }
        int xr = s.x1;
        while ( xr < g->w - 1 && GFX_INSIDE( g->buf[ (size_t) s.y * (size_t) g->w + (size_t)( xr + 1 ) ] ) ) {
            ++xr;
        }
        for ( int i = xl; i <= xr; ++i ) {
            g->buf[ (size_t) s.y * (size_t) g->w + (size_t) i ] = (uint8_t) c;
        }
        for ( int dy = -1; dy <= 1; dy += 2 ) {
            int yy = s.y + dy;
            if ( yy < 0 || yy >= g->h ) {
                continue;
            }
            bool inrun = false;
            for ( int i = xl; i <= xr; ++i ) {
                bool in = GFX_INSIDE( g->buf[ (size_t) yy * (size_t) g->w + (size_t) i ] );
                if ( in && !inrun ) {
                    if ( n == cap ) {
                        gfxspan_t* bigger = (gfxspan_t*) realloc( stack, cap * 2U * sizeof(*stack) );
                        if ( bigger == 0 ) {
                            free( stack );
                            return;
                        }
                        stack = bigger; cap *= 2U;
                    }
                    stack[n++] = (gfxspan_t){ i, i, yy };
                }
                inrun = in;
            }
        }
    }
    free( stack );
    #undef GFX_INSIDE
}

size_t gfx_grab( const gfx_t* g, int x1, int y1, int x2, int y2, uint8_t* out, size_t outsize ) {
    if ( x1 > x2 ) { int t = x1; x1 = x2; x2 = t; }
    if ( y1 > y2 ) { int t = y1; y1 = y2; y2 = t; }
    int w = x2 - x1 + 1, h = y2 - y1 + 1;
    if ( w > 65535 || h > 65535 ) {
        return 0;
    }
    size_t size = 4U + (size_t) w * (size_t) h;
    if ( out == 0 || size > outsize ) {
        return size;
    }
    out[0] = (uint8_t)( w >> 8 ); out[1] = (uint8_t) w;
    out[2] = (uint8_t)( h >> 8 ); out[3] = (uint8_t) h;
    uint8_t* d = out + 4;
    for ( int y = y1; y <= y2; ++y ) {
        for ( int x = x1; x <= x2; ++x ) {
            int p = gfx_point( g, x, y );
            *d++ = (uint8_t)( p < 0 ? 0 : p );
        }
    }
    return size;
}

bool gfx_blit( gfx_t* g, const uint8_t* block, size_t size, int x, int y, bool transparent ) {
    if ( size < 4U ) {
        return false;
    }
    int w = ( block[0] << 8 ) | block[1], h = ( block[2] << 8 ) | block[3];
    if ( (size_t) w * (size_t) h + 4U > size ) {
        return false;
    }
    const uint8_t* s = block + 4;
    for ( int j=0; j < h; ++j ) {
        for ( int i=0; i < w; ++i, ++s ) {
            if ( transparent && *s == 0 ) {
                continue;
            }
            gfx_put( g, x + i, y + j, *s );
        }
    }
    return true;
}

void gfx_text( gfx_t* g, int x, int y, const char* text, size_t len, int color, int sx, int sy, const uint8_t* font ) {
    int c = color < 0 ? g->pen : color & 0xFF;
    if ( sx < 1 ) sx = 1;
    if ( sy < 1 ) sy = 1;
    if ( sx > 32 ) sx = 32;
    if ( sy > 32 ) sy = 32;
    for ( size_t k=0; k < len; ++k ) {
        const uint8_t* glyph = font + (size_t)(uint8_t) text[k] * 12U;
        for ( int row=0; row < 12; ++row ) {
            for ( int col=0; col < 8; ++col ) {
                if ( ( glyph[row] & ( 0x80 >> col ) ) == 0 ) {
                    continue;
                }
                for ( int j=0; j < sy; ++j ) {
                    for ( int i=0; i < sx; ++i ) {
                        gfx_put( g, x + ( (int) k * 8 + col ) * sx + i, y + row * sy + j, c );
                    }
                }
            }
        }
    }
}

// -- DRAW ----------------------------------------------------------------------------------

typedef struct _drawp_t {
    const char* s;
    size_t      n;
} drawp_t;

static void draw_skip( drawp_t* p ) {
    while ( p->n > 0 && ( *p->s == ' ' || *p->s == ';' || *p->s == '\t' ) ) {
        ++p->s; --p->n;
    }
}

// an optional signed integer; *given tells whether there was one
static int draw_num( drawp_t* p, int def, bool* given, bool* signedn ) {
    draw_skip( p );
    int sign = 1;
    bool sg = false;
    if ( p->n > 0 && ( *p->s == '+' || *p->s == '-' ) ) {
        sign = *p->s == '-' ? -1 : 1;
        sg = true;
        ++p->s; --p->n;
    }
    long v = 0; bool any = false;
    while ( p->n > 0 && *p->s >= '0' && *p->s <= '9' ) {
        v = v * 10 + ( *p->s - '0' );
        if ( v > 1000000 ) v = 1000000;
        any = true;
        ++p->s; --p->n;
    }
    if ( given ) *given = any;
    if ( signedn ) *signedn = sg;
    return any ? (int)( sign * v ) : def;
}

bool gfx_draw( gfx_t* g, const char* cmd, size_t len, char* err, size_t errsz ) {
    drawp_t p = { cmd, len };
    bool blank = false, back = false;
    for (;;) {
        draw_skip( &p );
        if ( p.n == 0 ) {
            return true;
        }
        char ch = *p.s++; --p.n;
        if ( ch >= 'a' && ch <= 'z' ) {
            ch = (char)( ch - 'a' + 'A' );
        }
        bool given = false, sg = false;
        int dx = 0, dy = 0;
        switch ( ch ) {
            case 'B': blank = true; continue;
            case 'N': back = true; continue;
            case 'C': {
                int c = draw_num( &p, 0, &given, &sg );
                if ( !given ) { snprintf( err, errsz, "Missing number after C in DRAW" ); return false; }
                g->drawcolor = c & 0xFF;
                continue;
            }
            case 'S': {
                int s = draw_num( &p, 4, &given, &sg );
                if ( !given || s < 1 || s > 255 ) { snprintf( err, errsz, "Bad scale in DRAW" ); return false; }
                g->drawscale = s;
                continue;
            }
            case 'A': {
                int a = draw_num( &p, 0, &given, &sg );
                if ( !given || a < 0 || a > 3 ) { snprintf( err, errsz, "Bad angle in DRAW" ); return false; }
                g->drawangle = a;
                continue;
            }
            case 'M': {
                int mx = draw_num( &p, 0, &given, &sg );
                if ( !given ) { snprintf( err, errsz, "Missing coordinate in DRAW" ); return false; }
                bool sg2 = false;
                draw_skip( &p );
                if ( p.n == 0 || *p.s != ',' ) { snprintf( err, errsz, "Missing comma in DRAW" ); return false; }
                ++p.s; --p.n;
                int my = draw_num( &p, 0, &given, &sg2 );
                if ( !given ) { snprintf( err, errsz, "Missing coordinate in DRAW" ); return false; }
                int nx = sg ? g->drawx + mx : mx, ny = sg ? g->drawy + my : my;
                int ox = g->drawx, oy = g->drawy;
                if ( !blank ) {
                    gfx_line( g, g->drawx, g->drawy, nx, ny, g->drawcolor );
                }
                g->drawx = back ? ox : nx;
                g->drawy = back ? oy : ny;
                blank = back = false;
                continue;
            }
            case 'U': dy = -1; break;
            case 'D': dy = 1; break;
            case 'L': dx = -1; break;
            case 'R': dx = 1; break;
            case 'E': dx = 1; dy = -1; break;
            case 'F': dx = 1; dy = 1; break;
            case 'G': dx = -1; dy = 1; break;
            case 'H': dx = -1; dy = -1; break;
            default:
                snprintf( err, errsz, "Unknown DRAW command '%c'", ch );
                return false;
        }
        int n = draw_num( &p, 1, &given, &sg );
        // scale (quarters) and rotation (quarter turns, counterclockwise on the screen)
        for ( int turn = 0; turn < g->drawangle; ++turn ) {
            int t = dx; dx = dy; dy = -t;
        }
        long len2 = (long) n * g->drawscale / 4;
        int nx = g->drawx + (int)( dx * len2 ), ny = g->drawy + (int)( dy * len2 );
        int ox = g->drawx, oy = g->drawy;
        if ( !blank ) {
            gfx_line( g, g->drawx, g->drawy, nx, ny, g->drawcolor );
        }
        g->drawx = back ? ox : nx;
        g->drawy = back ? oy : ny;
        blank = back = false;
    }
}
