#!/usr/bin/env bash
# spec 024 research R4: run-to-run variance and crash rate of the photon-pass
# parity scene, plus the red run of the JIT variant.
#
# Usage (from the worktree root, after a build of this tree):
#   nice -n 19 specs/024-shading-services-routing/tools/measure-photon-parity.sh \
#       <abs build dir> <runs> <out dir> [orender thread arg, e.g. -t:1 or ""] [external ref.tif]
#
# Without an external reference, run 1 is the reference and runs 2..N are
# diffed against it. With one (the Linux cross-platform check against the
# macOS run 1 image), every run, run 1 included, is diffed against it.
#
# Produces <out dir>/summary.txt:
#   - per run: exit code, MaxBlockAvgDiff against the reference, .gpm size;
#   - crash count (nonzero orender exit) out of <runs>;
#   - red run: the "slo" variant against the reference, plus its .gpm size
#     ("absent" means no photon was stored, so no map was written).
# The map is deleted before every run: an unmodified map is never written, so
# a leftover file would otherwise be read back.
set -u
BUILD=${1:?build dir}; RUNS=${2:?runs}; OUT=${3:?out dir}; TARG=${4:-}; EXTREF=${5:-}
SRC=$(cd "$(dirname "$0")/../../.." && pwd)
TOOLS="$SRC/specs/024-shading-services-routing/tools"
mkdir -p "$OUT/shaders" "$OUT/home" "$OUT/work"
cp "$SRC/tests/visual/orenderrc" "$OUT/home/.orenderrc"
"$BUILD/src/oshader/oshader" -o "$OUT/shaders/photonparity_view.rslo" "$TOOLS/photonparity_view.sl" || exit 2

export SHADERS="$OUT/shaders:$BUILD/shaders"
export ORENDERHOME="$OUT/home"
export DISPLAYS="$BUILD/src/display/file"
export GEOMETRIES="$SRC/geometry"
VR="$BUILD/tests/visual/test_visual_render"
ORENDER="$BUILD/src/orender/orender"
S="$OUT/summary.txt"; : > "$S"
cd "$OUT/work" || exit 2

gpmsize() { if [ -f "$1" ]; then wc -c < "$1" | tr -d ' '; else echo absent; fi; }

crashes=0
if [ -n "$EXTREF" ]; then
  REF="$EXTREF"; first=1
  echo "reference: $EXTREF (external)" | tee -a "$S"
else
  # Run 1 becomes the reference image for the variance runs.
  rm -f photon-parity-*.gpm photon-parity-*.tif
  "$ORENDER" $TARG "$TOOLS/photon-parity-rslo.rib" > run1.log 2>&1; rc=$?
  cp -f photon-parity-rslo.tif ref.tif 2>/dev/null
  REF="$OUT/work/ref.tif"; first=2
  [ $rc -ne 0 ] && crashes=1
  echo "run 1 rc=$rc gpm=$(gpmsize photon-parity-rslo.gpm) (reference)" | tee -a "$S"
fi

for i in $(seq "$first" "$RUNS"); do
  rm -f photon-parity-*.gpm photon-parity-rslo.tif
  "$VR" "$ORENDER" "$TOOLS/photon-parity-rslo.rib" photon-parity-rslo.tif "$REF" 255 "$TARG" > run$i.log 2>&1
  rc=$?
  d=$(grep -o 'MaxBlockAvgDiff: [0-9.]*' run$i.log | awk '{print $2}')
  failed=$(grep -c 'orender failed' run$i.log)
  [ "$failed" -ne 0 ] && crashes=$((crashes+1))
  echo "run $i rc=$rc orender_failed=$failed diff=${d:-n/a} gpm=$(gpmsize photon-parity-rslo.gpm)" | tee -a "$S"
done
echo "crashes: $crashes / $RUNS" | tee -a "$S"

# Red run: the JIT variant on today's tree, against the interpreter reference.
rm -f photon-parity-*.gpm photon-parity-slo.tif
"$VR" "$ORENDER" "$TOOLS/photon-parity-slo.rib" photon-parity-slo.tif "$REF" 255 "$TARG" > red.log 2>&1
d=$(grep -o 'MaxBlockAvgDiff: [0-9.]*' red.log | awk '{print $2}')
echo "red slo: diff=${d:-n/a} gpm=$(gpmsize photon-parity-slo.gpm)" | tee -a "$S"
