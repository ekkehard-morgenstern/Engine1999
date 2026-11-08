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


#include <strings.h>
#include "gfxops.h"

// sub-keywords are listed in the order of their numbers; ON / OFF: OFF = 0, ON = 1
#define DRAWMODES   "SET,CLEAR,XOR,OR,AND"
#define MOUSESHAPES "ARROW,WAIT,BUSY,TEXT,HAND,GRAB,CROSS,SIZEH,SIZEV,MOVE,NO,HELP"      // the order of CUR_* (cursors.h)

static const gfxcmd_t cmds[] = {
    { "GFX", "LAYER",    GFXOP_LAYER,    "nw",       "OFF,ON", false },
    { "GFX", "PRIORITY", GFXOP_PRIORITY, "nn",       0, false },
    { "GFX", "PALETTE",  GFXOP_PALETTE,  "nnn",      0, false },
    { "GFX", "PALNAME",  GFXOP_PALNAME,  "nns",      0, false },
    { "GFX", "PALRESET", GFXOP_PALRESET, "o",        0, false },
    { "GFX", "BACKDROP", GFXOP_BACKDROP, "n",        0, false },
    { "GFX", "TARGET",   GFXOP_TARGET,   "n",        0, false },
    { "GFX", "PEN",      GFXOP_PEN,      "n",        0, false },
    { "GFX", "MODE",     GFXOP_MODE,     "w",        DRAWMODES, false },
    { "GFX", "CLEAR",    GFXOP_CLEAR,    "o",        0, false },
    { "GFX", "PSET",     GFXOP_PSET,     "nno",      0, false },
    { "GFX", "LINE",     GFXOP_LINE,     "nnnnoW",   "B,BF", false },
    { "GFX", "CIRCLE",   GFXOP_CIRCLE,   "nnnoooo",  0, false },
    { "GFX", "PAINT",    GFXOP_PAINT,    "nnno",     0, false },
    { "GFX", "RADIUS",   GFXOP_RADIUS,   "nnnno",    0, false },
    { "GFX", "GRAB",     GFXOP_GRAB,     "nnnnv",    0, false },
    { "GFX", "BLIT",     GFXOP_BLIT,     "snnW",     "TRANS", false },
    { "GFX", "TEXT",     GFXOP_TEXT,     "nnsooo",   0, false },
    { "GFX", "DRAW",     GFXOP_DRAW,     "s",        0, false },
    { "GFX", "LOAD",     GFXOP_GFXLOAD,  "sooW",     "PALETTE", false },
    { "GFX", "SAVE",     GFXOP_GFXSAVE,  "soooo",    0, false },
    { "GFX", "PALLOAD",  GFXOP_PALLOAD,  "ns",       0, false },
    { "GFX", "PALSAVE",  GFXOP_PALSAVE,  "ns",       0, false },
    { "GFX", "VSYNC",    GFXOP_VSYNC,    "",         0, false },
    { "GFX", "RESET",    GFXOP_RESET,    "W",        "ALL", false },
    { "TXT", "FILL",     GFXOP_TXTFILL,  "nnnnooo",  0, false },
    { "TXT", "COPY",     GFXOP_TXTCOPY,  "nnnnnn",   0, false },
    { "TXT", "INVERT",   GFXOP_TXTINVERT,"nnnn",     0, false },
    { "TXT", "SAVE",     GFXOP_TXTSAVE,  "v",        0, false },
    { "TXT", "LOAD",     GFXOP_TXTLOAD,  "s",        0, false },
    { "",    "CLS",      GFXOP_CLS,      "",         0, false },
    { "",    "LOCATE",   GFXOP_LOCATE,   "nn",       0, false },
    { "",    "HTAB",     GFXOP_HTAB,     "n",        0, false },
    { "",    "VTAB",     GFXOP_VTAB,     "n",        0, false },
    { "",    "COLOR",    GFXOP_COLOR,    "no",       0, false },
    { "",    "SYMBOL",   GFXOP_SYMBOL,   "ns",       0, false },
    { "TILE", "DEF",     GFXOP_TILEDEF,  "ns",       0, false },
    { "TILE", "GET",     GFXOP_TILEGET,  "nv",       0, false },
    { "TILE", "SET",     GFXOP_TILESET,  "nnn",      0, false },
    { "TILE", "FILL",    GFXOP_TILEFILL, "s",        0, false },
    { "TILE", "LOAD",    GFXOP_TILELOAD, "snoW",     "PALETTE", false },
    { "TILE", "SAVE",    GFXOP_TILESAVE, "snno",     0, false },
    { "TILE", "SCROLL",  GFXOP_TILESCROLL,"nn",      0, false },
    { "MOUSE", "SHOW",   GFXOP_MOUSESHOW,"",         0, false },
    { "MOUSE", "HIDE",   GFXOP_MOUSEHIDE,"",         0, false },
    { "MOUSE", "POS",    GFXOP_MOUSEPOS, "nn",       0, false },
    { "MOUSE", "SHAPE",  GFXOP_MOUSESHAPE,"w",       MOUSESHAPES, false },
    { "MOUSE", "TIMEOUT",GFXOP_MOUSETMO, "n",        0, false },
    { "SPRITE", "DEF",   GFXOP_SPRDEF,   "ns",       0, false },
    { "SPRITE", "GET",   GFXOP_SPRGET,   "nv",       0, false },
    { "SPRITE", "LOAD",  GFXOP_SPRLOAD,  "snoW",     "PALETTE", false },
    { "SPRITE", "SAVE",  GFXOP_SPRSAVE,  "snno",     0, false },
    { "SPRITE", "SEQ",   GFXOP_SPRSEQ,   "ns",       0, false },
    { "SPRITE", "ON",    GFXOP_SPRON,    "n",        0, true },
    { "SPRITE", "OFF",   GFXOP_SPROFF,   "n",        0, true },
    { "SPRITE", "PRIORITY", GFXOP_SPRPRIO, "nn",     0, true },
    { "SPRITE", "IMAGE", GFXOP_SPRIMAGE, "nn",       0, true },
    { "SPRITE", "ANIM",  GFXOP_SPRANIM,  "nnnn",     0, true },
    { "SPRITE", "MOVETO",GFXOP_SPRMOVETO,"nnnn",     0, true },
    { "SPRITE", "COLLISION", GFXOP_SPRCOLL, "nwW",   "OFF,ON|TILES", true },
    { "SPRITE", "SIZE",  GFXOP_SPRSIZE,  "nnn",      0, true },
    { "SPRITE", "PATH",  GFXOP_SPRPATH,  "ns",       0, true },
    { "SPRITE", "",      GFXOP_SPRMOVE,  "nnno",     0, true },
    { "UI", "OPEN",      GFXOP_UIOPEN,   "",         0, false },
    { "UI", "CLOSE",     GFXOP_UICLOSE,  "",         0, false },
    { "UI", "CLEAR",     GFXOP_UICLEAR,  "",         0, false },
    { "UI", "UPDATE",    GFXOP_UIUPDATE, "",         0, false },
    { "UI", "LABEL",     GFXOP_UILABEL,  "nnnnns",   0, false },
    { "UI", "BUTTON",    GFXOP_UIBUTTON, "nnnnns",   0, false },
    { "UI", "CHECK",     GFXOP_UICHECK,  "nnnnns",   0, false },
    { "UI", "RADIO",     GFXOP_UIRADIO,  "nnnnnso",  0, false },
    { "UI", "TEXT",      GFXOP_UITEXT,   "nnnnns",   0, false },
    { "UI", "LIST",      GFXOP_UILIST,   "nnnnn",    0, false },
    { "UI", "PROGRESS",  GFXOP_UIPROGRESS, "nnnnn",  0, false },
    { "UI", "SLIDER",    GFXOP_UISLIDER, "nnnnno",   0, false },
    { "UI", "PANEL",     GFXOP_UIPANEL,  "nnnnns",   0, false },
    { "UI", "REMOVE",    GFXOP_UIREMOVE, "n",        0, false },
    { "UI", "CAPTION",   GFXOP_UICAPTION,"ns",       0, false },
    { "UI", "VALUE",     GFXOP_UIVALUE,  "nn",       0, false },
    { "UI", "RANGE",     GFXOP_UIRANGE,  "nnn",      0, false },
    { "UI", "ENABLE",    GFXOP_UIENABLE, "nn",       0, false },
    { "UI", "SHOW",      GFXOP_UISHOW,   "nn",       0, false },
    { "UI", "FOCUS",     GFXOP_UIFOCUS,  "n",        0, false },
    { "UI", "ITEM",      GFXOP_UIITEM,   "ns",       0, false },
    { "UI", "CLEARLIST", GFXOP_UICLEARLIST, "n",     0, false },
    { "UI", "MODAL",     GFXOP_UIMODAL,  "n",        0, false },
    { "UI", "POPUP",     GFXOP_UIPOPUP,  "nnn",      0, false },
    { "UI", "GETTEXT",   GFXOP_UIGETTEXT,"nv",       0, false },
    { "UI", "ITEMTEXT",  GFXOP_UIITEMTEXT, "nnv",    0, false },
    { "UI", "SCROLL",    GFXOP_UISCROLL, "nnnnno",   0, false },
    { "UI", "PAGE",      GFXOP_UIPAGE,   "nn",       0, false },
    { "UI", "JOY",       GFXOP_UIJOY,    "n",        0, false },
    { "UI", "WINDOW",    GFXOP_UIWINDOW, "nnnnns",   0, false },
    { "UI", "EDIT",      GFXOP_UIEDIT,   "nnnnns",   0, false },
    { "UI", "CARET",     GFXOP_UICARET,  "nn",       0, false },
    { "UI", "COLOR",     GFXOP_UICOLOR,  "nno",      0, false },
    { "UI", "THEME",     GFXOP_UITHEME,  "n",        0, false },
    { "UI", "SYNTAX",    GFXOP_UISYNTAX, "nn",       0, false },
    { "UI", "SYNTAXCOLOR", GFXOP_UISYNTAXCOLOR, "nn", 0, false },
    { "UI", "HILITE",    GFXOP_UIHILITE, "ns",       0, false },
    { "UI", "MOVE",      GFXOP_UIMOVE,   "nnn",      0, false },
    { "UI", "RAISE",     GFXOP_UIRAISE,  "n",        0, false },
    { "UI", "CANVAS",    GFXOP_UICANVAS, "nnnnnnn",  0, false },
    { "UI", "PIXEL",     GFXOP_UIPIXEL,  "nnnn",     0, false },
    { "UI", "IMAGE",     GFXOP_UIIMAGE,  "ns",       0, false },
    { "UI", "GETIMAGE",  GFXOP_UIGETIMAGE, "nv",     0, false },
    { "W3D", "RESET",    GFXOP_W3DRESET, "",         0, false },
    { "W3D", "TEXTURE",  GFXOP_W3DTEXTURE, "ns",     0, false },
    { "W3D", "SECTOR",   GFXOP_W3DSECTOR, "nnnooo",  0, false },
    { "W3D", "WALL",     GFXOP_W3DWALL,  "nnnnnno",  0, false },
    { "W3D", "WALLTEX",  GFXOP_W3DWALLTEX, "nnnnn",  0, false },
    { "W3D", "WALLOFF",  GFXOP_W3DWALLOFF, "nnnn",   0, false },
    { "W3D", "WALLFLAG", GFXOP_W3DWALLFLAG, "nnn",   0, false },
    { "W3D", "HEIGHT",   GFXOP_W3DHEIGHT, "nnn",     0, false },
    { "W3D", "LIGHT",    GFXOP_W3DLIGHT, "nn",       0, false },
    { "W3D", "TAG",      GFXOP_W3DTAG,   "nn",       0, false },
    { "W3D", "SKY",      GFXOP_W3DSKY,   "n",        0, false },
    { "W3D", "CAMERA",   GFXOP_W3DCAMERA, "nnnoo",   0, false },
    { "W3D", "TURN",     GFXOP_W3DTURN,  "no",       0, false },
    { "W3D", "PLAYER",   GFXOP_W3DPLAYER, "nnnn",    0, false },
    { "W3D", "VIEW",     GFXOP_W3DVIEW,  "nnnnoo",   0, false },
    { "W3D", "FOG",      GFXOP_W3DFOG,   "n",        0, false },
    { "W3D", "COLORMAP", GFXOP_W3DCOLORMAP, "",      0, false },
    { "W3D", "RENDER",   GFXOP_W3DRENDER, "",        0, false },
    { "W3D", "MOVE",     GFXOP_W3DMOVE,  "nn",       0, false },
    { "W3D", "THING",    GFXOP_W3DTHING, "nnnnnn",   0, false },
    { "W3D", "THINGZ",   GFXOP_W3DTHINGZ, "nn",      0, false },
    { "W3D", "THINGTEX", GFXOP_W3DTHINGTEX, "nn",    0, false },
    { "W3D", "THINGOFF", GFXOP_W3DTHINGOFF, "n",     0, false },
    { "W3D", "THINGMOVE", GFXOP_W3DTHINGMOVE, "nnn", 0, false },
    { "W3D", "THINGLIGHT", GFXOP_W3DTHINGLIGHT, "nn", 0, false },
    { "W3D", "LIFT",     GFXOP_W3DLIFT,  "n",        0, false },
};

