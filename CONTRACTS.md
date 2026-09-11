## Requirements

| Tool | Required? | Why | Verified with |
|---|---|---|---|
| `cbmc` 6+ | yes | Runs the proofs. CBMC 5.x lacks the contracts support used here; Ubuntu 24.04 ships 5.95, which will not work | `cbmc --version` (6.11.0) |
| `goto-cc` 6+ | yes | Compiles the preprocessed source to a goto binary. Ships with CBMC | `goto-cc --version` |
| `goto-instrument` 6+ | yes | Applies loop contracts, drops unreachable functions, enforces or replaces contracts. Ships with CBMC | `goto-instrument --version` |
| `/usr/bin/cc` | yes | Preprocesses the contract macros to CBMC syntax. Hardcoded path in `prove.sh` | `/usr/bin/cc --version` (gcc 11.5.0) |
| POSIX `sh` | yes | `prove.sh` and `proofs/run.sh` | — |
| `prove.sh` | yes | At the repo root, or a `c-contracts` checkout beside this one, or set `PROVE=` | `./prove.sh` |
| `z3` | no | One of the raced solvers. Without any of these CBMC still uses its built-in SAT backend | `z3 --version` (4.8.15) |
| `bitwuzla`, `cvc5` | no | Also raced if present. More installed solvers means the race has more to pick from | — |

## Running

| What | How |
|---|---|
| Set flags | `CF="-DNDEBUG -DZSTD_NO_INTRINSICS -I lib/common -I lib"` |
| `-DNDEBUG` | Matches the release build. Removes `assert()`, so asserts in the body are **not** checked by any proof here |
| `-DZSTD_NO_INTRINSICS` | Disables the SIMD paths `goto-cc` cannot parse |
| Run all proofs | `./proofs/run.sh` |
| Run one proof | See the Command column in the tables below |

## Legend

| Term | Means |
|---|---|
| mode `enforce` | `--enforce-contract`. CBMC generates the entry point from the contract itself. Preconditions assumed, **postconditions checked**, body verified for every input satisfying the contract. No harness file exists |
| mode `harness` | `-H`. Hand-written entry point in `proofs/*.c`. The body is inlined and checked for memory safety; the function's own contract clauses are **not** checked |
| mode `harness -r` | `-H -r FN`. The call is replaced by `FN`'s contract: preconditions **asserted** at the call site, assigns targets set nondeterministic, **postconditions assumed** |
| harness needed when | Pointers alias into one object. Contracts can describe that (`contract_same_object`, `*op - *ip == offset`) but `contract_fresh` only makes distinct objects, so CBMC cannot construct a witness |
| `contract_post` needs `contract_assigns` | Without it a caller assumes no write, so `*op - *ip` stays `== offset` and the postcondition forces `offset >= 8`, silently deleting the `offset < 8` branch |
| postconditions compare offsets | Replacement makes both pointers nondeterministic; the clauses compare `contract_pointer_offset` integers instead |
| assigns syntax | `contract_assigns (a; b)` — semicolons, not commas. Two clauses `contract_assigns(a) contract_assigns(b)` union on a function but are a syntax error on a loop |

## Undefined behavior discovered

| Where | Code | Why it is UB | Exhibited by |
|---|---|---|---|
| `lib/common/bitstream.h:264` | `bitD->limitPtr = bitD->start + sizeof(bitD->bitContainer)` | Forms `start + 8` before checking `srcSize >= 8`. Pointer past the end of the object when `srcSize < 8`. Stored, never dereferenced. C11 6.5.6p8 | `./prove.sh BIT_initDStream proofs/bitstream.c $CF` — FAIL 2s, enforce mode, so this covers every input satisfying the contract |
| `lib/common/zstd_internal.h:235` | `ptrdiff_t diff = (BYTE*)dst - (const BYTE*)src` | Subtracts pointers into different objects, before the `ovtype` check that would tell them apart. Under `ZSTD_no_overlap` they are different objects by definition. C11 6.5.6p9 | `./prove.sh harness proofs/wildcopy.c -H $CF` — FAIL 5s. Also reached through `./prove.sh harness proofs/safecopy.c -H $CF` — FAIL 15s, by inlining, not a separate defect |
| `lib/decompress/zstd_decompress_block.c:840` | `*ip -= sub2` | Forms a pointer before the start of its object; `:844` then does arithmetic on it. `*ip += 8` restores it before any dereference. C11 6.5.6p8 | `./prove.sh harness proofs/overlapcopy8.c -H $CF` — FAIL 3s |
| `lib/common/bitstream.h` `BIT_lookBits` | — | Documentation defect, not UB. Comment says `maxNbBits==56` on 64-bit, but `BIT_MASK_SIZE` is 32 (`DEBUG_STATIC_ASSERT`) and `BIT_getMiddleBits` indexes `BIT_mask[nbBits]`, reachable off x86_64 only | — |

