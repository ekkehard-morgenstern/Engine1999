# Program store, compiler, code generator, VM

## Program memory (`baslin.[ch]`, `baspgm.[ch]`)

`program_t` = `first`/`last` pointers of a doubly linked list of heap-allocated `line_t` nodes (`next`, `prev`, `lineno`, `toklen`, flexible `tokens[]`), sorted by line number. `init_program`/`free_program` manage it; `find_line`, `create_line`, `link_line`, `unlink_line`, `delete_line` and `enter_line` (insert/replace/delete) operate on it. `pgmiter_t` walks the list and exposes the current line as `tok` + `hdr`.
`linehdr_t{lineno, length}` is only the *encoded* header of a tokenized line (`LINEHDR_SIZE` = 5 bytes: `lineno.24 length.16`, big endian), produced by `tokenize_line` and consumed by `enter_line`; it is not stored in the program.

Key operations: `init_program`, `find_line` (returns `FOUND_EXACT/INSERT/BEYOND/NONE/ERROR` with neighbours), `create_line`, `emplace_line`, `update_line`, `unlink_line`, `quick_append_line`,
`defrag_memory`, `enter_line` (the user-facing entry; empty line body deletes, no number → direct-mode callback), iteration via `pgmiter_t` (`begin_iterate_program`/`step_iterate_program`), and `list_program`.

## Syntax tree (`bascomp.[ch]`, `nodetype.[ch]`)

`compiler_t` holds token cursor state (`iter`, `tokp`, `tokend`, `currtok`, heap `param`, `number`; set the token range with `comp_set_tokens()`, release with `cleanup_compiler()`), a context stack for backtracking (`comp_push_context` / `comp_commit_context` / `comp_pop_context`) and the syntax tree (`syntree` root; `nodes` chains every allocated node).

`node_t` = `{ chain, branches (growable array of node_t*), numbranches, maxbranches, datalen.16, type.8, data[] }`, one heap allocation per node (branch array separate). Branch count is unlimited; a branch may be `NODE_NONE`, standing for an optional branch that is absent. `chain` links all nodes of a compiler so abandoned parses are freed too (`comp_free_tree`, called by `cleanup_compiler`).
Create with `comp_create_node(comp, &node, type, nbranches, datalen, data, branch…)` (variable arguments are `node_t*`), append branches with `comp_add_branch`, iterate with `comp_node_iter_branches`; consumers read `node->type`, `node->data`, `node->datalen`, `node->branches[i]`.
Generic list parsing helpers: `comp_eat_list` / `comp_eat_list2` (separator tokens, operator-precedence flavour).

`comp_eat_*( comp, node_t** pnode )` return `false` on hard failure; a successful call may leave `*pnode == NODE_NONE` meaning "this rule did not match" (callers test both).
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

## VM (`vm.[ch]`)

Minimal executor: `init_vm( vm, &codegen, &varmem )`, `vm_run( vm )` (runs from offset 0 to the end of the code, returns `false` after reporting an error), `cleanup_vm`. Stacks are heap arrays that grow on demand; data stack items are tagged number/string (heap, NUL-terminated), so type mismatches are run time errors and strings are freed on errors. Output goes through `vm->print( vm, userdata, channel, text )` (default stdout), errors through `vm->report` (default stderr). `vm->maxsteps` guards against endless loops.

Implemented: control (`NOP PHPA PHIM BRIA BRIR JPCC JUMP DROP LINE`), loads (`LAN LIAN LUAN LAS FRES`), all arithmetic/logical/compare/concat instructions, scalar variable read/write (`RRNV RRIV RRSV RRLV WRNV WRIV WRSV`), array element access (`RNAE RIAE RSAE WNAE WIAE WSAE`: subscripts must be in range, else "Subscript out of range"), user function calls (`CALF RETF`), `PCHN PNUM PSTR`. System functions: `TI INKY FRE ASC LEN VAL STRS BINS QUAS OCTS HEXS DECS LFTS RGTS MIDS` (details and edge cases in the comment of `vm.h`; `INKEY$` reads from the `vm->inkey` callback, `TI` counts 1/60 s ticks since the VM was created). `SHEX` (a backtick shell literal) runs the command with `popen()` and returns its standard output without the trailing newlines, like a shell command substitution (exit status ignored; more than 65535 bytes of output is an error). `SAVE` ( mode name -- ) can't be done by the VM alone, because the program belongs to the host: after checking the mode (`""`/`B` binary, `A` ascii) the VM calls `vm->save( vm, userdata, filename, ascii )`; without that callback `SAVE` is an error. `baspgm.c` provides `save_program( pgm, FILE*, ascii )` for the host: ascii is the listing (one line per program line), binary is the encoded lines (`linehdr_t` of `LINEHDR_SIZE` bytes + tokens, no file signature yet); there is no loader yet. Not implemented (reported as "Instruction XXX not implemented"): `WRLV`, `BRK` (reports "Breakpoint"), and `SLFT SRGT SMID` — they take `( s s n -- )` but strings on the stack are copies, so they cannot change a variable; the instruction set needs a variable parameter for them.

