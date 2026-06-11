#!/usr/bin/env bash
# tests/shaders/test_shader_compile.sh
#
# Shader compiler immutability test.
#
# Usage:
#   test_shader_compile.sh <oshader> <shaders_dir> <references_dir>
#
# For each .sl file in <shaders_dir>, compiles it with oshader and compares
# the resulting .rslo against the reference copy in <references_dir>.
# Exits 0 if all compiled outputs match their references, non-zero otherwise.
#
# Reference update workflow:
#   When a planned compiler change is made:
#     1. Run the test — it fails, showing unified diffs
#     2. Verify every diff is exactly the expected change
#     3. Copy the new .rslo files from the scratch dir to <references_dir>
#     4. Commit the updated references with a message describing the compiler change
#
# The scratch directory for this run is printed on startup so you can inspect
# individual outputs.

set -euo pipefail

OSHADER="${1:?Usage: $0 <oshader> <shaders_dir> <references_dir>}"
SHADERS_DIR="${2:?}"
REFERENCES_DIR="${3:?}"
INCLUDES_DIR="${SHADERS_DIR}/includes"

# Working directory (set by CTest) is CMAKE_BINARY_DIR/tests/shaders
SCRATCH="$(pwd)/scratch"
rm -rf "$SCRATCH"
mkdir -p "$SCRATCH"

echo "Shader compiler immutability test"
echo "  oshader:    $OSHADER"
echo "  shaders:    $SHADERS_DIR"
echo "  references: $REFERENCES_DIR"
echo "  scratch:    $SCRATCH"
echo ""

passed=0
failed=0
missing_ref=0

for sl in "$SHADERS_DIR"/*.sl; do
    base=$(basename "$sl" .sl)
    out="$SCRATCH/${base}.rslo"
    ref="$REFERENCES_DIR/${base}.rslo"

    # Compile the shader
    compile_ok=1
    "$OSHADER" "-I${INCLUDES_DIR}" "$sl" -o "$out" 2>/dev/null || compile_ok=0

    if [ $compile_ok -eq 0 ]; then
        echo "FAIL [compile error]  $base"
        ((failed++)) || true
        continue
    fi

    # Check that output was actually written
    if [ ! -f "$out" ]; then
        echo "FAIL [no output]      $base"
        ((failed++)) || true
        continue
    fi

    # Check reference exists
    if [ ! -f "$ref" ]; then
        echo "MISS [no reference]   $base  (copy $out to $REFERENCES_DIR/ to establish reference)"
        ((missing_ref++)) || true
        continue
    fi

    # Text diff: .rslo files are plain text
    if diff -u "$ref" "$out" > "$SCRATCH/${base}.diff" 2>&1; then
        echo "PASS                  $base"
        ((passed++)) || true
    else
        echo "FAIL [output differs] $base"
        echo "--- diff follows ---"
        cat "$SCRATCH/${base}.diff"
        echo "--- end diff ---"
        ((failed++)) || true
    fi
done

echo ""
echo "Results: ${passed} passed, ${failed} failed, ${missing_ref} missing reference(s)"

if [ $missing_ref -gt 0 ]; then
    echo ""
    echo "To establish missing references, copy the compiled outputs:"
    echo "  cp ${SCRATCH}/*.rslo ${REFERENCES_DIR}/"
    echo "Then commit the new reference files."
fi

[ $failed -eq 0 ] && [ $missing_ref -eq 0 ]
