# Contract: orender-wire CLI Interface

**Date**: 2026-05-18 | **Branch**: `006-scene-wireframe-viewer`

---

## Invocation

```
orender-wire <rib-file>
orender-wire --help
orender-wire --version
```

---

## Arguments

| Argument | Required | Description |
|---|---|---|
| `<rib-file>` | Yes (unless --help/--version) | Path to a RenderMan Interface Bytestream (.rib) scene file. Absolute or relative to the current working directory. |
| `--help` | No | Print usage summary to stdout and exit 0. |
| `--version` | No | Print version string to stdout and exit 0. |

**No other flags are accepted in v1.** Additional flags (verbosity, wireframe color override, window size) may be added in future versions.

---

## Exit Codes

| Code | Meaning |
|---|---|
| 0 | Success — window was opened and the user closed it normally. |
| 1 | Usage error — wrong number of arguments or unrecognised flag. |
| 2 | File not found or unreadable — the RIB path does not exist or cannot be opened. |
| 3 | Parse error — the RIB file is fatally malformed (no geometry could be loaded). |
| 4 | Display system unavailable — no GPU, no display server (Linux without X11/Wayland). |

---

## Stdout

| Condition | Output |
|---|---|
| `--help` | Usage text (human-readable). |
| `--version` | `orender-wire <version>` |
| Normal operation | Nothing. Stdout is silent. |

---

## Stderr

| Condition | Message format |
|---|---|
| File not found | `orender-wire: cannot open '<path>': <system error>` |
| RIB parse warning | `orender-wire: warning: <file>:<line>: <description>` |
| Fatal parse error | `orender-wire: error: <file>:<line>: <description>` |
| Point cloud subsampled | `orender-wire: warning: point cloud at <primitive-id> has <N> points; displaying 100000 (subsampled)` |
| Procedural placeholder | `orender-wire: warning: procedural primitive represented as bounding box` |
| Named geometry skipped | `orender-wire: GEOMETRIES not set, geometry '<name>' skipped` |
| No display (Linux) | `orender-wire: error: no display server available (GTK 4 could not open a display — check WAYLAND_DISPLAY or DISPLAY)` |

---

## Environment Variables

| Variable | Status | Description |
|---|---|---|
| `GEOMETRIES` | Optional | Colon-separated search path for named geometry RIB files (`Geometry "name"` statements). When unset, `Geometry` primitives are skipped with a stderr notice; all other geometry is unaffected. |
| `WAYLAND_DISPLAY` | Optional (Linux) | Consulted by GTK 4 internally to select the Wayland display backend. Not read directly by orender-wire. |
| `DISPLAY` | Optional (Linux) | Consulted by GTK 4 internally to select the X11 display backend when `WAYLAND_DISPLAY` is unset. Not read directly by orender-wire. |

`ORENDERHOME`, `SHADERS`, and `DISPLAYS` are **not used** by orender-wire. They are consumed by the full renderer (`orender`) for shader and display plugin loading. orender-wire bypasses those subsystems entirely and works without them.

---

## Keyboard Shortcuts (Interactive Mode)

| Key | Action |
|---|---|
| **R** or **Home** | Reset view to original RIB-defined camera |
| **S** | Open native save dialog to export current camera to RIB |
| **⌘Q** (macOS) | Quit |
| **Escape** or **Q** (Linux) | Quit |

---

## Mouse / Trackpad Controls

| Gesture | Action |
|---|---|
| Left button drag | Orbit (arcball rotation around scene center) |
| Scroll wheel | Zoom in / out |
| Middle button drag | Pan (translate look-at point) |
| Two-finger scroll (macOS trackpad) | Zoom in / out |
| Two-finger drag / pan gesture (macOS trackpad) | Pan |

---

## Window Behaviour Contract

- Window title: `orender-wire — <basename of rib-file>` (e.g., `orender-wire — teapot.rib`)
- Initial window size: 1024×768 (resizable)
- Minimum window size: 400×300
- Aspect ratio: unconstrained (user controls freely); wireframe viewport fills the entire window
- macOS: appears in Dock; responds to standard application menu (Quit, About)
- Linux: standard window manager decorations; close button exits the application

---

## RIB Camera Export Contract

Output written by the "Save camera" feature:

```rib
## orender-wire camera export
## Source: <original-rib-file-basename>
## Exported: <ISO-8601 timestamp>

Projection "<perspective|orthographic>" "fov" [<degrees>]
Transform [<m00> <m01> <m02> <m03>
           <m10> <m11> <m12> <m13>
           <m20> <m21> <m22> <m23>
           <m30> <m31> <m32> <m33>]
```

Matrix order: row-major, RenderMan convention (camera-to-world). The 16 floats reproduce the camera position and orientation when loaded back into orender-wire or passed to `orender`.

When updating an existing RIB file, only the `Projection` and `Transform` statements in the pre-`WorldBegin` section are replaced. All other content is preserved byte-for-byte.
