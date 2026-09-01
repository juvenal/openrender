# Tasks: orender-wire Data-Structure Viewer (oshow Absorption)

**Input**: Design documents from `specs/016-oshow-data-viewer/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md

**Tests**: Constitution Principle III (TDD) is **NON-NEGOTIABLE** for this project — every test
listed below is REQUIRED, not optional, and MUST be written and confirmed failing before its
corresponding implementation task.

**Organization**: Tasks are grouped by user story per spec.md's priorities (US1=P1, US2=P2,
US4=P2, US3=P3). Because this feature's core (the sink, the headless loader, the C ABI, the
disc-expansion math) is genuinely shared by every story — no story can open a single data file
without all of it — that shared core lives in Phase 2 (Foundational) rather than being
duplicated per story. This matches the increment order (I0–I7) established in plan.md and
research.md; each Foundational sub-phase below corresponds 1:1 to one of those increments.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies on incomplete tasks)
- **[Story]**: Maps a task to US1/US2/US3/US4 from spec.md; Setup/Foundational/Polish tasks
  carry no story label
- File paths are exact and relative to the repository root

---

## Phase 1: Setup

**Purpose**: Mechanical, behavior-independent repository hygiene that nothing else depends on.

- [X] T001 [P] Add a `.gitignore` entry for `src/preview/orender-wire-macos/.build/` — **found
      already satisfied**: `.gitignore:23` already has a generic `.build/` pattern, and no
      `.build/` directory exists in this worktree (`git ls-files` returns zero tracked entries
      under it). The pre-spec research claim of a "committed SwiftPM index DB" did not hold on
      inspection; no change was needed.
- [X] T002 [P] Soften the four hard `REQUIRED` CMake probes (`OpenGL`, `gtk4>=4.20`,
      `libadwaita-1>=1.4`, `epoxy`) in `src/preview/orender-wire-linux/CMakeLists.txt` to
      warn-and-`return()`, mirroring the existing `swift not found` pattern in
      `src/preview/orender-wire-macos/CMakeLists.txt:11-14`. Verified by inspection (this
      `if(APPLE) return()`-gated file cannot be exercised by a configure on this macOS machine);
      Linux CI or a Linux checkout should confirm a missing dependency now warns and skips the
      target instead of failing configure.

**Checkpoint**: Repository hygiene fixed; no functional code touched yet.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: The platform-neutral core every user story depends on — the sink abstraction, the
headless data loader, the C ABI, and disc expansion — plus the one-time removal of `oshow`/FLTK
and the isolated SwiftUI shell migration. **No user-story-specific GUI work begins until this
entire phase is green.**

**⚠️ CRITICAL**: This phase is unusually large for this feature because almost nothing here is
story-specific — a photon map cannot open at all, on either platform, without T014–T049 existing
first.

### 2A — SwiftUI shell migration (increment I0), isolated against today's known-good app

> Sequenced first and validated in isolation, per plan.md: this is the only sub-phase where
> "compiles and launches" does not mean "works" (first-responder loss, exit-code plumbing,
> codesigning a SwiftUI bundle all fail silently), so isolating it here makes any regression
> unambiguous and revertible before any new capability is built on top of it.

- [X] T003 [P] Delete `src/preview/orender-wire-macos/Sources/AppDelegate.swift` and
      `src/preview/orender-wire-macos/Sources/Shaders.metal`; remove the `exclude:
      ["Shaders.metal"]` entry from `src/preview/orender-wire-macos/Package.swift`
- [X] T004 ~~Prove SPM cross-target header reach... `../../ribpreview_api.h` and
      `../../libribpreview/cameraExport.h`~~ **Corrected during implementation**: found that
      `ribcam_write`/`ribcam_replace` (the functions Swift actually calls) were declared only
      in the `CRibPreview.h` duplicate, not in `cameraExport.h` (which declares the unrelated
      C++-linkage `writeRibCamera`/`replaceRibCamera`). Fixed by adding `ribcam_write`/
      `ribcam_replace` to `ribpreview_api.h` itself (see contracts/c-abi.md's "Header
      consolidation" correction) — so only **one** external header (`ribpreview_api.h`) needs
      to reach the `CRibPreview` target, not two. Proved via `headerSearchPath: ["../.."]` on
      the `CRibPreview` target in `src/preview/orender-wire-macos/Package.swift`.
- [X] T005 Collapsed `src/preview/orender-wire-macos/CRibPreview/include/CRibPreview.h` to a
      single line, `#include "ribpreview_api.h"` (not two — see T004's correction).
- [X] T006 Create `src/preview/orender-wire-macos/Sources/OrenderWireApp.swift`: a
      `SwiftUI.App` (no `@main` — see T009) with a `WindowGroup` and a `Commands` block
      providing the application menu bar (About/Quit at minimum; further menu items added in
      Phase 4)
