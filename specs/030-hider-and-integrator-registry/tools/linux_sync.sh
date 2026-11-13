#!/usr/bin/env bash
# linux_sync.sh: the maintainer's sync recipe for spec 030's Linux validation (GCC 13, -Werror).
#
#   linux_sync.sh push    [--dry-run]
#       Build the file list with `git ls-files -z --cached` (tracked or staged: every new file
#       is `git add`ed before a push), drop entries that no longer exist on disk, and send it
#       with `rsync -rlpc --from0 --files-from=<list>`. It aborts, listing them, when any
#       untracked non-ignored file exists, so nothing is silently skipped. Never `-a`, `-t`
#       or `--delete`. tests/shading_contract/replay/linux.txt is a tracked reference and IS
#       pushed. Only the Linux-produced files are excluded from the list:
#           specs/030-hider-and-integrator-registry/tools/*-linux*
#       A file that an earlier push sent and the list no longer holds is removed from the
#       remote, using the remote's `.sync-manifest` of the previous push.
#   linux_sync.sh verify
#       shasum -a 256 of every listed file locally against sha256sum remotely. Fails on any
#       difference, and on a remote `.git`. Prints "N files, D differences".
#   linux_sync.sh pull
#       Copy the Linux-only files back (rsync -rlpc), then verify them the same way.
#   linux_sync.sh --dry-run | push --dry-run
#       Print the file list and run nothing remote.
#
# The remote root is hard-coded. The script refuses any other, and refuses a root that ends
# in /openrender, /cellar, /shrimp or /openrender-0NN (another agent's tree). Always "${H}:path", never $H:path
# (zsh's :P modifier copies locally and exits 0); this script is bash.

set -u
export LC_ALL=C

H="${H:-juvenal@ubuntu-2404-desktop.v2-labs.io}"
RR_FIXED="Projects/Development/CLI/openrender-030"
RR="${RR:-$RR_FIXED}"

die() { echo "linux_sync: $*" >&2; exit 2; }

[ "$RR" = "$RR_FIXED" ] || die "remote root must be $RR_FIXED (got $RR)"
case "$RR" in
*/openrender|*/cellar|*/shrimp) die "refusing remote root $RR" ;;
*/openrender-030) ;;
*/openrender-0*) die "refusing another spec's remote root $RR" ;;
esac
case "$H" in
juvenal@ubuntu-2404-desktop.v2-labs.io) ;;
*) die "refusing host $H" ;;
esac

here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../../.." && pwd)"
cd "$root" || die "cannot cd to $root"
[ -f specs/030-hider-and-integrator-registry/spec.md ] || die "not the spec 030 worktree: $root"

LINUX_ONLY_TOOLS_GLOB="specs/030-hider-and-integrator-registry/tools/*-linux*"

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

