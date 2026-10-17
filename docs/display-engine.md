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
| `textscreen.c` | `txtscr_*`: cell grid, cursor, scrolling, backspace/rubout, `txtscr_render()` into a layer's 8-bit memory |
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
- `sdlscr_lineinput( stop_evtmsk, buf, size, vblank_handler, userdata )` blocks for a line and returns an event mask;
  test `ev & SDLEV_SCREENWORKERFINISHED` to detect window close.
- `sdlscr_term()` is true once the screen worker has terminated.
- Threading: rendering runs on its own SDL thread; the application thread calls `sdlscr_*`, . Screen constants: `SDL_SCREENWIDTH 640`, `SDL_SCREENHEIGHT 300`, `TXTSCR_WIDTH 80`, `TXTSCR_HEIGHT 25`.
- `sdltest1.c` exercises text, tile and sprite output (tile 0, sprites 0–2, animation).
