---
title: "Display Drivers"
date: 2026-04-16
---

# Display Drivers

openRender supports multiple display drivers for outputting rendered images.

## Interactive Framebuffers

These drivers open a window on your desktop to show the render in real-time.

### Wayland (Linux)

The native Wayland display driver (`fbwl`) provides high-performance, tear-free previewing on modern Linux desktops.

- **Automatic Selection**: On Linux, the `framebuffer` device will automatically attempt to use Wayland if a compositor is detected, falling back to X11 if unavailable.
- **Controls**:
  - `q` or `Esc`: Close the window and stop the render.
  - Window Resizing: The buffer will automatically re-allocate to match the new window size.
- **HiDPI**: Supports both integer and fractional scaling for crisp output on high-resolution displays.

### X11 (Linux/macOS)

The X11 display driver (`fbx`) is the standard for Unix-like systems. It is used as a fallback on Linux when Wayland is not available.

### Windows GDI

On Windows, the `fbw` driver uses the native GDI API to display the render.

## File Drivers

- **TIFF**: Outputs to standard TIFF files.
- **OpenEXR**: Outputs to high-dynamic-range OpenEXR files.

## Troubleshooting Wayland

- **Failed to connect**: Ensure `XDG_RUNTIME_DIR` is set correctly in your environment.
- **Performance**: Ensure you have appropriate graphics drivers installed. While the current implementation uses software buffers (`wl_shm`), it still benefits from compositor acceleration.
- **Window not appearing**: Check the console output for `[ERROR]` logs from the display driver.
