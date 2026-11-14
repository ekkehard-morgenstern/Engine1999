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
    s   a string expression               S   the same, optional (omitted: the empty string)
    v   a string variable that receives the string result of the operation (always last)
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
#define GFXOP_UITAB     139             // UI TAB id, x, y, w, h, tabs$            (a tab strip: the tab names separated by |; the value is the selected tab 0.., a change event when it changes; Left / Right on the focus; the program shows and hides its pages)
#define GFXOP_UIPAGE    117             // UI PAGE id, n                           (the size of the visible part)

// the 3D sector renderer (w3d.h, docs/3d-subsystem.md): the words after W3D
#define GFXOP_W3DRESET  140             // W3D RESET
#define GFXOP_W3DTEXTURE 141            // W3D TEXTURE n, a$                      (a GFX GRAB block: width, height, pixels)
#define GFXOP_W3DSECTOR 142             // W3D SECTOR s, floor, ceil [, floortex [, ceiltex [, light]]]
#define GFXOP_W3DWALL   143             // W3D WALL s, x1, y1, x2, y2, tex [, next]
#define GFXOP_W3DWALLTEX 144            // W3D WALLTEX s, i, mid, upper, lower    (-1 = unchanged)
#define GFXOP_W3DWALLOFF 145            // W3D WALLOFF s, i, xoff, yoff
#define GFXOP_W3DWALLFLAG 146           // W3D WALLFLAG s, i, flags
#define GFXOP_W3DHEIGHT 147             // W3D HEIGHT s, floor, ceil
#define GFXOP_W3DLIGHT  148             // W3D LIGHT s, light
#define GFXOP_W3DTAG    149             // W3D TAG s, value
#define GFXOP_W3DSKY    150             // W3D SKY tex
#define GFXOP_W3DCAMERA 151             // W3D CAMERA x, y, angle [, pitch [, lift]]
#define GFXOP_W3DTURN   152             // W3D TURN dangle [, dpitch]
#define GFXOP_W3DPLAYER 153             // W3D PLAYER radius, height, eye, step
#define GFXOP_W3DVIEW   154             // W3D VIEW x, y, w, h [, scale [, fov]]
#define GFXOP_W3DFOG    155             // W3D FOG distance
#define GFXOP_W3DCOLORMAP 156           // W3D COLORMAP
#define GFXOP_W3DRENDER 157             // W3D RENDER
#define GFXOP_W3DMOVE   158             // W3D MOVE forward, right
#define GFXOP_W3DTHING  159             // W3D THING n, x, y, tex, w, h
#define GFXOP_W3DTHINGZ 160             // W3D THINGZ n, z
#define GFXOP_W3DTHINGTEX 161           // W3D THINGTEX n, tex
#define GFXOP_W3DTHINGOFF 162           // W3D THINGOFF n
#define GFXOP_W3DTHINGMOVE 163          // W3D THINGMOVE n, dx, dy
#define GFXOP_W3DTHINGLIGHT 164         // W3D THINGLIGHT n, light
#define GFXOP_UIGRID    167             // UI GRID id, x, y, cols, rows               (a character grid: cols x rows cells of the 8 x 12 font, each with its own colors; keys -> event 9, clicks -> event 10)
#define GFXOP_UIGRIDPUT 168             // UI GRIDPUT id, col, row, a$ [, fg [, bg]]  (text into the grid; colors are palette entries 1..255, 0 the theme's)
#define GFXOP_UIGRIDCLEAR 169           // UI GRIDCLEAR id [, fg [, bg]]              (spaces everywhere)
#define GFXOP_SCREENSHOT 166            // SCREENSHOT [file$]                       (the composed picture as a BMP; no name: shot-YYYYMMDD-HHMMSS-n.bmp in the current directory)
#define GFXOP_W3DLIFT   165             // W3D LIFT h                             (the height of the feet above the floor: a jump; the eye rises with it and steps up to step + h can be taken)
#define GFXOP_W3DSLOPE  170             // W3D SLOPE s, kind, gx, gy [, ox, oy]   (kind 0 floor, 1 ceiling, 2 both; rise per unit in x and y around the origin, default the middle of the sector's walls)
#define GFXOP_W3DFAR    171             // W3D FAR distance                       (draw distance, 0 = unlimited)
#define GFXOP_W3DTICK   172             // W3D TICK seconds                       (advances the world's clock: movers, scrolling, animated textures, light effects)
#define GFXOP_W3DSECMOVE 173            // W3D SECMOVE s, speed, floor [, ceil]   (the sector's floor / ceiling move to these heights at this speed per second; W3D("MOVING", s))
#define GFXOP_W3DANIM   174             // W3D ANIM n, first, count, fps          (texture n shows the frames first .. first + count - 1 in turn; count 0 = off)
#define GFXOP_W3DSCROLL 175             // W3D SCROLL s, kind, dx, dy             (kind 0 floor, 1 ceiling, 2 walls: texture offset speeds per second)
#define GFXOP_W3DLIGHTFX 176            // W3D LIGHTFX s, mode [, amount [, rate]]  (mode 0 none, 1 flicker, 2 pulse, 3 strobe)
#define GFXOP_W3DLAMP   177             // W3D LAMP n, x, y, z, radius, amount    (a point light: adds up to amount at the center, nothing at the radius; radius 0 = off)
#define GFXOP_W3DTHINGTRANS 178         // W3D THINGTRANS n, flag                 (a translucent thing)
#define GFXOP_W3DCLEAR  179             // W3D CLEAR                              (deletes the sectors, walls and things, keeps the pictures and the settings: a new map can be loaded)
#define GFXOP_W3DWORLD  196             // W3D WORLD                              (builds the world from the current JSON document: w3dworld.h, docs/world-model.md; replaces the sectors and things)
#define GFXOP_W3DGHOST  180             // W3D GHOST s, flag                      (a ghost sector is only seen through portals: it is never the sector of a position, a thing or the camera)
#define GFXOP_JSONCLEAR 181             // JSON CLEAR                             (the JSON statements work on one current document: json.h, docs/json-subsystem.md)
#define GFXOP_JSONLOAD  182             // JSON LOAD file$                        (".json" is added if the name has no extension)
#define GFXOP_JSONSAVE  183             // JSON SAVE file$ [, pretty]             (pretty: 0 = compact, default 1)
#define GFXOP_JSONPARSE 184             // JSON PARSE text$
#define GFXOP_JSONTEXT  185             // JSON TEXT path$, v$                    (the element as JSON text, "" = everything; at most 65535 bytes)
#define GFXOP_JSONGET   186             // JSON GET path$, v$                     (a value as text; "" if there is none)
#define GFXOP_JSONTYPE  187             // JSON TYPE path$, v$                    ("null" "boolean" "number" "string" "array" "object", "" if there is none)
#define GFXOP_JSONCOUNT 188             // JSON COUNT path$, v$                   (the number of elements of an array / members of an object as text, 0 otherwise)
#define GFXOP_JSONKEY   189             // JSON KEY path$, i, v$                  (the name of member i (from 0) of an object, "" if there is none)
#define GFXOP_JSONSET   190             // JSON SET path$, number
#define GFXOP_JSONSETS  191             // JSON SETS path$, text$                 (two strings: the VM joins them with one NUL byte for the host)
#define GFXOP_JSONSETB  192             // JSON SETB path$, flag                  (true if flag is not 0)
#define GFXOP_JSONNULL  193             // JSON NULL path$
#define GFXOP_JSONNEW   194             // JSON NEW path$, OBJECT|ARRAY           (an empty one)
#define GFXOP_JSONDELETE 195            // JSON DELETE path$
#define GFXOP_JSONFIRST GFXOP_JSONCLEAR
#define GFXOP_JSONLAST  GFXOP_JSONDELETE
#define GFXOP_IS_JSON(op) ( (op) >= GFXOP_JSONFIRST && (op) <= GFXOP_JSONLAST )
#define GFXOP_W3DFIRST  GFXOP_W3DRESET
#define GFXOP_W3DLAST   GFXOP_W3DLIFT
#define GFXOP_IS_W3D(op) ( ( (op) >= GFXOP_W3DFIRST && (op) <= GFXOP_W3DLAST ) || ( (op) >= GFXOP_W3DFIRST2 && (op) <= GFXOP_W3DLAST2 ) || (op) == GFXOP_W3DWORLD )
#define GFXOP_W3DFIRST2 GFXOP_W3DSLOPE
#define GFXOP_W3DLAST2  GFXOP_W3DGHOST

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
#define GFXQ_TXTGLYPH   29              // TXT("GLYPH", code, row): row 0..11 of the character's picture now (bit 7 = left)
#define GFXQ_TXTBUILTIN 31              // TXT("BUILTIN", code, row): the same in the built-in font (what SYMBOL has not changed)
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

