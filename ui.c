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

#include "ui.h"
#include "joy.h"
#include "clipboard.h"
#include <math.h>

#define ROWH        14                  // the height of a list row
#define CHECKSZ     13
#define THUMBW      10
#define DBLCLICK_MS 400
#define SBW         15                  // the width of a scroll bar
#define SB_MINKNOB  10
#define SB_FIRST_MS 400                 // a held arrow or track click repeats after this, then every SB_REPEAT_MS
#define SB_REPEAT_MS 60

// -- small helpers ------------------------------------------------------------------------------------------------------------------------------------

static int clampi( int v, int lo, int hi ) {
    return v < lo ? lo : v > hi ? hi : v;
}

static int index_of( const ui_t* ui, int id ) {
    for ( int i = 0; i < ui->count; ++i ) {
        if ( ui->widgets[i].id == id ) {
            return i;
        }
    }
    return -1;
}

ui_widget_t* ui_find( ui_t* ui, int id ) {
    int i = index_of( ui, id );
    return i < 0 ? 0 : &ui->widgets[i];
}

static bool overlaps( const ui_widget_t* a, const ui_widget_t* b ) {
    return a->x < b->x + b->w && b->x < a->x + a->w && a->y < b->y + b->h && b->y < a->y + a->h;
}

static bool inside( const ui_widget_t* inner, const ui_widget_t* outer ) {
    return inner->x >= outer->x && inner->y >= outer->y && inner->x + inner->w <= outer->x + outer->w && inner->y + inner->h <= outer->y + outer->h;
}

static bool focusable( const ui_widget_t* w ) {
    return ( w->flags & UIF_VISIBLE ) && ( w->flags & UIF_ENABLED ) &&
        ( w->kind == UIK_BUTTON || w->kind == UIK_CHECK || w->kind == UIK_RADIO || w->kind == UIK_TEXT || w->kind == UIK_LIST || w->kind == UIK_SLIDER || w->kind == UIK_SCROLL || w->kind == UIK_EDIT );
}

// does the widget react to input at the moment (a pop-up or a modal panel limits it)?
static bool in_scope( ui_t* ui, const ui_widget_t* w ) {
    if ( ui->popup != 0 ) {
        return w->id == ui->popup;
    }
    if ( ui->modal != 0 ) {
        const ui_widget_t* m = ui_find( ui, ui->modal );
        return m != 0 && ( w == m || inside( w, m ) );
    }
    return true;
}

static uint32_t ui_events_total = 0;           // every event ever queued (by any ui_t): ON UI watches the change

uint32_t ui_event_count( void ) {
    return ui_events_total;
}

static void push_event( ui_t* ui, int id, int kind, int value ) {
    ++ui_events_total;
    if ( ui->qcount >= UI_EVENTS ) {
        return;
    }
    ui_event_t* e = &ui->queue[ ( ui->qhead + ui->qcount ) % UI_EVENTS ];
    e->id = id;
    e->kind = kind;
    e->value = value;
    ++ui->qcount;
}

bool ui_next_event( ui_t* ui, ui_event_t* ev ) {
    if ( ui->qcount == 0 ) {
        return false;
    }
    *ev = ui->queue[ ui->qhead ];
    ui->qhead = ( ui->qhead + 1 ) % UI_EVENTS;
    --ui->qcount;
    return true;
}

static void dirty( ui_widget_t* w ) {
    w->flags |= UIF_DIRTY;
}

// -- the theme ----------------------------------------------------------------------------------------------------------------------------------------

void ui_theme_default( ui_theme_t* t, int base ) {
    t->desk = base + 0;
    t->face = base + 1;
    t->light = base + 2;
    t->shadow = base + 3;
    t->text = base + 4;
    t->hilite = base + 5;
    t->hilitetext = base + 6;
    t->field = base + 7;
    t->disabled = base + 8;
    t->border = base + 9;
    t->title = base + 10;
    t->titletext = base + 11;
    t->fill = base + 12;
    t->font = FONT_SANS;
    t->boldfont = FONT_BOLD;
}

static const uint32_t theme_presets[ UI_THEMES ][ UI_PAL_COLORS ] = {
    {   // 0 classic
        0xFF006060, 0xFFC0C0C0, 0xFFFFFFFF, 0xFF808080, 0xFF000000, 0xFF000080, 0xFFFFFFFF, 0xFFFFFFFF,
        0xFF808080, 0xFF000000, 0xFF000080, 0xFFFFFFFF, 0xFF0000AA, 0xFFFFFF00, 0xFFFF0000, 0xFF008000
    },
    {   // 1 dark
        0xFF1B2128, 0xFF3A3F47, 0xFF5E656F, 0xFF14171B, 0xFFE0E0E0, 0xFF2B5EA8, 0xFFFFFFFF, 0xFF1E1E1E,
        0xFF7C828A, 0xFFE0E0E0, 0xFF1F4E8C, 0xFFFFFFFF, 0xFF3A7FD5, 0xFFFFFF00, 0xFFFF5050, 0xFF40C040
    },
    {   // 2 high contrast
        0xFF000000, 0xFF000000, 0xFFFFFFFF, 0xFF909090, 0xFFFFFFFF, 0xFFFFFF00, 0xFF000000, 0xFF000000,
        0xFFA0A0A0, 0xFFFFFFFF, 0xFFFFFF00, 0xFF000000, 0xFF00FF00, 0xFFFFFF00, 0xFFFF4040, 0xFF40FF40
    }
};

static const uint32_t syntax_presets[ UI_THEMES ][ UI_SYN_COLORS ] = {
    {   // plain, line number, statement, control, command, function, operator, identifier, number, string, shell, comment, punctuation, 3 free
        0xFF000000, 0xFF808000, 0xFF000080, 0xFF800080, 0xFFC00000, 0xFF008080, 0xFF404040, 0xFF000000,
        0xFF800000, 0xFF008000, 0xFFC000C0, 0xFF808080, 0xFF000000, 0xFFC00000, 0xFF008000, 0xFF0000C0
    },
    {
        0xFFE0E0E0, 0xFFC8B060, 0xFF6CB0FF, 0xFFD08CFF, 0xFFFF7070, 0xFF50D8D0, 0xFFD0D0D0, 0xFFE0E0E0,
        0xFFFFA070, 0xFF90E070, 0xFFFF80FF, 0xFF808890, 0xFFC0C0C0, 0xFFFF7070, 0xFF90E070, 0xFF70A0FF
    },
    {
        0xFFFFFFFF, 0xFFFFFF00, 0xFF40FFFF, 0xFFFF80FF, 0xFFFF6060, 0xFF80FF80, 0xFFFFFFFF, 0xFFFFFFFF,
        0xFFFFC040, 0xFFFFE080, 0xFFFF9000, 0xFFA0A0A0, 0xFFFFFFFF, 0xFFFF6060, 0xFF80FF80, 0xFF8080FF
    }
};

void ui_theme_preset( int n, uint32_t argb[ UI_PAL_COLORS ] ) {
    memcpy( argb, theme_presets[ n >= 0 && n < UI_THEMES ? n : 0 ], sizeof(theme_presets[0]) );
}

void ui_syntax_argb( int n, uint32_t argb[ UI_SYN_COLORS ] ) {
    memcpy( argb, syntax_presets[ n >= 0 && n < UI_THEMES ? n : 0 ], sizeof(syntax_presets[0]) );
}

void ui_theme_argb( uint32_t argb[ UI_PAL_COLORS ] ) {
    ui_theme_preset( 0, argb );
}

// -- drawing primitives (clipped to the widget being drawn) ---------------------------------------------------------------------------------------------

static void cbox( ui_t* ui, int x1, int y1, int x2, int y2, int color, bool fill ) {
    if ( !fill && x2 > x1 && y2 > y1 ) {                    // an outline is four lines, each clipped on its own (clipping the rectangle would draw a frame along the clip edge)
        cbox( ui, x1, y1, x2, y1, color, true );
        cbox( ui, x1, y2, x2, y2, color, true );
        cbox( ui, x1, y1 + 1, x1, y2 - 1, color, true );
        cbox( ui, x2, y1 + 1, x2, y2 - 1, color, true );
        return;
    }
    if ( x1 < ui->clip[0] ) {
        x1 = ui->clip[0];
    }
    if ( y1 < ui->clip[1] ) {
        y1 = ui->clip[1];
    }
    if ( x2 > ui->clip[2] ) {
        x2 = ui->clip[2];
    }
    if ( y2 > ui->clip[3] ) {
        y2 = ui->clip[3];
    }
    if ( x1 > x2 || y1 > y2 ) {
        return;
    }
    ui->canvas.box( ui->canvas.ctx, x1, y1, x2, y2, color, fill );
}

static void cpset( ui_t* ui, int x, int y, int color ) {
    if ( x < ui->clip[0] || y < ui->clip[1] || x > ui->clip[2] || y > ui->clip[3] ) {
        return;
    }
    ui->canvas.pset( ui->canvas.ctx, x, y, color );
}

static void span_cb( void* ctx, int x, int y, int w, int color ) {
    cbox( ctx, x, y, x + w - 1, y, color, true );
}

static int text_width( ui_t* ui, int fontid, const char* text, size_t len ) {
    return font_measure( font_get( fontid ), text, len );
}

static void draw_text( ui_t* ui, int fontid, int x, int y, const char* text, size_t len, int color ) {
    font_draw( font_get( fontid ), x, y, text, len, color, span_cb, ui );
}

static void set_clip( ui_t* ui, int x1, int y1, int x2, int y2 ) {
    ui->clip[0] = x1 < 0 ? 0 : x1;
    ui->clip[1] = y1 < 0 ? 0 : y1;
    ui->clip[2] = x2 >= ui->canvas.w ? ui->canvas.w - 1 : x2;
    ui->clip[3] = y2 >= ui->canvas.h ? ui->canvas.h - 1 : y2;
    if ( ui->dmgon ) {                                      // (a damage redraw: only this part of the canvas may change)
        if ( ui->clip[0] < ui->dmg[0] ) ui->clip[0] = ui->dmg[0];
        if ( ui->clip[1] < ui->dmg[1] ) ui->clip[1] = ui->dmg[1];
        if ( ui->clip[2] > ui->dmg[2] ) ui->clip[2] = ui->dmg[2];
        if ( ui->clip[3] > ui->dmg[3] ) ui->clip[3] = ui->dmg[3];
    }
}

// -- regions and damage lists ---------------------------------------------------------------------------------------------------------------------------

// a region used while drawing: a list of rectangles (x1 y1 x2 y2, inclusive), not necessarily disjoint
#define RGNMAX 96
typedef struct _rgn_t {
    int     n;
    int     r[RGNMAX][4];
    bool    over;                                           // it grew beyond RGNMAX: the caller falls back to drawing everything
} rgn_t;

static void rgn_set( rgn_t* g, int x1, int y1, int x2, int y2 ) {
    g->n = 0;
    g->over = false;
    if ( x1 <= x2 && y1 <= y2 ) {
        g->r[0][0] = x1; g->r[0][1] = y1; g->r[0][2] = x2; g->r[0][3] = y2;
        g->n = 1;
    }
}

static void rgn_push( rgn_t* g, int x1, int y1, int x2, int y2 ) {
    if ( x1 > x2 || y1 > y2 ) {
        return;
    }
    if ( g->n >= RGNMAX ) {
        g->over = true;
        return;
    }
    g->r[g->n][0] = x1; g->r[g->n][1] = y1; g->r[g->n][2] = x2; g->r[g->n][3] = y2;
    ++g->n;
}

// takes the rectangle out of the region (each member is cut into at most four pieces)
static void rgn_sub( rgn_t* g, int x1, int y1, int x2, int y2 ) {
    rgn_t out;
    out.n = 0;
    out.over = g->over;
    for ( int k = 0; k < g->n; ++k ) {
        int a = g->r[k][0], b = g->r[k][1], c = g->r[k][2], d = g->r[k][3];
        if ( x1 > c || x2 < a || y1 > d || y2 < b ) {
            rgn_push( &out, a, b, c, d );
            continue;
        }
        int ix1 = x1 > a ? x1 : a, ix2 = x2 < c ? x2 : c, iy1 = y1 > b ? y1 : b, iy2 = y2 < d ? y2 : d;
        rgn_push( &out, a, b, c, iy1 - 1 );                 // above
        rgn_push( &out, a, iy2 + 1, c, d );                 // below
        rgn_push( &out, a, iy1, ix1 - 1, iy2 );             // left
        rgn_push( &out, ix2 + 1, iy1, c, iy2 );             // right
    }
    *g = out;
}

// adds a rectangle to a widget's damage list (merged into the bounding box when the list is full)
static void dmg_add( ui_dmg_t* d, int x1, int y1, int x2, int y2 ) {
    if ( x1 > x2 || y1 > y2 ) {
        return;
    }
    for ( int k = 0; k < d->n; ++k ) {
        if ( x1 >= d->r[k][0] && y1 >= d->r[k][1] && x2 <= d->r[k][2] && y2 <= d->r[k][3] ) {
            return;                                         // already in the list
        }
    }
    int m = 0;
    for ( int k = 0; k < d->n; ++k ) {                      // the entries that the new one swallows go
        if ( !( d->r[k][0] >= x1 && d->r[k][1] >= y1 && d->r[k][2] <= x2 && d->r[k][3] <= y2 ) ) {
            if ( m != k ) {
                memcpy( d->r[m], d->r[k], sizeof(d->r[m]) );
            }
            ++m;
        }
    }
    d->n = m;
    if ( d->n >= UI_DMGMAX ) {
        for ( int k = 0; k < d->n; ++k ) {
            if ( d->r[k][0] < x1 ) x1 = d->r[k][0];
            if ( d->r[k][1] < y1 ) y1 = d->r[k][1];
            if ( d->r[k][2] > x2 ) x2 = d->r[k][2];
            if ( d->r[k][3] > y2 ) y2 = d->r[k][3];
        }
        d->n = 0;
    }
    d->r[d->n][0] = x1; d->r[d->n][1] = y1; d->r[d->n][2] = x2; d->r[d->n][3] = y2;
    ++d->n;
}

// the damage of w within the rectangle of another widget: what a widget in front of w (or one that goes away) does to it
static void dmg_add_overlap( ui_widget_t* w, int x1, int y1, int x2, int y2 ) {
    int ax1 = x1 > w->x ? x1 : w->x, ay1 = y1 > w->y ? y1 : w->y, ax2 = x2 < w->x + w->w - 1 ? x2 : w->x + w->w - 1, ay2 = y2 < w->y + w->h - 1 ? y2 : w->y + w->h - 1;
    dmg_add( &w->dmg, ax1, ay1, ax2, ay2 );
}

// which group a widget is in: the widgets of one group do not hide one another's pixels (a window and the widgets in it, a plain panel and the widgets that lie
// inside it); a widget in another group that is in front does
static int group_of( const ui_t* ui, int i ) {
    const ui_widget_t* w = &ui->widgets[i];
    if ( w->owner != 0 ) {
        return w->owner;
    }
    if ( ( w->flags & UIF_WINDOW ) || w->kind == UIK_PANEL ) {
        return w->id;
    }
    for ( int k = i - 1; k >= 0; --k ) {
        const ui_widget_t* p = &ui->widgets[k];
        if ( p->kind == UIK_PANEL && !( p->flags & UIF_WINDOW ) && ( p->flags & UIF_VISIBLE ) && inside( w, p ) ) {
            return p->id;
        }
    }
    return w->id;
}

