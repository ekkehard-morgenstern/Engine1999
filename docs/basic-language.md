# BASIC language and tokenized format

The BASIC dialect is classic line-numbered BASIC with extensions (labels, `UNLESS`, multi-line `IF`, `WHILE/REPEAT`
tokens, user functions with `DEF FN`, `DYNAMIC`/`ASSOC` arrays, `SAVE`, shell literals). Only a subset has grammar and code generation
so far (see "Implemented subset").

## Source preprocessing (`preprocess_buffer`)

Collapses runs of blanks/tabs to one space, drops `\r`, upper-cases letters — everywhere except inside `"…"` and `` `…` ``.
Identifiers and keywords are therefore case-insensitive; string and shell literals keep case.

## Tokenized line (`tokenize_line` / `detokenize_line`)

`tokenize_line( text, buf_t* out, linehdr_t* )` appends the line to a growable buffer; `detokenize_line( buf_t* out, tokens, hdr )` rebuilds source text. Readers use the bounded `rbuf_t` and return heap strings through `char**`.

**Syntax highlighting (optional):** `detokenize_line_hl( out, tokens, hdr, style, usrdata )` calls `style( hlclass_t, usrdata )` whenever the class of the text changes and inserts the returned string (an escape sequence, say) before the new text; `HL_NONE` at the end of the line is the place for a reset. Classes (`hlclass_t`, `bastok.h`): line number, statement, control flow, command, function, operator, identifier, number, string, shell literal, comment, punctuation; spaces keep the class of the text before them. `style == 0` (and `detokenize_line()`) gives plain text. Only `LIST` output is colored: `list_program_hl()` (`baspgm.c`) takes the callback, the REPL passes `repl_t.hlstyle` when LIST writes to channel 0 (the screen). `SAVE`, `RENUM`, label search and the debug output always use the plain text. `engine1999` sets `host_hlstyle()` (a table of ANSI SGR sequences, one per class: easy to change); the sequences reach the screen through `host_print` → `sdlscr_ansi_puts()`.

```
linehdr_t (5 bytes, all big endian)
    lineno.24                  // 0..999999; 16777215 (2^24-1) = no number (direct mode) / deleted / invalid
    length.16                  // total bytes incl. header
tokens…  TOK_EOL (0)
```

- Token IDs are `TOK_*` in `bastok.h`. **Single characters keep their ASCII value** (`TOK_PLUS` = 43 = `'+'`, digits `TOK_DEC0..9` = 48..57,
  `TOK_COLON` = 58 …), so a lone digit is one byte. Keywords occupy the control-code range 11–31 and the high range 68–144 (`TOK_PRINT` 11, `TOK_GOTO` 89, `TOK_LABEL` 143, `TOK_LEN` 144). `TOK_NONE` = 255.
- Two-char operators: `<=`, `<>`/`><`, `>=`, `**` (→ `TOK_POW`).
- Literals carry a length prefix and are tokens followed by payload:

| Token | Source form | Payload |
|---|---|---|
| `TOK_IDENT`, `TOK_NUMIDENT`, `TOK_STRIDENT`, `TOK_INTIDENT` | `A-Z0-9_` names; `$` / `%` sigil | name (sigil is implied by the token, restored on read when `ri_sigil` is set) |
| `TOK_DECLIT/HEXLIT/OCTLIT/QUALIT/BINLIT` | decimal, hex, octal, quaternary, binary number | **digits as text** (architecture-independent; conversion errors are runtime errors) |
| `TOK_STRLIT` | `"…"` | text |
| `TOK_SHLLIT` | `` `…` `` shell command | text |
| `TOK_QUOLIT` | `'…` comment to end of line | text |
| `TOK_BRKLIT` / `TOK_BRCLIT` | `[…]` / `{…}` | text |

  Length prefix: **2 bytes, big endian** (max 65535). It was 1 byte before the refactor, so older tokenized data is not compatible; see [refactoring.md](refactoring.md).

## Keyword and operator groups (see `bastok.h` for all IDs)

