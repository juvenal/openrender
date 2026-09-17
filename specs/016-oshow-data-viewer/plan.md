# Implementation Plan: orender-wire Data-Structure Viewer (oshow Absorption)

**Branch**: `016-oshow-data-viewer` | **Date**: 2026-09-01 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/016-oshow-data-viewer/spec.md`

## Summary

`oshow` is dead: it `dlopen`s a `gui.dsply` module deleted in the same commit that introduced
`orender-wire`, so every invocation fails immediately (`show.cpp:141`, exit 255). This feature
retires `oshow`, `CShow`, and the FLTK build dependency they required entirely, and re-homes
`oshow`'s real capability — visualizing precomputed 3D data structures (photon maps,
irradiance/gather caches, point clouds, brick maps, debug-geometry dumps) as points, lines,
triangles, and oriented discs — into `orender-wire` as a second document type. The toolkit
boundary `oshow` used (`src/gui/opengl.h`'s `CView`) is replaced by a `CPrimitiveSink`/
`CDataView` pair with identical call syntax, so every existing `draw()` body in `src/ri/`
recompiles unchanged. A new headless loading path (`CDataDocument`, bypassing
`CRenderer::get*` which requires mid-render state) makes the entire data path — sniffing,
loading, primitive generation, interactive state — testable and scriptable via a new `--json`
CLI mode, before either platform's GUI code is touched. The macOS shell migrates from AppKit to
SwiftUI for a real menu bar (Metal renderer preserved); the Linux shell populates its currently
empty header-bar menu using its existing GTK4/libadwaita/OpenGL stack. Three confirmed bugs
found during design are fixed along the way (a disc-basis NaN, a duplicated debug-dump record,
a dropped last photon), and a GL depth-range defect already present in `orender-wire` becomes
worth fixing now that triangles and discs make it visible.

## Technical Context

**Language/Version**: C++20 (`libri` additions, `libribpreview` additions,
`orender-wire-linux`), Swift 6 (`orender-wire-macos`) — same versions spec 006 already
established for this codebase; no new language is introduced.

**Primary Dependencies**:
- macOS: Metal, MetalKit, AppKit, SwiftUI, simd — all system frameworks, no external deps
  (SwiftUI is already precedented in-tree via `orender-fb-macos`)
- Linux: OpenGL 3.3 Core, GTK 4 (≥4.20), libadwaita (≥1.4), epoxy — all already-approved,
  already-linked dependencies; none new
- `libri`/`libribpreview`: no new dependency; `libribpreview` continues to link only `ri` and
  `openrendercommon`, never the renderer runtime, shader engine, or display drivers
- **Net dependency change is negative**: FLTK (`fltk-config` probe, `FLTK::FLTK` target,
  `option(BUILD_SHOW)`) is removed entirely and nothing replaces it

**Storage**: N/A (reads existing openRender data-file formats directly; no new persistent
storage)

**Testing**: C++20 hand-rolled unit tests under `tests/preview/` (existing `add_preview_test`
macro, `LABELS "preview"`, `TIMEOUT 30`, no external test framework — matches spec 006's
established pattern), plus `ctest -L visual` scene registrations for the six retargeted RIB
fixtures; manual `quickstart.md` checklist for GUI-only behavior (menu firing, first-responder
keyboard behavior, disc orientation, z-fighting) on both platforms

**Target Platform**: macOS 12.0+ (unchanged floor), Linux X11/Wayland via GTK 4 — identical to
`orender-wire`'s existing targets; no platform is added or dropped

**Project Type**: Desktop application (CLI-invoked native GUI viewer) — extends the existing
`orender-wire` single-project layout; no new top-level project

**Performance Goals**: Headless `--json` statistics under 2 s on a typical file (SC-004);
opening a data file with on the order of a million primitives stays responsive and completes
under 5 s, via deterministic detail reduction (SC-006); existing scene-viewing performance
(spec 006: window open <1 s, ≥30 fps arcball navigation) is unaffected

**Constraints**:
- Exactly one document (scene or data) open at a time; no multi-window model in this feature
- No compatibility shim for the removed `oshow` binary or its (effectively nonexistent) CLI
- Visualization is unlit/flat-colored, matching prior behavior
- Detail reduction is a fixed, deterministic primitive cap with even-stride sampling, never
  adaptive (spec clarification)
- Keyboard shortcuts for data controls are plain, unmodified legacy keys, active only while a
  document they apply to is open
- `stdout` reserved for `--json` output and other explicit machine-readable modes; all
  interactive/diagnostic state must be visible in the UI, never only in a terminal (FR-016)
- No Qt, FLTK, SDL, GLFW, or other third-party GUI toolkit (unchanged from spec 006; this
  feature actively removes FLTK rather than adding a toolkit)

**Scale/Scope**: Single binary per platform, extended with one new document type and ~10 new
CLI/interaction surface points; six existing test RIB scenes retargeted; no new binaries

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|-----------|--------|-------|
| I. Clean Code | PASS | `CPrimitiveSink`/`CDataView` isolated in one new header/source pair; `dataLoad`, `dataScene`/`dataSink`, `diskExpand`, `wireCli` are each single-responsibility files mirroring the existing `tessellators/` convention |
| II. Language Standards | PASS | C++20 for all C++ additions; Swift 6 for the macOS shell (spec 006 precedent); every interop boundary (`ribpreview_api.h`, `CRibPreview.h`) is a flat C-linkage header |
| III. TDD (NON-NEGOTIABLE) | GATE | `tests/preview/` unit tests (sink chunking, debug-dump round-trip, disc expansion, headless load, world-matrix init, key handling, CLI grammar, subdivision tessellation) must be written and failing before the corresponding implementation lands. GUI phases (I0, I5, I6) have no new automated tests, matching the spec 006 precedent — see research.md §11 for why, and the manual `quickstart.md` checklist that covers them instead |
| IV. CLI Interface | GATE→PASS after this feature | Currently a violation: `orender-wire` has zero flags beyond a single positional argument. This feature closes the gap: `--help`, `--version`, `--json`, POSIX argument handling, errors to stderr, exit codes 0–5 |
| V. Minimal Dependencies | PASS | Net dependency count decreases (FLTK removed, nothing added). The Linux CMake `REQUIRED` probes are softened to warn-and-return — fixing an existing graceful-degradation violation, not introducing one |
| VI. Platform Targeting | PASS | Linux + macOS only, unchanged; all new platform-specific code stays isolated in `orender-wire-macos/` and `orender-wire-linux/`; `dataView`/`dataLoad`/`dataScene`/`dataSink`/`diskExpand`/`wireCli` are platform-neutral |
| VII. Documentation | GATE | A documentation page for the new document type and `--json` schema must be added under `docs/site/content/...` (the project's real tree; the constitution literally names `site/`, a known drift already present in spec 006 — see Complexity Tracking) before merge. Stale `oshow` references across 20+ files (README, INSTALL*, man pages, Hugo pages, AUTHORS.md, CLAUDE.md, DEVNOTES.md) must be corrected in the same removal commit |

**Gate actions**:
- TDD: the eight tests enumerated in `contracts/test-plan.md` must exist and fail before their
  corresponding source lands (I1/I3/I4 in Project Structure below)
- CLI: `contracts/cli-interface.md` is the acceptance contract for `--help`/`--version`/`--json`
  and exit codes 0–5
- Documentation: `docs/site/content/manual/reference/installing-and-running.md` and a new page
  documenting the data-document type and `--json` schema must be updated/added; every stale
  `oshow` reference removed in the same commit as the binary itself (FR-026/FR-027)

## Project Structure

### Documentation (this feature)

```text
specs/016-oshow-data-viewer/
├── plan.md                        ← this file
├── research.md                    ← Phase 0 output
├── data-model.md                  ← Phase 1 output
├── quickstart.md                  ← Phase 1 output
├── contracts/
│   ├── cli-interface.md           ← Phase 1 output: flags, exit codes, --json schema
│   ├── c-abi.md                   ← Phase 1 output: DataSceneC / ribdata_* contract
│   └── test-plan.md               ← Phase 1 output: the 8 tests, what each pins
├── checklists/
│   └── requirements.md            ← from /speckit-specify, re-validated by /speckit-clarify
└── tasks.md                       ← Phase 2 output (/speckit-tasks — not created here)
```

### Source Code (repository root)

```text
src/ri/                                       ← libri (unchanged public surface, new files only)
├── dataView.h / dataView.cpp                 ← NEW: CPrimitiveSink, CDataView (replaces CView)
│                                                also absorbs the debug-dump parser (NaN fix,
│                                                feof fix, quad→2-triangle split)
├── dataLoad.h / dataLoad.cpp                 ← NEW: dataSniff(), CDataDocument
├── debug.h / debug.cpp                       ← MODIFIED: CView → CDataView
├── texture3d.h                               ← MODIFIED: CView → CDataView
├── photonMap.h / photonMap.cpp               ← MODIFIED: CView → CDataView; off-by-one fix
├── pointCloud.cpp                            ← MODIFIED: printf → CDataView accessors
├── brickmap.h / brickmap.cpp                 ← MODIFIED: printf → CDataView accessors
├── irradiance.cpp, radiance.cpp              ← MODIFIED: CView → CDataView (no behavior change)
├── show.h / show.cpp                         ← DELETED (CShow hider)
└── renderer.cpp, rendererContext.cpp         ← MODIFIED: CShow #include/dispatch removed

