# Graphics subsystem

Status: **implemented** (2026-10-31): layers, palettes, the graphics layer, tiles, sprites with `DESIGN` pictures, collisions, text screen
areas, and the internal `sdlscr_*` API. The design decisions of the project owner are in "Decisions". The demo is `demos/gfx.bas`.

## What exists

The screen is 640 x 300 logical pixels, drawn as five layers from bottom to top (`sdlscreen.c`):

| # | Layer | Content | Drawn by |
|---|---|---|---|
| 0 | background | plain bitmap (`GFX TARGET 0`) | `gfxcore` through `sdlscreen` |
| 1 | tiles | 41 x 13 cells of 16 x 24 px from a 256-tile map, scrolled by 0..15 px | `tilescr_render` |
| 2 | text | 80 x 25 cells of 8 x 12 px, colors per cell | `txtscr_render` |
| 3 | graphics | plain bitmap (`GFX TARGET 3`, the default) | `gfxcore` through `sdlscreen` |
| 4 | sprites | 256 sprites of 16 x 24 px, priority, animation sequences, collisions | `sprscr_render` |

Behind them is the backdrop color (`GFX BACKDROP`). **Every layer is 640 x 300 bytes of palette indices with its own 256 entry ARGB palette**
(`sdllayer_t.palette`, index 0 = transparent by default; the names of the color table, such as "Gold", can be used by `GFX PALNAME`).
Colors in all statements are palette indices of the layer that is drawn on.

## Principles

