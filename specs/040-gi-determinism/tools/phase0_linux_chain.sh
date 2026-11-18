#!/usr/bin/env bash
# phase0_linux_chain.sh: T017 (freeze build-base) and T018 (the Linux baselines), run ON the Linux host in the per-spec tree
# ~/Projects/Development/CLI/openrender-040, every job through the remote cpujob at -j2. Output: build-logs/ (untracked) and the
# *-linux* result files next to this script (pulled back with linux_sync.sh pull).
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
W="$(cd "$HERE/../../.." && pwd)"
case "$W" in */openrender-040) ;; *) echo "refusing to run outside the spec 040 tree: $W"; exit 9 ;; esac
CJ="$HOME/.claude/bin/cpujob"
L="$W/build-logs"; mkdir -p "$L"; cd "$W" || exit 9
say() { echo "$(date +%T) $*"; }
if [ ! -d build-base ]; then say "freeze build-base"; cp -a build build-base; fi
libdirs="$(find "$W/build-base/src" -name '*.so*' -printf '%h\n' | sort -u | tr '\n' ':')"
say "smoke: camera-dof with build-base, LD_DEBUG"
LD_DEBUG=libs LD_LIBRARY_PATH="$libdirs" "$CJ" 040-l-base-smoke python3 "$HERE/render_in.py" "$W/build-base" "$W/examples/rib/camera-dof.rib" "$L/040-smoke" > "$L/040-l-smoke.log" 2>&1
echo "smoke exit $?; libRIBRender loaded from build-base: $(grep -c 'build-base/src/ri/libRIBRender' "$L/040-l-smoke.log") lines, from build/: $(grep -c "$W/build/src" "$L/040-l-smoke.log") lines"
say "suite"
"$CJ" 040-l-base-suite ctest --test-dir build -LE 'determinism-sweep|gi_determinism_long' -j2 --output-on-failure > "$L/040-l-base-suite.log" 2>&1
echo "suite exit $?"; cp build/Testing/Temporary/LastTest.log "$HERE/base-LastTest-linux.log"
say "architecture"
"$CJ" 040-l-base-arch ctest --test-dir build -L architecture --output-on-failure > "$L/040-l-base-arch.log" 2>&1
echo "arch exit $?"
for s in images images2 gi-visual counters records fixture crash frame2 timing; do
    say "step $s"
    if [ "$s" = fixture ]; then
        "$CJ" 040-l-fixture ctest --test-dir build -R '^GiFixture_ICache$' --output-on-failure > "$L/040-l-fixture.log" 2>&1
    else
        "$HERE/phase0_jobs.sh" "$s"
    fi
done
say "chain done"
