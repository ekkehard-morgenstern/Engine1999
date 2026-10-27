# Build and test

## Requirements

Linux, `gcc`, `make`, SDL2 development files (`libsdl2-dev`). Perl only to regenerate the font.

## Targets (`makefile`)

| Command | Result |
|---|---|
| `make` | `engine.lib` (all modules in `BASEMOD`) + `sdltest1 engine1999 basictest1 testcomp1 basictest2 vmtest1`, then writes marker file `all` |
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
