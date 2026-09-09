# C contracts experiment

This branch applies the experimental C contracts implementation from
[cs01/llvm-project `contracts-c-dev`](https://github.com/cs01/llvm-project/tree/contracts-c-dev)
to zstd's decompression path. The annotations are ordinary C when that compiler
is enabled and preprocess away with stock compilers.

## Run the unbounded `ZSTD_wildcopy` proof

Install CBMC 6.x with `goto-cc` and `goto-instrument`, plus z3. Build Clang from
the companion branch as described in its README, then run this from the zstd
checkout:

```sh
LLVM_CONTRACTS=/path/to/llvm-c-contracts
PATH=/path/to/cbmc/bin:$PATH \
ZSTD="$PWD" \
CLANG="$LLVM_CONTRACTS/build/bin/clang" \
"$LLVM_CONTRACTS/proofs/zstd/run-wildcopy-from-grammar.sh"
```

The default chooses z3 when it is installed. This is the relevant output from
the measured CBMC 6.11 / z3 4.8.15 run (the per-property listing is omitted):

```text
lowered 15 contract clauses from the grammar
CBMC version 6.11.0 (cbmc-6.11.0) 64-bit x86_64 linux
Passing problem to SMT2 QF_AUFBV using Z3
Running SMT2 QF_AUFBV using Z3

** Results:
** 0 of 413 failed (1 iterations)
VERIFICATION SUCCESSFUL
```

To select CBMC's built-in SAT backend explicitly, set `CBMC_SOLVER` to the
empty string before the same command:

```sh
CBMC_SOLVER= \
PATH=/path/to/cbmc/bin:$PATH \
ZSTD="$PWD" \
CLANG="$LLVM_CONTRACTS/build/bin/clang" \
"$LLVM_CONTRACTS/proofs/zstd/run-wildcopy-from-grammar.sh"
```

Its measured output ended with:

```text
CBMC version 6.11.0 (cbmc-6.11.0) 64-bit x86_64 linux
Passing problem to propositional reduction
Running propositional reduction
SAT checker: instance is UNSATISFIABLE

** Results:
** 0 of 413 failed (1 iterations)
VERIFICATION SUCCESSFUL
```

There is no `--unwind`: CBMC proves the loops by induction for every length
below 1 GiB in `ZSTD_no_overlap` mode. The complete run takes about one minute
with z3 here; CBMC's built-in SAT backend independently produced the same
verdict in 3m21s.

## What the experiment found

| Area | Finding | Assessment | Status on this branch |
|---|---|---|---|
| `ZSTD_wildcopy`, `ZSTD_safecopy` | Unconditionally subtract pointers that may belong to separate objects, which C does not define even without a dereference. | Real, low-severity UB | `ZSTD_wildcopy` fixed; `ZSTD_safecopy` remains |
| `ZSTD_overlapCopy8` | Can transiently form a pointer up to eight bytes before the output object. | Real, reachable, low-severity UB; the pointer is not dereferenced while out of bounds | Fix available in the companion repository |
| `BIT_initDStream` | Forms `start + 8` before checking that the input object is eight bytes long. | Real, reachable UB on short input | Fix available in the companion repository |
| `BIT_lookBits` | Documents a bound of 56 while its portable callee requires a value below 32. | Specification defect, not reachable from existing callers | Contract records the actual bound |
| `ZSTD_execSequence` | Buffer and pointer preconditions live only in debug assertions and caller allocation arithmetic. | Specification gap | Preconditions made explicit here |

The detailed proofs, controls, severity assessments, and reproduction scripts
are in the companion repository's
[`proofs/zstd`](https://github.com/cs01/llvm-project/tree/contracts-c-dev/proofs/zstd)
directory. These findings are deliberately separated into reachable UB,
specification defects, and checker limitations; a failed contract is not by
itself evidence of an exploitable bug.
