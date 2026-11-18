#!/usr/bin/env bash
# phase2_linux_chain.sh: T047's Linux checkpoint, ONE chain on the Linux host (~/Projects/Development/CLI/openrender-040), every job through the
# remote cpujob at -j2. Output in build-logs/ (untracked); the Linux reference recordings are build-logs/040-files-linux-{1,2}.txt and
# build-logs/040-capture{1,2}/ (pull with scp; nothing is copied into tests/).
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; W="$(cd "$HERE/../../.." && pwd)"
case "$W" in */openrender-040) ;; *) echo "refusing to run outside the spec 040 tree: $W"; exit 9 ;; esac
CJ="$HOME/.claude/bin/cpujob"; L="$W/build-logs"; mkdir -p "$L"; cd "$W" || exit 9
say() { echo "$(date +%T) $*"; }
say build
"$CJ" 040-l-p2-build sh -c 'cmake -S . -B build -DOPENRENDER_040_RED_TESTS=OFF && cmake --build build -j2' > "$L/040-l-p2-build.log" 2>&1; echo "build exit $?"
say "unit and byte tests"
"$CJ" 040-l-p2-unit ctest --test-dir build -R '^(GiDet_Photon(Claim|FileBytes|Repeat|RepeatReuse|Threads)|GiKeys_Photons|GiKeys_PhotonsThreads)$' --output-on-failure > "$L/040-l-p2-unit.log" 2>&1; echo "unit exit $?"
"$CJ" 040-l-p2-long ctest --test-dir build -L '^gi_determinism_long$' -R 'GiDet_Photon' -V > "$L/040-l-p2-long.log" 2>&1; echo "long exit $?"
"$CJ" 040-l-p2-registry ctest --test-dir build -R '^Registry_IntegratorImageIdentity$' --repeat until-fail:30 --output-on-failure > "$L/040-l-p2-registry.log" 2>&1; echo "registry exit $?"
"$CJ" 040-l-p2-sweep ctest --test-dir build -R '^DeterminismSweep_gi-(subdiv|motion-subdiv)' -j2 --output-on-failure > "$L/040-l-p2-sweep.log" 2>&1; echo "sweep exit $?"
say "crash series (16, 30 runs each)"
TREE=build TAG=p2 "$HERE/phase0_jobs.sh" crash > "$L/040-l-p2-crash.log" 2>&1; echo "crash exit $?"
say "GiFiles_Unchanged and the Linux reference recordings"
"$CJ" 040-l-p2-gifiles ctest --test-dir build -R '^GiFiles_Unchanged$|^Preview_DataSceneUnchanged$' --output-on-failure > "$L/040-l-p2-gifiles.log" 2>&1; echo "gifiles exit $?"
for n in 1 2; do
    "$CJ" "040-l-p2-capture$n" cmake --build build --target gi-split-capture-references-scratch -j2 > "$L/040-l-p2-capture$n.log" 2>&1; echo "capture$n exit $?"
    cp build/tests/gi_split/capture_scratch/gi/files.txt "$L/040-files-linux-$n.txt"
    mkdir -p "$L/040-capture$n" && cp build/tests/gi_split/capture_scratch/gi/* build/tests/gi_split/capture_scratch/preview/* "$L/040-capture$n/" 2>/dev/null
done
say "architecture, GI set"
"$CJ" 040-l-p2-arch ctest --test-dir build -L architecture --output-on-failure > "$L/040-l-p2-arch.log" 2>&1; echo "arch exit $?"
P="$(grep -v '^#' "$HERE/gi-test-set.txt" | paste -sd'|' -)"
"$CJ" 040-l-p2-gi ctest --test-dir build -R "$P" -j2 --output-on-failure > "$L/040-l-p2-gi.log" 2>&1; echo "gi exit $?"
say "chain done"