1. **All calls into the graphics modules go through `sdlscreen`** (owner's requirement). Nothing outside `sdlscreen.c` calls `sdllayer`, `textscreen`,
   `tilescreen`, `sprscreen` or `gfxcore`: the host (`engine1999.c`), the editors (`fulledit.c`, `monitor.c`, converted to `sdlscr_text_*`) and the tests
   use `sdlscr_*` (declared at the end of `sdlscreen.h`). Every function validates its arguments, marks the layer it changed as modified, and hands out no
   pointers into layer memory, so a lock or a command queue can be added in this one place later (there is none today, by decision: the screen thread
   may convert a layer while the main thread draws, which shows at worst one torn frame).
2. **Few reserved words.** Only ten words are new: `GFX SPRITE TILE TXT DESIGN RGB POINT CLS LOCATE COLOR`. Everything else is a *sub-keyword* after
   `GFX`, `TXT`, `TILE` or `SPRITE` (recognized in context, not reserved: `GFX LINE`, `TILE SET`, `SPRITE n,MOVETO`, `ON COLLISION`), and the functions
   take the name of the value as a string literal (`SPRITE("X",3)`). `SYMBOL` was reserved already. `@` at the start of a statement is a picture row.
3. **The statements are a table** (`gfxops.c`): group, name, operation number, a signature string (`n` number, `o` optional number, `s` string,
   `v` string variable that receives the result, `w`/`W` a word out of a list). The parser, the code generator, the VM and the hosts all read it, so a new
   statement is one line of the table plus a case in the host. Optional trailing arguments are `GFX_NONE` (-1e9) in the host call.
4. **The VM talks to the screen through `vm_gfx_t`** (`op` and `query` callbacks, like `vm_sound_t`; `repl_t.gfx`); `engine1999.c` maps them to `sdlscr_*`
   (`host_gfx_op`, `host_gfx_query`). The VM checks the argument types and ranges, so the host can rely on them. Without a host the statements check their
   arguments and do nothing, the functions return 0.
5. **Drawing algorithms live in `gfxcore.[ch]`** (no SDL), tested on a plain buffer (`gfxtest1`); the backends `tilescreen`, `sprscreen`, `textscreen` have no SDL either.
6. **Images are binary strings**: a tile or sprite image is 384 bytes (16 x 24 palette indices, row by row), a graphics block `MKI$(w)+MKI$(h)+pixels`, the
   text screen 8000 bytes (4 per cell: character, pen, paper, 0). Files come in through `OPEN`/`INPUT$` (from `res:`) and go out through `PRINT #`.

## Statements

`n` = numeric expression, `[ ]` = optional (an empty place between commas is allowed: `GFX LINE 1,2,3,4,,BF`). Coordinates are pixels, 0..639 by 0..299, origin top left;
drawing is clipped, never an error. `c` is a palette index (omitted: the pen). Out of range arguments are "Illegal argument".

```
-- layers, palettes
GFX LAYER n, ON | OFF                 layer 0..4 shown / hidden
GFX PRIORITY n, p                     the layer n takes place p (0 = bottom) and swaps with the one that had it
GFX PALETTE n, i, argb                color i (0..255) of layer n; argb as RGB() gives it
GFX PALNAME n, i, a$                  the same by a name of the color table: GFX PALNAME 1,5,"Gold" (an unknown name is an error)
GFX PALRESET [n]                      the palette of layer n (or all) back to the table
GFX BACKDROP argb                     the color behind all layers
-- the graphics layer (and the background layer)
GFX TARGET 0 | 3                      where the statements below draw (stays until changed, RUN does not reset it)
GFX PEN c                             the color used when a statement gives none
GFX MODE SET|CLEAR|XOR|OR|AND         how pixels combine (all drawing statements)
GFX CLEAR [c]                         fills the target layer (default 0 = transparent)
GFX PSET x, y [, c]                   a pixel                      POINT(x, y) reads one back (-1 outside)
GFX LINE x1,y1,x2,y2 [, c [, B|BF]]   a line, a box (B) or a filled box (BF)
GFX CIRCLE x,y,r [, c [, a1, a2 [, ratio]]]   circle, arc (angles in degrees, 0 = right, counterclockwise) or ellipse (vertical radius r * ratio)
GFX PAINT x, y, c [, border]          flood fill from x,y: without border it replaces the area of the color at x,y, else it fills up to the border color
GFX RADIUS x, y, r, angle [, c]       a line from the center at an angle in degrees
GFX GRAB x1,y1,x2,y2, a$              the block into a string variable (MKI$(w) MKI$(h) w*h bytes, at most 65531 pixels)
GFX BLIT a$, x, y [, TRANS]           the block from a string; TRANS skips index 0
GFX TEXT x, y, a$ [, c [, sx [, sy]]] the 8 x 12 font at pixel position, each pixel as sx * sy pixels (1..32)
GFX DRAW a$                           the GW-BASIC drawing language: U D L R E F G H n, M x,y (a sign makes it relative), B, N, C n, S n (quarters), A n (0..3)
GFX VSYNC                             waits for the next frame (60 Hz); a break key still works
GFX RESET [ALL]                       back to the defaults, to wipe the screen of everything a program drew (see below)
-- the text screen
CLS                                   clears the text screen
LOCATE x, y                           cursor to column x (0..79), row y (0..24)
COLOR fg [, bg]                       pen and paper for the following output
SYMBOL c, a$                          redefines character c with 12 row bytes (bit 7 = left); DESIGN CHAR does the same by picture
TXT FILL x1,y1,x2,y2 [, chr [, fg [, bg]]]   fills an area; an omitted part is left as it is (Simons' BASIC: FILL, FCHR, FCOL)
TXT COPY x1,y1,x2,y2, tx, ty          copies an area (may overlap)                 (MOVE)
TXT INVERT x1,y1,x2,y2                swaps pen and paper in an area              (INV)
TXT SAVE a$   /   TXT LOAD a$         the whole text screen as an 8000 byte string / back   (SCRSV, SCRLD)
-- tiles
TILE DEF n, a$                        tile image n (0..255) from 384 bytes;  TILE GET n, a$ reads it back
TILE SET x, y, n                      the cell (0..40, 0..12) of the visible tile screen shows tile n
TILE FILL a$                          all 41 * 13 = 533 cells from a string (row by row)
TILE SCROLL sx, sy                    scroll offset in pixels (taken modulo 16 and 24)
-- sprites
SPRITE DEF n, a$   SPRITE GET n, a$   sprite image n (0..255) from / to 384 bytes
SPRITE SEQ a, a$                      animation sequence a (0..255): the string holds 1..15 image numbers
SPRITE n, x, y [, image]              places sprite n (0..255) at x, y (-1024..1023), optionally with an image
SPRITE n, ON | OFF                    shows / hides it
SPRITE n, PRIORITY p                  0..7 (higher is in front)
SPRITE n, IMAGE i                     a fixed image
SPRITE n, ANIM a, length, speed       plays sequence a (length 0..15 images, speed 0..31 frames per image); IMAGE ends the animation and the other way round
SPRITE n, MOVETO x, y, ticks          the engine moves the sprite by itself in ticks/60 s (Simons' BASIC: RLOCMOB); a new position or MOVETO replaces it
SPRITE n, COLLISION OFF | ON [, TILES]  collision detection with other sprites (and, with TILES, with solid tile pixels)
-- pictures
DESIGN SPRITE n [, base]              see below
DESIGN TILE n [, base]
DESIGN CHAR c
@ "row"
-- events
ON COLLISION GOSUB|GOTO target        a handler for any new collision; ON COLLISION OFF clears it (like ON ERROR and AFTER/EVERY)
```

Functions (the name of the value is a string literal, so the compiler checks it; a wrong one is a compile error):

```
RGB(r, g, b [, a])                    0..255 each (a defaults to 255): the ARGB value for GFX PALETTE / BACKDROP
POINT(x, y)                           the palette index at x, y of the target layer, -1 outside
GFX("FRAME")  GFX("LAYER", n)  GFX("PRIORITY", n)  GFX("PALETTE", layer, i)  GFX("TARGET")      frame counter, -1/0 shown, priority, ARGB, target layer
TXT("X") TXT("Y") TXT("FG") TXT("BG") TXT("WIDTH") TXT("HEIGHT") TXT("CHAR",x,y) TXT("FGAT",x,y) TXT("BGAT",x,y)   cursor, colors, size, cell contents
TILE(x, y)                            the tile in a cell of the tile screen
SPRITE("X",n) ("Y",n) ("IMAGE",n) ("ON",n) ("MOVING",n) ("HITS",n) ("COLLIDE",n)   position, image, shown, moving, collision bits, the other sprite
```

`SPRITE("HITS", n)` returns the indicator bits since it was last asked and clears them (1 = hit another sprite, 2 = hit a tile). `SPRITE("COLLIDE", n)` is the number
of the sprite it overlapped at the last frame (-1 none). A collision means an overlap of **opaque pixels** (index not 0); a sprite is checked only with the flags set by
`SPRITE n,COLLISION ON`. The check runs once per frame in the screen thread (`sprscr_periodicals`) while the sprite layer is enabled.
`ON COLLISION`: the VM asks the host for a new collision about every 32 instructions (and while `WAIT`ing) and runs the handler between two statements, like a timer
handler (it has the highest priority); the program finds out which sprite with `SPRITE("HITS",n)`.

### `GFX RESET`

`GFX RESET` puts the graphics system back to its start state, so that nothing a program left behind stays on the screen:

- all five layers are shown again, in the original order (0 background .. 4 sprites);
- **all palettes** and the backdrop color are back to the color table (a changed palette would otherwise leave the next program with strange colors);
- the background and graphics bitmaps are cleared; target 3, pen 1, mode `SET`, and the `DRAW` position, color, scale and angle start again;
- the tile screen: all cells 0 and no scrolling;
- all 256 sprites: hidden, at 0,0, default image and animation, no movement, no collision flags, pending collision events dropped.

Kept: the **definitions**, because they are assets, not clutter: tile images, sprite images and animation sequences, redefined characters. `GFX RESET ALL` clears these too
(images to 0, sequences to image 0, the font back to the built-in one). **Not touched**: the text screen (use `CLS`; its colors stay), the `ON COLLISION` handler (it ends with
the run like all handlers), the contents of variables. A program that wants a clean start typically begins with `GFX RESET:CLS`.

### Pictures: `DESIGN` and `@` rows

```
100 DESIGN SPRITE 3              ' 24 rows of exactly 16 characters follow
110 @ "......1111......"
 ...
400 DESIGN TILE 7, 32            ' a tile; 32 is added to every non-zero value (palette window 32..47)
500 DESIGN CHAR 200              ' a character: 12 rows of 8 characters
```

- `@ "row"` is an *executed* statement: `DESIGN` starts collecting, every `@` row adds one, and when the last row has arrived the image is installed (through the same
  host operation as `SPRITE DEF`, `TILE DEF`, `SYMBOL`). Other statements, loops included, may stand between the rows (`FOR R=0 TO 11:@ "1111111122222222":NEXT R`).
  A second `DESIGN`, or the end of the run, drops a picture that is not finished. It works with `LIST`, `RENUM`, `MERGE` and `AUTO`.
- Row characters: `.` or space = 0 (transparent), `1`..`9` = 1..9, `A`..`F` (also lower case) = 10..15, `o` = 1. The base is added to every non-zero value (the sum may not
  exceed 255). For all 256 values use the string forms, which are the same images as binary strings.
- Errors: "Bad design row" (wrong length, a bad character, base overflow), "Design without DESIGN".

## Differences from Simons' BASIC

Simons' BASIC (1983) is the model of `DESIGN` and of the command groups; only the keyword list and one-line descriptions were available, so the syntax is our own.
`HIRES`/`MULTI`/`NRM`/`GRAPHICS` are layers here; `LOW COL`/`HI COL`/`CMOB`/`BCKGNDS` are palettes; `PLOT t` is `GFX PSET` with `GFX MODE`; `REC`/`BLOCK` are `GFX LINE ...,B|BF`;
`ANGL` is `GFX RADIUS`; `CHAR`/`TEXT` are `GFX TEXT`; `FILL`/`FCHR`/`FCOL`/`MOVE`/`INV`/`SCRSV`/`SCRLD` are the `TXT` statements; `MOB SET`/`MMOB`/`MOB OFF` are the `SPRITE` forms,
`RLOCMOB` is `MOVETO`, `DETECT`/`CHECK` are `COLLISION` and `HITS` (plus the `ON COLLISION` event). Not taken: `FLASH` (use `EVERY` and `GFX PALETTE`), `CSET` (a second font set
would need backend work), `JOY`/`POT`/`PENX`/`PENY` (input, not graphics: a separate small proposal for joystick and mouse), and the structure commands (`PROC`, `LOOP`, `ON KEY`: roadmap).

## The internal API of `sdlscreen`

At the end of `sdlscreen.h` (`sdlscr_layer_*`, `sdlscr_palette_*`, `sdlscr_setbackdrop`, `sdlscr_gfx_*`, `sdlscr_text_*`, `sdlscr_tile_*`, `sdlscr_sprite_*`, `sdlscr_vsync`),
next to the older text, tile and sprite functions. The drawing functions take the layer (0 or 3) as a parameter; the BASIC statements use `sdlscr_gfx_target()`, `sdlscr_gfx_setpen()`
and `sdlscr_gfx_setmode()`, kept in `sdlscreen.c`. The text functions cover what the editors need (screens, viewports, cells), so `fulledit.c` and `monitor.c` call no `txtscr_*`
any more (they include `textscreen.h` for the `TXTSCR_*` constants only).

## Backends and what was added

- `gfxcore.[ch]` (new): `gfx_pset/point/line/box/circle/radius/paint/grab/blit/text/draw` on a `uint8_t` bitmap with a mode, a pen and the DRAW state; modes `GFXM_*`.
- `gfxops.[ch]` (new): the statement and query tables, the `GFXOP_*` and `GFXQ_*` numbers.
- `tilescreen`: `tilescr_setcell/getcell/setcells/readtile/pixel` (the cell contents could not be set before; `tilescr_pixel` is what `tilescr_render` draws, for tile collisions).
- `sprscreen`: a fixed image per sprite (`sprscr_setimage`, ends with `sprscr_animcfg`), position and image read-back, `sprscr_moveto` (16.16 fixed point, one step per frame), collision flags,
  per-pixel sprite-sprite and sprite-tile checks per frame, latched indicator bits (`sprscr_hits`), `sprscr_collidewith`, `sprscr_nextevent` for `ON COLLISION`.
- `textscreen`: `txtscr_fill/copy/invert/save/restore/defchar/font`; a **bug fixed**: `txtscr_write` sign-extended characters >= 128 into the cell's colors.
- `sdllayer`: `sdllay_findcolor` (names), `sdllay_reset_palette`.
- VM: `vm_gfx_t`, instructions `GFXC` (statement, parameter = operation), `GFXQ` (function), `DSGN`, `DROW`, `COLT`; the collision handler lives in timer slot 4 (`VM_COLSLOT`); `RGB()` is computed by the VM.

## Decisions (project owner, 2026-10-31)

0. The internal `sdlscr_*` API as specified: accepted. 1. GW-BASIC style names, `GRAB`/`BLIT` instead of `GET`/`PUT`. 2. Palette indices per layer. 3. `GFX TARGET` 0 and 3 only.
4. As few reserved words as possible (hence the umbrella words). 5. The order of the phases is fine. 6. Own asset formats (raw strings); standalone conversion tools are to be written later
   (roadmap), and **built-in sprite and tile editors are planned**: they will use this API (`DESIGN`'s rows, `TILE GET`/`SPRITE GET`, the string forms) and need a way to save images, which is
   why every image can be read back as a string. 7. Implement the Simons' BASIC ideas, all of them if possible.

## Open

- Asset tools (images to `DESIGN` rows or image strings), the sprite and tile editors; an image file format with palette and frames (an own format, `.spr`).
- Joystick and mouse input (`JOY`, `MOUSEX`...); `ON KEY`; `PALETTE CYCLE` (flashing); a second font set; sprite scaling or flipping; more than one tile map (the map selection of `concept.txt`).
- Synchronization of the layer memory between the main and the screen thread (all calls are already funneled through `sdlscreen`).
