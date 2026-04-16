# Phase 0: Wayland Display Driver Research

## Unknowns & Clarifications

1.  **Wayland Buffer Management (`wl_shm`)**: How to efficiently share a pixel buffer from the renderer's output to the Wayland compositor.
    -   *Decision*: Use `wl_shm` for standard software-based rendering as per FR-010's initial requirement.
    -   *Rationale*: Widely supported across all Wayland compositors and avoids complex EGL/DRM dependencies for Phase 1.

2.  **Dedicated Event Thread**: How to implement a non-blocking event loop while the renderer works in parallel.
    -   *Decision*: Create a dedicated `pthread` for the Wayland event loop (`wl_display_dispatch`).
    -   *Rationale*: Ensures the UI remains responsive (resizing, closing) even if the renderer is processing a complex frame (FR-012).

3.  **DPI Scaling**: Handling HiDPI on Wayland.
    -   *Decision*: Implement `wp_fractional_scaling_v1` where available and fall back to integer `wl_surface_set_buffer_scale`.
    -   *Rationale*: Aligns with FR-014 for crisp visuals on modern displays.

4.  **Resilient Disconnect**: Handling compositor failure gracefully.
    -   *Decision*: Use the Wayland error callback (`wl_display_set_error_handler`) or detect `wl_display_dispatch` failure to return `FALSE` in `displayData`.
    -   *Rationale*: Allows the renderer's `dispatch` logic to drop only the failed output (NFR-008).

## Best Practices

-   **Wayland Protocols**: Use `xdg_wm_base` for standard window management.
-   **Pixel Formats**: Strictly use `WL_SHM_FORMAT_ARGB8888` for the shared buffer as per FR-013.
-   **Logging**: Ensure all driver-specific logs use the provided `logging.hpp` macros with appropriate levels.

