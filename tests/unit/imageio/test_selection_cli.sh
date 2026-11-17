#!/usr/bin/env bash
# tests/unit/imageio/test_selection_cli.sh -- ImageIO_Selection (spec 048, T073, FR-013, FR-014, P-Q15).
#
# Runs the C++ cases of test_selection (Option parsing, precedence, an unknown per-call value), then the command-line
# and file-level checks: otexmake -imageio, Option "imageio" in a RIB, and the spec 020 disk-cache entry name.
#
# Usage: test_selection_cli.sh <test_selection> <otexmake> <orender> <fixtures-dir> <cache-scene-template> <source-png>
# Environment (orender): SHADERS, ORENDERHOME, DISPLAYS
set -uo pipefail

TEST="$1"; OTEXMAKE="$2"; ORENDER="$3"; FIX="$4"; TEMPLATE="$5"; SOURCE_PNG="$6"

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
fail=0
check() { # <description> <condition-exit-status>
    if [ "$2" -eq 0 ]; then echo "ok:   $1"; else echo "FAIL: $1"; fail=1; fi
}

# ---- the C++ cases ----
for c in option precedence unknown_call; do
    "$TEST" "$c" > "$WORK/case-$c.log" 2>&1
    rc=$?
    check "test_selection $c" $rc
    [ $rc -eq 0 ] || cat "$WORK/case-$c.log"
done

TRNS="$FIX/differences/trns_rgb8_4x4.png"
RLE="$FIX/differences/rle_16x4_rle.hdr"
FLAT="$FIX/differences/rle_16x4_flat.hdr"

# ---- otexmake -imageio ----
"$OTEXMAKE" -imageio bogus "$TRNS" "$WORK/bad.tex" > "$WORK/bad.log" 2>&1; rc=$?
check "otexmake -imageio bogus exits 2" $([ $rc -eq 2 ] && echo 0 || echo 1)
check "otexmake -imageio bogus names the value and prints the usage" $(grep -q 'bogus' "$WORK/bad.log" && grep -q 'Usage: otexmake' "$WORK/bad.log" && echo 0 || echo 1)
check "otexmake -imageio bogus writes no file" $([ ! -e "$WORK/bad.tex" ] && echo 0 || echo 1)

"$OTEXMAKE" "$TRNS" "$WORK/x.tex" -imageio > "$WORK/missing.log" 2>&1; rc=$?
check "otexmake with -imageio and no value exits 2" $([ $rc -eq 2 ] && echo 0 || echo 1)

"$OTEXMAKE" -imageio legacy "$TRNS" "$WORK/a-legacy.tex" > /dev/null 2>&1; rc=$?
check "otexmake -imageio legacy exits 0" $rc
"$OTEXMAKE" "$TRNS" "$WORK/a-default.tex" > /dev/null 2>&1
check "-imageio legacy equals the default bake, byte for byte" $(cmp -s "$WORK/a-legacy.tex" "$WORK/a-default.tex" && echo 0 || echo 1)

"$OTEXMAKE" -imageio oiio "$TRNS" "$WORK/a-oiio.tex" > /dev/null 2>&1; rc=$?
check "otexmake -imageio oiio exits 0" $rc
check "the tRNS PNG bakes differently under oiio (4 samples per pixel against 3)" $(cmp -s "$WORK/a-oiio.tex" "$WORK/a-default.tex" && echo 1 || echo 0)

"$OTEXMAKE" -imageio oiio "$RLE" "$WORK/rle-oiio.tex" > /dev/null 2>&1; rc=$?
check "otexmake -imageio oiio decodes the RLE RGBE file" $rc
"$OTEXMAKE" "$FLAT" "$WORK/flat-legacy.tex" > /dev/null 2>&1
check "the RLE file under oiio bakes to the bytes of the flat file under legacy" $(cmp -s "$WORK/rle-oiio.tex" "$WORK/flat-legacy.tex" && echo 0 || echo 1)

# ---- Option "imageio" in a RIB: an unknown value is reported by the scene interface ----
cat > "$WORK/opt.rib" <<RIB
version 3.03
Option "imageio" "backend" ["bogus"]
Display "opt.tif" "file" "rgba"
Format 8 8 1
Projection "perspective"
Translate 0 0 4
WorldBegin
  Surface "constant"
  Sphere 1 -1 1 360
WorldEnd
RIB
(cd "$WORK" && "$ORENDER" opt.rib > opt.out 2> opt.err)
check "Option \"imageio\" \"backend\" [\"bogus\"] reports Unknown image backend" $(cat "$WORK/opt.out" "$WORK/opt.err" | grep -q 'Unknown image backend "bogus" (expected "legacy" or "oiio")' && echo 0 || echo 1)

# ---- the spec 020 disk-cache entry name: legacy names unchanged, ".oiio" only on the OIIO path (FR-014) ----
render_cached() { # <backend-statement> <cache-dir>
    mkdir -p "$2"
    sed -e "s|@CACHE_OPTION@|Option \"texturecache\" \"enable\" [1] \"directory\" [\"$2\"] $1|" \
        -e "s|@TEXTURE_PATH@|${SOURCE_PNG}|" "$TEMPLATE" > "$WORK/scene.rib"
    (cd "$WORK" && "$ORENDER" scene.rib > "$WORK/cache-render.log" 2>&1)
}
render_cached "" "$WORK/cache-legacy"
legacy_names="$(ls "$WORK/cache-legacy" 2>/dev/null)"
check "the default render wrote one cache entry" $([ "$(echo "$legacy_names" | grep -c .)" -eq 1 ] && echo 0 || echo 1)
check "its name is the legacy form <hash>-<mtime>.tex (no .oiio)" $(echo "$legacy_names" | grep -Eq '^[0-9a-f]+-[0-9]+\.tex$' && echo 0 || echo 1)
render_cached 'Option "imageio" "backend" ["oiio"]' "$WORK/cache-oiio"
oiio_names="$(ls "$WORK/cache-oiio" 2>/dev/null)"
check "the oiio render wrote one cache entry" $([ "$(echo "$oiio_names" | grep -c .)" -eq 1 ] && echo 0 || echo 1)
check "its name is <hash>-<mtime>.oiio.tex" $(echo "$oiio_names" | grep -Eq '^[0-9a-f]+-[0-9]+\.oiio\.tex$' && echo 0 || echo 1)

if [ "$fail" -ne 0 ]; then
    echo "test_selection_cli: FAILED"
    exit 1
fi
echo "test_selection_cli: ALL PASS"
