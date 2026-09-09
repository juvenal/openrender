---
title: "orender-wire — Wireframe Scene & Data-Structure Previewer"
date: 2026-05-19
weight: 1
---

# orender-wire — Wireframe Scene & Data-Structure Previewer

`orender-wire` opens either a RenderMan RIB scene, or one of openRender's precomputed
data-structure files, as an interactive 3-D viewer. It is a native application on macOS
(Metal/AppKit/SwiftUI) and Linux (GTK 4 / OpenGL 3.3).

The file type is always auto-detected from content, never from the filename or extension:

| Document type | Source |
|---|---|
| RIB scene | any `.rib` file — drawn as a flat wireframe |
| Photon map | `RiPhotonMap`-style photon files |
| Irradiance cache | `Attribute "irradiance" "filename"`-style cache files |
| Gather cache | gather-cache files written during raytraced GI passes |
| Point cloud | `.ptc`-style point-cloud files (e.g. baked radiosity) |
| Brick map | `.brk`-style brick-map (3-D texture) files |
| Debug-geometry dump | raw point/line/triangle/quad dumps |

Data-structure files render as points, lines, triangles, and oriented discs, replacing the
functionality of the old FLTK-based `oshow` tool (removed entirely — there is no compatibility
shim for its invocation or its FLTK dependency).

## Invocation

```
orender-wire <file>
orender-wire --json <file>
orender-wire --type=auto|rib|data <file>
orender-wire --help
orender-wire --version
```

The path may be absolute or relative to the current working directory.

- `--json` runs headlessly: it prints a structured description of the file to stdout and exits,
  opening no window and no display connection. See [Headless / `--json` mode](#headless---json-mode)
  below.
- `--type` overrides content-based auto-detection. `rib` forces RIB parsing even if the file also
  happens to match a data-structure file's magic number; `data` forces data-file handling and
  rejects anything that isn't one (exit 4), even a well-formed RIB file.

Only one file is ever open at a time — opening a new file (of either kind) fully replaces
whatever was previously open.

## Controls

### Keyboard — camera (all document types)

| Key | Action |
|-----|--------|
| **R** or **Home** | Reset view to the original camera (RIB-defined, or the synthesized framing camera for a data document) |
| **S** | Open native save dialog to export the current camera to a RIB file |
| **⌘Q** (macOS) | Quit |
| **Q** or **Escape** (Linux) | Quit — see the note below for data documents with channels |

### Keyboard — data-document controls (point clouds and brick maps only)

Carried over from the deleted `oshow` tool's key bindings, now also reachable as menu items (see
below) with their current state always visible on screen — never only printed to a terminal.

| Key | Action | Applies to |
|-----|--------|------------|
| **M** | Increase detail level | Brick maps only |
| **L** | Decrease detail level | Brick maps only |
| **B** | Draw as boxes | Brick maps only |
| **D** | Draw as discs | Brick maps and point clouds |
| **P** | Draw as points | Brick maps and point clouds |
| **Q** | Previous channel | Point clouds and brick maps with channels |
| **W** | Next channel | Point clouds and brick maps with channels |

