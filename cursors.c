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

#include "cursors.h"
#include "sprscreen.h"
#include "palette.h"
#include <strings.h>
#include <math.h>

// -- the canvas: one picture of 16 x 24 pixels (palette indices of the sprite layer) -----------------------
//
// The pointer looks like glass that floats above the screen: a translucent colored body (the translucent colors of the palette, so what is behind shows
// through), a bright edge at the top left where the light comes from, a dark edge at the bottom right, a lighter and a darker band inside (a bevel) and a soft
// shadow that falls to the lower right. Every shape is first drawn as a mask (a silhouette) and glass() turns that into the picture.

static uint8_t cv[SPRITE_WIDTH * SPRITE_HEIGHT];        // the picture
static uint8_t mk[SPRITE_WIDTH * SPRITE_HEIGHT];        // the silhouette
static uint8_t* tgt = cv;                               // what the drawing helpers draw into

typedef struct _hue_t {
    int light, mid, dark;       // the three translucent tones of the body
    int rim;                    // the dark edge (opaque)
} hue_t;

static int cWhite, cBlack, cShadow, cFrost;
static hue_t hBlue, hGold, hGreen, hRed, hPurple, hTeal;
static int comet[8];

static int pal_index( const char* name, int dflt ) {
    for ( int i = 0; i < 256; ++i ) {
        if ( strcasecmp( pal_title( i ), name ) == 0 ) {
            return i;
        }
    }
    return dflt;
}

static void cv_clear( void ) {
    memset( tgt, 0, SPRITE_WIDTH * SPRITE_HEIGHT );
}

static void cv_set( int x, int y, int c ) {
    if ( x >= 0 && y >= 0 && x < SPRITE_WIDTH && y < SPRITE_HEIGHT ) {
        tgt[ y * SPRITE_WIDTH + x ] = (uint8_t) c;
    }
}

static void cv_rect( int x0, int y0, int x1, int y1, int c ) {
    for ( int y = y0; y <= y1; ++y ) {
        for ( int x = x0; x <= x1; ++x ) {
            cv_set( x, y, c );
        }
    }
}

static void cv_line( int x0, int y0, int x1, int y1, int c ) {
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y1 - y0 : y0 - y1;
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx - dy;
    for ( ;; ) {
        cv_set( x0, y0, c );
        if ( x0 == x1 && y0 == y1 ) {
            break;
        }
        int e2 = 2 * err;
        if ( e2 > -dy ) { err -= dy; x0 += sx; }
        if ( e2 < dx )  { err += dx; y0 += sy; }
    }
}

// a filled disc; center and radius are doubled (cx2 = 2 * x + 1 is the middle of pixel x, an even number is a pixel edge)
static void cv_disc( int cx2, int cy2, int r2, int c ) {
    for ( int y = 0; y < SPRITE_HEIGHT; ++y ) {
        for ( int x = 0; x < SPRITE_WIDTH; ++x ) {
            int dx = 2 * x + 1 - cx2, dy = 2 * y + 1 - cy2;
            if ( dx * dx + dy * dy <= r2 * r2 ) {
                cv_set( x, y, c );
            }
        }
    }
}

// a filled polygon (x0, y0, x1, y1, ...); a pixel is in if the polygon touches it on its row
static void cv_poly( const int* pts, int n, int c ) {
    for ( int y = 0; y < SPRITE_HEIGHT; ++y ) {
        double sy = y + 0.5, xs[16];
        int k = 0;
        for ( int i = 0; i < n && k < 15; ++i ) {
            int x0 = pts[2 * i], y0 = pts[2 * i + 1], x1 = pts[2 * ( ( i + 1 ) % n )], y1 = pts[2 * ( ( i + 1 ) % n ) + 1];
            if ( ( y0 <= sy && y1 > sy ) || ( y1 <= sy && y0 > sy ) ) {
                xs[k++] = x0 + ( sy - y0 ) / (double)( y1 - y0 ) * ( x1 - x0 );
            }
        }
        for ( int a = 0; a < k; ++a ) {
            for ( int b = a + 1; b < k; ++b ) {
                if ( xs[b] < xs[a] ) { double t = xs[a]; xs[a] = xs[b]; xs[b] = t; }
            }
        }
        for ( int a = 0; a + 1 < k; a += 2 ) {
            for ( int x = (int) floor( xs[a] ); x < (int) ceil( xs[a + 1] ); ++x ) {
                cv_set( x, y, c );
            }
        }
    }
}

