# CLAUDE.md

Engine1999 — a 2D game engine in C (SDL2) that simulates a 1980s/90s home computer
(80x25 text screen, tiles, sprites, graphics layer) with a built-in BASIC dialect that is
tokenized, compiled to a syntax tree, and code-generated into a stack-machine instruction set.
GPLv3; programs written *in* the built-in language are explicitly exempt (see `LICENSE`, file headers).

**Git: never commit or run any git write command in this repo — the user does all git writes manually.** Read-only git is fine.
Session progress is tracked in `SESSION.md`; keep it updated when work changes the state. Open work is listed in `docs/roadmap.md`; keep that current too.

**History log: `HistoryLog-Claude.md` records every change Claude made, per commit (ID, timestamp, details). Keep it updated:**
- After each piece of work, add a short entry (what changed, why, tests run) under its **Pending** section — do this together with the `SESSION.md` update.
- At the start of a session (and whenever the user says they committed), run `git log -i --grep=claude --format='%h|%ad|%s' --date=iso` plus `git show --shortstat`, move the pending entries into the **Log** as `### <id> — <timestamp> — "<subject>"` entries (chronological, oldest first, with file/line stats), and clear Pending. Commits whose message lacks "Claude" are not logged, but mention in the entry if the user bundled Claude's work into one.
- Never commit it myself (git rule above); the user commits it with the rest.

Full documentation is in `docs/` — start with `docs/README.md`.

## Current state — read first

The branch `feature/refactor-to-remove-some-limits` (merged into `main` on 2026-10-28; work continues on `main`) completed the tokenizer refactor: `bastok.[ch]` now uses heap strings (`char**`),
growable output buffers (`buf_t`) and bounded read buffers (`rbuf_t`); identifiers/literals may be up to 65535 bytes (2-byte length prefix in the token stream).
All callers were converted and `make` builds cleanly; `basictest1`/`basictest2` run clean under valgrind.
The program store, the syntax tree and the variables are heap structures now; the code and data segments have 32-bit offsets and grow on demand; the remaining fixed limits (16-bit variable numbers, array dimensions, string length) are listed in `docs/refactoring.md`.

## Build

- `make` builds `sdltest1 sdltest2 engine1999 basictest1 testcomp1 basictest2 vmtest1 repltest1 ansitest1 txttest1 asmtest1 synthtest1` and `engine.lib` (static archive of all modules).
- `make DEBUG=1` → `-g` instead of `-O3`. Flags: `-Wall -Werror -p -march=native` (warnings are errors; `-p` writes `gmon.out`).
- Needs `gcc`, SDL2 (`-lSDL2 -lrt -lm`). Only `sdltest1` and `engine1999` link SDL2.
- `vmtest1` (back end), `repltest1` (REPL), `ansitest1` (ANSI decoder) and `txttest1` (text screen viewports), `asmtest1` (VM assembler/decoder) and `synthtest1` (synthesizer core and music string parser) are the automated regression tests (run `./vmtest1`, `./repltest1`, `./ansitest1`, `./txttest1`, `./asmtest1`, `./synthtest1`, exit status = failures); the other `*test*` programs are interactive/manual (see `docs/build-and-test.md`).
- The makefile has a stale `8x12font.o` rule (file is `8x12font1.c`) and every object depends on *all* headers.
- Build artifacts (`*.o`, binaries, `engine.lib`, `gmon.out`, `all`) are git-ignored but present in the working tree.

## Conventions (follow the surrounding code)

- C99/GNU, 4-space indent, spaces inside parentheses: `if ( x ) {`, `foo( a, b )`; `bool`/`stdint` types, `UINT8_C(…)` constants.
- Every header: GPL banner, `#pragma once` + classic `#ifndef X_H` guard, includes `stdtypes.h` (which defines `_POSIX_C_SOURCE 200112L` and pulls in the std headers).
- Function naming is `<module>_<verb>`: `sdlscr_*`, `txtscr_*`, `comp_eat_*`, `cgen_gen_*`, `cgen_from_*`, `vmem_*`; tokenizer helpers come in `eat_/print_/emit_/read_` quadruples.
- Booleans signal success; `bool` functions that can't continue report via the module's `report`/`halt` callbacks (`comp_error`, `rt->halt`, `vmem->halt`) — `halt` is `ATTR_NORETURN` and usually `longjmp`s.
- All multi-byte values inside the tokenized program, syntax tree node data and bytecode are **big endian**, read/written with explicit shifts (`EXTRACT16`, `VEXTRACT16`, `emit_*`/`read_*`), never by casting.
- Section dividers in `.c`/`.h`: `// -- name ------…`.
- Commit messages in this repo are terse ("Refactor WIP (bastok)", "Codegen WIP"); history carries little explanation, so keep `docs/` current instead.

## Where things are

| Area | Files |
|---|---|
| SDL display/audio engine | `sdlmain, sdlscreen, sdllayer, textscreen (three screens + viewports), tilescreen, sprscreen, sdlaudio (audio device + timed events), sdlevent, sdlutil, 8x12font1.c`; `synth.[ch]`: synthesizer core (oscillators, mixer, event queue, no SDL; `docs/sound.md`), `mml.[ch]`: parser of the `PLAY` music strings; `ansi.[ch]`: ANSI SGR decoder (colors only) behind `sdlscr_ansi_puts/printf()`, no SDL dependency |
| BASIC tokenizer | `bastok.[ch]` (token IDs, tokenize/detokenize, literal codecs) |
| Program store | `baslin.[ch]` (`line_t` nodes, encoded `linehdr_t`), `baspgm.[ch]` (pointer-linked list of lines, insert/replace/delete/list) |
| Compiler front end | `bascomp.[ch]` + `bascomp.ebnf` (recursive descent → syntax tree), `nodetype.[ch]` (node types) |
| Back end | `codegen.[ch]` (+ `cgenctl.c`: control flow, `cgendecl.c`: DIM/DEF, `cgenprog.c`: LOAD/RUN/LIST/NEW/CLEAR/DELETE/RENUM/EDIT), `instset.h` (ISA spec), `vars.[ch]` (variable/string memory), `runtime.[ch]`, `vm.[ch]` (minimal VM executor), `basrepl.[ch]` (host-independent REPL: direct mode, program lines; carries out the LOAD/RUN/NEW requests of a program run), `fulledit.[ch]` (host side: the full-screen editor of `EDIT`, text screen 1), `monitor.[ch]` (host side: the machine monitor of `MONITOR`, text screen 2; with single-step and breakpoints on the resumable VM), `vmasm.[ch]` (VM assembler/decoder, no SDL), `basdebug.[ch]` (debug output flags, token/tree dumps; off by default, `engine1999 --help`) |
| Executables | `engine1999.c` (main app), `sdltest1.c`, `sdltest2.c` (visual ANSI color test), `basictest1.c`, `basictest2.c`, `testcomp1.c`, `vmtest1.c` (automated back-end test), `repltest1.c` (automated REPL test), `ansitest1.c` (automated ANSI decoder test), `txttest1.c` (automated text screen viewport test), `asmtest1.c` (automated VM assembler/decoder test), `synthtest1.c` (automated synthesizer test) |
| Tools/data | `fontcomp.pl` (8x12font1.txt → 8x12font1.c), `colors-*.txt`, `concept.txt` (design notes) |
| Demos | `demos/` — example programs in the built-in BASIC (ASCII `.bas` listings; empty so far, see its README) |
| Residue | `residue/` — leftovers the user kept (test screenshots, a binary `SAVE` test file); disposable, not part of the project, ignore it |
