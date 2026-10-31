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

## Binary program files (`.e99`)

`SAVE "name"` (without the `A` option) writes the program in its stored, tokenized form; `LOAD`, `MERGE` and `CHAIN` read it back (`save_program()` in `baspgm.c`, `read_program_header()` there and `repl_read_program()` in `basrepl.c`). The file is a 5-byte header followed by the lines:

```
header (5 bytes)
    89 45 39 39                // magic: 0x89, then "E99"; the first byte is not ASCII, so a text listing can't be mistaken for it
    01                         // format version (PGMFILE_VERSION); any other version is "Unsupported program file version"
line, repeated until the end of the file (no line count, no trailer)
    lineno.24                  // big endian, 0..999999
    length.16                  // big endian, total size of this line including these 5 bytes
    tokens…  00 (TOK_EOL)      // exactly the tokenized line of the program store, see above
```

Example, `10 PRINT "HI";A$` and `20 GOTO 10` (the space after a line number is a token, too):

```
89 45 39 39 01                        header, version 1
00 00 0A  00 13                       line 10, 19 bytes in total
   20  0B  20                         space, PRINT, space
   03 00 02 48 49                     TOK_STRLIT, length 2, "HI"
   3B                                 ;
   8B 00 01 41                        TOK_STRIDENT, length 1, "A" (the $ is implied by the token)
   00                                 TOK_EOL
00 00 14  00 0E                       line 20, 14 bytes
   20  59  20                         space, GOTO, space
   0A 00 02 31 30                     TOK_DECLIT, length 2, "10"
   00                                 TOK_EOL
```

- Only the source is saved: no variables, no compiled code, no checksum. `RUN` compiles the program again, so the file does not depend on the instruction set or on variable numbers.
- The loader reads into a scratch program and replaces (or, for `MERGE`, merges into) the stored one only when the whole file is fine. A line with a length of 5 or less, a line number above 999999, a line that is cut off or doesn't end in `00`, and bytes left over at the end are "Bad program file".
- Compatibility: the tokens are only valid for the token set of the same build (`TOK_*` in `bastok.h`; the literal length prefix is 2 bytes since the tokenizer refactor, see [refactoring.md](refactoring.md)), which is what the version byte is for: a change of the token set or the line layout must bump `PGMFILE_VERSION`.
- The ASCII form (`SAVE "name",A`, `.bas`) is the listing text as `LIST` prints it, one line per program line; `LOAD` tells the two apart by the magic number.

## Keyword and operator groups (see `bastok.h` for all IDs)

- Immediate/system: `LIST SAVE LOAD RUN AUTO RENUM DELETE MERGE CHAIN FILES NEW CLEAR EDIT SHOW WARRANTY COPYING`
- I/O: `PRINT INPUT PUT GET READ DATA RESTORE INKEY`
- Declarations: `DIM DEF FN INT STR FLT OPTION BASE DYNAMIC ASSOC LET ERASE LABEL SYMBOL`
- Control flow: `GOTO GOSUB GO TO SUB RETURN IF UNLESS THEN ELSE ENDIF ENDUNLESS END FOR STEP NEXT REPEAT WHILE UNTIL WEND UEND POP EXIT LEAVE ITERATE AFTER EVERY ON OFF`
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

**Timers: `AFTER` and `EVERY`** (modelled on Locomotive BASIC; the unit is a tick of 1/60 s, like `TI` and `WAIT`):

```
AFTER delay [, timer] GOSUB target     run the handler once, delay ticks from now
EVERY delay [, timer] GOSUB target     run it again and again, every delay ticks
AFTER delay [, timer] GOTO target      the same, but jump to the target (no return)
AFTER OFF [timer]                      clear all AFTER timers (or only that one)
EVERY OFF [timer]                      clear all EVERY timers (or only that one)
```

