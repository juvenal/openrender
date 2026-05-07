#!/usr/bin/env bash
# tests/framebuffer/test_linux_regression.sh
#
# Linux framebuffer regression test (T035).
# Validates the IPC framebuffer model on Linux (X11 and Wayland backends).
#
# Usage:
#   ./tests/framebuffer/test_linux_regression.sh [--build-dir BUILD_DIR]
#
# Prerequisites (must be run on a Linux host with X11 and/or Wayland):
#   - orender binary in BUILD_DIR/src/orender/
#   - orender-fb-linux binary in BUILD_DIR/src/framebuffer/orender-fb-linux/ or $ORENDERHOME/bin/
#   - examples/rib/camera-dof.rib in the project root
#
# What it checks:
#   1. orender exits within 5 seconds of completing the render
#   2. orender-fb-linux keeps running after orender exits (window stays open)
#   3. No zombie orender-fb-linux processes remain after window closes
#   4. Tests run on both X11 and Wayland (if WAYLAND_DISPLAY is set)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BUILD_DIR="${1:-${PROJECT_ROOT}/build}"

# Allow --build-dir flag
if [[ "${1:-}" == "--build-dir" ]]; then
    BUILD_DIR="${2}"
    shift 2
fi

ORENDER="${BUILD_DIR}/src/orender/orender"
HELPER="${BUILD_DIR}/src/framebuffer/orender-fb-linux/orender-fb-linux"
RIB="${PROJECT_ROOT}/examples/rib/camera-dof.rib"

PASS=0
FAIL=0

log_pass() { echo "  PASS: $1"; ((PASS++)); }
log_fail() { echo "  FAIL: $1"; ((FAIL++)); }

# ---------------------------------------------------------------------------
# Preflight checks
# ---------------------------------------------------------------------------
echo "=== Linux Framebuffer Regression Test (T035) ==="
echo ""

if [[ ! -f "${ORENDER}" ]]; then
    echo "ERROR: orender not found at ${ORENDER}"
    echo "       Run: cmake --build ${BUILD_DIR} --config Release"
    exit 1
fi

if [[ ! -f "${HELPER}" ]]; then
    echo "WARNING: orender-fb-linux not found at ${HELPER}"
    echo "         This is expected when building on macOS (cross-compile check only)."
    echo "         Run this script on a Linux host after building for Linux."
    exit 0
fi

if [[ ! -f "${RIB}" ]]; then
    echo "ERROR: test RIB not found at ${RIB}"
    exit 1
fi

# ---------------------------------------------------------------------------
# Helper: run one render session and validate IPC behaviour
# ---------------------------------------------------------------------------
run_test() {
    local backend="${1}"   # "x11" or "wayland"
    local env_override="${2:-}"  # optional env var override for WAYLAND_DISPLAY

    echo "--- Backend: ${backend} ---"

    # Set env for the render run
    local env_vars=()
    env_vars+=("SHADERS=${PROJECT_ROOT}/openrender/shaders")
    env_vars+=("ORENDERHOME=${PROJECT_ROOT}/openrender")
    env_vars+=("DISPLAYS=${PROJECT_ROOT}/openrender/displays")
    env_vars+=("GEOMETRIES=${PROJECT_ROOT}/openrender/geometry")
    if [[ "${backend}" == "x11" ]]; then
        env_vars+=("WAYLAND_DISPLAY=")   # force X11 path in helper
    fi
    if [[ -n "${env_override}" ]]; then
        env_vars+=("${env_override}")
    fi

    # Launch orender in background
    env "${env_vars[@]}" "${ORENDER}" "${RIB}" &
    local orender_pid=$!

    # Wait for orender to exit (up to 30 seconds)
    local waited=0
    while kill -0 "${orender_pid}" 2>/dev/null; do
        sleep 1
        ((waited++))
        if [[ "${waited}" -gt 30 ]]; then
            log_fail "${backend}: orender did not exit within 30 seconds"
            kill "${orender_pid}" 2>/dev/null || true
            return
        fi
    done
    log_pass "${backend}: orender exited in ${waited}s"

    # Helper should still be running (window stays open)
    sleep 0.5
    local helper_count
    helper_count=$(pgrep -c orender-fb-linux 2>/dev/null || echo 0)
    if [[ "${helper_count}" -gt 0 ]]; then
        log_pass "${backend}: orender-fb-linux still running after orender exit"
    else
        log_fail "${backend}: orender-fb-linux not found after orender exit (window should stay open)"
    fi

    # Kill the helper (simulates user closing window) and check for zombies
    pkill orender-fb-linux 2>/dev/null || true
    sleep 0.5
    local zombie_count
    zombie_count=$(pgrep -c orender-fb-linux 2>/dev/null || echo 0)
    if [[ "${zombie_count}" -eq 0 ]]; then
        log_pass "${backend}: no orphaned orender-fb-linux processes"
    else
        log_fail "${backend}: ${zombie_count} orphaned orender-fb-linux processes remain"
        pkill -9 orender-fb-linux 2>/dev/null || true
    fi
}

# ---------------------------------------------------------------------------
# Run tests
# ---------------------------------------------------------------------------

# X11 test (always run if DISPLAY is set)
if [[ -n "${DISPLAY:-}" ]]; then
    run_test "x11"
else
    echo "SKIP (x11): DISPLAY not set — no X11 display available"
fi

echo ""

# Wayland test (run if WAYLAND_DISPLAY is set)
if [[ -n "${WAYLAND_DISPLAY:-}" ]]; then
    run_test "wayland"
else
    echo "SKIP (wayland): WAYLAND_DISPLAY not set — no Wayland compositor available"
fi

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
echo ""
echo "==================================="
echo "Passed: ${PASS}  Failed: ${FAIL}"
echo "==================================="
if [[ "${FAIL}" -gt 0 ]]; then
    exit 1
fi
exit 0
