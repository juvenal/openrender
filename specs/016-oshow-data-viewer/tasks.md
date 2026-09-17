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

**Retroactive correction (found during Phase 3/T053, not caught at the time):** three of this
phase's own new files — `Sources/DocumentView.swift`, `Sources/OrenderWireApp.swift`, and
`Sources/ViewerModel.swift` — were never actually tracked by git. The repo's `.gitignore` had an
unanchored `orender-*` pattern (matching *any* path component starting with `orender-`,
anywhere in the tree, not just at the repo root); since `src/preview/orender-wire-macos/` itself
matches that pattern, plain `git add` silently skipped every new file created under it, while
files that predated the ignore rule (`ArcballCamera.swift`, `WireframeRenderer.swift`,
`main.swift`, `Package.swift`, `Info.plist`, `CMakeLists.txt`, `CRibPreview.h`) stayed tracked
from before. Net effect: this phase's manual commits do not actually contain half of I0's new
Swift sources — anyone re-cloning the branch would be missing `OrenderWireApp.swift` entirely
and the build would fail. Fixed by anchoring the pattern to the repo root (`/orender-*` instead
of `orender-*`) in `.gitignore`, which stops matching nested directories while still ignoring
whatever stray root-level `orender-*` build artifact the rule originally existed for. The two
paths that are *supposed* to stay ignored under this directory (`.build/`, the staged
`CRibPreview/include/ribpreview_api.h` copy) have their own explicit `.gitignore` entries and
are unaffected. **Action needed from the user**: `git add` the three now-visible files (plus the
`.gitignore` fix itself) in the next commit — this session did not stage or commit them, per
standing instructions.

### 2B — `CPrimitiveSink`/`CDataView` sink (increment I1), tests first

- [X] T011 [P] Write failing test `tests/preview/test_dataview_chunking.cpp` (tail-flush at
      exactly `chunkSize`, `chunkSize±1`, `0`, and `1` primitives; asserts the corrected
      `CPhotonMap` loop bound — see T018). Implementation note: rather than asserting on
      `CPhotonMap`'s internal `numItems` (private, and always ≥1 because
      `CPhotonMap::balance()` seeds a dummy zero-color photon on an empty map), the test
      stores photons at `P.x = 1..count` and asserts the sink's observed max `P.x == count` —
      directly pins "was the last stored photon dropped" without coupling to that internal
      detail.
- [X] T012 [P] Write failing test `tests/preview/test_debugdump_roundtrip.cpp` (write via
      `CDebugView`'s point/line/triangle/quad writers, reopen, count via a test sink; quad → 2
      triangles; last written record emitted exactly once)
- [X] T013 Registered T011 and T012 in `tests/preview/CMakeLists.txt`; confirmed both failed
      to compile (Red — `ri/dataView.h` did not exist yet)
- [X] T014 Created `src/ri/dataView.h`: `class CPrimitiveSink` (pure-virtual `triangles`,
      `triangleMesh`, `lines`, `points`, `disks`) and `class CDataView` (replaces `CView`; same
      six static drawing entry points, same `chunkSize = 128 * 3`, plus `numChannels`,
      `channelName`, `currentChannel`, `detailLevel`, `drawMode`, `typeName` accessors) per
      research.md §1 and data-model.md.
      **Naming-collision correction found during implementation**: `CTexture3d` already had
      its own `int numChannels;` member field, and `CBrickMap` already had its own
      `static int detailLevel;` — C++ does not allow a class to declare both a data member
      and a same-named member function, so the new virtual accessors under those exact
      names could not be implemented on the classes that needed them. Fixed by renaming the
      **pre-existing internal fields** (not the new public accessor names, which match
      data-model.md/tasks.md exactly): `CTexture3d::numChannels` → `channelCount` (~35 call
      sites across `texture3d.h/.cpp`, plus external cross-references in
      `brickmap.cpp:1651`, `remoteChannel.cpp:710,715`, and `pointHierarchy.cpp:132` — a
      fourth `CTexture3d` consumer this task list never mentioned); `CBrickMap::detailLevel`
      → `detail`, not `level`, since `CBrickMap::draw()` already has an unrelated local
      `int level;` a few lines below the static's use — reusing that name would have been
      legal but confusing. `rendererDisplay.cpp`'s unrelated `CDisplayData::numChannels`
      (image display channels, a different struct entirely) was left untouched.
- [X] T015 Created `src/ri/dataView.cpp`: the six static forwarders to an installed
      `CPrimitiveSink*`, and the debug-dump parser lifted from
      `git show 33506f4^:src/gui/opengl.cpp`'s `pglFile`, fixed to
      `while (fread(&tag, sizeof(int), 1, f) == 1)` (not `!feof`) and splitting the quad tag
      into two triangles (`0,1,2` and `0,2,3`). The disc-basis NaN fix research.md §1 also
      lists under this heading does **not** belong here — the dump format has no disc tag,
      and disc geometry expansion is CPU-side in `libribpreview` (Phase 2E), not this parser;
      deferred to T046-T049 where it actually applies.
- [X] T016 [P] Reparented `src/ri/debug.h` from `CView` to `CDataView`
      (`#include "gui/opengl.h"` → `#include "dataView.h"`; `: public CView` → `: public
      CDataView`); added the required `typeName() { return "Debug Dump"; }` override.
      **Investigated-and-cleared, not a bug**: initially suspected `CDebugView::quad()` never
      wrote its 4th point (`fwrite(P4, ...)` looked missing from a truncated `git show`
      excerpt during investigation) — re-read the actual current file and confirmed the
      write is present and correct; `test_debugdump_roundtrip` passed on the first run with
      no writer change needed. No fix applied; noted here only so this false lead isn't
      rediscovered.
