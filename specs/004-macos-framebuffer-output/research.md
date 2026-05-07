# Research: Unified Framebuffer Display Architecture

**Feature**: 004-macos-framebuffer-output  
**Date**: 2026-05-06

## Decision Log

---

### D-01: macOS Deployment Target

**Decision**: macOS 12.0 (Monterey) minimum deployment target.

**Rationale**: Xcode 26 / Swift 6.3 with SDK 26.4 targets `arm64-apple-macosx26.0` natively. The SDK's minimum supported deployment for Swift targets is typically 4 years prior to the current OS release. macOS 11.0 (Big Sur, 2020) is 5 years old and Xcode 26 likely no longer supports it as a deployment target. macOS 12.0 (Monterey, 2021) is 4 years back from macOS 26 and provides: SwiftUI 3 (`@main` App protocol fully stable, `WindowGroup` reliable, async/await support), `NWListener`/`NWConnection` stable, and modern Swift 5.5+ concurrency. If macOS 11.0 support is confirmed achievable via the build system, it is safe to target since all APIs used are available on macOS 11 too.

**Alternatives considered**:
- macOS 11.0: SwiftUI 2, `@main` works, all POSIX APIs available — may be dropped by Xcode 26 toolchain.
- macOS 13.0: Too restrictive; cuts off Monterey users unnecessarily.
- macOS 10.15: Requires Xcode 15 toolchain, incompatible with Swift 6.

---

### D-02: Swift Version

**Decision**: Swift 6 (6.3) — strict concurrency enforced.

**Rationale**: Xcode 26 ships Swift 6.3. Swift 6 introduces mandatory actor isolation and strict concurrency checks which, while requiring some care when bridging to AppKit callbacks, prevents the data race conditions that would arise from mixing Unix socket I/O threads with SwiftUI main-actor updates. The forced safe concurrency model is directly relevant to this feature.

**Alternatives considered**:
- Swift 5.x with concurrency opt-in: Would require maintaining backward compatibility shims not needed since Xcode 26 is the target toolchain.

---

### D-03: macOS Window Style — HUD Panel

**Decision**: `NSPanel` with `[.hudWindow, .closable, .titled]` style mask, hosted via `NSHostingView<ContentView>`.

**Rationale**: The user specifically requested "HUD window class." `NSWindowStyleMask.hudWindow` on `NSPanel` provides the dark floating panel appearance appropriate for a live preview overlay. Using `NSPanel` (not `NSWindow`) is required — `.hudWindow` is only valid for `NSPanel` subclasses. An `NSHostingView<ContentView>` wraps the SwiftUI content inside the panel, giving the SwiftUI declarative update model without requiring a full `.app` bundle.

**Implementation approach**:
1. `NSPanel(contentRect:styleMask:backing:defer:)` with `.hudWindow | .closable | .titled`
2. Set `panel.isFloatingPanel = false` — HUD panels default to floating; disable so it doesn't stay above all apps permanently
3. Set `panel.level = .normal`
4. `panel.contentView = NSHostingView(rootView: ContentView(imageStore: store))`
5. Title set to render filename at START; updated on DONE/interrupt

**Alternatives considered**:
- `NSWindow` with `.titled | .closable`: Standard window, loses HUD aesthetic.
- `@main struct App: App` with `WindowGroup`: Requires a proper `.app` bundle which complicates CMake + `posix_spawn` integration; the helper is better as a standalone command-line tool hosting its own NSApp loop.

---

### D-04: App Lifecycle — Standalone Executable (No Bundle)

**Decision**: `orender-fb-macos` is a standalone command-line executable that runs `NSApplication.shared.run()`. No `.app` bundle required.

**Rationale**: The helper is spawned by the display driver via `posix_spawn` with a socket path argument. A full `.app` bundle would complicate the build and spawn paths. A standalone executable that directly sets up `NSApplication` and runs the event loop is simpler and has no functional disadvantage. The macOS menu bar appears because `NSApplication` registers a menu regardless of bundle presence.

**"Start without stealing focus"**: `NSApp.setActivationPolicy(.regular)` is set at launch so the Dock icon and menu bar appear (enabling File > Save / Quit). `panel.orderFrontRegardless()` shows the window without bringing the app to the foreground, so the user's current working app retains focus until they click the framebuffer window.

**Alternatives considered**:
- `.accessory` activation policy: No Dock icon, no menu bar — loses the Save/Quit menu requirement.
- Full `.app` bundle: Correct for distributed apps but over-engineered for a renderer helper.

---

### D-05: Unix Socket IPC in Swift (Server Side)

**Decision**: POSIX socket APIs (`socket`, `bind`, `accept`, `read`, `write`) on a `DispatchQueue.global()` background queue. Socket path passed as `CommandLine.arguments[1]`.

**Rationale**: `NWListener` (Network framework) is available on macOS 11+ and provides cleaner async I/O, but introduces an extra framework dependency and its Unix domain socket support has historically been less straightforward. POSIX sockets are available unconditionally, have zero overhead, and the implementation is small (~100 lines). The incoming data rate is bounded by the renderer output speed, so a simple blocking `read` loop on a background dispatch queue is entirely sufficient.