// the silhouette of a shape is drawn with the helpers above into mk (color 1)
static void mask_begin( void ) {
    tgt = mk;
    cv_clear();
}

static void mask_end( void ) {
    tgt = cv;
}

// how many pixels of the silhouette in a row from (x, y) in the direction (dx, dy), at most 3 (1 = the next one is outside)
static int run( int x, int y, int dx, int dy ) {
    int n = 0;
    while ( n < 3 && x >= 0 && y >= 0 && x < SPRITE_WIDTH && y < SPRITE_HEIGHT && mk[ y * SPRITE_WIDTH + x ] ) {
        ++n; x += dx; y += dy;
    }
    return n;
}

// turns the silhouette in mk into glass of the hue h on top of what is in the picture already: the shadow first (only where nothing is), then the body
static void glass( const hue_t* h ) {
    for ( int y = 0; y < SPRITE_HEIGHT; ++y ) {
        for ( int x = 0; x < SPRITE_WIDTH; ++x ) {
            if ( mk[ y * SPRITE_WIDTH + x ] ) {
                int sx = x + 2, sy = y + 3;
                if ( sx < SPRITE_WIDTH && sy < SPRITE_HEIGHT && !mk[ sy * SPRITE_WIDTH + sx ] && cv[ sy * SPRITE_WIDTH + sx ] == 0 ) {
                    cv[ sy * SPRITE_WIDTH + sx ] = (uint8_t) cShadow;
                }
            }
        }
    }
    for ( int y = 0; y < SPRITE_HEIGHT; ++y ) {
        for ( int x = 0; x < SPRITE_WIDTH; ++x ) {
            if ( !mk[ y * SPRITE_WIDTH + x ] ) {
                continue;
            }
            int a = run( x, y, 0, -1 ), b = run( x, y, -1, 0 );          // the distance to the edge above and to the left
            int tl = a < b ? a : b;
            int c = run( x, y, 0, 1 ), d = run( x, y, 1, 0 );            // ... below and to the right
            int br = c < d ? c : d;
            int col;
            if ( tl == 1 )      col = cWhite;                           // the lit edge
            else if ( br == 1 ) col = h->rim;                           // the edge in the shade
            else if ( tl == 2 ) col = h->light;                         // the bevel: light on the lit side, dark on the other
            else if ( br == 2 ) col = h->dark;
            else                col = h->mid;
            cv[ y * SPRITE_WIDTH + x ] = (uint8_t) col;
        }
    }
}

// a glass shape from the silhouette that is in mk now
static void glass_shape( const hue_t* h ) {
    glass( h );
}

// a picture from rows of characters (# the dark color of the hue): the question mark
static void cv_art( const char* const* rows, int n, int ox, int oy, int c ) {
    for ( int y = 0; y < n; ++y ) {
        for ( int x = 0; rows[y][x] != '\0'; ++x ) {
            if ( rows[y][x] == '#' ) {
                cv_set( ox + x, oy + y, c );
            }
        }
    }
}

// 8 dots on a ring around the center (cx, cy) with the radius r; the head of the comet at dot 'head', its tail (colors of 'comet') behind it
static void draw_dots( int cx, int cy, int r, int size, int head ) {
    static const int dx[8] = { 0, 7, 10, 7, 0, -7, -10, -7 };      // cos and sin of k * 45 degrees * 10
    static const int dy[8] = { -10, -7, 0, 7, 10, 7, 0, -7 };
    for ( int k = 0; k < 8; ++k ) {
        int d = ( head - k + 8 ) % 8;
        int x = cx + ( dx[k] * r + ( dx[k] < 0 ? -5 : 5 ) ) / 10, y = cy + ( dy[k] * r + ( dy[k] < 0 ? -5 : 5 ) ) / 10;
        cv_rect( x - size / 2, y - size / 2, x - size / 2 + size - 1, y - size / 2 + size - 1, comet[d] );
    }
}

// -- the pictures ------------------------------------------------------------------------------------------

static void mask_arrow( void ) {
    static const int p[] = { 0, 0,  0, 14,  3, 11,  5, 17,  8, 16,  6, 10,  10, 10 };
    cv_poly( p, 7, 1 );
}

static void draw_arrow( void ) {
    mask_begin(); mask_arrow(); mask_end();
    glass_shape( &hBlue );
}

static void draw_wait( int frame ) {
    cv_clear();
    mask_begin(); cv_disc( 14, 14, 14, 1 ); mask_end();         // a round glass badge, 14 pixels wide (the center is the edge between 6 and 7 plus one: (7, 7))
    glass_shape( &hTeal );
    draw_dots( 7, 7, 4, 2, frame );
}

