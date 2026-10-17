# Session state

Maintained by Claude; updated whenever work in a session changes something. Last update: 2026-10-17.

## Standing rules
- **Never run git write commands** (commit, add, push, merge, rebase, reset, checkout of files, stash, tag, branch changes). The user does all of these manually, even if git access is authorized later. Read-only git (`status`, `log`, `diff`, `show`) is fine.

## Branch / repo state
- Branch `feature/refactor-to-remove-some-limits`. The bastok refactor is complete and `make` builds everything cleanly.
- Uncommitted changes made by Claude (for the user to review and commit): `bastok.[ch]`, `baspgm.[ch]`, `bascomp.[ch]`, `basintp.[ch]`, `basictest1.c`, `basictest2.c`, `stdtypes.h`, plus doc updates (`CLAUDE.md`, `SESSION.md`, `docs/*.md`).
- `make` also rebuilt git-ignored artifacts (`*.o`, binaries, `engine.lib`, `all`).

## Done this session
- Documented the project (`CLAUDE.md`, `docs/`).
- Completed the tokenizer refactor (see `docs/refactoring.md` for API, behavior changes, remaining limits).
- Verified: clean build with `-Wall -Werror`; `basictest1`/`basictest2` round trips incl. 3000-byte identifiers, 5000-byte string literals; oversize and unterminated literals rejected; valgrind: no errors, no definite leaks.

## Open / next steps
- User to review the diff and commit manually.
- Remaining limits not touched: 64 KiB program/tree/code/data/var memories, variable names ≤ 255 (`vars.c`), `engine1999` 256-byte input line, `codegen` coverage (see `docs/compiler-and-vm.md`).
- No automated tests exist; consider adding a round-trip test program if desired.
