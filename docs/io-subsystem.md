# I/O subsystem with virtual device drivers

Status: **design agreed 2026-10-30; phases 1, 2 and 3 are implemented (BASIC-written drivers are a proposal)** (`iodev.[ch]`, tests `iotest1`, `vmtest1`, `repltest1`); phases 2 and 3 are
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
| `remove`, `rename`, `mkdir`, `chdir` | `KILL`, `RENAME`, `MKDIR`, `CHDIR` (0 = "Not supported by device") |
| `seek( h, pos )` | `SEEK #n, pos` (0 = not seekable) |

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

## What phase 2 does, exactly

- **`KILL name$`**, **`RENAME from$ TO to$`**, **`MKDIR name$`**, **`CHDIR name$`**: names may carry a `driver:` prefix, else the `ADDRESS`
  driver is used. `RENAME` needs both names on the same driver ("Not the same device") and **never replaces** an existing target
  ("File already exists"; `rename(2)` would do so silently). `MKDIR` of an existing name is "File already exists", `CHDIR` into a
  file "Not a directory". `CHDIR` changes the current directory of the whole process, so it also changes what `LOAD`, `SAVE` and
  `FILES` see. A driver without directories (`mem`, `null`, `sh`, `res`, `con`) answers "Not supported by device"; `KILL` on `res`,
  `sh`, `con` likewise. All five are one node (`NT_FILEOPSTMT`, kind `FOPK_*`) and one instruction (`FOP 0x155`, parameter = kind).
  The statements go through `iodev_allowed()` ("write"; `CHDIR`: "read"), the future sandbox hook.
