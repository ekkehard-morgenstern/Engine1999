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
#include "synhl.h"

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

#define UI_MAXWIDGETS   192
#define UI_TEXTMAX      255             // the longest text of a widget (a text field)
#define UI_EDITMAX      60000           // the longest text of a multi-line editor (UIK_EDIT)
#define UI_MAXITEMS     512             // the rows of a list
#define UI_EVENTS       64              // the event queue

#define UI_PAL_COLORS   16              // the theme uses this many consecutive palette entries (the host sets them: ui_theme_argb)
#define UI_SYN_COLORS   16              // the syntax colors (SYN_SLOTS) follow the theme: the next 16 palette entries (ui_syntax_argb)
#define UI_THEMES       3               // the presets: 0 classic (grey), 1 dark, 2 high contrast

typedef enum _ui_kind_t {
    UIK_NONE = 0, UIK_LABEL, UIK_BUTTON, UIK_CHECK, UIK_RADIO, UIK_TEXT, UIK_LIST, UIK_PROGRESS, UIK_SLIDER, UIK_PANEL, UIK_SCROLL, UIK_CANVAS, UIK_EDIT, UIK_TAB, UIK_GRID, UIK_COUNT
} ui_kind_t;

#define UIE_CLICK       1               // a button was clicked
#define UIE_CHANGE      2               // check box / radio button (value 1) / slider (value) / text field (length) changed
#define UIE_SELECT      3               // a list row was selected (value = row)
#define UIE_DOUBLE      4               // a list row was double clicked or Return was pressed on it (value = row)
#define UIE_ENTER       5               // Return in a text field (value = length)
#define UIE_PIXEL       7               // canvas: a cell was pressed or dragged over with the left button (value = y * cells wide + x)
#define UIE_PIXEL2      8               // canvas: the same with the right button
#define UIE_KEY         9               // grid: a key was typed while it had the focus (value = the key code: UIKEY_* or the character, Tab and Escape stay the UI's)
#define UIE_CELL        10              // grid: a cell was clicked with the left button (value = row * columns + column)
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
// modifiers (added to a key code; only the multi-line editor reacts to them, every other widget takes Shift+key as the key and ignores Ctrl+key):
// Shift + a cursor key (UIKEY_SHIFT | UIKEY_LEFT ...) extends the selection, Ctrl + a letter (UIKEY_CTRL | 'a' .. 'z'): A select all, C copy, X cut, V paste, Z undo, Y redo;
// Ctrl + Home / End (UIKEY_CTRL | UIKEY_HOME)
#define UIKEY_SHIFT     0x10000
#define UIKEY_CTRL      0x20000

#define UISB_NONE       0               // the parts of a scroll bar
#define UISB_ARROW1     1               // up / left
#define UISB_ARROW2     2               // down / right
#define UISB_BEFORE     3               // the track before the knob
#define UISB_AFTER      4               // the track behind the knob
#define UISB_KNOB       5

#define UIB_LEFT        1 
#define UIB_RIGHT       2               // (canvas widgets only)
              // mouse buttons of ui_input()

#define UIF_VISIBLE     1
#define UIF_ENABLED     2
#define UIF_DIRTY       4
#define UIF_WINDOW      16              // a panel made by UI WINDOW: draggable by its title bar, raised when clicked, owns the widgets created inside it
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

// a damage list: the parts of a widget's rectangle that are not valid on the canvas (never drawn, overwritten by a widget in front, changed while it was
// obscured) and are drawn again as soon as they are visible. At most UI_DMGMAX rectangles (x1 y1 x2 y2, inclusive); more are merged into their bounding box.
#define UI_DMGMAX       8
typedef struct _ui_dmg_t {
    int     n;
    int     r[UI_DMGMAX][4];
} ui_dmg_t;

