# Implementation Plan: Camera and Motion Layer

**Branch**: `028-camera-and-motion-layer` | **Date**: 2026-10-27 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/028-camera-and-motion-layer/spec.md`, with the
2026-10-27 clarify-stage answers and the maintainer's five plan-stage decisions (recorded in the
spec's Clarifications, session "maintainer decisions for the plan stage").

## Summary

Motion blur and depth of field stop being implemented more than once on the raytrace side
(DB-009, raytrace half; Principle VIII rule 3):

1. **`FrameParams`** (Graphics State): an immutable per-frame copy of the shutter, raster, lens and
   camera-motion values the renderer core computes today, embedding spec 039's `CSamplingConfig` by
   value. The core installs it for the frame; Geometry reads it through `frameParams()`.
2. **`ICamera`** (Graphics State): `generateRay(x, y, t, lensUV)` returns the primary ray and its
   differentials. `CFrameCamera` reproduces `CRaytracer::computeSamples`' pinhole and thin-lens
   arithmetic expression by expression. It exposes the lens model for spec 031 and reserves
   `evaluateImportance`/`sampleConnection` for BDPT. The raytrace hider receives it by constructor
   injection and reads no camera static.
3. **The motion evaluator** (Geometry, `motion.h`): one template definition of the two-key model
   (`atTime`), per-site precision entry points that are instantiations of it, the one `dPdtime`
   scale (`toShutterRate`, the same reciprocal `invShutterTime`), the grid transforms at time, and the
   ray transform with its camera-rotation SLERP.
4. **Migration, one family per phase**: the camera, the ray transform (`CDLObject` and quadrics),
   quadrics, patches, polygons, the implicit surface, the tessellation patch, then the grid-only
   families. Each phase moves its family's intersection and `sample()` time handling, deletes its
   own allowlist lines first, ends with `ctest -L architecture` green and **every image of the full
   suite byte-identical** to the starting tree's manifest.
5. **Guardrails**: 20 allowlist lines deleted, none added (DB-004 65 -> 52, DB-011 173 -> 166 on
   macOS); metrics lowered; DB-009 stays Open with its evidence narrowed to Reyes (spec 031).

No output changes anywhere in 028: there is no output-changing phase (decision 3 leaves precision
unification to a later, separately approved phase).

## Technical Context

**Language/Version**: C++20 (`-Werror`), CMake >= 3.19.

**Primary Dependencies**: none new. `libRIBCommon` (`algebra.h`, `mathSpec.h`, `sampling.h`),
`libRIBRender`.

**Storage**: N/A; in-memory, per frame.

**Testing**: ctest.
- New label `camera` (State-only): `Camera_Analytic`, `Camera_StartingTreeEquivalence`,
  `Camera_CropInvariance`, `Camera_GenerateRayOnly`, `Camera_LensModel`, `Frame_DefaultValue`.
- New label `motion`: `Motion_TwoKeyModel`, `Motion_Isolation`, `Motion_CameraSlerpEquivalence`,
  `Motion_TransformRayEquivalence` (State-only), `Frame_ParamsEqualCore` and the eight
  `Motion_Replay_<family>` tests (link `ribRender`); `Motion_ReplayRecord` (label `motion-record`)
  writes a fresh replay into the build tree only.
- Existing: `architecture`, `visual`, `shading_parity`, `^determinism$`, `convergence`, the camera-slot
  and sampling tests, the full suite per phase, the determinism sweep once before the merge.

**Target Platform**: macOS (Apple Silicon, Apple Clang 21, vcpkg) and Linux (Ubuntu 24.04, GCC 13,
`-Werror`).

**Project Type**: renderer library and CLI.

**Domains touched**:

| Domain | What changes |
|---|---|
| Graphics State | new: `frameParams.{h,cpp}`, `camera.{h,cpp}` |
| Geometry | new: `motion.h`; edited: `objectMisc.h`, `quadrics.cpp`, `dlobject.cpp`, `patches.cpp`, `polygons.cpp`, `implicitSurface.cpp`, `surface.cpp` (`CTesselationPatch::intersect` moving branch only), `curves.cpp`, `points.cpp`, `patchgrid.cpp`, `bsplinePatchgrid.cpp`, `subdivision.cpp` (Q2) |
| Hiders | `raytracer.{h,cpp}`: constructor argument, `computeSamples` |
| Renderer core | new: `rendererFrame.{h,cpp}`; edited: `renderer.cpp` (two calls, one constructor argument), `src/ri/CMakeLists.txt` |
| Consumers | `tests/unit/motion/` (new), `tests/unit/camera/` (new), `tests/unit/state/CMakeLists.txt`, `tests/CMakeLists.txt` |

**Not edited**: Shading (`shading.{h,cpp}`, `rendererServices.h`), Texturing (`src/ri/texture`),
`rendererServicesImpl.h`, `renderer.h`, `rendererStatics.cpp`, `rendererContext.cpp`, Scene
Interface, Display Outputs, the Procedures ABI headers (`implicit.h`, `dlo.h`), every Reyes and
zbuffer hider file, `sampler.{h,cpp}`, `object.cpp`, `csgDefinition.cpp`, and `surface.cpp` outside
`CTesselationPatch::intersect` (quickstart §3 checks it).

**Performance Goals**: no measurable change (SC-008). One virtual `generateRay` per camera sample;
the evaluator is inline; `frameParams()` is one pointer load, hoisted out of vertex loops.

**Constraints**:
- byte-identical output in **every** phase, on the full suite's 761 images (not only the camera,
  motion and DOF scenes: every raytrace ray goes through the camera from phase 4, and the quadrics'
  `sample()` serves static quadrics too);
- per-platform manifests and replay files, no cross-platform comparison (decision 1);
- no threshold widening, no reference re-render, no replay re-recording;
- no task depends on the merge; `cpujob` for every build and test, at most `-j4`.

**Scale/Scope**: 7 new source files (`frameParams.{h,cpp}`, `camera.{h,cpp}`, `motion.h`,
`rendererFrame.{h,cpp}`), about 15 edited source files, about 12 new test files plus 2 fixture
plug-ins and 2 replay data files; the allowlist, the metrics baseline and the registry.

**Measurements** (macOS, `build-028`, master `4eb683b`, 2026-10-27; [validation.md](validation.md) §0):
- **Architecture**: PASS, `ALLOWED DB-004 x 65`, `DB-011 x 173`; metrics Geometry 78/19, Hiders
  567/84, TOTAL 721/110, all equal to the baseline file.
- **Full suite** (`-LE determinism-sweep -j4`): 838/838 passed (one test skips itself), twice, 281 s
  and 242 s.
- **Manifest**: 761 images; 759 identical run to run, 2 shading_parity scratch files listed as
  noise; all 389 `MaxBlockAvgDiff` values identical run to run.
- **SC-008 timing**: `Convergence_dof-raytrace` 7.20-7.94 s (median 7.38) and
  `Visual_blobby-motion-grow-raytrace` 2.92-3.19 s (3.11) are stable; `Visual_teapot-motion-raytrace`
  is not (two series of five: median 20.29 s, then 25.00 s, range 19.98-31.11 s, machine load and
  thermals). SC-008 is therefore measured interleaved against a starting-tree build at phases 4 and
  11 (validation.md §0.5).
- **Compiler**: Apple Clang 21, `-O3`, default `-ffp-contract=on` (research R9).
- **Linux**: not measured; quickstart §4a, by the maintainer.

## Constitution Check

*GATE: must pass before Phase 0 research; re-checked after Phase 1 design.*

| Principle | Status | Notes |
|---|---|---|
| I. Clean code | PASS | Three copies of the camera-ray and motion logic become one camera, one evaluator and one SLERP primitive. Dead helpers (`interpolate`, `interpolate3`) are deleted. |
| II. C++20 / C17 | PASS | A function template, `enum class`, `final`, `override`. No extensions. |
| III. TDD (non-negotiable) | PASS (by plan order) | The camera and evaluator tests are written in phase 2 and are red by build failure on the starting tree. The replay tests are characterization tests, recorded on the starting tree and shown red by mutation (Q3). |
| IV. CLI | N/A | No CLI change. |
| V. Minimal dependencies | PASS | None added. |
| VI. Linux and macOS | PASS | Both platforms record their own starting-tree evidence and are checked against it (quickstart §4). |
| VII. Docs | PASS | Registry, allowlist header, metrics, DEVNOTES, HIDER_PARITY (phase 12). `CLAUDE.md` only with the maintainer's explicit authorization, never its "Spec-kit agent rules" (Q7). |
| VIII. Domain separation | PASS | Below. |

**Principle VIII — Domain Separation and Modular Boundaries** (every plan):

- [x] **Domains touched are named**: Graphics State, Geometry, Hiders, the renderer core,
      Consumers (tests). Shading, Texturing, Scene Interface, Display Outputs and the Procedures
      ABI are read, not edited.
- [x] **No new cross-domain dependency outside the table, none on the core from a domain**:
  - **Added**: Geometry -> Graphics State (`motion.h` includes `frameParams.h` and `xform.h`),
    Hiders -> Graphics State (`raytracer.h` includes `camera.h`), Core -> Graphics State and Core ->
    Hiders (`rendererFrame.cpp`). The new State headers include only Infra (`common/`). All allowed.
  - **Removed**: 7 Hiders -> Core link edges, 13 Geometry -> Core link edges.
  - **The slot** is a State-owned registration interface filled by the composition root (rule 1;
    rule 2's inversion), as `shaderParameterHooks.h` (spec 025) and `setDefaultServices()` (spec
    024). The `frameParams()` slot is a State-owned registration hook that the renderer core
    writes only between frames (installed in `beginFrame` before any render thread exists,
    uninstalled in `endFrame` after the last one is gone) and every other domain only reads.
- [x] **Interface changes specified**: [contracts/frame-params.md](contracts/frame-params.md),
      [contracts/camera.md](contracts/camera.md),
      [contracts/motion-evaluator.md](contracts/motion-evaluator.md); allowlist and registry in
      [contracts/allowlist-end-state.md](contracts/allowlist-end-state.md).
- [x] **No hider-specific special case**: the evaluator and the camera name no hider. The raytrace
      hider is the first consumer; Reyes adopts the same interfaces unchanged in spec 031.
- [x] **Plug-in interfaces**: none changed. `implicit.h` and `dlo.h` are untouched (DB-003 and DB-014
      untouched). `ICamera` is internal C++, not a plug-in boundary.
- [x] **Registered violations not extended; reductions noted**: DB-004 65 -> 52, DB-011 173 -> 166
      (macOS), DB-009 narrowed and kept Open. DB-006, DB-017 (no new dispatch pair:
      `rendererFrame.cpp` is a plain core unit, and ribVector excludes it as it excludes
      `renderer.cpp`) and DB-023 are not extended. The State-only test library compiles the new
      State units a second time, behind an option (research R16), as spec 025 did.

**Post-design re-check (after Phase 1)**: PASS, unchanged. The contracts add no edge beyond those
listed.

## Project Structure

### Documentation (this feature)

```text
specs/028-camera-and-motion-layer/
├── spec.md, plan.md, research.md, data-model.md, quickstart.md, validation.md
├── contracts/frame-params.md, camera.md, motion-evaluator.md, replay-format.md, allowlist-end-state.md
├── tools/image_manifest.sh                 # copied from spec 025 (decision 4)
├── tools/baseline-images-macos.sha256, baseline-diffs-macos.txt, baseline-image-noise-macos.txt,
│         archcheck-base-macos.txt          # measured at the plan stage
├── tools/*-linux.*                         # recorded by the maintainer's Linux baseline (quickstart §4a)
├── checklists/requirements.md
└── tasks.md                                # /speckit-tasks (not created here)
```

### Source code touched

```text
src/ri/state/
├── frameParams.{h,cpp}        # NEW: FrameParams, FrameCameraMotion, installFrameParams, frameParams()
└── camera.{h,cpp}             # NEW: ICamera, CameraSample, CameraRay, LensModel, CameraSupport, CFrameCamera
src/ri/geometry/
├── motion.h                   # NEW: atTime + entry points, sampleKey, shutterRate/toShutterRate,
│                              #      *transformAtTime, cameraRotationAt, transformRay, pluginTime
├── objectMisc.h               # - ray transform (phase 5), - timed grid transforms (phase 6),
│                              #   - interpolate/interpolate3 (phase 11, no caller)
├── dlobject.cpp               # phase 5
├── quadrics.cpp               # phases 5 (call sites) and 6
├── patches.cpp                # phase 7
├── polygons.cpp               # phase 8
├── implicitSurface.cpp        # phase 9
├── surface.cpp                # phase 10: CTesselationPatch::intersect, moving branch only
└── curves.cpp, points.cpp, patchgrid.cpp, bsplinePatchgrid.cpp, subdivision.cpp (Q2)   # phase 11
src/ri/hiders/raytracer.{h,cpp}   # phase 4: CRaytracer(int, const ICamera &), computeSamples
src/ri/render/
├── rendererFrame.{h,cpp}      # NEW (phase 3): beginFrameParams(), endFrameParams(), frameCamera()
└── renderer.cpp               # phase 3: the two calls; phase 4: new CRaytracer(i, frameCamera())
src/ri/CMakeLists.txt          # + state/frameParams.cpp, state/camera.cpp, render/rendererFrame.cpp in
│                              #   ri_sources; render/rendererFrame.cpp removed from ribVector_sources
tests/unit/camera/             # NEW: Camera_* and Frame_DefaultValue (State-only)
tests/unit/motion/             # NEW: Motion_* (State-only), Frame_ParamsEqualCore, motion_replay.cpp,
│                              #   fixtures/implicit_moving_sphere.cpp, fixtures/dlo_sphere.cpp (MODULE),
│                              #   replay/macos.txt, replay/linux.txt
tests/unit/state/CMakeLists.txt   # the State-only library gains frameParams.cpp, camera.cpp behind
│                                 #   OPENRENDER_CAMERA_MOTION_ONLY_TESTS (research R16)
tests/CMakeLists.txt           # + add_subdirectory(unit/camera), add_subdirectory(unit/motion), after unit/state
tests/architecture/allowlist.txt, metrics-baseline.txt
DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md, DEVNOTES_DETAILS/HIDER_PARITY.md, DEVNOTES.md
```

**Structure decision**: the existing layout. Every new source sits in a directory
`domain-map.txt` already classifies (`src/ri/state/`, `src/ri/geometry/`, `src/ri/render/`), so the
domain map does not change. Tests are under `tests/`, which `archcheck` does not analyse as a domain.

## Design

Details in [research.md](research.md); interfaces in [contracts/](contracts/). In brief:

1. **Placement (R1)**: `FrameParams` and the camera in Graphics State, the evaluator in Geometry,
   the builder in the core. State is the only domain both Geometry and Hiders may reach, and D30's
   Transport domain can reach it too.
2. **`FrameParams` and `CSamplingConfig` (R2)**: two structures; `FrameParams` embeds the sampling
   config by value and repeats none of its fields.
3. **Delivery (R3)**: Geometry reads the State-owned slot the core fills per frame; the raytrace
   hider receives the camera by constructor injection.
4. **Values (R4)**: copied from the `CRenderer` statics after `beginFrame` computes them, so every
   value is today's, bit for bit, including the reciprocal `invShutterTime` (decision 5). The
   statics stay for Reyes, `addObject` and Shading's service.
5. **The camera (R5)**: pinhole or thin lens, perspective or orthographic, the same expressions in
   the same shapes; crop invariance by construction; §7.4's `sampleRay` is a non-virtual forwarder;
   the BDPT operations are reserved with incomplete result types.
6. **The evaluator (R6)**: an expression-level primitive, because the families store keys four ways
   and the polygons fuse bilinear and temporal interpolation in one double expression. Float and
   double entry points share one template (decision 3).
7. **Bit-identity (R9)**: each site's expression shape is kept, so Clang's per-expression FMA
   contraction makes the same choice; per-platform evidence covers GCC's different default.
8. **Camera motion (R10)**: `addObject`'s synthesis stays in the core, unchanged (FR-020); the
   evaluator composes only what it synthesized (#106 preserved).
9. **Implicit surface (R11)**: no keys; its phase routes the plug-in time through `pluginTime()` and
   gives it its first test.

## Phases and order of work

Every phase from 3 on ends with the checkpoint of quickstart §2: **CP-a** clean build, **CP-b** full
suite green, **CP-c** architecture PASS with exactly the cumulative deletions, **CP-d** every image
and every `MaxBlockAvgDiff` equal to the starting tree's (noise list excepted), plus `ctest -L
'^motion$|^camera$'`. Each phase pauses for the maintainer.

| # | Phase | Content | Allowlist lines deleted first | Tests that turn green / guard |
|---|---|---|---|---|
| 1 | Setup and baselines | confirm §0 (already measured), write `motion_replay` and its two fixture plug-ins using only starting-tree interfaces, record `replay/macos.txt` through `Motion_ReplayRecord` (ctest), show each family's mutation red, re-confirm the manifest | none | `Motion_Replay_*` green by construction |
| 2 | Tests first | the `camera` and `motion` State-only tests and `Frame_ParamsEqualCore`; red evidence in a scratch build with `OPENRENDER_CAMERA_MOTION_ONLY_TESTS=ON`. **Gate**: the maintainer's Linux starting-tree baseline (quickstart §4a: manifest, diffs, noise list, archcheck report, replay file) is taken at this checkpoint, before any phase-3 edit | none | all new tests red by build failure (quickstart §1) |
| 3 | Foundation | `frameParams.{h,cpp}`, `camera.{h,cpp}`, `motion.h`, `rendererFrame.{h,cpp}`, the two `renderer.cpp` calls; option switched on; no consumer yet | none | `Camera_*`, `Frame_*`, `Motion_TwoKeyModel`, `Motion_Isolation`, `Motion_CameraSlerpEquivalence`, `Motion_TransformRayEquivalence` |
| 4 | US1: the camera | `CRaytracer(int, const ICamera &)`, `computeSamples` through `generateRay` | 7 DB-011 | CP-d over every raytrace scene; SC-008 timed interleaved |
| 5 | Ray transform (`CDLObject` + quadrics) | `transformRay` replaces `objectMisc.h`'s ray `transform()` at 8 call sites | 6 DB-004 | `Motion_Replay_raytransform` (camera and #106 scenes, dlo) |
| 6 | Quadrics (7) | parameters at intersection, `sample()`, `*transformAtTime`, 14 `dPdtime` scales | 1 DB-004 | `Motion_Replay_quadrics` |
| 7 | Patches | `CBilinearPatch::intersect`, three `sample()` key lerps, three `dPdtime` scales | 1 DB-004 | `Motion_Replay_bilinear`, `_tessellation` (sample side) |
| 8 | Polygons | triangle and quad intersection, fused `sample()` lerps, two `dPdtime` scales | 1 DB-004 | `Motion_Replay_polygons` |
| 9 | Implicit surface | `pluginTime()` at its three reads | none | `Motion_Replay_implicit` |
| 10 | Tessellation | `CTesselationPatch::intersect`'s moving branch through `atTimeV` | none | `Motion_Replay_tessellation` |
| 11 | Grid-only | `curves`, `points`, `patchgrid`, `bsplinePatchgrid` (+ `subdivision`, Q2); dead `interpolate`/`interpolate3` deleted | 4 DB-004 | `Motion_Replay_gridonly`, `_emptyshutter`; SC-001 greps; SC-008 re-timed |
| 12 | Guardrails, docs, Linux | `metrics-baseline.txt` lowered to the report, allowlist header counts, registry (DB-004, DB-009, DB-011), DEVNOTES, HIDER_PARITY; the JIT-off build (quickstart §4c); the maintainer's Linux finished-tree run (§4b, which also runs the determinism sweep once on Linux); the determinism sweep once on macOS | none | CP-a to CP-d on both platforms; sweep green on both |

Notes:
- **Why phase 3 changes no output**: `FrameParams` and the camera are built and installed, but no
  code reads them until phase 4; CP-d proves it.
- **The red step of an allowlist phase**: after deleting the phase's lines, `ctest -L architecture`
  must report exactly those edges as `VIOLATION` and nothing else; that is recorded before the code
  change.
- **Phase 11 also checks SC-001**: no `CRenderer::invShutterTime` under `src/ri/geometry` or
  `src/ri/hiders`, and the files under `src/` naming the qualified `CRenderer::invShutterTime` go from 9
  to 3 (`rendererServicesImpl.h`, `rendererStatics.cpp`, `rendererFrame.cpp`).
- **Stop rule**: any CP-d difference, any unexpected `VIOLATION`, or any replay difference stops the
  phase for the maintainer. Nothing is absorbed by a threshold, a noise-list entry or a re-record.

## Test design (red first)

| Id | Test | Red on `4eb683b` because | Covers |
|---|---|---|---|
| T-C1 | `Camera_Analytic` | build failure (`camera.h`) | US1 AS1-AS3 |
| T-C2 | `Camera_StartingTreeEquivalence` | build failure | FR-006, FR-014 (camera), US1 |
| T-C3 | `Camera_CropInvariance` | build failure | US1 AS4, FR-006 |
| T-C4 | `Camera_GenerateRayOnly` | build failure | FR-009, FR-010, SC-010, US5 AS2-AS3 |
| T-C5 | `Camera_LensModel` | build failure | FR-007, US5 AS1 |
| T-F1 | `Frame_DefaultValue` | build failure (`frameParams.h`) | data-model default |
| T-F2 | `Frame_ParamsEqualCore` | build failure | FR-001, FR-002, decision 5, session reuse |
| T-M1 | `Motion_TwoKeyModel` | build failure (`motion.h`) | FR-011, decision 3, US2 AS2-AS3 |
| T-M2 | `Motion_Isolation` | build failure | FR-012, US2 AS5, SC-007 |
| T-M3 | `Motion_CameraSlerpEquivalence` | build failure | FR-021(a) |
| T-M4 | `Motion_TransformRayEquivalence` | build failure | FR-019 |
| T-R1 | `Motion_Replay_<family>` (8) | **characterization**: green on the starting tree by construction; red under its family's mutation (contracts/replay-format.md) | US2 AS1, AS4, FR-013, FR-014, FR-016, FR-018, FR-021(b), the `sample()`/`dPdtime` equivalence of FR-024 (its sample-side check **is** the replay's `S` records: there is no separate sample-side test) |
| T-A1 | `ctest -L architecture` after each allowlist phase's deletion | exactly that phase's `VIOLATION` lines | FR-022, SC-001 to SC-003 |

Existing guards whose assertions do not change: the 15 camera/motion/DOF visual scenes, the 14
motion and DOF parity tests, `Convergence_dof-*`, `Convergence_motion-*`, `CameraSamples_*`,
`Sampling_Camera*`, `DepthDeterminism_*`, `ColorDeterminism_*`, and the 57 camera/motion/DOF
determinism-sweep scenes. `DiskSampling` tests `sampleDisk()` in `src/common/random.h`, which no
hider calls since spec 039: it is unaffected by 028 and stays as it is (not deleted, not extended).

## Risks

- **FMA contraction** (R9): a migrated expression whose shape differs from the site's (operand
  order, a split statement) can change one rounding and one pixel. Mitigated by verbatim-copy unit
  tests, the per-family replay and CP-d on every phase; never fixed by re-recording.
- **GCC 13 on Linux** contracts more aggressively (`-ffp-contract=fast`). Mitigated by Linux's own
  starting-tree replay and manifest (decision 1). If a Linux-only difference appears, it is reported
  with the site; the remedy is the site's expression, not a threshold.
- **Installation order**: Geometry reading `frameParams()` before `beginFrame` would see the default
  value. No rendering path does; `Frame_ParamsEqualCore` checks the value from inside the frame and
  the default outside it.
- **Spec 027 overlap** (R15): `renderer.cpp`, `src/ri/CMakeLists.txt`, the allowlist, the metrics
  baseline and the registry are shared files with disjoint edits. No task depends on 027.
- **Replay harness fidelity**: the capture context must call `CRendererContext::addObject` before
  recording, or camera-motion synthesis would be bypassed and the camera scenes would test nothing.
  `Motion_Replay_raytransform`'s mutation (drop the SLERP conjugate) proves it is exercised.
- **Known flaky tests** (memory `reference_ctest_flaky_tests.md`): a red one is re-run alone before
  it is written off; `ShadingParity_AmbientAccumulation` is a real heap bug.

## Open questions for the maintainer (with recommendations)

**Decided 2026-10-27** (recorded in spec.md, Clarifications, "maintainer decisions on the plan" and "at the tasks stage"). The questions below are kept as written at the plan stage; each answer follows its question.

None of them changes a reference-doc decision (D1-D30); Q3 would change the spec's wording.

- **Q1. Camera and `FrameParams` in Graphics State.** Recommended (R1): it is the one domain both
  Geometry and Hiders may reach, D30's Transport can reach it, and it adds no edge. The alternative,
  a new Camera domain, needs a constitution amendment. **Recommendation**: approve Graphics State.
  **Decided**: approved, Graphics State.

- **Q2. `subdivision.cpp`'s float-precision time interpolation** (R14 finding 2) is outside decision
  2's grid-only list and has no `invShutterTime` read. **Recommendation**: include it in phase 11
  through `atTimeF` (FR-018's "each family's `sample()`"); otherwise it stays as the one copy left in
  Geometry.
  **Decided**: yes, into phase 11 through `atTimeF`.

- **Q3. FR-024 and SC-007 wording for the replay tests.** A replay recorded on the starting tree cannot
  fail there. **Recommendation**: amend FR-024 to say the replay tests are characterization tests,
  green on the starting tree by construction and shown red by a per-family mutation (spec 025's
  T-P1a precedent). The camera and evaluator unit tests stay red-first as written (SC-007).
  **Decided**: FR-024 and SC-007 reworded (replays are characterization tests, red by mutation).

- **Q4. Include pruning.** After phase 11, `objectMisc.h`, `dlobject.cpp`, `implicitSurface.cpp`,
  `patchgrid.cpp` and others name no `CRenderer` member but still include `renderer.h`; removing
  those includes would delete more DB-004 include lines, but each removal can surface transitive
  needs (spec 025 R12). **Recommendation**: not in 028 (SC-003 is met at 52); record the candidates
  in DB-004's evidence for spec 031 or a follow-up.
  **Decided**: no pruning in 028; phase 12 records the candidates in DB-004's evidence.

- **Q5. The shading_parity scratch-file race** (`tests/shading_parity/{ri,test}.tif`, validation §0.3).
  **Recommendation**: keep the two files in the noise list for 028 and file an issue; fixing it
  (separate working directories, as spec 025's T006a did for `tests/imager/ri.tif`) is test-only and
  can be added to phase 1 if you prefer.
  **Decided**: keep the two files in the noise list; spec 027 fixes the race; no 028 task edits those registrations.

- **Q6. `CBilinearPatch::sample()`'s `dPdtime` precedence error** (R14 finding 3) is preserved bit for
  bit. **Recommendation**: file it as an issue; a fix is an output-changing change for its own
  approved phase or spec.
  **Decided**: filed as GitHub #108; preserved bit for bit.

- **Q7. `CLAUDE.md`.** The plan stage did not run the agent-context update, because no agent message
  may authorize editing `CLAUDE.md`. **Recommendation**: at phase 12, with your explicit
  authorization, add the 028 lines to "Active Technologies"/"Recent Changes" and a Known gotcha on
  `FrameParams`' frame lifetime and on keeping expression shapes in `motion.h`.
  **Decided**: not pre-authorized; task T107 pauses and asks the maintainer at that time, and never touches "Spec-kit agent rules".


- **Q8. When the Linux starting-tree baseline is taken.** Decision 1 needs Linux's own manifest and
  replay recorded on the starting `src/`. From phase 3 on, the worktree no longer builds the
  starting `src/` with 028's tests (the option defaults ON and the State-only library names the new
  files). **Recommendation**: a hard gate at Checkpoint 2 (quickstart §4a): you run the Linux
  baseline there, and phase 3 waits for its files. The alternative, rebuilding it later from master
  plus a named subset of 028's test files with the option forced OFF, is more fragile.
  **Decided**: approved, a hard gate at Checkpoint 2 (tasks T036, T037).

## Reference-document decisions (D1-D30, §5.4, §7.4)

- §5.4's `ICameraService` is 028's `ICamera`; §7.4's `sampleRay(CameraSample)` maps onto
  `generateRay` through a non-virtual forwarder; `evaluateImportance` and `sampleConnection` are
  reserved by name, as §7.4 says.
- D18 (one `Spectrum` type for new interfaces) is respected by leaving the reserved result types
  undefined; their definition belongs to the BDPT spec.
- D30 is respected: nothing in 028 needs the Transport domain, and the camera's placement keeps it
  reachable from there.
- No other decision is touched.

## Complexity Tracking

None. The Constitution Check has no violation to justify.
