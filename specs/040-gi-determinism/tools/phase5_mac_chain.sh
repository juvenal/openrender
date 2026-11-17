#!/usr/bin/env bash
# phase5_mac_chain.sh: T072/T076/T077 on macOS as one background chain, every step its own cpujob (-j4 at most). Output in build-logs/.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; W="$(cd "$HERE/../../.." && pwd)"
CJ="$HOME/.claude/bin/cpujob"; L="$W/build-logs"; mkdir -p "$L"; cd "$W" || exit 9
say() { echo "$(date +%T) $*"; }
"$CJ" 040-p5-build3 cmake --build build --config Release -j4 > "$L/040-p5-build3.log" 2>&1; echo "build exit $?"
P="$(grep -v '^#' "$HERE/gi-test-set.txt" | paste -sd'|' -)"
"$CJ" 040-p5-gi ctest --test-dir build -R "$P" -j4 --output-on-failure > "$L/040-p5-gi.log" 2>&1; echo "gi exit $?"
cp build/Testing/Temporary/LastTest.log "$L/040-p5-gi-LastTest.log"
"$CJ" 040-p5-suite ctest --test-dir build -LE 'determinism-sweep|gi_determinism_long' -j4 --output-on-failure > "$L/040-p5-suite.log" 2>&1; echo "suite exit $?"
"$CJ" 040-p5-arch ctest --test-dir build -L architecture --output-on-failure > "$L/040-p5-arch.log" 2>&1; echo "arch exit $?"
"$CJ" 040-p5-svc ctest --test-dir build -L '^link$|shading_contract' --output-on-failure > "$L/040-p5-svc.log" 2>&1; echo "svc exit $?"
SCALES="perf-occlusion-cache:format=12,shadingrate=3 perf-indirect-photon:format=6,shadingrate=3,emit=10 perf-photon-pass:emit=75" "$CJ" 040-p5-timing "$HERE/timing_sc007.sh" build-base build 3 > "$HERE/timing-p5-macos.txt" 2>&1; echo "timing exit $?"
say "chain done"
