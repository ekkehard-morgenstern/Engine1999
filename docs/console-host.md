# The console host (`engine1999c`)

The same BASIC system as `engine1999`, without a window and without SDL: for shell scripts, pipes and servers. It is a second `main()` (`engine1999c.c`) on top of the
host-independent REPL (`basrepl.c`); `engine1999c` links with nothing but the C and math libraries.

```
engine1999c prog.bas              load the file, run it, end; the exit status tells how it went
engine1999c -i prog.bas           the same, then go on with the lines from standard input
echo 'PRINT 1+2' | engine1999c    BASIC lines on standard input
engine1999c                       on a terminal: a banner and "Ready." prompts, like the window version
```

A script can start with a `#!` line (`#!/usr/bin/env engine1999c`): the loader skips a first line that begins with `#!` in text listings (also for `LOAD` and `MERGE` in the window version).
The file is a `.bas` listing or a binary `.e99` file, found the way `LOAD` finds it (`engine1999c prog` finds `prog.bas`).

## Arguments, environment, standard channels

Everything after the file name is for the program: `engine1999c prog.bas a "b c"` gives `ARGC` = 2, `ARG$(1)` = `a`, `ARG$(2)` = `b c`, `ARG$(0)` = `prog.bas`; `ENV$("HOME")` reads the environment; `OPEN "stderr:" FOR OUTPUT AS #1` writes to standard error, `OPEN "stdin:" FOR INPUT AS #2` reads standard input (`docs/basic-language.md`, "Program arguments and the environment"). With standard input as the program (`engine1999c - a b`) `ARG$(0)` is `-`.

## Input and output

