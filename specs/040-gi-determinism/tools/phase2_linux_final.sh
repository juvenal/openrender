#!/usr/bin/env bash
# phase2_linux_final.sh: the Phase 2 closing run on Linux after the references were applied: the two re-recorded references, the everyday suite,
# the architecture check. ONE chain, remote cpujob at -j2.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; W="$(cd "$HERE/../../.." && pwd)"
case "$W" in */openrender-040) ;; *) echo "refusing to run outside the spec 040 tree: $W"; exit 9 ;; esac
CJ="$HOME/.claude/bin/cpujob"; L="$W/build-logs"; cd "$W" || exit 9
"$CJ" 040-l-p2f-refs ctest --test-dir build -R '^(GiFiles_Unchanged|Preview_DataSceneUnchanged)$' --output-on-failure > "$L/040-l-p2f-refs.log" 2>&1; echo "refs exit $?"
"$CJ" 040-l-p2f-suite ctest --test-dir build -LE 'determinism-sweep|gi_determinism_long' -j2 --output-on-failure > "$L/040-l-p2f-suite.log" 2>&1; echo "suite exit $?"
cp build/Testing/Temporary/LastTest.log "$L/040-l-p2f-suite-LastTest.log"
"$CJ" 040-l-p2f-arch ctest --test-dir build -L architecture --output-on-failure > "$L/040-l-p2f-arch.log" 2>&1; echo "arch exit $?"
echo "chain done"
