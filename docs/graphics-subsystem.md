# Graphics subsystem for BASIC (proposal)

Status: **proposal, nothing implemented** (2026-10-31). It follows a look at `sdllayer`, `sdlscreen`, `textscreen`, `tilescreen` and
`sprscreen`. The decisions it needs from the project owner are at the end.

## What exists

The screen is 640 x 300 logical pixels, drawn as five layers from bottom to top (`sdlscreen.c`):

| # | Layer | Content | Renderer |
|---|---|---|---|
| 0 | background | plain bitmap, **no API** | none |
| 1 | tiles | 41 x 13 cells of 16 x 24 px from a 256-tile map, scrolled by 0..15 px | `tilescr_render` |
| 2 | text | 80 x 25 cells of 8 x 12 px, colors per cell | `txtscr_render` |
| 3 | graphics | plain bitmap, **no API** | none |
| 4 | sprites | 256 sprites of 16 x 24 px, priority, animation sequences | `sprscr_render` |

Behind them is one clear color (`sdlscr_bgcol`, a static with no setter). **Every layer is 640 x 300 bytes of palette indices with its
own 256-entry ARGB palette** (`sdllayer_t.palette`, index 0 = transparent by default; the table in `sdllayer.c` also has names such as
"Gold" that are not used yet). A layer is redrawn when its `modified` flag is set: the renderer callback fills `memory`, `sdllay_to_texture`
converts it through the palette to an ARGB texture. `sdllay_enable/disable/switch_priority` exist; the only user of `sdllayer` is
`sdlscreen.c` (and through it `engine1999.c`, `sdltest1.c`), and BASIC reaches **none** of it except the text screen through `PRINT` and
ANSI sequences and the sound.

Gaps found in the backends (not only missing BASIC words):

- **The tile screen cannot be filled**: `tilescr[]` (the 41 x 13 cell contents) is never written; only the tile *images* (`tilescr_writetile`)
  and the scroll offset exist. A setter is needed first.
- No palette access above `sdllayer` (`sdlscr_*` has no palette, layer enable or priority function), no setter for the clear color.
- No drawing code at all for the two bitmap layers (background, graphics).
- Sprites: `concept.txt` plans collision flags (sprite-sprite, sprite-tile); `sprscreen.c` has none. No way to read positions back.
- The font can be redefined only inside `textscreen.c` (`currfont`); `SYMBOL` is on the roadmap.
- The layers are modified from the main thread while the screen thread converts them (no lock, by decision; tearing is possible for a
  frame, never a crash).

## Principles of the proposal

1. **Few new reserved words, sub-keywords where possible.** Words that are already reserved or that fit common BASIC (GW-BASIC, AmigaBASIC, Locomotive) are preferred.
2. **Colors are palette indices (0..255) of the layer being drawn on**, as in the 1980s machines; `PALETTE` changes what an index means.
   `RGB(r,g,b[,a])` builds the 32 bit ARGB number for `PALETTE`.
3. **The VM talks to the screen through a host interface** (`vm_gfx_t`, like `vm_sound_t`), and the drawing algorithms live in a module
   without SDL (`gfxcore.[ch]`, working on a plain `uint8_t[640*300]` buffer) so that they are tested headless like `synth.c`. `engine1999`
   maps the interface to `sdlscr_*`; the tests use a plain buffer and compare pixels.
4. **Images are binary strings** (now possible): a tile or sprite image is a 384 byte string (16 x 24, one palette index per byte, row by row),
   a graphics block is `4 + w*h` bytes (`MKI$(w)+MKI$(h)+pixels`). Files come in through `OPEN ... FOR INPUT` and `INPUT$`, from `res:`.

## Phase 1: layers, palette, text screen

