# C contracts experiment

An experiment in writing zstd's decode-path preconditions as C syntax, and
proving them. The annotations are ordinary source: `libzstd.a` builds unchanged
and warning-free with no special compiler. Under GCC, MSVC and tcc every clause
preprocesses to nothing; under any clang with `diagnose_if` the preconditions
are additionally type-checked and folded at each call site, which is what stops
them going stale between proof runs.

Every clause is spelled `contract_`. See
[`lib/common/c_contracts.h`](lib/common/c_contracts.h), which is vendored here
and carries a `C_CONTRACTS_VERSION`.

**In one paragraph.** One unbounded memory-safety proof (`ZSTD_wildcopy`, every
length, by induction rather than by testing). Three real instances of undefined
behaviour, all low severity, none exploitable on conventional hardware, two with
proved fixes. One specification defect. One place where seven load-bearing
preconditions exist only in `-DNDEBUG` assertions and caller arithmetic. Nothing
here is urgent; the `BIT_initDStream` item is the one I would fix first.

## Findings

| Area | What | Severity | Fix |
|---|---|---|---|
| `BIT_initDStream` | Forms `start + 8` before checking the input object is 8 bytes long. Caller chain is **closed to a public entry point** — `ZSTD_decompressBlock` → `ZSTD_decodeLiteralsBlock` → `HUF_decompress4X_usingDTable` → here — and is opened by `RETURN_ERROR_IF(litCSize + lhSize > srcSize, ...)`, where `>` should be `>=`. | Real, reachable UB | Move the `limitPtr` computation onto the path that uses it |
| `ZSTD_overlapCopy8` | Transiently forms a pointer up to 8 bytes before the output object. | Real, reachable UB. Pointer is never dereferenced out of bounds; nothing misbehaves at runtime, which is why fuzzing never surfaced it. The argument is provenance-exploiting optimisers, not any current miscompile. | **3 lines, proved.** Fold the subtraction into the trailing `+= 8`; the final value is arithmetically identical |
| `ZSTD_wildcopy`, `ZSTD_safecopy` | Unconditionally subtract pointers that may belong to different objects — undefined in C even without a dereference. | Real, low severity. Confirmed by two independent tools | 2 lines. `ZSTD_wildcopy` fixed on this branch; `ZSTD_safecopy` (`zstd_decompress_block.c:856`) remains |
| `BIT_lookBits` | Documents a bound of 56 while its portable callee requires a value below 32. | Specification defect, **not reachable** from existing callers | Contract records the actual bound |
| `ZSTD_execSequence` | Seven buffer and pointer preconditions live only in debug assertions and in caller allocation arithmetic two frames away. | Specification gap, not a bug — all seven hold today | Preconditions made explicit here |

Severity is stated deliberately, not inflated: a failed contract is not by itself
evidence of an exploitable bug, and none of the above is.

### Why the `execSequence` one is the interesting one

Recovering what a caller must guarantee meant starting from nothing and adding an
assumption each time CBMC produced a counterexample:

```c
op != NULL
oend - op >= WILDCOPY_OVERLENGTH
sequence.matchLength >= 1
sequence.offset >= 1
sequence.offset <= (op - prefixStart) + sequence.litLength
litPtr + sequence.litLength <= litLimit
sequence.litLength + sequence.matchLength <= oend - op
/* and the one that is easiest to miss: */
at least WILDCOPY_OVERLENGTH readable bytes after litLimit
```

The last is established in `ZSTD_decodeLiteralsBlock`'s allocation arithmetic,
two call frames away. It holds. But a checker asked to verify `ZSTD_wildcopy`'s
write frame *refuses* it, because the frame's soundness depends on slack no
interface mentions. That refusal is the argument for writing these down: the
guarantee is real and load-carrying, and today it exists only as arithmetic in
another file.

## What was proved

**`ZSTD_wildcopy`, no-overlap mode: memory-safe for every length below 1 GiB.**
413 obligations, zero failures, **one iteration**. There is no `--unwind` — the
loop invariants and `decreases` clause let CBMC discharge the loops by
induction, so the result is quantified over all lengths rather than checked up
to a bound. For a function whose job is to deliberately over-copy past its
buffer, that is the result worth having.