- [X] T007 Create `src/preview/orender-wire-macos/Sources/DocumentView.swift`: an
      `NSViewRepresentable` wrapping the existing `WireframeRenderer` `MTKView` unchanged;
      `updateNSView` calls `window?.makeFirstResponder(nsView)` whenever the view is not
      already first responder (the named first-responder regression risk from research.md §8)
- [X] T008 Create `src/preview/orender-wire-macos/Sources/ViewerModel.swift`: an
      `ObservableObject` holding the currently open document's state, wired to the existing
      RIB-loading path only for now (`ribpreview_load`/`ribpreview_free`) — data-document
      bridging is added in Phase 3
- [X] T009 ~~Append a single trailing `OrenderWireApp.main()` call... do not reorder, remove,
      or otherwise modify anything above it~~ **Corrected during implementation**: literal
      append-only would have left the old blocking AppKit bootstrap (`app.run()`) in place
      before `OrenderWireApp.main()` could ever execute. Replaced the AppKit bootstrap block
      (not appended after it) with `ViewerModel.shared = ViewerModel(ribPath: ribPath)` +
      `OrenderWireApp.main()`; everything above the bootstrap (arg parsing, `--help`/
      `--version`, the `ORENDER_WIRE_GUI` re-exec detach block, exit codes 0–3) is untouched.
- [X] T009b **Found during T010 manual verification, real bug, fixed**: launching via
      `open build/orender-wire.app --args <rib>` (Finder/Dock/`open` — i.e. LaunchServices)
      showed the Dock icon bounce and then vanish, with no window ever appearing — reported
      by the user as "started and died". Root cause: the pre-existing (spec-006-era)
      terminal-detach re-exec in `main.swift` (see T009) fires unconditionally whenever
      `ORENDER_WIRE_GUI` is unset, including when launched via LaunchServices. Confirmed via
      process inspection: the LaunchServices-tracked process re-execs a child and exits
      immediately (Dock removes its icon the instant it exits — matching "icon jumps then
      dies"); the untracked child is reparented to launchd (ppid 1) and survives headlessly
      with stdin/stdout/stderr all on `/dev/null` and no way to ever be brought to the
      foreground. This is a real, pre-existing latent bug (the detach logic predates this
      spec) that simply had never been exercised via `open`/Finder before now — the
      SwiftUI migration didn't introduce it, T010 just exposed it. Fixed by gating the
      re-exec on `getppid() != 1`: LaunchServices-launched processes always have `launchd`
      (PID 1) as their immediate parent, so this reliably distinguishes "typed at a
      terminal prompt" (detach, to return the shell prompt) from "launched via Finder/Dock/
      `open`" (run in place — there is no shell prompt to give back, and detaching only
      orphans the Dock-tracked process). Verified: after the fix, `open --args <rib>`
      leaves exactly one live process — the original LaunchServices PID, in `R` (running)
      state — with no re-exec/self-replacement. File: `src/preview/orender-wire-macos/
      Sources/main.swift`.
- [X] T009c **Found during T010 manual verification, real bug, fixed**: after T009b's fix
      let a live window actually render, the user confirmed a visible window but reported
      it as solid black with no content. Root cause: `WireframeRenderer` is configured
      `isPaused = true` / `enableSetNeedsDisplay = true` (see its own doc comment) — `MTKView`
      then draws **only** in direct response to an explicit invalidation request, never on
      its own. The AppKit-era `AppDelegate.didLoad()` (recovered from git history at
      `33506f4`) supplied that first request explicitly, immediately after attaching the
      view: `window.contentView = renderer; renderer.setNeedsDisplay(renderer.bounds)`. That
      call had no equivalent when `didLoad` was ported to `ViewerModel.load()` for the
      SwiftUI migration — SwiftUI's hosting has no matching one-shot "just attached to a
      window" callback for `NSViewRepresentable`. Root-caused via `lldb` breakpoints on
      `DocumentView.body` and `ViewerModel.load()` plus temporary instrumentation (later
      removed) rather than guesswork — confirmed `load()`/`makeNSView` ran correctly and
      `draw(in:)` itself succeeded, but with `sceneVertexCount=0` specifically for
      `teapot.rib`; a second scene (`colorcube.rib`, polygon-based) rendered correctly
      end-to-end even before this fix landed, isolating the black-window report to two
      independent causes (see below). Fixed by requesting a redraw in `MetalRendererView.
      updateNSView` (called by SwiftUI on every layout pass, so it lands once AppKit has
      actually assigned the view real bounds) — `nsView.setNeedsDisplay(nsView.bounds)`.
      File: `src/preview/orender-wire-macos/Sources/DocumentView.swift`.
      **Separate, pre-existing, out-of-scope bug found in the same investigation** (not
      fixed here — belongs to the shared `libribpreview` data path, not this SwiftUI
      migration): `teapot.rib` specifically produces `sceneVertexCount=0` even after the
      draw-request fix — its patch geometry never reaches `PreviewScene::vertices` in
      `src/preview/libribpreview/previewContext.cpp`. `test_preview_patch` did **not** catch
      this: it is a standalone unit test that reimplements bilinear/bicubic tessellation
      math directly in the test file and never calls `ribpreview_load`/exercises
      `CPreviewContext` at all, so the real patch → vertex-list path has apparently never
      been exercised end-to-end with real pixels. Polygon-based scenes are confirmed
      unaffected (`colorcube.rib` → `sceneVertexCount=3072`, renders correctly). Flag for a
      follow-up fix/spec targeting `CPreviewContext`'s patch handling — out of scope for
      this SwiftUI-shell increment.
