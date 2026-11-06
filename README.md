# Engine1999

A 2D game engine in C (SDL2) that behaves like a home computer of the late 1980s and early 1990s, and comes with its own BASIC to program it.
You type a program, `RUN` it, and it can use an 80x25 text screen, a tile layer, sprites and a free-drawing graphics layer, a software synthesizer, files and devices.
When it is done, `BUILD` turns it into a standalone executable.

![Fruit Invaders II, a demo game written in Engine1999 BASIC](docs/images/fruit2-game.png)

> **Status: work in progress.** The engine and the language are still changing (statements may be renamed or reworked before a 1.0), and there are rough edges, but it is
> usable today, and the demos below show what it can do. If you like retro computing, BASIC or small games, you may find it useful already. Open work is listed in
> [docs/roadmap.md](docs/roadmap.md).

## What you get

- **A BASIC with line numbers and labels**, structured when you want it: `IF`/`ELSE`, `FOR`/`NEXT` (also `DOWNTO` and `FOR EACH`), `WHILE`, `REPEAT`, `EXIT`, `ITERATE`,
  procedures and functions with local variables and recursion (`PROC`, `FUNC`, `CALL`), `ON ERROR`/`RESUME`, `AFTER`/`EVERY` timers, `DATA`/`READ`, arrays up to six dimensions,
  binary-safe strings, `` `shell commands` `` as string literals. The whole syntax fits on one page: [docs/grammar.html](docs/grammar.html) ([text version](docs/grammar.md)).
- **Five display layers:** background, tiles, graphics, text and sprites, each with its own palette. Lines, circles, flood fill, `DRAW` strings, pictures defined in the program text
  with `DESIGN`, sprites with animation and collision detection, smooth scrolling. See [docs/graphics-subsystem.md](docs/graphics-subsystem.md).
