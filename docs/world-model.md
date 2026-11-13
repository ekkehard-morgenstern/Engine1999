# The renderer-independent world model (the loader is done; Deqq's House II uses the document)

*2026-11-13, owner's idea: describe a world (Deqq's House II first) as one big JSON file of 3D objects, independent of the sector renderer; a single `W3D` statement loads such a document into the renderer, a later 3D renderer would build its own structures from the same file. Step 1 (the JSON feature, `docs/json-subsystem.md`) and step 2 (this page: the document `demos/deqq2.world.json` and `W3D WORLD`, `w3dworld.[ch]`) are done.*

**What exists:** `JSON LOAD "deqq2.world.json"` + `W3D WORLD` builds the sectors, walls and things of the renderer from the document, and `demos/deqq2.bas` does exactly that in `LOADLEVEL` (`JSON LOAD` finds the file in `demos/` of the source tree or in the installed `lib/engine1999/worlds/`). **Since 2026-11-13 the document is the source of Deqq's House II**: it was first written by a generator (`demos/levels/deqq2.bas` with `mkw3d.lib`; the generator is gone, git history has it) as an exact copy of the old `DATA` world, then sanitized by hand (below); `mkw3d.lib` keeps `MK_EMITJSON` for the other level generators. `worldtest.sh` (in `make check`) checks the document: it builds (221 sectors, 1494 walls, 113 things), every room, solid and thing has a `name`, and `worldlint.awk` finds no wall of zero depth.

## Rules of the document (the sanitizing, 2026-11-13)

- **Walls have depth.** A wall is a gap between rooms (nothing there: both rooms show their own face on it) or a room of its own (a doorway, a door, an alcove, a window niche); two rooms never share a plane with a solid wall on both sides, because that is a wall of zero depth that shows a different face on each side. `worldlint.awk` (input: a map file written by `WM_SAVE` after `W3D WORLD`) reports every pair of solid walls in different sectors that lie on one plane, face away from each other and overlap in height. Interior partitions of the house are 10 thick (rooms end at 395 / 405 and 555 / 565; the openings through them are the doorway rooms 116..119); the alcoves (hiding places 40..46) leave 10 units of wall behind them.
- **Stair sides are invisible rails**, not walls: an opening with `"flags": ["block"]` and no picture (`tex` omitted) blocks walking but you see through it, and its `lower` picture is the side of the stair mass (planks) below the opening: the hall's stairs (65 / 66 to the steps 13..20) and the attic stairs (bedroom 22 to the steps 24 and 25) are open to the rooms beside them.
- **Names and notes:** every room, solid and thing has a `name`; `note` is a free comment on any object (the loader reads neither: unknown members are ignored, so documents can carry whatever people need). The world has a `note` of its own at the end.
- Where a thin thing is right (a railing, a fence, a window pane) it is a masked or translucent picture on an opening, never a bare wall.

**Still the generator's heritage, not checked:** the exterior walls of the house have different depths (front 30, west 20, east 40, back 50: the gaps between the garden rooms and the house rooms); the town's houses are boxes (solids with a rim), the fences are the loader's 20-deep fence sectors.

**What the loader does** (`w3dworld.c`, a port of the generator's work, so the output is identical): rooms are axis-parallel boxes; the walls of a room are cut along each side by its openings (gaps are solid walls in the room's own picture), solids are holes in a room with a ring of walls round them, `outdoor` rooms get **fence sectors** (thin, open to the sky, you see over them and cannot enter; numbered from `environment.fence.first`, default 100, in the order of the outdoor rooms) behind every solid wall and the **house** pictures where `house` spans say so, and the far wall and end caps of each fence sector open into the `ground` room (a ghost lawn: no ray ends in the void). Sector numbers are the rooms' `id` values (a game addresses sectors by number, tags and doors); `name` is for people. The loader checks what the generator checked (rooms must touch along an opening, spans lie inside both rooms, openings must not overlap) and names the element in its messages (`rooms[3]: "box" is missing`).

**Not done yet (differences from the draft below):** polygon rooms (`outline`: "not supported yet"), slopes, pillars, `materials` / `palette` / `textures` (textures are plain numbers: the program still draws them), **`movers`, `lights` and `game` data are not read by the loader** (a program reads them with the `JSON` statements; Deqq's doors and lamps are still the game's own code), `ghost` rooms are given as `"ghost": true` (the generator writes it for tag 97), things are placed in the room found at their feet unless `room` says otherwise.

## The schema as implemented

