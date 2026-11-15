#!/usr/bin/env bash
# specs/048-oiio-image-io/tools/vcpkg_shared_guard.sh  (T002)
#
# Proves that a vcpkg or configure run of THIS spec did not touch the shared install
# tree every other worktree uses (~/.cache/vcpkg-installed/openrender).
#
#   vcpkg_shared_guard.sh before   record the checksums of the shared tree's
#                                  vcpkg/status and vcpkg/manifest-info.json and the
#                                  manifest path the latter names
#   vcpkg_shared_guard.sh after    exit 1 with a loud message when
#                                    - vcpkg/status changed (an install happened in the
#                                      shared tree: nothing legitimate does that here), or
#                                    - manifest-info.json changed and the manifest path it
#                                      names is this spec's own worktree, or is not one of
#                                      <worktrees>/*/vcpkg.json or the main checkout's
#                                      vcpkg.json.
#                                  A change that names ANOTHER spec's worktree is that
#                                  spec's own configure (every worktree configures against
#                                  the shared tree and rewrites this file); it is reported
#                                  as "writer: <path>" and passes (coordinator decision,
#                                  2026-11-15, after M0 saw spec 057's configure).
#
# The shared tree is only ever READ: nothing here writes under $S. The only files
# written are the records in /tmp.
set -u

S="${OR048_SHARED_TREE:-$HOME/.cache/vcpkg-installed/openrender}"
WTS="${OR048_WORKTREES:-/Volumes/Projects/Development/CLI/openrender-worktrees}"
SELF="${OR048_WT:-$WTS/048-oiio-image-io}"
MAINS=("/Volumes/Projects/Development/CLI/openrender" "/Users/juvenal/Projects/Development/CLI/openrender")
SUM=/tmp/or048-shared-before.sha
PATHREC=/tmp/or048-shared-before.path

mpath() { sed -n 's/.*"manifest-path"[[:space:]]*:[[:space:]]*"\(.*\)".*/\1/p' "$S/vcpkg/manifest-info.json"; }
die() {
  echo "##########################################################" >&2
  echo "# vcpkg_shared_guard: THE SHARED VCPKG TREE CHANGED. STOP.  #" >&2
  echo "# $1" >&2
  echo "# Report to the coordinator; do not continue.               #" >&2
  echo "##########################################################" >&2
  exit 1
}

case "${1:-}" in
  before)
    for f in "$S/vcpkg/status" "$S/vcpkg/manifest-info.json"; do
      [ -f "$f" ] || { echo "vcpkg_shared_guard: $f does not exist" >&2; exit 2; }
    done
    shasum "$S/vcpkg/status" "$S/vcpkg/manifest-info.json" > "$SUM" || exit 2
    mpath > "$PATHREC"
    echo "vcpkg_shared_guard: recorded (manifest-path: $(cat "$PATHREC"))"; cat "$SUM"
    ;;
  after)
    [ -f "$SUM" ] || { echo "vcpkg_shared_guard: no 'before' record at $SUM" >&2; exit 2; }
    cur_status="$(shasum "$S/vcpkg/status" | cut -d' ' -f1)"
    old_status="$(grep "/vcpkg/status\$" "$SUM" | cut -d' ' -f1)"
    [ "$cur_status" = "$old_status" ] || die "vcpkg/status changed (an install happened in the shared tree)"
    cur_mi="$(shasum "$S/vcpkg/manifest-info.json" | cut -d' ' -f1)"
    old_mi="$(grep "/vcpkg/manifest-info.json\$" "$SUM" | cut -d' ' -f1)"
    if [ "$cur_mi" = "$old_mi" ]; then
      echo "vcpkg_shared_guard: shared tree untouched (status and manifest-info.json identical)"
      exit 0
    fi
    w="$(mpath)"
    echo "vcpkg_shared_guard: manifest-info.json changed; writer: ${w:-<unparsable>} (before: $(cat "$PATHREC" 2>/dev/null))"
    case "$w" in
      "$SELF"/*|"") die "manifest-info.json names this spec's own worktree (or nothing): '$w'" ;;
    esac
    ok=no
    case "$w" in
      "$WTS"/*/vcpkg.json) ok=yes ;;
    esac
    for m in "${MAINS[@]}"; do [ "$w" = "$m/vcpkg.json" ] && ok=yes; done
    [ "$ok" = yes ] || die "manifest-info.json names an unknown manifest: '$w'"
    echo "vcpkg_shared_guard: PASS (the change is another worktree's own configure; vcpkg/status identical)"
    ;;
  *)
    echo "usage: $0 before|after" >&2
    exit 2
    ;;
esac
