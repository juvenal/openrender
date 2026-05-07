# Implementation Plan: Unified Framebuffer Display Architecture (macOS + Linux)

**Branch**: `004-macos-framebuffer-output` | **Date**: 2026-05-06 | **Spec**: [spec.md](spec.md)

## Summary

Add a native macOS Cocoa framebuffer display window and migrate all three framebuffer backends (macOS, Linux X11, Linux Wayland) to a shared helper-executable IPC model. The renderer spawns a standalone display helper (`orender-fb-macos` on macOS, `orender-fb-linux` on Linux) via `posix_spawn`, connects to it over a Unix domain socket, and streams TLV-encoded pixel tiles to it. The helper owns the window lifecycle independently, allowing the renderer process to exit immediately after rendering while the window remains open for inspection.

**macOS helper**: Swift 6.3, SwiftUI + AppKit, `NSPanel` with HUD style, minimum deployment macOS 12.0.  
**Linux helper**: C++ (C++20), reuses existing X11/Wayland window code, refactored as a socket server.  
**Protocol**: TLV over Unix socket — START, DATA, DONE, QUIT packets. Defined in `fbipc.h` and `contracts/ipc-protocol.md`.

## Technical Context

**Language/Version**: C++20 (drivers, Linux helper), Swift 6.3 (macOS helper)  
**Primary Dependencies**: AppKit, SwiftUI, Foundation (macOS helper — system frameworks only); libX11, wayland-client, libdecor (Linux helper — existing); CMake 4.x with Swift language support  
**Storage**: N/A (display-only; no persistence except user-initiated Save Image as TIFF/PNG)  
**Testing**: CTest integration (existing); new unit tests via C++ catch-style or CTest scripts; Swift XCTest for `Protocol.swift` and `ImageStore.swift`  
**Target Platform**: macOS 12.0+ (orender-fb-macos Swift helper), Linux x86_64/arm64 (orender-fb-linux), renderer plugin (all Unix platforms)  
**Project Type**: Library module (framebuffer.so display plugin) + helper executables  
**Performance Goals**: Terminal returns within 1 second of render completion; pixel tiles visible within 1 second of production under normal loads; tile queue memory < 200 MB for renders ≤ 4K RGBA  
**Constraints**: No fork on macOS; no third-party dependencies; all tiles displayed in order (no dropping); closing window does not abort render  
**Scale/Scope**: Single render per session; single window per render; macOS 12–26, Linux (Wayland + X11)

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-checked after Phase 1 design.*

| Principle | Status | Notes |
|-----------|--------|-------|
| I. Clean Code | ✅ PASS | Each class has single responsibility. Protocol header isolated. Driver and helper fully decoupled. |
| II. Language Standards | ✅ PASS with justification | C++20 for all C++ files. Swift 6.3 for macOS helper — justified as macOS platform-specific language (see Complexity Tracking). |
| III. TDD (NON-NEGOTIABLE) | ✅ PASS | Tests written first for: TLV encode/decode, driver socket logic, image buffer tile application. SwiftUI visual layer is not unit-tested (impractical), but `ImageStore` and `Protocol.swift` are unit-tested. |
| IV. CLI | ✅ PASS | `orender-fb-macos <socket-path>` CLI. Warnings to stderr. Helper exits non-zero on startup failure. |
| V. Minimal Dependencies | ✅ PASS | macOS: system frameworks only. Linux: existing system libraries (X11, Wayland) already in use. No new third-party dependencies. |
| VI. Platform Targeting | ✅ PASS | Platform code isolated: `fbq.cpp` (Apple), `fbx.cpp`/`fbwl.cpp` (Linux), `fbw.cpp` (Windows, unchanged). `#ifdef APPLE` guard in `framebuffer.cpp`. |
| VII. Documentation | ⚠️ REQUIRED | Hugo site must be updated to document new framebuffer architecture, IPC protocol, macOS support, and platform-specific build notes. See tasks. |

## Project Structure

### Documentation (this feature)

```text
specs/004-macos-framebuffer-output/
├── plan.md              ← this file
├── research.md          ← Phase 0: decisions and rationale
├── data-model.md        ← Phase 1: entities, packet structs, state machines
├── quickstart.md        ← Phase 1: developer how-to
├── contracts/
│   └── ipc-protocol.md  ← Phase 1: TLV protocol specification
├── checklists/
│   └── requirements.md
└── tasks.md             ← Phase 2 (/speckit-tasks output)
```

### Source Code (repository root)

```text
src/framebuffer/
├── fbipc.h                         # NEW: shared TLV opcodes, packet structs (C++20)
├── fbq.h                           # NEW: CQDisplay class (macOS IPC client)
├── fbq.cpp                         # NEW: macOS IPC client — posix_spawn + socket + TLV
├── fbx.h                           # MODIFIED: remove pthread members; add IPC client fields
├── fbx.cpp                         # MODIFIED: constructor spawns helper; data/finish send TLV
├── fbwl.h                          # MODIFIED: same as fbx
├── fbwl.cpp                        # MODIFIED: same as fbx
├── framebuffer.cpp                 # MODIFIED: add #ifdef APPLE branch for CQDisplay
├── CMakeLists.txt                  # MODIFIED: add APPLE target, orender-fb-macos, orender-fb-linux
├── orender-fb-macos/                     # NEW: Swift/SwiftUI macOS helper executable
│   ├── CMakeLists.txt              #   Swift target, MACOSX_BUNDLE, deployment macOS 12.0
│   ├── Info.plist                  #   LSUIElement=NO; bundle identifier
│   └── Sources/
│       ├── main.swift              #   NSApplication setup; reads argv[1] as socket path
│       ├── AppDelegate.swift       #   NSPanel (HUD style, closable, titled); menu wiring
│       ├── ContentView.swift       #   SwiftUI Image(cgImage:) from ImageStore
│       ├── ImageStore.swift        #   @MainActor ObservableObject; CGContext; tile FIFO queue
│       ├── SocketServer.swift      #   POSIX bind/accept/read; dispatches parsed packets
│       └── Protocol.swift          #   TLV constants, StartPayload/DataPayload decoders
└── orender-fb-linux/               # NEW: C++ Linux helper executable
    ├── CMakeLists.txt              #   C++20 target; links X11 + optional Wayland/libdecor
    └── main.cpp                    #   POSIX bind/accept/read; existing window display logic

tests/framebuffer/                  # NEW: test suite
├── test_ipc_protocol.cpp           #   TLV roundtrip tests (all opcodes)
├── test_fbq_driver.cpp             #   CQDisplay unit tests (mock socket)
├── test_imagestore.swift           #   ImageStore tile queuing, CGContext updates
├── test_protocol.swift             #   Protocol.swift packet parsing
└── CMakeLists.txt
```

