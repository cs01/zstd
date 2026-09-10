| Function | File | Finding | Command |
|---|---|---|---|
| `BIT_lookBits` | `lib/common/bitstream.h` | PASS — spec defect only: documents bound of 56, callee requires < 32; not reachable from existing callers | `prove.sh BIT_lookBits proofs/bitstream.c` |
| `BIT_initDStream` | `lib/common/bitstream.h` | FAIL — forms `start + 8` before checking `srcSize >= 8`; real reachable UB | `prove.sh BIT_initDStream proofs/bitstream.c` |
| `ZSTD_wildcopy` | `lib/common/zstd_internal.h` | FAIL — `dst - src` subtracts pointers in different objects; real UB, low severity | `prove.sh harness proofs/wildcopy.c -H` |
| `ZSTD_wildcopy` (overlap) | `lib/common/zstd_internal.h` | PASS — overlap geometry is memory safe | `prove.sh harness proofs/wildcopy_overlap.c -H` |
| `ZSTD_overlapCopy8` | `lib/decompress/zstd_decompress_block.c` | FAIL — `*ip -= dec64table[offset]` forms pointer before object start; real reachable UB | `prove.sh harness proofs/overlapcopy8.c -H` |
| `ZSTD_safecopy` | `lib/decompress/zstd_decompress_block.c` | FAIL — `op - ip` subtracts pointers in different objects; real UB, low severity | `prove.sh harness proofs/safecopy.c -H` |
| `ZSTD_execSequence` | `lib/decompress/zstd_decompress_block.c` | TODO — 7 preconditions live only in debug assertions; spec gap, not a bug | — |