// the 3D sector renderer: W3D("name", ...)
#define GFXQ_W3DX       200             // W3D("X"): the player's position
#define GFXQ_W3DY       201
#define GFXQ_W3DZ       202             // W3D("Z"): the eye height (absolute)
#define GFXQ_W3DANGLE   203             // W3D("ANGLE"): 0..360
#define GFXQ_W3DPITCH   204
#define GFXQ_W3DSECTOR  205             // W3D("SECTOR"): the player's sector, -1 outside
#define GFXQ_W3DSECTORAT 206            // W3D("SECTORAT", x, y)
#define GFXQ_W3DFLOORAT 207             // W3D("FLOORAT", x, y)
#define GFXQ_W3DFLOOR   208             // W3D("FLOOR", s)
#define GFXQ_W3DCEIL    209             // W3D("CEIL", s)
#define GFXQ_W3DLIGHT   210             // W3D("LIGHT", s)
#define GFXQ_W3DTAG     211             // W3D("TAG", s)
#define GFXQ_W3DWALLS   212             // W3D("WALLS", s): the number of walls (= the number the next wall gets)
#define GFXQ_W3DSECTORS 213             // W3D("SECTORS"): the highest sector number + 1
#define GFXQ_W3DTX      214             // W3D("TX", n): position of a thing
#define GFXQ_W3DTY      215
#define GFXQ_W3DTSECTOR 216             // W3D("TSECTOR", n)
#define GFXQ_W3DTON     217             // W3D("TON", n): -1 if the thing exists
#define GFXQ_W3DDIST    218             // W3D("DIST", n): distance from the player to a thing
#define GFXQ_W3DBEARING 219             // W3D("BEARING", n): angle of a thing from the view direction (positive = to the right), -180..180
#define GFXQ_W3DHIT     220             // W3D("HIT"): flags of the last W3D MOVE / THINGMOVE
#define GFXQ_W3DSHOOT   221             // W3D("SHOOT", range): the thing hit by a shot along the view direction (-1 none); sets SHOTDIST etc.
#define GFXQ_W3DSHOTDIST 222
#define GFXQ_W3DSHOTX   223
#define GFXQ_W3DSHOTY   224
#define GFXQ_W3DUSE     225             // W3D("USE", reach): the sector behind the portal wall in front of the player, -1 none
#define GFXQ_W3DSEES    226             // W3D("SEES", n): -1 if the thing has a line of sight to the player
#define GFXQ_W3DTEXW    227             // W3D("TEXW", n)
#define GFXQ_W3DTEXH    228
#define GFXQ_W3DTZ      229             // W3D("TZ", n): height of a thing above the floor
#define GFXQ_W3DFLOORZ  230             // W3D("FLOORZ"): the floor under the player
#define GFXQ_W3DSHOTKIND 231            // W3D("SHOTKIND"): 0 nothing, 1 a wall, 2 a thing
#define GFXQ_W3DLIFT    232             // W3D("LIFT")
#define GFXQ_W3DCEILAT  233             // W3D("CEILAT", x, y)
#define GFXQ_W3DVISITED 234             // W3D("VISITED"): the number of sectors the last frame reached
#define GFXQ_W3DSLOPE   235             // W3D("SLOPE", s, kind, axis): kind 0 floor 1 ceiling, axis 0 = x 1 = y
#define GFXQ_W3DSECTORZ 236             // W3D("SECTORZ", x, y, z): the sector at a point and height (sectors can lie over each other)
#define GFXQ_W3DMOVING  237             // W3D("MOVING", s): -1 while the sector's heights are moving
#define GFXQ_W3DTIME    238             // W3D("TIME"): the world's clock in seconds
#define GFXQ_W3DLIGHTNOW 239            // W3D("LIGHTNOW", s): the light of the sector in the last frame (effects applied)
#define GFXQ_W3DWALLINFO 240            // W3D("WALLINFO", s, i, k): k 0..3 x1 y1 x2 y2, 4 tex, 5 upper, 6 lower, 7 next, 8 xoff, 9 yoff, 10 flags
#define GFXQ_W3DSECINFO 241             // W3D("SECINFO", s, k): k 0 floor, 1 ceiling, 2 floortex, 3 ceiltex, 4 light, 5 tag, 6..9 slopes (floor x, y, ceiling x, y), 10..11 slope origin, 12 -1 if the sector exists (0 if not: no error)
#define GFXQ_W3DTHINGINFO 242           // W3D("THINGINFO", n, k): k 0 x, 1 y, 2 tex, 3 w, 4 h, 5 z, 6 light, 7 translucent, 8 sector
#define GFXQ_W3DFIRST   GFXQ_W3DX
#define GFXQ_W3DLAST    GFXQ_W3DTHINGINFO


