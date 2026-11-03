# Engine1999 documentation

Engine1999 is a 2D game engine in C on top of SDL2. It emulates a retro computer — a layered
80x25 text / tile / graphics / sprite display, a synthesizer, files and devices — and has a built-in BASIC dialect (tokenizer → compiler → code generator → VM)
for writing games; `BUILD` makes standalone executables. It is a work in progress: see [roadmap.md](roadmap.md) for what is open.
Players and BASIC programmers: start with the [README](../README.md). Engine contributors: [contributing.md](contributing.md).

| Document | Contents |
|---|---|
| [architecture.md](architecture.md) | Module map, data flow, how the pieces fit together |
| [sound.md](sound.md) | Software synthesizer (`synth`, `sdlaudio`): channels, wave types, timed events, proposal for `SOUND`/`PLAY`, FM plans |
| [display-engine.md](display-engine.md) | Layers, text/tile/sprite screens, SDL threading/event model, audio |
| [basic-language.md](basic-language.md) | Tokenized line format, binary program file (`.e99`) format, token set, literals, grammar summary |
| [compiler-and-vm.md](compiler-and-vm.md) | Program store, syntax tree, node types, code generator, instruction set, variable memory |
| [grammar.md](grammar.md) | **The language at a glance**: the whole syntax as a quick reference card, generated from `bascomp.ebnf` (also [grammar.html](grammar.html), with colors and links) |
| [build-and-test.md](build-and-test.md) | Building, executables, test programs, tooling |
| [graphics-subsystem.md](graphics-subsystem.md) | BASIC statements for layers, palettes, the graphics layer, tiles, sprites, `DESIGN` pictures, collisions and text screen areas; the `sdlscr_*` API (implemented) |
| [console-host.md](console-host.md) | The console host `engine1999c`: the BASIC system without a window for scripts and pipes (input, output, exit status, what the display statements do) |
| [library-support.md](library-support.md) | Library support (implemented): `LOAD LIBRARY` in programs, `LIBRARY` files, `SAVE LIBRARY`, `LIBRARIES`, compile order, names, `BUILD`, the shared libraries of the demos |
| [io-subsystem.md](io-subsystem.md) | I/O subsystem: virtual device drivers (`file:`, `mem:`, ...), `driver:name` prefixes, `OPEN`/`CLOSE`, `ADDRESS`; phases and the open sandboxing issue |
| [controller-support.md](controller-support.md) | **Planned**: game controllers and joysticks: what works on a PC (Xbox, PS4, Switch pads), SDL's game controller API, open design points, how the support will be tested (also without hardware), trying a real pad |
| [contributing.md](contributing.md) | **For contributors**: who contributed, how to build and test, how a new statement goes in, conventions, the license of contributions |
| [screen-editor-proposal.md](screen-editor-proposal.md) | **Proposal (not implemented)**: a C-64 style screen editor for text screen 0: understanding, evaluation, design and the decisions needed |
| [roadmap.md](roadmap.md) | **Open work**: everything that is still to do, with the design questions that are waiting for a decision |
| [refactoring.md](refactoring.md) | **Current in-progress refactor**: goal, status, known bugs, remaining work, limits table |

Primary sources of truth in the code: `concept.txt` (display design), `bascomp.ebnf` (grammar),
`instset.h` (instruction set), `nodetype.h` (syntax tree nodes), `vars.h` (variable memory layout).
Where this documentation was inferred from code rather than stated in the code, it says so.