// the visible part of widget i: its rectangle without the rectangles of the visible widgets in front of it that are in another group; false: too complex
static bool vis_region( const ui_t* ui, const int* grp, int i, rgn_t* g ) {
    const ui_widget_t* w = &ui->widgets[i];
    int x1 = w->x < 0 ? 0 : w->x, y1 = w->y < 0 ? 0 : w->y;
    int x2 = w->x + w->w - 1 >= ui->canvas.w ? ui->canvas.w - 1 : w->x + w->w - 1, y2 = w->y + w->h - 1 >= ui->canvas.h ? ui->canvas.h - 1 : w->y + w->h - 1;
    rgn_set( g, x1, y1, x2, y2 );
    for ( int j = i + 1; j < ui->count && g->n > 0; ++j ) {
        const ui_widget_t* o = &ui->widgets[j];
        if ( ( o->flags & UIF_VISIBLE ) && grp[j] != grp[i] && overlaps( w, o ) ) {
            rgn_sub( g, o->x, o->y, o->x + o->w - 1, o->y + o->h - 1 );
        }
    }
    return !g->over;
}

// a 1 pixel border and a 1 pixel bevel: raised (light top left, shadow bottom right) or sunken (the other way round)
static void bevel( ui_t* ui, int x1, int y1, int x2, int y2, bool raised ) {
    const ui_theme_t* t = &ui->theme;
    cbox( ui, x1, y1, x2, y2, t->border, false );
    int tl = raised ? t->light : t->shadow, br = raised ? t->shadow : t->light;
    cbox( ui, x1 + 1, y1 + 1, x2 - 1, y1 + 1, tl, true );
    cbox( ui, x1 + 1, y1 + 1, x1 + 1, y2 - 1, tl, true );
    cbox( ui, x1 + 1, y2 - 1, x2 - 1, y2 - 1, br, true );
    cbox( ui, x2 - 1, y1 + 1, x2 - 1, y2 - 1, br, true );
}

static void dotted( ui_t* ui, int x1, int y1, int x2, int y2, int color ) {
    for ( int x = x1; x <= x2; x += 2 ) {
        cpset( ui, x, y1, color );
        cpset( ui, x, y2, color );
    }
    for ( int y = y1; y <= y2; y += 2 ) {
        cpset( ui, x1, y, color );
        cpset( ui, x2, y, color );
    }
}

static void disc( ui_t* ui, int cx, int cy, int r, int color ) {
    for ( int dy = -r; dy <= r; ++dy ) {
        int hw = (int) floor( sqrt( (double)( r * r - dy * dy ) ) + 0.5 );
        cbox( ui, cx - hw, cy + dy, cx + hw, cy + dy, color, true );
    }
}

// the colour behind a widget: the face of the panel it lies in, else the desk
static int background( ui_t* ui, int index ) {
    const ui_widget_t* w = &ui->widgets[index];
    if ( w->kind == UIK_PANEL ) {
        return ui->theme.desk;
    }
    int bg = ui->theme.desk;
    for ( int i = 0; i < index; ++i ) {
        if ( ui->widgets[i].kind == UIK_PANEL && ( ui->widgets[i].flags & UIF_VISIBLE ) && inside( w, &ui->widgets[i] ) ) {
            bg = ui->theme.face;
        }
    }
    return bg;
}

static int list_rows( const ui_widget_t* w ) {
    int n = ( w->h - 4 ) / ROWH;
    return n < 1 ? 1 : n;
}


// -- canvas: a zoomed grid of cells --------------------------------------------------------------------------------

// the size of a cell in pixels and the upper left corner of the grid (centered in the widget)
static bool canvas_geom( const ui_widget_t* w, int* pz, int* pox, int* poy ) {
    if ( w->cw < 1 || w->ch < 1 ) {
        return false;
    }
    int zx = ( w->w - 4 ) / w->cw, zy = ( w->h - 4 ) / w->ch, z = zx < zy ? zx : zy;
    if ( z < 1 ) {
        return false;
    }
    *pz = z;
    *pox = w->x + 2 + ( ( w->w - 4 ) - z * w->cw ) / 2;
    *poy = w->y + 2 + ( ( w->h - 4 ) - z * w->ch ) / 2;
    return true;
}

// the cell under the mouse, or -1
static int canvas_cell( const ui_widget_t* w, int mx, int my ) {
    int z, ox, oy;
    if ( w->pix == 0 || !canvas_geom( w, &z, &ox, &oy ) ) {
        return -1;
    }
    int cx = ( mx - ox ), cy = ( my - oy );
    if ( cx < 0 || cy < 0 || cx >= z * w->cw || cy >= z * w->ch ) {
        return -1;
    }
    return ( cy / z ) * w->cw + cx / z;
}

// -- scroll bars (the widget UIK_SCROLL and the bar inside a list that has more rows than it shows) ----------------------------------------------------------

static int list_rows( const ui_widget_t* w );
static int edit_nlines( const ui_widget_t* w );
static int edit_rows( const ui_widget_t* w );

typedef struct _sbgeom_t {
    bool    vert;
    int     x1, y1, x2, y2;             // the whole bar
    int     a1, a2;                     // along the bar: its first and last coordinate
    int     c1, c2;                     // across
    int     asz;                        // the size of an arrow button (along the bar)
    int     t1, t2, tlen;               // the track between the arrows
    int     k1, klen;                   // the knob: its first coordinate and length
    int     min, max, page, value;
} sbgeom_t;

// the parameters of the bar of a widget; false: it has none (a list that shows all its rows)
static bool sb_params( const ui_widget_t* w, sbgeom_t* g ) {
    memset( g, 0, sizeof(*g) );
    if ( w->kind == UIK_SCROLL ) {
        g->x1 = w->x; g->y1 = w->y; g->x2 = w->x + w->w - 1; g->y2 = w->y + w->h - 1;
        g->vert = w->h >= w->w;
        g->min = w->min; g->max = w->max; g->page = w->page < 1 ? 1 : w->page; g->value = w->value;
    } else if ( w->kind == UIK_LIST && w->nitems > list_rows( w ) ) {
        int rows = list_rows( w );
        g->x1 = w->x + w->w - 2 - SBW; g->x2 = w->x + w->w - 3; g->y1 = w->y + 2; g->y2 = w->y + w->h - 3;
        g->vert = true;
        g->min = 0; g->max = w->nitems - rows; g->page = rows; g->value = w->scroll;
    } else if ( w->kind == UIK_EDIT && w->edit != 0 && edit_nlines( w ) > edit_rows( w ) ) {
        int rows = edit_rows( w );
        g->x1 = w->x + w->w - 2 - SBW; g->x2 = w->x + w->w - 3; g->y1 = w->y + 2; g->y2 = w->y + w->h - 3;
        g->vert = true;
        g->min = 0; g->max = edit_nlines( w ) - rows; g->page = rows; g->value = w->scroll;
    } else {
        return false;
    }
    g->a1 = g->vert ? g->y1 : g->x1;
    g->a2 = g->vert ? g->y2 : g->x2;
    g->c1 = g->vert ? g->x1 : g->y1;
    g->c2 = g->vert ? g->x2 : g->y2;
    int cross = g->c2 - g->c1 + 1, len = g->a2 - g->a1 + 1;
    g->asz = cross < len / 2 ? cross : len / 2;
    g->t1 = g->a1 + g->asz;
    g->t2 = g->a2 - g->asz;
    g->tlen = g->t2 - g->t1 + 1;
    int range = g->max - g->min;
    g->klen = g->tlen;
    g->k1 = g->t1;
    if ( range > 0 && g->tlen > 0 ) {
        long kl = (long) g->tlen * g->page / ( range + g->page );
        g->klen = (int) ( kl < SB_MINKNOB ? SB_MINKNOB : kl );
        if ( g->klen > g->tlen ) {
            g->klen = g->tlen;
        }
        g->k1 = g->t1 + (int)( (long)( g->tlen - g->klen ) * ( g->value - g->min ) / range );
    }
    return true;
}

// which part is at the coordinate a along the bar
static int sb_part( const sbgeom_t* g, int a ) {
    if ( a < g->a1 + g->asz ) {
        return UISB_ARROW1;
    }
    if ( a > g->a2 - g->asz ) {
        return UISB_ARROW2;
    }
    if ( a < g->k1 ) {
        return UISB_BEFORE;
    }
    if ( a >= g->k1 + g->klen ) {
        return UISB_AFTER;
    }
    return UISB_KNOB;
}

// a box given along / across the bar
static void sbbox( ui_t* ui, const sbgeom_t* g, int a1, int c1, int a2, int c2, int color, bool fill ) {
    if ( g->vert ) {
        cbox( ui, c1, a1, c2, a2, color, fill );
    } else {
        cbox( ui, a1, c1, a2, c2, color, fill );
    }
}

// an arrow pointing to the start (dir -1) or the end (dir 1) of the bar, in the box a1..a2 x c1..c2
static void sb_arrow( ui_t* ui, const sbgeom_t* g, int a1, int a2, int dir, int color ) {
    int c = ( g->c1 + g->c2 ) / 2, am = ( a1 + a2 ) / 2;
    for ( int i = 0; i < 4; ++i ) {                                   // 4 steps: 1, 3, 5, 7 pixels wide
        int a = dir < 0 ? am - 2 + i : am + 1 - i;
        sbbox( ui, g, a, c - i, a, c + i, color, true );
    }
}

static void draw_scrollbar( ui_t* ui, const sbgeom_t* g, bool on, int pressedpart ) {
    const ui_theme_t* t = &ui->theme;
    int sav[4] = { ui->clip[0], ui->clip[1], ui->clip[2], ui->clip[3] };
    // the track: a light / face checker, so that it stands out from the knob and from the page behind it
    for ( int a = g->a1; a <= g->a2; ++a ) {
        for ( int c = g->c1; c <= g->c2; ++c ) {
            int color = ( ( a + c ) & 1 ) ? t->light : t->face;
            if ( g->vert ) {
                cpset( ui, c, a, color );
            } else {
                cpset( ui, a, c, color );
            }
        }
    }
    sbbox( ui, g, g->a1, g->c1, g->a2, g->c2, t->border, false );
    int tc = on ? t->text : t->disabled;
    for ( int i = 0; i < 2; ++i ) {
        int a1 = i == 0 ? g->a1 : g->a2 - g->asz + 1, a2 = a1 + g->asz - 1;
        bool pr = pressedpart == ( i == 0 ? UISB_ARROW1 : UISB_ARROW2 );
        sbbox( ui, g, a1, g->c1, a2, g->c2, t->face, true );
        if ( g->vert ) {
            bevel( ui, g->c1, a1, g->c2, a2, !pr );
        } else {
            bevel( ui, a1, g->c1, a2, g->c2, !pr );
        }
        sb_arrow( ui, g, a1 + ( pr ? 1 : 0 ), a2 + ( pr ? 1 : 0 ), i == 0 ? -1 : 1, tc );
    }
    if ( g->tlen > 0 && g->max > g->min ) {
        int k2 = g->k1 + g->klen - 1;
        sbbox( ui, g, g->k1, g->c1, k2, g->c2, t->face, true );
        if ( g->vert ) {
            bevel( ui, g->c1, g->k1, g->c2, k2, true );
        } else {
            bevel( ui, g->k1, g->c1, k2, g->c2, true );
        }
        if ( g->klen >= 16 ) {                                         // a grip: three short lines in the middle
            int m = g->k1 + g->klen / 2, cm1 = g->c1 + 4, cm2 = g->c2 - 4;
            for ( int i = -2; i <= 2; i += 2 ) {
                sbbox( ui, g, m + i, cm1, m + i, cm2, t->shadow, true );
            }
        }
    }
    ui->clip[0] = sav[0]; ui->clip[1] = sav[1]; ui->clip[2] = sav[2]; ui->clip[3] = sav[3];
}

// moves the bar of a widget to a new position (clamped); a scroll bar widget sends UIE_CHANGE
static void sb_set( ui_t* ui, ui_widget_t* w, int v ) {
    sbgeom_t g;
    if ( !sb_params( w, &g ) ) {
        return;
    }
    v = clampi( v, g.min, g.max );
    if ( w->kind == UIK_SCROLL ) {
        if ( v != w->value ) {
            w->value = v;
            dirty( w );
            push_event( ui, w->id, UIE_CHANGE, v );
        }
    } else if ( v != w->scroll ) {
        w->scroll = v;
        dirty( w );
    }
}

// one step of a held part (arrow: 1, track: a page)
static void sb_step( ui_t* ui, ui_widget_t* w, const sbgeom_t* g, int part ) {
    int v = g->value;
    switch ( part ) {
        case UISB_ARROW1: v -= 1; break;
        case UISB_ARROW2: v += 1; break;
        case UISB_BEFORE: v -= g->page; break;
        case UISB_AFTER:  v += g->page; break;
        default: return;
    }
    sb_set( ui, w, v );
}

// the mouse moved the knob that was grabbed
static void sb_drag( ui_t* ui, ui_widget_t* w, int mx, int my ) {
    sbgeom_t g;
    if ( !sb_params( w, &g ) || g.tlen <= g.klen ) {
        return;
    }
    int a = g.vert ? my : mx, range = g.max - g.min;
    int p = clampi( a - ui->capoff - g.t1, 0, g.tlen - g.klen );
    sb_set( ui, w, g.min + (int)( ( (long) p * range + ( g.tlen - g.klen ) / 2 ) / ( g.tlen - g.klen ) ) );
}

// -- the multi-line text editor (UIK_EDIT) -----------------------------------------------------------------------------------------------------------------
//
// The text is the widget's text, lines separated by LF, in the fixed font (8 x 12). The widget keeps the caret (a byte index, w->caret), the selection
// (the anchor in ui_edit_t, -1 if there is none), the first visible line (w->scroll), the first visible pixel column (hscroll) and undo steps
// (snapshots of the whole text). It has a vertical scroll bar (the machinery of the list) when it has more lines than it shows.

#define UI_UNDO         24
#define EDIT_CW         8                       // the width of a glyph of FONT_FIXED
#define EDIT_PAD        3                       // pixels between the bevel and the text

typedef struct _ui_edit_t {
    int     anchor;                             // the other end of the selection (a byte index), -1: none
    int     hscroll;                            // pixels the text is moved to the left
    int     wantcol;                            // the column Up / Down try to keep (-1: the caret's)
    bool    vert;                               // the last key was Up / Down / Page up / down (keeps wantcol)
    char*   undo[UI_UNDO];                      // snapshots of the text before a change (oldest first) and the caret at that time
    int     ucaret[UI_UNDO];
    int     nundo;
    char*   redo[UI_UNDO];
    int     rcaret[UI_UNDO];
    int     nredo;
    int     lastkind;                           // what the last change was (typing is joined to one undo step)
    int     lastend;                            // the caret after it
    uint8_t* hil;                               // syntax slots per byte of the text (UI HILITE), hillen bytes; 0: none
    int     hillen;
    bool    drag;                               // the left button went down in the text: the mouse extends the selection
    uint32_t lastclick_ms;
    int     lastclick_pos, clicks;
} ui_edit_t;

#define EK_NONE         0
#define EK_TYPE         1
#define EK_DELETE       2
#define EK_OTHER        3

