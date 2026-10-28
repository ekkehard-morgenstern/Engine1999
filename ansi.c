
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

#include "ansi.h"

// -- colors ----------------------------------------------------------------------------

// ANSI colors 0..7 (black red green yellow blue magenta cyan white), then the bright ones 8..15, as entries of the
// palette in sdllayer.c: Black Maroon Green Olive Navy Purple Teal Silver / Gray Red Lime Yellow Blue Fuchsia Aqua White
static const uint8_t ansipal[16] = { 1, 44, 7, 46, 2, 45, 8, 74, 47, 110, 13, 137, 5, 111, 15, 140 };

enum { ANSI_ST_TEXT, ANSI_ST_ESC, ANSI_ST_CSI };

void ansi_init( ansi_t* a, int deffg, int defbg ) {
    memset( a, 0, sizeof(*a) );
    a->deffg = a->fg = deffg;
    a->defbg = a->bg = defbg;
    a->state = ANSI_ST_TEXT;
}

// -- SGR -------------------------------------------------------------------------------

static void ansi_sgr( ansi_t* a ) {
    int n = a->nparams;
    if ( n > ANSI_MAXPARAMS ) {
        n = ANSI_MAXPARAMS;
    }
    if ( n == 0 ) {
        a->params[0] = 0;       // ESC [ m = ESC [ 0 m
        n = 1;
    }
    for ( int i=0; i < n; ++i ) {
        int p = a->params[i];
        if ( p == 0 ) {
            a->fg = a->deffg; a->bg = a->defbg;
        } else if ( p >= 30 && p <= 37 ) {
            a->fg = ansipal[p-30];
        } else if ( p >= 40 && p <= 47 ) {
            a->bg = ansipal[p-40];
        } else if ( p >= 90 && p <= 97 ) {
            a->fg = ansipal[p-90+8];
        } else if ( p >= 100 && p <= 107 ) {
            a->bg = ansipal[p-100+8];
        } else if ( p == 39 ) {
            a->fg = a->deffg;
        } else if ( p == 49 ) {
            a->bg = a->defbg;
        } else if ( p == 38 || p == 48 ) {
            int* target = p == 38 ? &a->fg : &a->bg;
            if ( i + 1 < n && a->params[i+1] == 5 ) {
                if ( i + 2 < n && a->params[i+2] >= 0 && a->params[i+2] <= 255 ) {
                    *target = a->params[i+2];
                }
                i += 2;
            } else if ( i + 1 < n && a->params[i+1] == 2 ) {
                i += 4;         // r;g;b: no true color
            }
        }
    }
}

// -- decoder ---------------------------------------------------------------------------

static void ansi_start_csi( ansi_t* a ) {
    a->state = ANSI_ST_CSI;
    a->nparams = 0;
    a->priv = false;
    memset( a->params, 0, sizeof(a->params) );
}

void ansi_feed( ansi_t* a, const char* text, ansi_emit_t emit, void* usrdata ) {
    char run[ANSI_RUNMAX + 1U];
    size_t len = 0;
    for ( const unsigned char* s = (const unsigned char*) text; *s != '\0'; ++s ) {
        unsigned char c = *s;
        if ( a->state == ANSI_ST_TEXT ) {
            if ( c == 0x1B ) {
                a->state = ANSI_ST_ESC;
                continue;
            }
            run[len++] = (char) c;
            if ( len == ANSI_RUNMAX ) {
                run[len] = '\0';
                emit( usrdata, a->bg, a->fg, run );
                len = 0;
            }
            continue;
        }
        // the colors may change here: hand over the text before the sequence first
        if ( len > 0 && a->state == ANSI_ST_ESC && c == '[' ) {
            run[len] = '\0';
            emit( usrdata, a->bg, a->fg, run );
            len = 0;
        }
        if ( a->state == ANSI_ST_ESC ) {
            if ( c == '[' ) {
                ansi_start_csi( a );
            } else {
                a->state = ANSI_ST_TEXT;        // another escape sequence of two characters: skipped
            }
            continue;
        }
        // in a CSI sequence: parameter bytes 0x30..0x3F, intermediate bytes 0x20..0x2F, final byte 0x40..0x7E
        if ( c >= 0x30 && c <= 0x39 ) {
            if ( a->nparams == 0 ) {
                a->nparams = 1;
            }
            int idx = a->nparams - 1;
            if ( idx < ANSI_MAXPARAMS && a->params[idx] < 100000 ) {
                a->params[idx] = a->params[idx] * 10 + ( c - 0x30 );
            }
        } else if ( c == ';' ) {
            if ( a->nparams == 0 ) {
                a->nparams = 1;         // empty first parameter
            }
            ++a->nparams;
        } else if ( c >= 0x3A && c <= 0x3F ) {
            a->priv = true;             // ':' and the private markers: not understood
        } else if ( c >= 0x20 && c <= 0x2F ) {
            a->priv = true;             // intermediate bytes: none of the sequences we know
        } else if ( c >= 0x40 && c <= 0x7E ) {
            if ( c == 'm' && !a->priv ) {
                ansi_sgr( a );
            }
            a->state = ANSI_ST_TEXT;
        } else {
            a->state = ANSI_ST_TEXT;    // a control character or a byte >= 0x7F aborts the sequence ...
            --s;                        // ... and is text again
        }
    }
    if ( len > 0 ) {
        run[len] = '\0';
        emit( usrdata, a->bg, a->fg, run );
    }
}
