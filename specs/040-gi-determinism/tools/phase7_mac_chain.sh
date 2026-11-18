#!/usr/bin/env bash
# phase7_mac_chain.sh: T094a, T095 and T096 on macOS as one chain, every step its own cpujob (-j4 at most, the sweeps -j2). Output in build-logs/.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; W="$(cd "$HERE/../../.." && pwd)"
CJ="$HOME/.claude/bin/cpujob"; L="$W/build-logs"; cd "$W" || exit 9
"$CJ" 040-p7-visual ctest --test-dir build -L visual -R 'Visual_(sphere-occlusion|sphere-indirectdiffuse|sphere-photonmap|photon-|subdiv-.*photon|sphere-ray-self-hit|sphere-gather)|^OcclusionGolden|^Preview_DataScene|^GiFiles|^GiRemote|ShadingContract_TraceReplay' --output-on-failure > "$L/040-p7-visual.log" 2>&1; echo "visual exit $?"
"$CJ" 040-p7-giset ctest --test-dir build -R "$(grep -v '^#' "$HERE/gi-test-set.txt" | paste -sd'|' -)" -j4 --output-on-failure > "$L/040-p7-giset.log" 2>&1; echo "giset exit $?"
"$CJ" 040-p7-suite ctest --test-dir build -LE 'determinism-sweep|gi_determinism_long' -j4 --output-on-failure > "$L/040-p7-suite.log" 2>&1; echo "suite exit $?"
"$CJ" 040-p7-arch ctest --test-dir build -L architecture --output-on-failure > "$L/040-p7-arch.log" 2>&1; echo "arch exit $?"
for i in 1 2 3; do
    "$CJ" 040-p7-sweep$i ctest --test-dir build -R '^DeterminismSweep_gi-' -j2 --output-on-failure > "$L/040-p7-sweep$i.log" 2>&1; echo "sweep$i exit $?"
done
"$CJ" 040-p7-det ctest --test-dir build -L '^determinism$' -j2 --output-on-failure > "$L/040-p7-det.log" 2>&1; echo "determinism-default exit $?"
echo "chain done"
