#!/usr/bin/env bash
# tests/unit/texture_tile/test_render_byte_identical.sh
#
# Byte-identical rendered-output regression test for spec 019
# (019-tilesource-extraction, T009/T010, FR-002/SC-001): renders every
# existing visual-regression scene that reads a baked texture, environment
# map, or shadow map -- SC-001's full "100% of existing scenes" scope, not
# a representative subset -- and cmp's each output against a checked-in
# reference under fixtures/references/, byte-for-byte. Stricter than the
# project's usual 8x8 block-average visual-diff metric, which is too
# coarse to catch a subtle tile-indexing or mip-level-selection
# regression in the CTileSource extraction this spec performs.
#
# Every render uses "-t:1" (single-threaded): this codebase's default
# multi-threaded reyes rendering is NOT byte-identical across repeated
# runs of the same unmodified binary on the same scene (bucket-scheduling
# nondeterminism, confirmed empirically during this spec's task planning).
# "-t:1" is this project's own established mechanism for exactly this
# situation (see tests/visual/CMakeLists.txt's existing "-t:1" extra-arg
# usage for other byte-identical-sensitive scenes).
#
# The references were captured once, before any CTileSource code existed,
# with the pre-refactor orender binary -- a permanent CI-runnable
# regression test, not a one-time manual check.
#
# Usage: test_render_byte_identical.sh <orender> <rib-parity-dir> <references-dir> <have-openexr:0|1>
set -euo pipefail

ORENDER="$1"
RIBDIR="$2"
REFDIR="$3"
HAVE_OPENEXR="$4"

WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

fail=0

render_and_compare() {
    local scene="$1"
    local ref="$REFDIR/${scene}.tif"

    if [ ! -f "$ref" ]; then
        echo "FAIL ($scene): no reference file at $ref"
        fail=1
        return
    fi

    (cd "$WORKDIR" && "$ORENDER" -t:1 "$RIBDIR/${scene}.rib" >/dev/null 2>&1) || true

    local out="$WORKDIR/${scene}.tif"
    if [ ! -f "$out" ]; then
        echo "FAIL ($scene): render did not produce an output file"
        fail=1
        return
    fi

    if cmp -s "$out" "$ref"; then
        echo "PASS ($scene): byte-identical to reference"
    else
        echo "FAIL ($scene): output differs from reference"
        cmp "$out" "$ref" || true
        fail=1
    fi
    rm -f "$out"
}

render_and_compare texture-png-reyes
render_and_compare texture-png-raytrace

if [ "$HAVE_OPENEXR" = "1" ]; then
    render_and_compare texture-exr-reyes
    render_and_compare texture-exr-raytrace
else
    echo "SKIP (texture-exr-reyes, texture-exr-raytrace): HAVE_OPENEXR is off"
fi

render_and_compare texture-rgbe-reyes
render_and_compare texture-rgbe-raytrace
render_and_compare shadow-reyes
render_and_compare shadow-raytrace
render_and_compare env-reyes
render_and_compare env-raytrace

if [ "$fail" -ne 0 ]; then
    echo "test_render_byte_identical: FAILED"
    exit 1
fi

echo "test_render_byte_identical: ALL PASS"
