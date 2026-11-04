#!/bin/sh
# R14a (spec 038, research R14a): what the compiler's passes do to DSO lines.
#
#   r14a.sh <worktree> <workdir>
#
# Compiles the five probe shaders of this directory to .rslo with the build's oshader against the
# spec 037 shadeop fixture directory, prints each DSO line and the instructions around it, then
# renders r14a_diffargs through orender and prints the centre pixel (a red/green/blue triple in
# 0..255). oshader has no switch that turns a pass off (the five passes are added unconditionally in
# rslo.cpp:1676-1682, and OPENRENDER_DUMP_IR prints only the pass names), so the "passes off" side
# of the comparison is the shader source: the expected DSO lines are the calls in source order.
# Run through: ~/.claude/bin/cpujob 038-p1-r14a <this script> ...
set -e
WT="$1"; WORK="$2"
OSHADER="$WT/build/src/oshader/oshader"
FIX="$WT/build/tests/plugins/shadeop/has_exec"
SRC="$WT/specs/038-scene-interface-untangle/tools/r14"
mkdir -p "$WORK"
for p in dup unused readback loop diffargs; do
    echo "=== r14a_$p.sl"
    "$OSHADER" -q -I "$FIX" -o "$WORK/r14a_$p.rslo" "$SRC/r14a_$p.sl" >/dev/null
    sed -n '/^#!Code:/,$p' "$WORK/r14a_$p.rslo"
done
echo "=== r14c_vary.sl"
"$OSHADER" -q -I "$FIX" -o "$WORK/r14c_vary.rslo" "$SRC/r14c_vary.sl" >/dev/null
sed -n "/^#!variables:/,\$p" "$WORK/r14c_vary.rslo"
cat > "$WORK/r14a.rib" <<RIB
Option "searchpath" "shader" ["$WORK:$WT/build/shaders"]
Option "searchpath" "display" ["$WT/build/src/display/file"]
Option "searchpath" "procedural" ["$FIX"]
Option "limits" "numthreads" [1]
Format 8 8 1
PixelSamples 1 1
PixelFilter "box" 1 1
Projection "perspective" "fov" 30
Translate 0 0 4
Display "r14a.tif" "file" "rgb"
WorldBegin
  Surface "r14a_diffargs"
  Sphere 1 -1 1 360
WorldEnd
RIB
(cd "$WORK" && SHADERS="$WORK:$WT/build/shaders" ORENDERHOME="$WT" "$WT/build/src/orender/orender" r14a.rib >/dev/null)
tiffcp -c none "$WORK/r14a.tif" "$WORK/r14a_raw.tif"
python3 -I - "$WORK/r14a_raw.tif" <<'PY'
import struct, sys
d = open(sys.argv[1], 'rb').read()
e = '<' if d[:2] == b'II' else '>'
off = struct.unpack(e + 'I', d[4:8])[0]
n = struct.unpack(e + 'H', d[off:off + 2])[0]
tags = {}
for i in range(n):
    t, ty, c, v = struct.unpack(e + 'HHII', d[off + 2 + 12 * i:off + 14 + 12 * i])
    tags[t] = (ty, c, v)
strip = tags[273][2]
if tags[273][1] > 1:
    strip = struct.unpack(e + 'I', d[strip:strip + 4])[0]
w = tags[256][2]
p = strip + (4 * w + 4) * 3
print('centre pixel of r14a_diffargs: R=%d G=%d B=%d (a=0.75 -> 191; b=0.3 -> 76 expected for G)' % (d[p], d[p + 1], d[p + 2]))
PY
