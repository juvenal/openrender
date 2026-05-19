# Tasks: orender-wire — Scene Wireframe Previewer

**Input**: Design documents from `specs/006-scene-wireframe-viewer/`

**Prerequisites**: plan.md ✓ spec.md ✓ research.md ✓ data-model.md ✓ contracts/ ✓ quickstart.md ✓

**Tests**: Included — Constitution III mandates TDD (non-negotiable). All test tasks must be written and **failing** before their corresponding implementation tasks begin.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no shared dependencies)
- **[Story]**: Which user story this task belongs to (US1–US4)
- Exact file paths included in every task description

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Create the `src/preview/` tree, CMake wiring, and SPM package skeleton. No implementation logic — only build system and directory structure.

- [ ] T001 Create `src/preview/` directory tree: `libribpreview/`, `libribpreview/tessellators/`, `orender-wire-macos/Sources/`, `orender-wire-linux/` per plan.md Project Structure
- [ ] T002 Create `src/preview/CMakeLists.txt` declaring `libribpreview` static target linked against `ri` (not the renderer runtime), `orender-wire-macos` custom target (swift build), and `orender-wire-linux` executable target
- [ ] T003 [P] Create `src/preview/orender-wire-macos/Package.swift` (swift-tools-version: 6.0, platforms: macOS 12, executable target `orender-wire-macos`, path `Sources/`)
- [ ] T004 [P] Create `src/preview/orender-wire-macos/CMakeLists.txt` mirroring `src/framebuffer/orender-fb-macos/CMakeLists.txt`: `swift build --configuration release` custom command, `.app` bundle assembly, `codesign --force --deep -s -`, install symlink `bin/orender-wire → libexec/orender-wire.app/Contents/MacOS/orender-wire`
- [ ] T005 [P] Create `src/preview/orender-wire-linux/CMakeLists.txt`: standard executable `orender-wire`, `find_package(OpenGL REQUIRED)`, `pkg_check_modules(GTK4 REQUIRED gtk4>=4.22)`, link against `GTK4_LIBRARIES` and `OpenGL::GL`, `install(TARGETS orender-wire DESTINATION bin)`
- [ ] T006 [P] Create `tests/preview/CMakeLists.txt`: test executable for libribpreview unit tests, linked against `libribpreview` and `ri`
- [ ] T007 Add `add_subdirectory(preview)` to `src/CMakeLists.txt` (after existing `add_subdirectory(ri)` line) and `add_subdirectory(preview)` to `tests/CMakeLists.txt` if it exists, or register `tests/preview/` with CTest

**Checkpoint**: `cmake --build build --target libribpreview` resolves without linker errors (no source files yet — empty library is acceptable at this stage).

---

## Phase 2: Foundational (libribpreview — C++ Geometry Extraction Layer)

**Purpose**: The shared C++ static library that all platform binaries depend on. Must be complete and tested before any platform-specific rendering work begins.

**⚠️ CRITICAL**: No user story work can begin until this phase is complete. TDD is mandatory — write every test task first, verify it fails, then implement.

### Tests — Write First, Verify Failing

- [ ] T008 Write `tests/preview/test_polygon.cpp`: tests that a synthetic `CPolygonMesh` (triangle and quad face) produces the expected edge count and that all output vertices are in world space after applying a known `CXform::from` transform
- [ ] T009 [P] Write `tests/preview/test_patch.cpp`: tests that `CBilinearPatch` produces exactly 4 edges (boundary quad) and `CBicubicPatch` produces 9×8 + 8×9 = 144 line segments from an 8×8 parameter grid
- [ ] T010 [P] Write `tests/preview/test_quadric.cpp`: tests that `CSphere` (radius=1, full sweep) produces 24×12 non-degenerate vertices; verify no NaN or zero-length edges
- [ ] T011 [P] Write `tests/preview/test_nurbs.cpp`: tests that `CNURBSPatch` De Boor evaluation on a 12×12 grid produces non-degenerate vertices within the expected bounding range
- [ ] T012 [P] Write `tests/preview/test_camera.cpp`: tests that `CPreviewContext` loaded with a minimal perspective RIB (known fov, aspect) produces a `PreviewCamera` with the expected `projMatrix` and `viewMatrix` values (tolerance 1e-4)

