# Build and test

## Requirements

Linux, `gcc`, `make`, SDL2 development files (`libsdl2-dev`). Perl only to regenerate the font.

## Targets (`makefile`)

| Command | Result |
|---|---|
| `make` | `engine.lib` (all modules in `BASEMOD`) + `sdltest1 sdltest2 engine1999 basictest1 testcomp1 basictest2 vmtest1 repltest1 ansitest1 txttest1 asmtest1 synthtest1 iotest1 gfxtest1 buildtest1 engine1999c contest1 keytest1`, then writes marker file `all` |
| `make DEBUG=1` | same with `-g` instead of `-O3` (do a `rm *.o engine.lib` first — objects do not track flags) |
| `make <name>.o` | compile one module |

`CFLAGS=-Wall -Werror -p $(DBGFLG) -march=native -mtune=native`. Any warning fails the build. Every object depends on *all* headers (`BASEHDR`), so any header edit rebuilds everything.
New modules must be added to both `BASEMOD` and `BASEHDR`. Known makefile wart: rule `8x12font.o: 8x12font.c` does not match the real file `8x12font1.c` (the implicit `.c.o` rule still builds it).

## Executables

| Program | Needs SDL | Purpose |
|---|---|---|
| `engine1999` | yes | Main app: opens window, GPL banner, `Ready.` prompt; only `SHOW WARRANTY` / `SHOW COPYING` are handled |
| `sdltest1` | yes | Visual test of text, tiles, sprites, animation |
| `sdltest2` | yes | Visual test of `sdlscr_ansi_puts()`: colored text through ANSI SGR sequences, then repeats typed lines in color |
| `basictest1` | no | Reads lines from stdin, tokenizes + detokenizes and prints hex dump of tokens (tokenizer test) |
| `basictest2` | no | Like 1, plus `enter_line` into a `program_t`, `list_program`, and its local `compile_program()` (parse → codegen) with colored output |
| `vmtest1` | no | **Automated** end-to-end test (BASIC text → tokens → tree → code → VM → output); table of cases in the source, prints PASS/FAIL per case, exit status = number of failures; `-v` keeps the compiler/codegen debug output; run under valgrind to check leaks. Add a case for every new statement/instruction |
| `repltest1` | no | **Automated** test of the REPL (`basrepl.c`): lines are fed like a user would type them (direct mode, stored lines, `RUN`/`LIST`/`LOAD`/`NEW`/`CLEAR` statements, `INPUT`/`GET`, Break); table of cases in the source, exit status = number of failures; `-v` keeps the compiler/codegen debug output |
| `asmtest1` | no | **Automated** test of the VM assembler / decoder (`vmasm.c`): encodings, error messages, decoding, and a round trip (decode → format → assemble gives the same bytes) over every instruction of a compiled program; exit status = number of failures |
| `engine1999c` | no | The console host (`engine1999c.c`, see [console-host.md](console-host.md)): no window, no SDL, links with `-lrt -lm` only |
| `contest1` | no | **Automated** test of the console host (and of `BUILD` from it, the console and window kits): runs `./engine1999c` in a temporary directory through the shell (pipes, files, exit codes, the `#!` line, `INPUT`/`GET`/`INKEY$` from a pipe, sound and graphics statements, SIGINT as Break); exit status = number of failures; run it in the build directory |
| `buildtest1` | no | **Automated** test of `BUILD` (`basbuild.c`): links a dynamic executable with the system linker only, using the stand-in runtime `buildstub.o` (its `main()` writes the embedded program out) and checks the program is intact (all byte values), the ELF file, the dynamic linker and libc, error messages (missing runtime files, libraries, linker, unwritable output), that no work files are left behind, and (when `engine1999.o` and `engine.lib` are there) links the real runtime with SDL2 and asks it for `--engine1999-about`; and (with `engine1999c.o`) builds a real program into a console executable without SDL2 and runs it (arguments, input, exit status); exit status = number of failures. Needs `ld` and the C library development files. |
| `txttest1` | no | **Automated** test of the text screen (`textscreen.c`): viewports (write clipping, wrapping, scrolling), the two screens, blank colors, rendering at the viewport position; exit status = number of failures |
| `keytest1` | no | **Automated** test of the key sequences (`keyseq.c`, `sdlkeys.c`): the canonical sequences of the cursor and function keys with all modifiers, make -> parse round trip, the dialects terminals send, prefixes, garbage, names, the mapping of SDL key codes (no display needed); exit status = number of failures |
| `ansitest1` | no | **Automated** test of the ANSI/SGR decoder (`ansi.c`): feeds texts with escape sequences and compares the text runs and colors handed to the callback; exit status = number of failures |
| `testcomp1` | no | Randomized stress test for `varmem_t` (despite the name, it tests variables/strings, not the compiler): 5000 random create/delete/read/write operations checked against a shadow model; exit status 0 = passed |

There is no automated test runner or CI. Typical manual check of the BASIC front end:

```
printf '10 PRINT "HI";A$:LET B=1+2*3\n20 GOTO 10\n' | ./basictest2
```

(`basictest*` read until EOF.) These test programs include `basic.h` and exercise `bastok.c` and the compiler; they read arbitrary-length lines with `getline`. Run under `valgrind --leak-check=full` to check the buffer code.

## Installing, the kit, the man pages

