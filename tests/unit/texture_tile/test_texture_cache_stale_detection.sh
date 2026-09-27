#!/usr/bin/env bash
# tests/unit/texture_tile/test_texture_cache_stale_detection.sh
#
# Stale-cache detection test for spec 020 (020-runtime-tile-paging, T020,
# SC-004): populates the cache for a source, replaces that source's
# content at the SAME path (simulating an artist re-exporting/editing the
# texture), re-renders, and confirms (a) the render output reflects the
# NEW content, not stale cached data, and (b) a NEW cache filename now
# exists (the source-mtime-encoding key, research.md SS5, naturally
# changed) rather than the old entry being silently reused.
#
# Uses two genuinely different fixtures (large_rgb8.png, spec 018's
# tests/unit/image_input/fixtures/; then cache_stale_v2.png, this spec's
# own fixtures/FIXTURES.md) copied to the SAME private path in turn --
# not just a bare `touch` of one file's mtime -- because a bug that
# served the stale cache entry would otherwise be indistinguishable from
# correct behavior (identical pixels either way). This test needs the
# pixel CONTENT to genuinely differ to be meaningful.
#
# Usage: test_texture_cache_stale_detection.sh <orender> <scene-template> <before-png> <after-png>
set -euo pipefail

ORENDER="$1"
TEMPLATE="$2"
BEFORE_PNG="$3"
AFTER_PNG="$4"

WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

CACHE_DIR="$WORKDIR/cache"
mkdir -p "$CACHE_DIR"

SOURCE_PATH="$WORKDIR/source.png"

sed -e "s|@CACHE_OPTION@|Option \"texturecache\" \"enable\" [1] \"directory\" [\"${CACHE_DIR}\"]|" \
    -e "s|@TEXTURE_PATH@|${SOURCE_PATH}|" \
    "$TEMPLATE" > "$WORKDIR/scene.rib"

render() {
    local out_name="$1"
    (cd "$WORKDIR" && "$ORENDER" -t:1 scene.rib >/dev/null 2>&1) || true
    if [ ! -f "$WORKDIR/cache-scene.tif" ]; then
        echo "FAIL: render did not produce an output file"
        exit 1
    fi
    mv "$WORKDIR/cache-scene.tif" "$WORKDIR/$out_name"
}

echo "=== Render 1: original source, populates the cache ==="
cp "$BEFORE_PNG" "$SOURCE_PATH"
render "before.tif"

before_entries="$(find "$CACHE_DIR" -name '*.tex' | sort)"
if [ -z "$before_entries" ]; then
    echo "FAIL: no cache entry written after render 1"
    exit 1
fi

echo "=== Replacing source content at the same path (simulated re-export) ==="
# A plain `cp -f` may leave the destination's mtime identical to the
# source's if both land within the same filesystem-mtime-resolution
# window; force a fresh mtime explicitly so this test cannot flake on a
# coarse-grained filesystem clock.
cp "$AFTER_PNG" "$SOURCE_PATH"
touch -d '+1 second' "$SOURCE_PATH" 2>/dev/null || touch -t "$(date -v+1S +%Y%m%d%H%M.%S)" "$SOURCE_PATH" 2>/dev/null || sleep 1

echo "=== Render 2: modified source, must detect staleness and rebuild ==="
render "after.tif"

after_entries="$(find "$CACHE_DIR" -name '*.tex' | sort)"

if cmp -s "$WORKDIR/before.tif" "$WORKDIR/after.tif"; then
    echo "FAIL: render output identical before/after modifying the source -- stale cache data was served"
    exit 1
fi
echo "PASS: render output changed after modifying the source (not stale)"

new_count="$(find "$CACHE_DIR" -name '*.tex' | wc -l | tr -d ' ')"
if [ "$new_count" -lt 2 ]; then
    echo "FAIL: expected a new cache entry alongside the old one (found $new_count under $CACHE_DIR); old entry may have been overwritten or reused instead of a fresh key being computed"
    echo "before: $before_entries"
    echo "after:  $after_entries"
    exit 1
fi
echo "PASS: a new cache filename now exists ($new_count entries total) -- old entry orphaned, not reused"

echo "test_texture_cache_stale_detection: ALL PASS"
