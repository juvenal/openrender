# Wayland Display Plugin Contract

## Interface Specification

The Wayland display driver plugin (`fbwl.so`) MUST export the following C-style functions as per the project's `framebuffer.h` and `ri/dsply.h`.

| Function | Signature | Purpose |
|----------|-----------|---------|
| `displayStart` | `void *displayStart(const char *name, int width, int height, int numSamples, const char *samples, TDisplayParameterFunction findParameter)` | Initializes the Wayland connection, surface, and event thread. Returns a handle to the `CWDisplay` instance. |
| `displayData` | `int displayData(void *handle, int x, int y, int w, int h, float *data)` | Receives pixel data from the renderer, maps it to the Wayland shared memory buffer, and commits the surface. |
| `displayFinish` | `void displayFinish(void *handle)` | Closes the Wayland window, terminates the event thread, and cleans up all shared memory resources. |

## Implementation Requirements

- **Multi-threaded Safety**: `displayData` MUST NOT block the rendering thread. Use a separate presentation queue or double-buffer if necessary.
- **Resilience**: If `displayData` fails to commit the buffer (e.g., connection lost), it MUST return `0` (FALSE) to signal the renderer to deactivate this specific output.
- **Logging**: All plugin-specific events MUST use the `log_info`, `log_warn`, and `log_error` macros from `src/includes/logging.hpp`.

