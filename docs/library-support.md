# Library support — design

Status: **design settled (2026-11-03: the owner's answers to the open points are worked in below), nothing implemented.** Owner's request, highest priority on the roadmap (item 48). This
document replaces the first sketch in `docs/roadmap.md` section 16 after the owner's decisions of the same day:

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
4. the installed libraries: `$(PREFIX)/lib/engine1999/lib/` next to the kit (`make install` puts the demos' shared libraries
   there, read-only).

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
- **Calls both ways:** a library `PROC` may `CALL` a `PROC` that the program defines (a callback by name: `CALL` is resolved late
  by the code generator). A library `FUNC` cannot call a program `FUNC` (it is compiled first, so it is unknown to the parser).
  Programs call library procedures and functions freely.

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

An executable made with `BUILD` must contain the libraries its program uses, else it could not run alone. The embedded program is
today a program file; the executable gets the program **plus the resolved library files as additional modules** (the embedded
payload becomes a small container: one entry per module, each a normal program file: nothing in the program file format changes,
the container is new and versioned from the start). The runtime resolves `LOAD LIBRARY` in an embedded program from the
container, not from the disk. `engine1999 --engine1999-about` and the license rule are unchanged; a library's license lines
travel with it.

## 7. Implementation plan

**Status (2026-11-03): steps 1 and 2 are done** (step 1: the words and their grammar; step 2: `baslib.[ch]`, the resolver, and its hook in the REPL: `RUN` and the monitor's `c` find the libraries of the stored program and report what is wrong, **but the libraries are not compiled yet**, so a program that names a library that exists still does not see its `FUNC`s: that is step 3).

1. **Extension word `LIBRARY`** (`XF_LIBRARY`), parser rules for `LIBRARY "name"[,"prefix"]` and `LOAD LIBRARY "name"` (new node
   types; no code is generated for either: `NOP`), `bascomp.ebnf` + `make grammar`, man page, `docs/basic-language.md`.
2. **`baslib.[ch]`** (DONE; no SDL; the logic of sections 2 and 3): the pre-scan of a stored program, finding and reading a file through
   the I/O drivers (`repl_open_program()` is reused), the dependency order, the cycle and clash checks; unit tests in `repltest1`
   with `mem:` files.
3. **Library mode in the compiler** (`compiler_t.library`: the statements allowed at top level; the variable check; per-module DATA
   chains and READ pointers, section 4a) and the **multi-module compile in `basrepl.c`** (`repl_compile()` / `repl_execute()`: modules compiled in order, a
   line table per module, the module table for error messages, the start address of the program, the init calls).
4. **`RUN` of a library** and the plain-`LOAD`-works rule; direct mode `LOAD LIBRARY` error; **`SAVE LIBRARY`** with `.lib` / `.l99`; **`LIBRARIES`**.
5. **`BUILD` container** and the runtime's lookup of embedded libraries.
6. **First libraries for the demos** (`demos/lib/`): the key / pad input of the games, text helpers; the games then shrink.
7. Tests: `vmtest1`, `repltest1`, `buildtest1`; `make install` installs the shared libraries; roadmap and `SESSION.md` updated.

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
