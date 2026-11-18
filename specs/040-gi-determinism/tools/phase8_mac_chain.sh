#!/usr/bin/env bash
# phase8_mac_chain.sh: T105, the closing validation on macOS, as one chain, every step its own cpujob (-j4 at most, the sweeps and long forms -j2). Output in build-logs/.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; W="$(cd "$HERE/../../.." && pwd)"
CJ="$HOME/.claude/bin/cpujob"; L="$W/build-logs"; cd "$W" || exit 9
"$CJ" 040-final-build cmake --build build --config Release -j4 > "$L/040-final-build.log" 2>&1; echo "build exit $?"
"$CJ" 040-final-suite ctest --test-dir build -LE 'determinism-sweep|gi_determinism_long' -j4 --output-on-failure > "$L/040-final-suite.log" 2>&1; echo "suite exit $?"
for i in 1 2 3; do
    "$CJ" 040-final-sweep$i ctest --test-dir build -L '^determinism-sweep$' -j2 --output-on-failure > "$L/040-final-sweep$i.log" 2>&1; echo "sweep$i exit $?"
done
"$CJ" 040-final-det ctest --test-dir build -L '^determinism$' -j2 --output-on-failure > "$L/040-final-det.log" 2>&1; echo "determinism exit $?"
"$CJ" 040-final-gidet ctest --test-dir build -L '^gi_determinism$' -j4 --output-on-failure > "$L/040-final-gidet.log" 2>&1; echo "gi_determinism exit $?"
"$CJ" 040-final-long ctest --test-dir build -L '^gi_determinism_long$' -j2 --output-on-failure > "$L/040-final-long.log" 2>&1; echo "gi_determinism_long exit $?"
"$CJ" 040-final-gisplit ctest --test-dir build -L '^gi_split(_threads)?$' --output-on-failure > "$L/040-final-gisplit.log" 2>&1; echo "gi_split exit $?"
for lab in shading_parity texture_only state plugin_abi link; do
    "$CJ" 040-final-$lab ctest --test-dir build -L "^$lab\$" -j2 --output-on-failure > "$L/040-final-$lab.log" 2>&1; echo "$lab exit $?"
done
"$CJ" 040-final-arch ctest --test-dir build -L architecture --output-on-failure > "$L/040-final-arch.log" 2>&1; echo "arch exit $?"
git diff 278b0394 -- tests/architecture/allowlist.txt | grep '^+[^+]' > "$L/040-final-allowlist-added.txt"; echo "allowlist added lines: $(wc -l < "$L/040-final-allowlist-added.txt")"
"$CJ" 040-final-registry ctest --test-dir build -R '^Registry_IntegratorImageIdentity$' --repeat until-fail:30 --output-on-failure > "$L/040-final-registry.log" 2>&1; echo "registry exit $?"
echo "chain done"
