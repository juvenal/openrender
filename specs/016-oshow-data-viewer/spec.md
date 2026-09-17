# Feature Specification: orender-wire Data-Structure Viewer (oshow Absorption)

**Feature Branch**: `016-oshow-data-viewer`

**Created**: 2026-09-01

**Status**: Draft

**Input**: User description: "Replace the dead `oshow` utility by absorbing its functionality into `orender-wire` as a second document type. `orender-wire` currently only opens RIB scene files and renders a flat wireframe (lines only). It should also be able to open openRender's precomputed 3D data-structure files — photon maps, irradiance caches, gather caches, point clouds, brick maps, and raw debug-geometry dumps — auto-detected by content, and visualize them with points, lines, triangles, and oriented discs (the primitive types `oshow` used to draw before its OpenGL module was deleted). The old FLTK-based `oshow` binary, the `CShow` hider, and all FLTK build dependencies are removed entirely — there is no compatibility shim. Interactive controls carried over from `oshow`: for brick maps, keys to change detail level (more/less), draw type (boxes/discs/points), and channel (previous/next); for point clouds, keys to toggle discs/points and change channel. These controls must be exposed both as real menu items (macOS) / header-bar controls (Linux) and as keyboard shortcuts, and their current state (channel name, detail level, draw mode) must be visible in the UI rather than printed to a terminal. The macOS app's shell moves from AppKit to SwiftUI (App/Scene/Commands) so it gains a real menu bar, while keeping its existing Metal renderer. The Linux app gains a populated header-bar menu (it currently has an empty one) using its existing GTK4/libadwaita/OpenGL stack. The CLI gains `--help`, `--version`, and a headless `--json` mode that prints scene/data statistics and exits without opening a window (no `ORENDERHOME`/`SHADERS`/`DISPLAYS` required, matching the existing RIB-loading path). Exactly one file is open at a time (no multi-window/multi-document model in this feature). Six existing test RIB scenes that named the dead `Hider "oshow:none"` are repointed at this new capability instead of asserting failure."

## Clarifications

### Session 2026-09-01

- Q: When a legacy single-letter shortcut (like `d` or `q`) is offered for a data-file control, should it stay a bare key with no modifier, or be namespaced behind a modifier to guarantee it never collides with a menu accelerator? → A: Keep legacy letters as plain, unmodified keys; offered only when relevant to the open document type, so no two controls ever compete for the same key at the same time.
- Q: Should the point-cloud/brick-map "hierarchical" variant that never had a working visualization (even before the legacy tool broke entirely) be built out with real visualization in this feature? → A: No — out of scope for this feature. It opens with a clear "not available" notice and no rendered content; real visualization is left to a future feature. **Superseded during `/speckit-analyze` (2026-09-01)**: source verification showed this variant cannot be selected by file content at all — it is a shading-time rendering strategy a shader requests at render time (`hierarchy=TRUE` passed to `CRenderer::getTexture3d`), never a distinguishable on-disk file format, and even the original tool's own dispatch never requested it. Since this feature's file-content detection (FR-001) can never produce it, the "not available" branch described here is unreachable and has been removed; see the retired FR-010 below. Every point-cloud and brick-map file this feature can detect now always visualizes (FR-002 is unconditional).
- Q: When a data file has too many primitives to display responsively, should detail reduction follow a fixed, deterministic rule or an adaptive one that may vary by machine? → A: Fixed and deterministic — a maximum primitive count with even sampling, so the same file always reduces the same way regardless of machine.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Inspect a precomputed data-structure file (Priority: P1)

A technical artist or renderer developer has produced an intermediate data file from a
render — a photon map, an irradiance or gather cache, a point cloud, a brick map, or a raw
debug-geometry dump — and wants to visually inspect its contents (positions, orientation,
density, per-channel values) to debug or verify a render. Today, the only tool that ever did
this is broken: it fails immediately with an error every time it is run. This story delivers a
working replacement: opening any of these file types displays its contents as a 3D scene the
user can orbit, pan, and zoom.

