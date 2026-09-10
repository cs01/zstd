# C contracts experiment

An experiment in writing zstd's decode-path preconditions as C syntax, and
proving them. The annotations are ordinary source: `libzstd.a` builds unchanged
and warning-free with no special compiler. Under GCC, MSVC and tcc every clause
preprocesses to nothing; under any clang with `diagnose_if` the preconditions
are additionally type-checked and folded at each call site, which is what stops
them going stale between proof runs.

Every clause is spelled `contract_`. See
[`lib/common/c_contracts.h`](lib/common/c_contracts.h), which is vendored from
[cs01/c-contracts](https://github.com/cs01/c-contracts) and carries a
`C_CONTRACTS_VERSION`.

**In one paragraph.** One memory-safety proof (`ZSTD_wildcopy`, loops discharged
by induction rather than by unwinding). Three real instances of undefined
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

**`ZSTD_wildcopy`, no-overlap mode: memory-safe.** 1487 properties, zero
failures, **one iteration**. There is no `--unwind` — the loop invariants and
`decreases` clause let CBMC discharge the loops by induction rather than by
unwinding them, which is why a single iteration settles it. The harness caps the
symbolic `length` below 4096 to keep the solver finite, so the theorem is "for
every length under 4096", but the loops inside are never unrolled and the bound
buys nothing but solve time. For a function whose job is to deliberately
over-copy past its buffer, that is the result worth having.

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

Needs [CBMC](https://www.cprover.org/cbmc/) **6.x** with `goto-cc` and
`goto-instrument`, plus z3. Note that Ubuntu 24.04 ships CBMC 5.95, which will
not work — loop-contract handling changed substantially across that major
version. Take a release build from the CBMC GitHub releases rather than from apt.

Clone [cs01/c-contracts](https://github.com/cs01/c-contracts) and run its test
suite against this checkout, pointing `ZSTD` at wherever this tree lives:

```sh
cd /path/to/c-contracts
ZSTD=/path/to/zstd test/zstd.sh
```

The suite lowers all 39 contract clauses from the real translation unit, then
proves `ZSTD_wildcopy` memory-safe via the harness in `test/zstd/wildcopy.c`:

```text
== c_contracts.h against /path/to/zstd ==
  1 lowers a real translation unit             PASS   39 clause(s) lowered from a real translation unit
  2 ZSTD_wildcopy is memory safe, unbounded    PASS   26s, budget 60s

every case that ran behaved as recorded
```

To run `prove.sh` directly on a single function, again from the c-contracts
checkout:

```sh
ZSTD=/path/to/zstd
./prove.sh harness test/zstd/wildcopy.c -H \
  -DNDEBUG -DZSTD_NO_INTRINSICS -I $ZSTD/lib/common -I $ZSTD/lib
```

```text
lowered 15 clause(s)
mode: harness (frame not checked)
CBMC version 6.11.0 (cbmc-6.11.0) 64-bit x86_64 linux
...
** 0 of 1487 failed (1 iterations)
VERIFICATION SUCCESSFUL
== solved by z3 in 27s
```

About 30 seconds with z3. `solve.sh` races every installed solver (z3, sat,
bitwuzla, cvc5) and takes the first clean answer.

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

**Only the `ZSTD_wildcopy` proof has a runner in this repo.** The
`ZSTD_execSequence` functional-correctness proof and the `FSE_readNCount` audit
were done under an earlier contract-aware clang fork that this branch no longer
depends on, and their harnesses did not survive the move; `ZSTD_execSequence`
does not converge under `prove.sh` within a few minutes as annotated. The
annotations those proofs produced are still in the source, and are the durable
part of the result.

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
