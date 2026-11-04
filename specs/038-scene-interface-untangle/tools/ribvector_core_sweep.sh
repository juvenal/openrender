#!/bin/sh
# The nm sweep of research M5 (spec 038): which Core files does the ribVector build need, and who needs them.
#
#   ribvector_core_sweep.sh <build-dir> [source-dir]
#
# Reads the built objects of ribVector_obj (build/src/ri/CMakeFiles/ribVector_obj.dir) and of libribpreview
# (build/src/preview/libribpreview/CMakeFiles/ribpreview.dir). A "Core object" is a ribVector object under
# render/ or vector/ (tests/architecture/domain-map.txt classifies src/ri/render/ and src/ri/vector/ as Core).
# For every Core object it prints the objects that reference one of its defined symbols, with the referencing
# object's domain, and then the RESIDUAL list: the Core files that remain once everything is followed
# from a non-Core referencer (a Core file referenced only by another Core file in the list is included).
#
# Output sections (stable, sorted; diff two runs to compare):
#   REFERENCED-BY <core file>  <- <referencing file> [<domain>] (<n> symbols)
#   RESIDUAL <core file>  needed by: <non-Core referencing files, or the Core file that needs it>
#   UNREFERENCED <core file>   (compiled into ribVector, referenced by nothing in ribVector or the preview)
# Read-only; takes a few seconds. Run through cpujob like every tool run.
set -e
BUILD="${1:?usage: ribvector_core_sweep.sh <build-dir> [source-dir]}"
VEC="$BUILD/src/ri/CMakeFiles/ribVector_obj.dir"
PRE="$BUILD/src/preview/libribpreview/CMakeFiles/ribpreview.dir"
[ -d "$VEC" ] || { echo "no $VEC (build the tree first)" >&2; exit 2; }
[ -d "$PRE" ] || { echo "no $PRE (build the tree first)" >&2; exit 2; }
WORK="${TMPDIR:-/tmp}/ribvector_core_sweep.$$"
mkdir -p "$WORK"
trap 'rm -rf "$WORK"' EXIT

case "$(uname -s)" in
    Darwin) NM_DEFINED="nm -g -U" ;;
    *)      NM_DEFINED="nm -g --defined-only" ;;
esac

# Normalised file name of an object: its path under the object directory without ".o"
# (render/rendererStatics.cpp, tessellators/tessPolygon.cpp).
objname() { echo "$1" | sed -e "s#^$2/##" -e 's#\.o$##'; }

# All objects: list.txt holds "<kind> <name> <path>" with kind vec or pre
: > "$WORK/list.txt"
find "$VEC" -name '*.o' | sort | while read -r o; do
    echo "vec $(objname "$o" "$VEC") $o" >> "$WORK/list.txt"
done
find "$PRE" -name '*.o' | sort | while read -r o; do
    echo "pre preview/$(objname "$o" "$PRE") $o" >> "$WORK/list.txt"
done

domain_of() { # <kind> <name>
    case "$1:$2" in
        pre:*) echo Consumer-preview ;;
        vec:render/*|vec:vector/*) echo Core ;;
        vec:parse/*|vec:output/*|vec:rib_generated*|vec:lex.rib*|vec:lex.if*|vec:ifexpr_generated*) echo SceneInterface ;;
        vec:state/*) echo GraphicsState ;;
        vec:geometry/*) echo Geometry ;;
        vec:hiders/*|vec:hcshader*) echo Hiders ;;
        vec:texture/*) echo Texturing ;;
        vec:dataviewer/*) echo Consumer-dataviewer ;;
        *) echo Other ;;
    esac
}

# defined.txt: "<core name> <symbol>" for every Core object; undefined.txt: "<name> <symbol>" for every object
: > "$WORK/defined.txt"; : > "$WORK/undefined.txt"
while read -r kind name path; do
    dom=$(domain_of "$kind" "$name")
    if [ "$dom" = Core ]; then
        $NM_DEFINED "$path" 2>/dev/null | awk -v n="$name" '{ if (NF==3) print n, $3 }' >> "$WORK/defined.txt"
    fi
    nm -g "$path" 2>/dev/null | awk -v n="$name" '{ t=""; s="";
        if (NF==2) { t=$1; s=$2 } else if (NF==3) { t=$2; s=$3 }
        if (t=="U") print n, s }' >> "$WORK/undefined.txt"
done < "$WORK/list.txt"

# join: symbol -> defining Core object (first definition wins)
awk '{ if (!($2 in d)) d[$2]=$1 } END { for (s in d) print s, d[s] }' "$WORK/defined.txt" | sort > "$WORK/symowner.txt"
# refs.txt: "<core> <referencer> <count>" (a Core object never counts as its own referencer)
awk 'NR==FNR { owner[$1]=$2; next }
     ($2 in owner) && owner[$2] != $1 { c[owner[$2] " " $1]++ }
     END { for (k in c) print k, c[k] }' "$WORK/symowner.txt" "$WORK/undefined.txt" | sort > "$WORK/refs.txt"

kind_of() { awk -v n="$1" '$2==n { print $1; exit }' "$WORK/list.txt"; }

awk '$1!="" { print }' "$WORK/list.txt" | while read -r kind name path; do
    [ "$(domain_of "$kind" "$name")" = Core ] || continue
    if ! grep -q "^$name " "$WORK/refs.txt" 2>/dev/null; then
        echo "UNREFERENCED $name"
        continue
    fi
    grep "^$name " "$WORK/refs.txt" | while read -r core ref n; do
        k=$(kind_of "$ref")
        echo "REFERENCED-BY $core  <- $ref [$(domain_of "$k" "$ref")] ($n symbols)"
    done
done | sort > "$WORK/report.txt"
cat "$WORK/report.txt"

# RESIDUAL: Core files referenced by a non-Core object, then followed through Core-to-Core references
: > "$WORK/resid.txt"
grep '^REFERENCED-BY' "$WORK/report.txt" | grep -v '\[Core\]' | awk '{ print $2 }' | sort -u >> "$WORK/resid.txt"
n0=0
while :; do
    grep '\[Core\]' "$WORK/report.txt" | awk '{ print $2, $4 }' | while read -r c r; do
        grep -qx "$r" "$WORK/resid.txt" && echo "$c" >> "$WORK/resid.txt"
    done
    sort -u "$WORK/resid.txt" -o "$WORK/resid.txt"
    n1=$(wc -l < "$WORK/resid.txt")
    [ "$n1" = "$n0" ] && break
    n0=$n1
done
echo "--- RESIDUAL ($(wc -l < "$WORK/resid.txt" | tr -d ' ') Core files)"
while read -r f; do
    by=$(grep "^REFERENCED-BY $f " "$WORK/report.txt" | sed -e 's/^[^<]*<- //' -e 's/ (.*//' | tr '\n' ',' | sed 's/,$//')
    echo "RESIDUAL $f  needed by: $by"
done < "$WORK/resid.txt"