- Immediate/system: `LIST SAVE LOAD RUN AUTO RENUM DELETE MERGE CHAIN FILES NEW CLEAR EDIT SHOW WARRANTY COPYING`
- I/O: `PRINT INPUT PUT GET READ DATA RESTORE INKEY`
- Declarations: `DIM DEF FN INT STR FLT OPTION BASE DYNAMIC ASSOC LET ERASE LABEL SYMBOL`
- Control flow: `GOTO GOSUB GO TO SUB RETURN IF UNLESS THEN ELSE ENDIF ENDUNLESS END FOR STEP NEXT REPEAT WHILE UNTIL WEND UEND POP AFTER EVERY ON OFF`
- Functions: `ASC CHR VAL LEFT MID RIGHT BIN QUA OCT DEC HEX FRE TI LEN`
- Operators: `+ - * / \ ^ ** NOT AND OR XOR NAND NOR XNOR LSHIFT RSHIFT = <> < > <= >=`, `@` (address-of), `!`, `#` (channel), `~`, `|`.

## Grammar (`bascomp.ebnf`)

The grammar is written over tokens and is the specification for `comp_eat_*`. Summary of the expression tower (low → high precedence):
`OR/XOR/NOR/XNOR` → `AND/NAND` → comparison (numeric, or string-vs-string) → `LSHIFT/RSHIFT` → `+ -` → `* / ^`(as `TOK_POW`) → unary `- + NOT` → base
(variable ref, literal, function call, parenthesized). String expressions are concatenation (`+`) of string variables, literals and string functions.
Statements: `PRINT [#chan,] args`, `SAVE`, `LET`/implicit assignment (numeric, string, `LEFT$/MID$/RIGHT$` substring assignment), `DIM`, `ERASE`, `DEF FN`,
`FOR/NEXT`, `GOTO/GOSUB/GO TO/GO SUB`, `RETURN`, `LABEL`, single- and multi-line `IF/UNLESS`. Statements separate with `:`; a line ends with an optional comment and `TOK_EOL`.

Variable kinds: float (64-bit), integer (`%`, 16-bit), string (`$`), label. Arrays up to `MAXDIM` = 6 dimensions.

## Implemented subset (as of the last commit)

Parser has rules for everything in `bascomp.ebnf` above. Code generation (`cgen_from_*`) covers literals, variable r-/l-values, expressions, user-function and system-function
calls, `PRINT`, `SAVE`, assignment/`LET`, substring assignment, `DIM`, `ERASE`, `DEF FN` (single- and multi-line), `FOR/NEXT`, `GOTO/GOSUB/RETURN/LABEL`, `IF/UNLESS` and statement lists. `RUN`, `LIST`, `LOAD`, `NEW`, `CLEAR`, `DELETE` and `RENUM` are statements too (below). `REPEAT/UNTIL`, `WHILE/WEND`, `UNTIL/UEND`, `AGAIN [n]`, `END`, `POP` and `ON n GOTO/GOSUB` are implemented too (see compiler-and-vm.md for the loop rules), and so are `DATA`, `READ` and `RESTORE [line|label]` (data items are signed numeric literals and string literals; no unquoted text), `INPUT` and `GET` (below). Keywords that have a token but no grammar yet (`PUT`, `AFTER`, `EVERY`, `ON`/`OFF` for timers, `SYMBOL`, `RUN`, `NEW`, `LIST`, `LOAD`, …) are not statements of the language yet; `AFTER`, `EVERY` and `SYMBOL` are planned to work through host callbacks.
Some grammar items are inconsistent in the EBNF (e.g. `sys-noarg-num-call := sys-no-arg-num`, a name that is not defined) — check the code before relying on it.

