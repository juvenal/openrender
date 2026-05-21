# Research: orender-wire — Phase 0 Findings

**Date**: 2026-05-18 | **Branch**: `006-scene-wireframe-viewer`

---

## 1. Geometry Extraction Entry Point

**Decision**: Introduce a new lightweight `CRibGeometryContext` (direct subclass of `CRiInterface`) and override `addObject()`. `CPreviewContext` subclasses `CRibGeometryContext`, not `CRendererContext`.

**Rationale**: The natural first instinct is to subclass `CRendererContext`, which already implements the full RIB parsing pipeline. However, `CRendererContext` initialises display plugins (`DISPLAYS`), shader search paths (`SHADERS`, `ORENDERHOME`), and network subsystems at startup. On any machine that has not configured the full renderer environment — the common case for a wireframe-only user — subclassing `CRendererContext` causes crashes when display plugin DSOs are not found and floods stderr with shader-lookup warnings for every `Surface` and `LightSource` statement in the RIB. This directly violates FR-022 (env-var independence).

`CRibGeometryContext` re-implements only the RIB commands that matter for geometry preview: transform and attribute stacks, camera options, geometry creation, object instancing, and `ReadArchive`. All display, shader, texture, atmosphere, light, and network methods are inherited as no-ops from `CRiInterface`. The geometry classes (`CPolygonMesh`, `CPatchMesh`, etc.) and the RIB parser are shared with the full renderer unchanged.

The `addObject()` interception point is identical in both approaches — every geometric primitive arrives here after the transform hierarchy has been resolved, before any shading or dicing occurs.

**Alternatives considered**:
- Subclassing `CRendererContext` directly — rejected: requires `ORENDERHOME`/`SHADERS`/`DISPLAYS`; crashes without them; floods stderr with shader warnings on any RIB with `Surface` or `LightSource` statements.
- Hooking `dice()` on each primitive — rejected: dice() invokes the shading pipeline, which requires shader execution, display drivers, and framebuffer allocation.
- Writing an independent RIB parser — rejected: duplicates complex existing code; violates Constitution V.

---

## 2. Vertex Position Extraction

**Decision**: Locate the `P` variable in `CVertexData::variables[]` (from `src/ri/pl.h`) and read from `CPl::data0`.

**Rationale**: `CVertexData` holds the list of per-vertex variables (`variables[]`), each a `CVariable*`. The position variable `P` is always the first vertex variable in all geometry types. `CPl::data0` is the flat `float*` array of vertex data (row-major, 3 floats per vertex for P).

**Key field**: `CXform::from` (in `src/ri/xform.h:74`) is the object→world transformation matrix (row-major, right-handed). Apply to each vertex position to get world-space coordinates.

---

## 3. Arcball Camera Source

**Decision**: Port from `src/gui/interface.h` (`CInterface` class), not `opengl.cpp`.

**Rationale**: `opengl.cpp` contains only legacy OpenGL 1.x drawing helpers (`pglTriangles`, `pglLines`, etc.) with no camera math. The quaternion trackball is in `src/gui/interface.h`. Key functions to port:

| Original (C++) | Port target | Purpose |
|---|---|---|
| `CInterface::toSphere(x, y, P)` | `ArcballCamera.toSphere(_:_:)` | Maps 2D mouse → unit sphere |
| `mulqq(orientation, drag, saved)` | Use `simd_mul` on `simd_quatf` | Accumulates rotation quaternion |
| `normalizeq(orientation)` | `simd_normalize` | Keeps quaternion unit |
| `qtoR(R, orientation)` | `simd_float4x4(q)` | Quaternion → rotation matrix |
| `computeMatrices()` | `ArcballCamera.viewProjectionMatrix` | Builds worldToCamera + projection |

**Button mapping change**: The existing `CInterface` maps MID=zoom, RIGHT=pan. The new tool maps scroll-wheel=zoom, middle-drag=pan (per spec FR-010). This is an intentional divergence for idiomatic macOS/Linux trackpad behavior.

**`src/gui/` removal**: After `ArcballCamera.swift` and the C++ Linux arcball are verified, the entire `src/gui/` directory is deleted. It has no `CMakeLists.txt` entry in `src/CMakeLists.txt` and is not part of any current build target.

---

## 4. Camera Parameter Extraction from RIB

**Decision**: Read camera state from `CRibGeometryContext::cameraState` after `RiWorldBegin()` is processed.

`CRibGeometryContext` accumulates camera options as RIB is parsed and snapshots them into a `cameraState` struct at `RiWorldBegin()`. `CPreviewContext::RiWorldBegin()` calls the base implementation first, then reads from `cameraState`.

