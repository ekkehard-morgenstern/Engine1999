# Roadmap — what is still to do

The list of open work, so that nothing is forgotten. `[ ]` = open, `[x]` = done. **Status of what already works** is in `SESSION.md` (state, tests, design decisions taken);
the roadmap is only about what is **not** done. Items marked *needs design* wait for a language decision by the project owner before they can be implemented.
Order = suggested order, not a commitment.

The roadmap is split into **parts** (2026-11-08, it had become too big for one file). This file holds the overview and the priorities; the details (the full item rows and the numbered sections, whose numbers were kept) are in the parts:

| Part | Sections | What is in it |
|---|---|---|
| [Language](roadmap/language.md) | 1, 2, 3, 7, 11, 13, 16 | The BASIC dialect: statements, functions, operators, types, program commands, timers, design decisions and ideas from other BASICs. |
| [Sound and music](roadmap/sound.md) | 2b | The synthesizer, `PLAY` / `SFX`, instruments, a tracker, speech. |
| [Hosts, editors and terminals](roadmap/hosts.md) | 4, 12, 14 | The window and the console host, the editors, screen mode, scripting without a GUI, a terminal emulator screen. |
| [I/O, VM and limits](roadmap/io-and-vm.md) | 5, 6, 8, 9 | The I/O subsystem and drivers, error handling, the VM and instruction set leftovers, remaining fixed limits. |
| [Graphics, UI and 3D](roadmap/graphics-and-ui.md) | 10, 15, 18 | The graphics subsystem, sprite / tile / image tools, the UI subsystem, the 3D renderer. |
| [Projects, games and packaging](roadmap/projects.md) | 17 | Ideas of 2026-11-03, games and demos, installation, packages. |
| [Done](roadmap/done.md) | | the rows of the items that are finished |

Item numbers are unique over all parts; "section n" in the texts refers to the numbered sections, which are found in the part named above. **When an item is done, strike it in its part (or move its row to `done.md`) and take it off the table below; new items get the next free number, a row in the fitting part and a line in the table below.**

## Priorities — an overview of everything that is open *(2026-11-01; reviewed against the code 2026-11-08)*


A rating of the open items below, so that the order of work can be chosen at a glance. **High** = games suffer, something is unsafe, or a decision blocks other work;
**Medium** = clearly useful, moderate effort or a quick check; **Low** = a nice-to-have, an edge case, a cleanup or a long-term idea. *Decision* = it needs a
language or design decision of the owner first. The ratings are my suggestion, not a commitment; the parts listed above hold the details. Keep this table in step with
the parts when items are done or added.


