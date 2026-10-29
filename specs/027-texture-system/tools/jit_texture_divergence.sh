#!/usr/bin/env bash
# jit_texture_divergence.sh <build-dir> [out-dir]
# Spec 027 (plan Q1): renders each texture/environment/shadow parity scene twice at -t:1,
# with the default .rslo shaders and with Option "shaderformat" "default" ["slo"], and prints
# "scene<TAB>identical|differs<TAB>MaxBlockAvgDiff(slo vs rslo)". Run it through cpujob.
set -u
B="$1"; OUT="${2:-$(mktemp -d)}"
W="$(cd "$(dirname "$0")/../../.." && pwd)"
export SHADERS="$B/shaders" ORENDERHOME="$B/tests/visual/_home" DISPLAYS="$B/src/display/file"
export TEXTURES="$W/examples/rib/tests:$W/tests/unit/image_input/fixtures:$W/tests/unit/texture_tile/fixtures"
for base in texture-png texture-exr texture-rgbe unbaked-png unbaked-nonpot unbaked-exr unbaked-rgbe env shadow; do
  for h in reyes raytrace; do
    s="$base-$h"; rib="$W/examples/rib/tests/parity/$s.rib"; [ -f "$rib" ] || continue
    tif=$(grep -o 'Display "[^"]*"' "$rib" | head -1 | sed 's/Display "//; s/"//')
    d="$OUT/$s"; mkdir -p "$d/rslo" "$d/slo"
    sed 's/^WorldBegin/Option "shaderformat" "default" ["slo"]\nWorldBegin/' "$rib" > "$d/slo/$s.rib"
    ( cd "$d/rslo" && "$B/src/orender/orender" -t:1 "$rib" >/dev/null 2>&1 )
    ( cd "$d/slo" && "$B/src/orender/orender" -t:1 "$d/slo/$s.rib" >/dev/null 2>&1 )
    if cmp -s "$d/rslo/$tif" "$d/slo/$tif"; then same=identical; else same=differs; fi
    diffv=$( cd "$d/slo" && "$B/tests/visual/test_visual_render" "$B/src/orender/orender" "$d/slo/$s.rib" "$tif" "$d/rslo/$tif" 255 "-t:1" 2>&1 | grep -o 'MaxBlockAvgDiff[^,;]*' | head -1 )
    printf '%s\t%s\t%s\n' "$s" "$same" "${diffv:-n/a}"
  done
done
