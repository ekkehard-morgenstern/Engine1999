# Roadmap — I/O, VM and limits

Part of the roadmap: see [the central roadmap](../roadmap.md). The I/O subsystem and drivers, error handling, the VM and instruction set leftovers, remaining fixed limits.

**Open items of this part** (the priorities are explained in the central roadmap; items that are done are in [done.md](done.md)):

| # | Item (section) | What it is | Priority | Notes |
|---|---|---|---|---|
| 9 | Binary helpers (9) | `HEX` to bytes, unsigned `CVI`/`CVL`, byte-wise `ANDS`/`ORS` | Medium-Low | `STRING$` and `SPACE$` are **done 2026-11-01** (extension words, see the note below the table) |
| 19 | Drivers written in BASIC (8) | `DRIVER "name","PREFIX"` with labelled handlers | Low | postponed by the owner; proposal exists (*decision*) |
| 28 | `tcp:` server side, `SEEK` as a function, byte-array records (8) | more I/O | Low | |
| 29 | `LIST #n` **done 2026-11-08**; open: `PRINT` comma zones on files, files kept open across `CHAIN` (1, 4, 8) | I/O corners | Low | |
| 32 | Machine monitor limits (5) | no step-over, conditional breakpoints, watches; no variable inspection | Low | |
| 33 | VM leftovers (5) | `WRLV`, inline assembler `[ … ]` / `{ … }`, `DEF FN` can't call itself | Low | |
| 34 | Remaining fixed limits (6) | 16-bit variable numbers, 6 array dimensions, strings ≤ 65535 | Low | far from being reached |

## 5. VM and instruction set leftovers

- [ ] `WRLV` — write a label variable (instruction exists in `instset.h`, reported as "not implemented" at run time)
- [x] `BRK` — stops the run when the monitor debugs (`vm_t.debug`); outside of it still the error "Breakpoint"
- [ ] Inline assembler: `TOK_BRKLIT` string literals (`[ … ]`) are tokenized but code generation says "not implemented yet";
      `TOK_BRCLIT` (`{ … }`) has no defined purpose yet
- [x] Array parameters of user functions: supported by reference for `PROC`/`FUNC`/`DEF FN` (2026-10-31; stale entry ticked 2026-11-01)
- [ ] Functions can't call themselves and must be defined before use in the text (design decision, revisit if recursion is wanted)

- [ ] Machine monitor (`MONITOR`): no step-over (`t` steps into function calls), no conditional breakpoints or watches, no resumable `MONITOR` inside a program (it ends the run, like `EDIT`); the variables and the tokenized program text can't be inspected (only code, data and the two stacks); `a` has no labels or symbolic operands (variable names, line numbers); `e` takes hex bytes only (no strings); the monitor doesn't re-compile when the program changed (use `c`).

## 6. Remaining fixed limits

(also in `docs/refactoring.md`)

- [ ] 16-bit variable numbers (≤ 65535 variables), 6 array dimensions, one dimension ≤ 65535
- [ ] VM strings ≤ 65535 bytes
- [x] the prompt line editor takes 4095 characters (`INPUT` lines 1023; the screen size limits it further, see section 4)

## 8. I/O subsystem with virtual device drivers *(implemented: `docs/io-subsystem.md`; leftovers below)*

- [x] **Phase 1** (done, 2026-10-30): `iodev.[ch]` with `file`, `null`, `mem` drivers and `driver:name` prefixes; channel table in the VM; `OPEN`, `CLOSE`, `LINE INPUT`, `WRITE`, `EOF`, `LOF`, `LOC`, `INPUT$`, `ADDRESS`, `ADDRESS$`; `PRINT #` / `INPUT #` / `GET #` on files; `LOAD`, `SAVE`, `MERGE`, `CHAIN`, `FILES` through the drivers
- [x] **Phase 2** (done, 2026-10-31): `KILL`, `RENAME … TO`, `MKDIR`, `CHDIR`, `SEEK` (`FOP 0x155`); drivers `sh` (popen), `res` (read-only `res/` directory), `con` (screen/keyboard channel). Open: `res:` from an archive
- [x] **Phase 3** (done, 2026-10-31): `OPEN … FOR RANDOM [LEN=]` / `FOR UPDATE`, `FIELD`, `LSET`, `RSET`, `GETREC`, `PUTREC` (`FLD`/`LRS`/`REC`); drivers `play` and `tcp` (client)
- [ ] **Drivers written in BASIC** (postponed by the owner, 2026-10-31): proposal in `docs/io-subsystem.md` (`DRIVER "name","PREFIX"` with labelled handlers, variables `DPATH$ DMODE DH DBUF$ DCOUNT DPOS DERR$`, `DRIVERS`, `DRIVER TRACE`); awaiting the owner's decision
- [ ] `tcp:` server side (listening sockets), `SEEK` as a function, binary-safe strings or a byte-array interface for records
- [ ] **Sandboxing (partly done: the policy `policy.[ch]`, `--allow=` / `--deny=`, grants file, item 54; open: per-path grants, a monitor, a grants editor)** (the owner's proposed policy of 2026-11-03 is in section 17 and item 54): permissions for shell literals, `sh:`, writing/reading outside the program directory, the network; how a policy is configured (command line switch, trust level per program, permission file) is undecided; `iodev_allowed()` is the single place for the decision
- [ ] Files stay open across `CHAIN` in GW-BASIC; here a restart (`RUN`, `NEW`, `LOAD`, `CHAIN`) closes all channels
- [x] (`LIST #n` done 2026-11-08) `LIST #n` / `LIST` to a file channel (`LIST` still writes to channel 0 only; with the drivers it could write a listing to `mem:` or a file); open: `PRINT` comma zones on a file channel use the screen column
- [x] Binary data in files (done with binary strings, 2026-10-31)

## 9. Error handling

- [x] `ON ERROR GOTO|GOSUB`, `ON ERROR OFF`, `ERR$`, `ERL` (done, 2026-10-31)
- [x] `RESUME` / `RESUME NEXT` / `RESUME target` (done, 2026-10-31; not possible after an error inside a `PROC`/`FUNC` call: the frame is unwound by the trap)
- [x] `ERR` as a number, `ON ERROR GOTO 0`, `ERROR n` (done, 2026-11-01)
- [x] **Binary-safe strings** (done, 2026-10-31; see `docs/basic-language.md` "Binary strings"): zero bytes in strings, control code glyphs 0..31 in the font, length based print path
- [x] **Number packing** (done, 2026-10-31): `MKI$`/`MKL$`/`MKS$`/`MKD$` and `CVI`/`CVL`/`CVS`/`CVD`, big endian (network byte order; owner decision)
- [ ] Helpers for binary data (`STRING$` and `SPACE$` are done, 2026-11-01): `HEX` to bytes / bytes to `HEX$` dump, unsigned `CVI`/`CVL` variants, 24 bit and 64 bit integers; byte-wise `ANDS`/`ORS`
