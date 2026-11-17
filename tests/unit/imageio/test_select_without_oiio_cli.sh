#!/usr/bin/env bash
# tests/unit/imageio/test_select_without_oiio_cli.sh -- ImageIO_SelectWithoutOIIO (spec 048, T074, FR-003, P-Q6).
#
# A build WITHOUT OpenImageIO: the C++ cases of test_select_without_oiio, then
#   - Option "imageio" "backend" ["oiio"] in a RIB reports ONE error and the scene renders with legacy: the image is
#     byte-identical to the render of the same scene without the Option;
#   - otexmake -imageio oiio prints "OpenImageIO support is not built into this binary", exits 1, writes no file;
#   - Display "x" "oiio" gets the display host's existing "Failed to open out device" error.
#
# Usage: test_select_without_oiio_cli.sh <test_select_without_oiio> <otexmake> <orender> <fixtures-dir>
# Environment (orender): SHADERS, ORENDERHOME, DISPLAYS
set -uo pipefail

TEST="$1"; OTEXMAKE="$2"; ORENDER="$3"; FIX="$4"

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
fail=0
check() { if [ "$2" -eq 0 ]; then echo "ok:   $1"; else echo "FAIL: $1"; fail=1; fi; }

for c in option call; do
    "$TEST" "$c" > "$WORK/case-$c.log" 2>&1; rc=$?
    check "test_select_without_oiio $c" $rc
    [ $rc -eq 0 ] || cat "$WORK/case-$c.log"
done

NOT_BUILT="OpenImageIO support is not built into this binary"

# ---- otexmake ----
"$OTEXMAKE" -imageio oiio "$FIX/differences/trns_rgb8_4x4.png" "$WORK/o.tex" > "$WORK/otex.log" 2>&1; rc=$?
check "otexmake -imageio oiio exits 1" $([ $rc -eq 1 ] && echo 0 || echo 1)
check "otexmake -imageio oiio says OpenImageIO is not built in" $(grep -q "$NOT_BUILT" "$WORK/otex.log" && echo 0 || echo 1)
check "otexmake -imageio oiio writes no file" $([ ! -e "$WORK/o.tex" ] && echo 0 || echo 1)

# ---- the scene: with and without the Option ----
scene() { # <option-line> <output-name>
cat > "$WORK/$2.rib" <<RIB
version 3.03
$1
Hider "reyes"
Display "$2.tif" "file" "rgba"
Format 16 16 1
PixelSamples 1 1
Projection "perspective"
Translate 0 0 4
WorldBegin
  LightSource "ambientlight" 1 "intensity" [0.5]
  Surface "plastic"
  Sphere 1 -1 1 360
WorldEnd
RIB
}
scene "" plain
scene 'Option "imageio" "backend" ["oiio"]' withopt
(cd "$WORK" && "$ORENDER" -t:1 plain.rib > plain.out 2> plain.err)
(cd "$WORK" && "$ORENDER" -t:1 withopt.rib > withopt.out 2> withopt.err)
check "the Option scene reports the not-built error exactly once" $([ "$(cat "$WORK/withopt.out" "$WORK/withopt.err" | grep -c "$NOT_BUILT")" -eq 1 ] && echo 0 || echo 1)
check "both scenes wrote an image" $([ -f "$WORK/plain.tif" ] && [ -f "$WORK/withopt.tif" ] && echo 0 || echo 1)
check "the image is byte-identical to the render without the Option (legacy)" $(cmp -s "$WORK/plain.tif" "$WORK/withopt.tif" && echo 0 || echo 1)

# ---- Display "x" "oiio" ----
cat > "$WORK/disp.rib" <<RIB
version 3.03
Display "disp.tif" "oiio" "rgba"
Format 8 8 1
Projection "perspective"
Translate 0 0 4
WorldBegin
  Surface "constant"
  Sphere 1 -1 1 360
WorldEnd
RIB
(cd "$WORK" && "$ORENDER" disp.rib > disp.out 2> disp.err)
check "Display \"x\" \"oiio\" gets the host's 'Failed to open out device' error" $(cat "$WORK/disp.out" "$WORK/disp.err" | grep -q 'Failed to open out device "oiio"' && echo 0 || echo 1)
check "Display \"x\" \"oiio\" writes no file" $([ ! -e "$WORK/disp.tif" ] && echo 0 || echo 1)

if [ "$fail" -ne 0 ]; then
    echo "test_select_without_oiio_cli: FAILED"
    exit 1
fi
echo "test_select_without_oiio_cli: ALL PASS"