static bool edit_init( ui_widget_t* w ) {
    w->edit = (ui_edit_t*) calloc( 1U, sizeof(ui_edit_t) );
    if ( w->edit == 0 ) {
        return false;
    }
    w->edit->anchor = -1;
    w->edit->wantcol = -1;
    w->caret = 0;
    return true;
}

static void edit_clear_undo( ui_edit_t* e ) {
    for ( int i = 0; i < e->nundo; ++i ) free( e->undo[i] );
    for ( int i = 0; i < e->nredo; ++i ) free( e->redo[i] );
    e->nundo = e->nredo = 0;
}

static void edit_free( ui_widget_t* w ) {
    if ( w->edit != 0 ) {
        free( w->edit->hil );
        edit_clear_undo( w->edit );
        free( w->edit );
        w->edit = 0;
    }
}

// a new text: the caret at the start, nothing selected, no undo
static void edit_reset( ui_widget_t* w ) {
    if ( w->edit == 0 ) {
        return;
    }
    edit_clear_undo( w->edit );
    free( w->edit->hil );
    w->edit->hil = 0;
    w->edit->hillen = 0;
    w->edit->anchor = -1;
    w->edit->hscroll = 0;
    w->edit->wantcol = -1;
    w->edit->lastkind = EK_NONE;
    w->caret = 0;
    w->scroll = 0;
}

static int edit_len( const ui_widget_t* w ) {
    return w->text != 0 ? (int) strlen( w->text ) : 0;
}

// the first byte of line n (0-based); the end of the text if there is no such line
static int edit_linestart( const ui_widget_t* w, int line ) {
    const char* t = w->text != 0 ? w->text : "";
    int pos = 0;
    while ( line > 0 && t[pos] != '\0' ) {
        if ( t[pos] == '\n' ) {
            --line;
        }
        ++pos;
    }
    return pos;
}

static int edit_lineend( const ui_widget_t* w, int start ) {      // the LF or the end of the text
    const char* t = w->text != 0 ? w->text : "";
    int pos = start;
    while ( t[pos] != '\0' && t[pos] != '\n' ) {
        ++pos;
    }
    return pos;
}

static int edit_nlines( const ui_widget_t* w ) {
    int n = 1;
    for ( const char* p = w->text != 0 ? w->text : ""; *p != '\0'; ++p ) {
        if ( *p == '\n' ) ++n;
    }
    return n;
}

static int edit_lineof( const ui_widget_t* w, int pos, int* col ) {
    const char* t = w->text != 0 ? w->text : "";
    int line = 0, ls = 0;
    for ( int i = 0; i < pos && t[i] != '\0'; ++i ) {
        if ( t[i] == '\n' ) {
            ++line;
            ls = i + 1;
        }
    }
    if ( col != 0 ) {
        *col = pos - ls;
    }
    return line;
}

static int edit_rows( const ui_widget_t* w ) {
    int n = ( w->h - 4 - 2 * ( EDIT_PAD - 2 ) ) / FONT_HEIGHT;
    return n < 1 ? 1 : n;
}

// the selection [*a, *b); false if there is none
static bool edit_sel( const ui_widget_t* w, int* a, int* b ) {
    if ( w->edit == 0 || w->edit->anchor < 0 || w->edit->anchor == w->caret ) {
        return false;
    }
    int x = w->edit->anchor, y = w->caret;
    *a = x < y ? x : y;
    *b = x < y ? y : x;
    return true;
}

// the width in pixels of the text area (the bar takes room when there are more lines than rows)
static int edit_textw( const ui_widget_t* w ) {
    int bar = edit_nlines( w ) > edit_rows( w ) ? SBW + 2 : 0;
    return w->w - 4 - 2 * ( EDIT_PAD - 2 ) - bar;
}

// scrolls so that the caret is visible
static void edit_show_caret( ui_widget_t* w ) {
    int col = 0, line = edit_lineof( w, w->caret, &col ), rows = edit_rows( w ), nl = edit_nlines( w );
    if ( line < w->scroll ) {
        w->scroll = line;
    } else if ( line >= w->scroll + rows ) {
        w->scroll = line - rows + 1;
    }
    if ( w->scroll > nl - rows ) {
        w->scroll = nl - rows;
    }
    if ( w->scroll < 0 ) {
        w->scroll = 0;
    }
    int tw = edit_textw( w ), cx = col * EDIT_CW;
    if ( cx < w->edit->hscroll ) {
        w->edit->hscroll = cx;
    } else if ( cx + EDIT_CW > w->edit->hscroll + tw ) {
        w->edit->hscroll = cx + EDIT_CW - tw;
    }
    if ( w->edit->hscroll < 0 ) {
        w->edit->hscroll = 0;
    }
}

static void edit_push_undo( ui_widget_t* w ) {
    ui_edit_t* e = w->edit;
    for ( int i = 0; i < e->nredo; ++i ) free( e->redo[i] );
    e->nredo = 0;
    if ( e->nundo == UI_UNDO ) {
        free( e->undo[0] );
        memmove( e->undo, e->undo + 1, ( UI_UNDO - 1 ) * sizeof(char*) );
        memmove( e->ucaret, e->ucaret + 1, ( UI_UNDO - 1 ) * sizeof(int) );
        --e->nundo;
    }
    char* copy = strdup( w->text != 0 ? w->text : "" );
    if ( copy != 0 ) {
        e->undo[e->nundo] = copy;
        e->ucaret[e->nundo] = w->caret;
        ++e->nundo;
    }
}

// replaces the bytes [from, to) of the text by ins (LF and the characters 32..255 without 127; the rest is dropped); the caret goes behind the new text.
// kind joins consecutive typing / deleting to one undo step. false: nothing changed (too long)
static bool edit_replace( ui_t* ui, ui_widget_t* w, int from, int to, const char* ins, size_t inslen, int kind ) {
    char* clean = (char*) malloc( inslen + 1U );
    if ( clean == 0 ) {
        return false;
    }
    size_t m = 0;
    for ( size_t i = 0; i < inslen; ++i ) {
        unsigned char c = (unsigned char) ins[i];
        if ( c == '\n' || ( c >= 32 && c != 127 ) ) {
            clean[m++] = (char) c;
        }
    }
    int len = edit_len( w );
    if ( from < 0 ) from = 0;
    if ( to > len ) to = len;
    if ( from > to ) from = to;
    if ( (long) len - ( to - from ) + (long) m > UI_EDITMAX ) {
        m = (size_t)( UI_EDITMAX - ( len - ( to - from ) ) );
        if ( (long) UI_EDITMAX < ( len - ( to - from ) ) ) {
            free( clean );
            return false;
        }
    }
    if ( m == 0 && to == from ) {
        free( clean );
        return false;
    }
    char* nt = (char*) malloc( (size_t)( len - ( to - from ) ) + m + 1U );
    if ( nt == 0 ) {
        free( clean );
        return false;
    }
    ui_edit_t* e = w->edit;
    bool join = kind != EK_OTHER && kind == e->lastkind && w->caret == e->lastend && e->anchor < 0;
    if ( !join ) {
        edit_push_undo( w );
    }
    if ( e->hil != 0 ) {                                    // the syntax slots move with the text; what is new is plain until the program colors it again
        int nl = len - ( to - from ) + (int) m;
        uint8_t* nh = (uint8_t*) calloc( (size_t) nl + 1U, 1U );
        if ( nh != 0 ) {
            for ( int i = 0; i < from && i < e->hillen; ++i ) nh[i] = e->hil[i];
            for ( int i = to; i < len && i < e->hillen; ++i ) nh[ from + (int) m + ( i - to ) ] = e->hil[i];
            free( e->hil );
            e->hil = nh;
            e->hillen = nl;
        } else {
            free( e->hil );
            e->hil = 0;
            e->hillen = 0;
        }
    }
    memcpy( nt, w->text, (size_t) from );
    memcpy( nt + from, clean, m );
    memcpy( nt + from + m, w->text + to, (size_t)( len - to ) + 1U );
    free( clean );
    free( w->text );
    w->text = nt;
    w->caret = from + (int) m;
    e->anchor = -1;
    e->lastkind = kind;
    e->lastend = w->caret;
    e->vert = false;
    edit_show_caret( w );
    dirty( w );
    push_event( ui, w->id, UIE_CHANGE, edit_len( w ) );
    return true;
}

static void edit_clip_set( ui_t* ui, const char* text, size_t len ) {
    if ( ui->clip_set != 0 ) {
        ui->clip_set( text, len );
    } else {
        clip_internal_set( text, len );
    }
}

static void edit_copy( ui_t* ui, ui_widget_t* w ) {
    int a = 0, b = 0;
    if ( edit_sel( w, &a, &b ) ) {
        edit_clip_set( ui, w->text + a, (size_t)( b - a ) );
    }
}

static void edit_paste( ui_t* ui, ui_widget_t* w ) {
    size_t len = 0;
    char* text = ui->clip_get != 0 ? ui->clip_get( &len ) : clip_internal_get( &len );
    if ( text == 0 ) {
        return;
    }
    len = clip_normalize( text, len );
    int a = w->caret, b = w->caret;
    edit_sel( w, &a, &b );
    edit_replace( ui, w, a, b, text, len, EK_OTHER );
    free( text );
}

static void edit_undo( ui_t* ui, ui_widget_t* w, bool redo ) {
    ui_edit_t* e = w->edit;
    char** from = redo ? e->redo : e->undo;
    int* fcaret = redo ? e->rcaret : e->ucaret;
    int* nfrom = redo ? &e->nredo : &e->nundo;
    if ( *nfrom == 0 ) {
        return;
    }
    char** to = redo ? e->undo : e->redo;
    int* tcaret = redo ? e->ucaret : e->rcaret;
    int* nto = redo ? &e->nundo : &e->nredo;
    char* cur = strdup( w->text != 0 ? w->text : "" );
    if ( cur == 0 ) {
        return;
    }
    if ( *nto == UI_UNDO ) {
        free( to[0] );
        memmove( to, to + 1, ( UI_UNDO - 1 ) * sizeof(char*) );
        memmove( tcaret, tcaret + 1, ( UI_UNDO - 1 ) * sizeof(int) );
        --*nto;
    }
    to[*nto] = cur;
    tcaret[*nto] = w->caret;
    ++*nto;
    --*nfrom;
    free( w->text );
    free( e->hil );
    e->hil = 0;
    e->hillen = 0;
    w->text = from[*nfrom];
    w->caret = fcaret[*nfrom];
    int len = edit_len( w );
    if ( w->caret > len ) w->caret = len;
    e->anchor = -1;
    e->lastkind = EK_NONE;
    e->vert = false;
    edit_show_caret( w );
    dirty( w );
    push_event( ui, w->id, UIE_CHANGE, len );
}

// moves the caret to pos; extend = keep (or begin) the selection, else drop it
static void edit_move( ui_widget_t* w, int pos, bool extend, bool vert ) {
    ui_edit_t* e = w->edit;
    int len = edit_len( w );
    pos = pos < 0 ? 0 : pos > len ? len : pos;
    if ( extend ) {
        if ( e->anchor < 0 ) {
            e->anchor = w->caret;
        }
    } else {
        e->anchor = -1;
    }
    w->caret = pos;
    e->lastkind = EK_NONE;
    if ( !vert ) {
        e->wantcol = -1;
    }
    e->vert = vert;
    edit_show_caret( w );
    dirty( w );
}

// the byte position at the column col of line (clamped to the line)
static int edit_pos_at( const ui_widget_t* w, int line, int col ) {
    int nl = edit_nlines( w );
    line = line < 0 ? 0 : line >= nl ? nl - 1 : line;
    int ls = edit_linestart( w, line ), le = edit_lineend( w, ls );
    return col < 0 ? ls : ls + col > le ? le : ls + col;
}

static bool edit_wordchar( int c ) {
    return ( c >= '0' && c <= '9' ) || ( c >= 'A' && c <= 'Z' ) || ( c >= 'a' && c <= 'z' ) || c == '_' || c >= 160;
}

static void edit_key( ui_t* ui, ui_widget_t* w, int key, int mods ) {
    ui_edit_t* e = w->edit;
    bool shift = ( mods & UIKEY_SHIFT ) != 0, ctrl = ( mods & UIKEY_CTRL ) != 0;
    int len = edit_len( w ), a = 0, b = 0;
    bool hassel = edit_sel( w, &a, &b );
    int col = 0, line = edit_lineof( w, w->caret, &col );
    int rows = edit_rows( w ), nl = edit_nlines( w );
    if ( ctrl ) {
        switch ( key ) {
            case 'a':
                e->anchor = 0;
                w->caret = len;
                e->lastkind = EK_NONE;
                edit_show_caret( w );
                dirty( w );
                return;
            case 'c':
                edit_copy( ui, w );
                return;
            case 'x':
                if ( hassel ) {
                    edit_copy( ui, w );
                    edit_replace( ui, w, a, b, "", 0, EK_OTHER );
                }
                return;
            case 'v':
                edit_paste( ui, w );
                return;
            case 'z':
                edit_undo( ui, w, false );
                return;
            case 'y':
                edit_undo( ui, w, true );
                return;
            case UIKEY_HOME:
                edit_move( w, 0, shift, false );
                return;
            case UIKEY_END:
                edit_move( w, len, shift, false );
                return;
            default:
                return;
        }
    }
    if ( key >= 32 && key <= 255 && key != 127 ) {
        char c = (char) key;
        edit_replace( ui, w, hassel ? a : w->caret, hassel ? b : w->caret, &c, 1U, EK_TYPE );
        return;
    }
    switch ( key ) {
        case UIKEY_ENTER: {
            char nlbuf[ 64 ];
            size_t m = 0;
            nlbuf[m++] = '\n';
            int ls = edit_linestart( w, line );
            while ( m < sizeof(nlbuf) - 1U && ls + (int) m - 1 < w->caret && w->text[ ls + (int) m - 1 ] == ' ' ) {      // the new line starts as indented as this one
                nlbuf[m] = ' ';
                ++m;
            }
            edit_replace( ui, w, hassel ? a : w->caret, hassel ? b : w->caret, nlbuf, m, EK_OTHER );
            break;
        }
        case UIKEY_BACKSPACE:
            if ( hassel ) {
                edit_replace( ui, w, a, b, "", 0, EK_OTHER );
            } else if ( w->caret > 0 ) {
                edit_replace( ui, w, w->caret - 1, w->caret, "", 0, EK_DELETE );
            }
            break;
        case UIKEY_DELETE:
            if ( hassel ) {
                edit_replace( ui, w, a, b, "", 0, EK_OTHER );
            } else if ( w->caret < len ) {
                edit_replace( ui, w, w->caret, w->caret + 1, "", 0, EK_DELETE );
            }
            break;
        case UIKEY_LEFT:
            if ( hassel && !shift ) {
                edit_move( w, a, false, false );
            } else {
                edit_move( w, w->caret - 1, shift, false );
            }
            break;
        case UIKEY_RIGHT:
            if ( hassel && !shift ) {
                edit_move( w, b, false, false );
            } else {
                edit_move( w, w->caret + 1, shift, false );
            }
            break;
        case UIKEY_HOME:
            edit_move( w, edit_linestart( w, line ), shift, false );
            break;
        case UIKEY_END:
            edit_move( w, edit_lineend( w, edit_linestart( w, line ) ), shift, false );
            break;
        case UIKEY_UP: case UIKEY_DOWN: case UIKEY_PGUP: case UIKEY_PGDN: {
            int step = key == UIKEY_UP ? -1 : key == UIKEY_DOWN ? 1 : key == UIKEY_PGUP ? -( rows - 1 ) : rows - 1;
            if ( !e->vert || e->wantcol < 0 ) {
                e->wantcol = col;
            }
            int want = e->wantcol, to = line + step;
            if ( to < 0 ) to = 0;
            if ( to >= nl ) to = nl - 1;
            if ( key == UIKEY_UP && line == 0 ) {
                edit_move( w, 0, shift, true );
            } else if ( key == UIKEY_DOWN && line == nl - 1 ) {
                edit_move( w, len, shift, true );
            } else {
                edit_move( w, edit_pos_at( w, to, want ), shift, true );
            }
            e->wantcol = want;
            break;
        }
        default:
            break;
    }
}

