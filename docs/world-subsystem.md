# The static world (`world.[ch]`, `randseed.[ch]`)

Status: **steps 1 .. 3 of the static-world project** (2026-11-21): the file format, the seed field, the creation step and the loader (the ground drawn by a quadtree of tiles). The stored
features (trees, ruins, water, step 4), saving the game (step 5) and the game's *New game / Continue* start (step 6) follow; the plan is at the end.

## Why

The first 1987 world was dynamic: ground, roads, trees and rings were decided and rebuilt while the player moved, and things popped up and changed in front of the player.
The owner's direction (2026-11-21, like Minecraft): **create the world once when a new game starts, keep it on disk in chunks, load it from there, save it when the player
leaves, and never decide anything during play.** The old code (`M3D RING`, `roadnet`, `chunks.lib`) stays in the repository and keeps working.

## Numbers

| What | Value |
|---|---|
| world | `n` x `n` chunks, `n` = 1 .. 1000 (`WORLD_MAXN`); the full world is 500 km x 500 km |
| chunk | `WORLD_CHUNK` = 15625 units = 500 m (a unit is 3.2 cm) |
| window in memory (step 3) | 21 x 21 chunks (10.5 km) |
| record | 32 bytes a chunk: 1000 x 1000 chunks = 32 MB |
| origin | (0, 0) is where the four middle chunks meet; chunk (cx, cy) starts at `x0 = (cx - n/2) * 15625`, `y0 = (cy - n/2) * 15625` |

A chunk is a *column*: the ground is a height field over its 500 x 500 m, the stored height range (`hmin` .. `hmax`) is its vertical extent. Caves and tunnels (roadmap) would
need real cubes; nothing in the format forbids another record later.

## The file `<dir>/world.dat`

All numbers big endian. A header of 64 bytes, then the records row by row (`cy` 0 .. n-1, in a row `cx` 0 .. n-1), so a chunk is read with one `pread` and the file is
written by several threads at once.

| Offset | Header | |
|---|---|---|
| 0 | `"E99WORLD"` | magic |
| 8 | u32 | version (`WORLD_VERSION` = 4) |
| 12 | u32 | `n` |
| 16 | u64 | the world seed |
| 24 | u32 | chunk size in units (15625) |
| 28 | u32 | record size (32) |
| 32 | u32 | state: 0 not finished, 1 finished (written last, after `fsync`) |

| Offset | Record | |
|---|---|---|
| 0 | u64 | the chunk's **seed** |
| 8 | i32 x 3 | lowest, highest and mean height in units (5 x 5 samples of the ground *without* the road cuts) |
| 20 | u8 | kind of ground at the middle: 0 plain, 1 meadow, 2 field, 3 forest |
| 21 | u8 | flags: 1 a tarred road passes, 2 dirt tracks, 4 water, 8 mountain (above 48 m), 16 in the wall |
| 22 | u8 | tarred road links through the chunk |
| 23 | u8 | unused (0) |
| 24 | 8 bytes | reserved (0) |

`world_open()` refuses a file that is not finished, truncated, of another version or not a world. The geometry is **not stored**: it is made again from the record and the
world's ground function, which depends on the world seed only (the pure layers of `m3d.c`: ground noise, hills, mountains, lakes, the road networks of `roadnet.c`).

## The seeds (`randseed.[ch]`)

The random field of the owner's RANDPATH program (`../randpath/randseed.[ch]`): a 1024 bit state that walks one step south (`rs_south`) or east (`rs_east`); the state at a
place depends on the way there. `rs_seed()` (new) makes the state at the origin from the world seed (the original read `/dev/urandom`), `rs_fold()` folds 1024 bits into 64. The
creation walks the field row by row (start of a row = the row before, one step south; along a row, one step east) and stores the folded state as the chunk's seed;
`world_field_seed(seed, cx, cy)` recomputes one (a walk of `cx + cy` steps) for tests and tools. A chunk's decorations (trees, ruins, bales, ...) will be placed from this seed
with `nz_hash64`, so they come out the same every time the chunk is built.

## Creation (`world_create`)

`world_create(dir, n, seed, threads, progress, ud)` makes the folder, writes the header (state 0), computes the seed field, then `threads` workers (0: the processors, at most
16) take rows from an atomic counter, make the records of their row and `pwrite` them; the calling thread polls and calls `progress(ud, rows_done, n)`. At the end the header's
state becomes 1, so a crashed creation is never opened. Creating twice with the same seed gives identical files, whatever the number of threads (`worldtest1`).

