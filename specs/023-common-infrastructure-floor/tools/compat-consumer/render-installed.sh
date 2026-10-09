#!/usr/bin/env bash
# Spec 023 (T003): render with the install's own orender, so installed display
# modules and shaders are exercised from the installed tree.
#
# Usage: render-installed.sh <install-root> [out-dir]
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd -P)"
source "$here/common.sh"
repo="$(cd "$here/../../../.." && pwd -P)"

root="$(cd "$1" && pwd -P)"
prefix="$(prefix_of "$root")"
out="${2:-$(mktemp -d)}"
mkdir -p "$out"
runtime_env "$prefix"

cd "$out"
check_resolution "$root" "$prefix/bin/orender" --help
rib="$repo/examples/rib/camera-dof.rib"
"$prefix/bin/orender" "$rib"
tif=$(ls -t ./*.tif 2>/dev/null | head -1 || true)
if [ -z "$tif" ]; then
    echo "render-installed: no TIFF written in $out" >&2
    exit 1
fi
echo "render-installed: $out/${tif#./} ($(wc -c < "$tif") bytes)"
