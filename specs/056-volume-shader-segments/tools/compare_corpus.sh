#!/usr/bin/env bash
# Spec 056 (tasks.md T137): one-off byte-identity check of FR-016 / SC-004. NOT a ctest test and not part of any build.
#
#   cpujob 056-corpus bash specs/056-volume-shader-segments/tools/compare_corpus.sh \
#       <master-build-dir> <branch-build-dir> <branch-source-dir> <out-dir> [tier1|all]
#
# Renders every RIB that a Visual_*, Parity_* or Determinism* test of the branch's build registers and that binds no
# Interior/Exterior, once with the `orender` of <master-build-dir> (built from master 0e554fed) and once with the branch's,
# each from its own scratch directory, at one thread (-t:1) with the variables of VISUAL_ENV, and compares every output
# file with cmp semantics (bytes). It never runs a test binary of the suite and calls no git. Two jobs at most (the Python
# pool below is fixed at 2). The verdict goes to <out-dir>/summary.txt and <out-dir>/results.json; any difference is
# reported with its scene name and is never answered with a threshold.
#   COMPARE_STRIDE=<n>      a smoke run: every n-th scene only
#   COMPARE_ONLY=<a,b>      only the scenes whose path contains one of these
#   COMPARE_PREPEND='<RIB line>'  render a copy of each scene with this line first (the sweep's 'Option "sampling" "sampler" ["sobol"]')
#   COMPARE_SELF=1          also render each tree twice, to tell a non-deterministic scene from a real difference
#   COMPARE_CONTROL=<rib>   (colon separated) scenes that bind Interior/Exterior and MUST differ: a positive control, reported apart
set -euo pipefail
if [ $# -lt 4 ]; then
    sed -n "2,19p" "$0"; exit 2
fi
A_BUILD="$1"; B_BUILD="$2"; B_SRC="$3"; OUT="$4"; TIER="${5:-all}"
mkdir -p "${OUT}"
# The scenes the suite registers: ctest's own listing of the branch's build (no git, no test is run)
ctest --test-dir "${B_BUILD}" --show-only=json-v1 > "${OUT}/ctest.json"
exec python3 -I "$(dirname "$0")/compare_corpus.py" "${A_BUILD}" "${B_BUILD}" "${B_SRC}" "${OUT}" "${TIER}"
