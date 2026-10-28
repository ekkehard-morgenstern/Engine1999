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

(none)

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