**Why this priority**: This is the entire reason the feature exists. Without it, the previous
tool's core capability — the only thing it was ever for — remains permanently unavailable.
Every other story in this feature builds on top of a file actually opening.

**Independent Test**: Can be fully tested by opening one file of each of the six supported
types and confirming each renders a non-empty, navigable 3D view with a shape appropriate to
its data (e.g., a point cloud shows points, a brick map shows boxes/discs/points). Delivers
value on its own even before any interactive controls exist.

**Acceptance Scenarios**:

1. **Given** a valid point-cloud file, **When** the user opens it, **Then** the application
   displays the cloud's points positioned and colored according to the file's data, framed so
   the whole dataset is visible.
2. **Given** a valid brick-map file, **When** the user opens it, **Then** the application
   displays its contents (as boxes, discs, or points, per current draw mode) rather than
   failing or showing a blank window.
3. **Given** a valid photon-map, irradiance-cache, gather-cache, or debug-geometry-dump file,
   **When** the user opens it, **Then** the application displays its contents without error.
4. **Given** a file that is not a recognized RIB scene or data-structure file, **When** the
   user attempts to open it, **Then** the application reports a clear error and does not crash
   or hang.
5. **Given** a data file created by an incompatible (older or newer) version of the software,
   **When** the user attempts to open it, **Then** the application reports that the file is
   from an incompatible version rather than displaying corrupted or nonsensical content.

---

### User Story 2 - Adjust the visualization through discoverable controls (Priority: P2)

While inspecting a point cloud or brick map, a user wants to cycle through the file's data
channels (e.g., different recorded quantities), change how densely a brick map is sampled
(its detail level), and switch how primitives are drawn (as points, discs, or boxes) — and
wants to do all of this through visible, discoverable controls, not by memorizing keyboard
letters and watching a terminal window for feedback that most users will never see.

**Why this priority**: The prior tool's controls existed only as undiscoverable keyboard
shortcuts with feedback printed to a terminal that a windowed application's user is unlikely
to be watching. Making these controls visible and menu-accessible is what turns this from "a
keyboard trick a developer remembers" into a real application feature.

**Independent Test**: Can be fully tested by opening a point cloud or brick map, using the
on-screen menu/toolbar controls (with no keyboard input) to change channel, detail level, and
draw mode, and confirming both the visualization and an on-screen indicator update each time.
Also independently testable by using the equivalent keyboard shortcuts and confirming the same
effect.

**Acceptance Scenarios**:

1. **Given** a point cloud with multiple data channels is open, **When** the user selects the
   next/previous channel from a menu or toolbar control, **Then** the visualization updates to
   reflect the new channel and the currently active channel's name is visible on screen.
2. **Given** a brick map is open, **When** the user changes its detail level via a menu or
   toolbar control, **Then** the visualization updates to the new level of detail and the
   current level is visible on screen.
3. **Given** a brick map or point cloud is open, **When** the user switches its draw mode
   (e.g., between points and discs, or boxes and discs), **Then** the visualization redraws
   using the new primitive type and the active mode is visible on screen.
4. **Given** any of the above actions is performed via its keyboard shortcut instead of a
   menu/toolbar control, **When** the shortcut is pressed, **Then** the same visualization and
   on-screen state update occurs.
5. **Given** a data file with no distinct channels (such as a debug-geometry dump), **When**
   the user opens it, **Then** channel-related controls are not offered, or are clearly shown
   as not applicable.

---

### User Story 3 - Retrieve file statistics without a graphical window (Priority: P3)

A developer or an automated test wants to confirm that a scene or data file loads correctly
and get basic facts about it (primitive counts, bounds, file type) without a display being
available — for example, in a continuous-integration run or a quick terminal check.

