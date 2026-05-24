---
title: "orender-wire — Wireframe Scene Previewer"
date: 2026-05-19
weight: 1
---

# orender-wire — Wireframe Scene Previewer

`orender-wire` opens a RenderMan RIB scene as an interactive 3-D wireframe viewer.
It is a native application on macOS (Metal/AppKit) and Linux (GTK 4 / OpenGL 3.3).

## Invocation

```
orender-wire <scene.rib>
orender-wire --help
orender-wire --version
```

The RIB path may be absolute or relative to the current working directory.

## Controls

### Keyboard

| Key | Action |
|-----|--------|
| **R** or **Home** | Reset view to the original RIB-defined camera |
| **S** | Open native save dialog to export the current camera to a RIB file |
| **⌘Q** (macOS) | Quit |
| **Q** or **Escape** (Linux) | Quit |

### Mouse

| Gesture | Action |
|---------|--------|
| Left button drag | Orbit — rotate around the scene center |
| Scroll wheel | Zoom in / out |
| Middle button drag | Pan — translate the look-at point |

### macOS Trackpad

| Gesture | Action |
|---------|--------|
| Two-finger scroll | Zoom in / out |
| Two-finger pan | Pan the camera |
| Pinch | Zoom in / out |

## Environment Variables

`orender-wire` does **not** require `ORENDERHOME`, `SHADERS`, or `DISPLAYS` — those are consumed by the full renderer and are irrelevant to wireframe preview.

| Variable | Description |
|----------|-------------|
| `GEOMETRIES` | Optional colon-separated search path for named geometry files (`Geometry "name"` RIB statements). When unset, `Geometry` primitives are skipped with a stderr notice. |
| `WAYLAND_DISPLAY` | (Linux) Wayland display socket — used by GTK 4 |
| `DISPLAY` | (Linux) X11 display server — used by GTK 4 when Wayland is unavailable |

## Camera Export Workflow

Press **S** to open a native save dialog. The resulting `.rib` snippet contains:

```rib
## orender-wire camera export
## Exported: <ISO-8601 timestamp>

Projection "perspective" "fov" [45.0]
Transform [<16 row-major floats — camera-to-world>]
```

When saving to an existing RIB file that contains a `WorldBegin` statement,
`orender-wire` replaces only the `Projection` and `Transform` statements in the
pre-`WorldBegin` section. All other content is preserved byte-for-byte.

To re-use the exported camera, include the `.rib` snippet in your scene or pass
it directly to `orender`.

## Exit Codes

| Code | Meaning |
|------|---------|
| 0 | Success — window opened and closed normally |
| 1 | Usage error — missing or unrecognised argument |
| 2 | File not found or unreadable |
| 3 | RIB parse failed (fatal error written to stderr) |
| 4 | No display available (Linux — no X11 or Wayland server) |

## Platform Notes

**macOS**: Requires macOS 12 (Monterey) or later. Metal GPU required.
The application appears in the Dock and supports the standard application menu.

**Linux**: Requires GTK 4.20 or later and an OpenGL 3.3 Core-capable GPU.
Both Wayland and X11 display backends are supported automatically.
