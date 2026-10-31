# Demos

Example programs written in the built-in BASIC of Engine1999. They show what the engine can do and serve as manual tests.
The demo files carry no license information: they are free for everybody to use, copy and change, without conditions.
(The engine's own license does not apply to programs written in the built-in language, see `LICENSE`.)

| File | What it is |
|---|---|
| `sfx.bas` | A sound effect board: press 1-9 for laser, explosion, power-up, coin, jump, siren, alarm, laser burst and footsteps, 0 for an engine whose pitch and volume are driven from the program (`PITCH`, `VOLUME`), Q to quit. The effects are stored with `SFX` and use glides, envelopes, vibrato and the noise wave. |
| `gfx.bas` | Graphics: palettes, the graphics layer (lines, circle, arc, paint, text, `DRAW`), a tile screen, a sprite defined with `DESIGN` rows that moves by itself with `MOVETO`, text screen areas. |
| `fruit.bas` | **Fruit Invaders**, a Space Invaders style game: cherries, apples and oranges march down, bomb you with seeds, and a pineapple flies by for bonus points. Keyboard control (A/D, S, SPACE, P, Q), crumbling shields on the graphics layer, a starfield on the tile layer, sprites defined with `DESIGN`, sound effects with `SFX`, `PLAY` jingles. Uses `RND`, `INT` and `\\`. |
| `fruit2.bas` | **Fruit Invaders II**, the same game written with procedures and with much more of the engine: shaded fruit with faces and two animation frames (cherry, strawberry, apple, orange, lemon), a flying pineapple with a spinning propeller, engine-driven animations (`SPRITE SEQ`/`ANIM`), explosions with flying bits (`MOVETO`), a formation that flies in and glides from step to step, a scrolling and twinkling starfield on the tile layer, a ringed planet, a moon and mountains on the background layer, a rainbow title, shaded crumbling shields, bananas that give rapid fire, music while you play (two voices on their own channels, timed by `EVERY`, faster with every wave), sound effects, and a high score kept in `fruit2.hi`. Uses `PROC`/`FUNC`/`CALL`/`LOCAL`, a `LOCAL` array and an array parameter, `FOR EACH`, `EXIT`, `ITERATE`, `DOWNTO`, `AFTER`/`EVERY`, `ON ERROR` with `RESUME <label>`, files, `SIN`/`COS`/`SQR`. Same keys as `fruit.bas`. |
| `climb.bas` | **The Long Climb**, a jump and run game in the spirit of Lode Runner and Donkey Kong (no likeness of either): a construction worker climbs a steel tower of 120 x 40 tiles (three screens wide, three and a half high, scrolling smoothly) up to an ice cream stand on the roof. Walk, jump (SPACE) and climb ladders (W / S), jump the gaps, hurdles and rust balls and slimes, and drill holes in the girders (Z / C) to trap monsters: when the hole closes again they are gone for a while (there is no weapon). Cars drive on the road, birds fly through the corridors, there is music (a melody, a bass and a drum on three channels, timed by `EVERY`) and sound effects, a time bonus, harder climbs, and a best height in `climb.hi`. Girders, ladders, bricks, concrete, dirt and grass are tiles; the world lives in the RAM (`POKE`, `PEEK`, `PEEK$`), its map is readable ASCII art in the `DATA` lines at the end. Uses `PROC`/`FUNC`/`LOCAL`, recursion (the trees on the hills), a `LOCAL` array passed to an array parameter, `FOR EACH`, `EXIT`, `ITERATE`, `AFTER`/`EVERY`, `ON ERROR` with `RESUME <label>`, `SIN`/`COS`/`EXP`/`LOG`, `DATA`/`RESTORE <label>`. Keys: A / D walk, W / S climb, SPACE jump, Z / C drill, X stop, P pause, M music, Q quit. |
| `ode.bas` | "Ode to Joy" from Beethoven's 9th symphony (1824, public domain): melody and a simple bass line, one `PLAY` per four bars. |
| `prelude.bas` | The first eight bars of Bach's "Prelude in C major" (BWV 846, public domain), built from a table of notes, played over a held bass note; a chord ends it. |

The music is public domain; the arrangements (note tables, bass lines, tempo, instrument settings) were written for these demos.
Music demos play until the end or until a key is pressed (`SOUND OFF` is issued then); a program that ends leaves queued music playing.

The demos use `WAIT` to pause (a busy loop on `TI` would keep the processor at 100 %).

Conventions:
- Programs are kept as **ASCII listings** with the extension `.bas` (`SAVE "name",A` writes them; `LOAD "name"` finds `name.bas`),
  so they can be read and diffed. Binary tokenized programs (`.e99`, the default of `SAVE "name"`) don't belong in this folder.
- Run one with `engine1999`, then `LOAD "demos/name"` and `RUN`.
