#!/usr/bin/env bash
# phase9_linux_chain.sh: T115 and T116 on Linux: build, the two new tests, the caustic sweep entries three times, the 30-run caustic repeats (one md5 per map, -t:1 and -t:4), architecture and link. ONE chain on the Linux host (remote cpujob at -j2).
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; W="$(cd "$HERE/../../.." && pwd)"
case "$W" in */openrender-040) ;; *) echo "refusing to run outside the spec 040 tree: $W"; exit 9 ;; esac
CJ="$HOME/.claude/bin/cpujob"; L="$W/build-logs"; mkdir -p "$L"; cd "$W" || exit 9
"$CJ" 040-l-p9-build sh -c 'cmake -S . -B build -DOPENRENDER_040_RED_TESTS=OFF && cmake --build build -j2' > "$L/040-l-p9-build.log" 2>&1; echo "build exit $?"
"$CJ" 040-l-p9-gidet ctest --test-dir build -R '^GiDet_(PhotonOnePhoton|CacheThreadExtremes)$' -j2 --output-on-failure > "$L/040-l-p9-gidet.log" 2>&1; echo "gidet exit $?"
for i in 1 2 3; do
    "$CJ" 040-l-p9-caustic$i ctest --test-dir build -R '^DeterminismSweep_gi-photon-caustic' -j2 --output-on-failure > "$L/040-l-p9-caustic$i.log" 2>&1; echo "caustic$i exit $?"
done
for m in gi-photon-caustic.gpm gi-photon-caustic-c.gpm; do for t in -t:1 -t:4; do
    "$CJ" 040-l-p9-rep python3 specs/040-gi-determinism/tools/measure_gi.py repeat "$W/build" "$W/tests/gi_determinism/scenes/gi-photon-caustic.rib" $m 30 fresh $t > "$L/040-l-p9-repeat-$m-$t.log" 2>&1; echo "repeat $m $t exit $?"
done; done
"$CJ" 040-l-p9-gidet-all ctest --test-dir build -L '^gi_determinism$' -j2 --output-on-failure > "$L/040-l-p9-gidet-all.log" 2>&1; echo "gi_determinism exit $?"
"$CJ" 040-l-p9-link ctest --test-dir build -L '^link$' -j2 --output-on-failure > "$L/040-l-p9-link.log" 2>&1; echo "link exit $?"
"$CJ" 040-l-p9-arch ctest --test-dir build -L architecture --output-on-failure > "$L/040-l-p9-arch.log" 2>&1; echo "arch exit $?"
echo "chain done"