### Data Structures and C API

- [ ] T013 [P] Define `src/preview/libribpreview/previewTypes.h`: `struct float3`, `enum ProjectionType { Perspective, Orthographic }`, `struct PreviewCamera` (projMatrix[16], viewMatrix[16], nearPlane, farPlane, projectionType, fov, frameAspectRatio), `struct AABB` (min, max), `struct PreviewScene` (vertices: `std::vector<float3>`, camera: PreviewCamera, sceneBounds: AABB, warnings: `std::vector<std::string>`)
- [ ] T014 [P] Define `src/preview/ribpreview_api.h`: `extern "C"` flat API — `PreviewCameraC` and `PreviewSceneC` structs (column-major matrices as `float[16]`), `PreviewSceneC *ribpreview_load(const char *ribPath)`, `void ribpreview_free(PreviewSceneC *scene)`; note that `ribpreview_load` transposes all matrices from row-major (C++) to column-major (Metal/GL) before returning

### CPreviewContext Core

- [ ] T015 Define `class CPreviewContext : public CRendererContext` in `src/preview/libribpreview/previewContext.h`: override `addObject(CObject *)`, `RiWorldBegin()`, and `RiWorldEnd()`; store `PreviewScene result_` and `PreviewCamera ribCamera_`
- [ ] T016 Implement `CPreviewContext::RiWorldBegin()` in `src/preview/libribpreview/previewContext.cpp`: call `CRendererContext::RiWorldBegin()` then extract camera from `COptions` — projection type, fov, frameAspectRatio, clipMin/clipMax, camera `CXform::to` (world→camera view matrix); build projection matrix; store in `ribCamera_`
- [ ] T017 Implement `CPreviewContext::addObject(CObject *obj)` dispatch in `src/preview/libribpreview/previewContext.cpp`: `dynamic_cast` chain to identify primitive type and call the appropriate tessellator; accumulate results into `result_.vertices`; update `result_.sceneBounds`; if no cast matches, log warning to stderr (depends on T013, T015, T016)

### Tessellators (all parallel after T013)

- [ ] T018 [P] Implement `src/preview/libribpreview/tessellators/tessPolygon.cpp`: extract `CPl::data0` (P array) + `nvertices[]` face vertex counts + `vertices[]` index list from `CPolygonMesh`; apply `CXform::from` to each vertex; emit one edge per consecutive vertex pair per face, closing each face
- [ ] T019 [P] Implement `src/preview/libribpreview/tessellators/tessPatch.cpp`: bilinear patch — 4 corners at (0,0),(1,0),(0,1),(1,1) mapped through the 4-point control mesh → 4 boundary edges; bicubic patch — standalone 8×8 Bernstein evaluation of the 16-point control grid (no `dice()` call) → 144 line segments; handle `CPatchMesh` by iterating patches
- [ ] T020 [P] Implement `src/preview/libribpreview/tessellators/tessNurbs.cpp`: De Boor knot-vector evaluation on a 12×12 (u,v) parameter grid using `uknot[]`, `vknot[]`, `uorder`, `vorder`, `umin/umax`, `vmin/vmax` from `CNURBSPatch`; emit grid edges; apply `CXform::from`
- [ ] T021 [P] Implement `src/preview/libribpreview/tessellators/tessQuadric.cpp`: parametric sin/cos sampling for all 7 quadric types — `CSphere` (24 longitude × 12 latitude), `CCone`, `CCylinder`, `CDisk` (polar sampling), `CTorus` (major × minor sweep), `CParaboloid`, `CHyperboloid`; apply `CXform::from` to each sample point; respect `thetamax`, `zmin`, `zmax` parameters
- [ ] T022 [P] Implement `src/preview/libribpreview/tessellators/tessCurve.cpp`: for each curve in `CCurveMesh`, read `nverts[]` vertex counts and the flat vertex array from `CPl::data0`; emit consecutive vertex pairs as line segments; handle periodic wrap (close last edge back to curve start when `wrap == "periodic"`)
- [ ] T023 [P] Implement `src/preview/libribpreview/tessellators/tessPoints.cpp`: emit a 3-axis cross-hair per point (6 vertices per point: ±X, ±Y, ±Z through each position); if `npts > 100000`, subsample with stride `npts / 100000` and emit a `warning()` to stderr with the original count; apply `CXform::from`
- [ ] T024 [P] Implement `src/preview/libribpreview/tessellators/tessSubdivision.cpp`: read `vertices[]` and `nvertices[]` from `CSubdivision`; emit face boundary edges (control cage, same logic as polygon mesh); no refinement
- [ ] T025 [P] Implement `src/preview/libribpreview/tessellators/tessProc.cpp`: for `RiProcedural`, read the `bound[6]` array and emit 12 edges of the axis-aligned bounding box wireframe as a placeholder

