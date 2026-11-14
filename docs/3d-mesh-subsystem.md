# The 3D mesh renderer `M3D`

*2026-11-14, owner's decision: the sector renderer (`W3D`, `docs/3d-subsystem.md`) cannot express Deqq's House II (a town round a house, tall things seen over fences, rooms over rooms), so a renderer without sectors was written next to it. The sector renderer stays for the other demos (Manor, Bug Hunt, the showroom, Deqq's House I). This page is the reference for the new one; the code is `m3d.[ch]` (no SDL, no global state), the test is `m3dtest1.c`.*

> The sector renderer `W3D` (`docs/3d-subsystem.md`) stays: it is a supported feature with its own demos (Manor, Bug Hunt, Showroom, Deqq's House I, Y2K38's server room, the map editor), and nobody plans to remove it (owner, 2026-11-14). Which renderer fits depends on the world: sectors are fast and simple for rooms joined by portals; meshes can put anything anywhere.

## Model

- A world is a set of **objects** (0 .. 4095). An object is a triangle mesh with a position, a heading (yaw about the z axis), a light level (0 .. 255) and flags. The mesh is in the object's own coordinates; `M3D OBJPOS o,x,y,z[,yaw]` moves and turns it (a door is an object that turns about its hinge).
- **Things** (0 .. 1023) are billboards: a picture of a given width and height that always faces the viewer, with its bottom at `z`.
- Coordinates: x east, y north, z up; angles in degrees, 0 = +x, 90 = +y; to the right of the view direction is clockwise. A triangle is **counterclockwise seen from its front** (the normal `(b-a) x (c-a)` points to the viewer); one-sided objects do not draw or collide with the back. `M3D OBJECT o,16` (two sided) draws both sides.
- Pictures are `GFX GRAB` blocks (`M3D TEXTURE n,a$`, as for `W3D`); palette index 0 is see-through where the object is masked (flag 1) or translucent (flag 2) and always in things. A picture number with no picture draws that palette color flat.
- `M3D BOX o,x1,y1,z1,x2,y2,z2,tex` adds the six faces of a box (picture coordinates 1 texel = 1 unit, anchored to the object, so neighbouring boxes match). `M3D VERTEX o,x,y,z[,u,v]` numbers vertices from 0 per object; `M3D FACE o,a,b,c,tex` and `M3D QUAD o,a,b,c,d,tex` add triangles; without `u,v` the picture coordinates follow the position by the direction of the face.
- Object flags: 1 masked, 2 translucent (mixed half and half, drawn last, far to near), 4 **passable** (no collision and no floor or ceiling), 8 **see-through** (shots, `USE` of walls and sight pass: glass, a railing), 16 two sided.

## Drawing

Every triangle is culled (back face, behind the viewer, off the screen), clipped against the near plane (2 units) and rasterized with a z buffer and perspective-correct picture coordinates; the light level of the object, the lamps (`M3D LAMP n,x,y,z,radius,amount`, up to 8, per pixel, brightening only) and the fog (`M3D FOG d`: dark at distance d) pick a row of the color map (`M3D COLORMAP` builds it from the palette). Order: the sky (`M3D SKY`, a picture around the viewer by angle), opaque and masked objects, things (far to near, depth tested), translucent objects (far to near). `M3D FAR d` skips what lies beyond d. Cost: about 1.6 ms for a test scene at 320 x 150 (`M3D VIEW x,y,w,h,2`).

## Collision, floors, rays

The player is a vertical capsule (`M3D PLAYER radius,height,eye,step`, default 14, 56, 46, 24), `M3D CAMERA x,y,z,yaw[,pitch]` puts the **feet**. `M3D MOVE forward,right` moves in small pieces, pushes the capsule out of the triangles it touches (spheres at the middle heights; everything lower than `step` above the feet is walked over), slides along an axis when blocked, refuses a floor more than `step` higher, refuses a ceiling lower than the player's height, and then lets the feet follow the floor (`M3D("FLOORZ")`, `M3D("FLOORAT",x,y,z)` = the highest surface at or below z, `("CEILAT",x,y,z)` the lowest above). `M3D LIFT h` raises the feet above the floor (a jump: steps up to `step` above the raised feet are taken). `M3D("HIT")`: 1 = something pushed, 2 = no movement at all.
Rays use the same triangles: `M3D("SHOOT",range)` (things and objects, the last shot is in `SHOTKIND` 0 / 1 object / 2 thing, `SHOTOBJ`, `SHOTDIST`, `SHOTX/Y/Z`), `M3D("USE",reach)` (the object in front, whatever its flags), `M3D("SEES",n)` (a thing's line of sight to the eye).

## Zones, walking things, the world document

- **Zones:** `M3D ZONE id,x1,y1,x2,y2,z1,z2[,tag]` names a box of space for the game (a room); `M3D("ZONE",x,y,z)` is the id of the first zone that contains the point (z within z1 - 2 .. z2), -1 if none, `M3D("ZONETAG",x,y,z)` its tag. The renderer does nothing with them. Put the zones of an upper floor and of a chair or table you can stand on *before* the zones below them. `M3D CLEAR` deletes them.
- **`M3D THINGMOVE n,dx,dy`** walks a thing like `M3D MOVE` walks the player (a body of radius 0.4 x width, at most 14, the player's step height; it follows the floor); `M3D("HIT")` tells whether a wall pushed it.
- **`M3D WORLD`** builds the world from the current JSON document (`JSON LOAD` first), replacing objects, things, lamps and zones; pictures and settings stay:

```
{ "format": "e99-mesh-world", "version": 1,
  "environment": { "fog": 4200, "sky": 11 },
  "start": { "x": 1730, "y": 1570, "z": 0, "angle": 90 },
  "zones":   [ { "id": 12, "name": "living room", "box": [x1,y1,x2,y2], "z": [z1,z2], "tag": 0 } ],
  "things":  [ { "id": 400, "tex": 282, "x": .., "y": .., "z": 0, "w": 32, "h": 76 } ],
  "objects": [ { "id": 12, "name": "..", "flags": ["masked","translucent","pass","seethru","twosided"] | number, "light": 255, "pos": [x,y,z], "yaw": 0,
                 "boxes": [ [x1,y1,z1,x2,y2,z2,tex], .. ], "quads": [ [x,y,z, x,y,z, x,y,z, x,y,z, tex], .. ] } ] }
```

Errors name the element (`objects[3].boxes[1] needs 7 numbers`). `name` and `note` are free text the loader ignores.

## Deqq's House II on the mesh renderer

Since 2026-11-14 `demos/deqq2.bas` runs on `M3D` (`demos/deqq2.mesh.json`, the source of its world). The first version of the document was converted from the sector document `demos/deqq2.world.json` (which stays: the sector renderer's loader and `worldtest.sh` still use it): **every room became an object with the room's number** (a floor slab, a ceiling slab where there is one, walls 8 thick inside the room where there is no opening, lower and upper wall pieces round an opening between rooms of different heights, floor and jamb pieces across the gap between rooms), windows and railings are separate objects (ids 1000 + the opening's number: masked or translucent panes; 1500 +: invisible rails; 2000 +: fences), the doors 6, 7, 8 and 33 are leaves that turn about their hinge (`M3D OBJPOS`), 3000 is the garden gate (`M3D OBJSHOW`), 51 the chair platform, 600 the ground (with holes where the cellar lies), furniture and houses are boxes. Rooms of the old document are the zones. `M3D OBJLIGHT room,level` darkens or lights a room. Facts found while doing it: floors and ceilings are 1 unit thick so a floor above a room does not lower it; a thing that stands on furniture needs the furniture's top as its level (`TLZ` in the game's `OBJDATA`: values from 3 up are used as they are, 114 / 226 are the upper floor and the attic: `M3D("FLOORAT",x,y,z)` takes the highest surface at or below z, and the ceiling slab of the room below is at 112 .. 113); the test hooks of `deqq2test.sh` take the same hints (`goto x y angle z`).

## Not done yet

Rotation about other axes (an object turns only about z), true tilt, per-vertex colors, texture animation / scrolling, the hardware path, a mesh file format, fast culling for very large worlds (all objects are visited each frame and each ray; fine for a few thousand), speed tuning. Missing for Deqq's House II: the light effects (flickering cellar, pulsing crash site) and the scrolling water, the pond is a still picture; the generated world is a first conversion (walls are 8 thick inside the rooms, so openings are as wide as before but rooms are 16 narrower: a hand-made mesh world would look better).

## Tests

`m3dtest1` (zones, walking things and the world document too; in `make check`, also clean under ASan / UBSan): the picture (wall, floor, depth order, culling, two sided, clipping inside a box, masked, translucent, fog, lamps, sky, things), movement (walls, sliding, steps, low ceiling, jump, passable), floors, shots, use, sight, a door turned about its hinge, errors, 300 random frames.
