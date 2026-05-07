# Data Model: Unified Framebuffer Display Architecture

**Feature**: 004-macos-framebuffer-output  
**Date**: 2026-05-06

## Entities

---

### TLVPacket

The fundamental unit of communication over the Unix socket. All messages in both directions follow this structure.

| Field | Type | Size | Notes |
|-------|------|------|-------|
| opcode | uint8_t | 1 byte | See Opcode enum |
| length | uint32_t | 4 bytes | Little-endian; byte count of payload |
| payload | uint8_t[] | `length` bytes | Opcode-specific structure |

**Opcodes**:

| Name | Value | Direction | Description |
|------|-------|-----------|-------------|
| START | 0x01 | driver → helper | Begin display session |
| DATA | 0x02 | driver → helper | Pixel tile update |
| DONE | 0x03 | driver → helper | Render complete |
| QUIT | 0x04 | either | Graceful shutdown |

---

### StartPayload

Sent as the first packet after socket connection. The helper creates the window upon receiving this packet.

| Field | Type | Size | Notes |
|-------|------|------|-------|
| width | uint32_t | 4 bytes | Image width in pixels |
| height | uint32_t | 4 bytes | Image height in pixels |
| numSamples | uint32_t | 4 bytes | Samples per pixel (e.g., 3 for RGB, 4 for RGBA) |
| titleLen | uint32_t | 4 bytes | Byte length of the title string |
| title | uint8_t[] | titleLen bytes | UTF-8 display name from RIB (no null terminator) |

---

### DataPayload

Sent once per pixel tile. Contains floating-point pixel data for a rectangular sub-region.

| Field | Type | Size | Notes |
|-------|------|------|-------|
| x | uint32_t | 4 bytes | Left edge of tile (pixels, 0-based) |
| y | uint32_t | 4 bytes | Top edge of tile (pixels, 0-based) |
| w | uint32_t | 4 bytes | Tile width in pixels |
| h | uint32_t | 4 bytes | Tile height in pixels |
| pixels | float32[] | w × h × numSamples × 4 bytes | Row-major, top-to-bottom, values in [0.0, 1.0] after clamping |

**Notes**:
- Data is clamped to [0.0, 1.0] by the display driver (`CDisplay::clampData()`) before sending.
- Channel order matches `numSamples`: 3 → RGB, 4 → RGBA. Alpha defaults to 1.0 when absent.

---

### DonePayload / QuitPayload

Both carry no payload bytes (`length = 0`).

---

### DisplaySession

Logical entity representing the lifetime of one render's display, from socket connection to window close.

| Attribute | Type | Notes |
|-----------|------|-------|
| socketPath | string | Temporary path, e.g., `/tmp/orender-fb-<pid>.sock` |
| state | SessionState | See state machine below |
| imageBuffer | ImageBuffer | Backing pixel store |
| tileQueue | queue\<DataPayload\> | FIFO, drained sequentially on display thread |
| windowTitle | string | Set from START title; updated on DONE/disconnect |

**Session State Machine**:

```
 [Idle]
    │  posix_spawn helper + connect socket
    ▼
[Connecting]
    │  socket accepted + START packet sent
    ▼
[Active] ──── DATA packets ──── (self)
    │  DONE packet sent
    ▼
[Complete]
    │  user closes window OR QUIT received
    ▼
[Closed]

[Active] ──── unexpected disconnect ──── [Interrupted]
    │  user closes window
    ▼
[Closed]
```

---

### ImageBuffer

The in-memory backing store for the progressive display image. One per display session.

| Attribute | Type | Notes |
|-----------|------|-------|
| width | int | Set from START payload |
| height | int | Set from START payload |
| numSamples | int | Set from START payload |
| pixels | float[] | width × height × numSamples, initialized to checkerboard |
| dirty | bool | True when tileQueue has been drained and redraw is pending |

**Platform representation**:
- macOS: `CGContext` (BGRA 8-bit) populated from float pixels; exposed as `CGImage` to SwiftUI `Image`
- Linux: Existing `void* imageData` with native display bit depth (8/15/16/32 bpp)

---

### DisplayDriver (C++ side, per-platform)

Abstract base: `CDisplay` (existing). Each platform subclass implements IPC client behavior.

| Platform | Class | File | Mechanism |
|----------|-------|------|-----------|
| macOS | `CQDisplay` | `fbq.h/.cpp` | `posix_spawn` + Unix socket client |
| Linux X11 | `CXDisplay` | `fbx.h/.cpp` | `posix_spawn` + Unix socket client (refactored) |
| Linux Wayland | `CWDisplay` | `fbwl.h/.cpp` | `posix_spawn` + Unix socket client (refactored) |

**Key fields added to each driver subclass** (replacing pthread/thread fields):

| Field | Type | Purpose |
|-------|------|---------|
| socketPath | char[256] | Temp socket path for this session |
| socketFd | int | Connected Unix socket FD |
| helperPid | pid_t | PID of spawned helper process |

---

### HelperApp (Swift — macOS)

The `orender-fb` process entity.

| Component | Responsibility |
|-----------|----------------|
| `SocketServer` | Binds Unix socket, accepts single connection, reads TLV stream |
| `ImageStore` | `@MainActor ObservableObject`; owns `ImageBuffer`, manages `tileQueue` |
| `ContentView` | SwiftUI view; renders `Image(cgImage:)` from `ImageStore` |
| `AppDelegate` | Creates `NSPanel` (HUD style), sets window title, handles menu actions |
| `Protocol` | TLV constants and payload decoders |

---

### HelperApp (C++ — Linux)

The `orender-fb-linux` process entity.

| Component | Responsibility |
|-----------|----------------|
| `SocketServer` | Binds Unix socket, accepts single connection, reads TLV stream |
| Display backend | Existing X11 (`CXDisplay::main()`) or Wayland (`CWDisplay::main()`) window loop, adapted to receive data from socket instead of in-process thread |
| `main()` | Parses args (socket path, display preference), runs display backend loop |
