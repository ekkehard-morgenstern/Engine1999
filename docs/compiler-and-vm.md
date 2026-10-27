# Program store, compiler, code generator, VM

## Program memory (`baslin.[ch]`, `baspgm.[ch]`)

`program_t` = `first`/`last` pointers of a doubly linked list of heap-allocated `line_t` nodes (`next`, `prev`, `lineno`, `toklen`, flexible `tokens[]`), sorted by line number. `init_program`/`free_program` manage it; `find_line`, `create_line`, `link_line`, `unlink_line`, `delete_line` and `enter_line` (insert/replace/delete) operate on it. `pgmiter_t` walks the list and exposes the current line as `tok` + `hdr`.
`linehdr_t{lineno, length}` is only the *encoded* header of a tokenized line (`LINEHDR_SIZE` = 5 bytes: `lineno.24 length.16`, big endian), produced by `tokenize_line` and consumed by `enter_line`; it is not stored in the program.

Key operations: `init_program`, `find_line` (returns `FOUND_EXACT/INSERT/BEYOND/NONE/ERROR` with neighbours), `create_line`, `emplace_line`, `update_line`, `unlink_line`, `quick_append_line`,
`defrag_memory`, `enter_line` (the user-facing entry; empty line body deletes, no number → direct-mode callback), iteration via `pgmiter_t` (`begin_iterate_program`/`step_iterate_program`), and `list_program`.

## Syntax tree (`bascomp.[ch]`, `nodetype.[ch]`)

`compiler_t` holds token cursor state (`iter`, `tokp`, `tokend`, `currtok`, heap `param`, `number`; set the token range with `comp_set_tokens()`, release with `cleanup_compiler()`), a context stack for backtracking (`comp_push_context` / `comp_commit_context` / `comp_pop_context`) and the tree arena `tree[65535]`.

Node layout (big endian): `nodetype.8 numbranches.8 datalen.16 firstbranch.16 lastbranch.16 data…` (8-byte header, `NODEHDR_SIZE`); each branch entry is `nodepos.16 nextbranch.16` (4 bytes).
Create with `comp_create_node(comp, &off, type, nbranches, datalen, data, …)`, link with `comp_add_branch`, iterate with `comp_node_iter_branches`.
Generic list parsing helpers: `comp_eat_list` / `comp_eat_list2` (separator tokens, operator-precedence flavour).

`comp_eat_*( comp, uint16_t* pnodeoffs )` return `false` on hard failure; a successful call may leave `*pnodeoffs == NODEOFFS_NONE` meaning "this rule did not match" (callers test both).
`comp_error` reports and halts. `run_compiler` is the driver; `begin_comp` primes the first token.

Node types (`NT_*`, documented per type in `nodetype.h` including the *immediate processing* each does): expression lists, array subscripts/declarations, variable refs (base vs full),
literals, user/system function names and calls, the expression-level nodes (`NT_STRADDEXPR`, `NT_NUMUNARYEX … NT_NUMOREX`, `NT_NUMCMPEX`), statements (`NT_PRINTSTMT`, `NT_LETSTMT`, `NT_DIMSTMT`, `NT_FORSTMT`, `NT_GOTOSTMT`, …) and containers (`NT_STMTLIST/LINE/LINES`).
`nodename()` gives printable names for debugging.

## Variable memory (`vars.[ch]`)

`varmem_t`: 64 KiB variable area + 64 KiB string area, max 1024 variables. The first 2 KiB of variable memory is an index of 16-bit offsets (deleted = `0xFFFF`); variables are laid out sequentially; the area is compacted when full;
strings live in a separate area that is also compacted on exhaustion.

Variable record: `size.16 type.8 namelen.8 name… [numdims.8 (dim.16 slice.16)… | numargs argdesc…] data…`.
Type byte: bit 5 = user function, bit 4 = array, low bits = element type (`0` float64, `1` int16, `2` string = 16-bit offset + 16-bit length, `3` label = 16-bit code offset).

API: `vmem_create_var / lookup_var / delete_var / get_var / set_var`, `vmem_alloc_vars / alloc_strs`. `testcomp1.c` is a randomized stress test of this subsystem.

## Code generator (`codegen.[ch]`)

`codegen_t`: `code[65536]`, `data[65536]`, sizes, callbacks. Low level: `cgen_alloc_code/data`, `cgen_gen_ins` (4-bit basic instr + optional extension + 16-bit param), `cgen_gen_ins12`,
`cgen_gen_ins12_imm17`, `cgen_gen_ins4_imm17`. One `cgen_gen_<mnemonic>` wrapper per instruction. Tree walkers `cgen_from_<rule>` mirror the parser rules.

## Instruction set (`instset.h`, the spec)

Stack VM, **two stacks**: data stack of 64-bit doubles (strings are heap pointers that instructions free), return stack of 16-bit code addresses. Forth-style stack comments in the spec.

Encoding: `<ins> [<ext>] [<hi><lo>]`. Instruction byte = `P C E r | iiii` (P: has 16-bit param, C: param/stack is code vs data, E: extended 12-bit opcode follows).
Basic opcodes: `BRK NOP PHPA PHIM BRIA BRIR JPCC JUMP DROP LINE LAN LIAN LUAN LAS FRES` (15 = reserved).
Extended: arithmetic/logic (`NEG NOT LSH RSH ADD SUB MUL DIV AND NAND OR NOR XOR XNOR POW`), `CON` (string concat), numeric/string compares (`CNEQ…CSLT`),
variable access (`RRNV RRIV RRSV RRLV RNAE RIAE RSAE`, `WR*`/`W*AE`), calls (`CALF RETF`), and BASIC runtime primitives (`TI INKY ASC VAL FRE LEN BINS … MIDS SLFT SRGT SMID SAVE PCHN PNUM PSTR`). See `instset.h` for the full table and bit layouts.
Immediates: 17-bit signed (`PHIM`, C bit = bit 16) so small integers need no data segment.

There is no executor for this ISA in the repo yet.

## Runtime (`runtime.[ch]`) and interpreter stub (`basintp.[ch]`)

`init_runtime` wires `varmem_t` and `codegen_t` to the runtime's `report`/`halt`. `basintp` is an earlier direct token interpreter skeleton
(`direct_mode`, `init_interpr`, `cleanup_interpr`, `intp_fetchtok`, `run_interpr`) with `setjmp` ready/exit jumps; it reads literals through the same `read_*` API.
