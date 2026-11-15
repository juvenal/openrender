#!/usr/bin/env bash
# specs/048-oiio-image-io/tools/capture_baseline.sh <build-dir> <out-dir>   (T008)
#
# Captures the byte-identity oracle of spec 048: three renders and the four CLI
# modes of otexmake on tests/unit/image_input/fixtures/medium_rgb.tif, with a
# sha256 list. Run it for the starting tree (T009), for build/ and build-oiio/
# (T135) and compare the lists.
#
# DO NOT call cpujob from inside this script: cpujob holds one machine-wide lock
# and a nested call would deadlock. Run the script itself under it:
#   ~/.claude/bin/cpujob or048-p1-baseline bash capture_baseline.sh <build> <out>
#
# This is a tool, not a test. The renders mirror the environment of the tests that
# already render these scenes (tests/unit/texture_tile/CMakeLists.txt,
# tests/unit/image_input/CMakeLists.txt), so the oracle is the same render those
# tests compare.
#
# Output: <out-dir>/baseline.sha256 (sha256sum -c compatible, names relative to
# <out-dir>), the files themselves, <out-dir>/selfcheck.txt (the four medium_rgb
# bakes compared with the approved references of ImageInput_TiffBakeByteIdentical).
set -euo pipefail

BUILD="$(cd "$1" && pwd)"
mkdir -p "$2"
OUT="$(cd "$2" && pwd)"
WT="${OR048_WT:-$(cd "$(dirname "$0")/../../.." && pwd)}"

ORENDER="$BUILD/src/orender/orender"
OTEXMAKE="$BUILD/src/otexmake/otexmake"
[ -x "$ORENDER" ] && [ -x "$OTEXMAKE" ] || { echo "capture_baseline: orender/otexmake not built in $BUILD" >&2; exit 2; }

# Driver directories: every directory holding a *.dsply in the build tree
# (file, rgbe, framebuffer, openexr when present), as the tests' DISPLAYS does.
DISPLAYS="$(find "$BUILD/src/display" -name '*.dsply' -exec dirname {} \; | sort -u | paste -sd: -)"
FIX="$WT/tests/unit/image_input/fixtures"
TTFIX="$WT/tests/unit/texture_tile/fixtures"

export SHADERS="$BUILD/shaders"
export ORENDERHOME="$BUILD/tests/visual/_home"
export DISPLAYS
export TEXTURES="$WT/examples/rib/tests:$TTFIX:$FIX"
[ -d "$ORENDERHOME" ] || { echo "capture_baseline: $ORENDERHOME missing (configure first)" >&2; exit 2; }

render() { # <name> <rib> <output-file> [orender args...]
  local name="$1" rib="$2" outfile="$3"; shift 3
  local d; d="$(mktemp -d "${TMPDIR:-/tmp}/or048-render.XXXXXX")"
  ( cd "$d" && "$ORENDER" "$@" "$rib" > orender.log 2>&1 ) || { echo "capture_baseline: render $name failed"; cat "$d/orender.log"; exit 1; }
  [ -f "$d/$outfile" ] || { echo "capture_baseline: render $name wrote no $outfile"; cat "$d/orender.log"; exit 1; }
  cp "$d/$outfile" "$OUT/$name.${outfile##*.}"
  cp "$d/orender.log" "$OUT/$name.log"
  rm -rf "$d"
}

# Same arguments the tests use: camera-dof and the concurrency scene at the default
# thread count (spec 039: bit-identical at any count), the RGBE scene at -t:1
# (Display_RgbeBytes).
render camera-dof "$WT/examples/rib/camera-dof.rib" camera-dof.tif
render rgbe-display "$WT/tests/unit/image_input/rgbe-display.rib" rgbe-display.hdr -t:1
render concurrency-scene "$WT/tests/unit/texture_tile/concurrency-scene.rib" concurrency-scene.tif

# The four CLI modes of ImageInput_TiffBakeByteIdentical, same invocations.
F="$FIX/medium_rgb.tif"
"$OTEXMAKE" "$F" "$OUT/otexmake-texture.tex" > "$OUT/otexmake-texture.log" 2>&1 || true
"$OTEXMAKE" -shadow "$F" "$OUT/otexmake-shadow.tex" > "$OUT/otexmake-shadow.log" 2>&1 || true
"$OTEXMAKE" -envlatl "$F" "$OUT/otexmake-envlatl-cylindrical.tex" > "$OUT/otexmake-envlatl-cylindrical.log" 2>&1 || true
"$OTEXMAKE" -envcube "$F" "$F" "$F" "$F" "$F" "$F" "$OUT/otexmake-envcube-cubic.tex" > "$OUT/otexmake-envcube-cubic.log" 2>&1 || true

: > "$OUT/selfcheck.txt"
sc=0
for pair in texture:texture shadow:shadow envlatl-cylindrical:envlatl-cylindrical envcube-cubic:envcube-cubic; do
  a="${pair%%:*}"
  if [ -f "$OUT/otexmake-$a.tex" ] && cmp -s "$OUT/otexmake-$a.tex" "$FIX/references/$a.tex"; then
    echo "PASS otexmake-$a.tex equals fixtures/references/$a.tex" >> "$OUT/selfcheck.txt"
  else
    echo "FAIL otexmake-$a.tex differs from fixtures/references/$a.tex" >> "$OUT/selfcheck.txt"; sc=1
  fi
done

SHA() { if command -v sha256sum > /dev/null; then sha256sum "$@"; else shasum -a 256 "$@"; fi; }
( cd "$OUT" && SHA camera-dof.tif rgbe-display.hdr concurrency-scene.tif otexmake-texture.tex otexmake-shadow.tex otexmake-envlatl-cylindrical.tex otexmake-envcube-cubic.tex > baseline.sha256 )
cat "$OUT/baseline.sha256" "$OUT/selfcheck.txt"
exit $sc
