# Demos

Example programs written in the built-in BASIC of Engine1999. They show what the engine can do and serve as manual tests.
The demos are free for everybody to use, copy, change and build into executables. Each file starts with four comment lines (lines 1 to 4) that say so
and carry the one condition: **an executable made from a demo with `BUILD` contains the Engine1999 runtime**, so the Engine1999 Runtime Exception
(`LICENSE-EXCEPTION`) applies to it: the runtime must be the unmodified one and its notice must stay in the executable (it shows with the command line
option `--engine1999-about`). The program text itself is not under the GPL (see `LICENSE-EXCEPTION`, section 1). Keep these four lines (or an equivalent
statement) when you copy a demo and pass an executable of it on.

The listings are indented by block structure (two blanks per level for `FOR`, `REPEAT`, `WHILE`, multi-line `IF`, `PROC` and `FUNC`): the blanks behind the line number are kept by `LOAD`, `LIST` and the editors, and the compiler ignores them.
The editors (`spriteed`, `musiced`, `sfxed`) are **tools** now: they live in the `tools/` folder and run with `TOOL "spriteed"` (see [../tools/README.md](../tools/README.md)).


| File | What it is |
|---|---|
| `sfx.bas` | A sound effect board: press 1-9 for laser, explosion, power-up, coin, jump, siren, alarm, laser burst and footsteps, 0 for an engine whose pitch and volume are driven from the program (`PITCH`, `VOLUME`), Q to quit. The effects are stored with `SFX` and use glides, envelopes, vibrato and the noise wave. |
| `gfx.bas` | Graphics: palettes, the graphics layer (lines, circle, arc, paint, text, `DRAW`), a tile screen, a sprite defined with `DESIGN` rows that moves by itself with `MOVETO`, text screen areas. |
| `mathplot.bas` | Math on one screen: a shaded surface of ripples (`SIN`, `COS`, `SQR`, a 64 color palette from `GFX PALETTE` and `RGB`) and graphs of SIN / COS and SQR / LOG / ATN drawn with `GFX PSET`. Window only; any key ends. |
| `fruit.bas` | **Fruit Invaders**, a Space Invaders style game: cherries, apples and oranges march down, bomb you with seeds, and a pineapple flies by for bonus points. Keyboard control with `KEY()` (LEFT / RIGHT or A / D move the cannon while the key is held, SPACE or UP fires, P, Q), crumbling shields on the graphics layer, a starfield on the tile layer, sprites defined with `DESIGN`, sound effects with `SFX`, `PLAY` jingles. Uses `RND`, `INT` and `\\`. **Game controller:** the D-pad or the left stick, A and the other buttons (see the comment at the top of the file); START pauses, BACK quits. |
| `fruit2.bas` | **Fruit Invaders II**, the same game written with procedures and with much more of the engine: shaded fruit with faces and two animation frames (cherry, strawberry, apple, orange, lemon), a flying pineapple with a spinning propeller, engine-driven animations (`SPRITE SEQ`/`ANIM`), explosions with flying bits (`MOVETO`), a formation that flies in and glides from step to step, a scrolling and twinkling starfield on the tile layer, a ringed planet, a moon and mountains on the background layer, a rainbow title, shaded crumbling shields, bananas that give rapid fire, music while you play (two voices on their own channels, timed by `EVERY`, faster with every wave), sound effects, and a high score kept in `fruit2.hi`. Uses `PROC`/`FUNC`/`CALL`/`LOCAL`, a `LOCAL` array and an array parameter, `FOR EACH`, `EXIT`, `ITERATE`, `DOWNTO`, `AFTER`/`EVERY`, `ON ERROR` with `RESUME <label>`, files, `SIN`/`COS`/`SQR`. Same keys as `fruit.bas`. **Game controller:** the D-pad or the left stick, A and the other buttons (see the comment at the top of the file); START pauses, BACK quits. |
| `fruit3.bas` | **Fruit Invaders III**: Fruit Invaders II plus a mothership made of three sprites (`SPRITE n,SIZE 3,1`) that flies by itself on a sprite path (`SPRITE n,PATH a$`), a boss every fifth wave (a giant watermelon of 4 x 2 sprites on a patrol path, with a health bar), and shooting stars on paths. Window only. |
| `climb.bas` | **The Long Climb**, a jump and run game in the spirit of Lode Runner and Donkey Kong (no likeness of either): a construction worker climbs a steel tower of 120 x 40 tiles (three screens wide, three and a half high, scrolling smoothly) up to an ice cream stand on the roof. Walk, jump (SPACE) and climb ladders (W / S), jump the gaps, hurdles and rust balls and slimes, and drill holes in the girders (Z / C) to trap monsters: when the hole closes again they are gone for a while (there is no weapon). Cars drive on the road, birds fly through the corridors, there is music (a melody, a bass and a drum on three channels, timed by `EVERY`) and sound effects, a time bonus, harder climbs, and a best height in `climb.hi`. Girders, ladders, bricks, concrete, dirt and grass are tiles; the world lives in the RAM (`POKE`, `PEEK`, `PEEK$`), its map is readable ASCII art in the `DATA` lines at the end. Uses `PROC`/`FUNC`/`LOCAL`, recursion (the trees on the hills), a `LOCAL` array passed to an array parameter, `FOR EACH`, `EXIT`, `ITERATE`, `AFTER`/`EVERY`, `ON ERROR` with `RESUME <label>`, `SIN`/`COS`/`EXP`/`LOG`, `DATA`/`RESTORE <label>`. Keys (read with `KEY()`, you move only while a key is held): LEFT / RIGHT or A / D walk, UP / DOWN or W / S climb, SPACE jump (a press a few frames early still counts), Z / X (or , / .) drill left / right, P pause, M music, Q quit. **Game controller:** the D-pad or the left stick, A and the other buttons (see the comment at the top of the file); START pauses, BACK quits. |
| `climb2.bas` | **The Long Climb II**: a giant ice cream cone of 3 x 3 sprites over the stand, title birds on sprite paths, and falling bolts (each a sprite on a path, read with `SPRITE("PATH",n)`). Window only. |
| `madrun.bas` | **MADRUN**, a crossing game for one to four players: a phone rings in the booth on the island in the middle of a busy crossing, and the first player there scores the level. Cars, trucks, buses, motorbikes and ambulances on sixteen lanes (four roads, two lanes in each direction; the lanes next to the middle line are faster) stop for each other and for the crossings (reservations, braking from the stopping distance, a vehicle that has waited a while gets its turn), never for you; pick-ups (whistle stops the traffic, boots, heart), levels with more and faster traffic, a time limit. Asks for keyboard (WASD, cursor keys; 1 or 2 players) or controllers (up to 4: a pad that is connected brings its player, one that is removed takes it away; a blinking question mark marks an empty corner). P pauses, Q quits (START / BACK on a pad). Pictures are drawn at start-up with `GFX LINE`, grabbed with `GFX GRAB` and defined with `SPRITE DEF`. |
| `madrun2.bas` | **MADRUN II**: a traffic helicopter of 4 x 3 sprites with a translucent rotor and shadow, and pigeons, all on sprite paths whose events (`E n`, `SPRITE("EVENT")`) make the sounds. Window only. |
| `river.bas` | **ETERNAL RIVER**, a scroll-down boat game in the spirit of River Raid: steer down an endless, meandering river (rows made up on the way from a hash of the row number and scrolled with `TILE SCROLL` / `TILE FILL`), avoid swimmers, divers, other boats and the rocks that people on the banks throw; bridges with decorative traffic (cars, a bicycle, people) that you pass under; a hit may only throw you into the water: swim to a bank, walk along it (up and down, the river scrolls with you and back) picking up apples, carrots and ice cream cones, and find a boat (moored at a jetty or left drifting); beware of the wraiths on the banks: touch one and you become a wraith yourself for a while (and lose a life). Levels make the river narrower and busier; music that is made up bar by bar. Keys or a pad: steer, UP faster, DOWN slower, P pause, Q quit (START / BACK on a pad). Uses the four libraries. |
| `notepad.bas` | A **text editor** written in BASIC on the UI subsystem: the multi-line editor widget (`UI EDIT`) with selection, the system clipboard, undo and a scroll bar, a file requester for Open / Save as |
| `river2.bas` | **Eternal River II**: a hot air balloon of 3 x 3 sprites with a translucent shadow and flocks of gulls on sprite paths; their events reach a handler through `ON SPRITE GOSUB`. Window only. |
| `rustball.bas` | **BEING A RUSTBALL**, a labyrinth game for one to four players (shared camera): you are a rust ball (three sizes, sprite groups `SPRITE n,SIZE s,s`) in a scrolling labyrinth made at random (depth first search, kept in the RAM, `POKE` / `PEEK` / `PEEK$` + `TILE FILL`) and sneak up on construction workers from behind to cover them in rust (an arrow at the edge points to the nearest one); screws, nuts and bolts make you grow and a big ball crushes walls, foremen with hoses and drying out make you shrink; workers flee from a ball they see, or ignore it while they walk to fruit (breadth first search over the rooms for the distance fields). Window host only; installed as `engine1999-rustball`. |
| `tour.bas` | **A tour of the language**: ten short lessons for somebody who has never used this BASIC (variables, loops and decisions, arrays and `DATA`, procedures and functions, errors, files, graphics and sprites, sound, timers, keys and controllers): each lesson shows a few lines and runs them. Run it in the window; `LIST` it afterwards, it is written the way it teaches. |
| `wumpus.bas` | **Hunt the Wumpus**, the classic cave game (1973), written the old way: line numbers, `GOTO` / `GOSUB` / `POP`, `DATA`, `READ`, `INPUT`, `REM` comments, no procedures and no labels. It only uses `PRINT` and `INPUT`, so it runs in the console too: `engine1999c demos/wumpus.bas`. |
| `lib/util.lib` | **A library** shared by the games (not a program): `RN`, `CLAMP`, `PAD$`, `DISK`, `PLAYSEQ`. The games name it with `LOAD LIBRARY "util"`; `RUN` finds it in `demos/lib/` and compiles it ahead of the game, and `BUILD` embeds it in the executable. |
| `lib/input.lib` | **A library** for the games' controls: `IN_POLL` (arrows / WASD / D-pad / stick of pad 1 -> `IN_DX`, `IN_DY`, `IN_BTN`), `IN_SERVICE` (P / START ask for a pause, Q / BACK quit, other keys in `IN_KEY`), `IN_PAUSELOOP`, and `IN_ANYSTART` / `IN_ANYBACK` / `IN_ANYA` over four pads. Used by `fruit2`, `climb` and `madrun`. |
| `lib/hiscore.lib` | **A library** that keeps one number in a small text file: `HS_LOAD(file$)` (0 if the file is missing or empty: no error, thanks to `EXISTS`) and `HS_SAVE file$, value`. Used by `fruit2`, `climb` and `madrun` for their high scores. |
| `lib/text.lib` | **A library** with the text helpers of the games: `TXTAT` (text screen), `BIGTEXT` (large centered text with a shadow on the graphics layer) and `TX_BYE` (the closing message). The colors are the variables `TX_PAPER` and `TX_SHADOW`. Used by `fruit2`, `climb` and `madrun`. |
| `keys.bas` | A small keyboard that lights up the keys you hold: `KEY("name")` tells whether a key is down right now (it knows when you let go, and several keys at once). Q quits. |
| `joy.bas` | The game controllers: plug in up to four pads (Xbox, PlayStation, Switch Pro, ...) and see the buttons that are held lit, both sticks and both triggers: `JOY(name$, player)` (window version only). Q quits. |
| `lib/uikit.lib` | **A library** of UI helpers (prefix `UK_`): `UK_MSG`, `UK_ASK` (Yes / No), `UK_INPUT$` (a line of text) as modal dialogs, and `UK_SLIDER` / `UK_TRACK` / `UK_VAL` / `UK_SET` for a slider with a label and a readout. Used by `musiced`, `sfxed` and `spriteed`. |
| `lib/mml.lib` | **A library** that cuts a `PLAY` / `SFX` music string into tokens (prefix `MK_`: `MK_SET`, `MK_NEXT$`, `MK_ARG`, `MK_JOIN$`). No UI needed. |
| `groups.bas` | **Sprite groups and tile groups**: a ball made of six sprites (`SPRITE n,SIZE 3,2`) bounces between walls made of tile groups (the library `tilegrp`), collides with the tile blocks as one sprite and counts the meetings with a ghost of four sprites. Window only. |
| `lib/tilegrp.lib` | **A library** for groups of tiles (prefix `TG_`): `TG_DEF`, `TG_PUT`, `TG_ERASE`, `TG_REPEAT`, `TG_HIT`, `TG_W` / `TG_H` / `TG_BASE`. |
| `windows.bas` | Overlapping, draggable windows (`UI WINDOW`): drag by the title bar, click to bring to the front, close boxes, "New window" makes more. Window only. |
| `paths.bas` | **Sprite paths**: four sprites run on paths (`SPRITE n,PATH a$`: back and forth, a square, a staircase that repeats, hide / show) without the program moving them; a handler (`ON SPRITE GOSUB`, `SPRITE("EVENT")`) counts the events the paths report. Window only. |
| `onui.bas` | `ON UI GOSUB`: the main loop only calls `UI UPDATE` and animates a bar and a frame counter; a handler reports the events of a check box, slider, list, text field and buttons. Window only. |
| `filedlg.bas` | The file requester of the library `filereq` (`LOAD LIBRARY "filereq"`): Open... and Save as... buttons, directories, a pattern, a "Replace?" question. Window only. |
| `ui.bas` | A form built from UI widgets (panel, text field, check boxes, radio buttons, list, slider and progress bar, buttons), a pop-up menu and a modal dialog; events with `UI("EVENT")`. Window only. |
| `mouse.bas` | A small paint program for the mouse: left button draws, right button erases, the wheel or a click on the color bar chooses the pen, the pointer's shape follows where it is (`MOUSE SHAPE`), `MOUSE("X")` / `("Y")` / `("B")` / `("PRESSED")` / `("WHEEL")`. C clears, Q quits. Window and console screen mode. |
| `ode.bas` | "Ode to Joy" from Beethoven's 9th symphony (1824, public domain): melody and a simple bass line, one `PLAY` per four bars. |
| `prelude.bas` | The first eight bars of Bach's "Prelude in C major" (BWV 846, public domain), built from a table of notes, played over a held bass note; a chord ends it. |

