#!/usr/bin/env bash
# Spec 023 (T055, research R12, SC-007): interleaved A/B wall-time comparison
# of the frozen baseline install against the current build tree.
#
# Usage: perf_ab.sh <baseline-install-root> <build-dir | install-root> <rounds>
#
# The 12 perf-manual scenes (six probes, interpreter and JIT variants) run
# serially. Every round renders each RIB once with the baseline orender, then
# once with the new one, so thermal drift on a fanless machine hits both sides
# alike. Each side is pinned to its own shaders, display drivers and
# .orenderrc: an install root to its install (as T003's render-installed.sh), a
# build directory to the build tree (as the visual suite's VISUAL_ENV). Passing
# the baseline root twice gives an A/A run, which measures the noise floor.
#
# Only the orender process itself is timed: the clock starts and stops inside
# one perl process that execs the render, after the side's environment is set.
#
# Prints the median wall time per RIB and side and the new/base ratio, and
# exits 1 if any ratio is above MAX_RATIO (default 1.02). With PERF_RAW=<file>,
# every individual timing is also kept there as "<rib> <side> <seconds>".
# PERF_ORENDER_ARGS adds options to every render on both sides (e.g. -t:1).
#
# Run it under nice on an otherwise idle machine:
#   nice -n 19 perf_ab.sh $S/023-base build 7
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd -P)"
source "$here/compat-consumer/common.sh"
repo="$(cd "$here/../../.." && pwd -P)"

if [ $# -ne 3 ]; then
    echo "usage: perf_ab.sh <baseline-install-root> <build-dir | install-root> <rounds>" >&2
    exit 2
fi
base_root="$(cd "$1" && pwd -P)"
new_root="$(cd "$2" && pwd -P)"
rounds="$3"
max_ratio="${MAX_RATIO:-1.02}"
# shellcheck disable=SC2206
extra=(${PERF_ORENDER_ARGS:-})

base_prefix="$(prefix_of "$base_root")"
base_bin="$base_prefix/bin/orender"
if [ -x "$new_root/src/orender/orender" ]; then
    new_kind=build
    new_bin="$new_root/src/orender/orender"
else
    new_kind=install
    new_prefix="$(prefix_of "$new_root")"
    new_bin="$new_prefix/bin/orender"
fi
for b in "$base_bin" "$new_bin"; do
    [ -x "$b" ] || { echo "perf_ab: not executable: $b" >&2; exit 2; }
done

scenes="cfrom ctransform matrixops comparisonlogic arrayops gather"
ribs=""
for s in $scenes; do
    for v in "" "-slo"; do
        ribs="$ribs sphere-$s-reyes$v"
    done
done

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
mkdir -p "$work/base" "$work/new"

# timed <binary> <args...>: run the render and print its wall time in seconds
# (bash 3.2 has no EPOCHREALTIME). Fails if the render fails.
timed() {
    perl -MTime::HiRes=time -e '
        open(STDOUT, ">", "/dev/null"); open(STDERR, ">", "/dev/null");
        my $t = time; my $rc = system @ARGV; my $d = time - $t;
        open(STDOUT, ">&", $ENV{PERF_FD} // 1);
        exit 1 if $rc != 0;
        printf "%.6f\n", $d;' "$@"
}

# Each side runs in a subshell, so its environment never leaks into the other.
# run_side <base|new> <rib>: render once, print the wall time.
run_side() {
    (
        unset GEOMETRIES TEXTURES RIBS PROCEDURALS
        if [ "$1" = base ]; then
            runtime_env "$base_prefix"
            bin="$base_bin"
        elif [ "$new_kind" = install ]; then
            runtime_env "$new_prefix"
            bin="$new_bin"
        else
            export SHADERS="$new_root/shaders"
            export ORENDERHOME="$new_root/tests/visual/_home"
            export DISPLAYS="$new_root/src/display/file:$new_root/src/display/rgbe:$new_root/src/display/framebuffer:$new_root/src/display/openexr"
            export GEOMETRIES="$repo/geometry"
            bin="$new_bin"
        fi
        cd "$work/$1"
        exec 3>&1
        PERF_FD=3 timed "$bin" ${extra[@]+"${extra[@]}"} "$2"
    )
}

# Before timing anything, make sure each binary resolves only its own tree's
# libraries, and that both sides actually render each scene.
check_resolution "$base_root" "$base_bin" --help >/dev/null
check_resolution "$new_root" "$new_bin" --help >/dev/null
for r in $ribs; do
    rib="$repo/examples/rib/tests/$r.rib"
    for side in base new; do
        run_side "$side" "$rib" >/dev/null || { echo "perf_ab: $side failed on $r" >&2; exit 2; }
        [ -s "$work/$side/$r.tif" ] || { echo "perf_ab: $side wrote no $r.tif" >&2; exit 2; }
    done
done

times="$work/times.txt"
: > "$times"
for ((i = 1; i <= rounds; i++)); do
    for r in $ribs; do
        rib="$repo/examples/rib/tests/$r.rib"
        echo "$r base $(run_side base "$rib")" >> "$times"
        echo "$r new $(run_side new "$rib")" >> "$times"
    done
    echo "perf_ab: round $i/$rounds done" >&2
done


median() {
    awk -v r="$1" -v s="$2" '$1 == r && $2 == s { print $3 }' "$times" | sort -g |
        awk '{ v[NR] = $1 } END { if (NR % 2) print v[(NR + 1) / 2]; else print (v[NR / 2] + v[NR / 2 + 1]) / 2 }'
}

status=0
printf '| %-32s | %9s | %9s | %6s |\n' "RIB" "base (s)" "new (s)" "ratio"
printf '|%s|%s|%s|%s|\n' "----------------------------------" "-----------" "-----------" "--------"
for r in $ribs; do
    mb=$(median "$r" base)
    mn=$(median "$r" new)
    ratio=$(awk -v a="$mn" -v b="$mb" 'BEGIN { printf "%.3f", a / b }')
    flag=""
    if awk -v x="$ratio" -v m="$max_ratio" 'BEGIN { exit !(x > m) }'; then
        flag=" over"
        status=1
    fi
    printf '| %-32s | %9.3f | %9.3f | %6s |%s\n' "$r" "$mb" "$mn" "$ratio" "$flag"
done
if [ -n "${PERF_RAW:-}" ]; then
    cp "$times" "$PERF_RAW"
fi
echo "perf_ab: $rounds interleaved rounds, pass if every ratio <= $max_ratio: $([ $status -eq 0 ] && echo PASS || echo FAIL)"
exit $status
