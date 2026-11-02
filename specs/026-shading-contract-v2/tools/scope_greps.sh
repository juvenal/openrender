#!/usr/bin/env bash
# scope_greps.sh: the scope, registry and documentation checks of spec 026 (quickstart §3; FR-014, FR-019 to FR-021,
# FR-025, FR-029; SC-002, SC-008, SC-009). Run it from anywhere inside the tree. Exit 1 if any check fails; each
# check prints PASS, FAIL or SKIP on its own line, then the failing details.
#
#   scope_greps.sh              all checks
#   scope_greps.sh --code-only  only the checks that read the working tree (no git needed: for the remote Linux
#                               tree, which has no .git)
#
# The base of every git comparison is BASE, the master commit this branch merged (spec 037 and the DLO fix edited
# the allowlist, the registry and some sources too, so comparing with the starting tree d01da47 would show their
# changes as this spec's). Override it with SCOPE_BASE=<commit>.

set -u
export LC_ALL=C
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT" || exit 2
BASE="${SCOPE_BASE:-628d6747}"
CODE_ONLY=0
[ "${1:-}" = "--code-only" ] && CODE_ONLY=1
HAVE_GIT=0
if [ "$CODE_ONLY" -eq 0 ] && git rev-parse --git-dir >/dev/null 2>&1 && git cat-file -e "$BASE^{commit}" 2>/dev/null; then
    HAVE_GIT=1
fi

FAILS=0
check() { # name, status (0 pass), details
    if [ "$2" -eq 0 ]; then
        printf 'PASS  %s\n' "$1"
    else
        printf 'FAIL  %s\n' "$1"
        [ -n "${3:-}" ] && printf '%s\n' "$3" | sed 's/^/        /'
        FAILS=$((FAILS + 1))
    fi
}
skip() { printf 'SKIP  %s (%s)\n' "$1" "$2"; }
need_git() { # name; returns 0 if git checks can run
    if [ "$HAVE_GIT" -eq 1 ]; then return 0; fi
    skip "$1" "no git or --code-only"
    return 1
}

SHADING=src/libshader/shading
REG=DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md
ALLOW=tests/architecture/allowlist.txt

# ---- code checks -----------------------------------------------------------------------------------------------------

# SC-002: the attribute reads of the eight builtins are gone; the sites that remain are research R5's, all in shading.cpp
# (a comment mentioning the expression does not count)
out=$(grep -rn 'currentObject->attributes' src/libshader | grep -v ':[0-9]*:[[:space:]]*//' | grep -v "^$SHADING/shading.cpp:")
check "currentObject->attributes is read only in shading.cpp" "$([ -z "$out" ]; echo $?)" "$out"

out=$(grep -rn 'currentObject->xform' src/libshader | grep -v ':[0-9]*:[[:space:]]*//')
check "no currentObject->xform in src/libshader" "$([ -z "$out" ]; echo $?)" "$out"

# FR-019: the executor includes no Geometry header
out=$(grep -n '#include "\(surface\|object\)\.h"' $SHADING/execute*.cpp)
check "execute*.cpp includes no surface.h or object.h" "$([ -z "$out" ]; echo $?)" "$out"

# FR-003, B2: the new headers (and the .slo loader) include only Shading/Infra headers
out=$(grep -n '#include "' $SHADING/{serviceTypes,tracer,shaderGlobals,situation,traceCalls}.h $SHADING/sloLoader.h $SHADING/sloLoader.cpp \
    | grep -v 'common/rendererc\.h' \
    | grep -E 'renderer[A-Za-z]*\.h|ri/(render|hiders|geometry|core)/|(surface|object|attributes|options)\.h"')
check "new headers and sloLoader include no renderer, Hiders or Geometry header" "$([ -z "$out" ]; echo $?)" "$out"

# FR-014: the removed methods have no caller left
out=$(grep -rnE '(->|\.)(findCoordinateSystemWithType|lookupUserOption|pixelXsamples)\(' src tests)
check "the removed service methods have no caller" "$([ -z "$out" ]; echo $?)" "$out"

