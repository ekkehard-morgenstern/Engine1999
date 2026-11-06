# CLAUDE.md

Engine1999 — a 2D game engine in C (SDL2) that simulates a 1980s/90s home computer
(80x25 text screen, tiles, sprites, graphics layer) with a built-in BASIC dialect that is
tokenized, compiled to a syntax tree, and code-generated into a stack-machine instruction set.
GPLv3 with the **Engine1999 Runtime Exception** (`LICENSE-EXCEPTION`, a GPLv3 section 7 additional permission): programs written *in* the built-in language are exempt, and so are executables made from them with `BUILD` (conditions: unmodified runtime, notice kept). Every source file header carries the 4-line `ADDITIONAL PERMISSION` note that points to it; new files get the same; the demos in `demos/` start with four license comment lines (1..4).

**Git: never commit or run any git write command in this repo — the user does all git writes manually.** Read-only git is fine.
Session progress is tracked in `SESSION.md` (imported below, so it is loaded at startup); keep it updated when work changes the state. Open work is listed in `docs/roadmap.md`; keep that current too.

**History log: `HistoryLog-Claude.md` records every change Claude made, per commit (ID, timestamp, details). Keep it updated:**
- After each piece of work, add a short entry (what changed, why, tests run) under its **Pending** section — do this together with the `SESSION.md` update.
- At the start of a session (and whenever the user says they committed), run `git log -i --grep=claude --format='%h|%ad|%s' --date=iso` plus `git show --shortstat`, move the pending entries into the **Log** as `### <id> — <timestamp> — "<subject>"` entries (chronological, oldest first, with file/line stats), and clear Pending. Commits whose message lacks "Claude" are not logged, but mention in the entry if the user bundled Claude's work into one.
- Never commit it myself (git rule above); the user commits it with the rest.

