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
    { "GFX", "VSYNC",    GFXOP_VSYNC,    "",         0, false },
    { "GFX", "RESET",    GFXOP_RESET,    "W",        "ALL", false },
    { "TXT", "FILL",     GFXOP_TXTFILL,  "nnnnooo",  0, false },
    { "TXT", "COPY",     GFXOP_TXTCOPY,  "nnnnnn",   0, false },
    { "TXT", "INVERT",   GFXOP_TXTINVERT,"nnnn",     0, false },
    { "TXT", "SAVE",     GFXOP_TXTSAVE,  "v",        0, false },
    { "TXT", "LOAD",     GFXOP_TXTLOAD,  "s",        0, false },
    { "",    "CLS",      GFXOP_CLS,      "",         0, false },
    { "",    "LOCATE",   GFXOP_LOCATE,   "nn",       0, false },
    { "",    "COLOR",    GFXOP_COLOR,    "no",       0, false },
    { "",    "SYMBOL",   GFXOP_SYMBOL,   "ns",       0, false },
    { "TILE", "DEF",     GFXOP_TILEDEF,  "ns",       0, false },
    { "TILE", "GET",     GFXOP_TILEGET,  "nv",       0, false },
    { "TILE", "SET",     GFXOP_TILESET,  "nnn",      0, false },
    { "TILE", "FILL",    GFXOP_TILEFILL, "s",        0, false },
    { "TILE", "SCROLL",  GFXOP_TILESCROLL,"nn",      0, false },
    { "MOUSE", "SHOW",   GFXOP_MOUSESHOW,"",         0, false },
    { "MOUSE", "HIDE",   GFXOP_MOUSEHIDE,"",         0, false },
    { "MOUSE", "POS",    GFXOP_MOUSEPOS, "nn",       0, false },
    { "MOUSE", "SHAPE",  GFXOP_MOUSESHAPE,"w",       MOUSESHAPES, false },
    { "MOUSE", "TIMEOUT",GFXOP_MOUSETMO, "n",        0, false },
    { "SPRITE", "DEF",   GFXOP_SPRDEF,   "ns",       0, false },
    { "SPRITE", "GET",   GFXOP_SPRGET,   "nv",       0, false },
    { "SPRITE", "SEQ",   GFXOP_SPRSEQ,   "ns",       0, false },
    { "SPRITE", "ON",    GFXOP_SPRON,    "n",        0, true },
    { "SPRITE", "OFF",   GFXOP_SPROFF,   "n",        0, true },
    { "SPRITE", "PRIORITY", GFXOP_SPRPRIO, "nn",     0, true },
    { "SPRITE", "IMAGE", GFXOP_SPRIMAGE, "nn",       0, true },
    { "SPRITE", "ANIM",  GFXOP_SPRANIM,  "nnnn",     0, true },
    { "SPRITE", "MOVETO",GFXOP_SPRMOVETO,"nnnn",     0, true },
    { "SPRITE", "COLLISION", GFXOP_SPRCOLL, "nwW",   "OFF,ON|TILES", true },
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
    { "UI", "CANVAS",    GFXOP_UICANVAS, "nnnnnnn",  0, false },
    { "UI", "PIXEL",     GFXOP_UIPIXEL,  "nnnn",     0, false },
    { "UI", "IMAGE",     GFXOP_UIIMAGE,  "ns",       0, false },
    { "UI", "GETIMAGE",  GFXOP_UIGETIMAGE, "nv",     0, false },
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
    { "UI",     "EVENT",    GFXQ_UIEVENT,   0, 0 },
    { "UI",     "KIND",     GFXQ_UIKIND,    0, 0 },
    { "UI",     "VALUE",    GFXQ_UIVALUE,   0, 0 },
    { "UI",     "GET",      GFXQ_UIGET,     1, 1 },
    { "UI",     "COUNT",    GFXQ_UICOUNT,   1, 1 },
    { "UI",     "FOCUS",    GFXQ_UIFOCUS,   0, 0 },
    { "UI",     "OPEN",     GFXQ_UIOPEN,    0, 0 },
    { "UI",     "EVENTS",   GFXQ_UIEVENTS,  0, 0 },
    { "UI",     "PIXEL",    GFXQ_UIPIXEL,   3, 3 },
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