- [X] T010 Build succeeds and codesigns; `ctest --test-dir build -L preview` is 9/9 green;
      `--help`/`--version` return correct output and exit 0; the running process shows a
      real SwiftUI-generated menu bar (Apple, orender-wire, File, Edit, View, Window, Help).
      T009b's and T009c's fixes verified together with the user's own eyes: launched via
      `open build/orender-wire.app --args colorcube.rib` (the exact `open`/Finder path the
      user first hit), confirmed via screenshot (Accessibility/`screencapture` access was
      granted to this session partway through this investigation, unblocking direct visual
      verification) — a single window titled `colorcube.rib`, correctly showing the colored
      wireframe cube grid plus the grid/axis overlay, sized well above the 800×600 minimum.
      `teapot.rib` still shows a black window due to the separate pre-existing bug logged
      under T009c — not a regression from this migration (confirmed via the same polygon
      vs. patch A/B test) and not blocking this task's "zero functional change to the
      wireframe shell" scope.
      Not independently re-verified by a human at the keyboard in this pass (already
      confirmed once by the user for window visibility): orbit/pan/zoom drag, keyboard
      shortcuts before/after clicking into the view, and the Save Camera dialog. These
      exercise `WireframeRenderer`/`ArcballCamera` code paths that this migration left
      byte-for-byte unchanged (confirmed by diff), so risk is low, but flagging since they
      were not independently re-clicked after T009c's fix landed.

**Checkpoint 2A**: macOS app is SwiftUI-shelled with a real (if minimal) menu bar; RIB-scene
viewing is provably unchanged.

### 2B — `CPrimitiveSink`/`CDataView` sink (increment I1), tests first

- [ ] T011 [P] Write failing test `tests/preview/test_dataview_chunking.cpp` (tail-flush at
      exactly `chunkSize`, `chunkSize±1`, `0`, and `1` primitives; asserts the corrected
      `CPhotonMap` loop bound — see T018)
- [ ] T012 [P] Write failing test `tests/preview/test_debugdump_roundtrip.cpp` (write via
      `CDebugView`'s point/line/triangle/quad writers, reopen, count via a test sink; quad → 2
      triangles; last written record emitted exactly once)
- [ ] T013 Register T011 and T012 in `tests/preview/CMakeLists.txt` via the existing
      `add_preview_test` macro; confirm both fail to compile/link (Red — `CDataView` does not
      exist yet)
- [ ] T014 Create `src/ri/dataView.h`: `class CPrimitiveSink` (pure-virtual `triangles`,
      `triangleMesh`, `lines`, `points`, `disks`) and `class CDataView` (replaces `CView`; same
      six static drawing entry points, same `chunkSize = 128 * 3`, plus `numChannels`,
      `channelName`, `currentChannel`, `detailLevel`, `drawMode`, `typeName` accessors) per
      research.md §1 and data-model.md
- [ ] T015 Create `src/ri/dataView.cpp`: the six static forwarders to an installed
      `CPrimitiveSink*`, and the debug-dump parser lifted from
      `git show 33506f4^:src/gui/opengl.cpp`'s `pglFile`, fixed to
      `while (fread(&tag, sizeof(int), 1, f) == 1)` (not `!feof`) and splitting the quad tag
      into two triangles (`0,1,2` and `0,2,3`)
- [ ] T016 [P] Reparent `src/ri/debug.h` and `src/ri/debug.cpp` from `CView` to `CDataView`
      (`#include "gui/opengl.h"` → `#include "dataView.h"`; `: public CView` → `: public
      CDataView`)
- [ ] T017 [P] Reparent `src/ri/texture3d.h` from `CView` to `CDataView`
- [ ] T018 [P] Reparent `src/ri/photonMap.h` and `src/ri/photonMap.cpp` from `CView` to
      `CDataView`; fix the confirmed off-by-one in `CPhotonMap::draw`
      (`src/ri/photonMap.cpp:565`): the loop must run `numItems` times from `items + 1`,
      matching `CPointCloud::draw`'s correct bound (`src/ri/pointCloud.cpp:364`), not
      `numItems - 1`