What made it fast enough (a full 1000 x 1000 world in well under a few minutes on four cores):
* the summary samples the ground **without the road network** (20 times cheaper: 0.9 instead of 20 microseconds a sample) and only 5 x 5 points;
* `rnet_kind_cold()`: the kind of ground without filling the road cache (the dirt tracks cover every chunk, so they are not counted);
* `link_exists()` in `roadnet.c` has a per-thread memo: every node asks for all its links, and each answer samples the mountain mask along a strip of kilometres.

## The ground (`world_height`)

`world_height(w, x, y)` is the ground function of the 1987 game (the pure layers of `m3d.c`, `m3d_ground_eval`), **tuned for 500 m chunks** (version 4 of the file: version 3 flattened the yard of the house and its wheat field at the origin to ground 0 for `demos/1987.bas`; version 1 had
the numbers of the old 29 m chunks, where 73 % of the chunks held water and a tarred road crossed every kilometre): rolling ground (waves of 150 m, 5 m high, base 4.8 m so it never
dips below the water level -12 by itself), hills (80 m waves, up to 22 m), mountain ranges (scale 1.6 km, peaks up to 1.9 km, mask 6.4 km, none within 14 .. 22 km of the
origin), lakes (about 1 km, 19 m deep; water only where a lake is), dirt tracks and kinds of ground (meadow / field / forest patches) on a 290 m grid, **tarred roads every 3.7 km** (since version 4; version 3 had 6.4 km)
(`tspacing` 114500), a quiet box at the origin, and **the wall**: from four chunks inside the edge the ground rises smoothly by 90000 units (2.9 km) over three chunks, so the world is
closed by an impassable ring of mountains. It is pure and thread safe. Measured over the whole 1000 x 1000 world (sample of 3481 chunks): water in 12 % of the chunks, mountain 23 %,
a tarred road through about 15 % (version 3: 5.5 %); kinds: plain 16 %, meadow 28 %, field 20 %, forest 36 %. The record's `ntar` / `WCF_TAR` count the tarred links whose way passes through the chunk.

## Tests

`worldtest1` (in `make check`): the random field, errors (size, missing folder), creation with 1 and 4 threads gives identical files and the right size, records agree with
`world_field_seed`, the summary is consistent, the wall is marked, `world_chunk_at` and the origin, reopening, another seed differs, unfinished / truncated / foreign files are
refused, a one chunk world, and a 100 x 100 timing line.

## The loader: a quadtree of ground tiles (`m3d.c`, `M3D WORLDOPEN`)

