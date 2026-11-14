#!/usr/bin/env bash
# cmake --install into DESTDIR, after the generated bundle step is patched
# so external libraries land in the staged prefix. See patch-bundle-destdir.py.
set -euo pipefail

build="${1:?build directory}"
destdir="${2:?DESTDIR}"

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
python3 "${here}/patch-bundle-destdir.py" "${build}/cmake_install.cmake"
DESTDIR="${destdir}" cmake --install "${build}"