## Proven correct

| Function | Mode | Verdict | What it establishes | Command |
|---|---|---|---|---|
| `BIT_lookBits` | enforce | PASS 3s | Memory safe for every input satisfying the contract. No harness: pointers never alias, so `contract_fresh` allocates them and the contract generates the entry point | `./prove.sh BIT_lookBits proofs/bitstream.c $CF` |
| `ZSTD_wildcopy` (overlap) | harness | PASS 12s | Overlap geometry is memory safe. Out of `run.sh`: 12s exceeds the 5s budget | `./prove.sh harness proofs/wildcopy_overlap.c -H $CF` |
| `ZSTD_wildcopy` (no overlap, contract) | harness `-r` | PASS 3s | The harness supplies inputs the contract permits | `./prove.sh harness proofs/wildcopy.c -H -r ZSTD_wildcopy $CF` |
| `ZSTD_overlapCopy8` (contract) | harness `-r` | PASS 3s | The harness supplies inputs the contract permits. **Says nothing about the postconditions at `:816-827`** — CBMC checks `ensures` only under `--enforce-contract`, which this function cannot use, and `-DNDEBUG` removes the body's `assert(*op - *ip >= 8)`, so nothing checks them | `./prove.sh harness proofs/overlapcopy8.c -H -r ZSTD_overlapCopy8 $CF` |

## TODO

| Function | File | Blocker |
|---|---|---|
| `ZSTD_overlapCopy8` postconditions | `lib/decompress/zstd_decompress_block.c:816-827` | Stated but unverifiable in the current setup; needs enforce mode, which needs a witness CBMC cannot construct |
| `ZSTD_execSequence` | `lib/decompress/zstd_decompress_block.c:1063` | 8 `contract_pre` clauses, unproven; deep call tree, needs `-r` per annotated callee |
| `ZSTD_execSequenceSplitLitBuffer` | `lib/decompress/zstd_decompress_block.c:1173` | 9 `contract_pre` clauses, unproven; same call tree as `ZSTD_execSequence` |
| `ZSTD_ldm_gear_feed` | `lib/compress/zstd_ldm.c:96` | Annotated; 33 clauses lower cleanly, then enforce mode stops at `has loops without contracts`. Both `while` loops need `contract_invariant` / `contract_decreases` |
| `ZSTD_convertBlockSequences` | `lib/compress/zstd_compress.c:7711` | Annotated; 30 clauses lower cleanly, then enforce mode stops at `has loops without contracts` |

## Open questions

| Where | Question |
|---|---|
| `ZSTD_execSequenceSplitLitBuffer` `oend_w` | `contract_pre (oend_w < oend)` is promoted from an upstream `assert`, but all three call sites (`zstd_decompress_block.c:1678`, `:1985`, `:2028`) pass `litPtr + sequence.litLength - WILDCOPY_OVERLENGTH` — derived from the **literals** buffer, not `oend - WILDCOPY_OVERLENGTH` as the non-split variant computes internally. Relational comparison of pointers into different objects is UB (C11 6.5.8p5). Either the two buffers are always one object here, or this is a fifth entry for the table above. Unresolved |
| `op` extent | Neither `execSequence` function states a writable extent for `op`. `contract_writable(op, (size_t)(oend - op))` is the obvious clause, but the fast path's unconditional `ZSTD_copy16` plus wildcopy slack means the real bound is not yet pinned down. Supplied by harness geometry today, stated nowhere |
