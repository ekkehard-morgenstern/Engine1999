# The 3D sector renderer (`W3D`)

*Roadmap item 78, 2026-11-08. Module `w3d.[ch]` (no SDL, no global state), statements and functions `W3D ...` (extension word `XF_W3D`), tests `w3dtest1`
and `vmtest1`, demos `demos/manor.bas`, `demos/bughunt.bas`, library `demos/lib/w3dkit.lib`.*

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
- **Sectors** are numbered 0 .. 4095, walls per sector (the number is the order they were added in, 0, 1, ...), things 0 .. 1023. A point must be in one sector only (no rooms above rooms).
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
| `W3D WALLFLAG s, i, flags` | 1 = masked (the `mid` picture is drawn over the opening of a portal, index 0 see-through; it does not repeat vertically: it sits on the higher floor, `yoff` raises it), 2 = movement can't pass the portal (window, railing), 4 = shots and sight can't pass it (frosted glass). Flags belong to one side: set them on both walls of a window |
| `W3D HEIGHT s, floor, ceil` | moves floor and ceiling (doors: ceiling = floor is closed; lifts) |
| `W3D LIGHT s, light`, `W3D TAG s, value` | brightness; a number of your own per sector (room names, door kinds, the exit) |
| `W3D SKY tex` | the picture of the sky, mapped around the viewer by angle (wraps once around: make it 4 times as wide as the view) |
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

## Functions

`W3D("X")`, `("Y")`, `("Z")` (the eye, absolute), `("ANGLE")` (0 .. 360), `("PITCH")`, `("SECTOR")` (-1 if none), `("FLOORZ")`, `("HIT")` (bits of the last move: 1 a wall pushed the body, 2 it could not move at all, 4 it changed sector),
`("SECTORS")` (highest number + 1), `("LIFT")` (the jump height), `("SECTORAT", x, y)` (-1 outside), `("FLOORAT", x, y)` (-1000000 outside), `("FLOOR", s)`, `("CEIL", s)`, `("LIGHT", s)`, `("TAG", s)`, `("WALLS", s)` (= the number the next wall gets),
`("TX", n)`, `("TY", n)`, `("TZ", n)`, `("TSECTOR", n)`, `("TON", n)` (-1 if the thing exists), `("DIST", n)` (from the viewer), `("BEARING", n)` (degrees from the view direction, positive = to the right, -180 .. 180),
`("TEXW", n)`, `("TEXH", n)` (0 if undefined),
`("SHOOT", range)`: shoots along the view direction (also up and down by the pitch) from the eye: returns the nearest *thing* hit (a thing is hit within 0.4 of its width of its center and between its feet and its top), -1 if there is none; walls and closed doors stop the shot, portals pass it if the opening is there at the height of the shot.
Afterwards `("SHOTKIND")` is 0 (nothing within range), 1 (a wall) or 2 (a thing), `("SHOTDIST")` the distance, `("SHOTX")`, `("SHOTY")` the place.
`("USE", reach)`: the sector behind the portal wall in front of the viewer within reach (-1 if the nearest wall is solid or none): the "open the door" test (door sectors carry a `TAG`).
`("SEES", n)`: -1 if thing n has a line of sight to the viewer (from 70 % of its height to the eye; closed doors, solid walls and frosted glass block it), else 0.
A bad number gives "Illegal argument".

## Doors, lifts, stairs

These are sectors. A **door** is a thin sector between two rooms with its ceiling at its floor (closed: the portal walls show the door picture as their upper wall; nobody fits through) that `W3D HEIGHT` raises. A **lift** is a sector whose floor `W3D HEIGHT` raises; the player stands in it and the eye follows the floor smoothly (the camera eases to the floor of its sector: stairs and lifts do not jump). **Stairs** are a row of
thin sectors, each floor 16 higher than the one before (a step of at most 24 can be walked up). A step that is too high (the pool in `manor.bas`) or a ceiling that is too low blocks the player. `demos/levels/` has a small Python generator for such levels made of rectangles (`mkw3d.py`; the level scripts `manor.py` and `bunker.py` print the `DATA` lines that the demos contain).

## The demos and the library

