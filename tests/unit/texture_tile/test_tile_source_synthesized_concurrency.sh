#!/usr/bin/env bash
# tests/unit/texture_tile/test_tile_source_synthesized_concurrency.sh
#
# Multi-threaded tile-fetch concurrency test for spec 020
# (020-runtime-tile-paging, T024-T027, FR-011/SC-005): targets
# CSynthesizedTileSource specifically -- new code this spec introduces,
# distinct from spec 019's existing per-CTextureBlock lock model and its
# own TileSource_Concurrency test (test_tile_source_concurrency.sh), which
# exercises CTiffTileSource against a baked .tex and therefore never
# reaches this spec's own concurrent fetchTile() handling at all.
#
# Design mirrors spec 019's own precedent exactly (that spec's research.md
# SS6, confirmed still true here per research.md SS11): CShadingContext is
# an abstract class with no standalone-unit-test-construction precedent
# anywhere in this codebase, so this drives the concurrency test through a
# real, production multi-threaded render instead of hand-constructing one.
# synthesized-concurrency-scene.rib deliberately references large_rgb8.png
# DIRECTLY (no otexmake bake, no disk cache -- the scene has no
# "Option \"texturecache\"" statement at all, so the cache stays at its
# default-disabled state), forcing CRenderer::textureLoad()'s fallback
# through CSynthesizedTileSource every time.
#
# A raw byte-for-byte `cmp` between a multi-threaded render and the -t:1
# reference is NOT valid here (same reasoning as spec 019's own test):
# this codebase's default multi-threaded reyes rendering is not
# byte-reproducible run to run (bucket-scheduling nondeterminism), so raw
# bytes -- and even antialiasing-jitter-driven pixel values at gradient
# edges -- will legitimately differ from run to run even with perfectly
# correct, race-free code. The block-average metric absorbs that expected
# jitter; a genuine tile-fetch race (a torn/wrong/stale tile, or a crash
# from concurrent use of the unsynchronized CRenderer::globalMemory arena
# during synthesis, research.md SS4a) produces a gross, localized
# deviation far above it, or an outright nonzero exit / crash.
#
# The threshold below (16) was measured empirically, not reused verbatim
# from spec 019's own value (4.55-4.62) since the scene/texture content
# differs, though it happens to land in a similar range for the same
# underlying reason (same closed-form gradient formula, same AA-jitter
# noise source): 5 multi-threaded renders of THIS scene, measured against
# the -t:1 reference, all measured MaxBlockAvgDiff in the 4.58-5.28 range.
# 16 leaves ~3x margin above that measured noise band while staying tight
# enough that a real wrong-tile/torn-tile/crash bug would be caught.
#
# Usage: test_tile_source_synthesized_concurrency.sh <orender> <test_visual_render> <rib> <reference_tif> [iterations]
set -euo pipefail

ORENDER="$1"
TEST_VISUAL_RENDER="$2"
RIB="$3"
REFERENCE="$4"
ITERATIONS="${5:-5}"
THRESHOLD=16

WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

fail=0

for i in $(seq 1 "$ITERATIONS"); do
    echo "=== TileSource_SynthesizedConcurrency: iteration $i/$ITERATIONS (multi-threaded, default thread count) ==="
    (cd "$WORKDIR" && "$TEST_VISUAL_RENDER" "$ORENDER" "$RIB" "synthesized-concurrency-scene.tif" "$REFERENCE" "$THRESHOLD") || fail=1
    rm -f "$WORKDIR/synthesized-concurrency-scene.tif"
done

if [ "$fail" -ne 0 ]; then
    echo "test_tile_source_synthesized_concurrency: FAILED"
    exit 1
fi

echo "test_tile_source_synthesized_concurrency: ALL PASS ($ITERATIONS iterations)"
