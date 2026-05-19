# Framebuffer Display Architecture

Unified IPC-based framebuffer display model: the renderer spawns a standalone helper executable, connects via a Unix domain socket (fixed per-user path `/tmp/orender-fb-<uid>.sock`), and streams TLV-encoded packets (START / DATA / DONE / QUIT). The helper owns the window lifecycle independently of the renderer.

## Implementation Status

- [x] **TLV IPC protocol** (`src/framebuffer/fbipc.h`): shared C++ header — opcodes, packed packet structs, socket path utilities. Full spec in `specs/004-macos-framebuffer-output/contracts/ipc-protocol.md`.
- [x] **Unified IPC driver** (`src/framebuffer/fbipc_display.cpp/.h`): `CIPCDisplay` — Consolidates previous macOS (`fbq.cpp`), Linux X11 (`fbx.cpp`), and Linux Wayland (`fbwl.cpp`) drivers into a single platform-neutral client. Handles `posix_spawn` of the helper, socket connection, and streaming of START/DATA/DONE packets.
- [x] **macOS helper** (`src/framebuffer/orender-fb-macos/`): Swift 6.3 / AppKit / SwiftUI. `NSPanel` (HUD style). Persistent outer `accept()` loop — one new window per render; exits when user closes all windows.
- [x] **Linux helper** (`src/framebuffer/orender-fb-linux/`): C++/X11/Wayland. `bind()+listen()+accept()` model. Persistent outer loop with per-session threads. Supports both X11 and Wayland via runtime detection.
- [x] **Helper persistence**: After DONE, helper loops back to `accept()`. Successive renders reuse the same process. `tryConnectExisting()` in the unified driver avoids redundant spawns.
- [x] **Multiple windows**: Each render opens its own window/panel. All remain visible until individually closed. Helpers track active sessions and exit only when the last window is closed.
- [x] **TTY hang fix**: `posix_spawn_file_actions_adddup2` redirects child stdin/stdout/stderr to `/dev/null`. Without this, AppKit/X11 modifies the inherited controlling TTY and orender's C-runtime stdio flush blocks after `main()` returns.
- [x] **CoreServices elimination**: `proc_pidpath()` (`<libproc.h>`) replaces `_NSGetExecutablePath()`. CoreServices initializes background threads that prevent clean process exit.
- [ ] **Validation**: Linux helper architecture is synced to macOS reference, but requires extensive end-to-end testing on varied X11/Wayland distributions.
- [ ] **Documentation**: Hugo site page for new framebuffer IPC architecture and macOS/Linux build notes.

**Key files**: `src/framebuffer/fbipc.h`, `fbipc_display.cpp`, `orender-fb-macos/Sources/SocketServer.swift`, `orender-fb-linux/main.cpp`  
**Full spec**: `specs/004-macos-framebuffer-output/` (plan.md, research.md, contracts/ipc-protocol.md)