static void draw_busy( int frame ) {
    cv_clear();
    mask_begin(); mask_arrow(); mask_end();
    glass_shape( &hBlue );
    mask_begin(); cv_disc( 20, 30, 8, 1 ); mask_end();           // a small badge at the lower right of the arrow, center (10, 15)
    glass_shape( &hTeal );
    draw_dots( 10, 15, 3, 1, frame );
}

static void draw_text( void ) {
    cv_clear();
    mask_begin();
    cv_rect( 0, 0, 6, 2, 1 );
    cv_rect( 2, 3, 4, 12, 1 );
    cv_rect( 0, 13, 6, 15, 1 );
    mask_end();
    glass_shape( &hTeal );
}

static void draw_hand( void ) {
    cv_clear();
    mask_begin();
    cv_rect( 4, 0, 6, 9, 1 );          // the pointing finger
    cv_rect( 7, 6, 9, 9, 1 );          // the other fingers, folded
    cv_rect( 10, 7, 12, 9, 1 );
    cv_rect( 3, 9, 12, 15, 1 );        // the hand
    cv_rect( 0, 9, 3, 12, 1 );         // the thumb
    mask_end();
    glass_shape( &hGold );
    cv_line( 7, 7, 7, 10, hGold.rim ); // the creases between the fingers
    cv_line( 10, 8, 10, 10, hGold.rim );
}

static void draw_grab( void ) {
    cv_clear();
    mask_begin();
    cv_rect( 1, 4, 3, 6, 1 );          // the knuckles of the closed fingers
    cv_rect( 4, 3, 6, 6, 1 );
    cv_rect( 7, 3, 9, 6, 1 );
    cv_rect( 10, 4, 12, 6, 1 );
    cv_rect( 1, 7, 12, 13, 1 );        // the fist
    cv_rect( 0, 8, 1, 11, 1 );         // the thumb
    cv_rect( 3, 14, 10, 16, 1 );       // the wrist
    mask_end();
    glass_shape( &hGold );
    cv_line( 4, 4, 4, 8, hGold.rim );
    cv_line( 7, 3, 7, 8, hGold.rim );
    cv_line( 10, 4, 10, 8, hGold.rim );
}

static void draw_cross( void ) {
    cv_clear();
    mask_begin();
    cv_rect( 0, 5, 4, 7, 1 );
    cv_rect( 8, 5, 12, 7, 1 );
    cv_rect( 5, 0, 7, 4, 1 );
    cv_rect( 5, 8, 7, 12, 1 );
    mask_end();
    glass_shape( &hGreen );
}

static void draw_sizeh( void ) {
    cv_clear();
    mask_begin();
    cv_rect( 3, 5, 9, 7, 1 );
    static const int l[] = { 0, 6,  4, 2,  4, 11 };
    static const int r[] = { 13, 6,  9, 2,  9, 11 };
    cv_poly( l, 3, 1 );
    cv_poly( r, 3, 1 );
    mask_end();
    glass_shape( &hPurple );
}

static void draw_sizev( void ) {
    cv_clear();
    mask_begin();
    cv_rect( 5, 3, 7, 9, 1 );
    static const int t[] = { 6, 0,  2, 4,  11, 4 };
    static const int b[] = { 6, 13,  2, 9,  11, 9 };
    cv_poly( t, 3, 1 );
    cv_poly( b, 3, 1 );
    mask_end();
    glass_shape( &hPurple );
}

static void draw_move( void ) {
    cv_clear();
    mask_begin();
    cv_rect( 3, 5, 9, 7, 1 );
    cv_rect( 5, 3, 7, 9, 1 );
    static const int l[] = { 0, 6,  3, 4,  3, 9 };
    static const int r[] = { 13, 6,  10, 4,  10, 9 };
    static const int t[] = { 6, 0,  4, 3,  9, 3 };
    static const int b[] = { 6, 13,  4, 10,  9, 10 };
    cv_poly( l, 3, 1 );
    cv_poly( r, 3, 1 );
    cv_poly( t, 3, 1 );
    cv_poly( b, 3, 1 );
    mask_end();
    glass_shape( &hPurple );
}

