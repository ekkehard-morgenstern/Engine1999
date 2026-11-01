# A C-64 style screen editor for text screen 0 — understanding and evaluation

*Written 2026-11-01 for the owner's review. **Nothing in this document is implemented.** It restates the request, says what I learned from
`../gforthstuff/forthbasic`, evaluates the idea against the code as it is today, proposes a design, and lists the decisions that are yours.*

## 1. Verdict in short

**Yes, the idea is reasonable and fits the engine well** — in the window version it is a moderate amount of work, and it gives the
BASIC prompt its classic feel (LIST, cursor up, edit the line number, RETURN; "history" for free, and a natural way to edit a program without
`EDIT`). I recommend three changes to the details you proposed, none of which changes the user-visible behavior you described:

1. **Do not use the NUL byte (and its glyph) as the "empty cell" marker; use a flag bit in the cell instead.** A cell is 32 bits wide and the top
   8 bits are unused. A flag "never written / cleared" keeps `CHR$(0)` printable and visible (the binary-strings feature and its control glyph
   stay as they are), needs **no font change at all**, and makes the "line ends at an empty cell" rule exact. (§5.1)
2. **Mark row wrapping with a second flag bit instead of inferring it from the last column** (what your Forth version does). Inference merges two
   different lines whenever one fills a row exactly. (§5.2)
3. **Say clearly what happens in the console host**, which has no text screen 0 to look at: the editor can only exist there if the console
   BASIC session runs on the alternate screen (like `EDIT` and `MONITOR` do today). I propose window first, console as a later opt-in mode. (§5.7)

Everything else you described — a line is the run of non-empty cells around the cursor, RETURN enters it into the REPL, cursor up/down navigate and
scroll, only while the system waits for a program line — I understood and would implement as described.

## 2. My understanding of the request

- A **screen editor** like the Commodore 64's: the text on the screen *is* the buffer. The user moves the cursor anywhere, edits what is there,
  and presses RETURN on a line to hand that line to the REPL, wherever the line was typed or printed (a `LIST` line, an old command).
- It exists **only on text screen 0** (the BASIC screen) and **only while the system waits for a program line**: the prompt, and `AUTO`.
  `INPUT`, `LINE INPUT`, the monitor and `EDIT` keep the line editor they have now.
- It is **a special case of the general line editor** (`scr_lineedit()` / the key handling of `scredit.c`): the same keys and the same behavior,
  but the text lives in the screen cells instead of a private buffer.
- **Line extent:** an *empty* cell is the default content of text screen 0 (`CLS` fills with it). A line begins at a non-empty cell and ends at an
  empty cell (not included), found by scanning left and right from the cursor (across screen rows, so a long line wraps).
- **Navigation:** cursor up/down move between rows and **scroll the screen** when they reach the top or bottom.
- **RETURN** takes the whole line under the cursor (not only the part left of the cursor) and enters it like a typed line.
- You suggested making the **NUL glyph blank** so that NUL cells look empty, and leaving the glyph untouched until the code is tested.

## 3. What the Forth version does (`../gforthstuff/forthbasic`)

Read: `screenupd.fs`, `keyboard.fs`, `execution.fs` (the rest is attributes, escape sequence output and window size handling).

| Mechanism | How it works there |
|---|---|
| Screen buffer | `b-window-buffer`: one cell per screen position (character in the low byte, attribute above). Cell value 0 = empty. It is a *shadow* of the terminal that is kept in step by hand (`b-refresh-lines` rewrites rows with ANSI sequences). |
| Logical line | Rows are linked **by the last column**: a row continues into the next if its rightmost cell is non-zero (`b-down-extent`, `b-up-extent`). The end of the line is the last non-zero cell of the final row (`b-y-line-width` counts back from the right edge). The beginning is always column 0 of the first row. |
| Grabbing | `b-grab-line` finds the extent and turns the **zero cells inside it into spaces** (`b-line-init-addr`), so a line whose interior was never written becomes ordinary text. |
| RETURN | `b-execute-line` copies the cells of the whole line (low bytes) into a buffer and tokenizes it; then the cursor goes down one row (`b-output-return`). |
| Cursor up/down | `b-anticipate-cursor-up/down`: move one row; at the edge **scroll the buffer** (`b-scroll-buffer-down/up`: copy rows, zero-fill the revealed row). Scrolling down *drops the bottom row*: there is no scroll-back. |
| Typing | Overwrites the cell under the cursor (`b-emit`), no insert mode yet; BACKSPACE moves left and writes a space; Delete, Insert, Home, End, Page keys are stubs. |
| Marking | The current line is highlighted by changing the attribute of its cells (`b-mark-line` / `b-unmark-line`) and repainting; this is how the user sees the extent of the line that RETURN would take. |

