# Feature Specification: orender-wire — Interactive Scene Wireframe Previewer

**Feature Branch**: `006-scene-wireframe-viewer`

**Created**: 2026-05-18

**Status**: Draft

**Input**: User description: "openRender needs a standalone scene previewer tool, orender-wire, that allows artists and technical directors to inspect the geometry of a RIB scene file interactively before committing to a full render."

## Clarifications

### Session 2026-05-18

- Q: Should the GPU rendering and windowing use a cross-platform library (GLFW/SDL + OpenGL) or platform-native APIs on each OS? → A: Platform-native — Metal + Cocoa on macOS; OpenGL 3.3 Core via GTK 4 (≥4.22) on Linux (GtkGLArea for context + windowing, GtkFileDialog for save dialogs). Scene-processing layer (parsing and tessellation) is shared.
- Q: How should the "Save camera" destination path be specified — native file dialog, terminal prompt, or command-line argument? → A: Native OS file save dialog (standard modal dialog on macOS and Linux).
- Q: What does the user see while scene parsing and tessellation run at startup? → A: Window opens immediately showing a loading indicator (spinner or progress text); geometry appears once tessellation completes.
- Q: How should dense point clouds be handled — display all points, subsample above a threshold, or replace with bounding box? → A: Subsample above a fixed maximum (e.g., 100,000 points); excess points are uniformly subsampled and a warning is emitted.
- Q: Should the tool print diagnostic output during normal operation? → A: Warnings to stderr only, by default; stdout is never written during normal operation.

### Session 2026-05-21

- Q: Does orender-wire require `ORENDERHOME`, `SHADERS`, or `DISPLAYS` to be set? → A: No. Those variables are consumed by the full renderer's display plugin and shader loading subsystems, which the wireframe previewer bypasses entirely. orender-wire works on a machine with none of those configured.
- Q: What happens when a RIB file uses the `Geometry "name"` statement and `GEOMETRIES` is not set? → A: The named primitive is skipped; a notice is written to stderr. No crash, no non-zero exit.
- Q: What does orender-wire do with `Surface`, `LightSource`, `Imager`, `Display`, and other non-geometry RIB statements? → A: Silently ignores them. The tool processes geometry only; shading, lighting, and display setup are irrelevant and must not produce errors or warnings.

## User Scenarios & Testing *(mandatory)*

### User Story 1 — Open and Inspect a RIB Scene (Priority: P1)

An artist or technical director invokes `orender-wire scene.rib` from the command line. A GPU-accelerated window opens showing all scene geometry as a wireframe, positioned correctly in world space, with a reference grid and axis indicators. The initial viewpoint matches the camera defined in the RIB file.

**Why this priority**: This is the core value proposition — getting geometry on screen with zero configuration. Everything else builds on this.

**Independent Test**: Run the tool against any valid RIB scene; the window opens, displays geometry in wireframe, and shows the correct initial camera viewpoint. Delivers immediate value even without any interaction.

**Acceptance Scenarios**:

1. **Given** a valid RIB file with at least one geometric primitive, **When** the user runs `orender-wire scene.rib`, **Then** a window opens immediately titled with the RIB file name, shows a loading indicator while tessellation runs, and then displays all geometry as wireframe edges over a dark background with a ground-plane reference grid and XYZ axis indicators at the origin.
2. **Given** a RIB file that defines a perspective or orthographic camera, **When** the window opens, **Then** the initial viewpoint, field of view, and aspect ratio match the RIB camera definition.
3. **Given** a RIB scene with a full transform hierarchy (nested coordinate systems), **When** the scene is displayed, **Then** every primitive appears at its correct world-space position and orientation.
4. **Given** a RIB scene containing procedural or DSO-backed geometry, **When** the scene is displayed, **Then** each such primitive is shown as an axis-aligned bounding box placeholder without crashing.

---

### User Story 2 — Navigate the Scene Interactively (Priority: P1)

Once the scene is visible, the artist orbits, zooms, and pans using the mouse or trackpad to examine geometry from any angle. Geometry closer to the camera occludes geometry further away. The camera navigation runs at a smooth, responsive frame rate.

