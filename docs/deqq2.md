# Deqq's House II — implementation guide and reference manual

*2026-11-14. The game is `demos/deqq2.bas` (installed as `engine1999-deqq2`), a real-time first person adventure on the 3D mesh renderer `M3D` of Engine1999. This document explains how it works, from the player's rules down to every procedure and data table, so that anybody can read the source, change the game or build a similar one. It describes the state of the code on the date above; the line numbers given are BASIC line numbers (the listing is numbered by 10 and the numbers carry no meaning, so they are only landmarks). What was seen and heard by a human and what was not is said in "Status" at the end.*

Related documents: [3d-mesh-subsystem.md](3d-mesh-subsystem.md) (the renderer `M3D`: model, statements, hardware renderer, the world document), [world-model.md](world-model.md) (rules of the world documents), [town-map.md](town-map.md) (the town round the house), [json-subsystem.md](json-subsystem.md), [sound.md](sound.md) (instruments, `SFX`, `PLAY`), [library-support.md](library-support.md).

Contents: 1 The game · 2 Files and architecture · 3 Start-up, the main loop, a frame · 4 The world document · 5 Things and the logic · 6 The puzzles · 7 Deqq · 8 The town and the cars · 9 Pictures and textures · 10 Meshes made by the game · 11 Light, water, sound and music · 12 Items that jump out · 13 The renderer, the view and the settings · 14 Saving · 15 The headless test mode and the dev tools · 16 Procedure reference · 17 Variable reference · 18 Extending the game · 19 Status, known issues, lessons

---

## 1 The game