### C API Entry Point

- [ ] T026 Implement `ribpreview_load()` and `ribpreview_free()` in `src/preview/libribpreview/previewContext.cpp`: instantiate `CPreviewContext`, call `RiBegin`/`RiEnd` pipeline on the RIB file path, transpose all matrices from row-major to column-major, pack into `PreviewSceneC`, return; implement synthesized default camera when `result_.camera` is absent (frame `sceneBounds`); `ribpreview_free()` deletes the heap struct (depends on T017–T025)

### Test Gate

- [ ] T027 Run `ctest --test-dir build -R preview`; **all tests from T008–T012 must pass**; fix tessellators until they do before proceeding to Phase 3

**Checkpoint**: `ribpreview_load("examples/rib/camera-dof.rib")` returns a non-null scene with `vertexCount > 0` and `camera.nearPlane > 0`.

---

## Phase 3: User Story 1 — Open and Inspect a RIB Scene (Priority: P1) 🎯 MVP

**Goal**: A native window opens with all RIB geometry displayed as wireframe, reference grid, and XYZ axis indicators. The initial viewpoint matches the RIB camera.

**Independent Test**: `orender-wire examples/rib/camera-dof.rib` — window opens within 1 second showing a loading indicator, geometry appears within 5 seconds, ground-plane grid and XYZ axis gizmo are visible, initial camera matches the RIB scene camera.

### macOS — Swift 6 / Metal / AppKit

