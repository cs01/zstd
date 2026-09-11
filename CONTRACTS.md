| Term | Means |
|---|---|
| `CF` | Flags every command passes to `prove.sh`, which forwards them to `cc -E` and `goto-cc`: `CF="-DNDEBUG -DZSTD_NO_INTRINSICS -I lib/common -I lib"` |
| `-DNDEBUG` | Matches the release build. Removes `assert()`, so asserts in the body are **not** checked by any proof here |
| `-DZSTD_NO_INTRINSICS` | Disables the SIMD paths `goto-cc` cannot parse |
| mode `enforce` | `--enforce-contract`. CBMC generates the entry point from the contract itself. Preconditions assumed, **postconditions checked**, body verified for every input satisfying the contract. No harness file exists |
| mode `harness` | `-H`. Hand-written entry point in `proofs/*.c`. The body is inlined and checked for memory safety; the function's own contract clauses are **not** checked |
| mode `harness -r` | `-H -r FN`. The call is replaced by `FN`'s contract: preconditions **asserted** at the call site, assigns targets set nondeterministic, **postconditions assumed** |
| harness needed when | Pointers alias into one object. Contracts can describe that (`contract_same_object(*ip, *op)`, `(*op - *ip) == offset`) but `contract_fresh` only makes distinct objects, so CBMC cannot construct a witness |
| `contract_post` needs `contract_assigns` | Without it a caller assumes no write, so `*op - *ip` stays `== offset` and the postcondition forces `offset >= 8`, silently deleting the `offset < 8` branch. Verify with a probe asserting `offset >= 8` after the call: it must fail |
| postconditions compare offsets | Replacement makes both pointers nondeterministic and comparing those is not checkable, so the clauses compare `contract_pointer_offset` integers |
| assigns syntax | `contract_assigns (a; b)` works on function and loop clauses. Two clauses `contract_assigns (a) contract_assigns (b)` union on a function but are a syntax error on a loop. `contract_assigns (a, b)` is a preprocessor error |
| run everything | `./proofs/run.sh` — the recorded cases only, all under 5s |

## Defects

| Where | Code | Why it is UB | Exhibited by |
|---|---|---|---|
| `lib/common/bitstream.h:270` | `bitD->limitPtr = bitD->start + sizeof(bitD->bitContainer)` | Forms `start + 8` before checking `srcSize >= 8`. Pointer past the end of the object when `srcSize < 8`. Stored, never dereferenced. C11 6.5.6p8 | `./prove.sh BIT_initDStream proofs/bitstream.c $CF` — FAIL 2s, enforce mode, so this covers every input satisfying the contract |
| `lib/common/zstd_internal.h:245` | `ptrdiff_t diff = (BYTE*)dst - (const BYTE*)src` | Subtracts pointers into different objects, before the `ovtype` check that would tell them apart. Under `ZSTD_no_overlap` they are different objects by definition. C11 6.5.6p9 | `./prove.sh harness proofs/wildcopy.c -H $CF` — FAIL 5s. Also reached through `./prove.sh harness proofs/safecopy.c -H $CF` — FAIL 15s, by inlining, not a separate defect |
| `lib/decompress/zstd_decompress_block.c:834` | `*ip -= sub2` | Forms a pointer before the start of its object; `:838` then does arithmetic on it. `*ip += 8` restores it before any dereference. C11 6.5.6p8 | `./prove.sh harness proofs/overlapcopy8.c -H $CF` — FAIL 3s |
| `lib/common/bitstream.h` `BIT_lookBits` | — | Documentation defect, not UB. Comment says `maxNbBits==56` on 64-bit, but `BIT_MASK_SIZE` is 32 (`DEBUG_STATIC_ASSERT`) and `BIT_getMiddleBits` indexes `BIT_mask[nbBits]`, reachable off x86_64 only | — |

## Proven correct

| Function | Mode | Verdict | What it establishes | Command |
|---|---|---|---|---|
| `BIT_lookBits` | enforce | PASS 3s | Memory safe for every input satisfying the contract. No harness: pointers never alias, so `contract_fresh` allocates them and the contract generates the entry point | `./prove.sh BIT_lookBits proofs/bitstream.c $CF` |
| `ZSTD_wildcopy` (overlap) | harness | PASS 12s | Overlap geometry is memory safe. Out of `run.sh`: 12s exceeds the 5s budget | `./prove.sh harness proofs/wildcopy_overlap.c -H $CF` |
| `ZSTD_wildcopy` (no overlap, contract) | harness `-r` | PASS 3s | The harness supplies inputs the contract permits | `./prove.sh harness proofs/wildcopy.c -H -r ZSTD_wildcopy $CF` |
| `ZSTD_overlapCopy8` (contract) | harness `-r` | PASS 3s | The harness supplies inputs the contract permits. **Says nothing about the postconditions at `:818-828`** — CBMC checks `ensures` only under `--enforce-contract`, which this function cannot use, and `-DNDEBUG` removes the body's `assert(*op - *ip >= 8)`, so nothing checks them | `./prove.sh harness proofs/overlapcopy8.c -H -r ZSTD_overlapCopy8 $CF` |

## TODO

| Function | File | Blocker |
|---|---|---|
| `ZSTD_overlapCopy8` postconditions | `lib/decompress/zstd_decompress_block.c:818-828` | Stated but unverifiable in the current setup; needs enforce mode, which needs a witness CBMC cannot construct |
| `ZSTD_execSequence` | `lib/decompress/zstd_decompress_block.c:1052` | 7 `contract_pre` clauses, unproven; deep call tree, needs `-r` per annotated callee |
| `ZSTD_execSequenceSplitLitBuffer` | `lib/decompress/zstd_decompress_block.c` | No `contract_pre`; `assert(op != NULL /* Precondition */)` only |
| `ZSTD_ldm_gear_feed` | `lib/compress/zstd_ldm.c` | Precondition in a comment only |
| `ZSTD_convertBlockSequences` | `lib/compress/zstd_compress.c` | Precondition in a comment only |
