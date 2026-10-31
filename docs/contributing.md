# For contributors

This page is for people who want to work on the engine itself. If you only want to use Engine1999 or write programs in it, start with the [README](../README.md) and the [language reference](basic-language.md).

## Who has contributed so far

- **Ekkehard Morgenstern**: the author and owner of the project; design, display engine, the original code, and every decision about what goes in.
- **Claude** (Claude Code, an AI coding assistant by Anthropic), working in the author's repository under his direction: most of the BASIC system (tokenizer refactor, compiler, code generator, VM),
  the I/O, sound and graphics subsystems, the editor and monitor, the demos, and the documentation. **[HistoryLog-Claude.md](../HistoryLog-Claude.md) lists every change Claude made, commit by commit**
  (commit ID, time, files, what was changed, why and how it was tested). Commits that include Claude's work say so in their message ("by Claude Code").
- The author makes all git commits himself; Claude does not commit.

## Get started

- Linux, `gcc`, `make`, SDL2 development files. `make` builds everything (`-Wall -Werror`: a warning fails the build); `make DEBUG=1` builds with `-g`.
- Run the automated tests after every change; the exit status of each is the number of failures:

  ```
  ./vmtest1 ./repltest1 ./ansitest1 ./txttest1 ./asmtest1 ./synthtest1 ./iotest1 ./gfxtest1 ./buildtest1
  ```

  (one at a time). `buildtest1` runs in the build directory. Details of every test program: [build-and-test.md](build-and-test.md).
- Where things are: [architecture.md](architecture.md) (module map and data flow), [compiler-and-vm.md](compiler-and-vm.md) (compiler, code generator, instruction set, VM),
  [display-engine.md](display-engine.md), [graphics-subsystem.md](graphics-subsystem.md), [sound.md](sound.md), [io-subsystem.md](io-subsystem.md). The sources of truth are
  `bascomp.ebnf` (grammar), `instset.h` (instruction set), `nodetype.h` (syntax tree), `bastok.h` (tokens).
- Open work and the questions waiting for a decision: [roadmap.md](roadmap.md).

## How a new statement goes in

The pipeline is: tokenizer (`bastok.c`) → parser (`bascomp.c`, recursive descent, produces a syntax tree) → code generator (`codegen.c`, `cgen*.c`, produces stack machine code) → VM (`vm.c`).
A statement touches every stage; `BUILD` and `FILES` are small examples to copy from:

1. `bastok.h` / `bastok.c`: a token number and its entry in the keyword table (and, for the colors of `LIST`, `keyword_hlclass()`).
2. `nodetype.h` / `nodetype.c`: a node type.
3. `bascomp.c` / `bascomp.h`: `comp_eat_<name>stmt()`, and its call in `comp_eat_progstmt()` (or the group it belongs to).
4. `instset.h` (the instruction with its documentation), `codegen.c` / `codegen.h` (its name, `cgen_gen_<name>()`, the dispatch) and `cgen_from_<name>stmt()` (in `cgenprog.c` or the file of its group).
5. `vm.c` / `vm.h`: executing the instruction and, if the host has to do something, a callback in `vm_t`.
6. `bascomp.ebnf`: its rule; then `make grammar` (needs perl) and `make grammar-check`.
7. Tests: cases in `vmtest1.c` (a logging host stands in for the screen) and, for REPL behavior, `repltest1.c`.
8. Documentation: [basic-language.md](basic-language.md), [compiler-and-vm.md](compiler-and-vm.md), [roadmap.md](roadmap.md), and `SESSION.md`.

## Conventions

- C99/GNU, 4-space indent, spaces inside parentheses (`if ( x ) {`, `foo( a, b )`), `bool` and `stdint` types, `UINT8_C(...)` constants.
- Every source file starts with the GPL banner including the `ADDITIONAL PERMISSION` note (copy it from a neighbor). A new header uses `#pragma once` plus a classic include guard, and includes `stdtypes.h`.
- Function names are `<module>_<verb>`. Functions that can fail return `bool`; errors go through the module's `report`/`halt` callbacks. Multi-byte values in the tokenized program, the syntax tree and the bytecode are big endian and are read and written with explicit shifts.
- Everything that does not need a window goes in a module without SDL, so it can be tested without a screen. The display is always called through the `sdlscr_*` API of `sdlscreen.c`.
- A new object file needs its dependency line in the `makefile` (the objects `fulledit.o`, `monitor.o` and `vmasm.o` once went stale for lack of one).
- Commit messages are terse. The history carries little explanation, so keep the documentation in `docs/` current instead.

## Contributing and the license

The engine is GPLv3 or later with the [Engine1999 Runtime Exception](../LICENSE-EXCEPTION), which lets programs and `BUILD` executables go out under other terms. The author can only grant that exception for code he
owns the rights to. **So before a contribution can be accepted, the author has to agree with the contributor that the contribution may be distributed under these terms, the exception included.** Please contact
ekkehard@ekkehardmorgenstern.de first (for anything but a small fix), and do not add code that has an incompatible license.
