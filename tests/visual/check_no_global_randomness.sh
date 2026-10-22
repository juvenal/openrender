#!/bin/sh
# Spec 039, SC-007 (quickstart section 9): no source file outside the offline
# tool src/precomp/ draws random numbers except through the sampler library
# (src/common/sampling.*) or a CKeyedStream. The generators this spec retires
# are searched for by name: _urand, _irand, random_device, rand(), mt19937,
# CSobol<, urand(), irand(), and the per-context generator members random2d,
# random3d and random4d (a use of one is a draw from the shared stream).
#
# Matches inside comments do not count: a line whose code part (before any //)
# no longer matches, and a line that starts inside a block comment (a leading
# '*' or '/*'), are skipped.
#
# Allowlist: tests/visual/no_global_randomness_allowlist.txt, one
# "path | reason | owner" line per file, for the legacy generators that stay
# until Phase 9 deletes them. The scan fails on
#   (a) a match in a file that is not on the allowlist, and
#   (b) a stale allowlist line: a file that no longer has a match,
# as archcheck does. At the end of spec 039 (T104) the allowlist is empty.
#
# Usage: tests/visual/check_no_global_randomness.sh [source-root [allowlist]]
#        (defaults: . and the allowlist next to this script)
# Exit status 0 when both checks pass, 1 otherwise.

root="${1:-.}"
allowlist="${2:-$(dirname "$0")/no_global_randomness_allowlist.txt}"
pattern='_urand|_irand|random_device|\brand\(\)|mt19937|CSobol<|urand\(\)|irand\(\)|\brandom[234]d\b'

matches=$(cd "$root" && grep -rnE "$pattern" src | grep -v '^src/precomp/' | awk -F: '
{
    file = $1; line = $2
    code = substr($0, length(file) + length(line) + 3)
    if (code ~ /^[ \t]*(\*|\/\*|\/\/)/) next
    sub(/\/\/.*$/, "", code)
    if (code ~ /_urand|_irand|random_device|[^A-Za-z0-9_]rand\(\)|^rand\(\)|mt19937|CSobol<|urand\(\)|irand\(\)|(^|[^A-Za-z0-9_])random[234]d([^A-Za-z0-9_]|$)/)
        print file ":" line ":" substr($0, length(file) + length(line) + 3)
}')

tmp=$(mktemp -d) || exit 2
trap 'rm -rf "$tmp"' EXIT
printf '%s\n' "$matches" > "$tmp/matches"

# The allowlisted paths (first field of each non-comment line), one per line.
sed -e '/^[[:space:]]*#/d' -e '/^[[:space:]]*$/d' "$allowlist" |
    awk -F'|' '{ gsub(/^[ \t]+|[ \t]+$/, "", $1); print $1 }' | sort -u > "$tmp/allowed" || exit 2
cut -d: -f1 "$tmp/matches" | sed '/^$/d' | sort -u > "$tmp/files"

status=0

# (a) matches in files that are not allowlisted
awk -F: 'NR == FNR { ok[$0] = 1; next } NF && !($1 in ok) { print }' "$tmp/allowed" "$tmp/matches" > "$tmp/unlisted" || exit 2
if [ -s "$tmp/unlisted" ]; then
    echo "Matches in files that are not on the allowlist (each must be converted or, if it is a legacy generator that stays until Phase 9, listed):"
    cat "$tmp/unlisted"
    echo "check_no_global_randomness: $(wc -l < "$tmp/unlisted" | tr -d ' ') non-allowlisted match(es) in $(cut -d: -f1 "$tmp/unlisted" | sort -u | wc -l | tr -d ' ') file(s)"
    status=1
fi

# (b) stale allowlist lines: a listed file with no match
comm -23 "$tmp/allowed" "$tmp/files" > "$tmp/stale"
if [ -s "$tmp/stale" ]; then
    echo "Stale allowlist line(s), the file has no match any more (remove the line):"
    sed 's/^/  /' "$tmp/stale"
    status=1
fi

if [ $status -eq 0 ]; then
    echo "check_no_global_randomness: no non-allowlisted matches, no stale allowlist lines"
fi
exit $status