- [ ] T019 [P] Reparent `src/ri/pointCloud.cpp` from `CView` to `CDataView`; replace its
      `printf` channel/draw-mode output (`pointCloud.cpp:412-423`) with the new `CDataView`
      accessors
- [ ] T020 [P] Reparent `src/ri/brickmap.h` and `src/ri/brickmap.cpp` from `CView` to
      `CDataView`; replace its `printf` detail-level/draw-type/channel output
      (`brickmap.cpp:1375-1405`) with the new `CDataView` accessors; assign
      `CDataView::drawTriangleMesh` for the first time (previously declared but never resolved,
      `show.cpp:73-77`)
- [ ] T021 [P] Reparent `src/ri/irradiance.cpp` and `src/ri/radiance.cpp` from `CView` to
      `CDataView` (no behavior change beyond the base-class swap)
- [ ] T022 Delete `src/gui/` in its entirety (the one remaining file, `opengl.h`) now that
      nothing includes it
- [ ] T023 Confirm T011 and T012 now pass (Green); run `ctest --test-dir build -L preview` in
      full to confirm no regression in the pre-existing 9 tests

**Checkpoint 2B**: The sink abstraction exists and every `src/ri/` visualization class targets
it; nothing outside `libri` consumes it yet.

### 2C — Remove `oshow` and FLTK (increment I2), one cohesive removal

- [ ] T024 Delete `src/oshow/` in its entirety, and `src/ri/show.h`/`src/ri/show.cpp`
- [ ] T025 Remove the `CShow` `#include`, dispatch, and construction from
      `src/ri/renderer.cpp` (the include, `preDisplaySetup` call, and `new CShow(i)` site) and
      `src/ri/rendererContext.cpp` (the `CShow`-related include)
- [ ] T026 [P] Remove the `show.cpp` entry from `src/ri/CMakeLists.txt`; remove
      `add_subdirectory(oshow)` from `src/CMakeLists.txt`
- [ ] T027 [P] Remove `option(BUILD_SHOW ...)`, the `fltk-config` probe block, and the
      `oshow` entry in the codesign executable list from the root `CMakeLists.txt`
- [ ] T028 [P] Remove the `add_not_required_test` macro, its explanatory comment block, and
      its six scene registrations from `tests/visual/CMakeLists.txt`
- [ ] T029 [P] Remove the five FLTK install steps from `.github/workflows/release.yml`
- [ ] T030 [P] Rename the six `*-oshow.rib` fixtures (in `examples/rib/tests/` and
      `examples/rib/tests/parity/`) to `*-wire.rib`, dropping their `Hider "oshow:none"` line
      (`orender-wire` ignores `Hider` entirely, so no replacement statement is needed)
- [ ] T031 [P] Correct every stale `oshow` reference: `README.md`, `INSTALL.md`,
      `INSTALL_ARTIFACTS.md`, `COMPILING.txt`, `HOMEBREW_GUIDE.md`, `openrender.rb.template`,
      `openrender.spec`, `AUTHORS.md`, `CLAUDE.md`, `DEVNOTES.md`, and the `oshow(1)` SEE ALSO
      cross-references in `man/orender.1`, `man/oshader.1`, `man/otexmake.1`, `man/rsloinfo.1`
- [ ] T032 Removal gate: from the repository root,
      `grep -rn "CShow\|oshow\|BUILD_SHOW\|FLTK" .` excluding `build/`, `specs/`, and
      `ChangeLog.md` returns zero hits (FR-026)

**Checkpoint 2C**: No trace of the legacy tool or its build dependency remains anywhere in the
tree except historical records.

### 2D — Headless loader and C ABI (increment I3), tests first

- [ ] T033 [P] Write failing test `tests/preview/test_data_world_init.cpp` (after opening a
      constructed photon-map fixture headlessly, the view's transform is non-degenerate and
      `bound()` returns finite values — pins the identity-matrix seeding requirement)
- [ ] T034 [P] Write failing test `tests/preview/test_data_pointcloud.cpp` (construct a
      `CPointCloud` via its write constructor, `store()` known points, destroy to flush, reopen
      through `CDataDocument::open()`, assert type/count/bounds/channels match; also repeat the
      round-trip storing zero points and assert the reopened document is valid with all counts
      `== 0` — covers FR-005, added during `/speckit-analyze` remediation)
- [ ] T035 Register T033 and T034 in `tests/preview/CMakeLists.txt`; confirm both fail to
      compile/link (Red — `CDataDocument` does not exist yet)
- [ ] T036 Create `src/ri/dataLoad.h` and `src/ri/dataLoad.cpp`: `dataSniff()` (ports the
      magic-number/version/word-size validation from `src/ri/show.cpp:82-99` verbatim) and
      `class CDataDocument` whose `open()` sets `CRenderer::fromWorld`, `toWorld`,
      `fromWorld1`, `toWorld1`, `fromNDC`, `toNDC` to identity and `worldBmin`/`worldBmax` to
      ±infinity **before** constructing any reader, then constructs `CPhotonMap`,
      `CIrradianceCache`, `CPointCloud`, `CBrickMap`, or `CDebugView` directly (never via
      `CRenderer::getPhotonMap`/`getCache`/`getTexture3d`, which assert on mid-render-only
      state) and owns whichever it constructs uniformly in the destructor