// the 3D mesh renderer (m3d.h, docs/3d-mesh-subsystem.md): the words after M3D
#define GFXOP_M3DRESET  300             // M3D RESET                              (everything: objects, things, pictures, settings)
#define GFXOP_M3DTEXTURE 301            // M3D TEXTURE n, a$                      (a GFX GRAB block)
#define GFXOP_M3DCLEAR  302             // M3D CLEAR                              (deletes the objects, things and lamps, keeps the pictures and the settings)
#define GFXOP_M3DVIEW   303             // M3D VIEW x, y, w, h [, scale [, fov]]
#define GFXOP_M3DFOG    304             // M3D FOG distance
#define GFXOP_M3DFAR    305             // M3D FAR distance
#define GFXOP_M3DSKY    306             // M3D SKY tex [, repeat [, shift]]
#define GFXOP_M3DCOLORMAP 307           // M3D COLORMAP
#define GFXOP_M3DRENDER 308             // M3D RENDER
#define GFXOP_M3DOBJECT 309             // M3D OBJECT o [, flags [, light]]       (creates or empties object o)
#define GFXOP_M3DVERTEX 310             // M3D VERTEX o, x, y, z [, u [, v]]      (vertices are numbered from 0 in the order they are added)
#define GFXOP_M3DFACE   311             // M3D FACE o, a, b, c, tex               (a triangle, counterclockwise seen from the front)
#define GFXOP_M3DQUAD   312             // M3D QUAD o, a, b, c, d, tex            (two triangles)
#define GFXOP_M3DBOX    313             // M3D BOX o, x1, y1, z1, x2, y2, z2, tex (the six faces of a box)
#define GFXOP_M3DOBJPOS 314             // M3D OBJPOS o, x, y, z [, yaw]
#define GFXOP_M3DOBJSHOW 315            // M3D OBJSHOW o, flag
#define GFXOP_M3DOBJLIGHT 316           // M3D OBJLIGHT o, light
#define GFXOP_M3DOBJFLAGS 317           // M3D OBJFLAGS o, flags
#define GFXOP_M3DTHING  319            // M3D THING n, x, y, z, tex, w, h        (a billboard; z = its bottom)
#define GFXOP_M3DTHINGPOS 320           // M3D THINGPOS n, x, y, z
#define GFXOP_M3DTHINGTEX 321           // M3D THINGTEX n, tex
#define GFXOP_M3DTHINGOFF 322           // M3D THINGOFF n
#define GFXOP_M3DTHINGLIGHT 323         // M3D THINGLIGHT n, light
#define GFXOP_M3DTHINGTRANS 324         // M3D THINGTRANS n, flag
#define GFXOP_M3DCAMERA 325             // M3D CAMERA x, y, z, yaw [, pitch]      (z = the feet)
#define GFXOP_M3DTURN   326             // M3D TURN dyaw [, dpitch]
#define GFXOP_M3DPLAYER 327             // M3D PLAYER radius, height, eye, step
#define GFXOP_M3DMOVE   328             // M3D MOVE forward, right
#define GFXOP_M3DLIFT   329             // M3D LIFT h
#define GFXOP_M3DLAMP   330             // M3D LAMP n, x, y, z, radius, amount
#define GFXOP_M3DTHINGMOVE 331          // M3D THINGMOVE n, dx, dy               (walks a thing like M3D MOVE: pushed out of walls, up steps, following the floor; M3D("HIT"))
#define GFXOP_M3DZONE   332             // M3D ZONE id, x1, y1, x2, y2, z1, z2 [, tag]  (a named box of space for the game: M3D("ZONE", x, y, z) finds the first one containing a point)
#define GFXOP_M3DWORLD  333             // M3D WORLD                              (builds the world from the current JSON document: m3dworld.h, docs/3d-mesh-subsystem.md)
#define GFXOP_M3DSOFTWARE 334           // M3D SOFTWARE                           (the software renderer draws; always possible)
#define GFXOP_M3DHARDWARE 335           // M3D HARDWARE                           (the OpenGL renderer draws; an error when it is not available, see M3D("HWAVAIL"))
#define GFXOP_M3DFIRST  GFXOP_M3DRESET
#define GFXOP_M3DLAST   GFXOP_M3DHARDWARE
#define GFXOP_IS_M3D(op) ( (op) >= GFXOP_M3DFIRST && (op) <= GFXOP_M3DLAST )
#define GFXQ_M3DX       350             // M3D("X"), ("Y"), ("Z" = the feet), ("EYE"), ("YAW"), ("PITCH"), ("LIFT"), ("HIT"), ("FLOORZ")
#define GFXQ_M3DY       351
#define GFXQ_M3DZ       352
#define GFXQ_M3DEYE     353
#define GFXQ_M3DYAW     354
#define GFXQ_M3DPITCH   355
#define GFXQ_M3DLIFT    356
#define GFXQ_M3DHIT     357
#define GFXQ_M3DFLOORZ  358
#define GFXQ_M3DFLOORAT 359             // M3D("FLOORAT", x, y, z): the highest surface at or below z (-1e9 none)
#define GFXQ_M3DCEILAT  360             // M3D("CEILAT", x, y, z): the lowest surface above z (1e9 none)
#define GFXQ_M3DTON     361             // M3D("TON", n)
#define GFXQ_M3DTX      362
#define GFXQ_M3DTY      363
#define GFXQ_M3DTZ      364
#define GFXQ_M3DDIST    365             // M3D("DIST", n): distance in the plane to a thing
#define GFXQ_M3DBEARING 366
#define GFXQ_M3DSHOOT   367             // M3D("SHOOT", range): the thing hit along the view direction (-1 none); sets the SHOT values
#define GFXQ_M3DSHOTKIND 368            // 0 nothing, 1 an object, 2 a thing
#define GFXQ_M3DSHOTOBJ 369
#define GFXQ_M3DSHOTDIST 370
#define GFXQ_M3DSHOTX   371
#define GFXQ_M3DSHOTY   372
#define GFXQ_M3DSHOTZ   373
#define GFXQ_M3DUSE     374             // M3D("USE", reach): the object in front of the player within reach, -1 none
#define GFXQ_M3DSEES    375             // M3D("SEES", n): -1 if the thing has a line of sight to the eye
#define GFXQ_M3DTEXW    376
#define GFXQ_M3DTEXH    377
#define GFXQ_M3DOBJON   378             // M3D("OBJON", o): -1 if the object exists
#define GFXQ_M3DOBJX    379             // M3D("OBJX", o), ("OBJY", o), ("OBJZ", o), ("OBJYAW", o)
#define GFXQ_M3DOBJY    380
#define GFXQ_M3DOBJZ    381
#define GFXQ_M3DOBJYAW  382
#define GFXQ_M3DVERTS   383             // M3D("VERTS", o), ("TRIS", o): counts
#define GFXQ_M3DTRIS    384
#define GFXQ_M3DDRAWN   385             // M3D("DRAWN"): the number of triangles the last frame rasterized
#define GFXQ_M3DZONE    386             // M3D("ZONE", x, y, z): the id of the first zone containing the point, -1 none
#define GFXQ_M3DZONETAG 387             // M3D("ZONETAG", x, y, z): its tag
#define GFXQ_M3DRENDERER 388            // M3D("RENDERER"): 0 the software renderer is in use, 1 the hardware (OpenGL) renderer
#define GFXQ_M3DHWAVAIL 389             // M3D("HWAVAIL"): 1 when M3D HARDWARE can succeed (OpenGL 2 was found and the renderer is built in), else 0
#define GFXQ_M3DFIRST   GFXQ_M3DX
#define GFXQ_M3DLAST    GFXQ_M3DHWAVAIL

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
