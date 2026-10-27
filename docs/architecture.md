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

`engine1999.c` wires up the display (`sdl_init`), prints the GPL notice and runs the REPL in the **main thread**:
a "Ready." line-input loop (`sdlscr_lineinput`) that understands `SHOW WARRANTY` / `SHOW COPYING` (paged via `pager()`)
and hands every other line to `repl_enter()` (`basrepl.[ch]`: direct mode, stored program lines, `RUN`/`LIST`/`NEW`).
The **screen runs in its own worker thread** (`sdlscreen.c`): it renders the layers and collects the keyboard events into a
queue; the main thread talks to it through `sdlscr_*` (output with `sdlscr_puts`, line input `sdlscr_lineinput`, single
keys `sdlscr_pollkey` / `sdlscr_getkey`) and waits for events with `sdlev_wait`. The host callbacks of the VM
(`vm->print`, `inkey`, `getkey`, `input`, `interrupt`, `save`) are implemented in `engine1999.c` on top of these
(see "Host wiring" in `compiler-and-vm.md`).
All modules except `sdl*`/`*screen` (and `engine1999.c`) use only the C library, so the BASIC system, including the REPL,
can be built and tested without SDL.

## BASIC pipeline (data flow)

1. **Preprocess + tokenize** (`bastok.c`): `preprocess_buffer()` upper-cases text outside quotes (per `ucase_buf` in `engine1999.c`/test programs);
   `tokenize_line()` turns a source line into a `linehdr_t` + token bytes.
2. **Store** (`baspgm.c`, `baslin.c`): `enter_line()` inserts/replaces/deletes the line in `program_t`
   (a doubly linked list of heap-allocated `line_t` nodes linked by pointers). A line without number runs in *direct mode* (callback).
3. **Parse** (`bascomp.c`): recursive-descent over the stored tokens (`comp_eat_*`, one function per grammar rule in `bascomp.ebnf`)
   builds a syntax tree from heap-allocated `node_t` nodes (each with a growable branch array; all nodes are chained in `compiler_t.nodes` and freed by `cleanup_compiler()`). Some semantic work happens during parsing
   ("immediate processing" in `nodetype.h`: constant dimension evaluation, variable lookup/creation).
4. **Generate** (`codegen.c`): `cgen_from_*` walk the tree and emit instructions with `cgen_gen_*` into `codegen_t.code` / `.data`.
5. **Run**: a stack VM defined in `instset.h` (data stack of doubles, return stack of 16-bit addresses). `vm.[ch]` is a minimal executor for it (see [compiler-and-vm.md](compiler-and-vm.md)); `runtime_t` only bundles `varmem_t` + `codegen_t`, the VM is created by the caller.

`runtime_t` (`runtime.h`) is the shared context: variable memory, code generator state, `report`/`halt` callbacks, user data.
`compiler_t` keeps a pointer to it. Error handling pattern: callbacks `report(text)` and `halt()` (noreturn, caller does `longjmp`).

## Conventions that cross modules

- Everything serialized (tokens, code, data) is big endian with 16-bit offsets, so the code and data segments are capped at 64 KiB (see [refactoring.md](refactoring.md#limits)); the program store, the syntax tree and the variables are heap structures.
- Offsets `0xFFFF` mean "none" (`VAROFFS_NONE`, `CODEOFFS_NONE`…); syntax tree nodes and program lines are plain pointers (`NODE_NONE` = null).
- Large structs (`compiler_t`, `codegen_t`, `runtime_t`) are 64–128+ KiB: declare them `static` or heap-allocate, never on the stack
  (the test programs use `static program_t pgm;`).
