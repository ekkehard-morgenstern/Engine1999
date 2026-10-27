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

## Open / next steps
- Remaining limits not touched: 64 KiB code/data/var memories, variable names ≤ 255 (`vars.c`), `engine1999` 256-byte input line, `codegen` coverage (see `docs/compiler-and-vm.md`).
- Next (codegen strategy): DIM/ERASE/DEF FN and the array/function instructions in the VM (RNAE.., CALF/RETF), remaining statements; direct mode access to a stored program; growable code/data segments; LINE with >16-bit line numbers.
