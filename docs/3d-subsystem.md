# The 3D sector renderer (`W3D`)

*Roadmap item 78, 2026-11-08. Module `w3d.[ch]` (no SDL, no global state), statements and functions `W3D ...` (extension word `XF_W3D`), tests `w3dtest1`
and `vmtest1`, demos `demos/manor.bas`, `demos/bughunt.bas`, library `demos/lib/w3dkit.lib`.*

> **Status (2026-11-14):** the sector renderer is a supported feature and is **not planned for removal**. A second renderer, the mesh renderer `M3D` (`docs/3d-mesh-subsystem.md`), exists next to it; Deqq's House II uses that one. The demos of *this* renderer are Manor, Bug Hunt, the Showroom, Deqq's House I, the server room of Y2K38 and the map editor (`demos/README.md` marks them).

## What it is and why software

A world in the style of **Doom and Duke Nukem 3D**: *sectors* (a floor and a ceiling height, a floor and a ceiling picture, a light level) bounded by
*walls*; a wall with a sector behind it is a *portal* (doorway, stairs, window, lift), plus flat pictures that always face the viewer (*things*). It is drawn by
the CPU into a view on a graphics layer and works with everything else (text, sprites, `GFX` drawing on top of it, `GFX GRAB`, palette fades).
The owner's question (2026-11-08) was OpenGL 2 to 4 or Vulkan through drivers, or software only. **Software was chosen:** it fits the palette bitmaps
of the engine (one byte per pixel), runs everywhere the engine runs, is deterministic and testable without a GPU, and `BUILD` executables keep linking with
`ld` only (no GL / Vulkan libraries, no driver stack). A hardware path stays possible later behind the same BASIC statements (the renderer is one
function, `w3d_render()`, with a small interface); see "Not done".

## How it draws

Every screen column is traced through the sectors with its own **2D ray**: find the nearest wall of the sector the ray is in; draw its ceiling and floor
rows up to there; a solid wall is drawn and ends the column; a portal draws the upper and lower step walls (when the neighbor's ceiling is lower or its
floor higher), narrows the window of rows that is still open, and the ray goes on in the neighbor sector. So **any sector shape works** (concave, with
holes: pillars are an inner loop of walls), no sorting or BSP tree is needed, and the walls of a sector need no order. The ray's parameter is the *depth*
along the view direction, so there is no fish-eye correction; floors and ceilings are mapped per pixel (perspective correct), walls per column. The view can
look up and down (the horizon moves: a shear, as in Duke Nukem 3D). A depth buffer (one float per pixel) lets things and masked walls be drawn in any order.
Brightness comes from the sector's light level and the distance (`W3D FOG`) through a **color map** of 32 brightness rows that `W3D COLORMAP` builds from
the layer's palette (the nearest palette entry to the darkened color, with a penalty for a change of tint; palette ramps of one hue make it look right).
Speed (measured, one core): 0.5 ms per frame for a 320 x 150 picture, 2 ms for 640 x 300, with a handful of sectors in view.

## Conventions

- **Units** are free; the defaults assume a player 56 high with the eye at 46 (`W3D PLAYER` changes that), a step height of 24, a sector 128 high, and a texture pixel = one unit. Heights are `z` (up), the map is `x`, `y`.
- **Angles** are in degrees whatever `DEG` / `RAD` says: 0 = along +x, 90 = along +y (counterclockwise on a map with y up); to the right of the viewer is clockwise. Pitch is -80 .. 80 (positive looks up).
- **Walls** are listed **counterclockwise** around their sector as seen from above (y up): then the picture runs left to right. A portal wall is listed in *both* sectors (each with its own `next`), with the same end points.
  Holes (pillars) are listed clockwise. Nothing checks this; a wrong order only mirrors the picture.
