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
- **ncurses checked for a key state** (the owner's idea, for the console `KEY()` review): the man page `getch(3ncurses)`, `/usr/include/curses.h` and the strings of `libncursesw.so.6.4` / `libtinfo.so.6` have nothing on key release or held keys; the only "released" events are mouse buttons (`BUTTON1_RELEASED` ...), delivered through the xterm mouse protocol (`kmous=\E[<`, `XM=\E[?1006;1000...`); a web search found nothing in ncurses' NEWS about the kitty keyboard protocol (the library of that family that implements it is notcurses, also crossterm and the terminals kitty, foot, WezTerm). Result: no ncurses method to copy; the kitty keyboard protocol stays the one idea for the review row. Useful for later: the xterm mouse reporting (modes 1000 / 1002 / 1003 with the SGR form 1006) is what the console host's mouse input would use. `docs/roadmap.md` (the review row). Documentation only.

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

### e0f7de8 — 2026-10-31 02:05:11 +0100 — "I/O subsystem (phase 1) created by Claude Code"
32 files, +2352/-112.
- **I/O subsystem with virtual device drivers, phase 1** (owner's request, decisions of 2026-10-30: unknown prefix is an error, random access later with new keywords, `LOAD`/`SAVE`/`MERGE`/`CHAIN`/`FILES` through the drivers, `ADDRESS` reset by `RUN`, `\n` written and `\r\n` read, sandboxing left open).
  - **Design first**: `docs/io-subsystem.md` (goals, concepts, name resolution, statements, drivers, phases, decisions, the open sandboxing issue with the questions and the `iodev_allowed()` hook) and a roadmap section 8 / `docs/README.md` line, before any code.
  - **`iodev.[ch]`** (new, no SDL, no VM; `makefile`: object, header, rule): driver registry and `iodev_resolve()`; drivers `file` (stdio, `files_list()` for `list`, `unlink` for `remove`), `null`, `mem` (RAM disk, reference counted so a deleted open file stays readable); `iodev_read_all/write_all/exists/list`; `iotab_*` channel table (1..255, read buffer, `LOC`, `LOF`, `EOF`, lines with `\r\n` stripped, "File not open" / "Bad file mode" / "Input past end" ...). `basfiles.[ch]`: `files_format_entry()` split off for the mem driver; `FILES` now sorts with nanosecond times (the earlier "st_mtim is not available" claim was wrong: `stdtypes.h` defines `_POSIX_C_SOURCE 200809L`; docs and `CLAUDE.md` corrected).
  - **Language** (`bascomp.[ch]`, `.ebnf`, `bastok.[ch]`, `nodetype.[ch]`, `codegen.[ch]`, `cgenprog.c`, `cgendecl.c`, `instset.h`): keywords `OPEN CLOSE LINE WRITE ADDRESS EOF LOF LOC` (162..169); `OPEN` in both forms (`OUTPUT`, `APPEND`, `AS` recognized in context), `CLOSE`, `ADDRESS` (bare name, string expression, or alone = swap), `LINE INPUT` (a flag of `NT_INPUTSTMT`), `WRITE` (a flag of `NT_PRINTSTMT`, `PRTARG_WRITE`), `PRINT #n` / `WRITE #n` without a comma, functions `EOF LOF LOC INPUT$ ADDRESS$`; nodes `0x6B..0x6D`, instructions `OPEN CLOS INPW EOFN LOFN LOCN INPD ADDR ADRS` (`0x14C..0x154`).
  - **VM** (`vm.[ch]`): `vm_t.io`, `addrcur` / `addrprev`, `iofail`, `inerr`; `vm_out()` / `vm_get_line()` / `vm_get_key()` use the channel table for channels above 0 (no prompt, "Bad data in file", `INPUT #` reads on with the next line, no "?Extra ignored"); a new VM starts with `file` and `cleanup_vm()` closes the channels.
  - **REPL** (`basrepl.c`): `LOAD`, `MERGE`, `CHAIN` read through `iodev_read_all()` + `fmemopen()` (`repl_open_program()`), the extension search uses `iodev_exists()`, `SAVE` writes through `open_memstream()` + `iodev_write_all()` (and prints the reason), `FILES` calls `iodev_list()`; the old `repl_is_file()` is gone.
  - **Tests**: new `iotest1` (81 checks, in `make` and `.gitignore`), `vmtest1` 612 (+62: both `OPEN` forms, append, `INPUT #`, `LINE INPUT`, `WRITE`, `EOF`/`LOF`/`LOC`, `GET #`, `INPUT$`, all the errors, `CLOSE` forms, `ADDRESS`, parse errors; two old tests changed from the host's channel callback to `null:`), `repltest1` 273 (+26: `SAVE`/`LOAD`/`MERGE`/`CHAIN`/`FILES` through `mem:`, `ADDRESS`, channels over direct lines, `RUN`/`NEW` closing channels); `vmtest1`, `repltest1` and `iotest1` are clean under valgrind. `mem:` is cleared per test (`iodev_mem_clear()`).
  - **Docs**: `docs/io-subsystem.md` (status, "What phase 1 does, exactly"), `docs/basic-language.md` (new section), `docs/compiler-and-vm.md`, `docs/roadmap.md`, `bascomp.ebnf`, `CLAUDE.md`, `SESSION.md`.

### bf1a6f6 — 2026-10-31 02:10:08 +0100 — "BASIC demos fixed by Claude Code"
3 files, +17/-13.
- **Demos fixed** (`demos/ode.bas`): it used `LINE` as its loop variable, which is a reserved word since the I/O subsystem (`LINE INPUT`), so it stopped with "Numeric variable expected in line 70"; renamed to `V`. Checked all three demos by parsing (`basictest2`) and by running them through the REPL with a scratch harness (no errors; `sfx.bas` with keys 1 and Q). `prelude.bas` and `sfx.bas` needed no change.

### 3d35279 — 2026-10-31 02:20:05 +0100 — "I/O subsystem (phase 2) created by Claude Code"
24 files, +812/-33.
- **I/O subsystem, phase 2** (owner's request: "proceed with phase 2"; scope as in `docs/io-subsystem.md`)
  - **Drivers** (`iodev.[ch]`): `iodrv_t` got `rename`, `mkdir`, `chdir`, `seek`; `file` implements all four (`RENAME` refuses an existing target), `mem` has `rename` and `seek` (no holes), `null` seeks; new drivers `sh` (`popen`, asks `iodev_allowed( "shell" )`), `res` (read-only `./res`, absolute names and `..` refused, `list` through `files_list`), `con` (screen/keyboard through `iodev_set_console()` / `iocon_t` hooks); `iodev_remove/rename/mkdir/chdir()` (default driver, `iodev_allowed`, "Not the same device" across drivers), `iotab_seek()`; new errors "File already exists", "Not a directory", "Bad position".
  - **Language**: tokens `KILL RENAME MKDIR CHDIR SEEK` 170..174 (reserved words), one node `NT_FILEOPSTMT 0x6E` (`FOPK_*`), parser `comp_eat_fileopstmt()` (`bascomp.[ch]`, `.ebnf`), `cgen_from_fileopstmt()` (`cgenprog.c`), `cgen_gen_fop()`, instruction `FOP 0x155` (`instset.h`, `cgen_opcode_name`).
  - **VM** (`vm.c`): `vm_file_operation()`; `vm_console_key()` factored out of `vm_get_key()`; `vm_con_write/getkey` hand the host callbacks to the `con` driver (registered in `vm_open_channel()`).
  - **Tests**: `iotest1` 81 -> 128 (mem rename/kill/seek, file mkdir/chdir/rename/kill/seek in a scratch directory, sh read and write, res, con with hooks), `vmtest1` 612 -> 633 (+21), `repltest1` 273 -> 275; all other tests unchanged and passing; `iotest1`, `vmtest1`, `repltest1` clean under valgrind.
  - **Docs**: `docs/io-subsystem.md` ("What phase 2 does, exactly", driver table, phases), `docs/basic-language.md`, `docs/compiler-and-vm.md`, `docs/roadmap.md`, `bascomp.ebnf`, `CLAUDE.md`, `SESSION.md`.

### b5e747c — 2026-10-31 02:37:09 +0100 — "I/O subsystem (phase 3, additional drivers) created by Claude Code"
25 files, +1198/-52.
- **I/O subsystem, phase 3 and a proposal for drivers in BASIC** (owner's request: "proceed with phase 3, add the play and tcp drivers, make a suggestion about driver development support in BASIC")
  - **Random access and update** (`iodev.[ch]`): `IOMODE_RANDOM`, `iotab_open_ex()` (record length, buffer of spaces), `iotab_getrec/putrec/recbuf`, `LOC` of a random file = last record; the `file` driver opens READ|WRITE as `r+b`/`w+b`, `mem` does not truncate for update and `mem_seek` extends a writable file with zeros.
  - **Drivers**: `play` (line buffered, `iodev_set_music()` hook, the parser's message is passed on), `tcp` (client: `getaddrinfo`, non-blocking connect with a 10 s limit, `poll` slices that call the `iodev_set_interrupt()` hook for Break, `MSG_NOSIGNAL`); `iodev_fail()` carries a driver's own message ("Break", "Connection reset") to the channel functions; `iodev_known_error()` shared by open and the file operations.
  - **Language**: tokens `FIELD LSET RSET GETREC PUTREC` 175..179, `NT_RECORDSTMT 0x6F` (`RECK_*`), `comp_eat_recordstmt()`, `cgen_from_recordstmt()`, instructions `FLD 0x156`, `LRS 0x157`, `REC 0x158`; `OPEN` parses `FOR RANDOM|UPDATE`, `LEN = n` and a record length in the short form (extra branch, 4th operand of `OPEN`).
  - **VM** (`vm.[ch]`): `vm_field_t` bindings (`vm_t.fields`, dropped by `CLOSE`), `vm_field/vm_lset/vm_record`, `vm_music_play`, `vm_interrupt_hook`; a stale `fields` pointer crashed `vmtest1` once (not initialized in `init_vm`), fixed.
  - **Tests**: `iotest1` 128 -> 172 (records on mem and disk, play hook, a real loopback TCP connection with a listening socket in the test, refused/unknown host), `vmtest1` 633 -> 656, `repltest1` 275 -> 277; one old expectation changed (the OPEN mode error text); all suites pass, `iotest1`/`vmtest1`/`repltest1` clean under valgrind.
  - **Proposal** (not implemented): `docs/io-subsystem.md`, "Drivers written in BASIC": `DRIVER "name","PREFIX"` with labelled handlers, predefined `D*` variables, `DRIVERS`, `DRIVER TRACE`, examples, open questions; roadmap entry.
  - **Docs**: `docs/io-subsystem.md`, `docs/basic-language.md`, `docs/compiler-and-vm.md`, `docs/roadmap.md`, `instset.h`, `bascomp.ebnf`, `CLAUDE.md`, `SESSION.md`.

(The later commit `5bee279`, "added a simple file test in BASIC", is the owner's own: `residue/filetest1.e99`.)

### d65c408 — 2026-10-31 03:04:25 +0100 — "ON ERROR statements implemented by Claude Code"
19 files, +316/-30.
- **`ON ERROR GOTO|GOSUB`, `ON ERROR OFF`, `ERR$`, `ERL`** (owner's request; the owner corrected "ERL$" to the numeric `ERL`; drivers written in BASIC were put on the roadmap as postponed)
  - **Language**: tokens `ERR` 180 / `ERL` 181 (`bastok.[ch]`, highlighted as functions), `ON ERROR` parsed in `comp_eat_onstmt()` (`ERROR` is an identifier recognized after `ON`), `NT_ERRTRAPSTMT 0x70` (`ERRT_*`), `cgen_from_errtrapstmt()` (`cgenctl.c`, the target address goes on the return stack like `AFTER`), instructions `ERRT 0x159`, `ERL 0x15A`, `ERRS 0x15B`, `bascomp.ebnf`.
  - **VM** (`vm.[ch]`): `vm_error()` split into `vm_report_error()` (keeps the error for the handler instead of printing it when a handler is set; Break and the step limit excluded) and a one-line `vm_error()` that returns `false` (needed: gcc otherwise reports dozens of "maybe uninitialized" errors); `vm_resume()` checks `errcaught` after each step; `vm_trap_error()` empties the data stack, unwinds function contexts (`vm_context_t.rsp`), sets ERR$/ERL and enters the handler; GOSUB handlers return to the next line (`vm_next_line_addr()`) and re-arm the trap after `RETURN`; `vm_dpop*` initialize their outputs.
  - **Tests**: `vmtest1` 656 -> 670 (+13), `repltest1` 277 -> 278; all other suites pass; `vmtest1`/`repltest1` clean under valgrind.
  - **Docs**: `docs/basic-language.md` ("Error handling"), `docs/compiler-and-vm.md`, `docs/roadmap.md` (new section 9: `RESUME` etc.; drivers in BASIC marked postponed), `instset.h`, `SESSION.md`.

### 69aedf2 — 2026-10-31 03:37:36 +0100 — "Strings now permit NUL bytes, implemented by Claude Code"
27 files, +783/-160.
- **Binary strings and control code glyphs** (owner: "go ahead ... before that, add the control code glyphs to the font (0..31 first) ... prepare for the number packing functions")
  - **Font**: 32 new glyphs for the codes 0..31 in `8x12font1.txt` (section `#00`; two-letter mnemonics in a 3x5 pixel font between an overline and an underline), `8x12font1.c` regenerated with `fontcomp.pl`.
  - **Screen path** (length based, zero bytes are characters): `ansi_emit_t` got a `len` parameter and `ansi_feed_n()` (`ansi.[ch]`, also fixed the re-read of an aborting byte), `txtscr_print_n()` (`textscreen.[ch]`), `sdlscr_ansi_write()` (`sdlscreen.[ch]`), `ed_put()` in `fulledit.c`, `host_printn()` in `engine1999.c`; `vm_t.printn` / `repl_t.printn` (optional; without it `vm_out_bytes()` hands the pieces between the zero bytes to `print`).
  - **VM** (`vm.[ch]`): `vmval_t.len`; `vm_dpop_bstr()`, `vm_dpop_str()` refuses embedded zero bytes ("Illegal argument (zero byte in string)"), `vm_push_string_n()`, `vm_push_bytes()` (for the number packing step), `vm_out_bytes()`; length based: concatenation, comparison (`memcmp`, unsigned, prefix is smaller), `LEN`, `ASC`, `LEFT$`/`MID$`/`RIGHT$`, substring assignment, `LSET`/`RSET`, `FIELD`/`GETREC`, `INPUT`/`INPUT #` (rewritten with bounds)/`LINE INPUT`/`GET`/`INPUT$`, `PRINT`/`WRITE`, the `con:` driver; `CHR$(0)` allowed; `iotab_readline_n()` (`iodev.[ch]`).
  - **Tests**: `vmtest1` 670 -> 691 (zero byte cases in strings, comparisons, substrings, arrays, functions, files, `INPUT #`, records; the `CHR$ zero` case changed from an error to `<0>`; the test host got a `printn` that shows `<0>`), `txttest1` 43 -> 46, `ansitest1` +1; all pass, `vmtest1`/`repltest1`/`iotest1` clean under valgrind.
  - **Docs**: `docs/basic-language.md` ("Binary strings"), `docs/display-engine.md` ("Control code glyphs"), `docs/compiler-and-vm.md`, `docs/io-subsystem.md`, `docs/roadmap.md` (number packing is the next item), `instset.h`, `vm.h`, `SESSION.md`.

### daff040 — 2026-10-31 03:46:22 +0100 — "Number packing/unpacking for strings implemented by Claude Code"
16 files, +216/-27.
- **Number packing `MKI$ MKL$ MKS$ MKD$` / `CVI CVL CVS CVD`, big endian** (owner's request: network byte order)
  - **Language**: tokens 182..189 (`bastok.[ch]`, highlighted as functions), parsed in the existing `STR$`/`CHR$` branch (`comp_eat_sysstrfn2argcall`, needs `$`) and the `ASC`/`VAL`/`LEN` branch (`comp_eat_sysnumfn1argcall`), `cgen_gen_mki` ... `cgen_gen_cvd`, instructions `PACK 0x15C` / `UNPK 0x15D` (parameter 1..4 = I/L/S/D), `bascomp.ebnf`.
  - **VM**: `vm_pack()` / `vm_unpack()` assemble the bytes by shifts (big endian on any host), floats via `memcpy`, built on `vm_push_bytes()`; ranges: `MKI$` -32768..65535, `MKL$` -2^31..2^32-1, else "Illegal argument"; `CVI`/`CVL` signed; short strings refused, extra bytes ignored.
  - **Tests**: `vmtest1` 691 -> 710 (+19: byte order of each width, round trips, ranges, errors, records, files, a loop), `repltest1` 278 -> 279; all suites pass, `vmtest1`/`repltest1` clean under valgrind.
  - **Docs**: `docs/basic-language.md` ("Number packing"), `docs/compiler-and-vm.md`, `docs/io-subsystem.md`, `docs/roadmap.md` (done; binary helpers listed as next), `instset.h`, `SESSION.md`.

### 3a23508 — 2026-10-31 03:55:01 +0100 — "Graphics subsystem proposal by Claude Code"
5 files, +172/-8.
- Owner's request: look at `sdllayer` and its users and suggest BASIC statements and functions (no code changed). Read `sdllayer`, `sdlscreen`, `textscreen`, `tilescreen`, `sprscreen`, `concept.txt`; found the gaps (the tile screen cells can't be written, no palette/layer/backdrop access above `sdllayer`, no drawing code for the background and graphics layers, no sprite collision, no BASIC access to any of it besides text output).
- New `docs/graphics-subsystem.md` (layers, principles, three phases, timing, backend work, decisions for the owner), `docs/README.md` index, `docs/roadmap.md` section 10.

### 6120896 — 2026-10-31 04:01:58 +0100 — "Graphics subsystem proposal by Claude Code"
4 files, +97/-11.
- On the owner's requirement that all graphics calls go through `sdlscreen`: a principle, a section specifying the internal `sdlscr_*` API (layers/palettes, bitmap drawing, text incl. viewports, tiles, sprites, timing) and the changed build order; checked with grep that `fulledit.c` and `monitor.c` (not `engine1999.c`) bypass `sdlscreen` today.

### 37d8dde — 2026-10-31 04:24:26 +0100 — "Graphics subsystem proposal by Claude Code"
4 files, +109/-6.
- On the owner's request, the proposal was amended with sprite and tile definitions modelled on Simons' BASIC (`DESIGN` with `@ "row"` statements, row alphabet, color base, read-back) and a table of further Simons' BASIC commands mapped to our words (sources: c64-wiki and Wikipedia, one-line descriptions only; the manual PDF could not be read, so the argument syntax is our own design); the `sdlscr_*` API got the matching functions.

### fef326a — 2026-10-31 05:03:21 +0100 — "Graphics subsystem fleshed out and integrated into BASIC, all by Claude Code"
48 files, +3714/-407.
- **Graphics subsystem implemented** (owner's decisions: palette indices per layer, `GFX TARGET` 0/3, few reserved words, own asset formats, conversion tools and sprite/tile editors later, "implement it all")
  - **Rule**: every call into the graphics modules goes through `sdlscreen`: the internal `sdlscr_*` API (layers, palettes, backdrop, bitmap drawing, text screen incl. viewports, tiles, sprites, `sdlscr_vsync`) at the end of `sdlscreen.h`; `fulledit.c` and `monitor.c` converted from `txtscr_*` to `sdlscr_text_*`.
  - **New modules**: `gfxcore.[ch]` (pixels with modes, line, box, circle/arc/ellipse, paint, radius, grab/blit, text, GW-style DRAW; no SDL), `gfxops.[ch]` (the table of statements and functions, `GFXOP_*`, `GFXQ_*`).
  - **Backends**: `tilescreen` (cell setter, `tilescr_pixel`, `readtile`), `sprscreen` (fixed image, position/image read-back, `moveto`, collision flags, per-pixel sprite-sprite and sprite-tile collisions, latched hits, events), `textscreen` (`fill/copy/invert/save/restore/defchar/font`; **fixed**: `txtscr_write` sign-extended characters >= 128 into the colors), `sdllayer` (color names, palette reset).
  - **Language**: tokens 190..199 (`GFX SPRITE TILE TXT DESIGN RGB POINT CLS LOCATE COLOR`; `keyword_name()` in `bastok`), `comp_eat_gfxstmt`/`comp_eat_gfxquerycall`/`comp_eat_designstmt` (table driven; `ON COLLISION` in `comp_eat_onstmt`), nodes `0x71..0x75`, `cgen_from_gfxstmt/gfxquery/designstmt/designrow/oncollstmt`, instructions `GFXC 0x15E`, `GFXQ 0x15F`, `DSGN 0x160`, `DROW 0x161`, `COLT 0x162`, `bascomp.ebnf`.
  - **VM**: `vm_gfx_t` (`op`/`query`, `vm_t.gfx`, `repl_t.gfx`), `vm_gfx_command/check/query`, `vm_design_start/row`, `vm_set_collision`; the collision handler is timer slot 4 (`VM_COLSLOT`, user timers `VM_NUSERTIMERS`); `RGB()` computed in the VM.
  - **Host** (`engine1999.c`): `host_gfx_op`/`host_gfx_query` map every operation to `sdlscr_*`.
  - **Tests**: new `gfxtest1` (104 checks; also valgrind clean), `vmtest1` 710 -> 807 (a logging gfx test host: every statement, word, default, range error, `DESIGN`, `ON COLLISION`), `repltest1` 279 -> 281; `makefile` and `.gitignore` have `gfxtest1`. Verified on the real screen under Xvfb with XTest key injection: `demos/gfx.bas` (backdrop, tile checker with its own palette, lines, circle, arc, painted box, scaled text, `DRAW`, DESIGN sprite moving by `MOVETO`, text bar), and the control code glyphs, binary strings and `MKS$` on the text screen.
  - **Demo and docs**: `demos/gfx.bas` (+ `demos/README.md`), `docs/graphics-subsystem.md` rewritten from the proposal to the implemented design (statement reference, functions, `DESIGN`, collision rules, Simons' BASIC mapping, decisions, open items), `docs/basic-language.md`, `docs/compiler-and-vm.md`, `docs/display-engine.md`, `docs/roadmap.md` (section 10: tools, editors, input, synchronization), `instset.h`, `CLAUDE.md`, `SESSION.md`.

### 8b0009f — 2026-10-31 05:15:10 +0100 — "GFX RESET statement, implemented by Claude Code"
16 files, +139/-14.
- **`GFX RESET [ALL]`** (owner's request: reset the graphics subsystem to its defaults, wipe the screen of clutter; palettes too)
  - `GFXOP_RESET 21` (`gfxops.[ch]`, signature `W` with the word `ALL`), host case in `engine1999.c`, `sdlscr_gfx_reset( images )` (`sdlscreen.[ch]`): layers shown in the original order, all palettes and the backdrop back to the table (`SDLSCR_DEFAULT_BGCOL`), background and graphics bitmaps cleared, target/pen/mode/DRAW state, tiles and sprites reset; `ALL` also clears tile and sprite images, animation sequences and restores the font.
  - Backends: `tilescr_reset`, `sprscr_reset`, `txtscr_resetfont`. The text screen is not touched.
  - Tests: `gfxtest1` 104 -> 111, `vmtest1` 807 -> 810 (`GFX RESET`, `GFX RESET ALL`, a bad word); checked on the real screen under Xvfb after `demos/gfx.bas` (backdrop, tiles, graphics and sprite gone, text kept).
  - Docs: `docs/graphics-subsystem.md` ("GFX RESET"), `SESSION.md`.

### 97f646a — 2026-10-31 05:23:34 +0100 — "GFX RESET statement, implemented by Claude Code"
8 files, +40/-11.
- **`GFX RESET` fix: the text pen** (owner's report: after `GFX RESET` the text pen was white, not black)
  - Cause: the text screen was deliberately left out of the reset, so the colors a program set with `COLOR` (the demo uses `COLOR 140`) or an ANSI sequence survived. Reproduced on the real screen (fresh start: black; after `color 140` and `gfx reset`: white).
  - `txtscr_resetcolors()` (`textscreen.[ch]`: pen, paper and the blank colors of the selected screen to `TXTSCR_FGCOL`/`TXTSCR_BGCOL`, contents stay), called by `sdlscr_gfx_reset()` which also restarts the ANSI decoders (`sdlscr_ansiok`); `gfxtest1` 111 -> 113; docs: `docs/graphics-subsystem.md`, `sdlscreen.h`, `SESSION.md`.

### 7ab2cd5 — 2026-10-31 05:49:42 +0100 — "Fruit invaders demo game, implemented by Claude Code"
7 files, +615/-7.
- **Fruit Invaders demo and a parser fix** (owner's request: a Space Invaders like game with suitable audio effects, keyboard controlled)
  - `demos/fruit.bas` (582 lines, listing generated by a scratch script that numbers the lines and writes the `DESIGN` rows): 5 x 8 formation of cherries, apples and oranges with two animation frames, bombs, shields on the graphics layer that crumble (`POINT`), a pineapple UFO, waves, lives, extra lives, a high score, a starfield from `DESIGN TILE`s, `SFX`/`SOUND`/`PLAY` audio, pause/quit, title and game over screens, `GFX RESET` on exit. The language has no `RND`/`INT`/`ABS`/`\`: it uses `Q%` for integer division and its own random number generator. `demos/README.md` lists it.
  - **Bug found and fixed**: `IF c THEN A=2` (an assignment without `LET` after `THEN`) was read as a jump to the label `A` and failed with "End of line expected"; `comp_eat_singlelineifstmt()` (`bascomp.c`) now takes an identifier after `THEN` as a label only when nothing but `:`, `ELSE` or the end follows. `vmtest1` 810 -> 816 (assignment, assignment with ELSE, array element, skipped when false, labels still work).
  - Checked on the real screen under Xvfb with key injection (title screen, play, pause, quit, game over, wave 2, the pineapple) and the audio with SDL's disk driver (the effects and jingles produce sound; no errors).
  - `docs/roadmap.md`: numeric functions and operators (`RND`, `INT`, `ABS`, ... `MOD`, `\`) and a key state function for games (no cursor keys or key-up events in `INKEY$`) added; `SESSION.md`.

### 11fb391 — 2026-10-31 05:55:52 +0100 — "PRINT color bug fixed by Claude Code"
4 files, +17/-8.
- **`COLOR` fix** (owner's report: the description text on the entry screen of `fruit.bas` was black)
  - Not the demo's color: `PRINT` goes through the ANSI decoder of the text screen (`sdlscr_ansi_write`), which keeps its own current colors from the time of its first use, so `COLOR` (only the text screen's pen) did nothing for `PRINT` once something had been printed; a fresh start hid it. Reproduced on the real screen (`print`, `color 140`, `print`: black).
  - `sdlscr_text_setcolors()` (`sdlscreen.c`) restarts the screen's decoder from the new colors (like `sdlscr_gfx_reset`); ANSI sequences still work. Verified on the real screen (white, yellow, an ANSI red, and the demo's entry screen after a warm-up `print`). SDL-bound, so no automated test. `docs/graphics-subsystem.md`, `SESSION.md`.

### b45a968 — 2026-10-31 06:08:37 +0100 — "Numeric BASIC statements, implemented by Claude Code"
22 files, +899/-581.
- **Numeric functions** (owner's request: RND, INT, ABS, SIN and the other numeric functions)
  - **Functions and operators**: `INT FIX ABS SGN SQR SIN COS TAN ATN EXP LOG RND PI`, `RANDOMIZE [seed]`, `\` (integer division) and `MOD`. Tokens 200..213 (`bastok.[ch]`, highlighted as functions / operator), the functions parse in `comp_eat_sysnumfn1argcall()` (`RND` alone is `RND(1)`) and `PI` as a no-argument call, `\` and `MOD` join `*`, `/`, `^` in `comp_eat_nummultex()`, `comp_eat_randomizestmt()` (`NT_RANDOMIZESTMT 0x76`); code generator: one instruction `MATH 0x165` with the kind as parameter, `RAND 0x166`, `IDIV 0x163`, `MOD 0x164`, `cgen_from_randomizestmt()`.
  - **VM**: `vm_math()` (errors "Illegal argument" for `SQR`/`LOG`, "Overflow" for non-finite results), `vm_randomize()`, xorshift64* seeded from the clock in `init_vm()` and by `RND(-n)` / `RANDOMIZE n`; `\`/`MOD` in `vm_binary_num()` (truncate both operands, "Division by zero").
  - **Tests**: `vmtest1` 816 -> 854 (+38: every function, domains, `RND` range/repeat/reseed/spread, `RANDOMIZE`, `\`, `MOD`, precedence), `repltest1` 281 -> 282 (listing); `demos/fruit.bas` now uses `RND`/`INT`/`\`/`RANDOMIZE` instead of its workaround and was run again on the real screen.
  - **Docs**: `docs/basic-language.md` ("Numeric functions"), `docs/compiler-and-vm.md`, `docs/roadmap.md` (done; further functions listed), `instset.h`, `bascomp.ebnf`, `demos/README.md`, `SESSION.md`.

### dd3fac4 — 2026-10-31 06:15:20 +0100 — "DEG/RAD/GRAD, implemented by Claude Code"
19 files, +136/-15.
- **`DEG`, `RAD`, `GRAD`** (owner's suggestion: choose how angles are represented, default radians)
  - Tokens 214..216, `comp_eat_anglestmt()`, `NT_ANGLESTMT 0x77`, `cgen_from_anglestmt()`, instruction `ANGM 0x167`, `vm_t.angmode` (2 = radians in `init_vm()`), `vm_trig()` (SIN/COS/TAN of degrees and gradians, quarter turns exact, `TAN(90)` "Overflow"), `ATN` converts its result; the graphics angles stay in degrees.
  - Tests: `vmtest1` 854 -> 867 (every function in each unit, big/negative angles, the unit inside a program, graphics angles unchanged), `repltest1` 282 -> 285 (listing, unit over direct lines, `RUN` back to radians).
  - Docs: `docs/basic-language.md` ("Angle units"), `docs/compiler-and-vm.md`, `instset.h`, `bascomp.ebnf`, `SESSION.md`.

### 953e2b3 — 2026-10-31 06:16:38 +0100 — "Claude state update"
2 files, +10/-6.
- `HistoryLog-Claude.md` (moved the DEG/RAD/GRAD entry into the log) and `SESSION.md` (repo state, test counts). No source changes.

### 4b1cedd — 2026-10-31 09:46:29 +0100 — "Implemented some ideas from AsmBASIC, implemented by Claude Code"
19 files, +418/-16.
- Read `../asmbasic` and its docs; `docs/roadmap.md` got section 11 with everything worth taking over (procedures with `LOCAL`, `RESUME`, `STOP`/`CONT`/`TRACE`/`AUTO`, logical operators and shifts, more math functions, `PEEK`/`POKE`/`SYS`, `PRINT USING`, `TIME$`/`DATE$`, aliases, open questions).
- Implemented `EXIT [n]`, `LEAVE [n]`, `ITERATE [n]`, `FOR … DOWNTO … [STEP]`, `FOR EACH [key,] v IN a()`: tokens 217..219, nodes `NT_FOREACHSTMT 0x78` / `NT_EXITSTMT 0x79`, instructions `ACNT 0x168` / `AFLT 0x169`; `bastok`, `bascomp` (+ `.ebnf`), `cgenctl.c` (loop jump list, `cc_resolve_loopjumps`), `codegen`, `vm.c`, `instset.h`; docs (`basic-language.md`, `compiler-and-vm.md`, `roadmap.md`), `SESSION.md`.
- Tests: `vmtest1` 908 cases (41 new; also under ASAN, which caught a double free in my first `AFLT`: fixed), all other test programs pass.

### bcf5b5e — 2026-10-31 10:16:07 +0100 — "Various additions (check documentation), implemented by Claude Code"
19 files, +946/-43.
- **Small additions**: `FLOOR CEIL LOG2 LOG10`, `IMP NIMP EQV NEQV`, `SHL SHR ASL ASR`, `TIME$ DATE$ DATETIME$` (tokens 220..234, instructions `URSH 0x16A`, `IMP 0x16B`, `NIMP 0x16C`, `DTIM 0x16D`, `MATH` kinds 14..16).
- **`PROC` / `FUNC` / `CALL` / `LOCAL`**: tokens 235..238, nodes 0x7A..0x7C, `LOCL 0x16E`; `RETURN expr`, `EXIT PROC|FUNC|FN`; recursion; `vm_context_t.saved` became a heap list; `bascomp.c` (`comp_eat_procstmt/callstmt/localstmt`), `cgendecl.c` (`cgen_from_procstmt/callstmt`), `cgenctl.c` (`LOCAL`, proc exits), `vm.c` (`vm_local`).
- **`RESUME`**: token 239, `NT_RESUMESTMT 0x7D`, `RSM 0x16F`; table of statement addresses in the code generator (`cgen_find_stmt`), `vm->steppc`/`errpc`/`resume*`, `vm_resume_error()`.
- Tests: `vmtest1` 908 -> 969 (+18 small additions, +25 PROC/FUNC, +15 RESUME; clean under ASAN/UBSAN), `repltest1` 286, the other test programs pass. Docs: `basic-language.md`, `compiler-and-vm.md`, `roadmap.md`, `instset.h`, `SESSION.md`.

### 90500ef — 2026-10-31 10:40:54 +0100 — "Various additions (check documentation), implemented by Claude Code"
25 files, +1003/-64.
- **History log**: the entry of `bcf5b5e` (small additions, PROC/FUNC, RESUME) moved into the log.
- **Array parameters and local arrays**: `vars.[ch]` (`vmem_array_alias/fresh/restore`, `var_t.aliased`), `vm.c` (`vm_savedvar_t.isarray`, call/local/unwind), `bascomp.c` (`comp_eat_callargs`, `comp_make_param_arrays`, `LOCAL` items), `cgendecl.c` (`cgen_check_call_args`), `codegen.c`.
- **STOP / CONT / TRACE / AUTO**: tokens 240..242, nodes 0x7E..0x81, instructions 0x170..0x173, `vm_stopstate_t` (stacks moved on STOP/Break), `codegen_t.progsize`, `repl_program_changed()`, `repl_t.trace/autoline/autostep`, `VMREQ_AUTO`; the AUTO prompting in `engine1999.c` (tried on the real screen under Xvfb).
- **PEEK / POKE / PEEK$**: tokens 243/244, `NT_POKESTMT 0x82`, instructions 0x174..0x176, `vm_t.ram` (1 MiB).
- Tests: `vmtest1` 969 -> 1011 (+17 arrays, +5 STOP/TRACE, +20 PEEK/POKE), `repltest1` 286 -> 302 (STOP/CONT/TRACE/AUTO); both clean under ASAN/UBSAN; the other test programs pass. Docs: `basic-language.md`, `compiler-and-vm.md`, `roadmap.md`, `instset.h`, `SESSION.md`.

### b1c9e18 — 2026-10-31 10:47:55 +0100 — "EBNF update by Claude Code"
4 files, +158/-31.
- **History log**: the entry of `90500ef` (array parameters, STOP/CONT/TRACE/AUTO, PEEK/POKE) moved into the log.
- `bascomp.ebnf`: added everything the parser has learned since the last update: the operators (`\`, `MOD`, `SHL SHR ASL ASR`, `IMP NIMP EQV NEQV`), the numeric and string functions (`INT`..`LOG10`, `RND`, `PI`, `CV*`, `MK*$`, `PEEK`, `PEEK$`, `TIME$ DATE$ DATETIME$`), the graphics functions and statements (written out from the table in `gfxops.c`: `GFX`, `TXT`, `TILE`, `SPRITE`, `DESIGN`, `@`, `CLS`, `LOCATE`, `COLOR`, `SYMBOL`, `ON COLLISION`), call arguments with array arguments, `PROC`/`FUNC`/`CALL`/`LOCAL`, `RETURN expr`, `EXIT PROC`, `RESUME`, `STOP`/`CONT`/`TRACE`/`AUTO`, `POKE`; the orphan rules (`OPEN`, `CLOSE`, `ADDRESS`, `FIELD`.., `ON ERROR`, `RANDOMIZE`, `DEG`.., file operations) are reachable from `stmt` now. Fixed three undefined names (`num-expr-list`, `sys-no-arg-num/str`) and defined `identifier`. Checked with a script: every production is defined. Ends with a list of what a grammar cannot say (words recognized in context, lookahead, the code generator's checks). `docs/basic-language.md`: the remark about inconsistent grammar items replaced. No code changes.

### 1e853e3 — 2026-10-31 10:57:50 +0100 — "Grammar documentation, created by Claude Code"
9 files, +1493/-31.
- **Grammar quick reference**: `grammar.pl` (reads `bascomp.ebnf`; `--check` verifies), `docs/grammar.md` (generated: sections, fenced `ebnf` rules with the tokens shown as written, the comments as prose, contents list) and `docs/grammar.html` (generated: colors, light/dark, links from each rule name to its definition); `makefile` targets `grammar` and `grammar-check`.
- `bascomp.ebnf`: `--# Title` section markers, the tail regrouped by topic, a statement-groups summary, `identifier-AS` / `identifier-ERROR` -> `AS` / `ERROR`, notes reworded.
- Rule kept: `CLAUDE.md` and `SESSION.md` say to update the EBNF and run `make grammar` when the language changes; `docs/README.md` and `docs/build-and-test.md` list the new files. No code changes; `make grammar-check` and `vmtest1` pass.

### 9af4054 — 2026-10-31 11:34:08 +0100 — "Fruit Invaders II demo, created by Claude Code"
9 files, +1472/-12.
- **History log**: the entries of `b1c9e18` (EBNF update) and `1e853e3` (grammar documentation) moved into the log (the grammar entry had been lost in the pending list and was rewritten).
- **New**: `demos/fruit2.bas` (Fruit Invaders II, see `SESSION.md`), a line in `demos/README.md`, `fruit2.hi` in `.gitignore`.
- **Fix**: `RESUME <target>` after an error inside a `PROC`/`FUNC` call works (`vm.c`, `vm_resume_error()`: only `RESUME` and `RESUME NEXT` are refused there, as the docs said); 2 tests in `vmtest1` (1013).
- **Fix**: the `grammar` / `grammar-check` targets were the first rules of the `makefile`, so `make` without arguments built only the grammar files; they come after `all` now (found when `vmtest1` was not rebuilt).
- Tests: `vmtest1` 1013 all pass; the demo ran headless for 100000 interrupt polls without an error and was looked at on the real screen (title, play, pause, quit).

### e7ba7c5 — 2026-10-31 12:27:43 +0100 — "The Long Climb demo game, created by Claude Code"
5 files, +2178/-9.
- **New**: `demos/climb.bas` (the jump and run demo "The Long Climb", see `SESSION.md`), a line in `demos/README.md`, `climb.hi` in `.gitignore`.
- Tests: no code changed; the demo was run headless (position traces for jump, ladder, drilling, a monster trapped and crushed, respawn; random keys 3 x 40000 polls without an error) and looked at on the real screen under Xvfb (title, level start, walking, jumping, climbing, the summit with the stand, the win screen, a death by a car and the game over screen, back to the title).

### edd42c7 — 2026-10-31 12:28:05 +0100 — "Documentation change, by Claude Code"
1 file, +14.
- `docs/graphics-subsystem.md`: a section on scrolling a world bigger than the screen with `TILE SCROLL` (the cell mapping that gives exact pixel positions, the top strip, the cost of a changed layer). (Part of the same work as `e7ba7c5`; committed separately.)

### a75599f — 2026-10-31 13:45:32 +0100 — "EVERY/AFTER handler triggering fixed by Claude Code"
5 files, +26/-11.
- **History log**: the entries of `e7ba7c5` (The Long Climb) and `edd42c7` (documentation) moved into the log.
- **Fix** (`vm.c`, `vm_irq_poll()`): an `EVERY`/`AFTER` handler was only entered when no function call was open; since procedures exist that meant never, in programs whose main loop is a `PROC` (the music of `climb.bas` stopped after one bar, the music and the pineapple of `fruit2.bas` too). A handler is held back only while a statement is unfinished (data stack not empty).
- Tests: 4 new cases in `vmtest1` (1017; three failed before the fix); `repltest1` 302 and the other test programs pass. Docs: `docs/basic-language.md` (Timers), `SESSION.md`.

### dbce006 — 2026-10-31 14:01:31 +0100 — "Claude Code session update"
2 files, +8/-6.
- Bookkeeping only: `HistoryLog-Claude.md` (the entry of `edd42c7`/`a75599f` moved into the log) and `SESSION.md` (latest commit line `a75599f`). No source changes.

### ed94d35 — 2026-10-31 16:57:19 +0100 — "BUILD statement, created by Claude Code"
111 files, +1852/-204.
- **`BUILD` statement and the Engine1999 Runtime Exception** (2026-10-31, owner's request: an executable that statically binds `engine.lib`, made with the system linker only; programs made with Engine1999 must not fall under the GPL).
  - **License**: new `LICENSE-EXCEPTION` (a GPLv3 section 7 additional permission: programs in the built-in language are not a work based on the Engine; a Standalone Executable made by `BUILD` may be conveyed under terms of the author's choice if the runtime is unmodified, its notice stays and `--engine1999-about` shows it; the runtime itself stays GPL; SDL2 and the C library keep their licenses). The vague `NOTE: Programs created with a built-in programming language …` in the header of 87 files (`.c`, `.h`, `makefile`, `bascomp.ebnf`, `concept.txt`) was replaced by a 4-line `ADDITIONAL PERMISSION` note that points to it (`grammar.pl` got a 2-line one); `LICENSE` (the GPL text) is untouched; `README.md`, `CLAUDE.md`, `demos/README.md` updated; every demo starts with four license comment lines (lines 1..4). The owner's banner change in `engine1999.c` (`cc2edbf`) was left as it is.
  - **Language**: `BUILD name$` (token 245, `NT_BUILDSTMT 0x83`, instruction `BLD 0x177`; parser `comp_eat_buildstmt`, `cgen_from_buildstmt`, `vm_build_program`, host callback `vm_t.build`, implemented by the REPL itself: `repl_vm_build()`); refuses `.bas`/`.e99` names and drivers other than `file:`. `bascomp.ebnf` has `build-stmt`; `make grammar` run, `make grammar-check` passes.
  - **`basbuild.[ch]`** (new, no SDL/VM): `build_executable()` finds `ld` (`$ENGINE1999_LD`, PATH, usual dirs), the runtime kit (`engine1999.o`, `engine.lib`: dir of the running executable, `$ENGINE1999_HOME`, `.`, `/usr/{local/,}{lib,share}/engine1999`), `crt1.o/crti.o/crtn.o` and the libraries (`$LIBRARY_PATH`, multiarch dirs, `lib64`, `lib`, `/usr/local/lib`; `libX.so`, else `libX.so.*`, else `libX-*.so*`), the dynamic linker per machine, the compiler's `crtbegin.o/crtend.o/libgcc.a` (newest version); writes the program to a `mkdtemp` directory as `program.e99`, runs `ld -r -b binary` and the final `ld` (fork/exec, no shell; SDL2, libm, libc and rt/pthread/dl if present are linked dynamically), removes its files; errors name what is missing. `ENGINE1999_BUILD_TRACE=1` prints the commands.
  - **Host**: `engine1999.c` `main()` runs the embedded program when the weak symbol `_binary_program_e99_start` exists (`run_embedded()`: no banner/REPL, ends when the program ends after queued sound (10 s at most), waits for a key after an error that was not a Break, `--engine1999-about` prints the notice and exits); `basrepl.c` got `repl_program_image()` and `repl_load_image()`.
  - **Tests**: new `buildtest1` (27 checks; stand-in runtime `buildstub.c`; also links the real runtime with SDL2; clean under ASAN/UBSAN), `vmtest1` 1025 (8 new), `repltest1` 308 (6 new); all other tests pass. Verified on the real screen under Xvfb + metacity: a small program (text, color, `PLAY`, `WAIT`), an error program ("Press any key to end the program."), and `fruit2.bas` built and started as an executable (title screen); `ldd` shows only the shared libraries; the real build links in a few hundred milliseconds. Makefile: `basbuild.o`, `buildstub.o`, `buildtest1` (needs `engine1999.o`, which stays outside the library), `.gitignore` got `buildtest1`.
  - **Docs**: `docs/basic-language.md` (section "`BUILD`"), `docs/compiler-and-vm.md`, `docs/build-and-test.md`, `docs/roadmap.md`, `docs/grammar.*`, `instset.h`, `SESSION.md`.

### b9787cb — 2026-10-31 17:14:28 +0100 — "SHOW COPYING statement and --engine1999-about modified by Claude Code"
4 files, +119/-13.
- **Exception shown by `SHOW COPYING`** (2026-10-31, owner's request): `engine1999.c`: the `copying` text now ends with the full text of `LICENSE-EXCEPTION` (embedded; keep in sync); the start banner says that programs and `BUILD` executables are covered by the Engine1999 Runtime Exception (the owner's earlier wording "do not fall under this license" replaced); `--engine1999-about` lists the conditions in short. `make` and `buildtest1` pass; not looked at on the screen.

### 7f94f47 — 2026-10-31 17:15:51 +0100 — "Claude Code state update"
2 files, +6/-2.
- Bookkeeping only: `HistoryLog-Claude.md` (the entry of `b9787cb` moved into the log) and `SESSION.md` (latest commit line `b9787cb`). No source changes.

### fc0507f — 2026-10-31 17:16:38 +0100 — "Claude Code state update"
1 file, +4.
- Bookkeeping only: `HistoryLog-Claude.md` (the entry of `7f94f47` added to the log). No source changes.

### b7f6029 — 2026-10-31 17:48:34 +0100 — "README.md update and associated files, generated by Claude Code"
9 files, +155/-10.
- **README and contributor page** (2026-10-31, owner's request): `README.md` rewritten for players and BASIC programmers (what it is, status "work in progress but possibly useful", features, quick start, demos, `BUILD`, the license with the exception in plain words, links, credits naming Ekkehard Morgenstern and Claude with a link to this log); new `docs/contributing.md` (who contributed, build and tests, the checklist for a new statement, conventions, the license of contributions: the author must agree before code is accepted, because he grants the exception); `docs/README.md` intro brought up to date and linked to it. Four screenshots in `docs/images/` (`fruit2-game.png`, `climb.png`, `graphics.png`, `listing.png`), made under Xvfb + metacity from executables built with `BUILD` (key presses by an XTest helper in the scratchpad: not in the repo). No source changes. **The statement about contributions needing the author's agreement is my proposal**, to be confirmed.

### 0a95461 — 2026-10-31 18:22:56 +0100 — "Attempt to work around Github problem with contributor page, created by Claude Code"
3 files, +20/-10.
- **Links in `docs/contributing.md` made absolute** (2026-10-31, owner's report: the links do not work in GitHub's "Contributing" tab, which shows `docs/contributing.md` as the repository's contributing guide and does not resolve relative links; the normal file view does). All links of that file point to `https://github.com/ekkehard-morgenstern/Engine1999/blob/main/...` now, with a comment saying why; the other docs keep relative links. Not checked in the tab itself (only the URLs were checked to exist). No source changes.

### dae3c97 — 2026-10-31 18:33:07 +0100 — "Versioning script, adapated from AsmBASIC by Claude Code"
1 file, +95.
- **`bump_version.sh` adapted** (2026-10-31, owner's request; the script was copied from AsmBASIC): names and messages say Engine1999 instead of AsmBASIC (header, the default text of `--annotate`), and the build hints (`tagtoverstr.pl` / `verstr.inc`, which this project did not have) are gone; the logic (versions `vMAJOR.MINOR.BUILD` from the highest tag, `--build/--minor/--major/--set/--annotate/--dry-run/--force/--show`, never pushes) is unchanged. Checked with `--show` and `--set 0.5.0 --dry-run` (read-only; creating the tag is the owner's git write). Suggested first version: v0.5.0 (the owner tagged it).

### 15329fc — 2026-10-31 18:39:01 +0100 — "Version support created by Claude Code"
10 files, +86/-7.
- **Version recognition in the build** (2026-10-31, owner's request, like AsmBASIC's `tagtoverstr.pl`): new `tagtoverstr.sh` (shell, not perl: perl is not needed to build) writes `version.h` (`#define ENGINE_VERSION "..."`) from `git describe` of the newest `vX.Y.Z` tag: `v0.5.0` on the tagged commit, `v0.5.0-1-gdae3c97` after it, `untagged-<hash>` without a tag, `unknown` without git (an existing header is kept); the header is only rewritten when the string changes. `makefile`: rule `version.h` (depends on `.git/HEAD`, `.git/logs/HEAD`, `.git/refs/tags`, `.git/packed-refs`), only `engine1999.o` depends on it (not in `BASEHDR`, so a commit recompiles and relinks `engine1999` and nothing else); `.gitignore` has `version.h`. `engine1999.c`: the version is in the start banner and in `--engine1999-about` (so in built programs: the runtime's version), and the new option `--version` prints it. The branch-name rule of the AsmBASIC script (a branch called `vX`) was left out. `bump_version.sh` header comment updated. Checked: the four cases of the script in a scratch repository (no git, no tag, tagged, after the tag), `make` twice (no rework), `engine1999 --version`, a built program's `--engine1999-about`, `buildtest1` 27/27; the banner not looked at on the screen. Docs: `docs/build-and-test.md` ("Version"), `docs/basic-language.md`, `CLAUDE.md`.

### 6b71639 — 2026-10-31 18:41:09 +0100 — "Claude Code session update"
2 files, +6/-2.
- Bookkeeping only: `HistoryLog-Claude.md` (the entry of `15329fc` moved into the log) and `SESSION.md` (latest commit line `15329fc`, the tags `v0.5.0` and `v0.5.1`). No source changes.

### 6be8835 — 2026-10-31 19:05:05 +0100 — "BYE/QUIT statements, roadmap entry for scripting support, all created by Claude Code"
25 files, +193/-14. Tagged `v0.5.2` by the owner.
- **`BYE` / `QUIT` statements** (2026-10-31, owner's request; the owner first asked for `BYE` and `EXIT`, then noticed that `EXIT` is the loop word and asked for `QUIT` instead: a first version that turned a direct-mode `EXIT` into `BYE` was removed again): `BYE [code]` and `QUIT [code]` (tokens 246 / 247, one node `NT_BYESTMT 0x84`, instruction `BYE 0x178`, `vm_quit_request()`: the code must be a whole number 0..255, else "Illegal argument"; `VMREQ_QUIT`, `repl_t.quit` / `exitcode`, `repl_finish()`), `bascomp.ebnf` has `bye-stmt` (`make grammar` run, `grammar-check` passes). Host: `engine1999.c` leaves its main loop (and `AUTO` prompting) when `repl.quit` is set and returns the code from `main()`; `run_embedded()` returns it at once (built programs: `./prog; echo $?`). Tests: `vmtest1` 1044 (19 new), `repltest1` 322 (the harness stops entering lines at a quit and appends `<quit n>`). Checked on the real screen under Xvfb: typing `quit 4` in the Engine gives exit status 4; built executables with `BYE 5` / `QUIT 5` return 5, also after a `WAIT`. (A hang I saw on the way was a stale test helper of mine that did not know the new words, not the engine.) **Roadmap section 12** (new): running Engine1999 without a GUI for scripts, with the analysis (the core is SDL-free; what a console host, `#!` lines, program arguments, standard channels, display statements and a headless `BUILD` kit would need). **Fix of an earlier omission:** the paragraph on `BUILD` that the `ed94d35` entry lists for `docs/compiler-and-vm.md` had never been written; it is there now. Docs: `docs/basic-language.md`, `docs/compiler-and-vm.md`, `docs/roadmap.md`, `docs/grammar.*`, `instset.h`, `vm.h`.

### a584183 — 2026-10-31 19:09:55 +0100 — "Claude Code state file update"
2 files, +6/-2.
- Bookkeeping only: `HistoryLog-Claude.md` (the entry of `6be8835` moved into the log) and `SESSION.md` (latest commit line `6be8835`, tag `v0.5.2`). No source changes.

### 40b6027 — 2026-10-31 19:27:00 +0100 — "Console host support added by Claude Code"
17 files, +782/-36.
- **The console host `engine1999c`** (2026-10-31, owner's request; roadmap section 12, first stage; `docs/console-host.md`): new `engine1999c.c` (no SDL; links with `-lrt -lm`): run mode (`engine1999c [-i] file`: load, run, exit status 0 / `BYE n` / 1 after an error / 2 if the file can't be loaded), stdin mode (lines are entered like typed lines; interactive banner and `Ready.` on a terminal), callbacks for output (binary-safe `printn`), messages on stderr, `INPUT` / `GET` / `INKEY$` from standard input (unbuffered, so lines and single keys stay in order; raw single keys on a terminal), Ctrl-C as Break (`sigaction` around each run), `CLS` / `LOCATE` as ANSI sequences on a terminal, the other graphics statements an error "Not available in the console host" (sound silent as before), text size queries from the terminal. `basrepl.c`: the text loader skips a first line starting with `#!`; new `repl_load_file()`. `basdebug.[ch]`: the `--debug-*` options moved there (`basdbg_option()`, `basdbg_usage()`), both hosts use them. `makefile`: `engine1999c`, `contest1` (also in `all`), `.gitignore`. Tests: new `contest1` (44 checks through the shell: pipes, `#!`, exit codes, `INPUT`/`GET`/`INKEY$` from a pipe, SIGINT, binary file, zero bytes); `engine1999c` under ASAN/UBSAN: clean; all other tests pass. Not done (roadmap): program arguments and environment, `stdin:`/`stdout:`/`stderr:` channels, a headless `BUILD` kit, syntax colors. Docs: `docs/console-host.md` (new), `docs/README.md`, `docs/build-and-test.md`, `docs/roadmap.md`, `README.md`, `CLAUDE.md`, `SESSION.md`. Design decisions I took are listed in the new doc.

### f92cf95 — 2026-10-31 19:44:59 +0100 — "Improved console host, authored by Claude Code"
26 files, +318/-31.
- **Program arguments, the environment and the standard channels; roadmap priorities** (2026-10-31, owner's request; console host stage 2): new functions `ARGC`, `ARG$(n)` and `ENV$(name$)` (tokens 248..250 `ARGC ARG ENV`, instructions `ARGC 0x179`, `ARGS 0x17A`, `ENVS 0x17B` in `vm_system_function()`; parsed like `TI` / `CHR$` and a new branch for `ENV$` with a string argument; `ARG$(0)` is the program, `ARG$(n)` beyond the arguments is `""`, negative or fractional is "Illegal argument"; `vm_t.argc/argv`, `repl_t.argc/argv`); the command line after the file name goes to the program in `engine1999c` (`ARG$(0)` = the file, `-` for standard input) and in built executables (`run_embedded()`: `argv`, and `--engine1999-about` only counts as the first argument); drivers `stdin:`, `stdout:`, `stderr:` in `iodev.c` (the process's streams as channels: no name behind the colon, `stdin:` input only and one line per read, never read ahead, the others output/append only, `stderr:` flushes `stdout`, closing keeps the stream). `engine1999c` now answers `EDIT` (without target) and `MONITOR` with "Not available in the console host". `bascomp.ebnf` (`make grammar` run, `grammar-check` passes). Tests: `vmtest1` 1061 (17 new, the test host has the arguments `prog a "b c"` and the variable `E1999_TEST`), `repltest1` 325, `iotest1` 182 (10 new), `contest1` 60 (16 new: arguments, `ENV$`, the three channels, turns between `stdin:` and `INPUT`, error cases); a built executable with arguments checked on the real screen under Xvfb (`ARGC`, `ARG$`, `ENV$`, `stdout:`, `BYE ARGC` returned 2); `engine1999c` under ASAN/UBSAN clean. **Roadmap section 12 updated** as the owner asked: the headless `BUILD` kit and syntax colors in the console `LIST` are marked **HIGH PRIORITY** follow-ups (with a plan for each), and a new item for a **full-screen `EDIT` and `MONITOR` for interactive console use** (a terminal text screen backend, escape sequence decoding, size changes); done items ticked. Docs: `docs/basic-language.md` ("Program arguments and the environment"), `docs/io-subsystem.md`, `docs/console-host.md`, `docs/roadmap.md`, `docs/grammar.*`, `instset.h`, `SESSION.md`.

### 42949c1 — 2026-10-31 20:03:27 +0100 — "syntax colors in console LIST, console BUILD, all created by Claude Code"
29 files, +335/-64.
- **Syntax colors in the console `LIST` and the headless `BUILD`** (2026-10-31, the two high-priority follow-ups of roadmap section 12, "go ahead as you see fit"): (1) the ANSI color table of `LIST`/`FILES` moved from `engine1999.c` to `ansi.c` (`ansi_hlstyle()`, `ansi.h` includes `bastok.h`); `engine1999c` sets it as `repl.hlstyle` when standard output is a terminal and `NO_COLOR` is not set; new option `--color[=always|never|auto]` (bad values: status 2). (2) `BUILD name$ [, WINDOW | CONSOLE]` (identifiers, like `SAVE`'s mode; parser `comp_eat_buildstmt`, `cgen_from_buildstmt`: the kind is the parameter of `BLD`, 0 default / 1 WINDOW / 2 CONSOLE; `vm_t.build` has a `kind` argument; `repl_t.buildconsole` is the default kind of the host, set by `engine1999c`; `build_console_opts()` in `basbuild.c`: runtime `engine1999c.o`, libraries `m`); the console runtime is in `engine1999c.c` (`con_run_embedded()` with its own copy of the license notice: keep in sync with `engine1999.c`: the whole command line is the program's except `--engine1999-about` as the first argument; exit status 0 / `BYE n` / 1 after an error / 2 if the image can't be loaded). `bascomp.ebnf` updated (`make grammar`, `grammar-check` pass). Tests: `buildtest1` 34 (7 new: builds a real program with the console kit from a REPL image, runs it with arguments and input, checks the exit code and that `libSDL2` is not in the file), `contest1` 71 (11 new: colors, `BUILD` from the console host in the three kinds, SDL only in the WINDOW one), `vmtest1` 1067 (6 new: the kinds and the error cases); `asmtest1` passes with the parameter on `BLD`. Checked by hand: `ldd` of a console executable shows only libm and libc; it runs from a pipe. Roadmap: both items ticked, a new item for a `make install` kit. Docs: `docs/basic-language.md` ("BUILD"), `docs/console-host.md`, `docs/build-and-test.md`, `docs/roadmap.md`, `README.md`, `docs/grammar.*`, `instset.h`.

### f9ad98f — 2026-10-31 20:18:54 +0100 — "Install option, created by Claude Code"
15 files, +778/-5.
- **`make install` with the portable kit, and man pages** (2026-10-31, owner's request: install under `/usr/local`, portable kit, man pages, which must be kept current): `makefile`: `make kit` compiles everything again into `kit/` with `-O2` only (no `-march=native`, no `-p`; static pattern rule on `$(BASEMOD)`, `kit/engine1999.o`, `kit/engine1999c.o`, `kit/engine.lib`, `kit/engine1999`, `kit/engine1999c`), `make install` (`PREFIX?=/usr/local`, `DESTDIR`; programs to `bin`, the `BUILD` kit to `lib/engine1999`, demos to `share/engine1999/demos`, docs and licenses to `share/doc/engine1999`, man pages to `share/man/man1|man7` with `@VERSION@` replaced), `make uninstall`, `make man-check`; `.gitignore` has `kit/`. `basbuild.c`: the kit is also looked for in `<dir of the running program>/../lib/engine1999` (any prefix works). **Man pages** (new `man/`): `engine1999.1`, `engine1999c.1`, `engine1999-basic.7` (the language summary, `BUILD`, drivers, exit status, license; the page for the language has the list of reserved words between `KEYWORDS-BEGIN/END`). **Kept current by** `mancheck.sh` (`make man-check`): every option of `engine1999 --help` / `engine1999c --help` must be in the page of that program, the reserved words must equal the new option `engine1999c --list-keywords`, `@VERSION@` must be in the `.TH` line, groff must have no remarks; `sh mancheck.sh --update` rewrites the list of reserved words. The rule is in `CLAUDE.md` and `SESSION.md` (standing rules). Checked: `make install DESTDIR=...` into a scratch tree (file list; an installed `engine1999c` found its kit through `../lib/engine1999`, built a console and a window executable from another directory; `ldd`: SDL2 only in the window one; no `mcount` in the kit; `man -w` finds the pages with `MANPATH`), `make uninstall` leaves no file, the check fails when an option or a reserved word is missing (tried), the rendered pages looked at. Docs: `docs/build-and-test.md` ("Installing, the kit, the man pages"), `README.md`, `docs/roadmap.md` (the kit item ticked). Tests unchanged and passing (`vmtest1` 1067, `repltest1` 325, `buildtest1` 34, `contest1` 71 ...).

### d3de07d — 2026-10-31 20:48:17 +0100 — "Claude Code state update, -p option optional in makefile, modified by Claude Code"
5 files, +11/-4.
- `makefile`: profiling (`-p`) is optional now, `make PROFILE=1` (off by default; the user's question whether it was needed: it wasn't). `CLAUDE.md`, `docs/build-and-test.md`, `SESSION.md` updated. Rebuilt without `-p`; all automated tests (vmtest1, repltest1, ansitest1, txttest1, asmtest1, synthtest1, iotest1, gfxtest1, buildtest1, contest1, testcomp1) and `make man-check` pass.

### 43ecab8 — 2026-10-31 21:00:15 +0100 — "roadmap update by Claude Code"
3 files, +16/-3.
- `docs/roadmap.md`: new section 13 (ideas from the owner's `../basic1/concept.txt`): `SWITCH`, `TAB`/`TABMODE`, `DEF type`, `DISPLAY()`, `BREAKPOINT`, with the open design questions; cross-reference in section 2. Documentation only, no code, no tests.

### b8e5439 — 2026-10-31 21:55:11 +0100 — "ISO-8859-15 default font, ANSI key sequence support (incl. debugging option), all created by Claude Code"
27 files, +2222/-232.
- **Font 128..255 and key sequences** (owner's request: step 1 of the keyboard design, plus a debugging tool): `8x12font1.txt` / `8x12font1.c` got the glyphs 128..255 (generated by a script in the session scratchpad: the C1 control codes as two-letter mnemonics like 0..31, ISO-8859-15 printable glyphs; accented letters composed from the base letters, capitals compressed by one row, the rest drawn by hand; checked on the real screen under Xvfb). New `keyseq.[ch]` (no dependencies: canonical xterm style sequences of the cursor block and F1..F12 with modifiers, `keyseq_parse()` for the dialects terminals send, names, hex/escape formatting) and `sdlkeys.[ch]` (SDL key code + modifiers -> sequence, keypad without Num Lock = cursor block). New option `--debug-keys` (`BASDBG_KEYS`, not in `--debug-all`): `engine1999` prints every SDL key event / text input with the sequence made from it, `engine1999c` runs no BASIC and dumps the bytes the terminal sends, item by item (hex, C string, meaning, whether it is the canonical form). New test `keytest1` (37 checks, clean under valgrind); `txttest1` +3 (every code has a glyph, no equal glyphs, codes >= 128 kept); `contest1` +11 (now 82). Docs: `docs/basic-language.md` (status of the keyboard section), `docs/display-engine.md` (character set, control glyphs), `docs/build-and-test.md`, `docs/roadmap.md`, `CLAUDE.md`, man pages (`--debug-keys` in both), `.gitignore` (`keytest1`), makefile (`keytest1`, new objects). **Also fixed:** four timer tests of `vmtest1` ("EVERY/AFTER handler runs inside a PROC", the two FUNC ones) busy-waited with a loop of 15000 iterations, which finished in less than one 1/60 s tick since the build lost `-p`, so they failed (4 failures, flaky); they wait with `WAIT 3` / a loop on `TI` now and the step limit of the test host is 20 million. Tests: all passed (vmtest1 1067, repltest1, ansitest1, txttest1 49, asmtest1, synthtest1, iotest1, gfxtest1, buildtest1, contest1, keytest1, testcomp1, man-check, grammar-check). Not done: the keys reach no `GET`/`INKEY$` yet (steps 2..5 of the roadmap).
- Proposal only (no code, no tests): the owner's design for cursor and function keys (ANSI escape sequences for keys that are not one byte, ISO-8859-15 character set with control glyphs for 0..31 and 127..159, double ESC / PAUSE as break, F1 reserved as HELP). Written to `docs/basic-language.md` ("Keyboard: cursor keys, function keys, the character set") and `docs/roadmap.md` section 4 (steps 1..5). `SESSION.md`: note added.

### f003fa8 — 2026-10-31 22:39:52 +0100 — "added some residue; key sequence I/O fixes by Claude Code"
19 files, +792/-55. (The user bundled residue files — `residue/keytest.bas`, the two dump files, a screenshot — into this commit.)
- **Cursor and function keys reach `INKEY$` / `GET` in the window host** (the owner's finding: `INKEY$` in a loop showed no escape sequences and nothing above 127): `sdlscreen.c` — `sdlscr_pollkey()` hands out the bytes of an escape sequence (from `sdlkeys_sequence()`, with modifiers) one per call, typed text is converted from UTF-8 to ISO-8859-15 when it arrives (`sdlscr_textlatin9()`; the line editor and `sdlscr_editkey()` take the printable codes 160..255 now, `SDLSCR_PRINTABLE`); `vm.c` — `INKEY$` and `GET` group an ESC with the keys that continue a sequence (`vm_key_sequence()` with `keyseq_parse()`), a key that does not continue it is kept in the new `vm_t.keyback` and returned by the next read; a numeric `GET` gets 27 and consumes the whole sequence. Tests: `vmtest1` +12 (1079). Checked on the real window under Xvfb with a program printing the bytes of `INKEY$` (Right, Shift+Up, Ctrl+Left, F1, Delete, F5, Home, PageUp, letter, Tab, Return). Docs: `docs/basic-language.md`, `docs/roadmap.md`, `vm.h`. Not done: double ESC / PAUSE as break, the console host (`INKEY$` there still skips these keys), UTF-8 conversion in the console host. All tests pass (vmtest1 1079, repltest1, ansitest1, txttest1, asmtest1, synthtest1, iotest1, gfxtest1, buildtest1, contest1, keytest1, testcomp1).

### 6c5de67 — 2026-10-31 22:59:40 +0100 — "Console line editor fix created by Claude Code"
7 files, +245/-9.
- **Console host: a line editor on a terminal, identical to the window's** (the owner's screenshot: at the BASIC prompt the terminal echoed cursor keys as `^[[A^[[A...`, because the host read prompt lines with `getline` in the terminal's cooked mode; then the owner's request: behave like the SDL version's line editor, not like a shell): `engine1999c.c` — `con_getline()` (the prompt: 255 characters, `INPUT` / `LINE INPUT`: 1023, the sizes of the window's buffers) reads through `con_readline()` when standard input and output are terminals: printable characters (also above 127, echoed as UTF-8) are typed at the end of the line, BACKSPACE takes the last back, RETURN accepts, everything else (cursor and function keys with their whole sequence, DELETE, control codes, TAB, ESC) is ignored, exactly what `sdlscr_lineinput()` does; the one addition is Ctrl-D on an empty line = end of the input (a first version had a shell-like editor with history, cursor movement and Ctrl keys: removed). `con_readkey()` returns control codes for the editor (`con_wantctrl`); the UTF-8 -> ISO-8859-15 conversion of lines is in `con_getline()` (for terminals without the editor), pipes are unchanged. Assumes one column per character and a line that does not wrap. Tests: `contest1` +7 cases on a pseudo terminal (`posix_openpt`; the harness collects until the output has been quiet for 200 ms): cursor and function keys ignored and not echoed, BACKSPACE, `ä`, control codes / TAB / lone ESC ignored, Ctrl-D ends only on an empty line, the 255 character cut, `INPUT`; now 98 checks. Docs: `docs/console-host.md`, man page `engine1999c.1`.

### 31d9340 — 2026-10-31 23:43:05 +0100 — "Shared console/SDL full-screen editor (EDIT) and monitor (MONITOR), some WIP, all created by Claude Code"
29 files, +2331/-660.
- **Console versions of EDIT (full-screen) and MONITOR, honoring terminal resizes; ESC = break in the console line editor; the consistency rule** (the owner's requests): *Bookkeeping:* the rule "editing and input are consistent between the window and the console version" is in `CLAUDE.md`, `SESSION.md` (standing rules) and `docs/roadmap.md`. *ESC:* a lone ESC in the console line editor is a break like Ctrl-C (`con_esc_break`; Ctrl-C also leaves the terminal in order now: a handler of ours is in effect while the line is typed, the old action follows: the process ends at the prompt, a running program sees Break at INPUT); on a terminal a lone ESC also breaks a *running program* now (as in the window): while it runs the terminal is raw and the keys typed meanwhile are queued for `GET`/`INKEY$`/`INPUT` and dropped at its end (`con_pump()`, `con_inq`, `con_catch_break()`). *The editors on a driver:* new `scrdrv.h` (the screen driver: text screen calls, sizes, keys incl. `SK_RESIZE`), `scredit.c` (the one-line editor and the editing keys, shared; fixes a bug of the old window editor: a burst of Backspaces left characters behind), `fulledit.c` and `monitor.c` rewritten for the driver with a layout that follows the screen size (the window's 80x25 gives the old layout; `Too small` below 24x6), `sdlscreen.c` has the window driver (`sdlscr_driver()`, `sdlscr_lineedit()` is `scr_lineedit()` now), `engine1999.c` passes it. *The terminal:* `textscreen.c` has runtime sizes (`txtscr_resize()`, up to 400x200; the window stays 80x25; `txtscr_setresizereserve()`, `txtscr_resizeshift()`, `txtscr_getrawcell()`, `txtscr_getabscursor()`), new `palette.[ch]` (the start palette moved out of `sdllayer.c`), new `conscr.[ch]` (the terminal as a screen: alternate screen, incremental painting, truecolor or the nearest of 256 colors, no curses), `engine1999c.c` (`con_drv`, SIGWINCH, `con_run_windows()` replaces `con_refuse_windows()`; output and messages go to the text screen while a session is open; without a terminal: `? EDIT and MONITOR need a terminal`). Tests: new `scrtest1` (38 checks: editors on a fake driver, resizes), `contest1` +14 pty cases (now 109), checked by hand in a pty with a small terminal emulator (30x100 -> 14x50 in the editor, 24x80 -> 12x40 -> 30x90 in the monitor with a program run) and in the window under Xvfb (`EDIT`, `MONITOR`, `EDIT 10`). Docs: `docs/console-host.md`, `docs/display-engine.md`, `docs/build-and-test.md`, `docs/roadmap.md`, `CLAUDE.md`, man page `engine1999c.1`. Not done: `EDIT n` (one-line editor) and `AUTO` in the console host, mouse.

### dfa718c — 2026-11-01 00:16:54 +0100 — "Shared line editor and overhaul, created by Claude Code"
17 files, +508/-264.
- **Console fixes: `EDIT n` works, and a lone ESC stops every running program** (the owner's report): `engine1999c.c` — `repl->edit` was never set in the console host: `con_edit()` edits the line (with its number) with the same one-line editor as `con_getline()` (shared set-up `con_runedit()`), RETURN enters it, ESC cancels, "? Line too long to edit" like the window; a lone ESC was only noticed by programs that don't read keys (the key queue of `con_pump()`): a program that polls `INKEY$` or waits in `GET` / `INPUT$` / `LINE INPUT` got the ESC as a key (27) instead of Break, so `con_readkey()` turns a lone ESC into Break while a program runs (`con_pumping`: `con_break`, the key reader returns -1); an ESC followed at once by a control code (as in a file: ESC, line end) is a lone ESC for the editors (`con_scrkey()`). Tests: `contest1` +9 (123 -> now more): `EDIT n` from a pipe (keys of the input: End, a character, RETURN; ESC cancels; Home/Delete; an unknown line) and on a pty (edit, cancel), ESC breaks an `INKEY$` loop and `GET` on a pty, a cursor key is a key for `GET` and no break; checked by hand in a pty for a busy loop, `INKEY$` loop, `GET`, `WAIT`, a `PRINT` loop, `INPUT`, `LINE INPUT`, `INPUT$`, a direct `FOR` loop and file mode (`engine1999c prog.bas`): all end with Break. Also corrected the log: the entry for `31d9340` was still under Pending.

### 342ba8f — 2026-11-01 00:48:07 +0100 — "BREAK statement and unified break handling for console/SDL versions, created by Claude Code"
32 files, +183/-43. (The owner tagged `v0.5.9` with it.)
- **Break keys: Ctrl+C everywhere, PAUSE/BREAK in the window, `BREAK ON` / `BREAK OFF`, the same in `BUILD` executables** (the owner's decision after my recommendation; double-stroke ESC dropped): *Language:* new reserved word `BREAK` (token 251), node `NT_BREAKSTMT 0x85`, instruction `BRKM 0x17C` (parameter 1 / 0), `vm_t.escbreak` (every run starts with `BREAK ON`: `vm_begin()`), host callback `breakesc` (`repl_t`, `vm_t`); grammar `break-stmt`, `docs/grammar.*` regenerated, the reserved-word list in the man pages updated. *Window* (`sdlscreen.c`): `sdlscr_isbreakkey()`: ESC (only with BREAK ON), Ctrl+C, PAUSE/BREAK (also `SDLK_CANCEL`) break a running program; with BREAK OFF the ESC key reaches `INKEY$` / `GET` as `CHR$(27)`; `sdlscr_setescbreak()` (reset by `sdlscr_pollkeys()`); in the editors Ctrl+C / PAUSE are the new key kind `SK_BREAK` (result `SCREDIT_BREAK`): INPUT is stopped by it always, by ESC only with BREAK ON (else an empty line), the full-screen editor treats it like ESC. *Console* (`engine1999c.c`): `con_escbreak` (the callback, reset by `con_catch_break()` / `con_drv_pollkeys()`): a lone ESC breaks only with BREAK ON (`con_pump()`, `con_readkey()`), Ctrl-C is `SK_BREAK` in both drivers and always breaks (no PAUSE/BREAK on a terminal). *BUILD:* the executables use the same host code (`init_host()` / `con_init()`), checked with a built console executable under a pty (ESC gives 27, Ctrl-C breaks). Tests: `vmtest1` +6 (1086 of 1086 passed), `contest1` +4 pty cases (now 129: BREAK OFF -> ESC is a key, with BREAK OFF only Ctrl-C stops, ESC at INPUT is an empty line, the next run has BREAK ON); checked in the window under Xvfb (Pause, Ctrl+C, Esc break; BREAK OFF: Esc gives 27, Ctrl+C still breaks). Docs: `docs/basic-language.md` ("Keyboard", Break; "BREAK ON / BREAK OFF"), `docs/roadmap.md`, man pages `engine1999.1`, `engine1999c.1`, `engine1999-basic.7`.

### 89d5e2d — 2026-11-01 01:01:26 +0100 — "Prompt line limit raised to 4095 by Claude Code"
8 files, +49/-18.
- **Prompt limit raised to 4095 in both hosts** (the owner's decision, after the consequence for piped lines was pointed out): `engine1999.c` `PROMPT_BUFSIZE` 4096 (prompt and AUTO buffers; was 256 / 200), `engine1999c.c` `CON_PROMPTMAX` 4095 (was 255); with a redirected input the editor's model screen is made tall enough for 4095 characters (nothing is drawn, so only the length counts). **Limit of the editor, said to the owner:** a line must fit the screen minus one row (`scr_lineedit()`), so the interactive limit is 1919 characters in the window (80 x 25) and columns x (rows - 1) on a terminal; only a redirected input reaches 4095. Tests: `contest1` +2 and 1 changed (now 131: a 3000 character line from a pipe is entered whole, a 5000 character line is cut at 4095; on the 24 x 80 pty a 1500 character line is entered and a 2500 character line is cut where the screen ends); checked in the window under Xvfb (the prompt works). Docs: `docs/roadmap.md`, `docs/console-host.md`, man page `engine1999c.1`, `SESSION.md`.

### 1a17a46 — 2026-11-01 01:22:11 +0100 — "C-64-like screen editor proposal and evaluation, by Claude Code"
5 files, +253/-2. (Committed on the branch `feature/c64-like-screen-editor`, created by the owner for this subproject.)
- **Proposal document for a C-64 style screen editor on text screen 0** (the owner's idea; nothing implemented, no code, no tests): new `docs/screen-editor-proposal.md` (understanding of the request, what `../gforthstuff/forthbasic` does: cell buffer with NUL = empty, rows linked by the last column, grab/zero-to-space, scroll with lossy scroll-down, RETURN copies the line; fit with the engine; evaluation with recommendations: a flag bit in the unused top byte of the text cell instead of NUL + font change, a WRAP flag for row links, scroll-back ring, keys, AUTO, console host only through an opt-in full-screen mode; design, phases, 8 decisions for the owner); pointers in `docs/README.md` and `docs/roadmap.md`. The entry for `89d5e2d` was moved into the log.

### 2747c99 — 2026-11-01 01:34:47 +0100 — "Claude state update"
2 files, +7/-3. (Bookkeeping of `1a17a46`: the log entry and `SESSION.md`; no entry of its own.)

### 63c17af — 2026-11-01 03:22:46 +0100 — "Proposal update, by Claude Code"
4 files, +230/-211. (Documentation only, no code, no tests; includes the pending entries of the proposal revisions after `2747c99`.)
- **Screen editor proposal revised** (the owner's change: the console host's BASIC session runs on the alternate screen too, so the screen editor is one implementation): `docs/screen-editor-proposal.md` — §1 point 3, §5.7 rewritten (benefits: real `COLOR`/`LOCATE`/`TXT` in the console, palette colors, simpler input code; costs and handling: scroll-back lost -> ring in phase 1 for the console, exit line, classic mode with `--line` / `E1999_SCREEN=0`, pipes and run mode unchanged, terminal restore on kill, paint batching, unsuitable terminals), phase 3, decision 6 (still open: a screen request for console executables, switch names); `docs/roadmap.md`, `SESSION.md`. The owner then decided that console executables made with `BUILD` and run mode behave exactly like inside the engine (screen mode by default when both ends are terminals) and agreed to the switches: §5.7 table row, switches table (`--screen` / `--line`, `E1999_SCREEN`, reserved `--engine1999-screen` / `--engine1999-line` for built executables), "Programs started as commands" (exit dump, an error waits for a key, a normal end leaves at once), decision 6. Then decided: the exit dump is on by default (`--no-dump` / `E1999_DUMP=0`, `--engine1999-no-dump`) and an error waits for a key (rules 1 and 2 of "Programs started as commands", decision 6: nothing open for the console). No code, no tests. The entry for `2747c99` (bookkeeping only) was moved into the log.
- **Screen editor proposal rewritten with the owner's decisions** (no code, no tests): `docs/screen-editor-proposal.md` has a decision table now (cell flags EMPTY bit 24 / WRAP bit 25 instead of NUL; overwrite by default, Shift+INSERT toggles INS/OVR; INSERT inserts a space, DELETE and BACKSPACE shift the rest of the logical line; BACKSPACE at a line start only moves left and abandons the line; free LEFT/RIGHT with scrolling, cursor to the end of the first row when scrolling down at the top left; no scroll-back; a colored status line with INS/OVR on the bottom row (see the correction below); the current logical line highlighted by a render-time overlay (brighter paper, not stored in the cells); the editor only while waiting for a program line, `EDIT n` / `AUTO` / `INPUT` keep the line editor (AUTO adds its number, EDIT n passes the typed text); the console host on the alternate screen incl. run mode and `BUILD` executables, switches, exit dump, an error waits for a key), the pondering of the owner's LINE-bit idea (not taken: the free cursor re-creates gaps anyway, it needs host cooperation for LIST/EDIT, it gives up re-entering any text; the one benefit, ignoring stale output, can be a host check), design details (logical line extent, RETURN clears WRAP of the line's last cell), phases and open points; `docs/roadmap.md`, `SESSION.md`. **Correction by the owner the same day:** the status line (and the highlight) exist only while no program runs and the system waits for a program line, not in AUTO / EDIT n / INPUT: the proposal now makes both render-time overlays (`txtscr_setstatus()`, a highlight range) that cover the bottom row's cells without touching them, shown with a viewport of rows - 1 only in that state; programs always have the full screen, so the `BUILD` question and the 24-row consequence are gone; the recheck also fixed leftover wording and added an open point about the row under the status line (open points now: a second INS/OVR key, status line colors, INSERT with no line, highlight brightness, the row under the status line).
- **Screen editor proposal: the owner's answers to the open points** (no code, no tests): F12 toggles INS/OVR (also Shift+INSERT) and the status line names it; the status line is white on red and shows mode + key, row, column and the BASIC line number if known; INSERT/DELETE/BACKSPACE act on the logical line, or, in a gap of a non-blank screen row, on the rest of that screen row (ASCII graphics), and do nothing on a blank row; the highlight covers the screen rows in which the logical line sits (whole rows; 30 % toward white as a first try); the row under the status line is cleared when the state is entered (the screen scrolls up one row first if the cursor is on the last row). `docs/screen-editor-proposal.md` (§1, §5.3 with the edit unit, §5.6, §5.8, §7 now has no open decisions), `docs/roadmap.md`, `SESSION.md`.

### 8b3779b — 2026-11-01 04:45:31 +0100 — "Screen editor proposal, phases 1 and 2, created by Claude Code"
19 files, +1502/-31. (Committed on the branch `feature/c64-like-screen-editor`; one commit holds all three pieces of work below; it also contains the `SESSION.md` / roadmap / proposal / man page updates.)
- **Screen editor, phase 1: cell flags and overlays** (the owner's "go ahead" after my review of the proposal; `feature/c64-like-screen-editor`): `textscreen.[ch]` — cells have the flags `TXTSCR_EMPTY` (bit 24; blank cells from clear/scroll/resize/initial fill) and `TXTSCR_WRAP` (bit 25; set on the last cell of a row when printed text wraps, moves with the row when scrolling, dropped by `txtscr_resize()` except on the new last column); `txtscr_write()` and `txtscr_fill()` with a character clear them, color-only fill and `invert` keep them, `txtscr_save()`/`restore()` carry them in the 4th byte; the getters (`txtscr_getcell/getrawcell`) hide them; new `txtscr_getflags/isempty/iswrapped/setwrap/setempty`. Overlays (per screen, drawn by the renderers, not stored in the cells): `txtscr_setstatus()` (replaces the bottom row), `txtscr_sethighlight()` (rows drawn with a brighter paper), `txtscr_sethighlightmap()`, `txtscr_getdisplaycell()`; `txtscr_render()` and the console painter (`conscr.c`) draw through it. `palette.[ch]`: `pal_lighter_map( pal, percent, backdrop, out )` (default highlight map, 30 % toward white; a transparent entry is lightened as composited over the backdrop, because the start paper is transparent and lightening it as black gave DarkSlateGray, darker than the SlateGray backdrop: found when the owner asked to check the highlight at startup). The host must recompute the map from the layer palette and `GFX BACKDROP` when the editor state begins (phase 2). Choice: a full row wraps (and sets WRAP) when the cursor leaves it, as the existing cursor logic does; the editor's extent rule ends a run at an empty cell, so a full row followed by an empty row is one line. Tests: `txttest1` 95 (was 49; 46 new: flags, wrapping, scrolling, resize, save/restore, viewport, overlays), all other suites unchanged and passing (`vmtest1` 1086, `repltest1` 325, `scrtest1` 42, `contest1`, `buildtest1` 34, ...). Not yet run on the real window (nothing uses the overlays yet).
- **Screen editor, phase 2: the editor in the window** (the owner: "use 255 for paper 0, then continue with phase 2"): `pal_lighter_map()` maps a fully transparent entry to entry 255 (`TransparentWhite`, `0x88FFFFFF`) when that is a translucent white, so the highlight of the start paper (transparent over the SlateGray backdrop) is a translucent white bar the layers show through (the owner's idea: the table has translucent entries for this). `scredit.c` (+ `scrdrv.h`): `scr_screenedit()` — the screen editor of text screen 0 with the logical line (`scr_screenline_extent()` / `scr_screenline_text()`, pure functions over the cells and the EMPTY/WRAP flags), the edit unit (the line, or the rest of a non-blank screen row in a gap), the keys of proposal §5.3 (characters overwrite, F12 / Shift+INSERT toggle INS/OVR, INSERT, DELETE, BACKSPACE, free cursor with scrolling at all four corners, HOME/END, Ctrl+HOME/END, RETURN, ESC/TAB/others ignored), the status line (`OVR  F12=INS     Row r  Col c  Line n`, white on red) and the highlight, entering/leaving the state (scroll up if the cursor is on the last row, clear the bottom row, viewport rows - 1, overlays on/off), resizes in the middle, a fallback for screens under 3 rows; the driver table got an optional `palette` member (the editor builds the highlight map from the layer's palette and the backdrop, so `GFX PALETTE`/`BACKDROP` changes are honored). Window: `sdlscr_screeninput()` (`sdlscreen.c`, the typing mode is kept between lines), the main loop of `engine1999.c` uses it for the prompt (not for `AUTO`, `INPUT`, `EDIT n`, the pager). `man/engine1999.1` ("AT THE PROMPT") and `docs/display-engine.md` describe it. Design decisions I took: BACKSPACE in a gap of a non-blank row closes the gap (the rest of the row moves left), as the proposal's edit unit says; END goes to the cell behind the line, or stays on a row's last cell (behind it is no longer in the line); RETURN with no line only moves to the next row; INSERT is refused where the line would grow into another line; Ctrl+Letters, ESC and TAB are ignored; Break and end of input leave the editor (the host gets an empty line). Found: setting a viewport moves the cursor to its corner (read the cursor first); a burst of keys with a resize needs the screen's cursor updated before each key. Tests: `scrtest1` 114 (was 42; 72 new on the fake driver: entering/leaving, RETURN, gaps, wrap, typing, INS, INSERT/DELETE/BACKSPACE, case 2, scrolling, status, highlight, Break, resize, too-small screen), clean under valgrind; `txttest1` 95; all other suites pass. **Checked on the real window under Xvfb** (status line, highlight, editing a program line and re-entering it, INS mode, `LIST`, `RUN`). Not done: the console host (phase 3).
- **Screen editor: the owner's decisions after trying phase 2** (BACKSPACE kept, RETURN kept): **END** on a line that ends on a row's last cell opens a new blank screen line below it (the text below moves down; a blank row below is used as it is; on the last row the screen scrolls up), links it with WRAP and puts the cursor in its first cell; **INSERT** (and INS typing) where the line would run into another line opens such a blank line and grows into it (before: refused); **Break** (Ctrl+C / PAUSE) has no effect at the prompt, it no longer ends the editor or moves the cursor; the end of the input still ends it (`SCREDIT_EOF`, an open question marked as a future issue in `docs/screen-editor-proposal.md` §7 and `docs/roadmap.md`). `scredit.c` (`se_openrow()`, `se_rowblank()`), `scrdrv.h`, `man/engine1999.1`, the proposal's key table. Tests: `scrtest1` 120 (5 new: END opening a line, typing there, a blank row below, the last row, INSERT making room incl. at the screen bottom; Break ignored), all other suites unchanged. **Then decided and implemented:** the status line is white on `TransparentDarkRed` (190); if the text colors are that pair the text is yellow (`scrtest1` 120). The suggestion was: a translucent red for the status line — `TransparentDarkRed` (190, `0x888B0000`) gives about `#7E3C43` over the SlateGray backdrop (white text contrast 8.0, deep wine red), `TransparentRed` (234, `0x88FF0000`) about `#BC3C43` (contrast 5.4, brighter); opaque red has 4.0.
- **Tried by the owner in the window afterwards (2026-11-01):** works, not every edge case tried; typing, editing, INS/OVR, END, INSERT, Break and the status line were tried. **Not yet tested: the highlight after `GFX BACKDROP` / `GFX PALETTE` changes** (the editor builds its highlight map from the layer palette and the backdrop when its state begins, which should follow such changes; untested on the real window; noted in `SESSION.md` and `docs/roadmap.md`).

### f0b892d — 2026-11-01 05:44:58 +0100 — "Screen editor proposal implementation (phase 3), implemented by Claude Code"
12 files, +494/-34. (Committed on the branch `feature/c64-like-screen-editor`, which the owner then merged into `main`; the next tag is `v0.5.11`.)
- **Screen editor, phase 3: the console host's screen mode** (the owner: "go ahead with phase 3"): `engine1999c` runs the BASIC session on text screen 0 on the terminal's alternate screen whenever standard input and output are terminals and `TERM` is not `dumb` — interactive, run mode (`engine1999c prog.bas`, also `-i`) and `BUILD` console executables; the classic line mode stays for pipes and with `--line` / `E1999_SCREEN=0`. `engine1999c.c`: `con_screen` mode (`con_want_screen()`, `con_screen_begin()/end()`), the prompt is `scr_screenedit()` (`con_repl_screen()`, fallback `scr_lineedit()` under 3 rows), INPUT / LINE INPUT / EDIT n use `scr_lineedit()` on the screen (`con_runedit()`, no `ESC [ 6 n` query), EDIT / MONITOR open on the same alternate screen (`con_run_windows()`), program output and messages go to the screen through the ANSI decoder with painting throttled to 10 ms (`con_paint_soon()`, flushed when a program waits for a key, every 64 interrupt polls, at the end; a resize while waiting for a key is followed), `CLS` / `LOCATE` / `COLOR` / `TXT` statements and functions work (`con_gfx_op` / `con_gfx_query`), Ctrl-C and a lone ESC break a running program, at the prompt they do nothing (Ctrl-D too; open question), the exit dump (`conscr_dump()`: screen 0 as plain lines, no colors; `--no-dump`, `E1999_DUMP=0`), a program that fails waits for a key (`con_wait_key()`: the hint is a status overlay, not dumped), the terminal is restored at exit and on SIGTERM / SIGHUP / SIGQUIT (`con_sigfatal()`), the SIGINT handler stays installed in screen mode; options `--screen`, `--line`, `--no-dump`, environment `E1999_SCREEN`, `E1999_DUMP`, built executables `--engine1999-screen|line|no-dump` (first words, not the program's). `conscr.c`: translucent / transparent palette entries are shown over the window's SlateGray backdrop (`conscr_visible()`), so paper 0, the highlight and the status line look as in the window; `conscr_text_setcolors()`, `conscr_dump()`. Docs: `docs/console-host.md` ("Screen mode"), `docs/basic-language.md`, `man/engine1999c.1` (options, environment, built executables), `man/engine1999-basic.7`, proposal status, roadmap, `SESSION.md`; `make man-check` passes. Tests: `contest1` 143 (was 116: 9 new pty cases for screen mode: alternate screen, status line and exit dump with BYE's status, editing a line picked from the screen, Ctrl-C / Ctrl-D at the prompt, ESC break, INPUT editor, EDIT on its own screen, resize, CLS / LOCATE / COLOR / TXT, an error waiting for a key, a normal end, `--no-dump`, `--line`; the old pty cases run with `E1999_SCREEN=0`), all other suites pass. Checked with a small terminal emulator on a pty in the session scratchpad (screens, status line colors = TransparentDarkRed over SlateGray, EDIT / MONITOR, resize, run mode, SIGTERM leaves the alternate screen, a `BUILD` executable with the switches); **tried by the owner afterwards (2026-11-01): works; resizing while editing, closing the terminal window mid-session (the window manager asks to close, then kills the shell and its children), a built console executable, and `ssh` to another machine (localhost; in the engine and standalone) all work; not yet tried: a plain 256-color terminal versus a truecolor one (`COLORTERM`), noted in `SESSION.md` and `docs/roadmap.md` for later testing.** Design decisions I took: the exit dump has no colors; a failing program in run mode shows its message on the screen (not on standard error) and waits for a key; Ctrl-D at the prompt is ignored; a lost input ends the session; `AUTO` is not available in the console host (as before).

### f2723ab — 2026-11-01 05:49:07 +0100 — "Claude Code state update"
2 files, +6/-3. (Bookkeeping of `f0b892d`: the log entry and `SESSION.md`; no entry of its own.)

### 7c34a1d — 2026-11-01 06:03:01 +0100 — "Roadmap update, by Claude Code"
1 file, +57/-5. (`663ec21`, "Roadmap update", 05:59:24, came first without "Claude" in its message and contained only the pending log entry and the `SESSION.md` line of this work; `docs/roadmap.md` itself is in `7c34a1d`.)
- **Roadmap: a priority table** (the owner's request; documentation only, no code, no tests): `docs/roadmap.md` has a new section "Priorities — an overview of everything that is open" with 41 rows (item, short description, priority High / Medium / Low, notes incl. which ones need an owner decision). Suggested High: key state for games (`KEY(n)`), synchronization of the layer memory between the threads, sandboxing, joystick / mouse input. While rating, four stale entries were found already done and ticked: `SYMBOL`, file channels, the graphics statements on channel 0, array parameters of `DEF FN`. The ratings are suggestions for the owner to change. `SESSION.md` points at the table.

### 1219848 — 2026-11-01 06:04:57 +0100 — "Claude history log update"
(Bookkeeping of `7c34a1d`: the log entry; no entry of its own.)

### e298ef9 — 2026-11-01 06:42:29 +0100 — "Various small changes (see history log), by Claude Code"
8 files, +197/-112.
- **Lock-free hardening of the sprite and layer code; the "synchronization" item dropped** (the owner questioned the roadmap item: the design has no locks on purpose, for speed, and the layer memory may be torn; they asked me to change my code, not the concept, and to fix the `modified` flag too): I audited what the screen thread reads (nothing freed or reallocated while running, indices are `uint8_t` into 256-entry tables or clamped; the text screen overlays only show a garbled cell at worst) and found only *lost-update* races, no memory-safety problems. Fixed: **`sprscreen.c`** — the collision indicators left the packed bitfield words (the screen thread's read-modify-write there could undo `SPRITE IMAGE`/`ON`, my change of 2026-10-31) and are a byte array `sprhit` set with `__atomic_fetch_or` and taken with `__atomic_exchange_n` (`sprscr_hits()`); the event slot is a compare-exchange / exchange; `sprcollwith` is built locally and published at the end (no "no collision" in between); **`SPRITE n,MOVETO`** is owned by the screen thread: `sprscr_moveto()` posts one atomic 64-bit request word (`sprreq`, ticks 0..65535, target int16; the last request wins), `sprscr_periodicals()` picks it up (before: `active` was set before the other fields, so a step in between could finish a stale entry and lose the move); a jump (`ticks` 0) is done by the screen thread in its next frame, `sprscr_moving()` counts a posted request, `sprscr_reset()` posts a cancel; the "changed" flag (`sprchg`) is set with a release store and cleared with an atomic exchange before the data is read. **`sdllayer.c`** — `modified` is set with a release store and cleared with an atomic exchange (a full fence) before the layer is converted (a plain store could be reordered after the loads on x86 and lose a change made in those nanoseconds). No mutex, no pthreads, no library: `__atomic` builtins (single instructions on x86). Docs: `docs/display-engine.md` ("Threading": the rules), `docs/graphics-subsystem.md`, `docs/roadmap.md` (the item is gone; the priority table now has 40 rows), `SESSION.md` (a standing rule: no locks on layer memory). Tests: `gfxtest1` 119 (was 113: requests, the newest request wins, a jump stops a movement, negative / large targets); a scratch stress test with a second thread running `sprscr_periodicals()` (200000 image changes, 4000 movements, three runs each): **no loss with the old code either** (the window is a few nanoseconds), none with the new code; all other suites pass. **Then, at the owner's request:** `anim_pos` and `anim_speed_count` (the only fields the screen thread still wrote in the sprite control block) are bytes of their own instead of bitfields (`sprctrl_t`; reads of `anim_pos` are masked to 0..15), so nothing the screen thread writes shares a word with a field of the main thread; a scratch stress test (20 million `SPRITE ON/OFF` + priority writes against an animating sprite, three runs): no loss with the old layout either (GCC writes bitfields bytewise), none with the new; `gfxtest1` 119, all suites pass. **Tested by the owner afterwards (2026-11-01): the BASIC programs that use sprites and `sdltest1` work as expected.**

### b8e91b6 — 2026-11-01 06:46:25 +0100 — "Claude state file update"
(Bookkeeping of `e298ef9`: the log entry and `SESSION.md`; no entry of its own.)

### e94d6b2 — 2026-11-01 08:37:53 +0100 — "KEY() function, added by Claude Code"
32 files, +469/-30.
- **`KEY(name$)`: the key state for games** (the owner: "go ahead with key state for games (KEY(n))"; item 1 of the roadmap's priority table): `KEY("LEFT")` is -1 while the key is held down, else 0, for any number of keys at once. New: `keystate.[ch]` (no SDL: the key names -> ids: characters, `SPACE`, `RETURN`/`ENTER`, `ESC`, `TAB`, `BACKSPACE`, `UP DOWN LEFT RIGHT HOME END INSERT DELETE PGUP PGDN`, `F1`..`F12`, `SHIFT CTRL ALT` and their `L`/`R` forms, keypad `KP0`..`KP9 KPENTER KP+ KP- KP* KP/ KP.`, `ANY`; case does not matter), reserved word `KEY` (token 252, `bastok.[ch]`, highlighted as a function), parsed with `ASC`/`VAL`/`CVI` (`comp_eat_sysnumfn1argcall`, a string argument), instruction `KEYD 0x17D` (`codegen.c`, `instset.h`, `vm.c`: an unknown name is "Illegal argument"; `vm_t.keydown` / `repl_t.keydown`, no host = every key is up); also fixed the descriptions of `ENVS` / `BRKM` in `instset.h`, which were mixed up. Hosts: window — `sdlscr_keydown()` (`sdlscreen.c`) reads SDL's keyboard state without a lock (keys found by their character with `SDL_GetScancodeFromKey`, so the layout counts; `engine1999.c` `host_keydown`); console — a terminal reports presses and never releases, so `con_note_bytes()` (in `con_pump()`) notes the time of every key it sees (letters, cursor key sequences with their modifiers, capitals as SHIFT, Ctrl+letter as CTRL) and `con_keydown()` calls a key held for 140 ms after its last byte (only while a program runs on a terminal; a held key flickers once at the start because of the terminal's repeat delay). Docs: `docs/basic-language.md` ("KEY" with the decisions), `docs/console-host.md`, `bascomp.ebnf` + `make grammar`, `man/engine1999-basic.7` (keyword list via `mancheck.sh --update`, functions, a "Keys held down" entry), `docs/roadmap.md` (item 1 done, the keyboard entry ticked), `SESSION.md`, `demos/keys.bas` + `demos/README.md` (a keyboard that lights up the keys you hold). Tests: `vmtest1` 1095 (9 new: held / up keys, case, names, a string expression, a condition, unknown and empty names, a numeric argument is a type error), `keytest1` 57 (12 new on the names), `asmtest1` 62, `contest1` +3 pty cases (a typed letter, a cursor key sequence, a capital with SHIFT); checked in the window under Xvfb (Left held for 1 s = 60 frames of 120, two keys at once in `keys.bas`). Design decisions I took (revisit if unwanted): one function with a string argument (no numeric key code, no `KEYDOWN`), `KEY` is a reserved word, true is -1, `ESC` shows its state although it is the break key, key-up *events* (`ON KEY`) are not done, and `fruit`, `fruit2`, `climb` still use latched controls (changing them is the owner's call). Note: while writing these notes an unquoted heredoc made the shell run a few backtick words as commands; all failed harmlessly and the one empty file it created (`keydown`) was removed. **Tried by the owner afterwards (2026-11-01): `demos/keys.bas` and `KEY()` work in the window; in the console host the flicker of the held-key approximation is unacceptable (see the next entry).**

### 66f0081 — 2026-11-01 08:55:51 +0100 — "KEY() disabled for console by default, created by Claude Code"
14 files, +83/-63.
- **`KEY()` in the console host: switched off, marked for review** (the owner: the flicker of the held-key state is unacceptable; "comment out its console implementation, if possible, and generate a run-time error for now; mark the issue for review"): `engine1999c.c` — the approximation (`con_keyseen[]`, `con_note_key()`, `con_note_bytes()`, `con_keydown()`, the 140 ms hold) is kept but compiled only with `-DCON_KEYSTATE` (checked: it still compiles with the flag), `con_pump()` no longer notes the bytes and `con_init()` no longer sets `repl->keydown`; `vm.c` — `KEYD` without a host callback is now the run-time error "KEY is not available in this host" (before: every key was up; the name is still checked first: an unknown name stays "Illegal argument"), so the console host, console executables and any host without a keyboard give that error; docs: `instset.h`, `vm.h`, `basrepl.h`, `docs/basic-language.md` ("KEY"), `docs/console-host.md`, `man/engine1999-basic.7`, `demos/keys.bas` header, `docs/roadmap.md` (a new row of the priority table: review KEY() for terminals, with the ideas: the kitty keyboard protocol reports releases; or a longer hold; or leave it window-only), `SESSION.md`. Tests: `repltest1` 327 (2 new: the error, the name checked first), `contest1` (2 new table cases through a pipe, the 3 pty cases replaced by one: the error on a terminal); all other suites pass. Ideas for the review (not done): the kitty keyboard protocol (`CSI > 1 u` and the `CSI ? u` query; key events with press / repeat / release, supported by kitty, foot, WezTerm, recent Alacritty and others, not by xterm), or a hold time that adapts to the terminal's repeat delay (but a tap would then look like a press of 0.5 s).

### c51b72e — 2026-11-01 09:08:02 +0100 — "Demo games adapted to use KEY(), by Claude Code"
7 files, +72/-51. (The next tag is `v0.5.12`.)
- **The demos `fruit`, `fruit2` and `climb` use `KEY()`** (the owner: the spaceship and the worker should no longer move by themselves, which also makes the games much easier; and better keys, `climb` suffered most): `fruit.bas`: `LABEL KEYS` sets the direction every frame from `KEY("LEFT") OR KEY("A")` / `KEY("RIGHT") OR KEY("D")` (so the cannon stops when the key is let go: the old `S` = stop and the latching are gone) and fires while `KEY("SPACE") OR KEY("UP")` is held (the bullet limit stays: one at a time); `fruit2.bas`: the same in `PROC KEYS` (two bullets with a banana as before); `climb.bas`: `PROC KEYS` rewritten (renumbered in steps of 5): LEFT / RIGHT or A / D walk, UP / DOWN or W / S climb, only while held (the old latched `CLM` and `X` = stop are gone), SPACE jumps with a 6 frame jump buffer (`JBUF`: a press shortly before landing still counts; one jump per press), Z / X or , / . drill a hole left / right, one hole per press (`ZK` / `XK` remember the key state), P (pause), M (music), Q (quit) stay single presses read with `INKEY$`; the title screens and comments list the new keys; `demos/README.md`. Checked in the window under Xvfb: the cannon / ship moves while Left or Right is held and stays when it is let go (`fruit`, `fruit2`, screenshots), the worker walks while D is held, stops at the step when blocked, jumps onto the step with SPACE (`climb`); the console host reports the programs' graphics as before (they parse; `engine1999c` stops at the first graphics statement). Ladders and drilling in `climb` could not be tested under Xvfb (the software renderer runs it at ~12 frames per second); **tested by the owner afterwards (2026-11-01): ladders and drilling work fine.**
