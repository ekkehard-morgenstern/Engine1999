# Security policy (roadmap item 54, implemented 2026-11-05)

What a **running program** may do with the machine. Written from the owner's design of 2026-11-03 (roadmap section 17) and the decisions of 2026-11-05.
Code: `policy.[ch]` (no SDL, no VM), the hook `iodev_allowed()` in `iodev.c`, `repl_policy_begin()` / `repl_finish()` in `basrepl.c`, the dialogs in the hosts
(`host_policy_ask()` in `engine1999.c`, `con_policy_ask()` in `engine1999c.c`). Tests: `poltest1` (54 checks), the `policy:` cases of `contest1`.

## The rules

- **Only running programs are restricted.** What you type at the prompt (LOAD, SAVE, FILES, shell literals, `OPEN ...`) is you acting and is never checked.
  A program is "running" from `RUN` / `CHAIN` / `LOAD` + run / the monitor's `g` / an executable made with `BUILD` until it ends (`repl_policy_begin()` ...
  `policy_end_program()` in `repl_finish()`).
- **The program's folder.** The folder of the program file (`repl_t.filename`: the last file loaded or saved); for an executable made with `BUILD` the folder of
  the executable; for a program that was typed in (no file) the current directory when the run starts. Below it a program may do what it likes with files
  (the `file:` and `dir:` drivers). Never up (`..`), never an absolute path elsewhere; symbolic links are resolved (`policy_normalize()`: the longest existing
  part of the path goes through `realpath`, the rest is normalized lexically), so a link out of the folder is outside.
- **Powers that need permission** (`POL_*`): `read` (files / directories outside the folder, also listing), `write` (writing, `KILL`, `RENAME`, `MKDIR`,
  `CHDIR` is a read, `BUILD` writes its executable), `shell` (shell literals `` `cmd` `` and `OPEN "sh:..."`), `net` (`tcp:`). Without it the statement fails
  with the error **Permission denied** (a program can catch it with `ON ERROR`).
- **No permission needed:** `mem:`, `null:`, `con:`, `stdin:`, `stdout:`, `stderr:`, `res:` (read only, below `./res`), `config:` and `local:` (a program's own folders `~/.config/<name>/` and `~/.local/share/<name>/`, confined and limited to 64 MiB: `docs/io-subsystem.md`), `play:`, and drivers a program defines itself.
- **A power is granted for everything** (not per path): "write outside my folder" is one decision.

## Questions

When a program needs a power it has not got, the host is asked (`policy_set_asker()`):

- **Window:** a modal dialog on the UI layer ("Permission: write - The running program wants to: write the file outside of its folder: /x/y - Allow it?"
  Allow / Deny), then after Allow "For how long?": **This session**, **24 hours**, **Always**, Cancel (= no). If the program has no UI open, the UI is opened
  for the question and the graphics layer (pixels, theme palette entries 224..239, enabled state) is put back afterwards; if it has, the dialog is a modal panel on
  top. Escape = no, Break = no. (The pointer timeout of the program is reset to the default after such a dialog.)
- **Console host:** in screen mode the question is the status line (keys n / s / d / a), in line mode on a terminal it is written to standard error and the key
  comes from the terminal, otherwise `/dev/tty` is tried when standard output or error is a terminal; **with no terminal the answer is no** (a script or a
  test never hangs: use `--allow=`).
- A **no** holds for the rest of the run (no second question for the same power). A **session** grant holds until the process ends (for the same program:
  path + content hash). **24 hours** and **always** are stored.

## Where the grants are stored

`~/.config/engine1999/grants` (`$XDG_CONFIG_HOME/engine1999/grants`; `ENGINE1999_GRANTS` names another file; the directory is made with mode 0700, the file
has mode 0600), one line per grant: `KEY POWER EXPIRY PROGRAM` (KEY = 16 hex digits, EXPIRY = seconds since 1970, 0 = always). **KEY** is an FNV-1a 64 bit hash
over the absolute path of the program, its content (for an executable made with `BUILD` the embedded image) and the text of its permission file, so a
**changed program has no consent** (it is asked again). Expired lines are dropped whenever the file is written. A program that was typed in has no key: nothing
is stored and no session grant is possible for it.

## The permission file

`NAME.perm` next to the program (NAME = the program file without its extension; for an executable: next to the executable) asks for powers up front, like
Java's policy files. Lines: `request read`, `request write`, `request shell`, `request net` or `request all`; `#` starts a comment; other lines are ignored.
When the program starts the user is asked once for each power that is not decided yet ("... (the program asks for it up front)"), and the answers are handled
as above. A program that changes its permission file changes its key.

## Options

`--allow=LIST` and `--deny=LIST` (`engine1999`, `engine1999c`; for executables made with `BUILD`: `--engine1999-allow=LIST`, `--engine1999-deny=LIST`, first on the
command line) decide for all programs without asking. LIST: `read`, `write`, `shell`, `net`, `all`, comma separated; the last option for a power wins. A bad
word ends the program with status 2.

## Design decisions I took (revisit if unwanted)

- The policy is **off in the library** (`policy_enable( false )` by default) and switched on by the two hosts, so the unit tests of the other modules are not
  affected.
- **One folder** (the program's); a program in `demos/` run from the project directory therefore asks before it writes its high score file into the current
  directory; answering *always* ends that. (A second allowed folder, the current directory at the start, would avoid the question for the installed games.)
- Powers are not per path, a session grant is bound to the program (path + hash), not to the process only.
- "This session" lasts for the life of the process (for a `BUILD` executable: for the run); "today" is 24 hours.
- `LOAD LIBRARY` (found and compiled when a program starts) and the kit of `BUILD` are the engine's own reading and are not checked; `CHAIN` / `LOAD` / `MERGE`
  / `SAVE` / `FILES` / `SPRITE LOAD` / `GFX SAVE` ... by a program go through the drivers and are.
- Not done: the monitor's machine code patching (`MONITOR` ends a run), per-path grants, revoking from inside the engine (delete the line in the grants file).