**Alternatives considered**:
- `NWListener` + `NWConnection`: Cleaner Swift API, but non-trivial to configure for Unix domain sockets and adds Network.framework dependency.
- `Foundation.FileHandle`: Higher-level but not designed for streaming protocols; requires manual framing.

---

### D-06: Tile Queue Strategy (User-Confirmed: All Tiles In Order)

**Decision**: `ImageStore` maintains a `[TilePacket]` FIFO queue. A `DispatchQueue.main.async` loop processes one tile per runloop iteration, updating the SwiftUI image after each.

**Rationale**: The user confirmed Option A (all tiles shown, in order, even if display lags). The queue is unbounded to avoid dropped tiles. For typical render sizes (< 4K) and numSamples ≤ 4, a full-frame queue is < 200 MB which is acceptable. A post-DONE drain ensures all tiles are displayed before the "Rendering Complete" retitle settles permanently.

**Risk noted**: For very large renders (8K+, high numSamples) combined with a slow display, queue memory could grow significantly. This is an acceptable tradeoff per the user's explicit choice.

---

### D-07: Linux Migration — Helper Executable Architecture

**Decision**: Create a single `orender-fb-linux` C++ executable that tries Wayland first (if `$WAYLAND_DISPLAY` is set), falls back to X11. Refactor `fbx.cpp` and `fbwl.cpp` from in-process thread owners to IPC clients using the same TLV socket protocol.

**Rationale**: The current Linux implementation uses `pthread_create` to run the window in a background thread (confirmed in source). The migration:
1. Extracts the window thread main loops (`CXDisplay::main()`, `CWDisplay::main()`) into `orender-fb-linux/main.cpp`
2. Converts `fbx.cpp` / `fbwl.cpp` constructors to `posix_spawn` the helper and connect via socket (matching `fbq.cpp` pattern)
3. `orender-fb-linux` accepts connection, reads TLV, runs the X11/Wayland window exactly as before

This reuses all existing pixel-format conversion code with minimal change — it moves from being called in-thread to being called in the helper.

**Single helper vs separate**: One `orender-fb-linux` binary with Wayland/X11 runtime detection mirrors the current `framebuffer.cpp` `isWaylandAvailable()` check. Simpler than two separate binaries.

**Alternatives considered**:
- Keep Linux as-is (in-process pthread): Technically works; only macOS needs the helper model. But the spec requires all three backends to use the same protocol (FR-010), and the user confirmed this.
- Separate `orender-fb-wayland` and `orender-fb-x11`: Increases binary count and deployment complexity with no benefit.

---

### D-08: Shared Protocol Header

**Decision**: New `src/framebuffer/fbipc.h` defines TLV opcodes and packet structs as C++20. Included by all three driver files (`fbq.cpp`, `fbx.cpp`, `fbwl.cpp`) and by `orender-fb-linux/main.cpp`. The Swift helper has its own `Protocol.swift` with identical magic values.

**Rationale**: A single authoritative source for opcode constants prevents drift between platforms. The C++ header uses `enum class Opcode : uint8_t` and `#pragma pack(1)` structs for portable wire format.

---

### D-09: Build System Integration for Swift

**Decision**: Use CMake's native Swift language support (`enable_language(Swift)`) with a `MACOSX_BUNDLE` executable target for `orender-fb-macos`. CMake 4.x has solid Swift support; the `orender-fb-macos` target links only system frameworks (AppKit, Foundation).

**Rationale**: CMake 4.3.2 is available on the dev machine. CMake's Swift support handles dependency tracking and `@rpath` correctly. The helper does not need a full `.app` bundle structure for a `posix_spawn`'d tool, but using `MACOSX_BUNDLE` ensures the `Info.plist` is embedded for `NSApplication` initialization (required for proper menu bar registration on macOS).

**Alternatives considered**:
- Swift Package Manager (`swift build`) invoked via CMake `ExternalProject_Add`: More complex, introduces a second build system boundary.
- `xcodebuild` custom command: Requires Xcode project file maintenance, not portable.

---

### D-10: Fixed Per-User Socket Path (UID-Based)

**Decision**: The Unix socket path is `/tmp/orender-fb-<uid>.sock` (fixed per OS user), not `/tmp/orender-fb-<pid>.sock` (per renderer process).

**Rationale**: A PID-based path would create a unique socket per invocation. The driver would always find no existing helper, always spawn a fresh one, and the old helper's socket would become unreachable (nothing listening on the old PID-based path). The `quitOldHelper()` mechanism — which relies on connecting to the fixed path to send QUIT — requires the path to be stable across invocations. More importantly, since the helper is now persistent (see D-12), a fixed path lets successive orender runs find and reuse it. The path is still isolated per user (no cross-user socket sharing).

**Consequence**: Concurrent renders from the same user share one helper process. This is intentional — the helper manages multiple windows. Concurrent renders from different users get separate sockets and separate helpers.