- There are **four timers, 0 to 3** (default 0). A timer holds one `AFTER` or one `EVERY`; setting a timer again **replaces** what it held. `delay` is a number of ticks greater than 0 (fractions are fine); the target is a line number or a label, like `GOSUB`/`GOTO`. `OFF` removes the timers of its own kind only: `AFTER OFF` leaves the `EVERY` timers alone and the other way round; with a number it clears just that timer (and only if it is of that kind).
- A handler entered by `GOSUB` ends with `RETURN` and the program goes on where it was interrupted (the interrupted line number is restored). The timer **numbers are priorities**: while a handler runs, only a timer with a *higher* number can interrupt it (a nested handler); due timers with the same or a lower number wait until the handler has returned. A handler entered by `GOTO` just jumps (`POP`, `END` and so on work as usual); it keeps nothing running, so timers are not held back.
- **When a handler runs:** between two statements, and during `WAIT` (which gives way to the handler and then sleeps for the rest of its time). Not while a statement is under way inside an expression (a `DEF FN` call), and not during `INPUT` or `GET` (they wait for the keyboard; use `INKEY$` loops for timed input). A timer that comes due meanwhile runs right after.
- An `EVERY` timer keeps its rhythm (the next tick is one interval after the previous *due* time); if the program was too busy to run it in time, the missed ticks are dropped instead of being run in a burst.
- **Timers end with the run** that set them: when the program (or the direct mode line) ends, is stopped by Break or an error, or a new run starts, all timers are gone. A program that ends while a timer is pending never runs the handler. (So `AFTER` typed alone at the prompt does nothing; put it in a line with the statements that should be interrupted, or in the program.)
- `AFTER 0`, a negative delay, or a timer number other than 0..3 is "Illegal argument".
- **`REMAIN(timer)`** returns the ticks (1/60 s, rounded up) until timer 0..3 is due, **0 if the timer is not set** (or has already fired), and **clears the timer** — as in Locomotive BASIC, where it is the way to cancel one timer and see how much time was left. It works for `AFTER` and `EVERY` timers alike (an `EVERY` timer is cleared, too). `REMAIN(4)` and `REMAIN(-1)` are "Illegal argument". `REMAIN` is a reserved word now.
- A **direct mode line** may use line numbers as targets (`GOTO 100`, `GOSUB 100`, `AFTER 60 GOSUB 100`, `RESTORE 20`): they refer to the program as it was when it was last compiled (by `RUN`, `CHAIN`, ...). After the stored program was changed (a line entered or deleted, `DELETE`, `RENUM`, `MERGE`, `LOAD`) the old line table no longer fits, and a line number in a direct line is "Undefined line number" until the next `RUN`; labels always work.

## Files and devices: `OPEN`, `CLOSE`, `PRINT #`, `INPUT #`, `LINE INPUT`, `WRITE`, `ADDRESS` ...

Design and details: [io-subsystem.md](io-subsystem.md). A file name may start with a **driver and a colon**: `file:` (the disk, the default), `null:` (accepts everything, always at the end), `mem:` (a RAM disk). An unknown driver is "Device not found". Names without a prefix belong to the `ADDRESS` driver; `RUN` resets it to `file`.

```
OPEN "mem:scores" FOR OUTPUT AS #1        OPEN "O",#1,"mem:scores"       (INPUT / OUTPUT / APPEND, or "I" / "O" / "A")
PRINT #1,"hello";1+2   WRITE #1,"a",5     LINE INPUT #1,A$    INPUT #1,A,B$    GET #1,K$
IF NOT EOF(1) THEN ...    PRINT LOF(1);LOC(1)    A$=INPUT$(10,#1)
CLOSE #1,#2     CLOSE                      (no argument: all channels)
ADDRESS MEM     ADDRESS "mem"     ADDRESS      (swap with the previous driver)     PRINT ADDRESS$
```

Phase 2 adds file system statements and three more drivers:

