#!/usr/bin/env bash
# specs/048-oiio-image-io/tools/sync_linux.sh  (T003)
#
# Syncs this worktree to the spec's own tree on the Linux validation host:
#   rsync -rlpc -0 --files-from=<git ls-files -z --cached --others --exclude-standard>
# builds files.list and manifest.sha256 on the Mac, syncs, and verifies remotely
# with sha256sum -c. It does not delete anything on the host; a retired file is
# removed with --remove <path>..., which runs an explicit ssh "rm -f ${R}/<path>".
#
# Writes only to the 048 tree (R below). It refuses any other remote tree.
#
# Usage: sync_linux.sh [--remove <relative-path>...]
set -euo pipefail

H="juvenal@ubuntu-2404-desktop.v2-labs.io"
R="/home/juvenal/Projects/Development/CLI/openrender-048"
WT="${OR048_WT:-/Volumes/Projects/Development/CLI/openrender-worktrees/048-oiio-image-io}"

# Refuse to run against anything but the 048 tree.
case "$R" in
  */openrender-048) ;;
  *) echo "sync_linux: refusing remote tree $R" >&2; exit 2 ;;
esac

REMOVE=()
if [ "${1:-}" = "--remove" ]; then
  shift
  REMOVE=("$@")
fi

cd "$WT"
TMP="$(mktemp -d /tmp/or048-sync.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT

# The list: tracked, staged and untracked-not-ignored files that exist on disk.
git ls-files -z --cached --others --exclude-standard | while IFS= read -r -d '' f; do
  [ -f "$f" ] && printf '%s\0' "$f"
done > "$TMP/files.list"

# The manifest, made on the Mac (sha256, binary mode, relative paths).
xargs -0 shasum -a 256 -b < "$TMP/files.list" | sed 's/ \*/  /' > "$TMP/manifest.sha256"

ssh "${H}" "mkdir -p ${R}"
rsync -rlpc -0 --files-from="$TMP/files.list" "$WT/" "${H}:${R}/"
rsync -c "$TMP/manifest.sha256" "${H}:${R}/manifest.sha256"

for p in "${REMOVE[@]+"${REMOVE[@]}"}"; do
  case "$p" in
    /*|*..*) echo "sync_linux: bad --remove path $p" >&2; exit 2 ;;
  esac
  ssh "${H}" "rm -f ${R}/${p}"
done

ssh "${H}" "cd ${R} && sha256sum -c --quiet manifest.sha256" \
  && echo "sync_linux: $(tr -cd '\0' < "$TMP/files.list" | wc -c | tr -d ' ') files synced and verified"