// a press (clicks counts the clicks in a row: 2 = the word, 3 = the line) / the pointer moving with the button held
static int edit_hit( const ui_widget_t* w, int mx, int my ) {
    int line = w->scroll + ( my - ( w->y + EDIT_PAD ) ) / FONT_HEIGHT;
    if ( my < w->y + EDIT_PAD ) {
        line = w->scroll - 1;
    }
    int col = ( mx - ( w->x + EDIT_PAD ) + w->edit->hscroll + EDIT_CW / 2 ) / EDIT_CW;
    return edit_pos_at( w, line, col );
}

static void edit_press( ui_t* ui, ui_widget_t* w, int mx, int my, uint32_t now_ms ) {
    ui_edit_t* e = w->edit;
    int pos = edit_hit( w, mx, my );
    if ( now_ms - e->lastclick_ms < 450U && e->lastclick_pos == pos ) {
        e->clicks = e->clicks >= 3 ? 1 : e->clicks + 1;
    } else {
        e->clicks = 1;
    }
    e->lastclick_ms = now_ms;
    e->lastclick_pos = pos;
    e->lastkind = EK_NONE;
    e->vert = false;
    e->wantcol = -1;
    if ( e->clicks == 3 ) {                                         // the line
        int col = 0, line = edit_lineof( w, pos, &col ), ls = edit_linestart( w, line );
        e->anchor = ls;
        w->caret = edit_lineend( w, ls );
    } else if ( e->clicks == 2 ) {                                  // the word
        const char* t = w->text;
        int s = pos, z = pos;
        if ( edit_wordchar( (unsigned char) t[z] ) || ( z > 0 && edit_wordchar( (unsigned char) t[z - 1] ) ) ) {
            if ( !edit_wordchar( (unsigned char) t[z] ) ) {
                --s; --z;
            }
            while ( s > 0 && edit_wordchar( (unsigned char) t[s - 1] ) ) --s;
            while ( t[z] != '\0' && edit_wordchar( (unsigned char) t[z] ) ) ++z;
        }
        e->anchor = s;
        w->caret = z;
    } else {
        e->anchor = pos;
        w->caret = pos;
        e->drag = true;
    }
    edit_show_caret( w );
    dirty( w );
    (void) ui;
}

static void edit_drag( ui_widget_t* w, int mx, int my ) {
    ui_edit_t* e = w->edit;
    int pos = edit_hit( w, mx, my );
    if ( my > w->y + w->h - EDIT_PAD ) {                            // below the text: the view moves on
        pos = edit_pos_at( w, w->scroll + edit_rows( w ), ( mx - ( w->x + EDIT_PAD ) + e->hscroll + EDIT_CW / 2 ) / EDIT_CW );
    }
    if ( pos != w->caret ) {
        w->caret = pos;
        edit_show_caret( w );
        dirty( w );
    }
}

bool ui_edit_caret( ui_t* ui, int id, int pos ) {
    ui_widget_t* w = ui_find( ui, id );
    if ( w == 0 || w->kind != UIK_EDIT || w->edit == 0 ) {
        return false;
    }
    edit_move( w, pos, false, false );
    return true;
}

int ui_edit_query( ui_t* ui, int id, int what ) {
    ui_widget_t* w = ui_find( ui, id );
    if ( w == 0 || w->kind != UIK_EDIT || w->edit == 0 ) {
        return -1;
    }
    int col = 0, line = edit_lineof( w, w->caret, &col ), a = 0, b = 0;
    switch ( what ) {
        case 0: return w->caret;
        case 1: return line;
        case 2: return col;
        case 3: return edit_sel( w, &a, &b ) ? b - a : 0;
        case 4: return edit_nlines( w );
        default: return -1;
    }
}

// -- colors and syntax highlighting (UI COLOR, UI SYNTAX, UI SYNTAXCOLOR, UI HILITE) ----------------------------------------------------------------------
// Colors are palette entries of the UI's layer; entry 0 is transparent in every layer (a widget painted with it would show the layers behind it, and it is the
// mouse pointer's background), so it is refused; -1 means the theme's color.

static bool color_ok( int c ) {
    return c == -1 || c == -2 || ( c >= 1 && c <= 255 );
}

bool ui_set_color( ui_t* ui, int id, int fg, int bg ) {
    ui_widget_t* w = ui_find( ui, id );
    if ( w == 0 || !color_ok( fg ) || !color_ok( bg ) ) {
        return false;
    }
    if ( fg != -2 ) {
        w->fg = fg == -1 ? 0 : fg;
    }
    if ( bg != -2 ) {
        w->bg = bg == -1 ? 0 : bg;
    }
    dirty( w );
    return true;
}

int ui_get_color( ui_t* ui, int id, int which ) {
    ui_widget_t* w = ui_find( ui, id );
    if ( w == 0 ) {
        return -1;
    }
    int c = which == 0 ? w->fg : w->bg;
    return c == 0 ? -1 : c;
}

bool ui_set_syntax( ui_t* ui, int id, int mode ) {
    ui_widget_t* w = ui_find( ui, id );
    if ( w == 0 || w->kind != UIK_EDIT || w->edit == 0 || mode < 0 || mode > 1 ) {
        return false;
    }
    w->syntax = mode;
    free( w->edit->hil );
    w->edit->hil = 0;
    w->edit->hillen = 0;
    dirty( w );
    return true;
}

int ui_get_syntax( ui_t* ui, int id ) {
    ui_widget_t* w = ui_find( ui, id );
    return w == 0 || w->kind != UIK_EDIT ? -1 : w->syntax;
}

bool ui_syntax_color( ui_t* ui, int slot, int entry ) {
    if ( slot < 0 || slot >= UI_SYN_COLORS || entry < 1 || entry > 255 ) {
        return false;
    }
    ui->syncolor[slot] = entry;
    for ( int i = 0; i < ui->count; ++i ) {
        if ( ui->widgets[i].kind == UIK_EDIT && ui->widgets[i].syntax != 0 ) {
            dirty( &ui->widgets[i] );
        }
    }
    return true;
}

bool ui_set_hilite( ui_t* ui, int id, const uint8_t* classes, size_t n ) {
    ui_widget_t* w = ui_find( ui, id );
    if ( w == 0 || w->kind != UIK_EDIT || w->edit == 0 ) {
        return false;
    }
    uint8_t* h = 0;
    size_t len = (size_t) edit_len( w );
    if ( n > len ) {
        n = len;
    }
    h = (uint8_t*) calloc( len + 1U, 1U );
    if ( h == 0 ) {
        return false;
    }
    for ( size_t i = 0; i < n; ++i ) {
        h[i] = (uint8_t)( classes[i] & 15U );
    }
    free( w->edit->hil );
    w->edit->hil = h;
    w->edit->hillen = (int) len;
    w->syntax = 2;
    dirty( w );
    return true;
}

static void edit_draw( ui_t* ui, ui_widget_t* w, bool focus, bool on ) {
    const ui_theme_t* t = &ui->theme;
    int x1 = w->x, y1 = w->y, x2 = w->x + w->w - 1, y2 = w->y + w->h - 1;
    cbox( ui, x1, y1, x2, y2, w->bg ? w->bg : t->field, true );
    bevel( ui, x1, y1, x2, y2, false );
    if ( w->edit == 0 ) {
        return;
    }
    int rows = edit_rows( w ), nl = edit_nlines( w ), tw = edit_textw( w ), a = 0, b = 0;
    bool hassel = edit_sel( w, &a, &b );
    int fgc = w->fg ? w->fg : t->text;
    int tc = on ? fgc : t->disabled;
    int tx0 = x1 + EDIT_PAD, ty0 = y1 + EDIT_PAD;
    sbgeom_t sg;
    bool hasbar = sb_params( w, &sg );
    set_clip( ui, tx0, y1 + 2, tx0 + tw - 1, y2 - 2 );
    int hs = w->edit->hscroll, c0 = hs / EDIT_CW, maxcols = tw / EDIT_CW + 2;
    for ( int r = 0; r < rows && w->scroll + r < nl; ++r ) {
        int ls = edit_linestart( w, w->scroll + r ), le = edit_lineend( w, ls );
        int y = ty0 + r * FONT_HEIGHT;
        int from = ls + c0 > le ? le : ls + c0, to = from + maxcols > le ? le : from + maxcols;
        int px = tx0 - ( hs - c0 * EDIT_CW );
        // the selected part: its background first
        int sa = hassel && a < le + 1 && b > ls ? ( a > from ? a : from ) : -1, sb = hassel && sa >= 0 ? ( b < to ? b : to ) : -1;
        if ( sa >= 0 && sb > sa ) {
            cbox( ui, px + ( sa - from ) * EDIT_CW, y, px + ( sb - from ) * EDIT_CW - 1, y + FONT_HEIGHT - 1, t->hilite, true );
        }
        if ( hassel && a <= le && b > le && le >= from && le - from <= maxcols ) {      // the selection runs over the end of the line: a selected blank marks it
            cbox( ui, px + ( le - from ) * EDIT_CW, y, px + ( le - from + 1 ) * EDIT_CW - 1, y + FONT_HEIGHT - 1, t->hilite, true );
        }
        // the syntax class of every byte of the line (BASIC: the lexer of the tokenizer; 2: what the program gave)
        uint8_t* cls = 0;
        if ( on && w->syntax == 1 && le > ls ) {
            cls = (uint8_t*) malloc( (size_t)( le - ls ) );
            if ( cls != 0 ) {
                synhl_line( w->text + ls, (size_t)( le - ls ), cls );
            }
        }
        // the text in runs of one color
        int i = from;
        while ( i < to ) {
            bool sel = sa >= 0 && i >= sa && i < sb;
            int slot = 0;
            if ( on && !sel ) {
                if ( cls != 0 ) {
                    slot = cls[ i - ls ];
                } else if ( w->syntax == 2 && w->edit->hil != 0 && i < w->edit->hillen ) {
                    slot = w->edit->hil[i];
                }
            }
            int col = sel ? t->hilitetext : slot > 0 && slot < UI_SYN_COLORS ? ui->syncolor[slot] : tc;
            int j = i + 1;
            while ( j < to ) {
                bool sel2 = sa >= 0 && j >= sa && j < sb;
                int slot2 = 0;
                if ( on && !sel2 ) {
                    if ( cls != 0 ) {
                        slot2 = cls[ j - ls ];
                    } else if ( w->syntax == 2 && w->edit->hil != 0 && j < w->edit->hillen ) {
                        slot2 = w->edit->hil[j];
                    }
                }
                if ( sel2 != sel || ( !sel && slot2 != slot ) ) {
                    break;
                }
                ++j;
            }
            draw_text( ui, FONT_FIXED, px + ( i - from ) * EDIT_CW, y, w->text + i, (size_t)( j - i ), col );
            i = j;
        }
        free( cls );
    }
    if ( focus ) {
        int col = 0, line = edit_lineof( w, w->caret, &col );
        if ( line >= w->scroll && line < w->scroll + rows ) {
            int cx = tx0 + col * EDIT_CW - hs, cy = ty0 + ( line - w->scroll ) * FONT_HEIGHT;
            if ( ui->blink_on ) {                           // a block the width of a character (the fixed font), the character under it inverted
                cbox( ui, cx, cy, cx + EDIT_CW - 1, cy + FONT_HEIGHT - 1, fgc, true );
                if ( w->caret < edit_len( w ) && w->text[ w->caret ] != '\n' ) {
                    draw_text( ui, FONT_FIXED, cx, cy, w->text + w->caret, 1U, w->bg ? w->bg : t->field );
                }
            }
        }
    }
    if ( hasbar ) {
        set_clip( ui, sg.x1, sg.y1, sg.x2, sg.y2 );
        draw_scrollbar( ui, &sg, on, ui->capture == w->id ? ui->cappart : UISB_NONE );
    }
}

// -- drawing the widgets --------------------------------------------------------------------------------------------------------------------------------

// is the window at this index the one in front (the last window of the list)?
static bool is_top_window( const ui_t* ui, int index ) {
    for ( int i = ui->count - 1; i >= 0; --i ) {
        if ( ( ui->widgets[i].flags & UIF_WINDOW ) && ( ui->widgets[i].flags & UIF_VISIBLE ) ) {
            return i == index;
        }
    }
    return false;
}

