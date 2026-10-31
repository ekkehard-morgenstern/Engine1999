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

#include "textscreen.h"

#define FONTSIZE 3072
#define BLINKCOUNTER_ON       45
#define BLINKCOUNTER_OFF      15

// one text screen: the cells, the pen/paper, the cursor and the viewport. The cursor and all coordinates of the
// txtscr_* functions are relative to the viewport, which is the whole screen unless set otherwise.
typedef struct _txtscreen_t {
      textcell_t  cells[TXTSCR_WIDTH * TXTSCR_HEIGHT];
      int         pen, paper, blankbg, blankfg;
      int         cursx, cursy;
      int         vx, vy, vw, vh;
} txtscreen_t;

static txtscreen_t screens[TXTSCR_NSCREENS];
static txtscreen_t* cur = &screens[0];      // the selected screen: the one drawn and written to
static int        selected;
static uint8_t    currfont[FONTSIZE];
static uint8_t    blnkti;
static bool       cursena, curson;

extern const uint8_t fontdef1[FONTSIZE];

// -- cursor ----------------------------------------------------------------------------

bool txtscr_enablecursor( bool enable ) {
      bool old = cursena;
      cursena = enable;
      return old != enable;
}

bool txtscr_blinkcursor( void ) {
      if ( !cursena ) {
            return false;
      }
      if ( blnkti ) {
            --blnkti;
      }
      if ( blnkti == 0 ) {
            curson = !curson;
            blnkti = curson ? BLINKCOUNTER_ON : BLINKCOUNTER_OFF;
            return true;
      }
      return false;
}

void txtscr_getsize( int* outsx, int* outsy ) {
      *outsx = cur->vw; *outsy = cur->vh;
}

void txtscr_getcursor( int* outx, int* outy ) {
      *outx = cur->cursx; *outy = cur->cursy;
}

// -- screens and viewport --------------------------------------------------------------

static void txtscr_initscreen( txtscreen_t* sc ) {
      sc->pen = sc->blankfg = TXTSCR_FGCOL;
      sc->paper = sc->blankbg = TXTSCR_BGCOL;
      sc->cursx = sc->cursy = 0;
      sc->vx = sc->vy = 0;
      sc->vw = TXTSCR_WIDTH; sc->vh = TXTSCR_HEIGHT;
      textcell_t cell = TXTSCR_MAKECELL( 32, TXTSCR_BGCOL, TXTSCR_FGCOL );
      for ( size_t i=0; i < TXTSCR_WIDTH * TXTSCR_HEIGHT; ++i ) {
            sc->cells[i] = cell;
      }
}

void txtscr_init( void ) {
      memcpy( currfont, fontdef1, FONTSIZE );
      for ( int i=0; i < TXTSCR_NSCREENS; ++i ) {
            txtscr_initscreen( &screens[i] );
      }
      selected = 0;
      cur = &screens[0];
      blnkti = 0;
      cursena = true;
      curson = true;
}

bool txtscr_select( int n ) {
      if ( n < 0 || n >= TXTSCR_NSCREENS ) {
            return false;
      }
      selected = n;
      cur = &screens[n];
      return true;
}

int txtscr_selected( void ) {
      return selected;
}

bool txtscr_setviewport( int x, int y, int w, int h ) {
      if ( x < 0 || y < 0 || w < 1 || h < 1 || x + w > TXTSCR_WIDTH || y + h > TXTSCR_HEIGHT ) {
            return false;
      }
      cur->vx = x; cur->vy = y; cur->vw = w; cur->vh = h;
      cur->cursx = cur->cursy = 0;
      return true;
}

void txtscr_resetviewport( void ) {
      txtscr_setviewport( 0, 0, TXTSCR_WIDTH, TXTSCR_HEIGHT );
}

void txtscr_getviewport( int* outx, int* outy, int* outw, int* outh ) {
      *outx = cur->vx; *outy = cur->vy; *outw = cur->vw; *outh = cur->vh;
}

void txtscr_setcolors( int bg, int fg ) {
      if ( bg >= 0 && bg <= 255 ) {
            cur->paper = bg;
      }
      if ( fg >= 0 && fg <= 255 ) {
            cur->pen = fg;
      }
}