**Why this priority**: Inspection without navigation is nearly useless. This story shares P1 with scene display because together they constitute the minimum viable tool.

**Independent Test**: Open any scene and exercise all three navigation modes (orbit, zoom, pan) — confirm continuous smooth response and correct depth-based occlusion.

**Acceptance Scenarios**:

1. **Given** an open scene, **When** the user drags the left mouse button, **Then** the view orbits around the scene center in proportion to cursor movement.
2. **Given** an open scene, **When** the user scrolls the mouse wheel (or performs a two-finger trackpad scroll on macOS), **Then** the view zooms in and out smoothly.
3. **Given** an open scene, **When** the user drags the middle mouse button (or two-finger drags with the appropriate gesture on macOS), **Then** the view pans laterally.
4. **Given** any navigation state, **When** the user presses the reset-view shortcut (R or Home), **Then** the viewpoint returns to the original RIB-defined camera.
5. **Given** a scene with overlapping geometry, **When** the user navigates, **Then** geometry closer to the camera visibly occludes geometry further away.

---

### User Story 3 — Window Resize and Application Lifecycle (Priority: P2)

The artist resizes the window; the wireframe view scales to fill the new size while maintaining the correct aspect ratio. On macOS the tool appears in the Dock. The tool exits cleanly when the window is closed or the standard quit shortcut is used.

**Why this priority**: Expected native-application polish. Valuable but does not block scene inspection.

**Independent Test**: Resize the window to several different sizes; confirm aspect ratio is preserved and no rendering artifacts appear. Quit via menu/shortcut and confirm clean exit.

**Acceptance Scenarios**:

1. **Given** an open scene, **When** the user resizes the window, **Then** the wireframe view fills the new area and the aspect ratio remains correct.
2. **Given** the application is running on macOS, **When** it is open, **Then** it appears in the Dock.
3. **Given** the application is running, **When** the user closes the window or presses the platform quit shortcut (⌘Q on macOS, Escape or Q on Linux), **Then** the application exits cleanly with no error output.

---

### User Story 4 — Export the Current Camera to a RIB File (Priority: P3)

After navigating to a desired viewpoint, the artist saves the current camera position back to a RIB file. The tool writes the camera transform in the correct RIB camera block, in the correct coordinate space. The output can be a new file or replace the camera section of an existing file.

**Why this priority**: Useful for iterating on shot framing, but the tool has full utility without it. Deferred to avoid blocking the P1/P2 path.

**Independent Test**: Navigate to a custom viewpoint, trigger the export, then open the output RIB in orender-wire and confirm the initial camera matches the exported position.

**Acceptance Scenarios**:

1. **Given** the user is at a custom viewpoint, **When** they trigger "Save camera" (e.g., S key or via a menu), **Then** a native OS file save dialog opens for the user to choose a destination file path.
2. **Given** a destination path pointing to a new file, **When** the save completes, **Then** the output RIB contains a valid camera block that reproduces the current viewpoint when loaded.
3. **Given** a destination path pointing to an existing RIB file, **When** the save completes, **Then** the camera section of that file is updated in place while all other scene content is preserved.
4. **Given** the exported RIB is opened in orender-wire, **When** the window opens, **Then** the initial viewpoint matches the viewpoint that was exported.

---

### Edge Cases

- What happens when the RIB file path is invalid or the file does not exist? The tool prints a clear error message to stderr and exits with a non-zero code.
- What happens when the RIB file is syntactically invalid? The tool reports the parse error and, if partial geometry was loaded, displays what was successfully parsed with a warning.
- What happens when no geometry is present in the RIB (cameras and lights only)? The tool opens the window showing the reference grid and axis indicators with an empty scene, without crashing.
- What happens with extremely large scenes (hundreds of thousands of primitives)? Tessellation is performed once at startup; interactive frame rate may degrade but the tool must not crash or produce corrupted output.
- What happens when the RIB defines motion blocks? The tool displays geometry at the first time sample (t=0) only, ignoring motion.
- What happens when subdivision surfaces are present? The tool displays the control cage in wireframe without performing subdivision.
- What happens when the RIB does not define a camera? The tool falls back to a default perspective viewpoint that frames all geometry.
- What happens on Linux when neither X11 nor Wayland is available? The tool prints a clear error and exits gracefully.
- What happens when a point cloud primitive contains more than 100,000 points? The tool uniformly subsamples the point cloud to 100,000 points, prints a warning to stderr identifying the primitive and the original count, and displays the subsampled result.
- What happens when the RIB uses a `Geometry "name"` statement and `GEOMETRIES` is not set? The named primitive is silently skipped and a notice is written to stderr; the rest of the scene is displayed normally.
- What happens when the RIB contains `Surface`, `LightSource`, `Imager`, `Display`, `Atmosphere`, or other non-geometry statements? They are silently ignored. The tool is geometry-only; these statements have no effect on wireframe preview.

