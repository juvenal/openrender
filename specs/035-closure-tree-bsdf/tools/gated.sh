#!/bin/bash
# Spec 035: run a macOS job behind the swap gate: `gated.sh <label> <command...>` blocks in swapgate.sh until at least 1 GiB of swap is free,
# then execs `~/.claude/bin/cpujob <label> <command...>`. If the gate gives up (SWAPGATE_MAX_WAIT) the job is NOT started and the exit code is 75.
here="$(cd "$(dirname "$0")" && pwd)"
if ! "$here/swapgate.sh"; then
    exit 75
fi
exec "$HOME/.claude/bin/cpujob" "$@"
