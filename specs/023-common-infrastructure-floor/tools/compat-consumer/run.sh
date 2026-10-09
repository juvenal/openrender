#!/usr/bin/env bash
# Spec 023 (T003): run the three compat consumers built by build.sh.
#
# Usage: run.sh <install-root> <tag> [--pinned]
#   Outputs land next to this script: out-RIBRender-<tag>.tif and
#   out-RIBVector-<tag>.rib / out-RIBOut-<tag>.rib.
#   By default no DYLD_*/LD_* library-path variable is set: the binaries
#   resolve libraries through their own rpath, and the run fails if any
#   libRIB*/libshader_* resolves outside <install-root>. --pinned also forces
#   the install's lib dir; it is a diagnostic aid, never the proof.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd -P)"
source "$here/common.sh"

root="$(cd "$1" && pwd -P)"
tag="$2"
pinned="${3:-}"
prefix="$(prefix_of "$root")"
runtime_env "$prefix"
[ "$pinned" = "--pinned" ] && pin_libs "$prefix"

cd "$here"
status=0
for lib in RIBRender RIBVector RIBOut; do
    if [ "$lib" = RIBRender ]; then
        mode=render
        output="out-$lib-$tag.tif"
    else
        mode=rib
        output="out-$lib-$tag.rib"
    fi
    rm -f "$output"
    echo "== consumer-$lib ($mode) -> $output"
    check_resolution "$root" "./consumer-$lib" "$mode" "$output" || status=1
    rm -f "$output"
    if "./consumer-$lib" "$mode" "$output" > "log-$lib-$tag.txt" 2>&1; then
        rc=0
    else
        rc=$?
    fi
    echo "    exit=$rc"
    sed 's/^/    | /' "log-$lib-$tag.txt"
    if [ "$rc" -ne 0 ]; then status=1; fi
    if [ -s "$output" ]; then
        echo "    output: $(wc -c < "$output") bytes"
    else
        echo "    output: (none)"
    fi
done
exit $status
