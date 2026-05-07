# Developer Quickstart: Unified Framebuffer Display Architecture

**Feature**: 004-macos-framebuffer-output

## Overview

This feature adds a macOS Cocoa framebuffer display window and migrates all three platform framebuffer backends (macOS, Linux X11, Linux Wayland) to a shared helper-executable IPC architecture.

## Key Concepts

- **Display driver** (`fbq.cpp` / `fbx.cpp` / `fbwl.cpp`): C++ IPC clients loaded as `framebuffer.so`. They spawn the helper and send TLV packets over a Unix socket.
- **Display helper** (`orender-fb-macos` on macOS, `orender-fb-linux` on Linux): Standalone executables that host the window and receive pixel data. They run independently of the renderer after `displayFinish()`.
- **IPC protocol**: TLV binary over Unix domain socket. See `contracts/ipc-protocol.md` and `src/framebuffer/fbipc.h` for packet definitions.

## Repository Layout (Post-Implementation)

```text
src/framebuffer/
├── fbipc.h                     # Shared TLV constants (new)
├── fbq.h / fbq.cpp             # macOS IPC client (new)
├── fbx.h / fbx.cpp             # Linux X11 IPC client (refactored)
├── fbwl.h / fbwl.cpp           # Linux Wayland IPC client (refactored)
├── framebuffer.cpp             # Platform dispatch — adds Apple branch
├── CMakeLists.txt              # Updated for all platforms
├── orender-fb-macos/                 # Swift/SwiftUI macOS helper (new)
│   ├── CMakeLists.txt
│   ├── Info.plist
│   └── Sources/
│       ├── main.swift          # NSApplication setup; parses socket path arg
│       ├── AppDelegate.swift   # NSPanel (HUD style) creation + menu
│       ├── ContentView.swift   # SwiftUI image display
│       ├── ImageStore.swift    # @MainActor ObservableObject; tile queue
│       ├── SocketServer.swift  # POSIX Unix socket listener + read loop
│       └── Protocol.swift      # TLV packet parser
└── orender-fb-linux/           # C++ Linux helper (new)
    ├── CMakeLists.txt
    └── main.cpp                # Socket server + X11/Wayland window host
```

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The build system detects the platform:
- **macOS**: Builds `framebuffer.so` (with `fbq.cpp`) + `orender-fb-macos` Swift executable
- **Linux**: Builds `framebuffer.so` (with `fbx.cpp` + optional `fbwl.cpp`) + `orender-fb-linux`

The helpers are installed to `${OPENRENDER_DISPLAYSDIR}/../bin/` (co-located with `orender`).

## Testing a Framebuffer Render

```bash
SHADERS="$(pwd)/openrender/shaders" \
ORENDERHOME="$(pwd)/openrender" \
DISPLAYS="$(pwd)/openrender/displays" \
GEOMETRIES="$(pwd)/openrender/geometry" \
build/src/orender/orender examples/rib/camera-dof.rib
```

The RIB file must contain a `Display` statement with type `"framebuffer"`. A window should appear immediately and update progressively. The terminal returns when rendering is done; the window stays open.

## Adding a New Test RIB with Framebuffer Output

Add a `Display` line to your `.rib` file:

```
Display "my-scene" "framebuffer" "rgb"
```

Use type `"rgba"` if your shaders produce alpha output.

## Debugging the IPC Protocol

Set `ORENDER_LOG_LEVEL=4` before running `orender` to enable verbose debug logging on stderr. This env var is inherited by `orender-fb-macos` via `posix_spawn`, so both sides emit debug output:

```bash
ORENDER_LOG_LEVEL=4 ORENDERHOME="$(pwd)/openrender" ... build/src/orender/orender scene.rib
```

This logs (from the driver side): first tile sent, sendData failures, finish() lifecycle, and whether an existing helper was reused or a new one was spawned. (From the helper side): tile receipt and session lifecycle events.

## macOS Helper Details

`orender-fb-macos` is a standalone executable (not a `.app` bundle). It can be run manually for testing:

```bash
# Start helper manually on a test socket
build/src/framebuffer/orender-fb-macos /tmp/orender-fb-test.sock &

# Send a minimal START + DATA + DONE using the provided test tool
tests/framebuffer/send-test-frame.sh /tmp/orender-fb-test.sock 320 240
```

The HUD window appears immediately upon receiving the START packet.

**Menu**: File > Save Image... (TIFF or PNG), File > Quit. No other menu items.

### Helper Persistence and Window Management

`orender-fb-macos` is a **long-lived process** — it does not exit after each render:

- The first `orender` run spawns the helper. The socket path is `/tmp/orender-fb-<uid>.sock` (fixed per user).
- Each subsequent `orender` run connects directly to the already-running helper — no new spawn.
- Each render session opens its **own new window**. All windows remain open until closed.
- Closing a window removes it from the helper's tracking. **Closing the last window exits the helper.**
- On the next `orender` run after the helper has exited, a fresh helper is spawned automatically.

### Checking the Socket

```bash
# See if the helper is currently running and listening
ls -la /tmp/orender-fb-$(id -u).sock

# List all running helper processes
pgrep -l orender-fb-macos
```

## Linux Helper Details

`orender-fb-linux` accepts the same protocol. It tries Wayland first (checks `$WAYLAND_DISPLAY`), then falls back to X11. Run manually:

```bash
build/src/framebuffer/orender-fb-linux /tmp/orender-fb-test.sock
```

## Running Unit Tests

```bash
cd build && ctest -R framebuffer --output-on-failure
```

Tests cover:
- TLV encoding/decoding roundtrip (all packet types)
- `CQDisplay` driver: socket connect, START packet construction, DATA forwarding, DONE/QUIT
- Integration: launch helper, send test frame, verify window appears (macOS: `xcrun simctl` or manual inspection)

## Key Invariants

- The driver first attempts to connect to an existing helper; it only spawns a new one if none is listening. Newly spawned helpers are waited on for up to 5 seconds.
- Every DATA tile sent by the driver **will** be displayed (no dropping); display may lag but catches up.
- Closing a framebuffer window does **not** abort the render; the render continues unaffected.
- Each render opens its own window; all windows remain open until the user closes them.
- The helper exits only when the user closes all open windows (or when QUIT is received).
- A render failure or missing helper emits a warning to stderr but the render continues.
- After `displayFinish()` returns, the renderer process exits; the helper owns its own lifecycle.
- The helper's stdio (stdin/stdout/stderr) is redirected to `/dev/null` at spawn time. This is required to prevent the helper from holding orender's controlling TTY, which would cause orender to hang in its C-runtime cleanup phase after `main()` returns.
