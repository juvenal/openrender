# Feature Specification: Unified Framebuffer Display Architecture (macOS + Linux)

**Feature Branch**: `004-macos-framebuffer-output`  
**Created**: 2026-05-06  
**Status**: Draft  

## Clarifications

### Session 2026-05-06

- Q: When the user closes the framebuffer window before the render finishes, what should the renderer do? → A: Continue rendering to completion; the display is simply dismissed.
- Q: If the display helper cannot be launched at render start, what should orender do? → A: Warn the user and continue rendering without the display window.
- Q: When the renderer is killed mid-render, what should the display window do? → A: Show the last rendered state, retitle to "Interrupted" (or equivalent), and remain open until the user closes it.
- Q: When tile updates arrive faster than the display can redraw, how should the helper handle the backlog? → A: Queue all tiles and display every one in order; no tiles are skipped, even if display lags behind the renderer.

### Session 2026-05-07 (implementation learnings)

- Q: Should the helper persist between successive renders, or exit after each render completes? → A: The helper persists. After DONE it loops back to `accept()`, ready for the next render session. It exits only when the user explicitly closes all windows.
- Q: Should successive renders reuse the same window, or open a new window each time? → A: Each render opens its own new window. All windows remain visible until the user closes them individually. Closing the last window exits the helper.
- Q: Should successive orender invocations spawn a new helper process every time? → A: No. orender first attempts to connect to an existing helper on the fixed socket path. Only if no helper is listening does it unlink the stale socket and spawn a fresh one. This avoids accumulating helper processes across renders.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - macOS Framebuffer Window (Priority: P1)

A user running orender on macOS specifies a framebuffer output in their RIB file. A display window opens at the start of the render and updates progressively as pixels are produced. When rendering finishes, the window title changes to signal completion and the window remains open so the user can inspect the result. The terminal returns immediately after the render — it is never locked waiting for the user to close the window.

**Why this priority**: macOS framebuffer support is entirely absent today. This is the primary gap being closed by this feature and the core value delivered.

**Independent Test**: Run any RIB file with a framebuffer Display statement on macOS. Verify a window appears, updates with pixel tiles, retitles on completion, and the orender command exits before the window is closed.

**Acceptance Scenarios**:

1. **Given** a RIB file with a framebuffer Display statement, **When** orender runs on macOS, **Then** a display window appears before any pixels are rendered.
2. **Given** the display window is open, **When** the renderer produces pixel tiles, **Then** the window updates to show each tile within 1 second of production under normal rendering loads (per SC-003).
3. **Given** rendering is complete, **When** the DONE signal is sent, **Then** the window title changes to reflect "Rendering Complete" (or equivalent) and the orender process exits.
4. **Given** the orender process has exited, **When** the user examines the window, **Then** the window remains open and interactive until the user closes it manually.
5. **Given** a render has completed and its window is still open, **When** orender is run again, **Then** a second window opens for the new render; the first window remains visible with its completed image.
6. **Given** multiple windows are open, **When** the user closes individual windows, **Then** each window closes independently; the helper process exits only when the last window is closed.

---

### User Story 2 - Non-Blocking Linux Framebuffer (Priority: P2)

A user running orender on Linux (Wayland or X11) uses the framebuffer display. The behavior they observe remains identical to today — progressive updates, window stays open after render — but the internal mechanism that keeps the window alive after the render is replaced by the same robust architecture used on macOS. The terminal returns immediately; there is no change in the visible experience.

**Why this priority**: Linux framebuffer already works today; this story improves reliability and eliminates a fragile OS-level mechanism, reducing future maintenance risk. User-visible behavior must not regress.

**Independent Test**: Run an existing Linux framebuffer regression RIB. Confirm the window opens, updates, retitles on completion, the process exits, and the window persists — identical to current behavior.

**Acceptance Scenarios**:

1. **Given** a Linux machine with a Wayland compositor, **When** orender renders a RIB with framebuffer output, **Then** the framebuffer window behaves identically to the current implementation.
2. **Given** a Linux machine with an X11 display, **When** orender renders a RIB with framebuffer output, **Then** the framebuffer window behaves identically to the current implementation.
3. **Given** the renderer finishes, **When** the terminal is observed, **Then** orender has exited and the window is still open — no regression from today.

---

### User Story 3 - Graceful Handling of Interrupted Renders (Priority: P3)

A user starts a render that uses framebuffer output, then kills the renderer before it finishes (e.g., Ctrl-C or process termination). The framebuffer window does not hang, crash, or leave zombie processes. It either closes cleanly or retains the last known state and retitles appropriately, then closes when the user dismisses it.

**Why this priority**: Interrupted renders are a normal part of iterating on a scene. The display architecture must not leave orphaned windows or processes when the renderer exits early.

**Independent Test**: Start a long render with framebuffer output, send SIGTERM to orender mid-render, and verify no orphaned window or process remains beyond the user manually closing the window.

**Acceptance Scenarios**:

1. **Given** the display window is open and updating, **When** the renderer process is killed, **Then** the display window detects the disconnection, retitles to "Interrupted" (or equivalent), shows the last rendered state, and remains open until the user closes it.
2. **Given** the renderer exits before sending any pixel data, **When** the display window detects disconnection, **Then** the window closes immediately and releases all resources without leaving orphaned processes.

---

### Edge Cases