- [ ] T028 [US1] Create `src/preview/orender-wire-macos/Info.plist`: `CFBundleName = orender-wire`, `CFBundleExecutable = orender-wire`, `NSPrincipalClass = NSApplication`, `LSUIElement = NO` (appears in Dock), `NSHighResolutionCapable = YES`
- [ ] T029 [US1] Create `src/preview/orender-wire-macos/Sources/ribpreview_bridge.h` bridging header: `#include "../../../../ribpreview_api.h"` (relative path from Sources/ to src/preview/); set `SWIFT_OBJC_BRIDGING_HEADER` in Package.swift or CMakeLists.txt
- [ ] T030 [US1] Write `src/preview/orender-wire-macos/Sources/Shaders.metal`: **Pass 1** — vertex shader reads `float3` from position buffer, applies `float4x4 mvp` uniform (MVP matrix), outputs `float4 position`; fragment shader outputs `float4(0.85, 0.85, 0.85, 1.0)` (light-grey wire color). **Pass 2** — vertex shader for ground-plane grid (Y=0 plane, 20×20 lines at 1-unit spacing) and XYZ axis gizmo (3 colored lines from origin). Depth buffer enabled (`MTLPixelFormatDepth32Float`, `depthCompareFunction = .less`, `isDepthWriteEnabled = true`)
- [ ] T031 [US1] Implement `src/preview/orender-wire-macos/Sources/WireframeRenderer.swift`: `MTKView` subclass; `init` uploads line-list from `PreviewSceneC.vertices` to a single `MTLBuffer` (stored vertex count); `draw(in:)` encodes one `MTLRenderCommandEncoder` per frame — sets `mvp` push constant, calls `drawPrimitives(.line, vertexStart: 0, vertexCount: vertexCount)`, then draws grid+axis gizmo; no per-frame CPU tessellation
- [ ] T032 [US1] Implement `src/preview/orender-wire-macos/Sources/AppDelegate.swift`: `NSApplicationDelegate`; `applicationDidFinishLaunching` creates `NSWindow` (not `NSPanel`), sets title to RIB filename basename, makes window key and front, shows loading indicator (`NSProgressIndicator`, style: .spinning) centered in the window
- [ ] T033 [US1] Implement `src/preview/orender-wire-macos/Sources/main.swift`: parse `CommandLine.arguments[1]` as RIB path; validate file exists (exit code 2 on failure, before `NSApplication.shared.run()`); call `ribpreview_load()` on a background `DispatchQueue`; on completion dispatch to main queue — if the returned pointer is nil, print `"orender-wire: error: RIB parse failed (see above)"` to stderr and call `exit(3)`; otherwise replace loading indicator with `WireframeRenderer` (MTKView), pass `PreviewSceneC` to renderer, apply `PreviewCameraC` matrices as initial MVP; call `NSApplication.shared.run()`

### Linux — C++20 / OpenGL 3.3 Core

- [ ] T034 [P] [US1] Implement GTK 4 window creation in `src/preview/orender-wire-linux/main.cpp`: in `main()`, validate `argv[1]` exists and is readable — if not, print `"orender-wire: cannot open '<path>': <strerror>"` to stderr and `exit(2)` before any GTK initialization; heap-copy the RIB path and pass it as `user_data` to `g_signal_connect(app, "activate", G_CALLBACK(on_activate), rib_path)` — inside `on_activate`, cast `user_data` back to `const char *` to retrieve the path; create a `GtkWindow` with a `GtkGLArea` child widget requesting an OpenGL 3.3 Core profile via `gtk_gl_area_set_required_version(area, 3, 3)`; set window title to `"orender-wire — " + basename(rib_path)`; GTK 4 selects Wayland or X11 backend automatically via `WAYLAND_DISPLAY` / `DISPLAY`; if `g_application_run` returns with no display, exit code 4
- [ ] T035 [P] [US1] Write GLSL 3.30 core vertex + fragment shaders inline in `src/preview/orender-wire-linux/main.cpp`: vertex — `layout(location=0) in vec3 position; uniform mat4 mvp; gl_Position = mvp * vec4(position, 1.0);`; fragment — `out vec4 color; color = vec4(0.85, 0.85, 0.85, 1.0);`; grid and axis gizmo as additional draw calls with color uniforms; depth test enabled (`glEnable(GL_DEPTH_TEST)`, `glDepthFunc(GL_LESS)`)
- [ ] T036 [US1] Implement VAO/VBO setup and render callbacks in `src/preview/orender-wire-linux/main.cpp`: connect `GtkGLArea::realize` signal — create VAO/VBO; connect `GtkGLArea::render` signal — `glClear`, set `mvp` uniform, `glDrawArrays(GL_LINES, 0, vertexCount)`, grid+axis draws; overlay a `GtkSpinner` while loading; call `ribpreview_load(argv[1])` on a background `GTask` thread (exit code 3 on NULL return); on completion, upload `PreviewSceneC.vertices` to VBO and hide spinner (depends on T034, T035)