## Requirements *(mandatory)*

### Functional Requirements

**Scene Loading**

- **FR-001**: The tool MUST accept a single RIB file path as its sole command-line argument and load the scene described therein.
- **FR-002**: The tool MUST parse the full transform hierarchy defined in the RIB file and apply it to each geometric primitive so that world-space positions are correct.
- **FR-003**: The tool MUST tessellate or approximate each supported primitive type into edge data suitable for wireframe display: polygon meshes (PointsPolygons, PointsGeneralPolygons), bilinear and bicubic patches (Patch, PatchMesh), NURBS surfaces (NuPatch), quadric primitives (Sphere, Cone, Cylinder, Disk, Torus, Paraboloid, Hyperboloid), curves (Curves), point clouds (Points — shown as cross-hair markers, subsampled to a maximum of 100,000 points with a warning when the source exceeds that count), and subdivision surfaces (SubdivisionMesh — shown as control cage).
- **FR-004**: The tool MUST represent procedural or DSO-backed geometry (Procedural) as axis-aligned bounding box wireframe placeholders.
- **FR-005**: The tool MUST perform all scene parsing and tessellation once at startup; no re-parsing or re-tessellation may occur during interactive navigation. The window MUST open immediately upon launch and display a loading indicator (spinner or progress text) while tessellation is in progress; the wireframe view replaces the indicator once loading completes.
- **FR-006**: When the RIB file cannot be found or read, the tool MUST print a descriptive error message to stderr and exit with a non-zero status code.
- **FR-007**: When the RIB file contains parse errors, the tool MUST report the error location and either display successfully parsed geometry with a warning or exit gracefully without crashing.
- **FR-021**: The tool MUST write all warnings (parse errors, unrecognised RIB statements, point cloud subsampling notices) to stderr. The tool MUST NOT write to stdout during normal operation; stdout is reserved for explicit future machine-readable output modes.
- **FR-022**: The tool MUST launch and display scene geometry without requiring `ORENDERHOME`, `SHADERS`, or `DISPLAYS` environment variables to be set. Those variables are consumed by the full renderer's shader and display subsystems and must not be prerequisites for wireframe preview.

**Camera and View**

- **FR-008**: The tool MUST reconstruct the scene camera from the RIB file, respecting projection type (perspective or orthographic), field of view, and pixel aspect ratio, and use it as the initial viewpoint.
- **FR-009**: When the RIB file defines no camera, the tool MUST synthesize a default perspective viewpoint that frames the full scene bounding box.
- **FR-010**: The tool MUST support arcball-style interactive camera navigation: left mouse drag → orbit around scene center; scroll wheel → zoom (dolly); middle mouse drag → pan.
- **FR-011**: On macOS, the tool MUST map native trackpad gestures to the corresponding navigation actions (two-finger scroll → zoom, two-finger drag → pan).
- **FR-012**: The user MUST be able to reset the view to the original RIB-defined camera at any time using a keyboard shortcut.
- **FR-013**: The user MUST be able to save the current camera viewpoint to a RIB file (new or existing) via a native OS file save dialog, with the camera transform written in correct RIB camera space and coordinate system.

**Wireframe Appearance**