```
KILL "old.dat"     RENAME "a.txt" TO "b.txt"     MKDIR "saves"     CHDIR "saves"     (all take a "driver:" prefix, else ADDRESS)
SEEK #1,100                                       (byte position from the start of the open channel; LOC follows)
OPEN "sh:ls -l" FOR INPUT AS #1                  (sh: the output of a shell command; FOR OUTPUT feeds its input)
OPEN "res:ship.dat" FOR INPUT AS #1              (res: read-only game assets in the directory ./res)
OPEN "con:" FOR OUTPUT AS #2: PRINT #2,"hi"      (con: the screen / the keyboard as an ordinary channel)
```

`RENAME` never replaces an existing file ("File already exists") and both names must be on one driver. `mem:` has no directories.
`CHDIR` changes the directory of the whole program, so it affects `LOAD`, `SAVE` and `FILES`. Details: [io-subsystem.md](io-subsystem.md).

Phase 3 adds random access files, read/write files, and two more drivers:

```
OPEN "people.dat" FOR RANDOM AS #1 LEN=32        OPEN "R",#1,"people.dat",32         (records of 32 bytes; default 128)
FIELD #1, 20 AS N$, 12 AS C$                     (binds N$ and C$ to parts of the record)
LSET N$="Bob":RSET C$="Paris":PUTREC #1,3        (fill the parts, padded with spaces; write record 3)
GETREC #1,3:PRINT N$;C$        GETREC #1         (read record 3; no number = the next one; LOC(1) = last record)
OPEN "x.dat" FOR UPDATE AS #2                    (read and write, nothing truncated; short form "U")
OPEN "play:2" FOR OUTPUT AS #3: PRINT #3,"T120 C D E"      (every line is a PLAY string on voice 2)
OPEN "tcp:example.com:80" FOR UPDATE AS #4: PRINT #4,"GET /": LINE INPUT #4,A$     (a network connection)
```

Strings are binary-safe (see "Binary strings"); put numbers into records with `MKI$`/`MKD$` and friends (or `STR$`/`VAL` for text records). Details, errors and a proposal for drivers written
in BASIC: [io-subsystem.md](io-subsystem.md).

Channel 0 is the screen and keyboard; channels 1 to 255 are opened by `OPEN`. `LOAD`, `SAVE`, `MERGE`, `CHAIN` and `FILES` take driver names too (`SAVE "mem:game"`, `FILES "mem:"`); the default extensions apply to the path. `WRITE` is `PRINT` with strings in double quotes and commas between the items; `LINE INPUT` takes the whole line; `INPUT #n` splits it into fields and goes on with the next line when it runs out. Lines are written with a newline and read with a newline or a carriage return and a newline. A restart (`RUN`, `NEW`, `LOAD`, `CHAIN`) closes all channels. See io-subsystem.md for the exact rules, the error messages and what comes next (`KILL`, `RENAME`, `SEEK`, more drivers, random access records, the open sandboxing question).

**File extensions:** `SAVE "name"` writes the binary (tokenized) form and appends `.e99` to a name without extension; `SAVE "name",A` writes the ASCII listing and appends `.bas`. A name with an extension is used as it is. `LOAD` recognizes both forms by the header, whatever the extension is.

## Loop control: `EXIT`, `LEAVE`, `ITERATE`, `FOR … DOWNTO`, `FOR EACH`

```
FOR I=1 TO 9: IF I=4 THEN EXIT: NEXT I     ' EXIT [n] leaves the n-th innermost loop (default 1); LEAVE is the same word
FOR I=1 TO 5: IF I=3 THEN ITERATE: PRINT I: NEXT    ' ITERATE [n] goes on with the next pass of the n-th innermost loop
FOR I=10 DOWNTO 1 STEP 3 ... NEXT I        ' 10 7 4 1; DOWNTO counts down by the STEP value (default 1, give it positive)
DIM A(3): A(0)=5: A(1)=6: A(2)=7
FOR EACH V IN A(): PRINT V;: NEXT          ' value of every element
FOR EACH K,V IN A(): PRINT K;V;: NEXT V    ' key (the index) and value
```

