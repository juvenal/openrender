#!/usr/bin/env bash
# phase5_linux_chain.sh: T078's Linux checkpoint for Phase 5: ONE chain on the Linux host, remote cpujob at -j2. Output in build-logs/.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; W="$(cd "$HERE/../../.." && pwd)"
case "$W" in */openrender-040) ;; *) echo "refusing to run outside the spec 040 tree: $W"; exit 9 ;; esac
CJ="$HOME/.claude/bin/cpujob"; L="$W/build-logs"; mkdir -p "$L"; cd "$W" || exit 9
say() { echo "$(date +%T) $*"; }
say build
"$CJ" 040-l-p5-build sh -c 'cmake -S . -B build -DOPENRENDER_040_RED_TESTS=OFF && cmake --build build -j2' > "$L/040-l-p5-build.log" 2>&1; echo "build exit $?"
"$CJ" 040-l-p5-unit ctest --test-dir build -R 'GiDet_(LookupPure|CommitOrder|CommitDedupe|CacheRoundTrip|BatchClamp|AcceptPure|MissIndependence|IndirectRayCount|RepeatedPoint)|GiKeys_|TextureOnly_|GiSplit_|GiRemote|GiFiles' --output-on-failure > "$L/040-l-p5-unit.log" 2>&1; echo "unit exit $?"
"$CJ" 040-l-p5-sweep ctest --test-dir build -R '^DeterminismSweep_gi-' -j2 --output-on-failure > "$L/040-l-p5-sweep.log" 2>&1; echo "sweep exit $?"
P="$(grep -v '^#' "$HERE/gi-test-set.txt" | paste -sd'|' -)"
"$CJ" 040-l-p5-gi ctest --test-dir build -R "$P" -j2 --output-on-failure > "$L/040-l-p5-gi.log" 2>&1; echo "gi exit $?"
cp build/Testing/Temporary/LastTest.log "$L/040-l-p5-gi-LastTest.log"
QTREE=build QOUT=build-logs/040-quality-p5 "$CJ" 040-l-p5-quality python3 "$HERE/quality_run.py" > "$L/040-l-p5-quality.log" 2>&1; echo "quality exit $?"
"$CJ" 040-l-p5-suite ctest --test-dir build -LE 'determinism-sweep|gi_determinism_long' -j2 --output-on-failure > "$L/040-l-p5-suite.log" 2>&1; echo "suite exit $?"
"$CJ" 040-l-p5-arch ctest --test-dir build -L architecture --output-on-failure > "$L/040-l-p5-arch.log" 2>&1; echo "arch exit $?"
"$CJ" 040-l-p5-svc ctest --test-dir build -L '^link$|shading_contract' --output-on-failure > "$L/040-l-p5-svc.log" 2>&1; echo "svc exit $?"
# the .icache reference, twice (T075/T078): the tests' own --record into the build tree
for i in 1 2; do
    "$CJ" 040-l-p5-capture$i cmake --build build --target gi-split-capture-references-scratch -j2 > "$L/040-l-p5-capture$i.log" 2>&1; echo "capture$i exit $?"
    cp build/tests/gi_split/capture_scratch/gi/files.txt "$L/040-files-linux-p5-$i.txt" 2>/dev/null
done
say "chain done"