**Checkpoint US1**: `orender-wire examples/rib/camera-dof.rib` opens a window, shows loading indicator, then displays wireframe geometry with ground-plane grid and XYZ axis gizmo.

---

## Phase 4: User Story 2 — Navigate the Scene Interactively (Priority: P1)

**Goal**: Left drag → orbit, scroll → zoom, middle drag → pan, R/Home → reset. ≥30 fps. Depth-correct occlusion (already provided by depth buffer from Phase 3).

**Independent Test**: Open any scene; exercise orbit, zoom, and pan; verify smooth response and that R restores the original RIB camera.

### Tests — Write First, Verify Failing

- [ ] T061 Write `tests/preview/test_arcball.cpp`: (a) `toSphere(center_x, center_y)` returns `(0,0,1)` (the near pole) for the window center; corner inputs land on the unit sphere (magnitude within 1e-5 of 1.0); (b) `orbit(p1, p2)` returns a unit quaternion and applying two sequential drags accumulates correctly (compose quaternions, compare against direct cross-product result); (c) `zoom(delta)` increases `distance` for positive delta and decreases for negative delta, never going below a minimum positive value; (d) `pan(dx, dy)` shifts `orbitCenter` proportionally in camera-local XY; (e) after `reset()`, `viewProjectionMatrix()` matches the original `ribCamera` matrices to within 1e-5; (f) `viewProjectionMatrix()` is non-degenerate (determinant ≠ 0) for a standard perspective setup (fov=45°, aspect=4/3, near=0.1, far=1000)

### macOS — ArcballCamera (port from src/gui/interface.h)

- [ ] T037 [US2] Implement `src/preview/orender-wire-macos/Sources/ArcballCamera.swift`: port `CInterface` from `src/gui/interface.h` — `toSphere(_ x: Float, _ y: Float) -> simd_float3` maps pixel position to unit sphere; `orbit(from:to:)` computes drag quaternion via cross product and `simd_quatf(ix:iy:iz:r:)`; `zoom(delta:)` scales `distance`; `pan(dx:dy:)` offsets `orbitCenter` in camera-local XY; `reset()` restores `ribCamera` matrices; computed property `viewProjectionMatrix: simd_float4x4` multiplies projection × view each call. `PreviewCameraC` matrices are already column-major (transposed inside `ribpreview_load`) — read them directly into `simd_float4x4` without any additional transpose
- [ ] T038 [US2] Add `NSResponder` mouse event handling to `WireframeRenderer.swift` (or a subclass of `NSView` wrapping `MTKView`): `mouseDown` → begin orbit drag (save `savedOrientation`); `mouseDragged` → call `arcball.orbit(from:to:)`; `scrollWheel` → `arcball.zoom(delta: event.scrollingDeltaY)`; `otherMouseDown/Dragged` → `arcball.pan(dx:dy:)` (middle button); `keyDown` — R or Home → `arcball.reset()` (depends on T037)
- [ ] T039 [US2] Add trackpad gesture recognizers to `WireframeRenderer` in `src/preview/orender-wire-macos/Sources/WireframeRenderer.swift`: `NSEvent.magnification` → `arcball.zoom(delta:)` (pinch-to-zoom); two-finger pan via `NSEvent.scrollingDelta` when `hasPreciseScrollingDeltas == true` and phase is `.changed` → `arcball.pan(dx:dy:)` (depends on T037, T038)
- [ ] T040 [US2] Wire `arcball.viewProjectionMatrix` into `WireframeRenderer.draw(in:)` as the `mvp` Metal uniform in `src/preview/orender-wire-macos/Sources/WireframeRenderer.swift`; trigger `mtkView.setNeedsDisplay(true)` on each input event (depends on T037, T038)

### Linux — C++ ArcballCamera

