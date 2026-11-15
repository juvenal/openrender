#!/usr/bin/env bash
# measure_plan2.sh: the second plan-stage job (research.md R2, R3, R7), as ONE job under the lock:
#   ~/.claude/bin/cpujob 040-plan-measure2 specs/040-gi-determinism/tools/measure_plan2.sh build > <log> 2>&1
# 1. GI counters (stats level 3, log level 3) at one thread: the occlusion scene with the cache (maxerror 0.5) and without (0),
#    and the indirectdiffuse scene (photon pass, then indirectdiffuse() with the default maxerror and with 0).
# 2. One photon pass under -P:2, its output kept (why it fails).
# 3. timing_sc007.sh, the starting tree against itself, 5 interleaved series (the run-to-run spread that is the bar).
set -u
B="$(cd "${1:?usage: measure_plan2.sh <build-dir>}" && pwd)"
HERE="$(cd "$(dirname "$0")" && pwd)"
M="python3 $HERE/measure_gi.py"
S="$(mktemp -d /tmp/m040-scenes2-XXXX)"
sed 's/@THREADS@/1/' "$HERE/perf-occlusion-cache.rib" > "$S/occlusion-cache.rib"
sed -e 's/@THREADS@/1/' -e 's/"float maxerror" \[0.5\]/"float maxerror" [0]/' "$HERE/perf-occlusion-cache.rib" > "$S/occlusion-nocache.rib"
sed 's/@THREADS@/1/' "$HERE/perf-indirect-photon.rib" > "$S/indirect-cache.rib"
perl -pe 'if (/FrameBegin 2/) { $f = 1 } if ($f && /WorldBegin/) { $_ .= "    Attribute \"irradiance\" \"float maxerror\" [0]\n"; $f = 0 }' "$S/indirect-cache.rib" > "$S/indirect-nocache.rib"
grep -v numthreads "$HERE/perf-photon-pass.rib" > "$S/photon-pass-nopin.rib"

echo "=== 1. GI counters, one thread"
for s in occlusion-cache occlusion-nocache indirect-cache indirect-nocache; do $M stats "$B" "$S/$s.rib"; done

echo "=== 2. photon pass under -P:2 (output)"
mkdir -p "$S/p2" && cd "$S/p2" && \
  SHADERS="$B/shaders" ORENDERHOME="$B/tests/visual/_home" DISPLAYS="$B/src/display/file" GEOMETRIES="$HERE/../../../geometry" \
  "$B/src/orender/orender" -P:2 "$S/photon-pass-nopin.rib" 2>&1 | tail -20; echo "exit ${PIPESTATUS[0]}"; ls -l "$S/p2"
cd "$HERE/../../.."

echo "=== 3. timing, starting tree against itself"
"$HERE/timing_sc007.sh" "$B" "$B" 5
echo "scenes: $S"
