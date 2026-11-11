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
| 4 | sprites | 256 sprites of 16 x 24 px (248 for programs, 8 reserved for the engine), priority, animation sequences, collisions | `sprscr_render` |

Behind them is the backdrop color (`GFX BACKDROP`). **Every layer is 640 x 300 bytes of palette indices with its own 256 entry ARGB palette**
(`sdllayer_t.palette`, index 0 = transparent by default; the names of the color table, such as "Gold", can be used by `GFX PALNAME`).
Colors in all statements are palette indices of the layer that is drawn on.

The 3D sector renderer (`W3D`, `w3d.[ch]`) draws into a view on the target graphics layer through `gfx_putbuf()` / `sdlscr_gfx_putbuf()`; it is described in [3d-subsystem.md](3d-subsystem.md).

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
GFX LOAD a$ [, x, y [, PALETTE]]      a BMP file onto the target layer at x, y (see "Picture files")
GFX SAVE a$ [, x1, y1, x2, y2]        the target layer (or a rectangle of it) as an 8 bit BMP file
GFX PALLOAD layer, a$                 the colors of a BMP file into the palette of a layer
GFX PALSAVE layer, a$                 the palette of a layer as a 16 x 16 picture
SPRITE LOAD a$, first [, count [, PALETTE]]   a BMP sheet cut into sprite images; SPRITE SAVE a$, first, count [, cols] writes them
TILE LOAD a$, first [, count [, PALETTE]]     the same for tiles;                  TILE SAVE a$, first, count [, cols]
-- the text screen
CLS                                   clears the text screen
LOCATE x, y                           cursor to column x (0..79), row y (0..24)
COLOR fg [, bg]                       pen and paper for the following output (PRINT too, whatever was printed before)
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
SPRITE n, x, y [, image]              places sprite n (0..247) at x, y (-1024..1023), optionally with an image; 248..255 are reserved
SPRITE n, ON | OFF                    shows / hides it
SPRITE n, PRIORITY p                  0..7 (higher is in front)
SPRITE n, IMAGE i                     a fixed image
SPRITE n, ANIM a, length, speed       plays sequence a (length 0..15 images, speed 0..31 frames per image); IMAGE ends the animation and the other way round
SPRITE n, PATH a$                     a path the engine runs by itself (X+3*20 W 5 [ ... ]3 E 1 R ...); "" stops it (see "Sprite paths")
SPRITE n, SIZE w, h                   a group: the sprites n .. n+w*h-1 are drawn as one large sprite (see "Sprite groups"); 1, 1 dissolves it
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
TXT("X") TXT("Y") TXT("FG") TXT("BG") TXT("WIDTH") TXT("HEIGHT") TXT("CHAR",x,y) TXT("FGAT",x,y) TXT("BGAT",x,y) TXT("GLYPH",c,row) TXT("BUILTIN",c,row)   cursor, colors, size, cell contents
TILE(x, y)                            the tile in a cell of the tile screen
SPRITE("X",n) ("Y",n) ("IMAGE",n) ("ON",n) ("MOVING",n) ("HITS",n) ("COLLIDE",n)   position, image, shown, moving, collision bits, the other sprite
SPRITE("COLS",n) ("ROWS",n)           the size of the group that n heads, in sprites (1 and 1 if there is none)
```

### Picture files (2026-11-05, roadmap item 53)
Pictures are **Windows BMP files**, so any paint program can edit them (`bmp.[ch]`: the codec, no SDL; `gfxfile.[ch]`: the statements on top of it through a small table of functions, tested with a fake screen in `gfxtest1`; the host reads and writes the bytes through the I/O drivers, so `mem:` and the other drivers work, and `ADDRESS` applies). A name without an extension gets `.bmp`; a missing file is "File not found".
- **Reading:** 1, 4, 8, 16 (555, or 565 by bit fields), 24 and 32 bits per pixel, bottom-up or top-down, uncompressed (RLE is refused: "Compressed BMP files are not supported"); the file is checked against its size ("The BMP file is truncated"). A 32 bit picture has transparency if any alpha byte is not zero (many programs leave it zero: then it is opaque); the reserved byte of a palette entry is the alpha if any is not zero.
- **Matching:** the colors are matched to the palette *of the layer the pictures belong to* (graphics or background layer for `GFX LOAD`, the sprite layer's palette for sprites, the tile layer's for tiles): the nearest entry by weighted RGB distance; **entry 0 is transparent**: pixels with alpha < 128 become 0, no other color is ever matched to entry 0, and entries that are fully transparent are skipped. With **`PALETTE`** the palette of the file is installed in the layer first and an indexed picture keeps its indices (so index 0 of the file is the transparent one); a true color picture gets a palette made by a median cut into the entries 1..255. **Entry 0 is never given a color** (fixed 2026-11-05, found with the editor: an opaque entry 0 on the sprite layer covers the whole screen, because every pixel without a sprite is index 0): a true color picture leaves it as it is, and an indexed picture's entry 0 is installed transparent (its alpha is taken away).
- **`GFX LOAD a$ [,x,y [,PALETTE]]`** draws onto the target layer (`GFX TARGET`: 0 or 3) at x, y (default 0, 0), clipped; always as `SET`, whatever `GFX MODE` says. **`GFX SAVE a$ [,x1,y1,x2,y2]`** writes the whole layer or the rectangle as an 8 bit BMP with the layer's 256 colors (so loading it again with `PALETTE` gives the same picture).
- **Sheets:** `SPRITE LOAD a$,first[,count[,PALETTE]]` and `TILE LOAD` cut the picture into cells of 16 x 24 pixels, left to right and top to bottom, into the images `first ..`; without `count` all cells that fit (sprites up to image 247, tiles up to 255); too many is "Illegal argument". The images of a sprite group are consecutive, so a sheet of w x h cells loaded at the head's image is a group (item 62). `SPRITE SAVE a$,first,count[,cols]` and `TILE SAVE` write `count` images, `cols` per row (default 16 or fewer); a missing cell at the end is 0.
- **Palettes:** `GFX PALSAVE layer,a$` writes a 16 x 16 picture in which the cell in column c and row r has the color r * 16 + c, and the 256 entry palette; `GFX PALLOAD layer,a$` takes the palette of any indexed BMP (entries 0..n-1; a true color picture gives its 255 best colors from entry 1 on).
- **Not in this item:** the tile map (`TILE MAP LOAD` / `SAVE`: a program keeps its map in the RAM or in a file of its own), writing true color BMPs, the own container `.spr` with sequences (roadmap item 20), the editors (`spriteed` still saves `DESIGN` listings), and the console host (no layers: "Not available").

### Sprite groups (2026-11-05, roadmap item 62)
`SPRITE n,SIZE w,h` (w, h 1..8, at most 64 sprites; `sprscr_setsize()`) makes the sprite **n the head of a block of w x h sprites**: the sprites `n .. n+w*h-1`, row by row, are drawn side by side as **one large sprite** of 16 w x 24 h pixels. The head rules the block, so everything you do to it is done to the whole group:
- `SPRITE n,x,y` places it (member k, in column `k mod w` and row `k \ w`, is drawn at x + 16 column, y + 24 row), `ON` / `OFF` show and hide it, `PRIORITY` is the group's order in front of or behind other sprites, `MOVETO` moves it, `ANIM` plays a sequence;
- **images:** member k shows the head's image + k (the images of a block are consecutive, row by row: a 2 x 3 group at image 20 uses images 20..25; an animation sequence holds the *first* image of each frame, so frames of 6 images are 20, 26, 32, ...; beyond image 255 the number is held at 255);
- **collisions:** the head's flags (`SPRITE n,COLLISION ON [,TILES]`) apply to all members; every member collides pixel by pixel; the members of one group don't hit each other; hits, `SPRITE("HITS",n)`, `SPRITE("COLLIDE",n)` (the *head* of the other group) and the `ON COLLISION` event all name the head. `SPRITE("X"|"Y"|"IMAGE"|"ON",member)` give the place, image and state a member has now;
- the settings of a member (its own position, image, state, flags) are ignored while it belongs to a group; the members are not usable as sprites of their own until the group is dissolved with `SPRITE n,SIZE 1,1` (they then show what they were set to before);
- a group needs free sprites: a member can't be the head of another group or belong to one, and the block must end at sprite 247 ("Illegal argument ..."); `SPRITE n,SIZE` on a head with a group resizes it; `GFX RESET` dissolves all groups.
`SPRITE("COLS",n)` and `SPRITE("ROWS",n)` give the size of the group n heads. Tiles have no engine groups: the BASIC library `demos/lib/tilegrp.lib` (`TG_DEF`, `TG_PUT`, `TG_ERASE`, `TG_REPEAT`, `TG_HIT`, ...; prefix `TG_`) names blocks of tiles (w x h tiles from a base number, row by row) and puts them on the tile screen; `demos/groups.bas` shows both kinds, and `demos/spriteed.bas` edits blocks of both.

`SPRITE("HITS", n)` returns the indicator bits since it was last asked and clears them (1 = hit another sprite, 2 = hit a tile). `SPRITE("COLLIDE", n)` is the number
of the sprite it overlapped at the last frame (-1 none). A collision means an overlap of **opaque pixels** (index not 0); a sprite is checked only with the flags set by
`SPRITE n,COLLISION ON`. The check runs once per frame in the screen thread (`sprscr_periodicals`) while the sprite layer is enabled.
`ON COLLISION`: the VM asks the host for a new collision about every 32 instructions (and while `WAIT`ing) and runs the handler between two statements, like a timer
handler (it has the highest priority of the timers except `ON MOUSE`, which is above it); the program finds out which sprite with `SPRITE("HITS",n)`.

### Sprite paths (2026-11-05, roadmap item 52)
`SPRITE n,PATH a$` gives sprite n (0..247) a **path**: a string of commands that the engine runs by itself, one step per frame in the screen thread, like a `PLAY` string runs per note. The BASIC program does not move the sprite and uses no time for it. The string is translated completely first (`sprpath.c`): a mistake is an error at once ("? Unknown path command (at 5)"), nothing is changed then. A new path replaces the running one; `SPRITE n,PATH ""` stops it (the sprite stays where it is); `GFX RESET` stops all paths. `SPRITE("PATH",n)` is -1 while a path runs for the sprite.

| command | meaning |
|---|---|
| `X+n` `X-n` `Y+n` `Y-n` | move n pixels (relative) over T frames (see `T`; default 1 frame = a jump); `X+3*20`: 3 pixels **per frame** for 20 frames |
| `X=n` `Y=n` | set the coordinate at once |
| `M dx,dy` `M dx,dy*k` | move in both directions at once (same rules as `X` / `Y`) |
| `W n` | wait n frames |
| `A a,len,speed` | play animation sequence a (like `SPRITE n,ANIM`) |
| `D i` | show the fixed image i |
| `H` `S` | hide / show the sprite |
| `P n` | priority 0..7 |
| `E n` | event n (0..255) for the program |
| `T n` | the default duration of the following moves, in frames (1..65535) |
| `[ ... ]n` | repeat the commands between the brackets n times; nested up to 4 deep |
| `R` | start again from the beginning (forever) |

Letters in either case, blanks between commands are ignored, numbers are decimal. At the end of the string the path is finished. A move takes exactly its frames (the first step happens in the frame the command is taken), the target is exact; `W n` leaves n frames without a step. At most 4096 commands; a path of instant commands only (`[ X=1 ]1000`) is run 256 commands per frame, so it can't freeze the screen thread.
- **Events:** `E n` queues (sprite, n) in a ring of 256 (a full ring drops new events; the screen thread is the only writer, the program the only reader, no locks). `SPRITE("EVENT")` takes the oldest one and returns n (-1 = none), `SPRITE("EVENTSPR")` is the sprite of the event taken last. **`ON SPRITE GOSUB|GOTO|OFF`** (`VM_SPRSLOT` 9, `COLT` bit 64, `NT_ONSPRSTMT 0x90`) runs a handler when a path has queued an event, polled like `ON UI` (a change of `SPRITE("EVENTS")`, the number of events so far); the handler loops on `SPRITE("EVENT")` until it is -1 (see `demos/paths.bas`).
- **Overrides:** `H` `S` `P` `D` `A` do not change the sprite's own settings but override them (words that the screen thread alone writes, `sprscr_view()` applies them); the program's own `SPRITE n,ON|OFF|PRIORITY|IMAGE|ANIM` takes the override of that property away again.
- **Mixed with `MOVETO` or `SPRITE n,x,y`:** all three write the position, the last writer in a frame wins (a path that moves and a `MOVETO` at the same time fight). Paths on group heads move the whole group; a member's path is ignored while it belongs to a group.
- Not done: `J n` (jump), absolute moves over time, easing, console host (`SPRITE` is not available there anyway).


### Scrolling a world that is bigger than the screen (`TILE SCROLL`)

The tile screen is a window of 41 x 13 cells (40.00 x 12.5 visible). `TILE SCROLL sx, sy` (both >= 0, taken modulo 16 / 24) moves the picture right / down by that
many pixels; **what is pushed in at the left or at the top comes from the last cell of the row or column** (the cells wrap). A big world therefore lives in the program
(an array, strings, or the RAM with `POKE`/`PEEK$`: `demos/climb.bas` keeps its 120 x 40 tiles in the RAM, one byte per tile) and the program copies the visible part into
the cells with `TILE FILL` when the camera crosses a tile boundary, and scrolls by the remainder. To get exact pixel positions (the world tile in column W at x = W * 16 - CX):

    cell I holds the world column  (CX-1)\16 + 1 + I        for I = 0 .. 39,  the cell 40 (off the screen) holds (CX-1)\16
    TILE SCROLL 15 - (CX-1) MOD 16, 23 - (CY-1) MOD 24   (the rows the same way, cell J holds the row (CY-1)\24 + 1 + J)

(CX, CY >= 1.) The strip pushed in at the top shows the wrong row for up to 23 pixels, so keep the top two text lines opaque (a score bar); the left strip is right because
of the cell 40. Sprites are placed at world position minus camera. Any frame in which a layer changes costs the screen thread a rendering pass: on a software renderer (a test
under Xvfb) a program that scrolls the tiles every frame ran at about a third of 60 frames per second; on a normal desktop it is fine.

### Reserved sprites and the mouse pointer (2026-11-01)

The sprites **248..255 belong to the engine**; a program can't reach them (`SPRITE 248,ON`, `SPRITE("X",255)` and the other statements and functions with such a number are
"Illegal argument"). The program's sprites are 0..247. The reserved ones: have **images and animation sequences of their own** (the second half of the sprite map: 256 more
images, numbered 256..511 in the map, and 16 sequences, `MAX_SYSANIM`; the program's images 0..255 and sequences stay as they were); are **drawn in front of every sprite of the
program** whatever its priority; **don't take part in collisions**; are **not touched by `GFX RESET` / `GFX RESET ALL`**; and they are on the sprite layer, so `GFX LAYER 4,OFF` hides them
with the others. The engine reaches them through `sprscr_sys_*` (`sprscreen.h`). **Sprite 255 is the mouse pointer** (248..254 are free for future use).

The pointer's pictures are drawn by `cursors.[ch]` (no SDL) at start-up, in the look of **colored glass that floats above the screen**: every shape is a silhouette that is turned into a
translucent body in one of six hues (blue, gold, green, red, purple, teal: the translucent entries 141..255 of the palette, so the picture behind shows through), with a bright edge at the
top left, a dark edge at the bottom right, a lighter and a darker band inside (a bevel) and a soft shadow that falls to the lower right. They are the colors of the sprite layer's palette:
a program that changes that palette changes the pointer. The shapes are 10 to 14 pixels wide: `ARROW` (blue, hot spot at the tip), `WAIT` (a teal glass badge with a comet of eight colored
dots that turns, 8 frames), `BUSY` (the arrow with a small glass badge and a comet, 8 frames), `TEXT` (I-beam, teal), `HAND` (pointing, gold), `GRAB` (a closed hand for dragging, gold), `CROSS`
(crosshair, green), `SIZEH`, `SIZEV` (resize arrows, purple), `MOVE` (four arrows, purple), `NO` (a red ring and slash around frosted glass) and `HELP` (the arrow with a gold bubble that holds a
question mark). `cursors_set()`, `cursors_move()` (the hot spot to x, y), `cursors_show()`; the window has `sdlscr_cursor_*`. **BASIC reaches the pointer with `MOUSE`** (see the next section and `docs/basic-language.md`, "The mouse"). `sdltest3` shows all shapes on a light, a dark and a striped background (the stripes show the translucency) and lets you move
and change the real pointer with the keyboard (N / P, W A S D).

### The mouse (2026-11-01)

`mouse.[ch]` (no SDL) is the logic: the position and buttons from the events of the window, the requests of the program, and **when the pointer is seen** (3 s after every event or
`MOUSE SHOW` / `POS` / `SHAPE`, `MOUSE TIMEOUT` changes the time, `MOUSE HIDE` hides at once). The statements and functions are rows of the table in `gfxops.c` (group `MOUSE`:
`GFXOP_MOUSESHOW .. GFXOP_MOUSETMO` 80..84, `GFXQ_MOUSEX .. GFXQ_MOUSEVISIBLE` 100..107), `host_gfx_op` / `host_gfx_query` in `engine1999.c` call `sdlscr_mouse_*`. **Threads, with no
locks (the owner's rule):** everything that changes the pointer (`cursors_*`, so the sprite) is done by `mouse_tick()` in the screen thread, once per frame; the screen thread also reads
the SDL events (`SDL_MOUSEMOTION`, `SDL_MOUSEBUTTONDOWN/UP`, `SDL_MOUSEWHEEL`, in the renderer's logical coordinates, which are the sprite layer's 640 x 300) and posts them to the same
module. The program's thread only posts requests (words that are exchanged to 0 when taken, the last request wins; `POS` and `SHAPE` are separate words, so neither is lost) and reads
the values the screen thread published (button presses and wheel steps are latched with atomic or / add and cleared by the reader's exchange). A read looks at the pending requests first, so it
agrees with what the program has just asked for. The window's own pointer is switched off (`SDL_ShowCursor`), `MOUSE POS` warps it with `SDL_RenderLogicalToWindow` / `SDL_WarpMouseInWindow`.
**The console host uses the same module** (`docs/console-host.md`, "The mouse"; `mouse_setarea()`, the terminal feeds `mouse_event_*` from its reports and calls `mouse_tick()`). Tests: `gfxtest1` (the state machine with a made-up clock, the area: 36 checks), `vmtest1` (the statements and functions with the logging host), the real window under Xvfb (events, buttons, wheel,
the 3 s, `POS`, `SHAPE`, `TIMEOUT 0`).

### `GFX RESET`

`GFX RESET` puts the graphics system back to its start state, so that nothing a program left behind stays on the screen:

- all five layers are shown again, in the original order (0 background .. 4 sprites);
- **all palettes** and the backdrop color are back to the color table (a changed palette would otherwise leave the next program with strange colors);
- the background and graphics bitmaps are cleared; target 3, pen 1, mode `SET`, and the `DRAW` position, color, scale and angle start again;
- the tile screen: all cells 0 and no scrolling;
- all 248 sprites of the program (not the 8 reserved ones, so the mouse pointer stays): hidden, at 0,0, default image and animation, no movement, no collision flags, pending collision events dropped;
- the **text colors**: pen, paper and the blank colors of the text screen go back to their defaults (pen 1 = black), and so do the colors an ANSI sequence in `PRINT` set;
  a `COLOR 140` of the program does not outlive `GFX RESET` (an earlier version left them, so text stayed white).

Kept: the **definitions**, because they are assets, not clutter: tile images, sprite images and animation sequences, redefined characters. `GFX RESET ALL` clears these too
(images to 0, sequences to image 0, the font back to the built-in one). **Not touched**: the contents of the text screen (use `CLS`), the `ON COLLISION` handler (it ends with
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

## Screenshots (roadmap item 82, 2026-11-11)

`shot.[ch]` (no SDL; test `shottest1`, in `make check`). The picture is the **composed layers** (the five layer memories drawn over the backdrop in priority order with the renderer's alpha blending: `shot_compose()`), 640 x 300, 24 bit BMP; not the window (no scaling, no letterbox bars; the mouse pointer sprite is in it while shown). The screen thread owns the layer memories, so only it composes. Two ways out, both without locks (atomic words, the standing rule):

- **The key Print** (`SDLK_PRINTSCREEN`, window host; the key is used up by the Engine like F11): the screen thread composes into a buffer of its own and starts a short-lived thread that writes `shot-YYYYMMDD-HHMMSS-n.bmp` in the current directory (`n` counts per run), so the screen thread never waits for the disk; the thread prints `screenshot saved: name` on the standard error output.
- **`SCREENSHOT [file$]`** (extension word `XF_SCREENSHOT` 33, `GFXOP_SCREENSHOT` 166, signature `S` = an optional string): the program thread asks by an atomic request slot (`shot_grab()`), the screen thread composes into the slot at the end of its frame and answers (`shot_serve()`; one frame, 1 s time-out), the program thread encodes and writes through the I/O layer (`iodev_write_all`), so the security policy question (a name outside the program's folder) is asked in the program's thread as for `GFX SAVE`. Console host: "Not available in the console host" (item 83 plans a console render thread).

This is step 1 of item 83 (the AV capture subsystem): its queue and writer threads will replace the thread-per-shot and the slot. Checks: `shottest1` (compose, priorities, disabled layers, BMP bytes, names, 200 requests across two threads, ASan / TSan clean), `vmtest1` (4 cases), `hosttest.sh` (the real window host: the statement and the Print key under Xvfb, BMP size and header, a picture that is not flat).

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
- *(Not planned: synchronization of the layer memory. There are no locks by design; see "Threading" in `docs/display-engine.md` for the rules that keep the unsynchronized access safe.)*