**Key fields accumulated by `CRibGeometryContext`**:
- `RiProjectionV()` → projection type and `fov` (perspective default: 90°, orthographic: identity projection)
- `RiFormat()` / `RiFrameAspectRatio()` → pixel aspect ratio and image resolution
- `RiClipping()` → near/far planes
- Current `CXform` at the time of `RiWorldBegin()` → camera-to-world matrix; the xform is reset to identity after WorldBegin so subsequent geometry xforms are object-to-world, not object-to-camera

**PreviewCamera construction**:
```
view_matrix   = cameraState.viewMatrix  (world → camera, column-major after transpose)
proj_matrix   = perspective(fov, frameAR, nearPlane, farPlane)
               or orthographic(screenWindow, nearPlane, farPlane)
```

---

## 5. Per-Primitive Tessellation Strategies

### CPolygonMesh
- Read `nvertices[]` (vertex count per face) and `vertices[]` (vertex indices).
- Walk each face: emit edges for consecutive vertex pairs, closing the last edge back to face start.
- Use `pl->data0` (position array) + `CXform::from` for world-space positions.

### CBilinearPatch
- Four corners at (u,v) = (0,0), (1,0), (0,1), (1,1) from the 4-point control mesh.
- Emit 4 edges (the boundary quad).

### CBicubicPatch / CPatchMesh
- Standalone 8×8 Bernstein (Bézier) evaluation using the 16-point control mesh.
- Do NOT call `dice()` — it requires a full `CShadingContext` and shader execution.
- Emit a grid: 9 horizontal + 9 vertical edge strips (64 quads → 144 line segments).

### CNURBSPatch / CNURBSPatchMesh
- De Boor evaluation on a 12×12 parameter grid.
- Parameters: `nu`, `nv`, `uorder`, `vorder`, `uknot[]`, `vknot[]`, `umin/umax`, `vmin/vmax` from the RIB call arguments stored in the object.
- Emit grid edges at each parameter step.

### Quadrics (CSphere, CCone, CCylinder, CDisk, CTorus, CParaboloid, CHyperboloid)
- Parametric sampling: 24 longitude × 12 latitude steps (or equivalent for each shape).
- Each quadric stores its geometric parameters (radius, height, thetamax, etc.) as constructor arguments — read directly from the object fields.
- Apply `CXform::from` to each sampled point.

### CCurveMesh
- Direct vertex chain: for each curve segment, emit consecutive vertex pairs as line-list entries.
- `nverts[]` (vertices per curve), `wrap` (periodic or non-periodic) from the constructor.

### CPoints
- Emit a screen-axis-aligned cross (3 segments: ±X, ±Y, ±Z through each point position) per point.
- Cap at 100,000 points: if `npts > 100000`, uniform stride `npts / 100000`; emit warning to stderr.

### CSubdivision
- Phase 1: display control cage only.
- Walk `vertices[]` face list (same structure as polygon mesh) and emit boundary edges.

### RiProcedural
- Emit an axis-aligned bounding box wireframe: 12 edges from the `bound[6]` array stored in the procedural object.

---

## 6. macOS Build Pattern

**Decision**: Mirror `orender-fb-macos` exactly: SPM Package.swift + CMake custom target invoking `swift build --configuration release`.

**Key differences from orender-fb-macos**:
- Bundle name: `orender-wire` (not `openRender`); `CFBundleName = orender-wire` in Info.plist
- No IPC or socket server — argv[1] is the RIB path
- Metal rendering via `MTKView`; no `NSImageView` or display framebuffer
- Install symlink: `bin/orender-wire` → `libexec/orender-wire.app/Contents/MacOS/orender-wire`

**Coordinate convention**: libribpreview outputs row-major `CXform` matrices (openRender convention). `ribpreview_load()` transposes all matrices to column-major before returning `PreviewSceneC`. Metal `simd_float4x4` is column-major — matrices from `PreviewCameraC` are read directly into `simd_float4x4` without any additional transpose in Swift.

---

## 7. Linux Build Pattern

**Decision**: GTK 4 (≥4.22) for windowing, OpenGL context, input, and file dialogs.

**Rationale**: GTK 4 provides `GtkGLArea` which creates a managed OpenGL 3.3 Core context and issues `realize` and `render` signals at the appropriate times — eliminating all manual EGL, xdg-shell, GLX, and X11 window creation code. GTK 4 also handles Wayland/X11 backend selection transparently by reading `WAYLAND_DISPLAY` and `DISPLAY` internally; the application does not inspect these variables directly. `GtkFileDialog` (available since GTK 4.10, present in all GTK 4.22 installations) provides a native modal file dialog with no external process dependency. GTK 4 is LGPL-licensed and available on all major Linux distributions; it carries no GNOME or KDE desktop environment requirement.

