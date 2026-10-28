# BASIC language and tokenized format

The BASIC dialect is classic line-numbered BASIC with extensions (labels, `UNLESS`, multi-line `IF`, `WHILE/REPEAT`
tokens, user functions with `DEF FN`, `DYNAMIC`/`ASSOC` arrays, `SAVE`, shell literals). Only a subset has grammar and code generation
so far (see "Implemented subset").

## Source preprocessing (`preprocess_buffer`)

Collapses runs of blanks/tabs to one space, drops `\r`, upper-cases letters — everywhere except inside `"…"` and `` `…` ``.
Identifiers and keywords are therefore case-insensitive; string and shell literals keep case.

## Tokenized line (`tokenize_line` / `detokenize_line`)

`tokenize_line( text, buf_t* out, linehdr_t* )` appends the line to a growable buffer; `detokenize_line( buf_t* out, tokens, hdr )` rebuilds source text. Readers use the bounded `rbuf_t` and return heap strings through `char**`.

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
calls, `PRINT`, `SAVE`, assignment/`LET`, substring assignment, `DIM`, `ERASE`, `DEF FN` (single- and multi-line), `FOR/NEXT`, `GOTO/GOSUB/RETURN/LABEL`, `IF/UNLESS` and statement lists. `RUN`, `LIST`, `LOAD`, `NEW` and `CLEAR` are statements too (below). `REPEAT/UNTIL`, `WHILE/WEND`, `UNTIL/UEND`, `AGAIN [n]`, `END`, `POP` and `ON n GOTO/GOSUB` are implemented too (see compiler-and-vm.md for the loop rules), and so are `DATA`, `READ` and `RESTORE [line|label]` (data items are signed numeric literals and string literals; no unquoted text), `INPUT` and `GET` (below). Keywords that have a token but no grammar yet (`PUT`, `AFTER`, `EVERY`, `ON`/`OFF` for timers, `SYMBOL`, `RUN`, `NEW`, `LIST`, `LOAD`, …) are not statements of the language yet; `AFTER`, `EVERY` and `SYMBOL` are planned to work through host callbacks.
Some grammar items are inconsistent in the EBNF (e.g. `sys-noarg-num-call := sys-no-arg-num`, a name that is not defined) — check the code before relying on it.

### INPUT and GET

    INPUT [#ch,] ["prompt" (;|,)] var {, var}
    GET   [#ch,] var {, var}

- `INPUT` shows the prompt (a plain string literal, or several), then `? ` if the prompt is followed by a semicolon or if there is no prompt; a comma after the prompt suppresses the `? `. It reads one line and distributes its comma-separated fields to the variables (plain or array elements, numeric, integer or string). A field starting with `"` extends to the closing quote, so it may contain commas. Numeric fields may be surrounded by blanks; an empty numeric field is 0; unquoted string fields are trimmed.
- A missing field or a non-numeric number prints `?Redo from start` and asks again (prompt included); variables already stored stay stored. More fields than variables print `?Extra ignored`. End of input (host callback returns nothing) is the error "End of input".
- `GET` waits for a key for every variable: a string variable gets the one-character string, a numeric variable the key code. (`INKEY$` is the non-waiting variant.)
- Without `#ch` both use channel 0. The channel is passed to the host callbacks (`vm->input`, `vm->getkey`); the VM's output channel (`PCHN`) is set to it too, so the prompt goes to the same channel.

## Program commands: `LOAD`, `RUN`, `LIST`, `NEW`, `CLEAR`

They are ordinary statements (`bascomp.ebnf`, `cgenprog.c`), so they work in direct mode and inside programs.

- `LOAD name$` — replaces the stored program entirely by the one in the file and **runs it from the beginning** (variables cleared). The file may be a binary `SAVE` file (recognized by its magic, see `baspgm.h`) or a text listing (lines with line numbers, e.g. `SAVE "X",A`). If the file can't be read or is bad, an error is reported and the old program stays. When used inside a program, the running program ends and the loaded one starts.
- `RUN [target]` — clears all variables and runs the stored program from the beginning, from a line number (`RUN 100`) or from a label (`RUN START`). Inside a program it restarts the program (a program ending with `RUN` runs forever; Break stops it). The target must exist ("Undefined line number N", "Undefined label X").
- `LIST [a][-[b]]` — lists the program to the output channel (always channel 0 for now): `LIST` all, `LIST 20` that line only, `LIST 20-50` a range, `LIST 20-` from line 20 to the end, `LIST -50` from the start up to line 50. Each of `a`/`b` is a line number or a label (a label stands for the line holding its `LABEL` statement); a label that doesn't exist is an error.
- `NEW` — deletes the program **and** all variables (the runtime is reset, so user functions are gone, too) and returns to the host (the READY prompt). Inside a program it ends the run: nothing after it is executed.
- `CLEAR` — empties all variables and goes on: numbers become 0, strings `""`, arrays are emptied (they keep their dimensions: arrays exist from parse time, so there is no un-dimensioning). The program, labels and user functions are not touched, and neither is the DATA pointer. Usable in programs; mind that the control variable of a running `FOR` is cleared too.
- `DELETE`, `RENUM`, … have no grammar yet.

`RUN` and `LOAD` need a restart of the whole compiled program, so the VM doesn't do them itself: it stops and leaves a request for the host (`docs/compiler-and-vm.md`).
