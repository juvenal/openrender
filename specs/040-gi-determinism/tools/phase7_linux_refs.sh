#!/usr/bin/env bash
# phase7_linux_refs.sh: T098's re-render check and the Linux text references, ONE chain on the Linux host (remote cpujob -j2). Nothing is written into tests/ or examples/.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; W="$(cd "$HERE/../../.." && pwd)"
case "$W" in */openrender-040) ;; *) echo "refusing to run outside the spec 040 tree: $W"; exit 9 ;; esac
CJ="$HOME/.claude/bin/cpujob"; L="$W/build-logs"; R="$L/040-refs-linux"; mkdir -p "$R"; cd "$W" || exit 9
REFS_DIR=040-refs-linux "$HERE/phase7_render_refs.sh" > "$L/040-l-p7-refs.log" 2>&1; echo "refs exit $?"
for i in 1 2; do
    "$CJ" 040-l-p7-capture$i cmake --build build --target gi-split-capture-references-scratch -j2 > "$L/040-l-p7-capture$i.log" 2>&1; echo "capture$i exit $?"
    cp build/tests/gi_split/capture_scratch/preview/datascene.txt "$R/datascene-linux-$i.txt"; cp build/tests/gi_split/capture_scratch/gi/files.txt "$R/files-linux-$i.txt"; cp build/tests/gi_split/capture_scratch/gi/loopback-cache.bin "$R/loopback-cache-linux-$i.bin"
    "$CJ" 040-l-p7-replay$i ctest --test-dir build -R ShadingContract_TraceReplayRecord > "$L/040-l-p7-replay$i.log" 2>&1; cp build/tests/shading_contract/replay-fresh.txt "$R/replay-linux-$i.txt"
    "$CJ" 040-l-p7-golden$i ctest --test-dir build -R OcclusionGolden_sobol_cache > "$L/040-l-p7-golden$i.log" 2>&1; cp build/tests/visual/sampling-occlusion/occlusion-golden-sobol_cache.tif "$R/occlusion-sobol_cache-linux-$i.tif"
done
echo "chain done"