- [ ] T041 [P] [US2] Implement `src/preview/orender-wire-linux/arcball.h` + `arcball.cpp`: C++20 inline `toSphere`, `orbit`, `zoom`, `pan`, `reset`, `viewProjectionMatrix()`; same math as Swift version; uses `float[16]` column-major matrices (no simd dependency)
- [ ] T042 [US2] Wire GTK 4 input events to `ArcballCamera` in `src/preview/orender-wire-linux/main.cpp`: add `GtkGestureClick` (button 1 pressed/released/moved → orbit; button 2 moved → pan), `GtkEventControllerMotion` (motion → orbit or pan depending on active gesture), `GtkEventControllerScroll` (scroll → zoom), `GtkEventControllerKey` (R or Home → `arcball.reset()`); call `gtk_widget_queue_render` on `GtkGLArea` after each update (depends on T041)

### src/gui/ Removal

- [ ] T043 [US2] Delete `src/gui/` from the repository after `ArcballCamera.swift` and `arcball.cpp` pass code review; verify `src/CMakeLists.txt` has no `add_subdirectory(gui)` entry before deleting; `git rm -r src/gui/` (depends on T037, T041)

**Checkpoint US2**: Interactive navigation at ≥30 fps; R/Home resets to RIB camera; depth occlusion correct.

---

## Phase 5: User Story 3 — Window Resize and Application Lifecycle (Priority: P2)

**Goal**: Window resizes freely with correct aspect ratio; app appears in Dock (macOS); exits cleanly on ⌘Q, Escape, Q, or window close.

**Independent Test**: Resize window through several sizes; confirm wireframe fills window and aspect ratio is preserved. Quit via all supported methods; verify no leaked processes.

- [ ] T044 [US3] Handle `mtkView(_:drawableSizeWillChange:)` in `src/preview/orender-wire-macos/Sources/WireframeRenderer.swift`: update `arcball.windowSize` and recompute `arcball.radius = sqrt(w²+h²)*0.5`; update projection matrix aspect ratio; call `mtkView.setNeedsDisplay(true)` (depends on T040)
- [ ] T045 [P] [US3] Handle resize via `GtkGLArea::resize` signal in `src/preview/orender-wire-linux/main.cpp`: signal delivers `(width, height)` directly; call `glViewport(0, 0, width, height)`; update `arcball.windowSize` and `arcball.radius`; update projection matrix aspect ratio; call `gtk_widget_queue_render` (depends on T042)
- [ ] T046 [P] [US3] Implement clean macOS shutdown in `src/preview/orender-wire-macos/Sources/AppDelegate.swift`: `applicationWillTerminate` — call `ribpreview_free()`, release `MTLBuffer`, release `MTLDevice`; `applicationShouldTerminateAfterLastWindowClosed` returns `true` so closing the window quits the app; exit code 0
- [ ] T047 [P] [US3] Implement clean Linux shutdown in `src/preview/orender-wire-linux/main.cpp`: connect `GtkWindow::close-request` signal → call `ribpreview_free()`, delete VAO/VBO, return `FALSE` to allow default close; Esc/Q via `GtkEventControllerKey` → call `gtk_window_close()`; GTK 4 handles all Wayland and X11 close-button events transparently; exit 0 (depends on T036)

**Checkpoint US3**: All resize and quit paths exercised cleanly.

---

## Phase 6: User Story 4 — Export Current Camera to RIB File (Priority: P3)

**Goal**: S key opens native file save dialog; writes current arcball camera viewpoint as a valid RIB camera block (new file) or replaces the camera section of an existing file.

**Independent Test**: Navigate to a custom viewpoint, trigger save, reopen the output RIB in orender-wire, confirm initial camera matches.

### Tests — Write First, Verify Failing

- [ ] T048 Write `tests/preview/test_camera_export.cpp`: test that `writeRibCamera(path, camera)` creates a valid file containing `Projection` and `Transform` statements that reproduce the input matrix; load the file back and compare matrices (tolerance 1e-5)
- [ ] T049 [P] Write `tests/preview/test_camera_replace.cpp`: test that `replaceRibCamera(existingPath, camera)` updates the `Projection` and `Transform` in the pre-`WorldBegin` section while leaving all other content byte-identical

