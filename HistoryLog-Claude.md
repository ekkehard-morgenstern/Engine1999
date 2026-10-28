# HistoryLog-Claude

Log of the changes Claude (Claude Code) made to this repository, one entry per commit.
The user makes all commits, so an entry is first written under **Pending** and gets its
commit ID and timestamp once the commit exists (see "Maintenance" in `CLAUDE.md`).

- A commit belongs here if its message contains "Claude" or "Claude Code" (`git log -i --grep=claude`).
- Timestamps are the commit's author date (local time of the committer, as `git log --date=iso` shows it).
- Stats are `git show --shortstat`. Details come from the commit diffs and from the `SESSION.md` changes in the same commit.
- Entries are in chronological order (oldest first). The entries up to `f5301e2` were reconstructed afterwards from git history; later ones are written while the work is done.

Related files: `SESSION.md` (current state), `docs/roadmap.md` (open work), `docs/refactoring.md` (refactoring notes).

## Pending (not committed yet)

- 2026-10-28: Created this file, `HistoryLog-Claude.md`, and added the maintenance rule to `CLAUDE.md` (and a pointer in `SESSION.md`). No source changes.

## Log

### 7a6cb67 — 2026-10-17 13:32:27 +0200 — "Introduced Claude to the project"
10 files, +414/-0.
- Read the history and code; wrote `CLAUDE.md`, `SESSION.md` and `docs/` (README, architecture, display-engine, basic-language, compiler-and-vm, build-and-test, refactoring).
- Documented the refactoring goal, the remaining checklist and 11 known defects in the half-converted `bastok.c`. `.gitignore` got build artifacts.
- No source files changed; the build was still broken mid-refactor (`bastok.o`).

### 64b4f1d — 2026-10-17 14:04:02 +0200 — "Bastok refactoring by Claude Code"
17 files, +814/-584.
- Completed the tokenizer refactor: `bastok.[ch]` uses heap strings (`char**`), growable output buffers (`buf_t`) and bounded read buffers (`rbuf_t`); identifiers/literals up to 65535 bytes (2-byte length prefix in the token stream).
- Converted all callers: `bascomp`, `baspgm`, `basintp`, `basictest1`, `basictest2`, `stdtypes.h`. Docs updated.
- Verified: clean `-Wall -Werror` build; round trips with 3000-byte identifiers and 5000-byte string literals; oversize/unterminated literals rejected; valgrind clean.

### fceddbe — 2026-10-17 14:08:45 +0200 — "Claude session state"
1 file, +1/-2.
- `SESSION.md`: recorded that the refactor was committed as `64b4f1d` and the tree is clean.

### b453dd4 — 2026-10-27 11:06:59 +0100 — "Changed line numbers to 24 bits using Claude Code."
13 files, +93/-62.
- Line numbers widened to 24 bit: `linehdr_t.lineno` is `uint32_t`, encoded in 3 bytes (`emit/read_uint24`). `LINENO_MAX` = 999999, `LINENO_NONE`/`LINENO_DEL` = 16777215. `NT_GOTOTARGET` data is 4 bytes, `NT_STMTLINE` data 3 bytes.
- Touched `bastok`, `baslin`, `baspgm`, `bascomp`, `nodetype.h`, test tools, docs. Verified with `basictest1` (999999 accepted, 1000000 rejected).

### 8aa739b — 2026-10-27 11:16:36 +0100 — "Program 64K limit removed using Claude Code"
12 files, +164/-728.
- Program store (`baspgm`/`baslin`) rewritten as a pointer-linked list of heap `line_t` nodes (`program_t{first,last}`); the 64 KiB block, offset list, defrag and `linenode_t`/`linelist_t` are gone. `linehdr_t` is just the encoded header `{lineno.24, length.16}` (`LINEHDR_SIZE` = 5). New `free_program()`; `find_line` returns bool + prev/line pointers.