void txtscr_getcolors( int* outbg, int* outfg ) {
      *outbg = cur->paper; *outfg = cur->pen;
}

void txtscr_setblank( int bg, int fg ) {
      if ( bg >= 0 && bg <= 255 ) {
            cur->blankbg = bg;
      }
      if ( fg >= 0 && fg <= 255 ) {
            cur->blankfg = fg;
      }
}

static textcell_t txtscr_blankcell( void ) {
      return TXTSCR_MAKECELL( 32, cur->blankbg, cur->blankfg );
}

static textcell_t* txtscr_row( int y ) {      // first cell of row y of the viewport
      return &cur->cells[ ( cur->vy + y ) * TXTSCR_WIDTH + cur->vx ];
}

textcell_t txtscr_getcell( int y, int x ) {
      if ( x < 0 || y < 0 || x >= cur->vw || y >= cur->vh ) {
            return 0;
      }
      return txtscr_row( y )[x];
}

void txtscr_clear( void ) {
      textcell_t cell = txtscr_blankcell();
      for ( int y=0; y < cur->vh; ++y ) {
            textcell_t* d = txtscr_row( y );
            for ( int x=0; x < cur->vw; ++x ) {
                  *d++ = cell;
            }
      }
      cur->cursx = cur->cursy = 0;
}

// -- areas, save, font -----------------------------------------------------------------

static bool txtscr_area( int* x1, int* y1, int* x2, int* y2 ) {
      if ( *x1 > *x2 ) { int t = *x1; *x1 = *x2; *x2 = t; }
      if ( *y1 > *y2 ) { int t = *y1; *y1 = *y2; *y2 = t; }
      if ( *x2 < 0 || *y2 < 0 || *x1 >= cur->vw || *y1 >= cur->vh ) {
            return false;
      }
      if ( *x1 < 0 ) *x1 = 0;
      if ( *y1 < 0 ) *y1 = 0;
      if ( *x2 >= cur->vw ) *x2 = cur->vw - 1;
      if ( *y2 >= cur->vh ) *y2 = cur->vh - 1;
      return true;
}

void txtscr_fill( int x1, int y1, int x2, int y2, int ch, int fg, int bg ) {
      if ( !txtscr_area( &x1, &y1, &x2, &y2 ) ) {
            return;
      }
      for ( int y = y1; y <= y2; ++y ) {
            textcell_t* d = txtscr_row( y );
            for ( int x = x1; x <= x2; ++x ) {
                  textcell_t c = d[x];
                  uint8_t nch = ch >= 0 ? (uint8_t) ch : TXTSCR_CELL_CHR( c );
                  uint8_t nfg = fg >= 0 ? (uint8_t) fg : TXTSCR_CELL_FG( c );
                  uint8_t nbg = bg >= 0 ? (uint8_t) bg : TXTSCR_CELL_BG( c );
                  d[x] = TXTSCR_MAKECELL( nch, nbg, nfg );
            }
      }
}

void txtscr_copy( int x1, int y1, int x2, int y2, int tx, int ty ) {
      if ( !txtscr_area( &x1, &y1, &x2, &y2 ) ) {
            return;
      }
      int w = x2 - x1 + 1, h = y2 - y1 + 1;
      textcell_t* tmp = (textcell_t*) malloc( (size_t) w * (size_t) h * sizeof(textcell_t) );
      if ( tmp == 0 ) {
            return;
      }
      for ( int y = 0; y < h; ++y ) {
            memcpy( tmp + (size_t) y * (size_t) w, txtscr_row( y1 + y ) + x1, (size_t) w * sizeof(textcell_t) );
      }
      for ( int y = 0; y < h; ++y ) {
            int dy = ty + y;
            if ( dy < 0 || dy >= cur->vh ) {
                  continue;
            }
            textcell_t* d = txtscr_row( dy );
            for ( int x = 0; x < w; ++x ) {
                  int dx = tx + x;
                  if ( dx >= 0 && dx < cur->vw ) {
                        d[dx] = tmp[ (size_t) y * (size_t) w + (size_t) x ];
                  }
            }
      }
      free( tmp );
}

