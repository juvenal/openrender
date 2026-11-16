#!/usr/bin/env bash
# tests/shaders/test_sloinfo_output137.sh
#
# Sloinfo_Output137 (spec 033, Phase 1; FR-026, research R17): the output of sloinfo, as it is on the unchanged tree, for
# every shipped shader in both formats. Phase 8 moves sloinfo onto the metadata API and the listing must not change.
#
# Usage:
#   test_sloinfo_output137.sh <sloinfo> <shaders build dir> <reference dir> <platform> <jit: ON|OFF>
#
# For each <name>.rslo of the build tree's shaders/ (137 of them) the script runs `sloinfo --rslo`, and with the JIT
# `sloinfo --slo` on the matching .slo, and records "### <format> <name>", the exit code, the standard output and the
# standard error (the directory name replaced by <shaders>). The record of each format is compared with
# <reference dir>/<format>.txt; a platform whose output differs bit for bit from the other's keeps its own copy in
# <reference dir>/<platform>/<format>.txt, which is preferred when it exists. A missing reference is written on the
# first run and never overwritten: this script has no mode that replaces an existing reference.

set -u
export LC_ALL=C

SLOINFO="${1:?usage: $0 <sloinfo> <shaders build dir> <reference dir> <platform> <jit>}"
SHADERS="${2:?}"
REFDIR="${3:?}"
PLATFORM="${4:?}"
JIT="${5:?}"

[ -x "$SLOINFO" ] || { echo "FAIL: no sloinfo at $SLOINFO"; exit 1; }
[ -d "$SHADERS" ] || { echo "FAIL: no shaders directory $SHADERS"; exit 1; }

failed=0
formats="rslo"
[ "$JIT" = "ON" ] && formats="rslo slo"

for fmt in $formats; do
    out="$(mktemp)"
    count=0
    for f in $(ls "$SHADERS"/*.rslo | sort); do
        name="$(basename "$f" .rslo)"
        target="$SHADERS/$name.$fmt"
        [ -f "$target" ] || { echo "FAIL: $target does not exist"; failed=1; continue; }
        o="$(mktemp)"; e="$(mktemp)"
        "$SLOINFO" "--$fmt" "$target" > "$o" 2> "$e"
        rc=$?
        {
            echo "### $fmt $name"
            echo "exit $rc"
            echo "stdout:"
            sed "s#$SHADERS#<shaders>#g" "$o"
            echo "stderr:"
            sed "s#$SHADERS#<shaders>#g" "$e"
        } >> "$out"
        rm -f "$o" "$e"
        count=$((count + 1))
    done
    echo "$fmt: $count shaders listed"
    if [ "$count" -ne 137 ]; then
        echo "FAIL: expected 137 shaders, found $count"
        failed=1
    fi

    ref="$REFDIR/$fmt.txt"
    [ -f "$REFDIR/$PLATFORM/$fmt.txt" ] && ref="$REFDIR/$PLATFORM/$fmt.txt"
    if [ ! -f "$ref" ]; then
        mkdir -p "$(dirname "$ref")"
        cp "$out" "$ref"
        echo "recorded $ref"
    elif cmp -s "$out" "$ref"; then
        echo "identical to $ref"
    else
        echo "FAIL: the $fmt output differs from $ref:"
        diff "$ref" "$out" | head -40
        cp "$out" "sloinfo137-$fmt.fresh"
        failed=1
    fi
    rm -f "$out"
done

exit $failed
