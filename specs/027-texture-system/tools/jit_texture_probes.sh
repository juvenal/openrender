#!/usr/bin/env bash
# jit_texture_probes.sh <build-dir>
# Spec 027 (research R16, T003): renders the texture, environment, shadow and unbaked parity
# scenes with the LLVM JIT shaders (Option "shaderformat" "default" ["slo"]) at -t:1, into
# <build-dir>/tests/jit-texture-probes/<scene>/, so `image_manifest.sh record` hashes the
# outputs with the suite's images. No existing -slo test renders texture(), environment()
# or shadow() (R16); these probes pin what the JIT sends today, so Phase 9 (which changes
# .slo output on purpose) is the only phase where they may differ.
# Prints "scene<TAB>ok|FAILED" per scene; exit 1 if any scene did not produce its image.
# JIT builds only; exit 0 with a message otherwise. Run it through cpujob.
set -u
B="$(cd "$1" && pwd)"
W="$(cd "$(dirname "$0")/../../.." && pwd)"
if ! grep -q '^OPENRENDER_ENABLE_JIT:BOOL=ON' "$B/CMakeCache.txt" 2>/dev/null; then
  echo "jit_texture_probes: $B is not a JIT build (OPENRENDER_ENABLE_JIT is not ON); nothing to do"
  exit 0
fi
OUT="$B/tests/jit-texture-probes"
rm -rf "$OUT"; mkdir -p "$OUT"
export SHADERS="$B/shaders" ORENDERHOME="$B/tests/visual/_home" DISPLAYS="$B/src/display/file"
export TEXTURES="$W/examples/rib/tests:$W/tests/unit/image_input/fixtures:$W/tests/unit/texture_tile/fixtures"
bad=0
for base in texture-png texture-exr texture-rgbe unbaked-png unbaked-nonpot unbaked-exr unbaked-rgbe env shadow; do
  for h in reyes raytrace; do
    s="$base-$h"; rib="$W/examples/rib/tests/parity/$s.rib"
    if [ ! -f "$rib" ]; then printf '%s\tMISSING-RIB\n' "$s"; bad=1; continue; fi
    tif=$(grep -o 'Display "[^"]*"' "$rib" | head -1 | sed 's/Display "//; s/"//')
    d="$OUT/$s"; mkdir -p "$d"
    sed 's/^WorldBegin/Option "shaderformat" "default" ["slo"]\nWorldBegin/' "$rib" > "$d/$s.rib"
    ( cd "$d" && "$B/src/orender/orender" -t:1 "$d/$s.rib" > "$d/orender.log" 2>&1 )
    if [ -s "$d/$tif" ]; then printf '%s\tok\n' "$s"; else printf '%s\tFAILED\n' "$s"; bad=1; fi
  done
done
exit $bad
