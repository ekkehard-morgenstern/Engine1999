# Display engine (SDL2)

Design intent is in `concept.txt`; this summarizes it together with the public APIs.

## Concept

- 80x25 text screen, 8x12 px font → 640x300 px logical screen.
- Layers, bottom to top: background colour, background layer, tiles, text, graphics, sprites. Each screen-sized layer is
  640x300, 8 bpp palettized with its own 256-entry ARGB8888 palette (SDL2 has no palettized textures, so each layer is expanded
  into a 32-bit texture).
- Tiles: 16x24 px, 256 per tile map (16x16 tiles = 256x384 px); visible grid 41x13 with 0..15 px scroll offsets; one active map.
- Sprites: 16x24 px, up to 256 sprites with 8-byte control structures, 256 animation sequences of up to 15 frames, priority 0..7,
  sprite/sprite and sprite/tile collision flags, coordinates are signed 11 bit.
- Text cell = background colour index, foreground colour index, character index (font of 256 redefinable 8x12 monochrome glyphs).
- Redraw model: a layer is flagged *modified* → its memory is regenerated if needed (tiles costliest, then text) → palette-expanded into its
  texture → textures are composed to the screen, presented around vblank. No redraw when nothing changed, except after focus loss/regain.

## Modules

| File | Role |
|---|---|
| `sdlmain.c` | `sdl_init()` — `SDL_Init`, then `sdlev_init`, `sdlscr_init` (spawns the screen worker thread `sdlscr_worker`), `sdlaud_init`; registers cleanup via `atexit`; exits on failure |
| `sdlscreen.c` | Window/renderer, screen worker thread, input queue, public `sdlscr_*` API (text printf/lineinput/locate, tile and sprite calls) |
| `sdllayer.c` | `sdllayer_t` (texture + 8-bit memory + palette + modified/disabled flags + priority + render callback), many-at-once helpers, priority swapping |
| `ansi.[ch]` | ANSI escape sequence decoder (no SDL): SGR colors only (`0`, `30-37`, `40-47`, `90-97`, `100-107`, `38;5;n`, `48;5;n`, `39`, `49`); other sequences are skipped; hands text runs plus colors to a callback; state survives between calls. The 16 standard colors map to palette entries in `ansi.c` (`ansipal`). `sdlscr_ansi_puts()` in `sdlscreen.c` connects it to `sdlscr_printf()` |
| `textscreen.c` | `txtscr_*`: two cell grids ("screens"), cursor, viewport, scrolling, backspace/rubout, `txtscr_render()` of the selected screen into a layer's 8-bit memory |
| `fulledit.[ch]` | the full-screen program editor (`EDIT` without a target), built on text screen 1 and viewports; needs the SDL screen and the REPL |
| `tilescreen.c` | `tilescr_*`: tile map memory, scroll, render |
| `sprscreen.c` | `sprscr_*`: sprite state, animation tables, periodic animation update, render |
| `8x12font1.c` | Generated font data. Edit `8x12font1.txt` (format `!!8x12`, `#XX` char header, pixel rows) and regenerate with `fontcomp.pl` |
| `sdlaudio.c` | `sdlaud_playnote/playfreq/stopchan` — software synthesizer (sine-wave generator, SDL audio device callback) |
| `sdlevent.c` | Event-flag helper (`sdlev_raise/wait`) used between threads. Flags: `SDLEV_SCREENWORKERINITDONE`, `SDLEV_SCREENWORKERFINISHED`, `SDLEV_VBLANK`, `SDLEV_AUDIOWORKERINITDONE`, `SDLEV_AUDIOWORKERFINISHED`, `SDLEV_INPUT` |
| `sdlutil.c` | Nanosecond time helpers |
| `sdltypes.h`, `unxtypes.h` | Shared SDL/unix type includes |

Colour tables used while building the palette live in `colors-raw.txt` → `colors-sorted.txt` → `colors-cfrag.txt` (git-ignored, generated C fragment).

## Usage notes

