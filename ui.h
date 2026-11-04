#pragma once

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

#ifndef UI_H
#define UI_H        1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

#include "font.h"

/*
The UI subsystem (docs/ui-subsystem.md): retained widgets on a canvas, no SDL, no VM. The program (the BASIC statements, or a test) creates widgets,
feeds the module once per frame with the mouse and the keys (ui_input) and reads the events it queued (ui_next_event); ui_draw draws what changed.

    kinds:   label, button, check box, radio button, text field, list, progress bar, slider, panel (a raised area with an optional title)
    events:  click (button), change (check box, radio button, slider, text field), select (list row), double (list row), enter (text field Return),
             cancel (a pop-up closed without a choice, Escape in a modal panel)
    input:   the mouse (position, buttons, wheel), keys (typed characters and the special keys below), focus with Tab / Shift+Tab, a modal panel
             (only its widgets react), a pop-up list (a menu: closes at a click outside)

The module draws on a canvas it is given (two callbacks: box and pset), so the same code runs on a layer of the window (the host maps the
callbacks to sdlscreen) and on a gfxcore bitmap in the test. All coordinates are pixels; the layout is the program's.
*/

#define UI_MAXWIDGETS   128
#define UI_TEXTMAX      255             // the longest text of a widget (a text field)
#define UI_MAXITEMS     512             // the rows of a list
#define UI_EVENTS       64              // the event queue

#define UI_PAL_COLORS   16              // the theme uses this many consecutive palette entries (the host sets them: ui_theme_argb)

typedef enum _ui_kind_t {
    UIK_NONE = 0, UIK_LABEL, UIK_BUTTON, UIK_CHECK, UIK_RADIO, UIK_TEXT, UIK_LIST, UIK_PROGRESS, UIK_SLIDER, UIK_PANEL, UIK_SCROLL, UIK_COUNT
} ui_kind_t;

#define UIE_CLICK       1               // a button was clicked
#define UIE_CHANGE      2               // check box / radio button (value 1) / slider (value) / text field (length) changed
#define UIE_SELECT      3               // a list row was selected (value = row)
#define UIE_DOUBLE      4               // a list row was double clicked or Return was pressed on it (value = row)
#define UIE_ENTER       5               // Return in a text field (value = length)
#define UIE_CANCEL      6               // a pop-up list was closed without a choice, or Escape in a modal panel (id = the pop-up / the panel)

typedef struct _ui_event_t {
    int     id;
    int     kind;                       // UIE_*
    int     value;
} ui_event_t;

// keys given to ui_input(): typed characters 32..255 as they are, the others as these codes
#define UIKEY_BACKSPACE 8
#define UIKEY_TAB       9
#define UIKEY_ENTER     13
#define UIKEY_ESCAPE    27
#define UIKEY_LEFT      0x1001
#define UIKEY_RIGHT     0x1002
#define UIKEY_UP        0x1003
#define UIKEY_DOWN      0x1004
#define UIKEY_HOME      0x1005
#define UIKEY_END       0x1006
#define UIKEY_PGUP      0x1007
#define UIKEY_PGDN      0x1008
#define UIKEY_DELETE    0x1009
#define UIKEY_BACKTAB   0x100A          // Shift+Tab

#define UISB_NONE       0               // the parts of a scroll bar
#define UISB_ARROW1     1               // up / left
#define UISB_ARROW2     2               // down / right
#define UISB_BEFORE     3               // the track before the knob
#define UISB_AFTER      4               // the track behind the knob
#define UISB_KNOB       5

#define UIB_LEFT        1               // mouse buttons of ui_input()

#define UIF_VISIBLE     1
#define UIF_ENABLED     2
#define UIF_DIRTY       4
#define UIF_POPUP       8               // a list used as a pop-up menu

typedef struct _ui_canvas_t {
    void*   ctx;
    int     w, h;
    void  (*box)( void* ctx, int x1, int y1, int x2, int y2, int color, bool fill );
    void  (*pset)( void* ctx, int x, int y, int color );
} ui_canvas_t;

// palette indices of the theme (consecutive from a base: ui_theme_default)
typedef struct _ui_theme_t {
    int     desk, face, light, shadow, text, hilite, hilitetext, field, disabled, border, title, titletext, fill;
    int     font, boldfont;             // FONT_*
} ui_theme_t;

