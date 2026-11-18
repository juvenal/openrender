#!/usr/bin/env bash
# phase1_linux_chain.sh: T034's Linux checkpoint (CP-L), ONE chain on the Linux host in ~/Projects/Development/CLI/openrender-040, every job
# through the remote cpujob at -j2. Output in build-logs/ (untracked). Run it once, after `linux_sync.sh push` and `verify`.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; W="$(cd "$HERE/../../.." && pwd)"
case "$W" in */openrender-040) ;; *) echo "refusing to run outside the spec 040 tree: $W"; exit 9 ;; esac
CJ="$HOME/.claude/bin/cpujob"; L="$W/build-logs"; mkdir -p "$L"; cd "$W" || exit 9
say() { echo "$(date +%T) $*"; }
say "configure OFF + build"
"$CJ" 040-l-p1-build-off sh -c 'cmake -S . -B build -DOPENRENDER_040_RED_TESTS=OFF && cmake --build build -j2' > "$L/040-l-p1-build-off.log" 2>&1; echo "build-off exit $?"
say "harness self-tests, architecture"
"$CJ" 040-l-p1-harness ctest --test-dir build -R '^DeterminismHarness_' --output-on-failure > "$L/040-l-p1-harness.log" 2>&1; echo "harness exit $?"
"$CJ" 040-l-p1-arch ctest --test-dir build -L architecture --output-on-failure > "$L/040-l-p1-arch.log" 2>&1; echo "arch exit $?"
"$CJ" 040-l-p1-none ctest --test-dir build -N -R 'GiDet_|GiKeys_IrradianceRecordsThreads|GiRemote_LoopbackCommit|DeterminismSweep_gi-' > "$L/040-l-p1-none.log" 2>&1; tail -1 "$L/040-l-p1-none.log"
say "configure ON; red targets one at a time"
"$CJ" 040-l-p1-on cmake -S . -B build -DOPENRENDER_040_RED_TESTS=ON > "$L/040-l-p1-on.log" 2>&1; echo "configure-on exit $?"
for t in test_gi_photon_store test_gi_photon_claim test_gi_photon_runs test_gi_cache_store test_gi_miss test_gi_remote_commit gi_quality; do
    "$CJ" "040-l-red-$t" cmake --build build --target "$t" -j2 > "$L/040-l-red-$t.log" 2>&1
    echo "build $t exit $?: $(grep -E 'error:|fatal error:' "$L/040-l-red-$t.log" | head -2 | cut -c1-200 | tr '\n' ' ')"
done
say "red evidence"
"$CJ" 040-l-p1-red ctest --test-dir build -R '^(GiDet_(PhotonFileBytes|PhotonLookupPure|PhotonInMemoryLookup|PhotonRoundTrip|PhotonRepeat|PhotonRepeatReuse|PhotonThreads|MissIndependence_rslo|MissIndependence_slo|IndirectRayCount_rslo|IndirectRayCount_slo|RepeatedPoint_rslo|RepeatedPoint_slo)|GiKeys_IrradianceRecordsThreads)$' -j2 --output-on-failure > "$L/040-l-p1-red.log" 2>&1; echo "red exit $?"
"$CJ" 040-l-p1-sweep ctest --test-dir build -R '^DeterminismSweep_gi-' -j2 --output-on-failure > "$L/040-l-p1-sweep.log" 2>&1; echo "sweep exit $?"
say "configure OFF again"
"$CJ" 040-l-p1-off cmake -S . -B build -DOPENRENDER_040_RED_TESTS=OFF > "$L/040-l-p1-off.log" 2>&1; echo "configure-off exit $?"
say "chain done"
