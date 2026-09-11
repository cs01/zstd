#!/bin/sh
# Prove annotated zstd functions with CBMC. Needs CBMC 6+.
#
#   ./proofs/run.sh
#
# PROVE defaults to ./prove.sh at the top of this tree, falling back to a
# c-contracts checkout sitting beside it. Override to point at another copy.
#
# Each case records the verdict it is expected to produce, and the runner
# fails only on a mismatch. FAIL is the recorded verdict for a case that
# exposes a real defect in upstream zstd: the harness exists to demonstrate
# the violation, so a PASS there would mean the demonstration stopped working.
set -eu

HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/.." && pwd)
if [ -z "${PROVE:-}" ]; then
  if [ -x "$ROOT/prove.sh" ]; then PROVE=$ROOT/prove.sh
  else PROVE=$(dirname "$ROOT")/c-contracts/prove.sh
  fi
fi
BUDGET=${BUDGET:-120}
FAILED=0
SKIPPED=0
MATCHED=0

CFLAGS="-DNDEBUG -U__ARM_NEON -DZSTD_NO_INTRINSICS -I $ROOT/lib/common -I $ROOT/lib"

report() {
  NAME=$1; EXPECT=$2; ACTUAL=$3; DETAIL=$4
  if [ "$ACTUAL" = "SKIP" ]; then
    printf '  %-34s %-6s %s\n' "$NAME" SKIP "$DETAIL"
    SKIPPED=$((SKIPPED + 1))
  elif [ "$EXPECT" = "$ACTUAL" ]; then
    printf '  %-34s %-6s %s\n' "$NAME" "$ACTUAL" "$DETAIL"
    MATCHED=$((MATCHED + 1))
  else
    printf '  %-34s %-6s %s  <-- RECORDED %s\n' "$NAME" "$ACTUAL" "$DETAIL" "$EXPECT"
    FAILED=$((FAILED + 1))
  fi
}

run_proof() {
  NAME=$1; EXPECT=$2; shift 2
  if ! command -v cbmc >/dev/null 2>&1; then
    report "$NAME" "$EXPECT" SKIP "no cbmc"; return
  fi
  if [ ! -x "$PROVE" ]; then
    report "$NAME" "$EXPECT" SKIP "no prove.sh at \$PROVE"; return
  fi
  T0=$(date +%s)
  # shellcheck disable=SC2086
  OUT=$(TIMEOUT=$BUDGET "$PROVE" "$@" 2>&1) || true
  E=$(( $(date +%s) - T0 ))
  if printf '%s' "$OUT" | grep -q "VERIFICATION SUCCESSFUL"; then
    report "$NAME" "$EXPECT" PASS "${E}s"
  elif printf '%s' "$OUT" | grep -q "VERIFICATION FAILED"; then
    D=$(printf '%s' "$OUT" | grep -m1 "FAILURE" | sed 's/^\[[^]]*\] //' | cut -c1-48)
    report "$NAME" "$EXPECT" FAIL "${E}s  $D"
  else
    D=$(printf '%s' "$OUT" | grep -m1 "error:\|no solver" | cut -c1-48)
    report "$NAME" "$EXPECT" ERROR "${E}s  ${D:-no verdict}"
  fi
}

if ! grep -qs "c_contracts.h" "$ROOT/lib/common/zstd_internal.h"; then
  echo "SKIP: this checkout does not carry the contract annotations"
  exit 0
fi

echo "== zstd contract proofs =="
echo

# Enforce mode: no harness exists, and none should. The contract generates
# the entry point, so the proof covers every input satisfying it rather than
# one geometry someone chose -- and the same clauses are what stock clang
# checks at each real call site. Available only where contract_fresh can
# describe the pointers, i.e. where they do not alias.
echo "--- enforce mode: contract is the entry point ---"

# shellcheck disable=SC2086
run_proof "BIT_lookBits" PASS \
  BIT_lookBits "$HERE/bitstream.c" $CFLAGS

# limitPtr = start + 8 is formed before srcSize >= 8 is known.
# shellcheck disable=SC2086
run_proof "BIT_initDStream" FAIL \
  BIT_initDStream "$HERE/bitstream.c" $CFLAGS

echo
# Harness mode: wildcopy takes pointers that may alias into a single object,
# which contract_fresh cannot express, so CBMC has nothing to allocate for a
# generated entry point. The geometry is built symbolically instead --
# lengths and offsets constrained only by the function's own contract_pre.
echo "--- harness mode: memory safety of the real body ---"

# diff = dst - src subtracts pointers in different objects.
# shellcheck disable=SC2086
run_proof "ZSTD_wildcopy (no overlap)" FAIL \
  harness "$HERE/wildcopy.c" -H $CFLAGS

echo
# The check that keeps a harness honest. Replacing the call with the callee's
# contract makes CBMC assert every contract_pre at the call site instead of
# inlining the body, so a harness that set up inputs the contract does not
# actually permit fails here. Without it a harness is free to drift looser
# than the contract and prove something no real caller relies on -- which is
# not hypothetical: this section silently re-ran the section above it until
# prove.sh was fixed to pass -r through in harness mode.
echo "--- harness inputs satisfy the contract ---"

# shellcheck disable=SC2086
run_proof "ZSTD_wildcopy (no overlap)" PASS \
  harness "$HERE/wildcopy.c" -H -r ZSTD_wildcopy $CFLAGS

echo
# Held back deliberately. The harnesses are on disk and runnable by hand; they
# are out of the recorded suite because we cannot yet say what a failure of
# each one means, and a suite whose verdicts need a footnote is worse than a
# smaller one that does not.
echo "--- held back: not yet attributable ---"
printf '  %-34s %-6s %s\n' "ZSTD_wildcopy (overlap)" HOLD \
  "fails at zstd_internal.h:196, harness or defect unclear"
printf '  %-34s %-6s %s\n' "ZSTD_overlapCopy8" HOLD \
  "original finding masked by unstated preconditions"
printf '  %-34s %-6s %s\n' "ZSTD_safecopy" HOLD \
  "15s, and duplicates the wildcopy finding"

echo
echo "--- not yet covered ---"
printf '  %-34s %-6s %s\n' "ZSTD_execSequence" TODO \
  "8 preconditions, deep call tree, needs -r"
printf '  %-34s %-6s %s\n' "ZSTD_execSequenceSplitLitBuffer" TODO \
  "9 preconditions, same call tree"
printf '  %-34s %-6s %s\n' "ZSTD_ldm_gear_feed" TODO \
  "annotated, blocked on loop contracts"
printf '  %-34s %-6s %s\n' "ZSTD_convertBlockSequences" TODO \
  "annotated, blocked on loop contracts"

echo
echo "--- summary ---"
echo "$MATCHED behaved as recorded, $FAILED did not, $SKIPPED skipped"
[ "$SKIPPED" -gt 0 ] && echo "(skipped = missing prerequisite, not a result)"
exit "$FAILED"
