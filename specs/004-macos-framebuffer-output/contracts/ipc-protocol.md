# Contract: Framebuffer IPC Protocol

**Feature**: 004-macos-framebuffer-output  
**Version**: 1.0  
**Date**: 2026-05-06

## Overview

The display driver (`fbq.cpp`, `fbx.cpp`, `fbwl.cpp`) and the display helper (`orender-fb-macos`, `orender-fb-linux`) communicate over a **Unix domain socket** using a **TLV (Type-Length-Value)** binary protocol. The driver is always the client; the helper is always the server.

## Socket Lifecycle

1. The **helper** is spawned first via `posix_spawn`. It receives the socket path as `argv[1]`.
2. The **helper** creates the socket, binds it, and begins listening before signalling readiness.
3. The **driver** connects to the socket path after spawn.
4. Exactly **one connection** is accepted per session. The helper does not accept a second connection.
5. When the connection is closed (by either side), the session ends.

**Socket path format**: `/tmp/orender-fb-<renderer-pid>.sock`

The renderer PID guarantees uniqueness across concurrent render processes.

## Packet Format

All packets share the same header structure:

```
┌──────────┬──────────────────────────┬──────────────────────────────┐
│  Opcode  │         Length           │           Payload            │
│  1 byte  │      4 bytes (LE)        │       Length bytes           │
└──────────┴──────────────────────────┴──────────────────────────────┘
```

- **Opcode**: unsigned 8-bit integer identifying packet type.
- **Length**: unsigned 32-bit integer, **little-endian**, byte count of the payload. Zero for packets with no payload.
- **Payload**: opcode-specific binary data.

All multi-byte integer fields within payloads are **little-endian**.

## Opcodes

| Opcode | Hex | Direction | Description |
|--------|-----|-----------|-------------|
| START | 0x01 | driver → helper | Open display session |
| DATA | 0x02 | driver → helper | Pixel tile update |
| DONE | 0x03 | driver → helper | Render complete |
| QUIT | 0x04 | either direction | Graceful shutdown |

## Packet Specifications

### START (0x01)

Sent **once**, immediately after the socket connection is established. The helper MUST NOT display or resize the window until START is received.

**Payload**:

```
┌──────────┬──────────┬────────────┬──────────┬──────────────────┐
│  width   │  height  │ numSamples │ titleLen │     title        │
│ uint32_t │ uint32_t │  uint32_t  │ uint32_t │ titleLen bytes   │
│  4 bytes │  4 bytes │   4 bytes  │  4 bytes │  (UTF-8, no NUL) │
└──────────┴──────────┴────────────┴──────────┴──────────────────┘
```

| Field | Constraints |
|-------|-------------|
| `width` | 1 ≤ width ≤ 16384 |
| `height` | 1 ≤ height ≤ 16384 |
| `numSamples` | 3 (RGB) or 4 (RGBA) |
| `titleLen` | 0 ≤ titleLen ≤ 512 |
| `title` | UTF-8 encoded display name from RIB `Display` statement |

**Helper response**: Create window of `width × height` pixels, set window title to `title` (or a default if `titleLen == 0`).

---

### DATA (0x02)

Sent once per pixel tile during rendering. Multiple DATA packets MAY arrive in rapid succession.

**Payload**:

```
┌───────┬───────┬───────┬───────┬──────────────────────────────────────────┐
│   x   │   y   │   w   │   h   │                 pixels                   │
│ uint32│ uint32│ uint32│ uint32│  w × h × numSamples × sizeof(float) bytes│
│4 bytes│4 bytes│4 bytes│4 bytes│            float32, little-endian         │
└───────┴───────┴───────┴───────┴──────────────────────────────────────────┘
```

| Field | Constraints |
|-------|-------------|
| `x` | 0 ≤ x < width (from START) |
| `y` | 0 ≤ y < height (from START) |
| `w` | 1 ≤ w, x + w ≤ width |
| `h` | 1 ≤ h, y + h ≤ height |
| `pixels` | float32 values, pre-clamped to [0.0, 1.0] by driver; channel order matches numSamples |

**Channel order**: RGB when `numSamples = 3`; RGBA when `numSamples = 4`.

**Helper response**: Update the corresponding rectangle in the image buffer. Enqueue the tile for ordered display (all tiles MUST be shown, none dropped).

---

### DONE (0x03)

Sent once, after the last DATA packet. Signals that rendering is complete.

**Payload**: empty (`length = 0`).

**Helper response**: After draining any remaining queued tiles, update window title to indicate "Rendering Complete" (platform-appropriate wording). Keep window open until user closes it or QUIT is received.

**Driver behavior**: After sending DONE, the driver closes the socket and returns from `displayFinish()`. The renderer process exits. The helper continues running independently.

---

### QUIT (0x04)

May be sent by **either side**. Signals graceful shutdown.

**Payload**: empty (`length = 0`).

**Driver → helper**: Driver is requesting the helper exit (e.g., render abort with no output needed).

**Helper → driver**: Not expected in normal flow; reserved for future bidirectional use.

**Helper response on receiving QUIT**: Drain any queued tiles (best-effort), then close the window and exit.

**Helper behavior on connection close without QUIT**: The driver exited unexpectedly (renderer killed). The helper MUST detect the closed socket (EOF / read error), retitle the window to "Interrupted" (or equivalent), preserve the last rendered state, and remain open until the user closes it.

## Error Handling

| Condition | Driver Behavior | Helper Behavior |
|-----------|-----------------|-----------------|
| Helper fails to start | Emit warning to stderr; continue render without display | N/A |
| Socket connect timeout (> 5s) | Emit warning to stderr; continue render without display | Exit with non-zero code |
| Partial/corrupt packet received | N/A (driver always writes complete packets) | Close connection; treat as interruption |
| DATA before START | N/A | Discard; emit warning to stderr |
| Second connect attempt | N/A | Reject (close immediately) |

## Versioning

Protocol version 1.0. No version negotiation handshake in v1 — both sides must be from the same build. Future protocol changes increment the version and add a handshake in START.

## Implementation Reference Files

| File | Role |
|------|------|
| `src/framebuffer/fbipc.h` | C++ opcode constants, packet structs, helper path utilities |
| `src/framebuffer/fbq.cpp` | macOS IPC client (driver side) |
| `src/framebuffer/fbx.cpp` | Linux X11 IPC client (driver side, refactored) |
| `src/framebuffer/fbwl.cpp` | Linux Wayland IPC client (driver side, refactored) |
| `src/framebuffer/orender-fb-macos/Sources/Protocol.swift` | Swift TLV parser (helper side) |
| `src/framebuffer/orender-fb-linux/main.cpp` | C++ TLV parser + window host (helper side) |