**Story.** Deqq, an alien, crash-landed behind a crooked house in the middle of a town block and lives in it. The eight parts of his ship are hidden in the house, the garden, the shed and the cellar, each behind a small puzzle. You are an investigator (the opening banner: your chief wants a report on Deqq's House): you start at a telephone booth in the far north-east corner of the town and walk round the block to the house's gate. You find the parts, carry them to the wreck at the end of the back yard and build them in. When all eight are built in, the ship lights up, Deqq thanks you and goes home. *Nobody is hurt in this game.*

**Rules.**
- You carry **three places** of things. Small things (key, lantern, screwdriver, crank, jelly, ice) take one place; the big parts (thruster nozzle, hull plate) and the chair take two; the other parts take one (`SL()` in the thing table).
- **Deqq** walks a round through the house; if he sees you, he follows you and carries you out of the front door (or throws you out of the garden, if you are outside). What you carried goes back to where you found it (to where it was last put down); the parts already built in stay. You can hide in seven alcoves (zone tag 50), close doors in his face, run (SHIFT) and listen for his footsteps.
- A car on the ring road brakes and honks if you stand in front of it; it never hurts you.
- The built parts are saved; the title screen offers *Continue*.

**Keys** (`demos/deqq2.bas` header, lines 80–110, and `W3K_WALK` in `demos/lib/w3dkit.lib`):

| Key | Action |
|---|---|
| arrows or W A S D | walk forward / back, turn (arrows left / right turn; A and D slide sideways) |
| SHIFT | run (300 units per second instead of 190) |
| SPACE | jump (up to about 40 units; `W3K_JUMP`) |
| R and F, PgUp / PgDn | look up / down; HOME looks straight |
| right mouse button, drag | turn and look (`W3K_MSENS`) |
| E or RETURN | use or take what is in front of you (the prompt at the bottom says what) |
| G | put down the last thing you carry |
| O | the 3D settings dialog (`prefs3d`) |
| M | music on / off |
| P | pause |
| Q or ESC | quit to the title |
| F3 | a debug line: position, angle, zone number |

Game controller: left stick walks, right stick turns and looks, LT runs, A jumps, X uses / takes, B puts down, Y the settings, START pauses, BACK quits.

**How to start it.** `engine1999` (the window host) and `LOAD "demos/deqq2"`, `RUN`; installed: `engine1999-deqq2`. Hardware (OpenGL) or software drawing is chosen by the settings (§13). With the environment variable `ENGINE1999_NOBANNER` the "Engine1999 proudly presents" opening is skipped.

---

## 2 Files and architecture

| File | Role |
|---|---|
| `demos/deqq2.bas` | the game: logic, the data tables, the meshes of furniture and decorations, the cars, Deqq, the tests' play-back; about 2750 lines |
| `demos/deqq2.mesh.json` | **the world document**: zones (rooms), objects (boxes and quads), things (billboards for the logic), environment, start; the source of the world (edit it directly) |
| `demos/lib/deqqart.lib` | the pictures: `DA_FACADE`, `DA_FENCEBACK`, `DA_TOWN` and the table of sprite pictures `DA_SPRITES` (DATA lines); written by `demos/levels/deqq2art.bas` |
| `demos/lib/w3dkit.lib` | helpers: the palette (15 hues x 16 shades), the procedural textures (`W3K_BRICK` ...), `W3K_WALK`, `W3K_JUMP`, `W3K_SHOW`, `W3K_VIEW`, the renderer choice; with `W3K_MESH` it works on `M3D` |
| `demos/lib/prefs3d.lib` | the settings dialog (`P3_`): inverted axes, gamma, field of view, turning speed, renderer, render size; kept in `local:3dprefs.cfg` |
| `demos/lib/music3d.lib` | the music (`MU_`): the tunes and moods; set 2 belongs to this game |
| `demos/lib/banner3d.lib` | the opening (`B3_M3D`) |
| `demos/lib/input.lib`, `text.lib` | keys and pad (`IN_`), big text (`BIGTEXT`, `TX_BYE`) |
| `demos/levels/deqq2art.bas` | generator of the picture table in `deqqart.lib` |
| `deqq2test.sh` | the automated test (headless, `make check`) |
| `man/engine1999-deqq2.6`, `desktop/engine1999-deqq2.desktop`, `desktop/icons/engine1999-deqq2.svg` | man page, menu entry, icon |

**Layers of the program.**
1. *The world* (renderer side, `M3D`): objects, zones, things, lamps. Built once by `LOADLEVEL` from the JSON document plus meshes the game makes itself.
2. *The logic* (game side): about 200 numbered **things** (`OBJDATA`): the parts, the items, the fixtures you can use, hiding places, decorations; Deqq; the cars; the doors; the timers. The logic knows *where* things are and *what state* they are in, not how they look.
3. *The sync*: `MESHSYNC` (every frame) shows the mesh that belongs to the state of each thing. The things themselves are drawn as one see-through pixel (picture 299), so the billboard renderer draws nothing for them; the meshes are what you see.

The same logic runs without any picture in the console host (`DEQQ2_HEADLESS`, §15), which is how it is tested.

---

## 3 Start-up, the main loop, a frame

### 3.1 Start-up (the main program, lines 12810–13410)

`LABEL MAIN`: `HL = ENV$("DEQQ2_HEADLESS")<>""`, `CALL CONSTS` (the numbers of things: `IDEQQ=0`, `IGLORP=1`, `IKEY=10` ... `PCOIL=2` ... `PANT=9`, `OOVEN=20` ... `OWRECK=32`, `CHAIRSEC=51`).

*Headless* (`HL`): `LOADLEVEL`, `LOADROUTE`, `LOADDESC`, `M3D PLAYER 14,56,46,24`, `RANDOMIZE 7`, `MU_USE(2)`, open the play-back file on channel 4, `NEWGAME(0)`, `PLAYBACK`, `BYE`.

*Window*: `W3K_SETUP` (palette, layers), `W3K_MESH` (use `M3D`, choose the renderer), `MAKETEX` (all pictures), `DA_SPRITES` (sprite table pictures), `LOADLEVEL` (the world), `LOADROUTE`, `LOADDESC`, `M3D PLAYER 14,56,46,24` (capsule radius 14, height 56, eye 46, step 24), `W3K_VIEW(0,0,640,264,2)`, `M3D FOG 4200`, `M3D COLORMAP`, `P3_LOAD` (settings), `BENCH` (only if `DEQQ2_BENCH` names a file), `MU_USE(2)`, the instrument 40 `CARHORN` and the 22 `SFX` definitions (§11.2). Then: `TITLE`; loop: `NEWGAME(TITLERES=2)` (continue or new), the *Investigation* banner for a new game, `PLAYLEVEL`, `ENDING` when the ship is complete, `TITLE` again; at the end `W3K_DONE`, `TX_BYE`.

An error anywhere goes to `LABEL FAILED`: `W3K_DONE` (the graphics layers back), then `Error: ... in line ...` is printed. (A frozen-looking picture after an error was a real problem in the 3D demos; this is the cure.)

### 3.2 `LOADLEVEL` (line 3110)

```
JSON LOAD "deqq2.mesh.json"     ' searched: the program's folder, the current folder, worlds/, demos/, the installed worlds folder
M3D WORLD                       ' builds objects, things, lamps, zones, environment, start from the document
CALL BUILDCARS / BUILDFIX / BUILDDECOR / BUILDWATER   ' the meshes the game makes itself
JSON CLEAR
```

`BUILDFIX` also calls `BUILDCHAIRS` and `BUILDBOOTH`; `BUILDDECOR` calls `BUILDTOWN`.

### 3.3 One frame of play (`PLAYLEVEL`, line 11150)

```
DT = (TI-T0)/60   (capped at 0.1 s)
W3K_WALK                         ' keys / pad: M3D MOVE, M3D TURN (scaled by DT), mouse look
W3K_JUMP(SPACE or pad A)         ' M3D LIFT
E / RETURN / pad X (edge)        -> USEKEY
G / pad B (edge)                 -> DROPLAST
ESC                              -> IN_QUIT
TICK(DT)                         ' one step of the world, see below
every 5th frame: PROMPTUPDATE    ' the "E: ..." prompt
M3D RENDER ; HUD ; (F3 debug line) ; aim cross ; W3K_SHOW
pause (P / START), M (music), O / pad Y (settings dialog)
until IN_QUIT or ENDREQ
```

`W3K_SHOW` shows the finished layer, swaps drawing layers (flicker free), waits for the frame and calls `MU_TICK` (the music loop queue).

### 3.4 `TICK(DT)` (line 10260): one step of the world

Order matters:
1. `GT = GT + DT` (game time in seconds), `PX, PY` = the player's feet.
2. `DOORS(DT)`: the four doors turn toward open (100) or closed (0) at 90 per second.
3. `LANTERNLIGHT`: cellar light levels depend on whether you carry the lantern.
4. `LIGHTFX(DT)`: flicker of the bulb and the lantern flame, the wreck's glow pulse, the television light, the pond animation.
5. `M3D LAMP 0` the lantern (when carried: radius 280, 170 x flicker), `LAMP 1` the wreck's glow, `LAMP 2` a red glow on Deqq while he chases, (`LAMP 3` the TV is set in `LIGHTFX`).
6. `TIMERS`: the oven cools, the ice melts.
7. The washing machine's picture alternates while it runs (`TXN(OWASH) = 70 + (INT(GT*5) AND 1)`).
8. The gate (object 3000) opens again when `GATET` is over.
9. `CARS(DT)`, then `DEQQAI(DT)` if Deqq is on, `MUSICMOOD`, `FLYITEMS(DT)` (items in flight), `MESHSYNC`.

---

## 4 The world document `demos/deqq2.mesh.json`

Format: see [3d-mesh-subsystem.md](3d-mesh-subsystem.md) ("`M3D WORLD`"). It has `environment` (`fog` 4200), `start` (x 1730, y 1570, angle 90, z 0: the phone booth), `zones` (103), `things` (69) and `objects` (281). `name` and `note` fields are comments. The file was converted once from the old sector world document (2026-11-14) and is the source now.

### 4.1 Coordinates and levels

x east, y north, z up, angles in degrees (0 = +x, 90 = +y). The house occupies x 180 .. 840, y 300 .. 730; the town lies round it (ring road at about x -560 .. 1650, y -190 .. 1700). Storeys:

| Level | z range | Notes |
|---|---|---|
| cellar | -80 .. 40 | reached by the hatch in the west garden and 5 steps (zones 34..38) |
| ground floor | 0 .. 112 | kitchen, hall, living room |
| upper floor | 112 .. 224 | bathroom, landing, bedroom (the stairs: 8 steps of 14, zones 13..20) |
| attic | 224 .. 300 | by 8 steep steps (24..31) from the bedroom; the roof is the facade picture |

Floors and ceilings are slabs 1 unit thick (a floor at 111 .. 112 / 112 .. 113), so the room above is not lowered. Walls inside are 8 thick, house walls 8; partitions 10.

### 4.2 Zones (rooms) and their tags

A zone is a named box of space for the game (`M3D("ZONE",x,y,z)` = its id, `M3D("ZONETAG",...)` its tag; the first zone in the list that contains the point wins, so the zones of upper floors, stairs and the chair platform come first). The game uses:

| Tag | Meaning (zone ids) |
|---|---|
| 0 | stairs steps (13..20 hall, 24..31 attic, 34..38 cellar), the niches (60, 61), the chair place 51 |
| 1 / 2 / 3 / 4 | the doors: front 6, kitchen (west) 7, shed 8, cellar hatch 33 |
| 10 .. 16 | outdoors: front garden 0 (10), west garden 1 (11), back yard 2 and back garden 69 (12), crash site 3 (13), east garden 4 (14), north-east corner 5 (15), shed 9 (16) |
| 20 .. 27 | rooms: kitchen 10 (20), hall parts 11, 65, 66, 67 and the doorways 116, 117 (21), living room 12 (22), landing 21 (23), bedroom 22 and 48 (24), bathroom 23 (25), attic 32, 47 (26), cellar 39 (27) |
| 30 / 31 | the ring road (400 .. 417 and others) / the pavement 418 |
| **50** | **hiding places**: alcoves 40 hall, 41 kitchen, 42 living room, 43 bedroom stairs, 44 bathroom, 45 cellar, 46 attic. (The game's header calls them the coat closet, pantry, sofa, wardrobe, shower curtain, barrels and trunk, in the order of the rooms of the house.) |

Only tag 50 is read by the logic (`TAGP = 50`: hidden; `SPOTOK` refuses such spots), and zone ids >= 400 mean "in the town" (`ZONEP >= 400`: Deqq ignores you and stops at the fence). The other tags document the world. Zone 51 (box x 640..680, y 516..556, z 30..260) is the chair platform on which you can stand to reach the crystal (`CHAIRSEC`).

### 4.3 Objects: number ranges

The document's objects keep the numbers of the old sector world; the game adds its own. All are meshes (boxes and quads) with flags (1 masked, 2 translucent, 4 passable, 8 see-through, 16 two sided) and a light level 0 .. 255.

| Object | What |
|---|---|
| 0 .. 48 | the garden areas, rooms, stairs steps, cellar: floor slab, ceiling slab, walls (boxes, with holes at doors and windows), jambs |
| 6, 7, 8, 33 | the **door leaves** (turn about their hinge with `M3D OBJPOS`; the game's `DHX/DHY/DBY/DSG/DSEC`) |
| 50, 52, 53 | tall cupboard, bed, kitchen table (solid furniture in the document); 51 the chair platform (built by the game) |
| 60, 61 | niches in front of the cellar hatch and the shed door |
| 65 .. 67, 116 .. 119 | parts of the hall, doorways between rooms |
| 69, 70 | the back garden, the pond (pond animation: 3011 .. 3013, see §11.4) |
| 80 .. 95 | **window niches**: the little rooms in a wall hole between the wall and the world outside, front / west / east / back, ground / upper / attic |
| 400 .. 417, 426 | the ring road in pieces (corners and stretches; 426 is the detour round the cellar) |
| 418 .. 421 | the pavement in front of the premises (418) and the gate's frame pieces (419 .. 421; the gate leaf is object 3000) |
| 422 .. 425 | the four grass strips round the town |
| 430 .. 433, 441 .. 527 (odd) | the **houses of the town** (a box without its front face plus a quad with the picture that has the door baked in: pictures 260 .. 263 / 270 .. 273 plain, 264 .. 267 / 274 .. 277 with a door; shops 285 .. 288 on larger fronts) |
| 435, 436 | the gas station's kiosk and the post of its price sign |
| 600 | the ground (holes where the cellar lies) |
| 1058 .. 1085 | **the glass panes** of the windows (28 thin boxes, flags 10; §4.4) |
| 1100 .. 1179 | the railing in front of the premises and along the road (flags 1: thin boxes with the railing picture 250, named "pane" in the document) |
| 1688 .. 1705 | invisible rails (flags 9: solid and see-through: the sides of stairs and platforms) |
| 2000 .. 2526 | fences round the premises and the lots (flags 1, the railing picture 250, 56 high; the gas station's is 2434) |
| 3000 | the garden gate (a box of 80 x 80; shown only while it is locked) |
| 3106 .. 3108, 3133 | door frames (3100 + the door's number) |

Objects made by the game at start (all through `M3D OBJECT`, `BOX`, `VERTEX`, `QUAD`):

| Object | What (procedure) |
|---|---|
| 3010 + K (K = 1 .. 3) | the other three pictures of the pond (`BUILDWATER`) |
| 3200 + 3 * slot + color | cars: 6 slots x 3 colors, one shown per slot (`BUILDCARS`, §8) |
| 3500 + T | the *state front* for picture T of a fixture: a quad in front of the fixture shown while its state is T (`BUILDFIX`) |
| 4000 + N | the body of fixture N (kind 1), or the mesh that follows thing N (items, parts, the chairs) |
| 4100 | the telephone booth (`BUILDBOOTH`) |
| 4300 / 4301 | Deqq calm / alerted; 4310 / 4311 the glorp hungry / fed |
| 5000 + N | static decorations: trees (61), plants (90), lamps (91), barrels (92), the sign (63), the washing line (84), sofa (80), shelf (83), the wreck (45: object 5000 + 32 = **5032**) |
| 5300 .. 5307 | the eight hull panels of the wreck, shown as parts are built in |
| 6000 + N | the town's trees (things 404 ..) |
| 6200 + n | the lamp posts (things 700 .. 799; built by `BUILDLAMPS`, called from `TOWNTHINGS` because the lamp things only exist after `PLACEALL`) |
| 6800 .. 6802 | the three gas pumps (things 401 .. 403; *not* 6100 + n: see §19) |

### 4.4 Windows and glass

Every window is a hole in a wall (the wall is built from boxes round the hole) with a **window niche** object (80 .. 95) behind it that makes the hole a small room (so the hole has a floor, ceiling and sides) and **two glass panes** (objects 1058 .. 1085: one on each side of the wall, thin boxes of 2 units in the hole, picture number **11**, flags **10** = translucent + see-through). The panes are solid (you do not walk through a window), but shots and sight pass (flag 8).

**What the glass looks like, and why.** Picture number 11 is the **sky picture** (256 x 64: gradient and clouds; made by `W3K_SKY(11)` in `MAKETEX` and set as the world's sky by `M3D SKY 11`). It began as a mistake: a plain tinted glass was meant (a palette color of that number used flat), but number 11 has a picture, so each translucent pane shows the sky picture, stretched over the pane and mixed half and half with what lies behind it. The result is a glass with clouds in it, seen from both sides, that the owner liked, so it stays. It is not a bug. To get a plain tint instead: give the panes a picture number that has no picture (the palette color of that number is drawn flat), define that palette entry with `GFX PALETTE layer,index,RGB(r,g,b,alpha)` on the layers 0 and 3 (the renderers mix a color with an alpha below 255 at that alpha in translucent objects: [3d-mesh-subsystem.md](3d-mesh-subsystem.md), "palette alpha"), and change the last number of the 28 pane boxes in the JSON. Before 2026-11-14 the panes were never drawn at all (a bug of both renderers: see §19).

### 4.5 Other world facts

- The **fence** round the premises and the lots is the railing picture 250 (iron bars on a brick base, see-through where the bars are) on boxes 56 high; picture 29 (`DA_FENCEBACK`: a brick fence, a line of trees and a night sky) is used by eight boxes as a backdrop wall (so rays that pass a fence do not end in nothing).
- The **gate** (object 3000) in front of the premises closes for 25 seconds after Deqq has thrown you out.
- **Ground**: object 600 with grass picture 10; grass strips, road (asphalt picture 220), pavements.
- **Fog**: `M3D FOG 4200`, so far houses fade.

---

## 5 Things and the logic

### 5.1 The thing table `OBJDATA` (line 13420 on)

Each thing is one DATA record: `N, X, Y, T, W, H, ZO, LT, K, SLT, A, R, SH, LV, NA$`

| Field | Meaning | Array |
|---|---|---|
| N | thing number (0 .. 199) | |
| X, Y | home position | `HX`, `HY` |
| T | the picture number of its state (see §9; the things themselves show picture 299, the state is read by `MESHSYNC`) | `TXN` |
| W, H | width, height of the billboard (also the size of the thing for lookup) | `TWD`, `THT` |
| ZO | height above the floor | `TZO` |
| LT | light level (-1 = leave as is) | `TLT` |
| K | **kind**: 0 decoration, 1 item (taken with E), 2 part (taken with E, may have a condition), 3 fixture (used with E) | `KD` |
| SLT | places it takes in your hands (1 or 2) | `SL` |
| A | the **action number** of a fixture (§6) | `ACT` |
| R | reach (units) within which E works | `RCH` |
| SH | shown at start (-1) or hidden until revealed (0) | |
| LV | level hint for the floor: 0 ground (`TLZ = 1`), 1 upper (114), 2 attic (226), -1 cellar (-60), >= 3 used as given (101 = the cupboard's top) | `TLZ` |
| NA$ | name (shown in the prompt and the messages) | `NMS$` |

**Numbers.** 0 Deqq (`IDEQQ`), 1 the glorp, 2 .. 9 the eight **parts** (`PCOIL` 2 flux coil, `PCOMPASS` 3 star compass, `PCRYSTAL` 4 warp crystal, `PNOZZLE` 5 thruster nozzle, `PFUEL` 6 fuel cell, `PPLATE` 7 hull plate, `PGLYPH` 8 ignition glyph, `PANT` 9 antenna), 10 .. 16 the **items** (key, lantern, screwdriver, crank handle, green jelly, block of ice, chair), 20 .. 41 the **fixtures** (oven 20, fridge 21, sink 22, cupboard 23, fuse box 24, washing machine 25, mirror 26, photo 27, safe 28, well 29, flowerpot 30, toolbox 31, wreck 32, sign 33, gnome 34, blueprint 35, note 36, television 37, telescope 38, star globe 39, tree 40, washing line 41), 61 .. 68 the displayed copies of the parts on the wreck (`SHOWTHING(60+K)` when part K is built in), 100 .. 128 decorations (trees, plants, lamps, barrels, sofa, shelf; no actions), 401 .. 599 the town's gas pumps and trees (from the document), 700 .. 799 the town's lamp posts (`TOWNTHINGS`).

`PLACEALL` (line 640) clears all 200 things (`KD=0`, `M3D THINGOFF`), reads the table, fills the arrays, builds the **list of usable things** `IL()` (`NIL` entries: K > 0) and shows the things that start shown (`SHOWTHING`), then `TOWNTHINGS` creates the lamp posts.

**Hidden things** (SH = 0) are revealed by the puzzles: `REVEAL(N)` shows thing N at its home and lets it jump out of its container (`POPOUT`, §12).

### 5.2 Showing things

- `SHOWTHING(N)`: `M3D THING N, HX, HY, floor + ZO, TXN, TWD, THT` and the light; for things with a mesh (`MD(N)<>0`) the picture is replaced by the blank 299.
- `SHOWAT(N,X,Y)`: the same at another place (`AZ` = the level to look for the floor).
- `TT(N,X,Y,TX,W,H)`: a town thing (lamp post) at the floor of (X,Y).

### 5.3 Using and taking

`USEKEY` (E): `N = TARGET(0)`; if N exists: kinds 1 and 2 → `PICKUP(N)`, else `DOACT(N)`; with no target → `DOORUSE`.

`TARGET(X)` (line 1810) finds the thing to use: of the usable things `IL()`, those that are on (`M3D("TON",N)`), within their reach (`M3D("DIST",N) < RCH(N)`), in front (bearing within 40 degrees, or closer than 30), and seen (`M3D("SEES",N)`: nothing in the line of sight). The nearest wins; but an item that has just jumped out of a box (`FLG(N)<>0`) wins over a fixture that is not more than 80 units nearer. The chair while you stand on the chair place (zone 51) is skipped.

`PICKUP(N)`: the crystal needs you to stand on the chair place (`ZONEP = CHAIRSEC`: "out of reach"); the nozzle needs the glorp fed. Otherwise `TAKE(N)`.

`TAKE(N)`: refuses if the places used (`SLOTSUSED`) plus `SL(N)` exceed 3; puts N into the lowest free slot `INV(0..2)`, hides the thing (`M3D THINGOFF`), message and sound 1.

`DROPLAST` (G): puts the last carried thing 34 units in front of you (or at your feet if that is outside a room or in an alcove), through `FINDSPOT` / `LAUNCH` so that it lands on a free spot (§12), and makes that its new home.

`USEUP(N)`: removes N from `INV` (used up or built in); the thing stays hidden. `HAS(N)`: carried? (-1 / 0).

`DOORUSE`: with nothing in front: `M3D("USE",70)` finds the object in front (reach 70); if it is a door leaf: the front door is "stuck open for years"; an open door is closed; a locked door needs the spare key (`IKEY`); then it is unlocked (`DLK`) and opened (`DOP`).

`PROMPTUPDATE` makes `PROMPT$`: `E: TAKE THE <name>` for kinds 1 / 2, `E: <name>` for fixtures, `E: OPEN/CLOSE THE DOOR` for the three doors.

---

## 6 The puzzles

All state is in global flags (§17), reset by `NEWGAME`. The eight parts and their puzzles (`DOACT`, line 4190, dispatches on `ACT(N)` = A; `INSTALL` builds in):

| # | Part (place) | Puzzle |
|---|---|---|
| 2 | **flux coil** (inside the oven, kitchen) | Use the oven (A = 1): the knob is turned off (`OVENT = GT`, sound 19), the oven cools for 30 seconds (`TIMERS`: `OVENCOOL`, picture 65), then it opens (picture 66) and the coil jumps out. Before it has cooled: "too hot". |
| 3 | **star compass** (frozen in the ice) | Open the fridge (A = 2; picture 68): green jelly and a block of ice appear. Carry the ice to the sink (A = 3) while you have it: `USEUP(IICE)`, `MELTT = GT + 5`; five seconds later the compass lies in the sink. |
| 4 | **warp crystal** (on top of the tall cupboard, living room) | Find the chair (item 16, kitchen). Use the cupboard (A = 4) while carrying the chair: the chair is placed under it (object 51 shown, zone 51), you are moved to (660, 490) facing the cupboard. Jump onto the chair (zone 51); `PICKUP(PCRYSTAL)` works only while `ZONEP = CHAIRSEC`. |
| 5 | **thruster nozzle** (bedroom, under the glorp) | The glorp (thing 1, A = 6) sits on it and wants green jelly. Use it with the jelly (from the fridge): `FED = -1`, picture 44, the glorp moves 60 units away from the nozzle (`HX(IGLORP) - 60`), the nozzle can be taken (two places). |
| 6 | **fuel cell** (in the washing machine, cellar) | The cellar is dark: take the **lantern** (item 11, front hall; `M3D LAMP 0` while carried; the cellar's light levels follow `LANTERNLIGHT` / `LIGHTFX`). Use the fuse box (A = 8) with the lantern: `POWEROFF`, the machine stops (picture 72). Then use the machine (A = 9): the door opens (pictures 70 / 71 alternate while it runs, 72 stopped) and the cell appears. |
| 7 | **hull plate** (the "mirror" on the bathroom wall) | Four screws: take the **screwdriver** from the **toolbox** (shed, A = 16: also the crank). Use the mirror (A = 11) with the screwdriver four times (`SCREWS`); the fourth shows the plate (picture 77). |
| 8 | **ignition glyph** (in the safe, attic) | The safe (A = 13) has a 3 digit code: look at the **photo** (A = 12, living room) and turn it over: `7 3 1`. The dial (`DIAL(X)`): UP / DOWN change the digit, LEFT / RIGHT choose, RETURN tries, ESC leaves. (Headless: `dial 731`.) The safe opens (picture 75). |
| 9 | **antenna** (in the bucket of the well, north-east garden) | The windlass has no crank. With the crank from the toolbox use the well (A = 14): the bucket comes up with the antenna. |

**Doors and keys.** The front door is open for good (`DOP(1) = -1`, `DLK(1) = 0`). The kitchen door (west, 2), the shed door (3) and the cellar hatch (4) are locked (`DLK`); the **spare key** (item 10) lies under the crooked **flowerpot** in the back yard (A = 15): lift it (the pot stands in the back yard at (-300, 900)). The sign in the front garden hints at it: "SMALLER: SPARE KEY? LOOK WHERE THE PLANTS SIT CROOKED."

**The wreck** (fixture 32, A = 17): use it to `INSTALL`: every carried part (numbers `PCOIL` .. `PANT`) is removed from your hands, `BUILT(K)` and `NBUILT` rise, thing `60 + K` (the displayed copy) appears, the panels of the hull (objects 5300 + K) appear one by one (`SYNCWRECK`), the game is saved (`SAVEGAME`). With `NBUILT = 8`, `ENDREQ` is set and the main loop calls `ENDING`.

**Reading things** (A = 20): the sign, gnome, blueprint, note, television, telescope, star globe, tree, washing line give a description (`DESCDATA`, line 14190 on; `LOADDESC`).

**The ending** (`ENDING`, line 11620): music fanfare (mood 6), banner "THE SHIP HUMS", the wreck rises 90 steps (`M3D THINGPOS 32 ...`), Deqq's thanks, the time and the number of times caught, the saved game is deleted.

---

## 7 Deqq

### 7.1 The thing and the state

Deqq is thing 0 (`IDEQQ`), a billboard hidden behind the blank picture; his mesh is object 4300 (calm) or 4301 (alerted: arms raised, red eyes), turned to his heading `DQANG` (`SYNCSPECIAL(4, 0)`). He walks with `M3D THINGMOVE` (collision like the player's, radius 0.4 x width, steps, floors).

State variables: `DQON` (on), `DQS` (0 walking to a way point, 1 pausing at one, 2 **chasing** you), `DQWP` (the way point), `DQPT` (pause end), `DQAWAY` (-1 on the walk through the town, 0 at home), `DQLEAVE` (-1 when he should leave after his time at home), `DQHT` (the time he stays at home), `DQCD` (cooldown before he can see you: after a catch and at the start), `DQLOST` (seconds since he saw you), `DQANG` (heading, radians), `DQPIC` (picture: 40 calm, 42 alerted: used to choose the mesh), `DQFT` (next footstep time), `DQHEARD` (next "heavy footsteps" message), `DQSTUCK`.

### 7.2 Routes

- `ROUTEDATA` (line 14300): the **round of the house**, 49 way points 0 .. 48: `x, y, pause` (seconds, 0 = walk on). From the front garden (0, pause 3) through the porch and hall to the kitchen (7: pause 2), back through the hall to the living room (18: pause 2), up the hall stairs (way points 22 .. 26: x 480, y 365 .. 635) to the landing and the bedroom (30: pause 2), the bathroom (37: pause 2), down again and out into the front garden (45, 46), east of it (47: pause 1) and west of it (48: pause 2), then round again. (The attic and the cellar are not on his round.)
- `TOWNDATA` (line 14810): the **walk through the town**, way points 50 ..  `TWEND`: out of the gate, once round the block (south road east, east road north, north road west, west side, back along the south road), back in at the gate. `LOADROUTE` reads both into `WPX / WPY / WPP`.
- `NEARWP(X,Y)`: the nearest way point of the current set (home 0..48 or town 50..`TWEND`, by `DQAWAY`).
- `NEXTWP(I)`: the next one. At home it wraps from 48 to 0; at way point 46 with `DQLEAVE` set the town walk begins (`DQAWAY = -1`, way point 50). In the town, when you are near the gate (way point 71 and `PY > -30`): "THE GATE CREAKS: DEQQ IS COMING HOME!" (sound 3). At the end of the town walk (`I >= TWEND`): `DQAWAY = 0`, he stays at home for 60 .. 120 seconds (`DQHT = GT + 60 + INT(RND(1)*60)`), then leaves again.

At a new game Deqq starts away, in the town (`DQAWAY = -1`, `DQWP = 60`), `DQCD = 3` seconds.

### 7.3 `DEQQAI(DT)` (line 7700), each frame

1. **Sight** (only when `GT >= DQCD`): the limit is 430 units (55 while you hide: `TAGP = 50`). He sees you if you are within the limit, and either closer than 95 or within 0.95 radians (about 54 degrees) of his heading, and `M3D("SEES", 0)` (line of sight). He never sees you while he is in the street (`DYP < 0`) or you are in the town (`ZONEP >= 400`).
2. He leaves when his time at home is over (`DQLEAVE`).
3. **Chasing** (`DQS = 2`): if you are in the town: he stops at the fence and shakes his fist (`DQS = 0`). While he sees you `DQLOST = 0`, else it counts up; after 3.5 s he loses your track ("MUTTERS ... GOES BACK TO HIS ROUND") and goes to the nearest way point. He moves toward you at 108 per second (you walk 190, run 300). Within 46 units and seen: `CAUGHT`.
4. **Not chasing:** if he sees you: `DQS = 2`, sound 12, "DEQQ HAS SEEN YOU! RUN, SHUT A DOOR, OR HIDE!", alert picture. Else at a pause (`DQS = 1`) he waits until `DQPT`; else he walks toward the way point at 62 per second (the footsteps alternate pictures `F = 40 + (INT(GT*4) AND 1)`); within 22 units of it he pauses (`WPP > 0`) or takes the next. If he is blocked 1.5 seconds (a closed door: `M3D("HIT") AND 2`) he gives up that way point. **A closed door stops him**; doors you open stay open until you close them.
5. **Footsteps:** unless pausing, within 420 units: sound 2 every 0.55 s; at most every 30 seconds the message "HEAVY FOOTSTEPS. DEQQ IS SOMEWHERE NEAR." when he is out of sight.

### 7.4 `CAUGHT` (line 7130)

`OUT` = you are outside the house (not inside x 180 .. 840, y 300 .. 730) and not in the cellar area. Counts `CAUGHTN`, music sting (mood 5, then calm), sound 4, banner ("HE MARCHES YOU TO THE GATE AND THROWS YOU OUT OF HIS GARDEN. THE GATE SLAMS SHUT." or "HE CARRIES YOU OUT OF THE HOUSE AND PUTS YOU DOWN IN THE GARDEN."). Everything you carried goes back (`SHOWTHING`: to the thing's home, which is where it was last put down). You are put at (480, -35) facing 270 (outside: the gate shuts for 25 s, `GATET`) or (480, 150) facing 90 (in the garden); Deqq is put at (480, 50) (nearest way point) or at way point 4; `DQCD = GT + 8` (eight seconds of grace).

---

## 8 The town and the cars

The town is described in [town-map.md](town-map.md): the premises with the fence and gate in the south, the ring road at x -560 .. 1650 / y -190 .. 1700 (zone tag 30), four grass strips with 43 houses and shops, the gas station (kiosk 435, three pumps, price sign 441 on post 436), trees, lamp posts (every 280 units along both edges), the phone booth at (1730, 1660). You start at (1730, 1570).

**Cars** (`MAKECARS`, `CARMESH`, `BUILDCARS`, `CARPLACE`, `CARS`):
- Nine car objects are built (6 slots x 3 body colors), each: a lower body box, bumpers, two headlights and two tail lights (boxes), a tapering cabin of glass quads with a body-colored roof, four eight-sided wheels (`CARWHEEL`: prism quads with a tyre picture and a hub-cap picture 238 on the sides). Local coordinates: x forward, y left, z up; flags 12 (see-through + passable: harmless).
- A new car every 6 .. 20 seconds in a free slot (`CARNEXT`), at most six at a time; each drives the loop `CARLOOP` = 9380 units (51 s) once and disappears. `CARPLACE(S)` maps the driven distance S to the place and heading on eight road stretches (south road eastbound at y -190, the east road northbound at x 1650, the north road westbound at y 1700, the west road southbound at x -560, then the side loop at x -1150 / y 780 and 245).
- A car with you in front of it on the road (within 200 forward and 70 sideways, and you are on the road) brakes to 0 and honks (`CARHON`, sound 11, "A CAR HONKS: GET OUT OF THE ROAD!"); it speeds up at 240 per second and brakes at 480 per second to a top speed of 150. Within 380 units of you it plays the passing sound (10) every 9 seconds.

---

## 9 Pictures and textures

All pictures are drawn with `GFX` statements on the hidden layer and grabbed (`W3K_GRAB(N,w,h)` → `M3D TEXTURE N,a$`, and for the sector renderer `W3D TEXTURE`). The **palette** is `w3dkit`'s: entries 16 .. 255 are 15 hues x 16 shades, `FN CI(H,S) = 16 + H*16 + S` (hues: 0 gray, 1 red, 2 orange, 3 yellow, 4 lime, 5 green, 6 teal, 7 cyan, 8 azure, 9 blue, 10 purple, 11 magenta, 12 brown, 13 sand, 14 pink); entries 1 .. 15 are left as the system has them. A picture number that has no picture draws the palette entry of that number as a flat color; palette index 0 is see-through in masked and translucent objects.

| Pictures | What (made by) |
|---|---|
| 1 .. 26 | room and garden surfaces, `MAKETEX` (line 2720): 1 brick, 2 .. 5 and 7 plaster in colors, 6, 13, 14, 17 tiles, 8 and 19 stone, 9, 18, 25, 26 planks, 10 **grass**, **11 sky** (see §4.4), 12 and 20 doors, 15 and 16 carpet, 23 plaster, 24 stone (the numbers 21 and 22 are not used; picture 0 has no picture and index 0 draws nothing: the invisible rails use it) |
| 27, 28, 29 | house front with roof (128 x 300), the same with a door (64 x 300), the fence backdrop with a night sky (`DA_FACADE`, `DA_FENCEBACK`) |
| 40 .. 99 | the **sprite table** (`DA_SPRITES`, `ARTDATA`): Deqq and the glorp (40 .. 44), the parts (46 .. 53), the items (54 .. 59), the **state pictures of the fixtures** (oven 64 closed .. 66 open, fridge 67 / 68, sink 69, washing machine 70 .. 72, fuse box 73, safe 74 / 75, mirror 76 / 77, photo 78, toolbox 79, television 82, bookshelf 83, ...), plants 90 .. 92 etc. |
| 90 .. 99 | decorations (as above) |
| 201 .. 204 | the four phases of the pond's ripples (`MAKEWATER`) |
| 210 .. 214 | painted house fronts and a hedge (`MAKETOWN`: `TOWNFACADE`, `TOWNHEDGE`) |
| 220 | asphalt; 250 the railing (64 x 60 with see-through bars); 251 brick |
| 230 .. 238 | car pictures: bodies 230 .. 232, glass 233, tyre 234, headlight 235, tail light 236, bumper 237, wheel cap 238 (`CARTEX`, 16 x 16 flat colors) |
| 260 .. 277, 285 .. 290 | town houses, shops, the kiosk (289) and price sign (290) (`DA_TOWN`) |
| 299 | the blank picture (one see-through pixel): what a thing's billboard shows, so only the mesh is seen |
| 300 .. 313 | furniture colors (white enamel, steel, dark wood, dark metal, red cushion, linen, the cupboard's front 306 (80 x 58), water, clay, leaves, yellow, skin, blue cloth, red cloth; `MAKEFURN`) |
| 314 .. 338 | flat colors for the decoration meshes (`MAKEMTEX`) |

A **state picture** T of a fixture is the front of the fixture in one state; the thing's `TXN(N)` says which T is current; see §10.

---

## 10 Meshes made by the game

### 10.1 The fixtures (`BUILDFIX`, table `FIXDATA` at line 37565)

Each record (read by `BUILDFIX`): `N, kind, face, C, BP, PW, PH, D, Z0, BT, LZ, then three pairs T,P`:

- kind **1**: a box with a front picture; kind **0**: a flat thing on a surface (mirror, photo); kind **2**: a special mesh (tv, sink, well, gnome, pot, telescope, globe: each has its own code in `FIXSPECIAL`); kind **3**: a flat thing on a table top (the note).
- `face` 0 .. 3: the front looks east / north / west / south (the object's yaw = face * 90); `C` the middle along the wall, `BP` the wall's coordinate, `PW`, `PH` the picture's width and height (one texel per unit), `D` the depth, `Z0` the height above the floor, `BT` the picture of the sides, `LZ` the floor level for `M3D("FLOORAT")`.
- The three pairs `T, P`: the thing's state picture T (the value of `TXN(N)`) and the picture P used for that state's front. For each T an object 3500 + T is built (a quad 0.2 units in front of the body with picture P), hidden; `VOK(T)` remembers it exists.

`FIXORIGIN` computes the object's origin from face, C, BP and D. `FIXBIND(N,K)` registers thing N in the list `MB()` for `MESHSYNC` with kind K: 0 state fronts, 1 the mesh follows the thing, 4 Deqq, 5 the glorp, -1 static special.

### 10.2 `MESHSYNC` (line 37405), every frame

For each registered thing: if the thing is on, make its billboard blank (`M3D THINGTEX N,299`); kind 1 (items, parts): move object 4000 + N to the thing (when it moved) and show it; kinds 4 and 5 (Deqq, glorp): `SYNCSPECIAL`; other: show the front 3500 + `TXN(N)` and hide the previous one. If the thing is off, hide its mesh. Finally `SYNCWRECK` (panels). So *the thing's state alone decides what you see*.

### 10.3 The helpers

`PICQ` (a picture on a vertical plane), `TOPQ` (a picture lying on a horizontal surface), `LATHE` + `RING(J,R,Z)` (a solid of revolution with 8 sides: `LRR / LRZ` hold the radii and heights), `TUBE` (a square bar between two points: the telescope, the wreck's ribs), `CHAIRMESH(O,S)` (the chair at scale S), `MKTREE`, `MKLAMP`, `MKPLANT`, `MKBARREL`, `MKPUMP`, `MKSIGN`, `MKLINE` (washing line), `MKSOFA`, `MKSHELF`, `MKWRECK` (keel and nine ribs), `MKPANEL`, `MKPART(O,K)` (the eight parts), `MKITEM(O,K)` (the six items), `MKDEQQ(O,ALERT)`, `MKGLORP(O,FED)`, `PUTMESH(O,X,Y,LZ,ZO)` (place a mesh on the floor at X, Y).

`BUILDDECOR` (line 39820) reads `OBJDATA` again: by the picture T of a thing (61 tree, 90 plant, 91 lamp, 92 barrel, 63 sign, 84 washing line, 80 sofa, 83 shelf, 45 wreck, 46 .. 53 parts, 54 .. 59 items, 43 glorp) it builds the decoration object 5000 + N or the following mesh 4000 + N, and sets `MD(N)=1` (the thing shows the blank picture). Then Deqq and the glorp, then `BUILDTOWN`.

---

## 11 Light, water, sound and music

### 11.1 Light

Each object has a light level (0 .. 255, scaled to 32 levels of the color map); `M3D OBJLIGHT room,level` darkens or lights a whole room. Lamps (`M3D LAMP n,x,y,z,radius,amount`, up to 8, brightening only): 0 the lantern you carry, 1 the wreck's glow (radius 320, `30 + NBUILT * 18`, pulsing: faster at 8 parts), 2 Deqq's red glow while he chases (radius 220, 120), 3 the television in the living room (370, 792, 45, radius 120, flickering blue).

`LIGHTFX(DT)` (line 40410): `LFL` the flicker of the lantern flame, `LFK` the cellar bulb that flickers and now and then goes out for a moment (`LFU`; 0.3 of the light), `LFW` the wreck's pulse. In the cellar (`M3D("Z") < -4`) the light of the cellar rooms 34 .. 38 (stairs), 39 (cellar), 45 (its alcove) is set from these (with the lantern 170 / 185 level, without 90 / 45); `LANTERNLIGHT` sets the base levels when you take or drop the lantern.

### 11.2 Sound effects

`SOUND_(N)` = `SFX N` unless headless. Defined in the main program (lines 13155 – 13261); the instrument 40 `CARHORN` is a program-defined instrument (two saws a major third apart + a sub square; [sound.md](sound.md)). Uses:

| SFX | Where |
|---|---|
| 1 | a thing taken | 
| 2 | Deqq's footsteps | 
| 3 | the gate creaks (Deqq coming home) |
| 4 | caught |
| 5 | something opens / found (oven, fridge, washing machine, safe, plate, key, toolbox) |
| 6 | refused (too high, too hot, dark, wrong code) |
| 7 | the glorp eats |
| 8 | the oven cooled / the ice melted |
| 9 | the tap |
| 10 | a car passes |
| 11 | a car's horn (two honks) |
| 12 | Deqq has seen you |
| 13 / 14 | door opens / closes |
| 15 | parts built in |
| 16 | the fuse |
| 17 | a screw turns |
| 18 | the windlass |
| 19 | the oven's knob |
| 20 | the chair is put under the cupboard |
| 21 / 22 | an item lands / jumps out (§12) |

### 11.3 Music

`music3d.lib`, tune set 2 (`MU_USE(2)`): light and bouncy in C major; the danger tune is metal. Moods set by `MUSICMOOD` (every frame) and the events: 1 title, 2 calm (during play), 3 uneasy (Deqq within 380 units: the calm tune with a heartbeat), 4 danger (while he chases and four seconds after: `MUT = GT + 4`), 5 the sting when caught (then calm: `MU_AF = 2`), 6 the fanfare of the ending. `W3K_SHOW` calls `MU_TICK` so loops are queued without gaps; M toggles the music; pause silences and restarts it.

### 11.4 Water

The pond (object 70, picture 201 and the other three phases as the objects 3011 .. 3013 with pictures 202 .. 204) shows one phase at a time, advanced every 0.22 s (`LIGHTFX`: `WT`, `WPH`).

---

## 12 Items that jump out

Added 2026-11-14 (the owner: found items should jump out of their container "in a tight upward arc, so they land in front of the player on a free space"):

- `POPOUT(N)` (called by `REVEAL`): `FINDSPOT(125)` then `LAUNCH`.
- `DROPLAST` uses `FINDSPOT(60)`.
- `FINDSPOT(D0)` looks for a spot in front of you: first within 36 degrees of the view direction (so that `USE` finds it: it looks 40 degrees to each side), then all round the front; distances start at `D0` (the view shows the floor only beyond about 114 units) and grow by 12, five steps; angles by 18 or 22 degrees. The answer is `LOKF` and `LX, LY`.
- `SPOTOK(X,Y,ZR)`: a spot is free when it is in a room zone and not an alcove, the floor there is at the player's level (within 4), the floor is flat all round (a circle of 10 units: floors within 3), nothing stands on it up to 100 high, there are 34 units of ceiling, nothing blocks the line from the eye to it (nine sample points), and no other item lies within 20 units.
- `LAUNCH(N,OX,OY,OZ,X,Y)`: from the origin to the target in 0.45 s along a parabola (the extra height `136 * t * (1 - t)`), the thing's home becomes the target (`HX`, `HY`, `TLZ`, `FLG(N) = -1`: a "fresh" item that `TARGET` prefers), its reach is at least 200; `FLYITEMS(DT)` moves up to 8 flying things each frame (arrays `FLN`, `FLT`, `FOX..FTZ`), sound 22 at the start and 21 at the landing.
- Headless runs keep the items in place unless `popout 1` is given (`POPT`).

---

## 13 The renderer, the view and the settings

`W3K_SETUP` + `W3K_MESH`: the game uses `M3D`. `W3K_MESH` chooses the **hardware renderer** (OpenGL 2, true color) when `M3D("HWAVAIL")` and the settings allow it, else the software renderer; both draw the same scene. The picture is `M3D VIEW 0,0,640,264,scale,fov`: `W3K_VIEW(0,0,640,264,2)` is the program's request (320 x 132, every pixel twice) and the settings (`P3_SC`) override it: 640 x 264 with the hardware renderer by default, 320 x 132 with software. The text layer is hidden, the game draws its HUD on the graphics layer: the message strip (top), the prompt (`PROMPT$`, row 246), the aim cross, the bar at the bottom (y 264 .. 299: three slots for what you carry, eight squares for the parts, "DEQQ SEES YOU!" / "HIDDEN").

`M3D FOG 4200`, `M3D SKY 11` (the sky picture, §4.4), `M3D PLAYER 14,56,46,24`.

The **settings dialog** (O or pad Y; `P3_DIALOG`): invert X / Y of the right stick, gamma (0.6 .. 1.8), field of view (60 .. 110), turning speed (50 .. 200 %), renderer (when hardware is available), render size; RETURN saves to `local:3dprefs.cfg` (`~/.local/share/<program>/`), ESC cancels. The defaults are those of `prefs3d` (field of view 90); `P3_LOAD` applies the file if there is one.

`DEQQ2_BENCH=file` runs a speed benchmark (`m3dbench.sh`): 120 frames turning in place at four places per renderer and render size.

---

## 14 Saving

`SAVEGAME` writes `local:deqq2.sav` (the folder `~/.local/share/deqq2/`): the line `DEQQ2 1`, the mask of built parts (bit K-1 for part K), the game time in whole seconds, the number of catches. It is written whenever parts are built in. `LOADSAVE` reads it (`SAVEDMASK`, `SAVEDT`, `SAVEDC`); the title screen shows *C: continue* when the mask is not 0; `NEWGAME(-1)` rebuilds the state: the parts' flags, the displayed copies on the wreck, time and catches. Nothing else is saved: the puzzles start again, the things are at their start places. `ENDING` deletes the file. (The format has a version line; a different first line is ignored.)

---

## 15 The headless test mode and the dev tools

**Headless.** `DEQQ2_HEADLESS=1` with `engine1999c`: no graphics (`HL` is true: `SAY` prints `> text`, `BANNER` prints `# title`), all logic runs, including `M3D` collision, `SEES`, `THINGMOVE` (the console host runs the world and draws nothing). `DEQQ2_PLAY=file` is a command file (one command per line; `#` lines of the output are test output):

| Command | Does |
|---|---|
| `step n` | n ticks of 1/60 s |
| `goto x y angle [z]` | put the player (z picks the level) |
| `walk x y` | walk there with real collision (turn and move, up to 1500 steps) |
| `jump x y` | the same with jump physics |
| `face x y` | turn toward |
| `use`, `drop`, `give n` | E, G; `give` puts thing n into your hands |
| `dial 731` | the answer for the safe's dial |
| `deqq x y [z]`, `deqqoff`, `deqqon`, `deqqaway [wp]`, `deqqleave` | put Deqq somewhere or switch him |
| `state` | print the game state |
| `say text` | print a line |
| `continue`, `new` | start a game from the save / a new one |
| `popout 0/1` | items jump out in the headless run |
| `shot name` | a screenshot (window only) |
| `quit` | end |

`deqq2test.sh` (in `make check`; uses a temporary `HOME` so a saved game is not touched) plays: 1 the whole game without Deqq (all eight parts, locked doors, the carrying limit, the ending); 2 the carrying limit; 3 the saved game and *continue*; 4 Deqq's round, catching, losing track, hiding; 5 the town (thrown out of the garden, the gate locked, his walk through the town and home again); 6 the town is open to the player (out through the gate, over the pavement, across the road, along the strip into the east fields); 7 found items jumping out to a free spot (`popout 1`). Exit status = the number of failures.

**Screenshots of the real game.** In the window host the statement `SCREENSHOT "file.bmp"` and the keys Print / Ctrl+Shift+P write a BMP of the composed layers. A convenient way to look at places without playing: a copy of the game with a few lines added to `PROC BENCH` that read camera places (x, y, z, yaw, pitch) from the file `DEQQ2_BENCH` names and call `M3D CAMERA ...`, `M3D RENDER` (twice), `WAIT 10`, `SCREENSHOT`. Typed into a window under Xvfb (`xtype.c`). **Caveat:** `JSON LOAD` searches the program's folder, the current folder, `worlds/`, `demos/` and then the installed `worlds` folder: a copy of the program in another folder takes the *installed* world document unless a copy of `deqq2.mesh.json` is beside the program.

---

## 16 Procedure reference

(`LINE` = the BASIC line number of its header; everything is global unless the procedure says LOCAL.)

**Helpers.** `CI(H,S)` palette index of hue / shade (280). `RI(M)` random integer 0 .. M-1 (310). `ADIFF(A,B)` angle difference in radians 0 .. pi (350). `SAY(T$)` message of at most two rows, or a printed line when headless (440). `SOUND_(N)` `SFX N` unless headless (600). `ZONEP` / `TAGP` the zone / tag at the feet (632 / 635). `BITS(M)` number of bits set (2570).

**Things and carrying.** `PLACEALL` read the thing table, clear, show (640). `TT`, `TOWNTHINGS` the town's lamp post things (890 / 920). `SHOWTHING`, `SHOWAT`, `REVEAL` (1070 / 1120 / 1190). `SLOTSUSED`, `HAS`, `TAKE`, `USEUP`, `DROPLAST` (1220 .. 1610). `TARGET` the thing in front (1810). `DIAL` the safe's dial (2010).

**Set-up.** `CONSTS` (2660). `MAKETEX` all pictures (2720). `LOADLEVEL`, `LOADROUTE`, `LOADDESC` (3110 / 3350 / 3550). `NEWGAME(CN)` a new or continued game (10440).

**Doors, timers.** `DOORS(DT)` (3660), `LANTERNLIGHT` (3750), `TIMERS` (3900).

**Puzzles.** `PICKUP(N)` (4060), `DOACT(N)` (4190), `DOORUSE` (6000), `USEKEY` (6310), `INSTALL` (6730).

**Saving.** `SAVEGAME` (6410), `LOADSAVE` (6550).

**Deqq.** `DQMOVE(DX,DY,SP,TF)` walk with `M3D THINGMOVE` and set heading and picture (6980), `NEARWP`, `NEXTWP`, `CAUGHT`, `DEQQAI` (7030 / 7550 / 7130 / 7700).

**Screen.** `CTEXT` centered text, `BANNER` the story banners, `HUD`, `TITLEBG`, `TITLE`, `ENDING` (8450 / 8490 / 8670 / 9060 / 9750 / 11620).

**Play.** `MUSICMOOD`, `TICK`, `PROMPTUPDATE`, `PLAYLEVEL` (10150 / 10260 / 10960 / 11150).

**Test play-back.** `SPLIT`, `WALKTO`, `PLAYBACK`, `JUMPTO` (11830 / 12040 / 12180 / 38000).

**Pictures.** `MAKEWATER` (33170), `TOWNWINDOW`, `TOWNFACADE`, `TOWNHEDGE`, `TOWNASPHALT`, `TOWNRAIL`, `MAKETOWN` (33340 .. 34250), `CARTEX`, `CARHUB`, `MAKECARS` (34382 .. 34430), `MAKEFURN` (36030), `MAKEMTEX` (38525).

**Cars.** `CARWHEEL`, `CARMESH`, `BUILDCARS`, `CARPLACE`, `CARS` (34534 .. 35270).

**Furniture and fixtures.** `PICQ`, `TOPQ`, `LATHE`, `RING`, `CHAIRMESH` (36190 .. 36425), `FIXORIGIN`, `BUILDFIX`, `FIXBIND`, `FIXSPECIAL`, `FIXNOTE`, `TUBE`, `BUILDCHAIRS`, `MESHSYNC` (36530 .. 37405), `BUILDBOOTH` (38105), `BENCH` (38210).

**Decorations.** `MKTREE` .. `MKITEM`, `MKDEQQ`, `MKGLORP` (38660 .. 39740), `PUTMESH`, `BUILDDECOR`, `BUILDTOWN`, `SYNCSPECIAL`, `SYNCWRECK` (39805 .. 40300).

**Effects.** `LIGHTFX`, `BUILDWATER` (40410 / 40595).

**Items that jump.** `SPOTOK`, `FINDSPOT`, `LAUNCH`, `POPOUT`, `FLYITEMS` (40715 .. 41025).

---

## 17 Variable reference (globals)

*Player and world:* `PX, PY` the player's feet (set each tick); `GT` game time in seconds; `AZ` the level to look for a floor in `SHOWAT`; `HL` headless; `CHAIRSEC = 51`; `LV`-derived `TLZ()`.
*Things (arrays of 200):* `KD, SL, HX, HY, TXN, TWD, THT, TZO, TLZ, TLT, ACT, RCH, NMS$, MD` (has a mesh: the billboard is blank), `FLG` (fresh item), `IL()`/`NIL` the usable list, `DESC$`, `PARTNO`.
*Carrying and progress:* `INV(0..2)` (-1 empty), `BUILT(1..8)`, `NBUILT`, `ENDREQ`, `QUITREQ`, `CAUGHTN`.
*Puzzle flags:* `OVENT, OVENCOOL, OVENOPEN, FRIDGEOP, MELTT, CHAIRON, FED, SCREWS, PLATEOUT, SAFEOP, CRANKED, PHOTOSEEN, POWEROFF, WASHOPEN, KEYTAKEN, TBOXOP, LANTON, DIALIN$, DIALMSG`.
*Doors (index 1 .. 4: front, kitchen west, shed, cellar hatch):* `DHX, DHY` hinge, `DBY` closed yaw, `DSG` turning direction, `DSEC` object, `DOP` open, `DLK` locked, `DCUR` the current opening 0 .. 100.
*Deqq:* see §7.1; routes `WPX, WPY, WPP`, `NWP`, `TWEND`.
*Cars:* `CARON, CARD, CARV, CARCOL, CARHON, CARSND, CARNEXT, CARSP, NCARS, CX, CY, CH`.
*Meshes:* `MB, MBK, NMB` (registered things), `MCUR, MPX, MPY` (current state and position of each mesh), `VOK` (state front exists), `LRR, LRZ` (lathe rings), `TUBETX`, `WRB` (wreck state).
*Effects:* `LFK, LFL, LFW, LFU, LVS, LV2, WPH, WT`, `MSGT, MSG1$, MSG2$, MSGH, PROMPT$, BNL$`, `GATET, GATESEC`, `MUT`.
*Flying items:* `NFLY, FLN, FLT, FOX, FOY, FOZ, FTX, FTY, FTZ, LOKF, LX, LY, POPT`.
*Save:* `SAVEDMASK, SAVEDT, SAVEDC`; `TITLERES`.

---

## 18 Extending the game

- **A new item or fixture:** add a record to `OBJDATA` (a free thing number; the picture number T; K, SLT, A, R); for a mesh add a case in `MKITEM` / `BUILDDECOR` and a `FIXBIND`, or a record in `FIXDATA` with its state pictures; add its action in `DOACT` (a new number A) and its description in `DESCDATA`. Things 0 .. 199 only (the arrays are `DIM`ed to 200).
- **A new puzzle:** add the flag to `NEWGAME`'s reset line, the effect in `DOACT`, hide the part (SH = 0) and `REVEAL` it; the headless test (`deqq2test.sh`, part 1) should walk it.
- **A new room or building:** edit `deqq2.mesh.json`: the room's object (floor, ceiling, walls with holes), a zone (put zones of upper floors and furniture *before* the zones below), window niche and panes, and check the numbers against §4.3. Keep wall thickness 8 and the opening between rooms of different heights at least 56 high (the player's height). `M3D WORLD` errors name the element.
- **Another Deqq route:** change `ROUTEDATA` / `TOWNDATA` (way points must be visible walking steps: he is a capsule with steps up to 24; a closed door stops him for 1.5 s).
- **The glass:** §4.4.
- **Language facts that cost time while writing this game:** `ELSEIF` does not exist; `STEP`, `CONT`, `LABEL`, `ON`, `OFF`, `TEMPO`, `DEC` are reserved (not usable as names); a `FUNC` must be defined before the line that calls it and is called with `FN`; `NOT` is bitwise (flags are -1 / 0); `DIM` before the first use; a `PROC` has at most 8 parameters; **a bare `RETURN` inside a `PROC` leaves it like `EXIT PROC` (and a `GOSUB`'s return inside it)**; the graphics layer is 300 pixels high.

---

## 19 Status, known issues, lessons

**Seen and heard.** The owner played the game from the title to the ending several times (2026-11-14) and reported: it works, "a much nicer overall feel" after the light effects, water, sounds and music; Deqq and the cars take the detour; the glass looks nice; the installed game works. **Not heard by the author:** every sound and the tunes (instruments and balance are a first guess; tune in `demos/levels/music3d.bas` and the `SFX` lines of the main program). Not examined: the `BUILD` executable with the `JSON LOAD` of the world document (the document is then read from the `res:` image: written, not checked); the pad with the real game.

**The sky in the windows** is the sky picture (§4.4): intended now, documented here.

**Lessons (bugs that were found and fixed; the checks that find them):**
- *Translucent objects were skipped whenever the world had things* (2026-11-14): the renderers kept the list of translucent objects and the list of things in the same scratch array; the things' sort overwrote the list. Fixed in `m3dsoft.c` and `m3dgl.c` (the things use the upper half of the arrays), `m3dtest1` has a case that fails without the fix. Until then the 28 glass panes were never drawn.
- *The gas station had one pump instead of three* (2026-11-14): the pump meshes were numbered 6100 + the thing number (6501 .. 6503), the trees' 6000 + the thing number reaches 6501 and 6503 (things 501, 503): the trees rebuilt (emptied) the pumps. The pumps are 6800 .. 6802 now. **Rule: mesh numbers derived from thing numbers must not overlap between kinds.**
- *Object numbers and thing numbers are different spaces* (a leftover canopy post and a tree both had the number 438): when editing the JSON by script, edit objects by their section.
- *The lamp posts never showed* (2026-11-14): their meshes were built in `LOADLEVEL`, but their things are created later by `TOWNTHINGS` (in `NEWGAME`); `M3D("TON",N)` was false when `BUILDTOWN` looked. Trees come from the JSON document and exist at load time. **Rule: a mesh made from a thing must be built after the thing exists** (`BUILDLAMPS`).
- *Zone order matters* (first match wins).
- *A test copy of the game takes the installed world document* unless one lies beside it (§15).
- *The software renderer in a screenshot harness* needs the graphics layers switched on (`GFX LAYER 3,ON` / `0,ON`), the hardware renderer does not.

**Open / ideas.** A canopy over the gas pumps (the posts were removed; no roof exists); a plain tinted glass (§4.4); the pond animation is four objects switched in turn; vertex caching in the hardware renderer (not needed: both renderers are far under 16.7 ms per frame); a title picture and sounds by ear; Deqq's speed and view cone are first guesses.