```
LAYER n, ON | OFF              n = 0 background, 1 tiles, 2 text, 3 graphics, 4 sprites
LAYER n, PRIORITY p            the drawing order (0 = bottom); the layer that has p is swapped with it (sdllay_switch_priority)
BACKDROP c                     the color behind all layers: a palette index of layer 0, or RGB(): decided below
PALETTE n, i, argb             the color i (0..255) of layer n; argb is a number such as RGB(255,128,0) or a name: PALETTE 1,5,"Gold"
PALETTE n                      resets the palette of layer n;  PALETTE alone resets all
x = RGB(r,g,b)  RGB(r,g,b,a)   0..255 each, a default 255; returns the ARGB number (a double holds it exactly)
y = PALETTE(n, i)              reads a color back
```

The text screen has the words every BASIC has and BASIC still lacks (today only `PRINT` and ANSI escapes):

```
CLS                            clears the text screen, cursor to 0,0
LOCATE x, y                    cursor to column x (0..79), row y (0..24)        (existing: txtscr_locate)
COLOR fg [, bg]                the pen and paper for the following output        (existing: txtscr_setcolors)
x = POS(0)   y = CSRLIN        cursor column and row
SYMBOL c, r0, r1, ... r11      redefine character c (12 row bytes), already planned; or SYMBOL c, a$ (12 bytes)
```

`COLOR` and `PALETTE 2, ...` affect the ANSI colors, which are palette indices of layer 2 (`ansi.c`), so a program can re-theme the text screen.

## Phase 2: the graphics layer (and the background layer)

Drawing goes to a **target layer**, the graphics layer (3) unless `GTARGET 0` selects the background layer; the choice is per program run.
Coordinates are pixels 0..639 / 0..299, origin top left; drawing is clipped, never an error. `c` is a palette index; omitted, the
current pen from `GCOLOR c` is used.

```
GCOLOR c                       the pen of the graphics statements          (GTARGET layer: 0 or 3)
GCLS [c]                       fills the target layer with c (default 0 = transparent)
PSET x, y [, c]                a pixel                              c = POINT(x, y) reads one back
LINE x1, y1, x2, y2 [, c [, B | BF]]      a line, a box (B) or a filled box (BF); GW-BASIC's form
CIRCLE x, y, r [, c [, a1, a2 [, ratio]]] a circle, arc or ellipse
PAINT x, y, c [, border]       flood fill from x,y with c up to the color border (default: the color at x,y)
GRAB x1, y1, x2, y2, a$        block to a binary string:  a$ = MKI$(w)+MKI$(h)+w*h bytes   (GW-BASIC: GET)
BLIT a$, x, y [, mode]         block from a string back (GW-BASIC: PUT); mode PSET (default), TRANS (index 0 skipped), AND, OR, XOR
DRAW a$                        the GW-BASIC drawing language (U D L R E F G H M B N C S A TA), optional, later
```

`LINE` already is a reserved word (`LINE INPUT`); the parser chooses by what follows `LINE`. GW-BASIC's `GET`/`PUT` for blocks are
not used because `GET #` / `PUT` have other meanings here (`GRAB`/`BLIT` instead, in line with the choice of `GETREC`/`PUTREC`).
`VSYNC` (below) belongs to this phase: animation without it flickers.

## Phase 3: tiles and sprites

```
TILE n, a$                     define tile image n (0..255) from 384 bytes (16 x 24)       (tilescr_writetile)
TILEMAP x, y, n                set the cell (0..40, 0..12) of the visible tile screen to tile n   (needs the new setter)
TILEMAP a$                     fill all 41*13 cells from a 533 byte string
TILESCROLL sx, sy              scroll offset 0..15 pixels                                  (tilescr_scroll; larger map windows are the program's business)
n = TILEAT(x, y)               the tile in a cell

SPRDEF n, a$                   define sprite image n (0..255) from 384 bytes               (sprscr_writemap)
SPRITE n, x, y [, image]       place sprite n (0..255) at x,y (-1024..1023), optionally set its image
SPRITE n, ON | OFF             show / hide
SPRITE n, PRIORITY p           0..7
ANIM a, a$                     animation sequence a (0..255): the string holds up to 15 image numbers   (sprscr_animdata)
SPRITE n, ANIM a, length, speed  attach it (sprscr_animcfg)
x = SPRX(n)   y = SPRY(n)      position back
c = COLLIDE(n)                 the number of another sprite that overlaps sprite n (-1 = none), by opaque pixels; needs the collision backend
c = COLLIDE(n, TILES)          0 or the tile number sprite n touches (index 0 pixels ignored)
```