- **Sprite groups, paths and pictures:** one sprite can be a block of several (`SPRITE n,SIZE 3,3`) that moves and collides as one object, sprites can follow a path string on their own (`SPRITE n,PATH "..."`, with events), and pictures load and save as BMP files (`GFX LOAD`, `SPRITE SAVE`, ...).
- **Sound:** an eight channel synthesizer (sine, square, triangle and noise waves, envelopes, vibrato, glides). `PLAY "T120 O4 CDEFG"` plays music strings, `SOUND` and `SFX` make effects. See [docs/sound.md](docs/sound.md).
- **Files and devices:** `OPEN`, `PRINT #`, `LINE INPUT`, random access records, and drivers addressed like `mem:`, `sh:`, `tcp:`, `res:`. See [docs/io-subsystem.md](docs/io-subsystem.md).
- **Mouse, keyboard and game controllers:** `MOUSE(...)`, `KEY("name")` (is a key held right now?), `JOY(...)` for up to four pads, with `ON MOUSE`, `ON KEY` and `ON JOY` handlers. See [docs/controller-support.md](docs/controller-support.md).
- **A UI toolkit and editors:** the `UI` statements give buttons, lists, sliders, scroll bars, overlapping windows and a pixel canvas; a file requester and three editors for sprites and tiles, music and sound effects are written in BASIC on top of it (`demos/spriteed.bas`, `musiced.bas`, `sfxed.bas`). See [docs/ui-subsystem.md](docs/ui-subsystem.md).
- **Libraries:** `LOAD LIBRARY "name"` pulls in a shared BASIC library (the games share `util`, `input`, `text`, `hiscore`, `uikit`, ...); `BUILD` embeds them. See [docs/library-support.md](docs/library-support.md).
- **Safe by default:** a running program may use the files below its own folder; anything else (other folders, shell commands, the network) asks first, and the answer can be remembered. Listings can be UTF-8. See [docs/security-policy.md](docs/security-policy.md).
- **Selection and the clipboard in the editors:** the full-screen editor selects with Shift and the arrows or the mouse and copies, cuts and pastes through the system clipboard (the terminal's in the console version); BASIC programs get `CLIPBOARD$` / `CLIPBOARD a$` and a multi-line text editor widget (`UI EDIT`); `EDIT WINDOW` opens the program in a window editor built on it (`demos/notepad.bas` is a small text editor in BASIC).
- **Tools built in:** a full-screen editor (`EDIT`), a machine monitor with single-step and breakpoints (`MONITOR`), syntax colored `LIST`, `TRACE`, `STOP`/`CONT`, `RENUM`, `MERGE`, `CHAIN`.
- **`BUILD "name"`** makes a standalone Linux executable from your program (details below).
- **`engine1999c`**, a console version without a window for shell scripts and pipes: `engine1999c prog.bas` (also with a `#!` line), `echo 'PRINT 2^10' | engine1999c`, exit codes with `BYE n`. See [docs/console-host.md](docs/console-host.md).

![A program lists itself, with syntax colors](docs/images/listing.png)

### Edit, look around, debug

`EDIT` opens a **full-screen text editor** for the whole program: line numbers in their own pane, syntax colors, a scroll bar, a help bar. It works with the keyboard
(cursor keys, PgUp/PgDn, Ctrl+Home/End, Ctrl+Y deletes a line) and with the **mouse**: click into the text to put the cursor there, turn the wheel, click or hold the arrows of the
scroll bar, drag its thumb. The same editor runs in the console version, in a terminal.

![The full-screen editor showing procedures of Fruit Invaders II](docs/images/editor.png)

At the `Ready.` prompt the **screen itself is the editor**, like on a Commodore 64: `LIST` something, move the cursor up into the listing, change a line and press RETURN to enter it.
The red status line shows the mode (overwrite or insert, F12 switches), the position and the BASIC line number; the screen rows of the line under the cursor are highlighted.

![The screen editor: a listing on the screen, a line picked up for editing](docs/images/screen-editor.png)

`MONITOR` opens a **machine monitor** for the virtual machine your program is compiled to: disassemble, dump and patch memory, set breakpoints, step through the code and watch the stacks.

![The machine monitor: a listing of the compiled code, and twelve steps of it](docs/images/monitor.png)

Select text with Shift and the cursor keys or the mouse, copy, cut and paste with Ctrl+C / X / V (the system clipboard; Ctrl+Insert, Shift+Delete and Shift+Insert as well; in the console version the terminal's):

![The full-screen editor with a selection](docs/images/editor-selection.png)

`EDIT WINDOW` edits the program in a window in the UI toolkit, and `demos/notepad.bas` shows the same editor widget in a small text editor written in BASIC:

![The editor window opened by EDIT WINDOW](docs/images/editor-window.png)

![Notepad, a text editor written in BASIC](docs/images/notepad.png)

The sprite and tile editor, itself a BASIC program built on the UI statements, imports and exports BMP files:

![The sprite and tile editor, a UI program in BASIC](docs/images/spriteed.png)

## Try it

You need Linux, `gcc`, `make` and the SDL2 development files (on Debian and Ubuntu: `sudo apt install build-essential libsdl2-dev`). It is developed and tested on Debian 12 on x86-64.

```
make
./engine1999
```

`sudo make install` puts the programs, the demos, the documentation and the man pages (`man engine1999`, `man engine1999c`, `man engine1999-basic`) under `/usr/local`; with it, `BUILD` works from any directory.

You get a `Ready.` prompt, like a home computer of old. Type a program (ESC stops a running one):

```
10 PRINT "Hello from Engine 1999!"
20 FOR I=1 TO 3
30   SOUND 1,440*I,0.2
40 NEXT I
RUN
LIST
SAVE "hello"
```

Then try the demos: `LOAD "demos/fruit2"` and `RUN`. After `make install` the games are programs of their own (and `engine1999-wumpus`, a text game for any terminal): `engine1999-fruit`, `-fruit2`, `-fruit3`, `-climb`, `-climb2`, `-madrun`, `-madrun2`, `-river`, `-river2` and `-rustball` (each with a manual page, e.g. `man engine1999-rustball`).

| Demo | |
|---|---|
| `demos/fruit2.bas` | **Fruit Invaders II**, a Space Invaders style game with shaded sprites, a scrolling starfield, music and effects (keys: A/D move, SPACE fires, P pauses, Q quits) |
| `demos/climb.bas` | **The Long Climb**, a jump and run game: climb a steel tower, drill holes to trap monsters, reach the ice cream stand on the roof |
| `demos/fruit3.bas` | **Fruit Invaders III**: the same game with a sprite-group mothership, shooting stars and a giant watermelon boss every fifth wave |
| `demos/climb2.bas`, `demos/madrun2.bas`, `demos/river2.bas` | Second editions of the games with new graphics: a giant ice cream cone and falling bolts, a traffic helicopter and pigeons, a hot air balloon and gulls |
| `demos/madrun.bas` | **MADRUN**: a crossing game for one to four players, with a phone booth, traffic and a rescue ambulance |
| `demos/river.bas` | **ETERNAL RIVER**: a scroll-down boat game over an endless river, with bridges, swimmers and wraiths on the banks |
| `demos/rustball.bas` | **BEING A RUSTBALL**: a labyrinth game for one to four players: you are a rust ball, sneak up on the workers from behind, grow by eating nuts and bolts, and crush walls when you are big |
| `demos/spriteed.bas`, `musiced.bas`, `sfxed.bas` | The sprite and tile editor, the music editor and the sound effect editor (UI programs) |
| `demos/fruit.bas` | The first, simpler Fruit Invaders |
| `demos/gfx.bas` | The graphics layers: palettes, drawing, tiles, `DESIGN` pictures, sprites |
| `demos/mouse.bas` | A small paint program for the mouse: draw, erase, choose pens from a color bar (the pointer changes its shape) |
| `demos/keys.bas` | A keyboard that lights up the keys you hold down: `KEY("name")` knows when you let go, and several keys at once |
| `demos/sfx.bas`, `demos/ode.bas`, `demos/prelude.bas` | A sound effect board; Beethoven's "Ode to Joy" and Bach's "Prelude in C" |

![The Long Climb, a jump and run demo](docs/images/climb.png)

![The graphics demo: layers, drawing, a sprite](docs/images/graphics.png)

![Fruit Invaders, the first and simpler version of the game](docs/images/fruit.png)

![Fruit Invaders III, the third edition of the invaders game](docs/images/fruit3.png)

![Being a Rustball: a scrolling labyrinth, a rust ball, workers, bolts and nuts](docs/images/rustball.png)

![The title screen of Being a Rustball](docs/images/rustball-title.png)

![Eternal River II: a boat on the endless river, a bridge with traffic](docs/images/river2.png)

![Madrun II: the crossing with the phone booth in the middle](docs/images/madrun2.png)

![The Long Climb II: steel girders, the sun and a yellow truck](docs/images/climb2.png)

![The paint demo: strokes in several colors, the color bar below, the crosshair pointer](docs/images/mouse-paint.png)

![The keyboard demo: the keys that are held down right now are lit](docs/images/keys.png)

See [demos/README.md](demos/README.md) for all of them. The language is documented in [docs/basic-language.md](docs/basic-language.md).

## Standalone executables

```
LOAD "demos/fruit2"
BUILD "fruit2"
```

and `./fruit2` starts the game without the Engine. `BUILD` uses only the system linker (`ld`; no compiler is needed to build programs), finds what it needs on the system itself,
and links SDL2 and the C library dynamically, so the person who runs the program needs `libSDL2` installed. `BUILD "name", CONSOLE` makes a program without a window that needs **no SDL2 at all** (for servers and scripts: it reads standard input, writes standard output and returns its exit code). To build you need `ld` (binutils), the development files of the C library and SDL2, and `engine1999.o` and `engine.lib` next to `engine1999` (`make` leaves them there). See [docs/basic-language.md](docs/basic-language.md) (section `BUILD`).

## License

Engine1999 is free software under the **GNU General Public License, version 3 or later** ([LICENSE](LICENSE)), with an additional permission, the
**Engine1999 Runtime Exception** ([LICENSE-EXCEPTION](LICENSE-EXCEPTION)):

- **Programs you write in Engine1999 BASIC are yours.** The GPL does not apply to them, and you can license them as you like.
- **Executables made from them with `BUILD`** contain the Engine1999 runtime, but you may still distribute them under terms of your choice, as long as the runtime in them is the
  unmodified one and its notice stays (the executable shows it with `--engine1999-about`).
- Changes to the engine itself stay under the GPL. `SHOW COPYING` in the Engine prints the full terms; the demos start with four comment lines that repeat the conditions.

(The license text was drafted without a lawyer. If it matters for your project, have it reviewed.)

## Documentation

Start with [docs/README.md](docs/README.md). The most useful pages for programmers: the [language reference](docs/basic-language.md), the [syntax at a glance](docs/grammar.html),
[graphics](docs/graphics-subsystem.md), [sound](docs/sound.md), [files and devices](docs/io-subsystem.md).

## Contributing and credits

How the engine is built, how to run the tests and what conventions the code follows is in [docs/contributing.md](docs/contributing.md).

Engine1999 is written by **Ekkehard Morgenstern**, together with **Claude** (Claude Code, an AI coding assistant by Anthropic), which wrote a large part of the code and the documentation
under his direction. Everything Claude changed is recorded commit by commit in [HistoryLog-Claude.md](HistoryLog-Claude.md).

Contact: ekkehard@ekkehardmorgenstern.de