So the model you describe for the engine is the model of the Forth version, with "non-NUL begins, NUL ends" stated as a scan instead of a column
rule. Two things in the Forth version are *not* needed here: the shadow-buffer bookkeeping (the engine's text screen already is the buffer, and
the screen thread draws it) and the terminal escape sequence handling.

## 4. How it fits the engine as it is today

Facts I checked in the code:

- **Cells.** `textcell_t` (`textscreen.h`) is 32 bits: character in bits 0–7, pen 8–15, paper 16–23, **bits 24–31 unused**. `TXTSCR_CELL_*`
  macros mask only their own bytes, so extra bits do not disturb existing readers (`TXTCHAR()` etc. in the host mask with `0xFF`, `>> 8`, `>> 16`).
- **Blank cells.** Today the blank is a *space* in the screen's blank colors (`txtscr_blankcell()`, used by `txtscr_clear()`, `txtscr_scrollup()`,
  `txtscr_scrolldown()`, `txtscr_resize()`, the initial fill). All of these are in one file, `textscreen.c`.
- **Wrapping.** Text that runs over the right edge continues in the next row (`txtscr_print_n()`: `if ( cx >= cur->vw ) { cx = 0; ++cy; ... }`),
  scrolling at the bottom. That is the single place where a "this row continues" fact is created.
- **PRINT** pads comma zones with real spaces (`vm.c`, `VM_PRINT_SEP_COMMA`), so `PRINT` output does not leave gaps of empty cells inside a line.
  `LOCATE` + `PRINT`, `TXT` statements and `CLS` can of course leave empty cells.
- **Screens.** Text screen 0 is BASIC's; screens 1 and 2 belong to `EDIT` and `MONITOR` (they already run on the driver of `scrdrv.h`).
- **Line input today.** The prompt calls `sdlscr_lineinput()` → `scr_lineedit()`: the line lives in a private buffer, is drawn at the cursor, and
  RETURN returns the buffer. The editor never *reads* the screen.
- **Console host.** Output goes to the normal, scrolling terminal; there is **no text screen 0 in the console host** (it only has the text screens
  for `EDIT`/`MONITOR`, on the alternate screen).
- **Tests that touch this:** `txttest1` (the text screen), `gfxtest1`, `scrtest1` (editors on a fake driver), the pty cases of `contest1`.

## 5. Evaluation

### 5.1 NUL as "empty": the problem with using the byte value

Using code 0 as the empty marker (as in the Forth version) works, but in *this* engine it costs more than it appears:

- **`CHR$(0)` is a real character here** (binary-safe strings, 2026-10-31). A program that prints a NUL would create a hole that the editor reads
  as the end of a line; and if the NUL glyph were blanked, the NUL would also be *invisible* on every screen, which undoes the "binary data on
  the screen is visible" feature for code 0 (the other 31 control codes keep their glyphs).
- **`txttest1` checks that no glyph is blank except space and NBSP** — that check would have to be loosened.
- **The cell would change meaning** for the `TXT` functions (`TXTCHAR()` of an empty cell would return 0 instead of 32) and for `TXT SAVE`/`RESTORE`.

**Alternatives**

| Option | Empty cell is… | Font | `CHR$(0)` | Cost |
|---|---|---|---|---|
| A (yours) | character 0 | glyph 0 blank | invisible, ends a line | font change, test change, behavior change |
| A′ | character 0, blank **only when drawn on screen 0** (renderer rule) | untouched | invisible on screen 0, glyph on 1 and 2 | small renderer rule; still splits lines at a printed NUL |
| **B (recommended)** | any character with a **flag bit 24 = EMPTY** | **untouched** | unchanged: a printed NUL is a normal cell with the glyph | `textscreen.c` (clear/scroll/fill/init) sets the flag; `txtscr_write`/`print` clear it |

