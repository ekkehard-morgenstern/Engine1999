# Library support — design

Status: **implemented (2026-11-03, steps 1 to 6; this is step 7, the documentation pass).** Owner's request, highest priority on the roadmap (item 48). The sections 1 to 6 are the design as built; 7a to 7c record what each step does; a short user guide is section 9. This
document replaces the first sketch in `docs/roadmap/language.md` section 16 after the owner's decisions of the same day:

- Programs load libraries with **`LOAD LIBRARY "name"`** (the AmigaBASIC word), not with a new word like `USE`.
- **At the prompt the user never has to type it**: `RUN` takes care of the libraries a program names.
- (From the first proposal) a library is an ordinary BASIC program whose first statement is `LIBRARY "name"`; the compiler is
  in *library mode* for it; only `FUNC` / `PROC` and labelled sections are allowed; a library cannot be run.

Goal: share code between programs (the demos first: pad and keyboard input, text and banner helpers, sprite grabbing, the high
score file code) without copying it into every listing.

## 1. What the programmer writes

A program names the libraries it needs, near its top, each on a line of its own:

```
10 LOAD LIBRARY "sprites"
20 LOAD LIBRARY "pads"
30 CALL SP_INIT
40 ...
```

A library is a file like any other program (ASCII `.bas` or binary `.e99`; written with `SAVE`):

```
10 LIBRARY "sprites","SP_"
20 PROC SP_INIT
30   DIM SP_X(8)
40 END PROC
50 FUNC SP_FREE%()
60   ...
70 END FUNC
```

- **`LIBRARY "name" [, "prefix"]`** must be the first statement of the file (comment lines, such as the four license lines of
  the demos, may come before it). It names the library and gives the variable prefix (section 5). A file without it is an
  ordinary program.
- **Everything else in a library** is `PROC … END PROC`, `FUNC … END FUNC`, `DEF FN`, `LABEL name` sections (entered with `GOSUB name`),
  `LOAD LIBRARY` (a library may need other libraries), and comments. Any other statement outside of a procedure or function is
  the compile error "Not allowed in a library" with its line. `DATA` is allowed (section 4a). A library has no code of its own that runs: it cannot be run
  (`RUN` of a stored library gives "Library cannot be run").
- **Editing a library** is editing a program: `LOAD "sprites"` (plain `LOAD`), `EDIT`, `LIST`, `SAVE "sprites"` work as always.
  Nothing marks the file except its first statement, so **the file format does not change** (no new header version, nothing
  breaks the rule that saved programs always load).
