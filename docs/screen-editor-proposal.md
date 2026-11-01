# A C-64 style screen editor for text screen 0 — requirements, design and open points

*First written 2026-11-01 for the owner's review; **revised the same day with the owner's decisions** (they are marked **decided**).
**Nothing in this document is implemented.** The branch for the subproject is `feature/c64-like-screen-editor`.*

## 1. Status: what is decided

| Topic | Decision (owner, 2026-11-01) |
|---|---|
| Empty-cell marker | **Cell flags** in the unused top byte of the text cell (not the NUL character, no font change) |
| Row links | A **WRAP flag** on the last cell of a row that continues in the next row |
| Typing mode | The screen editor (and only it) starts in **overwrite (OVR)**; **F12** toggles INS/OVR (shown in the status line; **Shift+INSERT** works too); INSERT, DELETE, BACKSPACE as in §5.3 |
| Cursor movement | **Free**, like the C-64: over empty cells and across rows, scrolling at the screen edges |
| Backspace at a line start | Never deletes beyond the logical line: the cursor just moves left and **abandons** the line |
| Cursor left at the top left | The screen scrolls **down**, the cursor goes to the end of the first screen row |
| Scroll-back | **None**: what leaves the screen is lost |
| Status line | The bottom row of the screen becomes a **status line, white on red** (not the default text colors), showing the mode with the key to switch it (`OVR  F12=INS`), and **row, column and BASIC line number (if known)** — **only while no program runs and the system waits for a program line** (the screen editor's state; **not** during a program, **not** in `AUTO` / `EDIT n` / `INPUT`). The editor works in the rows above it; the row under the status line is **cleared** when the state is entered. Programs always have the full screen (25 rows in the window) |
| Highlight | The **screen lines (rows) in which the logical line sits** are shown with a **brighter background** (whole rows, not just the line's cells; 30 % toward white to start with, adjustable later); screen editor only, never in `EDIT n` / `AUTO` / `INPUT` |
| Where it is active | **Only when no program runs and the system waits for a program line.** `EDIT n`, `AUTO` and `INPUT` are promptless line inputs (as on the C-64), with the line editor that exists today |
| Console host | The BASIC session runs on the terminal's **alternate screen** (text screen 0) whenever both ends are terminals — interactive mode, run mode, `BUILD` console executables; switches `--screen` / `--line`, `E1999_SCREEN`, `--engine1999-screen|line`; **exit dump on by default**; **an error waits for a key** (§5.9) |

Nothing is open any more (§7 lists what to look at while implementing).

## 2. The idea, as I understand it

A **screen editor** like the Commodore 64's: the text on the screen *is* the buffer. The user moves the cursor anywhere, edits what is there,
and presses RETURN on a line to hand that line to the REPL — wherever the line was typed or printed (a `LIST` line, an old command).
It exists on **text screen 0** only, and only while the system **waits for a program line** (the prompt). It is a special case of the general line
editor (`scr_lineedit()` / `scredit.c`): the same keys and behavior, with the text in the cells instead of a private buffer. A **logical line** is
the run of non-empty cells around the cursor; empty cells are the default content of the screen (`CLS` fills with them).

## 3. What the Forth version does (`../gforthstuff/forthbasic`)

Read: `screenupd.fs`, `keyboard.fs`, `execution.fs`.

| Mechanism | How it works there |
|---|---|
| Screen buffer | One cell per position (character low byte, attribute above); value 0 = empty. A shadow of the terminal kept in step by hand. |
| Logical line | Rows are linked **by the last column** (non-zero last cell = the row continues); the end is the last non-zero cell of the final row; the beginning is column 0 of the first row. |
| Grabbing | `b-grab-line` finds the extent and turns the zero cells inside it into spaces. |
| RETURN | Copies the cells of the whole line into a buffer and tokenizes it; the cursor goes down one row. |
| Cursor up/down | One row; at the edge the buffer scrolls (scrolling down drops the bottom row). |
| Typing | Overwrite only; BACKSPACE moves left and writes a space; Delete, Insert, Home, End, Page keys are stubs. |
| Marking | The current line is highlighted by changing the cell attributes and repainting. |

Not taken over: the shadow-buffer bookkeeping and the escape sequence layer (the engine's text screen, painter and key driver do that), the F2–F4
attribute keys, ESC = `bye`, the attribute-changing highlight (see §5.6 for how the highlight is done here).

## 4. How it fits the engine as it is today

- **Cells.** `textcell_t` (`textscreen.h`) is 32 bits: character in bits 0–7, pen 8–15, paper 16–23, **bits 24–31 unused**. The `TXTSCR_CELL_*` macros mask only
  their own bytes; the host's `TXTCHAR()`/`TXTFGAT()`/`TXTBGAT()` mask too.
- **Blank cells.** Today the blank is a *space* in the blank colors (`txtscr_blankcell()` in `textscreen.c`, used by `txtscr_clear()`,
  `txtscr_scrollup()`, `txtscr_scrolldown()`, `txtscr_resize()` and the initial fill) — all in one file.
- **Wrapping.** Text that runs over the right edge continues in the next row (`txtscr_print_n()`, `if ( cx >= cur->vw ) { cx = 0; ++cy; … }`): the single
  place where a "this row continues" fact is created.
- **PRINT** pads comma zones with real spaces (`vm.c`); `LOCATE` + `PRINT`, `TXT` statements and `CLS` can leave empty cells.
- **Viewports.** A text screen has a viewport (`txtscr_setviewport()`); coordinates, the cursor, scrolling, `CLS` and the `TXT` statements work *inside* it. The editors
  and the monitor already use viewports for their bars; the screen editor uses one (rows − 1) while it waits for a line, so the cursor and scrolling stay above the status line. Note that setting a
  viewport moves the cursor to its corner: the editor must put it back.
- **Line input today.** The prompt calls `sdlscr_lineinput()` → `scr_lineedit()`: the line lives in a private buffer, RETURN returns the buffer. The editor never reads the screen.
- **Console host.** Text screens exist there for `EDIT`/`MONITOR` (alternate screen, `conscr.c`); the BASIC session itself uses the terminal's scrolling output.

## 5. Design

### 5.1 Cell flags (decided: option B)

Two flags in the top byte of the cell (`textscreen.h`):

- `TXTSCR_EMPTY` (bit 24): the cell was never written since the screen was cleared / the row was scrolled in. An empty cell still looks like a space (character 32 in the blank colors),
  so nothing changes visually and `TXTCHAR()` still answers 32.
- `TXTSCR_WRAP` (bit 25): on the **last cell of a row**: the text continues in the first cell of the next row.

Rules (all in `textscreen.c`): `txtscr_blankcell()` returns `(32, blank colors) | EMPTY` (clear, scroll, resize, initial fill); `txtscr_write()` (and so print, `TXT`
statements, typing) clears both flags of the cells it writes; `txtscr_print_n()` sets WRAP on the last cell of a row it leaves by wrapping; `txtscr_getcell()` and the painters
mask the flags; `txtscr_save()`/`restore()` carry them in the unused 4th byte. `txtscr_isempty( cell )` / `txtscr_iswrapped( cell )` are what the editor uses, so the marker could
still be changed later without touching the editor. `CHR$(0)` stays a normal, visible cell (the control glyph is untouched).

### 5.2 The logical line

**Extent.** Cells are numbered row by row. A **row link** exists between the last cell of a row and the first cell of the next row only if the last cell has WRAP. The line
containing a cursor position is:

1. its **anchor cell**: the cursor cell if it is non-empty, else the cell directly left of it if that is non-empty (the cursor stands *behind* the text), else none (no line);
2. the maximal run of **non-empty** cells around the anchor, where the run crosses a row boundary only over a row link; an empty cell ends a run (never part of a line).

RETURN takes the **whole** run (cells left and right of the cursor), trailing spaces included (the tokenizer ignores them); the cursor then goes to the start of the row below the
line's last row (the screen scrolls at the bottom) and the line's last cell **loses its WRAP flag** (so the next line typed there is not linked to it).

**Gaps — and the owner's idea of marking lines (pondered).** The idea: mark every cell of a program line (output by `LIST`, `EDIT`, or typed) with a LINE bit, so no gaps
are needed. What it would change:

| | Gap rule (empty cell ends a line) | LINE-bit rule (a marked run is a line) |
|---|---|---|
| Which text can be entered | anything non-empty (as on the C-64) | only marked text: program lines and what was typed; `Ready.`, errors, `PRINT` output are not lines |
| Needs host cooperation | no | yes: `LIST`/`EDIT` output must be written in "mark mode" (a `txtscr` flag set by the host around the listing callback) |
| Does it remove the need for an "unwritten" notion? | — | **no**: the cursor moves freely over unwritten cells, so something must still say where a typed line ends; that is exactly what an unmarked/empty cell does |
| Interior gaps (from `LOCATE` output) | split a row into several lines | the gap is simply unmarked: same result for output; a *typed* gap (cursor skipped some cells) splits as well, unless the editor marks the skipped cells, which would turn invisible empty cells into line content |
| Safety | any old text can be executed by RETURN (C-64 behavior: a stale `Ready.` gives a syntax error) | stale output cannot be executed |

My conclusion: **the gap rule stays**. Marking does not remove the gaps (the free cursor re-creates them every time the user moves over empty cells), it needs
host cooperation for `LIST`/`EDIT`/`FILES`, and it gives up the C-64 trick of re-entering *any* text. The one real benefit — stale output cannot be executed — can be had later
as an extra check in the host (ignore a line that is exactly `Ready.` or starts with `? `), without any cell bit. If you still want the LINE bit, it is a third flag (bit 26) and
the extent rule of §5.2 would add "and marked" to step 2.

### 5.3 Keys (the screen editor only)

The mode is **OVR** at start and stays what the user chose until it is toggled (shown in the status line while that is visible). The **edit unit** of INSERT, DELETE and BACKSPACE is defined below the table.

| Key | Behavior |
|---|---|
| printable character | **OVR:** writes the cell under the cursor (colors: the current pen/paper) and moves right. **INS:** inserts the character at the cursor, shifting the rest of the edit unit right (in a logical line a new row is added if it reaches the end of a row, and at the bottom of the screen the screen scrolls up; in a screen-row unit what is pushed past the last column is lost). Writing in the last column sets WRAP on that cell and the cursor continues in the next row (the bottom right scrolls the screen up). |
| **INSERT** | inserts an empty character (a space) at the cursor and shifts the rest of the edit unit right; the cursor stays. Same in OVR and INS. |
| **F12** (also **Shift+INSERT**) | toggles INS / OVR. F12 is the key named in the status line, so nobody has to look for it; Shift+INSERT stays as a second binding where the keyboard has it and the terminal passes it on. |
| **DELETE** | deletes the character at the cursor and shifts the rest of the edit unit left; the cell freed at its end becomes empty (a last row that becomes empty drops the previous row's WRAP flag). The cursor stays. |
| **BACKSPACE** | if the cell left of the cursor belongs to the edit unit: deletes it, moves the cursor there, shifts the rest of the unit left. **At the first cell of the unit it deletes nothing: the cursor just moves left, abandoning it** (from column 0 to the end of the row above). |
| LEFT / RIGHT | **free** movement: one cell, wrapping from the first column to the end of the row above and from the last column to the start of the row below; at the top left the screen scrolls **down** and the cursor goes to the **end of the first row**; at the bottom right the screen scrolls **up** and the cursor goes to the start of the last row |
| UP / DOWN | one row, same column; at the top / bottom row the screen scrolls down / up (the row that leaves is lost; no scroll-back) |
| HOME / END | start / end of the logical line (no line: the start / end of the screen row). Ctrl+Home / Ctrl+End: top left / bottom left of the editor area, i.e. the rows above the status line (as in the full-screen editor). |
| RETURN | enters the line (§5.2, §5.5); with no line it just moves to the start of the next row |
| ESC | nothing at the prompt (it is the break key while a program runs) |
| Ctrl+C, TAB, the other function keys, PgUp/PgDn | ignored (PgUp/PgDn have nothing to scroll back to) |

**The edit unit of INSERT, DELETE and BACKSPACE.** (1) If the cursor is in or directly behind a **logical line** (§5.2), the unit is that line, across its wrapped rows. (2) Otherwise — the cursor is on an
empty cell that is not behind text — but the **screen row** under the cursor is **not blank** (it has a non-empty cell anywhere), the unit is **the rest of that screen row**, from the cursor to the last column: the
keys work on it exactly as on a line, cells that are pushed past the last column are lost, and DELETE/BACKSPACE pull the remainder left (empty cells at the end). This is what ASCII graphics need: art is built
from runs separated by gaps, and INSERT/DELETE/BACKSPACE must work anywhere in such a row. (3) If the screen row is blank, nothing happens (BACKSPACE at column 0 only moves the cursor left).
Shifting stays inside the unit: cells beyond the screen's last cell are lost. In case (1) a run that is shifted right grows into the empty cell after it (it may then touch the next run, as it would on the C-64).

### 5.4 Scrolling

At the bottom edge the existing `txtscr_scrollup()` is used (the top row is lost, as when output scrolls). At the **top** edge `txtscr_scrolldown()` brings an empty row in and **the
bottom row of the editor area is lost**. **There is no scroll-back** (decided): information that goes off the screen is gone. (While the screen editor waits for a line the scrolling happens in the **editor area**: the rows above the status line; the status line is not part of it.)

### 5.5 RETURN, the REPL, and the prompt-less inputs

- **Active only when** no program runs and the system waits for a program line (the main loop's prompt). The host reads the bytes of the line's cells (Latin-9) into a string and
  passes it to `repl_enter()` exactly as the string from `sdlscr_lineinput()` is passed today (`handle_input()`); the REPL does not change. `Ready.` and the other messages are
  printed as before.
- **`INPUT`, `LINE INPUT`, `AUTO`, `EDIT n`** (while a program asks, or when the host asks for a line to edit) are **promptless line inputs, as on the C-64**: the line editor that exists
  today (`scr_lineedit()`), which cannot leave its line, with **no** highlight and no screen reading.
  - `AUTO`: the host prints the suggested line number as the prompt, the user types the text, and the host **adds the suggested number to the text** before passing it to the REPL
    (what the main loop does today); an empty text ends `AUTO`.
  - `EDIT n`: the editor starts with the line's text (with its number) and **the text the user entered is passed to the REPL** (as today).
  - Break keys, ESC (cancel), Ctrl-D (console EOF) behave as they do in these editors today.
- The pager's "Press ENTER" question also keeps the line editor.

### 5.6 The status line and the highlight

Both are **render-time overlays** of the text screen, shown **only while the screen editor waits for a program line** (no program running; not in `AUTO` / `EDIT n` / `INPUT`, which are
promptless line inputs). They are not stored in the cells, so nothing has to be saved or restored when they appear or disappear, programs never see them, and `TXT` functions, `SAVE`/`RESTORE`
and the exit dump are not affected.

**Status line.** An overlay on the **bottom row** of text screen 0: `txtscr_setstatus( text, paper, pen )` makes both renderers (`txtscr_render()` in the window, `conscr_paint()` on the terminal) draw
that row from the overlay instead of from the cells. It is **white on red** — different from the default text colors; if the user's own colors are white on red, the fallback is yellow on dark red
(a pair that differs from the current text colors is always chosen). Content (left to right, cut at the screen width, the position part kept when narrow):

```
OVR  F12=INS     Row 12  Col 7  Line 120
```

- the **mode**, `INS` or `OVR`, and the **key that switches it** (`F12=INS` / `F12=OVR` naming the mode it would switch to), so the user does not have to look through the keys;
- the cursor's **row** and **column** (1-based, in the editor area), and the **BASIC line number** if known: the digits at the start of the logical line under the cursor (`Line 120`); `Line -` when
  the cursor is in no line or the line has no number (`PRINT 5`, `Ready.`).

While it is shown, the editor sets a **viewport of rows − 1** (24 rows in the window) so the cursor and scrolling stay above it, and restores the full viewport (and the cursor) when it leaves the state.
**Programs always have the whole screen** (25 rows in the window, the terminal's rows in the console's screen mode): the status line exists only while the user is expected to type a program line.

**Entering the state** (after "Ready.", after a program ended, at start-up): (1) if the cursor is on the last row, the **whole screen scrolls up one row** so the cursor stands on the row above it; (2) the **bottom row is
cleared** — its cells become empty, so text a program left there is discarded (decided: no stale text reappears when the state ends); (3) overlays on, viewport rows − 1, cursor restored.
**Leaving it** (RETURN): overlays off, full viewport, the cursor at the start of the row below the line, the REPL runs the line (a command may print on the bottom row; a program owns the screen).

**Highlight.** The **screen rows in which the logical line sits** — from the line's first row to its last row, whole rows from the first to the last column, not just the line's own cells — are drawn with a
**brighter background**: the text screen keeps a *highlight row range* (first and last row; none = off) and the renderers draw every cell of those rows with the **brighter variant of its paper** (the palette entry nearest to the
paper color blended 30 % toward white — a first try, to be adjusted — as a table computed once from `palette.c`, or from the layer's current palette in the window). When the cursor is in no logical line but its screen row is
not blank (the edit unit of case 2 in §5.3), that **screen row** is highlighted, so the user sees what INSERT / DELETE / BACKSPACE act on; on a blank row nothing is highlighted. The range is updated after every key
and cleared with the state (RETURN, a program starts, an `AUTO` / `EDIT n` / `INPUT` line editor takes over).

### 5.7 Sharing code with the general line editor

`scr_lineedit()` owns a buffer (`work`, `len`, `pos`) and draws it; the screen editor needs the same decisions (which key does what) with the cells as the store. I would write the screen
editor beside it (`scr_screenedit()` in `scredit.c`, no SDL, same driver `scrdrv.h`, same resize handling) and share the key mapping through a small edit-target interface only where it fits (character,
BACKSPACE, DELETE, LEFT, RIGHT, HOME, END); a later unification is possible but not needed. The logical line (§5.2) is pure functions over the selected screen
(`scr_screenline_extent()`, `scr_screenline_text()`), easy to test on the fake driver of `scrtest1`.

### 5.8 Key detail for terminals

**F12** is `ESC [ 2 4 ~` in the canonical key sequences (`SDLK_F12` in the window), generated and parsed by the engine already, and terminals pass it on. A few drop-down terminals (Yakuake, Guake, Tilda) use F12 to
show and hide themselves; there Shift+INSERT is the alternative — which many other terminals take for *paste* — so the status line names F12 and the manual names both.

### 5.9 The console host: the BASIC screen on the alternate screen (decided)

In interactive mode, in run mode (`engine1999c prog.bas`) and in console executables made with `BUILD`, the BASIC session runs on the terminal's **alternate screen with text screen 0 as the
buffer**, painted by `conscr.c` as the editors are today — whenever standard input *and* output are terminals and `TERM` is not `dumb`. A program therefore behaves the same however it was
started, the screen editor is one implementation for both hosts, and the user gets a fresh screen.

**Why it is good beyond the editor.** `COLOR`, `LOCATE`, `CLS`, the `TXT` statements and the text screen functions can work for real in the console (today `COLOR` is ignored and `TXT` is "not available");
colored `LIST`/`FILES` use the Engine's palette; the output path exists already (`con_fullscreen`, `conscr_write()`); resizing is handled (`txtscr_resize()`, SIGWINCH).

**Costs and handling**

| Consequence | Handling |
|---|---|
| No scroll-back, and the session text is gone when the alternate screen is left | the **exit dump** (below); the **classic line mode** stays as an option |
| Copy and paste of earlier output | terminals still select text on the alternate screen |
| Scripts, pipes, redirections | unchanged: screen mode only when both ends are terminals |
| A crash or kill must not leave the terminal on the alternate screen, raw, cursor hidden | handlers (SIGTERM, SIGHUP, …) plus `atexit` restore the terminal |
| Painting speed in a print loop | paint when the program waits for a key, after a newline at most every ~10 ms, and at the end of a statement; never per character |
| Unsuitable terminals (`TERM=dumb`, no alternate screen) | classic line mode automatically |

**Switches (agreed).**

| Where | Force the screen mode | Force the classic line mode |
|---|---|---|
| `engine1999c` | `--screen` | `--line` |
| every console program, `BUILD` executables included | environment `E1999_SCREEN=1` | environment `E1999_SCREEN=0` |
| `BUILD` executables, command line | `--engine1999-screen` | `--engine1999-line` |

A built executable hands every command-line word to the program (`ARG$(n)`) except `--engine1999-about`; the new switches are reserved in the same way, with the same prefix. `--no-dump` /
`E1999_DUMP=0` (`--engine1999-no-dump` for built executables) switch the exit dump off.

**Programs started as commands.** (1) **Exit dump (on by default):** when a screen mode session ends, the text screen is written to the normal terminal screen as plain lines (trailing empty
cells cut, optionally with SGR colors), so the output stays in the shell's scroll-back; the overlays (status line, highlight) are not part of the cells and so are not dumped. (2) **An error waits for a key** before the screen is left, and the dump includes
the message. (3) A normal end leaves at once, after queued sound has finished.

**Effect on the console input code.** `con_getline()` (the line editor with the cursor-position model `conscr_line_*`) is then needed only for the classic line mode; in screen mode the prompt uses the screen
editor and `INPUT` / `LINE INPUT` / `EDIT n` / `AUTO` use `scr_lineedit()` on the text screen itself (no `ESC [ 6 n` query). The console input code gets simpler.

### 5.10 Other effects and risks

| Item | Effect |
|---|---|
| `txtscr_getcell()`, `TXT` functions, `TXT SAVE/RESTORE` | flags masked (getters) / saved in the 4th byte (save/restore) |
| Console painter (`conscr.c`) | compare cells without the flags (empty → space is not a visual change); draw the highlight overlay |
| Viewport of 24 rows | only the editor's own, only while it waits for a line: programs, `TXT` functions and `LOCATE` see the whole screen (25 rows in the window); `GFX`/`TILE`/`SPRITE` layers are not affected |
| Resizing (console) | `txtscr_resize()` creates empty cells and keeps the flags of the copied part; a row cut at the new width loses its WRAP flag; the overlays are set again by the editor after the resize (new last row, new cell indices) |
| `CLS`, `TXT CLS`, `TXT FILL`, `GFX RESET` | clear with empty cells; `FILL` with a character writes non-empty cells (the overlays are off while a program or command runs anyway) |
| `PRINT` of spaces | non-empty (text): `PRINT "A     B"` is one line |
| `LOCATE`d output with gaps | gaps are empty cells: such a row is several lines for RETURN |
| Existing behavior at the prompt | a typed line stays on the screen as text that can be edited; ESC and Ctrl-C at the prompt have nothing to cancel |
| Scripts / pipes / `--line` | no screen editor (no screen) |

## 6. Phases and tests

1. **Cell flags** (`textscreen.[ch]`, §5.1) + `txttest1` cases; the **overlays** (status line, highlighted row range) in both renderers.
2. **Logical line and the screen editor** (`scredit.c`: extent, text, the keys of §5.3, INS/OVR); window integration (main loop prompt; `AUTO`, `EDIT n`, `INPUT` stay on `scr_lineedit()`). `scrtest1` on the fake
   driver: extent rules (gap, wrap, exactly-full row, cursor on an empty or the first/last cell), RETURN text and WRAP clearing, INS/OVR typing, INSERT/DELETE/BACKSPACE across rows and in a row with gaps (edit unit case 2), the status line text (row, column, line number), the highlighted rows, abandoning a line by BACKSPACE,
   scrolling at all four corners, no scroll-back, entering/leaving the state (viewport, cursor, overlays), `AUTO` / `EDIT n` / `INPUT` without overlays; a window check under Xvfb.
3. **Console screen mode** (§5.9): BASIC on the alternate screen, painting, the switches, the exit dump, error wait, terminal restore on signals; pty tests for both modes.

Rough size: phase 1 ≈ 250 lines, phase 2 ≈ 500 lines + tests, phase 3 ≈ 350 lines (the pieces exist: `con_fullscreen`, `conscr.c`, `con_drv`; it also brings `COLOR`/`LOCATE`/`TXT` to the console).

## 7. Open points

**None.** To look at while implementing, because they are matters of taste or of the screen itself: the exact status line layout and colors (white on red is the start), the highlight brightness (30 % toward white), how INSERT/DELETE
behave in a row with several runs (case 1 shifts a run into the gap after it), and whether F12 collides with the user's desktop.
