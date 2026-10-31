# I/O subsystem with virtual device drivers

Status: **design agreed 2026-10-30; phase 1 is implemented** (`iodev.[ch]`, tests `iotest1`, `vmtest1`, `repltest1`); phases 2 and 3 are
open. The decisions of the project owner are listed under "Decisions"; the rest is the design as proposed. "What phase 1 does,
exactly" at the end of "Statements and functions" lists the details that the proposal left open.

## Goals

- BASIC programs get files and devices through the usual statements (`OPEN`, `CLOSE`, `PRINT #n`, `INPUT #n`, ...), following
  GW-BASIC and AmigaBASIC.
- A file name may start with a **driver name and a colon** (`mem:scores`, `file:/tmp/x`). The driver decides what the name
  means: a disk file, a RAM file, a pipe, a network connection, a read-only asset, ...
- `ADDRESS` (as in ARexx) chooses the driver that names *without* a prefix refer to.
- Everything that takes a file name goes through the same layer: `OPEN`, `LOAD`, `SAVE`, `MERGE`, `CHAIN`, `FILES` (and later
  `KILL`, `RENAME`, `MKDIR`, `CHDIR`). A program can therefore live on `mem:` or in an asset archive as well as on disk.
- The layer is independent of SDL and of the VM (`iodev.[ch]`, like `basfiles`/`synth`), so it can be tested without a screen.
  The host can register more drivers, the way it supplies callbacks today.

## Concepts

**Driver** (`iodrv_t`): a name and a table of operations. Drivers are registered once (`iodev_register()`), looked up by name
(case-insensitive). The operations of a driver (all optional except `open`; a missing one is "Not supported by device"):

| Operation | Meaning |
|---|---|
| `open( drv, path, mode, err )` | opens `path` (the part after the prefix) for INPUT, OUTPUT or APPEND; returns a handle |
| `read( h, buf, n )` / `write( h, buf, n )` | bytes; `read` returns 0 at the end of the data |
| `seek( h, pos )`, `size( h )` | random access and the length (for `SEEK`, `LOF`, `LOC`, `EOF`) |
| `close( h )` | |
| `list( drv, spec, style, emit )` | the lines of `FILES` |
| `remove`, `rename`, `mkdir`, `chdir` | phase 2 |

**Channel**: `#n`. Channel 0 is the screen and the keyboard (the host callbacks `print`, `input`, `getkey`); it is always open and
cannot be opened or closed. Channels 1 to 255 are opened by `OPEN`. The VM keeps a table of them: driver, handle, mode, a read
buffer (so `LINE INPUT` and `INPUT$` work on any driver), the position and an end-of-file flag. `PRINT #n`, `INPUT #n`, `GET #n`
and the new statements use it; everything that used to be discarded on channels other than 0 now goes to the file.

**Name resolution** (`iodev_resolve`): a name `ident:rest` is `driver:path` if `ident` is a letter followed by letters, digits
or underscores (two to sixteen characters). The driver must exist: an **unknown driver is an error** ("Device not found", as in
AmigaBASIC); `file:` forces a plain disk file, which is also the way to name a file that really contains a colon (`file:a:b`).
A name without such a prefix belongs to the current `ADDRESS` driver (initially `file`). One-letter prefixes are not drivers
(`C:` stays a plain name).

## Statements and functions

```
OPEN name$ FOR INPUT|OUTPUT|APPEND AS #n          the long form (GW-BASIC, AmigaBASIC)
OPEN mode$, #n, name$                             the short form: "I", "O", "A" (also lower case)
CLOSE [#n [, #m ...]]                             no argument: closes all channels (not channel 0)
PRINT #n, ...        INPUT #n, a, b$       GET #n, a$       these exist already and now work on files
LINE INPUT [#n,] a$                               the whole line, nothing is split at commas, no prompt "? "
WRITE [#n,] expr {, expr}                         strings in quotes, numbers without a leading blank, commas, newline
EOF(n)    LOF(n)    LOC(n)                        end of file (-1/0), length in bytes, current position in bytes
INPUT$(count [, #n])                              count characters (from the keyboard without #n)
ADDRESS [driver]     ADDRESS$                     chooses / returns the default driver
```