- [ ] T037 Confirm T033 and T034 now pass (Green); if `CTexture3d::retrieveDisplayChannel`
      (`texture3d.cpp:119`) turns out to be reachable on this read-only path (research.md §2
      open question), add the minimal display-channel stub `CDataDocument::open()` needs to
      satisfy it, within this same task
- [ ] T038 Extend `src/preview/ribpreview_api.h` per contracts/c-abi.md: `PrimArrayC`,
      `DataSceneC`, the `RibDataType` enum, and the opaque-handle functions `ribdata_sniff`,
      `ribdata_open`, `ribdata_snapshot`, `ribdata_key`, `ribdata_channel_name`,
      `ribdata_close` (declarations only at this point; `ribpreview_load`/`ribpreview_free` are
      untouched)
- [ ] T039 Create `src/preview/libribpreview/dataScene.h`: the `DataScene` struct (four
      primitive arrays — lines, points, triangles, plus a pre-expansion `disks` list) per
      data-model.md
- [ ] T040 [P] Write failing test `tests/preview/test_data_keys.cpp` (drive `ribdata_key()`
      with each legacy key `m l b d p q w` against brick-map and point-cloud fixtures; assert
      the corresponding `DataSceneC` fields change; assert **zero bytes written to stdout**
      across the whole sequence)
- [ ] T041 [P] Write failing test `tests/preview/test_wire_cli.cpp` (argument grammar from
      contracts/cli-interface.md; every exit code 1–5 reachable; `--json` output is
      well-formed JSON containing `schemaVersion` and every required key per `documentType`;
      assert the synthesized `camera` in a data document's output actually frames the reported
      `bounds`, covering FR-008; assert `--json` on a small fixture completes under 2 seconds,
      covering SC-004 — both added during `/speckit-analyze` remediation)
- [ ] T042 Register T040 and T041 in `tests/preview/CMakeLists.txt`; confirm both fail (Red —
      `ribdata_key` and `wireCli` do not exist yet)
- [ ] T043 Create `src/preview/libribpreview/dataSink.h` and
      `src/preview/libribpreview/dataSink.cpp`: `class CDataSceneSink : public CPrimitiveSink`,
      appending into a `DataScene`, applying the deterministic fixed-cap/even-stride decimation
      decided in spec clarification, and implementing `ribdata_open`/`ribdata_snapshot`/
      `ribdata_key`/`ribdata_channel_name`/`ribdata_close` from T038 against `CDataDocument`
      from T036
- [ ] T044 Create `src/preview/libribpreview/wireCli.h` and
      `src/preview/libribpreview/wireCli.cpp`: shared argument parsing, `--help`, `--version`,
      `--json` (headless statistics, exit codes 1–5, JSON serialization per
      contracts/cli-interface.md), with C linkage so both platform frontends can call the same
      implementation
- [ ] T045 Confirm T040 and T041 now pass (Green)

**Checkpoint 2D**: The entire data path — sniff, load, snapshot, interactive state, headless
JSON — is proven correct before either platform's GUI code has been touched.

### 2E — Disc expansion (increment I4)

- [ ] T046 [P] Write failing test `tests/preview/test_data_disk_expand.cpp` (exactly 60
      vertices per disc; every rim vertex within `dP + ε` of `P`; every triangle's plane normal
      parallel to `N`; no NaN when `P == (0,0,0)` or `P ∥ N`)
- [ ] T047 Register T046 in `tests/preview/CMakeLists.txt`; confirm it fails (Red —
      `diskExpand` does not exist yet)
- [ ] T048 Create `src/preview/libribpreview/diskExpand.h` and
      `src/preview/libribpreview/diskExpand.cpp`: 20-segment CPU-side disc expansion matching
      the deleted `pglDisks` geometry, using an axis-picking basis (never `P × N`) to avoid the
      NaN identified in research.md §1; wire `CDataSceneSink` (T043) to call it and append
      results into `DataScene`'s triangle array
- [ ] T049 Confirm T046 now passes (Green); run `ctest --test-dir build -L preview` in full —
      every Foundational test (T011, T012, T033, T034, T040, T041, T046) must be green

**Checkpoint 2 (end of Foundational)**: Every user story below can now be implemented against a
complete, tested, headless-provable core. `orender-wire --json` already works for every
document type via T044/T045, even though no GUI has been touched.

---

## Phase 3: User Story 1 - Inspect a precomputed data-structure file (Priority: P1) 🎯 MVP

