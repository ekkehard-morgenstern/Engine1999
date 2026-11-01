# BASIC language and tokenized format

The BASIC dialect is classic line-numbered BASIC with extensions (labels, `UNLESS`, multi-line `IF`, `WHILE/REPEAT`
tokens, user functions with `DEF FN`, `DYNAMIC`/`ASSOC` arrays, `SAVE`, shell literals). Only a subset has grammar and code generation
so far (see "Implemented subset").

## Source preprocessing (`preprocess_buffer`)

Collapses runs of blanks/tabs to one space, drops `\r`, upper-cases letters — everywhere except inside `"…"` and `` `…` ``; **a comment (from the apostrophe to the end of the line) keeps its case** (since 2026-11-01; it was upper-cased before, so old listings keep their capitals).

**Indentation (2026-11-01):** the run of blanks *behind the line number* is kept as it is, so a program can be indented (`20     PRINT I` stays that way in `LIST`, `SAVE ... ,A`, `EDIT`, the full-screen editor and
the screen editor; the first blank is the separator after the number, the rest is the indent). Every blank or tab of that run counts as one blank (a tab is not expanded: the preprocessing works in place and the text must not
grow). The blanks are stored as `TOK_SPACE` tokens (one byte each), which the compiler ignores, so indentation costs program memory but no code. Blanks elsewhere in a line (`PRINT    A`) are still one,
and a line with blanks before its number gets one. Programs saved before this change load as they were. Tests: `repltest1` ("indent ..."), `contest1` (prompt, `EDIT n`, full-screen editor).
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

- Immediate/system: `LIST SAVE LOAD RUN AUTO RENUM DELETE MERGE CHAIN FILES BUILD BYE QUIT NEW CLEAR EDIT SHOW WARRANTY COPYING`
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
`bascomp.ebnf` was brought up to date on 2026-10-31 (all statements and functions up to `PEEK`/`POKE`; every production is defined, checked with a script); it ends with a list of what a grammar cannot say (words recognized in context, lookahead, checks of the code generator, the table-driven graphics statements are written out from `gfxops.c`). The parser and code generator remain the authority.

