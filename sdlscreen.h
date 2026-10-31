#pragma once

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

#ifndef SDLSCREEN_H
#define SDLSCREEN_H     1

#ifndef SDLTYPES_H
#include "sdltypes.h"
#endif

#ifndef TILESCREEN_H
#include "tilescreen.h"
#endif

#ifndef SPRSCREEN_H
#include "sprscreen.h"
#endif

#define SDL_SCREENWIDTH     640
#define SDL_SCREENHEIGHT    300

#define SDLSCR_INPUT_KEYPRESS       0
#define SDLSCR_INPUT_TEXT           1

typedef struct _sdlscr_inputmsg_t {
    int     inputtype;
    union {
        struct {
            SDL_Keycode symbol;
            Uint16      mods;       // SDL_Keymod bits (KMOD_CTRL, ...) at the time of the key press
        };
        char            text[SDL_TEXTINPUTEVENT_TEXT_SIZE];
    };
} sdlscr_inputmsg_t;

bool sdlscr_init( void );
void sdlscr_cleanup( void );
bool sdlscr_term( void );
uint64_t sdlscr_getnsec( struct timespec* pts );
uint64_t sdlscr_getframecnt( void );
sdlscr_inputmsg_t* sdlscr_dequeueinputmsg( void );
void sdlscr_deleteinputmsg( sdlscr_inputmsg_t* msg );     // frees a message returned by sdlscr_dequeueinputmsg()
void sdlscr_enableinput( bool enable );

// keyboard access for running programs (INKEY$, GET); the calling thread is the main thread, the keys come from the
// screen thread. ESC is not a key then: it is the break request.
void sdlscr_pollkeys( bool enable );        // queue keys (without the cursor) while a program runs; clears the break request
int sdlscr_pollkey( void );                 // the next key code, 0 = none
int sdlscr_getkey( void );                  // waits for a key; -1 = screen closed or break requested
bool sdlscr_breakrequested( void );         // ESC was pressed (or the screen was closed) since sdlscr_pollkeys( true )
// key codes: printable ASCII as is, 13 = RETURN, 8 = BACKSPACE, 9 = TAB

// text screen functions
void sdlscr_printf( int y, int x, int bg, int fg, const char* fmt, ... );
void sdlscr_puts( const char* text );        // prints text of any length at the cursor, in the current colors
void sdlscr_ansi_printf( const char* fmt, ... );     // printf-style sdlscr_ansi_puts (any length)
void sdlscr_ansi_write( const char* text, size_t len );   // the same for len bytes; zero bytes are printed (control code glyphs)
void sdlscr_ansi_puts( const char* text );   // like sdlscr_puts, but ESC [ ... m (SGR) sets the colors (see ansi.h); no lock
// simple line editor for one line (EDIT): buf holds the text to edit and receives the result (bufsz includes the
// terminating NUL). The editor starts at the cursor and redraws the line (which may wrap over several screen rows) after
// every change. Keys: printable characters (inserted), LEFT, RIGHT, HOME, END, BACKSPACE, DELETE; RETURN accepts, ESC
// cancels (buf is then unchanged). The cursor is placed on a new line afterwards. No lock, like all text screen output.
#define SDLSCR_EDIT_ACCEPTED    0
#define SDLSCR_EDIT_CANCELLED   1
#define SDLSCR_EDIT_TOOLONG     2       // the text doesn't fit on the screen: nothing was done
#define SDLSCR_EDIT_CLOSED      3       // the screen was closed
// the editing state of a line (buf holds maxlen characters + NUL) and the keys shared by both editors: printable
// characters (inserted), LEFT, RIGHT, HOME, END, BACKSPACE, DELETE; returns true if the text or the position changed
typedef struct _sdlscr_edit_t {
    char*   buf;
    size_t  len, pos, maxlen;
} sdlscr_edit_t;
bool sdlscr_editkey( sdlscr_edit_t* ed, const sdlscr_inputmsg_t* msg );
bool sdlscr_editmode_begin( void );             // keys are queued, ESC is a key; returns the state for _end
void sdlscr_editmode_end( bool oldstate );
void sdlscr_textchanged( void );                // the text screen was changed directly through txtscr_*: redraw it
int sdlscr_lineedit( char* buf, size_t bufsz, void (*vblank_handler)( void* ), void* usrdata );
int sdlscr_lineinput( int stop_evtmsk, char* buf, size_t bufsz, void (*vblank_handler)( void* ), void* usrdata );
void sdlscr_gettextscreensize( int* outsx, int* outsy );
void sdlscr_gettextcursor( int* outx, int* outy );
void sdlscr_locate( int x, int y );

// tile screen functions
void sdlscr_writetile( int tileno, const uint8_t data[TILE_WIDTH * TILE_HEIGHT]);
void sdlscr_scrolltiles( int sx, int sy );

// sprite screen functions
void sdlscr_showsprite( int sprno, bool active );
void sdlscr_spriteprio( int sprno, int prio );
void sdlscr_movesprite( int sprno, int x, int y );
void sdlscr_spriteanimdata( int animno, const uint8_t* seq, size_t size );
void sdlscr_spriteanimcfg( int sprno, int animno, int length, int speed );
void sdlscr_writesprite( int sprno, const uint8_t data[SPRITE_WIDTH * SPRITE_HEIGHT]);


// -- the internal API for everything that draws (docs/graphics-subsystem.md) --------------------------------
// Nothing outside of sdlscreen.c calls sdllayer, textscreen, tilescreen, sprscreen or gfxcore. Every function validates its arguments
// (out of range = ignored), marks the layer it changed as modified itself, and hands out no pointers into layer memory, so that a lock
// or a command queue can be added here later without changing any caller. Layers: 0 background, 1 tiles, 2 text, 3 graphics, 4 sprites.

