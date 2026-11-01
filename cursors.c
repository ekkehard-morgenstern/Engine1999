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

// -- the canvas: one picture of 16 x 24 pixels (palette indices of the sprite layer) -----------------------

static uint8_t cv[SPRITE_WIDTH * SPRITE_HEIGHT];
static int cBlack, cWhite, cGray, cLight, cSilver, cDark, cDim, cGold, cRed, cBlue;

static int pal_index( const char* name, int dflt ) {
    for ( int i = 0; i < 256; ++i ) {
        if ( strcasecmp( pal_title( i ), name ) == 0 && ( pal_argb( i ) >> 24 ) == 255U ) {
            return i;
        }
    }
    return dflt;
}

static void cv_clear( void ) {
    memset( cv, 0, sizeof(cv) );
}

static void cv_set( int x, int y, int c ) {
    if ( x >= 0 && y >= 0 && x < SPRITE_WIDTH && y < SPRITE_HEIGHT ) {
        cv[ y * SPRITE_WIDTH + x ] = (uint8_t) c;
    }
}

static int cv_get( int x, int y ) {
    return x >= 0 && y >= 0 && x < SPRITE_WIDTH && y < SPRITE_HEIGHT ? cv[ y * SPRITE_WIDTH + x ] : 0;
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

// every empty pixel next to a drawn one (also diagonally) gets the color c: an outline that keeps the shape visible on any background
static void cv_outline( int c ) {
    uint8_t src[SPRITE_WIDTH * SPRITE_HEIGHT];
    memcpy( src, cv, sizeof(src) );
    for ( int y = 0; y < SPRITE_HEIGHT; ++y ) {
        for ( int x = 0; x < SPRITE_WIDTH; ++x ) {
            if ( src[ y * SPRITE_WIDTH + x ] != 0 ) {
                continue;
            }
            for ( int j = -1; j <= 1; ++j ) {
                for ( int i = -1; i <= 1; ++i ) {
                    int xx = x + i, yy = y + j;
                    if ( xx >= 0 && yy >= 0 && xx < SPRITE_WIDTH && yy < SPRITE_HEIGHT && src[ yy * SPRITE_WIDTH + xx ] != 0 ) {
                        cv_set( x, y, c );
                    }
                }
            }
        }
    }
}

// a picture from rows of characters: . transparent, # black, o white, g gray, l light gray, y gold, r red, b blue
static void cv_art( const char* const* rows, int n, int ox, int oy ) {
    for ( int y = 0; y < n; ++y ) {
        for ( int x = 0; rows[y][x] != '\0'; ++x ) {
            int c;
            switch ( rows[y][x] ) {
                case '#': c = cBlack; break;
                case 'o': c = cWhite; break;
                case 'g': c = cGray; break;
                case 'l': c = cLight; break;
                case 'y': c = cGold; break;
                case 'r': c = cRed; break;
                case 'b': c = cBlue; break;
                default:  c = -1; break;
            }
            if ( c >= 0 ) {
                cv_set( ox + x, oy + y, c );
            }
        }
    }
}

// -- the pictures ------------------------------------------------------------------------------------------

static const char* const art_arrow[] = {
    "#...........",
    "##..........",
    "#o#.........",
    "#oo#........",
    "#ooo#.......",
    "#oooo#......",
    "#ooooo#.....",
    "#oooooo#....",
    "#ooooooo#...",
    "#oooooooo#..",
    "#ooooooooo#.",
    "#oooooo#####",
    "#oo#oo#.....",
    "#o#.#oo#....",
    "##..#oo#....",
    "#....#oo#...",
    ".....#oo#...",
    "......##....",
};

static void draw_arrow( void ) {
    cv_art( art_arrow, (int)( sizeof(art_arrow) / sizeof(art_arrow[0]) ), 0, 0 );
}

// a filled disc (the center and the radius are doubled: 15 = pixel 7.5)
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

// 8 dots on a ring, the head (white) at dot 'head', a tail of gray ones behind it; r = the radius of the ring, size = the size of a dot
static void draw_ring( int cx2, int cy2, int r2, int size, int head ) {
    static const int dx[8] = {  0,  7,  10,  7,  0,  -7, -10, -7 };      // cos / sin of k * 45 degrees * 10
    static const int dy[8] = { -10, -7,  0,  7, 10,   7,   0, -7 };
    for ( int k = 0; k < 8; ++k ) {
        int d = ( head - k + 8 ) % 8;              // 0 = the head, 1.. = how far behind
        int c = d == 0 ? cWhite : d == 1 ? cLight : d == 2 ? cSilver : d == 3 ? cGray : cDim;
        int x = ( cx2 * 10 + dx[k] * r2 ) / 20, y = ( cy2 * 10 + dy[k] * r2 ) / 20;      // (cx2 = twice the center, r2 = twice the radius)
        cv_rect( x, y, x + size - 1, y + size - 1, c );
    }
}

static void draw_wait( int frame ) {
    cv_disc( 15, 15, 15, cBlack );                 // a dark round badge: the dots can be seen on any background
    draw_ring( 15, 15, 9, 2, frame );
    cv_outline( cWhite );                           // and a white edge: the badge shows on a dark background, too
}

static void draw_busy( int frame ) {
    uint8_t arrow[SPRITE_WIDTH * SPRITE_HEIGHT];
    cv_clear();
    draw_arrow();
    memcpy( arrow, cv, sizeof(arrow) );
    cv_clear();
    cv_disc( 23, 35, 9, cBlack );                  // a small badge at the lower right of the arrow, center (11.5, 17.5)
    draw_ring( 23, 35, 5, 1, frame );
    cv_outline( cWhite );
    for ( int i = 0; i < SPRITE_WIDTH * SPRITE_HEIGHT; ++i ) {      // the arrow stays in front where they meet
        if ( arrow[i] != 0 ) cv[i] = arrow[i];
    }
}

static void draw_text( void ) {
    cv_line( 3, 0, 3, 15, cBlack );
    cv_line( 1, 0, 5, 0, cBlack );
    cv_line( 1, 15, 5, 15, cBlack );
    cv_outline( cWhite );
}

static void draw_hand( void ) {
    cv_rect( 5, 1, 7, 10, cWhite );        // the pointing finger
    cv_rect( 8, 6, 9, 10, cWhite );        // the other fingers
    cv_rect( 10, 7, 11, 10, cWhite );
    cv_rect( 12, 8, 13, 10, cWhite );
    cv_rect( 3, 9, 13, 14, cWhite );       // the palm
    cv_rect( 1, 8, 3, 11, cWhite );        // the thumb
    cv_outline( cBlack );
    cv_line( 8, 6, 8, 9, cBlack );         // the gaps between the fingers
    cv_line( 10, 7, 10, 9, cBlack );
    cv_line( 12, 8, 12, 9, cBlack );
    cv_line( 4, 9, 4, 11, cGray );         // where the thumb meets the palm
}

static void draw_grab( void ) {
    cv_rect( 3, 4, 4, 6, cWhite );         // the knuckles
    cv_rect( 6, 3, 7, 6, cWhite );
    cv_rect( 9, 3, 10, 6, cWhite );
    cv_rect( 12, 4, 13, 6, cWhite );
    cv_rect( 2, 7, 13, 13, cWhite );       // the fist
    cv_rect( 4, 14, 12, 15, cWhite );      // the wrist
    cv_rect( 1, 8, 2, 11, cWhite );        // the thumb
    cv_outline( cBlack );
    cv_line( 5, 4, 5, 7, cBlack );
    cv_line( 8, 3, 8, 7, cBlack );
    cv_line( 11, 3, 11, 7, cBlack );
}

static void draw_cross( void ) {
    cv_line( 0, 7, 5, 7, cBlack );
    cv_line( 9, 7, 14, 7, cBlack );
    cv_line( 7, 0, 7, 5, cBlack );
    cv_line( 7, 9, 7, 14, cBlack );
    cv_set( 7, 7, cBlack );
    cv_outline( cWhite );
}

// an arrow head: from the tip (x, y) in the direction (dx, dy), 'len' pixels long, widening by one pixel on each side per pixel
static void draw_head( int x, int y, int dx, int dy, int len, int c ) {
    for ( int k = 0; k < len; ++k ) {
        int px = x + dx * k, py = y + dy * k;
        for ( int w = -k; w <= k; ++w ) {
            cv_set( dx != 0 ? px : px + w, dy != 0 ? py : py + w, c );
        }
    }
}

static void draw_sizeh( void ) {
    cv_rect( 4, 6, 10, 8, cBlack );
    draw_head( 0, 7, 1, 0, 4, cBlack );
    draw_head( 14, 7, -1, 0, 4, cBlack );
    cv_outline( cWhite );
}

static void draw_sizev( void ) {
    cv_rect( 6, 4, 8, 10, cBlack );
    draw_head( 7, 0, 0, 1, 4, cBlack );
    draw_head( 7, 14, 0, -1, 4, cBlack );
    cv_outline( cWhite );
}

static void draw_move( void ) {
    cv_rect( 4, 6, 10, 8, cBlack );
    cv_rect( 6, 4, 8, 10, cBlack );
    draw_head( 0, 7, 1, 0, 3, cBlack );
    draw_head( 14, 7, -1, 0, 3, cBlack );
    draw_head( 7, 0, 0, 1, 3, cBlack );
    draw_head( 7, 14, 0, -1, 3, cBlack );
    cv_outline( cWhite );
}

static void draw_no( void ) {
    for ( int y = 0; y < 15; ++y ) {
        for ( int x = 0; x < 15; ++x ) {
            int dx = 2 * x - 14, dy = 2 * y - 14;                // the center is (7, 7)
            int d2 = dx * dx + dy * dy;
            if ( d2 <= 14 * 14 && d2 >= 9 * 9 ) {
                cv_set( x, y, cRed );
            }
        }
    }
    for ( int k = 2; k <= 12; ++k ) {                      // the slash: 2 x 2 pixels along the diagonal
        cv_rect( k, k, k + 1, k + 1, cRed );
    }
    for ( int y = 0; y < 15; ++y ) {                       // a white inside, so that the sign is visible on any background
        for ( int x = 0; x < 15; ++x ) {
            int dx = 2 * x - 14, dy = 2 * y - 14;
            if ( dx * dx + dy * dy <= 14 * 14 && cv_get( x, y ) == 0 ) {
                cv_set( x, y, cWhite );
            }
        }
    }
    cv_outline( cWhite );
}

static void draw_help( void ) {
    static const char* const q[] = {
        ".###.",
        "#...#",
        "....#",
        "...#.",
        "..#..",
        "..#..",
        ".....",
        "..#..",
    };
    cv_art( art_arrow, (int)( sizeof(art_arrow) / sizeof(art_arrow[0]) ), 0, 0 );
    uint8_t save[SPRITE_WIDTH * SPRITE_HEIGHT];
    memcpy( save, cv, sizeof(save) );
    cv_clear();
    cv_art( q, 8, 10, 10 );
    cv_outline( cWhite );
    for ( int i = 0; i < SPRITE_WIDTH * SPRITE_HEIGHT; ++i ) {
        if ( save[i] != 0 ) cv[i] = save[i];
    }
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
    { "HAND",   5,  1, 18,  1, -1, 0 },
    { "GRAB",   7,  8, 19,  1, -1, 0 },
    { "CROSS",  7,  7, 20,  1, -1, 0 },
    { "SIZEH",  7,  7, 21,  1, -1, 0 },
    { "SIZEV",  7,  7, 22,  1, -1, 0 },
    { "MOVE",   7,  7, 23,  1, -1, 0 },
    { "NO",     7,  7, 24,  1, -1, 0 },
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
    cGray = pal_index( "Gray", cBlack );
    cLight = pal_index( "LightGray", cWhite );
    cSilver = pal_index( "Silver", cLight );
    cDark = pal_index( "DarkGray", cSilver );
    cDim = pal_index( "DimGray", cGray );
    cGold = pal_index( "Gold", cWhite );
    cRed = pal_index( "Red", cBlack );
    cBlue = pal_index( "DodgerBlue", cBlack );
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
    (void) cGold; (void) cBlue; (void) cDark;
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
