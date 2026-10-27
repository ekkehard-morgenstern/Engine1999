# Engine1999 documentation

Engine1999 is a 2D game engine in C on top of SDL2. It emulates a retro computer — a layered
80x25 text / tile / graphics / sprite display — and is meant to host a built-in BASIC dialect for writing games.
The display side is working; the BASIC side (tokenizer → compiler → code generator → VM) is under construction.

| Document | Contents |
|---|---|
| [architecture.md](architecture.md) | Module map, data flow, how the pieces fit together |
| [display-engine.md](display-engine.md) | Layers, text/tile/sprite screens, SDL threading/event model, audio |
| [basic-language.md](basic-language.md) | Tokenized line format, token set, literals, grammar summary |
| [compiler-and-vm.md](compiler-and-vm.md) | Program store, syntax tree, node types, code generator, instruction set, variable memory |
| [build-and-test.md](build-and-test.md) | Building, executables, test programs, tooling |
| [roadmap.md](roadmap.md) | **Open work**: everything that is still to do, with the design questions that are waiting for a decision |
| [refactoring.md](refactoring.md) | **Current in-progress refactor**: goal, status, known bugs, remaining work, limits table |

Primary sources of truth in the code: `concept.txt` (display design), `bascomp.ebnf` (grammar),
`instset.h` (instruction set), `nodetype.h` (syntax tree nodes), `vars.h` (variable memory layout).
Where this documentation was inferred from code rather than stated in the code, it says so.
