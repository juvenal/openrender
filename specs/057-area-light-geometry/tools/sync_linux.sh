#!/usr/bin/env bash
# Spec 057 (tasks.md T005): sync the worktree to the Linux validation tree and verify it.
#
#   tools/sync_linux.sh [--remove <path>...]
#
# Copies every tracked and untracked-but-not-ignored file of the worktree to ${R} on ${H} with rsync (checksums, no
# delete), writes a manifest.sha256 on this machine, copies it, and runs `sha256sum -c` on the host. --remove names
# retired files to delete on the host first (the sync never deletes). Only ${R} is touched: this script refuses to run
# if it is anything but the 057 tree. In zsh a host-path pair is written "${H}:path": $H:path is a modifier.
set -euo pipefail

H="juvenal@ubuntu-2404-desktop.v2-labs.io"
R="/home/juvenal/Projects/Development/CLI/openrender-057"
case "${R}" in
    */openrender-057) ;;
    *) echo "sync_linux.sh: refusing to run against ${R}: not the openrender-057 tree" >&2; exit 2 ;;
esac
# Never another tree of the host: openrender, cellar, shrimp, openrender-029, openrender-038 (nor another spec's tree).
case "${R}" in
    */openrender|*/cellar|*/shrimp|*/openrender-029|*/openrender-038)
        echo "sync_linux.sh: refusing to run against ${R}: a forbidden tree" >&2; exit 2 ;;
esac

WT="$(cd "$(dirname "$0")/../../.." && pwd)"
SNAP="${WT}/build/sync-057"
mkdir -p "${SNAP}"

removals=()
if [ "${1:-}" = "--remove" ]; then
    shift
    removals=("$@")
fi

if [ "${#removals[@]}" -gt 0 ]; then
    for p in "${removals[@]}"; do
        case "${p}" in
            /*|*..*) echo "sync_linux.sh: refusing to remove ${p}" >&2; exit 2 ;;
        esac
        ssh "${H}" "rm -f '${R}/${p}'"
    done
fi

# files.list: NUL separated, only files that exist in the worktree (a staged deletion is not synced)
git -C "${WT}" ls-files -z --cached --others --exclude-standard | while IFS= read -r -d '' f; do
    if [ -f "${WT}/${f}" ] || [ -L "${WT}/${f}" ]; then printf '%s\0' "${f}"; fi
done > "${SNAP}/files.list"
n=$(tr -cd '\0' < "${SNAP}/files.list" | wc -c | tr -d ' ')

( cd "${WT}" && xargs -0 shasum -a 256 < "${SNAP}/files.list" > "${SNAP}/manifest.sha256" )

ssh "${H}" "mkdir -p '${R}'"
rsync -rlpc -0 --files-from="${SNAP}/files.list" "${WT}/" "${H}:${R}/"
rsync -c "${SNAP}/manifest.sha256" "${H}:${R}/manifest.sha256"
ssh "${H}" "cd '${R}' && sha256sum -c --quiet manifest.sha256" && echo "sync_linux.sh: ${n} files synced and verified on ${H}"