Optional arguments: the parser supplies a literal `-1` for an omitted second argument of `BIN$ QUA$ OCT$ HEX$ DEC$` (digits) and an omitted third argument of `MID$` (length), so the instructions always get all their arguments; `cgen_from_expr` accepts such a bare `NT_NUMLIT`.

`CALF` pops the argument count and the arguments, saves the current values of the parameter variables (strings as heap copies) in a context kept by the VM (`vm_context_t`, indexed by a 16-bit number), assigns the arguments to the parameter variables, pushes `<return address> <context index>` on the return stack and branches to the function. `RETF` restores the variables and returns; the result stays on the data stack. `vm_run_at( vm, start )` runs from a given code offset (needed when direct mode lines are appended to the code).

Policies the ISA leaves open (documented in `vm.h`): comparisons give -1/0; bitwise ops work on 32-bit signed integers; print separator parameter 0 = newline, 4 = `;`, 8 = `,` (next 10-column zone); numbers print with `%.15g`.

Code generator driver: `cgen_from_program( cgen, comp )` handles `NT_STMTLINES` / `NT_STMTLINE` / `NT_STMTLIST` and the statements `LET`, `PRINT`, `SAVE` (`codegen.c`) plus the control flow statements below (`cgenctl.c`); `ERASE` (and multi-line `DEF FN` bodies) report "feature not implemented yet". `LINE` has a 16-bit parameter, so line numbers above 65535 are rejected by code generation for now. Call `cgen_reset()` before generating into a reused `codegen_t` and `cleanup_codegen()` / `cleanup_runtime()` when done.

### Declarations (`cgendecl.c`)

- **`DIM`** generates nothing: the compiler creates the array when it parses the declaration (constant dimensions; `DIM A(n)` gives subscripts `0..n-1`), and an array used without `DIM` is created with 10 elements. A second `DIM` of the same array is a compile error ("Variable already exists"), so arrays can't be re-dimensioned.
- **`ERASE`** is not implemented: the parser itself notes the unresolved design problem (the array variable already exists at compile time) and the ISA has no instruction for it.
- **`DEF FN`**: the single-line body is generated inline, jumped over (`BRIA end  start: <expr>  RETF  end:`); the start offset is stored in the function variable at compile time. The function must be defined *before* the call in the program text (the parser looks it up while parsing the call), and a function can't call itself (it doesn't exist while its body is parsed). The parameter variables are created if needed. Array parameters and multi-line bodies aren't supported.
- **Spelling:** the tokenizer reads `FNA` as one identifier, so a function is written `DEF FN A(X)=…` and called as `FN A(…)` (with a space).

### Control flow (`cgenctl.c`)

- **Line table and fixups:** every numbered line is recorded (`cgen_note_line`, line number → code offset). `GOTO`/`GOSUB`/`IF … <line>` emit a `BRIA` that is patched by `cgen_finish_control()` when the whole program is known; an unknown target is the compile error "Undefined line number N in line M". Consequence: a line can only jump to lines of the same compilation (direct mode can't `GOTO` a stored program line yet).
- **`GOSUB`/`RETURN`:** `GOSUB` pushes the return address (`PHPA.C`, patched) then branches; `RETURN` is `JUMP`.
- **Labels:** a label is a variable holding a code offset, set at compile time by `LABEL`. `GOTO`/`GOSUB label` do `RRLV` (pushes the offset on the return stack) + `JUMP`; a label that is never defined fails at run time ("Undefined label").
- **`IF`/`UNLESS`:** `<cond> PHPA.C then JPCC <else part> BRIA end then: <then part> end:`; used for the single-line forms (`THEN <line>`, `GOTO`/`GOSUB <line|label>`, `THEN <stmt-list> [ELSE <stmt-list>]`) and the multi-line form. `UNLESS` compares the condition with 0.
- **`FOR`/`NEXT`:** matched at compile time (lexically): `NEXT` closes the innermost open `FOR`, which must use the named variable (`NEXT` alone, `NEXT I` and `NEXT J,I` are accepted; mismatches are "NEXT without FOR" / "FOR without NEXT"). Limit and step live in hidden variables `@FOR<n>L` / `@FOR<n>S`; the loop test happens *before* the first pass (a loop with `FROM` past `TO` runs zero times). The loop variable may be numeric or integer.
- **Parser fixes made for this:** the `IF` nodes (single- and multi-line) now carry the condition expression as their first branch (it was dropped), and a multi-line `ELSE` now consumes the end of line after it.

## Runtime (`runtime.[ch]`)

`init_runtime` wires `varmem_t` and `codegen_t` to the runtime's `report`/`halt`. (The former `basintp` token interpreter stub was removed: the BASIC system compiles, it does not interpret.)