static void draw_no( void ) {
    cv_clear();
    mask_begin();
    for ( int y = 0; y < 13; ++y ) {
        for ( int x = 0; x < 13; ++x ) {
            int dx = 2 * x - 12, dy = 2 * y - 12;               // the center is (6, 6)
            int d2 = dx * dx + dy * dy;
            if ( d2 <= 13 * 13 && d2 >= 8 * 8 ) {
                cv_set( x, y, 1 );
            }
        }
    }
    for ( int k = 1; k <= 10; ++k ) {                           // the slash, 4 pixels wide
        cv_rect( k, k, k + 2, k + 2, 1 );
    }
    mask_end();
    for ( int y = 0; y < 13; ++y ) {                            // the inside of the sign is inside the disc only
        for ( int x = 0; x < 13; ++x ) {
            int dx = 2 * x - 12, dy = 2 * y - 12;
            if ( dx * dx + dy * dy < 8 * 8 && !mk[ y * SPRITE_WIDTH + x ] ) {
                cv[ y * SPRITE_WIDTH + x ] = (uint8_t) cFrost;   // frosted glass
            }
        }
    }
    glass_shape( &hRed );
    for ( int y = 0; y < 13; ++y ) {                            // (the shadow must not fall on the frosted inside)
        for ( int x = 0; x < 13; ++x ) {
            int dx = 2 * x - 12, dy = 2 * y - 12;
            if ( dx * dx + dy * dy < 8 * 8 && !mk[ y * SPRITE_WIDTH + x ] ) {
                cv[ y * SPRITE_WIDTH + x ] = (uint8_t) cFrost;
            }
        }
    }
}

static void draw_help( void ) {
    static const char* const q[] = {
        "###",
        "..#",
        ".#.",
        "...",
        ".#.",
    };
    cv_clear();
    mask_begin(); mask_arrow(); mask_end();
    glass_shape( &hBlue );
    mask_begin(); cv_disc( 20, 24, 8, 1 ); mask_end();           // a round bubble of gold glass at the lower right, center (10, 12)
    glass_shape( &hGold );
    cv_art( q, 5, 9, 10, hGold.rim );
}

// -- the shapes ----------------------------------------------------------------------------------------------

typedef struct _shape_t {
    const char* name;
    int         hotx, hoty;     // the hot spot in the picture
    int         first;          // the first image (of the engine's half of the map)
    int         frames;         // 1, or the length of the animation
    int         seq;            // the animation sequence of the engine (-1: none)
    int         speed;          // frames per image of the animation
} shape_t;

static const shape_t shapes[CUR_COUNT] = {
    { "ARROW",  0,  0,  0,  1, -1, 0 },
    { "WAIT",   7,  7,  1,  8,  0, 4 },
    { "BUSY",   0,  0,  9,  8,  1, 4 },
    { "TEXT",   3,  8, 17,  1, -1, 0 },
    { "HAND",   5,  0, 18,  1, -1, 0 },
    { "GRAB",   6,  8, 19,  1, -1, 0 },
    { "CROSS",  6,  6, 20,  1, -1, 0 },
    { "SIZEH",  6,  6, 21,  1, -1, 0 },
    { "SIZEV",  6,  6, 22,  1, -1, 0 },
    { "MOVE",   6,  6, 23,  1, -1, 0 },
    { "NO",     6,  6, 24,  1, -1, 0 },
    { "HELP",   0,  0, 25,  1, -1, 0 },
};

static int  cur_shape = CUR_ARROW;
static int  cur_x = 0, cur_y = 0;           // where the hot spot is
static bool cur_visible = false;

static void put_image( int index ) {
    sprscr_sys_writemap( index, cv );
}

