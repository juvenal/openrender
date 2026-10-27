#!/usr/bin/env bash
# characterization_scenes.sh <build-dir>
# Spec 027 (T030a, finding F2): renders the characterization scenes of tools/scenes/ into
# <build-dir>/tests/characterization/<scene>[-slo]/ so `image_manifest.sh record` hashes them with
# the suite's images. They cover what no existing scene does (SC-006 names them): occlusion() and
# indirectdiffuse() with "environmentmap" (the hider's lookupEnvironment() and
# occlusionParams.environment) and the point hierarchy with a non-default "maxsolidangle"
# (CPointHierarchy::lookup reads the limit). Each scene is rendered as .rslo and, in a JIT build, as
# .slo (an `Option "shaderformat" "default" ["slo"]` line before WorldBegin). They are not committed
# reference images and not ctest entries: their reference is the starting tree's hash (T035a).
# `-t:1` for all of them. Prints "scene<TAB>ok|FAILED" per scene and, last, the proof that the two
# maxsolidangle scenes differ ("maxsolidangle<TAB>image changes|image UNCHANGED"); exit 1 on any
# failure or if the limit changes nothing (then the scene checks nothing). Run it through cpujob.
set -u
B="$(cd "$1" && pwd)"
W="$(cd "$(dirname "$0")/../../.." && pwd)"
SC="$W/specs/027-texture-system/tools/scenes"
OUT="$B/tests/characterization"
rm -rf "$OUT"; mkdir -p "$OUT"
export SHADERS="$B/shaders" ORENDERHOME="$B/tests/visual/_home" DISPLAYS="$B/src/display/file"
export TEXTURES="$W/examples/rib/tests:$W/tests/unit/image_input/fixtures:$W/tests/unit/texture_tile/fixtures"
JIT=0
grep -q '^OPENRENDER_ENABLE_JIT:BOOL=ON' "$B/CMakeCache.txt" 2>/dev/null && JIT=1
bad=0

# render <variant: rslo|slo> <scene file> [<scene file> ...]: the scenes of one variant share a directory
# (so the point cloud pass 1 writes is there for pass 2)
render() {
  local variant="$1"; shift
  local first="$(basename "$1" .rib)"
  local suffix=""; [ "$variant" = slo ] && suffix="-slo"
  local d="$OUT/${first%-*}$suffix"
  case "$first" in pointhier-*) d="$OUT/pointhier$suffix";; esac
  mkdir -p "$d"
  local f
  for f in "$@"; do
    local name="$(basename "$f" .rib)"
    local rib="$d/$name$suffix.rib"
    if [ "$variant" = slo ]; then
      sed 's/^WorldBegin/Option "shaderformat" "default" ["slo"]\nWorldBegin/' "$f" > "$rib"
    else
      cp "$f" "$rib"
    fi
    local tif; tif=$(grep -o 'Display "[^"]*"' "$f" | head -1 | sed 's/Display "//; s/"//')
    ( cd "$d" && "$B/src/orender/orender" -t:1 "$rib" > "$d/$name$suffix.log" 2>&1 )
    if [ -s "$d/$tif" ]; then printf '%s\tok\n' "$name$suffix"; else printf '%s\tFAILED\n' "$name$suffix"; bad=1; fi
  done
}

for variant in rslo slo; do
  [ "$variant" = slo ] && [ "$JIT" != 1 ] && { echo "characterization_scenes: not a JIT build: no .slo variants"; continue; }
  for h in reyes raytrace; do
    render $variant "$SC/occlusion-envmap-$h.rib"
    render $variant "$SC/indirectdiffuse-envmap-$h.rib"
  done
  render $variant "$SC/pointhier-bake.rib" "$SC/pointhier-occlusion-default-reyes.rib" "$SC/pointhier-occlusion-wide-reyes.rib"
done

# the proof that maxsolidangle changes the image (rslo variant)
a="$OUT/pointhier/pointhier-occlusion-default-reyes.tif"; b="$OUT/pointhier/pointhier-occlusion-wide-reyes.tif"
if [ -s "$a" ] && [ -s "$b" ] && ! cmp -s "$a" "$b"; then
  printf 'maxsolidangle\timage changes\n'
else
  printf 'maxsolidangle\timage UNCHANGED\n'; bad=1
fi
exit $bad
