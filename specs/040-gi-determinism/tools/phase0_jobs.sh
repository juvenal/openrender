#!/usr/bin/env bash
# phase0_jobs.sh: the Phase 0 measurement jobs of spec 040 (tasks.md T008 to T014a), the same script on macOS and on Linux.
#
#   phase0_jobs.sh <step>            run one step as ONE job under the lock (cpujob), from this worktree's root
#
# Steps (each writes its own result file next to this script, `-macos` or `-linux` by uname, and its scratch under build-logs/):
#   images      T008  float images of the 18 GI scenes with build-base at -t:1 (run twice: images, images2) -> baseline-gi-images-<p>.sha256
#   gi-visual   T009  the GI Visual_* tests' MaxBlockAvgDiff from ctest's LastTest.log -> baseline-gi-diffs-<p>.txt
#   counters    T011  the GI statistics counters, cache on and off
#   records     T013  record counts of today's cache -> records-base-<p>.txt
#   crash       T014  30 fresh runs per photon scene at the default thread count and at -t:4 -> photon-crash-base-<p>.txt
#   frame2      T014a frame-2-only photon scenes, 30 runs at -t:4 and at -t:1 -> photon-frame2-base-<p>.txt
#   timing      T010  timing_sc007.sh build-base against build-base, 5 series -> timing-base-<p>.txt
# Every cpujob of a step is its own job (one at a time machine-wide). `crash` and `frame2` run several series, each its own job; the
# whole script then does not hold the lock between series. Never -j above 4 (the Linux jobs are at -j2 by the machine's own limit).
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
W="$(cd "$HERE/../../.." && pwd)"
T="$HERE"
CJ="${CJ:-$HOME/.claude/bin/cpujob}"
case "$(uname)" in Darwin) P=macos ;; *) P=linux ;; esac
L="$W/build-logs"
mkdir -p "$L"
cd "$W" || exit 9
step="${1:-}"
BASE="$W/build-base"
case "$step" in
images|images2)
    out="$L/gi-base-images"; [ "$step" = images2 ] && out="$L/gi-base-images2"
    "$CJ" "040-gi-$step-$P" "$T/image_manifest.sh" gi-render "$BASE" "$out" -t:1 > "$L/040-gi-$step-$P.log" 2>&1
    echo "gi-render exit $?"
    "$T/image_manifest.sh" gi-hash "$out" > "$L/040-gi-hash-$step-$P.txt"
    wc -l "$L/040-gi-hash-$step-$P.txt"
    ;;
gi-visual)
    "$CJ" "040-base-gi-visual-$P" ctest --test-dir build --output-on-failure -R \
        '^Visual_(sphere-(occlusion|indirectdiffuse|photonmap|ray-self-hit)|photon-|subdiv-.*-photon)|^NotRequired_motion-subdiv-translate-photon' \
        > "$L/040-base-gi-visual-$P.log" 2>&1
    echo "ctest exit $?"
    cp build/Testing/Temporary/LastTest.log "$L/040-base-gi-visual-LastTest-$P.log"
    ;;
counters)
    S="$L/040-counters-scratch"; rm -rf "$S"; mkdir -p "$S"
    # three variants of each scene: the attribute left alone (the default maxerror is 0: the cache is OFF), maxerror 0.5 (ON), maxerror 0 written out
    mk() { # <out-prefix> <template> <anchor regex> <indent> <maxerror or ->
        local out=$1 tpl=$2 anchor=$3 ind=$4 me=$5
        if [ "$me" = - ]; then cp "$tpl" "$out.rib"; else
            perl -pe 'BEGIN{$f=0} if (/FrameBegin 2/) { $f = 1 } if (($f || "'"$anchor"'" eq "any") && /^\s*WorldBegin/ && !$d) { $_ .= "'"$ind"'Attribute \"irradiance\" \"float maxerror\" ['"$me"']\n"; $d = 1 }' "$tpl" > "$out.rib"; fi; }
    sed 's/@THREADS@/1/' "$T/perf-indirect-photon.rib" > "$S/perf-indirect.tpl"
    sed 's/@THREADS@/1/' "$T/perf-occlusion-cache.rib" > "$S/perf-occlusion.tpl"
    cp "$W/examples/rib/tests/sphere-indirectdiffuse-reyes.rib" "$S/suite-indirect.tpl"
    mk "$S/perf-indirect-default" "$S/perf-indirect.tpl" frame2 "    " -
    mk "$S/perf-indirect-maxerror0.5" "$S/perf-indirect.tpl" frame2 "    " 0.5
    mk "$S/perf-indirect-maxerror0" "$S/perf-indirect.tpl" frame2 "    " 0
    mk "$S/suite-indirect-default" "$S/suite-indirect.tpl" any "  " -
    mk "$S/suite-indirect-maxerror0.5" "$S/suite-indirect.tpl" any "  " 0.5
    mk "$S/suite-indirect-maxerror0" "$S/suite-indirect.tpl" any "  " 0
    cp "$S/perf-occlusion.tpl" "$S/perf-occlusion-maxerror0.5.rib"
    sed 's/"float maxerror" \[0.5\]/"float maxerror" [0]/' "$S/perf-occlusion.tpl" > "$S/perf-occlusion-maxerror0.rib"
    for s in perf-indirect-default perf-indirect-maxerror0.5 perf-indirect-maxerror0 suite-indirect-default suite-indirect-maxerror0.5 suite-indirect-maxerror0 perf-occlusion-maxerror0.5 perf-occlusion-maxerror0; do
        "$CJ" "040-counters-$s-$P" python3 "$T/measure_gi.py" stats "$W/build" "$S/$s.rib" -t:1 2>&1
    done
    ;;