- **Textures** are numbered 0 .. 1023 and are pictures in the `GFX GRAB` format (`MKI$(w) + MKI$(h) + w * h palette indices`, up to 1024 x 1024). **A texture number that was never defined is drawn as the solid palette color of that number** (a quick way to
  try a map). Index 0 is see-through in things, masked walls and the sky; on a solid wall or a floor it leaves the pixel transparent (the layers below show).
  Wall pictures are anchored to the world height (the bottom edge at multiples of the picture's height) and repeat; floors and ceilings are anchored to the world's x, y.
- **Sectors** are numbered 0 .. 4095, walls per sector (the number is the order they were added in, 0, 1, ...), things 0 .. 1023. Sectors may lie over each other (see "Rooms over rooms"); a ray only ever reaches the sectors that are joined by portals.
- The **layer**: `W3D RENDER` draws into the layer `GFX TARGET` names (0 or 3), replacing the pixels of the view. The program draws its HUD on top with `GFX` statements. For a picture without flicker draw into the layer that is not shown and swap (`w3dkit`'s `W3K_SHOW`);
  the tile and text layers (1, 2) lie between layers 0 and 3, so a program that swaps those two hides them (`W3K_SETUP` does).

## Statements

| Statement | Meaning |
|---|---|
| `W3D RESET` | forget the world, the textures and the things (the view stays) |
| `W3D TEXTURE n, a$` | texture n from a picture string |
| `W3D SECTOR s, floor, ceil [, floortex [, ceiltex [, light]]]` | define (or change) a sector; `ceiltex` -1 = the sky (`W3D SKY`); light 0 .. 255 (255 default); a new sector has floor and ceiling picture 1 |
| `W3D WALL s, x1, y1, x2, y2, tex [, next]` | add a wall to sector s; `next` = the sector behind it (default -1: solid) |
| `W3D WALLTEX s, i, mid, upper, lower` | pictures of wall i of sector s (-1 = keep): `mid` for a solid wall (and the masked picture of a portal), `upper` / `lower` for the steps of a portal (all start as `tex`) |
| `W3D WALLOFF s, i, xoff, yoff` | shifts the picture of a wall |
| `W3D WALLFLAG s, i, flags` | 1 = masked (the `mid` picture is drawn over the opening of a portal, index 0 see-through; it does not repeat vertically: it sits on the higher floor, `yoff` raises it), 2 = movement can't pass the portal (window, railing), 4 = shots and sight can't pass it (frosted glass). Flags belong to one side: set them on both walls of a window, 8 = the wall shows the sky picture (a painted horizon), 16 = translucent (a portal: the picture is mixed with what is behind it) |
| `W3D HEIGHT s, floor, ceil` | moves floor and ceiling at once (doors: ceiling = floor is closed; lifts); see `SECMOVE` for a smooth move |
| `W3D GHOST s, flag` | a ghost sector is only seen through portals: it is never the sector of a position (camera, thing, `SECTORAT`), so it may lie over other sectors and serve as a view (the lawn behind the fences of Deqq's House II); collision ignores its walls too (it is nobody's neighbour), so a ghost sector's solid railing never blocks the bodies next to it |
| `W3D WORLD` | builds the world from the current JSON document (`JSON LOAD "deqq2.world.json"` first): rooms, solids, openings, things, environment, start (`docs/world-model.md`, `w3dworld.[ch]`); it replaces the sectors and things (like `W3D CLEAR` first), pictures and settings stay; errors name the element ("rooms[3]: ...") |
| `W3D LIGHT s, light`, `W3D TAG s, value` | brightness; a number of your own per sector (room names, door kinds, the exit) |
| `W3D SKY tex [, repeat [, shift]]` | the picture of the sky, mapped around the viewer by angle (it goes around `repeat` times, default 1) and moved by `shift` rows |
| `W3D CAMERA x, y, angle [, pitch [, lift]]` | puts the viewer somewhere (error "Position outside of all sectors"); `lift` is added to the eye height (jumping, crouching) |
| `W3D TURN dangle [, dpitch]` | turns the viewer |
| `W3D MOVE forward, right` | walks relative to the view direction, with collision: slides along walls, up steps of at most the step height, through portals the body fits through (opening at least the player's height); sets `W3D("HIT")` |
| `W3D LIFT h` | the height of the feet above the floor, for a jump (`W3D("LIFT")` reads it): the eye rises with it, and **steps up to the step height + h can be taken** (so a jump gets out of a pool whose edge is too high to walk); a program makes the arc itself (`W3K_JUMP` in `w3dkit`: up to 40 units, 0.7 s) |
| `W3D PLAYER radius, height, eye, step` | the body (defaults 14, 56, 46, 24) |
| `W3D VIEW x, y, w, h [, scale [, fov]]` | the picture's place on the layer in layer pixels (w 8 .. 1280, h 8 .. 800); `scale` 1 .. 4 draws every pixel of a (w / scale) x (h / scale) picture scale x scale times (default 1); `fov` 30 .. 150 degrees, 90 default. The default view is 0, 0, 640, 300 at scale 2 |
| `W3D FOG distance` | the distance where light fades to black (default 1200, 0 = no darkening by distance) |
| `W3D COLORMAP` | builds the brightness table from the palette of the target layer (call it after the palette is set up; before the first call all rows are the identity, so no shading) |
| `W3D RENDER` | draws |
| `W3D THING n, x, y, tex, w, h` | places (or moves) thing n: a picture w x h units that stands on the floor of the sector it is in and faces the viewer |
| `W3D THINGZ n, z` | height above the floor (flying things) |
| `W3D THINGTEX n, tex`, `W3D THINGLIGHT n, light`, `W3D THINGOFF n` | change the picture (animation); a fixed brightness 0 .. 255 instead of the sector's, no darkening by distance (torches, shots; -1 = the sector's); remove |
| `W3D THINGMOVE n, dx, dy` | walks a thing in world directions with the same collision as `W3D MOVE` (radius 0.35 * width, its height, step 24); sets `W3D("HIT")` |
| `W3D SLOPE`, `W3D FAR`, `W3D TICK`, `W3D SECMOVE`, `W3D ANIM`, `W3D SCROLL`, `W3D LIGHTFX`, `W3D LAMP`, `W3D THINGTRANS`, `W3D CLEAR` | see "Newer features" |

## Functions

New ones: `("SECTORZ", x, y, z)`, `("CEILAT", x, y)`, `("SLOPE", s, kind, axis)`, `("VISITED")`, `("MOVING", s)`, `("TIME")`, `("LIGHTNOW", s)`, `("WALLINFO", s, i, k)`, `("SECINFO", s, k)`, `("THINGINFO", n, k)` (see the sections below and `gfxops.h`).

`W3D("X")`, `("Y")`, `("Z")` (the eye, absolute), `("ANGLE")` (0 .. 360), `("PITCH")`, `("SECTOR")` (-1 if none), `("FLOORZ")`, `("HIT")` (bits of the last move: 1 a wall pushed the body, 2 it could not move at all, 4 it changed sector),
`("SECTORS")` (highest number + 1), `("LIFT")` (the jump height), `("SECTORAT", x, y)` (-1 outside), `("FLOORAT", x, y)` (-1000000 outside), `("FLOOR", s)`, `("CEIL", s)`, `("LIGHT", s)`, `("TAG", s)`, `("WALLS", s)` (= the number the next wall gets),
`("TX", n)`, `("TY", n)`, `("TZ", n)`, `("TSECTOR", n)`, `("TON", n)` (-1 if the thing exists), `("DIST", n)` (from the viewer), `("BEARING", n)` (degrees from the view direction, positive = to the right, -180 .. 180),
`("TEXW", n)`, `("TEXH", n)` (0 if undefined),
`("SHOOT", range)`: shoots along the view direction (also up and down by the pitch) from the eye: returns the nearest *thing* hit (a thing is hit within 0.4 of its width of its center and between its feet and its top), -1 if there is none; walls and closed doors stop the shot, portals pass it if the opening is there at the height of the shot.
Afterwards `("SHOTKIND")` is 0 (nothing within range), 1 (a wall) or 2 (a thing), `("SHOTDIST")` the distance, `("SHOTX")`, `("SHOTY")` the place.
`("USE", reach)`: the sector behind the portal wall in front of the viewer within reach (-1 if the nearest wall is solid or none): the "open the door" test (door sectors carry a `TAG`).
`("SEES", n)`: -1 if thing n has a line of sight to the viewer (from 70 % of its height to the eye; closed doors, solid walls and frosted glass block it), else 0.
A bad number gives "Illegal argument".

## Newer features (2026-11-12)

These came with the showroom demo (`demos/showroom.bas`, its map is made by `demos/levels/showroom.bas`). All of them are in `w3d.c` and tested by `w3dtest1`.

### Rooms over rooms

Sectors may overlap in the plan: a hall and a gallery over it, a bridge and the road under it. Rendering never needed more than the portals (a ray only goes where portals lead), so
nothing changed there; what was missing is **finding the sector of a point**. `W3D("SECTORZ", x, y, z)` returns the sector at a point *and a height* (the one whose floor is the highest at or below z and whose ceiling is above it,
else the one with the nearest floor), `W3D CAMERA x, y, angle, pitch, lift, sector` and `W3D THING n, x, y, tex, w, h, sector` name the sector explicitly (an error if the point is not inside it; naming a sector for the camera also snaps the eye
to that floor instead of easing). `W3D("SECTORAT", x, y)` prefers the viewer's own sector when it contains the point. Movement and collision change sector only through portals, so stacked rooms do not interfere; stairs or a ramp (below) join them.
Nothing joins the floors of stacked rooms automatically: there is no hole to look through.

### Slopes

`W3D SLOPE s, kind, gx, gy [, ox, oy]` tilts the floor (kind 0), the ceiling (1) or both (2): `gx`, `gy` = rise per unit in x and y (|gx|, |gy| <= 100), around the point (ox, oy) (default: the middle of the sector's walls),
where the sector keeps the heights given by `W3D SECTOR` / `W3D HEIGHT`. Walls, portal steps, flats, things (their feet), the viewer's feet, collision (a step is measured at the place where the body crosses the wall), shots and sight follow the plane.
`W3D("FLOORAT", x, y)`, `W3D("CEILAT", x, y)`, `W3D("FLOORZ")`, `W3D("SLOPE", s, kind, axis)` read it. A floor is drawn per pixel by solving the plane for the pixel's ray (`depth = (eye - h) * f / (row - horizon + slope along the ray * f)`).
Slopes of a few percent walk like ramps; the step limit still applies to the portal walls (a ramp that meets a floor at a different height needs a step of at most 24, as before).

### Sky, draw distance

`W3D SKY tex [, repeat [, shift]]`: the sky picture goes around the viewer `repeat` times (0.1 .. 64, default 1: a 256 pixel picture repeated 3 times is sharper than stretched once), `shift` moves it up or down in rows.
A wall with the flag **8** shows the sky picture over its whole height: a *painted horizon* at the end of the world, so no ray ends in empty space (the fence sectors of Deqq's House II are the same job done with a backdrop picture).
`W3D FAR d` stops the rays at distance d (0 = no limit): with `FOG` fading to black at d it saves the work of far sectors without a visible change; `W3D("VISITED")` is the number of sectors the last frame reached.
(A single ray still passes at most 64 portals.)

### Time: movers, scrolling, animated textures, light effects

The world has a clock that **the program advances**: `W3D TICK seconds` (`w3dkit`'s `W3K_WALK` does it for you with the time since its last call). It drives:
- `W3D SECMOVE s, speed, floor [, ceil]`: the floor and ceiling of sector s move to these heights at `speed` units a second (`W3D("MOVING", s)` is -1 while they do; `W3D HEIGHT` stops a mover). Lifts, doors, crushers: a program only starts them and asks when they are done.
- `W3D ANIM n, first, count, fps`: texture number n shows the textures first .. first + count - 1 in turn (count 0 = off). Use n in walls, floors and things; the frames are textures like any other.
- `W3D SCROLL s, kind, dx, dy`: the floor (0), ceiling (1) or walls (2) of the sector slide by dx, dy units a second (water, conveyor belts).
- `W3D LIGHTFX s, mode [, amount [, rate]]`: 1 flicker (random between light - amount and light, `rate` changes a second), 2 pulse (sine, plus / minus amount, `rate` periods a second), 3 strobe (off by amount every other half period); `W3D("LIGHTNOW", s)` is the light of the last frame.
`W3D("TIME")` reads the clock.

### Lamps and translucency

`W3D LAMP n, x, y, z, radius, amount` (n 0 .. 7; radius 0 or amount 0 switches it off): a point light that adds up to `amount` of brightness at its center, falling linearly to nothing at `radius` (in 3D), to walls, floors, ceilings and things.
It brightens; the light is a level of the color map's shading, so there are **no colored lights** (a palette of hue ramps would need a color per level). Each lamp costs a little per floor pixel: a few are fine.
**Translucency:** a portal wall with the flag **16** draws its middle picture over the opening mixed half and half with what is behind it (palette index 0 is see-through; like masked walls the picture sits on the higher floor and does not repeat vertically);
`W3D THINGTRANS n, flag` does the same for a thing. The mix is the palette entry nearest to the mean of the two colors, from a 256 x 256 table that is made from the palette on the first frame that needs it (about 50 ms; it is made again after `W3D COLORMAP`).
Translucent walls are drawn after the things (at most 4 per screen column), so a thing behind a pane of glass is seen through it.

### Map files, the generators and the editor

A map is a text file (`demos/lib/w3dmap.lib`, `LOAD LIBRARY "w3dmap"`: `N = FN WM_LOAD(file$)`, `CALL WM_SAVE(file$)`, `CALL WM_CLEAR`, `CALL WM_LINE(l$)` for lines that come from `DATA`; the format is in the library's header). The map holds the geometry, the sky and fog,
the things; the pictures, animations, effects, lamps and the player belong to the program. `W3D CLEAR` deletes sectors and things and keeps the rest; the functions `W3D("WALLINFO", s, i, k)`, `("SECINFO", s, k)`, `("THINGINFO", n, k)` read everything back (that is how `WM_SAVE` works).
`demos/levels/mkw3d.lib` gained **polygon sectors** (`MK_POLY(id, floor, ceil, look$, "x,y,x,y,...")`, `MK_PLINK` / `MK_PLINKT` / `MK_PTEX` for the edges, `FN MK_ARC$(cx, cy, r, a1, a2, steps)` for curves), `MK_SLOPE`, `MK_THING`, `MK_RAW` and the writers `MK_EMITMAP` / `MK_EMITMAPDATA`.
The map editor is `tools/mapedit.bas` (see `tools/README.md`).

## Doors, lifts, stairs

These are sectors. A **door** is a thin sector between two rooms with its ceiling at its floor (closed: the portal walls show the door picture as their upper wall; nobody fits through) that `W3D HEIGHT` raises. A **lift** is a sector whose floor `W3D HEIGHT` raises; the player stands in it and the eye follows the floor smoothly (the camera eases to the floor of its sector: stairs and lifts do not jump). **Stairs** are a row of
thin sectors, each floor 16 higher than the one before (a step of at most 24 can be walked up). A step that is too high (the pool in `manor.bas`) or a ceiling that is too low blocks the player. `demos/levels/` has a small generator for such levels made of rectangles, written in the built-in BASIC (the library `mkw3d.lib`; the level programs `manor.bas` and `bunker.bas`, run with `engine1999c`, print the `DATA` lines that the demos contain).

## The demos and the library

`demos/lib/w3dkit.lib` (`LOAD LIBRARY "w3dkit"`): `W3K_SETUP` (palette: 15 hues x 16 shades at 16 .. 255 on layers 0 and 3, `W3D RESET`, hides the tile and text layers), `FN W3K_C(hue,shade)`, the texture makers (`W3K_BRICK`, `STONE`, `PLASTER` (64 x 128 with a wainscot), `METAL`, `CARPET`, `TILES`, `PLANKS`, `GRASS`, `DOORTEX`, `WINDOW` and `GRATE` (see-through), `SKY` (256 x 64)),
the sprite makers (`PLANT`, `LAMP`, `BARREL`, `TORCH` (two frames), `MEDKIT`, `AMMOBOX`, `KEYCARD`), `W3K_WALK` (arrows / WASD / R F look / SHIFT run or a game controller: moves and turns by the time passed), `W3K_SHOW` (the flicker free swap), `W3K_HUDBAR`, `W3K_DONE`. `manor.bas` is a walk through a house (doors, stairs, a pool, a window, a cupola, a cellar); `bughunt.bas` is a game (installed as `engine1999-bughunt`).

## The user's 3D settings (`demos/lib/prefs3d.lib`)

A library, `LOAD LIBRARY "prefs3d"`: **invert the X axis** (the right stick turns the other way), **invert the Y axis** (looking up and down), **gamma** (0.6 .. 1.8; it shapes the shades of the palette that `W3K_PALETTE` makes, so it also changes how the light falls off), **field of view** (60 .. 110) and **turning speed** (50 .. 200 %).
`CALL P3_LOAD` reads `local:3dprefs.cfg` (the file of the driver `local:`, `~/.local/share/<program>/`: **each program has its own copy**; a shared file would need a driver feature) and applies the settings; `CALL P3_DIALOG` shows the dialog over the picture of the world
(keys: UP / DOWN choose, LEFT / RIGHT change, RETURN saves, ESC cancels, DELETE defaults; pad: D-pad or left stick, A, B / BACK, X; changes show at once) and the programs open it on **O** (or the pad's **Y**). The programs honor the settings through `w3dkit`: `W3K_WALK` (inversion, turning speed), `W3K_VIEW(x,y,w,h,scale)` instead of `W3D VIEW` (field of view), `W3K_PALETTE` (gamma). The file is five numbers after a `3DPREFS 1` line.
**Renderer** (programs on the mesh renderer, `W3K_MESH`; the row appears only when `M3D("HWAVAIL")` is true): `P3_REND` 1 = OpenGL (hardware), 0 = software; the default is hardware, `W3K_MESH` switches to it at once (`M3D HARDWARE`) and `W3K_SETRENDER` (called by `P3_PUSH`) follows the dialog; it is the sixth line of the settings file (older files without it get the default). The dialog draws the world with the program's own renderer (`W3K_DRAW3D`).

A library can not use the variables of another library ("Variable W3K_INVX is global in the library"), so `prefs3d` keeps `P3_INVX`, `P3_INVY`, `P3_GAMMA`, `P3_FOV`, `P3_TURN` and hands them over with `W3K_SETPREFS`. Settings are 3D renderer *demo* settings: the `W3D` statements themselves know nothing of them.

## The demos as examples of what the engine can do

`manor.bas` (a house to walk through), `bughunt.bas` (a shooter: monsters that chase and shoot, keys, a lift) and **`deqq2.bas`** (Deqq's House II, a stealth adventure). In Deqq's House II everything you touch is a *thing* with a table of properties; the program finds the one in front of you with `W3D("DIST")`, `W3D("BEARING")` and `W3D("SEES")` (E takes or uses it); doors and the chair platform are sectors whose heights change (`W3D HEIGHT`); Deqq walks along a list of way points with `W3D THINGMOVE` (which keeps him inside the walls and on the stairs), sees you when `W3D("SEES", 0)` and the angle between his walking direction and you fit, follows you, loses you when he has not seen you for 3.5 seconds and carries you out when he touches you (a closed door stops him: he can not open one); a *hiding place* is a small dark sector with the tag 50 (he notices you there only from 55 units). The jump (`W3D LIFT`) gets you onto the chair. Its whole logic runs in the console host without a picture (`DEQQ2_HEADLESS=1`, `DEQQ2_PLAY=file`), which is what `deqq2test.sh` does.

## Things are drawn only in sectors that were reached

`w3d_render()` notes which sectors a column of the picture got into; **a thing is drawn only if its sector was reached** (before 2026-11-08 every thing in the world was drawn with the depth test only, so furniture behind a low wall showed over the sky above it). A thing in a reached sector is still hidden by nearer walls (the depth test), a thing in a sector you only see over a wall (above it) is not drawn: put a wall there or make the sector tall.

## Buildings seen from outside (a trick of `deqq2.bas`)

A sector can show nothing above its own ceiling, and a portal can not be higher than the lower of its two sectors, so a house that is to look tall from the garden needs **tall garden sectors** (300 here, a roof band at 256 .. 300), and a low fence needs **low sectors**. `demos/levels/mkw3d.lib` therefore turns every solid wall of a tall outdoor room that is not part of the house into a *fence sector*: a thin sector behind the wall, as tall as the garden (300; `MK_FINISH(HT, FH, FTX)`), joined by a portal that blocks movement, shots and sight (flags 2 and 4); its far wall is a 300 high *backdrop picture* (`deqq2.bas`: texture 29, a 100 high brick fence below a night sky and a dark tree line), so you see a low fence with a tree line behind it and **no ray ever leaves the world** (earlier the fence sectors were 100 high with the sky above: over a fence you saw empty space where the house should be, and a 100 high room such as the crash site cut a tall house off at height 100 because a portal is never higher than the lower sector). Rule of thumb: **every ray must end on a wall, and a sector you look through must be at least as tall as what lies behind it.** Rooms need `MK_HOUSE` or `MK_OUTDOOR` to get fence sectors. The house walls get a tall picture (`W3D TEXTURE` 128 x 300: a roof band, two belt courses at the floor levels, plain bricks; no painted windows, the wall height is exactly the picture's height; **the graphics layer is only 300 pixels high, so no picture and no outdoor sector can be taller than 300**); the door in it is a 64 x 300 picture (the portal's upper wall); a hatch in a fence is a niche sector of fence height in front of the door sector. Since 2026-11-13 (roadmap item 89) the house is a real two-storey house with an attic: see "A house with storeys" below. The garden parts that are not next to the house (the east garden, the crash site) are simply low.

## Music (`demos/lib/music3d.lib`)

The two games (`bughunt.bas`, `deqq2.bas`) have music: `LOAD LIBRARY "music3d"` (`MU_`) keeps loops of `PLAY` strings going on the music channels 1 .. 4 (the sound effects use 9 .. 16) in **moods**: 1 title / intro, 2 calm (a slow, ponderous tune), 3 uneasy (the calm tune with a heartbeat), 4 danger (fast, driving), 5 a one-shot sting (game over, caught), 6 a one-shot fanfare (victory, the ending). `MU_USE(set)` chooses the tunes (1 = Bug Hunt, A minor; 2 = Deqq's House II, D minor), `MU_MODE(m)` changes the mood (ignored while a one-shot plays; `MU_AF` is the mood that follows a one-shot), `MU_FORCE(m)` also breaks off a one-shot, `MU_TOGGLE` is the **M** key, `MU_SILENCE` / `MU_START` are used around a pause. `W3K_SHOW` calls `MU_TICK` in every frame, so the next loop is queued behind the current one shortly before it ends (no gap) in all screens that show pictures (the title, the dialogs, the banners); the loop lengths are exact because the strings keep their own tempo. The games decide the mood: Bug Hunt is in danger while a bug that has noticed you is within 520 units or a glob flies (and for 5 seconds after), Deqq's House II while Deqq follows you (and for 4 seconds after), uneasy while he is within 380 units. The tunes are composed in `demos/levels/music3d.bas` (it checks that all voices of a tune are equally long and writes the `PLAY` procedure of the library). In the console host the `PLAY` statements only check their strings, so `deqq2test.sh` finds a mistake in a tune.

**The tunes use the synthesizer's instruments** (2026-11-10, roadmap item 81): every voice selects a factory preset with `I n` in its `PLAY` string (4 BASS for the bass lines and sting / fanfare basses, 6 PAD, 5 LEAD for the title and danger leads, 7 PLUCK for the danger arpeggios, 14 SNARE, 13 KICK for the heartbeat, 12 BELL for the bunker pings and the shimmer, 19 EPIANO for the calm melody of Deqq's House II, 9 STRINGS for the stings, 10 BRASS for the fanfares); before, the voices were the four basic waves (`W n`). Because the presets are calibrated to the same loudness (`docs/sound.md`), the volumes `V` were chosen to keep every voice's share of the mix: measured offline (the old wave mixes against the instrument mixes, RMS and peak; the renderer was a scratchpad tool), the tunes are within about 3 dB of the old ones, the slow Deqq tunes 5 dB quieter (the old ones peaked at 1.0), so effects stand out a little more. The tunes are made by `demos/levels/music3d.bas` (its voices carry the `I` / `V` values; the generator's length check knows `I`). The sounds themselves are **not heard by the owner yet**: change the instrument numbers / volumes in the generator and paste its output into the lib (`levelstest.sh` checks that they match). The music depends on the factory presets: a change of a preset changes the music.

## Hosts

The window host draws. **The console host runs the whole world** (`W3D RESET` ... the functions, collision, shots, sight) **but draws nothing** (`W3D RENDER` and `W3D COLORMAP` return at once), so the logic of a game can be tested headless. The graphics layer statements around it (`GFX ...`) are "Not available in the console host" there, as always.

## Design decisions (revisit if unwanted)

- Software, per-column 2D ray tracing instead of a BSP or a sector sort (any shape, simplest, fast enough); no room-over-room, no sloped floors, no sprites with several view angles, no sector-wide lighting effects other than the light value; things are always upright billboards.
- Statements and functions are one group, `W3D` (a new extension word), with the usual `W3D name args` and `W3D("name", args)` forms; the world is one global world per host.
- Walls are listed per sector and counterclockwise; a portal is given from both sides (so each side can have its own pictures and flags).
- Player movement and collision, shots, sight and the door test are *in the engine* (a BASIC loop over all walls would be too slow); monsters, pick-ups, doors and the lift are programs' business.
- The viewer's height follows the floor smoothly, 40 % of the distance per drawn frame (frame rate dependent; a program that wants something else sets `lift`).
- Sky pictures wrap around once; the eye height is clamped to 2 below the ceiling.
- An undefined texture number is a solid color (handy for trying a map), a thing without a picture is a rectangle of that color.

## Not done

Hardware rendering (OpenGL / Vulkan; the interface would be `w3d_render()`), **holes between stacked rooms** (a floor opening you can look down through: stacked sectors are only joined by stairs, ramps and the like), **true tilt** of the view (looking up and down is still a shear of the picture, fine to about 30 degrees; mouse look exists in `w3dkit`: right button and drag), colored lights (lamps only brighten), multi-angle sprites, a map view function (the editor shows a plan), more than 4 translucent walls in a column, things on moving floors (they stay where they are; `W3D THINGMOVE` re-reads the floor), a sky that follows the view up and down (it moves with the horizon), a level editor that edits things and textures (it keeps the things a map has), floors that move things standing on them, texture mapping of the `W3D SECTOR` ceiling per sector offset, and things that block the player (do it in the program with `W3D("DIST", n)`).

## Tests

`./w3dtest1` (no display; 202 cases, 91 of them the original ones, the rest slopes, stacked sectors, sky walls, the draw distance, movers, animation, scrolling, light effects, lamps and translucency; also run under ASan / UBSan; `sh w3dmaptest.sh` (map files: save, load, save again, the files must be equal), `sh showroomtest.sh` (the showroom map and a walk through it in the console host) and `sh mapedittest.sh` (the editor's model) are in `make check` as well; the picture of a small world pixel by pixel from the four directions, looking up, textures and offsets, masked walls, sky, light and the color map, concave rooms, movement (walls, sliding, doorways, steps, low ceilings, blocking portals), things, shots, sight, use, queries, errors, and 600 random frames that must keep the viewer inside its sector); `./vmtest1` has the 35 statement / function forms (parser, code generator, VM);
the demos were run under Xvfb (screenshots of every room, keys held with XTest, a play through the first fights).

### A house with storeys (Deqq's House II)

The house is x 180 .. 840, y 300 (front) .. 730 (back); the storeys are 112 high (ground 0 .. 112, upper 112 .. 224, attic 224 .. 300, the garden sectors 300). The rooms upstairs lie **over** the rooms downstairs (kitchen under the bathroom, living room under the bedroom, the back of the hall under the landing, the attic over everything), the stairs stand in the middle of the hall (the hall is a ring of four sectors round the stair island), the ladder to the attic runs along the north wall of the bedroom and ends in a hole of the attic's plan (the attic is two rectangles: the steps from the third on are taller than the attic floor, so the attic must not lie over them; the opening between two sectors has to be at least the player's height, 56). The plan is in the world document `demos/deqq2.world.json`.

**Windows are real**: a window is a niche sector in the thickness of the wall (`WIN` in the generator) whose two portals carry a translucent glass picture (texture 200, wall flags 2 + 16: nothing passes, everybody sees through; no flag 4, so `W3D("SEES")` and the shots' sight go through it): the window you see from the garden is the window you look out of inside, at the same place and height. Windows on different storeys can not lie above each other, because a garden wall holds one portal per span: the upper windows are shifted sideways.

**Things upstairs need their sector**: `W3D THING ... , sector` (the game takes it from `W3D("SECTORZ", x, y, z)` with a floor height per thing: the `LV` column of `OBJDATA`), and the test commands `goto x y angle [z]` / `deqq x y [z]` take a height. **`trace()` (sight, shots, `SEES`) now also stops at the floor and the ceiling of the sector it is in** (it only looked at walls before, so a thing upstairs was "seen" through the ceiling).

### The town round Deqq's House II

Since roadmap item 90 the premises lie in a small town (the 1983 text adventure had one): a **ring road** (160 wide, sectors 400..417) round the block x -440 .. 1530, y -60 .. 1580, in segments whose walls are different painted *house fronts* (textures 210..214, 128 x 300; the houses are only fronts, nothing can be entered); in front of the premises a pavement (418), a low **railing** (419, 421: a masked picture, flags 1 + 2: you see over it, nothing passes) and the **gate** (420: its floor rises to 80 when it is shut, a lower wall of 80 that you can not jump over). The other sides of the premises keep their solid fences with the backdrop. Sector numbers 100 and up are taken by the fence sectors of `MK_FINISH`, so the town starts at 400. The cellar lies under the west road (stacked sectors): things there carry the floor height -60 in their `LV` column (`SECTORZ` at height 1 would find the road).

**Deqq** has two modes: *at home* (the round of the house, 0 .. 48 in the way point list, as before: he hunts you on the premises) and *away* (way points 50 .. 72: out of the gate, once round the block, in again; he ignores you while he is in the street, `DYP < 0`). After a minute or two at home (`DQHT`) he leaves through the front garden; coming back he is announced at the gate (`THE GATE CREAKS`). New game: he starts in the street. A chase stops at the fence if you leave through the gate. **Caught in the garden** (outside the house and the cellar) he throws you out: you stand on the pavement facing the road, the gate is shut for 25 s (`GATET`); caught in the house you are put in the front garden as before.

**Cars** (`CARS` in `deqq2.bas`): up to three, a new one every 15 .. 40 s, they drive the outer lanes anticlockwise once round the block (150 units a second, 51 s) and vanish; a car with you in front of it on the road brakes and honks (`SFX 11`), a passing car makes a noise (`SFX 10`). A car is a thing whose picture depends on the side you see it from: right-facing side, left-facing side, front, back (`CARSIDE`, `CARENDS`, `MAKECARS2`; textures 230..241, different sizes: the thing is placed again every frame with the width and height of the picture).

**The open country, the neighbourhood and the fence** (2026-11-13, owner: a town map first; houses of one or two storeys with yards and fences, shops, a gas station, a phone booth as the start; the high wall round the premises becomes a fence; the map is in `docs/town-map.md`): the outer side of the ring road is open: four grass strips (sectors 422..425, 300 high, sky above, a far backdrop 2400 .. 3000 units away, `W3D FOG 4200`) hold the buildings as blocks: shops (`SHOP`, 224 high, own 256 x 224 pictures 285..288), the gas station (forecourt 430.., kiosk 289, pillars, price sign 290, pump sprites 281) and **yards** (`YARD`: a grass lot inside the strip whose ring of walls is the masked railing 250 with flags 3, made with the new `MK_INNERF`; in it a house of one or two storeys, 112 / 224 high with its own pictures 260..263 / 270..273, a door sprite 280 and trees). The generator writes the town's things (doors, trees, pumps, the telephone booth 282) as `DATA` lines after the sectors (ended by `DATA -1`), `LOADLEVEL` reads them. The premises' fence: `MK_FENCE(250,3)` makes the portal between a garden and its fence sector the masked railing (default: invisible barrier). Behind the fence there is **no wall at all**: `MK_GROUND(426)` makes the far wall and the two end caps of every fence sector portals into sector 426, a *ghost* lawn over the whole block (tag 97; `W3D GHOST s, 1` in `LOADLEVEL`: such a sector is only seen through portals and never found by a position, so lying over the premises does no harm), whose edge is the railing again, through which the ring road and the town are seen; the pavement's and the railing's ends open into the lawn the same way. (Before that a skyline picture, then a brick wall, closed the 20-unit fence sectors.) The inner side of the road (the block's edge) is a hedge (214). Rules found: a hole must not touch the strip's walls or another hole (black gaps); a sector's ceiling must be above its floor; a `PROC` has at most 8 parameters; `BUILD` and `THING` are not usable as procedure names; the fields lie over the cellar's plan, so a position in the cellar needs its height (`goto -860 590 180 -60`) and no yard may stand over the cellar. The game starts on the pavement next to the booth (340, -35) with a banner (the investigator has just phoned his chief). `TOWNTHINGS` places the lamp posts and the trees along the road (things 700 ..). **Deqq does not notice you and stops chasing as soon as you are off the premises** (your sector is 400 or higher: road, pavement, gate, fields).

### Deqq's House I on the renderer (roadmap item 91)

`demos/deqq.bas` (the text adventure with the split screen) draws its 14 places with `W3D` since 2026-11-13. How it works:

- **One box per place.** Place R (the game's room number 1 .. 14) lies at x = (R mod 5) * 800, y = (R \ 5) * 700 of the plan: its shell is the sector R * 40 (tag R, x 0 .. 600, depth 0 .. 400, 170 high; the outdoor places 300), the furniture are the sectors R * 40 + 1 ... (blocks: `MK_ROOM` + `MK_INNER`; the floor of the block is its top, a negative top is a hole in the floor), in front lies the camera sector R * 40 + 39 (560 deep, 400 high). **Both ceilings are the sky** (`W3D SKY`, a dark night with stars), so the upper wall of the opening is not drawn and the view from the camera (eye 217 above the floor, `pitch` -14, field of view 62 degrees through `W3K_SETPREFS`) is a doll's house with walls 170 high, like the old pictures. Blocks must not touch each other or the walls (`MK_INNER` makes a hole with a portal ring; two holes that share an edge, or a hole that touches the outer wall, leave black gaps in the picture): the generator (`demos/levels/deqq1.bas`) clamps them 2 units off the walls and the data keeps gaps between neighbours.
- **Things are sprites.** `V3_OBJTEX(i)` paints an object's icon with the old `ICON` procedure at 48 x 48 and grabs it as texture 300 + i (the first time it is needed); `V3_THINGS` places the visible objects: floor items at their spot, things in / on / under furniture at the **anchors** of the table `ANCHORDATA` (x, height, depth; `THINGZ` is the height above the floor at that spot, so a thing on a block lies on its top); Deqq (textures 211 / 212 painted by `DEQQFIG` at scale 1), the glorp (213 / 214) and you standing on the chair (215) are things too. `W3D FOG 0` (no darkening by distance).
- **Decals** are flat pictures on a face of the furniture whose picture depends on the state of the game: closed or open doors and drawers (190 dark opening, 191 doors, 192 drawer), the fridge (193 / 194), the oven (195 off, 196 on, 197 hot), the washing machine (198 .. 200), the safe (201), the mirror (202 / 203), the clock, the picture, the skylight, the blueprint, the ladder, front and back doors, the shed door (130 = a shut door); `V3_DECALS(R)` lists them per place (thing numbers 220 ...). The art of game II (the gnome, the sign, the tree, the washing line, the wreck) comes from the library `deqqart` (textures 40 .. 99).
- **Palette.** The pictures use the 15 hues of `w3dkit` (index 16 + hue * 16 + shade); `CC(H,S)` maps the game's 16 hues (`MHT`), so all old drawing code (icons, title, ending) works unchanged; `SETPAL(br)` keeps the day / night tint and rebuilds the color map (`W3D COLORMAP`). A dark cellar (no light) is a black picture with a text, as before.
- **Not in the console host.** `V3_SETUP` (renderer, textures, level) and `DRAWSCENE` do nothing with `DEQQ_HEADLESS`; `deqqtest.sh` is unchanged.
- **Open:** the town as places of game I, Deqq away from home most of the time, more detail.