static void draw_widget( ui_t* ui, int index ) {
    ui_widget_t* w = &ui->widgets[index];
    const ui_theme_t* t = &ui->theme;
    int x1 = w->x, y1 = w->y, x2 = w->x + w->w - 1, y2 = w->y + w->h - 1;
    bool on = ( w->flags & UIF_ENABLED ) != 0;
    bool focus = ui->focus == w->id && on;
    bool pressed = ui->capture == w->id;
    int fgc = w->fg ? w->fg : t->text, facec = w->bg ? w->bg : t->face, fieldc = w->bg ? w->bg : t->field;
    int tc = on ? fgc : t->disabled;
    size_t len = w->text ? strlen( w->text ) : 0;
    set_clip( ui, x1, y1, x2, y2 );
    switch ( w->kind ) {
        case UIK_LABEL:
            if ( w->bg ) {
                cbox( ui, x1, y1, x2, y2, w->bg, true );
            }
            draw_text( ui, t->font, x1, y1 + ( w->h - FONT_HEIGHT ) / 2, w->text ? w->text : "", len, tc );
            break;
        case UIK_PANEL:
            cbox( ui, x1, y1, x2, y2, facec, true );
            bevel( ui, x1, y1, x2, y2, true );
            if ( len > 0 ) {
                bool win = ( w->flags & UIF_WINDOW ) != 0;
                cbox( ui, x1 + 3, y1 + 3, x2 - 3, y1 + 18, win && !is_top_window( ui, index ) ? t->shadow : t->title, true );
                draw_text( ui, t->boldfont, x1 + 8, y1 + 4, w->text, len, t->titletext );
                if ( win ) {                                // the close box: an X on a small raised button
                    int cx1 = x2 - 17, cy1 = y1 + 5;
                    cbox( ui, cx1, cy1, cx1 + 12, cy1 + 12, t->face, true );
                    bevel( ui, cx1, cy1, cx1 + 12, cy1 + 12, true );
                    for ( int k = 0; k < 7; ++k ) {
                        cpset( ui, cx1 + 3 + k, cy1 + 3 + k, t->text );
                        cpset( ui, cx1 + 9 - k, cy1 + 3 + k, t->text );
                    }
                }
            }
            break;
        case UIK_BUTTON: {
            cbox( ui, x1, y1, x2, y2, facec, true );
            bevel( ui, x1, y1, x2, y2, !pressed );
            int tw = text_width( ui, t->boldfont, w->text ? w->text : "", len );
            int off = pressed ? 1 : 0;
            draw_text( ui, t->boldfont, x1 + ( w->w - tw ) / 2 + off, y1 + ( w->h - FONT_HEIGHT ) / 2 + off, w->text ? w->text : "", len, tc );
            if ( focus ) {
                dotted( ui, x1 + 3, y1 + 3, x2 - 3, y2 - 3, t->border );
            }
            break;
        }
        case UIK_CHECK: {
            if ( w->bg ) {
                cbox( ui, x1, y1, x2, y2, w->bg, true );
            }
            int by = y1 + ( w->h - CHECKSZ ) / 2;
            cbox( ui, x1, by, x1 + CHECKSZ - 1, by + CHECKSZ - 1, t->field, true );
            bevel( ui, x1, by, x1 + CHECKSZ - 1, by + CHECKSZ - 1, false );
            if ( w->value ) {
                // a tick: two strokes, 2 pixels thick
                static const int tick[][2] = { {3,6},{4,7},{5,8},{6,7},{7,6},{8,5},{9,4},{3,7},{4,8},{5,9},{6,8},{7,7},{8,6},{9,5} };
                for ( size_t i = 0; i < sizeof(tick) / sizeof(tick[0]); ++i ) {
                    cpset( ui, x1 + tick[i][0], by + tick[i][1] - 1, tc );
                }
            }
            draw_text( ui, t->font, x1 + CHECKSZ + 6, y1 + ( w->h - FONT_HEIGHT ) / 2, w->text ? w->text : "", len, tc );
            if ( focus ) {
                dotted( ui, x1 + CHECKSZ + 4, y1 + ( w->h - 14 ) / 2, x1 + CHECKSZ + 8 + text_width( ui, t->font, w->text ? w->text : "", len ), y1 + ( w->h - 14 ) / 2 + 13, t->border );
            }
            break;
        }
        case UIK_RADIO: {
            if ( w->bg ) {
                cbox( ui, x1, y1, x2, y2, w->bg, true );
            }
            int cy = y1 + w->h / 2;
            disc( ui, x1 + 6, cy, 6, t->border );
            disc( ui, x1 + 6, cy, 5, t->field );
            if ( w->value ) {
                disc( ui, x1 + 6, cy, 2, tc );
            }
            draw_text( ui, t->font, x1 + 19, y1 + ( w->h - FONT_HEIGHT ) / 2, w->text ? w->text : "", len, tc );
            if ( focus ) {
                dotted( ui, x1 + 17, y1 + ( w->h - 14 ) / 2, x1 + 21 + text_width( ui, t->font, w->text ? w->text : "", len ), y1 + ( w->h - 14 ) / 2 + 13, t->border );
            }
            break;
        }
        case UIK_TEXT: {
            cbox( ui, x1, y1, x2, y2, fieldc, true );
            bevel( ui, x1, y1, x2, y2, false );
            set_clip( ui, x1 + 2, y1 + 2, x2 - 2, y2 - 2 );
            int tx = x1 + 4 - w->scroll, ty = y1 + ( w->h - FONT_HEIGHT ) / 2;
            draw_text( ui, t->font, tx, ty, w->text ? w->text : "", len, tc );
            if ( focus ) {
                int cx = tx + text_width( ui, t->font, w->text ? w->text : "", (size_t) w->caret );
                cbox( ui, cx, ty, cx, ty + FONT_HEIGHT - 1, fgc, true );
            }
            break;
        }
        case UIK_EDIT:
            edit_draw( ui, w, focus, on );
            break;
        case UIK_LIST: {
            cbox( ui, x1, y1, x2, y2, fieldc, true );
            bevel( ui, x1, y1, x2, y2, false );
            sbgeom_t sg;
            bool hasbar = sb_params( w, &sg );
            set_clip( ui, x1 + 2, y1 + 2, hasbar ? sg.x1 - 1 : x2 - 2, y2 - 2 );
            int rows = list_rows( w );
            for ( int r = 0; r < rows && w->scroll + r < w->nitems; ++r ) {
                int row = w->scroll + r, ry = y1 + 2 + r * ROWH;
                bool sel = row == w->value;
                if ( sel ) {
                    cbox( ui, x1 + 2, ry, x2 - 2, ry + ROWH - 1, t->hilite, true );
                }
                const char* s = w->items[row];
                draw_text( ui, t->font, x1 + 5, ry + 1, s, strlen( s ), sel ? t->hilitetext : tc );
                if ( sel && focus ) {
                    dotted( ui, x1 + 2, ry, x2 - 2, ry + ROWH - 1, t->hilitetext );
                }
            }
            if ( hasbar ) {
                set_clip( ui, sg.x1, sg.y1, sg.x2, sg.y2 );
                draw_scrollbar( ui, &sg, on, ui->capture == w->id ? ui->cappart : UISB_NONE );
            }
            break;
        }
        case UIK_SCROLL: {
            sbgeom_t sg;
            if ( sb_params( w, &sg ) ) {
                draw_scrollbar( ui, &sg, on, pressed ? ui->cappart : UISB_NONE );
                if ( focus ) {
                    dotted( ui, sg.x1 + 2, sg.y1 + 2, sg.x2 - 2, sg.y2 - 2, t->border );
                }
            }
            break;
        }
        case UIK_CANVAS: {
            cbox( ui, x1, y1, x2, y2, t->shadow, true );
            bevel( ui, x1, y1, x2, y2, false );
            int z, ox, oy;
            if ( w->pix != 0 && canvas_geom( w, &z, &ox, &oy ) ) {
                int gap = z >= 4 ? 1 : 0;
                for ( int cy = 0; cy < w->ch; ++cy ) {
                    for ( int cx = 0; cx < w->cw; ++cx ) {
                        int c = w->pix[ cy * w->cw + cx ];
                        int px = ox + cx * z, py = oy + cy * z;
                        cbox( ui, px, py, px + z - 1 - gap, py + z - 1 - gap, c != 0 ? c : ( ( cx + cy ) & 1 ) ? t->light : t->face, true );
                    }
                }
            }
            break;
        }
        case UIK_PROGRESS: {
            cbox( ui, x1, y1, x2, y2, t->field, true );
            bevel( ui, x1, y1, x2, y2, false );
            int range = w->max - w->min, inner = w->w - 4;
            if ( range > 0 && w->value > w->min ) {
                int fill = (int)( (long)( w->value - w->min ) * inner / range );
                cbox( ui, x1 + 2, y1 + 2, x1 + 1 + fill, y2 - 2, t->fill, true );
            }
            break;
        }
        case UIK_SLIDER: {
            int cy = y1 + w->h / 2;
            cbox( ui, x1 + 2, cy - 2, x2 - 2, cy + 1, t->field, true );
            bevel( ui, x1 + 2, cy - 2, x2 - 2, cy + 1, false );
            int range = w->max - w->min, travel = w->w - THUMBW;
            int tx = x1 + ( range > 0 ? (int)( (long)( w->value - w->min ) * travel / range ) : 0 );
            cbox( ui, tx, y1 + 1, tx + THUMBW - 1, y2 - 1, t->face, true );
            bevel( ui, tx, y1 + 1, tx + THUMBW - 1, y2 - 1, !pressed );
            if ( focus ) {
                dotted( ui, tx + 2, y1 + 3, tx + THUMBW - 3, y2 - 3, t->border );
            }
            break;
        }
        default:
            break;
    }
}

static void clear_damage( ui_t* ui ) {
    ui->deskdmg.n = 0;
    for ( int i = 0; i < ui->count; ++i ) {
        ui->widgets[i].dmg.n = 0;
        ui->widgets[i].flags &= ~UIF_DIRTY;
    }
}

// the place of a widget changed (it went away, moved, was hidden): the desk behind it, and everything it overlapped, has to be drawn again
static void reveal( ui_t* ui, int skip, int x1, int y1, int x2, int y2 ) {
    dmg_add( &ui->deskdmg, x1, y1, x2, y2 );
    for ( int j = 0; j < ui->count; ++j ) {
        if ( j != skip && ( ui->widgets[j].flags & UIF_VISIBLE ) ) {
            dmg_add_overlap( &ui->widgets[j], x1, y1, x2, y2 );
        }
    }
}

// a widget (or a group) was put over the rectangle: what is under it in other groups is overwritten, which goes into its damage list as well (the obscured part)
static void obscure( ui_t* ui, int top, int x1, int y1, int x2, int y2 ) {
    int gt = group_of( ui, top );
    for ( int j = 0; j < top; ++j ) {
        if ( ( ui->widgets[j].flags & UIF_VISIBLE ) && group_of( ui, j ) != gt ) {
            dmg_add_overlap( &ui->widgets[j], x1, y1, x2, y2 );
        }
    }
}

void ui_draw( ui_t* ui, bool all ) {
    if ( !ui->drawn ) {
        all = true;
    }
    ui->dmgon = false;
    if ( all ) {
        set_clip( ui, 0, 0, ui->canvas.w - 1, ui->canvas.h - 1 );
        cbox( ui, 0, 0, ui->canvas.w - 1, ui->canvas.h - 1, ui->theme.desk, true );
        for ( int i = 0; i < ui->count; ++i ) {
            if ( ui->widgets[i].flags & UIF_VISIBLE ) {
                draw_widget( ui, i );
            }
        }
        clear_damage( ui );
        int grp[UI_MAXWIDGETS];
        for ( int i = 0; i < ui->count; ++i ) {
            grp[i] = group_of( ui, i );
        }
        for ( int i = 0; i < ui->count; ++i ) {             // what a widget in front of another group hides is overwritten: it goes on the damage list
            ui_widget_t* w = &ui->widgets[i];
            rgn_t vis, hid;
            if ( ( w->flags & UIF_VISIBLE ) && vis_region( ui, grp, i, &vis ) ) {
                rgn_set( &hid, w->x, w->y, w->x + w->w - 1, w->y + w->h - 1 );
                for ( int v = 0; v < vis.n; ++v ) {
                    rgn_sub( &hid, vis.r[v][0], vis.r[v][1], vis.r[v][2], vis.r[v][3] );
                }
                for ( int k = 0; k < hid.n; ++k ) {
                    dmg_add( &w->dmg, hid.r[k][0], hid.r[k][1], hid.r[k][2], hid.r[k][3] );
                }
            }
        }
        ui->drawn = true;
        return;
    }
    int grp[UI_MAXWIDGETS];
    for ( int i = 0; i < ui->count; ++i ) {
        grp[i] = group_of( ui, i );
    }
    // 1. changed widgets: a visible one has to be drawn all over, one that went away gives its place back
    for ( int i = 0; i < ui->count; ++i ) {
        ui_widget_t* w = &ui->widgets[i];
        if ( !( w->flags & UIF_DIRTY ) ) {
            continue;
        }
        w->flags &= ~UIF_DIRTY;
        if ( w->flags & UIF_VISIBLE ) {
            dmg_add( &w->dmg, w->x, w->y, w->x + w->w - 1, w->y + w->h - 1 );
        } else {
            w->dmg.n = 0;
            reveal( ui, i, w->x, w->y, w->x + w->w - 1, w->y + w->h - 1 );
        }
    }
    // 2. the desk: what is damaged and not covered by a widget
    if ( ui->deskdmg.n > 0 ) {
        rgn_t g;
        g.n = 0;
        g.over = false;
        for ( int k = 0; k < ui->deskdmg.n; ++k ) {
            rgn_push( &g, ui->deskdmg.r[k][0], ui->deskdmg.r[k][1], ui->deskdmg.r[k][2], ui->deskdmg.r[k][3] );
        }
        ui->deskdmg.n = 0;
        for ( int j = 0; j < ui->count && g.n > 0; ++j ) {
            const ui_widget_t* o = &ui->widgets[j];
            if ( o->flags & UIF_VISIBLE ) {
                rgn_sub( &g, o->x, o->y, o->x + o->w - 1, o->y + o->h - 1 );
            }
        }
        if ( g.over ) {
            ui_draw( ui, true );
            return;
        }
        for ( int k = 0; k < g.n; ++k ) {
            set_clip( ui, g.r[k][0], g.r[k][1], g.r[k][2], g.r[k][3] );
            cbox( ui, g.r[k][0], g.r[k][1], g.r[k][2], g.r[k][3], ui->theme.desk, true );
        }
    }
    // 3. the widgets from the back to the front: the damaged part that is visible is drawn, the rest stays on the damage list until it is revealed
    for ( int i = 0; i < ui->count; ++i ) {
        ui_widget_t* w = &ui->widgets[i];
        if ( w->dmg.n == 0 ) {
            continue;
        }
        if ( !( w->flags & UIF_VISIBLE ) ) {
            w->dmg.n = 0;
            continue;
        }
        rgn_t vis;
        if ( !vis_region( ui, grp, i, &vis ) ) {
            ui_draw( ui, true );
            return;
        }
        rgn_t todo;                                         // damage inside the visible part
        todo.n = 0;
        todo.over = false;
        for ( int k = 0; k < w->dmg.n; ++k ) {
            for ( int v = 0; v < vis.n; ++v ) {
                int x1 = w->dmg.r[k][0] > vis.r[v][0] ? w->dmg.r[k][0] : vis.r[v][0], y1 = w->dmg.r[k][1] > vis.r[v][1] ? w->dmg.r[k][1] : vis.r[v][1];
                int x2 = w->dmg.r[k][2] < vis.r[v][2] ? w->dmg.r[k][2] : vis.r[v][2], y2 = w->dmg.r[k][3] < vis.r[v][3] ? w->dmg.r[k][3] : vis.r[v][3];
                rgn_push( &todo, x1, y1, x2, y2 );
            }
        }
        // what stays: the damage outside the visible part (obscured)
        rgn_t keep;
        keep.n = 0;
        keep.over = false;
        for ( int k = 0; k < w->dmg.n; ++k ) {
            rgn_push( &keep, w->dmg.r[k][0], w->dmg.r[k][1], w->dmg.r[k][2], w->dmg.r[k][3] );
        }
        for ( int v = 0; v < vis.n; ++v ) {
            rgn_sub( &keep, vis.r[v][0], vis.r[v][1], vis.r[v][2], vis.r[v][3] );
        }
        w->dmg.n = 0;
        for ( int k = 0; k < keep.n; ++k ) {
            dmg_add( &w->dmg, keep.r[k][0], keep.r[k][1], keep.r[k][2], keep.r[k][3] );
        }
        if ( todo.over ) {
            ui_draw( ui, true );
            return;
        }
        for ( int k = 0; k < todo.n; ++k ) {
            int x1 = todo.r[k][0], y1 = todo.r[k][1], x2 = todo.r[k][2], y2 = todo.r[k][3];
            ui->dmg[0] = x1; ui->dmg[1] = y1; ui->dmg[2] = x2; ui->dmg[3] = y2;
            ui->dmgon = true;
            set_clip( ui, x1, y1, x2, y2 );
            cbox( ui, x1, y1, x2, y2, background( ui, i ), true );
            draw_widget( ui, i );
            ui->dmgon = false;
            for ( int j = i + 1; j < ui->count; ++j ) {     // what was drawn over: the widgets in front of it in the same group are drawn again
                if ( grp[j] == grp[i] && ( ui->widgets[j].flags & UIF_VISIBLE ) ) {
                    dmg_add_overlap( &ui->widgets[j], x1, y1, x2, y2 );
                }
            }
        }
    }
}

// -- the widgets ------------------------------------------------------------------------------------------------------------------------------------------