void cursors_install( void ) {
    cBlack = pal_index( "Black", 1 );
    cWhite = pal_index( "White", 140 );
    cShadow = pal_index( "TransparentBlack", cBlack );
    cFrost = pal_index( "TransparentWhite", cWhite );
    #define HUE(h, l, m, d, r) h.light = pal_index( l, cWhite ); h.mid = pal_index( m, cWhite ); h.dark = pal_index( d, cBlack ); h.rim = pal_index( r, cBlack )
    HUE( hBlue,   "TransparentLightSkyBlue", "TransparentDodgerBlue",  "TransparentRoyalBlue",    "MidnightBlue" );
    HUE( hGold,   "TransparentGold",         "TransparentOrange",      "TransparentDarkOrange",   "SaddleBrown" );
    HUE( hGreen,  "TransparentPaleGreen",    "TransparentLimeGreen",   "TransparentForestGreen",  "DarkGreen" );
    HUE( hRed,    "TransparentSalmon",       "TransparentRed",         "TransparentDarkRed",      "Maroon" );
    HUE( hPurple, "TransparentViolet",       "TransparentBlueViolet",  "TransparentIndigo",       "Indigo" );
    HUE( hTeal,   "TransparentAquamarine",   "TransparentDarkTurquoise", "TransparentTeal",       "DarkSlateGray" );
    #undef HUE
    static const char* const cometnames[8] = { "White", "Yellow", "Gold", "Orange", "OrangeRed", "DeepPink", "MediumPurple", "DodgerBlue" };
    for ( int k = 0; k < 8; ++k ) {
        comet[k] = pal_index( cometnames[k], cWhite );
    }
    cv_clear(); draw_arrow(); put_image( shapes[CUR_ARROW].first );
    uint8_t seq[16];
    for ( int f = 0; f < 8; ++f ) {
        cv_clear(); draw_wait( f ); put_image( shapes[CUR_WAIT].first + f );
        seq[f] = (uint8_t)( shapes[CUR_WAIT].first + f );
    }
    sprscr_sys_animdata( shapes[CUR_WAIT].seq, seq, 8U );
    for ( int f = 0; f < 8; ++f ) {
        cv_clear(); draw_busy( f ); put_image( shapes[CUR_BUSY].first + f );
        seq[f] = (uint8_t)( shapes[CUR_BUSY].first + f );
    }
    sprscr_sys_animdata( shapes[CUR_BUSY].seq, seq, 8U );
    cv_clear(); draw_text(); put_image( shapes[CUR_TEXT].first );
    cv_clear(); draw_hand(); put_image( shapes[CUR_HAND].first );
    cv_clear(); draw_grab(); put_image( shapes[CUR_GRAB].first );
    cv_clear(); draw_cross(); put_image( shapes[CUR_CROSS].first );
    cv_clear(); draw_sizeh(); put_image( shapes[CUR_SIZEH].first );
    cv_clear(); draw_sizev(); put_image( shapes[CUR_SIZEV].first );
    cv_clear(); draw_move(); put_image( shapes[CUR_MOVE].first );
    cv_clear(); draw_no(); put_image( shapes[CUR_NO].first );
    cv_clear(); draw_help(); put_image( shapes[CUR_HELP].first );
    cur_shape = CUR_ARROW; cur_x = cur_y = 0; cur_visible = false;
    cursors_set( CUR_ARROW );
    sprscr_sys_show( SPR_MOUSE, false );
}

bool cursors_set( int shape ) {
    if ( shape < 0 || shape >= CUR_COUNT ) {
        return false;
    }
    const shape_t* s = &shapes[shape];
    if ( s->frames > 1 ) {
        sprscr_sys_animcfg( SPR_MOUSE, s->seq, s->frames, s->speed );
    } else {
        sprscr_sys_setimage( SPR_MOUSE, s->first );
    }
    cur_shape = shape;
    sprscr_sys_move( SPR_MOUSE, cur_x - s->hotx, cur_y - s->hoty );
    return true;
}

int cursors_shape( void ) {
    return cur_shape;
}

void cursors_move( int x, int y ) {
    cur_x = x; cur_y = y;
    sprscr_sys_move( SPR_MOUSE, x - shapes[cur_shape].hotx, y - shapes[cur_shape].hoty );
}

void cursors_position( int* x, int* y ) {
    *x = cur_x; *y = cur_y;
}

void cursors_show( bool on ) {
    cur_visible = on;
    sprscr_sys_show( SPR_MOUSE, on );
}

bool cursors_visible( void ) {
    return cur_visible;
}

void cursors_hotspot( int shape, int* hx, int* hy ) {
    if ( shape < 0 || shape >= CUR_COUNT ) {
        *hx = *hy = 0;
        return;
    }
    *hx = shapes[shape].hotx; *hy = shapes[shape].hoty;
}

int cursors_find( const char* name, size_t len ) {
    for ( int i = 0; i < CUR_COUNT; ++i ) {
        if ( strlen( shapes[i].name ) == len && strncasecmp( shapes[i].name, name, len ) == 0 ) {
            return i;
        }
    }
    return -1;
}

const char* cursors_name( int shape ) {
    return shape >= 0 && shape < CUR_COUNT ? shapes[shape].name : "";
}

int cursors_images( int shape, int* first, int* count ) {
    if ( shape < 0 || shape >= CUR_COUNT ) {
        *first = *count = 0;
        return 0;
    }
    *first = shapes[shape].first; *count = shapes[shape].frames;
    return shapes[shape].speed;
}