**Test processes (owner's rule, 2026-11-05): clean up everything started for a test.** Background programs (Xvfb, metacity, `engine1999`, test runners, injectors) get a `timeout`, their PID is recorded and they are killed in the same step that ends the test; at the end of every piece of work run `ps -eo pid,etimes,args | grep -E 'Xvfb|metacity|engine1999|test1'` and kill leftovers. Logs and scratch output go to the scratchpad directory, never into the repo. Never use `pkill -f` with a pattern that can match the own shell.

**Backups and unattended work (owner's rules, 2026-11-04):** the milestone backups (`./backup.sh --label <what>`, a ZIP of the working tree in `backups/`, no git; `--list`, `--restore <zip>`) are my own safety net: one at each milestone of a roadmap item and before a risky step (label `pre-<what>`); if something goes awry I restore it myself (into `backups/restored/`, then copy back only the files I need). When the owner is away I do at most ONE roadmap item per stretch (finish, check, bookkeeping in `SESSION.md` / `HistoryLog-Claude.md` / docs, a last backup), then stop and wait for the owner to review and commit; the next item starts when the owner says so (or named several items himself).

**Installed games (owner's rule, 2026-11-03):** executables get collision-free names (`engine1999-<name>`) and each game its own man page (`man/engine1999-<name>.6`); new games go into `GAMES` / `CONGAMES` in the makefile.

**Compatibility of saved programs (owner's rule, 2026-11-03): never make a breaking change to the program file format (`.e99`, `PGMFILE_*` in `baspgm.h`).** A program saved once must always load again. If the format must change, bump the version number in the header and keep reading the old versions in `LOAD` / `MERGE` / `CHAIN`; add a test with a saved file of each old version. (`SAVE "name"` writes the ASCII listing `.bas` by default since 2026-11-03; `SAVE "name",B` writes the binary form.)

**Uninitialized memory (owner's rule, 2026-11-03): in C, local variables and memory from `malloc()` and the new part of a `realloc()` block are uninitialized.** (Static and global variables are zeroed by the system, but don't rely on that for anything that is re-initialized.) Always set such memory explicitly (`memset`, `calloc`, an initializer) before it is read; check new code for it, and look for it first when something behaves differently between machines.

**Man pages: `man/engine1999.1`, `man/engine1999c.1` and `man/engine1999-basic.7` (installed by `make install`, read with `man engine1999`) must stay current: when a command line option, an environment variable, a statement or function group, a driver, `BUILD` behavior or an exit status changes, update them in the same piece of work. `make man-check` (`sh mancheck.sh`) fails if an option of `engine1999`/`engine1999c --help` is missing from its page or the list of reserved words (between the `KEYWORDS-BEGIN/END` comments of the section page) is not `engine1999c --list-keywords`; `sh mancheck.sh --update` rewrites that list. It must pass. Pages carry `@VERSION@` in the `.TH` line (filled in at install time).**

**Consistency of the hosts: editing and input behave the same in the window version (`engine1999`) and the console version (`engine1999c`): the line input of the prompt and `INPUT`, the one-line editor, the full-screen editor, the monitor, break keys, key codes and escape sequences. When one is changed, change the other (or write down why they differ); shared code (one implementation behind a small driver interface) is preferred to two copies. Keep this in `SESSION.md` and `docs/roadmap.md` as a standing rule.**

**Grammar: when a statement, function or operator is added or changed, update its rule in `bascomp.ebnf` (documentation only: nothing reads it; a new section starts with a `--# Title` line) and run `make grammar` (`perl grammar.pl`) to regenerate `docs/grammar.md` / `docs/grammar.html`; `make grammar-check` must pass.**

Full documentation is in `docs/` — start with `docs/README.md`.

@SESSION.md

## Current state — read first

The branch `feature/refactor-to-remove-some-limits` (merged into `main` on 2026-10-28; work continues on `main`) completed the tokenizer refactor: `bastok.[ch]` now uses heap strings (`char**`),
growable output buffers (`buf_t`) and bounded read buffers (`rbuf_t`); identifiers/literals may be up to 65535 bytes (2-byte length prefix in the token stream).
All callers were converted and `make` builds cleanly; `basictest1`/`basictest2` run clean under valgrind.
The program store, the syntax tree and the variables are heap structures now; the code and data segments have 32-bit offsets and grow on demand; the remaining fixed limits (16-bit variable numbers, array dimensions, string length) are listed in `docs/refactoring.md`.

## Build

- `make` builds `sdltest1 sdltest2 sdltest3 sdltest4 engine1999 joytest1 basictest1 testcomp1 basictest2 vmtest1 repltest1 ansitest1 txttest1 asmtest1 synthtest1 iotest1 gfxtest1 buildtest1 engine1999c contest1 keytest1 scrtest1 libtest1 poltest1` and `engine.lib` (static archive of all modules).
- `make DEBUG=1` → `-g` instead of `-O3`. `make PROFILE=1` adds `-p` (every run writes `gmon.out`). Flags: `-Wall -Werror -march=native` (warnings are errors).
- Needs `gcc`, SDL2 (`-lSDL2 -lrt -lm`). Only `sdltest1`, `sdltest3` and `engine1999` link SDL2; `engine1999c` (the console host, no window) does not.
- `make kit` builds the portable, profiling-free kit in `kit/` (git-ignored) and `make install` [`PREFIX=/usr/local`, `DESTDIR=`] installs the programs, the `BUILD` kit (`$(PREFIX)/lib/engine1999`), demos, docs and man pages; `make uninstall` removes them. `BUILD` finds an installed kit through `../lib/engine1999` relative to the running program.
- The version (banner, `--version`, `--engine1999-about`) is the newest `vX.Y.Z` git tag: `bump_version.sh` tags (local only), `tagtoverstr.sh` writes the git-ignored `version.h` during `make` (`docs/build-and-test.md`, "Version").
- `vmtest1` (back end), `repltest1` (REPL), `ansitest1` (ANSI decoder) and `txttest1` (text screen viewports, cell flags, overlays), `asmtest1` (VM assembler/decoder), `synthtest1` (synthesizer core and music string parser) and `iotest1` (I/O drivers and channels) and `gfxtest1` (drawing core, tiles, sprites, text areas) `buildtest1` (the `BUILD` linking, run in the build directory) and `contest1` (the console host `engine1999c`, run in the build directory) `keytest1` (key sequences), `joytest1` (game controller state; `sdltest4 --selftest` is the SDL side with virtual pads) `scrtest1` (the editors on a fake screen), `synhltest1` (the syntax highlighter `synhl.c` against `LIST`'s colors, on the demos) and `libtest1` (the library resolver `baslib.c`: scanning, dependency order, cycles, errors) and `poltest1` (the security policy `policy.c`: paths, grants, questions) are the automated regression tests (run `./poltest1`, `./vmtest1`, `./repltest1`, `./ansitest1`, `./txttest1`, `./asmtest1`, `./synthtest1`, `./iotest1`, `./gfxtest1`, `./buildtest1`, `./contest1`, `./keytest1`, `./scrtest1`, `./libtest1`, exit status = failures); the other `*test*` programs are interactive/manual (see `docs/build-and-test.md`).
- The makefile has a stale `8x12font.o` rule (file is `8x12font1.c`) and every object depends on *all* headers.
- Build artifacts (`*.o`, binaries, `engine.lib`, `gmon.out`, `all`) are git-ignored but present in the working tree.

## Conventions (follow the surrounding code)

- C99/GNU, 4-space indent, spaces inside parentheses: `if ( x ) {`, `foo( a, b )`; `bool`/`stdint` types, `UINT8_C(…)` constants.
- Every header: GPL banner, `#pragma once` + classic `#ifndef X_H` guard, includes `stdtypes.h` (which defines `_POSIX_C_SOURCE 200809L` and pulls in the std headers).
- Function naming is `<module>_<verb>`: `sdlscr_*`, `txtscr_*`, `comp_eat_*`, `cgen_gen_*`, `cgen_from_*`, `vmem_*`; tokenizer helpers come in `eat_/print_/emit_/read_` quadruples.
- Booleans signal success; `bool` functions that can't continue report via the module's `report`/`halt` callbacks (`comp_error`, `rt->halt`, `vmem->halt`) — `halt` is `ATTR_NORETURN` and usually `longjmp`s.
- All multi-byte values inside the tokenized program, syntax tree node data and bytecode are **big endian**, read/written with explicit shifts (`EXTRACT16`, `VEXTRACT16`, `emit_*`/`read_*`), never by casting.
- Section dividers in `.c`/`.h`: `// -- name ------…`.
- Commit messages in this repo are terse ("Refactor WIP (bastok)", "Codegen WIP"); history carries little explanation, so keep `docs/` current instead.

## Where things are

| Area | Files |
|---|---|
| SDL display/audio engine | `sdlmain, sdlscreen, sdllayer, textscreen (three screens + viewports), tilescreen, sprscreen, sdlaudio (audio device + timed events), sdlevent, sdlutil, 8x12font1.c`; `synth.[ch]`: synthesizer core (oscillators, mixer, event queue, no SDL; `docs/sound.md`), `mml.[ch]`: parser of the `PLAY` music strings; `ansi.[ch]`: ANSI SGR decoder (colors only) behind `sdlscr_ansi_puts/printf()`, no SDL dependency; `palette.[ch]`: the engine's start palette (no SDL), `keyseq.[ch]`: the escape sequences of cursor/function keys (canonical forms, parser of terminal dialects, no dependencies), `sdlkeys.[ch]`: SDL key code -> sequence (SDL headers only) |
| BASIC tokenizer | `bastok.[ch]` (token IDs, tokenize/detokenize, literal codecs) |
| Program store | `baslin.[ch]` (`line_t` nodes, encoded `linehdr_t`), `baspgm.[ch]` (pointer-linked list of lines, insert/replace/delete/list) |
| Compiler front end | `bascomp.[ch]` + `bascomp.ebnf` (recursive descent → syntax tree), `nodetype.[ch]` (node types) |
| Back end | `codegen.[ch]` (+ `cgenctl.c`: control flow, `cgendecl.c`: DIM/DEF, `cgenprog.c`: LOAD/RUN/LIST/NEW/CLEAR/DELETE/RENUM/EDIT, `cgenlib.c`: library mode checks), `instset.h` (ISA spec), `vars.[ch]` (variable/string memory), `runtime.[ch]`, `vm.[ch]` (minimal VM executor), `basfiles.[ch]` (the `FILES` listing, no SDL), `iodev.[ch]` (the I/O subsystem: drivers `file`/`null`/`mem`/`sh`/`res`/`con`/`play`/`tcp`, `driver:` names, the channel table of `OPEN`; no SDL, `docs/io-subsystem.md`), `baslib.[ch]` (library support: finds the libraries a program names with `LOAD LIBRARY`, in dependency order, cycle / clash checks; no SDL; `docs/library-support.md`), `basrepl.[ch]` (host-independent REPL: direct mode, program lines; carries out the LOAD/RUN/NEW/BYE requests of a program run), `engine1999c.c` (the console host: stdin/stdout/exit status, no SDL, `docs/console-host.md`), `fulledit.[ch]` (the full-screen editor of `EDIT`, text screen 1) and `monitor.[ch]` (the machine monitor of `MONITOR`, text screen 2; single-step and breakpoints on the resumable VM): both run on a **screen driver** `scrdrv.h` (display + keys + resize; no SDL) with two backends, the window's (`sdlscr_driver()` in `sdlscreen.c`) and the terminal's (`con_drv` in `engine1999c.c`, painting by `conscr.[ch]`: alternate screen, no curses); `scredit.c` is the one-line editor and the editing keys for both, `vmasm.[ch]` (VM assembler/decoder, no SDL), `basbuild.[ch]` (the `BUILD` statement: standalone executables made with `ld` only, no SDL; kit = `engine1999.o` + `engine.lib`), `basdebug.[ch]` (debug output flags, token/tree dumps; off by default, `engine1999 --help`) |
| Graphics | `cursors.[ch]` (the mouse pointer's pictures for the reserved sprite 255, no SDL), `mouse.[ch]` (the mouse: position, buttons, the 3 s show/hide of the pointer, no SDL; the screen thread owns the pointer), `joy.[ch]` (the game controllers: slots, buttons, axes, dead zone, no SDL; atomic words) and `sdljoy.[ch]` (SDL game controller / joystick events -> `joy.h`), `gfxcore.[ch]` (drawing on a palette bitmap, no SDL), `gfxops.[ch]` (the table of the GFX/TXT/TILE/SPRITE statements and functions), backends `tilescreen`, `sprscreen`, `textscreen`; **everything is called through the `sdlscr_*` API of `sdlscreen`**; `docs/graphics-subsystem.md` |
| Executables | `engine1999.c` (main app), `sdltest1.c`, `sdltest2.c` (visual ANSI color test), `sdltest3.c` (visual test of the mouse pointer shapes), `basictest1.c`, `basictest2.c`, `testcomp1.c`, `vmtest1.c` (automated back-end test), `repltest1.c` (automated REPL test), `ansitest1.c` (automated ANSI decoder test), `txttest1.c` (automated text screen viewport test), `asmtest1.c` (automated VM assembler/decoder test), `synthtest1.c` (automated synthesizer test), `iotest1.c` (automated I/O layer test), `gfxtest1.c` (automated graphics backend test), `buildtest1.c` + `buildstub.c` (automated `BUILD` test and its stand-in runtime) |
| Tools/data | `fontcomp.pl` (8x12font1.txt → 8x12font1.c), `grammar.pl` (`bascomp.ebnf` → `docs/grammar.md` + `docs/grammar.html`, the quick reference card), `colors-*.txt`, `concept.txt` (design notes) |
| Demos | `demos/` — example programs in the built-in BASIC (ASCII `.bas` listings; lines 1..4 of each are the license comment for `BUILD` executables, see its README) |
| Residue | `residue/` — leftovers the user kept (test screenshots, a binary `SAVE` test file); disposable, not part of the project, ignore it |
