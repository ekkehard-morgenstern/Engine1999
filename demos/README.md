# Demos

Example programs written in the built-in BASIC of Engine1999 (none yet). They are meant to show what the engine can do
(text screen, tiles and sprites, graphics, sound) and to serve as manual tests.

Conventions:
- Programs are kept as **ASCII listings** with the extension `.bas` (`SAVE "name",A` writes them; `LOAD "name"` finds `name.bas`),
  so they can be read and diffed. Binary tokenized programs (`.e99`, the default of `SAVE "name"`) don't belong in this folder.
- Programs written in the built-in language are not covered by the engine's license (see `LICENSE`); a demo may carry its own.
- Run one with `engine1999`, then `LOAD "demos/name"` and `RUN`.