**Wayland/X11 detection**: Fully transparent — GTK 4 selects the backend at startup based on `WAYLAND_DISPLAY` / `DISPLAY` without any code in orender-wire. Exit code 4 is raised if `g_application_run` fails to acquire a display.

**Rendering**: VAO + VBO; upload line-list once in the `GtkGLArea::realize` handler; `glDrawArrays(GL_LINES, ...)` in the `GtkGLArea::render` handler. GLSL 3.30 core profile (same logical content as Metal shaders). Depth test enabled (`GL_DEPTH_TEST`, `glDepthFunc(GL_LESS)`).

**Background loading**: `ribpreview_load()` runs on a `GTask` background thread while a `GtkSpinner` overlay is displayed; on completion the spinner is hidden and the VBO is populated (satisfies FR-005: window opens immediately with loading indicator).

**Input**: GTK 4 event controllers — `GtkGestureClick` (orbit / pan), `GtkEventControllerMotion` (drag), `GtkEventControllerScroll` (zoom), `GtkEventControllerKey` (reset, quit, save). No GLFW, SDL, or raw X11/Wayland event handling.

**File dialog**: `GtkFileDialog::save()` async API — always available when GTK 4.22 is installed; no external tool (zenity, kdialog) dependency.

**CMake detection**: `pkg_check_modules(GTK4 REQUIRED gtk4>=4.22)` in `src/preview/orender-wire-linux/CMakeLists.txt`; link `GTK4_LIBRARIES` alongside `OpenGL::GL`.

---

## 8. C API Bridge (ribpreview_api.h)

**Decision**: Expose a flat `extern "C"` API for Swift/C interoperability.

```c
// ribpreview_api.h
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float projMatrix[16];   // column-major 4×4 projection matrix
    float viewMatrix[16];   // column-major 4×4 view matrix
    float nearPlane;
    float farPlane;
    int   projectionType;   // 0 = perspective, 1 = orthographic
} PreviewCameraC;

typedef struct {
    float *vertices;        // flat line-list: [x0,y0,z0, x1,y1,z1, ...]
    int    vertexCount;     // total number of float3 vertices (pairs → line segments)
    PreviewCameraC camera;
} PreviewSceneC;

// Load a RIB file. Returns NULL on fatal error (message written to stderr).
PreviewSceneC *ribpreview_load(const char *ribPath);

// Free resources allocated by ribpreview_load.
void ribpreview_free(PreviewSceneC *scene);

#ifdef __cplusplus
}
#endif
```

Matrices are transposed from row-major (C++ side) to column-major (Metal/GL side) before returning from `ribpreview_load`.

---

## 9. Test Strategy

**TDD gate**: All tests under `tests/preview/` must be written and failing before libribpreview implementation begins.

| Test file | What it covers |
|---|---|
| `test_polygon.cpp` | CPolygonMesh → correct edge count and world-space positions |
| `test_patch.cpp` | CBilinearPatch (4 edges), CBicubicPatch (8×8 grid edge count) |
| `test_quadric.cpp` | CSphere (24×12 grid), CCone, CCylinder — non-degenerate positions |
| `test_nurbs.cpp` | CNURBSPatch — De Boor evaluation produces non-degenerate curve |
| `test_camera.cpp` | Camera extraction from a known RIB → expected view/proj matrices |

Integration test: pipe `examples/rib/camera-dof.rib` through `ribpreview_load`; assert `vertexCount > 0` and `camera.nearPlane > 0`.

---

## 10. src/gui/ Removal

**Decision**: Delete `src/gui/` after `ArcballCamera.swift` and the Linux C++ arcball are code-reviewed and passing tests.

**What is deleted**: `interface.h`, `opengl.h`, `opengl.cpp`, `opengl-fltk.h`, `opengl-qt.h`, `opengl.pro`, `statView.h`, `statView.cpp`.

**What is preserved**: The algebra macros `mulqq`, `normalizeq`, `qtoR`, `toSphere` used by the arcball are re-implemented in Swift using `simd_quatf` primitives (macOS) and as standalone C++20 inline functions (Linux) — none of them depend on Qt or FLTK headers.

**Rationale**: `src/gui/` has no CMakeLists.txt and is not referenced by any current build target. Keeping dead code violates Constitution I. The Qt/FLTK build files are artifacts of the original openRender code base with no path to compilation on the current build system.