```
{ "format": "e99-world", "version": 1, "name": "Deqq's House II",
  "rooms":    [ { "id": 0, "name": "front garden", "box": [x1,y1,x2,y2], "floor": {"z":0,"tex":24}, "ceiling": {"z":300,"tex":-1 | "sky"},
                  "light": 255, "tag": 10, "walls": 1, "ghost": false, "outdoor": true, "house": [ {"side":"N","range":[180,840]} ] } ],
  "solids":   [ { ...the same fields as a room..., "in": 12, "rim": {"tex": 26, "flags": ["masked","block"]} } ],     // a hole in the room "in": floor.z = its top
  "openings": [ { "from": 10, "side": "N", "range": [lo,hi], "to": 11, "tex": 12, "upper": -1, "lower": -1, "flags": ["block","solid"], "offset": [xo,yo], "oneway": false } ],
  "things":   [ { "id": 400, "tex": 280, "x": .., "y": .., "w": 28, "h": 52, "z": 0, "room": 3 } ],
  "environment": { "fog": 4200, "far": 0, "sky": 11 | {"tex":11,"repeat":1,"shift":0},
                   "fence": { "tex": 250, "flags": ["masked","block"], "height": 300, "wall": 214, "house": 27, "ground": 426, "first": 100 } },
  "start": { "x": 1730, "y": 1570, "angle": 90 } }
```

`side` is "S" (the room's y1 edge), "E" (x2), "N" (y2) or "W" (x1); `range` runs along that edge; an opening is made in both rooms (the other side is the opposite edge) unless `"oneway": true`. `flags` is a number or names (`masked` 1, `block` 2, `solid` 4, `sky` 8, `translucent` 16). Rooms and solids are listed in the order they were made (the fence sector numbers follow it); a solid's host must come before it (all rooms first, then solids in order).

## The first draft (2026-11-13), kept for the parts not done

```
{ "format": "e99-world", "version": 1, "name": "Deqq's House II",
  "palette":  { ... },                      // optional: named colors / the shading ramps used by the textures
  "textures": [ { "id": 24, "name": "sand", "kind": "proc|bmp|sprite", "src": "..." } ],
  "materials": { "brick": { "tex": 251 }, "grass": { "tex": 10 }, ... },   // names used below
  "rooms": [ ... ],  "solids": [ ... ],  "openings": [ ... ],  "things": [ ... ],  "lights": [ ... ],
  "start": { "x": 480, "y": -35, "angle": 270, "room": "pavement" },
  "environment": { "fog": 4200, "sky": 11, "backdrop": { "tex": 29, "distance": 2400 } } }
```

**room** (a convex or simple-polygon volume with a floor and a ceiling; the renderer's sector): `name`, `outline` (`[[x,y],...]` counterclockwise, or `box: [x1,y1,x2,y2]`), `floor` {`z`, `tex`, optional `slope`}, `ceiling` {`z`, `tex` or `"sky"`, optional `slope`}, `light` (0..255), `walls` (default material, or a list per outline edge), `tags`, `outdoor` (true: the loader adds the fence / ground so no ray ends in the void), `fence` {`tex`, `height`}, `ghost` (a view-only volume).
**solid** (a building, furniture block, pillar: a volume standing in a room): `room`, `box` or `outline`, `base` (z), `height`, `material`, `roof` {`tex`}. The loader makes it a hole in the room (floor = top) or, if it overlaps real rooms, a ghost block.
**opening** (doors, windows, gates, stairs steps, hatches between rooms): `between` [`roomA`, `roomB`], `edge` / `at` [x1,y1,x2,y2] or `side` + `range`, `kind` (`passage`, `door`, `window`, `railing`, `glass`), `tex`, `upper` / `lower` textures, `flags` (named: `block`, `solid`, `masked`, `translucent`, `sky`), `tag`, optional `z` range for windows. The loader turns each into a portal pair.
**thing** (sprite, decal, door sprite, pickup, car, light post): `id`, `tex`, `x`, `y`, `z`, `w`, `h`, `room`, `light`, `tags`.
**light** (point lamp or sector effect): `kind` (`lamp`, `flicker`, `pulse`, `strobe`), `room` or position, `radius`, `amount`.

**Decisions (owner, 2026-11-13: "go with your recommendations on all three questions"):**
1. The model is at the level of **rooms, solids and openings** (not raw polygons with named surfaces): the loader generates fences, the ghost lawn, outdoor ground and the portal pairs itself.
2. **Animated parts** (doors, the gate, lifts, moving floors and ceilings) are separate **`movers`** nodes: `target` (a room, an opening or a thing), `floor` / `ceiling` / `open` targets, `speed`, `tag`; the game opens them by name or tag.
3. **Game data lives in the same file** under `game` (items and their rooms, puzzles, Deqq's way points and hiding places, texts), so one file is the whole world; the `W3D` loader ignores `game`, the program reads it with the `JSON` statements.

Next: put the movers, lamps and game data (items, puzzles, way points, hiding places) into the same file and read them with the `JSON` statements; polygon rooms and slopes in the loader.
