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
#   FilesDiffer       (spec 040) --files: a stand-in for orender (a script that writes determinism-harness-files.bin from its
#                     first argument and then runs the real orender) gives the two configurations "-t:1" and "-t:2" different
#                     bytes in the file: exit 1, and the report names the file and the first differing offset
#   FilesSame         (spec 040) the same stand-in with the configurations "-t:1" and "-t:1": identical bytes, exit 0, and the
#                     report names the file as byte-identical
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
FilesDiffer|FilesSame)
    stub="$PWD/determinism-harness-orender.sh"
    printf '#!/bin/bash\nprintf "%%s" "$1" > determinism-harness-files.bin\nexec "%s" "$@"\n' "$orender" > "$stub"
    chmod +x "$stub"
    if [ "$which" = FilesDiffer ]; then configs="-t:1|-t:2"; else configs="-t:1|-t:1"; fi
    out=$("$driver" --configs "$configs" --outputs sphere-random-reyes.tif --files determinism-harness-files.bin --prepend "$sobol" --no-dither "$stub" "$scenes/sphere-random-reyes.rib" 2>&1)
    rc=$?
    echo "$out"
    rm -f "$stub" determinism-harness-files.bin
    if [ "$which" = FilesDiffer ]; then
        expect_rc $rc 1 "two configurations with different file bytes"
        echo "$out" | grep -q 'determinism-harness-files.bin' || fail "the differing file is not named"
        echo "$out" | grep -q 'first differing byte at offset' || fail "the first differing offset is not reported"
    else
        expect_rc $rc 0 "two configurations with identical file bytes"
        echo "$out" | grep -q 'determinism-harness-files.bin.*byte-identical' || fail "the file is not reported as byte-identical"
    fi
    ;;
*)
    fail "unknown case $which"
    ;;
esac
