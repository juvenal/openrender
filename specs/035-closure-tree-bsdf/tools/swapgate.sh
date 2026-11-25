#!/bin/bash
# Spec 035: the swap gate. BLOCKS until at least SWAPGATE_MIN_MB (default 1024, one GiB) of swap is free on macOS, then exits 0; if
# SWAPGATE_MAX_WAIT seconds (default 1800) pass first it exits 1 and prints why, so a job chained after it (`swapgate.sh && cpujob ...`)
# does not start. It prints the figure it saw each time it waits. Not a build or test step: a helper the agent runs before a macOS job.
min=${SWAPGATE_MIN_MB:-1024}
maxwait=${SWAPGATE_MAX_WAIT:-1800}
interval=${SWAPGATE_INTERVAL:-10}
waited=0
while true; do
    free=$(sysctl -n vm.swapusage | sed 's/.*free = \([0-9.]*\)M.*/\1/')
    if [ "$(echo "$free >= $min" | bc)" = 1 ]; then
        echo "swapgate: ${free}M free (needs ${min}M): go"
        exit 0
    fi
    if [ "$waited" -ge "$maxwait" ]; then
        echo "swapgate: ${free}M free after ${waited}s (needs ${min}M): NOT starting the job" >&2
        exit 1
    fi
    echo "swapgate: ${free}M free (needs ${min}M): waiting"
    sleep "$interval"
    waited=$((waited + interval))
done