### 3f0438a — 2026-10-27 11:22:20 +0100 — "Basintp module removed using Claude Code"
10 files, +13/-213.
- Removed the `basintp` interpreter stub (the system compiles, it doesn't interpret): files, makefile rules, docs, CLAUDE.md table row.

### a494fc2 — 2026-10-27 11:42:47 +0100 — "Removed 64K syntax tree size limit using Claude Code"
10 files, +984/-1024.
- Syntax tree (`bascomp`) moved from a 64 KiB byte arena to heap `node_t` nodes with growable branch arrays (`NODE_NONE` = null); `compiler_t.nodes` chains all nodes, freed by `cleanup_compiler()`/`comp_free_tree()`. `codegen.[ch]` and `basictest2.c` converted.
- Fixed a latent bug in the base variable-reference node data layout (`<type> <name>`, no length byte).

### cb7178a — 2026-10-27 12:10:24 +0100 — "Minimal VM and test harness created using Claude Code"
12 files, +1136/-17.
- New minimal VM `vm.[ch]` and the automated end-to-end test `vmtest1.c` (32 cases at that point, valgrind clean; exit status = failures).
- Codegen got a statement/line/program driver (`cgen_from_stmt/stmtlist/stmtline/stmtlines/program`) for LET/PRINT/SAVE; an empty PRINT now emits a newline.
- VM policies chosen by Claude as placeholders (comparison = -1, number format `%.15g`, comma zones of 10), documented in `vm.h`. Makefile gained `vmtest1`.

### 50b9030 — 2026-10-27 12:20:33 +0100 — "VM control flow instructions, created using Claude Code"
13 files, +740/-22.
- New `cgenctl.c`: GOTO/GOSUB/RETURN (line table + patched BRIA fixups), LABEL, single- and multi-line IF/UNLESS, FOR/NEXT (compile-time matching, hidden limit/step variables, pre-test). `vmtest1`: 70 cases.
- Parser fixes: IF nodes lacked the condition branch.
- Design decisions: FOR/NEXT matched lexically at compile time (no run-time FOR stack); FOR tests before the first pass; direct mode can't GOTO stored lines.

### 77c46c5 — 2026-10-27 12:31:59 +0100 — "VM array and DEF FN handling, created using Claude Code"
13 files, +557/-66.
- VM: RNAE/RIAE/RSAE/WNAE/WIAE/WSAE (arrays), CALF/RETF (functions, contexts kept in the VM), `vm_run_at()`. New `cgendecl.c` (DIM, single-line DEF FN). `vars.[ch]`: `vmem_get_func_info/param`. `vmtest1`: 99 cases.
- Fixed `comp_gather_branches` dereferencing `NODE_NONE` for `DEF FN` without parameters.
- Open questions recorded: `FN A` needs a space, ERASE, multi-line DEF FN, persistence of direct mode code.

### fb33b95 — 2026-10-27 12:46:37 +0100 — "VM system statements, created using Claude Code"
8 files, +431/-8.
- System functions in the VM: TI, INKEY$, FRE, ASC, LEN, VAL, STR$, BIN$/QUA$/OCT$/HEX$/DEC$, LEFT$/RIGHT$/MID$ (`vm->inkey` callback).
- SHEX (popen) and SAVE (host callback `vm->save`); `save_program()` in `baspgm.c` (ascii or binary); fixed `cgen_from_savestmt` not NUL-terminating an empty option string. `vmtest1`: 154 cases.
- Open: substring assignment (SLFT/SRGT/SMID) as specified can't work; no loader for the binary SAVE format.

### 2052cbb — 2026-10-27 13:26:21 +0100 — "variable system overhaul, created using Claude Code"
18 files, +534/-782.
- `vars.[ch]` rewritten with heap variables (`var_t`, pointer table by variable number + hash by type/name). Removed the 64 KiB areas, compaction, string space, 1024-variable and 255-byte name limits. Variable numbers stay `uint16_t`.
- Fixed array aliasing in 2+ dimensions (stride was always 1); arrays are row-major now; `testcomp1` model fixed.
- `vmtest1`: 158 cases (3000-char name, 2000 variables, 300x300 array).

### 8322e7d — 2026-10-27 13:36:58 +0100 — "Code generation for remaining syntax-defined statements, created using Claude Code"
18 files, +430/-119.
- Every statement of `bascomp.ebnf` now has code generation: ERASE (new instruction `ERAS` 0x116, `vmem_erase_array`), substring assignment (`SLFT/SRGT/SMID` redefined to take the variable number as parameter), multi-line DEF FN (result through `FN A = expr`, hidden variable `@FN<name>`).
- `instset.h` updated; `vmtest1` extended.

### c7e5428 — 2026-10-27 13:57:22 +0100 — "Remaining loop constructs, created using Claude Code"
16 files, +730/-42.
- New keyword `AGAIN` (token 145). Parser + codegen for END, POP, AGAIN [n], REPEAT/UNTIL, WHILE/WEND, UNTIL/UEND, ON n GOTO|GOSUB (nodes 0x4E..0x56). A compile-time loop stack (`cgen_loopctx_t`) replaces the FOR-only stack.
- Design decisions: UNTIL closes the innermost loop if it's a REPEAT, else opens an UNTIL..UEND loop; AGAIN doesn't re-test/step; ON truncates the fraction and falls through when out of range; END inside a function ends the program.
- `testcomp1` flake fixed (random variable names could be empty). `vmtest1`: 251 cases.

### d554d0d — 2026-10-27 14:00:18 +0100 — "Claude session state updated"
1 file, +36/-50.
- `SESSION.md` restructured: per-change "Done this session" lists replaced by a state description (repo state, tests, what exists, design decisions, open steps).

### c5c2214 — 2026-10-27 14:11:00 +0100 — "DATA/READ/RESTORE statements, created using Claude Code"
16 files, +519/-17.
- Parser, `cgendecl.c`, instructions `RDNM` 0x117, `RDST` 0x11E, `RSTR` 0x11F; items chained in the data segment; numeric (signed) and string literals only; `RESTORE n` = first item at line ≥ n. `vm_run` starts at the first item, `vm_run_at` keeps the pointer. `vmtest1`: 281 cases.

### 7e16a40 — 2026-10-27 20:18:39 +0100 — "INPUT, GET, etc., created using Claude Code"
15 files, +652/-12.
- `INPUT [#ch,] ["prompt"(;|,)] vars` and `GET [#ch,] vars`; instructions `INPL 0x134, INPN 0x135, INPS 0x136, INPE 0x137, GETK 0x138`; host callbacks `vm->input`, `vm->getkey`. Bad field → "?Redo from start", extra fields → "?Extra ignored". `vmtest1`: 308 cases.
- Found, not fixed: PRINT without `#ch` left the sticky output channel as it was.

### 5a7f6b3 — 2026-10-27 20:25:22 +0100 — "PRINT output channel fix, created using Claude Code"
6 files, +28/-13.
- PRINT, INPUT and GET always emit `PCHN` (0 without `#ch`) via `cgen_gen_channel0()`. `vmtest1`: 309 cases.

### 8a98b3f — 2026-10-27 20:30:26 +0100 — "CHR$() function, created using Claude Code"
13 files, +47/-15.
- `CHR$(n)`: token `CHR` 146, instruction `CHRS` 0x126; n must be 1..255 else "Illegal argument" (strings are NUL-terminated). `vmtest1`: 317 cases.

### 41e9ed4 — 2026-10-27 20:54:32 +0100 — "Engine wiring, created using Claude Code"
17 files, +905/-38.
- New `basrepl.[ch]` (host-independent REPL: direct mode, program lines, RUN/LIST/NEW) and `repltest1.c` (36 cases). `engine1999` runs the REPL in the main thread; the SDL screen thread only renders and queues keys.
- Callbacks wired: print → `sdlscr_puts`, INPUT → `sdlscr_lineinput`, GET → `sdlscr_getkey`, INKEY$ → `sdlscr_pollkey`, interrupt → `sdlscr_breakrequested`, SAVE → `repl_save_program`. ESC = break. `sdlscreen`, `sdlevent` extended.
- Verified on the real screen under Xvfb with injected keys (scripts only in the session scratchpad).

### ff53541 — 2026-10-27 21:04:47 +0100 — "Debug options (implemented using Claude Code)"
15 files, +350/-88.
- New `basdebug.[ch]`: flags `--debug-tokenize | -detokenize | -parse | -tree | -code | -all`, `--help`; all off by default. The unconditional `*** TOKEN` / disassembly output in `bascomp.c`/`codegen.c` is gated by them. `vmtest1 -v` and `repltest1 -v` enable them; the fd-redirect silencing in the tests was removed.

### 761339e — 2026-10-27 21:09:20 +0100 — "the road ahead (recorded using Claude Code)"
4 files, +93/-7.
- New `docs/roadmap.md` with the full list of open work; `SESSION.md` now points to it.

### f5301e2 — 2026-10-27 21:10:43 +0100 — "Claude session backup"
1 file, +1/-1.
- `SESSION.md`: repo state line updated to `761339e`.
