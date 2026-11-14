#!/usr/bin/env bash
# Configure and build the tree in ./build. Reads:
#   COMPILER          gcc, clang, or apple-clang
#   BUILD_TYPE        Release or Debug
#   EXTRA_CFLAGS      optional extra C and C++ flags
set -euo pipefail

compiler="${COMPILER:?}"
build_type="${BUILD_TYPE:?}"
extra_cflags="${EXTRA_CFLAGS:-}"

case "${compiler}" in
    gcc)
        export CC=gcc
        export CXX=g++
        ;;
    clang)
        export CC=clang
        export CXX=clang++
        ;;
    apple-clang)
        unset CC CXX || true
        ;;
    *)
        echo "configure-build.sh: unknown compiler '${compiler}'" >&2
        exit 2
        ;;
esac

if [ -x /opt/homebrew/opt/bison/bin/bison ]; then
    export PATH="/opt/homebrew/opt/bison/bin:${PATH}"
elif [ -x /usr/local/opt/bison/bin/bison ]; then
    export PATH="/usr/local/opt/bison/bin:${PATH}"
fi

export CCACHE_DIR="${HOME}/.cache/ccache"
export CCACHE_MAXSIZE="${CCACHE_MAXSIZE:-1G}"
export CCACHE_COMPRESS="${CCACHE_COMPRESS:-true}"
export CCACHE_SLOPPINESS="${CCACHE_SLOPPINESS:-time_macros}"
mkdir -p "${CCACHE_DIR}"
ccache --zero-stats || true

shopt -s nullglob
candidates=(
    llvm-config
    llvm-config-18
    llvm-config-19
    llvm-config-20
    /usr/lib/llvm-18/bin/llvm-config
    /usr/lib/llvm-19/bin/llvm-config
    /opt/homebrew/opt/llvm/bin/llvm-config
    /usr/local/opt/llvm/bin/llvm-config
)
llvm_dir=""
for candidate in "${candidates[@]}"; do
    if command -v "${candidate}" >/dev/null 2>&1 || [ -x "${candidate}" ]; then
        llvm_dir="$("${candidate}" --cmakedir)"
        break
    fi
done

cores="$(nproc 2>/dev/null || sysctl -n hw.ncpu)"
args=(
    -S .
    -B build
    -G Ninja
    -DCMAKE_BUILD_TYPE="${build_type}"
    -DCMAKE_C_COMPILER_LAUNCHER=ccache
    -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
    -DINSTALL_SELFCONTAINED=ON
)
if [ -n "${llvm_dir}" ]; then
    args+=("-DLLVM_DIR=${llvm_dir}")
    echo "Using LLVM_DIR=${llvm_dir}"
else
    echo "llvm-config not found; CMake will search on its own and may disable the JIT"
fi
if [ "$(uname -s)" = Darwin ]; then
    args+=(-DCMAKE_OSX_DEPLOYMENT_TARGET=13.3)
fi
if [ -n "${extra_cflags}" ]; then
    args+=("-DCMAKE_C_FLAGS=${extra_cflags}" "-DCMAKE_CXX_FLAGS=${extra_cflags}")
fi

cmake "${args[@]}"
cmake --build build --parallel "${cores}"
ccache --show-stats || true