### Implementation

- [ ] T050 Implement `src/preview/libribpreview/cameraExport.h` + `cameraExport.cpp`: `struct CameraExport` (cameraToWorld[16], projectionType, fov, outputPath, updateExisting); `writeRibCamera(const CameraExport &)` — writes minimal RIB snippet (header comment, `Projection`, `Transform` with 16-float row-major matrix); `replaceRibCamera(const CameraExport &)` — reads existing file, locates pre-`WorldBegin` `Projection`/`Transform`, replaces them, writes all other content unchanged; inserts before `WorldBegin` if no existing camera block is found
- [ ] T051 Run `ctest --test-dir build -R camera_export`; tests from T048–T049 must pass (depends on T050)
- [ ] T052 [US4] Wire camera export to S key on macOS in `src/preview/orender-wire-macos/Sources/ArcballCamera.swift` or `WireframeRenderer.swift`: `keyDown` S → open `NSSavePanel` (configured for `.rib` extension); on `NSModalResponse.OK` → invert current view matrix → construct `CameraExport` → call through bridging to `replaceRibCamera` or `writeRibCamera` depending on whether the chosen path exists (depends on T050)
- [ ] T053 [P] [US4] Wire camera export to S key on Linux in `src/preview/orender-wire-linux/main.cpp`: `GtkEventControllerKey` S → open `GtkFileDialog` (GTK 4.10+, available in GTK ≥4.22) via `gtk_file_dialog_save()`; set initial filename to `"camera.rib"` and filter to `*.rib`; in the async callback, construct `CameraExport` from current arcball state and call `writeRibCamera` or `replaceRibCamera` depending on whether the chosen path exists; no external tool dependency (depends on T050)

**Checkpoint US4**: Save → reload → same camera verified on both platforms.

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: CLI contract compliance, site documentation, integration validation.

- [ ] T054 [P] Implement `--help` flag on both platforms: print usage text per `contracts/cli-interface.md` to stdout, exit 0 (`src/preview/orender-wire-macos/Sources/main.swift` + `src/preview/orender-wire-linux/main.cpp`)
- [ ] T055 [P] Implement `--version` flag on both platforms: print `orender-wire <version>` to stdout, exit 0
- [ ] T056 [P] Audit and enforce all exit codes 0–4 per `contracts/cli-interface.md` on both platforms; add `static_assert`-level comments mapping each exit site to its code
- [ ] T057 [P] Audit all stderr warning messages per `contracts/cli-interface.md`: file-not-found, parse warnings, point cloud subsampling notices, procedural placeholder notices; ensure no warning text appears on stdout
- [ ] T058 [P] Write `site/content/tools/orender-wire.md` documentation page (Constitution VII gate): invocation syntax, controls table (keyboard + mouse), environment variables, camera export workflow, platform notes
- [ ] T059 Write `tests/preview/test_integration.cpp`: integration test — call `ribpreview_load("examples/rib/camera-dof.rib")`; assert `vertexCount > 0`, `camera.nearPlane > 0`, `camera.farPlane > camera.nearPlane`; assert all vertices are finite (no NaN/Inf)
- [ ] T060 Run the full test suite (`cmake --build build --config Release && ctest --test-dir build --output-on-failure`) and validate all quickstart.md scenarios manually on macOS; document any failures

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1, T001–T007)**: No dependencies — start immediately
- **Foundational (Phase 2, T008–T027)**: Requires Setup complete — **blocks all user stories**
- **US1 (Phase 3, T028–T036)**: Requires Phase 2 complete
- **US2 (Phase 4, T061, T037–T043)**: Requires Phase 3 complete (needs the window and renderer); T061 (arcball test) must be written and failing before T037/T041
- **US3 (Phase 5, T044–T047)**: Requires Phase 3 complete; can run in parallel with US2
- **US4 (Phase 6, T048–T053)**: Requires Phase 4 complete (needs arcball camera state)
- **Polish (Phase 7, T054–T060)**: Requires all desired stories complete