void ui_init( ui_t* ui, const ui_canvas_t* canvas, int palbase ) {
    memset( ui, 0, sizeof(*ui) );
    ui->canvas = *canvas;
    ui_theme_default( &ui->theme, palbase );
    ui->palbase = palbase;
    ui->blink_on = ui->blink_drawn = true;
    for ( int i = 0; i < UI_SYN_COLORS; ++i ) {
        ui->syncolor[i] = palbase + UI_PAL_COLORS + i;
    }
    ui->lastclick_row = -1;
}

static void edit_free( ui_widget_t* w );

static void free_widget( ui_widget_t* w ) {
    edit_free( w );
    free( w->text );
    free( w->pix );
    for ( int i = 0; i < w->nitems; ++i ) {
        free( w->items[i] );
    }
    free( w->items );
    memset( w, 0, sizeof(*w) );
}

void ui_clear( ui_t* ui ) {
    for ( int i = 0; i < ui->count; ++i ) {
        free_widget( &ui->widgets[i] );
    }
    ui->count = 0;
    ui->focus = ui->modal = ui->popup = ui->capture = 0;
    ui->qhead = ui->qcount = 0;
    ui->drawn = false;
}

void ui_free( ui_t* ui ) {
    ui_clear( ui );
}

// a copy of at most max bytes; the block has room for at least cap bytes (a text field is edited in place and needs the room)
static char* dupn( const char* s, size_t max, size_t cap ) {
    size_t n = s ? strlen( s ) : 0;
    if ( n > max ) {
        n = max;
    }
    char* r = malloc( ( n + 1U > cap ? n + 1U : cap ) );
    if ( r != 0 ) {
        memcpy( r, s ? s : "", n );
        r[n] = '\0';
    }
    return r;
}

static bool edit_init( ui_widget_t* w );

bool ui_add( ui_t* ui, int kind, int id, int x, int y, int w, int h, const char* text, int extra ) {
    if ( kind <= UIK_NONE || kind >= UIK_COUNT || id <= 0 || ui->count >= UI_MAXWIDGETS || index_of( ui, id ) >= 0 || w < 1 || h < 1 ) {
        return false;
    }
    ui_widget_t* wd = &ui->widgets[ ui->count ];
    memset( wd, 0, sizeof(*wd) );
    wd->text = dupn( text, kind == UIK_EDIT ? UI_EDITMAX : UI_TEXTMAX, kind == UIK_TEXT ? UI_TEXTMAX + 1U : 0U );
    if ( wd->text == 0 ) {
        return false;
    }
    wd->id = id;
    wd->kind = kind;
    wd->x = x;
    wd->y = y;
    wd->w = w;
    wd->h = h;
    wd->flags = UIF_VISIBLE | UIF_ENABLED | UIF_DIRTY;
    wd->value = 0;
    wd->min = 0;
    wd->max = 100;
    if ( kind == UIK_PANEL && extra == 1 ) {
        wd->flags |= UIF_WINDOW;
        for ( int i = ui->count - 1; i >= 0; --i ) {         // the window that was in front loses the blue title bar
            if ( ui->widgets[i].flags & UIF_WINDOW ) {
                dirty( &ui->widgets[i] );
                break;
            }
        }
    } else if ( kind != UIK_PANEL ) {                     // a widget created inside a window belongs to the topmost one
        for ( int i = ui->count - 1; i >= 0; --i ) {
            if ( ( ui->widgets[i].flags & UIF_WINDOW ) && inside( wd, &ui->widgets[i] ) ) {
                wd->owner = ui->widgets[i].id;
                break;
            }
        }
    }
    if ( kind == UIK_RADIO ) {
        wd->group = extra;
    }
    if ( kind == UIK_SLIDER ) {
        wd->min = extra;
        wd->value = extra;
        if ( wd->max < wd->min ) {
            wd->max = wd->min;
        }
    }
    if ( kind == UIK_LIST ) {
        wd->value = -1;
    }
    if ( kind == UIK_SCROLL ) {
        wd->min = 0;
        wd->max = 100;
        wd->page = extra > 0 ? extra : 10;
    }
    if ( kind == UIK_TEXT ) {
        wd->caret = (int) strlen( wd->text );
    }
    if ( kind == UIK_EDIT && !edit_init( wd ) ) {
        free( wd->text );
        memset( wd, 0, sizeof(*wd) );
        return false;
    }
    ++ui->count;
    obscure( ui, ui->count - 1, x, y, x + w - 1, y + h - 1 );
    if ( ui->focus == 0 && focusable( wd ) ) {
        ui->focus = id;
    }
    return true;
}

bool ui_set_canvas( ui_t* ui, int id, int cw, int ch ) {
    ui_widget_t* w = ui_find( ui, id );
    if ( w == 0 || w->kind != UIK_CANVAS || cw < 1 || ch < 1 || cw > 256 || ch > 256 ) {
        return false;
    }
    uint8_t* p = (uint8_t*) calloc( (size_t) cw * (size_t) ch, 1U );
    if ( p == 0 ) {
        return false;
    }
    free( w->pix );
    w->pix = p;
    w->cw = cw;
    w->ch = ch;
    dirty( w );
    return true;
}

bool ui_canvas_set( ui_t* ui, int id, int x, int y, int c ) {
    ui_widget_t* w = ui_find( ui, id );
    if ( w == 0 || w->kind != UIK_CANVAS || w->pix == 0 || x < 0 || y < 0 || x >= w->cw || y >= w->ch || c < 0 || c > 255 ) {
        return false;
    }
    if ( w->pix[ y * w->cw + x ] != (uint8_t) c ) {
        w->pix[ y * w->cw + x ] = (uint8_t) c;
        dirty( w );
    }
    return true;
}

int ui_canvas_get( ui_t* ui, int id, int x, int y ) {
    ui_widget_t* w = ui_find( ui, id );
    if ( w == 0 || w->kind != UIK_CANVAS || w->pix == 0 || x < 0 || y < 0 || x >= w->cw || y >= w->ch ) {
        return -1;
    }
    return w->pix[ y * w->cw + x ];
}

bool ui_canvas_load( ui_t* ui, int id, const uint8_t* data, size_t n ) {
    ui_widget_t* w = ui_find( ui, id );
    if ( w == 0 || w->kind != UIK_CANVAS || w->pix == 0 ) {
        return false;
    }
    size_t cells = (size_t) w->cw * (size_t) w->ch;
    memset( w->pix, 0, cells );
    memcpy( w->pix, data, n < cells ? n : cells );
    dirty( w );
    return true;
}

bool ui_canvas_save( ui_t* ui, int id, uint8_t* out, size_t cap, size_t* n ) {
    ui_widget_t* w = ui_find( ui, id );
    if ( w == 0 || w->kind != UIK_CANVAS || w->pix == 0 ) {
        return false;
    }
    size_t cells = (size_t) w->cw * (size_t) w->ch;
    if ( cells > cap ) {
        cells = cap;
    }
    memcpy( out, w->pix, cells );
    *n = cells;
    return true;
}

bool ui_remove( ui_t* ui, int id ) {
    int i = index_of( ui, id );
    if ( i < 0 ) {
        return false;
    }
    ui_widget_t w = ui->widgets[i];
    // its place goes to the desk and to what was under it (the damage lists; ui_draw draws it)
    reveal( ui, i, w.x, w.y, w.x + w.w - 1, w.y + w.h - 1 );
    bool wastop = ( w.flags & UIF_WINDOW ) && is_top_window( ui, i );
    free_widget( &ui->widgets[i] );
    memmove( &ui->widgets[i], &ui->widgets[i + 1], (size_t)( ui->count - i - 1 ) * sizeof(ui_widget_t) );
    --ui->count;
    memset( &ui->widgets[ ui->count ], 0, sizeof(ui_widget_t) );
    if ( wastop ) {                                          // the next window in front gets the blue title bar
        for ( int j = ui->count - 1; j >= 0; --j ) {
            if ( ui->widgets[j].flags & UIF_WINDOW ) {
                dirty( &ui->widgets[j] );
                break;
            }
        }
    }
    if ( ui->focus == id ) {
        ui->focus = 0;
        for ( int j = 0; j < ui->count && ui->focus == 0; ++j ) {
            if ( focusable( &ui->widgets[j] ) ) {
                ui->focus = ui->widgets[j].id;
                dirty( &ui->widgets[j] );                   // (it shows the focus now)
            }
        }
    }
    if ( ui->modal == id ) {
        ui->modal = 0;
    }
    if ( ui->popup == id ) {
        ui->popup = 0;
    }
    if ( ui->capture == id ) {
        ui->capture = 0;
    }
    return true;
}

static void edit_reset( ui_widget_t* w );

bool ui_set_text( ui_t* ui, int id, const char* text ) {
    ui_widget_t* w = ui_find( ui, id );
    if ( w == 0 ) {
        return false;
    }
    char* t = dupn( text, w->kind == UIK_EDIT ? UI_EDITMAX : UI_TEXTMAX, w->kind == UIK_TEXT ? UI_TEXTMAX + 1U : 0U );
    if ( t == 0 ) {
        return false;
    }
    free( w->text );
    w->text = t;
    if ( w->kind == UIK_EDIT ) {
        edit_reset( w );
    }
    if ( w->kind == UIK_TEXT ) {
        w->caret = (int) strlen( t );
        w->scroll = 0;
        int cw = text_width( ui, ui->theme.font, t, strlen( t ) ), inner = w->w - 8;
        if ( cw > inner ) {
            w->scroll = cw - inner;
        }
    }
    dirty( w );
    return true;
}

const char* ui_get_text( ui_t* ui, int id ) {
    ui_widget_t* w = ui_find( ui, id );
    return w == 0 ? 0 : w->text;
}

static void radio_select( ui_t* ui, ui_widget_t* w ) {
    for ( int i = 0; i < ui->count; ++i ) {
        ui_widget_t* o = &ui->widgets[i];
        if ( o->kind == UIK_RADIO && o->group == w->group && o != w && o->value ) {
            o->value = 0;
            dirty( o );
        }
    }
    w->value = 1;
    dirty( w );
}

static void list_show_selected( ui_widget_t* w ) {
    int rows = list_rows( w );
    if ( w->value < w->scroll ) {
        w->scroll = w->value;
    }
    if ( w->value >= w->scroll + rows ) {
        w->scroll = w->value - rows + 1;
    }
    w->scroll = clampi( w->scroll, 0, w->nitems > rows ? w->nitems - rows : 0 );
}

bool ui_set_value( ui_t* ui, int id, int value ) {
    ui_widget_t* w = ui_find( ui, id );
    if ( w == 0 ) {
        return false;
    }
    switch ( w->kind ) {
        case UIK_CHECK:
            w->value = value != 0;
            break;
        case UIK_RADIO:
            if ( value != 0 ) {
                radio_select( ui, w );
            } else {
                w->value = 0;
            }
            break;
        case UIK_SLIDER: case UIK_PROGRESS: case UIK_SCROLL:
            w->value = clampi( value, w->min, w->max );
            break;
        case UIK_LIST:
            w->value = value < 0 || value >= w->nitems ? -1 : value;
            if ( w->value >= 0 ) {
                list_show_selected( w );
            }
            break;
        default:
            return false;
    }
    dirty( w );
    return true;
}

bool ui_get_value( ui_t* ui, int id, int* value ) {
    ui_widget_t* w = ui_find( ui, id );
    if ( w == 0 ) {
        return false;
    }
    *value = w->kind == UIK_TEXT || w->kind == UIK_EDIT ? (int) strlen( w->text ) : w->value;
    return true;
}

bool ui_set_range( ui_t* ui, int id, int min, int max ) {
    ui_widget_t* w = ui_find( ui, id );
    if ( w == 0 || ( w->kind != UIK_SLIDER && w->kind != UIK_PROGRESS && w->kind != UIK_SCROLL ) || max < min ) {
        return false;
    }
    w->min = min;
    w->max = max;
    w->value = clampi( w->value, min, max );
    dirty( w );
    return true;
}

bool ui_set_page( ui_t* ui, int id, int page ) {
    ui_widget_t* w = ui_find( ui, id );
    if ( w == 0 || w->kind != UIK_SCROLL || page < 1 ) {
        return false;
    }
    w->page = page;
    dirty( w );
    return true;
}

bool ui_set_enabled( ui_t* ui, int id, bool on ) {
    ui_widget_t* w = ui_find( ui, id );
    if ( w == 0 ) {
        return false;
    }
    if ( on ) {
        w->flags |= UIF_ENABLED;
    } else {
        w->flags &= ~UIF_ENABLED;
        if ( ui->focus == id ) {
            ui->focus = 0;
        }
    }
    dirty( w );
    return true;
}

bool ui_set_visible( ui_t* ui, int id, bool on ) {
    ui_widget_t* w = ui_find( ui, id );
    if ( w == 0 ) {
        return false;
    }
    if ( on ) {
        w->flags |= UIF_VISIBLE;
        obscure( ui, index_of( ui, id ), w->x, w->y, w->x + w->w - 1, w->y + w->h - 1 );
    } else {
        w->flags &= ~UIF_VISIBLE;
        if ( ui->focus == id ) {
            ui->focus = 0;
        }
    }
    dirty( w );
    return true;
}

bool ui_set_focus( ui_t* ui, int id ) {
    ui_widget_t* w = ui_find( ui, id );
    if ( w == 0 || !focusable( w ) ) {
        return false;
    }
    ui_widget_t* old = ui_find( ui, ui->focus );
    if ( old != 0 ) {
        dirty( old );
    }
    ui->focus = id;
    dirty( w );
    return true;
}

bool ui_list_add( ui_t* ui, int id, const char* item ) {
    ui_widget_t* w = ui_find( ui, id );
    if ( w == 0 || w->kind != UIK_LIST || w->nitems >= UI_MAXITEMS ) {
        return false;
    }
    if ( w->nitems >= w->nalloc ) {
        int na = w->nalloc == 0 ? 16 : w->nalloc * 2;
        char** n = realloc( w->items, (size_t) na * sizeof(char*) );
        if ( n == 0 ) {
            return false;
        }
        w->items = n;
        w->nalloc = na;
    }
    char* s = dupn( item, UI_TEXTMAX, 0U );
    if ( s == 0 ) {
        return false;
    }
    w->items[ w->nitems++ ] = s;
    dirty( w );
    return true;
}

bool ui_list_clear( ui_t* ui, int id ) {
    ui_widget_t* w = ui_find( ui, id );
    if ( w == 0 || w->kind != UIK_LIST ) {
        return false;
    }
    for ( int i = 0; i < w->nitems; ++i ) {
        free( w->items[i] );
    }
    w->nitems = 0;
    w->value = -1;
    w->scroll = 0;
    dirty( w );
    return true;
}

const char* ui_list_get( ui_t* ui, int id, int row ) {
    ui_widget_t* w = ui_find( ui, id );
    if ( w == 0 || w->kind != UIK_LIST || row < 0 || row >= w->nitems ) {
        return 0;
    }
    return w->items[row];
}