- The mode words after `FOR` and `AS` are recognized in context (like `APPEND` in `MERGE`): only `OPEN`, `CLOSE`, `LINE`, `WRITE`,
  `ADDRESS`, `EOF`, `LOF`, `LOC` become reserved words.
- `OPEN` of an open channel is "File already open", `PRINT #n` on a channel that is not open "File not open", reading from an
  OUTPUT file or writing to an INPUT file "Bad file mode", reading past the end "Input past end" (test with `EOF(n)`).
- `INPUT #n` splits the line at commas like the keyboard `INPUT` (fields that don't fit are "Redo from start" on the keyboard; on
  a file they are an error: "Bad data in file").
- **ADDRESS**: `ADDRESS mem` or `ADDRESS "mem"` sets the default; a bare identifier is taken as the name (like ARexx), a string
  expression is evaluated. `ADDRESS` alone swaps with the previous default (ARexx does the same). `ADDRESS$` returns the current
  name. An explicit prefix always wins over it. **It is reset to `file` by `RUN`** (and `NEW`, `LOAD`, `CHAIN`: everything that
  restarts the program with a fresh runtime). Direct mode lines that follow each other see the same setting.
- **Line ends**: text is written with `"\n"` only. Reading accepts `"\n"` and `"\r\n"`: the `"\r"` in front of a `"\n"` is dropped.
- Files are closed by `RUN`, `NEW`, `LOAD` and `CHAIN` (a restart), at the end of the session, and by `CLOSE`. A direct mode line
  may open a file and a later line may use it.

**What phase 1 does, exactly** (details the proposal left open):

- `LOC(n)` is the number of **bytes** read or written so far (GW-BASIC counts records of 128 bytes); `LOF(n)` is the length in bytes
  (an append channel starts at the end: `LOC` = the old length); `EOF(n)` is -1 when no data is left (a channel opened FOR OUTPUT or
  APPEND: "Bad file mode"). Channel 0 is not accepted by `EOF`, `LOF`, `LOC` ("Bad file number"; "File not open" for a closed one).
- `PRINT #n` and `WRITE #n` may stand alone (`PRINT #1` writes a newline). The comma zones of `PRINT ,` use the screen column, not the
  file's.
- `INPUT #n, a, b$, c` reads the fields of a line; when the line has no more fields **it reads on with the next line** (a line end
  separates fields like a comma). Fields are numbers or strings as on the keyboard (a quoted string may contain commas); a field
  that is not a number is "Bad data in file". `INPUT #n` shows no prompt and never prints "?Extra ignored" (the rest of the line is
  dropped). Reading past the end is "Input past end".
- `GET #n, a$` reads **one byte** (a numeric variable gets its code); `INPUT$(count, #n)` reads count bytes. A zero byte cannot be
  held in a string (strings end at NUL), so text files are the target for now.
- `LINE INPUT #n, a$` and `LINE INPUT "prompt"; a$` (keyboard) put the whole line into a string variable; there is no `? ` and no
  splitting at commas. The prompt must be a string literal, as for `INPUT`.
- `WRITE`: strings are put in double quotes (an embedded quote is not escaped), numbers are written as `PRINT` writes them but
  without the `PRINT` separators, items are separated by commas and the line ends with a newline.
- Mode letters of the short form `OPEN "I"|"O"|"A", #n, name` may be lower case; any other letter is "Bad file mode" (`"R"` for
  random files comes with phase 3). Channels are numbered 1 to 255.
- `ADDRESS "mem:"` is accepted as well as `ADDRESS mem` (a trailing colon is dropped); an unknown driver is "Device not found". The
  names are kept in lower case.
