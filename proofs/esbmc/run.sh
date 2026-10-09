#!/usr/bin/env bash
# Run zstd's ESBMC harnesses with stock ESBMC and print one verdict per harness.
#
#   ./proofs/esbmc/run.sh                  # every *_harness.c in this directory
#   ./proofs/esbmc/run.sh <name> [<name>]  # just the named harness(es)
#
# Ported from OpenZL's proofs/esbmc/run.sh. Each harness #includes unmodified
# zstd sources, models its inputs as nondeterministic, and constrains them only
# by what the real caller guarantees. No annotations in lib/ are needed.
#
# .esbmc in this directory is sourced and may set:
#   ESBMC_UNWIND=N                         default loop unwind bound
#   ESBMC_CHECKS=(flag ...)                checks for every harness
#   ESBMC_HARNESS_CHECKS[name]="flag ..."  replaces ESBMC_CHECKS for one harness
#   ESBMC_HARNESS_UNWIND[name]=N           replaces ESBMC_UNWIND for one harness
#   ESBMC_EXPECT[name]=VERDICT             recorded verdict (default PROVEN)
#
# Verdicts:
#   PROVEN          every property holds for every input within the bound
#   COUNTEREXAMPLE  ESBMC found a concrete input that violates a property
#   INCONCLUSIVE    a loop needed more than the unwind bound; nothing is proven
#   VACUOUS         ESBMC proved it, but no input reaches the end of the harness
#   ERROR           ESBMC could not parse or run the harness
#
# A COUNTEREXAMPLE can be the recorded verdict: such a harness exists to
# demonstrate a violation, and the runner fails only when a harness stops
# behaving as recorded. Every PROVEN is re-run with -DESBMC_VACUITY_PROBE
# (see harness.h) unless VACUITY=0.
#
# A missing esbmc fails the run. Set ESBMC_ALLOW_SKIP=1 to exit 0 instead.
# Environment overrides: UNWIND, ESBMC_OUT, ESBMC, VACUITY.
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "${HERE}/../.." && pwd)"

ESBMC_UNWIND=9
ESBMC_CHECKS=(--overflow-check --ub-shift-check --force-malloc-success)
declare -A ESBMC_HARNESS_CHECKS=()
declare -A ESBMC_HARNESS_UNWIND=()
declare -A ESBMC_EXPECT=()
if [[ -f "${HERE}/.esbmc" ]]; then
  # shellcheck source=/dev/null
  source "${HERE}/.esbmc"
fi
OUT="${ESBMC_OUT:-/tmp/esbmc/zstd}"
ESBMC="${ESBMC:-esbmc}"
VACUITY="${VACUITY:-1}"
if ! command -v "${ESBMC}" >/dev/null 2>&1; then
  echo "esbmc not found (https://github.com/esbmc/esbmc/releases), or set ESBMC" >&2
  if [[ "${ESBMC_ALLOW_SKIP:-0}" == 1 ]]; then exit 0; fi
  exit 1
fi
mkdir -p "${OUT}"

includes=(-I "${HERE}" -I "${ROOT}/lib/common" -I "${ROOT}/lib")
defines=(-DDEBUGLEVEL=1 -DZSTD_NO_INTRINSICS -DMEM_FORCE_MEMORY_ACCESS=0)

if [[ $# -gt 0 ]]; then
  harnesses=()
  for name in "$@"; do
    f="${HERE}/${name%.c}.c"
    [[ -f "${f}" ]] || { echo "no such harness: ${f}" >&2; exit 2; }
    harnesses+=("${f}")
  done
else
  shopt -s nullglob
  harnesses=("${HERE}"/*_harness.c)
  shopt -u nullglob
fi

classify() {
  local log=$1 rc=$2 violated
  violated="$(grep -A3 '^Violated property:' "${log}" | sed -n '2,3p' | sed 's/^ *//' | paste -sd' ')"
  if [[ "${violated}" == *"unwinding assertion"* ]]; then
    echo INCONCLUSIVE
  elif [[ ${rc} -eq 0 ]] && grep -q 'VERIFICATION SUCCESSFUL' "${log}"; then
    echo PROVEN
  elif [[ ${rc} -ne 0 ]] && grep -q 'VERIFICATION FAILED' "${log}"; then
    echo COUNTEREXAMPLE
  else
    echo ERROR
  fi
}

echo "$("${ESBMC}" --version)  logs=${OUT}"
matched=0
mismatched=0
for harness in "${harnesses[@]}"; do
  name="$(basename "${harness}" .c)"
  log="${OUT}/${name}.txt"
  checks=("${ESBMC_CHECKS[@]}")
  if [[ -n "${ESBMC_HARNESS_CHECKS[${name}]+set}" ]]; then
    read -r -a checks <<<"${ESBMC_HARNESS_CHECKS[${name}]}"
  fi
  unwind="${UNWIND:-${ESBMC_HARNESS_UNWIND[${name}]:-${ESBMC_UNWIND}}}"
  expect="${ESBMC_EXPECT[${name}]:-PROVEN}"
  start=${SECONDS}
  "${ESBMC}" --std c11 "${checks[@]}" --unwind "${unwind}" \
    "${includes[@]}" "${defines[@]}" "${harness}" >"${log}" 2>&1
  rc=$?
  verdict="$(classify "${log}" "${rc}")"
  if [[ "${verdict}" == PROVEN && "${VACUITY}" != 0 ]]; then
    vlog="${OUT}/${name}.vacuity.txt"
    "${ESBMC}" --std c11 "${checks[@]}" --unwind "${unwind}" --no-unwinding-assertions \
      "${includes[@]}" "${defines[@]}" -DESBMC_VACUITY_PROBE "${harness}" >"${vlog}" 2>&1
    vrc=$?
    if ! { [[ ${vrc} -ne 0 ]] && grep -q 'vacuity probe: end of harness reached' "${vlog}"; }; then
      verdict=VACUOUS
    fi
  fi
  elapsed=$((SECONDS - start))
  counts="$(grep -oE '[0-9]+ of [0-9]+ properties failed' "${log}" | tail -1)"
  violated="$(grep -A3 '^Violated property:' "${log}" | sed -n '2,3p' | sed 's/^ *//' | paste -sd' ')"
  note=""
  if [[ "${verdict}" == "${expect}" ]]; then
    matched=$((matched + 1))
  else
    mismatched=$((mismatched + 1))
    note="  <-- RECORDED ${expect}"
  fi
  printf '%-15s %-36s %-30s %5ss  unwind=%s%s\n' \
    "${verdict}" "${name}" "${counts}" "${elapsed}" "${unwind}" "${note}"
  if [[ -n "${violated}" ]]; then
    printf '    %s\n' "${violated}"
  fi
done
echo "${matched} behaved as recorded, ${mismatched} did not"
exit "${mismatched}"
