#!/bin/bash
# Comprehensive debugging script for orender skeleton TIFF issue

RENDER_OPTS="${OPTS}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${SCRIPT_DIR}" || exit 1

export DYLD_LIBRARY_PATH="${DYLD_LIBRARY_PATH}:${PWD}/openrender/lib"
export LD_LIBRARY_PATH="${LD_LIBRARY_PATH}:${PWD}/openrender/lib"
export ORENDERHOME="${PWD}/openrender"
export DISPLAYS="${PWD}/openrender/displays"
export GEOMETRIES="${PWD}/openrender/geometry"
export SHADERS="${PWD}/openrender/shaders:."

ORENDER_BIN="${PWD}/openrender/bin/orender"
RIB_FILE="${1:-examples/rib/quadrics-b.rib}"

echo "=== Debugging orender Skeleton TIFF Issue ==="
echo ""
echo "Environment:"
echo "  LD_LIBRARY_PATH=${LD_LIBRARY_PATH}"
echo "  ORENDERHOME=${ORENDERHOME}"
echo "  DISPLAYS=${DISPLAYS}"
echo "  GEOMETRIES=${GEOMETRIES}"
echo "  SHADERS=${SHADERS}"
echo ""
echo "Checking display driver module..."
if [ -f "${PWD}/openrender/displays/libfile.so" ]; then
    echo "  ✓ Found libfile.so"
elif [ -f "${PWD}/openrender/displays/file.so" ]; then
    echo "  ✓ Found file.so"
else
    echo "  ✗ Display driver module not found in ${PWD}/openrender/displays/"
    echo "    Looking for: libfile.so or file.so"
    echo ""
    echo "  Available files in displays directory:"
    ls -la "${PWD}/openrender/displays/" 2>/dev/null || echo "    Directory does not exist!"
fi
echo ""

echo "Checking RIB file: ${RIB_FILE}"
if [ ! -f "${RIB_FILE}" ]; then
    echo "  ✗ RIB file not found: ${RIB_FILE}"
    exit 1
fi
echo "  ✓ RIB file found"
echo ""

echo "Checking for Format and Display statements in RIB file..."
if grep -q "^Format" "${RIB_FILE}"; then
    echo "  ✓ Format statement found"
    grep "^Format" "${RIB_FILE}"
else
    echo "  ✗ Format statement NOT found"
fi

if grep -q "^Display" "${RIB_FILE}"; then
    echo "  ✓ Display statement found"
    grep "^Display" "${RIB_FILE}"
else
    echo "  ✗ Display statement NOT found"
fi
echo ""

echo "Checking for .orenderrc..."
DEFAULTS_RIB="${ORENDERHOME}/.orenderrc"
if [ -f "${DEFAULTS_RIB}" ]; then
    echo "  ✓ Found .orenderrc"
    echo "  Contents:"
    cat "${DEFAULTS_RIB}" | sed 's/^/    /'
else
    echo "  ✗ .orenderrc not found at ${DEFAULTS_RIB}"
fi
echo ""

echo "=== Running orender with debug output ==="
echo "Command: ${ORENDER_BIN} ${RENDER_OPTS} ${RIB_FILE}"
echo ""

# Run with debug output and framebuffer display
"${ORENDER_BIN}" ${RENDER_OPTS} "${RIB_FILE}" 2>&1 | tee /tmp/orender_debug.log

echo ""
echo "=== Debug output saved to /tmp/orender_debug.log ==="
echo ""
echo "Checking output files..."
find . -name "*.tif" -o -name "*.tiff" -newer "${RIB_FILE}" 2>/dev/null | while read f; do
    if [ -f "$f" ]; then
        SIZE=$(stat -c%s "$f" 2>/dev/null || stat -f%z "$f" 2>/dev/null)
        echo "  Found: $f (size: $SIZE bytes)"
        if [ "$SIZE" -lt 1000 ]; then
            echo "    ⚠ WARNING: File is very small, likely a skeleton TIFF"
        fi
    fi
done

echo ""
echo "=== Analysis ==="
echo "Look for these debug messages in the output:"
echo "  [DEBUG computeDisplayData] - Shows if displays are found"
echo "  [DEBUG] Processing display - Shows each display being processed"
echo "  [DEBUG] Looking for display driver - Shows if driver module is being searched"
echo "  [DEBUG displayStart] - Shows if displayStart is called and what it returns"
echo "  [DEBUG] HIDER_BREAK - Shows if rendering is being aborted"
echo ""