- [X] T017 [P] Reparented `src/ri/texture3d.h` from `CView` to `CDataView`; added
      `numChannels()`/`channelName()` overrides backed by the renamed `channelCount`/
      `channels` fields (shared by every `CTexture3d` subclass, so implemented once here —
      see T014's naming-collision note for why the field rename was necessary).
- [X] T018 [P] Reparented `src/ri/photonMap.h` from `CView` to `CDataView`; added
      `typeName() { return "Photon Map"; }`; fixed the confirmed off-by-one in
      `CPhotonMap::draw` (`src/ri/photonMap.cpp`): the loop now runs `numItems` times from
      `items + 1`, matching `CPointCloud::draw`'s correct bound, not `numItems - 1`. Pinned
      by T011. (`CPhotonMap::bound()` has the same `i < numItems` off-by-one one method
      down — found but deliberately **not** fixed: no task in this phase scopes it, no test
      covers it, and its failure mode is a marginally undersized bounding box, not a crash
      or dropped-data desync. Flag for a future cleanup pass.)
- [X] T019 [P] `src/ri/pointCloud.cpp`/`.h` already reach `CDataView` transitively through
      `CTexture3d` (T017) — `CPointCloud` never inherited `CView` directly. Replaced its
      `printf` channel output (`pointCloud.cpp` `keyDown()`) with the new accessors: added
      `typeName() { return "Point Cloud"; }`, `currentChannel() { return drawChannel; }`,
      `drawMode() { return drawDiscs; }` (0 = points, 1 = discs — matches `drawDiscs`
      directly, no translation needed); updated `keyDown()`'s channel-clamp to use the
      renamed `channelCount`.
- [X] T020 [P] `src/ri/brickmap.cpp`/`.h` likewise already reach `CDataView` transitively
      through `CTexture3d`. Replaced its `printf` detail-level/channel output with the new
      accessors: `typeName() { return "Brick Map"; }`, `currentChannel() { return
      drawChannel; }`, `detailLevel() { return detail; }`, `drawMode() { return drawType; }`
      (0 = boxes, 1 = discs, 2 = points — matches `drawType` directly). **Scope correction**:
      did not wire up `CDataView::drawTriangleMesh` in `CBrickMap::draw()`'s box-drawing
      path. Confirmed via `grep` that no `draw()` body anywhere calls `drawTriangleMesh`
      today (it was declared but never `osResolve`d under the old `CView` system, exactly as
      this task says) — converting the existing per-vertex `drawTriangles` box rendering to
      an indexed mesh would be new logic, not a base-class swap, and would violate the
      "zero diff in draw() bodies" principle research.md/plan.md both commit to for no
      test-covered benefit. `CDataView::drawTriangleMesh` exists and forwards correctly
      (T015) for whenever a future change actually wants it.
- [X] T021 [P] **Corrected — both halves were no-ops**: `src/ri/radiance.cpp` (`CRadianceCache`)
      is not compiled at all (absent from `src/ri/CMakeLists.txt`) and references neither
      `CView` nor `gui/opengl.h` — dead code, nothing to reparent. `src/ri/irradiance.h`'s
      `CIrradianceCache : public CTexture3d` already reaches `CDataView` transitively via
      T017 with zero direct reference of its own to change. The one real, additive change
      needed: `CIrradianceCache` is concrete and must satisfy the new pure-virtual
      `typeName()` — added `{ return "Irradiance Cache"; }`.
      **Also found, not originally scoped by any task**: `src/ri/pointHierarchy.h`
      (`CPointHierarchy : public CTexture3d, ...`) is a fourth `CTexture3d` consumer this
      task list never mentioned. Its empty stub `draw()`/`bound()` (matching the
      out-of-scope "point-hierarchy variant" assumption) needed the same `typeName()`
      treatment (`{ return "Point Hierarchy"; }`), and `pointHierarchy.cpp:132` needed the
      `numChannels` → `channelCount` cross-reference update from T014.
- [~] T022 **Deferred to Phase 2C, folded into T024** — cannot delete `src/gui/` yet.
      `src/ri/show.cpp` (the dead `CShow` hider, not deleted until T024) still
      `#include`s `gui/opengl.h` for `CView::handle`/`drawTriangles`/etc. — its whole
      dlopen-based module-loading block is unreachable today (confirmed in the original
      pre-spec investigation: the `gui.dsply` module it looks for was deleted in `33506f4`,
      so this always hits `error(CODE_SYSTEM, "Opengl wrapper not found...")`) but the code
      still has to *compile*. Since every class it references (`CPhotonMap`, `CTexture3d`,
      `CDebugView`) now derives from `CDataView` instead, `show.cpp`'s local `CView *view`
      and the `pglVisualize`-resolved function pointer no longer type-check against them.
      Applied the minimal interim fix to keep `show.cpp` compiling without touching
      `gui/opengl.h`/`CView` itself: a locally-scoped `typedef void
      (*TGlVisualizeFunction2)(CDataView *)` and `CDataView *view` in place of the `CView`
      equivalents. `src/gui/opengl.h` itself is untouched and will be deleted together with
      `show.cpp` in T024, when both halves of the dependency disappear in the same commit.
- [X] T023 T011 and T012 pass (Green). Full preview suite: `ctest --test-dir build -L
      preview` — 11/11 pass (9 pre-existing + 2 new). Full project build
      (`cmake --build build --config Release`) succeeds with no new warnings from touched
      files. `ctest -L visual` could not serve as an additional regression check in this
      worktree — pre-existing, unrelated to this change: the `openrender/` deploy tree
      (shaders/displays) was never populated here (confirmed absent entirely via `ls`; per
      CLAUDE.md's documented deploy-tree gotcha, this requires a manual `cmake --install`
      this worktree never had run), so all 191 visual tests fail identically on
      "Failed to find shader defaultsurface" — an environment gap, not a code regression.
      The preview suite is the regression gate this task actually specifies, and it is green.

**Checkpoint 2B**: The sink abstraction exists and every `src/ri/` visualization class targets
it; nothing outside `libri` consumes it yet (aside from `show.cpp`'s dead, interim-patched
`CShow` hider, removed whole in T024).

### 2C — Remove `oshow` and FLTK (increment I2), one cohesive removal

- [X] T024 Delete `src/oshow/` in its entirety, and `src/ri/show.h`/`src/ri/show.cpp`
- [X] T025 Remove the `CShow` `#include`, dispatch, and construction from
      `src/ri/renderer.cpp` (the include, `preDisplaySetup` call, and `new CShow(i)` site) and
      `src/ri/rendererContext.cpp` (the `CShow`-related include)
- [X] T026 [P] Remove the `show.cpp` entry from `src/ri/CMakeLists.txt`; remove
      `add_subdirectory(oshow)` from `src/CMakeLists.txt`
- [X] T027 [P] Remove `option(BUILD_SHOW ...)`, the `fltk-config` probe block, and the
      `oshow` entry in the codesign executable list from the root `CMakeLists.txt`
- [X] T028 [P] Removed the six oshow scene registrations and rewrote the now-stale doc comment
      in `tests/visual/CMakeLists.txt`. **Correction**: kept the `add_not_required_test` macro
      itself (rephrased its comment to drop oshow-specific rationale, retained the still-valid
      photon-motion-blur rationale) — it is also used by the unrelated
      `motion-subdiv-translate-photon` scene, so deleting it would have broken that test.
- [X] T029 [P] Remove the five FLTK install steps from `.github/workflows/release.yml`
- [X] T030 [P] Rename the six `*-oshow.rib` fixtures (in `examples/rib/tests/` and
      `examples/rib/tests/parity/`) to `*-wire.rib`, dropping their `Hider "oshow:none"` line
      (`orender-wire` ignores `Hider` entirely, so no replacement statement is needed)
- [X] T031 [P] Corrected every stale `oshow` reference in the named files (`README.md`,
      `INSTALL.md`, `INSTALL_ARTIFACTS.md`, `COMPILING.txt`, `HOMEBREW_GUIDE.md`,
      `openrender.rb.template`, `openrender.spec`, `AUTHORS.md`, `CLAUDE.md`, `DEVNOTES.md`, the
      `oshow(1)` SEE ALSO cross-references in 4 man pages), preserving historical/attribution
      content by rephrasing rather than deleting (e.g. `AUTHORS.md`'s Jordan Smith credit line
      now reads "Crystal ball interface for the original (since-removed) interactive viewer").
      **Expanded beyond the named list** once T032's gate was run repeatedly: also fixed
      `DEVNOTES_DETAILS/SUBDIVISION_SURFACES.md`, `DEVNOTES_DETAILS/HIDER_PARITY.md`,
      `DEVNOTES_DETAILS/PATH-TRACING_HIDER.md`, and four `docs/site/content/...` pages (`faq.md`,
      `source-at-a-first-glance.md`, `installing-and-running.md`) — the task list's file
      enumeration wasn't exhaustive; T032's grep is the actual completeness gate, not this list.
- [X] T032 Removal gate: `grep -rn "CShow\|oshow\|BUILD_SHOW\|FLTK" .` from the repository root
      now returns zero hits, but the exemption list needed three additions beyond `build/`,
      `specs/`, and `ChangeLog.md`, all judgment calls made during this task and recorded here
      rather than left implicit:
      - **`NEWS.md`** — a dated, historical release-notes entry describing a past release
        verbatim; same category as `ChangeLog.md`, whose exemption was almost certainly an
        oversight that also should have named `NEWS.md`. Left unedited rather than falsified.
      - **`doc/`** (the legacy `Documentation/`/`Tutorials/` HTML tree) — inspection showed
        these are byte-mirrors of the external "PixieWiki" (page titles read "PixieWiki", body
        text says "Pixie is a photorealistic renderer", uses pre-rename tool names like
        `sdr`/`sdrinfo`). This is a frozen historical archive of the *predecessor* project's own
        docs, not current openRender documentation — unlike `docs/site/`, which is the live
        Hugo site and *was* corrected. Scrubbing only the `oshow` mentions out of a page that
        still says "Pixie" throughout would be inconsistent, selective revisionism of a
        preserved external artifact, so the whole tree is exempt.
      - **`.claude/settings.local.json`** — a gitignored Claude Code permission-cache file, not
        project documentation; its one match is a cached bash-command string from an earlier
        session's *different* checkout path. Left untouched as out-of-scope tooling state.
      Also checked (no action needed): `specs/010-full-subdivision-support/contracts/
      hider-invariant-contract.md` references a `show.cpp` grep target in prose, but that check
      is not wired into any CMakeLists/CI script anywhere in the tree — it's unexecuted spec
      prose, and the file is already under the exempted `specs/` tree regardless.

**Checkpoint 2C**: No trace of the legacy tool or its build dependency remains anywhere in the
tree except historical records.

### 2D — Headless loader and C ABI (increment I3), tests first

- [X] T033 [P] Write failing test `tests/preview/test_data_world_init.cpp` (after opening a
      constructed photon-map fixture headlessly, the view's transform is non-degenerate and
      `bound()` returns finite values — pins the identity-matrix seeding requirement). **Fixture
      correction**: the fixture itself must seed `CRenderer::fromWorld`/`toWorld` to identity
      before writing (`CPhotonMap::write()` persists them into the file), and the test must
      deliberately corrupt those statics to a degenerate value *between* writing the fixture and
      calling `open()` — otherwise the assertions can pass on a zero-times-identity coincidence
      instead of actually exercising `open()`'s seeding.
- [X] T034 [P] Write failing test `tests/preview/test_data_pointcloud.cpp` (construct a
      `CPointCloud` via its write constructor, `store()` known points, destroy to flush, reopen
      through `CDataDocument::open()`, assert type/count/bounds/channels match; also repeat the
      round-trip storing zero points and assert the reopened document is valid — covers FR-005).
      **Spec-level correction, found while implementing**: FR-005's literal wording ("all counts
      == 0") does not hold for point clouds (or photon maps). `CPointCloud::balance()`
      (`pointCloud.cpp:279-295`) — and `CPhotonMap::balance()` identically
      (`photonMap.cpp:503-512`) — unconditionally inserts one dummy item at the origin with
      zeroed data before writing an otherwise-empty map, "to avoid an if statement during
      lookup." This is deliberate, existing, working behavior on the hot lookup path and is out
      of scope to change for this feature. A zero-point round trip is still valid (doesn't
      crash, reopens successfully) but always reports exactly one degenerate point at the
      origin, never a literal zero count. The test asserts this documented convention instead of
      a literal zero. **This same correction applies to T041's `test_wire_cli`** — a `--json`
      fixture built the same way will report `"points": 1`, not `0`; do not re-derive this there.
      (Debug-geometry dumps have no such convention and can genuinely report zero of every
      primitive kind — the correction is specific to `CPointCloud`/`CPhotonMap`.)
- [X] T035 Register T033 and T034 in `tests/preview/CMakeLists.txt`; confirm both fail to
      compile/link (Red — `CDataDocument` does not exist yet)
- [X] T036 Create `src/ri/dataLoad.h` and `src/ri/dataLoad.cpp`: `dataSniff()` (ports the
      magic-number/version/word-size validation from `src/ri/show.cpp:82-99`) and
      `class CDataDocument` whose `open()` sets `CRenderer::fromWorld`, `toWorld`,
      `fromWorld1`, `toWorld1`, `fromNDC`, `toNDC` to identity and `worldBmin`/`worldBmax` to
      ±infinity **before** constructing any reader, then constructs `CPhotonMap`,
      `CIrradianceCache`, `CPointCloud`, `CBrickMap`, or `CDebugView` directly (never via
      `CRenderer::getPhotonMap`/`getCache`/`getTexture3d`, which assert on mid-render-only
      state) and owns whichever it constructs uniformly in the destructor. **Correction, not
      verbatim**: `show.cpp`'s own version check (`!((version[0]==VERSION_MAJOR) ||
      (version[1]==VERSION_MINOR))`) was loose OR-based dead code that almost never actually
      fired — the real gatekeeper in the legacy system was `ropen()`'s strict AND-based check
      (`(version[0]!=VERSION_MAJOR) || (version[1]!=VERSION_MINOR)`), which is what every actual
      `CRenderer::get*` reopen enforced. `dataSniff()` uses the strict, actually-enforced check,
      since it is now the sole gatekeeper (no double-open/`ropen`-reopen fallback remains).
      **Debug-dump detection is stricter than legacy**: `show.cpp` treated *any* non-magic-
      matching file as a debug dump unconditionally; `dataSniff()`/`sniffHeader()` adds a real
      structural dry-run validator (`isValidDebugDump()`: reads bmin/bmax, then walks the
      tag/payload stream to confirm it reaches exact EOF with only known tags) so a RIB file or
      garbage input correctly reports `DATA_NOT_A_DATA_FILE` instead of being misidentified,
      satisfying data-model.md's validation rule.
      **Ownership per reader type, worked out from source (not stated in research.md)**:
      `CPhotonMap` neither closes nor retains `in` (caller must `fclose` immediately after
      construction); `CIrradianceCache` and `CPointCloud` both close `in` themselves on every
      path; `CBrickMap` and `CDebugView` both retain `in` for their full lifetime (lazy brick
      reads / re-open-by-name at draw time) and close it in their own destructors. `open()`
      follows this per-type, not a single uniform rule.
- [X] T037 Confirmed T033 and T034 pass (Green). `CTexture3d::retrieveDisplayChannel`
      (`texture3d.cpp:119`) is **not** reachable on the read-only load path — it is called only
      from `defineChannels(const char *)` (the comma-string overload), never from
      `defineChannels(int, char **, char **)` (the array overload) or from `readChannels()` (the
      read-constructor path `CPointCloud(name, from, to, in)` actually calls). No stub needed.
      **Caveat for a later fixture**: this conclusion covers the read path and the array-overload
      write path (used by this task's own fixture and by `ptcapi.cpp`) — if a brick-map fixture
      is later built using the comma-string `defineChannels(const char *)` overload instead, it
      *will* reach `retrieveDisplayChannel` and need a pre-declared channel; check which overload
      that fixture actually uses.
- [X] T038 Extended `src/preview/ribpreview_api.h` per contracts/c-abi.md: `PrimArrayC`,
      `DataSceneC`, the `RibDataType` enum, and the opaque-handle functions `ribdata_sniff`,
      `ribdata_open`, `ribdata_snapshot`, `ribdata_key`, `ribdata_channel_name`,
      `ribdata_close`. Also appended two trailing fields (`fov`, `frameAspectRatio`) to the
      existing `PreviewCameraC` struct — contracts/cli-interface.md's JSON schema requires them
      in the camera object, but the C ABI never exposed them even though the C++ `PreviewCamera`
      already tracked both; backward-compatible append, both `previewContext.cpp` (RIB path) and
      `dataSink.cpp` (data path) populate them. `CRibPreview.h`'s duplicate-header collapse
      (research.md's SPM `headerSearchPath` risk) was proven separately in the macOS build; no
      blocker here.
- [X] T039 Created `src/preview/libribpreview/dataScene.h`: `DataScene` struct (four flat
      primitive arrays — lineVerts/Cols, pointVerts/Cols, triVerts/Cols, plus a pre-expansion
      `std::vector<DiskPrimitive> disks`) and `AABB`/`PreviewCamera` fields per data-model.md.
- [X] T040 [P] Wrote `tests/preview/test_data_keys.cpp`: drives `ribdata_key()` with each legacy
      key (`m l b d p q w`) against sequentially-opened brick-map and point-cloud fixtures,
      asserting `DataSceneC` field changes and zero bytes on stdout (captured via dup/dup2).
      **Two non-obvious fixes required to get this fixture working, both documented for anyone
      building a similar fixture later:**
      - **`renderMan` NULL-deref**: `info()`/`error()` unconditionally dereference the global
        `renderMan` (error.cpp); nothing sets it outside a `CDataDocument`/`RiBegin` bracket, and
        `makeBrickMap()` (the only usable brick-map fixture writer — `CBrickMap`'s direct write
        ctor needs a protected `CChannel` type) calls `info()`/`error()` internally. Fixed in
        production at T036 (`CDataDocument::open()`/`~CDataDocument()`, see T037's notes) and
        worked around in the test by save/restoring a scratch `CRiInterface` around the
        `makeBrickMap()` call.
      - **Single-document concurrency (FR-019)**: opening both fixtures simultaneously
        SIGSEGVs inside `CRenderer::shutdownDeclarations()`, because `CDataDocument` manipulates
        global state and the design assumes exactly one instance alive at a time. Fixed by
        testing each fixture sequentially (open → drive keys → close) rather than holding both
        open at once — this is the FR-019 constraint surfacing as a real test-authoring
        constraint, not a bug.
- [X] T041 [P] Wrote `tests/preview/test_wire_cli.cpp`: argument grammar (help/version/usage
      errors/file-not-found/two-positionals), `--json` well-formedness (`schemaVersion`,
      per-`documentType` required keys) for both a RIB scene and a point-cloud fixture, the
      FR-008 camera-framing check (near/far clipping actually brackets the reported bounds'
      diagonal), the SC-004 2-second budget, and an auto-detection regression test added during
      this task (see below). **Two findings from this task changed behavior/scope and are
      recorded here rather than silently worked around:**
      - **Exit code 3 ("RIB parse failed") is specified but not currently reachable.**
        `ribpreview_load()`/`ribParse()` never signal a syntax-level parse failure — the RIB
        grammar recovers from malformed input rather than aborting, so even genuinely invalid
        RIB text (e.g. `"this is not a valid RIB file {{{"`) parses "successfully" into an
        empty scene, exit 0. `emitRibJson()`'s `return 3` branch for a NULL
        `ribpreview_load()` result is the contract's correct handler for a case the current
        parser cannot produce — it is not dead code to delete, just currently unreachable. The
        planned test case for this was removed rather than replaced with a heuristic (a
        declarations-only RIB with zero geometry is legitimately empty and must not be treated
        as a parse failure). **Follow-up, out of scope for spec 016**: give `ribParse()`/
        `ribpreview_load()` a real failure-signaling channel (e.g. an error-count callback) —
        this is spec-006/RIB-parsing territory, not spec 016's. Exit code 5 remains the other
        documented-but-unreachable-under-`--json` code (GUI-only, by design).
      - **Pre-existing bug found while probing the RIB path with an empty scene, out of scope
        to fix here**: `previewContext.cpp`'s clipping-plane synthesis (`ribpreview_load()`,
        ~line 167-171) computes `diag = sqrt(dx²+dy²+dz²)` from `scene.sceneBounds`, which stays
        at its reset value (`±FLT_MAX`) for a scene with no geometry. `dx = -FLT_MAX - FLT_MAX`
        overflows to `-inf` in float, so `farPlane` becomes `inf` — not valid JSON. This is
        pre-existing spec-006 code, unrelated to spec 016's data path (whose own
        `synthesizeCamera()` in `dataSink.cpp` already guards the degenerate/non-finite-box
        case). Since **T041's own contract** is "well-formed JSON" for `orender-wire --json`,
        added a local, minimal guard entirely inside `wireCli.cpp` (a `sane()` helper clamping
        non-finite camera floats to `0.0` before printing, applied in both `emitRibJson()` and
        `emitDataJson()`) rather than touching `previewContext.cpp`'s synthesis logic.
      - **Auto-detection routing bug, fixed (not just a test finding)**: `ribdata_sniff()`
        collapsed both "no data-file magic number at all" and "magic matched but
        version/word-size incompatible" to the same `-1` return value. `wireCliRun()`'s
        `--type=auto` path treated any `-1` as "not a data file, try RIB" — meaning a corrupted
        or version-mismatched data file would silently "succeed" as an empty RIB scene (exit 0)
        instead of being rejected (exit 4), the one behavior change here a real user could hit.
        Fixed by extending `ribdata_sniff()`'s contract with a distinct `-2` sentinel for
        "recognized magic, incompatible" (`ribpreview_api.h` contract comment updated to
        document both sentinels explicitly) and updating auto-mode to route anything `!= -1` to
        the data path. Covered by a new test case that corrupts a real point-cloud fixture's
        on-disk `VERSION_MAJOR` field (4 bytes after the leading magic number, confirmed against
        `fileResource.cpp`'s writer) and asserts `--json` (no `--type` override) exits 4.
- [X] T042 Registered T040 and T041 in `tests/preview/CMakeLists.txt`; both failed Red before
      T043/T044 existed (confirmed via the standard TDD sequence for this feature).
- [X] T043 Created `src/preview/libribpreview/dataSink.h`/`.cpp`: `class CDataSceneSink :
      public CPrimitiveSink` appending into a `DataScene`; `decimateGrouped()`/
      `decimateDisks()` (even-stride sampling, `MAX_PRIMITIVES_PER_KIND = 100000`, matching
      `tessPoints.cpp`'s existing `MAX_POINTS` idiom); `synthesizeCamera()` (hand-rolled
      row-major look-at + perspective framing of the reported bounds, with a degenerate/
      non-finite-box fallback); `buildDataScene()`; and the six `ribdata_*` C-linkage functions
      (including the `ribdata_sniff()` fix above) against `CDataDocument` from T036.
- [X] T044 Created `src/preview/libribpreview/wireCli.h`/`.cpp`: shared argument parsing,
      `--help`, `--version`, `--json` (exit codes documented in cli-interface.md, JSON
      serialization with the `sane()` finiteness guard above), `--type=auto|rib|data`, C linkage
      for both platform frontends. Auto-detection calls `ribdata_sniff()` first and falls back
      to the RIB path only on a genuine `-1` ("no data-file magic at all").
- [X] T045 Confirmed T040 and T041 pass (Green) — full `ctest --test-dir build -L preview`:
      15/15 passing, including both. Full `cmake --build build` (all targets, both the Linux-
      style CLI pieces and the macOS SwiftUI/Metal `orender-wire.app`) rebuilt clean after the
      `PreviewCameraC` field addition, confirming no frontend regression from the C ABI change.

**Checkpoint 2D**: The entire data path — sniff, load, snapshot, interactive state, headless
JSON — is proven correct before either platform's GUI code has been touched.

### 2E — Disc expansion (increment I4)

- [X] T046 [P] Wrote `tests/preview/test_data_disk_expand.cpp`: exactly 60 vertices per disc (20
      triangles x 3, non-indexed — matches the deleted `pglDisks`'s 20-segment fan, stored as a
      flat triangle list since neither GL 3.3 core nor Metal has `GL_TRIANGLE_FAN`); every rim
      vertex within ~1e-3 of the disc's radius from `P`; every triangle's plane normal parallel
      (or anti-parallel) to `N`; four cases including `P == (0,0,0)` and `P ∥ N`, all asserting
      every emitted vertex is finite. **Found and fixed a real stride bug in already-written
      T043 code before writing this test**: `DiskPrimitive`'s `dP` field (and
      `CDataSceneSink::disks()`'s handling of it) was modeled as a `float3` "radius vector", but
      `CPrimitiveSink::disks()`'s `dP` argument is a **scalar** radius array (stride 1) at every
      real call site — confirmed against `pointCloud.cpp:353` (`float dP[chunkSize]`, not
      `chunkSize*3`), `brickmap.cpp:1148`/`1338` (`float R[chunkSize]`, `cR += 1`), and
      `irradiance.cpp:1086`/`1100` (same pattern), all consistent with the deleted historical
      `pglDisks`'s own `dP++` scalar iteration (`git show 33506f4^:src/gui/opengl.cpp:107`).
      `CDataSceneSink::disks()` was reading `dP[i*3+0..2]` for an `n`-element array — a 3x
      out-of-bounds heap over-read for every disc passed through the sink, never previously
      exercised by any test because nothing yet called `expandDisk()` or otherwise read the
      `disks` field. Fixed by renaming the field to a plain `float radius` and reading `dP[i]`.
      Documented at the point of use (`dataScene.h`) so it can't regress silently.
- [X] T047 Registered T046 in `tests/preview/CMakeLists.txt`; confirmed Red (`diskExpand.h` not
      found) before creating the implementation.
- [X] T048 Created `src/preview/libribpreview/diskExpand.h`/`.cpp`: 20-segment CPU-side disc
      expansion matching the deleted `pglDisks` geometry (radius, segment count), but with an
      axis-picking basis derived from `N` alone — **never** `P × N`, the deleted
      implementation's basis, which is NaN when `P == (0,0,0)` or `P ∥ N` (both are structurally
      impossible to hit here, since the new basis has no dependency on `P` at all — not just
      guarded against, but removed). Wired into `dataSink.cpp`'s `buildDataScene()`: discs are
      decimated by disc *count* first (`decimateDisks()`, unchanged from T043), then every
      surviving disc is expanded and appended to `scene.triVerts`/`triCols` — so the disc
      decimation cap and the triangle decimation cap stay independent, as `dataScene.h`'s
      original design intended.
- [X] T049 Confirmed T046 passes (Green: 332/332 assertions). Full
      `ctest --test-dir build -L preview`: **16/16 passing** — every Foundational test (T011,
      T012, T033, T034, T040, T041, T046) green, plus all pre-existing preview coverage
      unaffected by the `DiskPrimitive` field rename.

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

- [X] T050 [P] [US1] Wrote `tests/preview/test_preview_subdiv.cpp`: loads all six retargeted RIB
      scenes (`examples/rib/tests/**/*-wire.rib` and `.../parity/motion-subdiv-translate-wire.rib`,
      from T030) through `ribpreview_load()`, asserting non-zero vertex count, finite bounds, and
      finite vertices for each. **This task's own assumption — "the tessellator is pre-existing
      and unchanged, so this test may pass immediately" — was wrong, and finding out was the
      point of writing the test first.** `subdiv-loop-wire.rib` (`SubdivisionMesh "loop"`)
      loaded successfully but produced **zero vertices**: `CRibGeometryContext::
      RiSubdivisionMeshV` (`ribGeometryContext.cpp:690`, pre-existing spec-006 code, unrelated to
      spec 016's own new code) unconditionally rejected any scheme string other than
      `RI_CATMULLCLARK`, silently dropping Loop-scheme meshes with no warning — a real,
      previously-latent gap in `orender-wire`'s wireframe path, not a spec-016 regression. Since
      this exact fixture was retargeted specifically to demonstrate Loop-scheme support in the
      new viewer (see the fixture's own header comment), leaving it broken would defeat the
      point of the retarget — in scope to fix here. Fixed by also accepting `RI_LOOP`: the
      preview path's tessellator (`tessSubdivision.cpp`) only ever draws the base control-cage
      edges regardless of scheme (it never subdivides), so the scheme string was purely a
      restrictive filter with no corresponding rendering logic tied to it — safe to relax.
      `RiHierarchicalSubdivisionMeshV`'s equivalent check was left untouched (still
      catmull-clark-only): none of the six fixtures exercises `HierarchicalSubdivisionMesh
      "loop"`, so there is no test-driven reason to extend it, and doing so speculatively would
      be scope creep.
- [X] T051 [US1] Registered T050 in `tests/preview/CMakeLists.txt` (with the same
      `WORKING_DIRECTORY` override as `test_preview_integration`, since it uses relative RIB
      paths); confirmed it fails Red for `subdiv-loop-wire.rib` before the
      `RiSubdivisionMeshV` fix above, and passes Green after. Full
      `ctest --test-dir build -L preview`: 17/17 passing.

### Implementation for User Story 1

- [X] T052 [US1] Both frontends now route a newly opened file through content-based detection
      (`ribdata_sniff` vs. the existing RIB-parse attempt) instead of always assuming RIB, per
      FR-001 — implemented together with T053/T054 below (`ViewerModel.load()` on macOS,
      `load_scene_thread()` on Linux), since the routing decision and the two load paths it
      chooses between are inseparable in both files. Both mirror `wireCliRun()`'s auto-detection
      rule exactly: `ribdata_sniff() != -1` routes to the data path, so a `-2` (recognized magic,
      incompatible version/word-size) is correctly treated as "this is a data file, let
      `ribdata_open()` report the real failure" rather than silently falling back to an empty
      RIB scene.
- [X] T053 [US1] Added Metal render pipelines for points and reused the existing pipeline for
      triangles in `src/preview/orender-wire-macos/Sources/WireframeRenderer.swift`. **Only one
      new pipeline was actually needed, not three**: discs arrive already CPU-expanded into the
      triangle buffer (T048's `diskExpand.h`), so "triangles" and "discs" share one pipeline and
      one `drawPrimitives(type: .triangle, …)` call — only points need a distinct vertex
      function, since Metal renders `.point` primitives as 1px dots unless the vertex shader
      writes `[[point_size]]` itself (the existing `sceneVertex`/`SceneOut` never did). Added
      `pointVertex`/`PointOut`/`pointsPipeline` for that case. `encoder.setCullMode(.none)` is
      applied once per frame (not per-draw-call) since it costs nothing on lines/points, which
      Metal never culls regardless. Extended `ViewerModel.load()` to branch on `ribdata_sniff()`
      (T052) into `loadRibScene()` (unchanged existing behavior) or a new `loadDataDocument()`
      that calls `ribdata_open()`/constructs `WireframeRenderer(metalDevice:dataDoc:)`. This new
      initializer **retains** the opened `RibDataDocument*` handle (via `ribdata_snapshot()`, not
      freed like `ribpreview_free()`) for the document's session lifetime, since User Story 2's
      `ribdata_key()` re-emit cycle (not implemented yet — T058) needs the underlying `CDataView`
      to stay alive; released in `deinit`. Swift 6 required `nonisolated(unsafe)` on the stored
      `OpaquePointer` for the `@MainActor` class's (nonisolated) `deinit` to read it. Verified:
      `cmake --build build` (full project including this target) succeeds; a real point-cloud
      fixture (`.ptc`, 200 points across 10×10×2, via `CPointCloud`'s write constructor) was
      opened with the built `orender-wire.app` binary directly (`ORENDER_WIRE_GUI=1`, bypassing
      the terminal-detach re-exec) and stayed running with no crash and no stderr output after 3
      seconds — the launch, content-detection routing, `ribdata_open`, and GPU buffer upload
      paths all complete without crashing. **Not verified**: the actual rendered pixels (no
      display/interactive session available in this environment) — deferred to `quickstart.md`
      manual validation (T057).

      **Update (T057 manual validation, 2026-09-08): found and fixed a real, significant bug —
      every data document rendered as a completely blank window**, confirmed on the user's
      macOS machine across all four fixture types tried (point cloud, brick map, photon map,
      debug-geometry dump). The RIB path (`camera-dof.rib`) rendered correctly, and the
      on-screen status overlay (channel/detail/draw-mode) correctly reflected each document's
      real state, proving the whole data-loading pipeline was sound — only the actual
      geometry was invisible. **Root cause**: `dataSink.cpp`'s `synthesizeCamera()` built its
      view/projection matrices assuming the OpenGL-standard convention (camera looks down `-Z`,
      `w' = -z_view`), but openRender's own camera convention — confirmed against
      `ribGeometryContext.cpp:337-352`'s projection matrix, the one the already-working RIB path
      uses — is the opposite: `proj[14] = 1.0` (not `-1.0`), meaning `w' = +z_view`, camera looks
      down `+Z`. Paired with the wrong-convention view matrix, this put the entire synthesized
      scene behind the camera. Since the grid/axis overlay is drawn every frame with the exact
      same view-projection matrix regardless of document type, its total absence (not just the
      data geometry's) was the key clue that pointed at the camera math rather than the
      vertex-buffer upload path. **Why the existing test suite never caught this**:
      `test_wire_cli.cpp`'s FR-008 check only asserts the *scalar* near/far plane values are
      sane (`nearP > 0`, `farP - nearP > diagonal`) — it never renders anything or checks the
      matrices' actual orientation, so a numerically-plausible but geometrically-inverted camera
      passed every automated check while being completely non-functional. **Fixed** by flipping
      the view matrix's Z basis (now points from eye toward center, matching the "+Z into the
      screen" convention) and rewriting the projection matrix's Z-row to match
      `ribGeometryContext.cpp`'s exact formula (`proj[10] = far/(far-near)`,
      `proj[11] = -(far*near)/(far-near)`, `proj[14] = 1`) instead of the OpenGL-standard form.
      Rebuilt and confirmed `ctest -L preview` still 17/17 (the existing numeric checks don't
      distinguish the two conventions, as expected) — **re-verification with an actual rendered
      window from the user is the next step**, this fix has not yet been visually confirmed.

      **Second, separate bug found on re-test: the camera fix had never actually been linked
      into the tested binary.** The user re-tested all four fixtures after the fix above and
      still saw a completely blank window for every one, unchanged from before. Comparing
      `stat` timestamps exposed why: `build/.../swift-build/release/orender-wire-macos` (the
      real SPM link output) was from **9:54 AM that day** — hours before the camera fix (and
      possibly predating other same-day changes) — while the `.app` bundle's copied binary had
      a fresh timestamp from *this* build. Root cause: `orender-wire-macos/CMakeLists.txt`'s
      custom command correctly re-invokes `swift build` whenever its own `DEPENDS` list
      (`libribpreview.a`/`libopenrendercommon.a`) changes, but `swift build`'s *own* incremental
      build system only tracks `.swift` source files — it has no visibility into an externally
      linked static library passed via raw `-Xlinker` flags, so it concluded "no Swift source
      changed" and silently skipped the actual link step. The subsequent
      `${CMAKE_COMMAND} -E copy` step then copied that same stale binary into the `.app` bundle
      again, refreshing its mtime and making the *output* look freshly built even though its
      *content* hadn't changed since 9:54 AM. **This means every "smoke test" and manual
      verification claim earlier in this feature that involved rebuilding `orender-wire-macos`
      after a `libribpreview`-only change (no `.swift` file touched) may have silently tested a
      stale binary** — T053's crash-free point-cloud launch test is the clearest one at risk,
      since it followed exactly this pattern (`dataSink.cpp`/`WireframeRenderer.swift` were
      touched together for T053, so that specific rebuild likely *did* relink correctly since
      Swift sources also changed — but any subsequent libribpreview-only rebuild afterward would
      not have). Fixed by adding `${CMAKE_COMMAND} -E remove -f "${WIRE_MACOS_BINARY}"` before
      the `swift build` invocation, forcing SPM to always perform the final link step (its own
      output no longer exists, so "produce this file" is unconditionally true regardless of
      what its source-level change tracking concluded). Confirmed via `stat`: after this fix, a
      rebuild triggered by a `dataSink.cpp`-only change actually re-links (visible `[1/2] Linking
      orender-wire-macos`, ~6s, vs. the previous suspiciously-instant "Build complete! (0.21s)"
      with no link step shown at all) and the output binary's timestamp advances correctly.

      **Third bug, found once the stale-binary issue was fixed and the real rebuild retested:
      geometry rendered (correct shape, color, and — per the Z-basis fix — no longer behind the
      camera), but badly off-center, cut off in a screen corner rather than framed. Panning
      found the content fully intact, confirming the geometry itself and its world-space
      position were correct — only the *initial camera framing* was wrong.** Root cause:
      `PreviewCamera::viewMatrix` must hold the **camera-to-world** matrix, not
      world-to-camera — confirmed independently on both platforms via matching comments in
      `ArcballCamera.swift` (`"viewMatrix = from = to^{-1} (world → +Z-forward baked space)"`)
      and Linux's `arcball.cpp`/`arcball.h` (`"from = to^{-1} = camera-to-world"`,
      `"stores viewMatrix (= camera-to-world, from) directly"`). Both implementations invert
      whatever `PreviewCamera::viewMatrix` gives them and use *that inverted result* directly as
      the actual render/view matrix — so the input must be camera-to-world for the inversion to
      recover world-to-camera. `synthesizeCamera()` had built a standard look-at
      **world-to-camera** matrix (correct in isolation, matching the textbook formula) and
      stored it directly — one matrix-orientation step short of what the consumer needed.
      Fixed by transposing the rotation part and using the eye position as translation
      (camera-to-world = `[Rᵀ | eye]` for the look-at's `[R | -R·eye]`). This is also why camera
      *export* (`cameraToWorldMatrix`) round-trips correctly for the RIB path — same
      requirement, independently confirmed. Rebuilt (relink confirmed via the CMake fix above),
      `ctest -L preview` still 17/17 (no test exercises actual camera-to-world orientation
      end-to-end) — **awaiting the user's re-test for final visual confirmation.**

      **Confirmed by the user: the camera-to-world fix resolved the framing bug — all four data
      documents now open already centered and fully framed on load, no panning needed.** One
      further, distinct bug surfaced during that same re-test: pressing **R** (reset camera)
      squashed the view vertically. Root cause, present on **both platforms** (pre-existing
      spec-006 code, not introduced by this feature, but only now surfaced because a
      synthesized data-document camera's aspect assumption is meaningless where a RIB scene's
      usually isn't): `ArcballCamera.reset()` (macOS)/`ArcballCamera::reset()` (Linux) restore
      `projMatrix`/`projMatrix_` to the ORIGINAL `ribProj`/`ribProj_` — which for a RIB scene
      encodes the file's own declared frame aspect ratio, but for a synthesized data-document
      camera is `synthesizeCamera()`'s hardcoded 1:1 — **without re-applying the
      aspect-ratio correction `updateAspect()` had already made for the actual window shape at
      load time**, silently discarding it and reverting to the stale 1:1 assumption whenever the
      window isn't square. Fixed on both platforms by calling `updateAspect()` again immediately
      after restoring `ribProj`/`ribProj_`, using the arcball's own already-tracked current
      window size — this keeps the "restore the originally-authored vertical FOV/orientation"
      intent of reset intact while no longer discarding the horizontal aspect correction.
      Verified on macOS: rebuilt (relink confirmed), `ctest -L preview` still 17/17 including
      `test_preview_arcball`/`test_camera_export` (which directly exercise `reset()`), confirming
      no regression to the already-working RIB-scene reset behavior. **Linux fix is
      compile-unverified**, same standing caveat as the rest of that file.

      **Confirmed by the user: reset (R) now snaps back to the same correctly-framed,
      non-squashed view.** This closes out the macOS half of T057's core claim — all four data
      documents tested (point cloud, brick map, photon map, debug-geometry dump) plus the
      original RIB scene open, frame correctly on load, and support orbit/pan/zoom/reset,
      confirmed via four independently-diagnosed-and-fixed real bugs along the way (disc-radius
      stride, the RPATH build issue, the stale-SPM-binary CMake issue, the camera-to-world
      convention, and the reset-aspect bug — five, not four). **Still not explicitly walked**:
      clicking through each individual Data-menu item and confirming the visualization *and*
      status text update per action (T063's actual per-control check, as opposed to confirming
      the menu's enabled/disabled state, which *was* observed correctly); the equivalent via
      keyboard shortcuts, including the macOS first-responder regression check after clicking in
      the render view; opening a data file with a primitive count exceeding the decimation cap;
      launching from Finder and via a `bin/orender-wire` symlink; and single-document replacement
      (opening a second file while one is already open). These remain open across T057/T063/T066.

      **User walked the remaining checks (2026-09-09) and found three more real bugs:**

      **Bug 6 — Data menu and keyboard shortcuts, confirmed working** (Batches 1-2 of the
      manual walkthrough): every Data-menu item and its keyboard equivalent correctly updates
      both the visualization and the status text, on both the point cloud and brick map
      fixtures, including after a click-in-view and app-switch (the named first-responder risk
      from research.md §8). No bug — closes out T063's remaining check.

      **Bug 7 — SC-006 violation, ~9.5 minutes to open a 500K-point cloud (target: under 5
      seconds), root-caused via a dedicated investigation.** `diskExpand.cpp`'s `expandDisk()`
      called `outVerts.reserve(outVerts.size() + kVertsPerDisk)` / same for `outCols` on
      **every single call** — `std::vector::reserve()` allocates *exactly* the requested
      capacity (no amortized headroom, unlike `push_back`'s own internal doubling), so calling
      it every invocation with "current size + this batch" forces a full reallocation and copy
      of the *entire* buffer built so far, on every call. Over the ~100,000 discs surviving
      decimation for a 500K-point cloud (default draw mode is discs, `CPointCloud::drawDiscs`
      defaults `TRUE`), this is genuine **O(N²)** — computed at ~7.2 TB of cumulative
      realloc-copy traffic, squarely bracketing the observed 570s at a realistic effective
      throughput. Fixed by removing `expandDisk()`'s internal `reserve()` entirely (documented
      why, so it doesn't get "helpfully" re-added) and instead reserving the *exact final size
      once*, up front, in `dataSink.cpp`'s `buildDataScene()` before the expansion loop — turning
      100,000 calls' worth of full-buffer reallocations into zero. Exposed a new
      `kVertsPerDisk = 60` constant in `diskExpand.h` so the caller's reserve size can't drift
      out of sync with `expandDisk()`'s own per-disc vertex count.

      **Bug 8 — found alongside bug 7, a non-monotonic decimation cap**: `decimateGrouped()`/
      `decimateDisks()` computed `stride = total / cap` via integer division, which truncates to
      `1` for any `total` in `[cap, 2×cap)` — meaning the cap silently **doesn't bind** in that
      range (e.g. 199,999 discs would all survive, not be capped to 100,000, doing ~4x the
      downstream work of a cloud comfortably over the cap — non-monotonic: a *smaller* point
      count can take *longer* to open). Fixed by switching both functions to an
      output-indexed exact-cap scheme (`srcIndex = i * total / cap` for output index `i` in
      `[0, cap)`) that always keeps exactly `cap` elements, evenly spread, regardless of the
      total/cap ratio.

      **Bug 9 — "New Window" corrupts all open windows.** With one data document open, the
      user tried **File → New Window** (a default SwiftUI `WindowGroup` command never
      suppressed) and got a second window showing the same data — then closing *either* window
      left *all* remaining windows blank. Root cause: `ViewerModel.shared` is a per-process
      singleton, and `MetalRendererView.makeNSView` always returns `model.renderer` — so every
      window created this way hosts the exact same `WireframeRenderer`/`MTKView` instance.
      AppKit cannot attach one `NSView` to two windows at once; closing one window tears down
      the shared view out from under all the others. Since this feature is explicitly
      single-document (FR-019) with no multi-window model to actually support, fixed by
      suppressing the command entirely (`CommandGroup(replacing: .newItem) {}`) rather than
      trying to make multi-window "work" — there was never a design for it to work correctly.
      This also effectively settles T066 (single-document replacement) for the GUI: there is no
      "Open..." menu action at all (only launch-argument-based opening), so with "New Window"
      removed there is no code path left in the running app that could ever open a second
      document in the same process — FR-019 holds structurally, not just by observed behavior.

      Rebuilt (relink confirmed) after bugs 7-9; `ctest -L preview` 17/17 unaffected.

      **Confirmed by the user: the 500K-point fixture now opens in ~2 seconds (was ~9.5
      minutes — a ~285x speedup), comfortably under SC-006's 5-second bar, and "New Window" is
      confirmed gone from the File menu.** This closes out the macOS side of T057, T063, and
      T066 (single-document replacement, settled structurally per bug 9's notes) — every
      manually-checkable claim in `quickstart.md` steps 2-8 has now been walked and confirmed on
      macOS, across nine real bugs found and fixed along the way (disc-radius stride, the RPATH
      build issue, the stale-SPM-binary CMake issue, the camera-to-world convention, the
      reset-aspect bug, the O(N²) disc-expansion reserve, the non-monotonic decimation cap, and
      the New Window window-corruption bug — eight distinct fixes, not counting the Data-menu/
      keyboard check which found no bug). **Not yet done on macOS**: launching via a
      `bin/orender-wire` symlink specifically (requires an actual `cmake --install`, not just
      the build tree — low priority, since direct-path launches have been exhaustively tested
      and the symlink is just an indirection to the same binary) and T064's own explicit
      RIB-scene regression walkthrough (orbit/pan/zoom/reset re-confirmed unaffected by the
      `reset()` fix via the automated `test_preview_arcball`/`test_camera_export` suite, but not
      re-walked interactively since those fixes landed).
- [X] T054 [P] [US1] Added the equivalent GLSL 330 points pipeline (`POINT_VERT`/`POINT_FRAG`,
      `gl_PointSize` written in the vertex shader + `glEnable(GL_PROGRAM_POINT_SIZE)`) to
      `src/preview/orender-wire-linux/main.cpp`; triangles reuse the existing `SCENE_VERT`/
      `SCENE_FRAG` program for the same reason as T053 (discs pre-expanded into the triangle
      array). Added `glDisable(GL_CULL_FACE)` explicitly in `on_realize` (already GL's default,
      stated for clarity per the plan). Added a `LoadResult` struct so `load_scene_thread()` can
      report back which of `ribpreview_load()`/`ribdata_open()` it took (GTask's
      `g_task_return_pointer` carries one typed pointer); `on_load_done()` branches on
      `result->isData` to build either the existing RIB path or a new `upload_data_scene()`
      uploading `DataSceneC`'s lines/points/triangles into three new VAO/VBO sets. `AppState`
      retains the opened `RibDataDocument*` (same reasoning as T053 — `ribdata_key()` needs it
      alive for User Story 2), released in `on_close_request`. **⚠️ Compile-unverified**: this
      session runs on macOS, and `src/preview/CMakeLists.txt` only adds
      `add_subdirectory(orender-wire-linux)` under `elseif(UNIX)` — mutually exclusive with
      `if(APPLE)`, so this file is structurally never part of the build graph here regardless of
      whether GTK4/libadwaita/epoxy happen to be installed. The change was written with care
      against the existing GLSL/GTask/VAO patterns already in the file and cross-checked line by
      line, but **needs a real Linux build (CI or a Linux box) to confirm it actually compiles**
      before being trusted.

      **Update (user's first real Linux build, 2026-09-02/03), compile now confirmed, two
      infrastructure bugs found and fixed — neither is in the T053-T062 application code
      above, both are in `src/preview/orender-wire-linux/CMakeLists.txt`:**
      - The compile itself succeeded (`ctest --test-dir build -L preview` also passed
        unmodified, as expected — that suite never touches this GTK frontend at all). Running
        the built binary produced no window and no visible error.
      - **Root cause of the silent failure**: `CMakeLists.txt` set
        `BUILD_WITH_INSTALL_RPATH TRUE` on the executable target — a pre-existing
        misconfiguration, not introduced by spec 016, and confirmed to be the *only* place in
        the whole project setting this property. That forces the binary to be linked with
        `INSTALL_RPATH` (`$ORIGIN/../lib`, correct once installed under `libexec/` next to
        `lib/libri.so`) even in the *build tree*, where `$ORIGIN/../lib` resolves to a directory
        that doesn't exist (`libri.so` actually lives under `build/src/ri/`) — the dynamic
        linker fails before `main()` (and therefore before `daemon()`) ever runs, which is
        exactly what "starts, no error, no window" looks like when invoked in a context that
        doesn't surface the loader's stderr clearly. Fixed by removing
        `BUILD_WITH_INSTALL_RPATH` (left at its default `FALSE`), which lets CMake auto-compute
        a correct build-tree RPATH while still applying `INSTALL_RPATH` at actual
        `cmake --install` time — standard, default CMake behavior.
      - **Separately, renamed the CMake target** from `orender-wire` to `orender-wire-linux`
        (user request, for parity with `orender-wire-macos`) via `add_executable(orender-wire-linux ...)`
        plus `OUTPUT_NAME "orender-wire"`, so `cmake --build build --target orender-wire-linux`
        now works and the on-disk binary name (build tree and installed) is unchanged.
      - Also added an `ORENDER_WIRE_GUI` env var check (mirroring the macOS frontend's identical
        sentinel) that skips `daemon()` entirely — a debugging aid so a future crash/hang after
        the fork isn't invisible to the calling shell.
      - **Still outstanding**: the RPATH fix has not yet been re-tested by actually launching
        the app and seeing a window — that's the next step, now that the loader error is
        resolved.
- [X] T055 [US1] Applied the GL depth-range fix in `src/preview/orender-wire-linux/arcball.cpp`'s
      constructor: `remapClipZMetalToGL()` rewrites the loaded projection matrix's z row
      (`row_z_new = 2*row_z − row_w` for each column) once, baked into `ribProj_` so `reset()`
      (which just copies `ribProj_` back) keeps the fix automatically — `ribGeometryContext.cpp`
      builds `projMatrix16` in Metal-NDC convention (`clip.z ∈ [0,1]`) since the macOS renderer
      consumes it unmodified, and GL 3.3 core has no `glClipControl` to accept that convention
      directly. Also reconciled `updateAspect()`'s orthographic divergence: the previous
      perspective-only gate (`if (projMatrix_.at(2,3) == 1.0f)`) left orthographic scenes
      unrescaled (stretched) on window resize; removed the gate so the same `(0,0) = (1,1)/(w/h)`
      rescale applies unconditionally, matching `ArcballCamera.swift`'s `updateAspect`, which
      never had this gate and was already correct for both projection types (the ratio-rescale
      formula is identical for perspective and orthographic in a symmetric frustum). **Same
      compile-unverified caveat as T054** — this file is also `orender-wire-linux`-only and
      structurally excluded from this machine's build graph.
- [ ] T056 *(Retired during `/speckit-analyze`, 2026-09-01.)* This task implemented a
      "not available" notice for a hierarchical point-cloud/brick-map variant later shown to be
      unreachable through file-content detection (see spec.md's retired FR-010,
      contracts/c-abi.md's `RibDataType`, and research.md §2). No replacement task is needed:
      `RibDataType` has no corresponding enumerator, so every document `ribdata_open` returns is
      always visualizable. This ID is intentionally left retired rather than reused.
- [ ] T057 [US1] **macOS half fully confirmed (2026-09-08/09) — see T053's notes for the full
      account, including the four real bugs found and fixed to get there. Linux half still
      blocked**, pending resolution of the Linux VM's Mesa/LLVM crash (T063's notes) or a
      different Linux machine. Manual validation: `quickstart.md` steps 2–3 (headless CLI per
      document type, then GUI open per document type) on both platforms.

**Checkpoint**: User Story 1 is fully functional and independently testable — every data type
opens and renders on both platforms. **Not yet confirmed**: T054/T055 (Linux) are
compile-unverified, and T057's visual/manual validation on both platforms is outstanding —
this checkpoint's automated portion (T050/T051, the full preview ctest suite, and the macOS
crash-free smoke test) is green, but the checkpoint's own stated goal ("every data type opens
and renders on both platforms") cannot be fully confirmed without a person or CI at a display
on each platform.

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

- [X] T058 [US2] Added a `CommandMenu("Data")` to
      `src/preview/orender-wire-macos/Sources/OrenderWireApp.swift`: Previous/Next Channel
      (`q`/`w`), Increase/Decrease Detail Level (`m`/`l`), Draw as Boxes/Discs/Points (`b`/`d`/`p`),
      each a `Button` with a bare `.keyboardShortcut` (no modifiers) calling
      `model.sendDataKey(_:)`. `.disabled(...)` is driven by new `ViewerModel` computed
      properties (`hasChannels`, `supportsDetailLevel`, `supportsBoxDrawMode`,
      `supportsDrawModeToggle`) so brick-map-only controls (detail level, box mode) and
      channel controls (FR-017: only meaningful for the two document types with channels) are
      inert everywhere else. `d`/`p` are genuinely shared between brick maps (3 draw modes) and
      point clouds (2 draw modes) with no real conflict, since only one document is ever open.
      `ViewerModel.sendDataKey(_:)` forwards to a new `WireframeRenderer.sendDataKey(_:)`, which
      calls `ribdata_key()`, rebuilds the data GPU buffers via the existing `buildDataBuffers()`
      on change, and triggers a redraw; `WireframeRenderer` caches the raw `DataSceneC` by value
      (`lastDataSnapshot`) after every rebuild, since the pointer `ribdata_snapshot()` returns is
      documented as invalidated by the next snapshot/key call. No conflict with macOS's Cmd+Q
      (system-provided, unrelated to bare `q`).
- [X] T059 [US2] Added a semi-transparent status overlay (bottom-left, monospaced) to
      `DocumentView.swift`, driven by `ViewerModel.dataStatusText` — a computed property built
      from the same `@Published` mirror of `lastDataSnapshot` used by T058's menu, e.g.
      `"Channel: _radiosity   •   Detail: 2   •   Draw: Discs"`. `nil` (overlay hidden entirely)
      for a RIB document or before a data document finishes loading. `drawModeDisplayName`
      mirrors `wireCli.cpp`'s `drawModeName()` (boxes/discs/points for brick map, discs/points
      for point cloud, "Fixed" otherwise). Verified: full `cmake --build build` succeeds; the
      same point-cloud-fixture smoke test as T053 (launch via `ORENDER_WIRE_GUI=1`, 3-second
      alive check) re-run after these changes, still zero crash/stderr. Rendered appearance
      still unverified (no display in this environment) — deferred to T063/`quickstart.md`.
- [X] T060 [P] [US2] Populated the previously-empty `AdwHeaderBar` in
      `src/preview/orender-wire-linux/main.cpp`: a `GtkMenuButton` (`open-menu-symbolic`) bound
      to a `GMenuModel` (four sections: channel, detail, draw mode, save camera), each item
      invoking a `GSimpleAction` added to the window's own action group (`win.<name>`). Every
      action forwards to the exact same shared functions `on_key` already calls —
      `apply_data_key(state, <letter>)` (new, factored out of what used to be inlined per-key
      logic) and a new `trigger_save_camera(state)` (factored out of `on_key`'s old inlined 's'
      case) — "one action implementation, two entry points" as specified. Added an
      `AdwWindowTitle` with a subtitle refreshed by a new `update_header_bar_state()` (mirrors
      `draw_mode_display_name()` from `wireCli.cpp`'s `drawModeName()`), called once after
      `on_load_done()` and again after every `apply_data_key()` state change.
      **Real key-binding collision found and resolved, not just a straightforward
      port**: the legacy oshow convention needs bare `q`/`Q` to mean "previous channel" for a
      document that has channels, but this app's own pre-existing `on_key` already binds bare
      `q`/`Q` to **quit** — a conflict that didn't exist on macOS (quit there is Cmd+Q, a
      completely different key combination from any bare-letter data-document shortcut).
      Resolved by routing `q`/`Q` at runtime: if the open document currently has channels
      (`ribdata_snapshot(...)->numChannels > 0`), it means channel-previous; otherwise it falls
      through to the existing quit behavior. `Escape` remains an unconditional quit either way,
      so quitting is never actually blocked. **⚠️ Compile-unverified** — same reasoning as
      T054/T055 (this machine's CMake config structurally excludes `orender-wire-linux` under
      `if(APPLE)`/`elseif(UNIX)`); written and cross-checked carefully against the existing
      GTK4/GMenu/GAction patterns in the file, but needs a real Linux build to confirm.
- [X] T061 [P] [US2] Document-type-conditional enabling, in the same
      `update_header_bar_state()` added for T060: channel actions enabled only when
      `numChannels > 0`; detail-level and "draw as boxes" actions enabled only for a brick map;
      "draw as discs/points" enabled for brick map or point cloud; **Save Camera disabled
      whenever a data document is open** (`!hasData`), matching the plan's stated scope — the
      keyboard `s` shortcut itself is intentionally left unconditional (matches macOS, where
      `WireframeRenderer.keyDown`'s `s` case is likewise not document-type-gated — the
      conditional behavior applies to the *discoverable menu control*, not the raw key). No
      `AdwViewStack` used, per the plan — one `GtkGLArea`, one set of pipelines, for both
      document types. Same compile-unverified caveat as T060.
- [X] T062 [US2] Confirmed structurally on both platforms: the channel actions'/menu-items'
      enabled state is driven directly by `numChannels > 0` (macOS: `ViewerModel.hasChannels`;
      Linux: `update_header_bar_state()`'s `hasChannels` local) — since only point clouds and
      brick maps ever report `numChannels > 0` (confirmed against `dataSink.cpp`'s
      `buildDataScene()`, which reads `view->numChannels()`, itself only overridden by
      `CPointCloud`/`CBrickMap` per `dataView.h`'s default-0 base), a photon map, irradiance/
      gather cache, or debug-geometry dump always disables/hides the channel control per FR-017.
      No document type can reach an "offered but silently does nothing" state, since the same
      `numChannels` value gates both the enabled-state check here and `ribdata_key()`'s own
      `q`/`w` handling underneath.
- [ ] T063 [US2] **macOS half fully confirmed (2026-09-09) — every Data-menu item and its
      keyboard equivalent correctly updates both the visualization and status text, on point
      cloud and brick map fixtures, including after a click-in-view and app-switch (the
      first-responder regression check). Linux half still blocked** on the Mesa/LLVM crash (see
      below). Manual validation: `quickstart.md` steps 4–5 (menu-only path, then keyboard-only
      path) on both platforms.

      **Update (user's Linux VM, 2026-09-02/03): T054/T055/T060/T061's "compile-unverified"
      status is resolved (confirmed compiling and linking on a real Fedora/Wayland box after the
      CMake target/RPATH fixes below), but T063 itself is now blocked on something new — the
      user's Linux test VM (a Proxmox VM with no hardware GPU, `llvmpipe`-only by design and
      previously working fine) currently cannot create ANY GL or Vulkan context at all,
      confirmed three independent ways, all via `gdb bt full` on the resulting core dumps:**
      1. Default (`GSK_RENDERER` forced to `"gl"` by `main.cpp`, pre-existing/unrelated to spec
         016): crashes in `llvmpipe_init_sampler_matrix` → `compile_jit_size_function` →
         `gallivm_compile_module` → `lp_build_create_jit_compiler_for_module`, reached via
         `gtk_window_realize`'s own GSK compositor context creation. Our own code
         (`on_activate`) is the last application frame; everything below it is
         GTK/EGL/Mesa/LLVM.
      2. `GSK_RENDERER=cairo` (rules out the window compositor's own GL path): window now maps
         successfully, but `GtkGLArea`'s *own* context creation (`on_realize` in `main.cpp`,
         via `gtk_gl_area_real_create_context` → `gdk_surface_create_gl_context`) hits the
         identical `llvmpipe_init_sampler_matrix` → JIT-compiler crash. Confirms the bug is in
         `llvmpipe` itself, not which caller asks for a GL context.
      3. `GALLIUM_DRIVER=softpipe` (rules out `llvmpipe` specifically): GL context creation now
         fails cleanly with warnings instead of crashing ("Could not initialize EGL display"),
         but GTK's renderer fallback chain then tries **Vulkan**, whose only available driver on
         this VM is **Lavapipe** (`libvulkan_lvp.so`, Mesa's software Vulkan implementation) —
         which internally calls into the *same* Gallium/`llvmpipe` JIT compiler
         (`lvp_CreateDevice` → `lvp_queue_init` → `llvmpipe_create_context` →
         `llvmpipe_init_sampler_matrix` → identical crash signature).

      **Conclusion**: every software-rendering path this Mesa build offers (GL via `llvmpipe`,
      Vulkan via Lavapipe) funnels through the same broken `lp_build_create_jit_compiler_for_module`
      call — there is no remaining application-level renderer choice left to try. All three
      traces terminate application-side at `main()`/`on_activate()`; `orender-wire`'s own code
      (old or new) is never implicated in any of the three. This is a Mesa/LLVM system
      regression on this specific VM (very recent version numbers observed —
      Mesa 26.1.7→26.1.8 and LLVM 22.1.8, ticking up mid-session, consistent with a
      fast-moving/rawhide-adjacent install), not a spec-016 defect. T063 stays blocked until
      either this VM's Mesa/LLVM packages are downgraded/fixed, or a different Linux machine
      with a working Mesa install is used.

      **Two infrastructure fixes made along the way (in `src/preview/orender-wire-linux/
      CMakeLists.txt` and `main.cpp`), both real and durable regardless of the above**:
      - `CMakeLists.txt` had `BUILD_WITH_INSTALL_RPATH TRUE` — the only place in the whole
        project setting that property — which bakes the install-time RPATH
        (`$ORIGIN/../lib`) into the *build-tree* binary too, where it resolves to a directory
        that doesn't exist. Every fresh build-tree run failed at the dynamic-linker stage
        before `main()` ever executed ("error while loading shared libraries: libri.so.1").
        Fixed by removing the property (default `FALSE`), which lets CMake auto-compute a
        build-tree-correct RPATH while still honoring `INSTALL_RPATH` at actual
        `cmake --install` time.
      - Renamed the CMake target from `orender-wire` to `orender-wire-linux` (parity with
        `orender-wire-macos`) via `add_executable(orender-wire-linux ...)` + `OUTPUT_NAME
        "orender-wire"`, so `cmake --build build --target orender-wire-linux` works and the
        on-disk binary name is unchanged in both the build tree and once installed.
      - Added an `ORENDER_WIRE_GUI` env var check in `main()` (mirroring the macOS frontend's
        identical sentinel) that skips the `daemon()` background-detach call — without it, a
        post-fork crash is invisible to the calling shell (it detaches into a session the shell
        isn't waiting on), which is exactly what made the original RPATH bug look like "starts
        silently, no error, no window" instead of a visible loader error.

**Checkpoint**: User Stories 1 and 2 both work independently; every interactive control from the
legacy tool is now discoverable and terminal-free. **Not yet confirmed**: T063's manual
visual/interactive validation on both platforms is outstanding — on macOS via the crash-free
smoke test, on Linux blocked on the test VM's broken Mesa/LLVM software-rendering stack (see
T063's notes above). The Linux *code* itself has now survived three independent crash-trace
inspections with zero findings pointing at `orender-wire`'s own code, which is meaningfully
stronger evidence than the earlier "compile-unverified" status, even without an actual rendered
window yet.

---

## Phase 5: User Story 4 - Continue viewing scene files exactly as before (Priority: P2)

**Goal**: Existing RIB-scene viewing shows zero observable disruption from this feature.

**Independent Test**: Open a RIB scene file; confirm the window, camera controls (orbit, pan,
zoom, reset, save), and visual output are unchanged from before this feature. This story has no
dependency on User Stories 1–3 and can be validated as soon as Phase 2A (T010) lands.

### Implementation for User Story 4

- [X] T064 [US4] **macOS confirmed (2026-09-09)**: opened `camera-dof.rib`, exercised orbit
      (left-drag), pan (middle-drag/two-finger), zoom (scroll/pinch), reset (`R`, specifically
      re-checked post-T057-fixes to confirm the `reset()` aspect-correction change doesn't
      regress RIB scenes — it doesn't), and camera export (`S`, native save dialog, wrote a new
      `.rib` successfully) — all behaviorally identical to before this feature. This closes out
      Phase 5's macOS half. **Linux still open**, see T065.
- [ ] T065 [P] [US4] Same regression pass on Linux — blocked on the Mesa/LLVM crash (T063's
      notes).
- [ ] T066 [US4] Confirm FR-019 in both directions on both platforms: opening a new file fully
      replaces whatever was previously open, for every combination (RIB→RIB, RIB→data,
      data→RIB, data→data). **macOS: settled structurally** — with "New Window" removed (T057's
      bug 9) and no "Open..." menu action existing at all, there is no code path in the running
      app that can open a second document in the same process; the only way to open a different
      file is a fresh process invocation, which is a separate `ViewerModel`/window entirely, not
      "replacement" in the sense this requirement means. **Linux: still open**, blocked on Mesa.

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

- [X] T067 [US3] Wired `wireCliRun()` into `src/preview/orender-wire-macos/Sources/main.swift`,
      replacing that file's own ad-hoc `--help`/`--version`/`argc`/`fileExists` checks (which
      never supported `--json` or `--type` at all) with a single delegated call, positioned
      before the `ORENDER_WIRE_GUI` re-exec check as required. **`wireCliRun`/`WireCliAction`/
      `WIRE_CLI_EXIT`/`WIRE_CLI_OPEN` lived in a separate header (`src/preview/libribpreview/
      wireCli.h`) not part of the Swift-visible `CRibPreview` module** (which only wraps
      `ribpreview_api.h`) — rather than add a second staged-header mechanism, folded
      `wireCli.h`'s declarations directly into `ribpreview_api.h` (already established as "the
      single source of truth for the whole C ABI" per `CRibPreview.h`'s own comment) and deleted
      `wireCli.h`; updated its only two other includers (`wireCli.cpp`, `test_wire_cli.cpp`).
      **Verified against the real built binary, not just ctest**: `--help` prints usage and
      exits 0; `--version` now prints `orender-wire 1.0.0` (via `openrender_version_string()`,
      previously hardcoded `"1.0.0"`-less `"1.0"` — a minor, intentional accuracy improvement,
      not a regression); `--json` on a real RIB scene with `ORENDERHOME`/`SHADERS`/`DISPLAYS`
      all unset in the shell produces well-formed JSON on stdout and exits 0 (no window opened);
      no-args exits 1; a nonexistent file with `--json` exits 2; the original GUI-open path
      (no flags) still opens and stays alive with zero crash/stderr, confirming the refactor
      didn't regress User Story 1/2/4's existing behavior. Full `ctest -L preview`: 17/17.
- [X] T068 [P] [US3] Wired the same `wireCliRun()` entry point into
      `src/preview/orender-wire-linux/main.cpp`'s `main()`, replacing its own ad-hoc
      `--help`/`--version`/`argc`/`access()` checks (and the now-dead `HELP_TEXT` constant,
      removed) the same way as T067, positioned before the `daemon()` call so `--json` never
      daemonizes into a background process before printing anything. `outPath` (malloc'd by
      `wireCliRun`) is intentionally never freed, matching every other `AppState` member that
      lives until process exit. **Compile-unverified** (same structural reason as T054/T055/
      T060/T061 — this machine's CMake excludes `orender-wire-linux` under `if(APPLE)`), but
      lower-risk than those: this change mirrors T067's already-verified pattern exactly, and
      touches only argument handling, not GTK/GL/rendering code.
- [ ] T069 [US3] **Deferred at the user's request (2026-09-08)**: GTK/OpenGL checks on the
      Linux VM are on hold pending resolution of the Mesa/LLVM software-rendering crash recorded
      in T063's notes (a system bug on that VM, unrelated to this feature). `quickstart.md` step
      2's *headless* checks (no window server, no `ORENDERHOME`/`SHADERS`/`DISPLAYS`) don't
      actually need a working GL/Vulkan renderer at all — they only need `wireCliRun()`'s
      already-verified-on-macOS `--json`/`--help`/`--version` paths, which never touch GTK/GL —
      so this task could in principle be completed on the Linux VM independently of the
      rendering-crash blocker whenever convenient, but is left unchecked until actually run
      there.

**Checkpoint**: All four user stories are independently functional and validated. **Not yet
confirmed**: T069's Linux-side headless CLI validation (deferred per the user's request; not
blocked by the Mesa crash, just not yet run), and T063/T064/T065's interactive GUI validation on
both platforms (T063/T065 additionally blocked by the Linux VM's Mesa/LLVM issue).

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Documentation and final whole-suite verification, per Constitution Principle VII
and the plan's overall acceptance bar.

- [X] T070 [P] Rewrote `docs/site/content/tools/orender-wire.md` to cover the new
      data-document type: the six auto-detected content types, the full legacy key/menu control
      set (with the Linux `Q`-collision resolution explained), the `--json` headless-mode
      section with the full JSON schema (common envelope + RIB-specific + data-specific
      fields), `--type=auto|rib|data`, and the corrected exit-code table (see T071 — the old
      table was wrong).
- [X] T071 [P] Updated every Hugo page found describing `orender-wire`'s prior RIB-only scope
      (`grep -rl orender-wire docs/site/content/`, 4 files): `_index.md` and
      `manual/reference/source-at-a-first-glance.md` (one-line description updates, RIB-only →
      RIB + data-structure files); `manual/reference/installing-and-running.md` (added the
      pre-existing-but-previously-undocumented libadwaita dependency alongside GTK4 while that
      line was open, and corrected the `bin/` layout table's `orender-wire` description).
      **Found and fixed a real, more-than-cosmetic doc bug while rewriting the exit-code
      table**: the OLD `orender-wire.md` said exit code 4 meant "No display available (Linux)"
      — that was true under spec 006, but this feature reassigned exit code 4 to "data file
      rejected" and moved "no display" to the new exit code 5 (per
      `contracts/cli-interface.md`, matching `wireCli.cpp`'s actual implementation). Left
      uncorrected, the shipped docs would have told a reader the *wrong* exit code for two
      different real failure conditions. Also documented that exit code 3 is specified but not
      currently reachable in practice (matches the T041 finding).
- [ ] T072 **Blocked on the user/CI — same reasoning as T057/T063**: requires walking
      `quickstart.md` interactively on both platforms with a working display (and, for Linux, a
      working Mesa/GL install — see T063's notes on the current test VM's blocker). Not
      attempted from a headless check.
- [X] T073 Final full-suite gate, run together in this session: `ctest --test-dir build -L
      preview` — **17/17 passing**; `ctest --test-dir build -L visual` — **191/191 passing**
      (153s); the T032 removal grep
      (`grep -rn "CShow\|oshow\|BUILD_SHOW\|FLTK"` excluding `specs/`) returns hits only in
      `NEWS.md` (an already-sanctioned historical-record exemption from Phase 2C) and in new
      prose this feature itself wrote (`test_preview_subdiv.cpp`'s comment explaining a
      retargeted fixture's history, `orender-wire.md`'s explanation of what was removed and why
      the Linux `Q` collision exists, `main.cpp`'s comment on the same) — all explanatory
      mentions of something correctly and completely removed, not residual live references to
      it. Zero hits represent functional/build-system remnants.

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
