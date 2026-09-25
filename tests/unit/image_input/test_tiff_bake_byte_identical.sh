#!/usr/bin/env bash
# tests/unit/image_input/test_tiff_bake_byte_identical.sh
#
# Byte-identical regression test for spec 018 (T006, FR-007/SC-002): bakes
# a TIFF fixture through every otexmake bake mode reachable from the CLI
# (plain texture, shadow, cylindrical environment via -envlatl, cubic
# environment via -envcube) and cmp's each output against a checked-in
# reference under fixtures/references/, byte-for-byte.
#
# The references were generated once, with the migrated (spec 018) code,
# from fixtures/medium_rgb.tif (64x64 -- 2x2 tiles at DEFAULT_TILE_SIZE=32,
# a real mip pyramid, kept small so the checked-in .tex files stay light).
# Separately (not part of this permanent test, since there is no
# "before" binary once this change is merged), the migration was verified
# byte-identical against a genuine pre-migration otexmake build using the
# large_rgb.tif (512x512) fixture across all 4 modes -- see this feature's
# implementation notes.
#
# NOTE: makeSphericalEnvironment (texmake.cpp) is migrated by spec 018 too
# (all 5 readLayer() call sites), but is not reachable from any RI API
# binding or otexmake CLI flag today (grepped repo-wide, zero callers) --
# a pre-existing gap unrelated to this feature, so it cannot be exercised
# by an otexmake-binary-level regression test here.
#
# Usage: test_tiff_bake_byte_identical.sh <otexmake> <fixture.tif> <references-dir>
set -euo pipefail

OTEXMAKE="$1"
FIXTURE="$2"
REFDIR="$3"

WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

fail=0

bake_and_compare() {
    local label="$1"
    shift
    local out="$WORKDIR/${label}.tex"
    local ref="$REFDIR/${label}.tex"

    "$OTEXMAKE" "$@" "$out" >/dev/null 2>&1 || true

    if [ ! -f "$out" ]; then
        echo "FAIL ($label): bake did not produce an output file"
        fail=1
        return
    fi
    if [ ! -f "$ref" ]; then
        echo "FAIL ($label): no reference file at $ref"
        fail=1
        return
    fi

    if cmp -s "$out" "$ref"; then
        echo "PASS ($label): byte-identical to reference"
    else
        echo "FAIL ($label): output differs from reference"
        cmp "$out" "$ref" || true
        fail=1
    fi
}

bake_and_compare texture "$FIXTURE"
bake_and_compare shadow -shadow "$FIXTURE"
bake_and_compare envlatl-cylindrical -envlatl "$FIXTURE"
bake_and_compare envcube-cubic -envcube "$FIXTURE" "$FIXTURE" "$FIXTURE" "$FIXTURE" "$FIXTURE" "$FIXTURE"

if [ "$fail" -ne 0 ]; then
    echo "test_tiff_bake_byte_identical: FAILED"
    exit 1
fi

echo "test_tiff_bake_byte_identical: ALL PASS"
