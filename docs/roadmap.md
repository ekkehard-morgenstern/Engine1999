# Roadmap — what is still to do

The list of open work, so that nothing is forgotten. `[ ]` = open, `[x]` = done (moved here when finished, then the entry can be
dropped). **Status of what already works** is in `SESSION.md` (state, tests, design decisions taken); this file is only about
what is **not** done. Items marked *needs design* wait for a language decision by the project owner before they can be implemented.
Order = suggested order, not a commitment.

## 1. Program commands as real language statements *(needs design — suggested next)*

`RUN [line|label]`, `LIST [a][-[b]]`, `LOAD name$`, `NEW`, `CLEAR`, `DELETE a[-[b]]` and `RENUM [a][-[b]][,step]` are language statements now (usable in programs, see
`docs/basic-language.md`). The rest of the family has nothing yet.

- [ ] `LIST` with an output channel (`LIST #1,10-20`; today always the VM's channel 0) and `RUN`/`LOAD` taking more arguments?
- [ ] `LOAD`/`SAVE`: file names are plain paths relative to the current directory; no `.BAS` default extension, no directory handling
- [ ] `EDIT`, `MERGE`, `CHAIN`, `FILES`
- [ ] `RENUM` also adjusts targets in `RUN`/`LIST`/`DELETE`, but not the range of another `RENUM` statement in the program, and a changed line is
      re-tokenized from the listing text (a round-trip through `detokenize_line`/`tokenize_line`; fine so far in all tests)
- [ ] `DELETE`/`RENUM` end a running program; a "continue" (`CONT`) would need the code to be patched or recompiled while running
- [ ] `CLEAR` empties variables but can't drop code, so it does not cure the growth of the code segment by direct mode lines (see
      section 4); arrays can't be un-dimensioned by `CLEAR` (arrays exist from parse time), only emptied

Questions to decide: what `CHAIN`
and `MERGE` do with variables and line numbers; where files live (current directory?); what `FILES` shows.

## 2. Smaller language items *(each needs a short design decision)*

- [ ] `OPTION BASE` — lowest array index (0 or 1); when is it allowed (before the first `DIM`?)
- [ ] `INT` / `STR` / `FLT` — tokens exist; presumably default type declarations (like `DEFINT`/`DEFSTR`/`DEFSNG`): syntax and scope
- [ ] `PUT` — token exists, probably the counterpart of `GET` for channels/files: meaning to be defined
- [ ] `OFF` — token exists; only meaningful together with the timer statements below (`ON … OFF`?)

## 3. Timers and `SYMBOL` *(planned by the project owner, all through host callbacks)*

- [ ] `AFTER` — one-shot timer, like `setTimeout`
- [ ] `EVERY` — recurring timer, like `setInterval`
- [ ] `SYMBOL` — redefine one character of the character set

Questions to decide: when timer handlers run (between VM instructions, then a call like `GOSUB`? what if one is already running?);
how a handler is named (line number / label / `FN`?); how they are cancelled (`OFF`?); host callback signatures for timers
(the screen thread already produces a vblank event per frame, see `docs/display-engine.md`) and for `SYMBOL` (`txtscr` font access).

## 4. Host polish (`engine1999.c`, `basrepl.[ch]`, `sdlscreen.c`)

- [ ] Cursor keys and function keys for `GET` / `INKEY$` (only printable ASCII, RETURN, BACKSPACE, TAB are delivered; ESC is the
      break key; need a key-code convention, e.g. like the classic home computers)
- [ ] File channels (`#n` with `n` ≠ 0): `PRINT #n`, `INPUT #n`, `GET #n` are accepted and passed to the callbacks, but the engine
      discards output / reports end of input for every channel except 0
- [ ] Direct mode lines accumulate in the code segment until `NEW` / `RUN` (every line appends its code; a long session ends
      in "Out of code memory"). Options: roll back the code of a direct line that defined nothing persistent, or reset on `NEW`/`RUN` as today
- [ ] Direct mode can't `GOTO` / `GOSUB` into a stored program (the line table belongs to the code of a `RUN`)
- [ ] `PRINT` comma zones use the VM's own column count and ignore the screen wrap at column 80
- [ ] `vmtest1` still has its own copy of the compile-and-run logic; it could use `basrepl` (`repltest1` already does)
- [ ] Output on channel 0 only goes to the text screen: no graphics-layer / tile / sprite statements exist yet (the `sdlscr_*`
      functions for tiles, sprites and the graphics layer have no BASIC statements)

## 5. VM and instruction set leftovers

- [ ] `WRLV` — write a label variable (instruction exists in `instset.h`, reported as "not implemented" at run time)
- [ ] `BRK` — breakpoint (reports "Breakpoint"); could be the hook for a future debugger
- [ ] Inline assembler: `TOK_BRKLIT` string literals (`[ … ]`) are tokenized but code generation says "not implemented yet";
      `TOK_BRCLIT` (`{ … }`) has no defined purpose yet
- [ ] Array parameters of user functions (`DEF FN A( B() )`) are parsed but not supported by the VM
- [ ] Functions can't call themselves and must be defined before use in the text (design decision, revisit if recursion is wanted)

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
