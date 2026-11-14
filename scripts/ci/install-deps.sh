#!/usr/bin/env bash
# Install the compilers and libraries a hosted runner needs to configure openRender.
# Ubuntu 24.04 is the supported Linux baseline. macOS uses Homebrew. Fedora is the RPM image.
set -euo pipefail

compiler="${COMPILER:-gcc}"

if [ "$(uname -s)" = Darwin ]; then
    brew install cmake ninja ccache pkg-config libtiff libpng flex bison openexr llvm
    exit 0
fi

# shellcheck disable=SC1091
. /etc/os-release
sudo=()
if [ "$(id -u)" -ne 0 ] && command -v sudo >/dev/null 2>&1; then
    sudo=(sudo)
fi

case "${ID}" in
    fedora)
        "${sudo[@]}" dnf install -y \
            cmake gcc-c++ make flex bison ninja-build ccache pkgconfig git \
            libtiff-devel libpng-devel zlib-devel openexr-devel \
            libX11-devel wayland-devel wayland-protocols-devel libdecor-devel \
            gtk4-devel libadwaita-devel libepoxy-devel mesa-libGL-devel \
            rpm-build llvm-devel
        if [ "${compiler}" = clang ]; then
            "${sudo[@]}" dnf install -y clang
        fi
        ;;
    ubuntu|debian)
        export DEBIAN_FRONTEND=noninteractive
        "${sudo[@]}" apt-get update
        "${sudo[@]}" apt-get install -y --no-install-recommends \
            build-essential cmake ninja-build ccache pkg-config git \
            flex bison gcc g++ clang \
            libtiff-dev libpng-dev zlib1g-dev \
            libopenexr-dev \
            libx11-dev libwayland-dev libwayland-bin wayland-protocols libdecor-0-dev \
            llvm-18-dev \
            python3 lua5.4 \
            libgtk-4-dev libadwaita-1-dev libepoxy-dev
        if [ "${REQUIRE_ABI_TOOLS:-false}" = true ]; then
            "${sudo[@]}" apt-get install -y --no-install-recommends \
                abi-dumper abi-compliance-checker elfutils
        fi
        ;;
    *)
        echo "install-deps.sh: unsupported OS id '${ID}'" >&2
        exit 1
        ;;
esac