**Goal**: Opening any of the six supported data-structure file types displays its contents as a
navigable 3D scene, on both platforms — replacing a tool that failed 100% of the time.

**Independent Test**: Open one file of each of the six types on each platform; each renders a
non-empty, navigable 3D view with a shape appropriate to its data, framed so the whole dataset
is visible.

### Tests for User Story 1

- [ ] T050 [P] [US1] Write test `tests/preview/test_preview_subdiv.cpp` exercising the six
      retargeted RIB scenes (`examples/rib/tests/**/*-wire.rib`, from T030) through
      `libribpreview`; assert non-zero vertex count and finite bounds for each. (The underlying
      tessellator, `tessSubdivision.cpp`, is pre-existing and unchanged — this test adds
      coverage that has never existed, rather than driving new production code, so it may pass
      immediately once registered.)
- [ ] T051 [US1] Register T050 in `tests/preview/CMakeLists.txt`; confirm it passes

### Implementation for User Story 1

- [ ] T052 [US1] In both frontends, route a newly opened file through content-based detection
      (`ribdata_sniff` vs. the existing RIB-parse attempt) instead of the current
      always-assume-RIB behavior, per FR-001
- [ ] T053 [US1] Add three new Metal render pipelines (points, triangles, discs) to
      `src/preview/orender-wire-macos/Sources/WireframeRenderer.swift`'s `buildPipelines`/
      `draw(in:)`, reusing the existing line/triangle vertex layout; points use
      `[[point_size]]`; explicitly `setCullMode(.none)` (discs are single-sided fans); wire
      `ViewerModel` (T008) to call `ribdata_open`/`ribdata_snapshot` and upload `DataSceneC`'s
      buffers when a data file is opened
- [ ] T054 [P] [US1] Add the equivalent three new GLSL 330 pipelines (points, triangles,
      discs) to `src/preview/orender-wire-linux/main.cpp`, reusing the existing vertex layout;
      `glEnable(GL_PROGRAM_POINT_SIZE)` and set `gl_PointSize`; `glDisable(GL_CULL_FACE)`; wire
      it to call `ribdata_open`/`ribdata_snapshot` when a data file is opened
- [ ] T055 [US1] Apply the GL depth-range fix in `src/preview/orender-wire-linux/main.cpp`'s
      projection-upload path (`clip.z = 2 * z_metal - w`, since `ribGeometryContext.cpp:337`
      builds Metal-NDC `z ∈ [0,1]` and GL 3.3 core has no `glClipControl`), and reconcile
      `src/preview/orender-wire-linux/arcball.cpp`'s orthographic `updateAspect` with
      `ArcballCamera.swift`'s in the same change
