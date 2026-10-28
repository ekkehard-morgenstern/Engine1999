# Build and test

## Requirements

Linux, `gcc`, `make`, SDL2 development files (`libsdl2-dev`). Perl only to regenerate the font.

## Targets (`makefile`)

| Command | Result |
|---|---|
| `make` | `engine.lib` (all modules in `BASEMOD`) + `sdltest1 engine1999 basictest1 testcomp1 basictest2 vmtest1 repltest1`, then writes marker file `all` |
| `make DEBUG=1` | same with `-g` instead of `-O3` (do a `rm *.o engine.lib` first — objects do not track flags) |
| `make <name>.o` | compile one module |

`CFLAGS=-Wall -Werror -p $(DBGFLG) -march=native -mtune=native`. Any warning fails the build. Every object depends on *all* headers (`BASEHDR`), so any header edit rebuilds everything.
New modules must be added to both `BASEMOD` and `BASEHDR`. Known makefile wart: rule `8x12font.o: 8x12font.c` does not match the real file `8x12font1.c` (the implicit `.c.o` rule still builds it).

## Executables

| Program | Needs SDL | Purpose |
|---|---|---|
| `engine1999` | yes | Main app: opens window, GPL banner, `Ready.` prompt; only `SHOW WARRANTY` / `SHOW COPYING` are handled |
| `sdltest1` | yes | Visual test of text, tiles, sprites, animation |
| `basictest1` | no | Reads lines from stdin, tokenizes + detokenizes and prints hex dump of tokens (tokenizer test) |
| `basictest2` | no | Like 1, plus `enter_line` into a `program_t`, `list_program`, and its local `compile_program()` (parse → codegen) with colored output |
| `vmtest1` | no | **Automated** end-to-end test (BASIC text → tokens → tree → code → VM → output); table of cases in the source, prints PASS/FAIL per case, exit status = number of failures; `-v` keeps the compiler/codegen debug output; run under valgrind to check leaks. Add a case for every new statement/instruction |
| `repltest1` | no | **Automated** test of the REPL (`basrepl.c`): lines are fed like a user would type them (direct mode, stored lines, `RUN`/`LIST`/`LOAD`/`NEW`/`CLEAR` statements, `INPUT`/`GET`, Break); table of cases in the source, exit status = number of failures; `-v` keeps the compiler/codegen debug output |
| `testcomp1` | no | Randomized stress test for `varmem_t` (despite the name, it tests variables/strings, not the compiler): 5000 random create/delete/read/write operations checked against a shadow model; exit status 0 = passed |

There is no automated test runner or CI. Typical manual check of the BASIC front end:

```
printf '10 PRINT "HI";A$:LET B=1+2*3\n20 GOTO 10\n' | ./basictest2
```

(`basictest*` read until EOF.) These test programs include `basic.h` and exercise `bastok.c` and the compiler; they read arbitrary-length lines with `getline`. Run under `valgrind --leak-check=full` to check the buffer code.

## Font and colors

`perl fontcomp.pl` reads `8x12font1.txt` and writes `8x12font1.c` (it is not wired into `make`; output is written directly to `8x12font1.c`, overwriting it).
`colors-*.txt` are git-ignored scratch/generated data for the palette.

## Gotchas

- `-p` makes every run write `gmon.out` (ignored by git).
- Git-ignored stray files in the working tree: `head`, `main.o`, `all`.
- `compiler_t`, `codegen_t`, `runtime_t` are huge: use `static`.

## Debug output

The BASIC system prints nothing by default. `basdebug.[ch]` holds a flag word (`basdbg_flags`); a host sets bits to get
trace output on **stdout** (one line per item, prefixed with the stage):

| `engine1999` option | Flag | Output |
|---|---|---|
| `--debug-tokenize` | `BASDBG_TOKENIZE` | `[tokenize]` the token bytes of every input line (hex, with the printable characters) |
| `--debug-detokenize` | `BASDBG_DETOKENIZE` | `[detokenize]` the line rebuilt from its tokens (what the tokenizer understood) |
| `--debug-parse` | `BASDBG_PARSE` | `[parse]` every token as the compiler fetches it |
| `--debug-tree` | `BASDBG_TREE` | `[tree]` the syntax tree after parsing (node names, data bytes as hex) |
| `--debug-code` | `BASDBG_CODE` | `[code]` every instruction as the code generator emits it |
| `--debug-all` | `BASDBG_ALL` | all of the above |

`engine1999 --help` lists them. The tokenize/detokenize/tree dumps are produced by hooks in `repl_enter()` / `repl_execute()`;
the parse and code output comes from the compiler and the code generator themselves, so any host gets it by setting the flags.
`vmtest1 -v` turns on parse + code output, `repltest1 -v` everything; `basictest2` always sets `BASDBG_PARSE` (it has its
own, colored syntax tree printer).