**`ZSTD_execSequence` reconstructs the correct bytes.** Functional correctness of
the LZ reconstruction, not just bounds — literals copied verbatim, and match
bytes equal to the bytes they reference, with the specification written as LZ77
semantics rather than as a property of the code. Deliberately self-referential,
since a match may overlap its own output. This one is *exhaustive over a bounded
domain* (`dst` 48 bytes, `litLength <= 16`, `3 <= matchLength <= 16`, prefix
only), not unbounded — but it covers every overlapping-match path, including
`ZSTD_overlapCopy8` and the `ZSTD_execSequenceEnd` slow path.

**`FSE_readNCount`: clean.** Audited because `ZSTD_buildSeqTable`'s
`set_compressed` path relies on it to bound `max` with only a `-DNDEBUG`-erased
assert in between. A corrupt stream cannot drive `charnum` past `maxSV1`. Recorded
as a negative result.

## Reproducing

Needs CBMC **6.x** with `goto-cc` and `goto-instrument`, plus z3. Note that
Ubuntu 24.04 ships CBMC 5.95, which will not work — loop-contract handling
changed substantially across that major version. Take a release build from the
CBMC GitHub releases rather than from apt.

Build clang from the [companion branch](https://github.com/cs01/llvm-project/tree/contracts-c-dev)
as its README describes, then from this checkout:

```sh
LLVM_CONTRACTS=/path/to/llvm-c-contracts
PATH=/path/to/cbmc/bin:$PATH \
ZSTD="$PWD" \
CLANG="$LLVM_CONTRACTS/build/bin/clang" \
"$LLVM_CONTRACTS/proofs/zstd/run-wildcopy-from-grammar.sh"
```

```text
lowered 15 contract clauses from the grammar
CBMC version 6.11.0 (cbmc-6.11.0) 64-bit x86_64 linux
Running SMT2 QF_AUFBV using Z3

** 0 of 413 failed (1 iterations)
VERIFICATION SUCCESSFUL
```

About one minute with z3 4.8.15. Setting `CBMC_SOLVER=` selects CBMC's built-in
SAT backend, which reaches the same verdict independently in 3m21s — a useful
cross-check, since the two solvers share no reasoning core.

## What the annotations look like

From `ZSTD_wildcopy` in `lib/common/zstd_internal.h`:

```c
while (1)
contract_assigns   (contract_locations(op, ip))
contract_invariant (contract_same_object(op, dstStart))
contract_invariant (contract_same_object(ip, srcStart))
contract_decreases (contract_pointer_offset(dstStart) + (contract_ssize_t)length
                    - contract_pointer_offset(op))
{   ZSTD_copy8(op, ip); op += 8; ip += 8;
    if (!(op < oend)) break;
}
```

`contract_assigns` is the write frame; `contract_invariant` and
`contract_decreases` are what replace unrolling with induction.

The loop was a `do`/`while` and the body was `COPY8(op, ip)`. Both had to change,
and neither is a matter of taste: `goto-instrument` refuses a loop contract on a
`do` loop, and `COPY8` is itself a `do {} while (0)`, which CBMC counts as a
second, uncontracted loop. A contract-aware compiler performs that rewrite
itself; nothing outside one can, so out of tree it is the author's edit. The
behaviour is identical, and the body still runs at least once.

## Limits, honestly

- Coverage is the decode path, and within it a handful of functions.
- **The main scaling cost:** annotating a function requires annotating every loop
  reachable through its *inlined* callees, not just the loops in its own body.
  `ZSTD_execSequence` has no loops of its own and still needed contracts on
  `ZSTD_wildcopy` and `ZSTD_safecopy` before it could be checked.
- Getting `--enforce-contract` to run on `ZSTD_execSequence` required
  `--replace-call-with-contract` on the callees rather than letting them inline;
  the direct forms hit `goto-instrument` limitations.
- The only build-affecting change is suppressing `FORCE_INLINE_ATTR` under
  `-DZSTD_CONTRACTS`, which is off by default. A contract on an `always_inline`
  function otherwise silently does nothing.

Detailed proofs, controls, severity assessments and reproduction scripts are in
the companion repository's
[`proofs/zstd`](https://github.com/cs01/llvm-project/tree/contracts-c-dev/proofs/zstd).
