# The static world (`world.[ch]`, `randseed.[ch]`)

Status: **steps 1 and 2 of the static-world project** (2026-11-21): the file format, the seed field and the creation step. The loader that builds the meshes round the player
(step 3), the stored features (step 4), saving the game (step 5) and the game's *New game / Continue* start (step 6) follow; the plan is at the end.

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
| 8 | u32 | version (`WORLD_VERSION` = 1) |
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
| 21 | u8 | flags: 1 tarred road near, 2 dirt tracks, 4 water, 8 mountain (above 48 m), 16 in the wall |
| 22 | u8 | tarred road links near the chunk |
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

`world_height(w, x, y)` is the ground function of the 1987 game (`demos/1987.bas` lines 3272 .. 3278: ground noise scale 900 / amplitude 22, hills, mountains from 14000 .. 22000
units from the origin on, lakes, dirt tracks every 1800 units, tarred roads every 36000, a quiet box at the origin) without the hand-made area, plus **the wall**: from four
chunks inside the edge the ground rises smoothly by 90000 units (2.9 km) over three chunks, so the world is closed by an impassable ring of mountains. It is pure and thread safe.

## Tests

`worldtest1` (in `make check`): the random field, errors (size, missing folder), creation with 1 and 4 threads gives identical files and the right size, records agree with
`world_field_seed`, the summary is consistent, the wall is marked, `world_chunk_at` and the origin, reopening, another seed differs, unfinished / truncated / foreign files are
refused, a one chunk world, and a 100 x 100 timing line.

## Plan

1. File format, seed field, creation step (**done**).
2. The loader: a worker thread builds the 21 x 21 window from the records (full detail near the player, a coarser mesh on the same surface farther away, no morphing: coarse
   vertices lie on the fine surface), the game thread never generates anything.
3. Stored features: roads, lakes, ruins and forest as records (and, where they matter, per chunk feature lists in a second file).
4. Saving: the player's state and changed chunks on exit; *Continue*.
5. `1987.bas`: *New game / Continue*, a statement / functions to reach all this from BASIC (`WORLD CREATE`, `WORLD OPEN`, ...).
