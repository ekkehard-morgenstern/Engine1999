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

An executable made with `BUILD` must contain the libraries its program uses, else it could not run alone. The embedded program is
today a program file; the executable gets the program **plus the resolved library files as additional modules** (the embedded
payload becomes a small container: one entry per module, each a normal program file: nothing in the program file format changes,
the container is new and versioned from the start). The runtime resolves `LOAD LIBRARY` in an embedded program from the
container, not from the disk. `engine1999 --engine1999-about` and the license rule are unchanged; a library's license lines
travel with it.

## 7. Implementation plan

**Status (2026-11-03): steps 1 to 4 are done** (step 1: the words and their grammar; step 2: `baslib.[ch]`, the resolver, and its hook in the REPL: `RUN` and the monitor's `c` find the libraries of the stored program and report what is wrong, **but the libraries are not compiled yet**, so a program that names a library that exists still does not see its `FUNC`s: that is step 3). (step 1: the words and their grammar; step 2: `baslib.[ch]`, the resolver, and its hook in the REPL; **step 3: the libraries are compiled ahead of the program**, see "What step 3 does, exactly" below). **Step 4:** `SAVE LIBRARY`, `LIBRARIES`, the direct-mode error (see "What step 4 does" below). Open: the `BUILD` container (step 5), the shared libraries for the demos (step 6).

1. **Extension word `LIBRARY`** (`XF_LIBRARY`), parser rules for `LIBRARY "name"[,"prefix"]` and `LOAD LIBRARY "name"` (new node
   types; no code is generated for either: `NOP`), `bascomp.ebnf` + `make grammar`, man page, `docs/basic-language.md`.
2. **`baslib.[ch]`** (DONE; no SDL; the logic of sections 2 and 3): the pre-scan of a stored program, finding and reading a file through
   the I/O drivers (`repl_open_program()` is reused), the dependency order, the cycle and clash checks; unit tests in `repltest1`
   with `mem:` files.
3. **Library mode in the compiler** (DONE) (`compiler_t.library`: the statements allowed at top level; the variable check; per-module DATA
   chains and READ pointers, section 4a) and the **multi-module compile in `basrepl.c`** (`repl_compile()` / `repl_execute()`: modules compiled in order, a
   line table per module, the module table for error messages, the start address of the program, the init calls).
4. **`RUN` of a library** and the plain-`LOAD`-works rule; direct mode `LOAD LIBRARY` error; **`SAVE LIBRARY`** with `.lib` / `.l99`; **`LIBRARIES`** (DONE).
5. **`BUILD` container** and the runtime's lookup of embedded libraries.
6. **First libraries for the demos** (`demos/lib/`): the key / pad input of the games, text helpers; the games then shrink.
7. Tests: `vmtest1`, `repltest1`, `buildtest1`; `make install` installs the shared libraries; roadmap and `SESSION.md` updated.

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
- **Known limits:** the monitor lists the libraries' code like any other code; `BUILD` does not embed libraries yet (step 5); `LIBRARIES` and `SAVE LIBRARY` come with step 4.

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
