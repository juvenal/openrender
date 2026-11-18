#!/usr/bin/env bash
# phase10_linux_chain.sh: the re-validation of the merged tree (spec 033 merged, services 2.5.0) on Linux, ONE chain (remote cpujob at -j2).
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; W="$(cd "$HERE/../../.." && pwd)"
case "$W" in */openrender-040) ;; *) echo "refusing to run outside the spec 040 tree: $W"; exit 9 ;; esac
CJ="$HOME/.claude/bin/cpujob"; L="$W/build-logs"; mkdir -p "$L"; cd "$W" || exit 9
"$CJ" 040-l-m-build sh -c 'cmake -S . -B build -DOPENRENDER_040_RED_TESTS=OFF && cmake --build build -j2' > "$L/040-l-m-build.log" 2>&1; echo "build exit $?"
TREE=build TAG=after "$HERE/phase0_jobs.sh" crash > "$L/040-l-m-crash.log" 2>&1; echo "crash exit $?"
"$CJ" 040-l-m-registry ctest --test-dir build -R '^Registry_IntegratorImageIdentity$' --repeat until-fail:30 --output-on-failure > "$L/040-l-m-registry.log" 2>&1; echo "registry exit $?"
"$CJ" 040-l-m-long ctest --test-dir build -L '^gi_determinism_long$' -j2 --output-on-failure > "$L/040-l-m-long.log" 2>&1; echo "gi_determinism_long exit $?"
"$CJ" 040-l-m-suite ctest --test-dir build -LE 'determinism-sweep|gi_determinism_long' -j2 --output-on-failure > "$L/040-l-m-suite.log" 2>&1; echo "suite exit $?"
for i in 1 2 3; do
    "$CJ" 040-l-m-sweep$i ctest --test-dir build -L '^determinism-sweep$' -j2 --output-on-failure > "$L/040-l-m-sweep$i.log" 2>&1; echo "sweep$i exit $?"
done
"$CJ" 040-l-m-det ctest --test-dir build -L '^determinism$' -j2 --output-on-failure > "$L/040-l-m-det.log" 2>&1; echo "determinism exit $?"
"$CJ" 040-l-m-gidet ctest --test-dir build -L '^gi_determinism$' -j2 --output-on-failure > "$L/040-l-m-gidet.log" 2>&1; echo "gi_determinism exit $?"
"$CJ" 040-l-m-gisplit ctest --test-dir build -L '^gi_split(_threads)?$' --output-on-failure > "$L/040-l-m-gisplit.log" 2>&1; echo "gi_split exit $?"
for lab in shading_parity texture_only state plugin_abi link; do
    "$CJ" 040-l-m-$lab ctest --test-dir build -L "^$lab\$" -j2 --output-on-failure > "$L/040-l-m-$lab.log" 2>&1; echo "$lab exit $?"
done
"$CJ" 040-l-m-arch ctest --test-dir build -L architecture --output-on-failure > "$L/040-l-m-arch.log" 2>&1; echo "arch exit $?"
echo "chain done"
