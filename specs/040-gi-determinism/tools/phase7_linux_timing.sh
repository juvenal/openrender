#!/usr/bin/env bash
# phase7_linux_timing.sh: T097 on Linux, ONE chain (remote cpujob). The same scenes and scales as the macOS run; THREADS is the host's own default of timing_sc007.sh.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; W="$(cd "$HERE/../../.." && pwd)"
case "$W" in */openrender-040) ;; *) echo "refusing to run outside the spec 040 tree: $W"; exit 9 ;; esac
CJ="$HOME/.claude/bin/cpujob"; cd "$W" || exit 9
SCALES="perf-occlusion-cache:format=12,shadingrate=3 perf-indirect-photon:format=6,shadingrate=3,emit=10 perf-photon-pass:emit=75" "$CJ" 040-l-p7-timing "$HERE/timing_sc007.sh" build-base build 5 > "$HERE/timing-after-linux.txt" 2>&1; echo "timing exit $?"
SCENES="perf-occlusion-cache perf-occlusion-twoframe" SCALES="perf-occlusion-cache:format=6,shadingrate=3 perf-occlusion-twoframe:format=6,shadingrate=3" "$CJ" 040-l-p7-twoframe "$HERE/timing_sc007.sh" build-base build 5 > "$HERE/timing-twoframe-linux.txt" 2>&1; echo "twoframe exit $?"
echo "chain done"