The music is public domain; the arrangements (note tables, bass lines, tempo, instrument settings) were written for these demos.
Music demos play until the end or until a key is pressed (`SOUND OFF` is issued then); a program that ends leaves queued music playing.

The demos use `WAIT` to pause (a busy loop on `TI` would keep the processor at 100 %).

Conventions:
- Programs are kept as **ASCII listings** with the extension `.bas` (`SAVE "name"` writes them; `LOAD "name"` finds `name.bas`),
  so they can be read and diffed. Binary tokenized programs (`.e99`, `SAVE "name",B`) don't belong in this folder.
- Run one with `engine1999`, then `LOAD "demos/name"` and `RUN`; make a standalone executable with `BUILD "name"` (`./name` starts it).
- The first four lines (1..4) are the license comment: new demos get the same four lines.

Libraries (`demos/lib/*.lib`): a library is a program whose first statement is `LIBRARY "name"`; a program says `LOAD LIBRARY "name"` (a line of its own, never at the prompt)
and `RUN` finds the file next to the program (its own folder, then `lib/` below it). A library is written with `SAVE LIBRARY "name"` (the extension `.lib` is added), edited with
`LOAD "lib/name"` like any program, and cannot be run by itself. Names in a library are global, so a game that loads `util` must not define `RN`, `CLAMP`, `PAD$`, `DISK` or `PLAYSEQ` itself, and the variables of a library start with its prefix (`IN_` for `input`, `HS_` for `hiscore`, `TX_` for `text`).
See `docs/library-support.md`.

