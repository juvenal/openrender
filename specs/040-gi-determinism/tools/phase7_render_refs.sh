#!/usr/bin/env bash
# phase7_render_refs.sh: T094's one re-render step on macOS: each scene the Phase 3 to 6 drift touched, rendered by the finished tree at the DEFAULT thread count (the pins
# removed in a scratch copy of the .rib where the pin is a numthreads line), twice, into build-logs/040-refs/<run>/. Every render is its own cpujob.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; W="$(cd "$HERE/../../.." && pwd)"
CJ="$HOME/.claude/bin/cpujob"; L="$W/build-logs/${REFS_DIR:-040-refs}"; mkdir -p "$L"; cd "$W" || exit 9
R=examples/rib/tests
strip_pin() { grep -v '^Option "limits" "numthreads" \[1\]' "$1" > "$2"; }
for scene in sphere-occlusion-reyes sphere-occlusion-reyes-slo photon-parity photon-parity-slo photon-solar0-alone photon-solar0-alone-slo photon-solar0-after-distant photon-solar0-after-distant-slo; do
    for run in 1 2; do
        d="$L/$scene.run$run"; mkdir -p "$d"
        src="$R/$scene.rib"
        case "$scene" in sphere-occlusion-reyes*) mkdir -p "$L/src"; strip_pin "$src" "$L/src/$scene.rib"; src="$L/src/$scene.rib" ;; esac
        "$CJ" "040-p7-ref-$scene-$run" python3 "$HERE/render_in.py" build "$src" "$d" > "$d.log" 2>&1; echo "$scene run$run: $(tail -1 "$d.log")"
    done
done
echo done
