# Build and test

## Requirements

Linux, `gcc`, `make`, SDL2 development files (`libsdl2-dev`). Perl only to regenerate the font.

## Targets (`makefile`)

| Command | Result |
|---|---|
| `make` | `engine.lib` (all modules in `BASEMOD`) + `sdltest1 engine1999 basictest1 testcomp1 basictest2`, then writes marker file `all` |
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
| `testcomp1` | no | Randomized stress test for `varmem_t` (despite the name, it tests variables/strings, not the compiler) |

There is no automated test runner or CI. Typical manual check of the BASIC front end:

```
printf '10 PRINT "HI";A$:LET B=1+2*3\n20 GOTO 10\n' | ./basictest2
```

(`basictest*` read until EOF.) These test programs include `basic.h` and therefore exercise `bastok.c`, which currently does not compile — see [refactoring.md](refactoring.md).

## Font and colors

`perl fontcomp.pl` reads `8x12font1.txt` and writes `8x12font1.c` (it is not wired into `make`; output is written directly to `8x12font1.c`, overwriting it).
`colors-*.txt` are git-ignored scratch/generated data for the palette.

## Gotchas

- `-p` makes every run write `gmon.out` (ignored by git).
- Git-ignored stray files in the working tree: `head`, `main.o`, `all`.
- `program_t`, `compiler_t`, `varmem_t`, `codegen_t` are huge: use `static`.
