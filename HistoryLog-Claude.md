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

- **I/O subsystem with virtual device drivers, phase 1** (owner's request, decisions of 2026-10-30: unknown prefix is an error, random access later with new keywords, `LOAD`/`SAVE`/`MERGE`/`CHAIN`/`FILES` through the drivers, `ADDRESS` reset by `RUN`, `\n` written and `\r\n` read, sandboxing left open).
  - **Design first**: `docs/io-subsystem.md` (goals, concepts, name resolution, statements, drivers, phases, decisions, the open sandboxing issue with the questions and the `iodev_allowed()` hook) and a roadmap section 8 / `docs/README.md` line, before any code.
  - **`iodev.[ch]`** (new, no SDL, no VM; `makefile`: object, header, rule): driver registry and `iodev_resolve()`; drivers `file` (stdio, `files_list()` for `list`, `unlink` for `remove`), `null`, `mem` (RAM disk, reference counted so a deleted open file stays readable); `iodev_read_all/write_all/exists/list`; `iotab_*` channel table (1..255, read buffer, `LOC`, `LOF`, `EOF`, lines with `\r\n` stripped, "File not open" / "Bad file mode" / "Input past end" ...). `basfiles.[ch]`: `files_format_entry()` split off for the mem driver; `FILES` now sorts with nanosecond times (the earlier "st_mtim is not available" claim was wrong: `stdtypes.h` defines `_POSIX_C_SOURCE 200809L`; docs and `CLAUDE.md` corrected).
  - **Language** (`bascomp.[ch]`, `.ebnf`, `bastok.[ch]`, `nodetype.[ch]`, `codegen.[ch]`, `cgenprog.c`, `cgendecl.c`, `instset.h`): keywords `OPEN CLOSE LINE WRITE ADDRESS EOF LOF LOC` (162..169); `OPEN` in both forms (`OUTPUT`, `APPEND`, `AS` recognized in context), `CLOSE`, `ADDRESS` (bare name, string expression, or alone = swap), `LINE INPUT` (a flag of `NT_INPUTSTMT`), `WRITE` (a flag of `NT_PRINTSTMT`, `PRTARG_WRITE`), `PRINT #n` / `WRITE #n` without a comma, functions `EOF LOF LOC INPUT$ ADDRESS$`; nodes `0x6B..0x6D`, instructions `OPEN CLOS INPW EOFN LOFN LOCN INPD ADDR ADRS` (`0x14C..0x154`).
  - **VM** (`vm.[ch]`): `vm_t.io`, `addrcur` / `addrprev`, `iofail`, `inerr`; `vm_out()` / `vm_get_line()` / `vm_get_key()` use the channel table for channels above 0 (no prompt, "Bad data in file", `INPUT #` reads on with the next line, no "?Extra ignored"); a new VM starts with `file` and `cleanup_vm()` closes the channels.
  - **REPL** (`basrepl.c`): `LOAD`, `MERGE`, `CHAIN` read through `iodev_read_all()` + `fmemopen()` (`repl_open_program()`), the extension search uses `iodev_exists()`, `SAVE` writes through `open_memstream()` + `iodev_write_all()` (and prints the reason), `FILES` calls `iodev_list()`; the old `repl_is_file()` is gone.
  - **Tests**: new `iotest1` (81 checks, in `make` and `.gitignore`), `vmtest1` 612 (+62: both `OPEN` forms, append, `INPUT #`, `LINE INPUT`, `WRITE`, `EOF`/`LOF`/`LOC`, `GET #`, `INPUT$`, all the errors, `CLOSE` forms, `ADDRESS`, parse errors; two old tests changed from the host's channel callback to `null:`), `repltest1` 273 (+26: `SAVE`/`LOAD`/`MERGE`/`CHAIN`/`FILES` through `mem:`, `ADDRESS`, channels over direct lines, `RUN`/`NEW` closing channels); `vmtest1`, `repltest1` and `iotest1` are clean under valgrind. `mem:` is cleared per test (`iodev_mem_clear()`).
  - **Docs**: `docs/io-subsystem.md` (status, "What phase 1 does, exactly"), `docs/basic-language.md` (new section), `docs/compiler-and-vm.md`, `docs/roadmap.md`, `bascomp.ebnf`, `CLAUDE.md`, `SESSION.md`.

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

### d951a9c — 2026-10-28 04:45:30 +0100 — "Introduced history log for Claude Code changes (created by Claude Code)"
3 files, +138/-0.
- Created this file, reconstructed from git history (23 commits up to `f5301e2`), with a Pending section for uncommitted work.
- `CLAUDE.md`: added the rule to keep the log updated (pending entries per change; move them into the log with commit ID and timestamp after the user commits).
- `SESSION.md`: added the same rule to the standing rules. No source changes.

### fa623a3 — 2026-10-28 04:47:10 +0100 — "Claude History Log updated"
1 file, +7/-1.
- `HistoryLog-Claude.md`: moved the pending entry for the history log itself into the log as `d951a9c`. No source changes.

### f6d83bf — 2026-10-28 05:11:21 +0100 — "Binary SAVE with header, LOAD, RUN and LIST as true BASIC statements (created by Claude Code)"
29 files, +983/-88.
- Binary `SAVE` files start with a 5-byte header: magic `89 45 39 39` + format version 1 (`PGMFILE_*` in `baspgm.h`); `read_program_header()` detects it (rewinds on a text file, rejects unknown versions / truncated headers).
- `LOAD name$`, `RUN [line|label]` and `LIST [a][-[b]]` became language statements usable in programs (new `cgenprog.c`; nodes `0x5C..0x5E`; instructions `LOAD 0x139`, `RUNP 0x13A`, `LIST 0x13B`). `LOAD`/`RUN` can't replace the running code, so the VM stores a request (`vm->request`) and ends the run; `repl_finish()` in `basrepl.c` carries it out in a loop (replace/recompile/restart; Break polled between chained restarts). `LIST` uses the new callback `vm->list`; labels resolved to lines by `find_label_line()`. `LOAD` reads binary files and text listings into a scratch program that replaces the old one only on success.
- The host level `RUN`/`LIST` in `repl_enter()` were removed. `vmtest1` 346 and `repltest1` 65 cases; docs, `CLAUDE.md` and `SESSION.md` updated.

### 05ca28f — 2026-10-28 05:18:49 +0100 — "NEW and CLEAR as host commands (implemented by Claude Code)"
25 files, +193/-50.
- **`NEW` and `CLEAR` are language statements** (the last host level command, `NEW` in `repl_enter()`, is gone; `repl_command()` removed).
  - Grammar/nodes: `new-stmt := NEW`, `clear-stmt := CLEAR`; `NT_NEWSTMT 0x5F`, `NT_CLEARSTMT 0x60`; code generation in `cgenprog.c`; instructions `NEW 0x13C`, `CLR 0x13D`.
  - `NEW`: third request kind `VMREQ_NEW`; the VM stops, `repl_finish()` calls `repl_new()` (program, variables, functions deleted) and starts nothing, so a program using `NEW` ends and the host returns to the prompt. `CLEAR`: done by the VM itself, `vmem_clear_vars()` in `vars.c`: numbers 0, strings "", arrays emptied (dimensions kept, because arrays exist from parse time); labels, functions and hidden compiler variables (`@…`, e.g. FOR limit/step) are skipped, so the program keeps running correctly; the DATA pointer is not touched. The control variable of a running FOR is a normal variable and is cleared (documented).
  - Tests: `vmtest1` 356 cases (+10), `repltest1` 74 cases (+9); both pass, valgrind clean (builds without `-p`, see the previous entry). Not tried on the real SDL screen.
  - Docs: `basic-language.md`, `compiler-and-vm.md`, `architecture.md`, `build-and-test.md`, `roadmap.md` (CLEAR can't cure the code growth of direct mode lines; noted), `CLAUDE.md`, `SESSION.md`, `basrepl.h`.

### 01133de — 2026-10-28 05:19:57 +0100 — "Claude History Log update"
1 file, +9/-5.
- `HistoryLog-Claude.md`: moved the pending `NEW`/`CLEAR` entry into the log as `05ca28f`. No source changes.

### 038f171 — 2026-10-28 05:24:14 +0100 — "Claude History Log and session state update"
2 files, +9/-5.
- `SESSION.md` brought up to date (latest commit, stale "no loader yet" remark, valgrind note: build without `-p`); `HistoryLog-Claude.md`: logged `01133de`. No source changes.

### fd7e324 — 2026-10-28 05:41:22 +0100 — "DELETE and RENUM as language statements (implemented by Claude Code)"
22 files, +640/-36.
- **`DELETE` and `RENUM` are language statements.**
  - Syntax: `DELETE a[-[b]]` (the `LIST` syntax, at least one end required) and `RENUM [a][-[b]][,step]` (range as for LIST, step a numeric expression, default 10; new numbers start at the first line of the range, 10 without a start; a single target means "to the end"). Shared parser helper `comp_eat_linerange()`; nodes `NT_DELETESTMT 0x61`, `NT_RENUMSTMT 0x62` (flag `LISTF_STEP`); code generation in `cgenprog.c`; instructions `DELE 0x13E`, `RENM 0x13F`.
  - VM/REPL: new callbacks `vm->deletelines`, `vm->renumber` (implemented in `basrepl.c`); after the callback the VM ends the run (the program changed, the code didn't). Errors: "Renumbering would overlap other lines", "Line number too large", "Illegal argument" (step < 1), "Line number or label expected" (bare DELETE).
  - `baspgm.c`: `delete_lines()` and `renumber_program()` (transactional: builds a new program; maps old to new numbers; `renum_text()` rewrites the line references in the detokenized text — GOTO/GOSUB/GO TO/GO SUB lists, THEN, ELSE, RESTORE, RUN, LIST/DELETE ranges — skipping strings, backtick literals and comments; unchanged lines keep their tokens, changed ones are tokenized again).
  - Bug found by valgrind and fixed: the buffer with the rewritten line text is not NUL-terminated (was passed to `snprintf` as a string); the plain test run didn't show it.
  - Tests: `vmtest1` 374 cases (+18: parsing, callbacks, ends the run, errors), `repltest1` 106 cases (+32: ranges, labels, all reference kinds, strings/comments untouched, overlap, overflow, in a program, SAVE/LOAD after RENUM); both pass; valgrind clean (builds without `-p`); `testcomp1` passes. Not tried on the real SDL screen.
  - Docs: `basic-language.md`, `compiler-and-vm.md`, `roadmap.md`, `bascomp.ebnf`, `instset.h`, `CLAUDE.md`, `SESSION.md`.
- Also in this commit: the log entry for `038f171` and the `SESSION.md` repo-state line.

### 2d7f59f — 2026-10-28 05:42:25 +0100 — "Claude History Log and session state update"
2 files, +13/-9.
- `HistoryLog-Claude.md`: logged `fd7e324`; `SESSION.md`: latest-commit line. No source changes.

### 01d467d — 2026-10-28 06:01:09 +0100 — "Code/Data size limit removal (step 1) (created by Claude Code)"
10 files, +221/-72.
- **Step 1 of lifting the 64 KiB code/data limits** (the limits themselves stay; they come from the 16-bit ISA, step 2 will lift them).
  - `codegen_t`: `code` and `data` are heap buffers that grow on demand (zero-filled, doubling from 4 KiB) instead of 128 KiB arrays inside the struct; `cgen_reset()` keeps them, `cleanup_codegen()` frees them.
  - Shared constants: new `cgen_data_const()` (hash table `consts` in `codegen_t`, FNV-1a, open addressing, cleared by `cgen_reset()`); used for number and string literals, SAVE options, INPUT prompts, the empty string, the channel-0 number, and the label names / line numbers pushed by LOAD/RUN/LIST/DELETE/RENUM. DATA item records are not shared.
  - New `cgen_push_number()`: integer literals from -65536 to 65535 (not negative zero) are generated as `PHIM` (3 bytes, no data) instead of `PHPA.D` + `LAN` + an 8 byte constant.
  - Effect, measured with generated programs of 1500..20000 lines before/after: data use fell from 40654 to 17 bytes (print/IF lines), 36757 to 13801 (assignments), 14176 to 8 (FOR loops); code only by ~5 % (the first "Out of code memory" moved from 830 to 874, 1457 to 1490 and 590 to 602 lines), so code stays the bottleneck and step 2 is still needed. The measuring program is a scratch file, not in the repo.
  - Tests: `vmtest1` 378 cases (+4: `PHIM` limits, fractions, repeated constants, and a 600-line program with a 200 character string that needs 120 KB of data without sharing); `repltest1` 106; both pass, valgrind clean (builds without `-p`); `testcomp1` passes.
  - Docs: `compiler-and-vm.md`, `roadmap.md` (step 2 described), `refactoring.md`, `SESSION.md`.

### 28da093 — 2026-10-28 06:10:54 +0100 — "Code/Data size limit removal (step 2) (created by Claude Code)"
21 files, +318/-234.
- **Step 2 of lifting the 64 KiB code/data limits: 32-bit code and data addresses.**
  - ISA (`instset.h`): the reserved bit 0x10 of the instruction byte is now the **W (wide) flag** (`INSF_W`): the parameter field has 32 bits instead of 16. Wide `PHIM` (32-bit signed immediate), `BRIA`/`BRIR` (32-bit address/offset); the return stack holds 32-bit addresses; `INS_NODATA`, `CODEOFFS_NONE`, `DATAOFFS_NONE` are 0xFFFFFFFF; DATA records are `type.8 next.32 payload` (`DATAREC_HEADER` = 5, `cgen_datarec_next/set_next`). The old `CODESIZE_MAX`/`DATASIZE_MAX` of 64 KiB became soft limits of 256 MiB.
  - Code generator: `cgen_gen_ins` takes a 32-bit parameter and makes an instruction wide by itself above 65535; `cgen_gen_ins12_wide()` forces it. Code addresses (`PHPA.C`, `BRIA`, `BRIR`, `RSTR` with a target) are always generated wide because they are patched later: new `cgen_patch_param()` replaces the byte patching in `cgenctl.c` (`cc_patch_*`) and `cgendecl.c` (function skip jump, RESTORE fixups). `cgen_alloc_code/data`, `cgen_data_const`, offsets in `cgenctl.c`/`cgendecl.c`/`cgenprog.c`, the DATA item table and `datafirst/datalast` are 32 bit. `cgen_gen_phim` is wide outside the 17-bit range, and `cgen_push_number()` now uses `PHIM` for every integer in the int32 range. `LINE` is wide above 65535, so the "Line number too large for code generation" error is gone. Disassembler (`cgen_disasm_ins`) shows wide parameters.
  - VM/vars: `vm_step` decodes wide parameters (`param` is 32 bit; instructions that carry variable numbers or counts reject a parameter above 65535 with "Parameter out of range"); return stack, `vm_rpush/rpop`, function call frames, `vmem_get_func_info`, `varvalue_t.lbloffs/codeoffs` and `var_t.v.codeoffs` are 32 bit; `testcomp1`'s shadow model adjusted (labels only ever hold "undefined").
  - Effect: generated 20000-line programs (1.6 / 0.9 / 2.3 MB of code, the limit used to be 64 KiB, i.e. ~600–1500 lines) compile and run. Code size grew by about 3 % because code addresses are always 5 bytes instead of 3.
  - Tests: `vmtest1` 380 cases (+2: 32-bit immediates, and a 2500-line program with 125 KB of code and 100 KB of data followed by far `GOTO`, `GOSUB`, `FOR`, `ON`, `DEF FN`, label, `DATA`/`RESTORE` and line numbers above 65535 incl. a run time error message); `repltest1` 106; `testcomp1`; all pass, valgrind clean (builds without `-p`). Not tried on the real SDL screen.
  - Docs: `instset.h`, `compiler-and-vm.md`, `architecture.md`, `refactoring.md`, `roadmap.md` (the two limit items removed), `CLAUDE.md`, `SESSION.md`.
- Also in this commit: the log entry and latest-commit line for `01d467d`.

### c45e514 — 2026-10-28 06:11:43 +0100 — "Claude History Log and session state update"
2 files, +13/-9.
- `HistoryLog-Claude.md`: logged `28da093`; `SESSION.md`: latest-commit line. No source changes.

### 7a6b4ae — 2026-10-28 06:19:27 +0100 — "Direct mode code pileup eliminated to some degree (implemented using Claude Code)"
11 files, +198/-13.
- **Direct mode code growth fixed: a direct line is rolled back after it ran.**
  - `repl_execute()` (`basrepl.c`) remembers the code and data size before a direct line and, when the line has run or failed, calls the new `cgen_rollback( cgen, codesize, datasize )` (`codegen.c`): sizes are reset, the freed bytes zeroed, the table of shared constants rebuilt without the entries in the discarded part. Not done for lines that stored something pointing into their code/data: `cgen_note_persistent()` (counter `codegen_t.persistent`) is called by `LABEL` (`cgenctl.c`), the end of `DEF FN` (the function variable gets the code offset, `cgendecl.c`) and each `DATA` record (`cd_add_data_item`). The counter also protects a line that fails after such a statement (e.g. `DEF FN B(X)=X+1:GOTO 99`: the function stays defined and callable).
  - Remaining growth (documented in `roadmap.md`): lines that define functions/labels/DATA keep their code, and a function defined again leaves its old code behind until `NEW`/`RUN`. The hidden loop variables (`@FOR1L`, ...) are looked up by name and reused, so they don't accumulate.
  - Tests: `repltest1` 112 (+6, a new block that checks `codesize`/`datasize` directly: 3000 ordinary lines, 300 failing lines of three kinds, 1000 lines after a `DEF FN`, a failing line behind a `DEF`, `DATA` surviving 500 lines, `RUN` after a long session); `vmtest1` 380; `testcomp1`; all pass, valgrind clean (builds without `-p`). Not tried on the real SDL screen.
  - Docs: `basrepl.h`, `compiler-and-vm.md`, `roadmap.md`, `SESSION.md`.
- Also in this commit: the log entry and latest-commit line for `c45e514`. The branch `feature/refactor-to-remove-some-limits` was merged into `main` after this commit (fast-forward); work continues on `main`.

### bcc157d — 2026-10-28 06:22:13 +0100 — "Claude History Log and session state update"
4 files, +13/-9.
- Branch references in `CLAUDE.md`, `SESSION.md` and `docs/refactoring.md` updated (feature branch merged into `main`); `HistoryLog-Claude.md`: logged `7a6b4ae`. No source changes.

### 3c0e106 — 2026-10-28 06:22:57 +0100 — "Claude History Log and session state update"
2 files, +6/-2.
- `HistoryLog-Claude.md`: logged `bcc157d`; `SESSION.md` updated. No source changes.

### 8854d23 — 2026-10-28 20:26:55 +0100 — "Minor fixes, created using Claude Code"
7 files, +34/-20.
- **No "Ready." after a program line; direct-mode `LOAD` no longer runs.** `handle_input()` (`engine1999.c`) returns false for lines starting with a digit, so the READY loop prints nothing after entering/deleting a program line. `repl_finish()` (`basrepl.c`) got a `direct` flag: a `LOAD` typed in direct mode loads, clears the runtime (`repl_reset_runtime()`) and stops; `LOAD` in a program still runs the loaded program. `repltest1`: 5 cases changed (added `RUN`/`PRINT A`), 1 added (113); `vmtest1`, `testcomp1` pass, `repltest1` valgrind clean. Docs: `basic-language.md`, `compiler-and-vm.md`, `SESSION.md`. Not tried on the real SDL screen.
- Also: `SESSION.md`: "Repo state" now names `3c0e106` as the latest commit (with `bcc157d` before it) and no longer mentions an uncommitted history-log entry; `HistoryLog-Claude.md`: logged `3c0e106`. No source changes, no tests run.

### da35e56 — 2026-10-28 21:00:44 +0100 — "ANSI SGR sequence output, created using Claude Code"
12 files, +491/-4.
- **ANSI SGR output function for the text screen.** New `ansi.[ch]` (pure decoder: `ansi_init()`, `ansi_feed()` with an emit callback; SGR colors only, other sequences skipped, state kept across calls, runs ≤ 255 chars); `sdlscr_ansi_puts()` in `sdlscreen.[ch]` calls `sdlscr_printf()` per run (no lock, as decided); new visual test program `sdltest2.c` (colored text through `sdlscr_ansi_puts()`: standard colors, 256-palette strip, split sequence, colored echo of typed lines; `makefile`, `.gitignore`, `build-and-test.md`); `makefile` (module, `ansitest1` target), `.gitignore`. New `ansitest1` (26 cases, valgrind clean). Docs: `display-engine.md`, `build-and-test.md`, `SESSION.md`. Not wired into `engine1999`'s `host_print`; `sdltest2` was checked on the real screen by the user (screenshots: colors, backgrounds, palette strip, split sequence, colored echo all correct).

### 8e9d6e2 — 2026-10-28 21:02:37 +0100 — "Claude Code history log and session update"
2 files, +11/-4.
- `HistoryLog-Claude.md`: logged `8854d23` and `da35e56`; `SESSION.md`: latest commit line. No source changes.

### 944b4dc — 2026-10-28 21:11:38 +0100 — "BASIC PRINT using ANSI output, and sdlscr_ansi_printf() function (both created by Claude Code)"
7 files, +33/-11.
- **`host_print` → `sdlscr_ansi_puts`; new `sdlscr_ansi_printf()`.** `engine1999.c`: program output on channel 0 now goes through the ANSI decoder (error messages via `host_report` and `Ready.` unchanged). `sdlscreen.[ch]`: `sdlscr_ansi_printf( fmt, … )` (1 KiB stack buffer, heap buffer for longer output, no truncation); `sdltest2.c` uses it for the colored echo. Docs: `display-engine.md`, `SESSION.md`. Tests: `repltest1`, `ansitest1` pass; build clean. Checked on the real screen by the user (`engine1999`, `sdltest2` work as expected).

### ee0b676 — 2026-10-28 21:18:52 +0100 — "Created residue subfolder for some things; Updated Claude Code log and session info"
5 files, +11/-3.
- The user moved the two ANSI test screenshots and `foo.bas` (a binary `SAVE` test file) into `residue/` (kept, not needed). The commit also carried the log entries for `8e9d6e2`/`944b4dc` and the `SESSION.md` update that Claude wrote. No source changes.

### 4ca5c10 — 2026-10-28 21:22:32 +0100 — "Claude Code bookkeeping files updated"
3 files, +11/-6.
- `CLAUDE.md`: added a "Residue" row to the "Where things are" table: `residue/` holds disposable leftovers and is to be ignored; also updated the stale rows/lists: `ansi.[ch]` in the display engine row, `sdltest2.c`/`ansitest1.c` in the executables row, the `make` target list and the automated tests in "Build". No source changes.
- Also: the entry for `ee0b676`; `SESSION.md`: latest commit line.

### 7861f6b — 2026-10-28 21:26:30 +0100 — "Claude Code bookkeeping files updated"
2 files, +8/-3.
- `SESSION.md`: the ANSI bullet rewritten (it had contradictory leftovers from earlier edits) and the xterm-palette question closed by the user's decision: `38;5;n` / `48;5;n` keep indexing the engine's own palette. No source changes.
- Also: the entry for `4ca5c10`.

### b984b8b — 2026-10-28 21:35:21 +0100 — "Syntax highlighting for LIST output, created using Claude Code"
11 files, +213/-18.
- **Optional syntax highlighting for LIST.** `bastok.[ch]`: `hlclass_t` (`HL_*`), `hlstyle_t`, `detokenize_line_hl()` (`detokenize_line()` is now a wrapper with no style); token → class tables `keyword_hlclass()` / `sngchr_hlclass()`; spaces keep the class. `baspgm.[ch]`: `list_program_hl()` (`baspgm.h` now includes `bastok.h`). `basrepl.[ch]`: `repl_t.hlstyle`, used by `repl_list()` only on channel 0. `engine1999.c`: `host_hlstyle()` with an ANSI SGR table. `repltest1`: 8 new cases with marker strings (121 total), valgrind clean; `vmtest1`, `testcomp1`, `ansitest1` pass. Docs: `basic-language.md`, `SESSION.md`. Checked on the real SDL screen by the user.

### 340a04c — 2026-10-28 21:37:01 +0100 — "Claude Code bookkeeping files updated"
2 files, +7/-3.
- `HistoryLog-Claude.md`: logged `b984b8b`; `SESSION.md`: latest commit line, highlighting marked as checked on the screen. No source changes.

### ed9626c — 2026-10-28 21:51:36 +0100 — "EDIT single line editor, created using Claude Code"
25 files, +417/-13.
- **`EDIT` statement: simple line editor.** Language: `TOK_EDIT [ goto-target ]` (`bascomp.[ch]`, `bascomp.ebnf`), `NT_EDITSTMT 0x63` (`nodetype.[ch]`), `cgen_from_editstmt()` (`cgenprog.c`), instruction `EDIT 0x140` (`instset.h`, `codegen.[ch]`), `vm_edit_request()` and `VMREQ_EDIT` (`vm.[ch]`). REPL: `repl_t.edit` callback and `repl_t.pending`, `repl_edit_line()`, `repl_enter()` is now `repl_enter_line()` plus a pending loop (`repl_enter_pending()`, also after `repl_run()`); no target = "? Not implemented yet". Host: `sdlscr_lineedit()` (`sdlscreen.[ch]`; keys insert/LEFT/RIGHT/HOME/END/BACKSPACE/DELETE, RETURN accepts, ESC cancels; switches break-on-ESC off while it runs) and `host_edit()` in `engine1999.c`. Tests: `repltest1` +14 (135), `vmtest1` +5 (385), valgrind clean; drawing/scroll logic checked in a scratch program; checked on the real screen by the user (works as expected). Docs: `basic-language.md`, `compiler-and-vm.md`, `display-engine.md`, `roadmap.md`, `CLAUDE.md`, `SESSION.md`.

### de09d9a — 2026-10-28 21:54:50 +0100 — "Claude Code bookkeeping files updated"
2 files, +8/-3.
- `HistoryLog-Claude.md`: logged `ed9626c`; `SESSION.md`: latest commit line. No source changes.

### e09a0cc — 2026-10-28 21:55:50 +0100 — "Claude Code bookkeeping files updated"
1 file, +1/-0.
- Small bookkeeping fix. No source changes.

### ccf115c — 2026-10-28 23:12:37 +0100 — "Claude Code bookkeeping files updated"
2 files, +12/-6.
- `HistoryLog-Claude.md`: logged `de09d9a` and `e09a0cc`. `SESSION.md` tidied: removed the duplicated `ansitest1` test line and the outdated second "ANSI colors" bullet, updated the latest-commit line and the `make` target list. No source changes.

### 839feb6 — 2026-10-28 23:49:38 +0100 — "Improved font, created by Claude Code"
9 files, +316/-310.
- **Font redesign** of all printable ASCII glyphs (0x21–0x7E) in `8x12font1.txt`, recompiled to `8x12font1.c` with `fontcomp.pl`. Original design from basic typographic rules (not copied from any existing computer font): 2 px stems, 1 px horizontals, rounded corners; caps/digits rows 2–9, x-height rows 4–9, ascenders from row 2, descenders rows 10–11; slashed-diagonal `0`. Glyphs 0x7F and above untouched.
- Refined after screenshots checked on the real screen by the user: `%` and `&` redrawn; 6-wide `M`/`W` tried but read as `H`, so both stay 7 wide; `( ) [ ] { }` and `|` are 11 rows tall (rows 1–11), `/` and `\` a regular 2 px wide staircase (rows 1–10).
- `make`, `vmtest1`, `repltest1` pass. Docs: `SESSION.md`.

### 40748f3 — 2026-10-28 23:51:14 +0100 — "Claude bookkeeping files updated"
2 files, +13/-7.
- `HistoryLog-Claude.md`: logged `ccf115c` and `839feb6`, Pending cleared. `SESSION.md`: latest commit line, font marked as checked on the real screen. No source changes.

### 6e4ea7e — 2026-10-28 23:53:22 +0100 — "Claude bookkeeping files updated"
3 files, +10/-2.
- `HistoryLog-Claude.md`: logged `40748f3`. `SESSION.md`: latest commit line. `docs/roadmap.md`: new last section "Later: look at again" with a to-do to review the symbol glyphs (and maybe `N`) of the font. No source changes.

### 0acbb1f — 2026-10-28 23:53:58 +0100 — "Claude bookkeeping files updated"
2 files, +6/-2.
- `HistoryLog-Claude.md`: logged `6e4ea7e`, Pending cleared. `SESSION.md`: latest commit line. No source changes.

### b1eb173 — 2026-10-28 23:54:25 +0100 — "Claude bookkeeping files updated"
2 files, +5/-1.
- `HistoryLog-Claude.md`: logged `0acbb1f`. `SESSION.md`: latest commit line. No source changes.

### 6d12ea9 — 2026-10-29 01:31:54 +0100 — "comment bug fixed by Claude Code"
6 files, +29/-14.
- **Fix: comments stopped a program.** A line consisting only of a comment (`20 'text`) gave "Internal error (not at end of text)", and a comment after a colon (`PRINT 1: 'note`) gave "Statement expected". `comp_eat_stmtline()` (`bascomp.c`) now accepts a line without statement list (the `NT_STMTLINE` node then has no branch); `comp_eat_list()` doesn't complain when a `:` is followed by a comment; `cgen_from_stmtline()` (`codegen.c`) accepts 0 branches. `repltest1`: 6 new comment cases (141 total, all pass); `vmtest1`, `ansitest1` pass. Docs: `bascomp.ebnf` (stmt-line), `SESSION.md`.
- Checked on the real screen by the user (works). Also: logged `b1eb173`; `SESSION.md`: latest commit line fixed.

### c0f5592 — 2026-10-29 01:32:58 +0100 — "Claude bookkeeping files updated"
2 files, +9/-6.
- `HistoryLog-Claude.md`: logged `6d12ea9`. `SESSION.md`: latest commit line. No source changes.

### 33c03a1 — 2026-10-29 20:15:29 +0100 — "Full-screen text editor (created by Claude Code)"
20 files, +1468/-141.
- `textscreen.[ch]`: state moved into a per-screen struct; two screens (`txtscr_select()`), per-screen viewport (`txtscr_setviewport/resetviewport/getviewport`; coordinates, cursor, wrapping and scrolling relative to it), `txtscr_clear/setblank/getcell`. Fixed along the way: cursor row was `uint8_t` (backspace at the top never scrolled), `txtscr_write` accepted `y == height`.
- `fulledit.[ch]` (new): the editor on screen 1 (title bar, line number pane, text pane with highlighted lines, scroll bar, help bar, one viewport each); in-place editing of the current line, new-line insertion with number entry in the number pane / automatic number, CTRL+Y, ESC, remembered position.
- `sdlscreen.[ch]`: shared `sdlscr_editkey()`, `sdlscr_editmode_begin/end()`, `sdlscr_textchanged()`, public `sdlscr_deleteinputmsg()`; key messages carry `mods`; `sdlscr_lineedit()` uses the viewport size.
- `basrepl.[ch]`: `repl_t.fulledit` (EDIT without a target sets it instead of "Not implemented yet"), `repl_t.filename` (set by LOAD/SAVE, cleared by NEW). `engine1999.c` opens the editor after `repl_enter()`.
- New `txttest1.c` (40 checks), 7 new `repltest1` cases (2 EDIT cases changed); makefile (`fulledit.o`, `txttest1`), `.gitignore`, `CLAUDE.md`, `SESSION.md`, `docs/` (display-engine, basic-language, build-and-test, roadmap).
- Tests: `vmtest1` 385/385, `repltest1` 148/148, `ansitest1` 0 failures, `txttest1` 40/40; `make` clean with `-Wall -Werror`. The editor was checked on the real screen under Xvfb with XTest key injection.

### 6b2d5ae — 2026-10-29 20:27:39 +0100 — "Full-screen editor fix, authored by Claude Code"
5 files, +40/-17.
- `fulledit.c`: UP/DOWN/PGUP/PGDN/CTRL+HOME/CTRL+END were ignored while a new line was being inserted. They now first end the insertion like RETURN (`ed_endins()`: the new line is entered, an empty one or one whose number isn't complete is dropped; if the REPL refuses it the insertion goes on with the message), then move. `docs/display-engine.md`, `SESSION.md` updated.
- Tests: `make` clean, `repltest1` 148/148, `txttest1` 40/40; checked on the real screen under Xvfb (new line 25 entered by UP, cursor moved on).

### 3f6f51f — 2026-10-29 20:57:21 +0100 — "Machine monitor (created by Claude Code)"
37 files, +1555/-45.
- **Language/VM**: keyword `MONITOR` (`TOK_MONITOR` 147, `bastok`), `NT_MONITORSTMT 0x64` (`nodetype`, `bascomp`, `bascomp.ebnf`), instruction `MON 0x141` (`instset.h`, `codegen`, `cgenprog.c`, `vm.c`), `VMREQ_MONITOR` (`vm.h`); `repl_t.monitor` flag set by `repl_finish()`.
- **`basrepl.[ch]`**: `repl_compile()` (compile the stored program without running), `repl_go( addr )` (run the code segment as it is, then carry out requests).
- **`vmasm.[ch]`** (new, no SDL): decoder, formatter and assembler for the VM's instruction set; `codegen.c`: `cgen_opcode_name()` / `cgen_has_cd()` are public now.
- **`monitor.[ch]`** (new): the monitor on text screen 2; commands `? r m u/d a e f l c g q`; regions code, data, dstack, rstack; syntax-colored output; hex numbers by default.
- **`textscreen`/`sdlscreen`**: `TXTSCR_NSCREENS` 3, `txtscr_setcolors/getcolors`; one ANSI decoder per text screen with the screen's own default colors (program output shows on the dark monitor screen); `engine1999.c` opens the monitor after `repl_enter()`.
- Tests: new `asmtest1.c` (56 checks incl. a decode → format → assemble round trip over a compiled program); `vmtest1` +3 cases (388), `repltest1` +3 cases (151), `txttest1` +3 checks (43); makefile (`vmasm.o`, `monitor.o`, `asmtest1`), `.gitignore`, `CLAUDE.md`, `SESSION.md`, `docs/` (display-engine, basic-language, compiler-and-vm, build-and-test, roadmap). Checked on the real screen under Xvfb with XTest key injection; the user tried it as well and found no problems.
- Found while testing: scrolling filled with the title bar's colors until the output viewport restored its own blank colors (fixed before the commit).

### a9a496a — 2026-10-29 21:12:03 +0100 — "Additional machine monitor features, created by Claude Code"
13 files, +445/-45.
- **`vm.[ch]`**: the VM is resumable: `vm_begin()`, `vm_resume( singlestep )` (`vm_run_at()` is both now), `vm_t.suspended` / `stopreason` / `breakpoints` / `debug` / `brkhit`, `VMSTOP_*`. `BRK` stops the run when `vm_t.debug` is set, else it is still the error "Breakpoint". A breakpoint is checked before an instruction but never at the first one of a resume.
- **`basrepl.[ch]`**: `repl_vm_start()` / `repl_vm_resume()` replace `repl_go()` (a run that ends is finished like after `RUN`, a suspended one is left alone).
- **`monitor.c`**: commands `g` (continue a suspended run or start one), `s [a]`, `t [n]`, `b [a]`, `k a|*`, `i`; markers `>` (pc) and `*` (breakpoint) in the disassembly, `pc $…` in the title bar, register line (`pc line ds rs top`) and next instruction after a stop; breakpoints are cleared by `c`; help bar and help text updated.
- Tests: `repltest1` +12 cases (163): breakpoints, leaving a breakpoint, single-step through a whole program, BRK with and without the debugger, refused start address. `vmtest1` 388/388, `asmtest1`, `txttest1`, `ansitest1` unchanged and passing. Checked on the real screen under Xvfb with XTest key injection.
- Docs: `vm.h` header, `monitor.h`, `docs/display-engine.md`, `docs/compiler-and-vm.md`, `docs/roadmap.md` (BRK ticked, monitor item shortened), `SESSION.md`, `CLAUDE.md`.

### 5bbf687 — 2026-10-29 21:15:22 +0100 — "Claude state file update"
2 files, +11/-8.
- Bookkeeping: `HistoryLog-Claude.md` (entry for `a9a496a` moved into the log) and `SESSION.md`. No source changes.

### f509f04 — 2026-10-29 21:39:22 +0100 — "EDIT on empty program fixed by Claude Code"
5 files, +18/-7.
- **`fulledit.c`**: with no program lines the full-screen editor opens straight in line number entry (`ed_idle()`, called on opening and after every key), stays there after an empty or cancelled new line (and after the last line was deleted), and ESC in the number phase leaves the editor. Before, ESC cancelled the insertion and left a state with no current line where typed text was ignored.
- `docs/display-engine.md`, `SESSION.md` updated. Tests: vmtest1, repltest1, txttest1, asmtest1, ansitest1 pass; checked on the real screen under Xvfb + XTest.
- The user's commit also contained `residue/foo3.bas` (a binary file from the residue folder, not Claude's work).

### ba63ecb — 2026-10-29 21:41:44 +0100 — "Claude Code state file update"
2 files, +13/-5.
- Bookkeeping: `HistoryLog-Claude.md` (entry for `f509f04` moved into the log) and `SESSION.md`. No source changes.

### b6a3dce — 2026-10-29 22:16:20 +0100 — "Audio subsystem rewritten by Claude Code"
15 files, +909/-586.
- **`synth.[ch]`** (new, no SDL): 8-channel synthesizer — phase-accumulator oscillators (sine, square, triangle, noise), 2.5 ms gain slew, scaling mixer, per-channel time cursors (`synth_slot`, `synth_sync`), time-sorted stable event queue (`synth_schedule/runevents/nextevent/silence`), note table by formula (0..71).
- **`sdlaudio.[ch]`** rewritten on top of it: one mutex, worker thread applying due events (≥1 kHz), callback renders; new API `sdlaud_setwave/setbpm/syncchans/silenceall`. Fixed: init hang when the device can't be opened (worker raises INITDONE on failure), NULL `memset( userdata )` in the callback (now `stream`), pitch error up to 2.8 % on high notes, head-of-line blocking between channels, stale metronome making later sequences fire at once. Clicks removed by the gain slew.
- **`sdlmain.c`**: a failed audio init is a warning, the engine runs without sound (ERR4 path gone).
- Tests: new `synthtest1.c` (77 checks, valgrind clean); vmtest1, repltest1, txttest1, asmtest1, ansitest1 pass. Real-time check with SDL's disk audio driver: pitches exact, a rest-then-note on channel 0 queued before an immediate note on channel 1 plays in the right order, a 3-note sequence on channel 2 at the right times, open failure returns instead of hanging.
- Docs: new `docs/sound.md` (architecture, API, proposal for `SOUND`/`WAVE`/`TEMPO`/`PLAY`, FM instrument plans), `docs/README.md`, `docs/display-engine.md`, `docs/roadmap.md` (section 2b), `CLAUDE.md`, `SESSION.md`; `makefile` (`synth.o`, `synthtest1`), `.gitignore`.

### 3a09ac1 — 2026-10-29 22:46:44 +0100 — "residue files renamed by user; Audio subsystem fleshed out and integrated into BASIC by Claude Code"
37 files, +1148/-66.
- **Language**: `SOUND ch,freq,beats[,vol]`, `SOUND OFF [ch]`, `SOUND SYNC ch,…`, `PLAY a$[,b$…]`, `WAVE ch,w`, `TEMPO bpm`, function `PLAYING(ch)`; tokens 148..153 (`SOUND PLAY WAVE TEMPO PLAYING SYNC`), `NT_SOUNDSTMT 0x65` + `SNDK_*`, instructions `SND 0x142` / `PLNG 0x143`; `bastok.[ch]`, `bascomp.[ch]` + `.ebnf`, `nodetype.[ch]`, `cgenprog.c`, `codegen.[ch]`, `instset.h`, `vm.[ch]` (`vm_sound_t`, `vm_t.sound`), `basrepl.[ch]` (`repl_t.sound`), `engine1999.c` (host sound callbacks). Volume 0..1, channels 1..8, `PLAY` returns at once.
- **`mml.[ch]`** (new, no SDL): parser of the `PLAY` music strings (notes, rests, O < >, L, T, V, W, ML/MN/MS), all strings are parsed before anything is queued.
- **Audio**: `sdlaud_busy/getbpm/silencechan`, `synth_busy/synth_silencechan`; the event queue appends in O(1) (tail pointer; was linear per event), limit 262144; the audio worker waits on a condition variable while the queue is empty (idle CPU 0.06 → 0.009 s per 3 s); `makefile` (`mml.o`).
- **Tests**: `vmtest1` +43 cases (431; a sound output that logs the calls), `repltest1` +4 (167), `asmtest1` +3 (59, incl. the round trip over sound instructions), `synthtest1` +43 checks (120: `mml`, busy, silencechan, queue order/tail/speed); valgrind clean. Real-time check on the engine under Xvfb with SDL's disk audio driver (`PLAY` voices: exact pitches and timing).
- **Docs**: `docs/sound.md` (statements, music strings, cost of many short notes, **proposal for sound effects**: immediate `PITCH`/`VOLUME`, glide, `ENVELOPE`, `VIBRATO`/`TREMOLO`, stored `SFX n, "..."`, instruments), `basic-language.md`, `compiler-and-vm.md`, `roadmap.md`, `bascomp.ebnf`, `CLAUDE.md`, `SESSION.md`. Owner decisions recorded: effects proposal approved, queued music keeps playing after a program ends.
- The commit also contained the user's rename of the `residue/` program files from `*.bas` to `*.e99` (not Claude's work).

### 4ae1ef1 — 2026-10-29 23:02:16 +0100 — "Effects framework for audio subsystem and BASIC language, created by Claude Code"
29 files, +1125/-140.
- **Sound effects** (the approved proposal): `PITCH`, `VOLUME` (at once), glide as the fifth argument of `SOUND`, `ENVELOPE`, `VIBRATO`, `TREMOLO`, `SFX n,a$` / `SFX n[,ch]`; tokens 154..159, `SNDK_*` 7..13 (`nodetype.h`), `comp_eat_soundstmt` / `cgen_from_soundstmt` extended, `vm_sound()` / `vm_sfx_define()` / `vm_sfx_play()` / `vm_queue_voice()` in `vm.c`, `vm_t.sfx[]`, `vm_sound_t` extended (`tone` with freq2, `setenv`, `setvib`, `settrem`, `setpitch`, `setvolume`), `engine1999.c` host callbacks, `instset.h`.
- **`synth.[ch]`**: glide, ADSR envelope, vibrato, tremolo, immediate pitch/volume, `synth_schedule_ev()`; **`sdlaudio.[ch]`**: `sdlaud_playsweep/setenvelope/setvibrato/settremolo/setpitch/setvolume`; **`mml.[ch]`**: effect commands `H S X XR ~ ^`.
- **File extensions**: `SAVE` appends `.e99` (binary) / `.bas` (ASCII, `,A`) to names without an extension, `LOAD` finds `name`, `name.bas`, `name.e99`, `name.BAS`, `name.E99` (`basrepl.c`); owner decision `.BAS` / `.E99`, written in lower case.
- **Tests**: `vmtest1` 469 cases (+38), `repltest1` 179 (+12: extensions, SFX persistence, listing), `asmtest1` 60, `synthtest1` 169 (+49: glide, envelope, LFOs, immediate control, effect commands); `vmtest1` and `synthtest1` are clean under valgrind. Real-time check on the engine (Xvfb + SDL disk audio driver): an `SFX` sweep with an envelope.
- **Docs**: `docs/sound.md` (the proposal became the description), `basic-language.md` (sound, file extensions), `compiler-and-vm.md`, `roadmap.md`, `bascomp.ebnf`, `CLAUDE.md`, `SESSION.md`.

### f587a1b — 2026-10-29 23:15:11 +0100 — "Audio demo files created by Claude Code and state update"
8 files, +104/-6.
- **`demos/`** folder with `README.md`, `sfx.bas` (sound effect board, engine hum via `PITCH`/`VOLUME`), `ode.bas` (Ode to Joy), `prelude.bas` (Bach, Prelude in C, 8 bars); all free of license text; `CLAUDE.md` table. Found a language quirk: `IF c THEN GOSUB n:stmt` always runs `stmt` (roadmap).
- Also: `demos/README.md` (what the demos are, no license text), the roadmap finding about `IF c THEN GOSUB n:stmt`, `CLAUDE.md` / `SESSION.md`.

### b1737df — 2026-10-29 23:32:49 +0100 — "WAIT statement and makefile fix, created by Claude Code"
26 files, +148/-19.
- **`WAIT n`**: new statement (token 160, `NT_WAITSTMT 0x66`, instruction `WAIT 0x144`; `bastok.[ch]`, `bascomp.[ch]` + `.ebnf`, `nodetype.[ch]`, `cgenprog.c`, `codegen.[ch]`, `instset.h`, `vm.c`): sleeps n/60 s without using the processor, Break works. Reason: the "engine" of `demos/sfx.bas` waited with `WHILE TI<T:WEND`, a busy loop (CPU at 100 % for 7 s).
- **`makefile`**: dependency rules for `fulledit.o`, `monitor.o`, `vmasm.o` (they depended only on their `.c`): they were not rebuilt after `repl_t` / `vm_t` changed, so the full-screen editor read `hlstyle` at a stale offset (no syntax highlighting) and the monitor used a wrong layout.
- **Demos**: `sfx.bas`, `ode.bas`, `prelude.bas` use `WAIT` instead of busy loops.
- **Tests**: `vmtest1` 477 (+8 WAIT), `repltest1` 180 (+1: Break ends a WAIT), `asmtest1` 61; the editor highlighting and the monitor were checked on the real engine (Xvfb) after the rebuild.
- **Docs**: `basic-language.md`, `compiler-and-vm.md`, `sound.md`, `roadmap.md` (comments are upper-cased by the tokenizer; no `FRAME`), `demos/README.md`, `SESSION.md`.

### b57d615 — 2026-10-29 23:39:02 +0100 — "IF ... THEN GOSUB ... STMT fixed by Claude Code"
8 files, +68/-16.
- **`bascomp.c`** (`comp_eat_singlelineifstmt`): `IF c THEN GOSUB 100 : PRINT "x"` (also `THEN 100`, `IF c GOTO 100`) kept only the jump in the IF; the statements after the colon ran whether or not the condition was true. Now a colon after the jump target turns the jump into a `GOTO`/`GOSUB` statement and the rest of the line becomes the rest of the THEN statement list (ELSE then takes a statement list); without a colon the grammar is unchanged. `bascomp.ebnf` updated.
- `demos/sfx.bas` back to the natural form `IF K$="0" THEN GOSUB 500:GOTO 180`; `docs/roadmap.md` item removed.
- **Tests**: `vmtest1` 490 (+13: false/true for GOSUB, GOTO, `THEN line`, `IF … GOSUB` without THEN, several statements, ELSE, UNLESS, comment after the colon, unchanged forms), `repltest1` 181 (+1: RENUM adjusts such a jump); all other tests pass; `sfx.bas` plays through the REPL harness.

### 991b5d7 — 2026-10-29 23:40:53 +0100 — "Claude Code state update"
2 files, +9/-6.
- Bookkeeping only: `HistoryLog-Claude.md` (entries for `b1737df` and `b57d615` moved from Pending into the Log) and `SESSION.md` (repo state: latest commit `b57d615`, everything committed). No code changes, no tests needed.

### 4073221 — 2026-10-30 19:51:24 +0100 — "Frame rate limiting fixed by Claude Code"
5 files, +30/-16.
- **Frame pacing fixed** (`sdlscreen.c`, `sdlscr_worker`): the refresh rate was irregular. The old code measured the time after `SDL_RenderPresent`, slept until the *previous measurement* + 1/60 s and then stored the pre-sleep time as the new reference, so the next frame's elapsed time was short by the sleep and frame periods alternated; it also sleeped for a full period when a frame was merely fast. Now each frame has an absolute due time (`lastTick`/`lts`, previous due time + 16 666 667 ns, no drift); after presenting, the thread sleeps until it with `sdlutil_nanosleep( 0, &lts )`, and if it is more than one frame late it resynchronizes instead of trying to catch up. Also paces displays faster than 60 Hz (vsync alone gave their rate). `make` builds cleanly; not verified on a real display (no timing harness), please check on the other computer.
- **`sdlutil.c`: `CLOCK_REALTIME` → `CLOCK_MONOTONIC`** (`sdlutil_getnsec/projecttime/nanosleep`), so a system clock change can't disturb frame pacing, audio worker or other waits. All callers (`sdlscreen`, `sdlaudio`, `sdlevent`, `testcomp1`) only use relative times; `vm.c` already used `CLOCK_MONOTONIC`.

### 8530b23 — 2026-10-30 20:01:54 +0100 — "SFX demo program fixed by Claude Code"
4 files, +14/-6.
- **`demos/sfx.bas` engine (key 0) silent or wrong after other effects**: reported as an unstable sound engine. Cause (reproduced at the audio layer with SDL's disk driver, a harness in the session scratchpad: coin effect, then the engine): `SFX n` without a channel takes the highest free channel, which is channel 8, the engine's channel, and an effect leaves its envelope/tremolo behind (documented behavior of `SFX`); the engine reset only wave and vibrato, so after e.g. `SFX 4` (envelope with sustain 0) the engine decayed to silence in 0.3 s. A still-running effect on channel 8 would also delay `SOUND 8` (it queues after the channel's time cursor) while `PITCH`/`VOLUME` act at once. Fix: the engine routine starts with `SOUND OFF 8:ENVELOPE 8:TREMOLO 8,0,0` (lines 505/506). With the reset the rms rises steadily (0.16 → 0.35) as intended; without it it is 0 after 0.3 s. No engine (C) change; `docs/sound.md` got a note. Not tested with a real audio device; the audio layer repeated the sequence 4 times without any irregularity.

### f307368 — 2026-10-30 20:05:30 +0100 — "SFX statement fixed by Claude Code"
5 files, +46/-7.
- **`SFX` takes back its effects when it ends** (owner decision, after the sfx demo bug): `vm_sfx_cleanup()` (`vm.c`), called by `vm_sfx_play()` after the voice: if the string set a wave, envelope, vibrato or tremolo, the same events that start the effect reset them at the end of the voice (wave 0, default envelope, vibrato/tremolo off); with an envelope the channel first rests for its release time (`X … r`), so the release is not cut short. A following `SOUND` on that channel starts clean (the cause of the engine bug). `PLAY` is unchanged. `docs/sound.md` updated (the sentence "effects stay afterwards" and the note I added for the demo).
- **Tests**: `vmtest1` 493 (+3: wave, vibrato/tremolo, envelope with release wait), all other tests pass. At the audio layer (SDL disk driver, scratchpad harness) the engine tone after the coin effect plays at full level without the manual reset (rms 0.16 → 0.26 …, was 0 after 0.3 s).

### c377fbf — 2026-10-30 20:35:04 +0100 — "MERGE and CHAIN statements added by Claude Code"
24 files, +596/-27.
- **`MERGE` and `CHAIN` statements** (owner's design, my suggestions accepted; `COMMON` skipped).
  - **Front end**: `MERGE name$ [, APPEND]` (`NT_MERGESTMT 0x67`; `APPEND` is an identifier checked in the parser, not a keyword) and `CHAIN [MERGE] name$ [, line|label]` (`NT_CHAINSTMT 0x68`): `bascomp.c/.h/.ebnf`, `nodetype.[ch]`, `cgenprog.c`, `codegen.[ch]` (`cgen_gen_mrg/chn`), `instset.h` (`MRG 0x145 ( s -- )`, `CHN 0x146 ( s1 s2 n -- )`, parameter = APPEND / MERGE flag; documented).
  - **VM** (`vm.[ch]`): `MRG` calls the new host callback `vm_t.merge` and ends the run (like `DELE`/`RENM`); `CHN` leaves the request `VMREQ_CHAIN` (`reqname`, new `reqlabel`, `reqline`, new `reqmerge`; `vm_clear_request()` frees `reqlabel`). Also initialized `addr` in `vm_step()` (a gcc maybe-uninitialized false positive caused by the two new cases).
  - **Program store** (`baspgm.[ch]`): `merge_programs()` (linear merge of two sorted line lists, a replaced line is deleted, cannot fail), `find_label_clash()` (a label that both programs define in lines the merge does not replace).
  - **Variables** (`vars.[ch]`): `vmem_copy_vars( dst, src )` copies the values of variables with the same type and name (arrays only with equal dimensions; not labels, functions, hidden `@` variables).
  - **REPL** (`basrepl.[ch]`): `repl_merge_program()` (reads into a scratch program, `APPEND` renumbers it with `renumber_program()` to start behind the last stored line - first multiple of 10 above it, 10 if empty - the stored program is not renumbered; the stored program changes only if everything is fine), `repl_vm_merge()`, `repl_reset_runtime_ex()` (moves the old runtime aside and keeps the `SFX` definitions), `repl_start_program_ex( …, keepvars )`; `repl_finish()` handles `VMREQ_CHAIN` (load or merge, then run with the variables kept; also in direct mode); `repl_execute()` copies the variables after code generation (arrays exist from parse time, so they can only be copied afterwards). New field `repl_t.keeprt`.
  - **Tests**: `vmtest1` 508 (+15: operands, request, errors), `repltest1` 213 (+32: replace/insert, binary/empty/missing/bad files, `APPEND` numbering and reference adjustment, line number overflow, duplicate labels, run ends, `CHAIN` variables/arrays/string arrays/targets/`SFX`/missing file, `CHAIN MERGE`); all other tests pass; `repltest1` is clean under valgrind (copy without `-p`).
  - **Old variables survive a failed `CHAIN`** (owner decision): `repl_start_program_ex()` puts the old runtime back with the new `repl_restore_runtime()` when the new program fails before it runs (`repl_t.started`: compile error, empty file, unknown target); a run time error in the new program is an ordinary one. `repltest1` 221 (+8: the three failure kinds, functions and `SFX` kept, no restore after a run time error, and `MERGE APPEND` / `CHAIN` / `CHAIN MERGE` of binary files); valgrind clean. A first version forgot to reset `started` on the empty-program path (found by the empty-file test).
  - **Docs**: `docs/basic-language.md`, `docs/compiler-and-vm.md`, `docs/roadmap.md` (`FILES` remains), `bascomp.ebnf`, `SESSION.md`.
  - A mistake of mine, for the record: I tried `git stash` to compare against the old tree; the permission layer denied it before it ran (git writes are yours), nothing was changed.

### b9f2c7b — 2026-10-30 20:53:15 +0100 — "FILES statement created by Claude Code"
26 files, +690/-26.
- **`FILES [spec$]` statement** (owner's spec: like `ls -ltr`, names and times, no mode/user info, `ls` type conventions, syntax highlighted, paged).
  - **New module `basfiles.[ch]`** (no SDL; `makefile`: object, header, dependency rule): `files_list()` collects the entries (current directory without hidden names, a directory, a `glob()` pattern or one file), sorts them by modification time (seconds), then name, and emits one line each: size (right-aligned), `YYYY-MM-DD HH:MM`, name + `ls -F` indicator (`/ * @ | =`, ` -> target` for links). Colored through `hlstyle_t`: new classes `HL_FILEDIR`, `HL_FILELINK`, `HL_FILEEXEC`, `HL_FILEPROG` (`bastok.h`; sizes use `HL_NUMBER`, times `HL_COMMENT`); `engine1999.c`'s `host_hlstyle()` table has them (navy, teal, green, purple).
  - **Front end/VM**: `FILES [str-expr]` (`NT_FILESSTMT 0x69`; `comp_eat_filesstmt`, `cgen_from_filesstmt`, instruction `FILS 0x147 ( s -- )`, `vm_t.files` callback; the run goes on). `bascomp.ebnf`, `instset.h`, `nodetype`, `codegen` updated.
  - **REPL** (`basrepl.[ch]`): `repl_vm_files()` prints through the channel's print callback (colors only on channel 0), polls `interrupt` per line ("Break"), pages channel 0 output with the new `repl_t.pagesize` / `repl_t.pause` (a stop by the user is no error; ESC at the prompt is reported as "Break").
  - **Host** (`engine1999.c`): `host_pause()` ("-- more: any key continues, Q stops --" on the last line, erased afterwards; `sdlscr_getkey()`), `repl.pagesize` = screen height - 1.
  - **Tests**: `vmtest1` 514 (+6: operands, expressions, run goes on, before ELSE/colon), `repltest1` 235 (+14: a fixture directory with fixed times (TZ=UTC) and sizes: ordering, alignment, hidden names, link/directory/executable indicators, single file, no match, expression, in a program, highlighting of each class, paging asked/stopped/not needed); all other tests pass; `repltest1` clean under valgrind. Not seen on the real screen by me; the owner checked it and reports that it works.
  - **Docs**: `docs/basic-language.md`, `docs/compiler-and-vm.md`, `docs/roadmap.md` (`MERGE`, `CHAIN`, `FILES` done), `CLAUDE.md` (module table), `bascomp.ebnf`, `SESSION.md`.

### cf10477 — 2026-10-30 20:59:37 +0100 — "Documentation and Claude session updates"
4 files, +50/-11.
- Documentation only: `docs/basic-language.md` got a section "Binary program files (`.e99`)" (header, line layout, a byte-by-byte example dumped from a real `SAVE`, reading rules, compatibility note); `docs/README.md` index line updated. No code changed, no tests needed.

### ffe0042 — 2026-10-30 21:17:10 +0100 — "AFTER/EVERY statements created by Claude Code"
19 files, +440/-16.
- **`AFTER` and `EVERY` statements** (Locomotive-style, per the owner: `AFTER n GOSUB|GOTO target`, `AFTER OFF`, `EVERY OFF` clear the handlers of that statement type).
  - **Language**: `AFTER|EVERY delay [, timer] GOSUB|GOTO target`, `AFTER OFF [timer]`, `EVERY OFF [timer]` (`NT_TIMERSTMT 0x6A` with `TMRF_*` flags; `comp_eat_timerstmt()` in `bascomp.c`, `cgen_from_timerstmt()` in `cgenctl.c`, instructions `TMR 0x148 ( delay timer -- )` with the handler address on the return stack, and `TMRO 0x149 ( timer -- )`; `bascomp.ebnf`, `instset.h`, `nodetype`, `codegen`). The tokens `AFTER`/`EVERY` existed already (116/117, highlighted as control flow now).
  - **VM** (`vm.[ch]`): four timers (`vm_timer_t`, `CLOCK_MONOTONIC` nanoseconds, unit 1/60 s), `vm_irq_poll()` called by `vm_resume()` before each instruction while a timer is active: ends returned handlers (return stack at or below the entry value: `RETURN` and `POP`; the interrupted line is restored), enters the highest due timer above the running handler's number between two statements (data stack empty, no function call); `GOSUB` handlers push the interrupted `pc`, `GOTO` handlers jump. `WAIT` gives way: with a timer due it puts the remaining ticks back, rewinds to itself and sets `irqsafe`. `vm_begin()` clears the timers (timers end with the run). Fixed on the way: valgrind found `irqpending` uninitialized.
  - **Tests**: `vmtest1` 542 (+28: fires during WAIT, once, in a busy loop, WAIT time kept, EVERY, both OFF kinds and their independence, timer numbers, GOTO, GO SUB and labels, replacing, priorities in both directions, return to the interrupted place, expressions, timers end with the program, illegal arguments, parse errors, ELSE), `repltest1` 239 (+4: direct mode lines, timers do not outlive the run, listing); the timing cases also pass six at a time on four cores; all other tests pass; `vmtest1` and `repltest1` clean under valgrind.
  - **Docs**: `docs/basic-language.md` ("Timers"), `docs/compiler-and-vm.md`, `docs/roadmap.md` (AFTER/EVERY done; new finding: `GOTO`/`GOSUB` to a line number typed in direct mode is "Undefined line number", labels work), `bascomp.ebnf`, `SESSION.md`.

### 6764232 — 2026-10-30 21:28:26 +0100 — "REMAIN() function and direct mode GOTO/GOSUB fix added by Claude Code"
18 files, +123/-23.
- **`REMAIN(timer)` function**: token `TOK_REMAIN` 161 (reserved word, highlighted as a function), parsed with `FRE`/`PLAYING` in `comp_eat_sysnumfn1argcall()`, `cgen_gen_rmn()`, instruction `RMN 0x14A ( timer -- n )` in `vm_system_function()`: ticks left of the timer (rounded up), 0 if it is not set, and the timer is cleared (as in Locomotive BASIC); timers outside 0..3 are "Illegal argument".
- **Line numbers as targets in direct mode lines fixed** (found with `AFTER`): `cgen_from_program()` (`codegen.c`) emptied the line table on every compile, so `GOTO 100` / `GOSUB 100` / `AFTER n GOSUB 100` / `RESTORE 20` typed at the prompt were "Undefined line number". It keeps the table now when it compiles a direct line (a statement line without a line number). `repl_t.linesstale` (`basrepl.[ch]`: set when a line is entered, `DELETE`, `RENUM`, `MERGE`, `LOAD`; cleared by a program compile in `repl_execute()` / `repl_compile()`) makes `repl_execute()` empty the table before a direct line when the program changed since it was compiled, so a stale line number can't enter old code.
- **`HLT 0x14B` ends a compiled program** (needed by the fix above, `codegen.c`, `cgenctl.c`, `vm.c`, `instset.h`): `END` and the end of the last line used to branch to `codesize`, which is the start of the next direct line's code; a direct `GOTO 100` into a program whose line 100 ended with `END` therefore ran the direct line again, endlessly (found when the first test hung; the test run was killed). A program now ends in a `HLT`; `codegen_t.endaddr` is the target of `END` (direct lines have none: their `END` still goes to the end of the code). The monitor lists the extra instruction at the end of a compiled program.
- **Tests**: `vmtest1` 550 (+8: `REMAIN` unset / time left and cleared / `EVERY` / fired / in an expression / illegal timers / missing parenthesis), `repltest1` 247 (+8: `GOTO` and `GOSUB` into the program from a direct line, no compiled program, stale table after a changed line / `RENUM` / `DELETE` and fresh after `RUN`, `RESTORE n` direct, `AFTER ... GOSUB 100` direct); all other tests pass; `vmtest1` and `repltest1` clean under valgrind. A `pkill -f repltest1` of mine also killed my own shell (its pattern matched the command line): no harm done.
- **Docs**: `docs/basic-language.md` (Timers: `REMAIN`, direct mode line numbers), `docs/compiler-and-vm.md`, `docs/roadmap.md` (the finding is removed), `SESSION.md`.
