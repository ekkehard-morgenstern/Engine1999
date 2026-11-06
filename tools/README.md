# Tools

The tools are programs written in the built-in BASIC that belong to the development environment: the editors for sprites and tiles, music and sound effects. They run on the
UI subsystem and need the window version (`engine1999`). Unlike the demos they are meant to be **started from the prompt or from a program without losing the program you are working on**:

```
TOOL "spriteed"                  (the sprite and tile editor; your program in memory stays as it is)
TOOL "spriteed","ship.bas"       (the same, and opens the file ship.bas: the tool reads it as ARG$(1))
```

When the tool ends (its Quit button) you are back at the prompt with the program, its variables and the text on the screen as they were. A tool is found in `tools/` of the
current directory, in `tools/` next to your program, and in the installed `lib/engine1999/tools`; it finds its libraries (`filereq`, `uikit`, `mml`) in `lib/` next to it
(installed: `lib/engine1999/lib`; in the source tree: `demos/lib`). Tools are trusted: the security policy does not apply to them. `BUILD` embeds no tools. See `TOOL` in
`docs/basic-language.md`. A tool can also be run as a program of its own: `LOAD "tools/spriteed"`, `RUN`.

| File | What it is |
|---|---|
| `musiced.bas` | A **music editor** on the UI subsystem: piano keys add notes to up to three voices (a token list you can select, edit, delete), controls for octave, length, dots, tempo, volume, wave and note style, a preview of every note, Play / Stop, Save / Open as text and Export as a `PLAY` line. Uses the libraries `uikit`, `mml` and `filereq`. Window only. |
| `sfxed.bas` | A **sound effect editor**: sliders for pitch (start / end, logarithmic), length, volume, envelope, vibrato and tremolo build the `SFX` string; presets, Random, Play (or play on every change), the string can be edited by hand and applied, Save / Open / Export as `SFX n,"..."` lines. Uses `uikit`, `mml`, `filereq`. Window only. |
| `spriteed.bas` | A sprite and tile editor on the UI subsystem (reads and writes BMP files, too: Import BMP / Export BMP): a zoomed canvas, a palette window, pen / fill / flip, undo and redo (30 steps), groups of up to 8 pictures (a block of sprites or tiles), a preview, Store / Revert, and Open / Save of `DESIGN` listings through the file requester. Window only. |
