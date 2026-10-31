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


#ifndef GFXOPS_H
#define GFXOPS_H    1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

/*
The graphics, tile, sprite and text screen statements and functions of BASIC as a table (docs/graphics-subsystem.md). The parser, the
code generator, the VM and the hosts use the same table, so a statement is described once:

    GFX PSET x, y [, c]     group "GFX", name "PSET", signature "nno"

Signature characters, one per argument in source order:
    n   a numeric expression              o   an optional numeric expression (omitted: GFX_NONE is passed)
    s   a string expression               v   a string variable that receives the string result of the operation (always last)
    w   a word out of the command's word list (required)    W   the same, optional (omitted: GFX_NONE)
A word is looked up in the list (words "A,B,C" gives A = 0, B = 1, C = 2) and passed as that number.
The statement runs the instruction GFXC with the operation number; the VM pops the arguments, calls the host and pushes the string result for 'v'.
*/

#define GFX_NONE        (-1.0e9)        // "argument omitted"
#define GFX_MAXARGS     8

// operations (vm_gfx_t.op)
#define GFXOP_LAYER     1               // GFX LAYER n, ON|OFF
#define GFXOP_PRIORITY  2               // GFX PRIORITY n, p
#define GFXOP_PALETTE   3               // GFX PALETTE n, i, argb
#define GFXOP_PALNAME   4               // GFX PALNAME n, i, name$
#define GFXOP_PALRESET  5               // GFX PALRESET [n]
#define GFXOP_BACKDROP  6               // GFX BACKDROP argb
#define GFXOP_TARGET    7               // GFX TARGET 0|3
#define GFXOP_PEN       8               // GFX PEN c
#define GFXOP_MODE      9               // GFX MODE SET|CLEAR|XOR|OR|AND
#define GFXOP_CLEAR     10              // GFX CLEAR [c]
#define GFXOP_PSET      11              // GFX PSET x, y [, c]
#define GFXOP_LINE      12              // GFX LINE x1, y1, x2, y2 [, c [, B|BF]]
#define GFXOP_CIRCLE    13              // GFX CIRCLE x, y, r [, c [, a1, a2 [, ratio]]]
#define GFXOP_PAINT     14              // GFX PAINT x, y, c [, border]
#define GFXOP_RADIUS    15              // GFX RADIUS x, y, r, angle [, c]
#define GFXOP_GRAB      16              // GFX GRAB x1, y1, x2, y2, a$
#define GFXOP_BLIT      17              // GFX BLIT a$, x, y [, TRANS]
#define GFXOP_TEXT      18              // GFX TEXT x, y, a$ [, c [, sx [, sy]]]
#define GFXOP_DRAW      19              // GFX DRAW a$
#define GFXOP_VSYNC     20              // GFX VSYNC
#define GFXOP_TXTFILL   30              // TXT FILL x1, y1, x2, y2 [, chr [, fg [, bg]]]
#define GFXOP_TXTCOPY   31              // TXT COPY x1, y1, x2, y2, tx, ty
#define GFXOP_TXTINVERT 32              // TXT INVERT x1, y1, x2, y2
#define GFXOP_TXTSAVE   33              // TXT SAVE a$
#define GFXOP_TXTLOAD   34              // TXT LOAD a$
#define GFXOP_CLS       40              // CLS
#define GFXOP_LOCATE    41              // LOCATE x, y
#define GFXOP_COLOR     42              // COLOR fg [, bg]
#define GFXOP_SYMBOL    43              // SYMBOL c, a$        (12 bytes)
#define GFXOP_TILEDEF   50              // TILE DEF n, a$      (384 bytes)
#define GFXOP_TILEGET   51              // TILE GET n, a$
#define GFXOP_TILESET   52              // TILE SET x, y, n
#define GFXOP_TILEFILL  53              // TILE FILL a$        (533 bytes)
#define GFXOP_TILESCROLL 54             // TILE SCROLL sx, sy
#define GFXOP_SPRDEF    60              // SPRITE DEF n, a$    (384 bytes)
#define GFXOP_SPRGET    61              // SPRITE GET n, a$
#define GFXOP_SPRSEQ    62              // SPRITE SEQ a, a$    (animation sequence: the image numbers)
#define GFXOP_SPRON     63              // SPRITE n, ON
#define GFXOP_SPROFF    64              // SPRITE n, OFF
#define GFXOP_SPRPRIO   65              // SPRITE n, PRIORITY p
#define GFXOP_SPRIMAGE  66              // SPRITE n, IMAGE i
#define GFXOP_SPRANIM   67              // SPRITE n, ANIM a, length, speed
#define GFXOP_SPRMOVETO 68              // SPRITE n, MOVETO x, y, ticks
#define GFXOP_SPRCOLL   69              // SPRITE n, COLLISION ON|OFF [, TILES]
#define GFXOP_SPRMOVE   70              // SPRITE n, x, y [, image]

