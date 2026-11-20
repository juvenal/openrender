#!/usr/bin/env bash
# phase7_linux_chain.sh: T098's Linux half of Phase 7: ONE chain on the Linux host, remote cpujob at -j2. Output in build-logs/.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; W="$(cd "$HERE/../../.." && pwd)"
case "$W" in */openrender-040) ;; *) echo "refusing to run outside the spec 040 tree: $W"; exit 9 ;; esac
CJ="$HOME/.claude/bin/cpujob"; L="$W/build-logs"; mkdir -p "$L"; cd "$W" || exit 9
say() { echo "$(date +%T) $*"; }
say build
"$CJ" 040-l-p7-build sh -c 'cmake -S . -B build -DOPENRENDER_040_RED_TESTS=OFF && cmake --build build -j2' > "$L/040-l-p7-build.log" 2>&1; echo "build exit $?"
"$CJ" 040-l-p7-visual ctest --test-dir build -L visual -R 'Visual_(sphere-occlusion|sphere-indirectdiffuse|sphere-photonmap|photon-|subdiv-.*photon|sphere-ray-self-hit|sphere-gather)|^OcclusionGolden|^Preview_DataScene|^GiFiles|^GiRemote|ShadingContract_TraceReplay' --output-on-failure > "$L/040-l-p7-visual.log" 2>&1; echo "visual exit $?"
"$CJ" 040-l-p7-suite ctest --test-dir build -LE 'determinism-sweep|gi_determinism_long' -j2 --output-on-failure > "$L/040-l-p7-suite.log" 2>&1; echo "suite exit $?"
"$CJ" 040-l-p7-arch ctest --test-dir build -L architecture --output-on-failure > "$L/040-l-p7-arch.log" 2>&1; echo "arch exit $?"
for i in 1 2 3; do
    "$CJ" 040-l-p7-sweep$i ctest --test-dir build -R '^DeterminismSweep_gi-' -j2 --output-on-failure > "$L/040-l-p7-sweep$i.log" 2>&1; echo "sweep$i exit $?"
done
"$CJ" 040-l-p7-det ctest --test-dir build -L '^determinism$' -j2 --output-on-failure > "$L/040-l-p7-det.log" 2>&1; echo "determinism-default exit $?"
say "chain done"