With B the empty cell can still *look* like a space (character 32, flag set) — nothing changes visually — and `TXTCHAR()` keeps answering 32 for it.
`txtscr_save()`/`restore()` store 4 bytes per cell and the 4th is unused today, so they can carry the flags unchanged. If you prefer A, it is a
one-day change in the font and tests; B is the cleaner design, and B can be switched to "also character 0" later without touching the editor,
because the editor would only ever ask `txtscr_isempty( cell )`.

### 5.2 Where a logical line ends: rows need an explicit link

"Scan left and right across rows until an empty cell" has a hole at the row boundary: if one line fills a row exactly (80 characters), the last cell
of the row and the first cell of the next row are both non-empty, so the scan **merges two unrelated lines** (a long `LIST` line followed by the next
line, for example). The Forth version has the same property (it tests the last column). The C-64 solves it with a per-row link table.

**Recommendation: flag bit 25 = WRAP** on the last cell of a row that was left by *wrapping* (set in `txtscr_print_n()`, which is the only place
that wraps; cleared when the cell is overwritten by a non-wrapping write, cleared with the row by clear/scroll). The scan then crosses a row
boundary only when the WRAP flag is set; otherwise the row boundary ends the line like an empty cell would. Cost: ~10 lines in `textscreen.c`.

### 5.3 The cursor cell and "empty" gaps

Precise rule I would implement (it matters: after typing the cursor sits *on an empty cell behind the text*):

1. The line under the cursor is the maximal run of non-empty cells, linked across rows by WRAP, that contains the cursor cell **or the cell
   directly left of it** (whichever is non-empty; the left one wins if both are, which cannot happen since they are in one run).
2. If both are empty, the line is empty: RETURN just moves to the start of the next row.
3. The run ends at the last non-empty cell before the first empty cell (empty cells are never part of a line). Interior gaps (from `LOCATE`
   output) therefore **split** a printed row into several lines; typed and `PRINT`ed text has no gaps.
4. RETURN enters the **whole** run (cells left *and* right of the cursor), trailing spaces included (the tokenizer ignores them).

This differs from the Forth version in one detail: it turns interior empty cells into spaces ("grab") so a line with a gap is still one line;
you described "ends at a NUL", so I keep gaps as line ends.

### 5.4 Scrolling at the top and bottom

At the bottom edge cursor-down scrolls the screen up (the existing `txtscr_scrollup()`; the top row is lost, as when output scrolls). At the **top**
edge cursor-up would scroll *down*: a blank row appears at the top and the **bottom row is lost**, exactly as in the Forth version — only useful if
there is something above to reveal, and there is not: the engine keeps no scroll-back. Options:

- **Minimal:** cursor up at the top row does nothing (the C-64 behavior: it stops at the top). No data loss.
- **Better, recommended as a second step:** a **scroll-back ring** per text screen (say the last 500–1000 rows, cells with flags). Rows scrolled off
  the top go into it; cursor up at the top (and PgUp) brings them back, pushing the bottom row into a small forward ring. Then "scroll up and down if
  needed" works in both directions without losing text, and the user can review earlier output. Memory: 1000 × 80 × 4 bytes = 320 KB. It touches
  only `txtscr_scrollup()` / `txtscr_scrolldown()` and a new "scroll the view" call.

### 5.5 Keys and modes

Proposed behavior (the keys of the general editor, applied to cells). Open points are marked **?**.

