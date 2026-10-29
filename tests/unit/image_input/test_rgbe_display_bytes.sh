#!/usr/bin/env bash
# tests/unit/image_input/test_rgbe_display_bytes.sh
#
# Display_RgbeBytes (spec 027, T-D1, T028, FR-017, SC-009, User Story 4 AS2): renders
# rgbe-display.rib through the "rgbe" display driver and compares the file it writes, byte for
# byte, with fixtures/references/rgbe-display.hdr.
#
# CHARACTERIZATION: this test is GREEN ON THE STARTING TREE BY DESIGN. The reference holds the
# bytes the starting tree's driver wrote (T029, rendered twice and identical). It exists so that
# Phase 3, which makes the display driver use the one RGBE codec of src/common, can prove it changed
# no byte of what the driver writes. Nothing here may be regenerated to get a green result.
#
# Usage: test_rgbe_display_bytes.sh <orender> <rgbe-display.rib> <reference.hdr>
# Environment: SHADERS, ORENDERHOME, DISPLAYS (the directory of the rgbe driver)
set -euo pipefail

ORENDER="$1"
RIB="$2"
REFERENCE="$3"

WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

[ -f "$REFERENCE" ] || { echo "FAIL: no reference file at $REFERENCE"; exit 1; }

( cd "$WORKDIR" && "$ORENDER" -t:1 "$RIB" > orender.log 2>&1 ) || {
    echo "FAIL: orender exited non-zero"
    cat "$WORKDIR/orender.log"
    exit 1
}

OUT="$WORKDIR/rgbe-display.hdr"
[ -f "$OUT" ] || { echo "FAIL: the rgbe driver wrote no file"; cat "$WORKDIR/orender.log"; exit 1; }

if cmp -s "$OUT" "$REFERENCE"; then
    echo "PASS: rgbe-display.hdr is byte-identical to the reference ($(wc -c < "$OUT" | tr -d ' ') bytes)"
else
    echo "FAIL: rgbe-display.hdr differs from the reference"
    cmp "$OUT" "$REFERENCE" || true
    exit 1
fi