- Errors have the usual form ("? File not found in line 20"); they stop the program. `SAVE` also prints the reason ("? Device not
  found") before its own "? Save failed".
- The `mem` driver is one RAM disk for the whole process: its files survive `RUN` and `NEW` (not the end of the program that runs
  `engine1999`), `FILES "mem:"` lists them (oldest first, like `FILES`), a file that is open when the driver deletes it stays
  readable until it is closed.

## Existing commands through the drivers (phase 1)

`LOAD`, `MERGE`, `CHAIN` read the program with `iodev` (a whole file is read into memory, then parsed from a memory stream),
`SAVE` writes through it, `FILES` calls the driver's `list`. The default extensions (`.e99`, `.bas`) and the extension search of
`LOAD` work on the path part, whatever the driver. `SAVE "mem:game"` and `LOAD "mem:game"` therefore work, and a test needs no
disk. The `file` driver uses the current directory of the process.

## Drivers

| Name | Phase | What it is |
|---|---|---|
| `file` | 1 | the disk (default); `list` is the `ls -ltr`-like listing of `FILES` |
| `null` | 1 | accepts everything, always at the end |
| `mem` | 1 | a RAM disk, one namespace for the whole process (survives `RUN`); files are created by OUTPUT/APPEND; `FILES "mem:"` lists them |
| `sh` | 2 | `OPEN "sh:ls -l" FOR INPUT AS #1` reads the output of a command, `FOR OUTPUT` feeds its input (see Sandboxing) |
| `res` | 2 | read-only game assets (a directory or an archive next to the program) |
| `con` | 2 | the screen / keyboard as an ordinary channel, so generic routines can write to `#n` |
| `play` | 3 | writing a music string to the channel plays it (`PLAY` voices) |
| `tcp` | 3 | network connections (see Sandboxing) |
| programmable | 3 | a driver written in BASIC (open/read/write/close as subroutines) |

## Phases

1. **Phase 1** (done): `iodev.[ch]` with the registry and `file`, `null`, `mem`; the channel table in the VM;
   `OPEN`, `CLOSE`, `LINE INPUT`, `WRITE`, `EOF`, `LOF`, `LOC`, `INPUT$`, `ADDRESS`, `ADDRESS$`; `PRINT #`, `INPUT #`, `GET #` on
   files; `LOAD`, `SAVE`, `MERGE`, `CHAIN` and `FILES` through the drivers. Tests use `mem:`.
2. **Phase 2**: `KILL`, `RENAME ... TO`, `MKDIR`, `CHDIR`, `SEEK`; the `sh`, `res`, `con` drivers.
3. **Phase 3**: random access records: `FIELD #n, len AS a$ ...`, `LSET`, `RSET`, `GETREC #n [, rec]`, `PUTREC #n [, rec]` (new words,
   because `GET #n, v` already reads one character or byte, which clashes with GW-BASIC's `GET #n, record`); engine drivers
   (`play`, `tcp`); drivers written in BASIC.

## Decisions (project owner, 2026-10-30)

1. An unknown driver prefix is an error for now ("Device not found"), as in AmigaBASIC.
2. Random access records use new keywords (`FIELD`, `GETREC`, `PUTREC`), not GW-BASIC's `GET`/`PUT`.
3. `LOAD`, `SAVE`, `MERGE`, `CHAIN`, `FILES` go through the drivers in phase 1.
4. `ADDRESS` is reset by `RUN`. Text is written with `"\n"` and read with `"\n"` or `"\r\n"`.
5. Sandboxing is an **open issue** (next section).

## Sandboxing (open)

Programs written in the built-in language can already do anything the user can: shell literals (`` `rm -rf ~` ``, `A$=`...``) run a
shell command, and the `file` driver reaches every file the process can. That is acceptable for the programmer's own programs
and not for programs from other people (downloaded games, demos). The I/O layer is the natural place to enforce limits, and the
shell literals need the same treatment. Nothing of this is implemented; the questions are:

- **Which operations need permission?** shell literals and `sh:`, writing outside a program directory, reading outside it, the
  network (`tcp`), `MONITOR` (machine code patching), `CHAIN`/`LOAD` of programs from elsewhere.
- **How is the permission given?** Options: (a) a command line switch of `engine1999` (`--sandbox`, or `--allow=shell,net,write`);
  (b) a per-program trust level the user grants when a program is loaded (and remembers); (c) a permission file next to the
  program, which the user has to approve. A sandboxed program gets "Permission denied" for what it may not do.
- **Which root?** In a sandbox the `file` driver would be confined to a directory (the program's own, or `ADDRESS`-like
  `mem:`/`res:` only), with `..` and symbolic links resolved before the check (`realpath`).
- **What about direct mode?** The user typing at the prompt is trusted; a loaded program is not. So the restrictions would be a
  property of a *run*, set when a program is `LOAD`ed or `CHAIN`ed from elsewhere, and cleared by the user.

The drivers take the access decision through one function (`iodev_allowed( op, driver, path )`) so that a policy can be added in one
place; phase 1 allows everything. The decision on how the policy is configured is still to be made.
