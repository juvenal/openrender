#!/usr/bin/env bash
# phase10_mac_chain.sh: the re-validation of the merged tree (spec 033 merged, services 2.5.0) on macOS, as one chain, every step its own cpujob (-j4 at most, the sweeps and long forms -j2). Output in build-logs/.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; W="$(cd "$HERE/../../.." && pwd)"
CJ="$HOME/.claude/bin/cpujob"; L="$W/build-logs"; cd "$W" || exit 9
sysctl vm.swapusage | sed "s/^/swap: /"
"$CJ" 040-m-build cmake --build build --config Release -j4 > "$L/040-m-build.log" 2>&1; echo "build exit $?"
sysctl vm.swapusage | sed "s/^/swap: /"
"$CJ" 040-m-suite ctest --test-dir build -LE 'determinism-sweep|gi_determinism_long' -j4 --output-on-failure > "$L/040-m-suite.log" 2>&1; echo "suite exit $?"
for i in 1 2 3; do
sysctl vm.swapusage | sed "s/^/swap: /"
    "$CJ" 040-m-sweep$i ctest --test-dir build -L '^determinism-sweep$' -j2 --output-on-failure > "$L/040-m-sweep$i.log" 2>&1; echo "sweep$i exit $?"
done
sysctl vm.swapusage | sed "s/^/swap: /"
"$CJ" 040-m-det ctest --test-dir build -L '^determinism$' -j2 --output-on-failure > "$L/040-m-det.log" 2>&1; echo "determinism exit $?"
sysctl vm.swapusage | sed "s/^/swap: /"
"$CJ" 040-m-gidet ctest --test-dir build -L '^gi_determinism$' -j4 --output-on-failure > "$L/040-m-gidet.log" 2>&1; echo "gi_determinism exit $?"
sysctl vm.swapusage | sed "s/^/swap: /"
"$CJ" 040-m-long ctest --test-dir build -L '^gi_determinism_long$' -j2 --output-on-failure > "$L/040-m-long.log" 2>&1; echo "gi_determinism_long exit $?"
sysctl vm.swapusage | sed "s/^/swap: /"
"$CJ" 040-m-gisplit ctest --test-dir build -L '^gi_split(_threads)?$' --output-on-failure > "$L/040-m-gisplit.log" 2>&1; echo "gi_split exit $?"
for lab in shading_parity texture_only state plugin_abi link; do
sysctl vm.swapusage | sed "s/^/swap: /"
    "$CJ" 040-m-$lab ctest --test-dir build -L "^$lab\$" -j2 --output-on-failure > "$L/040-m-$lab.log" 2>&1; echo "$lab exit $?"
done
sysctl vm.swapusage | sed "s/^/swap: /"
"$CJ" 040-m-arch ctest --test-dir build -L architecture --output-on-failure > "$L/040-m-arch.log" 2>&1; echo "arch exit $?"
git diff master -- tests/architecture/allowlist.txt | grep '^[+-][^+-]' > "$L/040-m-allowlist-vs-master.txt"; echo "allowlist lines differing from master: $(wc -l < "$L/040-m-allowlist-vs-master.txt")"
sysctl vm.swapusage | sed "s/^/swap: /"
"$CJ" 040-m-registry ctest --test-dir build -R '^Registry_IntegratorImageIdentity$' --repeat until-fail:30 --output-on-failure > "$L/040-m-registry.log" 2>&1; echo "registry exit $?"
echo "chain done"