**Why this priority**: This makes the feature's correctness verifiable by automation, and
gives developers and scripts a fast way to sanity-check a file without launching a full
graphical application.

**Independent Test**: Can be fully tested by invoking the application in its headless mode
against a known scene file and a known data file, with no display or renderer environment
configured, and confirming it prints structured, correct information and exits successfully
without opening any window.

**Acceptance Scenarios**:

1. **Given** a valid RIB scene file, **When** the user requests headless statistics for it,
   **Then** the application prints structured information about the scene (at minimum: bounds
   and a geometry count) and exits successfully without opening a window.
2. **Given** a valid data-structure file, **When** the user requests headless statistics for
   it, **Then** the application prints structured information about the file (at minimum: file
   type, primitive counts, and bounds) and exits successfully without opening a window.
3. **Given** no display/graphics environment is available, **When** headless statistics are
   requested, **Then** the application still succeeds, because it never attempts to open a
   window in this mode.
4. **Given** an invalid or unreadable file, **When** headless statistics are requested,
   **Then** the application reports a clear error and exits with a failure status.
5. **Given** the user asks for help or version information, **When** the corresponding option
   is used, **Then** the application prints that information and exits successfully.

---

### User Story 4 - Continue viewing scene files exactly as before (Priority: P2)

An existing user of the wireframe scene viewer opens a RIB scene file just as they always
have, to check camera framing and geometry before or after a render.

**Why this priority**: This feature changes the application's underlying construction
significantly. Existing scene-viewing users must see zero disruption — this story exists to
protect against regressions introduced while adding the new capability.

**Independent Test**: Can be fully tested by opening a RIB scene file and confirming the
window, camera controls (orbit, pan, zoom, reset, save), and visual output are unchanged from
before this feature.

**Acceptance Scenarios**:

1. **Given** a valid RIB scene file, **When** the user opens it, **Then** the wireframe
   displays exactly as it did before this feature was added.
2. **Given** a scene is open, **When** the user orbits, pans, zooms, resets, or saves the
   camera, **Then** each action behaves exactly as it did before this feature was added.

---

### Edge Cases

- What happens when a data file is truncated or corrupted partway through? The application
  must report an error and must not crash.
- What happens when a data file was produced on a machine with a different word size (e.g., a
  32-bit vs. 64-bit build)? The application must recognize the mismatch and report it clearly
  rather than misinterpreting the file's contents.
- What happens when a data file contains an extremely large number of primitives (e.g.,
  millions of points)? The application must remain responsive, reducing displayed detail if
  necessary, rather than becoming unresponsive or exhausting available memory. If detail is
  reduced, this must be visibly indicated to the user.
- What happens when a data file contains zero primitives? The application must open it without
  error and clearly indicate that there is nothing to display, rather than showing an empty
  window indistinguishable from a failure.
- What happens when the user tries to open a second file while one is already open? The newly
  opened file replaces the current one; the application does not manage multiple open
  documents in this feature.
- What happens when the application is launched with no file argument, or with an unrecognized
  option? It must print a clear usage message and exit with a failure status rather than
  opening an empty window.

## Requirements *(mandatory)*

### Functional Requirements — Data File Opening & Detection

- **FR-001**: The system MUST detect, from a file's own content (not its name or extension),
  whether an opened file is a RIB scene or one of the supported precomputed data-structure
  types (photon map, irradiance cache, gather cache, point cloud, brick map, or raw
  debug-geometry dump).
- **FR-002**: The system MUST successfully open and visualize every one of the six supported
  data-structure file types.
- **FR-003**: The system MUST reject, with a clear and specific error message, a data file
  produced by an incompatible software version or an incompatible machine word size, without
  attempting to display its contents.
- **FR-004**: The system MUST reject, with a clear error message, a file that is neither a
  valid RIB scene nor a recognized data-structure file, without crashing.
- **FR-005**: The system MUST handle a data file containing zero primitives by opening it
  successfully and indicating that there is nothing to display.
