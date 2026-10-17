# Session state

Maintained by Claude; updated whenever work in a session changes something. Last update: 2026-10-17.

## Standing rules
- **Never run git write commands** (commit, add, push, merge, rebase, reset, checkout of files, stash, tag, branch changes). The user does all of these manually, even if git access is authorized later. Read-only git (`status`, `log`, `diff`, `show`) is fine.

## Branch / repo state
- Branch `feature/refactor-to-remove-some-limits`; mid-refactor in `bastok.[ch]`, build is broken (`make bastok.o` fails). See `docs/refactoring.md`.
- Untracked files created this session (not committed): `CLAUDE.md`, `SESSION.md`, `docs/*.md`.
- No source files have been modified by Claude.

## Done this session
- Read git history (terse "WIP" messages) and the last two refactor commits; reviewed headers and main modules.
- Wrote `CLAUDE.md` and `docs/` (README, architecture, display-engine, basic-language, compiler-and-vm, build-and-test, refactoring).
- Documented the refactor goal, the remaining checklist and 11 known defects in the converted `bastok.c` code.

## Open / next steps
- Awaiting user direction. Likely next: continue the `bastok.c` conversion following the order in `docs/refactoring.md`, only if the user asks.