**Structure Decision**: Single-project layout. The display plugin (`framebuffer.so`) and helpers are in `src/framebuffer/`. New code is isolated in `orender-fb-macos/` and `orender-fb-linux/` subdirectories to minimize impact on the existing module structure. Windows (`fbw.cpp`) is left unchanged.

## Implementation Phases

### Phase A — Shared Protocol Foundation (prerequisite for all other work)

1. Write tests for TLV encoding/decoding (all four opcodes, edge cases).
2. Implement `fbipc.h` — opcode enum, packed packet structs, `makeSocketPath(pid_t)` utility.
3. Tests pass.

### Phase B — macOS Driver (`fbq.h` / `fbq.cpp`)

1. Write tests for `CQDisplay` constructor: `posix_spawn` invocation, socket connect, START packet construction.
2. Write tests for `CQDisplay::data()`: DATA packet encoding, sequential send.
3. Write tests for `CQDisplay::finish()`: DONE packet, socket close, graceful helper exit.
4. Implement `CQDisplay` to pass all tests.
5. Update `framebuffer.cpp` with `#ifdef __APPLE__` dispatch to `CQDisplay`.
6. Update `src/framebuffer/CMakeLists.txt` with `APPLE` branch.

### Phase C — macOS Helper (`orender-fb-macos`)

1. Write unit tests for `Protocol.swift` — TLV parser for all packet types.
2. Write unit tests for `ImageStore` — tile queuing, ordered drain, CGContext update, title transitions.
3. Implement `Protocol.swift`, `ImageStore.swift` (tests pass).
4. Implement `SocketServer.swift` — POSIX bind/accept/read loop on background DispatchQueue.
5. Implement `ContentView.swift` — SwiftUI `Image(cgImage:)` observing `ImageStore`.
6. Implement `AppDelegate.swift` — `NSPanel` with `.hudWindow | .closable | .titled`; menu with Save Image (TIFF/PNG via NSSavePanel) and Quit; `panel.orderFrontRegardless()` for non-focus-stealing launch; disconnection → retitle "Interrupted".
7. Implement `main.swift` — `NSApplication` setup, `.regular` activation policy, delegate wiring, socket path from `CommandLine.arguments[1]`.
8. Add `orender-fb-macos` CMake target with Swift 6, deployment target macOS 12.0, `MACOSX_BUNDLE`.

### Phase D — Linux Driver Refactor (`fbx.cpp`, `fbwl.cpp`)

1. Write tests for refactored `CXDisplay` IPC client (mirrors Phase B tests for X11 path).
2. Write tests for refactored `CWDisplay` IPC client.
3. Refactor `CXDisplay`: replace `pthread_create` + X11 window setup in constructor with `posix_spawn` + socket connect. `data()` and `finish()` send TLV. Remove all X11 fields from the class header.
4. Refactor `CWDisplay`: same pattern for Wayland.
5. Tests pass; existing Linux regression RIBs produce correct output.

### Phase E — Linux Helper (`orender-fb-linux`)

1. Extract `CXDisplay::main()` and `CWDisplay::main()` window loops into `orender-fb-linux/main.cpp` as standalone socket-server-fed display routines.
2. `main()` binds socket, accepts connection, reads TLV: START → create window; DATA → update pixel buffer; DONE → retitle; QUIT/disconnect → handle.
3. Add `orender-fb-linux` CMake target linking X11 + optional Wayland/libdecor.
4. Run Linux regression suite end-to-end.

### Phase F — Documentation

1. Update Hugo site: new page documenting framebuffer IPC architecture, macOS support, build notes.
2. Update `ORENDERHOME` environment documentation to include helper binary location.

## Complexity Tracking

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|--------------------------------------|
| Swift language in a C++20 project | macOS ObjC/Swift runtime locks are not fork-safe (macOS ≥ 10.13). A native macOS GUI window requires Cocoa/AppKit. Swift is the canonical Apple platform language and is significantly safer and more maintainable than Objective-C++ for new code. | Objective-C++ (`fbq.mm`) would satisfy the C++ constitution letter but violates the spirit (it introduces ObjC semantics into a C++ codebase). C++ with direct CoreGraphics calls would bypass AppKit safety guarantees. Swift + SwiftUI is maximally isolated (separate executable, separate CMake target, zero C++ coupling). |
| New helper executables alongside the display plugin | The entire point of the feature is to decouple window lifetime from renderer lifetime. The helper must be a separate process. | An in-process approach (thread) is what we have today on Linux; it cannot work on macOS due to fork-safety constraints, and the spec (FR-010) requires a unified model across all platforms. |
