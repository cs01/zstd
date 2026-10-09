# ESBMC harnesses for zstd

Bounded model checking of zstd decode paths with stock [ESBMC](https://github.com/esbmc/esbmc) 8.5.
Each `*_harness.c` includes the unmodified sources in `lib/`.

```bash
./proofs/esbmc/run.sh                       # every harness
./proofs/esbmc/run.sh overlapcopy8_harness  # one harness
```

`.esbmc` holds each harness's flags, unwind bound and expected verdict. Each run writes its full
ESBMC output to `$ESBMC_OUT/<harness>.txt` (default `/tmp/esbmc/zstd`) on the machine running it.

| Harness | Verdict |
|---------|---------|
| `bitstream_init_limitptr_harness` | COUNTEREXAMPLE |
| `wildcopy_nooverlap_harness` | COUNTEREXAMPLE |
| `overlapcopy8_harness` | PROVEN |
| `wildcopy_overlap_harness` | PROVEN |
| `execsequence_harness` | PROVEN |

## Counterexamples

ESBMC 8.5 traces, trimmed: repeated byte-level states are collapsed to `(xN)` and pointer-encoding
noise is elided.

### `bitstream_init_limitptr_harness`

<details>
<summary>Counterexample</summary>

```
proofs/esbmc/bitstream_init_limitptr_harness.c:12 main
  srcSize = 6
proofs/esbmc/bitstream_init_limitptr_harness.c:16 main
  src[(signed long int)i] = 0   (x6)
proofs/esbmc/bitstream_init_limitptr_harness.c:19 main
  bitD = { .bitContainer = 0 }
  ... 1 state(s) elided
lib/common/bitstream.h:275 BIT_initDStream
  bitD->bitContainer = { .bitContainer=0, .bitsConsumed=0, .anon_pad#2=0, .ptr=&dynamic_1_array[0], .start=&dynamic_1_array[0], .limitPtr=&dynamic_1_array[8] }
lib/common/bitstream.h:278 BIT_initDStream
  bitD->bitContainer = { .bitContainer=0, .bitsConsumed=0, .anon_pad#2=0, .ptr=&dynamic_1_array[0], .start=&dynamic_1_array[0], .limitPtr=&dynamic_1_array[8] }
lib/common/bitstream.h:281 BIT_initDStream
  bitD->bitContainer = { .bitContainer=0, .bitsConsumed=0, .anon_pad#2=0, .ptr=&dynamic_1_array[0], .start=&dynamic_1_array[0], .limitPtr=&dynamic_1_array[8] }
lib/common/bitstream.h:284 BIT_initDStream
  bitD->bitContainer = { .bitContainer=0, .bitsConsumed=0, .anon_pad#2=0, .ptr=&dynamic_1_array[0], .start=&dynamic_1_array[0], .limitPtr=&dynamic_1_array[8] }
lib/common/bitstream.h:287 BIT_initDStream
  bitD->bitContainer = { .bitContainer=0, .bitsConsumed=0, .anon_pad#2=0, .ptr=&dynamic_1_array[0], .start=&dynamic_1_array[0], .limitPtr=&dynamic_1_array[8] }
lib/common/bitstream.h:292 BIT_initDStream
  lastByte = 0
lib/common/bitstream.h:293 BIT_initDStream
  bitD->bitsConsumed = { .bitContainer=0, .bitsConsumed=0, .anon_pad#2=0, .ptr=&dynamic_1_array[0], .start=&dynamic_1_array[0], .limitPtr=&dynamic_1_array[8] }
  ... 1 state(s) elided

Violated property:
  file proofs/esbmc/bitstream_init_limitptr_harness.c line 22 column 5 function main
  limitPtr points into its object or one past its end
  tmp$4
```

</details>

### `wildcopy_nooverlap_harness`

<details>
<summary>Counterexample</summary>

```
proofs/esbmc/wildcopy_nooverlap_harness.c:14 main
  length = 12
lib/common/zstd_internal.h:220 ZSTD_wildcopy
  diff = &dynamic_1_array[0] - &dynamic_2_array[0]
lib/common/zstd_internal.h:223 ZSTD_wildcopy
  oend = &dynamic_1_array[0] + 12
  ... 1 state(s) elided

Violated property:
  file lib/common/zstd_internal.h line 231 column 9 function ZSTD_wildcopy
  assertion diff >= WILDCOPY_VECLEN || diff <= -WILDCOPY_VECLEN
  diff >= 16 || diff <= (signed long int)(-16)
```

</details>
