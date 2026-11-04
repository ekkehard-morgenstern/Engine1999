# The UI subsystem

Widgets (panels, labels, buttons, check boxes, radio buttons, text fields, lists, sliders, progress bars) drawn on the **graphics layer** (layer 3, 640 x 300), driven by the mouse and the keyboard. Roadmap item 47 / section 15. Demo: `demos/ui.bas`. Window host only: the console host answers "Not available in the console host" (a text screen renderer is a later step).

## Layers of the implementation
- `font.[ch]` (no SDL): proportional fonts built from the 8 x 12 font (sans = ink columns shifted left plus one pixel gap, bold = double strike). `font_measure()`, `font_draw()`, `font_fit()`. Test: `fonttest1`.
- `ui.[ch]` (no SDL, no VM): the retained widget table (up to 128 widgets, ids chosen by the caller), theme, drawing of the changed parts only, hit testing, focus / Tab order, text editing, lists, sliders, modal panel, pop-up, an event queue of 64. It draws through a small canvas table (`ui_canvas_t`: box and pixel), so it is tested on a `gfxcore` bitmap. Test: `uitest1` (122 checks, pixel checks included).
- `gfxops.c` rows (group `UI`) and the extension word `UI` (`XF_UI`) in the parser; the host (`engine1999.c`, `host_ui_op()`) maps them to `ui.h`; the mouse comes from `mouse_get()` and the keys from `sdlscr_pollkey()` (escape sequences parsed with `keyseq_parse()`).

## Decisions taken (owner, 2026-11-01) and mine
- The UI owns the graphics layer; its theme uses the palette entries 224..239 of that layer (`UI_PALBASE`); the mouse pointer sprite stays in front; `UI OPEN` shows the pointer permanently.
- One form plus modal panels and pop-ups first; overlapping windows later. Retained widgets.
- Mine: widgets are drawn in creation order (z-order = order of creation); a **pop-up is a hidden list** that `UI POPUP id,x,y` shows (a click outside or Escape closes it with a cancel event); a **modal panel** (`UI MODAL id`) makes only the panel and widgets lying inside it react; events are polled (`UI("EVENT")`), not `ON UI` (not done).

## Statements
`UI OPEN`, `UI CLOSE`, `UI CLEAR`, `UI UPDATE`; `UI LABEL|BUTTON|CHECK|TEXT|PANEL id,x,y,w,h,text$`; `UI RADIO id,x,y,w,h,text$[,group]`; `UI LIST|PROGRESS id,x,y,w,h`; `UI SLIDER id,x,y,w,h[,min]`; `UI SCROLL id,x,y,w,h[,page]` (a scroll bar: vertical when it is higher than wide; positions 0..100 until `UI RANGE id,min,max`; `UI PAGE id,n` sets the size of the visible part, which sizes the knob and the Page up / down step; events: change with the position); `UI REMOVE|FOCUS|CLEARLIST|MODAL id` (`MODAL 0` ends it); `UI CAPTION|ITEM id,text$`; `UI VALUE|ENABLE|SHOW id,n`; `UI RANGE id,min,max`; `UI POPUP id,x,y`; `UI GETTEXT id,a$`; `UI ITEMTEXT id,row,a$`.

## Functions
`UI("EVENT")` next event id (0 none; makes it current), `UI("KIND")`, `UI("VALUE")` of the current event, `UI("GET",id)`, `UI("COUNT",id)` rows of a list, `UI("FOCUS")`, `UI("OPEN")`.
Event kinds: 1 click (button), 2 change (check, radio, slider, text field: the new length), 3 select (list row), 4 double click (or Return in a list), 5 Return in a text field, 6 cancel (Escape in a modal panel / pop-up, click outside a pop-up).

## Scroll bars
A scroll bar has an arrow button at each end, a light / gray checkered track and a raised knob whose length is `page / (range + page)` of the track (at least 10 pixels), with a grip. A click on an arrow moves one step, a click in the track one page; both repeat while the button is held (after 400 ms, then every 60 ms; the track repeat stops when the knob reaches the mouse); the knob can be dragged; the wheel moves 3 steps; with the focus the arrow keys, Page up / down, Home and End work. A **list** that has more rows than it shows gets such a bar by itself (it only scrolls the view: the selection does not change).

## The end of a run
The UI belongs to the program that opened it: when a run stops (END, an error, Break) the host closes the UI and clears the graphics layer, so that the prompt is visible again. (Before this was done, ESC, which is the break key, left the form over the text screen and it looked like a hang.) A program that wants ESC for itself (cancel a dialog) uses `BREAK OFF`; Ctrl+C still stops it.

## Keys
Tab / Shift+Tab move the focus; Space and Return press buttons and toggle boxes; arrows work in lists (Home, End, Page up / down), sliders and radio groups; text fields take typing, Backspace, Delete, Home, End, Left, Right.

## Not done
`ON UI GOSUB`, overlapping and draggable windows, scroll bars as widgets, combo boxes, a file requester, the sprite / tile editors, the console text renderer, the joystick driving the focus. Widgets are not drawn until `UI UPDATE` runs (the demo updates once per frame).
