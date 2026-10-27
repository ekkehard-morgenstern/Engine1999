# Session state

Maintained by Claude; updated whenever work in a session changes something. Last update: 2026-10-27.

## Standing rules
- **Never run git write commands** (commit, add, push, merge, rebase, reset, checkout of files, stash, tag, branch changes). The user does all of these manually, even if git access is authorized later. Read-only git (`status`, `log`, `diff`, `show`) is fine.

## Branch / repo state
- Branch `feature/refactor-to-remove-some-limits`. The bastok refactor is complete and `make` builds everything cleanly.
- All Claude's changes were committed by the user as `64b4f1d` ("Bastok refactoring by Claude Code"); the working tree is clean.
- `make` also rebuilt git-ignored artifacts (`*.o`, binaries, `engine.lib`, `all`).

## Done this session
- Documented the project (`CLAUDE.md`, `docs/`).
- Completed the tokenizer refactor (see `docs/refactoring.md` for API, behavior changes, remaining limits).
- Verified: clean build with `-Wall -Werror`; `basictest1`/`basictest2` round trips incl. 3000-byte identifiers, 5000-byte string literals; oversize and unterminated literals rejected; valgrind: no errors, no definite leaks.

- Line numbers widened to 24 bit: `linehdr_t.lineno` is `uint32_t`, encoded in 3 bytes (`emit/read_uint24`), header encoded size `LINEHDR_SIZE` = 13 (use it instead of `sizeof(linehdr_t)`). `LINENO_MAX` = 999999, `LINENO_NONE`/`LINENO_DEL` = 16777215. `NT_GOTOTARGET` data is now 4 bytes, `NT_STMTLINE` data 3 bytes. Builds clean; basictest1 verified (999999 accepted, 1000000 rejected).

- Program store (`baspgm`/`baslin`) rewritten as a pointer-linked list: `program_t{first,last}` of heap `line_t` nodes; the 64 KiB block, offset linked list, defrag and `linenode_t`/`linelist_t` are gone. `linehdr_t` is now just the encoded header `{lineno.24, length.16}` (`LINEHDR_SIZE` = 5; node and alloc fields dropped). New `free_program()`. `find_line` returns bool + prev/line pointers. Verified with basictest1 (insert/replace/delete/out-of-order) and basictest2 under valgrind: clean.

- Removed the `basintp` module (interpreter stub; the BASIC system compiles, not interprets): files, makefile rules/object/header, docs, CLAUDE.md table row. Clean rebuild OK.

- Syntax tree (`bascomp`) converted from the 64 KiB byte arena to heap `node_t` nodes with a growable branch array (`NODE_NONE` = null, optional branches may be `NODE_NONE`); `compiler_t.nodes` chains all nodes, freed by `cleanup_compiler()`/`comp_free_tree()`. `codegen.c/h`, `basictest2.c` converted. Fixed latent bug: base var-ref node data is `<type> <name>` (no length byte); old code read the first name char as the length. `cgen_from_substrassign` datalen check read the wrong field (now `node->datalen`). Verified: basictest2 tree dumps identical to the previous build on a varied input; valgrind clean. codegen is not exercised by any test program (compile-only verified).

- Added the minimal VM `vm.[ch]` (see `docs/compiler-and-vm.md`) and the automated end-to-end test `vmtest1.c` (32 cases, all passing, valgrind clean; exit status = number of failures). Codegen got a statement/line/program driver (`cgen_from_stmt/stmtlist/stmtline/stmtlines/program`) for LET/PRINT/SAVE, absent optional PRINT branches are skipped, and an empty PRINT now emits a newline. The harness found one bug in the VM itself (string concat over-read), fixed.
- Known gaps: statements other than LET/PRINT/SAVE report "not implemented yet"; `LINE` takes a 16-bit parameter, so codegen rejects line numbers > 65535; VM policies (comparison = -1, number format `%.15g`, comma zones of 10) are placeholders chosen by me, see `vm.h`.

- Control flow code generation (`cgenctl.c`, new): GOTO/GOSUB/RETURN (line table + patched BRIA fixups), LABEL/label GOTO/GOSUB (RRLV in the VM), single- and multi-line IF/UNLESS, FOR/NEXT (compile-time matching, hidden limit/step variables, pre-test). `vmtest1` now has 70 cases, all passing, valgrind clean. Parser fixes: IF nodes (single- and multi-line) lacked the condition branch; multi-line ELSE didn't eat the end of line. New API: `cgen_reset`, `cleanup_codegen`, `cleanup_runtime`; `codegen_t` gained the line table / fixup / FOR stack tables.
- Design decisions taken (revisit if unwanted): FOR/NEXT matched lexically at compile time (not a run-time FOR stack); FOR tests before the first pass; direct mode can't GOTO stored lines.

