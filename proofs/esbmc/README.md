# ESBMC harnesses for zstd

Bounded model checking of zstd decode paths with stock [ESBMC](https://github.com/esbmc/esbmc) 8.5.
Each `*_harness.c` includes the unmodified sources in `lib/`.

```bash
./proofs/esbmc/run.sh                       # every harness
./proofs/esbmc/run.sh overlapcopy8_harness  # one harness
```

Logs and counterexample traces go to `/tmp/esbmc/zstd/<harness>.txt`. `.esbmc` holds each
harness's flags, unwind bound and expected verdict.

| Harness | Verdict |
|---------|---------|
| `bitstream_init_limitptr_harness` | COUNTEREXAMPLE |
| `wildcopy_nooverlap_harness` | COUNTEREXAMPLE |
| `overlapcopy8_harness` | PROVEN |
| `wildcopy_overlap_harness` | PROVEN |
| `execsequence_harness` | PROVEN |
