# The renderer-independent world model (DRAFT, not implemented)

*2026-11-13, owner's idea: describe a world (Deqq's House II first) as one big JSON file of 3D objects, independent of the sector renderer; a single `W3D` statement loads such a document into the renderer, a later 3D renderer would build its own structures from the same file. Step 1 (the JSON feature, `docs/json-subsystem.md`) is done; this page is the proposed schema for step 2.*

Units as in `w3d.c`: x / y in the plan (y grows towards the viewer's "south" as in the demos), z up, angles in degrees (0 = east, 90 = +y). Names (strings) are used for references; the loader resolves them.

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

Open questions for the owner: the level of the model (volumes / openings as above, versus polygons with named surfaces); how animated parts (doors that open, the gate, lifts, moving sectors) are described (a `mover` node with `floor` / `ceiling` targets and speeds); whether the game's own data (item tables, Deqq's way points, hiding places) lives in the same file under `game` (my suggestion: yes, so the file is the whole world).
