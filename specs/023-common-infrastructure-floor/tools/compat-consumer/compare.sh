#!/usr/bin/env bash
# Spec 023 (T004a/T051): compare compat-consumer outputs between two tags in
# a directory produced by run.sh. Rule established by the T004a determinism
# probe on the baseline:
#   *.tif -> byte-identical (cmp)
#   *.rib -> identical except the "##CreationDate" header line, which
#            libRIBOut/libRIBVector stamp with the wall-clock time
#
# Usage: compare.sh <dir> <tag-a> <tag-b>
set -euo pipefail
dir="$1"; a="$2"; b="$3"
status=0
for lib in RIBRender RIBVector RIBOut; do
    for ext in tif rib; do
        fa="$dir/out-$lib-$a.$ext"; fb="$dir/out-$lib-$b.$ext"
        [ -e "$fa" ] || [ -e "$fb" ] || continue
        if [ ! -e "$fa" ] || [ ! -e "$fb" ]; then
            echo "MISSING   out-$lib-{$a,$b}.$ext"; status=1; continue
        fi
        if [ "$ext" = rib ]; then
            if diff <(grep -v '^##CreationDate' "$fa") <(grep -v '^##CreationDate' "$fb") >/dev/null; then
                echo "IDENTICAL out-$lib ($ext, ##CreationDate excluded)"
            else
                echo "DIFFERS   out-$lib ($ext)"; status=1
            fi
        elif cmp -s "$fa" "$fb"; then
            echo "IDENTICAL out-$lib ($ext, bytes)"
        else
            echo "DIFFERS   out-$lib ($ext)"; status=1
        fi
    done
done
exit $status