static const gfxquery_t queries[] = {
    { "",       "RGB",      GFXQ_RGB,       3, 4 },
    { "",       "POINT",    GFXQ_POINT,     2, 2 },
    { "",       "TILE",     GFXQ_TILECELL,  2, 2 },
    { "GFX",    "FRAME",    GFXQ_FRAME,     0, 0 },
    { "GFX",    "LAYER",    GFXQ_LAYER,     1, 1 },
    { "GFX",    "PRIORITY", GFXQ_PRIORITY,  1, 1 },
    { "GFX",    "PALETTE",  GFXQ_PALETTE,   2, 2 },
    { "GFX",    "TARGET",   GFXQ_TARGET,    0, 0 },
    { "TXT",    "X",        GFXQ_TXTX,      0, 0 },
    { "TXT",    "Y",        GFXQ_TXTY,      0, 0 },
    { "TXT",    "FG",       GFXQ_TXTFG,     0, 0 },
    { "TXT",    "BG",       GFXQ_TXTBG,     0, 0 },
    { "TXT",    "CHAR",     GFXQ_TXTCHAR,   2, 2 },
    { "TXT",    "FGAT",     GFXQ_TXTFGAT,   2, 2 },
    { "TXT",    "BGAT",     GFXQ_TXTBGAT,   2, 2 },
    { "TXT",    "WIDTH",    GFXQ_TXTWIDTH,  0, 0 },
    { "TXT",    "HEIGHT",   GFXQ_TXTHEIGHT, 0, 0 },
    { "TXT",    "GLYPH",    GFXQ_TXTGLYPH,  2, 2 },
    { "TXT",    "BUILTIN",  GFXQ_TXTBUILTIN,2, 2 },
    { "MOUSE",  "X",        GFXQ_MOUSEX,    0, 0 },
    { "MOUSE",  "Y",        GFXQ_MOUSEY,    0, 0 },
    { "MOUSE",  "B",        GFXQ_MOUSEB,    0, 0 },
    { "MOUSE",  "PRESSED",  GFXQ_MOUSEPRESSED, 0, 0 },
    { "MOUSE",  "RELEASED", GFXQ_MOUSERELEASED, 0, 0 },
    { "MOUSE",  "WHEEL",    GFXQ_MOUSEWHEEL,0, 0 },
    { "MOUSE",  "SHAPE",    GFXQ_MOUSESHAPE,0, 0 },
    { "MOUSE",  "VISIBLE",  GFXQ_MOUSEVISIBLE, 0, 0 },
    { "MOUSE",  "EVENTS",   GFXQ_MOUSEEVENT, 0, 0 },
    { "SPRITE", "X",        GFXQ_SPRX,      1, 1 },
    { "SPRITE", "Y",        GFXQ_SPRY,      1, 1 },
    { "SPRITE", "IMAGE",    GFXQ_SPRIMAGE,  1, 1 },
    { "SPRITE", "ON",       GFXQ_SPRON,     1, 1 },
    { "SPRITE", "HITS",     GFXQ_SPRHITS,   1, 1 },
    { "SPRITE", "COLLIDE",  GFXQ_SPRCOLLIDE,1, 1 },
    { "SPRITE", "MOVING",   GFXQ_SPRMOVING, 1, 1 },
    { "SPRITE", "COLS",     GFXQ_SPRCOLS,   1, 1 },
    { "SPRITE", "ROWS",     GFXQ_SPRROWS,   1, 1 },
    { "SPRITE", "PATH",     GFXQ_SPRPATH,   1, 1 },
    { "SPRITE", "EVENT",    GFXQ_SPREVENT,  0, 0 },
    { "SPRITE", "EVENTSPR", GFXQ_SPREVENTSPR, 0, 0 },
    { "SPRITE", "EVENTS",   GFXQ_SPREVENTS, 0, 0 },
    { "UI",     "EVENT",    GFXQ_UIEVENT,   0, 0 },
    { "UI",     "KIND",     GFXQ_UIKIND,    0, 0 },
    { "UI",     "VALUE",    GFXQ_UIVALUE,   0, 0 },
    { "UI",     "GET",      GFXQ_UIGET,     1, 1 },
    { "UI",     "COUNT",    GFXQ_UICOUNT,   1, 1 },
    { "UI",     "FOCUS",    GFXQ_UIFOCUS,   0, 0 },
    { "UI",     "OPEN",     GFXQ_UIOPEN,    0, 0 },
    { "UI",     "EVENTS",   GFXQ_UIEVENTS,  0, 0 },
    { "UI",     "PIXEL",    GFXQ_UIPIXEL,   3, 3 },
    { "UI",     "CARET",    GFXQ_UICARET,   1, 1 },
    { "UI",     "LINE",     GFXQ_UILINE,    1, 1 },
    { "UI",     "COLUMN",   GFXQ_UICOLUMN,  1, 1 },
    { "UI",     "SELECTED", GFXQ_UISELECTED, 1, 1 },
    { "UI",     "LINES",    GFXQ_UILINES,   1, 1 },
    { "UI",     "THEME",    GFXQ_UITHEME,   0, 0 },
    { "UI",     "SYNTAX",   GFXQ_UISYNTAX,  1, 1 },
    { "UI",     "COLOR",    GFXQ_UICOLOR,   2, 2 },
    { "UI",     "SYNTAXCOLOR", GFXQ_UISYNTAXCOLOR, 1, 1 },
    { "W3D",    "X",         GFXQ_W3DX, 0, 0 },
    { "W3D",    "Y",         GFXQ_W3DY, 0, 0 },
    { "W3D",    "Z",         GFXQ_W3DZ, 0, 0 },
    { "W3D",    "ANGLE",     GFXQ_W3DANGLE, 0, 0 },
    { "W3D",    "PITCH",     GFXQ_W3DPITCH, 0, 0 },
    { "W3D",    "SECTOR",    GFXQ_W3DSECTOR, 0, 0 },
    { "W3D",    "SECTORAT",  GFXQ_W3DSECTORAT, 2, 2 },
    { "W3D",    "FLOORAT",   GFXQ_W3DFLOORAT, 2, 2 },
    { "W3D",    "FLOOR",     GFXQ_W3DFLOOR, 1, 1 },
    { "W3D",    "CEIL",      GFXQ_W3DCEIL, 1, 1 },
    { "W3D",    "LIGHT",     GFXQ_W3DLIGHT, 1, 1 },
    { "W3D",    "TAG",       GFXQ_W3DTAG, 1, 1 },
    { "W3D",    "WALLS",     GFXQ_W3DWALLS, 1, 1 },
    { "W3D",    "SECTORS",   GFXQ_W3DSECTORS, 0, 0 },
    { "W3D",    "TX",        GFXQ_W3DTX, 1, 1 },
    { "W3D",    "TY",        GFXQ_W3DTY, 1, 1 },
    { "W3D",    "TSECTOR",   GFXQ_W3DTSECTOR, 1, 1 },
    { "W3D",    "TON",       GFXQ_W3DTON, 1, 1 },
    { "W3D",    "DIST",      GFXQ_W3DDIST, 1, 1 },
    { "W3D",    "BEARING",   GFXQ_W3DBEARING, 1, 1 },
    { "W3D",    "HIT",       GFXQ_W3DHIT, 0, 0 },
    { "W3D",    "SHOOT",     GFXQ_W3DSHOOT, 1, 1 },
    { "W3D",    "SHOTDIST",  GFXQ_W3DSHOTDIST, 0, 0 },
    { "W3D",    "SHOTX",     GFXQ_W3DSHOTX, 0, 0 },
    { "W3D",    "SHOTY",     GFXQ_W3DSHOTY, 0, 0 },
    { "W3D",    "USE",       GFXQ_W3DUSE, 1, 1 },
    { "W3D",    "SEES",      GFXQ_W3DSEES, 1, 1 },
    { "W3D",    "TEXW",      GFXQ_W3DTEXW, 1, 1 },
    { "W3D",    "TEXH",      GFXQ_W3DTEXH, 1, 1 },
    { "W3D",    "TZ",        GFXQ_W3DTZ, 1, 1 },
    { "W3D",    "FLOORZ",    GFXQ_W3DFLOORZ, 0, 0 },
    { "W3D",    "SHOTKIND",  GFXQ_W3DSHOTKIND, 0, 0 },
    { "W3D",    "LIFT",      GFXQ_W3DLIFT, 0, 0 },
};

