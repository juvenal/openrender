#!/usr/bin/env bash
# image_manifest.sh: spec 040. A copy of specs/032-gi-cache-split/tools/image_manifest.sh (032's copy is not edited), with two
# modes added for the 18 GI scenes of tests/visual/determinism-scenes.cmake ("GI scenes: not swept (spec 040)"):
#   gi-render <build-dir> <out-dir> [orender options...]
#       renders each GI scene (the list GI_SCENES below) with <build-dir>'s orender from a scratch copy, in its own directory
#       <out-dir>/<scene>/, with the options given (default -t:1), the scene's own pins left in place and
#       `Quantize "rgba" 0 0 0 0` put on the first line of the copy (float output; a scene's own later Quantize wins).
#       The directory keeps the copy of the scene, every Display image and every map or cache the scene writes.
#       The environment is the Visual_* tests' for that build tree (measure_gi.py's env_for()).
#   gi-hash <out-dir>
#       print "sha256  <scene>/<file>" for every .tif, .png and .exr under <out-dir>, sorted (the manifest of a gi-render).
# The older modes below are 032's, unchanged.
#
#   image_manifest.sh stamp  <build-dir>
#       touch <build-dir>/.image_stamp (run it just before the test suite).
#   image_manifest.sh record <build-dir>
#       print "sha256  relative-path" for every *.tif, *.png and *.exr under
#       <build-dir>/tests, plus <build-dir>/imager_*_test.tif (the Imager*Render outputs at
#       the build root), that is NEWER than the stamp, sorted by path. A stale file from an
#       earlier run, or a copied fixture, is never hashed.
#   image_manifest.sh diff   <manifest> <build-dir> [noise-list]
#       report every image in the manifest that differs or is missing (not fresh), and list
#       fresh images that are not in the manifest as information only. Paths named in the
#       noise list (one per line, '#' comments allowed) are ignored. Exit 1 on any
#       difference or missing image, 0 otherwise.
#   image_manifest.sh diffs  <LastTest.log>
#       print "test<TAB>MaxBlockAvgDiff[,...]" sorted by test name, for the CP-d check that
#       every MaxBlockAvgDiff equals the baseline (specs/028-camera-and-motion-layer/tools/baseline-diffs-<platform>.txt). ctest prints this
#       only for failing tests, so read ctest's own Testing/Temporary/LastTest.log, and copy it
#       before the next ctest run overwrites it.
#
# Images are discovered generically: no list of test directories, and no dependence on a
# first run. Works with shasum (macOS) and sha256sum (Linux).

set -u
export LC_ALL=C

if command -v sha256sum >/dev/null 2>&1; then
    hash_cmd() { sha256sum "$@"; }
else
    hash_cmd() { shasum -a 256 "$@"; }
fi

usage() {
    sed -n '2,36p' "$0" | sed 's/^# \{0,1\}//' >&2
    exit 2
}

GI_SCENES="parity/motion-subdiv-translate-photon photon-parity-slo photon-parity sampling-probe-independence-mid
sampling-probe-independence-min sphere-indirectdiffuse-reyes-slo sphere-indirectdiffuse-reyes sphere-occlusion-reyes-slo
sphere-occlusion-reyes sphere-photonmap-reyes-slo sphere-photonmap-reyes sphere-ray-self-hit-raytrace
sphere-ray-self-hit-reyes-slo sphere-ray-self-hit-reyes subdiv-facevarying-seam-photon subdiv-hierarchical-override-photon
subdiv-loop-photon subdiv-new-tags-photon"