src/gui/                                      ← DELETED (opengl.h only; never add_subdirectory'd)

src/oshow/                                    ← DELETED entirely

src/preview/
├── ribpreview_api.h                          ← MODIFIED: + PrimArrayC, DataSceneC, ribdata_*
├── libribpreview/
│   ├── dataScene.h                           ← NEW: DataScene (4 primitive arrays + disks)
│   ├── dataSink.h / dataSink.cpp             ← NEW: CDataSceneSink : CPrimitiveSink
│   ├── diskExpand.h / diskExpand.cpp         ← NEW: 20-segment CPU disc expansion
│   ├── wireCli.h / wireCli.cpp               ← NEW: shared arg parsing + --json, C linkage
│   └── tessellators/tessSubdivision.cpp      ← unchanged (now exercised by retargeted tests)
├── orender-wire-macos/
│   ├── Package.swift                         ← MODIFIED: CRibPreview header search paths
│   ├── CRibPreview/include/CRibPreview.h     ← MODIFIED: collapsed to 2 #includes
│   └── Sources/
│       ├── main.swift                        ← MODIFIED: append OrenderWireApp.main() only
│       ├── OrenderWireApp.swift              ← NEW: SwiftUI App/Scene/Commands (menu bar)
│       ├── DocumentView.swift                ← NEW: NSViewRepresentable wrapping WireframeRenderer
│       ├── ViewerModel.swift                 ← NEW: ObservableObject bridging to ribdata_*/ribpreview_*
│       ├── WireframeRenderer.swift           ← MODIFIED: +3 pipelines (points/triangles/discs)
│       ├── AppDelegate.swift                 ← DELETED
│       └── Shaders.metal                     ← DELETED (already dead/excluded)
└── orender-wire-linux/
    ├── CMakeLists.txt                        ← MODIFIED: REQUIRED → warn-and-return()
    ├── arcball.cpp                           ← MODIFIED: orthographic updateAspect reconciled
    └── main.cpp                              ← MODIFIED: +3 pipelines, GMenu header bar,
                                                 GL depth-range fix, document-type-conditional UI

tests/preview/
├── test_dataview_chunking.cpp                ← NEW
├── test_debugdump_roundtrip.cpp              ← NEW
├── test_data_disk_expand.cpp                 ← NEW
├── test_data_pointcloud.cpp                  ← NEW
├── test_data_world_init.cpp                  ← NEW
├── test_data_keys.cpp                        ← NEW
├── test_wire_cli.cpp                         ← NEW
└── test_preview_subdiv.cpp                   ← NEW

tests/visual/CMakeLists.txt                   ← MODIFIED: add_not_required_test macro + its 6
                                                 registrations removed; new add_wire_json_test
                                                 macro + 6 retargeted registrations added

examples/rib/tests/{parity/,}*-oshow.rib      ← RENAMED to *-wire.rib, Hider line dropped (6 files)

CMakeLists.txt                                ← MODIFIED: BUILD_SHOW option, fltk-config probe,
                                                 codesign exe-list entry all removed
src/CMakeLists.txt                            ← MODIFIED: add_subdirectory(oshow) removed
src/ri/CMakeLists.txt                         ← MODIFIED: show.cpp entry removed, dataView.cpp added
.github/workflows/release.yml                 ← MODIFIED: 5 FLTK install steps removed

docs/site/content/...                         ← MODIFIED/NEW: data-document-type page,
                                                 stale oshow references corrected
README.md, INSTALL.md, INSTALL_ARTIFACTS.md,
COMPILING.txt, HOMEBREW_GUIDE.md,
openrender.rb.template, openrender.spec,
AUTHORS.md, CLAUDE.md, DEVNOTES.md,
man/{orender,oshader,otexmake,rsloinfo}.1     ← MODIFIED: oshow references removed/corrected
```

**Structure Decision**: Extends the existing single-project layout — no new top-level
directory. All new logic is either platform-neutral (`src/ri/dataView.*`, `dataLoad.*`,
`src/preview/libribpreview/*`) or lives inside the existing per-platform `orender-wire-{macos,
linux}/` directories, mirroring exactly the split spec 006 already established. `src/gui/` and
`src/oshow/` are removed rather than migrated, since nothing in them survives in usable form
(the one surviving file, `src/gui/opengl.h`, is replaced in place by `src/ri/dataView.h`).

## Complexity Tracking

> **Fill ONLY if Constitution Check has violations that must be justified**

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|---------------------------------------|
| Opaque-handle C ABI (`ribdata_open`/`snapshot`/`key`/`close`) alongside the existing value-pair `ribpreview_load`/`free` — two API shapes in one header | The data-document interaction cycle (`keyDown` → re-emit) needs the underlying `CDataView` to survive across frames; the RIB path has no such cycle | A single unified handle-based API for both document types was considered, but would churn the RIB path's simpler, already-shipping value-pair API for a capability it doesn't need — see research.md §4 |
| Constitution VII references a `site/` folder; this plan (like spec 006 before it) targets the real tree, `docs/site/` | The literal `site/` path named in the constitution does not exist in this repository; `docs/site/` is what Hugo actually builds from, and is what CI already deploys | Renaming the tree to match the constitution is out of scope for this feature; this is pre-existing drift inherited from spec 006, not introduced here |

## Post-Design Constitution Re-check

Re-evaluated after Phase 1 (`data-model.md`, `contracts/`, `quickstart.md`) produced no new
gates or violations beyond the two already tracked above:

- **III. TDD** — `contracts/test-plan.md` fixes the exact eight tests and what each pins;
  every one is written against a fixture strategy requiring no render (research.md §3 closed
  the one open fixture question from pre-design). Gate remains open only in the procedural
  sense that tasks must actually be sequenced tests-first — the design itself introduces no new
  untestable surface.
- **IV. CLI** — `contracts/cli-interface.md` is now a concrete, reviewable contract rather than
  a stated intent; the gate converts to PASS once implemented as specified.
- **V. Minimal Dependencies** — the C ABI contract (`contracts/c-abi.md`) confirms no new
  third-party dependency is introduced by the opaque-handle addition; it is pure C++/C linkage
  on top of already-approved system frameworks and toolkits.
- **VII. Documentation** — `contracts/cli-interface.md`'s `--json` schema and
  `data-model.md`'s document-type enumeration are exactly what the new documentation page
  (Project Structure, `docs/site/content/...`) needs to describe; no additional design work is
  required to write it.

No new Complexity Tracking entries were added during Phase 1.
