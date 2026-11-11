# Tools

The tools are programs written in the built-in BASIC that belong to the development environment: the editors for sprites and tiles, the text font, music and sound effects. They run on the
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
| `fonted.bas` | A **font editor** for the text screen font (`SYMBOL`): a table of all 256 characters at their real size, an 8 x 12 pixel grid, a three-times preview, Clear / Invert / Flip / shift, Copy / Paste, Built-in / All built-in, 30 undo steps; every stroke is sent to the engine with `SYMBOL` at once and **the font stays changed when the tool ends** (so you can stop a program, edit its characters and go on). Save writes the changed characters (or all) as `DESIGN CHAR n` + 12 `@ "........"` rows that you can `MERGE` or `RUN`; Open reads such a file (also `TOOL "fonted","file.bas"`). Uses `uikit`, `filereq`. Window only. |
| `musiced.bas` | A **music editor** on the UI subsystem: piano keys add notes to up to three voices (a token list you can select, edit, delete), controls for octave, length, dots, tempo, volume, wave and note style, a preview of every note, Play / Stop, Save / Open as text and Export as a `PLAY` line. Uses the libraries `uikit`, `mml` and `filereq`. Window only. |
| `sfxed.bas` | A **sound effect editor**: sliders for pitch (start / end, logarithmic), length, volume, envelope, vibrato and tremolo build the `SFX` string; presets, Random, Play (or play on every change), the string can be edited by hand and applied, Save / Open / Export as `SFX n,"..."` lines. Uses `uikit`, `mml`, `filereq`. Window only. |
| `insted.bas` | An **instrument editor** for the synthesizer's instruments (`INSTRUMENT n,a$`, `I n` in `PLAY`): six tabs (three oscillators with FM index and Mod, the filter with the pitch envelope, the envelope with vibrato and gain, FM / Poly: FM decay, the number of voices, FM feedback of the oscillator chosen on the Osc pages and the algorithm 1 check), the 24 named factory instruments with `<` / `>`, a note slider, **Play**, **Chord** and **Hold** (a note that sounds until Stop: every change of a control is heard at once, the Note slider bends its pitch), play on change, the definition string below (edit it and press Return), Save / Open (`.ins` text files), Export as an `INSTRUMENT n,"..."` BASIC line, Copy to the clipboard. Uses instrument 63 and channels 1 to 3. `TOOL "insted"` or `TOOL "insted","file.ins"`. |
| `spriteed.bas` | A sprite and tile editor on the UI subsystem (reads and writes BMP files, too: Import BMP / Export BMP): a zoomed canvas, a palette window, pen / fill / flip, undo and redo (30 steps), groups of up to 8 pictures (a block of sprites or tiles), a preview, Store / Revert, and Open / Save of `DESIGN` listings through the file requester. Window only. |