- `EXIT`/`LEAVE`/`ITERATE` work in every loop (`FOR`, `FOR EACH`, `REPEAT`, `WHILE`, `UNTIL … UEND`). They are resolved at compile time like `AGAIN [n]`: the number counts the open loops
  around the statement, loops outside a function body are not visible, and "EXIT without loop" / "ITERATE without loop" is an error otherwise. `EXIT` jumps behind the end of the loop (the loop variable
  keeps its value), `ITERATE` to the step of a `FOR` (`NEXT`), the test of a `WHILE` / `UNTIL`, or the `UNTIL` test of a `REPEAT`.
- `EXIT`, `LEAVE` and `ITERATE` are new reserved words (tokens 217..219). `DOWNTO`, `EACH` and `IN` are **not** keywords: they are identifiers recognized in the place of `TO` / after `FOR`
  (`FOR EACH = 1 TO 3` still works, the variable is called EACH).
- `FOR EACH` goes over **all elements of an array in flat row-major order** (a 2-dimensional `A(2,2)` has 4 elements; the key is the flat index 0..3). The array must have been named by a `DIM`
  earlier in the text and is written with empty parentheses, like in `ERASE`. The value variable must match the array (numeric/integer array: numeric or integer variable, string array: string variable;
  else "Type mismatch"); the element is *copied* into the variable, changing the variable does not change the array. The element count is read once per pass, so the loop notices a `CLEAR`/`ERASE`.
  The loop is closed by `NEXT`, `NEXT value` or `NEXT key` (a string value variable can't be named: use plain `NEXT`).
- Known quirk: the code generator compiles the `ELSE` part of an `IF` before the `THEN` part, so `IF c THEN EXIT ELSE … : NEXT` (the `NEXT` that closes the loop in the `ELSE` part) is
  "EXIT without loop"; write the `NEXT` outside the `IF` or in the `THEN` part. (`AGAIN` has the same quirk.)
- Design decisions I took (revisit if unwanted): `DOWNTO` with a `STEP` counts down by that amount (the step is negated, so `STEP -1` would count *up*); the key of `FOR EACH` is the flat index and not
  the subscripts; `AGAIN` in a `FOR EACH` restarts after the element was fetched (does not re-fetch); `DYNAMIC`/`ASSOC` arrays do not exist yet, so only plain arrays can be traversed.

## Numeric functions, `\`, `MOD`, `RND`

```
PRINT INT(3.7);FIX(-3.7);ABS(-2);SGN(-9)        ' 3 -3 2 -1
PRINT SQR(2);EXP(1);LOG(10);PI                  ' square root, e^x, natural logarithm, 3.14159265358979
PRINT SIN(PI/2);COS(0);TAN(0);ATN(1)*4          ' angles in radians; ATN is the arc tangent
D=INT(RND(1)*6)+1                               ' a die
PRINT 17\5;17 MOD 5                             ' 3 2   (integer division and remainder)
```

| Function | Result |
|---|---|
| `INT(x)` | the largest integer not above x (rounds down: `INT(-3.2)` is -4) |
| `FIX(x)` | x with the fraction cut off (toward zero: `FIX(-3.7)` is -3) |
| `ABS(x)` `SGN(x)` | the absolute value, the sign (-1, 0, 1) |
| `SQR(x)` | the square root (x >= 0) |
| `SIN(x)` `COS(x)` `TAN(x)` `ATN(x)` | trigonometry in radians; `ATN` is the arc tangent |
| `EXP(x)` `LOG(x)` | e to the power x; the natural logarithm (x > 0) |
| `PI` | 3.14159265358979 (no parentheses) |
| `RND(x)` / `RND` | x > 0 or no argument: the next random number, 0 <= r < 1; `RND(0)`: the last number again; `RND(-n)`: starts the sequence over from the seed n and returns its first number |

**Angle units.** `RAD` (the default), `DEG` and `GRAD` choose the unit of the angles of `SIN`, `COS`, `TAN` (arguments) and `ATN` (result): radians, degrees (360 to
the turn) or gradians (400 to the turn). After `DEG`, `SIN(30)` is 0.5 and `ATN(1)` is 45. The multiples of a quarter turn are exact in degrees and gradians (`SIN(90)` is
exactly 1, `COS(90)` exactly 0); `TAN` of an odd multiple of a quarter turn (`TAN(90)`) is "Overflow". The unit is a property of the runtime: it stays over direct mode lines
(`DEG` typed at the prompt is in effect for the next line), and `RUN`, `NEW` and `LOAD` return to radians, so a program that does not choose a unit always starts in radians.
The angles of the **graphics statements** (`GFX CIRCLE`, `GFX RADIUS`) stay in degrees whatever the unit is: they are not affected by `DEG`/`RAD`/`GRAD`.

`RANDOMIZE` seeds the generator from the clock, `RANDOMIZE n` from the number n (the same n gives the same sequence). Without either, the sequence of each
run is different. `x \ y` divides after cutting both operands to integers and cuts the quotient; `x MOD y` is the remainder, with the sign of x; a divisor of 0 is
"Division by zero". Both stand at the level of `*` and `/` (evaluated left to right with them). A negative `SQR`/`LOG` argument is "Illegal argument", a result
that does not fit a number (`EXP(1000)`) is "Overflow". `ABS SGN SQR SIN COS TAN ATN EXP LOG RND FIX PI MOD RANDOMIZE DEG RAD GRAD` are reserved words now
(`INT` was one already).

## Error handling: `ON ERROR GOTO|GOSUB`, `ON ERROR OFF`, `ERR$`, `ERL`

```
10 ON ERROR GOTO 900           ' or ON ERROR GOSUB 900 / a label; ON ERROR OFF clears the handler
20 OPEN "data.txt" FOR INPUT AS #1
...
900 PRINT "Error: ";ERR$;" in line ";ERL
```

A run time error does not end the program while a handler is set: the data stack and open function calls are dropped (the parameter
variables of a function that failed get their old values back), `ERR$` becomes the error text without the `?` and the line
("Division by zero", "File not found", ...), `ERL` the number of the line (0 if the error happened in a direct mode line), and the
handler runs.

- **`ON ERROR GOTO target`**: the program continues at the target. The trap is then **off** (an error in the handler is an ordinary,
  fatal error, which prevents endless loops); execute `ON ERROR GOTO` again to set it anew.
- **`ON ERROR GOSUB target`**: the handler is a subroutine. `RETURN` continues with **the first line after the line that failed**
  (not the failed statement: the rest of that line is skipped; there is no `RESUME` yet). The trap is off while the handler runs and
  on again after its `RETURN`. In a direct mode line there is no next line: the run ends after the handler.
- **`ON ERROR OFF`**: errors are fatal again. The handler is also cleared when the program ends (`RUN`, `NEW`, `LOAD`, `CHAIN`, and after
  every direct mode line).
- `ERR$` and `ERL` keep the last error until the run ends. Not caught: Break (ESC) and the step limit of the tests. Syntax errors
  are found before the program runs and are not run time errors.
- `ERROR` is recognized after `ON` only (so a variable named `ERROR` still works elsewhere); `ERR` and `ERL` are reserved words now.

## Binary strings

A string may contain any bytes, zero included (`CHR$(0)` .. `CHR$(255)`); its length is kept with it, up to 65535 bytes as before.

- Everything works on the real length: `LEN`, `+`, `LEFT$`/`MID$`/`RIGHT$`, substring assignment, `LSET`/`RSET`, `FIELD`/`GETREC`
  (a part keeps its whole width), string variables and arrays, function parameters. `ASC(CHR$(0))` is 0. Comparison is byte by byte
  as unsigned values; a string that is a prefix of another one is the smaller. `STR$` and `VAL` read and write text: `VAL` stops
  at a zero byte (like at any non-digit).
- Files and devices carry them unchanged: `PRINT #n,a$` writes all bytes (a `;` at the end of the statement avoids the line feed),
  `LINE INPUT #n` reads a line up to `"\n"` with zero bytes in it, `INPUT$(n,#m)` and `GET #n` return zero bytes. `INPUT #n` accepts them
  inside string fields; a zero byte in a *numeric* field is "Bad data in file".
- **On the screen** a zero byte (and every control code 0..31 except line feed and ESC sequences) is drawn as a small glyph made of
  two letters between lines: `NU` for code 0, `SH`, `SX`, `EX`, `ET`, `EQ`, `AK`, `BL`, `BS`, `HT`, `LF` (code 10 is a line feed and not
  drawn), `VT`, `FF`, `CR`, `SO`, `SI`, `DL`, `D1`..`D4`, `NK`, `SY`, `EB`, `CN`, `EM`, `SB`, `EC` (27 starts an ANSI sequence), `FS`, `GS`,
  `RS`, `US`. The glyphs are part of the font (`8x12font1.txt`, section `#00`).
- Where a string is used as a **C string** (file and driver names, the command of a shell literal, `PLAY`/`SFX` strings, `LIST`/`RENUM`/...
  arguments) a zero byte is refused: "Illegal argument (zero byte in string)".
- Host callbacks: `vm_t.printn` / `repl_t.printn` (optional) receive output with zero bytes; without it the zero bytes are left out of
  what `print` gets.
- Number packing: see below.

### Number packing: `MKI$ MKL$ MKS$ MKD$`, `CVI CVL CVS CVD`

```
A$=MKI$(300)                  ' 2 bytes: 1, 44
PRINT CVI(A$)                 ' 300
LSET F$=MKD$(2.5):PUTREC #1   ' a number in a random file record
PRINT #1,MKL$(IP);MKI$(PORT); ' a binary message for a tcp: channel (the final ; avoids the line feed)
```

| Function | Result | Range of the argument |
|---|---|---|
| `MKI$(n)` | 2 bytes, 16 bit integer | -32768 .. 65535 (the low 16 bits are stored) |
| `MKL$(n)` | 4 bytes, 32 bit integer | -2147483648 .. 4294967295 |
| `MKS$(x)` | 4 bytes, IEEE 754 single | any (rounded to single precision) |
| `MKD$(x)` | 8 bytes, IEEE 754 double | any |
| `CVI(a$)` `CVL(a$)` `CVS(a$)` `CVD(a$)` | the number | the string needs at least 2 / 4 / 4 / 8 bytes; more are ignored, fewer are "Illegal argument" |

All of them are **big endian** (network byte order, unlike GW-BASIC, which is little endian), so a packed value can go straight into a
`tcp:` message or be read from one. `CVI` and `CVL` return signed values; for the unsigned one use `AND`:
`CVI(a$) AND 65535`, `CVL(a$) AND 4294967295`. A value outside the range of `MKI$`/`MKL$` is "Illegal argument" (GW-BASIC: Overflow).
`MKI`, `MKL`, `MKS`, `MKD`, `CVI`, `CVL`, `CVS`, `CVD` are reserved words now.

## Graphics: `GFX`, `TXT`, `TILE`, `SPRITE`, `DESIGN`, `CLS`, `LOCATE`, `COLOR`

```
GFX LINE 10,10,200,100,5            GFX CIRCLE 320,150,50,15            GFX PAINT 320,150,3
SPRITE 0,ON:SPRITE 0,100,50         SPRITE 0,MOVETO 500,50,120          DESIGN SPRITE 0 / @ "....11...." ...
GFX PALETTE 3,16,RGB(255,128,0)     TILE SET 1,2,3                      TXT FILL 0,24,79,24,32,1,46
PRINT POINT(3,4);SPRITE("X",0);GFX("FRAME")       ON COLLISION GOSUB 1000
```

Ten new reserved words (`GFX SPRITE TILE TXT DESIGN RGB POINT CLS LOCATE COLOR`); everything else is a sub-keyword. The complete reference, the `DESIGN`
picture rows, the functions, the collision rules and the internals are in [graphics-subsystem.md](graphics-subsystem.md); the example is `demos/gfx.bas`.
`CHR$(0)`..`CHR$(31)` are drawn as two letter glyphs on the text screen (see "Binary strings").

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
