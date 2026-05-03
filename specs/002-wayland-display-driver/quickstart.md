# Quickstart: Wayland Display Driver

## Prerequisites

- **Linux Distribution**: Ubuntu 22.04+, Fedora 38+, or equivalent.
- **Wayland Compositor**: GNOME, KDE Plasma, or wlroots-based.
- **Development Libraries**: `libwayland-client-dev`, `wayland-protocols-dev`.

## Building the Plugin

```bash
# Configure the project with CMake
mkdir build && cd build
cmake .. -DENABLE_WAYLAND=ON

# Build the display drivers
make framebuffer fbwl
```

## Using the Display

To use the interactive display (which will prioritize Wayland and fall back to X11 if unavailable), specify the `framebuffer` output device in your RIB files:

```rib
Display "test.rib" "framebuffer" "rgb"
```

## Verifying the Installation

Run a simple test render and check the logs:

```bash
./bin/orender tests/scenes/teapot.rib
```

Check for `INFO` logs from the display driver identifying the backend:
`[INFO] [fbwl.cpp:123] - Initialized Wayland display driver v1.0.0`

