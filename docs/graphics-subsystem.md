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
3. **All calls into the graphics modules go through `sdlscreen`** (owner's requirement, 2026-10-31). No caller outside
   `sdlscreen` (`engine1999.c`, the VM host mapping, the editors `fulledit.c` and `monitor.c`, tests of the engine) calls `sdllayer`, `tilescreen`,
   `sprscreen`, `textscreen` or `gfxcore` directly, only `sdlscr_*`. Today `engine1999.c` already obeys this, but `fulledit.c` and `monitor.c`
   call `txtscr_*` directly (about 60 calls: viewports, screen select, colors, write, clear), so they must be converted. `sdlscreen`
   is the single place where a lock, a command queue or a frame handshake can be added later (today there is none, by decision), without touching
   any caller. **So the internal `sdlscr_*` API is specified first** (next section), before any BASIC statement is built on it.
4. **The VM talks to the screen through a host interface** (`vm_gfx_t`, like `vm_sound_t`), which `engine1999` maps to that `sdlscr_*` API.
   The drawing algorithms live in a module without SDL (`gfxcore.[ch]`, working on a plain `uint8_t[640*300]` buffer) so that they are tested
   headless like `synth.c`; only `sdlscreen` calls it, on the buffers of the layers it owns. The tests use a plain buffer and compare pixels.
5. **Images are binary strings** (now possible): a tile or sprite image is a 384 byte string (16 x 24, one palette index per byte, row by row),
   a graphics block is `4 + w*h` bytes (`MKI$(w)+MKI$(h)+pixels`). Files come in through `OPEN ... FOR INPUT` and `INPUT$`, from `res:`.

## The internal API of `sdlscreen` (to be specified first)

Today `sdlscreen.h` has the text functions (`sdlscr_printf`, `sdlscr_puts`, `sdlscr_ansi_*`, `sdlscr_locate`, ...), tile functions
(`sdlscr_writetile`, `sdlscr_scrolltiles`) and sprite functions (`sdlscr_showsprite`, ... `sdlscr_writesprite`); the layer array
(`layers[]`, `sdlscr_bgcol`) is `static` in `sdlscreen.c`, which is why this is the right choke point. Rules for the API:

- Every function is `sdlscr_<noun>_<verb>`-style like the existing ones, takes plain C types (no `sdllayer_t*` or SDL types leak out), validates
  its arguments (out of range = ignored or `false`, never a crash) and **marks the layer it changed as modified** itself, so callers never
  call `sdllay_set_modified`.
- Whatever synchronization is needed later (a mutex around layer memory, a command queue drained once per frame, double buffering) is added inside
  these functions; the signatures must therefore not hand out pointers into layer memory. Reads copy.
- The functions are also the test seam: a headless build of `sdlscreen` is not planned, but `vm_gfx_t` (VM side) is implemented by thin wrappers
  over exactly these functions, so the API listed here is everything the VM can ask for.

Proposed set (new unless an existing name is given; `layer` is 0..4 as in the table above):

```
// layers and palette
void     sdlscr_layer_enable( int layer, bool enable );
bool     sdlscr_layer_enabled( int layer );
void     sdlscr_layer_setpriority( int layer, int prio );     // swaps with the layer that has prio (sdllay_switch_priority)
int      sdlscr_layer_getpriority( int layer );
void     sdlscr_palette_set( int layer, int index, uint32_t argb );
uint32_t sdlscr_palette_get( int layer, int index );
void     sdlscr_palette_reset( int layer );                   // layer -1 = all
bool     sdlscr_palette_setbyname( int layer, int index, const char* name );   // the names of colinittab; false if unknown
void     sdlscr_setbackdrop( uint32_t argb );                 // replaces the static sdlscr_bgcol

// bitmap layers (0 background, 3 graphics): the drawing statements, all clipped to 640 x 300
void     sdlscr_gfx_clear( int layer, int color );
void     sdlscr_gfx_pset( int layer, int x, int y, int color );
int      sdlscr_gfx_point( int layer, int x, int y );         // -1 outside
void     sdlscr_gfx_line( int layer, int x1, int y1, int x2, int y2, int color );
void     sdlscr_gfx_box( int layer, int x1, int y1, int x2, int y2, int color, bool fill );
void     sdlscr_gfx_circle( int layer, int x, int y, int r, int color, int a1, int a2, double ratio );
void     sdlscr_gfx_paint( int layer, int x, int y, int color, int border );   // border -1 = the color at x,y
size_t   sdlscr_gfx_grab( int layer, int x1, int y1, int x2, int y2, uint8_t* out, size_t outsize );   // copy out; returns the size needed
void     sdlscr_gfx_blit( int layer, const uint8_t* block, size_t size, int x, int y, int mode );
void     sdlscr_gfx_setmode( int mode );                      // how pixels are combined: SET (default), CLEAR, XOR, OR, AND (Simons' BASIC: PLOT's third parameter)
void     sdlscr_gfx_radius( int layer, int x, int y, int r, double angle, int color );   // a line from the centre at an angle (ANGL)
void     sdlscr_gfx_text( int layer, int x, int y, const char* text, size_t len, int color, int sx, int sy );  // TEXT / CHAR: the 8x12 font, scaled

// text screen (existing: sdlscr_locate, sdlscr_gettextcursor, sdlscr_gettextscreensize, sdlscr_printf/puts/ansi_*)
// the functions fulledit.c and monitor.c use today through txtscr_*: screens (0 BASIC, 1 editor, 2 monitor), viewports, cells
int      sdlscr_text_select( int screen );                    // returns the previously selected screen
int      sdlscr_text_selected( void );
bool     sdlscr_text_setviewport( int x, int y, int w, int h );
void     sdlscr_text_resetviewport( void );
void     sdlscr_text_getviewport( int* x, int* y, int* w, int* h );
void     sdlscr_text_setblank( int bg, int fg );
int      sdlscr_text_write( int y, int x, int bg, int fg, const char* text, int len );   // no wrap; returns the characters written
uint32_t sdlscr_text_getcell( int y, int x );                 // a textcell_t (character, colors)
void     sdlscr_text_getcursor( int* x, int* y );
void     sdlscr_text_clear( void );
void     sdlscr_text_setcolors( int bg, int fg );
void     sdlscr_text_getcolors( int* bg, int* fg );
void     sdlscr_text_defchar( int code, const uint8_t rows[12] );              // SYMBOL, DESIGN CHAR
void     sdlscr_text_fill( int x1, int y1, int x2, int y2, int ch, int fg, int bg );   // FILL / FCHR / FCOL: -1 = leave that part alone
void     sdlscr_text_copy( int x1, int y1, int x2, int y2, int tx, int ty );            // MOVE: overlap-safe
void     sdlscr_text_invert( int x1, int y1, int x2, int y2 );                          // INV: swaps pen and paper
size_t   sdlscr_text_save( uint8_t* out, size_t outsize );                              // SCRSV: the whole screen as 4 bytes per cell; returns the size needed
bool     sdlscr_text_restore( const uint8_t* data, size_t size );

// tiles (existing: sdlscr_writetile, sdlscr_scrolltiles)
void     sdlscr_tile_setcell( int x, int y, int tile );       // new backend: tilescr_setcell
int      sdlscr_tile_getcell( int x, int y );
void     sdlscr_tile_setcells( const uint8_t* cells, size_t count );   // 41 * 13
bool     sdlscr_tile_getimage( int tile, uint8_t out[384] );   // read back (TILE$)

// sprites (existing: sdlscr_showsprite, sdlscr_spriteprio, sdlscr_movesprite, sdlscr_spriteanimdata, sdlscr_spriteanimcfg, sdlscr_writesprite)
void     sdlscr_sprite_setimage( int sprno, int image );
bool     sdlscr_sprite_getpos( int sprno, int* x, int* y );
bool     sdlscr_sprite_getimage( int image, uint8_t out[384] );   // read back (SPRITE$)
void     sdlscr_sprite_moveto( int sprno, int x, int y, int ticks );   // RLOCMOB: the screen thread moves it by itself in ticks/60 s (sprscr_periodicals)
bool     sdlscr_sprite_moving( int sprno );
void     sdlscr_sprite_setcollide( int sprno, bool sprites, bool tiles );   // DETECT: the flags of concept.txt
uint32_t sdlscr_sprite_hits( int sprno );                     // CHECK: bit 0 sprite hit, bit 1 tile hit, then cleared (the indicator bits of concept.txt)
int      sdlscr_sprite_collide( int sprno );                  // another sprite's number, -1 none (new backend)
int      sdlscr_sprite_collide_tiles( int sprno );            // tile number or 0 (new backend)

// timing
void     sdlscr_vsync( void );                                // waits for the next frame; false/early return when the screen closed
uint64_t sdlscr_getframecnt( void );                          // existing
```

The existing functions keep their names (they are used by `sdltest1`, `engine1999`, the editor and the monitor); the new ones follow
the same style. `fulledit.c` and `monitor.c` use `txtscr_*` directly today (and `sdlscr_textchanged()` to get the layer redrawn); they are converted
to the `sdlscr_text_*` functions above when this API is built, which also makes `sdlscr_textchanged()` unnecessary (each function marks the layer
modified), so that the rule holds without exceptions. `textscreen.c` itself stays free of SDL and keeps its own tests (`txttest1`).

## Ideas from Simons' BASIC

Simons' BASIC (David Simons, 1983, a 114 command extension of Commodore BASIC V2 for the C64) is the model for the *definition* statements and a
list of further graphics commands worth considering. **Source and limits:** I could read the keyword list and the one-line description of each
command (c64-wiki and Wikipedia), not the user manual; the argument syntax below is **our own design in the spirit of those commands**,
not a copy, and where a Simons' BASIC command is mentioned only its purpose is taken from the sources.

### Definitions by picture: `DESIGN` and `@` rows

In Simons' BASIC `DESIGN` defines a sprite or a character from `@` lines that follow it, so the picture is visible in the listing. The same here:

```
100 DESIGN SPRITE 3              ' a 16 x 24 sprite image: exactly 24 rows of exactly 16 characters follow
110 @ "......1111......"
120 @ "....11111111...."
  ...                            ' (24 rows)
340 @ "......1111......"
400 DESIGN TILE 7, 32            ' a tile image, 24 rows of 16; 32 is added to every non-zero color (palette window 32..47)
  ...
500 DESIGN CHAR 200              ' a character, 12 rows of 8 characters (the notation of 8x12font1.txt: . and o)
510 @ "..oooo.."
  ...
```

- `@ "row"` is a statement of its own (the token `@` is today only the address-of operator in expressions; at the start of a statement it is free).
  It is an *executed* statement, not DATA: `DESIGN` starts collecting, every `@` row adds one row, and when the last row has arrived the image is installed
  (tile, sprite image or font character). This needs no compile time knowledge of neighbouring lines, works with `LIST`, `RENUM`, `MERGE` and `AUTO`,
  and a row of the wrong length or with a bad character is a run time error ("Bad design row") with the line number of that row.
- Row characters: `.` (or a space) = 0 = transparent, `1`..`9` = 1..9, `A`..`F` (also lower case) = 10..15, `o` = 1. The optional second number of `DESIGN`
  is added to every non-zero value, so one picture can use any 16 colors of the 256 of the layer's palette. For all 256 values use the string forms
  (`SPRDEF n, a$`, `TILE n, a$`), which are the same images as binary strings.
- Unrelated statements between the rows are allowed (so a loop can generate rows). A second `DESIGN` or the end of the run drops a picture that is not
  finished; an `@` row without an open `DESIGN` is "Design without DESIGN".
- Reading back: `a$ = TILE$(n)` and `a$ = SPRITE$(n)` return the 384 byte image (to save it with `PRINT #`, or to copy it).

### Sprites by Simons' BASIC's model

| Simons' BASIC (purpose) | Proposed here |
|---|---|
| `MOB SET` (enable a sprite, set attributes) | `SPRITE n, ON` and `SPRITE n, x, y, image` / `PRIORITY` / `ANIM` as in phase 3 |
| `MMOB` (place a sprite) | `SPRITE n, x, y` |
| `RLOCMOB` (move a sprite smoothly from one place to another) | **`SPRITE n, MOVETO x, y, ticks`**: the engine moves the sprite by itself over ticks/60 s (`sprscr_periodicals`, which already runs once per frame); `MOVING(n)` tells whether it is still on its way. Needs the new backend function; very useful because the program can do other things meanwhile |
| `CMOB` (the two global multicolor colors) | not needed: every layer has a 256 color palette (`PALETTE`) |
| `MOB OFF` | `SPRITE n, OFF` |
| `DETECT` / `CHECK` (collision detection / test) | `SPRITE n, COLLISION ON|OFF` sets the flags of `concept.txt`, `HITS(n)` returns and clears the indicator bits (1 sprite, 2 tile) and `COLLIDE(n)` names the other sprite; plus an event form **`ON COLLISION GOSUB target`** built on the machinery of `ON ERROR` and `AFTER`/`EVERY` (checked between statements; `ON COLLISION OFF`) |

### Other graphics commands of Simons' BASIC worth taking (mapped to our words)

| Simons' BASIC (purpose) | Here |
|---|---|
| `HIRES`, `MULTI`, `NRM`, `GRAPHICS` (select graphics or text screen) | not modes here: `LAYER 3, ON|OFF` and `LAYER 2, ON|OFF` |
| `LOW COL` / `HI COL` (change the plotting colors) | `PALETTE` |
| `PLOT x,y,t` (set, clear or invert a pixel) | `PSET` plus **`GMODE SET|CLEAR|XOR|OR|AND`**, which applies to all drawing statements (also `LINE`, `CIRCLE`, `BLIT`): XOR drawing is how games erase without a background copy |
| `LINE`, `CIRCLE`, `ARC`, `PAINT`, `TEST` | `LINE`, `CIRCLE` (arc parameters), `PAINT`, `POINT()` as in phase 2 |
| `ANGL x,y,angle,radius` (a line from the center at an angle) | **`RADIUS x, y, r, angle [, c]`** (angle in degrees) |
| `REC` (hollow rectangle), `BLOCK` (solid rectangle) | `LINE ..., B` and `BF` as in phase 2 |
| `DRAW`, `ROT` (a drawing language with scale and rotation) | `DRAW a$` (GW-BASIC language, which has `S` scale and `A`/`TA` angle) |
| `CHAR`, `TEXT` (plot text on the high resolution screen, with size) | **`GTEXT x, y, a$ [, c [, sx [, sy]]]`**: the 8 x 12 font on the graphics layer at pixel positions, scaled by whole factors, so a game can mix text and graphics without cell alignment |
| `FCHR`, `FCOL`, `FILL` (fill an area of the text screen with a character and / or color) | **`TFILL x1,y1,x2,y2 [, chr [, fg [, bg]]]`** (an omitted part is left as it is) |
| `MOVE` (copy a section of the screen) | **`TCOPY x1,y1,x2,y2, tx,ty`** (text screen) |
| `INV` (reverse an area) | **`TINVERT x1,y1,x2,y2`** |
| `SCRSV` / `SCRLD` (save / load the text screen) | **`a$ = TSAVE$`** and **`TLOAD a$`**: the text screen as a binary string (4 bytes per cell), so it can be written to a file, kept in a variable or sent over `tcp:` |
| `CSET` (select a character set), `DESIGN` for characters | `DESIGN CHAR`, `SYMBOL`; a second font set would need backend work (not proposed now) |
| `FLASH` / `OFF` (flashing screen color) | not as such: `EVERY 30 GOSUB` + `PALETTE` does it; a cycling palette entry could be added later (`PALETTE CYCLE`) |
| `BCKGNDS` (extended background colors) | not needed: every text cell has its own pen and paper from 256 |
| `JOY`, `POT`, `PENX`/`PENY` (joystick, paddle, light pen) | input, not graphics: `JOY(n)`, `MOUSEX`, `MOUSEY`, `MOUSEB` through SDL (joysticks and the mouse are not used yet); a separate small proposal |
| `SPLIT`-style modes | not in the sources I read; no equivalent needed (layers are independent) |

Not graphics, only mentioned because the command list has them: `PROC`/`END PROC`/`CALL`/`EXEC`/`LOCAL`/`GLOBAL`, `LOOP`/`EXIT IF`/`END LOOP`,
`ON KEY` (a key event handler: it would fit next to `AFTER`/`EVERY` and `ON ERROR`), `PAUSE`, `TRACE`, `MEM`, `PLACE` (`INSTR`), `DIV`/`MOD`. They belong on the
roadmap as separate items, not in this subsystem.

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
SYMBOL c, r0, r1, ... r11      redefine character c (12 row bytes), already planned; or SYMBOL c, a$ (12 bytes); or DESIGN CHAR c with @ rows
TFILL x1,y1,x2,y2 [,chr[,fg[,bg]]]    TCOPY x1,y1,x2,y2,tx,ty    TINVERT x1,y1,x2,y2    a$=TSAVE$   TLOAD a$     (Simons' BASIC's FILL/MOVE/INV/SCRSV/SCRLD)
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
GMODE m                        how pixels combine: SET (default), CLEAR, XOR, OR, AND (applies to PSET, LINE, CIRCLE, PAINT, BLIT, GTEXT)
RADIUS x, y, r, angle [, c]    a line from the centre at an angle in degrees (Simons' BASIC: ANGL)
GTEXT x, y, a$ [, c [, sx [, sy]]]   text from the 8 x 12 font at pixel position x,y, scaled by sx, sy (default 1)
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

DESIGN TILE n [, base]         define tile n from 24 following  @ "16 characters"  rows (see "Ideas from Simons' BASIC")
DESIGN SPRITE n [, base]       the same for sprite image n;  DESIGN CHAR c  for a character (12 rows of 8)
a$ = TILE$(n)   a$ = SPRITE$(n)    the 384 byte image back

SPRDEF n, a$                   define sprite image n (0..255) from 384 bytes               (sprscr_writemap)
SPRITE n, x, y [, image]       place sprite n (0..255) at x,y (-1024..1023), optionally set its image
SPRITE n, ON | OFF             show / hide
SPRITE n, PRIORITY p           0..7
ANIM a, a$                     animation sequence a (0..255): the string holds up to 15 image numbers   (sprscr_animdata)
SPRITE n, ANIM a, length, speed  attach it (sprscr_animcfg)
SPRITE n, MOVETO x, y, ticks   the engine moves the sprite to x,y in ticks/60 s (Simons' BASIC: RLOCMOB);  MOVING(n) = -1 while it is on its way
SPRITE n, COLLISION ON | OFF   collision detection for sprite n (DETECT)
h = HITS(n)                    the collision indicator bits (1 = a sprite, 2 = a tile) and clears them (CHECK)
ON COLLISION GOSUB target      an event handler for any detected collision, ON COLLISION OFF (like ON ERROR / AFTER)
x = SPRX(n)   y = SPRY(n)      position back
c = COLLIDE(n)                 the number of another sprite that overlaps sprite n (-1 = none), by opaque pixels; needs the collision backend
c = COLLIDE(n, TILES)          0 or the tile number sprite n touches (index 0 pixels ignored)
```

To keep the words few, `SPRITE` is a statement whose second item may be a sub-keyword (`ON`, `OFF`, `PRIORITY`, `ANIM`) and `LAYER`/`PALETTE`/
`TILE` likewise; they are recognized in context like `RANDOM`/`UPDATE` in `OPEN` and are not reserved themselves. New reserved words
would be: `LAYER`, `PALETTE`, `BACKDROP`, `RGB`, `CLS`, `LOCATE`, `COLOR`, `POS`, `CSRLIN`, `GCOLOR`, `GTARGET`, `GCLS`, `PSET`, `POINT`,
`CIRCLE`, `PAINT`, `GRAB`, `BLIT`, `GMODE`, `RADIUS`, `GTEXT`, `TFILL`, `TCOPY`, `TINVERT`, `TSAVE$`, `TLOAD`, `DESIGN`, `TILE`, `TILE$`, `SPRITE$`, `TILEMAP`, `TILESCROLL`, `TILEAT`, `SPRDEF`, `SPRITE`, `ANIM`, `SPRX`, `SPRY`, `COLLIDE`, `HITS`, `MOVING`, `VSYNC`, `SYMBOL` (`COLLISION`, `MOVETO`, `SET`, `CLEAR`, `XOR` are recognized in context)
(about 40 now, comparable to what the sound statements and the I/O subsystem added). Whether a few should be avoided by a prefix form
(`GFX LINE ...`) is a decision for the owner.

## Timing

```
VSYNC                          waits for the next frame (60 Hz, SDLEV_VBLANK); a break key still works
n = FRAME                      the frame counter (sdlscr_getframecnt), for animation without TI
```

`WAIT n` (1/60 s units) and the timers `AFTER`/`EVERY` already exist; `VSYNC` is the one that is locked to the display and the sprite animation.
A sprite or tile change is visible with the next frame.

## Backend work (in order)

1. **Specify the internal `sdlscreen` API** (previous section) and review it with the owner; then add the declarations to `sdlscreen.h` with stub
   bodies that do the validation and modified-flag handling, and convert the direct `txtscr_*` calls of the host to it.
2. `gfxcore.[ch]` (no SDL): `PSET/LINE/CIRCLE/PAINT/GRAB/BLIT` on a buffer; tests with pixel comparisons (`gfxtest1`); called only from `sdlscreen`.
3. Fill in the `sdlscreen` functions: palette, layer enable/priority, backdrop, vsync, the tile cell setter in `tilescreen.c`, position getters in
   `sprscreen.c`, sprite collision.
4. `vm_gfx_t` in `vm.h`, `repl_t.gfx`, instructions (one per group with a kind parameter, like `SND` / `FOP`: e.g. `GFX`, `SPR`, `PAL`), parser and code generator.
5. Docs, tests (`vmtest1` with a logging gfx host like the sound host), a demo (`demos/`), checked on the real screen.

## Decisions for the project owner

0. **The `sdlscr_*` API** of the section above: complete and named as you want it? (It is the first thing to be built.)
1. **Names**: GW-BASIC style (`PSET`, `LINE`, `CIRCLE`, `PAINT`; `GRAB`/`BLIT` instead of `GET`/`PUT`) as above, or something else?
2. **Colors**: palette indices per layer (proposal) or one global 256-color table for all layers? (The layers have separate palettes today.)
   `BACKDROP` takes an RGB value in the proposal because layer 0 is a bitmap, not a single color.
3. **Target layer**: only the graphics layer is drawn on, plus layer 0 by `GTARGET`, or a general `TARGET n` that could also draw into the text
   layer's pixels (not meaningful) -- the proposal says only 0 and 3.
4. **Reserved words**: ~30 new words as listed, or sub-keyword forms (`GFX LINE`, `SPR ...`) with fewer?
5. **Order**: phase 1 (layers, palette, text screen words) first, then 2, then 3, with the collision backend last?
6. **Asset formats**: raw strings only (proposal), or also an image loader (PNG needs a library; a small own format `.spr` with palette and
   frames could be added later)?
7. **Simons' BASIC ideas** (previous section): `DESIGN` with `@ "row"` statements (executed, not DATA) and the row alphabet `. 1-9 A-F o`
   plus a color base; engine-driven `MOVETO`; `ON COLLISION GOSUB`; `GMODE`; `GTEXT`; the text area commands `TFILL`/`TCOPY`/`TINVERT`/`TSAVE$`/`TLOAD`.
   Which of them do you want in, and is the `@ "row"` form right (alternatives: a `DATA`-style row list read by `DESIGN`, or one `DESIGN` statement with the
   rows as string arguments)? Also: should I fetch the real manual (archive.org, a PDF I could not read here) and align the argument orders with it where that makes sense?
