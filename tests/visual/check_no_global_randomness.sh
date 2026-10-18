#!/bin/sh
# Spec 039, SC-007 (quickstart section 9): no source file outside the offline
# tool src/precomp/ draws random numbers except through the sampler library
# (src/common/sampling.*) or a CKeyedStream. The generators this spec retires
# are searched for by name: _urand, _irand, random_device, rand(), mt19937,
# CSobol<, urand(), irand().
#
# Matches inside comments do not count: a line whose code part (before any //)
# no longer matches, and a line that starts inside a block comment (a leading
# '*' or '/*'), are skipped.
#
# Usage: tests/visual/check_no_global_randomness.sh [source-root]   (default: .)
# Exit status 0 when nothing is found, 1 otherwise (the matches are printed).
#
# NOT registered with ctest until Phase 9 removes the legacy generators (T104):
# it reports the legacy sites until then.

root="${1:-.}"
pattern='_urand|_irand|random_device|\brand\(\)|mt19937|CSobol<|urand\(\)|irand\(\)'

found=$(cd "$root" && grep -rnE "$pattern" src | grep -v '^src/precomp/' | awk -F: '
{
    file = $1; line = $2
    code = substr($0, length(file) + length(line) + 3)
    if (code ~ /^[ \t]*(\*|\/\*|\/\/)/) next
    sub(/\/\/.*$/, "", code)
    if (code ~ /_urand|_irand|random_device|[^A-Za-z0-9_]rand\(\)|^rand\(\)|mt19937|CSobol<|urand\(\)|irand\(\)/)
        print file ":" line ":" substr($0, length(file) + length(line) + 3)
}')

if [ -n "$found" ]; then
    echo "$found"
    echo "check_no_global_randomness: $(echo "$found" | wc -l | tr -d ' ') match(es) outside comments"
    exit 1
fi
echo "check_no_global_randomness: no matches outside comments"
exit 0
