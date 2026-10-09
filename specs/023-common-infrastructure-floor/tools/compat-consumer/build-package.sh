#!/usr/bin/env bash
# Spec 023 (T003): out-of-tree CMake package consumer. Configures a two-target
# project with find_package(openRender COMPONENTS Shared Static) against an
# installed tree, then renders with both targets:
#   pkg-shared -> openRender::RIBRender
#   pkg-static -> openRender::RIBRender_static
#
# Usage: build-package.sh <install-root> <out-dir>
# The Static component's TIFF/PNG/OpenEXR come from DEPS_PREFIX (default: the
# vcpkg tree this checkout builds against, else Homebrew's prefix).
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd -P)"
source "$here/common.sh"

root="$(cd "$1" && pwd -P)"
out="$2"
prefix="$(prefix_of "$root")"
mkdir -p "$out/src"
out="$(cd "$out" && pwd -P)"

deps="${DEPS_PREFIX:-}"
toolchain=()
if [ -z "$deps" ] && [ -n "${VCPKG_ROOT:-}" ] && [ -n "${VCPKG_INSTALLED_DIR:-}" ]; then
    # vcpkg's static libtiff/libpng carry transitive deps (lzma, zstd, ...)
    # that only vcpkg's find_package wrappers add, so use its toolchain in
    # classic (non-manifest) mode against the already-installed tree.
    for t in "$VCPKG_INSTALLED_DIR"/*-openrender; do [ -d "$t" ] && triplet="$(basename "$t")"; done
    toolchain=(-DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
               -DVCPKG_INSTALLED_DIR="$VCPKG_INSTALLED_DIR" -DVCPKG_TARGET_TRIPLET="$triplet"
               -DVCPKG_MANIFEST_MODE=OFF)
fi
if [ -z "$deps" ] && [ ${#toolchain[@]} -eq 0 ] && command -v brew >/dev/null; then deps="$(brew --prefix)"; fi

cp "$here/consumer.cpp" "$out/src/"
cat > "$out/src/CMakeLists.txt" <<'CML'
cmake_minimum_required(VERSION 3.19)
project(openrender_pkg_consumer CXX)
find_package(openRender CONFIG REQUIRED COMPONENTS Shared Static)
add_executable(pkg-shared consumer.cpp)
target_link_libraries(pkg-shared PRIVATE openRender::RIBRender)
add_executable(pkg-static consumer.cpp)
target_link_libraries(pkg-static PRIVATE openRender::RIBRender_static)
foreach(t pkg-shared pkg-static)
    target_compile_features(${t} PRIVATE cxx_std_20)
    target_include_directories(${t} PRIVATE "${OPENRENDER_INCLUDE}")
endforeach()
CML

cmake -S "$out/src" -B "$out/build" -DCMAKE_BUILD_TYPE=Release "${toolchain[@]}" \
    -DCMAKE_PREFIX_PATH="$prefix${deps:+;$deps}" -DOPENRENDER_INCLUDE="$prefix/include" > "$out/configure.log"
nice -n 19 cmake --build "$out/build" -j4 > "$out/build.log"

runtime_env "$prefix"
cd "$out"
status=0
for t in pkg-shared pkg-static; do
    rm -f "$t.tif"
    if "./build/$t" render "$t.tif" > "log-$t.txt" 2>&1 && [ -s "$t.tif" ]; then
        echo "$t: rendered $t.tif ($(wc -c < "$t.tif") bytes)"
    else
        echo "$t: FAILED (see $out/log-$t.txt)"; sed 's/^/    | /' "log-$t.txt"; status=1
    fi
done
exit $status