const gfxcmd_t* gfxops_find( const char* group, const char* name ) {
    for ( size_t i=0; i < sizeof(cmds) / sizeof(cmds[0]); ++i ) {
        if ( strcasecmp( cmds[i].group, group ) == 0 && strcasecmp( cmds[i].name, name ) == 0 ) {
            return &cmds[i];
        }
    }
    return 0;
}

const gfxcmd_t* gfxops_byop( uint16_t op ) {
    for ( size_t i=0; i < sizeof(cmds) / sizeof(cmds[0]); ++i ) {
        if ( cmds[i].op == op ) {
            return &cmds[i];
        }
    }
    return 0;
}

const gfxquery_t* gfxops_findquery( const char* group, const char* sel ) {
    for ( size_t i=0; i < sizeof(queries) / sizeof(queries[0]); ++i ) {
        if ( strcasecmp( queries[i].group, group ) == 0 && strcasecmp( queries[i].sel, sel ) == 0 ) {
            return &queries[i];
        }
    }
    return 0;
}

const gfxquery_t* gfxops_queryby( uint16_t q ) {
    for ( size_t i=0; i < sizeof(queries) / sizeof(queries[0]); ++i ) {
        if ( queries[i].q == q ) {
            return &queries[i];
        }
    }
    return 0;
}

