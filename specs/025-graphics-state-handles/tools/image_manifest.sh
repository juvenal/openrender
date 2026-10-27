#!/usr/bin/env bash
# image_manifest.sh: prove "byte-identical output" for spec 025 phases 3-6 (CP-d).
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
#       every MaxBlockAvgDiff equals the baseline (tools/baseline-diffs.txt). ctest prints this
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
    sed -n '2,22p' "$0" | sed 's/^# \{0,1\}//' >&2
    exit 2
}

cmd="${1:-}"
case "$cmd" in
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