// moves the widget (and the widgets that lie inside it, in their order) to the end of the list: in front of everything else
static void raise_group( ui_t* ui, int id ) {
    int p = index_of( ui, id );
    if ( p < 0 ) {
        return;
    }
    ui_widget_t panel = ui->widgets[p];
    ui_widget_t* tmp = malloc( sizeof(ui_widget_t) * (size_t) ui->count );
    if ( tmp == 0 ) {
        return;
    }
    int n = 0;
    bool win = ( panel.flags & UIF_WINDOW ) != 0;         // a window owns the widgets created in it, any other panel the ones that lie inside it
    for ( int i = 0; i < ui->count; ++i ) {                 // the others first
        bool in = i == p || ( i > p && ( win ? ui->widgets[i].owner == id : ( ui->widgets[i].kind != UIK_PANEL && inside( &ui->widgets[i], &panel ) ) ) );
        if ( !in ) {
            tmp[n++] = ui->widgets[i];
        }
    }
    for ( int i = 0; i < ui->count; ++i ) {                 // then the panel and what is in it
        bool in = i == p || ( i > p && ( win ? ui->widgets[i].owner == id : ( ui->widgets[i].kind != UIK_PANEL && inside( &ui->widgets[i], &panel ) ) ) );
        if ( in ) {
            tmp[n++] = ui->widgets[i];
        }
    }
    memcpy( ui->widgets, tmp, sizeof(ui_widget_t) * (size_t) ui->count );
    free( tmp );
    int first = ui->count;                                  // the group is the tail of the list: find where it starts
    while ( first > 0 && ( ui->widgets[ first - 1 ].id == id || ( win ? ui->widgets[ first - 1 ].owner == id : inside( &ui->widgets[ first - 1 ], &panel ) && ui->widgets[ first - 1 ].kind != UIK_PANEL ) ) ) {
        --first;
    }
    for ( int i = first; i < ui->count; ++i ) {              // the group is drawn again, and what it covers goes onto the damage lists of the widgets under it
        dirty( &ui->widgets[i] );
        obscure( ui, i, ui->widgets[i].x, ui->widgets[i].y, ui->widgets[i].x + ui->widgets[i].w - 1, ui->widgets[i].y + ui->widgets[i].h - 1 );
    }
    if ( win ) {                                            // the window that was in front loses its blue title bar
        for ( int i = first - 1; i >= 0; --i ) {
            if ( ui->widgets[i].flags & UIF_WINDOW ) {
                dirty( &ui->widgets[i] );
                break;
            }
        }
    }
}

bool ui_set_modal( ui_t* ui, int panelid ) {
    if ( panelid == 0 ) {
        ui->modal = 0;
        return true;
    }
    ui_widget_t* w = ui_find( ui, panelid );
    if ( w == 0 || w->kind != UIK_PANEL ) {
        return false;
    }
    raise_group( ui, panelid );
    ui->modal = panelid;
    // the focus goes into the panel
    ui_widget_t* m = ui_find( ui, panelid );
    for ( int i = 0; i < ui->count; ++i ) {
        if ( &ui->widgets[i] != m && inside( &ui->widgets[i], m ) && focusable( &ui->widgets[i] ) ) {
            ui_set_focus( ui, ui->widgets[i].id );
            break;
        }
    }
    return true;
}

bool ui_raise( ui_t* ui, int id ) {
    ui_widget_t* w = ui_find( ui, id );
    if ( w == 0 || w->kind != UIK_PANEL ) {
        return false;
    }
    int idx = index_of( ui, id );
    bool top = true;                                        // already in front: the window is last, with the widgets it owns behind it
    for ( int i = idx + 1; i < ui->count; ++i ) {
        if ( ui->widgets[i].owner != id ) {
            top = false;
        }
    }
    if ( !top ) {
        raise_group( ui, id );
    }
    return true;
}

bool ui_move( ui_t* ui, int id, int x, int y ) {
    ui_widget_t* w = ui_find( ui, id );
    if ( w == 0 ) {
        return false;
    }
    int dx = x - w->x, dy = y - w->y;
    if ( dx == 0 && dy == 0 ) {
        return true;
    }
    int idx = index_of( ui, id );
    bool win = ( w->flags & UIF_WINDOW ) != 0;
    reveal( ui, -1, w->x, w->y, w->x + w->w - 1, w->y + w->h - 1 );      // the old place: the desk and what was under it
    for ( int i = 0; i < ui->count; ++i ) {
        ui_widget_t* m = &ui->widgets[i];
        if ( i == idx || ( win && m->owner == id ) ) {
            m->x += dx;
            m->y += dy;
            m->dmg.n = 0;                                   // (the damage was in the old place; the moved widget is drawn all over)
            dirty( m );
        }
    }
    for ( int i = 0; i < ui->count; ++i ) {                 // the new place: what is under it in other groups is overwritten
        ui_widget_t* m = &ui->widgets[i];
        if ( i == idx || ( win && m->owner == id ) ) {
            obscure( ui, i, m->x, m->y, m->x + m->w - 1, m->y + m->h - 1 );
        }
    }
    return true;
}

bool ui_popup( ui_t* ui, int listid, int x, int y ) {
    ui_widget_t* w = ui_find( ui, listid );
    if ( w == 0 || w->kind != UIK_LIST ) {
        return false;
    }
    w->flags |= UIF_POPUP;
    w->x = x;
    w->y = y;
    ui_set_visible( ui, listid, true );
    raise_group( ui, listid );                                // (a list has nothing inside: it only moves itself)
    ui->prevfocus = ui->focus;
    ui->popup = listid;
    ui_set_focus( ui, listid );
    return true;
}

// -- the input --------------------------------------------------------------------------------------------------------------------------------------------

static void close_popup( ui_t* ui ) {
    int id = ui->popup;
    ui->popup = 0;
    ui_set_visible( ui, id, false );
    if ( ui->prevfocus != 0 ) {
        ui_set_focus( ui, ui->prevfocus );
    }
    ui->prevfocus = 0;
}

static void text_show_caret( ui_t* ui, ui_widget_t* w ) {
    int cx = text_width( ui, ui->theme.font, w->text, (size_t) w->caret ), inner = w->w - 8;
    if ( cx - w->scroll > inner ) {
        w->scroll = cx - inner;
    }
    if ( cx < w->scroll ) {
        w->scroll = cx;
    }
    if ( w->scroll < 0 ) {
        w->scroll = 0;
    }
}

static int text_pos_at( ui_t* ui, ui_widget_t* w, int mx ) {
    int target = mx - ( w->x + 4 ) + w->scroll, acc = 0;
    size_t n = strlen( w->text );
    for ( size_t i = 0; i < n; ++i ) {
        int gw = text_width( ui, ui->theme.font, w->text + i, 1U );
        if ( target < acc + gw / 2 ) {
            return (int) i;
        }
        acc += gw;
    }
    return (int) n;
}

static void slider_from_x( ui_t* ui, ui_widget_t* w, int mx ) {
    int travel = w->w - THUMBW, range = w->max - w->min;
    int v = w->min;
    if ( travel > 0 && range > 0 ) {
        int p = clampi( mx - w->x - THUMBW / 2, 0, travel );
        v = w->min + (int)( ( (long) p * range + travel / 2 ) / travel );
    }
    if ( v != w->value ) {
        w->value = v;
        dirty( w );
        push_event( ui, w->id, UIE_CHANGE, v );
    }
}

static void select_row( ui_t* ui, ui_widget_t* w, int row, bool always ) {
    if ( w->nitems == 0 ) {
        return;
    }
    row = clampi( row, 0, w->nitems - 1 );
    if ( row != w->value || always ) {
        bool changed = row != w->value;
        w->value = row;
        list_show_selected( w );
        dirty( w );
        if ( changed || always ) {
            push_event( ui, w->id, UIE_SELECT, row );
        }
    }
}

static void text_insert( ui_t* ui, ui_widget_t* w, int ch ) {
    size_t n = strlen( w->text );
    if ( n >= UI_TEXTMAX ) {
        return;
    }
    memmove( w->text + w->caret + 1, w->text + w->caret, n - (size_t) w->caret + 1U );
    w->text[ w->caret ] = (char) ch;
    ++w->caret;
    text_show_caret( ui, w );
    dirty( w );
    push_event( ui, w->id, UIE_CHANGE, (int)( n + 1U ) );
}

static void focus_step( ui_t* ui, int dir ) {
    if ( ui->count == 0 ) {
        return;
    }
    int cur = index_of( ui, ui->focus );
    for ( int k = 1; k <= ui->count; ++k ) {
        int i = ( ( cur < 0 ? ( dir > 0 ? -1 : 0 ) : cur ) + dir * k + ui->count * 2 ) % ui->count;
        ui_widget_t* w = &ui->widgets[i];
        if ( focusable( w ) && in_scope( ui, w ) ) {
            ui_set_focus( ui, w->id );
            return;
        }
    }
}

// the arrow keys move the focus to the nearest widget in that direction (for the widgets that do not use the arrow themselves)
static void focus_dir( ui_t* ui, int key ) {
    ui_widget_t* cur = ui_find( ui, ui->focus );
    if ( cur == 0 ) {
        focus_step( ui, 1 );
        return;
    }
    int cx = cur->x + cur->w / 2, cy = cur->y + cur->h / 2, best = -1;
    long bestscore = 0;
    for ( int i = 0; i < ui->count; ++i ) {
        ui_widget_t* w = &ui->widgets[i];
        if ( w == cur || !focusable( w ) || !in_scope( ui, w ) ) {
            continue;
        }
        int dx = w->x + w->w / 2 - cx, dy = w->y + w->h / 2 - cy;
        int p = key == UIKEY_UP ? -dy : key == UIKEY_DOWN ? dy : key == UIKEY_LEFT ? -dx : dx;
        int o = ( key == UIKEY_UP || key == UIKEY_DOWN ) ? abs( dx ) : abs( dy );
        if ( p <= 0 ) {
            continue;
        }
        long score = p + 2L * o;
        if ( best < 0 || score < bestscore ) {
            best = i;
            bestscore = score;
        }
    }
    if ( best >= 0 ) {
        ui_set_focus( ui, ui->widgets[best].id );
    }
}

int ui_pad_keys( ui_t* ui, uint32_t buttons, double lx, double ly, uint32_t now_ms, int* keys, int max ) {
    int n = 0;
    uint32_t pressed = buttons & ~ui->padprev;
    ui->padprev = buttons;
    const ui_widget_t* f = ui_find( ui, ui->focus );
    bool enter = f != 0 && ( f->kind == UIK_LIST || f->kind == UIK_TEXT || f->kind == UIK_EDIT );
    if ( ( pressed & JOYB_A ) && n < max ) {
        keys[n++] = enter ? UIKEY_ENTER : ' ';
    }
    if ( ( pressed & JOYB_START ) && n < max ) {
        keys[n++] = UIKEY_ENTER;
    }
    if ( ( pressed & JOYB_B ) && n < max ) {
        keys[n++] = UIKEY_ESCAPE;
    }
    if ( ( pressed & JOYB_LB ) && n < max ) {
        keys[n++] = UIKEY_BACKTAB;
    }
    if ( ( pressed & JOYB_RB ) && n < max ) {
        keys[n++] = UIKEY_TAB;
    }
    int dir = 0;
    if ( buttons & JOYB_UP ) {
        dir = UIKEY_UP;
    } else if ( buttons & JOYB_DOWN ) {
        dir = UIKEY_DOWN;
    } else if ( buttons & JOYB_LEFT ) {
        dir = UIKEY_LEFT;
    } else if ( buttons & JOYB_RIGHT ) {
        dir = UIKEY_RIGHT;
    } else if ( fabs( lx ) > 0.5 || fabs( ly ) > 0.5 ) {
        dir = fabs( ly ) >= fabs( lx ) ? ( ly < 0.0 ? UIKEY_UP : UIKEY_DOWN ) : ( lx < 0.0 ? UIKEY_LEFT : UIKEY_RIGHT );
    }
    if ( dir == 0 ) {
        ui->padkey = 0;
    } else if ( dir != ui->padkey ) {
        ui->padkey = dir;
        ui->padnext_ms = now_ms + 400U;
        if ( n < max ) {
            keys[n++] = dir;
        }
    } else if ( now_ms >= ui->padnext_ms ) {
        ui->padnext_ms = now_ms + 60U;
        if ( n < max ) {
            keys[n++] = dir;
        }
    }
    return n;
}