- Arrays and user functions: VM got RNAE/RIAE/RSAE/WNAE/WIAE/WSAE and CALF/RETF (contexts kept in the VM, saved string params are heap copies) plus `vm_run_at()`; codegen got `cgendecl.c` (DIM = no code, DEF FN single-line bodies); `vars.[ch]` got `vmem_get_func_info/param`. `vmtest1`: 99 cases, all pass, valgrind clean. Fixed: `comp_gather_branches` dereferenced NODE_NONE for `DEF FN` without parameters (old arena code read junk instead of crashing); absent argument list of a call is skipped.
- Open decisions for the user: (1) tokenizer reads `FNA` as one identifier, so the dialect needs `FN A` with a space; (2) ERASE is not implemented (design problem noted in bascomp.c, no ISA instruction); (3) multi-line DEF FN bodies (how do they return a value?); (4) direct mode code must persist (the test harness appends direct lines to the code and runs from the new start offset).

- System functions: VM got TI, INKY, FRE, ASC, LEN, VAL, STRS, BINS/QUAS/OCTS/HEXS/DECS, LFTS/RGTS/MIDS (policies documented in `vm.h`; `vm->inkey` callback, `vm_t.started` for TI). Codegen needed only `cgen_from_expr` accepting the parser's default-argument `NT_NUMLIT` (-1 = omitted). `vmtest1` now has 142 cases (INKEY$ via a scripted input field), all pass, valgrind clean.
- Open: SLFT/SRGT/SMID (substring assignment) can't work as specified ( s s n -- ): strings on the stack are copies; the ISA needs a variable operand. SAVE/SHEX/BRK/WRLV not implemented.

- SHEX and SAVE: VM `INS_SHEX` (popen, trailing newlines stripped) and `INS_SAVE` (host callback `vm->save`, mode check); `save_program()` in `baspgm.c` (ascii listing or binary encoded lines, no signature, no loader yet); fixed `cgen_from_savestmt` not NUL-terminating an empty option string. `vmtest1`: 154 cases, all pass, valgrind clean (the harness implements the save callback with `open_memstream`).
- Open: a loader for the binary SAVE format (and a file signature/version); the host (engine1999.c) doesn't use the VM yet, so nothing wires `vm->save` to a real file.

- Variable system overhaul: `vars.[ch]` rewritten as heap variables (`var_t`, table of pointers by variable number + hash table by type/name). No 64 KiB areas, compaction, string space, 1024-variable limit or 255-byte name limit any more; strings/arrays/names on the heap. The API kept its shape (`vmem_create_var/lookup_var/get_var/set_var/delete_var`, variable number still `uint16_t` in the instructions and tree), new: `cleanup_varmem`, `vmem_var`; `vmem_get_func_param` returns a stable name pointer; `varvalue_t` strings are `strptr/strlen`; `vmem_alloc_strs/vars` gone. Ported `bascomp.c` (names via `comp_node_name()` instead of 256-byte buffers; `compiler_t.name`), `cgenctl.c`, `cgendecl.c`, `vm.c` (FRE now reads MemAvailable from /proc/meminfo), `runtime.c` (`cleanup_runtime` also frees variables), `testcomp1.c`. Array limit: ≤ `MAXELEMS` elements in total (was 65535), one dimension still ≤ 65535.
- Found while testing: the old arrays aliased cells in 2+ dimensions (stride formula always gave 1, in `vars.c` and in the `testcomp1` model). Now row-major; `testcomp1` model fixed.
- Tests: `vmtest1` 158 cases (new dynamic ones: 3000-char name, 2000 variables, 300x300 array), `testcomp1` passes repeatedly, both valgrind clean.
- Not done (deliberately): syntax tree nodes still carry the variable number in their data bytes (not a `var_t*`).

## Open / next steps
- Remaining limits not touched: 64 KiB code/data segments (16-bit ISA addresses), 16-bit variable numbers, `engine1999` 256-byte input line, `codegen` coverage (see `docs/compiler-and-vm.md`).
- Next (codegen strategy): the open decisions above; the remaining instructions (substring assignment, WRLV, BRK); direct mode access to a stored program; growable code/data segments; LINE with >16-bit line numbers.
