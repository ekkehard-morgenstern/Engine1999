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

static textcell_t textscr[TXTSCR_WIDTH * TXTSCR_HEIGHT];
static uint8_t    currfont[FONTSIZE];
static uint8_t    pen, paper, cursx, cursy, blnkti;
static bool       cursena, curson;

extern const uint8_t fontdef1[FONTSIZE];

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
      *outsx = TXTSCR_WIDTH; *outsy = TXTSCR_HEIGHT;
}

void txtscr_getcursor( int* outx, int* outy ) {
      *outx = cursx; *outy = cursy;
}

void txtscr_init( void ) {
      memcpy( currfont, fontdef1, FONTSIZE );
      textcell_t cell = TXTSCR_MAKECELL( 32, TXTSCR_BGCOL, TXTSCR_FGCOL );
      paper = TXTSCR_BGCOL;
      pen = TXTSCR_FGCOL;
      cursx = 0;
      cursy = 0;
      blnkti = 0;
      cursena = true;
      curson = true;
      for ( size_t i=0; i < TXTSCR_WIDTH * TXTSCR_HEIGHT; ++i ) {
            textscr[i] = cell;
      }
}

void txtscr_render( uint8_t* target ) {
      const textcell_t* s = &textscr[0];
      uint8_t* d = target;
      int stride = TXTSCR_WIDTH * 8;
      for ( uint8_t y=0; y < (uint8_t) TXTSCR_HEIGHT; ++y ) {
            for ( uint8_t x=0; x < (uint8_t) TXTSCR_WIDTH; ++x ) {
                  textcell_t cell = *s++;
                  uint8_t bg = TXTSCR_CELL_BG(cell);
                  uint8_t fg = TXTSCR_CELL_FG(cell);
                  uint8_t ch = TXTSCR_CELL_CHR(cell);
                  if ( cursena && curson && y == cursy && x == cursx ) {
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

int txtscr_write( int y, int x, int bg, int fg, const char* text, int len ) {
      const char* s = text;
      size_t n = len < 0 ? strlen( s ) : len;
      if ( x <= (int)(-n) ) {
            return 0;
      } else if ( x < 0 ) {
            int skip = -x;
            x = 0;
            n -= skip; text += skip;
      }
      if ( n > (size_t)( TXTSCR_WIDTH - x ) ) {
            n = (size_t)( TXTSCR_WIDTH - x );
      }
      if ( x >= TXTSCR_WIDTH || y < 0 || y > TXTSCR_HEIGHT ) {
            return 0;
      }
      if ( bg < 0 ) {
            bg = paper;
      }
      if ( fg < 0 ) {
            fg = pen;
      }
      if ( bg < 0 || bg > 255 || fg < 0 || fg > 255 || text == 0 ) {
            return 0;
      }
      textcell_t* d = &textscr[ y * TXTSCR_WIDTH + x ];
      int ret = n;
      while ( n-- ) {
            *d++ = TXTSCR_MAKECELL( *s++, bg, fg );
      }
      return ret;
}

void txtscr_scrolldown( void ) {
      size_t chunksz = TXTSCR_WIDTH * ( TXTSCR_HEIGHT - 1 );
      memmove( &textscr[TXTSCR_WIDTH], &textscr[0], sizeof(textcell_t) * chunksz );
      textcell_t cell = TXTSCR_MAKECELL( 32, TXTSCR_BGCOL, TXTSCR_FGCOL );
      for ( size_t i=0; i < TXTSCR_WIDTH; ++i ) {
            textscr[i] = cell;
      }
}

void txtscr_backspace( void ) {
      if ( cursx == 0 ) {
            cursx = TXTSCR_WIDTH - 1;
            if ( --cursy < 0 ) {
                  cursy = 0;
                  txtscr_scrolldown();
            }
            return;
      }
      --cursx;
}

void txtscr_rubout( void ) {
      txtscr_backspace();
      txtscr_write( cursy, cursx, -1, -1, " ", 1 );
}

void txtscr_locate( int x, int y ) {
      if ( x < 0 ) {
            x = cursx;
      }
      if ( y < 0 ) {
            y = cursy;
      }
      if ( x < 0 || x >= TXTSCR_WIDTH || y < 0 || y >= TXTSCR_HEIGHT ) {
            return;
      }
      cursx = x; cursy = y;
}

void txtscr_scrollup( void ) {
      size_t chunksz = TXTSCR_WIDTH * ( TXTSCR_HEIGHT - 1 );
      memmove( &textscr[0], &textscr[TXTSCR_WIDTH], sizeof(textcell_t) * chunksz );
      textcell_t cell = TXTSCR_MAKECELL( 32, TXTSCR_BGCOL, TXTSCR_FGCOL );
      for ( size_t i=0; i < TXTSCR_WIDTH; ++i ) {
            textscr[chunksz + i] = cell;
      }
}

void txtscr_print( int y, int x, int bg, int fg, const char* text ) {
      const char* s = text;
      int cx = x >= 0 ? x : cursx, cy = y >= 0 ? y : cursy;
      while ( *s != '\0' ) {
            const char* s0 = s;
            while ( *s != '\n' && *s != '\0' ) {
                  ++s;
            }
            size_t len = s - s0;
            while ( len ) {
                  int n = txtscr_write( cy, cx, bg, fg, s0, (int) len );
                  if ( n == 0 ) {
                        break;
                  }
                  cx += n; len -= n; s0 += n;
                  if ( cx >= TXTSCR_WIDTH ) {
                        cx = 0;
                        ++cy;
                        if ( cy >= TXTSCR_HEIGHT ) {
                              cy = TXTSCR_HEIGHT - 1;
                              txtscr_scrollup();
                        }
                  }
            }
            if ( *s == '\n' ) {
                  ++s;
                  cx = 0;
                  if ( ++cy >= TXTSCR_HEIGHT ) {
                        cy = TXTSCR_HEIGHT - 1;
                        txtscr_scrollup();
                  }
            }
      }
      cursx = cx; cursy = cy;
}

void txtscr_printf( int y, int x, int bg, int fg, const char* fmt, ... ) {
      static char buf[512];
      va_list ap;
      va_start( ap, fmt );
      vsnprintf( buf, 512U, fmt, ap );
      va_end( ap );
      txtscr_print( y, x, bg, fg, buf );
}