| # | Item | Priority | Part |
|---|---|---|---|
| 4 | Joystick and mouse input (10) | Low | [Hosts, editors and terminals](roadmap/hosts.md) |
| 5 | Screen editor: highlight after `GFX BACKDROP` / `GFX PALETTE` (11) | Medium | [Hosts, editors and terminals](roadmap/hosts.md) |
| 6 | Screen mode: plain 256-color vs truecolor terminals (11, 12) | Medium | [Hosts, editors and terminals](roadmap/hosts.md) |
| 7 | Sprite and tile editors (10) | Medium | [Graphics, UI and 3D](roadmap/graphics-and-ui.md) |
| 9 | Binary helpers (9) | Medium-Low | [I/O, VM and limits](roadmap/io-and-vm.md) |
| 12 | `FRAME` done; handlers during `INPUT`/`GET` (2) | Medium-Low | [Language](roadmap/language.md) |
| 13 | `SWITCH` / `CASE` (13) | Medium | [Language](roadmap/language.md) |
| 14 | Full-screen editor (`EDIT`) limits (1) | Medium | [Hosts, editors and terminals](roadmap/hosts.md) |
| 17 | `PLAY` extras (2b) | Medium-Low | [Sound and music](roadmap/sound.md) |
| 18 | Audio without a window (12) | Medium-Low | [Sound and music](roadmap/sound.md) |
| 19 | Drivers written in BASIC (8) | Low | [I/O, VM and limits](roadmap/io-and-vm.md) |
| 20 | Conversion tools and an own image format (10) | Low | [Graphics, UI and 3D](roadmap/graphics-and-ui.md) |
| 23 | `TAB`/`TABMODE`, `DISPLAY(x)`, `BREAKPOINT` (2, 13) | Low | [Language](roadmap/language.md) |
| 24 | `DO … LOOP`, `FOR EVER` (11) | Low | [Language](roadmap/language.md) |
| 26 | `DYNAMIC`/`ASSOC` arrays, `RECORD`/`TYPE` (11) | Low | [Language](roadmap/language.md) |
| 27 | Sound extras (2b) | Low | [Sound and music](roadmap/sound.md) |
| 28 | `tcp:` server side, `SEEK` as a function, byte-array records (8) | Low | [I/O, VM and limits](roadmap/io-and-vm.md) |
| 29 | (`LIST #n` done 2026-11-08) `PRINT` comma zones on files, files kept open across `CHAIN` (1, 4, 8) | Low | [I/O, VM and limits](roadmap/io-and-vm.md) |
| 31 | `RENUM` / `DELETE` / `CLEAR` corners (1) | Low | [Language](roadmap/language.md) |
| 32 | Machine monitor limits (5) | Low | [I/O, VM and limits](roadmap/io-and-vm.md) |
| 33 | VM leftovers (5) | Low | [I/O, VM and limits](roadmap/io-and-vm.md) |
| 34 | Remaining fixed limits (6) | Low | [I/O, VM and limits](roadmap/io-and-vm.md) |
| 35 | Direct mode and `PRINT` corners (4) | Low | [Language](roadmap/language.md) |
| 36 | Test cleanup (4, 12) | Low | [Hosts, editors and terminals](roadmap/hosts.md) |
| 37 | Design decisions to revisit (7) | Low | [Language](roadmap/language.md) |
| 38 | Font review (later) | Low | [Graphics, UI and 3D](roadmap/graphics-and-ui.md) |
| 40 | `BUILD` extras (1) | Low | [Projects, games and packaging](roadmap/projects.md) |
| 41 | Remaining limits of the console host (12) | Low | [Hosts, editors and terminals](roadmap/hosts.md) |
| 42 | Graphics in terminals (12) | Lowest | [Hosts, editors and terminals](roadmap/hosts.md) |
| 43 | Indent fixer (1) | Low | [Hosts, editors and terminals](roadmap/hosts.md) |
| 45 | Terminal emulator text screen (14) | Low-Medium | [Hosts, editors and terminals](roadmap/hosts.md) |
| 46 | Claude integration text screen (14) | Low | [Hosts, editors and terminals](roadmap/hosts.md) |
| 47 | UI subsystem (15) | Medium | [Graphics, UI and 3D](roadmap/graphics-and-ui.md) |
| 49 | Window opens in the background (4) | High | [Hosts, editors and terminals](roadmap/hosts.md) |
| 56 | Audio tracker (17) | Medium-Low | [Sound and music](roadmap/sound.md) |
| 57 | Speech synthesis (17) | Low | [Sound and music](roadmap/sound.md) |
| 58 | Demo scene programs (17) | Low | [Projects, games and packaging](roadmap/projects.md) |
| 65 | Split screen: `TILE VIEW` and the cost of it (64) | Low-Medium | [Graphics, UI and 3D](roadmap/graphics-and-ui.md) |
| 66 | Packages for Debian and Ubuntu, build system (60) | libc-dev`, `libsdl2-dev`), `engine1999-games` | [Projects, games and packaging](roadmap/projects.md) |
| 80 | 3D renderer: hardware path, room-over-room, translucency, more (18) | Low | [Graphics, UI and 3D](roadmap/graphics-and-ui.md) |
| 81 | Synthesizer instruments: subtractive first, 16 presets (2b) | Medium | [Sound and music](roadmap/sound.md) |
| 82 | Screenshot key and `SCREENSHOT` (item 83 step 1) | Medium | [Graphics, UI and 3D](roadmap/graphics-and-ui.md) |
| 83 | AV capture subsystem: audio / video recording with a lock-free queue and ffmpeg | Medium | [Graphics, UI and 3D](roadmap/graphics-and-ui.md) |
| 84 | Demo "Y2K38", a sequel to Y2K Countdown (17) | Low | [Projects, games and packaging](roadmap/projects.md) |


*Token space (2026-11-01):* the one-byte tokens 0..255 are used up (only the unused `PUT`, 13, is free). New words are **extension words**: token 254 (`TOK_XFUNC`) plus a second byte (`XF_*`, table `xkeywtbl` in `bastok.c`): 255 more are possible. `SPACE STRING MIN MAX ATAN2` are the first. Keep this in mind when adding words: a new word is an `xkeywtbl` row plus its parser branch, not a `TOK_` define.

*Cleaned up while rating (2026-11-01): four stale entries were found already done and ticked (`SYMBOL`, file channels, the graphics statements on channel 0, array parameters of `DEF FN`).*

## Not planned / explicitly out of scope for now

- Interpreting source directly: the system compiles to the stack-machine code and runs that (the old interpreter stub was removed).

## Later: look at again

- [ ] Font: review the symbol glyphs again (`% & @ $ ~ # *` and the other punctuation in `8x12font1.txt`); they are the tightest on the 8x12 grid. Also maybe `N` (close to `H` at small size).