## Where to look: which demo shows what

The numbers are BASIC line numbers (`LOAD "demos/river"`, then `LIST 7420-7500`). They point at the first place where the feature is used.

| Feature | Demo and line |
|---|---|
| `PROC` / `FUNC` / `LOCAL`, recursion | `tour.bas`, line 50 |
| procedures that call each other, a game written from them | `fruit2.bas`, line 13730 |
| array parameter (`PROC P(A())`) | `lib/util.lib`, line 270 |
| `FOR EACH`, `EXIT`, `ITERATE`, `DOWNTO` | `fruit2.bas`, line 350 |
| old style: `GOSUB`, `GOTO`, `ON`, `POP`, `DATA` / `READ` | `wumpus.bas`, line 640 |
| `DATA` / `READ` / `RESTORE` with labels | `climb.bas`, line 1740 |
| `ON ERROR` + `RESUME <label>` in the main program | `river.bas`, line 1140 |
| `EXISTS()`, files that may be missing | `lib/hiscore.lib`, line 110 |
| files and drivers: `OPEN`, `LINE INPUT #`, `mem:` | `tour.bas`, line 2180 |
| `PEEK` / `POKE` / `PEEK$`: a world in RAM | `climb.bas`, line 13910 |
| tiles: `TILE FILL` and `TILE SCROLL` (a scrolling world) | `river.bas`, line 7420 |
| an endless world from a hash of the row number | `river.bas`, line 210 |
| pictures drawn with `GFX LINE` and grabbed (`GFX GRAB`, `SPRITE DEF`) | `madrun.bas`, line 2660 |
| pictures by rows: `DESIGN SPRITE` and `@ "row"` | `gfx.bas`, line 130 |
| `SPRITE n,MOVETO`, engine-driven movement | `fruit2.bas`, line 11620 |
| palette changes, translucent colors | `river.bas`, line 1610 |
| `SOUND`, `PLAY`, `WAVE`, `ENVELOPE` | `ode.bas`, line 90 |
| `SFX` effects (stored sounds) | `sfx.bas`, line 20 |
| music made up bar by bar by an `EVERY` handler | `river.bas`, line 13190 |
| `EVERY` / `AFTER` timers | `tour.bas`, line 3020 |
| `KEY()` (keys held down) | `keys.bas`, line 50 |
| game controllers: `JOY()` | `joy.bas`, line 60 |
| `MOUSE`, `ON MOUSE` | `mouse.bas`, line 30 |
| libraries: `LOAD LIBRARY`, `LIBRARY` files | `lib/text.lib`, line 70 |
| a traffic simulation (leaders, reservations) | `madrun.bas`, line 10100 |
| an animation made of sprite frames and sounds (the phone call) | `madrun.bas`, line 12320 |

Start with `tour.bas`, then read a small demo (`sfx`, `gfx`, `keys`), then a game: `wumpus` for the classic style, `fruit2` or `river` for the modern one.