int gfxops_word( const char* words, int slot, const char* word ) {
    if ( words == 0 ) {
        return -1;
    }
    const char* p = words;
    for ( int s = 0; s < slot; ++s ) {
        p = strchr( p, '|' );
        if ( p == 0 ) {
            return -1;
        }
        ++p;
    }
    int index = 0;
    size_t wl = strlen( word );
    while ( *p != '\0' && *p != '|' ) {
        size_t n = strcspn( p, ",|" );
        if ( n == wl && strncasecmp( p, word, n ) == 0 ) {
            return index;
        }
        p += n;
        if ( *p == ',' ) {
            ++p;
        }
        ++index;
    }
    return -1;
}

int gfxops_sigargs( const char* sig ) {
    int n = 0;
    for ( ; *sig; ++sig ) {
        if ( *sig != 'v' ) {
            ++n;
        }
    }
    return n;
}

const char* gfxops_opname( uint16_t op ) {
    static char buf[48];
    const gfxcmd_t* c = gfxops_byop( op );
    if ( c == 0 ) {
        snprintf( buf, sizeof(buf), "op%u", (unsigned) op );
    } else if ( c->group[0] == '\0' ) {
        snprintf( buf, sizeof(buf), "%s", c->name );
    } else {
        snprintf( buf, sizeof(buf), "%s %s", c->group, c->name[0] ? c->name : "MOVE" );
    }
    return buf;
}
