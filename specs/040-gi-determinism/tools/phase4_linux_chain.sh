#!/usr/bin/env bash
# phase4_linux_chain.sh: T062's Linux checkpoint for Phase 4: ONE chain on the Linux host, remote cpujob at -j2. Output in build-logs/.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; W="$(cd "$HERE/../../.." && pwd)"
case "$W" in */openrender-040) ;; *) echo "refusing to run outside the spec 040 tree: $W"; exit 9 ;; esac
CJ="$HOME/.claude/bin/cpujob"; L="$W/build-logs"; mkdir -p "$L"; cd "$W" || exit 9
say() { echo "$(date +%T) $*"; }
say build
"$CJ" 040-l-p4-build sh -c 'cmake -S . -B build -DOPENRENDER_040_RED_TESTS=OFF && cmake --build build -j2' > "$L/040-l-p4-build.log" 2>&1; echo "build exit $?"
"$CJ" 040-l-p4-unit ctest --test-dir build -R 'GiDet_(AcceptPure|MissIndependence|IndirectRayCount)|TextureOnly_|GiRemote_|GiKeys_|GiCache_|GiFiles' --output-on-failure > "$L/040-l-p4-unit.log" 2>&1; echo "unit exit $?"
P="$(grep -v '^#' "$HERE/gi-test-set.txt" | paste -sd'|' -)"
"$CJ" 040-l-p4-gi ctest --test-dir build -R "$P" -j2 --output-on-failure > "$L/040-l-p4-gi.log" 2>&1; echo "gi exit $?"
cp build/Testing/Temporary/LastTest.log "$L/040-l-p4-gi-LastTest.log"
"$CJ" 040-l-p4-sampling ctest --test-dir build -L sampling -j2 --output-on-failure > "$L/040-l-p4-sampling.log" 2>&1; echo "sampling exit $?"
"$CJ" 040-l-p4-suite ctest --test-dir build -LE 'determinism-sweep|gi_determinism_long' -j2 --output-on-failure > "$L/040-l-p4-suite.log" 2>&1; echo "suite exit $?"
"$CJ" 040-l-p4-arch ctest --test-dir build -L architecture --output-on-failure > "$L/040-l-p4-arch.log" 2>&1; echo "arch exit $?"
"$CJ" 040-l-p4-svc ctest --test-dir build -L '^link$|shading_contract' --output-on-failure > "$L/040-l-p4-svc.log" 2>&1; echo "svc exit $?"
say "chain done"
