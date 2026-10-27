#!/usr/bin/env bash
# time_texture_scenes.sh <build-dir> <runs>
# Spec 027 (research R19, T005): wall time of two texture scenes, <runs> runs each, at the
# default thread count and at -t:1. Prints "scene<TAB>threads<TAB>seconds" per run (threads is
# "default" or "1"), to be redirected to tools/timing-*.tsv. Run it through cpujob, with no
# other job interleaved: the spread between runs is the noise bar for SC-010.
set -u
B="$(cd "$1" && pwd)"; N="${2:-10}"
W="$(cd "$(dirname "$0")/../../.." && pwd)"
OUT="$B/timing-texture-scenes"; rm -rf "$OUT"; mkdir -p "$OUT"
export SHADERS="$B/shaders" ORENDERHOME="$B/tests/visual/_home" DISPLAYS="$B/src/display/file"
export TEXTURES="$W/examples/rib/tests:$W/tests/unit/image_input/fixtures:$W/tests/unit/texture_tile/fixtures"
now() { python3 -c 'import time; print(time.time())'; }
for rib in "$W/tests/unit/texture_tile/concurrency-scene.rib" "$W/examples/rib/tests/parity/texture-png-raytrace.rib"; do
  scene="$(basename "$rib" .rib)"
  for th in default 1; do
    flag=""; [ "$th" = 1 ] && flag="-t:1"
    d="$OUT/$scene-$th"; mkdir -p "$d"
    i=0
    while [ "$i" -lt "$N" ]; do
      t0=$(now)
      ( cd "$d" && "$B/src/orender/orender" $flag "$rib" > "$d/orender.log" 2>&1 )
      t1=$(now)
      printf '%s\t%s\t%.3f\n' "$scene" "$th" "$(python3 -c "print($t1-$t0)")"
      i=$((i+1))
    done
  done
done