- **Extensions (owner's decision):** `.lib` (ASCII listing) and `.l99` (binary) are **optional** extensions for libraries.
  **`SAVE LIBRARY "name" [,B]`** writes them (`name.lib`, with `,B` `name.l99`; a stored program whose first statement is not
  `LIBRARY` gives "Not a library"); a plain `SAVE "name"` writes the regular `.bas` / `.e99`, also for a library. What makes a file a
  library is only the `LIBRARY` statement inside it, never its name. `SAVE LIBRARY` is `SAVE` followed by the extension word
  `LIBRARY`. `BUILD`'s refusal to overwrite a program file covers `.lib` and `.l99`, too.
- The two words: `LIBRARY` is a new **extension word** (token 254 + an id byte; the one-byte token space is full), `LOAD LIBRARY`
  is `LOAD` followed by it. `LIBRARY` is therefore a reserved word.

## 2. When it takes effect: the compiler, not the run time

The compiler needs a `FUNC` before it parses the lines that call it (a `FUNC` must precede its callers), so `LOAD LIBRARY` cannot
be a statement that *executes*: by then the program has been compiled. It is a **declaration that the compiler resolves**:

1. `RUN` (and every compile of the stored program: `repl_compile()`, the monitor's `c`, `BUILD`) first **scans the program's
   tokens** for `LOAD LIBRARY "literal"` (a pre-pass over the stored lines, token level: `TOK_LOAD`, `XF_LIBRARY`, a string
   literal). The name must be a string literal (not an expression) and the statement must stand unconditionally (not inside
   `IF`, a loop, a procedure): else "LOAD LIBRARY must be a plain statement with a name in quotes".
2. Each named library is **found and read** (section 3) into a scratch program, checked (first statement is `LIBRARY`, library mode
   rules), and scanned for its own `LOAD LIBRARY` lines. The dependencies form a list in **dependency order** (a library comes
   before the libraries and the program that need it); a library is loaded once however often it is named; a cycle is the
   error "Library cycle (a, b, a)".
3. The libraries are **compiled one after the other into the same code and data segments, then the program last** (the same way
   direct mode lines are compiled behind the code that exists). Because the library code is compiled first, every library `FUNC`
   is known when the program's callers are parsed, and no relocation is ever needed.
4. At run time the `LOAD LIBRARY` statement does **nothing** (the work was done at compile time). It is not an error to reach it.
   Typed at the prompt it is an error ("LOAD LIBRARY belongs in a program"), because a direct line has no program to attach the
   libraries to.

The compiled libraries stay in the code segment after the run, so at the prompt after `RUN` the user can `CALL SP_INIT`, test a
`FUNC`, use `MONITOR`. `NEW`, `LOAD` and the next `RUN` throw them away with the rest of the code. **There is no state that
survives `NEW`: nothing to unload, nothing to list; what the program names is what is there.** (`LIBRARIES`, a statement that
lists the libraries of the last compile with their files, is a cheap extra.)

## 3. Finding the file

`LOAD LIBRARY "sprites"` follows the rules of `LOAD` for extensions, with the library extensions first: a name without an extension is
tried as it is, then with `.lib`, `.l99`, `.bas`, `.e99` and the upper-case variants (plain `LOAD` gets `.lib` / `.l99` appended to its
list as well, so `LOAD "sprites"` finds a library for editing). It looks in this order, the first hit wins:

1. the current directory (relative to where the program was started or `CHDIR`ed),
2. a `lib/` directory below it,
3. the directory of the stored program's own file (`repl_t.filename`) and its `lib/`,
4. (a program in a folder called `tools`: first the `lib/` folder next to it and, in the source tree, `../demos/lib`; roadmap item 70)
5. the installed libraries (`baslib_system_dirs()`): `$ENGINE1999_HOME/lib`, `<program>/../lib/engine1999/lib`, `/usr/local/lib/engine1999/lib`, `/usr/lib/engine1999/lib`.
   *(As built: `make install` puts the demos' libraries into `$(PREFIX)/share/engine1999/demos/lib`, beside the installed demos, where step 3 finds them for an installed demo; a library of your own that should be found from anywhere goes into one of the directories above.)*

The name may not be absolute or contain `..` (downward only; the owner's default I/O policy, roadmap item 54 and section 17), except that
step 4 is the system's own directory. Driver names (`mem:`, `res:`) work as for `LOAD` (so the tests need no files). A library that
is not found is the compile error "Library not found: sprites" in the line of the `LOAD LIBRARY`.

## 4. Names, line numbers, errors

- **Names are global:** `PROC`, `FUNC`, `DEF FN` and label names of all libraries and the program share one namespace. A clash is
  the compile error "Duplicate name X (library Y)" naming both places (as `MERGE` already refuses a duplicate label). The prefix
  convention (`SP_INIT`) is what keeps libraries apart; a later extension could add qualified names.
- **Line numbers are local to a library.** A `GOTO 120` or `GOSUB 300` inside a library means the library's own line (the line
  table is kept per module, as the direct mode already keeps its own). Libraries and programs can therefore number freely.
  `RENUM` of a library (loaded as a program) works as for any program.
- **Errors at run time** report the line **and the library**: `? Illegal argument in line 120 (library sprites)`. The VM's `LINE`
  instructions carry the line number only; a module table (code address ranges -> library name, built by the compile) gives the
  name for the message, `ERL` stays the number, `ERR$` unchanged.
- **Calls go one way:** the program calls library procedures and functions freely, and a library may call the libraries it names. A library can
  **not** call a `PROC` or `FUNC` of the program (the libraries are compiled before the program is parsed, so the name does not exist yet:
  "Undefined procedure"). *(Correction 2026-11-03: the first sketch said a library `PROC` could call a program `PROC` by name; it can't.)* A
  library that needs a callback takes the work as data (a line number or label for `GOSUB`), or a later version adds forward declarations.

## 4a. `DATA` in libraries (owner's decision)

`DATA` is allowed in a library. **Unlabelled `DATA` belongs to the library: only the library's own code reads it.** The data
segment keeps **one chain of DATA records per module** (the program is a module, each library is a module; today there is one
chain for the program), and the VM keeps **one READ pointer per module**:

- A `READ` in code of module M reads M's pointer, so a library's `READ` never sees the program's data and the program's never sees
  the library's, whatever the order of the calls.
- `RESTORE` inside a library without an argument or with a library line number / label restores the library's own chain; the program's
  `RESTORE` the program's.
- **Labelled DATA is public:** a `DATA` line that has a label can be named from anywhere with `RESTORE label`; the pointer of the module that
  executes the `RESTORE` is then set to that record and `READ` goes on along the chain's own links. (A label is a global name,
  like a procedure's; a label on a `DATA` line in a library is how it publishes a table.)
- A run starts every module's pointer at the first record of its chain (the library's pointers are reset at `RUN` like the program's).
- Implementation note: `cgen_datarec_next()` / `datafirst` / `datalast` and `vm.datapos` become per-module (a small table indexed by
  the module number that the compile already assigns for line tables); direct mode keeps working on the program's chain.

## 5. Variables — the real problem

Variables are global by name: a library's `I` or `T$` would overwrite the program's. Three rules, the first two by convention, the
third checked:

1. A library **declares every temporary `LOCAL`** (saved and restored per call: this works today).
2. A library's **persistent state lives in variables that start with the library's prefix** (`SP_X(8)`, `SP_COUNT`), the second
   argument of `LIBRARY`.
3. **Compile check in library mode (owner's decision: an error, a warning would be ignored):** a variable that is neither a
   parameter, nor `LOCAL`, nor starts with the prefix is the compile error "Variable I is global in library sprites (use LOCAL or the
   prefix SP_)" at the line where it occurs. A library without a prefix can therefore use only parameters and `LOCAL` variables (no
   state of its own). The prefix may not be the empty string. Arrays: a library that needs `DIM`s provides an **init procedure named `<prefix>INIT`** (`SP_INIT` above) because
   no code may stand outside a procedure; **the runtime calls it** before the first line of the program, for every library
   that defines one, in dependency order (so the program never forgets it; it may also call it again to reset).

## 6. Libraries and `BUILD`

An executable made with `BUILD` must contain the libraries its program uses, else it could not run alone. **Done (step 5, 2026-11-03):** `BUILD` resolves the libraries of the stored program like `RUN` does (a missing or wrong one stops
the build with the same message) and embeds a **bundle** instead of the plain program file. A program without libraries is embedded as before (the plain binary program file), so nothing changes for it and executables made earlier still run.

```
bundle := 89 'E' 'B' 'N'   version.8 (1)   count.16   entry { count }                 (all numbers big endian)
entry  := kind.8 (0 = the program, 1 = a library)   namelen.16   name   length.32   a binary program file image (89 'E' '9' '9' ...)
```

The first entry is the program (empty name), the others the libraries in dependency order, named by the name they declare. The bundle is new and versioned (the program file format itself does not change, and old files load as before). In the running
executable `repl_load_image()` recognizes the magic, loads the program and keeps the libraries (`repl_t.embedlibs`, `embedded`); `LOAD LIBRARY` is resolved from them only (by the declared name: a request like `"lib/gfx.lib"` or `"mem:gfx"` is reduced to
`gfx`), never from the disk; a name that is not in the bundle is "Library not found". `engine1999 --engine1999-about` and the license rule are unchanged; a library's license lines travel with it. Window and console executables share the code.

## 7. Implementation plan

**Status (2026-11-03): all seven steps are done.** Step 1: the words and their grammar; step 2: `baslib.[ch]`, the resolver, and its hook in the REPL; step 3: the libraries are compiled ahead of the program (7a); step 4: `SAVE LIBRARY`, `LIBRARIES`, the direct-mode error (7b); step 5: the `BUILD` bundle (section 6); step 6: the shared libraries of the demos (7c); step 7: this documentation pass.

1. **Extension word `LIBRARY`** (`XF_LIBRARY`), parser rules for `LIBRARY "name"[,"prefix"]` and `LOAD LIBRARY "name"` (new node
   types; no code is generated for either: `NOP`), `bascomp.ebnf` + `make grammar`, man page, `docs/basic-language.md`.
2. **`baslib.[ch]`** (DONE; no SDL; the logic of sections 2 and 3): the pre-scan of a stored program, finding and reading a file through
   the I/O drivers (`repl_open_program()` is reused), the dependency order, the cycle and clash checks; unit tests in `repltest1`
   with `mem:` files.
3. **Library mode in the compiler** (DONE) (`compiler_t.library`: the statements allowed at top level; the variable check; per-module DATA
   chains and READ pointers, section 4a) and the **multi-module compile in `basrepl.c`** (`repl_compile()` / `repl_execute()`: modules compiled in order, a
   line table per module, the module table for error messages, the start address of the program, the init calls).
4. **`RUN` of a library** and the plain-`LOAD`-works rule; direct mode `LOAD LIBRARY` error; **`SAVE LIBRARY`** with `.lib` / `.l99`; **`LIBRARIES`** (DONE).
5. **`BUILD` container** and the runtime's lookup of embedded libraries (DONE).
6. **First libraries for the demos** (`demos/lib/`: `util`, `input`, `hiscore`, `text`; the games shrank by about 300 lines) (DONE). *Survey and what was done: section 7c.*
7. Tests: `libtest1` (47), `vmtest1`, `repltest1`, `buildtest1`; `make install` installs the shared libraries; roadmap, `SESSION.md`, man pages, grammar (DONE).

## 7a. What step 3 does, exactly (2026-11-03)

- **Compile order** (`repl_compile_libraries()` in `basrepl.c`, called by `RUN` and the monitor's `c` inside the protected compile): each library of `repl_t.libs` is compiled in dependency order into the same code and data
  segments, then the program. A library's compile has its own line table, fixups and DATA chain (`cgen_module_begin()` / `cgen_module_end()`: the library name, prefix,
  code range and DATA chain are kept in `codegen_t.modules`), the statement table for `RESUME` keeps growing across modules (`codegen_t.keepstmts`). The program starts at
  `codegen_t.mainstart`; `RUN` begins there (and `RUN label` / `RUN line` find their targets as before). Line numbers in messages: a compile error is
  "? text in line N of the library X" (`compiler_t.libname`), a run time error the same (`cgen_module_at( steppc )` in `vm_report_error()`); `ERL` stays the number.
- **Library mode of the code generator** (`cgen_check_library_line()` in `codegen.c`, `cgenlib.c`): only `PROC` / `FUNC` / `DEF FN` / `LABEL` / `DATA` / `LIBRARY` /
  `LOAD LIBRARY` may stand at the top level; any other statement only inside a labelled section (from a `LABEL` to the next `RETURN`), else "Not allowed in a library: code
  outside of a PROC, FUNC or labelled section" (and "RETURN outside of a labelled section"). The variable rule (section 5, an error): in a `PROC`, `FUNC` or `DEF FN` only
  parameters, `LOCAL` variables, the compiler's hidden `@` variables and names that start with the prefix; in a labelled section only prefixed names; "Variable X is global in
  the library (declare it LOCAL, or start its name with P_)" with the line (a library without a prefix: "...; the library has no prefix").
- **Name clashes:** a `PROC`, `FUNC`, `DEF FN` or label that an earlier module defined is "Duplicate name X (also defined in the library L)" (`cgen_defined_elsewhere()`).
- **`<prefix>INIT`:** for every library with a prefix that defines a `PROC` of that name the program starts with `PHIM 0 / CALF` of it, in dependency order, then a `BRIA` to the
  real start (`codegen_t.initjump` / `initnext`; patched before every run, so `RUN 30` initializes the libraries first, too). It must take no parameters.
- **DATA** (section 4a): every module has its own chain (`cgen_module_begin()` resets `datafirst` / `datalast`; the program's chain is the one in `codegen_t`); the VM keeps a READ pointer
  per module (`vm_t.moddata`, `vm_datapos_ref()`: the module of the executing instruction decides; `vm_reset_data()` at the start of a run); `RESTORE` without a target restores the
  module's own chain; the labels of the libraries stay known to the modules after them (`codegen_t.publabels`), so `RESTORE label` of a library's labelled DATA works from the program.
- **`RUN` of a library:** "Library cannot be run (m is a library: a program names it with LOAD LIBRARY)".
- **Tests:** `repltest1` (23 cases: calls, local line numbers, error messages with the library, library mode, variable rule, duplicates, `INIT`, DATA private / `RESTORE` / public label,
  two runs, nested libraries, a program without libraries), all suites pass, clean under valgrind (`repltest1`) and ASAN / UBSAN (`vmtest1`, `repltest1`, `libtest1`).
- **Known limits:** the monitor lists the libraries' code like any other code (no library names in the disassembly); a library cannot call the program's procedures (section 4).

## 7b. What step 4 does, exactly (2026-11-03)

- **`SAVE LIBRARY [name$] [,A|B]`** (the extension word `LIBRARY` after `SAVE`; the mode string of the `SAVE` instruction starts with `L`; `vm_t.savemode` bit 4): writes the stored program as a library with
  the library extensions, `name.lib` (ASCII, the default) or `name.l99` (`,B`); a name with an extension is used as it is. The stored program has to be a library, else "Not a library (its first
  statement must be LIBRARY "name")" and "Save failed". Without a name the remembered file is used with its standard extension (`.bas .e99 .lib .l99`) replaced by the library extension of the format
  (the remembered file's format without an option). **Plain `SAVE`** writes the regular extensions, also for a library; with a remembered `.lib` / `.l99` and no name it keeps the name and the format;
  an explicit option swaps `.lib` <-> `.l99` (and `.bas` <-> `.e99`). What makes a file a library is only its `LIBRARY` statement. `LOAD` finds `.lib` / `.l99` (after the old four extensions); `BUILD`
  refuses to overwrite them like `.bas` / `.e99`.
- **`LIBRARIES`** (extension word `XF_LIBRARIES` 10, `NT_LIBRARIESSTMT 0x8E`, instruction `LIBS 0x187` `( name -- )`, `vm_t.libraries`, `repl_vm_libraries()`): `LIBRARIES` prints the libraries of the last compile
  (`repl_t.libs`: name, prefix, file; "No libraries ..." if there are none; `NEW` and a program without libraries empty the list), `LIBRARIES "name"` the `PROC`s, `FUNC`s (with their parameters, array
  parameters as `A()`, a `DEF FN` counts as a `FUNC`) and `LABEL`s that library defines, in the order of its code; an unknown name is "Library not loaded: name".
- **`LOAD LIBRARY` at the prompt** is the error "LOAD LIBRARY belongs in a program (RUN loads the libraries a program names)" (a direct line has no program to attach the libraries to; the check is in the code generator:
  a statement with no line number). In a program line it is accepted wherever (the checks of `baslib` apply).
- Tests: `repltest1` (14 cases), `vmtest1` (6), grammar, man page, `mancheck.sh --update` (`LIBRARIES` is a reserved word now).

## 7c. Step 6: survey of the demos and proposal for the first libraries (2026-11-03)

**What was looked at:** the PROCs and FUNCs of `orchard.bas`, `orchard2.bas`, `climb.bas` and `madrun.bas` (the four big games; `orchard.bas` has no procedures at all, it is the old line-number style and stays as it is), compared by name and by similarity; the small demos
(`gfx`, `joy`, `keys`, `mouse`, `sfx`, `ode`, `prelude`) have nothing worth sharing.

**What is duplicated** (identical = byte for byte, apart from the line numbers):

| Code | In | Lines each | Notes |
|---|---|---|---|
| `RN(M)`, `CLAMP(V,LO,HI)`, `PAD$(N,W)` | climb, orchard2, madrun | 3 + 5 + 3 | identical in all three |
| `DISK(x,y,r,c)` (filled circle) | climb, orchard2 | 7 | identical but for the parameter names |
| `PLAYSEQ(NT(),N,CH,BT,VL)` | climb, orchard2 | 6 | identical |
| `TXTAT(x,y,t$,c)` | climb, orchard2, madrun | 5 | differs in the paper (`COLOR c` / `COLOR c,210`) |
| `BIGTEXT(y,t$,c,s)` | climb, orchard2 | 6 | differs in the shadow color (2 / 11) |
| `SHUTDOWN` (`SOUND OFF`, `GFX RESET`, `CLS`, a thank-you line) | climb, orchard2, madrun | 6 | differs in the title |
| `KEYS` + `PAUSE` (arrows / WASD / space, pad stick, A, START edge for pause, BACK / Q to quit, P pauses) | climb, orchard2, madrun | 20 to 60 | the same logic; the games add their own keys (climb: UP / DOWN climb, Z / X drill) |
| `ANYSTART` / `ANYBACK` / `ANYA` (a button on any of four pads) | madrun | 24 | the other games read pad 1 only |
| the high score file (`ON ERROR GOTO`, `OPEN`, `INPUT #`, `CLOSE`, two handler labels in the main program) | climb, orchard2, madrun | ~22 | the same sequence |

About 330 lines in all; more important than the lines is that a fix (the pause box bug of `climb.bas` was exactly this kind) has to be made three times.

**Proposed libraries** (file names in `demos/lib/`, written with `SAVE LIBRARY`; found from a demo in `demos/` through the program's own `lib/`):

1. **`util`** (no prefix needed for the three identical one-liners: **keep their names** `RN`, `CLAMP`, `PAD$` so that no caller changes; the rule only constrains *variables*): `RN`, `CLAMP`, `PAD$`, `DISK`, `PLAYSEQ` (array parameter). The smallest and safest first step: it proves the whole chain (`LOAD LIBRARY` in a demo, `BUILD`, `make install`) on code that cannot go wrong. Each demo drops its own copy (a copy left behind is "Duplicate name").
2. **`input`** (prefix `IN_`): the common part of `KEYS` / `PAUSE`: `IN_POLL` sets `IN_DX`, `IN_DY`, `IN_FIRE`, `IN_START` (edge), `IN_BACK`, `IN_QUIT` from the keyboard and pad 1 (WASD, arrows, space, stick, D-pad, A); `IN_PAUSE` runs the pause loop (P / START to go on, Q / BACK to quit) and **returns** instead of drawing, so the game puts up its own box (a library cannot call the game's `TXTAT`); `IN_ANYSTART`, `IN_ANYBACK`, `IN_ANYA` over four pads (the button name of `JOY()` has to be a literal, so one function per button, as in `madrun.bas` today); `IN_PADS` (how many pads are connected). The games keep their extra keys in a few lines of their own.
3. **`text`** (prefix `TX_`): `TXTAT` with the paper as an argument, `BIGTEXT` with the shadow color as an argument, `TX_CENTER`, `SHUTDOWN` taking the title as an argument; the banner helper of `madrun.bas` (`BANNER` / `CENTER` on the graphics layer) if the other games want it.
4. **`hiscore`** (prefix `HS_`): `HS_LOAD(file$)` and `HS_SAVE(file$, value)`, **blocked by a language gap, see below**.

**Not worth sharing:** the art (`DEFSPRITES`, `DEFTILES`, `MAKEIMAGES`; game specific, mostly `DATA`), `SOUNDS` (each game has its own effect table), `INIT`, `TITLE`, `PLAYGAME`, `HUD`, `GAMEOVER`, the traffic simulation and its drawing code (`madrun`), the tower map (`climb`), `LOADPAL` (see below). Possible later: the particle code of `orchard2` (explosions) and `climb` (sparks) if they turn out the same on a closer look, and `GRABSPRITE` of `madrun` (`GFX GRAB` + `SPRITE DEF`).

**What the library rules mean for the extraction** (found while surveying, each checked against the implementation):
- A library **cannot call the game's procedures** (it is compiled first). Anything that needs the game's drawing (the "PAUSED" box, the title screen) stays in the game; the library returns a value or takes the colors as arguments.
- A library's **`READ` reads its own `DATA`**: `LOADPAL` (palette entries from the game's `DATA` rows) cannot move into a library; a shared `PAL_SET( layer, base, ARR() )` taking the colors in an array could, at the price of the games filling the array.
- **State becomes prefixed variables** (`IN_DX`, `HS_...`): the games read them directly; the library has no top-level code, so the arrays it needs are `DIM`ed in `IN_INIT`, which the runtime calls before the program starts.
- **Timer handlers** (`EVERY` / `AFTER ... GOSUB label`) can name a label *of the same library*; the games' music handlers call the game's own `BAR` and stay in the games.
- **High score: not possible today.** The sequence `ON ERROR GOTO handler` ... `RESUME label` only works at the program's top level: an error inside a `PROC` or `FUNC` unwinds its frame, so `RESUME label` lands in code whose `RETURN` finds no frame ("Return stack underflow", reproduced with a prototype `HS_LOAD`). The programs get away with it because their file code is in the main program. A library needs a way to test for a file without raising an error: **an `EXISTS(file$)` function** (`-1` / `0`, through the I/O drivers like `OPEN`), or errors that can be resumed inside procedures. I would add `EXISTS()` (a small, useful word on its own: it is roadmap item 59 now); `HS_LOAD` is then `IF EXISTS(F$) THEN OPEN ...`. *Decision for the owner.*

**Done 2026-11-03: `util`** (`demos/lib/util.lib`: `RN`, `CLAMP`, `PAD$`, `DISK`, `PLAYSEQ`, names unchanged, no prefix): `orchard2`, `climb` and `madrun` load it with `LOAD LIBRARY "util"` (line 6) and lost 59 lines of copies; checked in the window under Xvfb (each game started and played: score, high score from its file, planet / moon, music) and `orchard2` built with `BUILD` and run from another directory (the library came out of the bundle); `make install` installs `demos/lib/*.lib` next to the installed demos (found through the program's own `lib/`). **`EXISTS(file$)`** is done (roadmap item 59), so `hiscore` is possible next.

**Done 2026-11-03: `input`** (`demos/lib/input.lib`, prefix `IN_`; `IN_POLL`, `IN_SERVICE`, `IN_PAUSELOOP`, `IN_ANYSTART` / `IN_ANYBACK` / `IN_ANYA`; `IN_DX`, `IN_DY`, `IN_BTN`, `IN_QUIT`, `IN_KEY`, `IN_PSK`). `orchard2`, `climb` and `madrun` lost their `KEYS` / `CTRL` bodies and `PAUSE` loops (88 lines of the games gone, 63 added in the library and shared): each game keeps only what is its own (the pause box, `QT` / `GQ`, the extra keys: `Z` / `X` drilling and `M` in `climb`, the per-player movement of `madrun`). Differences from the old code: START and BACK count on **any of the four pads** (before: pad 1 in `orchard2` and `climb`), a pause request is handled after the keys of the frame have been read. Checked in the window under Xvfb for each game (play, `P` pause with the box, `P` resume, `Q` quit) and `madrun` as a `BUILD` executable run from another directory (both libraries in the bundle).

**Done 2026-11-03: `hiscore`** (`demos/lib/hiscore.lib`, prefix `HS_`: `HS_LOAD(file$)`, `HS_SAVE(file$, value)`; `EXISTS`, `EOF`, `LINE INPUT`, `VAL`, so a missing, empty or garbage file is 0 and never an error). The games load the high score with one line and save it with one; a file that cannot be *written* is still ignored by the handler that stays in each game's **main program** (`ON ERROR GOTO` ... `RESUME <label>` can only continue there, not inside a `PROC`; the library says so in its header). **Found on the way (engine bug, fixed):** a `FUNC` that used a file channel (`LINE INPUT #1`, `PRINT #1`) left that channel selected for the statement that called it, so `PRINT FN HS_LOAD(...)` printed to the closed file ("File not open"); the VM saves the current channel in the call context and restores it on return (`vm_context_t.channel`; 2 `vmtest1` cases).

**Done 2026-11-03: `text`** (`demos/lib/text.lib`, prefix `TX_`: `TXTAT`, `BIGTEXT`, `TX_BYE`; the variables `TX_PAPER` (default 210) and `TX_SHADOW` (default 2) are set by `TX_INIT`, which the runtime calls before the program, and `orchard2` assigns `TX_PAPER=0:TX_SHADOW=11` in its `INIT`). `BIGTEXT` and `madrun`'s `CENTER` were the same procedure (14 calls renamed), `SHUTDOWN` became `CALL TX_BYE("title")`. **All four libraries of the survey are done** (`util`, `input`, `hiscore`, `text`); the three games lost about 300 duplicated lines in all. Left in the games on purpose: the art, the sound tables, `INIT`, `TITLE`, `PLAYGAME`, `HUD`, `GAMEOVER`, `BANNER` (madrun), `LOADPAL`.

**Order of work** (each step ends with the demo running as before: the headless runners and the window under Xvfb, the same checks as when the games were written): `util` (three demos), then `input` (three demos plus the pad helpers; the biggest gain), then `text`, then `hiscore` once `EXISTS()` is there. After each library: `make install` must install `demos/lib/` (a line in the makefile), `BUILD` of each demo is tried (the bundle carries the libraries), the README of the demos gets a section on the libraries.

## 9. User guide in short

- **Use a library:** `LOAD LIBRARY "util"` near the top of the program, one per line; never at the prompt. `RUN` finds the file, compiles it before the program, calls `<prefix>INIT`. `LIBRARIES` shows what was loaded, `LIBRARIES "util"` what it defines.
- **Write one:** start a program with `LIBRARY "name","PFX_"`; only `PROC`, `FUNC`, `DEF FN`, `LABEL` sections, `DATA` and `LOAD LIBRARY` at the top level; variables are parameters, `LOCAL` or start with the prefix; state is set up in `PFX_INIT`; then `SAVE LIBRARY "name"` (`name.lib`, with `,B` `name.l99`). Put it in the program's directory or its `lib/`.
- **Share it:** `BUILD` embeds the libraries a program names, so the executable runs alone.
- **The shipped libraries** are in `demos/lib/` (list in `demos/README.md`): `util`, `input` (`IN_`), `hiscore` (`HS_`), `text` (`TX_`).
- **Limits:** a library cannot call the program's procedures; `RESUME label` cannot continue inside a `PROC` / `FUNC` (use `EXISTS()`); names are global (prefix them).

## 8. Decisions (owner, 2026-11-03)

- **(a)** The variable check is an **error**, not a warning. The prefix argument is optional (a library without one has no state of its own).
- **(b)** **Line numbers are local** per library.
- **(c)** The runtime **calls `<prefix>INIT` automatically** before the program starts.
- **(d)** The **search path** of section 3 is accepted.
- **(e)** **`DATA` is allowed** in libraries; unlabelled DATA is private to the library, labelled DATA can be reached with `RESTORE label` (section 4a).
- **(f)** **`.lib` / `.l99`** are optional library extensions written by `SAVE LIBRARY`; the `LIBRARY` statement alone makes a library.
- **(g)** A statement **`LIBRARIES`** lists the libraries of the last compile (name, file, prefix).

- **Message wording:** left to me by the owner. **`LIBRARIES name`** also lists the procedures and functions the library defines
  (the owner agreed, 2026-11-03); plain `LIBRARIES` lists the libraries of the last compile (name, file, prefix).
