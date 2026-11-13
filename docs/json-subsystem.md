# JSON trees from BASIC

*2026-11-13, owner's request ("create a feature to support JSON trees from BASIC; trees flattened such that the interface "a.b.c" selects element c from within element b from within element a; load and save"). Modules `json.[ch]` (the tree, parser, writer, paths; no SDL), `jsonops.[ch]` (the BASIC statements on one current document; no SDL), statements `JSON ...` (extension word `XF_JSON` 34, operations `GFXOP_JSON*` 181..195 in `gfxops.[ch]`), tests `jsontest1` (1383 cases, ASan / UBSan clean, fuzzed) and eight `contest1` cases.*

## The tree

A document is a tree of nodes: **null, boolean, number, string, array, object**. Objects keep their members in the order they were written (so a saved file reads like the program that made it), arrays are ordered. Numbers are doubles; the writer prints integers without a point and other numbers with the shortest text that reads back to the same value (15 to 17 digits). Strings are byte strings: the parser turns `\uXXXX` (also surrogate pairs) into UTF-8, other bytes pass through unchanged, and the writer escapes only `"`, `\` and control characters. The parser is strict (RFC 8259): no trailing commas, no comments, no `NaN`; a byte order mark is skipped; a repeated key keeps the last value at the place of the first; nesting up to 100 levels; errors say `... at line n`.

## Paths: the flattened interface

`a.b.c` is member `c` of member `b` of member `a`; array elements are addressed by their index from 0: `rooms.3.name` (also `rooms[3].name`). The empty path is the root. Member names that contain a dot or a bracket can be loaded and saved but not addressed. Writing makes missing parents: a member name makes an object, a number makes an array (an index may be the number of elements: that appends; further out is an error); a path through a string, number or boolean is an error.

## Statements

One current document (`JSON LOAD` / `PARSE` replace it, `JSON CLEAR` empties it).

| Statement | Meaning |
|---|---|
| `JSON CLEAR` | forget the document |
| `JSON LOAD f$` / `JSON SAVE f$ [, pretty]` | read / write a file through the I/O layer (drivers, the security policy); `.json` is added when the name has no extension; pretty = 0 gives one line, the default is indented (arrays of plain values stay on one line) |
| `JSON PARSE t$` | the document is the text t$ |
| `JSON SET p$, n` / `SETS p$, t$` / `SETB p$, flag` / `NULL p$` | set a number / string / boolean / null |
| `JSON NEW p$, OBJECT` or `ARRAY` | an empty object / array |
| `JSON DELETE p$` | remove the element (no error if there is none) |
| `JSON GET p$, v$` | the value as text: a string as it is, a number as text, `true` / `false`, null and missing as "", a container as JSON |
| `JSON TYPE p$, v$` | `null` `boolean` `number` `string` `array` `object`, "" if there is no such element |
| `JSON COUNT p$, v$` | number of elements / members (0 for anything else) |
| `JSON KEY p$, i, v$` | the name of member i (from 0) of an object, "" if none |
| `JSON TEXT p$, v$` | the element as compact JSON text (error if missing; at most 65535 bytes: save big documents with `JSON SAVE`) |

Results come back in string variables (the statement table has no numeric results): use `VAL(v$)`. Quote characters in strings: `CHR$(34)`.
A statement with two strings (`SETS`) hands them to the host in one buffer, the first, a NUL byte, the second (`vm_gfx_command` in `vm.c`).

## Design decisions I took (revisit if unwanted)

- Statements with string results instead of functions, because the graphics-table machinery already has them (no new VM instruction, the monitor and the logs name them); numbers come back as text.
- One current document, not handles. (Several documents would be a `JSON USE n` or a handle; not needed so far.)
- Not done: comments / trailing commas in files, big-number precision beyond a double, streaming of huge texts through `JSON TEXT`, copying a subtree to another path, iterating with one call.