typedef struct _ui_widget_t {
    int     id, kind;
    int     x, y, w, h;
    int     flags;                      // UIF_*
    char*   text;                       // caption / text field content / panel title
    int     value, min, max;            // check box and radio button: 1 on; slider; progress; list: the selected row (-1 none)
    int     page;                       // scroll bar: the size of the visible part (the knob is page / (max - min + page) of the track)
    int     group;                      // radio buttons of the same group exclude each other
    int     caret, scroll;              // text field: the caret (a byte index) and the first visible pixel column; list: the first visible row in scroll
    char**  items;                      // list
    int     nitems, nalloc;
} ui_widget_t;

typedef struct _ui_t {
    ui_canvas_t     canvas;
    ui_theme_t      theme;
    ui_widget_t     widgets[UI_MAXWIDGETS];
    int             count;
    int             focus;              // id (0 none)
    int             prevfocus;          // the focus before a pop-up opened
    int             modal;              // id of the modal panel (0 none)
    int             popup;              // id of the open pop-up list (0 none)
    int             capture;            // id of the widget that has the mouse button pressed (0 none)
    int             lastbuttons, lastx, lasty;
    int             cappart;            // scroll bar parts (UISB_*): the part that was pressed (while capture is a scroll bar or a list)
    int             capoff;             // the distance of the mouse from the top / left of the knob when it was grabbed
    uint32_t        capnext_ms;         // when a held arrow or track click repeats next
    int             clip[4];            // x1 y1 x2 y2 of the clip rectangle while drawing
    uint32_t        lastclick_ms;       // for the double click
    int             lastclick_id, lastclick_row;
    ui_event_t      queue[UI_EVENTS];
    int             qhead, qcount;
    bool            drawn;              // ui_draw( all ) has been done
} ui_t;

// the theme with the palette entries base .. base + UI_PAL_COLORS - 1; ui_theme_argb fills the ARGB values the host puts into the palette
void ui_theme_default( ui_theme_t* t, int base );
void ui_theme_argb( uint32_t argb[ UI_PAL_COLORS ] );

void ui_init( ui_t* ui, const ui_canvas_t* canvas, int palbase );
void ui_free( ui_t* ui );
void ui_clear( ui_t* ui );                                      // removes every widget (the screen is left as it is: ui_draw( true ) repaints)

// creates a widget; extra: the group of a radio button, the page of a scroll bar (default 10; positions 0..100 until ui_set_range), the minimum of a slider (its maximum is 100 until ui_set_range), unused otherwise. false: the id exists
// or is not positive, the kind is invalid or there is no room
bool ui_add( ui_t* ui, int kind, int id, int x, int y, int w, int h, const char* text, int extra );
bool ui_remove( ui_t* ui, int id );
ui_widget_t* ui_find( ui_t* ui, int id );

bool ui_set_text( ui_t* ui, int id, const char* text );         // caption, text field content (cut to UI_TEXTMAX), panel title
const char* ui_get_text( ui_t* ui, int id );                    // 0 for an unknown widget
bool ui_set_value( ui_t* ui, int id, int value );               // check box 0 / 1, radio button (1 selects it, the others of the group go off), slider, progress, list row
bool ui_get_value( ui_t* ui, int id, int* value );
bool ui_set_range( ui_t* ui, int id, int min, int max );        // slider, progress bar and scroll bar (the smallest and the largest position)
bool ui_set_page( ui_t* ui, int id, int page );                 // scroll bar: the size of the visible part (>= 1); sets the size of the knob and the step of Page up / down
bool ui_set_enabled( ui_t* ui, int id, bool on );
bool ui_set_visible( ui_t* ui, int id, bool on );
bool ui_set_focus( ui_t* ui, int id );
bool ui_list_add( ui_t* ui, int id, const char* item );
bool ui_list_clear( ui_t* ui, int id );
const char* ui_list_get( ui_t* ui, int id, int row );
bool ui_set_modal( ui_t* ui, int panelid );                     // 0 ends it; only the panel's widgets react (the panel is raised to the front: it should be the last)
bool ui_popup( ui_t* ui, int listid, int x, int y );            // shows the list at x, y as a pop-up menu (a click outside closes it: UIE_CANCEL)

// the input of one frame: the mouse (buttons UIB_*, wheel steps, up positive) and the keys typed since the last call; now_ms for the double click
void ui_input( ui_t* ui, int mx, int my, int buttons, int wheel, const int* keys, int nkeys, uint32_t now_ms );
bool ui_next_event( ui_t* ui, ui_event_t* ev );                 // false: none

// draws the changed widgets (all = true: the whole canvas)
void ui_draw( ui_t* ui, bool all );

#endif