### INPUT and GET

    INPUT [#ch,] ["prompt" (;|,)] var {, var}
    GET   [#ch,] var {, var}

- `INPUT` shows the prompt (a plain string literal, or several), then `? ` if the prompt is followed by a semicolon or if there is no prompt; a comma after the prompt suppresses the `? `. It reads one line and distributes its comma-separated fields to the variables (plain or array elements, numeric, integer or string). A field starting with `"` extends to the closing quote, so it may contain commas. Numeric fields may be surrounded by blanks; an empty numeric field is 0; unquoted string fields are trimmed.
- A missing field or a non-numeric number prints `?Redo from start` and asks again (prompt included); variables already stored stay stored. More fields than variables print `?Extra ignored`. End of input (host callback returns nothing) is the error "End of input".
- `GET` waits for a key for every variable: a string variable gets the one-character string, a numeric variable the key code. (`INKEY$` is the non-waiting variant.)
- Without `#ch` both use channel 0. The channel is passed to the host callbacks (`vm->input`, `vm->getkey`); the VM's output channel (`PCHN`) is set to it too, so the prompt goes to the same channel.

## Program commands: `LOAD`, `RUN`, `MERGE`, `CHAIN`, `FILES`, `LIST`, `NEW`, `CLEAR`, `DELETE`, `RENUM`, `EDIT`, `MONITOR`

They are ordinary statements (`bascomp.ebnf`, `cgenprog.c`), so they work in direct mode and inside programs.

- `LOAD name$` — replaces the stored program entirely by the one in the file (variables cleared; a name **without an extension** is looked up as it is, then with `.bas`, `.e99`, `.BAS`, `.E99`, the first existing file wins). **Typed in direct mode it only loads** (use `RUN` afterwards); inside a program it **runs the loaded program from the beginning**. The file may be a binary `SAVE` file (recognized by its magic, see `baspgm.h`) or a text listing (lines with line numbers, e.g. `SAVE "X",A`). If the file can't be read or is bad, an error is reported and the old program stays. When used inside a program, the running program ends and the loaded one starts.
- `RUN [target]` — clears all variables and runs the stored program from the beginning, from a line number (`RUN 100`) or from a label (`RUN START`). Inside a program it restarts the program (a program ending with `RUN` runs forever; Break stops it). The target must exist ("Undefined line number N", "Undefined label X").
- `MERGE name$ [, APPEND]` — enters the lines of a program file (same file name rules and formats as `LOAD`) into the stored program: a line whose number exists is **replaced**, the others are inserted in order. Variables stay. With `APPEND` (an identifier recognized after the comma, not a keyword) the file's lines are instead numbered 10 apart **behind the last stored line** (starting at the first multiple of 10 above it, 10 for an empty program), and the line numbers they refer to (`GOTO`, `GOSUB`, `ON`, `THEN`, `ELSE`, `RESTORE`, …) are adjusted; the stored program is **not** renumbered (use `RENUM` first if you want that). Nothing changes if the file can't be read, a line has no number, the new numbers would pass 999999, or a **label is defined twice** (a label of the file that the stored program defines in a line the merge does not replace: "Duplicate label X"). Like `DELETE`/`RENUM` it ends a running program (the code no longer matches).
- `CHAIN [MERGE] name$ [, target]` — hands over to another program and keeps the data. `CHAIN name$` replaces the program by the file (like `LOAD`) and **runs** it (also when typed in direct mode); `CHAIN MERGE name$` merges the file into the stored program (as `MERGE`) and runs the result: the classic overlay technique (`CHAIN MERGE "ovl",1000` followed by code at line 1000 of the overlay). The optional target (line number or label of the *new* program) is where the run starts; default the first line. **The values of the variables survive** for every variable that the new program also uses (same name and type; an array only if it has the same dimensions; a string array element-wise). Variables the new program doesn't mention are gone, labels, functions and `DATA` are those of the new program, `SFX` definitions are kept (a plain `RUN`/`LOAD` clears them). If the file can't be read nothing changes at all; if the new program does not get as far as running (compile error, empty file, unknown target) the old runtime is put back and **everything stays as it was** except the stored program, which is the new one (so the old variables, functions, labels and `DATA` are still there for debugging); an error that happens while the new program runs is an ordinary run time error and the new program's variables stay. `COMMON` lists are not implemented (everything the programs share is passed).
- `FILES [spec$]` — lists files like `ls -ltr`, **oldest first** (so the newest are next to the prompt), without mode bits and owners (use a shell literal for those: ``A$=`ls -l` ``). One line per entry: the **size in bytes** (right-aligned), the **modification time** `YYYY-MM-DD HH:MM` (local time) and the **name** followed by the `ls -F` type indicator: `/` directory, `*` executable, `@` symbolic link (with ` -> target`), `|` FIFO, `=` socket. Without a spec the current directory is listed (names starting with `.` are left out, as `ls` does); a directory lists its entries; a pattern with `*`, `?`, `[ ]` (e.g. `FILES "*.bas"`, `FILES "demos/*"`) lists the matching names with their directory part; a plain file name lists that file. Nothing matching is "File not found". On the screen the listing is **syntax colored** (directories navy, links teal, executables green, BASIC program files `.bas` / `.e99` purple, sizes maroon, times silver; the colors are the host's `hlstyle` table with the classes `HL_FILEDIR`, `HL_FILELINK`, `HL_FILEEXEC`, `HL_FILEPROG`) and **paged**: after a screenful (screen height minus one line) the host asks "-- more: any key continues, Q stops --" (ESC breaks). Output to other channels is plain and not paged. The run goes on after `FILES`.
- `LIST [a][-[b]]` — lists the program to the output channel (always channel 0 for now): `LIST` all, `LIST 20` that line only, `LIST 20-50` a range, `LIST 20-` from line 20 to the end, `LIST -50` from the start up to line 50. Each of `a`/`b` is a line number or a label (a label stands for the line holding its `LABEL` statement); a label that doesn't exist is an error.
- `NEW` — deletes the program **and** all variables (the runtime is reset, so user functions are gone, too) and returns to the host (the READY prompt). Inside a program it ends the run: nothing after it is executed.
- `CLEAR` — empties all variables and goes on: numbers become 0, strings `""`, arrays are emptied (they keep their dimensions: arrays exist from parse time, so there is no un-dimensioning). The program, labels and user functions are not touched, and neither is the DATA pointer. Usable in programs; mind that the control variable of a running `FOR` is cleared too.
- `DELETE a[-[b]]` — deletes the lines of the range; the syntax is that of `LIST` (`DELETE 20` that line only, `DELETE 20-50`, `DELETE 20-`, `DELETE -50`, labels allowed), but **at least one end is required**: a bare `DELETE` is an error ("Line number or label expected"), use `NEW` to delete everything. A range without lines is not an error.
- `RENUM [a][-[b]][,step]` — renumbers lines. Without parameters the whole program gets the numbers 10, 20, 30, …. The optional range (same notation as `LIST`) selects the lines to renumber, `step` (a numeric expression, ≥ 1, default 10) is the distance. **The new numbers start at the first line of the range** (`a`, or the line a label is in); without a start they begin at 10. For `RENUM`, a single target `a` means "from `a` to the end" (not "that line only" as in `LIST`). Examples: `RENUM` (10, 20, …), `RENUM ,5` (10, 15, …), `RENUM 100` (from line 100 to the end: 100, 110, …), `RENUM 100-200,5`, `RENUM -50` (the lines up to 50, starting at 10). The lines outside the range keep their numbers; if the new numbers would collide with them or pass them ("Renumbering would overlap other lines") or exceed 999999 ("Line number too large"), nothing is changed. References to renumbered lines are adjusted everywhere in the program: `GOTO`, `GOSUB`, `GO TO`, `GO SUB`, `ON .. GOTO/GOSUB` lists, `THEN`, `ELSE`, `RESTORE`, `RUN`, `LIST` and `DELETE` targets; references to lines that don't exist and labels stay as they are; text in strings, backtick literals and `'` comments is never touched. (The program text is changed through the listing, so only lines that actually contain a changed reference are tokenized again.)
- `EDIT target` — a simple line editor for one stored line, given by a line number (`EDIT 100`) or a label (`EDIT START`). The text of the line, with its number, appears at the cursor and can be changed: printable characters are inserted, LEFT / RIGHT / HOME / END move, BACKSPACE / DELETE delete. **RETURN** accepts the text and enters it like a typed line (a numbered line replaces the stored one — under another number it is added as a copy, a number alone deletes the line —, a line without a number is run); **ESC** leaves the editor without any change. The line may wrap over several screen rows but must fit the screen (else "Line too long to edit"). Undefined line / label: the usual errors. **`EDIT` without a target** opens the full-screen editor (`fulledit.c`, text screen 1, see `docs/display-engine.md`): line number pane, syntax highlighting, scroll bar, key help; ESC returns to the BASIC screen. The REPL only sets `repl_t.fulledit`; the host opens the editor after the line has been entered. Inside a program `EDIT` ends the run, like `NEW`, and then lets the user edit. The editor is a host callback (`repl_t.edit`); a host without one reports "No line editor".
- `MONITOR` — opens the **machine monitor** (`monitor.c`, text screen 2; see `docs/display-engine.md`): memory dumps, disassembly and assembly in the instruction set of the VM, byte editing, running the (patched) code. `Q` returns to the BASIC screen. Like `NEW` and `EDIT` it ends a running program; the host opens the monitor after the line has been entered (`repl_t.monitor`).
- `DELETE`, `RENUM` and `MERGE` change the stored program, so a program that executes one **ends** right there (the code being run no longer matches the program); `RUN` starts the changed program.

`RUN`, `LOAD` and `CHAIN` need a restart of the whole compiled program, so the VM doesn't do them itself (nor `NEW` and `EDIT`): it stops and leaves a request for the host (`docs/compiler-and-vm.md`).

`WAIT n` — sleeps for n/60 seconds (the unit of `TI`; n ≥ 0, at most 216000) without using the processor: use it instead of `T=TI+n:WHILE TI<T:WEND`, which keeps the processor 100 % busy. ESC breaks a wait ("Break"). `WAIT 0` returns at once. Instruction `WAIT 0x144`, `NT_WAITSTMT 0x66`, token `WAIT` 160.

**File extensions:** `SAVE "name"` writes the binary (tokenized) form and appends `.e99` to a name without extension; `SAVE "name",A` writes the ASCII listing and appends `.bas`. A name with an extension is used as it is. `LOAD` recognizes both forms by the header, whatever the extension is.

## Sound: `SOUND`, `PLAY`, `WAVE`, `TEMPO`, `PLAYING()`

Statements of the language (`bascomp.ebnf` `sound-stmt`, `cgenprog.c`); everything is queued and returns at once. Channels are 1..8.
`SOUND ch, freq, beats [, vol]` (a tone of 20..20000 Hz, 0 = a rest; beats 0 = until the next sound on the channel; volume 0..1, default 0.5),
`SOUND OFF [ch]`, `SOUND SYNC ch [, ch ...]`, `PLAY a$ [, b$ ...]` (a music string per voice, all start together; the music string language:
notes `A`-`G`, `#`/`+`/`-`, lengths and dots, `R`, `O < >`, `L`, `T`, `V`, `W`, `ML MN MS`), `WAVE ch, w` (0 sine, 1 square, 2 triangle, 3 noise),
`TEMPO bpm` (the unit of the beats of `SOUND`, default 60) and the function `PLAYING( ch )` (-1 while the channel sounds or has music queued, else 0).
Sound effects: `PITCH ch, f` and `VOLUME ch, v` (at once), a glide as the fifth argument of `SOUND`, `ENVELOPE ch, a, d, s, r`, `VIBRATO ch, rate, depth`,
`TREMOLO ch, rate, depth`, and stored effects `SFX n, a$` / `SFX n [, ch]` (music strings with the extra commands `H S X ~ ^`).
Full description, examples, the instruction/host interface: `docs/sound.md`. The words `SOUND PLAY WAVE TEMPO PLAYING SYNC PITCH VOLUME
ENVELOPE VIBRATO TREMOLO SFX` are keywords now (they can't be variable names).
