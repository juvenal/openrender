#!/bin/bash
# Spec 039, Phase 7 (T079): self-tests of the determinism harness, test_depth_determinism's new
# cross-configuration mode. Written before the flags exist (tests first), so each case fails
# until T079.
#
# Usage: check_determinism_harness.sh <driver> <orender> <scenes dir> <case>
#
# Cases (the exit code of the driver, and what it prints, are the contract):
#   identical         a scene that is thread independent under sobol: exit 0
#   detects           two configurations that differ by a deterministic seed (--config-prepends,
#                     random shading, single-threaded): exit 1, and at most 5 differing pixels
#                     are listed per output, at least one. Until Phase 9 the difference came
#                     from the Legacy sampler's thread-dependent draws, which no longer exist.
#   render_failure    a scene that does not exist: exit 2
#   multiple_outputs  two displays, both compared: exit 0, and each output is named in the report
#   print_prepend     --print-scene prints the prepared copy and exits 0 without rendering: the
#                     --prepend line is first and no `numthreads` line is left
#   print_keep_pins   with --keep-pins the `numthreads` line is kept
#   print_no_dither   --no-dither puts Quantize "rgba" 255 0 255 0 in the copy, after the
#                     --prepend line
#   usage_unchanged   the original usage (driver orender rib tif runs) still works
driver=$1; orender=$2; scenes=$3; which=$4
sobol='Option "sampling" "sampler" ["sobol"]'
fail() { echo "FAIL: $*" >&2; exit 1; }
expect_rc() { [ "$1" -eq "$2" ] || fail "$3: exit code $1, expected $2"; }

case $which in
identical)
    "$driver" --configs "-t:1||-t:4" --outputs sphere-random-reyes.tif --prepend "$sobol" --no-dither "$orender" "$scenes/sphere-random-reyes.rib"
    expect_rc $? 0 "a thread independent scene"
    ;;
detects)
    out=$("$driver" --configs "-t:1|-t:1" --outputs determinism-harness-noisy.tif \
        --config-prepends 'Option "sampling" "seed" [0]|Option "sampling" "seed" [1]' \
        "$orender" "$scenes/determinism-harness-noisy.rib" 2>&1)
    expect_rc $? 1 "two configurations with different seeds"
    echo "$out"
    lines=$(echo "$out" | grep -c 'pixel (')
    [ "$lines" -ge 1 ] || fail "no differing pixel was listed"
    [ "$lines" -le 5 ] || fail "$lines differing pixels were listed for one output, at most 5 expected"
    ;;
render_failure)
    "$driver" --configs "-t:1|-t:2" --outputs nothing.tif "$orender" "$scenes/does-not-exist.rib"
    expect_rc $? 2 "a scene that cannot be rendered"
    ;;
multiple_outputs)
    out=$("$driver" --configs "-t:1||-t:4" --outputs determinism-harness-two-outputs.tif,determinism-harness-two-outputs-z.tif --prepend "$sobol" "$orender" "$scenes/determinism-harness-two-outputs.rib" 2>&1)
    expect_rc $? 0 "two outputs"
    echo "$out"
    echo "$out" | grep -q 'determinism-harness-two-outputs.tif' || fail "the colour output is not in the report"
    echo "$out" | grep -q 'determinism-harness-two-outputs-z.tif' || fail "the depth output is not in the report"
    ;;
print_prepend)
    out=$("$driver" --print-scene --prepend "$sobol" "$orender" "$scenes/sphere-random-reyes.rib")
    expect_rc $? 0 "--print-scene"
    [ "$(echo "$out" | head -1)" = "$sobol" ] || fail "the first line of the prepared scene is not the --prepend line"
    echo "$out" | grep -q numthreads && fail "a numthreads line is left in the prepared scene"
    echo "$out" | grep -q 'Surface "random_probe"' || fail "the rest of the scene is missing"
    ;;
print_keep_pins)
    out=$("$driver" --print-scene --keep-pins --prepend "$sobol" "$orender" "$scenes/sphere-random-reyes.rib")
    expect_rc $? 0 "--print-scene --keep-pins"
    echo "$out" | grep -q numthreads || fail "--keep-pins did not keep the numthreads line"
    ;;
print_no_dither)
    out=$("$driver" --print-scene --no-dither --prepend "$sobol" "$orender" "$scenes/sphere-random-reyes.rib")
    expect_rc $? 0 "--print-scene --no-dither"
    echo "$out" | grep -q 'Quantize "rgba" 255 0 255 0' || fail "--no-dither did not add the Quantize line"
    [ "$(echo "$out" | head -1)" = "$sobol" ] || fail "the --prepend line is not first"
    ;;
usage_unchanged)
    "$driver" "$orender" "$scenes/color-determinism-opaque-reyes.rib" color-determinism-opaque.tif 2
    expect_rc $? 0 "the original usage"
    ;;
*)
    fail "unknown case $which"
    ;;
esac