`make install` installs into `PREFIX` (`/usr/local` by default; `make install PREFIX=$HOME/.local`, `DESTDIR=/tmp/pkg` for packagers) and builds first what it needs: `make kit` compiles everything again into `kit/` **without `-march=native` and without the profiling hook `-p`** (so the installed programs and the executables that `BUILD` makes from the installed kit run on other CPUs and carry no profiling code; the development build in the source directory keeps its flags). Installed:

| Where | What |
|---|---|
| `$(PREFIX)/bin` | `engine1999`, `engine1999c` |
| `$(PREFIX)/lib/engine1999` | the kit of `BUILD`: `engine1999.o`, `engine1999c.o`, `engine.lib` |
| `$(PREFIX)/share/engine1999/demos` | the demo programs |
| `$(PREFIX)/share/doc/engine1999` | README, LICENSE, LICENSE-EXCEPTION, `docs/` (with `grammar.html`) |
| `$(PREFIX)/share/man/man1`, `man7` | `engine1999(1)`, `engine1999c(1)`, `engine1999-basic(7)` (the `@VERSION@` of the sources is filled in) |

`make uninstall` removes them again. `BUILD` finds an installed kit on its own: it looks in `../lib/engine1999` next to the running program, so any prefix works (`~/.local/bin/engine1999` finds `~/.local/lib/engine1999`); a copied kit goes into `ENGINE1999_HOME`. The kit needs the system linker (`ld`, binutils) and the development files of the C library on the machine that runs `BUILD`, and SDL2 for window executables.

**Man pages** (`man/engine1999.1`, `man/engine1999c.1`, `man/engine1999-basic.7`) are written by hand and kept current by `make man-check` (`mancheck.sh`): it fails when an option that `engine1999 --help` or `engine1999c --help` lists is missing from the page of that program, when the list of reserved words in the language page is not `engine1999c --list-keywords` (`sh mancheck.sh --update` rewrites that list), and when groff finds a mistake. Read an uninstalled page with `man -l man/engine1999.1`.

## Version

The version is the newest git tag of the form `vMAJOR.MINOR.BUILD` (`./bump_version.sh` makes it; `--show` prints the current one, `--dry-run` shows what would happen; it only creates a local tag, push it yourself). `make` runs `tagtoverstr.sh`, which writes `version.h` (`#define ENGINE_VERSION "..."`, git-ignored, only `engine1999.o` includes it): `v0.5.0` when HEAD is the tagged commit, `v0.5.0-3-g1a2b3c4` three commits after it, `untagged-1a2b3c4` before the first tag, `unknown` outside a git repository (an existing `version.h` is kept then, so a source archive can ship one). The header is regenerated when `.git/HEAD`, `.git/logs/HEAD`, the tags or `packed-refs` change and only rewritten if the string differs; since a commit changes the version, the next `make` recompiles `engine1999.o` and links `engine1999` again (nothing else). The version shows in the start banner, in `engine1999 --version` and in `--engine1999-about` of a built program (the version of its runtime). Shell instead of perl on purpose: perl is not needed to build.

## Font and colors

`perl grammar.pl` (or `make grammar`) reads `bascomp.ebnf` and writes the quick reference `docs/grammar.md` and `docs/grammar.html`; `make grammar-check` (`perl grammar.pl --check`) fails if a grammar rule is used but not defined or if the two files are out of date. Not part of `make all` (it needs perl); run it whenever `bascomp.ebnf` changes.

`perl fontcomp.pl` reads `8x12font1.txt` and writes `8x12font1.c` (it is not wired into `make`; output is written directly to `8x12font1.c`, overwriting it).
`colors-*.txt` are git-ignored scratch/generated data for the palette.

## Gotchas

- Profiling is optional: `make PROFILE=1` adds `-p`, and every run then writes `gmon.out` (ignored by git). Without it no `gmon.out` appears and `vmtest1` can run under valgrind directly.
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
| `--debug-keys` | `BASDBG_KEYS` | `[keys]` what the keyboard delivers (not part of `--debug-all`): see below |

**`--debug-keys`** (the keyboard): `engine1999` prints one line per SDL event to stdout, e.g. `[keys]     3216 ms  SDL key down sym=0x40000052 "Up" scancode=82 mods=0x0001(LShift)  ->  1b 5b 31 3b 32 41  "\e[1;2A"  = Shift+Up` (the sequence is the one `sdlkeys_sequence()` makes, so it shows what a program will receive; modifier keys alone have none), key releases, and the text the keyboard layout makes (`SDL text input  c3 a4  "\xc3\xa4"` for `ä`). `engine1999c --debug-keys` runs no BASIC but dumps the bytes of the terminal: after each key press `N bytes, T ms after the previous key press` and one line per item (hex, C string, what it is, e.g. `Shift+Up (canonical)` or `unknown escape sequence`); Ctrl-D or the end of the input stops it; a pipe works, too (`printf '\033[1;2A' | engine1999c --debug-keys`). It is for finding out what a terminal sends before the console host's key input is written (roadmap section 4).

`engine1999 --help` lists them. The tokenize/detokenize/tree dumps are produced by hooks in `repl_enter()` / `repl_execute()`;
the parse and code output comes from the compiler and the code generator themselves, so any host gets it by setting the flags.
`vmtest1 -v` turns on parse + code output, `repltest1 -v` everything; `basictest2` always sets `BASDBG_PARSE` (it has its
own, colored syntax tree printer).