The first idea (one mesh per 500 m chunk, a window of 21 x 21) cannot give fine ground near the player and coarse ground far away: a chunk is too big a unit. So the ground is drawn
by **tiles of 8 x 8 cells** of every size `s = 976.56 * 2^level` units (`WL_S0` = a sixteenth of a chunk; level 0: cells of 3.9 m; level 4 is a chunk; the top, level 6, is
62500 units with cells of 244 m), a quadtree on the chunk grid. A tile is **split** into its four children when the camera comes nearer than `R = 3 s` (Chebyshev distance to the
tile's box) and **merged** again beyond `1.1 R`. A window of 10.5 chunks round the camera is drawn (roots of level 6; tiles outside the world are left out).

**Why nothing pops.** The grids are nested (a coarse cell is two fine ones, all on the world's grid) and every tile *morphs* (the `M3D RING` machinery, ratio 2): its vertices blend
from their own height into the surface of the parent's grid between the distances `a = 4.42 s` and `b = 6 s` from the camera. A tile splits at `dmin < R = 3 s`: all its vertices
are nearer than `a`, so it still shows its own grid, and the children's vertices are all farther than `R = 6 * s_child = b`, so they show the parent's grid: the surface is the same
before and after the split. Merging is the same the other way round. The children are made beforehand (nearer than 1.6 R) and a split waits until all four are there, so a slow
worker never leaves a gap or shows a half made tile; a child that is gone sends the parent back to showing itself. Cracks between neighbours of different sizes are closed by skirts.

**Work.** Every tile is one object (slot `i` = object `obj0 + i`, 3072 slots) made by a ground job of **mode 4** on the one worker thread of `M3D RING`: 81 vertices from
`world_height()` (pure, so the same every time), the picture of a cell by the kinds of ground / rock / mud rules, the slope shading of `M3D SUN`, **ribbons** of tarred roads (all
levels, wider far away) and of dirt tracks (cells up to 250 units), hubs. A tile costs about 2 ms; a whole window is a second of worker time, and while the player walks tiles are
made ahead (nearest first, at most 24 jobs a frame), never on the program thread. Tiles that are not visited any more are freed (a tile whose job is still out stays as a hidden
husk until the job ends).

**BASIC.** `M3D WORLDMAKE dir$,n,seed` (background, progress `M3D("WORLDINFO",5)`), `M3D WORLDOPEN dir$,obj0[,flags[,light]]`, `M3D WORLDVIEW x,y` every frame, `M3D WORLDWAIT`,
`M3D WORLDCLOSE`, `M3D("WORLDINFO",k)`, `M3D("WORLDCHUNK",x,y,f)`; after opening, `M3D("GROUND",x,y)`, `M3D("KIND")`, `M3D("ROADD" / "ROADT")` and the road queries answer for the
world, and `M3D RINGPIC` / `M3D SUN` choose the pictures (the pictures a game sets for rings serve the tiles; `rockh` should be about 900 in this world).

**Measured** (`m3dtest1`, world of 24 x 24 chunks, camera at the origin): about 600 .. 900 tiles shown (2 .. 3 times that built, hidden, for the splits), 10 ms a frame in the
software renderer at 640 x 264 (16000 triangles drawn), every place covered by exactly one shown tile, the floor within 1 unit of the ground function near the camera, walking
24 km in steps of 150 units changes the surface at 24 places by at most 11 units between two steps (the steep walls of mountains; nothing steps). Clean under ASan, UBSan and
ThreadSanitizer.

## Trees (step 4, first piece, 2026-11-22)

Owner's rule: **geometry is added only at the far end of the view, never in the middle.** So the trees are not part of the ground tiles (those split at 90 m and would add trees there) but
live in **cells of trees** of their own: squares of 3906 units (125 m), one object each, made by a job of **mode 5** (`job_wtrees`, no ground) when tree pictures are set (`M3D TREEPIC`).
A cell is cut into squares of 162.8 units (5 m) on the world's grid; each holds at most one tree at a place hashed from the **world seed** and the square's number (`nz_hash64`), so the
same tree stands in the same place whatever cells are built and in whatever order; nothing is stored (the chunk's own seed is not needed yet). Chance 0.15 in forest (0.6 at first: the frame rate tanked, the owner had three quarters taken away), 0.006 elsewhere (a
lone tree); none on or beside roads, on steep ground, below the water + 2 or above `rockh - 60`, in the house's yard or on its wheat field. The trees are the **cheap** ones (12
triangles, three kinds).

**Growing, not popping:** every vertex of a tree morphs (the machinery of the ground tiles, `M3D MORPH`) between the distances `WL_TB - WL_TBAND` = 6300 and `WL_TB` = 7000 units (224 m)
from the camera to the *foot* of its tree (`tcol` / `tbase` in the job), so a tree beyond 224 m is a flat disc on the ground and grows out of it while the camera comes nearer; a cell is
made when the camera is within `WL_TB` + 3.5 cells, shown from `WL_TB` on, dropped beyond `WL_TB` + 4.5 cells. What appears, appears at the far end and flat. `m3dtest1` checks that more
triangles are shown with tree pictures than without and exactly the same number on every opening (about 16000 more triangles a view in these samples; a dense forest holds up to about
35000 within 224 m). **Open:** the range (224 m) is set by the triangle budget (the hardware renderer could take more); full-detail trees near the player; collision with trunks (John walks
through them, as in the dynamic mode); beyond the range only the forest ground picture shows; ruins, bales, posts, real water.

## Plan

1. File format, seed field, creation step (**done**).
2. The loader (**done**): a quadtree of morphing tiles on the one worker thread; the program thread never generates anything.
3. Stored features: roads, lakes, ruins and forest as records (and, where they matter, per chunk feature lists in a second file).
4. Saving: the player's state and changed chunks on exit; *Continue*.
5. `1987.bas`: *New game / Continue*, a statement / functions to reach all this from BASIC (`WORLD CREATE`, `WORLD OPEN`, ...).