- [ ] T056 *(Retired during `/speckit-analyze`, 2026-09-01.)* This task implemented a
      "not available" notice for a hierarchical point-cloud/brick-map variant later shown to be
      unreachable through file-content detection (see spec.md's retired FR-010,
      contracts/c-abi.md's `RibDataType`, and research.md §2). No replacement task is needed:
      `RibDataType` has no corresponding enumerator, so every document `ribdata_open` returns is
      always visualizable. This ID is intentionally left retired rather than reused.
- [ ] T057 [US1] Manual validation: `quickstart.md` steps 2–3 (headless CLI per document type,
      then GUI open per document type) on both platforms

**Checkpoint**: User Story 1 is fully functional and independently testable — every data type
opens and renders on both platforms.

---

## Phase 4: User Story 2 - Adjust the visualization through discoverable controls (Priority: P2)

**Goal**: Channel, detail-level, and draw-mode controls for point clouds and brick maps are
reachable via visible menu/toolbar controls and via keyboard shortcuts, with current state
always visible on screen — never only in a terminal.

**Independent Test**: With a point cloud or brick map open, use on-screen menu/toolbar controls
(no keyboard) to change channel, detail level, and draw mode, confirming both the visualization
and an on-screen indicator update each time; separately, confirm the equivalent keyboard
shortcuts produce the same effect.

### Implementation for User Story 2

- [ ] T058 [US2] Add `Commands` menu items to
      `src/preview/orender-wire-macos/Sources/OrenderWireApp.swift` for channel next/prev,
      detail level +/-, and draw-mode switching, each wired to `ribdata_key`/`ViewerModel` and
      given a `.keyboardShortcut` matching the legacy letter (`m l b d p q w`); each item is
      enabled only when the open document type supports it, so no two controls ever compete for
      the same key
- [ ] T059 [US2] Add an on-screen indicator (window title or status text, driven by
      `ViewerModel`'s published state) showing current channel name, detail level, and draw
      mode, on macOS
- [ ] T060 [P] [US2] In `src/preview/orender-wire-linux/main.cpp`, populate the previously
      empty `AdwHeaderBar` (`adw_header_bar_new()`) with a `GtkMenuButton` bound to a
      `GMenuModel` whose entries are `GSimpleAction`s shared with the existing `on_key` key
      handler (one action implementation, two entry points), plus an `AdwWindowTitle` subtitle
      showing document type / channel / detail level / draw mode
- [ ] T061 [P] [US2] Make the Linux header-bar controls document-type-conditional (hide
      channel/detail controls when a RIB document is open; hide Save Camera when a data
      document is open) — no `AdwViewStack`, since both document types share one `GtkGLArea`
      and one set of pipelines
- [ ] T062 [US2] Confirm on both platforms that a channel control is not offered, or is
      clearly inert, for a document type with `numChannels == 0` (a debug-geometry dump), per
      FR-017
- [ ] T063 [US2] Manual validation: `quickstart.md` steps 4–5 (menu-only path, then
      keyboard-only path, including the macOS first-responder regression check after launch and
      after clicking the render view) on both platforms

**Checkpoint**: User Stories 1 and 2 both work independently; every interactive control from the
legacy tool is now discoverable and terminal-free.

---

## Phase 5: User Story 4 - Continue viewing scene files exactly as before (Priority: P2)

**Goal**: Existing RIB-scene viewing shows zero observable disruption from this feature.

**Independent Test**: Open a RIB scene file; confirm the window, camera controls (orbit, pan,
zoom, reset, save), and visual output are unchanged from before this feature. This story has no
dependency on User Stories 1–3 and can be validated as soon as Phase 2A (T010) lands.

### Implementation for User Story 4

- [ ] T064 [US4] Regression pass on macOS: re-run spec 006's `quickstart.md` in full against
      the post-SwiftUI-migration, post-renderer-changes build; confirm scene loading, orbiting,
      panning, zooming, resetting, and camera-saving are behaviorally identical to before this
      feature
- [ ] T065 [P] [US4] Same regression pass on Linux
- [ ] T066 [US4] Confirm FR-019 in both directions on both platforms: opening a new file fully
      replaces whatever was previously open, for every combination (RIB→RIB, RIB→data,
      data→RIB, data→data)

**Checkpoint**: All prior scene-viewing behavior is confirmed unregressed; single-document
replacement behaves correctly across all document-type combinations.

---

## Phase 6: User Story 3 - Retrieve file statistics without a graphical window (Priority: P3)

**Goal**: A developer or script can get structured, correct statistics about a scene or data
file with no display available, and exits successfully without ever opening a window.

**Independent Test**: Invoke the application in headless mode against a known scene file and a
known data file, with no display or renderer environment configured, and confirm it prints
structured, correct information and exits successfully without opening any window.

### Implementation for User Story 3

> The CLI grammar, JSON schema, and exit-code logic (`wireCli`) were already built and tested in
> Phase 2D (T041/T044/T045), because User Story 1's GUI paths call the same underlying
> `ribdata_*`/`ribpreview_*` functions `wireCli` wraps. What remains here is wiring `wireCli`
> into each platform's actual process entry point.

- [ ] T067 [US3] Wire `wireCli`'s `--help`/`--version`/`--json` handling into
      `src/preview/orender-wire-macos/Sources/main.swift`, positioned to run and call `exit()`
      **before** the `ORENDER_WIRE_GUI` re-exec check — otherwise headless mode would detach
      into a background GUI process and a test harness would see exit 0 with no output
- [ ] T068 [P] [US3] Wire the same `wireCli` entry point into
      `src/preview/orender-wire-linux/main.cpp`'s argument handling, before any GTK or OpenGL
      initialization occurs
- [ ] T069 [US3] Validation: `quickstart.md` step 2 (headless CLI checks, including over SSH
      with no window server) on both platforms

**Checkpoint**: All four user stories are independently functional and validated.

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Documentation and final whole-suite verification, per Constitution Principle VII
and the plan's overall acceptance bar.

- [ ] T070 [P] Add a documentation page under `docs/site/content/...` describing the new
      data-document type and the `--json` schema (Constitution VII merge gate)
- [ ] T071 [P] Update `docs/site/content/manual/reference/installing-and-running.md` and any
      other Hugo page describing `orender-wire`'s prior RIB-only scope
- [ ] T072 Walk the full `quickstart.md` sign-off checklist end-to-end on both macOS and Linux
- [ ] T073 Final full-suite gate: `ctest --test-dir build -L preview`,
      `ctest --test-dir build -L visual`, and the T032 removal grep all pass/return clean in
      the same run

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — can start immediately
- **Foundational (Phase 2)**: Depends on Setup; sub-phases 2A–2E have an internal order
  (2A is isolated by design; 2B must precede 2C only in the sense that 2C's removal is cleaner
  once nothing still includes `src/gui/`; 2D depends on 2B existing; 2E depends on 2D's
  `DataScene`/`CDataSceneSink`) — **BLOCKS all user stories**
- **User Stories (Phase 3–6)**: All depend on Foundational (Phase 2) completion.
  - US1 (Phase 3) has no dependency on US2/US3/US4
  - US2 (Phase 4) depends on US1's renderer pipelines existing (a control that changes
    `drawMode` needs a triangle/point/disc pipeline to see the effect) but is otherwise
    independent
  - US4 (Phase 5) depends only on Phase 2A — it can run in parallel with, or even before,
    Phases 3/4/6
  - US3 (Phase 6) depends on Phase 2D (`wireCli` already built and tested there); Phase 6's
    tasks are thin CLI-wiring, not new capability
- **Polish (Phase 7)**: Depends on all four user stories being complete

### Parallel Opportunities

- T001/T002 (Setup) in parallel
- Within 2A: T003 in parallel with nothing yet (T004 depends on it); T003–T010 are otherwise
  sequential (each Swift file builds on the last)
- Within 2B: T011/T012 in parallel; T016–T021 (the six reparenting tasks) in parallel once
  T014/T015 exist
- Within 2C: T026–T031 all in parallel (disjoint files); T024/T025 first, then T032 last
- Within 2D: T033/T034 in parallel; T040/T041 in parallel (after T036–T039)
- Within 2E: only T046 (T047–T049 are sequential gates around it)
- Once Phase 2 completes: US1 (Phase 3) and US4 (Phase 5) can start in parallel; US2 (Phase 4)
  can start once US1's T053/T054 land; US3 (Phase 6)'s T067/T068 can run in parallel with each
  other and with US1/US2/US4 work, since Phase 2D already did the hard part
- T053 (macOS renderer) and T054 (Linux renderer) are marked [P] — different files, same
  contract (`DataSceneC`)
- T064/T065 (regression pass, two platforms) in parallel
- T070/T071 (documentation) in parallel

---

## Parallel Example: Phase 2B (the sink)

```bash
# After T014/T015 (CPrimitiveSink, CDataView, dataView.cpp) exist, launch the six
# reparenting tasks together — each touches a disjoint file:
Task: "Reparent src/ri/debug.h and src/ri/debug.cpp from CView to CDataView"
Task: "Reparent src/ri/texture3d.h from CView to CDataView"
Task: "Reparent src/ri/photonMap.h and src/ri/photonMap.cpp from CView to CDataView; fix the off-by-one"
Task: "Reparent src/ri/pointCloud.cpp from CView to CDataView; replace printf output"
Task: "Reparent src/ri/brickmap.h and src/ri/brickmap.cpp from CView to CDataView; replace printf output"
Task: "Reparent src/ri/irradiance.cpp and src/ri/radiance.cpp from CView to CDataView"
```

## Parallel Example: Phase 3 (renderer pipelines)

```bash
# Once T052 (content-based detection) and the Foundational C ABI exist, the two platform
# renderers are fully independent:
Task: "Add 3 Metal pipelines to WireframeRenderer.swift"
Task: "Add 3 GLSL pipelines to orender-wire-linux/main.cpp"
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup
2. Complete Phase 2: Foundational — this is the bulk of the engineering work for this feature;
   by its end, `orender-wire --json <any-data-file>` already works end-to-end
3. Complete Phase 3: User Story 1
4. **STOP and VALIDATE**: walk `quickstart.md` steps 1–3 independently
5. This is a legitimate, demoable MVP: every data type opens and renders, on both platforms,
   with keyboard-only interaction and no menu polish yet

### Incremental Delivery

1. Setup + Foundational → the entire data path exists and is proven headlessly
2. Add User Story 1 → validate independently → this is the MVP
3. Add User Story 4 (can be done in parallel with the above, from as early as Phase 2A) →
   confirms nothing broke
4. Add User Story 2 → validate independently → controls become discoverable, not just
   keyboard-only
5. Add User Story 3 → validate independently → CLI wiring only, since the hard part shipped in
   Foundational
6. Polish → documentation and final whole-suite sign-off

### Notes specific to this feature

- Because Constitution Principle III is non-negotiable, every Foundational sub-phase (2B, 2D,
  2E) follows Red → Green explicitly: the test-writing task is listed, followed by a
  registration-and-confirm-failing task, followed by the implementation task(s), followed by a
  confirm-passing task. Do not skip the "confirm Red" step even though it feels redundant — it
  is what makes the later "confirm Green" step meaningful.
- 2A (SwiftUI migration) and 2C (removal) have no automated tests of their own — 2A's gate is
  T010 (full regression against spec 006's existing suite + quickstart), and 2C's gate is T032
  (the removal grep). This mirrors research.md §11's justification for why GUI-shell and
  removal work sit outside the TDD cycle in this codebase.
- Do not reorder 2A ahead of 2B/2C/2D/2E for convenience — its entire value is being validated
  in isolation before anything else changes. If 2A must be revisited after later phases reveal a
  problem, treat that as a new, separate fix, not a reason to have skipped the isolated gate.