### User Story Dependencies

- **US1 (P1)**: Depends on Foundational only
- **US2 (P1)**: Depends on US1 (requires a window to navigate)
- **US3 (P2)**: Depends on US1 (resize requires a window); **can start in parallel with US2**
- **US4 (P3)**: Depends on US2 (exports the current arcball camera state)

### TDD Order Within Phase 2

Tests (T008–T012) → Data structures (T013–T014) → Core context (T015–T017) → Tessellators (T018–T025, all parallel) → C API (T026) → Test gate (T027)

### Parallel Opportunities

Within **Phase 1**: T003, T004, T005, T006, T007 can all run in parallel after T001–T002.

Within **Phase 2**: T008–T012 (tests) all parallel; T013–T014 parallel; T018–T025 (tessellators) all parallel.

Within **Phase 3**: T030, T031, T032 (macOS) can run in parallel with T034, T035 (Linux).

Within **Phase 4**: T061 (arcball test) runs first; T041 (Linux arcball) can run in parallel with T037–T040 (macOS arcball) after T061.

Within **Phase 5**: T045, T046, T047 all parallel after T044 starts.

Within **Phase 7**: T054–T059 all parallel.

---

## Parallel Example: Phase 2 (Tessellators)

```
# After T015–T017 (addObject dispatch) is complete, launch all tessellators together:
Task T018: tessPolygon.cpp
Task T019: tessPatch.cpp
Task T020: tessNurbs.cpp
Task T021: tessQuadric.cpp
Task T022: tessCurve.cpp
Task T023: tessPoints.cpp
Task T024: tessSubdivision.cpp
Task T025: tessProc.cpp
# Then converge on T026 (C API) → T027 (test gate)
```

## Parallel Example: Phase 3 (US1 — macOS + Linux together)

```
# After Phase 2 completes, launch both platform tracks in parallel:
Track A (macOS): T028 → T029 → T030 → T031 → T032 → T033
Track B (Linux): T034 + T035 in parallel → T036
```

---

## Implementation Strategy

### MVP First (US1 + US2 Only)

1. Complete Phase 1: Setup
2. Complete Phase 2: Foundational (CRITICAL — blocks everything)
3. Complete Phase 3: US1 — get geometry on screen
4. Complete Phase 4: US2 — add navigation
5. **STOP and VALIDATE**: `orender-wire examples/rib/camera-dof.rib` — full interactive wireframe viewer
6. Ship as MVP; US3 and US4 add polish without breaking US1/US2

### Incremental Delivery

1. Setup + Foundational → `libribpreview` tested and green
2. + US1 → geometry on screen → demo-able (MVP!)
3. + US2 → fully interactive viewer → deploy
4. + US3 → production polish
5. + US4 → camera export workflow

### Parallel Team Strategy

After Phase 2 completes:
- Developer A: US1 macOS track (T028–T033)
- Developer B: US1 Linux track (T034–T036) + US3 Linux (T045, T047)
- Once US1 is merged: Developer A continues to US2 macOS (T037–T040); Developer B to US2 Linux (T041–T042)

---

## Notes

- `[P]` tasks target different files and have no dependency on any in-progress sibling task
- TDD is mandatory (Constitution III): tests must be written and **failing** before implementation
- `src/gui/` is not in the build system — verify before deletion (T043)
- All matrices cross the C++/Swift boundary as column-major (`float[16]`) — the transpose happens once inside `ribpreview_load()`
- Exit codes must match `contracts/cli-interface.md` exactly
- `stdout` must be silent during normal operation — all output goes to `stderr` or the window
- Commit after each phase checkpoint; use `/speckit-git-commit` between phases
