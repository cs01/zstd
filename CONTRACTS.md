| Term | Means |
|---|---|
| `CF` | Flags every command below passes to `prove.sh`, which forwards them to `cc -E` and `goto-cc`: `CF="-DNDEBUG -DZSTD_NO_INTRINSICS -I lib/common -I lib"` |
| `-DNDEBUG` | Matches the release build. Removes `assert()`, so asserts in the body are **not** checked by any proof here |
| `-DZSTD_NO_INTRINSICS` | Disables the SIMD paths `goto-cc` cannot parse |
| mode `enforce` | `--enforce-contract`. CBMC generates the entry point from the contract itself. Preconditions assumed, **postconditions checked**, body verified for every input satisfying the contract. No harness file exists |
| mode `harness` | `-H`. Hand-written entry point in `proofs/*.c`. The body is inlined and checked for memory safety; the function's own contract clauses are **not** checked |
| mode `harness -r` | `-H -r FN`. The call is replaced by `FN`'s contract: preconditions **asserted** at the call site, assigns targets set nondeterministic, **postconditions assumed**. Checks that the harness supplies inputs the contract permits |
| `PASS n s` | `VERIFICATION SUCCESSFUL` in n seconds |
| `FAIL n s` | `VERIFICATION FAILED`. For every row below this is the proof working: it exhibits a real defect, and a `PASS` there would mean the demonstration broke |
| `TODO` | Not proven yet |
| run everything | `./proofs/run.sh` — the recorded cases only, all under 5s |

| Function | Mode | Verdict | Finding | Command |
|---|---|---|---|---|
| `BIT_lookBits` | enforce | PASS 3s | Memory safe for every input satisfying the contract. No harness: pointers never alias, so `contract_fresh` allocates them and the contract generates the entry point | `./prove.sh BIT_lookBits proofs/bitstream.c $CF` |
| `BIT_initDStream` | enforce | FAIL 2s | **Defect.** `bitstream.h:270` forms `start + 8` before checking `srcSize >= 8`; out of bounds when `srcSize < 8`, stored but never dereferenced | `./prove.sh BIT_initDStream proofs/bitstream.c $CF` |
| `ZSTD_wildcopy` (no overlap) | harness | FAIL 5s | **Defect.** `zstd_internal.h:245` computes `dst - src` before the `ovtype` check; under `ZSTD_no_overlap` those are different objects | `./prove.sh harness proofs/wildcopy.c -H $CF` |
| `ZSTD_wildcopy` (no overlap, contract) | harness `-r` | PASS 3s | Harness supplies inputs the contract permits | `./prove.sh harness proofs/wildcopy.c -H -r ZSTD_wildcopy $CF` |
| `ZSTD_wildcopy` (overlap) | harness | PASS 12s | Overlap geometry is memory safe. Out of `run.sh`: 12s exceeds the 5s budget | `./prove.sh harness proofs/wildcopy_overlap.c -H $CF` |
| `ZSTD_overlapCopy8` | harness | FAIL 3s | **Defect.** `zstd_decompress_block.c:834` (`*ip -= sub2`) forms a pointer before the start of its object; `:838` then does arithmetic on it. `*ip += 8` restores it before any dereference | `./prove.sh harness proofs/overlapcopy8.c -H $CF` |
| `ZSTD_overlapCopy8` (contract) | harness `-r` | PASS 3s | Preconditions only. **Postconditions at `:818-828` are assumed, never checked** — CBMC checks `ensures` only under `--enforce-contract`, and this function cannot use it. Body's `assert(*op - *ip >= 8)` is removed by `-DNDEBUG` | `./prove.sh harness proofs/overlapcopy8.c -H -r ZSTD_overlapCopy8 $CF` |
| `ZSTD_safecopy` | harness | FAIL 15s | Reports `zstd_internal.h:245`, i.e. `ZSTD_wildcopy`'s `diff` reached by inlining, not `safecopy`'s own `op - ip`. Out of `run.sh`: duplicate finding, and 15s | `./prove.sh harness proofs/safecopy.c -H $CF` |
| *harness needed?* | — | — | Only where pointers alias into one object. Contracts can describe that (`contract_same_object(*ip, *op)`, `(*op - *ip) == offset`) but `contract_fresh` only makes distinct objects, so CBMC cannot construct a witness and needs `-H` | — |
| *`contract_post` needs `contract_assigns`* | — | — | Without it a caller assumes no write, so `*op - *ip` stays `== offset` and the postcondition forces `offset >= 8`, silently deleting the `offset < 8` branch. Verify with a probe asserting `offset >= 8` after the call: it must fail | — |
| *postcondition form* | — | — | Replacement makes both pointers nondeterministic and comparing those is not checkable, so the clauses compare `contract_pointer_offset` integers | — |
| *assigns syntax* | — | — | `contract_assigns (a; b)` works on function and loop clauses. Two clauses `contract_assigns (a) contract_assigns (b)` union on a function but are a syntax error on a loop. `contract_assigns (a, b)` is a preprocessor error | — |
| `ZSTD_execSequence` | — | TODO | 7 `contract_pre` clauses at `zstd_decompress_block.c:1052`, unproven; deep call tree, needs `-r` per annotated callee | — |
| `ZSTD_execSequenceSplitLitBuffer` | — | TODO | No `contract_pre`; `assert(op != NULL /* Precondition */)` only | — |
| `ZSTD_ldm_gear_feed` | — | TODO | Precondition in a comment only (`lib/compress/zstd_ldm.c`) | — |
| `ZSTD_convertBlockSequences` | — | TODO | Precondition in a comment only (`lib/compress/zstd_compress.c`) | — |
| *doc defect* | — | — | `BIT_lookBits` comment says `maxNbBits==56` on 64-bit, but `BIT_MASK_SIZE` is 32 (`DEBUG_STATIC_ASSERT`) and `BIT_getMiddleBits` indexes `BIT_mask[nbBits]`, reachable off x86_64 only | — |