# FR-029, B3: incident() reads the interior shader and opposite() the exterior one, in the interpreter and in the JIT
bad=""
n_in=$(grep -c 'DEFFUNC(Incident[A-Z]*, "incident"' $SHADING/shaderFunctions.h)
n_op=$(grep -c 'DEFFUNC(Opposite[A-Z]*, "opposite"' $SHADING/shaderFunctions.h)
[ "$n_in" -gt 0 ] || bad="$bad\nno DEFFUNC incident line found"
[ "$n_op" -gt 0 ] || bad="$bad\nno DEFFUNC opposite line found"
x=$(grep 'DEFFUNC(Incident[A-Z]*, "incident"' $SHADING/shaderFunctions.h | grep -v 'ACCESSOR_INTERIOR')
[ -z "$x" ] || bad="$bad\nincident line without ACCESSOR_INTERIOR: $x"
x=$(grep 'DEFFUNC(Opposite[A-Z]*, "opposite"' $SHADING/shaderFunctions.h | grep -v 'ACCESSOR_EXTERIOR')
[ -z "$x" ] || bad="$bad\nopposite line without ACCESSOR_EXTERIOR: $x"
jin=$(awk '/void CShadingContext::jitIncidentParameter/,/^}/' $SHADING/shading.cpp)
jop=$(awk '/void CShadingContext::jitOppositeParameter/,/^}/' $SHADING/shading.cpp)
printf '%s' "$jin" | grep -q 'ACCESSOR_INTERIOR' && ! printf '%s' "$jin" | grep -q 'ACCESSOR_EXTERIOR' || bad="$bad\njitIncidentParameter does not use ACCESSOR_INTERIOR only"
printf '%s' "$jop" | grep -q 'ACCESSOR_EXTERIOR' && ! printf '%s' "$jop" | grep -q 'ACCESSOR_INTERIOR' || bad="$bad\njitOppositeParameter does not use ACCESSOR_EXTERIOR only"
check "incident/opposite accessors agree in the interpreter and the JIT" "$([ -z "$bad" ]; echo $?)" "$(printf '%b' "$bad" | sed '/^$/d')"

# ---- registry and documentation checks (working tree) ---------------------------------------------------------------------

# FR-019: the allowlist section headers carry the new counts
db1=$(grep -E '^# --- DB-001:' $ALLOW)
db6=$(grep -E '^# --- DB-006:' $ALLOW)
bad=""
printf '%s' "$db1" | grep -q '(5) ---' || bad="$bad\nDB-001 header: $db1"
printf '%s' "$db6" | grep -q '(31) ---' || bad="$bad\nDB-006 header: $db6"
check "allowlist section headers read (5) for DB-001 and (31) for DB-006" "$([ -z "$bad" ]; echo $?)" "$(printf '%b' "$bad" | sed '/^$/d')"

# FR-020: the registry text. Take a section as the lines from its '### DB-nnn' heading to the next '### ' or '## '.
section() { awk -v h="### $1 " 'index($0, h) == 1 {f=1; print; next} f && /^##/ {exit} f' $REG; }
bad=""
s18=$(section DB-018); s6=$(section DB-006); s1=$(section DB-001)
for k in 'getAttribute' 'getMessage' '#115' '#121' 'spec 033' 'spec 034' 'spec 026'; do
    printf '%s' "$s18" | grep -qF -- "$k" || bad="$bad\nDB-018 lacks: $k"
done
for k in '31 edges' '24 include' 'Spec 026' 'no longer include' 'object.h'; do
    printf '%s' "$s6" | grep -qF -- "$k" || bad="$bad\nDB-006 lacks: $k"
done
for k in '5 edges' 'ITracer' 'Spec 026' 'tracer.h' '4 distinct'; do
    printf '%s' "$s1" | grep -qF -- "$k" || bad="$bad\nDB-001 lacks: $k"
done
check "DOMAIN_BOUNDARIES.md has the DB-018, DB-006 and DB-001 texts of the end-state contract" "$([ -z "$bad" ]; echo $?)" "$(printf '%b' "$bad" | sed '/^$/d')"

# No entry moved to Resolved: DB-001, DB-005, DB-006 and DB-018 stay before the '## Resolved' heading
res=$(grep -n '^## Resolved' $REG | head -1 | cut -d: -f1)
bad=""
for d in DB-001 DB-005 DB-006 DB-018; do
    l=$(grep -n "^### $d " $REG | head -1 | cut -d: -f1)
    if [ -z "$l" ] || [ -z "$res" ] || [ "$l" -gt "$res" ]; then bad="$bad\n$d is not in the Open part"; fi
done
check "DB-001, DB-005, DB-006 and DB-018 are still Open" "$([ -z "$bad" ]; echo $?)" "$(printf '%b' "$bad" | sed '/^$/d')"

# DEVNOTES.md has the spec 026 row
grep -q 'specs/026-shading-contract-v2/' DEVNOTES.md
check "DEVNOTES.md has the spec 026 row" $? ""

# validation.md §4 holds the classification and the seven filed issues
bad=""
grep -q '^### §4.1 Classification' specs/026-shading-contract-v2/validation.md || bad="$bad\nno §4.1 Classification"
for n in "O1 (#115)" "O2 (#116)" "O3 (#117)" "O4 (#118)" "O5 (#119)" "O6 (#120)" "O7 (#121)"; do
    grep -qF "#### $n" specs/026-shading-contract-v2/validation.md || bad="$bad\nno issue text for $n"
