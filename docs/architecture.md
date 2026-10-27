# Architecture

## Two halves

```
 +----------------------------- engine1999 (executable) ---------------------------+
 |  SDL display/audio engine                    BASIC system (in progress)           |
 |  sdlmain → sdlscreen ─┬ textscreen           bastok → baslin/baspgm → bascomp    |
 |            sdllayer   ├ tilescreen                       (tokens)  (program)  (tree)
 |            sdlevent   └ sprscreen                              → nodetype → codegen → bytecode
 |            sdlaudio, sdlutil                  vars (variable memory), runtime     |
 +-----------------------------------------------------------------------------------+
```

`engine1999.c` currently only wires up the display (`sdl_init`), prints the GPL notice, and runs a
"Ready." line-input loop that understands `SHOW WARRANTY` / `SHOW COPYING` (paged via `pager()`).
It does **not** yet call the BASIC pipeline. The pipeline is exercised by the separate test programs.
All modules except `sdl*`/`*screen` use only the C library, so the BASIC system can be built and tested without SDL.

## BASIC pipeline (data flow)

1. **Preprocess + tokenize** (`bastok.c`): `preprocess_buffer()` upper-cases text outside quotes (per `ucase_buf` in `engine1999.c`/test programs);
   `tokenize_line()` turns a source line into a `linehdr_t` + token bytes.
2. **Store** (`baspgm.c`, `baslin.c`): `enter_line()` inserts/replaces/deletes the line in `program_t`
   (a doubly linked list of heap-allocated `line_t` nodes linked by pointers). A line without number runs in *direct mode* (callback).
3. **Parse** (`bascomp.c`): recursive-descent over the stored tokens (`comp_eat_*`, one function per grammar rule in `bascomp.ebnf`)
   builds a syntax tree in `compiler_t.tree` (a 64 KiB byte arena, nodes addressed by 16-bit offset). Some semantic work happens during parsing
   ("immediate processing" in `nodetype.h`: constant dimension evaluation, variable lookup/creation).
4. **Generate** (`codegen.c`): `cgen_from_*` walk the tree and emit instructions with `cgen_gen_*` into `codegen_t.code` / `.data`.
5. **Run**: a stack VM defined in `instset.h` (data stack of doubles, return stack of 16-bit addresses). **The VM executor does not exist yet** —
   `basintp.c` is an older token-walking interpreter stub, and `runtime_t` only bundles `varmem_t` + `codegen_t`.

`runtime_t` (`runtime.h`) is the shared context: variable memory, code generator state, `report`/`halt` callbacks, user data.
`compiler_t` keeps a pointer to it. Error handling pattern: callbacks `report(text)` and `halt()` (noreturn, caller does `longjmp`).

## Conventions that cross modules

- Everything serialized (tokens, tree, vars, code) is big endian with 16-bit offsets, so each arena is capped at 64 KiB (see [refactoring.md](refactoring.md#limits)).
- Offsets `0xFFFF` mean "none" (`LINEOFFS_NONE`, `NODEOFFS_NONE`, `VAROFFS_NONE`, `CODEOFFS_NONE`…).
- Large structs (`program_t`, `compiler_t`, `varmem_t`, `codegen_t`) are 64–128+ KiB: declare them `static` or heap-allocate, never on the stack
  (the test programs use `static program_t pgm;`).
