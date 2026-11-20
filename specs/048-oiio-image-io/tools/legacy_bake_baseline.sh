#!/usr/bin/env bash
# specs/048-oiio-image-io/tools/legacy_bake_baseline.sh <build-dir> <out-prefix>   (coordinator addition to Phase 1)
#
# Records the sha256 of the LEGACY otexmake bake of every existing input, on the
# unmodified starting tree and with no new code. It is the starting-tree half of
# the M2 pin: T093 and T135 later check the legacy writer against it.
#
# DO NOT call cpujob from inside this script (a nested call deadlocks on the
# machine-wide lock). Run the script itself under it:
#   ~/.claude/bin/cpujob or048-p1-legacybake bash legacy_bake_baseline.sh <build> <out-prefix>
#
# Writes <out-prefix>.sha256 (sha256sum -c compatible; one line per successful bake,
# the name encodes input and mode: <input>.<mode>.tex), <out-prefix>.status (one line
# per attempt: input, mode, exit code, whether an output file exists, run-to-run
# determinism) and <out-prefix>.commands (the command template of each mode).
#
# Inputs: every file of tests/unit/image_input/fixtures (spec 018: TIFF, PNG, EXR,
# RGBE, tiny/medium/large, the negative fixtures included, which must fail) plus
# tests/unit/texture_tile/fixtures/nonpot_rgb.png (a non power of two source).
# Modes: the four otexmake CLI modes of ImageInput_TiffBakeByteIdentical. -envcube
# takes the same file six times. Each bake runs twice into different directories;
# a hash that differs between the two runs is reported as NONDETERMINISTIC and is
# left out of the .sha256.
set -euo pipefail

BUILD="$(cd "$1" && pwd)"
PREFIX="$2"
WT="${OR048_WT:-$(cd "$(dirname "$0")/../../.." && pwd)}"
OTEXMAKE="$BUILD/src/otexmake/otexmake"
[ -x "$OTEXMAKE" ] || { echo "legacy_bake_baseline: otexmake not built in $BUILD" >&2; exit 2; }
FIX="$WT/tests/unit/image_input/fixtures"
TTFIX="$WT/tests/unit/texture_tile/fixtures"

SHA() { if command -v sha256sum > /dev/null; then sha256sum "$1" | cut -d' ' -f1; else shasum -a 256 "$1" | cut -d' ' -f1; fi; }

W="$(mktemp -d "${TMPDIR:-/tmp}/or048-legacybake.XXXXXX")"
trap 'rm -rf "$W"' EXIT
mkdir -p "$W/r1" "$W/r2"
: > "$PREFIX.sha256"; : > "$PREFIX.status"
cat > "$PREFIX.commands" <<'CMDS'
texture : otexmake <input> <input>.texture.tex
shadow  : otexmake -shadow <input> <input>.shadow.tex
envlatl : otexmake -envlatl <input> <input>.envlatl.tex          (cylinder environment)
envcube : otexmake -envcube <input> <input> <input> <input> <input> <input> <input>.envcube.tex   (cubic environment, the same file six times)
CMDS

inputs=()
for f in "$FIX"/*.tif "$FIX"/*.png "$FIX"/*.exr "$FIX"/*.hdr "$TTFIX/nonpot_rgb.png"; do
  [ -f "$f" ] && inputs+=("$f")
done

bake() { # <dir> <mode> <input>   -> exit code on stdout
  local d="$1" mode="$2" in="$3" out
  out="$d/$(basename "$in").$mode.tex"
  case "$mode" in
    texture) "$OTEXMAKE" "$in" "$out" > "$out.log" 2>&1 ;;
    shadow)  "$OTEXMAKE" -shadow "$in" "$out" > "$out.log" 2>&1 ;;
    envlatl) "$OTEXMAKE" -envlatl "$in" "$out" > "$out.log" 2>&1 ;;
    envcube) "$OTEXMAKE" -envcube "$in" "$in" "$in" "$in" "$in" "$in" "$out" > "$out.log" 2>&1 ;;
  esac
}

for in in "${inputs[@]}"; do
  b="$(basename "$in")"
  # the real directory of the input, for the record
  rel="${in#$WT/}"
  for mode in texture shadow envlatl envcube; do
    name="$b.$mode.tex"
    rc1=0; bake "$W/r1" "$mode" "$in" || rc1=$?
    rc2=0; bake "$W/r2" "$mode" "$in" || rc2=$?
    have1=no; [ -s "$W/r1/$name" ] && have1=yes
    have2=no; [ -s "$W/r2/$name" ] && have2=yes
    if [ "$have1" = yes ] && [ "$have2" = yes ]; then
      h1="$(SHA "$W/r1/$name")"; h2="$(SHA "$W/r2/$name")"
      if [ "$h1" = "$h2" ]; then
        echo "$h1  $name" >> "$PREFIX.sha256"
        echo "OK exit=$rc1 output=yes deterministic=yes $rel $mode" >> "$PREFIX.status"
      else
        echo "NONDETERMINISTIC exit=$rc1/$rc2 output=yes $rel $mode" >> "$PREFIX.status"
      fi
    else
      echo "NOOUTPUT exit=$rc1/$rc2 output=$have1/$have2 $rel $mode" >> "$PREFIX.status"
    fi
  done
done

# Self-check: the four medium_rgb.tif entries must equal the approved references of
# ImageInput_TiffBakeByteIdentical (proves this invocation is the approved one).
for pair in texture:texture shadow:shadow envlatl:envlatl-cylindrical envcube:envcube-cubic; do
  mode="${pair%%:*}"; ref="${pair##*:}"
  want="$(SHA "$FIX/references/$ref.tex")"
  got="$(grep "  medium_rgb.tif.$mode.tex\$" "$PREFIX.sha256" | cut -d' ' -f1 || true)"
  if [ "$got" = "$want" ]; then echo "SELFCHECK PASS medium_rgb.tif $mode == references/$ref.tex" >> "$PREFIX.status"
  else echo "SELFCHECK FAIL medium_rgb.tif $mode (got '$got' want '$want')" >> "$PREFIX.status"; fi
done
echo "legacy_bake_baseline: $(wc -l < "$PREFIX.sha256" | tr -d ' ') hashes, $(grep -c . "$PREFIX.status") status lines"
