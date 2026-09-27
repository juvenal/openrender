#!/usr/bin/env bash
# tests/unit/texture_tile/test_texture_cache_byte_identical.sh
#
# Disk-cache-vs-live-synthesis byte-identical test for spec 020
# (020-runtime-tile-paging, T019, SC-003/research.md SS9): proves the
# opt-in disk cache's read-back path (CTiffTileSource, via an ordinary
# baked .tex the cache wrote) and the in-memory synthesis path
# (CSynthesizedTileSource, cache disabled) produce byte-identical rendered
# output for the same source.
#
# Sequence (research.md SS9's exact design, not a shortcut):
#   1. Populate: cache enabled, empty cache dir -> a cache MISS, so this
#      render synthesizes in memory AND writes a fresh cache entry.
#      Its own output is discarded -- it isn't the comparison basis.
#   2. Hit: cache enabled, SAME cache dir (now populated by step 1) -> a
#      cache HIT, reading back through the completely ordinary,
#      unmodified CTiffTileSource path. This output is kept (A).
#   3. Disabled: cache disabled entirely -> always synthesizes in memory
#      via CSynthesizedTileSource, cache never consulted. This output is
#      kept (B).
#   cmp A and B byte-for-byte.
#
# All renders "-t:1" (single-threaded): this codebase's default
# multi-threaded reyes/raytrace rendering is not byte-reproducible run to
# run (bucket-scheduling nondeterminism, established during spec 019's own
# planning) -- see tests/visual/CMakeLists.txt's existing "-t:1" usage for
# other byte-identical-sensitive scenes.
#
# Usage: test_texture_cache_byte_identical.sh <orender> <scene-template> <source-png>
set -euo pipefail

ORENDER="$1"
TEMPLATE="$2"
SOURCE_PNG="$3"

WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

CACHE_DIR="$WORKDIR/cache"
mkdir -p "$CACHE_DIR"

render() {
    local cache_option="$1"
    local out_name="$2"

    sed -e "s|@CACHE_OPTION@|${cache_option}|" \
        -e "s|@TEXTURE_PATH@|${SOURCE_PNG}|" \
        "$TEMPLATE" > "$WORKDIR/scene.rib"

    (cd "$WORKDIR" && "$ORENDER" -t:1 scene.rib >/dev/null 2>&1) || true

    if [ ! -f "$WORKDIR/cache-scene.tif" ]; then
        echo "FAIL: render did not produce an output file (option: $cache_option)"
        exit 1
    fi

    mv "$WORKDIR/cache-scene.tif" "$WORKDIR/$out_name"
}

echo "=== Step 1/3: populate (cache miss + write) ==="
render "Option \"texturecache\" \"enable\" [1] \"directory\" [\"${CACHE_DIR}\"]" "populate.tif"

if [ -z "$(find "$CACHE_DIR" -name '*.tex' -print -quit)" ]; then
    echo "FAIL: populate render did not write a cache entry to $CACHE_DIR"
    exit 1
fi

echo "=== Step 2/3: hit (read back via CTiffTileSource) ==="
render "Option \"texturecache\" \"enable\" [1] \"directory\" [\"${CACHE_DIR}\"]" "hit.tif"

echo "=== Step 3/3: disabled (in-memory CSynthesizedTileSource) ==="
render "Option \"texturecache\" \"enable\" [0]" "disabled.tif"

if cmp -s "$WORKDIR/hit.tif" "$WORKDIR/disabled.tif"; then
    echo "test_texture_cache_byte_identical: ALL PASS (byte-identical)"
else
    echo "FAIL: cache-hit output differs from cache-disabled output"
    cmp "$WORKDIR/hit.tif" "$WORKDIR/disabled.tif" || true
    exit 1
fi