### INPUT and GET

    INPUT [#ch,] ["prompt" (;|,)] var {, var}
    GET   [#ch,] var {, var}

- `INPUT` shows the prompt (a plain string literal, or several), then `? ` if the prompt is followed by a semicolon or if there is no prompt; a comma after the prompt suppresses the `? `. It reads one line and distributes its comma-separated fields to the variables (plain or array elements, numeric, integer or string). A field starting with `"` extends to the closing quote, so it may contain commas. Numeric fields may be surrounded by blanks; an empty numeric field is 0; unquoted string fields are trimmed.
- A missing field or a non-numeric number prints `?Redo from start` and asks again (prompt included); variables already stored stay stored. More fields than variables print `?Extra ignored`. End of input (host callback returns nothing) is the error "End of input".
- `GET` waits for a key for every variable: a string variable gets the one-character string, a numeric variable the key code. (`INKEY$` is the non-waiting variant.)
- Without `#ch` both use channel 0. The channel is passed to the host callbacks (`vm->input`, `vm->getkey`); the VM's output channel (`PCHN`) is set to it too, so the prompt goes to the same channel.

## Keyboard: cursor keys, function keys, the character set *(owner's design of 2026-10-31; the font, the sequence tables and `--debug-keys` are done, `GET` / `INKEY$` are not yet)*

**Status:** decided in outline by the owner. **Done (2026-10-31):** the font for 128..255 (`docs/display-engine.md`, "The character set"), the sequences module `keyseq.[ch]` (canonical sequences of all keys with modifiers, a parser for the dialects terminals send) with the SDL mapping `sdlkeys.[ch]`, and `--debug-keys` for both hosts (`docs/build-and-test.md`). **Window host, done (2026-10-31):** `GET` / `INKEY$` return the whole sequence (`sdlscr_pollkey()` hands out the bytes one by one, the VM groups them: `vm_key_sequence()`; a key after an ESC that does not continue a known sequence stays a key of its own), typed text is converted from Unicode to ISO-8859-15 (`ä ö ü ß €` work at the prompt, in the editors and for `GET`/`INKEY$`; characters outside the set are dropped). **Console host, done (2026-10-31):** `con_readkey()` (`engine1999c.c`) collects the bytes after an ESC (50 ms) and delivers the *canonical* sequence whatever the terminal sent (the real dumps of the owner's terminal: xterm-256color, all canonical, F1-F4 as `ESC O P..S`, Shift+Tab `ESC [ Z`); unknown CSI / SS3 sequences are skipped, an ESC not followed by a sequence is a plain ESC. **Shift+Tab** is a key of its own (`KS_BACKTAB`, `ESC [ Z`; the window sends it for Tab with Shift). On a terminal the UTF-8 of typed keys and of `INPUT` / program lines becomes ISO-8859-15, and output from 128 up becomes UTF-8 (the C1 codes 128..159 give U+FFFD); in a pipe or a file the bytes are passed unchanged both ways. **Not done:** the encoding of `.bas` listings. **Note on printing a sequence:** `PRINT A$` of a cursor key does not show it, because the ESC starts an escape sequence in the screen's ANSI decoder, which swallows it (`ESC [ A`, `ESC [ 15 ~`) or shows only the tail (`ESC O P` shows `OP`); print `ASC(MID$(A$,i,1))` instead. Today only printable ASCII, `RETURN` (13), `BACKSPACE` (8) and `TAB` (9) reach `GET` / `INKEY$`; every other key is dropped, and `ESC` is the break key.

**Character set.** Strings are bytes (see "Binary strings"); the character set is **ISO-8859-15** (Latin-9, ISO-8859-1 with `€ Š š Ž ž Œ œ Ÿ` in place of eight rarely used signs). Codes 0..31 and 127..159 are control codes, the rest is printable. Control codes are drawn as glyphs on the text screen (a convenience for the programmer: output that should not be a control code is visible as one; the glyphs are also free for user-defined characters, but users mostly redefine characters they can type). `DEL` (127) keeps the glyph it has now. The font gets the missing printable glyphs 160..255 (`8x12font1.txt`, `perl fontcomp.pl`), so every non-control character can be printed; 128..159 get control glyphs like 0..31. Typed text that is not ASCII (`ä ö ü ß €`...) is mapped from the Unicode the keyboard delivers to ISO-8859-15 (today it is ignored); characters outside the set are dropped. The console host converts between the terminal's UTF-8 and ISO-8859-15 on input and output (to decide: option for a raw/Latin-1 terminal).

**Keys that are not one byte arrive as ANSI escape sequences** (the xterm forms), as one string:

| Key | Sequence | Key | Sequence |
|---|---|---|---|
| UP / DOWN | `ESC [ A` / `ESC [ B` | F1 (= HELP, reserved for a later help system) | `ESC O P` |
| RIGHT / LEFT | `ESC [ C` / `ESC [ D` | F2 .. F4 | `ESC O Q`, `ESC O R`, `ESC O S` |
| HOME / END | `ESC [ H` / `ESC [ F` | F5 .. F8 | `ESC [ 15 ~`, `17 ~`, `18 ~`, `19 ~` |
| INSERT / DELETE | `ESC [ 2 ~` / `ESC [ 3 ~` | F9 .. F12 | `ESC [ 20 ~`, `21 ~`, `23 ~`, `24 ~` |
| PAGE UP / DOWN | `ESC [ 5 ~` / `ESC [ 6 ~` | with modifiers | `ESC [ 1 ; m A` etc., m = 2 Shift, 3 Alt, 5 Ctrl, 4/6/7/8 combined |

`INKEY$` returns the whole sequence (or `""` when no key is waiting), `GET a$` waits for one key and stores the whole sequence; a program compares with `CHR$(27)+"[A"`. `GET n` (numeric) stores the first byte, 27 for every sequence (use a string to tell the keys apart). The sequence is complete when it is delivered: the window host builds it from the key event; the console host reads the bytes of the terminal and aborts a sequence that does not continue within a short time (about 50 ms), so an `ESC` pressed once and forgotten does not wait for more characters.

**`KEY(name$)` — is a key held down? (2026-11-01).** `INKEY$` delivers key *presses*; a game needs to know whether a key is *held* and when it is let go. `KEY("LEFT")` is `-1` (true) while the key is down and `0` otherwise, for any number of keys at once: `IF KEY("LEFT") THEN X=X-1`. The argument is a string expression, case does not matter: `"A".."Z"`, `"0".."9"` and the other keys that type a character (`","`, `"/"`, ...; found by the character, whatever the keyboard layout is), `"SPACE"`, `"RETURN"` (`"ENTER"`), `"ESC"`, `"TAB"`, `"BACKSPACE"`, `"UP"`, `"DOWN"`, `"LEFT"`, `"RIGHT"`, `"HOME"`, `"END"`, `"INSERT"`, `"DELETE"`, `"PGUP"`, `"PGDN"`, `"F1"` .. `"F12"`, the modifiers `"SHIFT"`, `"CTRL"`, `"ALT"` (also `"LSHIFT"`, `"RSHIFT"`, `"LCTRL"`, `"RCTRL"`, `"LALT"`, `"RALT"`), the keypad `"KP0"` .. `"KP9"`, `"KPENTER"`, `"KP+"`, `"KP-"`, `"KP*"`, `"KP/"`, `"KP."`, and `"ANY"` (any key at all). A name that is no key is "Illegal argument". `KEY` reads the keyboard at the moment of the call and does not touch the keys `INKEY$` / `GET` deliver. `ESC` shows as down, too, although it is also the break key (`BREAK ON`). Token 252 (a reserved word), instruction `KEYD 0x17D` (`( name -- n )`), `vm_t.keydown`, names in `keystate.[ch]`. **Window:** the real state of the keyboard (SDL's keyboard state, read without a lock). **Console host: not available** — `KEY` is the run-time error "KEY is not available in this host" there (and in console executables made with `BUILD`). A terminal reports key presses but never releases, so the state can only be guessed; the guess (a key counts as held for 140 ms after the last byte the terminal sent for it) flickered while a key was held (the repeat delay of the keyboard is 250 to 500 ms), which was not acceptable; the code is kept, switched off (`-DCON_KEYSTATE` in `engine1999c.c`), and the owner reviewed it (2026-11-01) and left it so: a hold time can't be right (a tap looks like a hold until the repeat delay has passed, and a terminal repeats only the last pressed key), see the roadmap. Use the window version for games; the terminal programs of the future may report key releases (the kitty keyboard protocol). Demo: `demos/keys.bas`. Design decisions I took (revisit if unwanted): one function with a string argument instead of a numeric key code or `KEYDOWN`; the word `KEY` is reserved (`ON KEY` is still free to be built on it); no key-up *events* (`ON KEY` is in the roadmap); true is `-1` like `PLAYING()`.

**Break (decided 2026-11-01, implemented).** The break keys are `ESC`, **Ctrl+C**, and in the window `PAUSE/BREAK` (also with Ctrl): they stop a running program (also one that waits in `INKEY$` / `GET` / `INPUT`). Ctrl+C is the break key in both hosts and cannot be taken over; the console has no `PAUSE/BREAK` (a terminal never receives it). A double-stroke `ESC` was considered and dropped: it would delay a single `ESC` and add another timing rule. A program that wants `ESC` for itself says **`BREAK OFF`**: `ESC` is then an ordinary key (`INKEY$` and `GET` return `CHR$(27)`; at `INPUT` it gives an empty line), and Ctrl+C / `PAUSE/BREAK` still stop the program, so a runaway program can always be stopped. **`BREAK ON`** gives `ESC` its meaning back; every run starts (and ends) with `BREAK ON`. The same code runs in the window, the console host and executables made with `BUILD` (their break keys are the same; there is no way to switch Ctrl+C off). In `INPUT`, `LINE INPUT`, the line editors and the full-screen editor, `ESC` cancels and Ctrl+C / `PAUSE/BREAK` are the break key (the editors treat it like `ESC`).

**Implementation notes.** The host callbacks `inkey` / `getkey` keep returning one byte per call; the VM groups a sequence (after a first byte 27 it takes the continuation bytes the host has ready), so `vm.h` does not change; `sdlscr_msgtokey()` (`sdlscreen.c`) maps the SDL key symbols, `con_readkey()` (`engine1999c.c`) parses the terminal's sequences (the same table, plus the SS3/CSI variants terminals send), the break keys are `sdlscr_isbreakkey()` (window) and `con_pump()` / `con_readkey()` (console). Tests: `contest1` (sequences through a pipe, a lone ESC, a half sequence), `vmtest1` (a test input with sequences), `gfxtest1` / `txttest1` (the new glyphs).

**Open points for the owner:** whether `GET n` should rather return a code outside 0..255 for sequences (it would need the `GETK` result as a number), the double-press time, whether the Windows-style `Ctrl` + letter control codes (1..26) are delivered as bytes (proposal: yes).

## Program commands: `LOAD`, `RUN`, `MERGE`, `CHAIN`, `FILES`, `LIST`, `NEW`, `CLEAR`, `DELETE`, `RENUM`, `EDIT`, `MONITOR`, `BYE`, `QUIT`

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
- `BYE [code]` / `QUIT [code]` — ends the program **and the Engine** (or the built executable) at once; the process exit status is `code`, an integer 0..255 (0 without it, anything else is "Illegal argument"). Usable in direct mode and in programs, anywhere (inside loops and procedures, after `THEN`/`ELSE`); nothing after it runs, open files are closed, queued sound stops with the process. Two words for one statement. **`EXIT` is not a synonym**: `EXIT [n]` is the loop word (leave the innermost n-th loop, `EXIT PROC`), and it keeps meaning that. In a built program `BYE`/`QUIT` returns the code to the shell (`./game; echo $?`); a program that just ends returns 0, one that stops with an error 1. New reserved words: `BYE QUIT` (tokens 246, 247; `NT_BYESTMT 0x84`, instruction `BYE 0x178`, `VMREQ_QUIT`, `repl_t.quit` / `repl_t.exitcode`, which the host looks at after the run). Design decisions I took (revisit if unwanted): the code must be a whole number 0..255 (no wrapping of other values); `QUIT` and `BYE` are interchangeable and both stay as typed in a `LIST`.
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
- **When a handler runs:** between two statements (**also inside the body of a `PROC` or `FUNC`**; a handler is held back while an expression is half evaluated, e.g. inside a `FUNC` called in the middle of `A=1+FN F(5)`; until 2026-10-31 handlers did not run inside procedures at all and waited for the return: the music of the demos stopped), and during `WAIT` (which gives way to the handler and then sleeps for the rest of its time). Not while a statement is under way inside an expression (a `DEF FN` call), and not during `INPUT` or `GET` (they wait for the keyboard; use `INKEY$` loops for timed input). A timer that comes due meanwhile runs right after.
- An `EVERY` timer keeps its rhythm (the next tick is one interval after the previous *due* time); if the program was too busy to run it in time, the missed ticks are dropped instead of being run in a burst.
- **Timers end with the run** that set them: when the program (or the direct mode line) ends, is stopped by Break or an error, or a new run starts, all timers are gone. A program that ends while a timer is pending never runs the handler. (So `AFTER` typed alone at the prompt does nothing; put it in a line with the statements that should be interrupted, or in the program.)
- `AFTER 0`, a negative delay, or a timer number other than 0..3 is "Illegal argument".
- **`REMAIN(timer)`** returns the ticks (1/60 s, rounded up) until timer 0..3 is due, **0 if the timer is not set** (or has already fired), and **clears the timer** — as in Locomotive BASIC, where it is the way to cancel one timer and see how much time was left. It works for `AFTER` and `EVERY` timers alike (an `EVERY` timer is cleared, too). `REMAIN(4)` and `REMAIN(-1)` are "Illegal argument". `REMAIN` is a reserved word now.
- A **direct mode line** may use line numbers as targets (`GOTO 100`, `GOSUB 100`, `AFTER 60 GOSUB 100`, `RESTORE 20`): they refer to the program as it was when it was last compiled (by `RUN`, `CHAIN`, ...). After the stored program was changed (a line entered or deleted, `DELETE`, `RENUM`, `MERGE`, `LOAD`) the old line table no longer fits, and a line number in a direct line is "Undefined line number" until the next `RUN`; labels always work.

## `BUILD`: standalone executables

`BUILD name$` makes an executable file `name$` (no extension is added) from the stored program, in direct mode or inside a program (the run goes on). The executable starts the program at once, in the Engine's window, without banner and `Ready.` prompt; `RUN`, `LOAD`, `CHAIN` and `MERGE` inside it work as usual (they load other files), `EDIT` and `MONITOR` end the program, there is no direct mode.

```
LOAD "demos/fruit2"
BUILD "fruit2"          ' then, in a shell:  ./fruit2
BUILD "report", CONSOLE ' a program without a window: needs no SDL2 (see below)
```

- **Two kinds of runtime: `BUILD name$ [, WINDOW | CONSOLE]`** (`WINDOW` and `CONSOLE` are identifiers, not keywords). A **window** executable is the program with the runtime of `engine1999` (`engine1999.o`, SDL2 and the C and math libraries). A **console** executable has the runtime of the console host (`engine1999c.o`, see [console-host.md](console-host.md)): it needs **only the C and math libraries**, so it runs on a machine without `libSDL2` (a server, a container): its output goes to standard output, errors to standard error, `INPUT`/`GET` read standard input, `BYE n` returns the exit status, an error ends it with 1; `CLS`/`LOCATE` are ANSI sequences on a terminal, graphics statements fail, sound is silent (everything in console-host.md). Without the option `BUILD` makes the kind of the host that runs it: `engine1999` window executables, `engine1999c` console ones. The program and the command line arguments (`ARGC`, `ARG$`) work in both kinds; `--engine1999-about` is the first argument that the runtime itself takes.

- **Only the system linker is used** (`ld`, no compiler): the program (the binary `.e99` form) is turned into an object with `ld -r -b binary`, and `ld` binds it to the runtime kit, `engine1999.o` (the host: window, keyboard, sound; its `main()` runs the embedded program) and `engine.lib` (everything else), statically. **SDL2, the math library and the C library are linked dynamically**, so the executable needs `libSDL2` on the machine that runs it (and no Engine installation).
- **The system files are found automatically** (`basbuild.c`): the linker (`$ENGINE1999_LD`, else `ld`/`ld.bfd`/`ld.gold` in the `PATH` and the usual directories), the runtime kit (the directory of the running `engine1999`, `$ENGINE1999_HOME`, the current directory, `/usr/local/lib/engine1999`, `/usr/lib/engine1999`, `/usr/local/share/engine1999`, `/usr/share/engine1999`), the libraries and `crt1.o`/`crti.o`/`crtn.o` (`$LIBRARY_PATH`, `/usr/lib/<machine>-linux-gnu`, `/lib/...`, `/usr/lib64`, `/lib64`, `/usr/lib`, `/lib`, `/usr/local/lib`; `libSDL2.so` or else `libSDL2-2.0.so.0`), the dynamic linker (`/lib64/ld-linux-x86-64.so.2` and its relatives for aarch64, i386, arm, riscv64, ppc64le, musl, else a search of `/lib*/ld-linux*`), and, if present, the compiler's own `crtbegin.o`, `crtend.o` and `libgcc.a` (the newest version). What can't be found is named in the error: `? Build failed: Library SDL2 not found (install its development package)`. `ENGINE1999_BUILD_TRACE=1` prints the two `ld` command lines to the standard error output.
- **Errors:** `No program` (nothing stored), `Bad name for an executable` (a name with another driver than `file:`, an empty path, or a BASIC program file name `.bas` / `.e99`: `BUILD` never overwrites a program file), `Build failed: …` (with the linker's message), `Illegal argument` (an empty string). The program is **not compiled** by `BUILD`: a syntax error shows when the executable starts (as the error message plus "Press any key to end the program."). `RUN` it first.
- **Behavior of the executable:** the program runs once; when it ends normally the executable ends too, after queued sound has finished (10 s at most; a key press ends the wait). An error ends it with the message and "Press any key to end the program."; ESC (Break) and closing the window end it at once. **`--engine1999-about`** (the only command line option) prints the copyright and license notice of the runtime to the standard output and exits, without opening a window; it must stay in the executable (see below); it also names the version of the runtime (`Engine 1999 v0.5.0 ...`, see `docs/build-and-test.md`, "Version"). Output on channel 0 is the screen; files, `sh:`, `tcp:` and the other drivers work as in the Engine; `ADDRESS`, `res:` (`./res` of the current directory) and `SFX` definitions behave as after a `RUN`.
- **License:** the executable contains the runtime, which is GPLv3 code; the **Engine1999 Runtime Exception** (`LICENSE-EXCEPTION`) lets you distribute it under terms of your choice, if the runtime is the unmodified one and its notice stays (`--engine1999-about`). The program itself is not covered by the GPL. The demo programs carry a comment with the terms. See `LICENSE-EXCEPTION` for the exact text; `SHOW COPYING` in the Engine prints it after the GPL (the string in `engine1999.c` must be kept in sync with the file), and `--engine1999-about` of an executable prints its conditions in short.
- The code of the runtime is the code that was compiled for the Engine on the building machine (`make` uses `-march=native`): an executable runs where that CPU's instructions exist. To build executables for other machines, compile the Engine without `-march=native` first (`make` after editing `CFLAGS`).
- Design decisions I took (revisit if unwanted): the executable embeds the tokenized program (not the listing) and compiles it at start (a few milliseconds); `BUILD` writes to the file name given, no `.e99`-style default extension (Linux executables have none); the window closes when the program ends (no "press a key" after a normal end; end a program with `GET` or `WAIT` if the last screen should stay); no compile check in `BUILD`; the runtime kit is two files next to `engine1999` (`engine1999.o` and `engine.lib` stay in the build directory after `make`, a separately installed Engine has to put them in one of the kit directories); the executable is linked non-PIE with `-z relro`, no `-z now`, no stripping; only Linux (ELF, `ld`), tried on x86-64 (the other machines' file names are the usual ones, not tested).
- New reserved word: `BUILD` (token 245, `NT_BUILDSTMT 0x83`, instruction `BLD 0x177` (parameter 0 default / 1 WINDOW / 2 CONSOLE), `vm_t.build( …, kind )`, `repl_t.buildconsole`, `build_console_opts()`, `repl_vm_build()`, `repl_program_image()`, `repl_load_image()`, `build_executable()`). Tests: `buildtest1` (27 checks: dynamic linking with a stand-in runtime, error messages, no work files left behind, and the real runtime with SDL2), `vmtest1`, `repltest1`.

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

## Program arguments and the environment: `ARGC`, `ARG$`, `ENV$`; the standard streams

For programs that run as commands (`engine1999c prog.bas a b`, or an executable made with `BUILD`: `./prog a b`):

- `ARGC` — the number of arguments (0 if there are none).
- `ARG$(n)` — argument `n`: `ARG$(0)` is the program (the file given to `engine1999c`, `-` for standard input, the executable's own name in a built program), `ARG$(1)` ... `ARG$(ARGC)` the arguments as the shell split them. A larger `n` gives `""` (so a missing argument needs no test of `ARGC`), a negative or fractional one is "Illegal argument". In the window version `engine1999` there are no arguments (`ARGC` is 0, `ARG$(0)` is `""`).
- `ENV$(name$)` — the value of the environment variable of the process, `""` if it is not set (or the name is empty). Reading only: there is no way to set one yet. Works in every version.
- The words behind the program file are the program's, even if they look like options: `engine1999c prog.bas --version` hands `--version` to the program. A built program takes only one option of its own, `--engine1999-about`, and only as its first argument; a console executable also takes `--engine1999-screen`, `--engine1999-line` and `--engine1999-no-dump` (screen mode, see `docs/console-host.md`), before the program's words.

```
10 IF ARGC < 1 THEN PRINT "usage: greet NAME": BYE 2
20 PRINT "Hello, "; ARG$(1); " from "; ENV$("USER")
```

The **standard streams** are drivers like `mem:`: `OPEN "stdout:" FOR OUTPUT AS #1`, `OPEN "stderr:" FOR OUTPUT AS #2`, `OPEN "stdin:" FOR INPUT AS #3` (the short forms work too: `OPEN "O",#2,"stderr:"`). Nothing may follow the colon ("Bad file name"); `stdin:` can only be read and the other two only written or appended ("Bad file mode"). `PRINT #2,"warning"` goes to the standard error output, `LINE INPUT #3,A$` reads a line of standard input. They are the real streams of the process in every version, so a window program started from a shell can write to its terminal. `stdin:` reads at most one line per read and never ahead, so `INPUT`, `GET` and a channel on `stdin:` can take turns in a pipe; `stderr:` flushes `stdout` first, so both stay in order. Channel 0 is still the screen (the console host: standard output). New reserved words: `ARGC ARG ENV` (tokens 248..250; `ARG$` and `ENV$` are written with the dollar sign like `CHR$`); instructions `ARGC 0x179`, `ARGS 0x17A`, `ENVS 0x17B`; `repl_t.argc/argv` → `vm_t.argc/argv`. Design decisions I took (revisit if unwanted): `ARG$` beyond the arguments is `""` rather than an error; the arguments are strings as the shell gave them (no splitting of `a=b`); read-only environment; the program name counts as `ARG$(0)`, not in `ARGC`.

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

## Procedures and functions: `PROC`, `FUNC`, `CALL`, `LOCAL`, `RETURN value`, `EXIT PROC`

```
10 PROC STARS(N)                  ' a procedure: called with CALL, no value
20 LOCAL I
30 FOR I=1 TO N: PRINT "*";: NEXT I
40 PRINT
50 END PROC
60 FUNC FACT(N)                   ' a function: called like DEF FN, FN name(args)
70 IF N<=1 THEN RETURN 1
80 RETURN N*FN FACT(N-1)          ' recursion works
90 END FUNC
100 CALL STARS(5): PRINT FN FACT(6)
```

- `PROC name[(params)] … END PROC` is called with `CALL name[(args)]`; `FUNC name[$|%][(params)] … END FUNC` is called like a user function: `FN name(args)`. The header must be alone
  on its line; the body is jumped over where it stands (a definition can be anywhere in the program). A `FUNC` has to be **defined before the line that calls it** (the call is checked
  when the line is parsed, like `DEF FN`); a `CALL` is checked when the code is generated, so a `PROC` may follow its calls (the main program on top, the procedures below).
- Parameters are numeric, integer or string **variables passed by value**. They are ordinary global variables that get the arguments for the time of the call and their old values
  back afterwards (that is what makes recursion work: every call saves the values of the previous one).
- **Array parameters** are written with empty parentheses in the header (`PROC FILL(A(), N)`) and get an array of the same type as the argument: `CALL FILL(V(), 5)`. They are
  **by reference** (no copy: the procedure sees and changes the elements of `V`; the dimensions and the bounds checks are those of the argument, any number of dimensions). Technically
  the parameter array `A` becomes the same storage as `V` for the time of the call and gets its own one-element placeholder back afterwards. The code generator checks that an array
  parameter gets an array (`A()`) of the same base type and a scalar parameter no array. Arrays may be passed on (`CALL UP(A(), N-1)`).
- **`LOCAL a, b$, n%, T(10), D()`** (in the body of a `PROC`, `FUNC` or multi-line `DEF FN` only) makes variables local to the call: their values are saved when `LOCAL` runs, they start as 0 / `""`,
  and the old values return at the end of the call (also if an error unwinds it). Without `LOCAL` a variable is the global one (there is no `GLOBAL`, everything else *is* global).
  `LOCAL` takes effect when it is executed, so put it first.
- **Local arrays**: `LOCAL T(10)` declares a new array (like `DIM`: the dimensions are constants, it is an error if `T` exists) whose elements are new and zeroed for every call (a recursive
  procedure gets one per level) and go away at the end of the call; `LOCAL D()` gives a call its own zeroed copy, with the shape, of an array `D` that exists (also an array parameter). The
  array variable itself is still one global name: outside the call it is the original again (for `LOCAL T(10)` the array declared at parse time, a one-time declaration).
- A `FUNC` returns what `RETURN expr` or `FN name = expr` last set (0 / `""` if none); `RETURN expr` also leaves at once. `EXIT PROC` / `EXIT FUNC` (also `EXIT FN` in a multi-line `DEF FN`)
  leave at once (a `FUNC` returns the value set so far). A plain `RETURN` is still the `RETURN` of `GOSUB` (a `GOSUB` inside a procedure to a line of that procedure is fine). `RETURN expr` also works in
  a multi-line `DEF FN`.
- The result of a `FUNC` lives in the hidden variable `@FN<name>`, which is saved like a `LOCAL`, so a recursive call does not disturb the result of its caller.
- Design decisions I took (revisit if unwanted): `PROC` is a function variable of the label type (own name space: a `PROC A` and a `FUNC A` may both exist); redefining a name with the same number of
  parameters reuses it; a `PROC` or `FUNC` can't be defined inside another one; a `GOTO` out of a body leaves the call frame open (as with `DEF FN`; `POP` doesn't know about it).
- New reserved words: `PROC FUNC CALL LOCAL RESUME` (tokens 235..239).

## `STOP`, `CONT`, `TRACE`, `AUTO`

- **`STOP`** ends the run like the Break key ("Break in line n"), but keeps the state of the program for **`CONT`**: the next instruction, the data stack, the `GOSUB` return stack and
  the `PROC`/`FUNC` calls in progress (with their `LOCAL` variables, so you can `PRINT` them in between). The **Break key (ESC)** is continuable, too, when it hits between two
  instructions (not while the program waits in `INPUT`, `GET`, `WAIT`). Direct mode lines can be run in between (`PRINT A`, `A=5`, `GOSUB`...) and the program goes on with the changed variables.
- `CONT` says "Cannot continue" if nothing is stopped, if the **stored program was changed** since (a line entered, `DELETE`, `RENUM`, `MERGE`, `LOAD`: the code no longer matches), after a new
  run (`RUN`, `NEW`, `CHAIN`), and after a `STOP` or Break in a *direct mode line*. Timers (`AFTER`/`EVERY`) and the `ON ERROR` handler end with a run and are not restored by `CONT`.
  `STOP [n]` (AsmBASIC) has no argument here.
- **`TRACE ON` / `TRACE OFF`**: while on, every program line that starts prints its number in brackets (`[10]`), before the output of the line. It lasts until `TRACE OFF` (also over `RUN`).
- **`AUTO [start[,step]]`** (direct mode, both default 10) lets the host prompt with line numbers: type the rest of the line and RETURN to enter it, an empty line (or a number above
  999999) ends it. An existing line number is replaced. `engine1999` and `engine1999c` do this in their main loops (`repl_t.autoline`/`autostep`; the console host writes the numbers on a terminal only, a script reading from a pipe gets none); other hosts have to prompt themselves.
- New reserved words: `STOP CONT TRACE` (tokens 240..242; `AUTO` was a token already).

### `BREAK ON` / `BREAK OFF`: who owns the ESC key

`BREAK OFF` makes `ESC` an ordinary key for the program (`INKEY$` and `GET` deliver `CHR$(27)`, `INPUT` gives an empty line for it); `BREAK ON` is the default and gives `ESC` its meaning as a break key back. Ctrl+C (both hosts) and `PAUSE/BREAK` (window) always stop the program; every run starts with `BREAK ON` and the setting ends with the run. Instruction `BRKM` (parameter 1 / 0), `vm_t.escbreak`, host callback `breakesc`; the window is `sdlscr_setescbreak()`, the console `con_escbreak`. Built executables behave the same (the same host code).

## `PEEK` and `POKE`: a RAM area

```
POKE 100,65                      ' a byte (0..255; the low bits of -128..255 are stored)
POKE 100,1000,2 : PRINT PEEK(100,2)   ' size 2 = word, 4 = long: big endian; -4 = single, -8 = double precision float
PRINT PEEK(100)                  ' a byte, unsigned; PEEK(a,2) unsigned word, PEEK(a,4) signed long
POKE 200,"HELLO"                 ' a string value writes its bytes (any byte values)
PRINT PEEK$(200,5)               ' a string of 5 bytes from the RAM
```

- The RAM is **1 MiB (`VM_RAMSIZE`)**, addressed from 0, zeroed, allocated by the first `POKE`, **kept over direct mode lines** and cleared by `RUN`/`NEW`/`LOAD`. It is a block of its own: it does
  not show variables, the program or the screen (a window into the data segment or the graphics layers could be added later; `SYS addr` is not implemented: there is no machine code).
- Errors: "Illegal address" (outside the 1 MiB), "Illegal argument" (a size other than 1, 2, 4, -4, -8; a number that does not fit its size; a negative or too long `PEEK$` length). `PEEK$` needs both arguments.
- Multi-byte values are big endian like the other binary values (`MKI$`, `CVI`).
- Design decisions I took (revisit if unwanted): one `PEEK` / `POKE` word with a size argument instead of `PEEKB/W/L/F`; negative sizes for floats; only the RAM is addressable; byte and word reads are unsigned, long reads signed.
- New reserved words: `PEEK POKE` (tokens 243, 244).

## More operators and functions (2026-10-31)

- `IMP`, `NIMP` (`a IMP b` = `(NOT a) OR b`, `a NIMP b` = `a AND NOT b`), `EQV` (= `XNOR`) and `NEQV` (= `XOR`), bitwise on 32 bit integers like the other logical operators. **They share the precedence of `OR`/`XOR`**
  (left to right, below `AND`); AsmBASIC puts `IMP`/`EQV` on a level of their own below `OR`: use parentheses when mixing.
- Shifts: `SHL` and `ASL` (= `LSHIFT`), `ASR` (= `RSHIFT`, arithmetic: keeps the sign), `SHR` (logical: fills with zeros; instruction `URSH`). Shift counts outside 0..31 give 0 (`SHL`, `SHR`) or the sign fill (`ASR`).
- `FLOOR(x)` (= `INT`), `CEIL(x)`, `LOG2(x)`, `LOG10(x)` (`LOG2`/`LOG10` of a number <= 0 is "Illegal argument"). Not added: `LN`/`LG` (= `LOG`/`LOG10`) and `LOGN(b,x)` (= `LOG(x)/LOG(b)`: write it with `LOG`).
- `TIME$` (`HH:MM:SS`), `DATE$` (`YYYY-MM-DD`), `DATETIME$` (both, with a blank), the local time (instruction `DTIM`). They can't be assigned (no setting of the clock).
- New reserved words: `FLOOR CEIL LOG2 LOG10 TIME DATE DATETIME IMP NIMP EQV NEQV SHL SHR ASL ASR` (tokens 220..234).

## `MIN`, `MAX`, `ATAN2`, `SPACE$`, `STRING$` (2026-11-01)

- `MIN(a,b)`, `MAX(a,b)`: the smaller / larger of two numbers (two arguments only; nest them for more: `MAX(MIN(x,hi),lo)` clamps).
- `ATAN2(y,x)`: the angle of the point (x, y) in the unit of `DEG` / `RAD` / `GRAD` (radians by default), -PI..PI in all four quadrants (`ATN` only knows two). `ATAN2(0,0)` is 0.
- `SPACE$(n)`: n blanks. `STRING$(n,c)`: n copies of a character; `c` is a character code 0..255 (`STRING$(3,0)` is three zero bytes) or a string, whose first character is used (an empty string is "Illegal argument"). n is 0..65535, else "Illegal argument".
- **New reserved words** `SPACE STRING MIN MAX ATAN2`, but they are the first **extension words**: the one-byte token space (0..255) is full, so these are stored as the token `TOK_XFUNC` (254) followed by one byte naming the word (`XF_*` in `bastok.h`, table `xkeywtbl` in `bastok.c`). To the programmer nothing differs from any other reserved word (they list in upper case, `--list-keywords` and the man page include them). **New words are added to that table from now on**: 255 more are possible. Instructions `MTH2 0x17E` (`( x y -- z )`, parameter 1 MIN, 2 MAX, 3 ATAN2) and `STRN 0x17F` (`( n c -- s )`, parameter 0 code, 1 string, 2 `SPACE$`). Tests: `vmtest1` (17), `repltest1` (2), `asmtest1` (2).

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

## Error handling: `ON ERROR GOTO|GOSUB`, `ON ERROR OFF`, `ERR$`, `ERR`, `ERL`, `ERROR n`

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
  (not the failed statement: the rest of that line is skipped; use `RESUME` to go on in a finer way). The trap is off while the handler runs and
  on again after its `RETURN`. In a direct mode line there is no next line: the run ends after the handler.
- **`ON ERROR OFF`** (also written `ON ERROR GOTO 0`, as in other BASICs; so a handler can't be at line 0): errors are fatal again. The handler is also cleared when the program ends (`RUN`, `NEW`, `LOAD`, `CHAIN`, and after
  every direct mode line).
- **`ERR`** is the number of the last error (0 if there was none; `ERR$` is the text). The numbers are those of GW-BASIC where the message has one: 4 Out of DATA, 5 Illegal argument,
  6 Overflow, 7 Out of memory, 9 Subscript out of range, 11 Division by zero, 13 Type mismatch, 15 String too long, 52 Bad file number, 53 File not found, 68 Device not found; **every other message is 255**
  (table `errtab` in `vm.c`; a message counts if it begins with the text, so `Type mismatch (string expected)` is 13).
- **`ERROR n`** raises error number n (1..255, else "Illegal argument") by hand: the text of the table, or `Error n` for a number that has none (and `ERR` gives n back). It is caught by `ON ERROR` like a real
  error, or is fatal (`? Error 77 in line 10`) when no handler is set. `ERROR` is now an extension word (reserved; `ON ERROR` still reads the old identifier form in saved programs). Instructions `ERRN 0x180` (`ERR`), `ERRR 0x181` (`ERROR n`).
- `ERR$` and `ERL` keep the last error until the run ends. Not caught: Break (ESC) and the step limit of the tests. Syntax errors
  are found before the program runs and are not run time errors.
- **`RESUME`**, **`RESUME NEXT`**, **`RESUME target`** (line number or label) end the handler and go on: `RESUME` runs the statement that failed again
  (after the handler has fixed the cause), `RESUME NEXT` continues behind that statement, `RESUME target` jumps (like `GOTO`). The "statement" is the
  innermost one: in `IF c THEN a ELSE b` it is `a` or `b`, and `RESUME NEXT` continues behind the whole `IF`; in `A=1:B=1/0:C=3` it is `B=1/0`. A `GOSUB`
  handler drops its return address (no `RETURN` after `RESUME`), a `GOTO` handler is armed again. Errors: "RESUME without error" (no error pending, or already
  resumed); **"Cannot RESUME" when the error happened inside a `PROC` / `FUNC` call** (its frame was unwound when the handler was entered; resume from the
  caller is not possible; `RESUME target` works, if the target lies outside the procedure: a jump into its body would find no call frame). `ERL` is the line of the failed statement also after the resume; the stacks are not cleaned up
  further than for the handler itself (the data stack is empty, `GOSUB` return addresses stay).
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

## Game controllers: `JOY`, `ON JOY`, `JOY DEADZONE` *(2026-11-01, window host)*

Up to four game controllers (Xbox, PlayStation, Switch Pro and generic pads; see [controller-support.md](controller-support.md) for what works on a PC and how it is tested). Every pad is
mapped to **one standard layout**, whatever its brand, so a program reads the same names on all of them. A pad keeps its player number while it is plugged in; a pad that is plugged in
takes the lowest free number, the others keep theirs. **A player without a pad reads 0 everywhere** (no error), so a program needs no checks.

```
10 IF JOY("A") THEN PRINT "fire!"           ' player 1 is the default
20 X=X+JOY("LX",2)*4                         ' the left stick of player 2: -1 .. 1
30 PRINT JOY("COUNT");" pads"
40 ON JOY GOSUB 100                          ' a button was pressed or released, a pad was plugged in or out
100 IF JOY("PRESSED") AND 128 THEN PRINT "START"
110 RETURN
```

- **`JOY(name$ [, player])`**, player 1..4 (1 by default; anything else is "Illegal argument", except for `COUNT`, `EVENTS` and `DEADZONE`, which need no player). The name may be in any case and may be a variable.
  - **Buttons** return -1 (held) or 0: `"A"` `"B"` `"X"` `"Y"` (the four face buttons **by position**: A at the bottom, B at the right, X at the left, Y at the top, as on an Xbox pad; on a PlayStation pad
    these are Cross, Circle, Square, Triangle; on a Nintendo pad A and B are *not* swapped), `"LB"` `"RB"` (shoulder buttons), `"BACK"` `"START"` `"GUIDE"` (Xbox: View, Menu and the logo button; PlayStation: Share, Options and the PS button), `"LS"` `"RS"` (the sticks pressed), `"UP"` `"DOWN"` `"LEFT"` `"RIGHT"` (the D-pad), `"SHARE"` (the extra button of some pads: the Xbox Series Share button, the Switch Capture button; see below).
  - **Axes**: `"LX"` `"LY"` (left stick) and `"RX"` `"RY"` (right stick) return -1 .. 1 with the dead zone applied, **up is negative** (screen coordinates); `"LT"` `"RT"` (the triggers) 0 .. 1.
  - `"CONNECTED"` (-1 / 0), `"COUNT"` (how many pads are plugged in), `"BUTTONS"` (the buttons that are held, added up as bits: A 1, B 2, X 4, Y 8, LB 16, RB 32, BACK 64, START 128, GUIDE 256, LS 512, RS 1024, UP 2048, DOWN 4096, LEFT 8192, RIGHT 16384, SHARE 32768), `"PRESSED"` and `"RELEASED"`
    (the same bits for the buttons pressed / released **since the last call**: a quick tap between two calls is not lost), `"EVENTS"` (a counter of button presses and releases and of plugging in and out) and `"DEADZONE"` (the setting).
- **`"SHARE"`** is SDL's standard `MISC1` button where a pad's mapping has it; where it does not (the Xbox Series pad on the kernel's `xpad` driver reports Share as a 12th raw button that SDL's mapping does not use), the **first raw button that the mapping does not use** is taken. A pad without such a button reads 0.
- **`JOY DEADZONE d`** (0 .. 0.9, 0.2 at the start; "Illegal argument" otherwise) sets the dead zone of the sticks for all pads: inside it a stick reads 0, outside it the rest of the way is scaled to the full range (with 0.2, a stick at 0.5 reads 0.375). Triggers have no dead zone.
- **`ON JOY GOSUB|GOTO target`** / **`ON JOY OFF`**: the handler runs when a button is pressed or released or a pad is plugged in or out (**not** when a stick moves: read the sticks). It is an event slot like `ON MOUSE` (the highest priority of the handlers); only what happens after `ON JOY` counts.
- Hosts: the window uses SDL's game controller API (pads without a mapping fall back to the plain joystick API: axes 0, 1 are the left stick, 2, 3 the right one, buttons 0 .. 9 are A B X Y LB RB BACK START LS RS, the first hat is the D-pad). **The console host has no controllers: `JOY("COUNT")` is 0 and everything reads 0.**
  SDL delays the release of the Guide button by 250 ms, so that a quick tap is seen.
- New reserved word `JOY` (an extension word, see "MIN, MAX, ATAN2, SPACE$, STRING$"). Instructions `JOYQ 0x182` (`( name player -- v )`), `JOYD 0x183` (`( d -- )`), `ON JOY` is `COLT` with the parameter bit 8 (`VM_JOYSLOT`), nodes `NT_JOYSTMT 0x88`, `NT_ONJOYSTMT 0x89`.
  The state is `joy.[ch]` (no SDL), the SDL side `sdljoy.[ch]`. Example: `demos/joy.bas`. Tests: `joytest1` (the state), `vmtest1` (the language, 36 cases), `sdltest4 --selftest` (SDL with virtual pads).
- Not done: the pad's name as a string (`JOY$("NAME")`), rumble, a second set of names for the Nintendo labels, motion sensors and touchpads, joysticks in the console host.

## The mouse: `MOUSE` *(2026-11-01, window and console screen mode)*

```
MOUSE SHOW                  MOUSE HIDE                  MOUSE POS 320,150
MOUSE SHAPE HAND            MOUSE TIMEOUT 5             IF MOUSE("B") AND 1 THEN GFX PSET MOUSE("X"),MOUSE("Y")
```

The pointer is the reserved sprite 255 (`docs/graphics-subsystem.md`, "Reserved sprites and the mouse pointer"). It is **not on the screen all the time, so that the text can be
read**: it appears when the mouse moves, a button is pressed or the wheel turns, and when the program executes `MOUSE SHOW`, `MOUSE POS` or `MOUSE SHAPE`; it disappears **3 seconds
after the last of these** (every new one starts the 3 seconds again). The window's own pointer is not drawn above the Engine's window.

| Statement | Effect |
|---|---|
| `MOUSE SHOW` | shows the pointer (for 3 seconds, or the `TIMEOUT`) |
| `MOUSE HIDE` | hides it at once (it comes back when the mouse is used again) |
| `MOUSE POS x, y` | puts the pointer at `x` 0..639, `y` 0..299 (the coordinates of the sprite layer; the real mouse is moved with it where the system allows it; a position beyond the screen is put at its edge) and shows it |
| `MOUSE SHAPE name` | the shape: `ARROW`, `WAIT`, `BUSY`, `TEXT`, `HAND`, `GRAB`, `CROSS`, `SIZEH`, `SIZEV`, `MOVE`, `NO`, `HELP` (the pictures: `sdltest3`); shows the pointer |
| `MOUSE TIMEOUT s` | seconds the pointer stays after its last sign of life, 0..3600, default 3; **0 = it stays until `MOUSE HIDE`** |

| Function | Value |
|---|---|
| `MOUSE("X")`, `MOUSE("Y")` | the position, 0..639 and 0..299 (also while the pointer is hidden) |
| `MOUSE("B")` | the buttons held down now, as bits: 1 left, 2 right, 4 middle |
| `MOUSE("PRESSED")`, `MOUSE("RELEASED")` | the buttons pressed / released **since the last call** of this function (same bits), so a click between two looks of the program is not lost; reading clears them |
| `MOUSE("WHEEL")` | the wheel steps since the last call (up is positive), cleared by reading |
| `MOUSE("SHAPE")` | the shape's number (0 `ARROW` .. 11 `HELP`, in the order above) |
| `MOUSE("VISIBLE")` | -1 if the pointer is shown now, else 0 |
| `MOUSE("EVENTS")` | how many button presses, releases and wheel steps there have been since the program started looking (a counter that wraps at 2^31; `ON MOUSE` watches it) |

**`ON MOUSE GOSUB|GOTO target`** (and **`ON MOUSE OFF`**) runs a handler when something *discrete* happens: **a button is pressed or released, or the wheel turns** (not for motion: that would be
a call per frame; the handler or the main loop reads `MOUSE("X")` and `("Y")` when it wants the position). It works like `ON COLLISION`: the VM looks about every 32 instructions
(and while `WAIT`ing), the handler runs between two statements, never in the middle of an expression, `RETURN` goes back. **It has the highest priority of the timers** (above
`ON COLLISION` and the four `AFTER` / `EVERY` timers, and it interrupts any of their handlers; a running mouse handler is not interrupted). Several events that happen while the
handler is busy or the program is in the middle of a statement make *one* call (like timers whose ticks are dropped): the handler finds what happened in the latched values `MOUSE("PRESSED")`,
`("RELEASED")` and `("WHEEL")`, which collect everything since they were last read, so **a handler should read them** (an unread latch stays set and is reported by the next read; it is
not an event by itself). Only events after the `ON MOUSE` statement count. Like the other handlers it ends with the run (`RUN`, `NEW`, `LOAD`); not during `INPUT` / `GET`.

```
10 ON MOUSE GOSUB 100
20 REPEAT:WAIT 6:UNTIL INKEY$="q"
30 END
100 P=MOUSE("PRESSED"):W=MOUSE("WHEEL")
110 IF P AND 1 THEN PRINT "left click at";MOUSE("X");MOUSE("Y")
120 IF W THEN PRINT "wheel";W
130 RETURN
```

A request is true for the program at once: after `MOUSE POS 5,6`, `MOUSE("X")` is 5, and after `MOUSE HIDE`, `MOUSE("VISIBLE")` is 0, even if the screen has not been redrawn yet (the
screen thread carries the requests out, once per frame, and nothing is locked). The word `MOUSE` is reserved (token 253); `SHOW`, `HIDE`, `POS`, ... are words recognized after it and
stay free as variable names. `MOUSE POS` beyond the screen is put at its edge (the area is 640 x 300 in the window, the terminal's size in the console); numbers that are not positions at all (negative, above 32767) are "Illegal argument". Other arguments out of range are "Illegal argument", too. **Console host and console executables:** it works **in screen mode** on a terminal that reports the mouse (xterm
mouse reporting; a cell is 8 x 12 pixels, so `MOUSE("X")` is `(column-1)*8+4`; the pointer is the cell under it shown with swapped colors; the shape is set as the terminal's own
pointer by OSC 22), see `docs/console-host.md`, "The mouse"; elsewhere (pipes, `--line`, `E1999_MOUSE=0`) the statements give "Not available in the console host" and the functions
"Illegal argument". The demo is `demos/mouse.bas`.

Design decisions I took (revisit if unwanted): the statements and functions are a table in `gfxops.c` like `SPRITE` (one new reserved word); positions are those of the sprite layer
(640 x 300), not of the text screen; `MOUSE POS` beyond the screen is clamped; the buttons are bits (1 left, 2 right, 4 middle); latched
`PRESSED` / `RELEASED` latches and `ON MOUSE` for the discrete events (no event for motion, no filter by button); the timeout is in seconds.

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