// queries (vm_gfx_t.query): functions of the language
#define GFXQ_RGB        1               // RGB(r, g, b [, a]): computed by the VM itself
#define GFXQ_POINT      2               // POINT(x, y)
#define GFXQ_FRAME      10              // GFX("FRAME")
#define GFXQ_LAYER      11              // GFX("LAYER", n): 1 if enabled
#define GFXQ_PRIORITY   12              // GFX("PRIORITY", n)
#define GFXQ_PALETTE    13              // GFX("PALETTE", layer, i): the ARGB value
#define GFXQ_TARGET     14              // GFX("TARGET")
#define GFXQ_TXTX       20              // TXT("X")
#define GFXQ_TXTY       21              // TXT("Y")
#define GFXQ_TXTFG      22              // TXT("FG")
#define GFXQ_TXTBG      23              // TXT("BG")
#define GFXQ_TXTCHAR    24              // TXT("CHAR", x, y)
#define GFXQ_TXTFGAT    25              // TXT("FGAT", x, y)
#define GFXQ_TXTBGAT    26              // TXT("BGAT", x, y)
#define GFXQ_TXTWIDTH   27              // TXT("WIDTH")
#define GFXQ_TXTHEIGHT  28              // TXT("HEIGHT")
#define GFXQ_TILECELL   30              // TILE(x, y)
#define GFXQ_SPRX       40              // SPRITE("X", n)
#define GFXQ_SPRY       41
#define GFXQ_SPRIMAGE   42
#define GFXQ_SPRON      43
#define GFXQ_SPRHITS    44              // SPRITE("HITS", n): the indicator bits (1 sprite, 2 tile), cleared
#define GFXQ_SPRCOLLIDE 45              // SPRITE("COLLIDE", n): the other sprite, -1 none
#define GFXQ_SPRMOVING  46
#define GFXQ_EVENT      90              // (the VM polls it for ON COLLISION) a sprite with a new collision, -1 none

typedef struct _gfxcmd_t {
    const char* group;                  // "GFX", "TXT", "TILE", "SPRITE", or "" for the statements that are reserved words themselves
    const char* name;                   // the word after the group (the statement itself for group ""; "" for the position form of SPRITE)
    uint16_t    op;
    const char* sig;
    const char* words;                  // word lists, one per w / W in the signature, separated by '|'; each is a comma separated list
    bool        lead;                   // SPRITE only: the first argument (a number) comes before the word:  SPRITE n, ON
} gfxcmd_t;

typedef struct _gfxquery_t {
    const char* group;                  // "GFX", "TXT", "SPRITE" (selector first); "" for POINT / RGB / TILE (no selector)
    const char* sel;
    uint16_t    q;
    int         minargs, maxargs;       // numeric arguments after the selector
} gfxquery_t;

const gfxcmd_t* gfxops_find( const char* group, const char* name );        // names match without regard to case; 0 if unknown
const gfxcmd_t* gfxops_byop( uint16_t op );
const gfxquery_t* gfxops_findquery( const char* group, const char* sel );
const gfxquery_t* gfxops_queryby( uint16_t q );
int gfxops_word( const char* words, int slot, const char* word );          // the number of word in the list of slot, -1 if not in it
int gfxops_sigargs( const char* sig );                                      // the number of values the VM pops (not counting 'v')
const char* gfxops_opname( uint16_t op );                                   // "GFX PSET" for logging and the monitor

#endif
