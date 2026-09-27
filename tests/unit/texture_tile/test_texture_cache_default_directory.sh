#!/usr/bin/env bash
# tests/unit/texture_tile/test_texture_cache_default_directory.sh
#
# Default cache-directory resolution test for spec 020
# (020-runtime-tile-paging, T018b, analysis finding U1): renders a scene
# with "Option \"texturecache\" \"enable\" [1]" alone -- no "directory"
# token at all, the configuration a user would actually try first --
# confirming a cache entry is written to and read back from whatever the
# resolved default location actually is, not just the explicit-override
# path T017/T018/T019 were otherwise exercised against.
#
# Isolation: resolveDefaultTextureCacheDirectory() (texture.cpp) resolves
# TMPDIR/TMP-then-/tmp, appending a fixed "openRenderTextureCache/"
# subdirectory -- the SAME real, shared, system-wide location every
# render on this machine would use by default. Overriding TMPDIR to this
# test's own private $WORKDIR before launching orender redirects that
# resolution into an isolated sandbox, so this test can neither pollute
# nor race against the real default directory (or another concurrent
# ctest run doing the same).
#
# Usage: test_texture_cache_default_directory.sh <orender> <scene-template> <source-png>
set -euo pipefail

ORENDER="$1"
TEMPLATE="$2"
SOURCE_PNG="$3"

WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

sed -e "s|@CACHE_OPTION@|Option \"texturecache\" \"enable\" [1]|" \
    -e "s|@TEXTURE_PATH@|${SOURCE_PNG}|" \
    "$TEMPLATE" > "$WORKDIR/scene.rib"

DEFAULT_CACHE_DIR="$WORKDIR/openRenderTextureCache"

echo "=== Render 1 (expect a miss: no cache entry exists yet) ==="
(cd "$WORKDIR" && TMPDIR="$WORKDIR" "$ORENDER" -t:1 scene.rib >/dev/null 2>&1) || true

if [ ! -f "$WORKDIR/cache-scene.tif" ]; then
    echo "FAIL: render 1 did not produce an output file"
    exit 1
fi
mv "$WORKDIR/cache-scene.tif" "$WORKDIR/run1.tif"

if [ -z "$(find "$DEFAULT_CACHE_DIR" -name '*.tex' -print -quit 2>/dev/null)" ]; then
    echo "FAIL: no cache entry found under the resolved default directory ($DEFAULT_CACHE_DIR)"
    exit 1
fi
echo "Cache entry found under the default directory: $DEFAULT_CACHE_DIR"

echo "=== Render 2 (expect a hit against the same default-resolved entry) ==="
(cd "$WORKDIR" && TMPDIR="$WORKDIR" "$ORENDER" -t:1 scene.rib >/dev/null 2>&1) || true

if [ ! -f "$WORKDIR/cache-scene.tif" ]; then
    echo "FAIL: render 2 did not produce an output file"
    exit 1
fi
mv "$WORKDIR/cache-scene.tif" "$WORKDIR/run2.tif"

if cmp -s "$WORKDIR/run1.tif" "$WORKDIR/run2.tif"; then
    echo "test_texture_cache_default_directory: ALL PASS (default-directory cache entry written and reused, byte-identical)"
else
    echo "FAIL: run 1 and run 2 outputs differ"
    cmp "$WORKDIR/run1.tif" "$WORKDIR/run2.tif" || true
    exit 1
fi