A control that doesn't apply to the currently open document type is either grayed out (menu
item) or a harmless no-op (keyboard) — for example, **M**/**L** have no effect on a point cloud,
and the channel keys have no effect on a document with zero channels (a photon map, cache, or
debug-geometry dump).

**Linux-specific note on `Q`**: the legacy `oshow` convention needs bare `Q` to mean "previous
channel" when a document has channels, which collides with this app's own pre-existing "`Q`
quits" binding. `orender-wire` on Linux resolves this by document state: `Q` means "previous
channel" whenever the open document has channels, and quits otherwise. **Escape always quits**,
regardless of document type, so quitting is never blocked by this. macOS has no such collision —
quitting there is ⌘Q, an entirely different key combination from any bare-letter shortcut.

### Discoverable controls (menus / header bar)

Every data-document keyboard shortcut above is also reachable without a keyboard:

- **macOS**: a **Data** menu with Previous/Next Channel, Increase/Decrease Detail Level, and Draw
  as Boxes/Discs/Points, each showing its keyboard shortcut and enabled only when the open
  document type supports it.
- **Linux**: a menu button in the header bar with the same controls, plus a window subtitle
  showing the current document type, channel, detail level, and draw mode.

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

`orender-wire` does **not** require `ORENDERHOME`, `SHADERS`, or `DISPLAYS` — those are consumed
by the full renderer and are irrelevant to this viewer, for both RIB scenes and data-structure
files. `--help`, `--version`, and `--json` all work with none of them set.

| Variable | Description |
|----------|-------------|
| `GEOMETRIES` | Optional colon-separated search path for named geometry files (`Geometry "name"` RIB statements). When unset, `Geometry` primitives are skipped with a stderr notice. RIB scenes only. |
| `WAYLAND_DISPLAY` | (Linux) Wayland display socket — used by GTK 4 |
| `DISPLAY` | (Linux) X11 display server — used by GTK 4 when Wayland is unavailable |

## Headless / `--json` mode

`orender-wire --json <file>` prints a structured JSON description of the file and exits, with no
window and no display connection opened — useful for scripting, CI, or an SSH session with no
window server. The output always includes a common envelope:

```json
{
  "schemaVersion": 1,
  "tool": "orender-wire",
  "toolVersion": "<string>",
  "file": "<path as given>",
  "documentType": "rib" | "photonmap" | "irradiancecache" | "gathercache"
                | "pointcloud" | "brickmap" | "debugdump",
  "bounds": { "min": [x, y, z], "max": [x, y, z] },
  "warnings": ["<string>", ...]
}
```

A RIB scene adds:

```json
"scene": { "lineVertexCount": <int>, "lineSegmentCount": <int> },
"camera": { "projectionType": "perspective" | "orthographic",
            "fov": <float>, "frameAspectRatio": <float>,
            "nearPlane": <float>, "farPlane": <float> }
```

Any data-structure document type adds:

```json
"data": {
  "fileVersion": [<int>, <int>, <int>],
  "primitives": { "lines": <int>, "points": <int>, "triangles": <int>, "disks": <int> },
  "decimated": <int>,
  "channels": ["<string>", ...],
  "currentChannel": <int>,
  "detailLevel": <int>,
  "drawMode": "<string>"
},
"camera": { "projectionType": "perspective" | "orthographic",
            "fov": <float>, "frameAspectRatio": <float>,
            "nearPlane": <float>, "farPlane": <float> }
```

The `camera` object for a data document is a synthesized framing camera (not read from the
file). Fields that don't apply to a given document type are present with their "not applicable"
sentinel (`[]` or `-1`) rather than omitted, so the schema shape is stable regardless of
`documentType`. `decimated > 0` means the fixed detail-reduction cap dropped some primitives for
display — a success case, reported as a `warnings` entry, never an error.

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

| Code | Meaning | Reachable under `--json`? |
|------|---------|---|
| 0 | Success | Yes |
| 1 | Usage error — missing, unknown, or malformed arguments | Yes |
| 2 | File not found or unreadable | Yes |
| 3 | RIB parse failed | Yes (specified; not currently reachable in practice — the RIB parser recovers from malformed input rather than aborting) |
| 4 | Data file rejected — unrecognized type string, version mismatch, or word-size mismatch | Yes |
| 5 | Graphics initialization failed (no Metal device / no GL 3.3 context) | No — `--json` never initializes graphics |

## Platform Notes

**macOS**: Requires macOS 12 (Monterey) or later. Metal GPU required for interactive viewing
(not for `--help`/`--version`/`--json`). The application appears in the Dock and supports the
standard application menu, including the **Data** menu described above.

**Linux**: Requires GTK 4.20 or later, libadwaita 1.4 or later, and an OpenGL 3.3 Core-capable
GPU for interactive viewing (again, not for headless invocations). Both Wayland and X11 display
backends are supported automatically.
