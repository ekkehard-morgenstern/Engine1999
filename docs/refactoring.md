# Refactor: removing size limits in the BASIC tokenizer (completed)

Branch: `feature/refactor-to-remove-some-limits` (last commits `266c4bd`, `18a485c`, both titled "Refactor WIP (bastok)", 2026-09-05; base is the "Codegen WIP" series ending at `d608ca3`, 2026-06-27).
Commit messages carry no description; the intent below is inferred from the diffs.

## Goal

The tokenizer layer used fixed 256-byte buffers (`char target[256]`) and caller-managed output cursors (`uint8_t** pp`, `size_t* premain`), which limit identifiers/literals to 255 bytes
and make every emit function responsible for bounds checks. The refactor:

1. Adds a growable output buffer `buf_t { char* buffer; size_t alloc, fill; }` (initial 64 KiB, doubles) with `create_buffer`, `delete_buffer`, `grow_buffer`, `printto_buffer`, `writeto_buffer`, `anticipate_buffer`.
2. Adds a bounds-checked read buffer `rbuf_t { const char* buffer; size_t size, rpos; }` with `init_rbuf`, `readfrom_buffer`, `anticipate_rbuffer`.
3. Changes strings to heap-allocated `char**` out-parameters (callee frees the old value and `malloc`/`strdup`s the new one) instead of `char[256]`.
4. Widens literal/identifier length prefixes from 1 byte to 2 bytes (max 65535) — **a token-format change**; old saved/tokenized data is incompatible.
5. Replaces the `uint16` helper set with `uint32` (`eat/print/emit/read_uint32`, big endian) — apparently for line numbers or larger offsets (not yet used).

New signatures (in `bastok.h`):

```c
bool eat_ident ( rbuf_t* rbuf, char** ptarget );      bool print_ident( buf_t* buf, const char* source );
bool emit_ident( buf_t* buf, const char* source, uint8_t tok );   bool read_ident ( rbuf_t* rbuf, char** ptarget );
bool eat_lit   ( rbuf_t* rbuf, char** ptarget, int beg, int end );  bool print_lit( buf_t* buf, const char* source, int beg, int end );
bool emit_lit  ( buf_t* buf, const char* source, int tok );       bool read_lit  ( rbuf_t* rbuf, char** ptarget, int tok );
```

## Status: refactor complete (builds clean, valgrind clean)

Final API (all in `bastok.h`, documented there):

- `tokenize_line( const char* text, buf_t* out, linehdr_t* phdr )` appends header + tokens to `out`; on failure `out` is unchanged. Lines are capped at 65535 bytes (16-bit header length).
- `detokenize_line( buf_t* out, const uint8_t* tokens, const linehdr_t* hdr )` appends text and NUL-terminates (the NUL is not counted in `fill`); the token count comes from `hdr->length`.
- `eat_*( rbuf_t*, char** )`, `print_*( buf_t*, const char* )`, `emit_*( buf_t*, const char* )`, `read_*( rbuf_t*, char** )` for identifiers and all literal kinds; `eat_sngchrtok/eat_dblchrtok` take an `rbuf_t*`; `print_keyword( tok, buf_t* )`.
- `clear_buffer()` added. `grow_buffer( buf, requiredSize )` now means "ensure alloc >= requiredSize"; `anticipate_buffer( buf, n )` means "room for n more bytes".
- `emit_uint16/read_uint16` (raw pointer variants) were kept for the 16-bit fields of the line header in `baslin.c` (line numbers use `emit_uint24/read_uint24`). `eat/print/emit/read_uint32` are used for line numbers (`eat_uint32`, `print_uint32`).

Callers adapted:

- `baspgm`: `list_program` uses a `buf_t`; `enter_line`'s direct-mode callback is now `bool (*)( program_t*, const uint8_t* tokens, size_t size )`; new `iter_remaining()` gives the token bytes left in the loaded line.
- `bascomp`: `compiler_t.param` is a heap `char*` (no longer a union with `number`; `number` is a separate field); new `tokend` marks the end of the token bytes (set by `comp_nextline` or by the new `comp_set_tokens()`); new `cleanup_compiler()` (call it before re-`init_compiler` on a reused compiler and at exit). Node data for names/string literals is heap built (`comp_param_data`) so names up to 65534 bytes fit the tree; context push/pop copies `param`.
- `basintp`: same `param`/`tokend` change plus `cleanup_interpr()`; `intp_fetchtok` now backs up to the tag byte before calling `read_*` (the old code passed the position after the tag, which the readers reject). The numeric-conversion code below the `littbl` loop is still unreachable, as before.
- `basictest1/2`: read input with `getline`, tokenize into `buf_t`. `stdtypes.h`: `_POSIX_C_SOURCE` raised to 200809L (needed for `getline`) and `<limits.h>` added.

Behavior changes worth knowing:

- Token format: identifier/literal length prefix is now 2 bytes (was 1). Previously tokenized data is incompatible.
- Unterminated string/shell/bracket/brace literals are now a syntax error (the old `sscanf` approach silently accepted them without advancing).
- `eat_*` never move the read position when they fail (the WIP version swallowed leading blanks).
- `SAVE a, B` option name is copied before fetching the next token (it used to rely on a stale `param`).

Bugs from the earlier WIP review (all resolved): `vsnprintf`, `rbuf` typo in `eat_lit`, `strdup` declaration, `read_ident` failure check, `grow_buffer` semantics, unbounded `sscanf`/static scratch buffer (replaced by manual scanning), `uint32` cast, unused variable.

Verified by hand: tokenize/detokenize round trip of ordinary lines, 3000-byte identifiers and 5000-byte string literals, rejection of a 70000-byte literal and of an unterminated literal, and parse of PRINT/LET/GOTO/numeric literals through `basictest2` — valgrind reports no errors or definite leaks. There is no automated test suite.

## Limits (current values, for planning further "remove limits" work)

| Limit | Value | Where |
|---|---|---|
| Program memory | none (heap linked list of lines; a single line ≤ 65535 bytes) | `baspgm.h`, `baslin.h` |
| Line numbers | 0..999999 (24 bit) | `baslin.h` |
| Line length | 16-bit | `linehdr_t` |
| Identifier / literal length | 65535 (2-byte prefix); was 255 | `bastok.c` |
| Variable / label name length | 255 (`namelen.8`) — a 256+ byte `LABEL` name ends in "Internal error (invalid parameter to vmem_create_var())" | `vars.c` |
| Whole tokenized line | 65535 bytes | `linehdr_t.length` |
| Syntax tree | 65535 bytes | `bascomp.h` `TREESIZE_MAX` |
| Code / data segments | 64 KiB each | `codegen.h` |
| Variable area / string area | 64 KiB each, ≤ 1024 variables | `vars.h` |
| Array dimensions / UDF params | 6 / 8 | `MAXDIM`, `MAXPARAM` |
| `engine1999` input line | 256 bytes | `engine1999.c` `char buf[256]` (not part of this refactor) |

Most of these follow from using big-endian 16-bit offsets everywhere. Removing them for real means changing offset widths in the program store, tree, variable memory and ISA — a much larger change than the tokenizer work in this branch.
