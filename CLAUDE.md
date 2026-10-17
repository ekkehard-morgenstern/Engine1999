# CLAUDE.md

Engine1999 — a 2D game engine in C (SDL2) that simulates a 1980s/90s home computer
(80x25 text screen, tiles, sprites, graphics layer) with a built-in BASIC dialect that is
tokenized, compiled to a syntax tree, and code-generated into a stack-machine instruction set.
GPLv3; programs written *in* the built-in language are explicitly exempt (see `LICENSE`, file headers).

**Git: never commit or run any git write command in this repo — the user does all git writes manually.** Read-only git is fine.
Session progress is tracked in `SESSION.md`; keep it updated when work changes the state.

Full documentation is in `docs/` — start with `docs/README.md`.

## Current state — read first

Branch `feature/refactor-to-remove-some-limits` is **mid-refactor and does not compile**
(`make bastok.o` fails). The goal is to remove fixed 255/256-byte limits in the BASIC tokenizer layer
(`bastok.c/.h`) by replacing `char target[256]` + `char**/size_t* premain` cursors with heap strings
(`char**`), growable output buffers (`buf_t`) and bounded read buffers (`rbuf_t`).
Only `uint32`, buffers, identifiers and the generic literals (`eat/print/emit/read_lit`) are converted so far;
everything below them in `bastok.c`, plus all callers, still uses the old API.
Details, known bugs in the converted code, and the remaining checklist: `docs/refactoring.md`.
Do not "fix" unrelated code to make the build pass; continue the conversion in the order given there.

## Build

- `make` builds `sdltest1 engine1999 basictest1 testcomp1 basictest2` and `engine.lib` (static archive of all modules).
- `make DEBUG=1` → `-g` instead of `-O3`. Flags: `-Wall -Werror -p -march=native` (warnings are errors; `-p` writes `gmon.out`).
- Needs `gcc`, SDL2 (`-lSDL2 -lrt -lm`). Only `sdltest1` and `engine1999` link SDL2.
- No test framework; the `*test*` programs are interactive/manual (see `docs/build-and-test.md`).
- The makefile has a stale `8x12font.o` rule (file is `8x12font1.c`) and every object depends on *all* headers.
- Build artifacts (`*.o`, binaries, `engine.lib`, `gmon.out`, `all`) are git-ignored but present in the working tree.

## Conventions (follow the surrounding code)

- C99/GNU, 4-space indent, spaces inside parentheses: `if ( x ) {`, `foo( a, b )`; `bool`/`stdint` types, `UINT8_C(…)` constants.
- Every header: GPL banner, `#pragma once` + classic `#ifndef X_H` guard, includes `stdtypes.h` (which defines `_POSIX_C_SOURCE 200112L` and pulls in the std headers).
- Function naming is `<module>_<verb>`: `sdlscr_*`, `txtscr_*`, `comp_eat_*`, `cgen_gen_*`, `cgen_from_*`, `vmem_*`; tokenizer helpers come in `eat_/print_/emit_/read_` quadruples.
- Booleans signal success; `bool` functions that can't continue report via the module's `report`/`halt` callbacks (`comp_error`, `rt->halt`, `vmem->halt`) — `halt` is `ATTR_NORETURN` and usually `longjmp`s.
- All multi-byte values inside the tokenized program, syntax tree, variable memory and bytecode are **big endian**, read/written with explicit shifts (`EXTRACT16`, `VEXTRACT16`, `emit_*`/`read_*`), never by casting.
- Section dividers in `.c`/`.h`: `// -- name ------…`.
- Commit messages in this repo are terse ("Refactor WIP (bastok)", "Codegen WIP"); history carries little explanation, so keep `docs/` current instead.

## Where things are

| Area | Files |
|---|---|
| SDL display/audio engine | `sdlmain, sdlscreen, sdllayer, textscreen, tilescreen, sprscreen, sdlaudio, sdlevent, sdlutil, 8x12font1.c` |
| BASIC tokenizer | `bastok.[ch]` (token IDs, tokenize/detokenize, literal codecs) |
| Program store | `baslin.[ch]` (line list nodes/headers), `baspgm.[ch]` (64 KiB program memory, insert/update/list) |
| Compiler front end | `bascomp.[ch]` + `bascomp.ebnf` (recursive descent → syntax tree), `nodetype.[ch]` (node types) |
| Back end | `codegen.[ch]`, `instset.h` (ISA spec), `vars.[ch]` (variable/string memory), `runtime.[ch]` |
| Interpreter (stub) | `basintp.[ch]` (direct mode / token fetch) |
| Executables | `engine1999.c` (main app), `sdltest1.c`, `basictest1.c`, `basictest2.c`, `testcomp1.c` |
| Tools/data | `fontcomp.pl` (8x12font1.txt → 8x12font1.c), `colors-*.txt`, `concept.txt` (design notes) |