done
check "validation.md §4 has the classification and the text of #115 to #121" "$([ -z "$bad" ]; echo $?)" "$(printf '%b' "$bad" | sed '/^$/d')"

# ---- git checks (against BASE) -------------------------------------------------------------------------------------------

if need_git "allowlist: only the two header counts added, exactly the five lines deleted"; then
    added=$(git diff "$BASE" -- $ALLOW | grep '^+[^+]')
    deleted=$(git diff "$BASE" -- $ALLOW | grep '^-[^-]' | grep -v '^-# --- DB-00[16]:')
    bad=""
    [ "$(printf '%s\n' "$added" | sed '/^$/d' | wc -l | tr -d ' ')" = "2" ] || bad="$bad\nadded lines are not exactly 2:\n$added"
    printf '%s\n' "$added" | sed '/^$/d' | grep -vqE '^\+# --- DB-00[16]: .*\((5|31)\) ---$' && bad="$bad\nan added line is not a header count: $added"
    for k in 'executeMisc.cpp  ->  src/ri/geometry/surface.h' 'execute.cpp  ->  src/ri/geometry/surface.h' \
             'executeMisc.cpp  ->  _ZN15CShadingContext5traceEP4CRay' 'executeMisc.cpp  ->  _ZN15CShadingContext7traceExEP10CRayBundle' \
             'shading.cpp  ->  _ZN15CShadingContext7traceExEP10CRayBundle'; do
        printf '%s\n' "$deleted" | grep -qF -- "$k" || bad="$bad\nnot deleted: $k"
    done
    [ "$(printf '%s\n' "$deleted" | sed '/^$/d' | wc -l | tr -d ' ')" = "5" ] || bad="$bad\ndeleted lines are not exactly 5:\n$deleted"
    check "allowlist: only the two header counts added, exactly the five lines deleted" "$([ -z "$bad" ]; echo $?)" "$(printf '%b' "$bad" | sed '/^$/d')"
fi

if need_git "registry: the lines this spec adds do not depend on the merge"; then
    out=$(git diff "$BASE" -- $REG | grep '^+' | grep -v '^+++' | grep -iE '\bmerge[ds]?\b|\b[0-9a-f]{7,40}\b')
    check "registry: the lines this spec adds do not depend on the merge" "$([ -z "$out" ]; echo $?)" "$out"
fi

if need_git "registry: DB-005 is not edited"; then
    a=$(git show "$BASE:$REG" | awk 'index($0,"### DB-005 ")==1{f=1;print;next} f&&/^##/{exit} f')
    b=$(section DB-005)
    check "registry: DB-005 is not edited" "$([ "$a" = "$b" ]; echo $?)" ""
fi

if need_git "files 026 never edits are unchanged"; then
    out=$(git diff --stat "$BASE" -- src/ri/hiders/trace.cpp 'src/ri/hiders/bundles.*' 'src/ri/hiders/photonMap.*' 'src/ri/hiders/raytracer.*' \
        'src/ri/hiders/reyes.*' 'src/ri/hiders/stochastic.*' 'src/ri/hiders/zbuffer.*' src/ri/hiders/photon.cpp \
        src/ri/geometry src/ri/state src/ri/texture src/display/dsply.h src/procedures src/shadeop src/include)
    check "files 026 never edits are unchanged (hiders, Geometry, State, Texturing, plug-in ABI headers)" "$([ -z "$out" ]; echo $?)" "$out"
    # CLAUDE.md is edited only as the maintainer authorized (2026-11-01): the lines added are exactly those of
    # claude-md-026.diff, none is removed, so the "Spec-kit agent rules" section is untouched
    want=$(grep '^+[^+]' specs/026-shading-contract-v2/tools/claude-md-026.diff)
    got=$(git diff "$BASE" -- CLAUDE.md | grep '^+[^+]')
    gone=$(git diff "$BASE" -- CLAUDE.md | grep '^-[^-]')
    check "CLAUDE.md differs from the base only by the authorized diff" "$([ "$want" = "$got" ] && [ -z "$gone" ]; echo $?)" "$gone"
    # The JIT emitter is the one source with maintainer-approved exceptions (D8, D9 and D10): its diff is listed, not failed.
    e=$(git diff --stat "$BASE" -- src/libshader/compiler/llvmEmitter.cpp | tail -1)
    printf 'INFO  llvmEmitter.cpp differs only by the approved D8, D9 and D10 edits: %s\n' "${e:-no difference}"
fi

echo
if [ "$FAILS" -eq 0 ]; then
    echo "scope_greps: all checks passed"
    exit 0
fi
echo "scope_greps: $FAILS check(s) failed"
exit 1
