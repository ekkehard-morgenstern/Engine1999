# Session state

Maintained by Claude; updated whenever work in a session changes something. Last update: 2026-10-27 (INPUT/GET added).
Read this first when resuming; details are in `docs/` (start with `docs/README.md`) and `CLAUDE.md`.

## Standing rules
- **Never run git write commands** (commit, add, push, merge, rebase, reset, checkout of files, stash, tag, branch changes). The user does all of these manually, even if git access is authorized later. Read-only git (`status`, `log`, `diff`, `show`) is fine.
- Keep this file and `docs/` current when work changes the state.

## Repo state
- Branch `feature/refactor-to-remove-some-limits`; latest commit `c5c2214` ("DATA/READ/RESTORE statements").
- **Uncommitted (for the user to commit):** the INPUT/GET work — `bascomp.[ch]`, `bascomp.ebnf`, `cgendecl.c`, `codegen.[ch]`, `instset.h`, `nodetype.[ch]`, `vm.[ch]`, `vmtest1.c`, `docs/basic-language.md`, `docs/compiler-and-vm.md`, this file. It builds cleanly and all tests pass.
- `make` builds `sdltest1 engine1999 basictest1 testcomp1 basictest2 vmtest1` + `engine.lib` cleanly with `-Wall -Werror`.

## Tests (run these after any change)
- `./vmtest1` — automated end-to-end test (BASIC text → tokens → syntax tree → code → VM → output). **308 cases, all pass**; exit status = number of failures; `-v` shows compiler/codegen debug output. Add a case for every new statement/instruction. Also run it under `valgrind --leak-check=full -q` (clean).
- `./testcomp1` — randomized stress test of the variable store (5000 operations vs a shadow model), exit status 0 = passed; takes ~3 s. (Its random names used to be sometimes empty, which made it flake; fixed.)
- `basictest1` / `basictest2` — interactive tokenizer / parser tools (pipe lines into them). `basictest2` prints the syntax tree.