// layers, palettes, the color behind everything
void     sdlscr_layer_enable( int layer, bool enable );
bool     sdlscr_layer_enabled( int layer );
void     sdlscr_layer_setpriority( int layer, int prio );       // swaps places with the layer that has prio
int      sdlscr_layer_getpriority( int layer );                 // -1 for a bad layer
void     sdlscr_palette_set( int layer, int index, uint32_t argb );
uint32_t sdlscr_palette_get( int layer, int index );
void     sdlscr_palette_reset( int layer );                     // -1 = all layers
bool     sdlscr_palette_setbyname( int layer, int index, const char* name );    // false if the name is unknown
void     sdlscr_setbackdrop( uint32_t argb );

// the bitmap layers 0 (background) and 3 (graphics); color -1 = the pen; everything is clipped to 640 x 300
void     sdlscr_gfx_settarget( int layer );                     // 0 or 3: where the BASIC statements draw
int      sdlscr_gfx_target( void );
void     sdlscr_gfx_setpen( int color );
void     sdlscr_gfx_setmode( int mode );                        // GFXM_* of gfxcore.h
void     sdlscr_gfx_clear( int layer, int color );
void     sdlscr_gfx_pset( int layer, int x, int y, int color );
int      sdlscr_gfx_point( int layer, int x, int y );           // -1 outside
void     sdlscr_gfx_line( int layer, int x1, int y1, int x2, int y2, int color );
void     sdlscr_gfx_box( int layer, int x1, int y1, int x2, int y2, int color, bool fill );
void     sdlscr_gfx_circle( int layer, int x, int y, int r, int color, int a1, int a2, double ratio );
void     sdlscr_gfx_radius( int layer, int x, int y, int r, double angle, int color );
void     sdlscr_gfx_paint( int layer, int x, int y, int color, int border );
size_t   sdlscr_gfx_grab( int layer, int x1, int y1, int x2, int y2, uint8_t* out, size_t outsize );   // returns the size needed
bool     sdlscr_gfx_blit( int layer, const uint8_t* block, size_t size, int x, int y, bool transparent );
void     sdlscr_gfx_text( int layer, int x, int y, const char* text, size_t len, int color, int sx, int sy );
bool     sdlscr_gfx_draw( int layer, const char* cmd, size_t len, char* err, size_t errsz );

// the text screen (more above: sdlscr_printf, sdlscr_puts, sdlscr_ansi_*, sdlscr_locate, sdlscr_gettextcursor)
int      sdlscr_text_select( int screen );                      // returns the previously selected screen, -1 if the number is bad
int      sdlscr_text_selected( void );
bool     sdlscr_text_setviewport( int x, int y, int w, int h );
void     sdlscr_text_resetviewport( void );
void     sdlscr_text_getviewport( int* x, int* y, int* w, int* h );
void     sdlscr_text_setblank( int bg, int fg );
void     sdlscr_text_setcolors( int bg, int fg );
void     sdlscr_text_getcolors( int* bg, int* fg );
int      sdlscr_text_write( int y, int x, int bg, int fg, const char* text, int len );   // no wrapping; returns the characters written
uint32_t sdlscr_text_getcell( int y, int x );                   // character | paper << 16 | pen << 8, 0 outside
void     sdlscr_text_getcursor( int* x, int* y );
void     sdlscr_text_clear( void );
void     sdlscr_text_fill( int x1, int y1, int x2, int y2, int ch, int fg, int bg );     // -1 leaves that part alone
void     sdlscr_text_copy( int x1, int y1, int x2, int y2, int tx, int ty );
void     sdlscr_text_invert( int x1, int y1, int x2, int y2 );
size_t   sdlscr_text_save( uint8_t* out, size_t outsize );      // 4 bytes per cell; returns the size needed
bool     sdlscr_text_restore( const uint8_t* data, size_t size );
void     sdlscr_text_defchar( int code, const uint8_t rows[12] );

// tiles (more above: sdlscr_writetile, sdlscr_scrolltiles)
void     sdlscr_tile_setcell( int x, int y, int tile );
int      sdlscr_tile_getcell( int x, int y );
void     sdlscr_tile_setcells( const uint8_t* cells, size_t count );
bool     sdlscr_tile_getimage( int tile, uint8_t out[TILE_WIDTH * TILE_HEIGHT] );

// sprites (more above: sdlscr_showsprite, sdlscr_spriteprio, sdlscr_movesprite, sdlscr_spriteanimdata, sdlscr_spriteanimcfg, sdlscr_writesprite)
void     sdlscr_sprite_setimage( int sprno, int image );
int      sdlscr_sprite_getimage( int sprno );
bool     sdlscr_sprite_getpos( int sprno, int* x, int* y );
bool     sdlscr_sprite_active( int sprno );
bool     sdlscr_sprite_getmap( int image, uint8_t out[SPRITE_WIDTH * SPRITE_HEIGHT] );
void     sdlscr_sprite_moveto( int sprno, int x, int y, int ticks );
bool     sdlscr_sprite_moving( int sprno );
void     sdlscr_sprite_setcollide( int sprno, bool sprites, bool tiles );
uint32_t sdlscr_sprite_hits( int sprno );
int      sdlscr_sprite_collide( int sprno );
int      sdlscr_sprite_nextevent( void );

// timing (sdlscr_getframecnt() is above)
void     sdlscr_vsync( void );                                  // waits for the next frame (or until the screen is closed)

#endif