- **FR-014**: Geometry MUST be rendered as wireframe edges in a clearly legible color over a dark background.
- **FR-015**: The display MUST include a reference grid on the ground plane (Y=0) and XYZ axis indicators at the scene origin.
- **FR-016**: Depth-correct rendering MUST be used so that geometry closer to the camera occludes geometry further away.

**Window and Application Behavior**

- **FR-017**: On macOS, the tool MUST open as a native application window, appear in the Dock, and respond to the standard quit shortcut (⌘Q).
- **FR-018**: On Linux, the tool MUST auto-detect and use the available display system (X11 or Wayland); the window MUST close on Escape, Q, or the window close button.
- **FR-019**: The window title MUST reflect the name of the loaded RIB file.
- **FR-020**: The tool MUST support free window resizing; the wireframe view MUST update to fill the window while preserving the correct aspect ratio.

### Key Entities

- **RIB Scene**: The parsed representation of a RenderMan Interface Bytestream file, containing an optional camera definition, a transform hierarchy, and a list of geometric primitives.
- **Geometric Primitive**: A single renderable element (mesh, patch, quadric, curve, point cloud, subdivision cage, or bounding box placeholder) in world space with associated tessellated edge data.
- **Interactive Camera**: The viewer's current viewpoint — defined by position, orientation, projection type, and field of view — manipulated via mouse/trackpad input and resettable to the RIB-defined camera.
- **Wireframe Mesh**: The GPU-side representation of a primitive as a set of line-segment edges derived from tessellation.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A native window opens within 1 second of invoking the tool, and all geometry is correctly positioned in wireframe within 5 seconds of invocation on a typical workstation (scene up to 50,000 primitives); a loading indicator is visible during any remaining load time.
- **SC-002**: Interactive camera navigation runs at no fewer than 30 frames per second on any scene that loads successfully on a modern workstation GPU.
- **SC-003**: All RenderMan-standard primitive types listed in FR-003 are representable; no supported primitive type causes the tool to crash or produce a blank scene.
- **SC-004**: The tool correctly reproduces the RIB-defined camera viewpoint (field of view, aspect ratio, and position) as the initial view, verifiable by visual comparison with a reference render.
- **SC-005**: A camera viewpoint saved by the export feature, when reloaded in orender-wire, reproduces the same initial view to within perceptual tolerance.
- **SC-006**: The tool exits cleanly (no unhandled exceptions, no leaked windows or processes) in all tested scenarios including invalid input files and window close during load.
- **SC-007**: Window resize at any size from 400×300 to the display's native resolution produces no rendering artifacts and maintains correct aspect ratio.

## Assumptions

- macOS is the primary development and release target; Linux support is built in parallel sharing the scene-processing layer but is secondary in validation priority.
- The macOS build uses a platform-native rendering pipeline (Metal) and native windowing (Cocoa/AppKit). The Linux build uses OpenGL 3.3 Core via GTK 4 (≥4.22): `GtkGLArea` provides the OpenGL context and manages Wayland/X11 backend selection transparently; `GtkFileDialog` provides the native file save dialog. The scene-processing layer — RIB parsing and geometry tessellation — is shared across both platforms.
- The tool reuses the existing openRender RIB parser and geometry tessellation infrastructure; it does not implement an independent parser.
- Motion blur (motion blocks) and deformation data are out of scope for the wireframe previewer; only the first time sample (t=0) is displayed.
- Shading, lighting, and texture evaluation are entirely out of scope; the tool is geometry-only.
- Subdivision surface refinement is out of scope for the first implementation; the control cage is sufficient for geometry inspection.
- The reference grid is drawn on the Y=0 plane of the viewer's own coordinate system. RIB files authored in Z-up DCCs (3ds Max, AutoCAD, Blender Z-up mode) include a Y↔Z axis-swap camera transform such as `Transform [1 0 0 0  0 0 1 0  0 1 0 0  0 0 0 1]`; the viewer handles these transparently — the grid always appears as the natural ground plane in the displayed scene regardless of the source coordinate convention.
- The camera export feature writes a minimal RIB snippet containing only the camera transform; it does not regenerate the full scene RIB.
- The tool is a separate binary (`orender-wire`) independent of the existing framebuffer display helper (`orender-fb`) and the main renderer (`orender`).
