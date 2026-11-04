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
#include <math.h>

#define ROWH        14                  // the height of a list row
#define CHECKSZ     13
#define THUMBW      10
#define DBLCLICK_MS 400

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
        ( w->kind == UIK_BUTTON || w->kind == UIK_CHECK || w->kind == UIK_RADIO || w->kind == UIK_TEXT || w->kind == UIK_LIST || w->kind == UIK_SLIDER );
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

static void push_event( ui_t* ui, int id, int kind, int value ) {
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

void ui_theme_argb( uint32_t argb[ UI_PAL_COLORS ] ) {
    static const uint32_t c[ UI_PAL_COLORS ] = {
        0xFF006060, 0xFFC0C0C0, 0xFFFFFFFF, 0xFF808080, 0xFF000000, 0xFF000080, 0xFFFFFFFF, 0xFFFFFFFF,
        0xFF808080, 0xFF000000, 0xFF000080, 0xFFFFFFFF, 0xFF0000AA, 0xFFFFFF00, 0xFFFF0000, 0xFF008000
    };
    memcpy( argb, c, sizeof(c) );
}

// -- drawing primitives (clipped to the widget being drawn) ---------------------------------------------------------------------------------------------

static void cbox( ui_t* ui, int x1, int y1, int x2, int y2, int color, bool fill ) {
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

// -- drawing the widgets --------------------------------------------------------------------------------------------------------------------------------

static void draw_widget( ui_t* ui, int index ) {
    ui_widget_t* w = &ui->widgets[index];
    const ui_theme_t* t = &ui->theme;
    int x1 = w->x, y1 = w->y, x2 = w->x + w->w - 1, y2 = w->y + w->h - 1;
    bool on = ( w->flags & UIF_ENABLED ) != 0;
    bool focus = ui->focus == w->id && on;
    bool pressed = ui->capture == w->id;
    int tc = on ? t->text : t->disabled;
    size_t len = w->text ? strlen( w->text ) : 0;
    set_clip( ui, x1, y1, x2, y2 );
    switch ( w->kind ) {
        case UIK_LABEL:
            draw_text( ui, t->font, x1, y1 + ( w->h - FONT_HEIGHT ) / 2, w->text ? w->text : "", len, tc );
            break;
        case UIK_PANEL:
            cbox( ui, x1, y1, x2, y2, t->face, true );
            bevel( ui, x1, y1, x2, y2, true );
            if ( len > 0 ) {
                cbox( ui, x1 + 3, y1 + 3, x2 - 3, y1 + 18, t->title, true );
                draw_text( ui, t->boldfont, x1 + 8, y1 + 4, w->text, len, t->titletext );
            }
            break;
        case UIK_BUTTON: {
            cbox( ui, x1, y1, x2, y2, t->face, true );
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
            cbox( ui, x1, y1, x2, y2, t->field, true );
            bevel( ui, x1, y1, x2, y2, false );
            set_clip( ui, x1 + 2, y1 + 2, x2 - 2, y2 - 2 );
            int tx = x1 + 4 - w->scroll, ty = y1 + ( w->h - FONT_HEIGHT ) / 2;
            draw_text( ui, t->font, tx, ty, w->text ? w->text : "", len, tc );
            if ( focus ) {
                int cx = tx + text_width( ui, t->font, w->text ? w->text : "", (size_t) w->caret );
                cbox( ui, cx, ty, cx, ty + FONT_HEIGHT - 1, t->text, true );
            }
            break;
        }
        case UIK_LIST: {
            cbox( ui, x1, y1, x2, y2, t->field, true );
            bevel( ui, x1, y1, x2, y2, false );
            set_clip( ui, x1 + 2, y1 + 2, x2 - 2, y2 - 2 );
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
            if ( w->nitems > rows ) {                       // a bar for how much of the list is shown
                int track = w->h - 6, bar = track * rows / w->nitems, pos = ( track - bar ) * w->scroll / ( w->nitems - rows );
                cbox( ui, x2 - 4, y1 + 3 + pos, x2 - 3, y1 + 3 + pos + bar, t->shadow, true );
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

void ui_draw( ui_t* ui, bool all ) {
    if ( !ui->drawn ) {
        all = true;
    }
    if ( all ) {
        set_clip( ui, 0, 0, ui->canvas.w - 1, ui->canvas.h - 1 );
        cbox( ui, 0, 0, ui->canvas.w - 1, ui->canvas.h - 1, ui->theme.desk, true );
        for ( int i = 0; i < ui->count; ++i ) {
            if ( ui->widgets[i].flags & UIF_VISIBLE ) {
                draw_widget( ui, i );
            }
            ui->widgets[i].flags &= ~UIF_DIRTY;
        }
        ui->drawn = true;
        return;
    }
    for ( int i = 0; i < ui->count; ++i ) {                 // a widget that went away: its place gets the background, whatever was under it is drawn again
        ui_widget_t* w = &ui->widgets[i];
        if ( ( w->flags & UIF_DIRTY ) && !( w->flags & UIF_VISIBLE ) ) {
            set_clip( ui, w->x, w->y, w->x + w->w - 1, w->y + w->h - 1 );
            cbox( ui, w->x, w->y, w->x + w->w - 1, w->y + w->h - 1, background( ui, i ), true );
            w->flags &= ~UIF_DIRTY;
            for ( int j = 0; j < ui->count; ++j ) {
                if ( j != i && ( ui->widgets[j].flags & UIF_VISIBLE ) && overlaps( w, &ui->widgets[j] ) ) {
                    ui->widgets[j].flags |= UIF_DIRTY;
                }
            }
        }
    }
    for ( int i = 0; i < ui->count; ++i ) {                 // what lies over a changed widget is drawn again
        if ( ui->widgets[i].flags & UIF_DIRTY ) {
            for ( int j = i + 1; j < ui->count; ++j ) {
                if ( ( ui->widgets[j].flags & UIF_VISIBLE ) && overlaps( &ui->widgets[i], &ui->widgets[j] ) ) {
                    ui->widgets[j].flags |= UIF_DIRTY;
                }
            }
        }
    }
    for ( int i = 0; i < ui->count; ++i ) {
        ui_widget_t* w = &ui->widgets[i];
        if ( !( w->flags & UIF_DIRTY ) ) {
            continue;
        }
        set_clip( ui, w->x, w->y, w->x + w->w - 1, w->y + w->h - 1 );
        cbox( ui, w->x, w->y, w->x + w->w - 1, w->y + w->h - 1, background( ui, i ), true );     // (a hidden widget leaves the background)
        if ( w->flags & UIF_VISIBLE ) {
            draw_widget( ui, i );
        }
        w->flags &= ~UIF_DIRTY;
    }
}

// -- the widgets ------------------------------------------------------------------------------------------------------------------------------------------

void ui_init( ui_t* ui, const ui_canvas_t* canvas, int palbase ) {
    memset( ui, 0, sizeof(*ui) );
    ui->canvas = *canvas;
    ui_theme_default( &ui->theme, palbase );
    ui->lastclick_row = -1;
}

static void free_widget( ui_widget_t* w ) {
    free( w->text );
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

bool ui_add( ui_t* ui, int kind, int id, int x, int y, int w, int h, const char* text, int extra ) {
    if ( kind <= UIK_NONE || kind >= UIK_COUNT || id <= 0 || ui->count >= UI_MAXWIDGETS || index_of( ui, id ) >= 0 || w < 1 || h < 1 ) {
        return false;
    }
    ui_widget_t* wd = &ui->widgets[ ui->count ];
    memset( wd, 0, sizeof(*wd) );
    wd->text = dupn( text, UI_TEXTMAX, kind == UIK_TEXT ? UI_TEXTMAX + 1U : 0U );
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
    if ( kind == UIK_TEXT ) {
        wd->caret = (int) strlen( wd->text );
    }
    ++ui->count;
    if ( ui->focus == 0 && focusable( wd ) ) {
        ui->focus = id;
    }
    return true;
}

bool ui_remove( ui_t* ui, int id ) {
    int i = index_of( ui, id );
    if ( i < 0 ) {
        return false;
    }
    ui_widget_t w = ui->widgets[i];
    // the place is repainted: the widget goes on as a hidden, dirty entry for one ui_draw? no: the array entry is removed at once, so erase now
    if ( ui->drawn ) {
        int bg = background( ui, i );
        set_clip( ui, w.x, w.y, w.x + w.w - 1, w.y + w.h - 1 );
        cbox( ui, w.x, w.y, w.x + w.w - 1, w.y + w.h - 1, bg, true );
    }
    free_widget( &ui->widgets[i] );
    memmove( &ui->widgets[i], &ui->widgets[i + 1], (size_t)( ui->count - i - 1 ) * sizeof(ui_widget_t) );
    --ui->count;
    memset( &ui->widgets[ ui->count ], 0, sizeof(ui_widget_t) );
    for ( int j = 0; j < ui->count; ++j ) {                 // whatever was under it or near it is drawn again
        if ( overlaps( &w, &ui->widgets[j] ) ) {
            ui->widgets[j].flags |= UIF_DIRTY;
        }
    }
    if ( ui->focus == id ) {
        ui->focus = 0;
        for ( int j = 0; j < ui->count && ui->focus == 0; ++j ) {
            if ( focusable( &ui->widgets[j] ) ) {
                ui->focus = ui->widgets[j].id;
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

bool ui_set_text( ui_t* ui, int id, const char* text ) {
    ui_widget_t* w = ui_find( ui, id );
    if ( w == 0 ) {
        return false;
    }
    char* t = dupn( text, UI_TEXTMAX, w->kind == UIK_TEXT ? UI_TEXTMAX + 1U : 0U );
    if ( t == 0 ) {
        return false;
    }
    free( w->text );
    w->text = t;
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
        case UIK_SLIDER: case UIK_PROGRESS:
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
    *value = w->kind == UIK_TEXT ? (int) strlen( w->text ) : w->value;
    return true;
}

bool ui_set_range( ui_t* ui, int id, int min, int max ) {
    ui_widget_t* w = ui_find( ui, id );
    if ( w == 0 || ( w->kind != UIK_SLIDER && w->kind != UIK_PROGRESS ) || max < min ) {
        return false;
    }
    w->min = min;
    w->max = max;
    w->value = clampi( w->value, min, max );
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
    for ( int i = 0; i < ui->count; ++i ) {                 // the others first
        if ( !( i == p || ( i > p && ui->widgets[i].kind != UIK_PANEL && inside( &ui->widgets[i], &panel ) ) ) ) {
            tmp[n++] = ui->widgets[i];
        }
    }
    for ( int i = 0; i < ui->count; ++i ) {                 // then the panel and what is in it
        if ( i == p || ( i > p && ui->widgets[i].kind != UIK_PANEL && inside( &ui->widgets[i], &panel ) ) ) {
            tmp[n++] = ui->widgets[i];
        }
    }
    memcpy( ui->widgets, tmp, sizeof(ui_widget_t) * (size_t) ui->count );
    free( tmp );
    for ( int i = 0; i < ui->count; ++i ) {
        dirty( &ui->widgets[i] );
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

static void handle_key( ui_t* ui, int key ) {
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
    ui_widget_t* w = ui_find( ui, ui->focus );
    if ( w == 0 || !( w->flags & UIF_ENABLED ) || !( w->flags & UIF_VISIBLE ) || !in_scope( ui, w ) ) {
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

void ui_input( ui_t* ui, int mx, int my, int buttons, int wheel, const int* keys, int nkeys, uint32_t now_ms ) {
    bool down = ( buttons & UIB_LEFT ) != 0, was = ( ui->lastbuttons & UIB_LEFT ) != 0;
    bool press = down && !was, release = !down && was;
    ui_widget_t* hot = 0;
    for ( int i = ui->count - 1; i >= 0; --i ) {
        ui_widget_t* w = &ui->widgets[i];
        if ( ( w->flags & UIF_VISIBLE ) && mx >= w->x && mx < w->x + w->w && my >= w->y && my < w->y + w->h ) {
            hot = w;
            break;
        }
    }
    if ( wheel != 0 ) {
        ui_widget_t* lw = ui->popup != 0 ? ui_find( ui, ui->popup ) : ( hot != 0 && hot->kind == UIK_LIST && in_scope( ui, hot ) ? hot : 0 );
        if ( lw != 0 && lw->nitems > 0 ) {
            int rows = list_rows( lw );
            lw->scroll = clampi( lw->scroll - wheel * 3, 0, lw->nitems > rows ? lw->nitems - rows : 0 );
            dirty( lw );
        }
    }
    if ( press ) {
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
        if ( cw != 0 && cw->kind == UIK_SLIDER ) {
            slider_from_x( ui, cw, mx );
        }
    }
    if ( release && ui->capture != 0 ) {
        ui_widget_t* cw = ui_find( ui, ui->capture );
        ui->capture = 0;
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
    for ( int i = 0; i < nkeys; ++i ) {
        handle_key( ui, keys[i] );
    }
    ui->lastbuttons = buttons;
    ui->lastx = mx;
    ui->lasty = my;
}