## What exists (the whole pipeline works)
- **INPUT and GET** are implemented (`INPUT [#ch,] ["prompt"(;|,)] vars`, `GET [#ch,] vars`; instructions `INPL 0x134, INPN 0x135, INPS 0x136, INPE 0x137, GETK 0x138`; host callbacks `vm->input` (line) and `vm->getkey`; see `docs/basic-language.md` and `docs/compiler-and-vm.md`).
- **DATA/READ/RESTORE** are implemented (parser, `cgendecl.c`, instructions `RDNM` 0x117, `RDST` 0x11E, `RSTR` 0x11F; see `docs/compiler-and-vm.md`).
- **Tokenizer** (`bastok`): heap strings, growable buffers, identifiers/literals ≤ 65535 bytes; line numbers 0..999999 (24 bit); `linehdr_t` = `{lineno.24, length.16}` (`LINEHDR_SIZE` = 5). Keyword `AGAIN` (token 145) was added.
- **Program store** (`baspgm`/`baslin`): doubly linked list of heap `line_t` nodes; `save_program()` (ascii listing or binary encoded lines; no signature, no loader).
- **Compiler** (`bascomp`): recursive descent → heap syntax tree (`node_t`, growable branch arrays, `NODE_NONE`); all nodes freed by `cleanup_compiler()`. Names via `comp_node_name()` (no length limit).
- **Variables** (`vars`): heap `var_t` per variable, found by 16-bit variable number (table) or by type+name (hash). No fixed areas or compaction; ≤ 65535 variables; arrays row-major, ≤ `MAXELEMS` elements, one dimension ≤ 65535, ≤ 6 dimensions; function variables hold params + code offset.
- **Code generator**: `codegen.c` (expressions, LET, PRINT, SAVE, substring assignment, ERASE, program/line driver), `cgenctl.c` (GOTO/GOSUB/RETURN/LABEL, IF/UNLESS, FOR/NEXT, REPEAT/UNTIL, WHILE/WEND, UNTIL/UEND, AGAIN, END, POP, ON; line table + fixups; compile-time loop stack), `cgendecl.c` (DIM, DEF FN single- and multi-line, `FN A = expr`).
- **VM** (`vm.[ch]`, minimal executor): all instructions except `WRLV` and `BRK`; system functions (TI, INKEY$, FRE, ASC, LEN, VAL, STR$, BIN$/QUA$/OCT$/HEX$/DEC$, LEFT$/RIGHT$/MID$), SHEX (popen), SAVE (host callback `vm->save`), function calls with saved parameter contexts. Policies the ISA leaves open are documented in the comment at the top of `vm.h`.
- **Every statement in `bascomp.ebnf` has code generation.** The removed `basintp` interpreter stub is gone (the system compiles, it doesn't interpret).

## ISA changes made (instset.h is up to date)
- New `RDNM` (0x117), `RDST` (0x11E), `RSTR` (0x11F) for DATA/READ/RESTORE (item records chained in the data segment). New `ERAS` (0x116): resets an array. `SLFT/SRGT/SMID` take the variable number as parameter and `( s n inx … ndim -- )` / `( s start len inx … ndim -- )` from the stack (ndim 0 = plain variable).

## Design decisions I took (revisit if unwanted)
- FOR/NEXT, REPEAT/UNTIL, WHILE/WEND, UNTIL/UEND are matched at compile time with a loop stack; FOR/WHILE/UNTIL test before the first pass. **An `UNTIL` closes the innermost loop if that is a REPEAT, otherwise opens an UNTIL..UEND loop** (so an UNTIL loop can't be opened directly inside a REPEAT body). `AGAIN [n]` restarts the body of the n-th innermost loop without re-testing/stepping. `ON` truncates the fraction and falls through when out of range. `END` inside a function ends the whole program.
- ERASE resets the array (arrays exist from parse time; no re-dimensioning). Multi-line DEF FN returns what was last assigned with `FN A = expr` (hidden variable `@FN<name>`; default 0 / empty string).
- Functions must be defined before they are called in the text, can't call themselves, and need a space: `DEF FN A(X)=…`, `FN A(…)` (the tokenizer reads `FNA` as one identifier). Array parameters aren't supported.
- DATA items are numeric literals (signed) and string literals only; they live in the 64 KiB data segment; `RESTORE n` means the first item in line ≥ n; a program run (`vm_run`) starts at the first item, `vm_run_at` keeps the pointer (so a persistent REPL VM keeps its data pointer across lines).
- The syntax tree stores the variable number (not a `var_t*`) in node data bytes.

- INPUT: prompt must be plain string literal(s); `;` adds "? ", `,` doesn't. Bad/missing field → "?Redo from start" (fields already stored stay stored), extra fields → "?Extra ignored", empty numeric field = 0. GET waits for a key (numeric variable gets the key code). INPUT/GET without `#ch` emit `PCHN 0`.
- **Found, not fixed:** `PRINT` without `#ch` doesn't reset the VM's output channel, so after `PRINT #1,…` later plain `PRINT`s keep going to channel 1 (the channel set by `PCHN` is sticky). Probably `PRINT` should also emit `PCHN 0` when there is no channel specifier.

## Planned by the user for later (all through host callbacks)
- `AFTER` (one-shot timer, like setTimeout), `EVERY` (recurring timer, like setInterval), `SYMBOL` (redefine one character of the character set).

## Open / next steps
- No grammar/semantics yet (each needs a language design): `PUT`, `OFF`, `AFTER`, `EVERY`, `SYMBOL`, `OPTION BASE`, `INT`/`STR`/`FLT`, and the program commands `RUN`, `NEW`, `LIST`, `LOAD`, `DELETE`, `RENUM`, `CLEAR`, `EDIT`, `MERGE`, `CHAIN`, `FILES`. 
- VM instructions not implemented: `WRLV`, `BRK`.
- Host side: `engine1999.c` doesn't use the compiler/VM yet (nothing wires `vm->save`, `vm->inkey`, `vm->input`, `vm->getkey`, `vm->print` to the real screen/files); direct mode can't `GOTO` into a stored program and its code must persist between lines (the test harness appends direct lines and runs from the new start offset with `vm_run_at()`); a loader for the binary SAVE format (with a signature) doesn't exist.
- Remaining limits: 64 KiB code/data segments (16-bit ISA addresses), 16-bit variable numbers, 16-bit `LINE` parameter (codegen rejects line numbers > 65535), `engine1999` 256-byte input line, VM strings ≤ 65535 bytes.
