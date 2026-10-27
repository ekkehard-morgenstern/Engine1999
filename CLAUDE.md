# CLAUDE.md

Engine1999 — a 2D game engine in C (SDL2) that simulates a 1980s/90s home computer
(80x25 text screen, tiles, sprites, graphics layer) with a built-in BASIC dialect that is
tokenized, compiled to a syntax tree, and code-generated into a stack-machine instruction set.
GPLv3; programs written *in* the built-in language are explicitly exempt (see `LICENSE`, file headers).

**Git: never commit or run any git write command in this repo — the user does all git writes manually.** Read-only git is fine.
Session progress is tracked in `SESSION.md`; keep it updated when work changes the state.

Full documentation is in `docs/` — start with `docs/README.md`.

## Current state — read first

The branch `feature/refactor-to-remove-some-limits` has completed the tokenizer refactor: `bastok.[ch]` now uses heap strings (`char**`),
growable output buffers (`buf_t`) and bounded read buffers (`rbuf_t`); identifiers/literals may be up to 65535 bytes (2-byte length prefix in the token stream).
All callers were converted and `make` builds cleanly; `basictest1`/`basictest2` run clean under valgrind.
The program store, the syntax tree and the variables are heap structures now; the remaining fixed limits (code/data segments, 16-bit variable numbers) are listed in `docs/refactoring.md`.

## Build

- `make` builds `sdltest1 engine1999 basictest1 testcomp1 basictest2 vmtest1 repltest1` and `engine.lib` (static archive of all modules).
- `make DEBUG=1` → `-g` instead of `-O3`. Flags: `-Wall -Werror -p -march=native` (warnings are errors; `-p` writes `gmon.out`).
- Needs `gcc`, SDL2 (`-lSDL2 -lrt -lm`). Only `sdltest1` and `engine1999` link SDL2.
- `vmtest1` (back end) and `repltest1` (REPL) are the automated regression tests (run `./vmtest1`, `./repltest1`, exit status = failures); the other `*test*` programs are interactive/manual (see `docs/build-and-test.md`).
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
| SDL display/audio engine | `sdlmain, sdlscreen, sdllayer, textscreen, tilescreen, sprscreen, sdlaudio, sdlevent, sdlutil, 8x12font1.c` |
| BASIC tokenizer | `bastok.[ch]` (token IDs, tokenize/detokenize, literal codecs) |
| Program store | `baslin.[ch]` (`line_t` nodes, encoded `linehdr_t`), `baspgm.[ch]` (pointer-linked list of lines, insert/replace/delete/list) |
| Compiler front end | `bascomp.[ch]` + `bascomp.ebnf` (recursive descent → syntax tree), `nodetype.[ch]` (node types) |
| Back end | `codegen.[ch]` (+ `cgenctl.c`: control flow, `cgendecl.c`: DIM/DEF), `instset.h` (ISA spec), `vars.[ch]` (variable/string memory), `runtime.[ch]`, `vm.[ch]` (minimal VM executor), `basrepl.[ch]` (host-independent REPL: direct mode, program lines, RUN/LIST/NEW), `basdebug.[ch]` (debug output flags, token/tree dumps; off by default, `engine1999 --help`) |
| Executables | `engine1999.c` (main app), `sdltest1.c`, `basictest1.c`, `basictest2.c`, `testcomp1.c`, `vmtest1.c` (automated back-end test), `repltest1.c` (automated REPL test) |
| Tools/data | `fontcomp.pl` (8x12font1.txt → 8x12font1.c), `colors-*.txt`, `concept.txt` (design notes) |
