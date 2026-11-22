#!/bin/sh
# FR-023 probe (spec 057 plan stage, 2026-11-14). Usage: run.sh <build-dir> <scratch-dir>
# Renders every scene here through the build tree's orender with the visual suite's environment (build-tree
# shaders, displays and .orenderrc; the worktree has no openrender/ deploy tree), the probe shaders first on the
# shader path. Run it through ~/.claude/bin/cpujob. Compare with regioncmp.py / blockmetric.py after converting
# the TIFFs to BMP (sips -s format bmp).
set -e
B=$(cd "$1" && pwd); S=$2; HERE=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$S/shaders"
for s in vis_probe trans_probe; do
    "$B/src/oshader/oshader" -I "$HERE/../../../../shaders/includes" -o "$S/shaders/$s.rslo" "$HERE/shaders/$s.sl"
done
export SHADERS="$S/shaders:$B/shaders" ORENDERHOME="$B/tests/visual/_home" \
       DISPLAYS="$B/src/display/file:$B/src/display/rgbe:$B/src/display/framebuffer" GEOMETRIES="$HERE/../../../../geometry"
cd "$S"
for f in "$HERE"/*.rib; do "$B/src/orender/orender" "$f" > "$(basename "$f" .rib).log" 2>&1; echo "$(basename "$f") exit $?"; done
