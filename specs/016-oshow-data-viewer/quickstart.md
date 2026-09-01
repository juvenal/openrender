# Quickstart: Validating the orender-wire Data-Structure Viewer

This is the end-to-end validation guide for this feature — the automated `ctest` suite proves
the platform-neutral core; this guide proves the two native shells and everything that can only
be judged by looking at a window. Run the whole guide on **both** macOS and Linux before
considering a phase (I5, I6, or I7) complete.

## Prerequisites

- A built tree: `cmake --build build --config Release` — must succeed **without FLTK
  installed**, confirming the removal in I2 is complete.
- No `ORENDERHOME`, `SHADERS`, or `DISPLAYS` need to be set for anything in this guide (FR-024).
- Fixture files: a point cloud, a brick map, a photon map, an irradiance cache, and a
  debug-geometry dump, each generated the same way the corresponding `tests/preview/` fixture is
  built (see `contracts/test-plan.md`), plus any RIB scene already used by spec 006's own
  quickstart (e.g. `examples/rib/camera-dof.rib`).

## 1. Automated proof (run first — a GUI check is not a substitute for this)

```bash
ctest --test-dir build -L preview --output-on-failure
```
Expected: all tests pass, including the eight new tests in `contracts/test-plan.md`.

```bash
ctest --test-dir build -L visual -R wire --output-on-failure
```
Expected: the six retargeted subdivision scenes pass (previously these could only assert a
fixed failure under the `not-required` label).

```bash
grep -rn "CShow\|oshow\|BUILD_SHOW\|FLTK" . | grep -v ^./build | grep -v ^./specs | grep -v ChangeLog.md
```
Expected: no output (research.md §12 removal gate).

## 2. Headless CLI (proves the whole data path before touching a window)

```bash
build/src/preview/orender-wire --help          # exit 0, usage printed
build/src/preview/orender-wire --version       # exit 0, version printed
build/src/preview/orender-wire --json examples/rib/camera-dof.rib      # documentType: "rib"
build/src/preview/orender-wire --json <point-cloud-fixture>            # documentType: "pointcloud"
build/src/preview/orender-wire --json <brick-map-fixture>              # documentType: "brickmap"
build/src/preview/orender-wire --json /no/such/file; echo $?           # exit 2
build/src/preview/orender-wire --json <truncated-or-bad-version-file>; echo $? # exit 4
```
Expected: valid JSON on stdout per `contracts/cli-interface.md`, nothing on stdout for the
failing cases, diagnostics on stderr, exit codes matching the contract. Confirm this also works
with no display/window-server available (e.g., over SSH without X forwarding) — this is the
headless-mode guarantee from FR-023/SC-004.

## 3. Opening each document type (GUI)

For each of: a RIB scene, a photon map, an irradiance cache, a gather cache, a point cloud, a
brick map, a debug-geometry dump — launch `orender-wire <file>` and confirm:

- The window opens and frames the content so its full extent is visible without further action
  (FR-008).
- The window shows what kind of file is open (FR-020).
- Orbit, pan, zoom, and reset all work identically to the existing scene-viewing behavior
  (FR-009, User Story 4).
- For the debug-geometry dump: no channel control is offered (FR-017).

Then open a data file variant known to have no working visualization (the hierarchical
point-cloud/brick-map case): confirm it opens successfully with a clear "not available" notice
and no crash (FR-010).

## 4. Interactive controls — menu/toolbar path

With a brick map open:
- Change detail level via the menu/header-bar control (not the keyboard); confirm both the
  visualization and the on-screen detail-level indicator update (FR-011).
- Change draw mode among boxes/discs/points via the menu; confirm the same (FR-012).
- Change channel via the menu; confirm the on-screen channel name updates (FR-013).

With a point cloud open:
- Toggle points/discs via the menu; confirm the visualization and indicator update (FR-014).
- Change channel via the menu; confirm the indicator updates.

At every step: confirm **nothing is printed to the terminal** — all state is visible only in the
application window (FR-016, SC-003).

## 5. Interactive controls — keyboard path

Repeat every action in step 4 using the legacy keyboard shortcuts (`m l b d p q w`) instead of
the menu, and confirm identical results (FR-015, acceptance scenario 4 of User Story 2).

**macOS-specific regression check** (the named first-responder risk from research.md §8):
verify every shortcut works immediately after launch, **and again after clicking anywhere in
the render view**, and again after switching to another application and back. If any shortcut
stops responding after a focus change, this is a regression — file it before considering I6
complete.

## 6. Visual correctness

- Open a brick map with a non-trivial voxel count; confirm no z-fighting or flickering between
  overlapping triangles/discs (the GL depth-range fix from research.md §7 — check specifically
  on Linux, where the bug lived).
- Confirm discs render with visually correct orientation (no degenerate/inverted discs from the
  disc-basis NaN fix) — pay particular attention to any point near the world origin.
- Open a data file whose primitive count exceeds the deterministic decimation cap; confirm the
  application stays responsive, and confirm a visible indication that detail was reduced
  (FR-006, SC-006). Reopen the same file and confirm the reduction looks identical both times.

## 7. Application chrome

- **macOS**: confirm a real menu bar exists (not just About/Quit) exposing file opening, the
  FR-011–FR-014 controls when applicable, the existing scene-camera actions, and standard
  actions (Quit, About). Confirm the app still launches correctly from Finder, and via the
  `bin/orender-wire` symlink from a terminal.
- **Linux**: confirm the previously-empty header bar now shows a populated menu button and a
  subtitle reflecting document type / channel / detail level / draw mode. Confirm the same
  behavior on both X11 and Wayland sessions.

## 8. Single-document behavior

- With one file open, open a second file (of either type) via the menu; confirm the first
  document is fully replaced (no second window, no leftover state) — FR-019.

## Sign-off

This feature is ready to ship once every step above passes on both platforms, all automated
suites in step 1 are green, and the manual-checklist items in `contracts/test-plan.md`'s "What
stays manual" section have been walked at least once per platform.