- `sdlscr_printf( y, x, bg, fg, fmt, … )`: pass `-1` for "current cursor / colour".
- `sdlscr_lineedit( buf, bufsz, vblank_handler, usrdata )`: the one-line editor behind `EDIT` (insert, LEFT/RIGHT/HOME/END, BACKSPACE/DELETE, RETURN accepts, ESC cancels). It redraws the whole line after every key, follows the scrolling of the screen when the line wraps at the bottom, and suspends the break-on-ESC handling while it runs. Returns `SDLSCR_EDIT_ACCEPTED/CANCELLED/TOOLONG/CLOSED`.
- `sdlscr_ansi_puts( text )`: like `sdlscr_puts`, but `ESC [ … m` sets the colors (see `ansi.h`). There is deliberately **no lock** around the text screen (nothing may block rendering; the text screen tolerates the screen thread reading while the main thread writes), and this function adds none. `sdlscr_ansi_printf( fmt, … )` is the printf form (any length). The BASIC `PRINT` output of `engine1999` (channel 0, `host_print`) goes through `sdlscr_ansi_puts`, so `PRINT CHR$(27);"[31m"` changes the color; the colors stay until reset with `ESC [ 0 m` (error messages and `Ready.` use their own colors).
- `sdlscr_lineinput( stop_evtmsk, buf, size, vblank_handler, userdata )` blocks for a line and returns an event mask;
  test `ev & SDLEV_SCREENWORKERFINISHED` to detect window close.
- `sdlscr_term()` is true once the screen worker has terminated.
- Threading: rendering runs on its own SDL thread; the application thread calls `sdlscr_*`, . Screen constants: `SDL_SCREENWIDTH 640`, `SDL_SCREENHEIGHT 300`, `TXTSCR_WIDTH 80`, `TXTSCR_HEIGHT 25`.
- `sdltest1.c` exercises text, tile and sprite output (tile 0, sprites 0–2, animation).

## Text screens and viewports

`textscreen.c` keeps `TXTSCR_NSCREENS` (2) independent text screens, each with its own cells, cursor, pen/paper and viewport: screen 0 is the BASIC screen, screen 1 belongs to the full-screen editor. `txtscr_select( n )` chooses the screen that is drawn (`txtscr_render()`) and written to; the other one keeps its contents untouched, so switching back and forth costs nothing (after switching call `sdlscr_textchanged()` to redraw).

The **viewport** (`txtscr_setviewport( x, y, w, h )`, `txtscr_resetviewport()`) is the rectangle in which output happens. Everything is relative to it: the coordinates of `txtscr_write/print/locate/getcell`, the cursor, `txtscr_getsize()`. `txtscr_write()` clips at the right edge and below, `txtscr_print()` continues text that runs over the right edge in the next viewport row, and scrolling (`txtscr_scrollup/scrolldown`, printing below the last row, backspace at the corner) moves only the viewport. `txtscr_clear()` and scrolling fill with the blank colors (`txtscr_setblank()`, default `TXTSCR_BGCOL/FGCOL`). Setting a viewport puts the cursor at its corner; the default viewport is the whole screen, so BASIC output behaves as before. `sdlscr_lineedit()` works inside a viewport. Tested by `txttest1`.

## Full-screen editor (`fulledit.c`)

`EDIT` without a target sets `repl_t.fulledit`; `engine1999` checks the flag after `repl_enter()` returns (never from inside a callback, because the editor enters lines itself) and calls `fulledit_run()`. Layout on screen 1, one viewport per panel: title bar (row 0: "Text Editor", the name of the last file loaded or saved on the right: `repl_t.filename`), line number pane (7 columns, gold on navy), text pane (72 columns), scroll bar (1 column, informational: no mouse support yet), help bar (row 24; error messages replace it until the next key). Rows below the last line are grey (number pane dark slate).

Program lines are drawn with the syntax highlighting of `repl->hlstyle` (the ANSI sequences are decoded with `ansi.c`); the current line is edited in place as plain text with `sdlscr_editkey()` (the editing keys shared with `sdlscr_lineedit()`). A line longer than the text pane wraps inside it. Panels are redrawn completely after every key.

Keys: UP/DOWN/PGUP/PGDN/CTRL+HOME/CTRL+END change the line (an edited line is entered first through `repl_enter()`; if the REPL refuses it, the message appears in the help bar and the cursor stays), LEFT/RIGHT/HOME/END/BACKSPACE/DELETE/characters edit. RETURN at the end of the line inserts a new line after it: the editor makes room (a blank row), the cursor jumps into the number pane that shows dots; type the number (up to 6 digits), SPACE/TAB/RETURN go on to the text (no number: chosen automatically, the next multiple of 10 after the current line, or the middle of the gap; "No room" if there is none; an existing number is refused), ESC cancels the insertion. RETURN at the end of a new line's text enters it and opens another one, RETURN on an empty new line drops it. RETURN in the middle of a line enters it and goes to the next. CTRL+Y deletes the current line (or drops the new one). Emptying the text of an existing line deletes it when it is entered, as in BASIC. ESC enters what is being edited and leaves the editor; if that fails it needs a second ESC, which discards. Lines longer than 1656 characters are shown but read-only. The position (line numbers of the current and top line) is remembered for the next call; the first call starts at the first line.
