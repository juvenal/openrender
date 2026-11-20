#!/usr/bin/env bash
# phase8_linux_chain.sh: T104 and T106: the Linux crash and identity runs for the pins and the closing validation, ONE chain on the Linux host (remote cpujob at -j2).
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; W="$(cd "$HERE/../../.." && pwd)"
case "$W" in */openrender-040) ;; *) echo "refusing to run outside the spec 040 tree: $W"; exit 9 ;; esac
CJ="$HOME/.claude/bin/cpujob"; L="$W/build-logs"; mkdir -p "$L"; cd "$W" || exit 9
"$CJ" 040-l-final-build sh -c 'cmake -S . -B build -DOPENRENDER_040_RED_TESTS=OFF && cmake --build build -j2' > "$L/040-l-final-build.log" 2>&1; echo "build exit $?"
TREE=build TAG=after "$HERE/phase0_jobs.sh" crash > "$L/040-l-final-crash.log" 2>&1; echo "crash exit $?"
"$CJ" 040-l-final-registry ctest --test-dir build -R '^Registry_IntegratorImageIdentity$' --repeat until-fail:30 --output-on-failure > "$L/040-l-final-registry.log" 2>&1; echo "registry exit $?"
"$CJ" 040-l-final-long ctest --test-dir build -L '^gi_determinism_long$' -j2 --output-on-failure > "$L/040-l-final-long.log" 2>&1; echo "gi_determinism_long exit $?"
"$CJ" 040-l-final-suite ctest --test-dir build -LE 'determinism-sweep|gi_determinism_long' -j2 --output-on-failure > "$L/040-l-final-suite.log" 2>&1; echo "suite exit $?"
for i in 1 2 3; do
    "$CJ" 040-l-final-sweep$i ctest --test-dir build -L '^determinism-sweep$' -j2 --output-on-failure > "$L/040-l-final-sweep$i.log" 2>&1; echo "sweep$i exit $?"
done
"$CJ" 040-l-final-det ctest --test-dir build -L '^determinism$' -j2 --output-on-failure > "$L/040-l-final-det.log" 2>&1; echo "determinism exit $?"
"$CJ" 040-l-final-gidet ctest --test-dir build -L '^gi_determinism$' -j2 --output-on-failure > "$L/040-l-final-gidet.log" 2>&1; echo "gi_determinism exit $?"
"$CJ" 040-l-final-gisplit ctest --test-dir build -L '^gi_split(_threads)?$' --output-on-failure > "$L/040-l-final-gisplit.log" 2>&1; echo "gi_split exit $?"
for lab in shading_parity texture_only state plugin_abi link; do
    "$CJ" 040-l-final-$lab ctest --test-dir build -L "^$lab\$" -j2 --output-on-failure > "$L/040-l-final-$lab.log" 2>&1; echo "$lab exit $?"
done
"$CJ" 040-l-final-arch ctest --test-dir build -L architecture --output-on-failure > "$L/040-l-final-arch.log" 2>&1; echo "arch exit $?"
echo "chain done"
