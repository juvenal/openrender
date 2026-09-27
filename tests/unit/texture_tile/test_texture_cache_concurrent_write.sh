#!/usr/bin/env bash
# tests/unit/texture_tile/test_texture_cache_concurrent_write.sh
#
# Multi-PROCESS concurrent cache-write test for spec 020
# (020-runtime-tile-paging, T021, FR-016/SC-006, research.md SS10) --
# genuinely new test infrastructure for this project: spec 019's own
# concurrency test (test_tile_source_concurrency.sh) used threads within
# ONE process; this needs multiple separate OS processes racing to
# populate the SAME disk-cache entry, since the atomic write-then-rename
# guarantee (research.md SS6) is specifically about protecting against
# concurrent WRITERS -- a hazard threads-within-one-process cannot
# reproduce (this process is the only writer among its own threads; the
# disk cache is written once per (source, mtime) key regardless of how
# many threads first-referenced it).
#
# Launches N orender processes (each pinned -t:1, to isolate this test to
# cache-write concurrency specifically -- not conflating it with
# intra-process shading-thread concurrency, already covered separately by
# User Story 3/T024-T027) concurrently, all rendering the same scene
# against the same (initially empty) cache directory. After all complete:
#   (a) every process's own rendered output is correct (byte-identical to
#       every other process's -- they all decode the same source).
#   (b) the single resulting cache file opens successfully as a valid
#       baked texture (TIFFOpen()-based check) -- a corrupt/partial write
#       from a torn concurrent write would fail this directly.
#
# Usage: test_texture_cache_concurrent_write.sh <orender> <scene-template> <source-png> [num-processes]
set -euo pipefail

ORENDER="$1"
TEMPLATE="$2"
SOURCE_PNG="$3"
NUM_PROCESSES="${4:-8}"

WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

CACHE_DIR="$WORKDIR/cache"
mkdir -p "$CACHE_DIR"

sed -e "s|@CACHE_OPTION@|Option \"texturecache\" \"enable\" [1] \"directory\" [\"${CACHE_DIR}\"]|" \
    -e "s|@TEXTURE_PATH@|${SOURCE_PNG}|" \
    "$TEMPLATE" > "$WORKDIR/scene.rib"

pids=()
for i in $(seq 1 "$NUM_PROCESSES"); do
    procdir="$WORKDIR/proc$i"
    mkdir -p "$procdir"
    cp "$WORKDIR/scene.rib" "$procdir/scene.rib"
    (cd "$procdir" && "$ORENDER" -t:1 scene.rib >"$procdir/log.txt" 2>&1) &
    pids+=($!)
done

fail=0
for pid in "${pids[@]}"; do
    if ! wait "$pid"; then
        fail=1
    fi
done

if [ "$fail" -ne 0 ]; then
    echo "FAIL: at least one of the $NUM_PROCESSES concurrent orender processes exited non-zero"
    for i in $(seq 1 "$NUM_PROCESSES"); do
        echo "--- proc$i log ---"
        cat "$WORKDIR/proc$i/log.txt" 2>/dev/null || true
    done
    exit 1
fi

echo "=== Checking every process's own output is correct ==="
reference=""
for i in $(seq 1 "$NUM_PROCESSES"); do
    out="$WORKDIR/proc$i/cache-scene.tif"
    if [ ! -f "$out" ]; then
        echo "FAIL: proc$i did not produce an output file"
        exit 1
    fi
    if [ -z "$reference" ]; then
        reference="$out"
    elif ! cmp -s "$reference" "$out"; then
        echo "FAIL: proc$i's output differs from proc1's -- concurrent writers produced inconsistent renders"
        cmp "$reference" "$out" || true
        exit 1
    fi
done
echo "PASS: all $NUM_PROCESSES processes produced byte-identical, correct output"

echo "=== Checking the resulting cache file is valid (not corrupt/partial) ==="
cache_entries=("$CACHE_DIR"/*.tex)
if [ ! -e "${cache_entries[0]}" ]; then
    echo "FAIL: no cache entry found under $CACHE_DIR after $NUM_PROCESSES concurrent writers"
    exit 1
fi
if [ "${#cache_entries[@]}" -ne 1 ]; then
    echo "FAIL: expected exactly one cache entry (same source, same mtime -> same key) under $CACHE_DIR, found ${#cache_entries[@]}: ${cache_entries[*]}"
    exit 1
fi

CACHE_FILE="${cache_entries[0]}"

# Validity check via orender itself (no new external tool dependency --
# e.g. tiffinfo isn't guaranteed present on every build/CI machine this
# project targets): render a scene referencing CACHE_FILE directly as
# "texturename". A corrupt/partial TIFF fails TIFFOpen() and falls
# straight to CDummyTexture substitution + a nonzero exit code (the same
# CODE_NOFILE path proven in T010c) -- a genuinely valid file renders
# correctly with exit 0, through the completely ordinary, pre-existing
# TIFFOpen()-succeeds fast path (this bypasses the cache/synthesis logic
# entirely, so it is a check of the FILE, not of this spec's own code).
sed -e "s|@CACHE_OPTION@|Option \"texturecache\" \"enable\" [0]|" \
    -e "s|@TEXTURE_PATH@|${CACHE_FILE}|" \
    "$TEMPLATE" > "$WORKDIR/verify.rib"
verify_rc=0
(cd "$WORKDIR" && "$ORENDER" -t:1 verify.rib >"$WORKDIR/verify.log" 2>&1) || verify_rc=$?
if [ "$verify_rc" -ne 0 ] || [ ! -f "$WORKDIR/cache-scene.tif" ]; then
    echo "FAIL: $CACHE_FILE does not open as a valid texture (exit $verify_rc) -- possible torn/partial concurrent write"
    cat "$WORKDIR/verify.log" 2>/dev/null || true
    exit 1
fi
echo "PASS: $CACHE_FILE opens successfully as a valid baked texture"

echo "test_texture_cache_concurrent_write: ALL PASS ($NUM_PROCESSES concurrent writers)"
