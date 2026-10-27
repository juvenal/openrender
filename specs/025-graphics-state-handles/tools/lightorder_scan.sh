#!/bin/bash
# Spec 025 measurement: run every examples/rib scene through a probe-instrumented
# orender that hashes each object's active-light order and exits at RiWorldEnd.
# Usage: lightorder_scan.sh <build-dir> <source-dir> <out.tsv>
B="$1"; S="$2"; OUT="$3"
ORENDER="$B/src/orender/orender"
DISP=$(find "$B/src/display" -name '*.dsply' -exec dirname {} \; | sort -u | paste -sd: -)
export SHADERS="$B/shaders"
export ORENDERHOME="$B/tests/visual/_home"
export DISPLAYS="$DISP"
export GEOMETRIES="$S/geometry"
export TEXTURES="$S/examples/rib/tests:$S/tests/unit/image_input/fixtures:$S/tests/unit/texture_tile/fixtures"
export OR_LIGHTORDER_PROBE=1
WORK=$(mktemp -d /tmp/lo025.XXXXXX)
: > "$OUT"
cd "$S" || exit 2
find examples/rib -name '*.rib' | sort | while read -r rib; do
    frames=$(grep -c 'WorldBegin' "$rib")
    proc=$(grep -cE 'Procedural|DelayedReadArchive|RunProgram|DynamicLoad' "$rib")
    lights=$(grep -cE '^[[:space:]]*(LightSource|AreaLightSource)' "$rib")
    line=$( cd "$WORK" && perl -e 'alarm 120; exec @ARGV' "$ORENDER" "$S/$rib" 2>&1 >/dev/null | grep '^LIGHTORDER' | head -1 )
    [ -z "$line" ] && line="LIGHTORDER none"
    printf '%s\t%s\tframes=%s\tproc=%s\tlightstmts=%s\n' "$rib" "${line#LIGHTORDER }" "$frames" "$proc" "$lights" >> "$OUT"
done
rm -rf "$WORK"
