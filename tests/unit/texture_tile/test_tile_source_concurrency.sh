#!/usr/bin/env bash
# tests/unit/texture_tile/test_tile_source_concurrency.sh
#
# Multi-threaded tile-fetch concurrency test for spec 019
# (019-tilesource-extraction, T012/T013/T023, FR-006/SC-002).
#
# No existing test (unit or visual-regression) exercised concurrent
# tile-fetch before this spec (confirmed by repo-wide grep during task
# planning) -- this is new coverage this spec adds, not a pre-existing
# regression bar.
#
# Design (see concurrency-scene.rib's header comment for the full
# rationale): CShadingContext is an abstract class with no precedent for
# standalone-unit-test construction anywhere in this codebase (confirmed
# by reading src/libshader/shading/shading.h and searching every existing
# tests/unit/*.cpp for prior art -- found none), so this drives the
# concurrency test through the real, production multi-threaded render
# path instead of hand-constructing one: concurrency-scene.rib is
# rendered once, single-threaded (-t:1), producing the checked-in
# reference this test compares against; then rendered several more times
# with orender's DEFAULT (multi-threaded) thread count, each compared
# against that reference via test_visual_render's pixel-value block-average
# metric.
#
# A raw byte-for-byte `cmp` between a multi-threaded render and the -t:1
# reference is NOT valid here (unlike test_render_byte_identical.sh):
# this codebase's default multi-threaded reyes rendering is not
# byte-reproducible run to run (bucket-scheduling nondeterminism,
# confirmed empirically during this spec's planning), so raw bytes -- and
# even antialiasing-jitter-driven PIXEL values at gradient edges -- will
# legitimately differ from run to run even with perfectly correct,
# race-free code. The block-average metric absorbs that expected jitter;
# a genuine tile-fetch race (a torn/wrong/stale tile block) produces a
# gross, localized deviation far above it.
#
# The threshold below (15) was chosen empirically, not guessed: 5
# multi-threaded renders of this exact scene, measured against the -t:1
# reference before any CTileSource code existed, all measured
# MaxBlockAvgDiff in the 4.55-4.62 range (~26% of blocks exceed a
# threshold of 1, since the periodic-tiled linear-ramp texture has
# gradients almost everywhere -- expected AA-jitter noise, not a bug).
# 15 leaves ~3x margin above that measured noise band while staying tight
# enough that a real wrong-tile/torn-tile bug (which reads garbage or a
# stale/wrong region, not a small jitter) would be caught.
#
# Usage: test_tile_source_concurrency.sh <orender> <test_visual_render> <rib> <reference_tif> [iterations]
set -euo pipefail

ORENDER="$1"
TEST_VISUAL_RENDER="$2"
RIB="$3"
REFERENCE="$4"
ITERATIONS="${5:-5}"
THRESHOLD=15

WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

fail=0

for i in $(seq 1 "$ITERATIONS"); do
    echo "=== TileSource_Concurrency: iteration $i/$ITERATIONS (multi-threaded, default thread count) ==="
    (cd "$WORKDIR" && "$TEST_VISUAL_RENDER" "$ORENDER" "$RIB" "concurrency-scene.tif" "$REFERENCE" "$THRESHOLD") || fail=1
    rm -f "$WORKDIR/concurrency-scene.tif"
done

if [ "$fail" -ne 0 ]; then
    echo "test_tile_source_concurrency: FAILED"
    exit 1
fi

echo "test_tile_source_concurrency: ALL PASS ($ITERATIONS iterations)"
