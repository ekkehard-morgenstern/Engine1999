# Roadmap — Language

Part of the roadmap: see [the central roadmap](../roadmap.md). The BASIC dialect: statements, functions, operators, types, program commands, timers, design decisions and ideas from other BASICs.

**Open items of this part** (the priorities are explained in the central roadmap; items that are done are in [done.md](done.md)):

| # | Item (section) | What it is | Priority | Notes |
|---|---|---|---|---|
| 12 | `FRAME` done; handlers during `INPUT`/`GET` (2) | **`FRAME [n]` is done 2026-11-01**; timer handlers still do not run while waiting for input | Medium-Low | `VSYNC` and `FRAME` cover the games |
| 13 | `SWITCH` / `CASE` (13) | multi-way branch | Medium | design questions: fall-through, strings, `AGAIN` / `EXIT` levels (*decision*) |
| 23 | (`OPTION BASE` and `DEF type range` struck 2026-11-08: not language features) `TAB`/`TABMODE`, `DISPLAY(x)`, `BREAKPOINT` (2, 13) | assorted language ideas | Low | each needs a short design decision (*decision*) |
| 24 | `DO … LOOP`, `FOR EVER` (11) | test-at-the-end loop | Low | `REPEAT`/`WHILE`/`UNTIL` cover it |
| 26 | `DYNAMIC`/`ASSOC` arrays, `RECORD`/`TYPE` (11) | structured data | Low | long-term (*decision*) |
| 31 | `RENUM` / `DELETE` / `CLEAR` corners (1) | range of another `RENUM` statement is not adjusted; both end a running program; `CLEAR` can't un-dimension | Low | |
| 35 | Direct mode and `PRINT` corners (4) | redefined functions leave dead code; comma zones ignore the screen wrap at column 80 | Low | |
| 37 | Design decisions to revisit (7) | `UNTIL` inside `REPEAT`, `INPUT` prompt literal, `GET` waits, `RUN` clears variables | Low | only if the owner wants them changed (*decision*) |

## 1. Program commands as real language statements *(done: `LOAD`, `RUN`, `SAVE`, `LIST` ... are statements; leftovers below)*

`RUN [line|label]`, `LIST [a][-[b]]`, `LOAD name$`, `NEW`, `CLEAR`, `DELETE a[-[b]]` and `RENUM [a][-[b]][,step]` are language statements now (usable in programs, see
`docs/basic-language.md`). The rest of the family has nothing yet.

