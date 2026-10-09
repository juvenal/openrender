#!/usr/bin/env bash
# Spec 023 (T003): build one compat consumer per RI library against an
# installed openRender tree. The binaries carry an rpath into that tree, so
# installing new libraries over it ("in their place", spec US4) and re-running
# them with no loader variables tests real loader resolution.
#
# Usage: build.sh <install-root> <out-dir>
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
source "$here/common.sh"

root="$(cd "$1" && pwd -P)"
out="$2"
prefix="$(prefix_of "$root")"
mkdir -p "$out"
out="$(cd "$out" && pwd -P)"
CXX="${CXX:-c++}"

for lib in RIBRender RIBVector RIBOut; do
    "$CXX" -std=c++20 -O1 -I"$prefix/include" "$here/consumer.cpp" \
        "$(shlib "$prefix" "$lib")" -Wl,-rpath,"$prefix/lib" \
        -o "$out/consumer-$lib"
    echo "built $out/consumer-$lib against $(shlib "$prefix" "$lib")"
done
cp "$here/common.sh" "$here/run.sh" "$here/compare.sh" "$out/"
chmod +x "$out/run.sh" "$out/compare.sh"