void txtscr_invert( int x1, int y1, int x2, int y2 ) {
      if ( !txtscr_area( &x1, &y1, &x2, &y2 ) ) {
            return;
      }
      for ( int y = y1; y <= y2; ++y ) {
            textcell_t* d = txtscr_row( y );
            for ( int x = x1; x <= x2; ++x ) {
                  textcell_t c = d[x];
                  d[x] = TXTSCR_MAKECELL( TXTSCR_CELL_CHR( c ), TXTSCR_CELL_FG( c ), TXTSCR_CELL_BG( c ) );
            }
      }
}

size_t txtscr_save( uint8_t* out, size_t outsize ) {
      size_t size = (size_t) TXTSCR_WIDTH * TXTSCR_HEIGHT * 4U;
      if ( out == 0 || outsize < size ) {
            return size;
      }
      for ( size_t i=0; i < (size_t) TXTSCR_WIDTH * TXTSCR_HEIGHT; ++i ) {
            textcell_t c = cur->cells[i];
            out[ i * 4U ]      = TXTSCR_CELL_CHR( c );
            out[ i * 4U + 1U ] = TXTSCR_CELL_FG( c );
            out[ i * 4U + 2U ] = TXTSCR_CELL_BG( c );
            out[ i * 4U + 3U ] = 0;
      }
      return size;
}

bool txtscr_restore( const uint8_t* data, size_t size ) {
      if ( data == 0 || size != (size_t) TXTSCR_WIDTH * TXTSCR_HEIGHT * 4U ) {
            return false;
      }
      for ( size_t i=0; i < (size_t) TXTSCR_WIDTH * TXTSCR_HEIGHT; ++i ) {
            cur->cells[i] = TXTSCR_MAKECELL( data[ i * 4U ], data[ i * 4U + 2U ], data[ i * 4U + 1U ] );
      }
      return true;
}

void txtscr_defchar( int code, const uint8_t rows[12] ) {
      if ( code < 0 || code > 255 || rows == 0 ) {
            return;
      }
      memcpy( &currfont[ (size_t) code * 12U ], rows, 12U );
}

void txtscr_resetfont( void ) {
      memcpy( currfont, fontdef1, FONTSIZE );
}

const uint8_t* txtscr_font( void ) {
      return currfont;
}

// -- rendering -------------------------------------------------------------------------

void txtscr_render( uint8_t* target ) {
      const textcell_t* s = &cur->cells[0];
      uint8_t* d = target;
      int stride = TXTSCR_WIDTH * 8;
      int ccx = cur->vx + cur->cursx, ccy = cur->vy + cur->cursy;
      for ( uint8_t y=0; y < (uint8_t) TXTSCR_HEIGHT; ++y ) {
            for ( uint8_t x=0; x < (uint8_t) TXTSCR_WIDTH; ++x ) {
                  textcell_t cell = *s++;
                  uint8_t bg = TXTSCR_CELL_BG(cell);
                  uint8_t fg = TXTSCR_CELL_FG(cell);
                  uint8_t ch = TXTSCR_CELL_CHR(cell);
                  if ( cursena && curson && y == ccy && x == ccx ) {
                        uint8_t t = fg; fg = bg; bg = t;
                  }
                  const uint8_t* fontchar = &currfont[ ch * 12 ];
                  uint8_t* d0 = d;
                  for ( uint8_t cy=0; cy < UINT8_C(12); ++cy ) {
                        uint8_t by = *fontchar++;
                        for ( uint8_t cx=0; cx < UINT8_C(8); ++cx ) {
                              *d++ = by & UINT8_C(0X80) ? fg : bg;
                              by <<= UINT8_C(1);
                        }
                        d += stride - 8;
                  }
                  d = d0 + 8;
            }
            d += stride * 11;
      }
}

// -- output ----------------------------------------------------------------------------

