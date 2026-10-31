# Roadmap — what is still to do

The list of open work, so that nothing is forgotten. `[ ]` = open, `[x]` = done (moved here when finished, then the entry can be
dropped). **Status of what already works** is in `SESSION.md` (state, tests, design decisions taken); this file is only about
what is **not** done. Items marked *needs design* wait for a language decision by the project owner before they can be implemented.
Order = suggested order, not a commitment.

## 1. Program commands as real language statements *(needs design — suggested next)*

`RUN [line|label]`, `LIST [a][-[b]]`, `LOAD name$`, `NEW`, `CLEAR`, `DELETE a[-[b]]` and `RENUM [a][-[b]][,step]` are language statements now (usable in programs, see
`docs/basic-language.md`). The rest of the family has nothing yet.

- [ ] `LIST` with an output channel (`LIST #1,10-20`; today always the VM's channel 0) and `RUN`/`LOAD` taking more arguments?
- [ ] `LOAD`/`SAVE`: file names are plain paths relative to the current directory; no directory handling (default extensions are done: `.bas` ASCII / `.e99` binary, see `docs/basic-language.md`)
- [x] `MERGE`, `CHAIN`, `FILES` are done (see `docs/basic-language.md`); not done: `COMMON`, `CHAIN … DELETE`, a line range for `MERGE`, paging for `LIST`
- [ ] Full-screen editor (`EDIT`, first version done): no mouse support (the scroll bar is informational), no search/replace, block or clipboard operations, no Unicode; lines over 1656 characters are read-only; the position memory is by line number only. `EDIT n` still uses the one-line editor.
- [ ] `RENUM` also adjusts targets in `RUN`/`LIST`/`DELETE`, but not the range of another `RENUM` statement in the program, and a changed line is
      re-tokenized from the listing text (a round-trip through `detokenize_line`/`tokenize_line`; fine so far in all tests)
- [ ] `DELETE`/`RENUM` end a running program; a "continue" (`CONT`) would need the code to be patched or recompiled while running
- [ ] `CLEAR` empties the variables; arrays can't be un-dimensioned by it (arrays exist from parse time), only emptied

Open: directory handling for `LOAD`/`SAVE` beyond plain paths (there is no `CD`; use a shell literal).

## 2. Smaller language items *(each needs a short design decision)*

- [ ] **Comments are converted to upper case** by the tokenizer (`' Ode to Joy` is listed as `' ODE TO JOY`; strings keep their case): decide whether comment text should keep its case (found while writing the demos)
- [ ] `WAIT`, `AFTER` and `EVERY` exist; no `FRAME` (wait for the next screen refresh) yet, handlers do not run during `INPUT`/`GET`


- [ ] `OPTION BASE` — lowest array index (0 or 1); when is it allowed (before the first `DIM`?)
- [ ] `INT` / `STR` / `FLT` — tokens exist; presumably default type declarations (like `DEFINT`/`DEFSTR`/`DEFSNG`): syntax and scope
- [ ] `PUT` — token exists; random access records will use `FIELD`, `GETREC`, `PUTREC` instead (decision of 2026-10-30, `docs/io-subsystem.md`); the token `PUT` stays unused for now
- [ ] `OFF` — token exists; only meaningful together with the timer statements below (`ON … OFF`?)

## 2b. Sound

`SOUND`, `PLAY`, `WAVE`, `TEMPO`, `SOUND OFF/SYNC` and `PLAYING()` work (`docs/sound.md`; `synth`, `mml`, `sdlaudio`; tested by `synthtest1`, `vmtest1`, `repltest1`, `asmtest1`).

- [x] Sound effects: `PITCH`/`VOLUME`, glide, `ENVELOPE`, `VIBRATO`/`TREMOLO`, stored `SFX` (done, `docs/sound.md`)
- [ ] Sound effects, more: a pitch envelope, a pulse width for the square wave, a filter for the noise, `SFX` with parameters, `EVERY` timers can now drive `PITCH`/`VOLUME` (try it in the engine demo)
- [ ] Instruments / FM synthesis (operators, envelopes, `INSTRUMENT`), band-limited square/triangle, stereo, a voice allocator
- [ ] Sample-accurate event times (now ≈1 ms), mixing events into the audio callback
- [ ] `PLAY`: no chords inside a string (use one string per voice), no `N` (note number) or `X` (run a string variable) commands, no repeat; `PLAY` doesn't wait (a program that ends while music is queued leaves it playing until `SOUND OFF`: decided)
- [ ] Sound output channels other than the BASIC channels (`PRINT #n`) are not involved; no `SOUND` queue inspection beyond `PLAYING()`

## 3. Timers and `SYMBOL` *(planned by the project owner, all through host callbacks)*

- [x] `AFTER` — one-shot timer, like `setTimeout` (done, in the VM: `docs/basic-language.md`)
- [x] `EVERY` — recurring timer, like `setInterval` (done)
- [ ] `SYMBOL` — redefine one character of the character set

Questions to decide: when timer handlers run (between VM instructions, then a call like `GOSUB`? what if one is already running?);
how a handler is named (line number / label / `FN`?); how they are cancelled (`OFF`?); host callback signatures for timers
(the screen thread already produces a vblank event per frame, see `docs/display-engine.md`) and for `SYMBOL` (`txtscr` font access).

## 4. Host polish (`engine1999.c`, `basrepl.[ch]`, `sdlscreen.c`)

- [ ] Typed lines are limited to 255 characters (input buffer in `engine1999.c`); fine for now, may be raised later
- [ ] Cursor keys and function keys for `GET` / `INKEY$` (only printable ASCII, RETURN, BACKSPACE, TAB are delivered; ESC is the
      break key; need a key-code convention, e.g. like the classic home computers)
- [ ] File channels (`#n` with `n` ≠ 0): being done by the I/O subsystem, see section 8 and `docs/io-subsystem.md`
      discards output / reports end of input for every channel except 0
- [ ] Direct mode: ordinary lines are rolled back after they ran (done), but a line that defines a function, a label or DATA keeps
      its code and data, and defining the same function again leaves the old code behind (dead) until `NEW` / `RUN`; a session that
      redefines functions thousands of times grows (slowly) toward the 256 MiB limit
- [x] Direct mode can `GOTO` / `GOSUB` / `AFTER ... GOSUB` / `RESTORE` to a program line number (the table of the last compile; empty when the program changed since, then `RUN` first)
- [ ] `PRINT` comma zones use the VM's own column count and ignore the screen wrap at column 80
- [ ] `vmtest1` still has its own copy of the compile-and-run logic; it could use `basrepl` (`repltest1` already does)
- [ ] Output on channel 0 only goes to the text screen: no graphics-layer / tile / sprite statements exist yet (the `sdlscr_*`
      functions for tiles, sprites and the graphics layer have no BASIC statements)

## 5. VM and instruction set leftovers

- [ ] `WRLV` — write a label variable (instruction exists in `instset.h`, reported as "not implemented" at run time)
- [x] `BRK` — stops the run when the monitor debugs (`vm_t.debug`); outside of it still the error "Breakpoint"
- [ ] Inline assembler: `TOK_BRKLIT` string literals (`[ … ]`) are tokenized but code generation says "not implemented yet";
      `TOK_BRCLIT` (`{ … }`) has no defined purpose yet
- [ ] Array parameters of user functions (`DEF FN A( B() )`) are parsed but not supported by the VM
- [ ] Functions can't call themselves and must be defined before use in the text (design decision, revisit if recursion is wanted)

- [ ] Machine monitor (`MONITOR`): no step-over (`t` steps into function calls), no conditional breakpoints or watches, no resumable `MONITOR` inside a program (it ends the run, like `EDIT`); the variables and the tokenized program text can't be inspected (only code, data and the two stacks); `a` has no labels or symbolic operands (variable names, line numbers); `e` takes hex bytes only (no strings); the monitor doesn't re-compile when the program changed (use `c`).

## 6. Remaining fixed limits

(also in `docs/refactoring.md`)

- [ ] 16-bit variable numbers (≤ 65535 variables), 6 array dimensions, one dimension ≤ 65535
- [ ] VM strings ≤ 65535 bytes
- [ ] `engine1999`: the command prompt line editor is limited to 255 characters (`INPUT` lines to 1023)

## 7. Design decisions taken that the owner may want to revisit

Listed in full in `SESSION.md` ("Design decisions I took"). The ones most likely to matter:

- [ ] `UNTIL` closes the innermost loop if it is a `REPEAT`, otherwise it opens an `UNTIL … UEND` loop (so such a loop can't be
      opened directly inside a `REPEAT` body)
- [ ] `INPUT`: the prompt is a string *literal* (not an expression); a missing or non-numeric field restarts the whole input
      ("?Redo from start") instead of asking for the rest; an empty numeric field is 0
- [ ] `GET` waits for a key (`INKEY$` is the non-waiting one); a numeric `GET` variable receives the key code
- [ ] ESC is the break key while a program runs (not delivered as a key)
- [ ] `CHR$(0)` is an error, because VM strings are NUL-terminated and can't contain a zero character
- [ ] `RUN` clears all variables first; direct mode lines keep them

## Not planned / explicitly out of scope for now

- Interpreting source directly: the system compiles to the stack-machine code and runs that (the old interpreter stub was removed).

## Later: look at again

- [ ] Font: review the symbol glyphs again (`% & @ $ ~ # *` and the other punctuation in `8x12font1.txt`); they are the tightest on the 8x12 grid. Also maybe `N` (close to `H` at small size).

## 8. I/O subsystem with virtual device drivers *(designed: `docs/io-subsystem.md`)*

- [x] **Phase 1** (done, 2026-10-30): `iodev.[ch]` with `file`, `null`, `mem` drivers and `driver:name` prefixes; channel table in the VM; `OPEN`, `CLOSE`, `LINE INPUT`, `WRITE`, `EOF`, `LOF`, `LOC`, `INPUT$`, `ADDRESS`, `ADDRESS$`; `PRINT #` / `INPUT #` / `GET #` on files; `LOAD`, `SAVE`, `MERGE`, `CHAIN`, `FILES` through the drivers
- [ ] Phase 2: `KILL`, `RENAME … TO`, `MKDIR`, `CHDIR`, `SEEK`; drivers `sh`, `res`, `con`
- [ ] Phase 3: random access records (`FIELD`, `LSET`, `RSET`, `GETREC`, `PUTREC`); drivers `play`, `tcp`; drivers written in BASIC
- [ ] **Sandboxing (open issue)**: permissions for shell literals, `sh:`, writing/reading outside the program directory, the network; how a policy is configured (command line switch, trust level per program, permission file) is undecided; `iodev_allowed()` is the single place for the decision
- [ ] Files stay open across `CHAIN` in GW-BASIC; here a restart (`RUN`, `NEW`, `LOAD`, `CHAIN`) closes all channels
- [ ] `LIST #n` / `LIST` to a file channel (`LIST` still writes to channel 0 only; with the drivers it could write a listing to `mem:` or a file); `PRINT` comma zones on a file channel use the screen column
- [ ] Binary data in files: a zero byte can't be held in a string (`GET #n`, `INPUT$`); text files only until random access / binary records exist
