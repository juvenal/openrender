# Framebuffer Display Architecture

Unified IPC-based framebuffer display model: the renderer spawns a standalone helper executable, connects via a Unix domain socket (fixed per-user path `/tmp/orender-fb-<uid>.sock`), and streams TLV-encoded packets (START / DATA / DONE / QUIT). The helper owns the window lifecycle independently of the renderer.

## Implementation Status

- [x] **TLV IPC protocol** (`src/framebuffer/fbipc.h`): shared C++ header — opcodes, packed packet structs, socket path utilities. Full spec in `specs/004-macos-framebuffer-output/contracts/ipc-protocol.md`.
- [x] **macOS driver** (`src/framebuffer/fbq.h` / `fbq.cpp`): `CQDisplay` — `posix_spawn` helper, connect, send START/DATA/DONE. Tries to reuse existing helper before spawning.
- [x] **macOS helper** (`src/framebuffer/orender-fb-macos/`): Swift 6.3 / AppKit / SwiftUI. `NSPanel` (HUD style). Persistent outer `accept()` loop — one new window per render; exits when user closes all windows.
- [x] **Helper persistence**: After DONE, helper loops back to `accept()`. Successive renders reuse the same process. `tryConnectExisting()` in driver avoids redundant spawns.
- [x] **Multiple windows**: Each render opens its own `NSPanel`. All remain visible until individually closed. `AppDelegate` tracks a `sessions: [Session]` array; `NSApp.terminate()` fires only when the array empties.
- [x] **TTY hang fix**: `posix_spawn_file_actions_adddup2` redirects child stdin/stdout/stderr to `/dev/null`. Without this, AppKit modifies the inherited controlling TTY and orender's C-runtime stdio flush blocks after `main()` returns. `POSIX_SPAWN_SETSID` was tried and rejected (breaks Mach bootstrap port / WindowServer connection).
- [x] **CoreServices elimination**: `proc_pidpath()` (`<libproc.h>`) replaces `_NSGetExecutablePath()`. CoreServices initializes background threads that prevent clean process exit. Removed `-framework CoreServices` from `src/common/CMakeLists.txt`.
- [ ] **Linux X11 migration** (`src/framebuffer/fbx.cpp` + `orender-fb-linux/main.cpp`): Architecture synced to macOS reference. Fixed:
  - `main.cpp` role inversion: helper now `bind()+listen()+accept()` (was incorrectly calling `connect()`)
  - `fbx.cpp` socket path: `makeFixedSocketPath()` (UID-based, was PID-based)
  - `fbx.cpp` `tryConnectExisting()`: reuses existing helper before spawning
  - `fbx.cpp` `/dev/null` stdio redirect via `posix_spawn_file_actions_t`
  - `fbx.cpp` failure flag: `failure = TRUE` on spawn/connect error (was `disconnected = true`)
  - `fbx.cpp` `signal(SIGCHLD, SIG_IGN)` + `signal(SIGPIPE, SIG_IGN)` before `sendStart()`
  - `main.cpp` persistent outer accept loop + per-session `pthread` (one window per render)
  - `main.cpp` multi-window: threads stay alive until user closes window; helper exits when all windows close
  - `orender-fb-linux/CMakeLists.txt`: install to `${CMAKE_INSTALL_BINDIR}` (was `DISPLAYSDIR`)
  - **Requires Linux build + end-to-end test to validate**
- [ ] **Linux Wayland migration** (`src/framebuffer/fbwl.cpp`): Same fixes applied as X11 above. `fbwl.cpp` not currently compiled into `framebuffer.so` (only `fbx.cpp` is); Wayland is handled within the helper binary via runtime detection. Untested.
- [ ] **Documentation**: Hugo site page for new framebuffer IPC architecture and macOS build notes.

**Key files**: `src/framebuffer/fbipc.h`, `fbq.cpp`, `orender-fb-macos/Sources/SocketServer.swift`, `AppDelegate.swift`  
**Full spec**: `specs/004-macos-framebuffer-output/` (plan.md, research.md, contracts/ipc-protocol.md)
