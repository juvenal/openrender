#!/usr/bin/env bash
# tests/unit/texture_tile/test_texture_cache_long_resolved_path.sh
#
# Regression test for GitHub #46: the disk cache resolves its source to an
# absolute path with realpath(), and used to hand it a 512-byte buffer
# (OS_MAX_PATH_LENGTH), smaller than PATH_MAX. glibc's fortified realpath
# aborts on any such buffer ("*** buffer overflow detected ***"), so on Linux
# every cache-enabled render died on its first texture lookup.
#
# The case that overflows on EVERY platform is a source whose RESOLVED path is
# longer than the buffer. A literal source name that long never reached the
# cache in a test render (orender reported "Failed open texture"; the cause was
# not traced, see GitHub #147). So the source is named by a short symlink in the
# work directory that resolves to a file nested more than 512 characters deep.
#
# Passes when the render is not killed by a signal and writes a cache entry.
#
# Usage: test_texture_cache_long_resolved_path.sh <orender> <scene-template> <source-png>
set -euo pipefail

ORENDER="$1"
TEMPLATE="$2"
SOURCE_PNG="$3"

WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

CACHE_DIR="$WORKDIR/cache"
mkdir -p "$CACHE_DIR"

# Three 200-character directory levels: 600+ characters, under every
# supported PATH_MAX (macOS 1024, Linux 4096) and under NAME_MAX per level.
level="$(printf 'd%.0s' $(seq 1 200))"
DEEP_DIR="$WORKDIR/$level/$level/$level"
if ! mkdir -p "$DEEP_DIR" 2>/dev/null; then
    echo "SKIP: this filesystem cannot hold a path of ${#DEEP_DIR} characters"
    exit 77
fi
cp "$SOURCE_PNG" "$DEEP_DIR/source.png"

SOURCE_LINK="$WORKDIR/source.png"
ln -s "$DEEP_DIR/source.png" "$SOURCE_LINK"

resolved="$(cd -P "$DEEP_DIR" && pwd -P)/source.png"
if [ "${#resolved}" -le 512 ] || [ "${#SOURCE_LINK}" -ge 400 ]; then
    echo "FAIL: test setup: resolved path is ${#resolved} characters (need > 512), link name ${#SOURCE_LINK} (need < 400)"
    exit 1
fi
echo "Source name: ${#SOURCE_LINK} characters; resolved path: ${#resolved} characters"

sed -e "s|@CACHE_OPTION@|Option \"texturecache\" \"enable\" [1] \"directory\" [\"${CACHE_DIR}\"]|" \
    -e "s|@TEXTURE_PATH@|${SOURCE_LINK}|" \
    "$TEMPLATE" > "$WORKDIR/scene.rib"

rc=0
(cd "$WORKDIR" && "$ORENDER" -t:1 scene.rib >"$WORKDIR/orender.log" 2>&1) || rc=$?

if [ "$rc" -ge 128 ]; then
    echo "FAIL: orender was killed by signal $((rc - 128)) on a source that resolves to ${#resolved} characters"
    cat "$WORKDIR/orender.log"
    exit 1
fi

if [ ! -f "$WORKDIR/cache-scene.tif" ]; then
    echo "FAIL: render did not produce an output file (exit $rc)"
    cat "$WORKDIR/orender.log"
    exit 1
fi

if [ -z "$(find "$CACHE_DIR" -name '*.tex' -print -quit)" ]; then
    echo "FAIL: no cache entry written for a source that resolves to ${#resolved} characters (exit $rc)"
    cat "$WORKDIR/orender.log"
    exit 1
fi

echo "test_texture_cache_long_resolved_path: ALL PASS"