- If the renderer sends DONE without having sent any pixel tiles, the window shows its initial checkerboard/empty canvas state, retitles to "Rendering Complete", and remains open until the user closes it.
- If the display window is closed by the user before the render completes, the render continues to completion unaffected; closing the window is a display-only action with no effect on the rendering pipeline.
- If the renderer crashes without sending a completion signal (neither DONE nor QUIT), the display window treats this as an interrupted render: if at least one tile was received, it retitles to "Interrupted" and stays open; if no tiles were received, it closes immediately.
- If the display helper fails to launch (e.g., missing binary, insufficient permissions), orender emits a warning to the user and continues rendering to completion without a display window; the render is never aborted for a display failure.
- When pixel tile updates arrive faster than the display can redraw, the helper queues all tiles and displays every one in order; no tiles are skipped. Display may lag behind the renderer during bursts but must catch up once the render completes.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The framebuffer display MUST be available on macOS in addition to Linux.
- **FR-002**: The framebuffer display window MUST open before the first pixel tile is delivered, so the user sees the window appear at render start.
- **FR-003**: The window MUST update progressively as pixel tiles are produced by the renderer. Every tile MUST be displayed in order; no tiles may be skipped. Display may lag behind the renderer during high-throughput bursts but MUST show all tiles by the time the render session ends.
- **FR-004**: The window MUST remain open after rendering completes, allowing the user to inspect the result at their leisure.
- **FR-005**: The renderer process MUST return control to the terminal immediately after rendering completes; it MUST NOT block waiting for the user to close the window.
- **FR-006**: The window title MUST indicate rendering status: active during render, and a distinct "complete" state when done.
- **FR-007**: If the renderer exits without completing normally, the display window MUST detect the disconnection. If at least one pixel tile was received, the window MUST retitle to "Interrupted" (or equivalent), preserve the last rendered state on screen, and remain open until the user closes it. If no pixel tiles were received, the window MUST close immediately and release all resources. In both cases, no orphaned processes must remain.
- **FR-008**: The Linux Wayland framebuffer backend MUST be migrated to the new display architecture, preserving all observable behavior.
- **FR-009**: The Linux X11 framebuffer backend MUST be migrated to the new display architecture, preserving all observable behavior.
- **FR-010**: All three backends (macOS, Linux Wayland, Linux X11) MUST use the same communication protocol between the renderer and the display window.
- **FR-011**: The display helper component MUST be co-installed alongside the orender executable so no manual setup is required by the user.
- **FR-012**: Closing the display window while a render is in progress MUST NOT abort or interrupt the render; the renderer continues to completion independently.
- **FR-013**: If the display helper fails to launch, orender MUST emit a warning message and continue rendering to completion without a display window; a display failure MUST NOT be treated as a fatal render error.
- **FR-014**: Each orender invocation that produces framebuffer output MUST open its own new display window. All windows from previous renders MUST remain visible until explicitly closed by the user. The helper process MUST NOT be restarted per render; it persists across sessions and exits only when the user closes all open windows.
- **FR-015**: On each new render, the display driver MUST first attempt to connect to an already-running helper on the fixed per-user socket path. A new helper process MUST only be spawned when no existing helper is listening. This prevents accumulating multiple helper processes across successive orender runs.

### Key Entities

- **Renderer Process**: The orender executable that reads the RIB, produces pixel tiles, and drives the display lifecycle (start, update, finish).
- **Display Driver**: The framebuffer plugin loaded by the renderer that initiates the display session and forwards pixel data.
- **Display Helper**: A separate process that owns the window, receives pixel data, and manages its own lifecycle independently of the renderer.
- **Pixel Tile**: A rectangular region of pixel data sent from the renderer to the display helper during rendering.
- **Display Session**: The lifetime of a single render's framebuffer output, from window open through user close.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: macOS users can complete a full framebuffer render session (window opens, receives tiles, render completes, window stays open) for any valid RIB with a framebuffer Display statement.
- **SC-002**: The orender command exits within 1 second of render completion on all supported platforms — the terminal is never blocked by the framebuffer window.
- **SC-003**: Pixel tiles appear in the framebuffer window within 1 second of being produced by the renderer under normal rendering loads. During high-throughput bursts, display may lag per FR-003; all tiles MUST be visible by the time the render session ends.
- **SC-004**: No orphaned windows or processes remain after an interrupted render on any supported platform.
- **SC-005**: All current Linux framebuffer regression tests continue to pass after migration to the new architecture.
- **SC-006**: The new macOS backend passes the same test scenarios applied to Linux backends.

## Assumptions

- macOS 12.0 (Monterey) or later is the minimum supported macOS version; earlier versions are out of scope.
- The display helper is a separate executable co-located with orender and does not require user installation steps.
- Each render session opens a new window; all previously opened windows remain visible until the user closes them. The helper is a persistent process — it is not restarted per render.
- The pixel tile format passed from the renderer to the display driver (color depth, channel layout) remains unchanged from the current Linux implementation.
- The Linux migration replaces only the internal window-management mechanism; the rendering pipeline, RIB interface, and all user-visible behavior are preserved exactly.
- Communication between the renderer and display helper uses a local channel (no network) and is not exposed outside the local machine.
- The display helper for Linux will reuse the existing Wayland/X11 window code; only the process lifecycle and communication model change.
- Multiple displays and screen selection are out of scope; the window opens on the primary display.
- The fixed socket path is per-user (`/tmp/orender-fb-<uid>.sock`). Concurrent renders from the same user share the same helper; concurrent renders from different users use separate helpers.