- **`SEEK #n, pos`**: moves the next read or write of an open channel to byte `pos` (counted from 0); `LOC` follows. It drops the read
  buffer. `file` seeks anywhere (a file opened FOR APPEND still writes at the end, as `fopen("a")` does); `mem` within the
  file for reading ("Bad position" behind the end; a writable channel may seek behind it, the gap is zero-filled); `null` accepts everything; `sh` and `con` can't seek ("Not supported by device").
  A negative position is "Bad position". `SEEK` is a statement only (GW-BASIC's `SEEK` function is not provided; use `LOC`).
- **`sh:`**: `OPEN "sh:ls -l" FOR INPUT AS #1` runs the command through `popen()` and reads its standard output; FOR OUTPUT feeds its
  standard input; APPEND is "Bad file mode". Closing waits for the command; its exit status is ignored. `LOF` and `SEEK` are not
  supported. It asks `iodev_allowed( "shell", ... )` first, so the sandbox can forbid it together with the shell literals. Anything
  the command writes to its standard error goes to the terminal that started `engine1999`, not to the BASIC screen.
- **`res:`**: read-only files below the directory `res` of the current directory (`OPEN "res:sprites/ship.dat" FOR INPUT`). Absolute
  names and names with a `..` component are "Permission denied", as is any attempt to write. `FILES "res:"` lists it (the names
  then show the `res/` prefix, because the listing is the `file` driver's). `res` is a directory, not yet an archive.
- **`con:`**: the screen (FOR OUTPUT) and the keyboard (FOR INPUT) as an ordinary channel, so generic routines can write to `#n`
  wherever it points. Output goes to the host's print callback exactly like channel 0 (the screen column is updated), input reads
  keys one at a time through the host's `getkey`/`inkey` (no echo, no line editing: use `INPUT$(1,#n)` or `GET #n,k$`). The VM
  hands its callbacks to the driver with `iodev_set_console()` when a channel is opened; without a host the open fails with
  "Not supported by device". APPEND is not supported.
- New errors: "File already exists", "Not a directory", "Not the same device", "Bad position".
- Not done: `SEEK` as a function, `res:` from an archive, an `EXEC`-style statement for `sh` without a channel.

## What phase 3 does, exactly

- **Open modes**: `OPEN name FOR UPDATE AS #n` (short form `"U"`) opens for reading *and* writing without truncating (creates the file
  if it is missing). `OPEN name FOR RANDOM AS #n [LEN = reclen]` (short form `OPEN "R", #n, name [, reclen]`) is UPDATE with
  records of `reclen` bytes (default 128, at most 32767). A record length with any other mode is "Illegal argument". `RANDOM`
  and `UPDATE` are recognized in context like `OUTPUT`; the only new reserved words are `FIELD LSET RSET GETREC PUTREC`.
  A driver must be able to `seek` to hold a random file (`file` and `mem` can).
- **`FIELD #n, len AS a$ [, len AS b$ ...]`** binds plain string variables (no array elements) to consecutive parts of the record
  buffer of channel n; each `FIELD` statement starts again at offset 0 and drops the old bindings of that channel ("Field overflow"
  if the parts do not fit the record). A variable belongs to one part only.
- **`LSET a$ = expr`, `RSET a$ = expr`** put the text into the part of the record that `a$` is bound to (left / right justified,
  padded with spaces, cut to the width) and set `a$` to the padded text. On a variable that is not bound they justify within its
  current length (GW-BASIC does the same).
- **`PUTREC #n [, rec]`** writes the record buffer as record `rec` (numbered from 1; none or 0 = the record behind the last one read
  or written); **`GETREC #n [, rec]`** reads it ("Input past end" behind the last record) and assigns the parts to the bound
  variables. The buffer starts filled with spaces. A short last record is filled up with spaces. Writing behind the end of the
  file leaves a gap of zero bytes. `LOC(n)` of a random file is the number of the last record, `LOF(n)` the length in bytes,
  `SEEK` is in bytes. Records are independent of `PRINT #`, which is not meant for random files.
- Assigning to a bound variable with plain `A$ = ...` changes only the variable (the link is not GW-BASIC's pointer into the
  buffer): use `LSET`/`RSET` for what `PUTREC` should write. A string cannot hold a zero byte: a part is cut at the first zero byte when
  it is read, so binary fields (`MKI$`/`CVI` and friends) are not provided; store numbers as text (`STR$`, `VAL`).
- **`play:`**: `OPEN "play:" FOR OUTPUT AS #1` (or `"play:3"` for voice 3; voices 1..8, default 1). Every line written to the channel (ended by a
  newline, or by `CLOSE`) is a `PLAY` music string and is queued on that voice at once, exactly like `PLAY a$` with one voice, so
  `PRINT #1,"T120 O4 C D E"` plays. A bad string is an error with the music parser's message ("Unknown music command 'Z'"). Voices
  started by separate lines are not synchronized with each other (use `PLAY a$,b$` for chords). It is output only.
  Without a sound output (tests) the strings are checked and dropped.
- **`tcp:`**: `OPEN "tcp:host:port" FOR UPDATE AS #1` (`"tcp:[::1]:8080"` for IPv6 literals) connects (10 s limit; "Host not found",
  "Connection refused", "Timed out") and gives a duplex channel: `PRINT #1`, `LINE INPUT #1`, `INPUT$(n,#1)`, `EOF(1)` (waits until a byte
  arrives or the other side closes). Waiting is done in 100 ms slices that look at the Break key, so a stuck connection can be
  interrupted ("Break"); a reset connection is "Connection reset". Client side only (no listening sockets yet), no `LOF`/`SEEK`.
  It asks `iodev_allowed( "net", ... )`, so the sandbox can switch it off.
- New errors: "Field overflow", "Bad record number", "Host not found", "Connection refused", "Timed out", "Connection reset".
- Instructions: `FLD 0x156` (`FIELD`), `LRS 0x157` (`LSET`/`RSET`), `REC 0x158` (`GETREC`/`PUTREC`); `OPEN` takes the record length as an
  extra operand (0 = none). `mem` seeks behind its end when the channel is writable (the gap is zero-filled), so `PUTREC` can
  write any record.

## Drivers written in BASIC (proposal, not implemented)

**Goal**: a program can add its own device, for example `upcase:` (a filter), `pipe:` (a queue between two channels), `rot13:` that wraps
another driver, a driver for an in-memory database, or a protocol client on top of `tcp:`. Everything that takes a file name
(`OPEN`, `LOAD`, `SAVE`, `FILES`, `KILL`, ...) then works with it, and `ADDRESS` can select it.

**Registration by convention** (smallest possible syntax, labels already exist in the language):

```
DRIVER "upcase", "UPC"        ' registers upcase:, its handlers are the labels UPC_OPEN, UPC_READ, UPC_WRITE, UPC_CLOSE, ...
DRIVER OFF "upcase"           ' removes it (open channels of it are closed first)
```

A missing handler means "Not supported by device" for that operation. Handlers: `_OPEN _READ _WRITE _CLOSE _SIZE _SEEK _REMOVE _RENAME
_MKDIR _CHDIR _LIST`. They are ordinary subroutines (`LABEL UPC_READ: ... RETURN`).

**Calling convention**: the VM sets predefined variables, runs the handler as a `GOSUB`, and finishes the I/O statement with what the
handler left in them (the mechanism is the one `AFTER`/`EVERY` handlers and `WAIT` already use: the statement is suspended, the handler
runs between statements, then the statement is completed):

| Variable | Meaning |
|---|---|
| `DPATH$` | the part of the name behind `upcase:` (`_OPEN`, `_REMOVE`, `_RENAME` (also `DPATH2$`), `_LIST`) |
| `DMODE` | the open mode flags (1 read, 2 write, 4 append, 8 random) |
| `DH` | the handle: `_OPEN` assigns any number it likes, every later call gets it back (index into the driver's own arrays) |
| `DBUF$` | `_WRITE`: the data; `_READ`: the handler puts up to `DCOUNT` bytes here, `""` = end of the data |
| `DCOUNT` | `_READ`: how many bytes are wanted; `_SEEK`: unused |
| `DPOS` | `_SEEK`: the new position; `_SIZE`: the handler returns the length here (-1 = unknown) |
| `DERR$` | the handler sets an error text to make the statement fail (`DERR$="Disk full"`) |

Rules: a handler must not use its own driver's channels (the call is "Driver busy"; other drivers are fine, which is what makes
filters and wrappers possible: a handler of `rot13:` opens `file:` itself); timers may interrupt a handler only by the usual priority
rule; an error in the handler ends the program with the line of the handler and the operation (`? Type mismatch in line 1210 (driver upcase READ)`);
`RUN`, `NEW`, `LOAD` and `CHAIN` unregister BASIC drivers because their code goes away, after the channels were closed.

**Support for developing them** (the part that makes it practical):

1. `DRIVERS` (like `FILES`): lists the registered drivers, built-in or BASIC, and which handlers a BASIC driver has, so a missing label shows up at once.
2. `DRIVER TRACE "upcase", ON`: prints every call into the driver with its variables (`upcase OPEN path="x" mode=1 -> DH=1`), so the
   conversation between the VM and the handlers can be read without writing `PRINT`s into the driver.
3. Test doubles are built in: `mem:` as a backing store, `null:` as a sink, `con:` to see output, plus `sh:echo ...` for input. Together with
   `iotest1`-style checks written in BASIC this lets a driver be tried in direct mode.
4. Examples in `demos/`: `upcase` (a filter on `mem:`), `pipe` (a ring buffer between two channels), `rot13` (wrapping `file:`).
5. Binary data: handlers get strings, which end at a zero byte. If drivers for binary protocols matter, `DBUF` should become a byte
   array instead (`DIM DBUF(…)` with `DCOUNT`); decide before building the stream drivers.
6. Sandboxing needs nothing special: a handler is a BASIC program, its `OPEN`/`SHELL` go through `iodev_allowed()` like any other.

**Alternatives considered**: (a) a *server loop* in BASIC that waits for requests (`ACCEPT`/`REPLY`), which suits network daemons but needs
coroutines (a second stack) the VM does not have; (b) drivers as separate program files loaded on demand (clean isolation, but
heavy and hard to debug); (c) registering handlers one by one (`DRIVER "x", OPEN 100, READ 200, ...`), which is flexible but long. The
labelled-subroutine form (the proposal) needs the least new syntax and the least new VM machinery. **Open for your decision**: the
convention (labels with a prefix vs. explicit list), the variable names, whether to start with the byte-array interface, and whether
`DRIVERS` and `DRIVER TRACE` belong in the first version.

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
| `sh` | 2 (done) | `OPEN "sh:ls -l" FOR INPUT AS #1` reads the output of a command, `FOR OUTPUT` feeds its input (see Sandboxing) |
| `res` | 2 (done) | read-only game assets (a directory or an archive next to the program) |
| `con` | 2 (done) | the screen / keyboard as an ordinary channel, so generic routines can write to `#n` |
| `play` | 3 (done) | writing a music string to the channel plays it (`PLAY` voices) |
| `tcp` | 3 (done) | network connections (see Sandboxing) |
| programmable | proposal (below) | a driver written in BASIC (open/read/write/close as subroutines) |

## Phases

1. **Phase 1** (done): `iodev.[ch]` with the registry and `file`, `null`, `mem`; the channel table in the VM;
   `OPEN`, `CLOSE`, `LINE INPUT`, `WRITE`, `EOF`, `LOF`, `LOC`, `INPUT$`, `ADDRESS`, `ADDRESS$`; `PRINT #`, `INPUT #`, `GET #` on
   files; `LOAD`, `SAVE`, `MERGE`, `CHAIN` and `FILES` through the drivers. Tests use `mem:`.
2. **Phase 2** (done): `KILL`, `RENAME ... TO`, `MKDIR`, `CHDIR`, `SEEK`; the `sh`, `res`, `con` drivers (next section).
3. **Phase 3** (done, except drivers in BASIC, see "Drivers written in BASIC"): random access records: `FIELD #n, len AS a$ ...`, `LSET`, `RSET`, `GETREC #n [, rec]`, `PUTREC #n [, rec]` (new words,
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