records)
    OUT="$T/records-base-$P.txt"; S="$L/040-records-scratch"; rm -rf "$S"; mkdir -p "$S"
    { echo "# T013 record counts of the starting tree's irradiance cache (build-base, master 278b0394, $P), one render each at -t:1, unscaled."
      echo "# Counted with icache_dump.py from the written .icache file, never from an indirectdiffuse counter (T011)."
      for s in perf-occlusion-cache:perf-occlusion.icache perf-indirect-photon-cache:perf-indirect.icache; do
          name=${s%%:*}; cache=${s##*:}
          sed 's/@THREADS@/1/' "$T/$name.rib" > "$S/$name.rib"
          "$CJ" "040-records-$name-$P" python3 "$T/render_in.py" "$BASE" "$S/$name.rib" "$S/$name" -t:1 > "$S/$name.log" 2>&1
          echo "== $name: render $(tail -1 "$S/$name.log")"
          python3 "$T/icache_dump.py" "$S/$name/$cache"
      done; } > "$OUT" 2>&1
    cat "$OUT"
    ;;
crash)
    OUT="$T/photon-crash-base-$P.txt"
    { echo "# T014 starting tree (build-base, master 278b0394, $P): 30 separate processes per series, a fresh directory per run (dirmode fresh),"
      echo "# the .gpm the scene writes compared by md5. 'failed runs' lists (run, exit code); a negative code is the signal number."
      echo "# default = no -t option (the scene's own numthreads / the machine default); -t:4 = four threads."; } > "$OUT"
    for scene in photon-parity photon-parity-slo photon-solar0-alone photon-solar0-alone-slo photon-solar0-after-distant photon-solar0-after-distant-slo photon-solar0-before-distant photon-solar0-before-distant-slo; do
        for thr in default -t:4; do
            opts=""; [ "$thr" != default ] && opts="$thr"
            echo "=== $scene, threads $thr" >> "$OUT"
            "$CJ" "040-crash-$scene-$thr-$P" python3 "$T/measure_gi.py" repeat "$BASE" "$W/examples/rib/tests/$scene.rib" "$scene.gpm" 30 fresh $opts >> "$OUT" 2>&1
            echo "  (cpujob exit $?)" >> "$OUT"
        done
    done
    echo done >> "$OUT"
    ;;
frame2)
    OUT="$T/photon-frame2-base-$P.txt"; S="$L/040-frame2-scratch"; rm -rf "$S"; mkdir -p "$S"
    { echo "# T014a frame-2 images on the starting tree (build-base, master 278b0394, $P): frame 1 removed, Quantize \"rgba\" 0 0 0 0 prepended (float image),"
      echo "# the map = the one a single -t:1 run of the full scene wrote and left in place, copied unchanged for every run (M040_SEED),"
      echo "# 30 runs at -t:4 and 30 at -t:1, separate processes, dirmode reuse (one directory, the map and the previous image in place)."; } > "$OUT"
    for scene in photon-parity subdiv-loop-photon; do
        mkdir -p "$S/$scene-seed" "$S/$scene-map"
        "$CJ" "040-frame2-seed-$scene-$P" python3 "$T/render_in.py" "$BASE" "$W/examples/rib/tests/$scene.rib" "$S/$scene-seed" -t:1 > "$S/$scene-seed.log" 2>&1
        cp "$S/$scene-seed/$scene.gpm" "$S/$scene-map/"
        echo "== $scene: seed render $(tail -1 "$S/$scene-seed.log"); map md5 $(python3 -c 'import hashlib,sys;print(hashlib.md5(open(sys.argv[1],"rb").read()).hexdigest())' "$S/$scene-map/$scene.gpm")" >> "$OUT"
        python3 - "$W/examples/rib/tests/$scene.rib" "$S/$scene-frame2.rib" <<'PY'
import sys
out = ['Quantize "rgba" 0 0 0 0']
skip = False
for l in open(sys.argv[1]).read().split("\n"):
    if l.startswith("FrameBegin 1"):
        skip = True
    if not skip:
        out.append(l)
    if skip and l.startswith("FrameEnd"):
        skip = False
open(sys.argv[2], "w").write("\n".join(out) + "\n")
PY
        for thr in -t:4 -t:1; do
            echo "--- $scene frame 2 only, $thr" >> "$OUT"
            M040_SEED="$S/$scene-map" "$CJ" "040-frame2-$scene-$thr-$P" python3 "$T/measure_gi.py" repeat "$BASE" "$S/$scene-frame2.rib" "$scene.tif" 30 reuse $thr >> "$OUT" 2>&1
        done
    done
    echo done >> "$OUT"
    ;;
timing)
    SCALES="perf-occlusion-cache:format=12,shadingrate=3 perf-indirect-photon:format=6,shadingrate=3,emit=10 perf-photon-pass:emit=75" \
        "$CJ" "040-base-timing-$P" "$T/timing_sc007.sh" "$BASE" "$BASE" 5 > "$T/timing-base-$P.txt" 2>&1
    echo "timing exit $?"
    ;;
*)
    sed -n '2,18p' "$0" | sed 's/^# \{0,1\}//' >&2
    exit 2
    ;;
esac
