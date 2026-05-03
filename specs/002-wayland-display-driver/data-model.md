# Phase 1: Wayland Display Driver Data Model

## Display Driver Module (`CWDisplay`)

- **Connection State**:
    - `wl_display`: The connection to the Wayland compositor.
    - `wl_registry`: To discover compositor interfaces.
    - `wl_shm`: For shared memory pixel buffers.
    - `xdg_wm_base`: For window management.
- **Surface & Shell Handles**:
    - `wl_surface`: The main drawing surface.
    - `xdg_surface`: For window geometry management.
    - `xdg_toplevel`: For top-level window controls (resize, close, minimize).
- **Buffer Format**:
    - `WL_SHM_FORMAT_ARGB8888`: 32-bit pixel format with alpha (FR-013).
- **Event Management**:
    - `pthread_t`: Dedicated thread for `wl_display_dispatch` (FR-012).
    - `display_loop_mutex`: Thread-safety for surface/buffer updates.
    - **Input Queue**: A thread-safe FIFO queue (`std::queue` with mutex) to propagate keyboard/mouse events from the Wayland event thread to the main renderer loop (FR-015).

## Display Configuration

- **Window Settings**:
    - `width`, `height`: Requested dimensions.
    - `title`: Window name from `displayStart`.
    - `scale`: HiDPI scaling factor from compositor (FR-014).
- **Fallback Hierarchy**:
    - 1. Wayland
    - 2. X11
    - 3. File (Default)