typedef struct _ui_widget_t {
    int     id, kind;
    int     x, y, w, h;
    int     flags;                      // UIF_*
    char*   text;                       // caption / text field content / panel title
    int     value, min, max;            // check box and radio button: 1 on; slider; progress; list: the selected row (-1 none)
    int     page;                       // scroll bar: the size of the visible part (the knob is page / (max - min + page) of the track)
    ui_dmg_t dmg;                       // the damage list of this widget
    int     owner;                      // the id of the window this widget was created in (0 none): it moves and rises with it
    int     fg, bg;                     // palette entries of this widget's text and background (0: the theme's; UI COLOR)
    int     syntax;                     // UIK_EDIT: 0 plain text, 1 BASIC (synhl.c), 2 the classes the program gave (UI HILITE)
    int     group;                      // radio buttons of the same group exclude each other
    int     caret, scroll;              // text field: the caret (a byte index) and the first visible pixel column; list: the first visible row in scroll
    struct _ui_edit_t* edit;            // multi-line editor: the selection, the undo steps, the mouse state (ui.c)
    uint8_t* pix;                       // canvas: cw x ch cells, each a palette index (0 is drawn as a checkered 'transparent')
    int     cw, ch;
    char**  items;                      // list
    int     nitems, nalloc;
} ui_widget_t;

typedef struct _ui_t {
    ui_canvas_t     canvas;
    ui_theme_t      theme;
    int             palbase;            // the first palette entry of the theme
    int             syncolor[ UI_SYN_COLORS ];      // the palette entry of each syntax slot (default: palbase + UI_PAL_COLORS + slot)
    ui_widget_t     widgets[UI_MAXWIDGETS];
    int             count;
    int             focus;              // id (0 none)
    int             prevfocus;          // the focus before a pop-up opened
    int             modal;              // id of the modal panel (0 none)
    int             popup;              // id of the open pop-up list (0 none)
    int             capture;            // id of the widget that has the mouse button pressed (0 none)
    int             lastbuttons, lastx, lasty;
    int             dragx, dragy;       // window drag: the mouse offset inside the window's corner
    int             dmg[4];             // while a damaged part is drawn: its rectangle (x1 y1 x2 y2); the clip is cut to it
    bool            dmgon;
    ui_dmg_t        deskdmg;            // the parts of the desk (behind all widgets) that need the desk color again
    bool            blink_on, blink_drawn;  // the caret of an editor: shown now / the phase that was last drawn
    uint32_t        blink_base;         // the time of the last key or mouse action (the caret is solid for 500 ms after it)
    uint32_t        padprev;            // ui_pad_keys(): the pad buttons of the last call
    int             padkey;             // the direction key that is held (0 none)
    uint32_t        padnext_ms;         // when it repeats
    int             capcell;            // canvas: the cell last reported while the mouse is held
    int             cappart;            // scroll bar parts (UISB_*): the part that was pressed (while capture is a scroll bar or a list)
    int             capoff;             // the distance of the mouse from the top / left of the knob when it was grabbed
    uint32_t        capnext_ms;         // when a held arrow or track click repeats next
    int             clip[4];            // x1 y1 x2 y2 of the clip rectangle while drawing
    uint32_t        lastclick_ms;       // for the double click
    int             lastclick_id, lastclick_row;
    char*         (*clip_get)( size_t* len );                       // the clipboard of the multi-line editor (ISO-8859-15, LF line ends; malloc'ed, 0 if empty); 0: the internal clipboard
    void          (*clip_set)( const char* text, size_t len );
    ui_event_t      queue[UI_EVENTS];
    int             qhead, qcount;
    bool            drawn;              // ui_draw( all ) has been done
} ui_t;