`demos/lib/w3dkit.lib` (`LOAD LIBRARY "w3dkit"`): `W3K_SETUP` (palette: 15 hues x 16 shades at 16 .. 255 on layers 0 and 3, `W3D RESET`, hides the tile and text layers), `FN W3K_C(hue,shade)`, the texture makers (`W3K_BRICK`, `STONE`, `PLASTER` (64 x 128 with a wainscot), `METAL`, `CARPET`, `TILES`, `PLANKS`, `GRASS`, `DOORTEX`, `WINDOW` and `GRATE` (see-through), `SKY` (256 x 64)),
the sprite makers (`PLANT`, `LAMP`, `BARREL`, `TORCH` (two frames), `MEDKIT`, `AMMOBOX`, `KEYCARD`), `W3K_WALK` (arrows / WASD / R F look / SHIFT run or a game controller: moves and turns by the time passed), `W3K_SHOW` (the flicker free swap), `W3K_HUDBAR`, `W3K_DONE`. `manor.bas` is a walk through a house (doors, stairs, a pool, a window, a cupola, a cellar); `bughunt.bas` is a game (installed as `engine1999-bughunt`).

## The user's 3D settings (`demos/lib/prefs3d.lib`)

A library, `LOAD LIBRARY "prefs3d"`: **invert the X axis** (the right stick turns the other way), **invert the Y axis** (looking up and down), **gamma** (0.6 .. 1.8; it shapes the shades of the palette that `W3K_PALETTE` makes, so it also changes how the light falls off), **field of view** (60 .. 110) and **turning speed** (50 .. 200 %).
`CALL P3_LOAD` reads `local:3dprefs.cfg` (the file of the driver `local:`, `~/.local/share/<program>/`: **each program has its own copy**; a shared file would need a driver feature) and applies the settings; `CALL P3_DIALOG` shows the dialog over the picture of the world
(keys: UP / DOWN choose, LEFT / RIGHT change, RETURN saves, ESC cancels, DELETE defaults; pad: D-pad or left stick, A, B / BACK, X; changes show at once) and the programs open it on **O** (or the pad's **Y**). The programs honor the settings through `w3dkit`: `W3K_WALK` (inversion, turning speed), `W3K_VIEW(x,y,w,h,scale)` instead of `W3D VIEW` (field of view), `W3K_PALETTE` (gamma). The file is five numbers after a `3DPREFS 1` line.
A library can not use the variables of another library ("Variable W3K_INVX is global in the library"), so `prefs3d` keeps `P3_INVX`, `P3_INVY`, `P3_GAMMA`, `P3_FOV`, `P3_TURN` and hands them over with `W3K_SETPREFS`. Settings are 3D renderer *demo* settings: the `W3D` statements themselves know nothing of them.

## The demos as examples of what the engine can do

`manor.bas` (a house to walk through), `bughunt.bas` (a shooter: monsters that chase and shoot, keys, a lift) and **`deqq2.bas`** (Deqq's House II, a stealth adventure). In Deqq's House II everything you touch is a *thing* with a table of properties; the program finds the one in front of you with `W3D("DIST")`, `W3D("BEARING")` and `W3D("SEES")` (E takes or uses it); doors and the chair platform are sectors whose heights change (`W3D HEIGHT`); Deqq walks along a list of way points with `W3D THINGMOVE` (which keeps him inside the walls and on the stairs), sees you when `W3D("SEES", 0)` and the angle between his walking direction and you fit, follows you, loses you when he has not seen you for 3.5 seconds and carries you out when he touches you (a closed door stops him: he can not open one); a *hiding place* is a small dark sector with the tag 50 (he notices you there only from 55 units). The jump (`W3D LIFT`) gets you onto the chair. Its whole logic runs in the console host without a picture (`DEQQ2_HEADLESS=1`, `DEQQ2_PLAY=file`), which is what `deqq2test.sh` does.

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

Hardware rendering (OpenGL / Vulkan; the interface would be `w3d_render()`), room-over-room, sloped sectors, multi-angle sprites, transparent (translucent) walls and things, a map view function, saving worlds to a file (`W3D LOAD` / `SAVE` of a map format), the mouse for looking around, floors that move things standing on them, texture mapping of the `W3D SECTOR` ceiling per sector offset, and things that block the player (do it in the program with `W3D("DIST", n)`).

## Tests

`./w3dtest1` (no display; 91 cases: the picture of a small world pixel by pixel from the four directions, looking up, textures and offsets, masked walls, sky, light and the color map, concave rooms, movement (walls, sliding, doorways, steps, low ceilings, blocking portals), things, shots, sight, use, queries, errors, and 600 random frames that must keep the viewer inside its sector); `./vmtest1` has the 35 statement / function forms (parser, code generator, VM);
the demos were run under Xvfb (screenshots of every room, keys held with XTest, a play through the first fights).