- **FR-006**: The system MUST remain responsive when opening a data file with a very large
  number of primitives, reducing displayed detail if needed to do so, and MUST visibly
  indicate to the user when detail has been reduced. Detail reduction MUST be deterministic —
  a fixed maximum primitive count with even sampling — so the same file always reduces the
  same way regardless of the machine it is opened on.

### Functional Requirements — Visualization

- **FR-007**: The system MUST render each supported data-structure file using primitive shapes
  appropriate to its content: points, lines, triangles, and oriented discs, matching what the
  data represents (e.g., a photon map as points, a brick map's boxes as appropriate solid or
  outlined shapes).
- **FR-008**: The system MUST frame a newly opened file's content so that its full extent is
  visible without further user action.
- **FR-009**: The system MUST allow the same navigation (orbit, pan, zoom, reset) already
  available for scene files to be used when viewing a data-structure file.
- **FR-010**: *(Retired during `/speckit-analyze`, 2026-09-01 — see Clarifications.)* This
  requirement described a "no visualization available" fallback for a data-file variant later
  shown to be unreachable through this feature's own content-based detection (FR-001): it is
  selected only by a shader's runtime request during rendering, never by anything present in a
  file. No replacement requirement is needed — FR-002 already unconditionally covers every
  detectable data-structure file type. This ID is intentionally left retired rather than reused,
  so downstream references (data model, contracts, tasks) that predate this correction remain
  traceable.

### Functional Requirements — Interactive Controls

- **FR-011**: For a brick map, the system MUST let the user change its detail level (more or
  less detailed) and see the visualization and an on-screen detail-level indicator update.
- **FR-012**: For a brick map, the system MUST let the user switch its draw mode among the
  modes the file supports (e.g., boxes, discs, points) and see the visualization and an
  on-screen draw-mode indicator update.
- **FR-013**: For a brick map or point cloud with multiple data channels, the system MUST let
  the user select the next or previous channel and see the visualization and an on-screen
  channel-name indicator update.
- **FR-014**: For a point cloud, the system MUST let the user toggle between its available draw
  modes (points and discs) and see the visualization and an on-screen draw-mode indicator
  update.
- **FR-015**: Every control described in FR-011 through FR-014 MUST be reachable both through
  a visible menu, toolbar, or equivalent on-screen control, and through a keyboard shortcut.
  These keyboard shortcuts MUST be plain, unmodified keys carried over from the legacy tool,
  and MUST be active only while a document to which they apply is open, so they never compete
  with another control's shortcut or an application-standard shortcut.
- **FR-016**: The system MUST NOT rely on a terminal or console for any user-facing status —
  current channel, detail level, and draw mode MUST always be visible within the application's
  own window.
- **FR-017**: The system MUST NOT offer a channel-selection control for a file type that has
  no distinct data channels (e.g., a debug-geometry dump).

### Functional Requirements — Application Interface

- **FR-018**: The system MUST provide native application menus appropriate to each supported
  desktop platform, exposing at minimum: opening a file, the visualization controls in FR-011
  through FR-014 (when applicable to the open file), the existing scene-camera actions, and
  standard application actions (quit, about).
- **FR-019**: The system MUST support exactly one open file (scene or data) at a time; opening
  a new file replaces the currently displayed one.
- **FR-020**: The system MUST display, in the visible window (e.g., its title or a status
  area), what kind of file is currently open.

### Functional Requirements — Command-Line Interface

- **FR-021**: The system MUST support a help option that prints usage information and exits
  successfully.
- **FR-022**: The system MUST support a version option that prints the application's version
  and exits successfully.
- **FR-023**: The system MUST support a headless mode that prints structured, machine-readable
  statistics about a given scene or data file and exits without opening a window.
- **FR-024**: The headless mode MUST function without requiring any renderer-specific
  environment configuration to be present, consistent with the existing scene-loading
  behavior.
