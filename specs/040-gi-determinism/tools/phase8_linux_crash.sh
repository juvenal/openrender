#!/usr/bin/env bash
# phase8_linux_crash.sh: T104, ONE chain on the Linux host (remote cpujob, -j2): the 30-run photon series at the default thread count and at -t:4, Registry_IntegratorImageIdentity
# --repeat until-fail:30 (SC-002), and the GiDet 30-run forms (label gi_determinism_long).
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"; W="$(cd "$HERE/../../.." && pwd)"
case "$W" in */openrender-040) ;; *) echo "refusing to run outside the spec 040 tree: $W"; exit 9 ;; esac
CJ="$HOME/.claude/bin/cpujob"; L="$W/build-logs"; mkdir -p "$L"; cd "$W" || exit 9
TREE=build TAG=after "$HERE/phase0_jobs.sh" crash > "$L/040-l-p8-crash.log" 2>&1; echo "crash exit $?"
"$CJ" 040-l-p8-registry ctest --test-dir build -R '^Registry_IntegratorImageIdentity$' --repeat until-fail:30 --output-on-failure > "$L/040-l-p8-registry.log" 2>&1; echo "registry exit $?"
"$CJ" 040-l-p8-long ctest --test-dir build -L '^gi_determinism_long$' -j2 --output-on-failure > "$L/040-l-p8-long.log" 2>&1; echo "long exit $?"
echo "chain done"
