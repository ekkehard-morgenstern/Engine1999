# Demos

Example programs written in the built-in BASIC of Engine1999. They show what the engine can do and serve as manual tests.
The demo files carry no license information: they are free for everybody to use, copy and change, without conditions.
(The engine's own license does not apply to programs written in the built-in language, see `LICENSE`.)

| File | What it is |
|---|---|
| `sfx.bas` | A sound effect board: press 1-9 for laser, explosion, power-up, coin, jump, siren, alarm, laser burst and footsteps, 0 for an engine whose pitch and volume are driven from the program (`PITCH`, `VOLUME`), Q to quit. The effects are stored with `SFX` and use glides, envelopes, vibrato and the noise wave. |
| `gfx.bas` | Graphics: palettes, the graphics layer (lines, circle, arc, paint, text, `DRAW`), a tile screen, a sprite defined with `DESIGN` rows that moves by itself with `MOVETO`, text screen areas. |
| `fruit.bas` | **Fruit Invaders**, a Space Invaders style game: cherries, apples and oranges march down, bomb you with seeds, and a pineapple flies by for bonus points. Keyboard control (A/D, S, SPACE, P, Q), crumbling shields on the graphics layer, a starfield on the tile layer, sprites defined with `DESIGN`, sound effects with `SFX`, `PLAY` jingles. The program has its own random number generator (the language has no `RND` yet). |
| `ode.bas` | "Ode to Joy" from Beethoven's 9th symphony (1824, public domain): melody and a simple bass line, one `PLAY` per four bars. |
| `prelude.bas` | The first eight bars of Bach's "Prelude in C major" (BWV 846, public domain), built from a table of notes, played over a held bass note; a chord ends it. |

The music is public domain; the arrangements (note tables, bass lines, tempo, instrument settings) were written for these demos.
Music demos play until the end or until a key is pressed (`SOUND OFF` is issued then); a program that ends leaves queued music playing.

The demos use `WAIT` to pause (a busy loop on `TI` would keep the processor at 100 %).

Conventions:
- Programs are kept as **ASCII listings** with the extension `.bas` (`SAVE "name",A` writes them; `LOAD "name"` finds `name.bas`),
  so they can be read and diffed. Binary tokenized programs (`.e99`, the default of `SAVE "name"`) don't belong in this folder.
- Run one with `engine1999`, then `LOAD "demos/name"` and `RUN`.
