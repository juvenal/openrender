# Data Model: orender-wire — Scene Wireframe Previewer

**Date**: 2026-05-18 | **Branch**: `006-scene-wireframe-viewer`

---

## Core Entities

### PreviewScene

Top-level container returned by the C++ extraction layer. Immutable after construction.

| Field | Type | Description |
|---|---|---|
| `vertices` | `std::vector<float3>` | Flat line-list in world space. Each consecutive pair of float3 values is one line segment. Total count is always even. |
| `camera` | `PreviewCamera` | Scene camera reconstructed from the RIB file. |
| `sceneBounds` | `AABB` | Axis-aligned bounding box of all vertices in world space. Used to synthesize a default camera when none is defined. |
| `warnings` | `std::vector<std::string>` | Non-fatal parse/tessellation warnings (e.g., unrecognised RIB statement, point cloud subsampled). Written to stderr at load time; also available for programmatic inspection. |

**Invariants**:
- `vertices.size()` is always even (line segments come in pairs).
- `vertices` may be empty (valid: RIB file has no geometry).
- `camera` is always populated; if no camera in RIB, a synthesized default is stored.

---

### PreviewCamera

Represents the scene camera at the time of `WorldBegin`.

| Field | Type | Description |
|---|---|---|
| `projectionType` | enum `{ Perspective, Orthographic }` | Projection type from `RiProjection` call. |
| `fov` | `float` | Horizontal field of view in degrees (perspective only). |
| `frameAspectRatio` | `float` | Frame aspect ratio (width / height). |
| `nearPlane` | `float` | Near clipping plane distance. |
| `farPlane` | `float` | Far clipping plane distance. |
| `viewMatrix` | `matrix4x4` | World → camera (row-major, right-handed). Derived from `CXform::to` at `WorldBegin`. |
| `projMatrix` | `matrix4x4` | Camera → clip space (row-major). Computed from projectionType + fov + frameAspectRatio + near/far. |

**Invariants**:
- `nearPlane > 0` and `farPlane > nearPlane`.
- When `projectionType == Orthographic`, `fov` is not used; the projection matrix is derived from `RiScreenWindow` or a default that frames `sceneBounds`.
- `viewMatrix` and `projMatrix` are always valid (non-degenerate).

---

### AABB (Axis-Aligned Bounding Box)

Used internally for scene bounds and procedural placeholder geometry.

| Field | Type | Description |
|---|---|---|
| `min` | `float3` | Minimum corner in world space. |
| `max` | `float3` | Maximum corner in world space. |

**Derived**: `center = (min + max) * 0.5f`; `extent = max - min`.

---

### ArcballCamera (runtime, not persisted)

The interactive camera state maintained during a viewer session. Not stored in the RIB file; resets to `PreviewCamera` values on the reset shortcut.

| Field | Type | Description |
|---|---|---|
| `orientation` | `simd_quatf` (macOS) / `quaternion` (Linux C++) | Current rotation quaternion accumulated from drag events. |
| `orbitCenter` | `float3` | World-space point the camera orbits around. Initialized to `PreviewCamera` look-at target. |
| `distance` | `float` | Distance from camera to `orbitCenter` (positive). Controlled by scroll/zoom. |
| `pan` | `float2` | Lateral offset from orbit center in camera-local XY. Controlled by middle-drag. |
| `savedOrientation` | `simd_quatf` | Snapshot taken at mouse-down for orbit drag. |
| `savedDistance` | `float` | Snapshot taken at scroll start for zoom. |
| `savedPan` | `float2` | Snapshot taken at mouse-down for pan drag. |
| `ribCamera` | `PreviewCamera` | Immutable copy of the original scene camera. Used for reset. |
| `windowSize` | `float2` | Current window pixel dimensions. Updated on resize. |
| `radius` | `float` | Trackball sphere radius = `sqrt(w²+h²) * 0.5`. Recomputed on resize. |

**Derived output**: `viewProjectionMatrix: simd_float4x4` — product of projection and view matrices, recomputed each frame.

---

### CameraExport (ephemeral, for RIB save)

Transient structure assembled when the user triggers "Save camera". Not persisted in memory after the save dialog completes.

| Field | Type | Description |
|---|---|---|
| `cameraToWorld` | `matrix4x4` | Inverse of the current interactive view matrix. Written as a `Transform` block in the output RIB. |
| `projectionType` | enum | From the `ribCamera` — the export preserves the original projection type. |
| `fov` | `float` | From the `ribCamera` or current interactive state. |
| `outputPath` | `std::string` | Destination file path selected in the native save dialog. |
| `updateExisting` | `bool` | `true` when `outputPath` already exists; triggers camera-section replacement logic. |

---

## State Transitions

### Viewer Lifecycle

```
Launched (argv[1] = RIB path)
    │
    ▼
[Loading] — window open, spinner visible
    │   libribpreview::load() runs on background thread
    │
    ├─ Success → [Interactive] — wireframe visible, arcball active
    │
    └─ Fatal error → [Error] — error printed to stderr, exit non-zero
                                (window never opens on fatal error)

[Interactive]
    │
    ├─ Left drag     → orbit (update orientation)
    ├─ Scroll wheel  → zoom (update distance)
    ├─ Middle drag   → pan (update pan offset)
    ├─ R / Home      → reset (restore ribCamera state)
    ├─ S             → [SaveDialog] → write RIB → [Interactive]
    ├─ Window resize → resize (update windowSize, radius, MTKView drawable)
    └─ ⌘Q / Esc / Q / close → [Exiting] → clean shutdown
```

---

## RIB Camera Export Format

When writing to a **new file**, the output is a minimal RIB snippet:

```rib
# orender-wire camera export — <timestamp>
Projection "perspective" "fov" [<fov>]
Transform [<cameraToWorld matrix, row-major, 16 floats>]
```

When writing to an **existing file**, the tool:
1. Parses the file to locate the first `Projection` and `Transform`/`ConcatTransform` statements that appear before `WorldBegin`.
2. Replaces them in place.
3. Writes all other content unchanged.
4. If no `Projection` or `Transform` is found before `WorldBegin`, inserts the camera block immediately before `WorldBegin`.