- **FR-025**: The system MUST report errors (invalid file, bad arguments, unreadable path) on
  a distinct channel from normal output, and MUST exit with a non-zero status when an error
  prevents the requested operation from completing.

### Functional Requirements — Removal of the Legacy Tool

- **FR-026**: The system MUST NOT retain the previous, non-functional data-structure viewing
  tool, its underlying rendering hook, or any dependency that existed solely to build it — no
  replacement binary, command alias, or compatibility shim is provided for it.
- **FR-027**: Existing automated tests that previously could only confirm the legacy tool's
  fixed failure MUST be updated to exercise real, successful behavior of the new capability
  instead.

### Functional Requirements — Continuity

- **FR-028**: The system MUST continue to open and display RIB scene files with no observable
  change in behavior, appearance, or available scene-camera actions.

### Key Entities

- **Scene Document**: A RIB scene file's loaded, displayable representation — geometry
  rendered as a wireframe, together with camera information. Unchanged by this feature.
- **Data Document**: The loaded, displayable representation of one precomputed data-structure
  file (photon map, irradiance cache, gather cache, point cloud, brick map, or debug-geometry
  dump). Has a file type, a set of visualization primitives, an overall spatial extent, and,
  depending on type, zero or more data channels, a detail level, and a draw mode.
- **Display Channel**: One named, selectable data quantity a Data Document may expose (for
  example, a specific recorded value in a point cloud); at most one is active at a time.
- **Detail Level**: A brick map's current level-of-detail setting, adjustable up or down.
- **Draw Mode**: How a Data Document's primitives are currently rendered — the specific set of
  available modes depends on the document's file type (e.g., points/discs for a point cloud;
  boxes/discs/points for a brick map).

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Every one of the six supported data-structure file types opens and displays
  successfully, where previously 100% of attempts to view any of them failed immediately.
- **SC-002**: A user can perform every visualization adjustment (channel, detail level, draw
  mode) entirely through visible on-screen controls, without using the keyboard, and can
  separately perform every one of those same adjustments entirely through keyboard shortcuts.
- **SC-003**: The application's current visualization state (channel, detail level, draw mode)
  is visible on screen at all times while a data file with that state is open — zero instances
  of that information being available only via a terminal.
- **SC-004**: Headless statistics for a scene or data file are produced in under 2 seconds on a
  typical file, with no graphical display required, in a machine-readable form.
- **SC-005**: Existing scene-viewing workflows (open, orbit, pan, zoom, reset, save camera)
  show no observable change in behavior after this feature ships.
- **SC-006**: Opening a data file with a very large number of primitives (on the order of a
  million points) completes in under 5 seconds and leaves the application responsive; if
  detail is reduced to achieve this, reopening the same file always reduces it the same way.
- **SC-007**: All six existing automated test scenes that previously could only assert the
  legacy tool's failure now assert real, successful behavior of the new capability.

## Assumptions

- Exactly one document (a scene or a data file) is open at a time; this feature does not
  introduce multiple simultaneous windows or a document-management model. Opening a new file
  replaces the one currently displayed.
- The legacy tool being replaced is retired outright. No command-line alias, wrapper, or
  compatibility shim is provided in its name; there is no obligation to preserve its
  (effectively nonexistent) prior command-line behavior.
- Visualizations are unlit / flat-colored, consistent with how this data was previously shown;
  this feature does not add lighting, shadows, or material shading to data visualization.
- Channel selection, detail level, and draw mode are per-session display choices: they take
  sensible defaults each time a file is opened and are not required to be remembered between
  application launches.
- The headless statistics mode is aimed at users and scripts that already know which file they
  want to inspect; this feature does not add an interactive file-browsing capability.
- Every data-structure file type this feature can detect from file content always has a working
  visualization; there is no "recognized but unsupported" variant in scope (see the retired
  FR-010 and the corrected Clarifications entry).
