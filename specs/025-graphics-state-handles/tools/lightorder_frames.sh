#!/bin/bash
# Usage: lightorder_frames.sh <build> <src> <out> <rib>...
B="$1"; S="$2"; OUT="$3"; shift 3
export SHADERS="$B/shaders" ORENDERHOME="$B/tests/visual/_home" GEOMETRIES="$S/geometry"
export DISPLAYS=$(find "$B/src/display" -name '*.dsply' -exec dirname {} \; | sort -u | paste -sd: -)
export TEXTURES="$S/examples/rib/tests:$S/tests/unit/image_input/fixtures:$S/tests/unit/texture_tile/fixtures"
export OR_LIGHTORDER_PROBE=frames
: > "$OUT"
for rib in "$@"; do
  WORK=$(mktemp -d /tmp/lo025f.XXXXXX)
  ( cd "$WORK" && perl -e 'alarm 600; exec @ARGV' "$B/src/orender/orender" "$S/$rib" 2>&1 >/dev/null | grep '^LIGHTORDER' | sed "s|^|$rib\t|" ) >> "$OUT"
  rm -rf "$WORK"
done