is_linux_only() {
    case "$1" in
    specs/030-hider-and-integrator-registry/tools/*-linux*) return 0 ;;
    esac
    return 1
}

# the list of files to send, NUL separated: tracked or staged, present on disk, not Linux-only,
# not a build tree. Aborts when an untracked non-ignored file exists.
make_list() {
    local out="$1" untracked
    untracked="$(git ls-files --others --exclude-standard)"
    if [ -n "$untracked" ]; then
        echo "linux_sync: untracked non-ignored files (git add them first):" >&2
        echo "$untracked" >&2
        exit 3
    fi
    : > "$out"
    git ls-files -z --cached | while IFS= read -r -d '' f; do
        [ -e "$f" ] || [ -L "$f" ] || continue
        is_linux_only "$f" && continue
        case "$f" in
        build|build/*|build-*|.git|.git/*) continue ;;
        esac
        printf '%s\0' "$f"
    done | sort -z > "$out"
}

# the Linux-only files that exist locally, NUL separated
local_linux_only() {
    local out="$1"
    : > "$out"
    for f in $LINUX_ONLY_TOOLS_GLOB; do
        [ -f "$f" ] && printf '%s\0' "$f" >> "$out"
    done
}

ssh_run() { ssh -o BatchMode=yes "$H" "$@"; }

hash_local() {   # stdin: NUL list; stdout: "hash  path" sorted
    xargs -0 shasum -a 256 2>/dev/null | sort -k2
}

# compare a NUL list locally and remotely; prints "N files, D differences"
compare_list() {
    local list="$1" n d
    n="$(tr -cd '\0' < "$list" | wc -c | tr -d ' ')"
    if [ "$n" -eq 0 ]; then echo "0 files, 0 differences"; return 0; fi
    hash_local < "$list" > "$tmp/local.sha"
    ssh_run "cd $RR && xargs -0 sha256sum 2>&1" < "$list" | sort -k2 > "$tmp/remote.sha"
    d="$(diff "$tmp/local.sha" "$tmp/remote.sha" | grep -c '^[<>]' || true)"
    if [ "$d" -ne 0 ]; then
        diff "$tmp/local.sha" "$tmp/remote.sha" | head -40 >&2
    fi
    echo "$n files, $d differences"
    [ "$d" -eq 0 ]
}

cmd="${1:-}"
dry=0
[ "${2:-}" = "--dry-run" ] && dry=1
[ "$cmd" = "--dry-run" ] && { cmd=push; dry=1; }

case "$cmd" in
push)
    make_list "$tmp/list"
    n="$(tr -cd '\0' < "$tmp/list" | wc -c | tr -d ' ')"
    if [ "$dry" -eq 1 ]; then
        tr '\0' '\n' < "$tmp/list"
        echo "linux_sync: dry run, $n files, nothing sent" >&2
        exit 0
    fi
    ssh_run "mkdir -p $RR" || die "cannot create the remote root"
    rsync -rlpc --from0 --files-from="$tmp/list" ./ "${H}:$RR/" || die "rsync failed"
    # remove what the previous push sent and this list no longer holds
    ssh_run "cat $RR/.sync-manifest 2>/dev/null" | sort > "$tmp/prev.s" || true
    if [ -s "$tmp/prev.s" ]; then
        tr '\0' '\n' < "$tmp/list" | sort > "$tmp/cur.s"
        comm -23 "$tmp/prev.s" "$tmp/cur.s" > "$tmp/stale"
        if [ -s "$tmp/stale" ]; then
            echo "linux_sync: removing from the remote:" >&2
            cat "$tmp/stale" >&2
            # never a Linux-only file, never outside the tree
            while IFS= read -r f; do
                is_linux_only "$f" && continue
                case "$f" in /*|*..*) continue ;; esac
                printf '%s\0' "$f"
            done < "$tmp/stale" | ssh_run "cd $RR && xargs -0 rm -f --"
        fi
    fi
    tr '\0' '\n' < "$tmp/list" | ssh_run "cat > $RR/.sync-manifest"
    echo "linux_sync: pushed $n files"
    ;;
verify)
    make_list "$tmp/list"
    if ssh_run "test -e $RR/.git"; then
        echo "linux_sync: the remote tree has a .git" >&2
        exit 1
    fi
    compare_list "$tmp/list"
    ;;
pull)
    # enumerate the Linux-only files on the remote
    ssh_run "cd $RR && { ls $LINUX_ONLY_TOOLS_GLOB 2>/dev/null; }" \
        | tr '\n' '\0' > "$tmp/pull"
    if [ ! -s "$tmp/pull" ]; then
        echo "linux_sync: no Linux-only files on the remote" >&2
        exit 1
    fi
    mkdir -p specs/030-hider-and-integrator-registry/tools
    rsync -rlpc --from0 --files-from="$tmp/pull" "${H}:$RR/" ./ || die "rsync failed"
    compare_list "$tmp/pull"
    ;;
*)
    sed -n '2,24p' "$0" | sed 's/^# \{0,1\}//' >&2
    exit 2
    ;;
esac
