#!/usr/bin/env bash
# phase0_asan.sh: T015 (#164): configure and build build-asan (AddressSanitizer, RelWithDebInfo, JIT off) through the lock, then run
# photon-parity at -t:4 until ASan reports (at most 30 runs). Targets built: orender, file (display driver), openrender_shaders
# (not `all`: the test executables are not needed). The first report is kept in build-logs/040-asan-report-NN.txt.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; W="$(cd "$HERE/../../.." && pwd)"; CJ="${CJ:-$HOME/.claude/bin/cpujob}"; L="$W/build-logs"
cd "$W" || exit 9; mkdir -p "$L"
df -h /Volumes/Projects | tail -1
"$CJ" 040-asan-configure cmake -B build-asan -S "$W" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DOPENRENDER_ENABLE_JIT=OFF \
  -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer" -DCMAKE_C_FLAGS="-fsanitize=address" \
  -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address -DCMAKE_SHARED_LINKER_FLAGS=-fsanitize=address -DCMAKE_MODULE_LINKER_FLAGS=-fsanitize=address \
  -DCMAKE_TOOLCHAIN_FILE=/Users/juvenal/.vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_INSTALLED_DIR=/Users/juvenal/.cache/vcpkg-installed/openrender -DVCPKG_TARGET_TRIPLET=arm64-osx-openrender \
  -DVCPKG_OVERLAY_TRIPLETS="$W/CMake/vcpkg-triplets" > "$L/040-asan-configure.log" 2>&1
echo "configure exit $?"
for i in 1 2 3 4; do
  df -h /Volumes/Projects | tail -1
  "$CJ" 040-asan-build cmake --build build-asan --target orender file openrender_shaders -j4 > "$L/040-asan-build-$i.log" 2>&1
  rc=$?; echo "build attempt $i exit $rc"; [ $rc -ne 124 ] && break
done
[ $rc -eq 0 ] || exit $rc
ASAN_OPTIONS="detect_leaks=0:halt_on_error=1:symbolize=1:fast_unwind_on_malloc=0" \
  "$CJ" 040-asan-run python3 "$HERE/phase0_asan_run.py" build-asan "$W/examples/rib/tests/photon-parity.rib" 30 "$L/040-asan-report" -t:4