**Alternatives considered**:
- PID-based path: New socket per render, always spawns a new helper, accumulates zombie windows. Rejected.
- Named path in `$TMPDIR`: More robust on macOS (user-specific temp dir), but `/tmp/orender-fb-<uid>.sock` is simpler and works in practice for a local tool.

---

### D-11: orender Terminal Hang — Root Cause and Fix

**Decision**: Redirect the spawned helper's stdin/stdout/stderr to `/dev/null` using `posix_spawn_file_actions_adddup2` before spawning.

**Rationale**: Investigation revealed that after all orender application code completed (including `RiEnd()`, all atexit handlers, and the complete shutdown sequence), the process hung in the C-runtime stdio flush phase — after `return` from `main()` but before the actual `_exit()`. Using `_exit()` instead of `return` exited immediately, confirming the hang is in C-runtime cleanup, not application code.

**Root cause**: `orender-fb-macos` inherits orender's controlling TTY (file descriptors 0/1/2). AppKit modifies terminal state during initialization. When orender's C-runtime subsequently tries to flush stdio to the TTY, the flush blocks because the child still holds the TTY in a modified state.

**Fix**: Redirect child fds 0/1/2 to `/dev/null` before exec via `posix_spawn_file_actions_t`. The helper is a GUI app and never reads stdin or writes to the terminal, so this redirect is a pure win.

**Failed alternative**: `POSIX_SPAWN_SETSID` (creates a new session for the child, completely detaching it from the controlling TTY). This broke the helper — it failed to connect to the WindowServer within the 5-second timeout. Cause: a new session has a different (empty) Mach bootstrap namespace; AppKit needs the parent's bootstrap port to connect to the WindowServer. `POSIX_SPAWN_SETPGROUP` (used instead) creates a new process group — no controlling TTY for Ctrl-C propagation — without touching the session or Mach ports.

---

### D-12: Helper Persistence — Outer Accept Loop

**Decision**: The helper keeps its server socket open after each render completes and loops back to `accept()`. It exits only when the user closes all open windows (or when QUIT is received).

**Rationale**: The original design closed the server fd immediately after the first `accept()`, meaning after DONE the socket was no longer listening. Any subsequent orender run would fail to connect (ENOENT or ECONNREFUSED on the socket file), causing `quitOldHelper()` to silently no-op and then spawn a fresh helper. Result: N renders → N helper windows open simultaneously. The fix is to keep `serverFd` open throughout the helper's lifetime and re-enter `accept()` after each DONE, making the helper a long-lived service.

**Session lifecycle** (revised):
1. Helper starts → `bind()` + `listen()` + enter outer `accept()` loop
2. orender connects → `accept()` returns client fd → inner read loop begins
3. START received → new window created
4. DATA packets → tile updates applied
5. DONE received → window titled "Rendering Complete"; `clientFd` closed; outer loop continues back to `accept()`
6. orender reconnects for next render → go to step 2 (new window is created)
7. QUIT received (or user closes last window) → `serverFd` closed; `NSApp.terminate()` called; process exits

**Connection close without DONE (interrupted)**: If the socket EOF is detected and at least one tile was received, the window retitles to "Interrupted" and stays open; outer loop continues to `accept()` for the next render. If no tiles were received, the helper exits.

---

### D-13: Executable Path Resolution — `proc_pidpath()` vs `_NSGetExecutablePath()`

**Decision**: Use `proc_pidpath(getpid(), buf, sizeof(buf))` from `<libproc.h>` to resolve the current executable path on macOS.

**Rationale**: `_NSGetExecutablePath()` (from `<mach-o/dyld.h>`) implicitly pulls in CoreServices. CoreServices initializes background framework threads during initialization. These threads hold a reference to the process's CFRunLoop, which prevents `main()` from returning cleanly — the C runtime blocks waiting for the background threads to drain. `proc_pidpath()` calls a single Mach trap to the kernel (via `libproc`), stays entirely in `libSystem.B.dylib`, and initializes no frameworks.

**Consequence**: Removing CoreServices from the `openrendercommon` CMake target (`src/common/CMakeLists.txt`) was required — no source file in that library actually uses CoreServices symbols.

---

### D-14: Multiple Windows per Helper — `sessions` Array in AppDelegate

**Decision**: `AppDelegate` tracks all open windows in a `sessions: [Session]` array (each entry: panel + Combine title observer). `handleStart()` always creates a new panel. `windowWillClose()` removes the entry; `NSApp.terminate()` fires only when the array empties.

**Rationale**: Users want to compare results across renders. Reusing a single panel (replacing the previous render's image) would destroy that ability. Keeping all windows open requires tracking them explicitly so the helper knows when to exit. A flat array keyed by panel identity (`===`) is the simplest structure — no panel-to-session dictionary needed because closure is infrequent and the array is short.

**Title observer per session**: Each panel's window title is synced to its `ImageStore.windowTitle` via a `sink` subscriber stored in the `Session` struct. This subscriber is cancelled when the struct is removed from the array (on window close), preventing updates to a deallocated panel.
