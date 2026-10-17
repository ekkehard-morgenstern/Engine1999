# In-progress refactor: removing size limits in the BASIC tokenizer

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

## Status checklist

Done (in `bastok.[ch]`): buffers, rbuf, uint32 helpers, identifiers, generic literals (`eat/print/emit/read_lit`).

Still on the old API (`const char** pp`, `char[256]`, `size_t* premain`) — these call the converted functions with the wrong arguments and are the immediate compile errors:

- `bastok.[ch]`: `eat_numlit/print_numlit/emit_numlit/read_numlit` and the per-base wrappers (`declit hexlit octlit qualit binlit`), `strlit shllit quolit brklit brclit` wrappers (all are thin shims over the `*_lit` generics), `print_keyword`, `eat_dblchrtok`, `eat_sngchrtok`, `tokenize_line`, `detokenize_line`, `preprocess_buffer` (the last needs no change unless buffers change).
  `tokenize_line` also still declares `char item[256]` and a `numemittbl` of old-style function pointers, and calls `eat_uint16` (removed) for the line number.
- `bascomp.[ch]`: `compiler_t.param[256]` and `comp_ctxstk_t.param[256]` (union with `double number`) and the literal read table at `bascomp.c` ≈ line 3144 (`{ TOK_IDENT, read_ident } …`).
- `basintp.[ch]`: `interpr_t.param[256]` and the `littbl` of `read_*` function pointers.
- `baspgm.c` line ≈453 (`list_program` → `detokenize_line( buf, iter.tok, &remain, &iter.hdr )`), `basictest1.c`, `basictest2.c` (fixed `char buf[1024]`, `uint8_t tokens[1024]`).
- Interface decisions still open: how `tokenize_line` returns its token bytes (probably into a `buf_t`), whether `linehdr_t.length/alloc` (16 bit) must stay 16 bit, and what happens to the 64 KiB `program_t` once individual literals can be up to 65535 bytes (a single maximal literal nearly fills program memory).

Suggested order: finish `bastok.c` (numeric literal layer → keyword/dbl/sng tokens → `tokenize_line`/`detokenize_line`) → get `bastok.o` to compile → `basictest1` → `baspgm.c` → `bascomp` param handling → `basintp` → `basictest2`.

## Known defects in the already-converted code (`bastok.c`)

Verified by compiling `gcc -c -Wall -Werror bastok.c` and by reading:

1. `printto_buffer` passes a `va_list` (`ap`) to `snprintf` — must be `vsnprintf` (the per-retry `va_start`/`va_end` inside the loop is already correct). As written, formatted output is garbage and `-Wformat` may flag it.
2. `eat_lit`: label `STORE:` uses `buf->rpos += n;` — `buf` does not exist; should be `rbuf->rpos`. (The label sits inside the `if` body and is also reached by `goto` from the empty-literal cases below.)
3. `writeto_buffer`: unused variable `oldFill` (breaks the build under `-Werror`).
4. `strdup` is used in `eat_ident`/`eat_lit`, but `stdtypes.h` defines `_POSIX_C_SOURCE 200112L`, which does not declare `strdup` (needs `200809L`). Either bump the macro in `stdtypes.h` or write a small `dup_string()` helper.
5. `read_ident`: `if ( !anticipate_rbuffer( rbuf, 3U + len ) )` — `anticipate_rbuffer` returns `-1` on failure, which is truthy, so the failure is not detected (should be `< 0`).
6. `grow_buffer( buf, suggestedSize )`: callers pass an *absolute* required size (`fill + n`) but the function compares it with the *remaining* space. Pick one meaning (absolute is what the callers assume: `if ( buf->alloc >= suggestedSize ) return true;`).
   Also the overflow guard `suggestedSize > SIZE_MAX` is always false.
7. `eat_uint32` / `eat_ident` / `eat_lit` call `sscanf` directly on `rbuf->buffer + rpos`, which assumes the read buffer is NUL-terminated; `rbuf_t` has a `size`, so this can read past the end. Also `eat_ident`/`eat_lit` use a **static 64 KiB scratch** (`tmp64k`) → not re-entrant, and the 65535 limit is the *new* limit.
8. `read_uint32`: the third byte is cast to `uint16_t` before shifting by 8 (harmless, but should be `uint32_t` for consistency).
9. `emit_ident`: the `len == 65535 → --len` adjustment (reserving room for the sigil) happens after the `anticipate_buffer` call and only for string/integer identifiers; `read_ident` mirrors it. Keep both sides in sync when changing the limit.
10. The `rbuf`/`buf` error paths print to `stderr` directly instead of using a `report` callback like the rest of the BASIC system.
11. `bastok.h` declares `is_sngchrtok` twice (harmless duplication).

## Limits (current values, for planning further "remove limits" work)

| Limit | Value | Where |
|---|---|---|
| Program memory | 64 KiB, 16-bit offsets | `baspgm.h` `MAX_PROGRAMSIZE`, `baslin.h` |
| Line numbers | 0..65534 | `baslin.h` |
| Line length / alloc | 16-bit | `linehdr_t` |
| Identifier / literal length (old) | 255 (`char[256]`) | `bascomp.h`, `basintp.h`, old `bastok.c` |
| Identifier / literal length (new) | 65535 (2-byte prefix) | converted `bastok.c` |
| Syntax tree | 65535 bytes | `bascomp.h` `TREESIZE_MAX` |
| Code / data segments | 64 KiB each | `codegen.h` |
| Variable area / string area | 64 KiB each, ≤ 1024 variables | `vars.h` |
| Array dimensions / UDF params | 6 / 8 | `MAXDIM`, `MAXPARAM` |
| `engine1999` input line | 256 bytes | `engine1999.c` `char buf[256]` |

Most of these follow from using big-endian 16-bit offsets everywhere. Removing them for real means changing offset widths in the program store, tree, variable memory and ISA — a much larger change than the tokenizer work in this branch.