To keep the words few, `SPRITE` is a statement whose second item may be a sub-keyword (`ON`, `OFF`, `PRIORITY`, `ANIM`) and `LAYER`/`PALETTE`/
`TILE` likewise; they are recognized in context like `RANDOM`/`UPDATE` in `OPEN` and are not reserved themselves. New reserved words
would be: `LAYER`, `PALETTE`, `BACKDROP`, `RGB`, `CLS`, `LOCATE`, `COLOR`, `POS`, `CSRLIN`, `GCOLOR`, `GTARGET`, `GCLS`, `PSET`, `POINT`,
`CIRCLE`, `PAINT`, `GRAB`, `BLIT`, `TILE`, `TILEMAP`, `TILESCROLL`, `TILEAT`, `SPRDEF`, `SPRITE`, `ANIM`, `SPRX`, `SPRY`, `COLLIDE`, `VSYNC`, `SYMBOL`
(about 30, comparable to what the sound statements and the I/O subsystem added). Whether a few should be avoided by a prefix form
(`GFX LINE ...`) is a decision for the owner.

## Timing

```
VSYNC                          waits for the next frame (60 Hz, SDLEV_VBLANK); a break key still works
n = FRAME                      the frame counter (sdlscr_getframecnt), for animation without TI
```

`WAIT n` (1/60 s units) and the timers `AFTER`/`EVERY` already exist; `VSYNC` is the one that is locked to the display and the sprite animation.
A sprite or tile change is visible with the next frame.

## Backend work (in order)

1. `gfxcore.[ch]` (no SDL): `PSET/LINE/CIRCLE/PAINT/GRAB/BLIT` on a buffer; tests with pixel comparisons (`gfxtest1`).
2. `sdlscreen`: `sdlscr_palette/setpalette`, `sdlscr_layer_enable/priority`, `sdlscr_setbackdrop`, `sdlscr_gfx()` (the buffer of layer 3 / 0), `sdlscr_vsync`,
   the tile cell setter in `tilescreen.c`, position getters in `sprscreen.c`, sprite collision.
3. `vm_gfx_t` in `vm.h`, `repl_t.gfx`, instructions (one per group with a kind parameter, like `SND` / `FOP`: e.g. `GFX`, `SPR`, `PAL`), parser and code generator.
4. Docs, tests (`vmtest1` with a logging gfx host like the sound host), a demo (`demos/`), checked on the real screen.

## Decisions for the project owner

1. **Names**: GW-BASIC style (`PSET`, `LINE`, `CIRCLE`, `PAINT`; `GRAB`/`BLIT` instead of `GET`/`PUT`) as above, or something else?
2. **Colors**: palette indices per layer (proposal) or one global 256-color table for all layers? (The layers have separate palettes today.)
   `BACKDROP` takes an RGB value in the proposal because layer 0 is a bitmap, not a single color.
3. **Target layer**: only the graphics layer is drawn on, plus layer 0 by `GTARGET`, or a general `TARGET n` that could also draw into the text
   layer's pixels (not meaningful) -- the proposal says only 0 and 3.
4. **Reserved words**: ~30 new words as listed, or sub-keyword forms (`GFX LINE`, `SPR ...`) with fewer?
5. **Order**: phase 1 (layers, palette, text screen words) first, then 2, then 3, with the collision backend last?
6. **Asset formats**: raw strings only (proposal), or also an image loader (PNG needs a library; a small own format `.spr` with palette and
   frames could be added later)?