// writes at most the rest of the viewport row; returns the number of characters written
int txtscr_write( int y, int x, int bg, int fg, const char* text, int len ) {
      if ( text == 0 ) {
            return 0;
      }
      int n = len < 0 ? (int) strlen( text ) : len;
      if ( y < 0 || y >= cur->vh || x >= cur->vw || n <= 0 ) {
            return 0;
      }
      if ( x < 0 ) {
            if ( x <= -n ) {
                  return 0;
            }
            n += x; text -= x; x = 0;
      }
      if ( n > cur->vw - x ) {
            n = cur->vw - x;
      }
      if ( bg < 0 ) {
            bg = cur->paper;
      }
      if ( fg < 0 ) {
            fg = cur->pen;
      }
      if ( bg > 255 || fg > 255 ) {
            return 0;
      }
      textcell_t* d = txtscr_row( y ) + x;
      int ret = n;
      while ( n-- ) {
            *d++ = TXTSCR_MAKECELL( (uint8_t) *text++, bg, fg );     // (unsigned: codes >= 128 must not spill into the colors)
      }
      return ret;
}

void txtscr_scrolldown( void ) {
      for ( int y = cur->vh - 1; y > 0; --y ) {
            memmove( txtscr_row( y ), txtscr_row( y - 1 ), sizeof(textcell_t) * cur->vw );
      }
      textcell_t cell = txtscr_blankcell();
      textcell_t* d = txtscr_row( 0 );
      for ( int x=0; x < cur->vw; ++x ) {
            *d++ = cell;
      }
}

void txtscr_scrollup( void ) {
      for ( int y=0; y < cur->vh - 1; ++y ) {
            memmove( txtscr_row( y ), txtscr_row( y + 1 ), sizeof(textcell_t) * cur->vw );
      }
      textcell_t cell = txtscr_blankcell();
      textcell_t* d = txtscr_row( cur->vh - 1 );
      for ( int x=0; x < cur->vw; ++x ) {
            *d++ = cell;
      }
}

void txtscr_backspace( void ) {
      if ( cur->cursx == 0 ) {
            cur->cursx = cur->vw - 1;
            if ( cur->cursy > 0 ) {
                  --cur->cursy;
            } else {
                  txtscr_scrolldown();
            }
            return;
      }
      --cur->cursx;
}

void txtscr_rubout( void ) {
      txtscr_backspace();
      txtscr_write( cur->cursy, cur->cursx, -1, -1, " ", 1 );
}

void txtscr_locate( int x, int y ) {
      if ( x < 0 ) {
            x = cur->cursx;
      }
      if ( y < 0 ) {
            y = cur->cursy;
      }
      if ( x >= cur->vw || y >= cur->vh ) {
            return;
      }
      cur->cursx = x; cur->cursy = y;
}

// text that doesn't fit in the rest of a viewport row continues in the next one; below the last row the viewport scrolls
void txtscr_print( int y, int x, int bg, int fg, const char* text ) {
      txtscr_print_n( y, x, bg, fg, text, strlen( text ) );
}

// the same for len bytes: zero bytes are characters (the glyph of code 0), only '\n' is a line feed
void txtscr_print_n( int y, int x, int bg, int fg, const char* text, size_t textlen ) {
      const char* s = text;
      const char* end = text + textlen;
      int cx = x >= 0 ? x : cur->cursx, cy = y >= 0 ? y : cur->cursy;
      while ( s < end ) {
            const char* s0 = s;
            while ( s < end && *s != '\n' ) {
                  ++s;
            }
            size_t len = s - s0;
            while ( len ) {
                  int n = txtscr_write( cy, cx, bg, fg, s0, (int) len );
                  if ( n == 0 ) {
                        break;
                  }
                  cx += n; len -= n; s0 += n;
                  if ( cx >= cur->vw ) {
                        cx = 0;
                        ++cy;
                        if ( cy >= cur->vh ) {
                              cy = cur->vh - 1;
                              txtscr_scrollup();
                        }
                  }
            }
            if ( s < end && *s == '\n' ) {
                  ++s;
                  cx = 0;
                  if ( ++cy >= cur->vh ) {
                        cy = cur->vh - 1;
                        txtscr_scrollup();
                  }
            }
      }
      cur->cursx = cx; cur->cursy = cy;
}

void txtscr_printf( int y, int x, int bg, int fg, const char* fmt, ... ) {
      static char buf[512];
      va_list ap;
      va_start( ap, fmt );
      vsnprintf( buf, 512U, fmt, ap );
      va_end( ap );
      txtscr_print( y, x, bg, fg, buf );
}
