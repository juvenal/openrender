#!/usr/bin/env bash
# phase3_linux_chain.sh: T053's Linux checkpoint for Phase 3: ONE chain on the Linux host, remote cpujob at -j2. Output in build-logs/.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; W="$(cd "$HERE/../../.." && pwd)"
case "$W" in */openrender-040) ;; *) echo "refusing to run outside the spec 040 tree: $W"; exit 9 ;; esac
CJ="$HOME/.claude/bin/cpujob"; L="$W/build-logs"; mkdir -p "$L"; cd "$W" || exit 9
say() { echo "$(date +%T) $*"; }
say build
"$CJ" 040-l-p3-build sh -c 'cmake -S . -B build -DOPENRENDER_040_RED_TESTS=OFF && cmake --build build -j2' > "$L/040-l-p3-build.log" 2>&1; echo "build exit $?"
"$CJ" 040-l-p3-unit ctest --test-dir build -R 'GiDet_Photon|GiKeys_Photons|TextureOnly_GiStores|GiFiles_Unchanged|Preview_DataSceneUnchanged' --output-on-failure > "$L/040-l-p3-unit.log" 2>&1; echo "unit exit $?"
P="$(grep -v '^#' "$HERE/gi-test-set.txt" | paste -sd'|' -)"
"$CJ" 040-l-p3-gi ctest --test-dir build -R "$P" -j2 --output-on-failure > "$L/040-l-p3-gi.log" 2>&1; echo "gi exit $?"
cp build/Testing/Temporary/LastTest.log "$L/040-l-p3-gi-LastTest.log"
"$CJ" 040-l-p3-sweep ctest --test-dir build -R '^DeterminismSweep_gi-(photon-parity|subdiv|motion-subdiv)' -j2 --output-on-failure > "$L/040-l-p3-sweep.log" 2>&1; echo "sweep exit $?"
TREE=build TAG=p3 "$HERE/phase0_jobs.sh" frame2 > "$L/040-l-p3-frame2.log" 2>&1; echo "frame2 exit $?"
"$CJ" 040-l-p3-suite ctest --test-dir build -LE 'determinism-sweep|gi_determinism_long' -j2 --output-on-failure > "$L/040-l-p3-suite.log" 2>&1; echo "suite exit $?"
"$CJ" 040-l-p3-arch ctest --test-dir build -L architecture --output-on-failure > "$L/040-l-p3-arch.log" 2>&1; echo "arch exit $?"
SCALES="perf-occlusion-cache:format=12,shadingrate=3 perf-indirect-photon:format=6,shadingrate=3,emit=10 perf-photon-pass:emit=75" "$CJ" 040-l-p3-timing "$HERE/timing_sc007.sh" build-base build 5 > "$HERE/timing-p3-linux.txt" 2>&1; echo "timing exit $?"
say "chain done"
