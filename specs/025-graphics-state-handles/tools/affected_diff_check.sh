#!/usr/bin/env bash
# affected_diff_check.sh <ctest -V log> [affected-diff-compare.tsv]
#
# Spec 025, T074: extracts every MaxBlockAvgDiff from a `ctest -V` log of the affected tests and
# compares it with the starting-tree column of tools/affected-diff-compare.tsv (column 2:
# "name <TAB> starting-tree values <TAB> fix values <TAB> same"). Prints one line per test that
# differs or is missing, then a summary. Exit 1 on any difference or missing test.
set -u
export LC_ALL=C
log="${1:?usage: affected_diff_check.sh <ctest -V log> [affected-diff-compare.tsv]}"
here="$(cd "$(dirname "$0")" && pwd)"
tsv="${2:-$here/affected-diff-compare.tsv}"

# index -> name from "Test #N: name ....  Passed|Failed"; values from "N: ... MaxBlockAvgDiff: x"
awk '
  match($0, /Test +#[0-9]+: [^ ]+/) {
      s = substr($0, RSTART, RLENGTH); sub(/Test +#/, "", s); split(s, a, ": "); name[a[1]] = a[2]
  }
  /^[0-9]+: .*MaxBlockAvgDiff:/ {
      idx = $1; sub(/:$/, "", idx)
      for (i = 1; i <= NF; i++) if ($i == "MaxBlockAvgDiff:") vals[idx] = vals[idx] $(i+1) ","
  }
  END { for (i in vals) if (i in name) print name[i] "\t" vals[i] }
' "$log" | sort > /tmp/affected_now.$$

bad=0; n=0
while IFS=$'\t' read -r name start fixv same; do
    n=$((n+1))
    now="$(awk -F'\t' -v t="$name" '$1==t {print $2; exit}' /tmp/affected_now.$$)"
    case "$start" in
    *[0-9]*) ;;
    *)  # the three tests that print no MaxBlockAvgDiff on the starting tree must print none now
        if [ -n "$now" ]; then echo "DIFFERS  $name: printed no diff on the starting tree, now $now"; bad=$((bad+1)); fi
        continue ;;
    esac
    if [ -z "$now" ]; then
        echo "MISSING  $name (starting tree: $start)"; bad=$((bad+1))
    elif [ "$now" != "$start" ]; then
        echo "DIFFERS  $name: starting tree $start, now $now"; bad=$((bad+1))
    fi
done < "$tsv"
rm -f /tmp/affected_now.$$
echo "$n tests compared with the starting tree; $bad differing or missing"
[ "$bad" -eq 0 ]
