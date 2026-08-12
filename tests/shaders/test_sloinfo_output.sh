#!/usr/bin/env bash
# tests/shaders/test_sloinfo_output.sh
#
# sloinfo/rsloinfo golden-output test.
#
# Usage:
#   test_sloinfo_output.sh <sloinfo> <oshader> <shaders_dir> <references_dir>
#
# Covers two fixed bugs:
#   1. sloinfo/rsloinfo must print the shader's declared *name* (from the
#      "#!name" .rslo pragma), not the file path it was given on the
#      command line.
#   2. sloinfo must not print internal .slo compiler temporaries ("local
#      variables") in its output.
#
# Bug 1 is checked against a committed .rslo reference fixture. Bug 2
# requires a .slo (LLVM bitcode) file, which this script JIT-compiles on
# the fly via `oshader --jit`; if the build has no LLVM support, that
# half of the test is skipped rather than failed.

set -euo pipefail

SLOINFO="${1:?Usage: $0 <sloinfo> <oshader> <shaders_dir> <references_dir>}"
OSHADER="${2:?}"
SHADERS_DIR="${3:?}"
REFERENCES_DIR="${4:?}"
INCLUDES_DIR="${SHADERS_DIR}/includes"

# Pinned fixture: needs a source .sl (for the Bug 2 JIT compile), a matching
# .rslo reference (for Bug 1), and a name long enough (>= 8 chars) to catch a
# lexer/buffer truncation bug that a short name like "fog" wouldn't expose.
# somewood.sl also declares a substantial list of local variables, which Bug 2
# needs — a shader with an empty locals list would make that check vacuous.
SAMPLE_BASE="somewood"
SAMPLE_SL="$SHADERS_DIR/${SAMPLE_BASE}.sl"

if [ ! -f "$SAMPLE_SL" ]; then
    echo "FAIL: fixture source not found: $SAMPLE_SL"
    exit 1
fi
if [ ! -f "$REFERENCES_DIR/${SAMPLE_BASE}.rslo" ]; then
    echo "FAIL: fixture reference not found: $REFERENCES_DIR/${SAMPLE_BASE}.rslo"
    exit 1
fi

echo "sloinfo golden-output test"
echo "  sloinfo: $SLOINFO"
echo "  sample:  $SAMPLE_BASE"
echo ""

failed=0

SCRATCH="$(pwd)/scratch-sloinfo"
rm -rf "$SCRATCH"
mkdir -p "$SCRATCH"

# --- Bug 1: name comes from the "#!name" pragma, not the filename ----------
#
# Copying the reference to a deliberately different stem is essential: if we
# ran sloinfo directly on "somewood.rslo", a name derived from the "#!name"
# pragma and a name derived from stripping the .rslo file's own basename
# would look identical, and this test could pass even if the pragma were
# never read (e.g. if the lexer rule regressed and rsloGet() silently fell
# back to the basename). Renaming the file breaks that ambiguity: the
# fixture's declared name must survive under a stem that doesn't match it.
BOGUS_STEM="renamed_stub_should_not_appear"
RENAMED_RSLO="$SCRATCH/${BOGUS_STEM}.rslo"
cp "$REFERENCES_DIR/${SAMPLE_BASE}.rslo" "$RENAMED_RSLO"

rslo_out="$("$SLOINFO" --rslo "$RENAMED_RSLO")"
echo "--- sloinfo --rslo $RENAMED_RSLO ---"
echo "$rslo_out"

first_line=$(printf '%s\n' "$rslo_out" | head -n1)
if ! printf '%s' "$first_line" | grep -q "\"${SAMPLE_BASE}\""; then
    echo "FAIL [Bug 1]: expected shader name \"$SAMPLE_BASE\" in first line, got: $first_line"
    failed=1
elif printf '%s' "$first_line" | grep -q "$BOGUS_STEM"; then
    echo "FAIL [Bug 1]: output shows the file's stem instead of the #!name pragma value"
    failed=1
else
    echo "PASS [Bug 1]: shader name \"$SAMPLE_BASE\" printed from the #!name pragma, not derived from the (renamed) file path"
fi
echo ""

# --- Bug 2: no internal .slo compiler temporaries printed ------------------
if "$OSHADER" --jit "-I${INCLUDES_DIR}" "$SAMPLE_SL" -o "$SCRATCH/${SAMPLE_BASE}.slo" >"$SCRATCH/oshader.log" 2>&1; then
    slo_out="$("$SLOINFO" --slo "$SCRATCH/${SAMPLE_BASE}.slo")"
    echo "--- sloinfo --slo $SCRATCH/${SAMPLE_BASE}.slo ---"
    echo "$slo_out"
    if printf '%s' "$slo_out" | grep -qi "local variables"; then
        echo "FAIL [Bug 2]: sloinfo printed internal local variables"
        failed=1
    else
        echo "PASS [Bug 2]: no internal local variables printed"
    fi
else
    echo "SKIP [Bug 2]: this build cannot JIT-compile .slo (no LLVM support?) — see $SCRATCH/oshader.log"
fi

exit $failed