- [x] `BUILD name$` is done (2026-10-31, `docs/basic-language.md`): a standalone executable made with the system linker only. Not done: a `make install` that puts `engine1999.o` / `engine.lib` into a kit directory (today they are found next to `engine1999`), a compile check before linking, static linking of SDL2 on request, other systems than Linux/ELF, a build for another CPU than the building one (the kit is compiled with `-march=native`), an icon / window title for built programs, command line arguments for the program (`--engine1999-about` is the only option), bundling the program's data files (`res/`) into the executable
- [x] (`LIST #1,10-20` done 2026-11-08) `LIST` with an output channel (`LIST #1,10-20`; today always the VM's channel 0) and `RUN`/`LOAD` taking more arguments?
- [x] (done: `CHDIR`, `MKDIR`, `dir:` driver, file requester library) `LOAD`/`SAVE`: file names are plain paths relative to the current directory; no directory handling (default extensions are done: `.bas` ASCII / `.e99` binary, see `docs/basic-language.md`)
- [x] `MERGE`, `CHAIN`, `FILES` are done (see `docs/basic-language.md`); not done: `COMMON`, `CHAIN … DELETE`, a line range for `MERGE`, paging for `LIST`
- [ ] **Indent fixer** (owner, 2026-11-01; since the indentation of program lines is kept, `docs/basic-language.md` "Source preprocessing"): re-indent a program by its block structure with one key, **F9 in the full-screen editor** (the owner chose F9 as the key most likely to be unused: F12 is taken by the screen editor's INS/OVR, and F11 is planned for the full screen / window switch, item 44). *Rules, as used on the demos (2026-11-01):* two blanks per level; `FOR`..`NEXT` (`NEXT I,J` closes two), `REPEAT`..`UNTIL`, `WHILE`..`WEND`, `UNTIL`..`UEND`, multi-line `IF` / `UNLESS` (`ELSE` dedented, closed by `ENDIF` / `END IF`), `PROC`, `FUNC` and multi-line `DEF FN` with their `END ...`; statements after a colon count, leading closers dedent their own line; `GOSUB` bodies are not indented (nothing marks their end). *Design:* a shared module without SDL (`basindent.[ch]`, working on the tokens of a line, which knows the keywords exactly, instead of the text scan of the throw-away script), used by both hosts' full-screen editors (the standing consistency rule: the console editor gets the same key, F9 = `ESC [ 20 ~`) and by a statement for direct mode and programs (`INDENT [a][-[b]]`, perhaps `INDENT OFF` to strip) so that scripts and the console line mode have it, too. *Questions to settle:* undo (one keystroke that rewrites the whole program should be undoable or ask first; the editor has no undo yet), a block that does not match (`NEXT` without `FOR`: report the line and leave the program alone), the width (2, maybe a setting), whether it works on the whole program or from the cursor line to the end / a marked range, and what to do with comment-only lines (they take the level of their neighbour now). The result must not change the compiled code (check: `--debug-code` output identical before and after, as was done for the demos).
- [ ] Full-screen editor (`EDIT`, first version done): mouse support (scroll bar, wheel, click to place the cursor: done 2026-11-01; no selection), no search/replace, block or clipboard operations, no Unicode; lines over 1656 characters are read-only; the position memory is by line number only. `EDIT n` still uses the one-line editor.
- [ ] `RENUM` also adjusts targets in `RUN`/`LIST`/`DELETE`, but not the range of another `RENUM` statement in the program, and a changed line is
      re-tokenized from the listing text (a round-trip through `detokenize_line`/`tokenize_line`; fine so far in all tests)
- [ ] `DELETE`/`RENUM` end a running program; a "continue" (`CONT`) would need the code to be patched or recompiled while running
- [ ] `CLEAR` empties the variables; arrays can't be un-dimensioned by it (arrays exist from parse time), only emptied

Open: directory handling for `LOAD`/`SAVE` beyond plain paths (there is no `CD`; use a shell literal).

## 2. Smaller language items *(each needs a short design decision)*

- [x] **Comments keep their case** (done 2026-11-01, `preprocess_buffer()` in `bastok.c`: the case of everything behind an apostrophe is kept; the old listings keep the capitals they have)
- [x] `FRAME [n]` (wait for the next screen refresh) is done (2026-11-01); `WAIT`, `AFTER` and `EVERY` exist; [ ] handlers do not run during `INPUT`/`GET`


- [x] (struck 2026-11-08, owner: not a language feature) `OPTION BASE` — lowest array index (0 or 1); when is it allowed (before the first `DIM`?)
- [x] (struck 2026-11-08, owner: no `DEF INT/LNG/SNG/DBL`; the tokens 75..77 are unused now) `INT` / `STR` / `FLT` — tokens exist; presumably default type declarations (like `DEFINT`/`DEFSTR`/`DEFSNG`): syntax and scope (the owner's `../basic1` has `DEF type range`, see section 13)
- [x] (done 2026-11-08: `PUT` is an alias of `PUTREC`) `PUT` — token exists; random access records will use `FIELD`, `GETREC`, `PUTREC` instead (decision of 2026-10-30, `docs/io-subsystem.md`); the token `PUT` stays unused for now
- [x] (done: `OFF` is used by `BREAK OFF`, `ON ... OFF`, `ON KEY OFF` ...) `OFF` — token exists; only meaningful together with the timer statements below (`ON … OFF`?)

## 3. Timers and `SYMBOL` *(done, all through host callbacks)*

- [x] `AFTER` — one-shot timer, like `setTimeout` (done, in the VM: `docs/basic-language.md`)
- [x] `EVERY` — recurring timer, like `setInterval` (done)
- [x] `SYMBOL` — redefine one character of the character set (done with the graphics subsystem, 2026-10-31: `SYMBOL code, a$`; stale entry found 2026-11-01)

Questions to decide: when timer handlers run (between VM instructions, then a call like `GOSUB`? what if one is already running?);
how a handler is named (line number / label / `FN`?); how they are cancelled (`OFF`?); host callback signatures for timers
(the screen thread already produces a vblank event per frame, see `docs/display-engine.md`) and for `SYMBOL` (`txtscr` font access).

## 7. Design decisions taken that the owner may want to revisit

Listed in full in `SESSION.md` ("Design decisions I took"). The ones most likely to matter:

- [ ] `UNTIL` closes the innermost loop if it is a `REPEAT`, otherwise it opens an `UNTIL … UEND` loop (so such a loop can't be
      opened directly inside a `REPEAT` body)
- [ ] `INPUT`: the prompt is a string *literal* (not an expression); a missing or non-numeric field restarts the whole input
      ("?Redo from start") instead of asking for the rest; an empty numeric field is 0
- [ ] `GET` waits for a key (`INKEY$` is the non-waiting one); a numeric `GET` variable receives the key code
- [x] ESC is the break key while a program runs: done, with Ctrl+C and (window) PAUSE/BREAK as further break keys and `BREAK OFF` / `BREAK ON` to let a program have ESC (2026-11-01; `docs/basic-language.md` "Keyboard"); double-stroke ESC was considered and dropped
- [x] `CHR$(0)` works (binary strings, 2026-10-31)
- [ ] `RUN` clears all variables first; direct mode lines keep them

## 11. Ideas from the AsmBASIC project *(`../asmbasic`: grammar `defaultsyntax.ebnf`, `docs/language.md`; 2026-10-31)*

AsmBASIC (the owner's earlier x86-64 assembly BASIC) only got as far as the tokenizer and parser, so nothing there can be reused as code; what is
useful is the language it planned. The items below are in the order I would suggest.

- [x] **Loop control** (done, 2026-10-31): `EXIT [n]` / `LEAVE [n]`, `ITERATE [n]`, `FOR … DOWNTO …`, `FOR EACH [key,] v IN a()` (`docs/basic-language.md`, "Loop control")
- [ ] `DO … LOOP [WHILE|UNTIL cond]` (test at the end) and `FOR EVER`; `END IF` / `END UNLESS` exist already (`ENDIF`, `ENDUNLESS`). Low priority: `REPEAT/UNTIL`, `WHILE/WEND` and `UNTIL/UEND` cover it
- [x] **Procedures with local variables** (done, 2026-10-31: `PROC`/`FUNC`/`CALL`/`LOCAL`/`RETURN value`/`EXIT PROC`, recursion, array parameters by reference, local arrays; not done: `GLOBAL`, passing by reference, `EXEC`, a forward call of a `FUNC` (a `PROC` can be called before it is defined); see "Procedures" in `docs/basic-language.md`)
- [x] `RESUME` (done, see section 9)
- [x] `STOP`, `CONT`, `TRACE ON|OFF`, `AUTO` (done, 2026-10-31; `STOP [n]`: no argument; not continuable: Break inside `INPUT`/`GET`/`WAIT`; no `AUTO` prompt outside `engine1999.c`)
- [x] **Operators** (done, 2026-10-31): the logical operators `IMP NIMP EQV NEQV` the shifts as keywords `SHL SHR ASL ASR` (`LSHIFT`/`RSHIFT` tokens exist)
- [x] Math functions `FLOOR CEIL LOG2 LOG10` (done, 2026-10-31; `LN`/`LG`/`LOGN` not added; cf. the `ASN`/`ACS`/`ATAN2`/hyperbolic item in section 2)
- [x] `PEEK`/`POKE`/`PEEK$` on a 1 MiB RAM area (done, 2026-10-31); not done: `SYS addr[,args]`, a window into the data segment / code / screen memory, `PEEKB/W/L/F` spellings
- [x] (done: `PRINT USING`, `FORMAT$`, `AT`, `HTAB`, `VTAB`; open: `LPRINT USING`) `PRINT USING fmt$` (formatted output); `AT(row[,col])`, `HTAB`, `VTAB` in `PRINT` (`LOCATE` covers the placement)
- [x] `TIME$`, `DATE$`, `DATETIME$` (done, 2026-10-31; read only: no setting of the clock)
- [ ] Small aliases: `OLD` for `LOAD`, `CATALOG` for `FILES`, `RENUMBER` (these three done 2026-11-08), `FUNCTION`/`PROCEDURE` spellings; `YIELD` (give the host a time slice) and `SAFETY ON|OFF` (the sandbox switch; see section 8) are AsmBASIC's ideas, too
- [ ] To think about (the owner decides): `DYNAMIC`/`ASSOC` arrays (parsed since the start, no meaning: `DIM a(n, DYNAMIC, ASSOC)`), `RECORD`/`TYPE`, number tokens that keep their base (check that `&H1F` lists back as hex), AsmBASIC's "undimensioned arrays are 0..10" rule

> **Standing rule (owner, 2026-10-31): editing and input behave the same in the window and the console version** (line input, one-line editor, full-screen editor, monitor, break keys, key codes). Differences are listed here with their reason: Ctrl-D on an empty line ends the input in the console (the window has a close button); the graphics statements are not available in the console host yet.

- [ ] **C-64 style screen editor for text screen 0** (owner's idea, 2026-11-01; branch `feature/c64-like-screen-editor`): **requirements and design in `docs/screen-editor-proposal.md`; phases 1 (cell flags, overlays), 2 (the editor in the window, `scrtest1` 120) and 3 (the console host's screen mode, `contest1` 143: `docs/console-host.md` "Screen mode") are implemented (2026-11-01).** Tried by the owner (2026-11-01): resize while editing, closing the terminal window, a built console executable, `ssh` (localhost). **Still to test: plain 256-color versus truecolor terminals (`COLORTERM=truecolor`; the translucent entries are composited by `conscr_visible()` and mapped to the nearest xterm color on a plain terminal: check that the highlight, the status line and the SlateGray paper look right there), other terminal programs;** not done in screen mode: colors in the exit dump, `GFX`/`SPRITE`/`TILE` (no graphics layers on a terminal); `AUTO` and Ctrl-D (deletes a character) are done. Decided: cell flags EMPTY / WRAP in the unused top byte (no font change), overwrite mode by default with F12 (and Shift+INSERT) toggling INS/OVR, INSERT/DELETE/BACKSPACE working on the logical line or, in a gap of a non-blank row, on the rest of the screen row, free cursor, BACKSPACE never deletes beyond the line, no scroll-back, a white-on-red status line (mode and the F12 key, row, column, BASIC line number) on the bottom row and the screen rows of the current logical line highlighted, both render-time overlays shown only while the editor waits for a program line (the editor then uses a viewport of rows - 1; programs always have the full screen), the editor active only while the system waits for a program line (`EDIT n`, `AUTO`, `INPUT` keep the line editor), the console host's BASIC session on the alternate screen (also run mode and `BUILD` executables; switches; exit dump; an error waits for a key). The row under the status line is cleared when the state is entered. Open for later (proposal §7): what the screen editor does at the end of the input / Ctrl-D in the console (it is never left by Break). **To test: the editor's highlight after `GFX BACKDROP` / `GFX PALETTE` changes (window).** (The status line is white on `TransparentDarkRed`, entry 190, decided 2026-11-01.)

## 13. Ideas from the BASIC 1 concept *(`../basic1/concept.txt`; found 2026-10-31, nothing decided or started)*

- [ ] `SWITCH expr` / `CASE value[,value…]` / `DEFAULT` / `END SWITCH` — multi-way branch (the engine only has `ON … GOTO/GOSUB`); `AGAIN` inside a `SWITCH` jumps back to the evaluation of the `SWITCH`, `EXIT` leaves it; to decide: fall-through or not, string and range cases, interplay with the compile-time loop stack (`AGAIN`/`EXIT` count it as a level?)
- [ ] `TAB [a[,b…]]` and `TABMODE n` — programmable tab stops for screen and file output; modes 0 TABs become spaces at arbitrary stops, 1 hard tabs (kept in files, cursor jumps on screen), 2 all stops equal the first value, 3 = 2 with hard tabs, 4 one space, 5 TABs ignored; to decide: the clash with the existing `TAB(n)` print function (if any), where the stops live (per channel?), the effect on the `PRINT` comma zones
- [x] (struck 2026-11-08, owner: not wanted) `DEF type range` — default variable type by first letter (`DEF STR a-c`, types `STR`, `INT`, `FLT`, …); this is the likely meaning of the unused `INT`/`STR`/`FLT` tokens (section 2); to decide: syntax (`DEF` is taken by `DEF FN`), scope (affects variables created after the statement), interplay with the `$`/`%` suffixes
- [ ] `DISPLAY(x)` — display properties: 0 horizontal resolution, 1 vertical resolution, 2 vertical frequency (Hz), 3 vertical blanking interrupt info; fits next to the `GFX` queries (`vm_gfx_t` query host callback; the console host would return its own values or an error)
- [ ] `BREAKPOINT` — a statement that opens the machine monitor at that point (the VM and the monitor have `BRK` and breakpoints already, but there is no BASIC word); to decide: behavior in `engine1999c` and built executables (ignore?), name versus the monitor's `b` command
- Not taken over from that file (already done or differently): `AGAIN`, `PEEK`/`POKE` (one word with a size argument), `RND`, `FLOOR`/`CEIL`, `PRINT USING` and `AT()` (open, section 11), `OPEN … FOR …`, `RANDOMIZE`.

## 16. Library support *(owner's idea, 2026-11-03; **done 2026-11-03**; design and what was built: [library-support.md](../library-support.md))*

Revised the same day after the owner's decisions: programs load libraries with **`LOAD LIBRARY "name"`** (no `USE`), and **the user never types it at the prompt**: it is a declaration that the compiler resolves while `RUN` compiles (a pre-pass over the program's tokens finds the names, the libraries are compiled into the same code and data segments *before* the program, so their `FUNC`s are known to the parser; at run time the statement does nothing). A library is an ordinary program file whose first statement is `LIBRARY "name"[,"prefix"]`; only `PROC` / `FUNC` / `DEF FN` / labelled sections and `LOAD LIBRARY` are allowed in it; it cannot be run; plain `LOAD` still works for editing it, so **the file format does not change**. Line numbers are local to a library, names are global (clash = compile error), run time errors name the library, variables follow `LOCAL` + a prefix and anything else is a compile error, the runtime calls `<prefix>INIT`, `DATA` is allowed (unlabelled = private to the library, per-module chains and READ pointers), `SAVE LIBRARY` writes the optional `.lib` / `.l99`, `LIBRARIES` lists the loaded ones, `BUILD` embeds the libraries in a container. Search path: current directory, `lib/`, the program's own directory, the installed `lib/engine1999/lib/`. The plan (7 steps) is in the design document; **the owner settled decisions a to g on 2026-11-03** (recorded there).
