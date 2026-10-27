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

## Open / next steps
- Remaining limits not touched: 64 KiB program/tree/code/data/var memories, variable names ≤ 255 (`vars.c`), `engine1999` 256-byte input line, `codegen` coverage (see `docs/compiler-and-vm.md`).
- No automated tests exist; consider adding a round-trip test program if desired.