// the theme with the palette entries base .. base + UI_PAL_COLORS - 1; ui_theme_argb fills the ARGB values the host puts into the palette
void ui_theme_default( ui_theme_t* t, int base );
void ui_theme_argb( uint32_t argb[ UI_PAL_COLORS ] );
void ui_theme_preset( int n, uint32_t argb[ UI_PAL_COLORS ] );          // the preset n (0 .. UI_THEMES - 1; others: 0): the theme's ARGB values
void ui_syntax_argb( int n, uint32_t argb[ UI_SYN_COLORS ] );           // the syntax colors that go with the preset n

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
bool ui_set_canvas( ui_t* ui, int id, int cw, int ch );         // gives a canvas its cells (1..256 each way), all 0
bool ui_canvas_set( ui_t* ui, int id, int x, int y, int c );
int  ui_canvas_get( ui_t* ui, int id, int x, int y );           // the cell, -1 for a bad id or place
bool ui_canvas_load( ui_t* ui, int id, const uint8_t* data, size_t n );        // cw * ch bytes (fewer: the rest is 0)
// the character grid (UIK_GRID, roadmap item 56): cols x rows cells of the fixed 8 x 12 font, each a character with its own text and background color (a palette
// entry 1..255, 0 = the theme's). It takes the focus and reports every key but Tab / Escape (UIE_KEY) and the clicks on a cell (UIE_CELL): the program draws the
// contents and reacts, so the grid is the view of a tracker's pattern or any text based table.
bool ui_set_grid( ui_t* ui, int id, int cols, int rows );
bool ui_grid_put( ui_t* ui, int id, int col, int row, const char* text, size_t len, int fg, int bg );    // fg / bg -1: unchanged; the text is cut at the right edge
bool ui_grid_clear( ui_t* ui, int id, int fg, int bg );                                                  // spaces everywhere, with the colors (-1: unchanged... 0: the theme's)
int  ui_grid_cell( ui_t* ui, int id, int col, int row );                                                 // the character, -1 for a bad id or place
bool ui_canvas_save( ui_t* ui, int id, uint8_t* out, size_t cap, size_t* n );
bool ui_move( ui_t* ui, int id, int x, int y );                 // moves a widget to x, y; a window takes the widgets it owns along
bool ui_raise( ui_t* ui, int id );                              // a window (and its widgets) in front of the others
bool ui_set_page( ui_t* ui, int id, int page );                 // scroll bar: the size of the visible part (>= 1); sets the size of the knob and the step of Page up / down
bool ui_edit_caret( ui_t* ui, int id, int pos );                // multi-line editor: the caret to a byte position (the selection is dropped, the view follows)
int  ui_edit_query( ui_t* ui, int id, int what );               // multi-line editor: 0 the caret (byte), 1 its line (from 0), 2 its column, 3 the length of the selection, 4 the number of lines; -1 for a bad id
bool ui_set_color( ui_t* ui, int id, int fg, int bg );          // the colors of a widget's text and background: a palette entry 1..255, -1 the theme's, -2 unchanged (0 and everything else is refused)
int  ui_get_color( ui_t* ui, int id, int which );               // which 0 text, 1 background: the entry, -1 = the theme's (or no such widget)
bool ui_set_syntax( ui_t* ui, int id, int mode );               // multi-line editor: 0 plain, 1 BASIC (the colors of LIST); mode 2 is set by ui_set_hilite
int  ui_get_syntax( ui_t* ui, int id );                         // -1 for a bad id
bool ui_syntax_color( ui_t* ui, int slot, int entry );          // the palette entry (1..255) a syntax slot 0..15 is drawn with
bool ui_set_hilite( ui_t* ui, int id, const uint8_t* classes, size_t n );   // multi-line editor: a slot 0..15 per byte of the text (the rest is slot 0); kept (and moved) while the text is edited
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
uint32_t ui_event_count( void );                                // the number of events queued so far (all UIs): a change means a new event (ON UI)
// a game controller as keys: the D-pad and the left stick are the arrow keys (they repeat while held), A presses (Space, Return in lists and text fields),
// B is Escape, the shoulder buttons are Shift+Tab / Tab, Start is Return. buttons are JOYB_* bits (joy.h), lx / ly the left stick; returns the number of keys.
int  ui_pad_keys( ui_t* ui, uint32_t buttons, double lx, double ly, uint32_t now_ms, int* keys, int max );
bool ui_next_event( ui_t* ui, ui_event_t* ev );                 // false: none

// draws the changed widgets (all = true: the whole canvas)
void ui_draw( ui_t* ui, bool all );

#endif
