#!/usr/bin/env bash
# measure_plan.sh: spec 040's plan-stage measurements of the starting tree, as ONE job under the lock (research.md R1, R2, R7):
#   ~/.claude/bin/cpujob 040-plan-measure specs/040-gi-determinism/tools/measure_plan.sh build > <log> 2>&1
# 1. GitHub #163: the identity photon frame at -t:1, 30 separate processes, with the previous map present (reuse) and absent
#    (fresh), and again with MallocPreScribble=1 (macOS: freed and new heap blocks filled with 0xAA).
# 2. FR-003 / #164: the photon pass of photon-parity at 1 thread (10 runs) and 4 threads (30 runs): photons per run, crashes.
# 3. FR-001's process clause: the same photon pass under -P:2.
# 4. The cost proxy of the irradiance-cache mechanism: GI counters (stats level 3) of the occlusion and indirectdiffuse
#    scenes at one thread, with the cache (maxerror 0.5, the scene's) and without it (maxerror 0).
set -u
B="$(cd "${1:?usage: measure_plan.sh <build-dir>}" && pwd)"
HERE="$(cd "$(dirname "$0")" && pwd)"
M="python3 $HERE/measure_gi.py"
S="$(mktemp -d /tmp/m040-scenes-XXXX)"

sed 's/@THREADS@/1/' "$HERE/perf-photon-pass.rib" > "$S/photon-pass-t1.rib"
sed 's/@THREADS@/4/' "$HERE/perf-photon-pass.rib" > "$S/photon-pass-t4.rib"
grep -v numthreads "$HERE/perf-photon-pass.rib" > "$S/photon-pass-nopin.rib"
sed 's/@THREADS@/1/' "$HERE/perf-occlusion-cache.rib" > "$S/occlusion-cache.rib"
sed -e 's/@THREADS@/1/' -e 's/"float maxerror" \[0.5\]/"float maxerror" [0]/' "$HERE/perf-occlusion-cache.rib" > "$S/occlusion-nocache.rib"
sed 's/@THREADS@/1/' "$HERE/perf-indirect-photon.rib" > "$S/indirect-cache.rib"
# frame 2 of perf-indirect-photon: indirectdiffuse() with no maxerror attribute (the default, 0.6 per the spec) and with 0
awk '/FrameBegin 2/{f=1} f' "$S/indirect-cache.rib" | perl -pe 's/WorldBegin/WorldBegin\n    Attribute "irradiance" "float maxerror" [0]/' > "$S/indirect-nocache-frame2.rib"

echo "=== 1. #163 (identity photon frame, -t:1)"
$M repeat "$B" "$HERE/identity-photon.rib" identity.gpm 30 reuse
$M repeat "$B" "$HERE/identity-photon.rib" identity.gpm 30 fresh
MallocPreScribble=1 $M repeat "$B" "$HERE/identity-photon.rib" identity.gpm 30 reuse
MallocPreScribble=1 $M repeat "$B" "$HERE/identity-photon.rib" identity.gpm 30 fresh

echo "=== 2. photon pass, 1 and 4 threads"
$M repeat "$B" "$S/photon-pass-t1.rib" perf-photon-pass.gpm 10 fresh
$M repeat "$B" "$S/photon-pass-t4.rib" perf-photon-pass.gpm 30 fresh

echo "=== 3. photon pass under -P:2"
$M repeat "$B" "$S/photon-pass-nopin.rib" perf-photon-pass.gpm 3 fresh -P:2

echo "=== 4. GI counters, one thread"
$M stats "$B" "$S/occlusion-cache.rib"
$M stats "$B" "$S/occlusion-nocache.rib"
$M stats "$B" "$S/indirect-cache.rib"
echo "(frame 2 alone, reading the map frame 1 of the previous run left in its own scratch: not available, so it runs the photon pass first)"
cat "$S/indirect-cache.rib" | awk '/FrameBegin 2/{exit} {print}' > "$S/indirect-nocache.rib"
cat "$S/indirect-nocache-frame2.rib" >> "$S/indirect-nocache.rib"
$M stats "$B" "$S/indirect-nocache.rib"
echo "scenes: $S"
