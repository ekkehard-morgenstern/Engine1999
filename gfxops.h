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
#define GFXOP_RESET     21              // GFX RESET [ALL]
#define GFXOP_GFXLOAD   22              // GFX LOAD a$ [, x, y [, PALETTE]]       (a BMP file onto the target layer)
#define GFXOP_GFXSAVE   23              // GFX SAVE a$ [, x1, y1, x2, y2]
#define GFXOP_PALLOAD   24              // GFX PALLOAD layer, a$                  (the colors of a BMP file into the palette of a layer)
#define GFXOP_PALSAVE   25              // GFX PALSAVE layer, a$
#define GFXOP_TXTFILL   30              // TXT FILL x1, y1, x2, y2 [, chr [, fg [, bg]]]
#define GFXOP_TXTCOPY   31              // TXT COPY x1, y1, x2, y2, tx, ty
#define GFXOP_TXTINVERT 32              // TXT INVERT x1, y1, x2, y2
#define GFXOP_TXTSAVE   33              // TXT SAVE a$
#define GFXOP_TXTLOAD   34              // TXT LOAD a$
#define GFXOP_CLS       40              // CLS
#define GFXOP_LOCATE    41              // LOCATE x, y
#define GFXOP_COLOR     42              // COLOR fg [, bg]
#define GFXOP_HTAB      44              // HTAB x: the cursor to column x (0..79) of its row
#define GFXOP_VTAB      45              // VTAB y: the cursor to row y (0..24) of its column
#define GFXOP_SYMBOL    43              // SYMBOL c, a$        (12 bytes)
#define GFXOP_TILEDEF   50              // TILE DEF n, a$      (384 bytes)
#define GFXOP_TILEGET   51              // TILE GET n, a$
#define GFXOP_TILESET   52              // TILE SET x, y, n
#define GFXOP_TILEFILL  53              // TILE FILL a$        (533 bytes)
#define GFXOP_TILESCROLL 54             // TILE SCROLL sx, sy
#define GFXOP_TILELOAD  55              // TILE LOAD a$, first [, count [, PALETTE]]   (a BMP sheet cut into tiles of 16 x 24)
#define GFXOP_TILESAVE  56              // TILE SAVE a$, first, count [, cols]
#define GFXOP_SPRLOAD   72              // SPRITE LOAD a$, first [, count [, PALETTE]]  (a BMP sheet cut into sprite images)
#define GFXOP_SPRSAVE   73              // SPRITE SAVE a$, first, count [, cols]
#define GFXOP_MOUSESHOW 80              // MOUSE SHOW
#define GFXOP_MOUSEHIDE 81              // MOUSE HIDE
#define GFXOP_MOUSEPOS  82              // MOUSE POS x, y
#define GFXOP_MOUSESHAPE 83             // MOUSE SHAPE name
#define GFXOP_MOUSETMO  84              // MOUSE TIMEOUT seconds
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
#define GFXOP_SPRPATH   74              // SPRITE n, PATH a$     (a path run by the engine, see sprpath.h; "" stops it)
#define GFXOP_SPRSIZE   71              // SPRITE n, SIZE w, h     (a group: the sprites n .. n+w*h-1 are drawn as one large sprite, row by row; 1, 1 dissolves it)
// the UI subsystem (docs/ui-subsystem.md): the words after UI
#define GFXOP_UIOPEN    90              // UI OPEN
#define GFXOP_UICLOSE   91              // UI CLOSE
#define GFXOP_UICLEAR   92              // UI CLEAR
#define GFXOP_UIUPDATE  93              // UI UPDATE
#define GFXOP_UILABEL   94              // UI LABEL id, x, y, w, h, text$
#define GFXOP_UIBUTTON  95              // UI BUTTON id, x, y, w, h, text$
#define GFXOP_UICHECK   96              // UI CHECK id, x, y, w, h, text$
#define GFXOP_UIRADIO   97              // UI RADIO id, x, y, w, h, text$ [, group]
#define GFXOP_UITEXT    98              // UI TEXT id, x, y, w, h, text$          (a text field)
#define GFXOP_UILIST    99              // UI LIST id, x, y, w, h
#define GFXOP_UIPROGRESS 100            // UI PROGRESS id, x, y, w, h
#define GFXOP_UISLIDER  101             // UI SLIDER id, x, y, w, h [, min]
#define GFXOP_UIPANEL   102             // UI PANEL id, x, y, w, h, title$
#define GFXOP_UIREMOVE  103             // UI REMOVE id
#define GFXOP_UICAPTION 104             // UI CAPTION id, text$
#define GFXOP_UIVALUE   105             // UI VALUE id, n
#define GFXOP_UIRANGE   106             // UI RANGE id, min, max
#define GFXOP_UIENABLE  107             // UI ENABLE id, n
#define GFXOP_UISHOW    108             // UI SHOW id, n
#define GFXOP_UIFOCUS   109             // UI FOCUS id
#define GFXOP_UIITEM    110             // UI ITEM id, text$                      (a row of a list)
#define GFXOP_UICLEARLIST 111           // UI CLEARLIST id
#define GFXOP_UIMODAL   112             // UI MODAL id                            (0 ends it)
#define GFXOP_UIPOPUP   113             // UI POPUP id, x, y
#define GFXOP_UIGETTEXT 114             // UI GETTEXT id, a$
#define GFXOP_UIITEMTEXT 115            // UI ITEMTEXT id, row, a$
#define GFXOP_UISCROLL  116             // UI SCROLL id, x, y, w, h [, page]       (a scroll bar: vertical when it is higher than wide)
#define GFXOP_UICANVAS  118             // UI CANVAS id, x, y, w, h, cw, ch       (a zoomed grid of cw x ch cells, each a palette index; events 7 / 8 = left / right button on a cell)
#define GFXOP_UIPIXEL   119             // UI PIXEL id, x, y, c
#define GFXOP_UIIMAGE   120             // UI IMAGE id, a$                        (cw * ch bytes into the cells)
#define GFXOP_UIGETIMAGE 121            // UI GETIMAGE id, a$
#define GFXOP_UIJOY     122             // UI JOY n                               (1 = a game controller drives the focus, the default; 0 = the program reads the pads itself)
#define GFXOP_UIWINDOW  123             // UI WINDOW id, x, y, w, h, title$       (a panel the user can drag by its title bar; it rises when clicked and owns the widgets created inside it; the close box gives the cancel event)
#define GFXOP_UIMOVE    124             // UI MOVE id, x, y                       (a window takes its widgets along)
#define GFXOP_UIRAISE   125             // UI RAISE id
#define GFXOP_UIEDIT    126             // UI EDIT id, x, y, w, h, text$          (a multi-line text editor: LF separates the lines; selection, clipboard, undo)
#define GFXOP_UICARET   127             // UI CARET id, pos                       (the caret of an editor to a byte position)
#define GFXOP_UICOLOR   134             // UI COLOR id, fg [, bg]                 (the colors of a widget: palette entries 1..255, -1 the theme's; a missing bg stays as it is)
#define GFXOP_UITHEME   135             // UI THEME n                             (0 classic, 1 dark, 2 high contrast: the theme's and the syntax colors' palette entries)
#define GFXOP_UISYNTAX  136             // UI SYNTAX id, mode                     (an editor: 0 plain, 1 BASIC)
#define GFXOP_UISYNTAXCOLOR 137         // UI SYNTAXCOLOR slot, c                 (the palette entry a syntax slot 0..15 is drawn with)
#define GFXOP_UIHILITE  138             // UI HILITE id, a$                       (an editor: a slot 0..15 per byte of the text)
#define GFXOP_UIPAGE    117             // UI PAGE id, n                           (the size of the visible part)

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
#define GFXQ_SPRCOLS    47              // SPRITE("COLS", n): the width of the group n heads in sprites (1 if none)
#define GFXQ_SPRROWS    48              // SPRITE("ROWS", n)
#define GFXQ_SPRPATH    49              // SPRITE("PATH", n): -1 while a path runs for the sprite, else 0
#define GFXQ_SPREVENT   50              // SPRITE("EVENT"): takes the oldest event of a path (E n): its number, -1 if there is none
#define GFXQ_SPREVENTSPR 51             // SPRITE("EVENTSPR"): the sprite of the event taken last, -1 if none
#define GFXQ_SPREVENTS  52              // (the VM polls it for ON SPRITE) the number of path events so far; a change is an event
#define GFXQ_MOUSEX     100             // MOUSE("X"): 0..639, the sprite layer's coordinates
#define GFXQ_MOUSEY     101             // MOUSE("Y"): 0..299
#define GFXQ_MOUSEB     102             // MOUSE("B"): the buttons held down (1 left, 2 right, 4 middle)
#define GFXQ_MOUSEPRESSED 103           // MOUSE("PRESSED"): buttons pressed since the last call
#define GFXQ_MOUSERELEASED 104          // MOUSE("RELEASED")
#define GFXQ_MOUSEWHEEL 105             // MOUSE("WHEEL"): wheel steps since the last call (up is positive)
#define GFXQ_MOUSESHAPE 106             // MOUSE("SHAPE")
#define GFXQ_MOUSEVISIBLE 107           // MOUSE("VISIBLE"): -1 / 0
#define GFXQ_UIEVENT    120             // UI("EVENT"): the id of the next event (0 none); it becomes the current event
#define GFXQ_UIKIND     121             // UI("KIND"): the kind of the current event (1 click, 2 change, 3 select, 4 double, 5 enter, 6 cancel)
#define GFXQ_UIVALUE    122             // UI("VALUE"): the value of the current event
#define GFXQ_UIGET      123             // UI("GET", id): the value of a widget (check box 0 / 1, radio button, slider, progress, list row, text field length)
#define GFXQ_UICOUNT    124             // UI("COUNT", id): the rows of a list
#define GFXQ_UIFOCUS    125             // UI("FOCUS"): the id of the widget that has the focus
#define GFXQ_UICARET    129             // UI("CARET", id): the caret of an editor (a byte position, from 0)
#define GFXQ_UILINE     130             // UI("LINE", id): its line (from 0)
#define GFXQ_UICOLUMN   131             // UI("COLUMN", id): its column (from 0)
#define GFXQ_UISELECTED 132             // UI("SELECTED", id): the length of the selection (0 none)
#define GFXQ_UILINES    133             // UI("LINES", id): the number of lines
#define GFXQ_UITHEME    134             // UI("THEME"): the number of the theme set last
#define GFXQ_UISYNTAX   135             // UI("SYNTAX", id): the mode of an editor (-1 for a bad id)
#define GFXQ_UICOLOR    136             // UI("COLOR", id, which): 0 the text color, 1 the background color of a widget; -1 = the theme's
#define GFXQ_UISYNTAXCOLOR 137          // UI("SYNTAXCOLOR", slot): the palette entry of a syntax slot
#define GFXQ_UIPIXEL    128             // UI("PIXEL", id, x, y): a cell of a canvas (-1 for a bad id or place)
#define GFXQ_UIEVENTS   127             // (the VM polls it for ON UI) the number of events the UI has queued so far; a change is an event
#define GFXQ_UIOPEN     126             // UI("OPEN"): -1 / 0
#define GFXQ_MOUSEEVENT 108             // (the VM polls it for ON MOUSE) the number of presses, releases and wheel steps so far; a change is an event
#define GFXQ_EVENT      90              // (the VM polls it for ON COLLISION) a sprite with a new collision, -1 none

typedef struct _gfxcmd_t {
    const char* group;                  // "GFX", "TXT", "TILE", "SPRITE", "MOUSE", or "" for the statements that are reserved words themselves
    const char* name;                   // the word after the group (the statement itself for group ""; "" for the position form of SPRITE)
    uint16_t    op;
    const char* sig;
    const char* words;                  // word lists, one per w / W in the signature, separated by '|'; each is a comma separated list
    bool        lead;                   // SPRITE only: the first argument (a number) comes before the word:  SPRITE n, ON
} gfxcmd_t;

typedef struct _gfxquery_t {
    const char* group;                  // "GFX", "TXT", "SPRITE", "MOUSE" (selector first); "" for POINT / RGB / TILE (no selector)
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