cmd="${1:-}"
case "$cmd" in
gi-render)
    [ $# -ge 3 ] || usage
    bd="$(cd "$2" && pwd)"; out="$3"; shift 3
    [ $# -gt 0 ] || set -- -t:1
    root="$(cd "$(dirname "$0")/../../.." && pwd)"
    mkdir -p "$out"; out="$(cd "$out" && pwd)"
    disp=""
    for d in file rgbe framebuffer openexr; do [ -d "$bd/src/display/$d" ] && disp="${disp:+$disp:}$bd/src/display/$d"; done
    libpath=""
    if [ "$(uname)" = Linux ]; then
        libpath="$(find "$bd/src" -name '*.so*' -printf '%h\n' | sort -u | tr '\n' ':')"
    fi
    for s in $GI_SCENES; do
        name="$(basename "$s")"
        wd="$out/$name"
        rm -rf "$wd"; mkdir -p "$wd"
        { echo 'Quantize "rgba" 0 0 0 0'; cat "$root/examples/rib/tests/$s.rib"; } > "$wd/$name.rib"
        ( cd "$wd" && env SHADERS="$bd/shaders" ORENDERHOME="$bd/tests/visual/_home" DISPLAYS="$disp" \
              GEOMETRIES="$root/geometry" TEXTURES="$root/examples/rib/tests" ${libpath:+LD_LIBRARY_PATH="$libpath${LD_LIBRARY_PATH:-}"} \
              "$bd/src/orender/orender" "$@" "$name.rib" > render.log 2>&1 ) ; rc=$?
        echo "$name: exit $rc"
        [ $rc -eq 0 ] || echo "  FAILED (see $wd/render.log)"
    done
    ;;
gi-hash)
    [ $# -eq 2 ] || usage
    cd "$2" || exit 2
    find . -type f \( -name '*.tif' -o -name '*.png' -o -name '*.exr' \) | sed 's|^\./||' | sort | while read -r f; do hash_cmd "$f"; done
    ;;
stamp)
    [ $# -eq 2 ] || usage
    [ -d "$2" ] || { echo "image_manifest: $2 is not a directory" >&2; exit 2; }
    touch "$2/.image_stamp"
    echo "stamped $2/.image_stamp"
    ;;
record)
    [ $# -eq 2 ] || usage
    bd="$2"
    [ -f "$bd/.image_stamp" ] || { echo "image_manifest: no stamp in $bd (run: stamp $bd)" >&2; exit 2; }
    [ -d "$bd/tests" ] || { echo "image_manifest: $bd/tests does not exist" >&2; exit 2; }
    cd "$bd" || exit 2
    { find tests -type f \( -name '*.tif' -o -name '*.png' -o -name '*.exr' \) \
          -newer .image_stamp -print0
      # the four Imager*Render tests write their outputs at the build root
      for f in imager_*_test.tif; do
          [ -f "$f" ] && [ "$f" -nt .image_stamp ] && printf '%s\0' "$f"
      done
    } | sort -z | while IFS= read -r -d '' f; do
        hash_cmd "$f"
    done
    ;;
diff)
    [ $# -ge 3 ] && [ $# -le 4 ] || usage
    manifest="$2"; bd="$3"; noise="${4:-}"
    [ -f "$manifest" ] || { echo "image_manifest: no manifest $manifest" >&2; exit 2; }
    tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT
    "$0" record "$bd" > "$tmp/fresh" || exit 2
    # path-keyed tables: "path<TAB>hash"
    awk '{h=$1; $1=""; sub(/^ +/,""); sub(/^\*/,""); print $0 "\t" h}' "$manifest" | sort > "$tmp/base"
    awk '{h=$1; $1=""; sub(/^ +/,""); sub(/^\*/,""); print $0 "\t" h}' "$tmp/fresh"  | sort > "$tmp/new"
    if [ -n "$noise" ] && [ -f "$noise" ]; then
        grep -v '^[[:space:]]*#' "$noise" | sed '/^[[:space:]]*$/d' | sort > "$tmp/noise"
    else
        : > "$tmp/noise"
    fi
    differ=0; missing=0; extra=0
    while IFS=$'\t' read -r path h; do
        if grep -qxF -- "$path" "$tmp/noise"; then continue; fi
        nh="$(awk -F'\t' -v p="$path" '$1==p {print $2; exit}' "$tmp/new")"
        if [ -z "$nh" ]; then
            echo "MISSING  $path"; missing=$((missing+1))
        elif [ "$nh" != "$h" ]; then
            echo "DIFFERS  $path"; differ=$((differ+1))
        fi
    done < "$tmp/base"
    while IFS=$'\t' read -r path h; do
        if ! awk -F'\t' -v p="$path" '$1==p {f=1} END{exit !f}' "$tmp/base"; then
            echo "INFO     fresh image not in manifest: $path"; extra=$((extra+1))
        fi
    done < "$tmp/new"
    echo "manifest $(wc -l < "$tmp/base" | tr -d ' ') images, fresh $(wc -l < "$tmp/new" | tr -d ' '), noise-listed $(wc -l < "$tmp/noise" | tr -d ' '); $differ differing, $missing missing, $extra not in manifest"
    [ $((differ+missing)) -eq 0 ]
    ;;
diffs)
    [ $# -eq 2 ] || usage
    [ -f "$2" ] || { echo "image_manifest: no log $2" >&2; exit 2; }
    awk '/^[0-9]+\/[0-9]+ Test:/ { cur=$3; next }
         /MaxBlockAvgDiff:/ { for (i=1;i<=NF;i++) if ($i=="MaxBlockAvgDiff:") v[cur]=(cur in v ? v[cur] "," : "") $(i+1) }
         END { for (k in v) print k "\t" v[k] }' "$2" | sort
    ;;
*)
    usage
    ;;
esac