| Key | Behavior |
|---|---|
| printable character | writes the cell under the cursor and moves right (wrapping sets the WRAP flag; at the bottom right the screen scrolls up). **? Overwrite (C-64) or insert (the general editor's way)?** I would take *insert* as the default for consistency with the other editors, and INSERT toggles to overwrite. |
| LEFT / RIGHT | one cell, across row boundaries (up to the line's first and last cell? **?** C-64 moves freely over empty cells and rows; I propose freely, so the user can step into empty areas to start new text) |
| UP / DOWN | one row, same column; at the top/bottom scroll (§5.4) |
| HOME / END | start / end of the **logical line** (as in the line editor). Ctrl+Home / Ctrl+End: top-left / bottom-left of the screen (as in the full-screen editor). |
| BACKSPACE | deletes the cell left of the cursor and pulls the rest of the **line** one cell left (empty cell appended at its end); at a line start nothing |
| DELETE | the same at the cursor |
| INSERT | toggles insert/overwrite mode |
| RETURN | enter the line (§5.6); the cursor goes to the start of the row below the line (scrolling at the bottom) |
| ESC | nothing at the prompt (there is nothing to cancel; it still is the break key while a program runs) |
| PgUp / PgDn | scroll-back (§5.4), ignored in the minimal version |
| Ctrl+C, Ctrl-D (console), TAB, function keys | as in the general editor (ignored / break / EOF) |

Colors: new text is written in the current pen/paper (`COLOR`). I would **not** copy the Forth version's line highlight (`b-mark-line`): it repaints
cell attributes on every key and would fight with colored `LIST` output; the blinking/solid cursor shows where RETURN acts, and C-64 users know
the rule.

### 5.6 RETURN and the REPL

- The host reads the bytes of the cells of the line (Latin-9, matching the internal character set), builds a C string and passes it to
  `repl_enter()` exactly as the line from `sdlscr_lineinput()` is passed today (`handle_input()`); nothing in the REPL changes.
- **`AUTO`**: the host prints the number `110 ` and then waits; with the screen editor the *row* contains `110 …`, so RETURN delivers the number *and*
  the text. `AUTO` already receives a number plus text from the user and ends on an empty text; I would strip a leading `n ` that equals the prompted
  number (`110 ` alone = an empty line = AUTO ends).
- **Stale lines** (`Ready.`, an old error message) can be entered like any other line; that is the C-64 behavior and gives a syntax error.
- **Where it does not apply:** `INPUT`, `LINE INPUT`, `EDIT n`, the monitor prompt, the pager's "Press ENTER" question. They keep `scr_lineedit()`.
  `INPUT` on the C-64 also reads the screen; I do not propose that (a program's prompt text would have to be removed from the answer).
- The `Ready.`/prompt printing after a line stays as it is.

### 5.7 The console host

The console host has no text screen 0: its output is the terminal's own scrolling output, and a program cannot read what a terminal shows. So the
screen editor **cannot be added to the existing console mode**. Options, in the order I would consider them:

1. **Window only for now**, console keeps today's line editor, and the roadmap says why (a documented difference, as your standing rule asks for).
2. **Console full-screen mode** (later, opt-in, e.g. `engine1999c --screen`, default off): when both ends are terminals, the REPL session runs on the
   alternate screen with **text screen 0 as the buffer**, painted by `conscr.c` exactly like the editors today; program output goes to the text screen
   (the path `con_fullscreen` already has); keys come from `con_drv`. Then the *same screen editor code* runs in both hosts. Costs: the terminal's
   own scroll-back and copy/paste of earlier output are lost while the session runs (the scroll-back ring of §5.4 compensates a little), and pipes /
   scripts stay on today's path. It is the only way to satisfy "editing is consistent in both hosts" completely.

My suggestion: implement the window version with the shared code written against the driver (`scrdrv.h`) so that option 2 is mostly wiring, and
decide on 2 after you have used the window version.

### 5.8 Sharing code with the general line editor

`scr_lineedit()` today owns a buffer (`char work[]`, `len`, `pos`) and *draws* it. The screen editor needs the same decisions (which key does
what, limits, burst handling) with a different store. The clean way is a small **edit-target interface** (`get length`, `char at`, `insert`, `delete`, `move`)
with two implementations: the private buffer (today) and a *cell run* (new). `scr_editkey()` then works on either. I would not rewrite the existing
editor for this in the first step: write the cell-run editor beside it, reuse `scr_editkey()`'s key policy through the interface only where it
fits (character, BACKSPACE, DELETE, LEFT, RIGHT, HOME, END), and unify later if it turns out to be natural.

### 5.9 Other effects and risks

| Item | Effect |
|---|---|
| `txtscr_getcell()`, `TXT` functions, `TXT SAVE/RESTORE` | flags must be masked (getters) / saved in the 4th byte (save/restore); trivial |
| Console painter (`conscr.c`) | compares cells to find changes: mask the flags (an empty→space change is not a visual change) |
| Performance | none: flag tests on keys only |
| Resizing (console) | `txtscr_resize()` creates empty cells and keeps flags of the copied part; a row that was cut at the new width loses its WRAP flag (the line is split) |
| `GFX RESET`, `CLS`, `TXT CLS`, `TXT FILL` | clear with EMPTY cells; `FILL` with a character writes non-empty cells |
| `PRINT` of spaces | non-empty (a typed or printed space is text): `PRINT "A     B"` is one line |
| `LOCATE`d output with gaps | gaps are empty cells: such a row is several lines for RETURN (§5.3) |
| Existing behavior at the prompt | changes: a typed line now stays on the screen as text that can be edited (before, the editor's text also stayed, but could not be edited after RETURN); `Ctrl-C`/ESC at the prompt no longer "cancel a line" (nothing to cancel) |
| `BUILD` executables | no prompt, no change |
| Scripts/pipes | no change (no screen editor without a terminal) |

## 6. Proposed design (if you agree)

**Cell flags (`textscreen.[ch]`).** `TXTSCR_EMPTY = 1<<24`, `TXTSCR_WRAP = 1<<25`; `txtscr_isempty()`, `txtscr_iswrapped()`;
`txtscr_blankcell()` returns `(32, blank colors) | EMPTY`; `txtscr_write()` clears both flags of the cells it writes; `txtscr_print_n()` sets WRAP on the
last cell of a row it leaves by wrapping; `txtscr_getcell()` masks the flags; a raw getter for the editor returns them. `txttest1` gets the new cases.

**Logical line (`scredit.c`, no SDL).** `scr_screenline_extent( cursorx, cursory ) → (first, last)` over the selected screen with the rules of §5.3;
`scr_screenline_text( first, last, char* out )`.

**The editor (`scredit.c`).** `scr_screenedit( drv, char* out, size_t cap, … )`: a loop like `scr_lineedit()` (same driver, same keys, same
resize handling) that edits cells in place and returns the line text on RETURN, or a cancel/break/EOF result. Window: `sdlscr_lineinput()` for the
prompt and `AUTO` chooses it when the selected text screen is 0 (a parameter, so `INPUT` etc. are unaffected).

**Tests.** `scrtest1` (fake driver, scripted keys): extent rules (gap, wrap, exactly-full row, cursor on an empty cell), RETURN text, insert/delete across
rows, scrolling at the bottom, and later the scroll-back; `txttest1` for the flags; a window check under Xvfb; the console variant gets pty tests when
option 2 is done.

**Phases.** (0) flags in `textscreen.c` + tests; (1) logical line + editor core + window integration (prompt, `AUTO`), without scroll-back;
(2) scroll-back ring and PgUp/PgDn; (3) console full-screen mode (opt-in). Rough size: phase 0 ≈ 150 lines, phase 1 ≈ 400 lines + tests, phase 2 ≈ 150,
phase 3 ≈ 200 (the pieces exist).

## 7. Decisions I need from you

1. **Empty-cell marker:** B (flag bit, font untouched — recommended), A′ (character 0, blank on screen 0 only), or A (character 0, glyph blanked everywhere)?
2. **Row links:** WRAP flag (recommended) or the Forth version's last-column rule?
3. **Typing mode:** insert by default (INSERT toggles to overwrite) or overwrite by default (C-64)?
4. **Interior gaps:** a gap of empty cells ends a line (as you described; recommended) or is turned into spaces when the line is grabbed (Forth version)?
5. **Scrolling at the top:** do nothing / lossy scroll like the Forth version / scroll-back ring (recommended as phase 2)?
6. **Console:** window only first (recommended), or plan the full-screen console mode right away?
7. **LEFT/RIGHT** may walk over empty cells and rows freely (C-64; recommended) or stay inside the current line?
8. **`INPUT`**: keep the plain line editor (recommended) — or read the screen like the C-64 does?

## 8. What I did *not* take from the Forth version

The line highlight (`b-mark-line`), the attribute keys F2–F4, ESC = `bye`, the shadow buffer and its refresh logic, and the terminal escape sequence
layer: the engine's text screen, painter and key driver already do those jobs.