static void handle_key( ui_t* ui, int key ) {
    int mods = key & ( UIKEY_SHIFT | UIKEY_CTRL );
    key &= ~mods;
    if ( key == UIKEY_ESCAPE ) {
        if ( ui->popup != 0 ) {
            int id = ui->popup;
            close_popup( ui );
            push_event( ui, id, UIE_CANCEL, 0 );
        } else if ( ui->modal != 0 ) {
            push_event( ui, ui->modal, UIE_CANCEL, 0 );
        }
        return;
    }
    if ( key == UIKEY_TAB || key == UIKEY_BACKTAB ) {
        if ( ui->popup == 0 ) {
            focus_step( ui, key == UIKEY_TAB ? 1 : -1 );
        }
        return;
    }
    bool arrow = key == UIKEY_UP || key == UIKEY_DOWN || key == UIKEY_LEFT || key == UIKEY_RIGHT;
    ui_widget_t* w = ui_find( ui, ui->focus );
    if ( w == 0 && arrow && ui->popup == 0 ) {
        focus_step( ui, 1 );
        return;
    }
    if ( w == 0 || !( w->flags & UIF_ENABLED ) || !( w->flags & UIF_VISIBLE ) || !in_scope( ui, w ) ) {
        return;
    }
    if ( w->kind != UIK_EDIT && ( mods & UIKEY_CTRL ) != 0 ) {
        return;                                                 // (Ctrl + a key is for the multi-line editor only; Shift + a key is the key)
    }
    if ( w->kind == UIK_EDIT && w->edit != 0 ) {
        edit_key( ui, w, key, mods );
        return;
    }
    if ( arrow && ui->popup == 0 && ( w->kind == UIK_BUTTON || w->kind == UIK_CHECK ||
         ( w->kind == UIK_TEXT && ( key == UIKEY_UP || key == UIKEY_DOWN ) ) ||
         ( w->kind == UIK_LIST && ( key == UIKEY_LEFT || key == UIKEY_RIGHT ) ) ) ) {
        focus_dir( ui, key );
        return;
    }
    switch ( w->kind ) {
        case UIK_BUTTON:
            if ( key == ' ' || key == UIKEY_ENTER ) {
                push_event( ui, w->id, UIE_CLICK, 0 );
            }
            break;
        case UIK_CHECK:
            if ( key == ' ' ) {
                w->value = !w->value;
                dirty( w );
                push_event( ui, w->id, UIE_CHANGE, w->value );
            }
            break;
        case UIK_RADIO:
            if ( key == ' ' && !w->value ) {
                radio_select( ui, w );
                push_event( ui, w->id, UIE_CHANGE, 1 );
            } else if ( key == UIKEY_UP || key == UIKEY_LEFT || key == UIKEY_DOWN || key == UIKEY_RIGHT ) {
                int dir = ( key == UIKEY_UP || key == UIKEY_LEFT ) ? -1 : 1, cur = index_of( ui, w->id );
                for ( int k = 1; k < ui->count; ++k ) {                      // the next radio button of the group (a ring)
                    ui_widget_t* o = &ui->widgets[ ( cur + dir * k + ui->count * 2 ) % ui->count ];
                    if ( o->kind == UIK_RADIO && o->group == w->group && ( o->flags & UIF_ENABLED ) && ( o->flags & UIF_VISIBLE ) ) {
                        radio_select( ui, o );
                        ui_set_focus( ui, o->id );
                        push_event( ui, o->id, UIE_CHANGE, 1 );
                        break;
                    }
                }
            }
            break;
        case UIK_TEXT: {
            size_t n = strlen( w->text );
            if ( key >= 32 && key <= 255 && key != 127 ) {
                text_insert( ui, w, key );
            } else if ( key == UIKEY_BACKSPACE && w->caret > 0 ) {
                memmove( w->text + w->caret - 1, w->text + w->caret, n - (size_t) w->caret + 1U );
                --w->caret;
                text_show_caret( ui, w );
                dirty( w );
                push_event( ui, w->id, UIE_CHANGE, (int)( n - 1U ) );
            } else if ( key == UIKEY_DELETE && (size_t) w->caret < n ) {
                memmove( w->text + w->caret, w->text + w->caret + 1, n - (size_t) w->caret );
                dirty( w );
                push_event( ui, w->id, UIE_CHANGE, (int)( n - 1U ) );
            } else if ( key == UIKEY_LEFT && w->caret > 0 ) {
                --w->caret;
                text_show_caret( ui, w );
                dirty( w );
            } else if ( key == UIKEY_RIGHT && (size_t) w->caret < n ) {
                ++w->caret;
                text_show_caret( ui, w );
                dirty( w );
            } else if ( key == UIKEY_HOME ) {
                w->caret = 0;
                text_show_caret( ui, w );
                dirty( w );
            } else if ( key == UIKEY_END ) {
                w->caret = (int) n;
                text_show_caret( ui, w );
                dirty( w );
            } else if ( key == UIKEY_ENTER ) {
                push_event( ui, w->id, UIE_ENTER, (int) n );
            }
            break;
        }
        case UIK_LIST: {
            int rows = list_rows( w ), cur = w->value < 0 ? 0 : w->value;
            if ( key == UIKEY_DOWN ) {
                select_row( ui, w, w->value < 0 ? 0 : cur + 1, false );
            } else if ( key == UIKEY_UP ) {
                select_row( ui, w, w->value < 0 ? 0 : cur - 1, false );
            } else if ( key == UIKEY_HOME ) {
                select_row( ui, w, 0, false );
            } else if ( key == UIKEY_END ) {
                select_row( ui, w, w->nitems - 1, false );
            } else if ( key == UIKEY_PGDN ) {
                select_row( ui, w, cur + rows, false );
            } else if ( key == UIKEY_PGUP ) {
                select_row( ui, w, cur - rows, false );
            } else if ( key == UIKEY_ENTER && w->value >= 0 ) {
                if ( ui->popup == w->id ) {
                    int row = w->value;
                    close_popup( ui );
                    push_event( ui, w->id, UIE_SELECT, row );
                } else {
                    push_event( ui, w->id, UIE_DOUBLE, w->value );
                }
            }
            break;
        }
        case UIK_SCROLL: {
            int v = w->value, page = w->page < 1 ? 1 : w->page;
            if ( key == UIKEY_LEFT || key == UIKEY_UP ) {
                v -= 1;
            } else if ( key == UIKEY_RIGHT || key == UIKEY_DOWN ) {
                v += 1;
            } else if ( key == UIKEY_PGUP ) {
                v -= page;
            } else if ( key == UIKEY_PGDN ) {
                v += page;
            } else if ( key == UIKEY_HOME ) {
                v = w->min;
            } else if ( key == UIKEY_END ) {
                v = w->max;
            }
            sb_set( ui, w, v );
            break;
        }
        case UIK_SLIDER: {
            int range = w->max - w->min, step = range / 10 < 1 ? 1 : range / 10, v = w->value;
            if ( key == UIKEY_LEFT || key == UIKEY_DOWN ) {
                v -= 1;
            } else if ( key == UIKEY_RIGHT || key == UIKEY_UP ) {
                v += 1;
            } else if ( key == UIKEY_PGDN ) {
                v -= step;
            } else if ( key == UIKEY_PGUP ) {
                v += step;
            } else if ( key == UIKEY_HOME ) {
                v = w->min;
            } else if ( key == UIKEY_END ) {
                v = w->max;
            }
            v = clampi( v, w->min, w->max );
            if ( v != w->value ) {
                w->value = v;
                dirty( w );
                push_event( ui, w->id, UIE_CHANGE, v );
            }
            break;
        }
        default:
            break;
    }
}

// the topmost visible widget at the point
static ui_widget_t* find_hot( ui_t* ui, int mx, int my ) {
    for ( int i = ui->count - 1; i >= 0; --i ) {
        ui_widget_t* w = &ui->widgets[i];
        if ( ( w->flags & UIF_VISIBLE ) && mx >= w->x && mx < w->x + w->w && my >= w->y && my < w->y + w->h ) {
            return w;
        }
    }
    return 0;
}

void ui_input( ui_t* ui, int mx, int my, int buttons, int wheel, const int* keys, int nkeys, uint32_t now_ms ) {
    bool down = ( buttons & UIB_LEFT ) != 0, was = ( ui->lastbuttons & UIB_LEFT ) != 0;
    bool press = down && !was, release = !down && was;
    {                                                       // the caret of an editor blinks (500 ms on, 500 ms off) and is solid for 500 ms after every key or mouse action
        if ( nkeys > 0 || wheel != 0 || buttons != ui->lastbuttons ) {
            ui->blink_base = now_ms;
        }
        bool on = ( ( now_ms - ui->blink_base ) / 500U ) % 2U == 0U;
        if ( on != ui->blink_on ) {
            ui->blink_on = on;
        }
        ui_widget_t* fw = ui->focus != 0 ? ui_find( ui, ui->focus ) : 0;
        if ( fw != 0 && fw->kind == UIK_EDIT ) {
            if ( ui->blink_drawn != ui->blink_on || nkeys > 0 ) {
                dirty( fw );
            }
            ui->blink_drawn = ui->blink_on;
        }
    }
    ui_widget_t* hot = find_hot( ui, mx, my );
    // windows: a press raises the window it is in, a press on the title bar starts dragging it (the close box asks for the close: event kind 6 cancel)
    if ( press && ui->popup == 0 && hot != 0 && in_scope( ui, hot ) ) {
        int wid = ( hot->flags & UIF_WINDOW ) ? hot->id : hot->owner;
        if ( wid != 0 ) {
            ui_raise( ui, wid );
            hot = find_hot( ui, mx, my );                   // (the widgets moved in the list)
            ui_widget_t* win = ui_find( ui, wid );
            if ( win != 0 && hot == win && my < win->y + 20 && win->text != 0 && win->text[0] != '\0' ) {
                if ( mx >= win->x + win->w - 17 && mx <= win->x + win->w - 5 && my >= win->y + 5 && my <= win->y + 17 ) {
                    push_event( ui, win->id, UIE_CANCEL, 0 );
                } else {
                    ui->capture = win->id;
                    ui->cappart = UISB_NONE;
                    ui->dragx = mx - win->x;
                    ui->dragy = my - win->y;
                }
            }
        }
    }
    if ( down && ui->capture != 0 ) {                       // dragging a window
        ui_widget_t* dw = ui_find( ui, ui->capture );
        if ( dw != 0 && ( dw->flags & UIF_WINDOW ) ) {
            int nx = mx - ui->dragx, ny = my - ui->dragy;
            if ( nx < 40 - dw->w ) nx = 40 - dw->w;
            if ( nx > ui->canvas.w - 40 ) nx = ui->canvas.w - 40;
            if ( ny < 0 ) ny = 0;
            if ( ny > ui->canvas.h - 20 ) ny = ui->canvas.h - 20;
            ui_move( ui, dw->id, nx, ny );
        }
    }
    if ( wheel != 0 ) {
        ui_widget_t* lw = ui->popup != 0 ? ui_find( ui, ui->popup ) : ( hot != 0 && ( hot->kind == UIK_LIST || hot->kind == UIK_SCROLL || hot->kind == UIK_EDIT ) && in_scope( ui, hot ) ? hot : 0 );
        if ( lw != 0 && lw->kind == UIK_SCROLL ) {
            sb_set( ui, lw, lw->value - wheel * 3 );
        } else if ( lw != 0 && lw->kind == UIK_EDIT ) {
            int top = edit_nlines( lw ) - edit_rows( lw );
            lw->scroll = clampi( lw->scroll - wheel * 3, 0, top > 0 ? top : 0 );
            dirty( lw );
        } else if ( lw != 0 && lw->nitems > 0 ) {
            int rows = list_rows( lw );
            lw->scroll = clampi( lw->scroll - wheel * 3, 0, lw->nitems > rows ? lw->nitems - rows : 0 );
            dirty( lw );
        }
    }
    sbgeom_t pg;
    if ( press && ( ui->popup != 0 ? ui_find( ui, ui->popup ) == hot : hot != 0 && in_scope( ui, hot ) && ( hot->flags & UIF_ENABLED ) ) &&
         hot != 0 && sb_params( hot, &pg ) && mx >= pg.x1 && mx <= pg.x2 && my >= pg.y1 && my <= pg.y2 ) {
        // a press on a scroll bar (the widget or the bar of a list)
        if ( focusable( hot ) && ui->popup == 0 ) {
            ui_set_focus( ui, hot->id );
        }
        int a = pg.vert ? my : mx;
        ui->capture = hot->id;
        ui->cappart = sb_part( &pg, a );
        ui->capoff = a - pg.k1;
        ui->capnext_ms = now_ms + SB_FIRST_MS;
        if ( ui->cappart != UISB_KNOB ) {
            sb_step( ui, hot, &pg, ui->cappart );
        }
        dirty( hot );
    } else if ( press ) {
        if ( ui->popup != 0 ) {
            ui_widget_t* pw = ui_find( ui, ui->popup );
            if ( pw != 0 && hot == pw ) {
                int row = pw->scroll + ( my - ( pw->y + 2 ) ) / ROWH;
                if ( row >= 0 && row < pw->nitems ) {
                    pw->value = row;
                    int id = pw->id;
                    close_popup( ui );
                    push_event( ui, id, UIE_SELECT, row );
                }
            } else {
                int id = ui->popup;
                close_popup( ui );
                push_event( ui, id, UIE_CANCEL, 0 );
            }
        } else if ( hot != 0 && in_scope( ui, hot ) && ( hot->flags & UIF_ENABLED ) ) {
            if ( focusable( hot ) ) {
                ui_set_focus( ui, hot->id );
            }
            if ( hot->kind == UIK_BUTTON || hot->kind == UIK_CHECK || hot->kind == UIK_RADIO || hot->kind == UIK_SLIDER ) {
                ui->capture = hot->id;
                dirty( hot );
                if ( hot->kind == UIK_SLIDER ) {
                    slider_from_x( ui, hot, mx );
                }
            } else if ( hot->kind == UIK_EDIT && hot->edit != 0 ) {
                ui->capture = hot->id;
                ui->cappart = UISB_NONE;
                edit_press( ui, hot, mx, my, now_ms );
            } else if ( hot->kind == UIK_TEXT ) {
                hot->caret = text_pos_at( ui, hot, mx );
                text_show_caret( ui, hot );
                dirty( hot );
            } else if ( hot->kind == UIK_LIST && hot->nitems > 0 ) {
                int row = hot->scroll + ( my - ( hot->y + 2 ) ) / ROWH;
                if ( row >= 0 && row < hot->nitems ) {
                    select_row( ui, hot, row, false );
                    if ( ui->lastclick_id == hot->id && ui->lastclick_row == row && now_ms - ui->lastclick_ms < DBLCLICK_MS ) {
                        push_event( ui, hot->id, UIE_DOUBLE, row );
                        ui->lastclick_id = 0;
                    } else {
                        ui->lastclick_id = hot->id;
                        ui->lastclick_row = row;
                        ui->lastclick_ms = now_ms;
                    }
                }
            }
        }
    } else if ( down && ui->capture != 0 ) {
        ui_widget_t* cw = ui_find( ui, ui->capture );
        sbgeom_t cg;
        if ( cw != 0 && cw->kind == UIK_SLIDER ) {
            slider_from_x( ui, cw, mx );
        } else if ( cw != 0 && cw->kind == UIK_EDIT && cw->edit != 0 && ui->cappart == UISB_NONE ) {
            if ( cw->edit->drag ) {
                edit_drag( cw, mx, my );
            }
        } else if ( cw != 0 && sb_params( cw, &cg ) ) {
            if ( ui->cappart == UISB_KNOB ) {
                sb_drag( ui, cw, mx, my );
            } else if ( now_ms >= ui->capnext_ms ) {
                // a held arrow or track click repeats (the track stops where the knob has reached the mouse)
                int a = cg.vert ? my : mx;
                bool onbar = mx >= cg.x1 && mx <= cg.x2 && my >= cg.y1 && my <= cg.y2;
                if ( onbar && sb_part( &cg, a ) == ui->cappart ) {
                    sb_step( ui, cw, &cg, ui->cappart );
                }
                ui->capnext_ms = now_ms + SB_REPEAT_MS;
            }
        }
    }
    if ( release && ui->capture != 0 ) {
        ui_widget_t* cw = ui_find( ui, ui->capture );
        ui->capture = 0;
        if ( cw != 0 && cw->kind == UIK_EDIT && cw->edit != 0 ) {
            cw->edit->drag = false;
            if ( cw->edit->anchor == cw->caret ) {
                cw->edit->anchor = -1;                          // (a click without a drag selects nothing)
            }
        }
        if ( cw != 0 ) {
            dirty( cw );
            if ( hot == cw && ( cw->flags & UIF_ENABLED ) ) {
                if ( cw->kind == UIK_BUTTON ) {
                    push_event( ui, cw->id, UIE_CLICK, 0 );
                } else if ( cw->kind == UIK_CHECK ) {
                    cw->value = !cw->value;
                    push_event( ui, cw->id, UIE_CHANGE, cw->value );
                } else if ( cw->kind == UIK_RADIO && !cw->value ) {
                    radio_select( ui, cw );
                    push_event( ui, cw->id, UIE_CHANGE, 1 );
                }
            }
        }
    }
    // canvas: a press with either button and dragging over cells report the cell (once per cell) until the buttons are let go
    bool anybtn = ( buttons & ( UIB_LEFT | UIB_RIGHT ) ) != 0, wasany = ( ui->lastbuttons & ( UIB_LEFT | UIB_RIGHT ) ) != 0;
    if ( anybtn && !wasany && ui->popup == 0 && hot != 0 && hot->kind == UIK_CANVAS && in_scope( ui, hot ) && ( hot->flags & UIF_ENABLED ) ) {
        ui->capture = hot->id;
        ui->capcell = -1;
    }
    if ( anybtn && ui->capture != 0 ) {
        ui_widget_t* cw = ui_find( ui, ui->capture );
        if ( cw != 0 && cw->kind == UIK_CANVAS ) {
            int cell = canvas_cell( cw, mx, my );
            if ( cell >= 0 && cell != ui->capcell ) {
                ui->capcell = cell;
                push_event( ui, cw->id, ( buttons & UIB_LEFT ) ? UIE_PIXEL : UIE_PIXEL2, cell );
            }
        }
    } else if ( !anybtn && ui->capture != 0 ) {
        ui_widget_t* cw = ui_find( ui, ui->capture );
        if ( cw != 0 && cw->kind == UIK_CANVAS ) {
            ui->capture = 0;
        }
    }
    for ( int i = 0; i < nkeys; ++i ) {
        handle_key( ui, keys[i] );
    }
    ui->lastbuttons = buttons;
    ui->lastx = mx;
    ui->lasty = my;
}