- **Output** of the program (channel 0) goes to standard output, binary-safe (`CHR$(0)` is written as a zero byte). **Messages and errors** go to standard error (`? Division by zero in line 20`); on a terminal an error message starts on a line of its own.
- **Lines on standard input** are entered like typed lines: a numbered line is stored, any other runs at once; an error in a line is reported and the next line is read (the exit status of this mode is 0, or the code of `BYE`/`QUIT`).
- **`INPUT`, `LINE INPUT`, `GET` and `INKEY$`** of a running program read the same standard input, in order, so `printf '10 INPUT A$\nRUN\nbob\n' | engine1999c` works. `INPUT` at the end of the input is the error "End of input"; `GET` too; `INKEY$` simply returns "" then.
  **Line editor:** the BASIC prompt and `INPUT` / `LINE INPUT` read their lines through the one-line editor of the host (`scr_lineedit()` in `scredit.c`), **the same code as in the window**, which also serves `EDIT n` and the monitor's prompt: characters are typed at the cursor, LEFT / RIGHT / HOME / END move, BACKSPACE / DELETE remove, RETURN accepts, ESC cancels (at the prompt the line is dropped, at `INPUT` that is Break), Ctrl-D on an empty line is the end of the input, the line is cut at 255 characters at the prompt and 1023 for `INPUT`, other keys (function keys, control codes, TAB) are ignored. There is no history or word movement (the window has none). On a terminal the editor draws with absolute cursor addressing on a model of the normal, scrolling terminal (`conscr_line_*`; the terminal is asked where its cursor is with `ESC [ 6 n` when a line starts, a terminal that does not answer is treated as if the cursor were on the bottom row); Ctrl-C is ESC to it, a resize redraws the line.
  **A redirected input is no special case:** the same editor runs, the bytes of a file or pipe are its keys (a line end is RETURN, a CR LF counts once, an escape sequence is a cursor key, the end of the input completes a last line that has no line end). Nothing is drawn if the input or the output is not a terminal (with only the output redirected the terminal's own line mode echoes the typing). This allows redirection from another source than the console. Consequence: lines from a file are limited like typed ones (255 characters at the prompt).
  Keys are the same as in the window: the printable characters, `RETURN` (13, also for a line end in a pipe), `BACKSPACE` (8), `TAB` (9), and the cursor and function keys as escape sequences in the canonical form (`ESC [ A`, `ESC O P`, `ESC [ 1 ; 5 D`, ...: see `docs/basic-language.md`, "Keyboard"); the dialects of other terminals are turned into it, unknown sequences are skipped. `engine1999c --debug-keys` shows what a terminal sends. On a terminal the text is ISO-8859-15 inside and UTF-8 outside: typed characters (keys, `INPUT`, program lines) are converted to it and output from code 128 up is converted back; in a pipe or a file the bytes are not touched. On a terminal `GET` and `INKEY$` take single keys without echo (the terminal is put back at the end).
- **Ctrl-C is Break** while something runs (`? Break in line 10`, exit status 1 in run mode). Between lines it has its usual effect and ends the process.

## Exit status

| Situation | Status |
|---|---|
| the program ends, or the end of the input | 0 |
| `BYE n` / `QUIT n` (`n` 0..255) | `n` |
| an error ends the program (run mode `engine1999c file`) | 1 |
| the file can't be loaded, an unknown option | 2 |

## Display and sound statements

- **Sound** statements (`SOUND`, `PLAY`, `SFX`, `ENVELOPE` ...) have no sound device: their arguments are checked and nothing plays, `PLAYING(n)` is 0.
- **`CLS` and `LOCATE`** write ANSI escape sequences when standard output is a terminal (and nothing when it is not). `COLOR` is accepted and ignored: the Engine's palette is not the terminal's; `PRINT CHR$(27);"[31m";...` passes through to terminals as it is.
- **Everything else** that needs a screen (`GFX`, `SPRITE`, `TILE`, `TXT`, `DESIGN`, `SYMBOL`, and the graphics functions) fails with `? Not available in the console host`. `AUTO` and the pager of `FILES` are window-only; `EDIT` without a target and `MONITOR` work on a terminal (next section).
- The functions that ask for the text screen size answer with the size of the terminal (80 x 25 without one).

## Syntax colors

`LIST` (and `FILES`) are colored like in the window (`ansi_hlstyle()` in `ansi.c`, shared by both hosts: ANSI SGR sequences) when standard output is a terminal and `NO_COLOR` is not set (no-color.org). `--color=always` forces them (also into a pipe, e.g. for `less -R`), `--color=never` or `NO_COLOR` switch them off, `--color=auto` is the default. `PRINT` output is never colored by the host.

## Executables without a window

`BUILD "name"` in `engine1999c` makes a console executable (and `BUILD "name", WINDOW` a window one); `engine1999` makes window executables by default and `BUILD "name", CONSOLE` the console kind. A console executable is this host with the program inside: same input, output, exit status and arguments, no options except `--engine1999-about`; it needs only the C and math libraries. The kit is `engine1999c.o` + `engine.lib` (`docs/basic-language.md`, "BUILD").

## Options

`-i` (go on with standard input after the file), `-` (read the lines from standard input), `--color[=always|never|auto]`, `--version`, `-h`/`--help`, and the `--debug-*` options of `engine1999` (`--help` lists them).

## The full-screen editor and the monitor on a terminal

`EDIT` without a target and `MONITOR` open the same editors as the window version (`fulledit.c`, `monitor.c`), because both hosts hand them a *screen driver* (`scrdrv.h`): the window's (`sdlscr_driver()` in `sdlscreen.c`) or the terminal's (`con_drv` in `engine1999c.c`). Editing is therefore the same in both (the owner's rule): the same keys, the same layout, the same break keys.

- **The screen buffers** are the text screens of `textscreen.c` (screen 1: the editor, screen 2: the monitor, as in the window); the program's own screen is the terminal's normal screen. The size of the screens is the terminal's size (`txtscr_resize()`; the window's is fixed at 80 x 25).
- **Painting** (`conscr.[ch]`, no curses, no library): the terminal is switched to its *alternate screen* (`ESC [ ? 1049 h`, back with `l`, so the shell's text returns), autowrap and the cursor are controlled by us, and only the cells that changed since the last paint are written (a copy of what the terminal shows is kept). Colors are the Engine's palette entries (`palette.[ch]`): exact with `COLORTERM=truecolor` or `24bit`, else the nearest of the 256 xterm colors. Characters are ISO-8859-15, written as UTF-8; control codes are drawn as `?`.
- **Keys** are those of `GET` / `INKEY$`: the canonical escape sequences (`keyseq.h`), mapped to the editors' keys; Ctrl-C is ESC there (it leaves the editor, cancels a line in the monitor), a lone ESC is ESC.
- **Resizing the terminal** (`SIGWINCH`): the screens are resized (the top left parts of their contents are kept; a screen whose cursor row would fall off moves up, like a terminal does), the editors get a resize event and lay themselves out again: the panes, the scroll bar and the bars of the editor follow, the monitor's bars and output area too, and a line being typed in the monitor is drawn again at its place. A terminal smaller than 24 x 6 shows `Too small` until it is big enough again (ESC leaves the editor).
- **Programs run by the monitor** (`g`, `t`) print on the monitor's screen (the ANSI colors of the program work there); ESC (a lone ESC) and Ctrl-C break them.
- **Without a terminal** (standard input or output is not one) both fail with `? EDIT and MONITOR need a terminal`.
- **`EDIT n`** works: the line is edited with the same one-line editor as the prompt (RETURN enters it, ESC cancels).
- **ESC breaks a running program** on a terminal now (like the window), also a program that polls `INKEY$` or waits in `GET`, `INPUT$` or `LINE INPUT`: while a program runs the terminal is in raw mode, the keys typed meanwhile are queued for `GET` / `INKEY$` / `INPUT` (and dropped when the program ends, like the window does), and a lone ESC is Break, like Ctrl-C.

## Not done yet (roadmap section 12)

Audio without a window. `AUTO` in the console host. See roadmap section 12.

## Design decisions I took (revisit if unwanted)

- `GFX`-like statements are errors, not silent no-ops (a program that needs graphics should not seem to work); sound is silent because the VM treats "no sound device" that way already.
- Standard input is unbuffered (`getline` and the key reads both go to the file descriptor), so lines and single keys stay in step in a pipe; for very large scripts on standard input a file argument is faster.
- A line error in the pipe mode doesn't change the exit status; run mode returns 1.
- Ctrl-C only catches while a program runs (the handler is installed around each line), so an idle prompt ends on Ctrl-C.
- Tests: `contest1` (116 checks: through the shell, and on a pseudo terminal for the line editor: pipes, exit codes, the `#!` line, `INPUT`/`GET` from a pipe, SIGINT; run it in the build directory).
