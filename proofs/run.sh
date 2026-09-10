#!/bin/sh
# Prove annotated zstd functions. Needs CBMC 6+ and a C preprocessor.
#
#   cd ~/git/zstd && ./proofs/run.sh
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/.." && pwd)
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT

SOLVER=""
command -v z3       >/dev/null 2>&1 && SOLVER=--z3
[ -z "$SOLVER" ] && command -v bitwuzla >/dev/null 2>&1 && SOLVER=--bitwuzla

PLATFORM=""
case "$(uname -m)" in
  arm64|aarch64) PLATFORM="-U__ARM_NEON -U__ARM_FEATURE_SVE -U__ARM_FEATURE_SVE2" ;;
esac

prove_harness() {
  NAME=$1; SRC=$2; FN=$3; shift 3
  printf '  %-30s ' "$NAME"
  # shellcheck disable=SC2086
  /usr/bin/cc -E -DC_CONTRACTS_CPROVER $PLATFORM -I"$ROOT/lib" -I"$ROOT/lib/common" \
     "$SRC" -o "$W/pp.i" 2>/dev/null
  goto-cc "$W/pp.i" -o "$W/a.goto" 2>/dev/null
  goto-instrument --apply-loop-contracts "$W/a.goto" "$W/b.goto" >/dev/null 2>&1
  # shellcheck disable=SC2086
  OUT=$(cbmc "$W/b.goto" --function "$FN" --bounds-check --pointer-check \
        $SOLVER "$@" 2>&1)
  if echo "$OUT" | grep -q 'VERIFICATION SUCCESSFUL'; then
    PROPS=$(echo "$OUT" | grep -oE '[0-9]+ of [0-9]+ failed' | head -1)
    echo "PASS ($PROPS)"
  else
    PROPS=$(echo "$OUT" | grep -oE '[0-9]+ of [0-9]+ failed' | head -1)
    echo "FAIL ($PROPS)"
    echo "$OUT" | grep 'FAILURE' | head -5
    return 1
  fi
}

echo "== zstd contract proofs =="
FAILED=0

prove_harness "ZSTD_wildcopy (no overlap)" \
  "$HERE/wildcopy.c" harness || FAILED=$((FAILED + 1))

echo
if [ "$FAILED" -eq 0 ]; then
  echo "all proofs passed"
else
  echo "$FAILED proof(s) failed"
fi
exit "$FAILED"
