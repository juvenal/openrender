#!/usr/bin/env bash
# jit_texture_stability.sh <build-dir> <out-dir>
# Spec 027 (T107a): renders the seven ray-traced texture/unbaked probe scenes with the LLVM JIT
# shaders (Option "shaderformat" "default" ["slo"]) at -t:1 into <out-dir>/<scene>/ and prints
# "scene<TAB>sha256" for each image. On the starting tree these seven differed run to run (the JIT
# read an uninitialized scratch, validation.md section 1.5); after Phase 9 four runs must print the
# same hashes, equal to the hashes of the .rslo renders. Seven renders per run: run it through cpujob.
set -u
B="$(cd "$1" && pwd)"; OUT="$2"
W="$(cd "$(dirname "$0")/../../.." && pwd)"
export SHADERS="$B/shaders" ORENDERHOME="$B/tests/visual/_home" DISPLAYS="$B/src/display/file"
export TEXTURES="$W/examples/rib/tests:$W/tests/unit/image_input/fixtures:$W/tests/unit/texture_tile/fixtures"
rm -rf "$OUT"; mkdir -p "$OUT"
for base in texture-png texture-exr texture-rgbe unbaked-png unbaked-nonpot unbaked-exr unbaked-rgbe; do
  s="$base-raytrace"; rib="$W/examples/rib/tests/parity/$s.rib"
  tif=$(grep -o 'Display "[^"]*"' "$rib" | head -1 | sed 's/Display "//; s/"//')
  d="$OUT/$s"; mkdir -p "$d"
  sed 's/^WorldBegin/Option "shaderformat" "default" ["slo"]\nWorldBegin/' "$rib" > "$d/$s.rib"
  ( cd "$d" && "$B/src/orender/orender" -t:1 "$d/$s.rib" > "$d/orender.log" 2>&1 )
  printf '%s\t%s\n' "$s" "$(shasum -a 256 "$d/$tif" | cut -d' ' -f1)"
done
