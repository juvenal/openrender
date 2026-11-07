# Implementation Plan: Geometry, Hider-Agnostic

**Branch**: `031-geometry-hider-agnostic` | **Date**: 2026-11-07 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/031-geometry-hider-agnostic/spec.md`, with the decisions of the
specify stage and of the 2026-11-07 session (lens unification, the irradiance pair to spec 032, `IObjectSink` now,
DB-025's one line).

**Status of this plan**: **approved 2026-11-07.** The maintainer accepted the recommended option of every plan
question, P-Q1 to P-Q14, kept orthographic depth of field off (P-Q10), and brought the Reyes bucket bound into the
lens phase (new FR-031). The answers are recorded in "Maintainer answers" below; spec.md carries the corrections
(Clarifications, "Plan-stage corrections (2026-11-07)").

## Summary

Geometry stops naming the hiders, the shading engine and the renderer core. Seven parts:

1. **Two Geometry-owned interfaces** (FR-002, FR-003, FR-007): `IRayContext` (thread, scratch arena, varying arrays,
   `shade`, `displace`, save/restore of the shading state), implemented by `CHider` and so by every hider; and
   `IDicer` (`rayContext()`, `drawObject`/`drawGrid`/`drawPoints`, `probe`, `inFrustum`), implemented by `CReyes`.
   Every `dice`, `splitToChildren`, `intersect`, `shade`, `cluster`, `materialize`, `create` and tessellation
   signature takes them instead of `CReyes *`/`CShadingContext *`; the region and crossing hooks take a nullable
   `IRayContext *`. No diamond: `IDicer` reaches the ray context through an accessor.
2. **One frame value** (FR-002, FR-015): `CGeometryFrame`, Geometry-owned, installed by the core once per frame
   (stored copies of the grid and split limits, thread count, flags, raytracing flags, projection and pixel
   constants, `clipMin`, the dicing lens values), exposed by both interfaces and read by `raytraced()`.
3. **The host and the sink** (FR-004, FR-010, FR-028): `CGeometryHost` gains the expansion of delayed objects and
   instances (the `processDelayed*` calls, same lock and order) and the coordinate-system lookup; `IObjectSink`
   replaces `CRiInterface *` in `instantiate`, with `CRiInterface` deriving from it; `blobby.cpp` asks Graphics
   State's `CDeclarations` directly.
4. **Shading's forwarding entries retire** (FR-005, FR-006): `CShadingContext::shade(CSurface *, ...)`,
   `displace`, `displaceEstimate`, `setProducer` and `IShadingProducer` go; `EShadingDim` moves to Infra
   (`common/shadingDim.h`); `kRendererServicesVersion` 2.1.0 -> 2.2.0.
5. **The three Geometry pairs go** (FR-008 to FR-011): `geometryDispatch.cpp`'s bodies move to their classes'
   files, `surfaceCrossingDispatch` becomes `surfaceCrossings` compiled for both libraries, the
   `rendererDeclarations` pair's two functions move into the core; `libRIBVector` compiles `surface.cpp` and no
   Geometry stub; the residual list goes from 9 to 5 lines.
6. **Reyes on the shared layer** (FR-012 to FR-014): Reyes and the stochastic rasterizer read the camera rotation,
   translation, flags and lens values from `FrameParams` and rotate through `motion::cameraRotationAt`; the 11
   DB-011 lines go.
7. **The lens phase, last and gated** (FR-022 to FR-030): `LensModel::circleOfConfusion(z)` is the one thin-lens
   function; Reyes scatters with it (signed, per axis), the raytracer's ray construction is pinned to it by a test;
   the raytracer's noise is investigated and reported before anything else changes.

Every image is byte-identical through Phase 8, on macOS and on Linux, each against its own baseline (spec 029's
manifest method). Only Reyes depth-of-field images are expected to change in the lens phase, each approved
individually.

**The lens derivation, verified** ([research.md](research.md) R9): the raytracer's blur radius is
`imagePlane·A·|1/z − 1/f|` (`camera.cpp:94-118`), the exact thin lens. Reyes's screen value is that times
`f/(f + A)` (`renderer.cpp:455`, `camera.cpp:38`), as the brief read it, **but what Reyes scatters is larger again**:
`cocFactorSamples` (`renderer.cpp:456`) uses the diagonal `sqrt(dSampledx² + dSampledy²)` and is applied on each
sample axis (`stochasticQuad.h:614-622`), an extra `sqrt(2)` for square sampling, and `absf` drops the sign. Reyes
blurs 1.35x (`DepthOfField 2 1 5`) to 1.39x (`sampling-frames.rib`) the physical radius, and its near field is not
mirrored. That, not an algorithmic limit, is most of spec 008's D9 residual.

## Technical Context

**Language/Version**: C++20 (`-Werror`), CMake >= 3.19.

**Primary Dependencies**: none new. `libRIBCommon`, `libRIBRender`, `libRIBVector`, `libshader_shading`; LLVM only for
the existing JIT.

**Storage**: N/A; memory only. New recorded files are test baselines under `tools/` (manifests, `nm` lists).

**Testing**: ctest. New label `geometry_contract` (in the everyday run); additions to `architecture` and `camera`;
the temporary option `OPENRENDER_031_RED_TESTS` (as spec 029's DEP-2) keeps the new targets out of the normal build
until their phase. Existing suites at every checkpoint: `-LE determinism-sweep`, `architecture`, `^determinism$`,
`Motion_Replay_*`, `Motion_CameraSlerpEquivalence`; the sweep at the Phase 8 gate and at the end.

**Target Platform**: macOS (Apple Silicon, Apple Clang, vcpkg, 13.3 target) and Linux (Ubuntu 24.04, GCC 13,
`-Werror`, over ssh in `~/Projects/Development/CLI/openrender-031`, remote `cpujob -j2`).

**Project Type**: renderer library and CLI.

**Domains touched**:

| Domain | What changes |
|---|---|
| Geometry | New: `rayContext.h`, `dicer.h`, `objectSink.h`, `geometryFrame.{h,cpp}`; renamed `surfaceCrossingDispatch.{h,cpp}` -> `surfaceCrossings.{h,cpp}`. Edited: `object.{h,cpp}`, `objectMisc.h`, `surface.{h,cpp}`, `curves.{h,cpp}`, `points.{h,cpp}`, `delayed.{h,cpp}`, `quadrics.{h,cpp}`, `polygons.{h,cpp}`, `patches.{h,cpp}`, `patchgrid.{h,cpp}`, `bsplinePatchgrid.{h,cpp}`, `subdivision*.{h,cpp}`, `csg*.{h,cpp}`, `dlobject.{h,cpp}`, `implicitSurface.{h,cpp}`, `blobby.cpp`, `geometryHost.h` |
| Hiders | `hider.{h,cpp}` (`IRayContext`, no `IShadingProducer`), `shadingProducer.cpp`, `reyes.{h,cpp}` (`IDicer`, `FrameParams`, lens), `stochastic.cpp`, `stochasticQuad.h`, `trace.cpp` (call sites), `sampler.{h,cpp}` (lens phase), `photonMap.cpp` (P-Q3 (a)) |
| Shading | `shading.{h,cpp}` (entries removed, `EShadingDim` include), `shadingProducer.h` (deleted), `rendererServices.h` (2.2.0) |
| Graphics State | `frameParams.{h,cpp}` (two copies, Phase 7; removed in the lens phase), `camera.{h,cpp}` (lens phase) |
| Scene Interface | `parse/riInterface.h` (`CRiInterface : IObjectSink`) |
| Infra | new `src/common/shadingDim.h` |
| Renderer core | `renderer.{h,cpp}` (frame-value install/uninstall, `setChildren` call, `tesselateMutex` removed; lens phase: the DoF block), `rendererFrame.cpp`, `rendererContext.{h,cpp}` (host overrides, `processDelayed*` signatures), `rendererDeclarations.cpp` (`notifyGlobalVariable`), `rendererMutexes.cpp`; deleted `geometryDispatch.cpp`, `rendererDeclarationsDispatch.cpp`, `vector/{geometry,surfaceCrossing,rendererDeclarations}DispatchStub.cpp`; `src/ri/CMakeLists.txt` |
| Tests, docs | `tests/geometry_contract/` (new), `tests/architecture/` (allowlist, residual list, metrics, two checks), the approved-test edits of [contracts/tests.md](contracts/tests.md); `DOMAIN_BOUNDARIES.md`, `DEVNOTES.md`, `HIDER_PARITY.md`, spec 008's D9 text |

**Not edited**: Texturing, Display Outputs, Shader DSO Operands, Procedures; the JIT and every `op_*`; the plug-in ABI
headers; hider selection in `beginFrame()` (spec 030); `irradianceDispatch.cpp` and its stub (spec 032); every motion
expression of spec 028; the replay recordings; every image reference before the lens phase; `CLAUDE.md` (the
maintainer updates gotchas 13, 14, 17, 19, 26, 35 at the end; a draft is written at the last phase).

**Performance Goals**: no measurable change. Added per call: a virtual `drawObject`/`drawGrid`/`drawPoints` (once per
object or grid) and a virtual `varyings()` (a few per dice); the shading calls lose one hop (no forwarding entry).
Measured with spec 029's timing method (research R12).

**Constraints**: byte-identical output per platform through Phase 8; no threshold, reference, manifest or replay
changed; the allowlist only shrinks and no line is re-keyed; no task depends on any merge; `cpujob` for every job.

**Scale/Scope**: about 6 new source files, 5 deleted, 2 renamed, about 60 edited sources; 7 new tests (+2 or 4 in the
lens phase); about 30 approved test files edited mechanically.

**Measurements at the plan stage** (static; Phase 1 re-measures on a build):

- Allowlist: 344 entries; DB-004 52, DB-006 30, DB-011 164, DB-017 78, DB-025 12. Metric: Geometry 48/15, Hiders 556/81.
- The 11 DB-011 lines of FR-012/FR-013 verified line by line (research R8).
- `libRIBVector` compiles 9 Core files (residual list), 4 of them stubs.
- Include fallout: one Hiders file, `photonMap.cpp`, relies on `object.h -> renderer.h` (research R7, P-Q3).

## Constitution Check

*GATE: must pass before Phase 0 research; re-checked after Phase 1 design.*

| Principle | Status | Notes |
|---|---|---|
| I. Clean code | PASS | Three link-time substitution pairs, a forwarding entry, a producer interface, an unused parameter (`setChildren`), a dead declaration (`curvesCreate`) and two dead debug blocks go; the dicing limits become one named value. |
| II. C++20 / C17 | PASS | Abstract interfaces with defaulted virtual destructors, `override`, references for required collaborators, `std::mutex` for the Geometry lock. |
| III. TDD (non-negotiable) | PASS (by plan order) | Phase 2 writes every approved new test red first; the image manifests, the motion replay and the analytic lens tests are the guards. No test is written before the maintainer approves [contracts/tests.md](contracts/tests.md). |
| IV. CLI | N/A | No CLI change. |
| V. Minimal dependencies | PASS | None added. |
| VI. Linux and macOS | PASS (gated) | Each platform records its own baseline (Phase 1); Phase 8 is green on both before the lens phase. |
| VII. Docs | PASS | Registry, allowlist, metric, residual list, DEVNOTES, HIDER_PARITY, spec 008's D9; `CLAUDE.md` only as a draft. |
| VIII. Domain separation | PASS | Below. |

**Principle VIII — Domain Separation and Modular Boundaries**:

- [x] **Domains touched are named**: Geometry, Hiders, Shading, Graphics State, Scene Interface, Infra, the renderer
      core, tests and docs (table above). Texturing, Display, Shader DSO and Procedures are not edited.
- [x] **No new cross-domain dependency outside the table, none on the core from a domain**:
  - **Added** (all allowed): Hiders -> Geometry (`hider.h` includes `rayContext.h`, `reyes.h` includes `dicer.h`,
    Reyes uses `motion.h`); Hiders -> Graphics State (`frameParams.h` in `reyes.cpp`/`stochastic.cpp` and
    `photonMap.cpp`); Scene Interface -> Geometry (`riInterface.h` includes `objectSink.h`);
    Geometry -> Graphics State (`declarations.h` in `blobby.cpp`, `options.h` and `camera.h`/`frameParams.h` in
    `geometryFrame.h`); every domain -> Infra (`shadingDim.h`); Core -> Geometry (installing the frame value,
    the host overrides).
  - **Removed**: Geometry -> Hiders and Geometry -> Shading (DB-006, 30 edges); Geometry -> Core and Geometry ->
    Scene Interface (DB-004, 49 edges); Geometry and Hiders -> Core symbols defined in the pair files
    (DB-017, 78 edges); Hiders -> Core (DB-011, 13 edges); Geometry -> Consumer (DB-025, 1 edge).
  - **The upward capabilities** (drawing, probing, shading hits, expansion, coordinate systems) are inverted through
    interfaces Geometry owns and the hiders or the core implement (rule 2); none is bound at link time any more.
  - No new Hiders file reads a `CRenderer` static; `CReyes::inFrustum` is defined in `reyes.cpp`, whose
    `CRenderer::inFrustrum` line already exists.
- [x] **Interface changes specified**: [contracts/geometry-interfaces.md](contracts/geometry-interfaces.md),
      [contracts/host-and-sink.md](contracts/host-and-sink.md), [contracts/shading-boundary.md](contracts/shading-boundary.md),
      [contracts/pairs-and-ribvector.md](contracts/pairs-and-ribvector.md), [contracts/reyes-shared-layer.md](contracts/reyes-shared-layer.md),
      [contracts/lens-model.md](contracts/lens-model.md), [contracts/allowlist-end-state.md](contracts/allowlist-end-state.md),
      [data-model.md](data-model.md).
- [x] **No hider-specific special case**: Geometry names no hider; the ray side is implemented by every hider; the
      dicing side by the hiders that rasterize. Depth of field and camera-rotation motion are implemented once
      (rule 3): one rotation evaluator (Phase 7) and one lens function (lens phase) for both hiders.
- [x] **Plug-in interfaces**: none changed (shadeop, procedural, display untouched). The internal Shading boundary
      steps `kRendererServicesVersion` to 2.2.0 with a history entry (gotcha 28).
- [x] **Registered violations not extended; reductions noted**: DB-006 resolved; DB-017 to 0 lines; DB-004 to 3; DB-011 by 13; DB-025 by 1; DB-009 resolved with the lens phase
      ([contracts/allowlist-end-state.md](contracts/allowlist-end-state.md)). No allowlist line added.

**Post-design re-check (after Phase 1 design)**: PASS. The design adds no edge outside the table. The two places it
leans on an existing registered edge instead of a new interface are named: `CReyes::inFrustum` (DB-011, existing
line, P-Q12) and `blobbyRepeller.cpp` (DB-004's remainder, out of scope by FR-016).

## Maintainer answers (2026-11-07, plan approved)

| # | Answer | Consequence recorded in |
|---|---|---|
| P-Q1 | **Yes**: `IDicer`, `IRayContext`, `IObjectSink` at global scope; `CGeometryHost` extended (no `IProceduralExpander`) | contracts/geometry-interfaces.md, host-and-sink.md |
| P-Q2 | **Yes**: references where a context is required, nullable pointers for the region and crossing hooks; `setChildren` loses its unused parameter. The ~30 mechanical edits to approved tests are shown at the test-approval pause | contracts/tests.md |
| P-Q3 | **(a)**: `photonMap.cpp` reads `frameParams().camera.fromWorld/toWorld`; allowlist lines 137-138 (`photonMap.cpp -> CRenderer::toWorld`, `fromWorld`) are deleted. Spec 032 deletes the same two lines in its Phase 5 (it splits `photonMap.cpp`); whichever spec merges second drops its duplicate edit. The coordinator checked line by line: this is the only overlap between 031's deletions and 030's and 032's | research R7, R14, R17; contracts/allowlist-end-state.md; spec SC-005, SC-006, SC-007 |
| P-Q4 | **Yes**: `blobby.cpp` calls `CDeclarations::find` directly | research R4; contracts/host-and-sink.md; spec US5 AS2, FR-010, Key Entities |
| P-Q5 | **Yes**: the CSG units, `dlobject.cpp`, `implicitSurface.cpp` stay out of `libRIBVector` | research R6; contracts/pairs-and-ribvector.md |
| P-Q6 | **Yes**: the tessellation lock moves into Geometry | contracts/geometry-interfaces.md; spec FR-003 |
| P-Q7 | **Yes**: `kRendererServicesVersion` 2.2.0; if 032 merges first and has stepped it, 031 renumbers to the next MINOR | research R15; contracts/shading-boundary.md |
| P-Q8 | **Yes**: the raytracer's construction is the optical reference; only Reyes depth-of-field images change from the formula | research R9; contracts/lens-model.md |
| P-Q9 | **Yes**: US7 AS2 covers Reyes and raytrace only (zbuffer has no depth of field) | spec US7 |
| P-Q10 | **Keep orthographic depth of field OFF** in both hiders. Out of scope, a possible follow-up (the coordinator handles any issue). `renderer.cpp:447` is not edited; the analytic test asserts 0 for orthographic | research R9; contracts/lens-model.md; spec FR-024, Scope Boundaries |
| P-Q11 | **Accept**: the Reyes blur becomes physical (about 26-28 % smaller in the test scenes, mirrored near field, the same smooth shading); each depth-of-field reference is approved image by image | contracts/lens-model.md |
| P-Q12 | **Yes**: the frustum test stays in the core (`CReyes::inFrustum` -> `CRenderer::inFrustrum`, existing allowlist line) | contracts/geometry-interfaces.md |
| P-Q13 | **Approved at plan level**; each new test is still shown at the TEST-APPROVAL pause before it is written | contracts/tests.md |
| P-Q14 | **Yes**: spec corrected (SC-005, SC-006, SC-007 -> 344 -> 173, US5 AS2, US7, FR-003, FR-010, FR-024, Key Entities, Scope Boundaries; new FR-031) | spec.md Clarifications |
| Pinned tests | The changes to `test_frame_params_equal_core.cpp` and `test_camera_lens_model.cpp` are approved in principle; each diff is shown before it is applied | contracts/tests.md items 8-9 |
| Bucket bound | **Brought into the lens phase** (new FR-031): the bound at `reyes.cpp:616-636` must cover the unified circle of confusion on each axis, verified by `Hiders_ReyesBucketBoundCoversCoC` (below); the possible link to #112 is noted without claiming a fix | contracts/lens-model.md, tests.md; Phase 9 |

The table below is the question list as it was put to the maintainer, kept for the record.

## Questions for the maintainer (answered above)

Each has a recommendation; the plan assumes it. Evidence in [research.md](research.md).

| # | Question | Recommended | Alternatives |
|---|---|---|---|
| **P-Q1** | Names | `IDicer`, `IRayContext`, `IObjectSink` at global scope; extend `CGeometryHost` (no `IProceduralExpander`) (R2, R4) | `C`-prefixed names; an `openrender::geometry` namespace; a separate `IProceduralExpander` |
| **P-Q2** | Reference or pointer | References where a context is required (FR-007); nullable `IRayContext *` for the region and crossing hooks (the "may be NULL" contract); `setChildren` loses its unused parameter. Costs the mechanical test edits of [contracts/tests.md](contracts/tests.md) items 2-6 | pointers everywhere (fewer test edits, but a call without an implementer stays writable); keep `setChildren`'s dead parameter |
| **P-Q3** | `hiders/photonMap.cpp` breaks when `object.h` drops `renderer.h` (R7) | (a) read `frameParams().camera.fromWorld/toWorld` (bit-identical copies; deletes 2 more DB-011 lines; touches a file spec 032 moves; both specs delete the same 2 lines) | (b) keep `object.h -> renderer.h` until `photonMap.cpp` stops reading the statics: DB-004 ends at 4 lines and FR-016/SC-005 are not met as written |
| **P-Q4** | Blobby's variable lookup (US5 AS2 says "a Geometry-owned service") | call `CDeclarations::find` (Graphics State) directly: it is exactly what `CRenderer::retrieveVariable` returns | a `CGeometryHost::retrieveVariable` or a Geometry-owned lookup interface forwarding to Graphics State |
| **P-Q5** | FR-008 "compiled identically for both libraries" | read as "no per-library variant": `surface.cpp` and `surfaceCrossings.cpp` join `libRIBVector`; the CSG units, `dlobject.cpp`, `implicitSurface.cpp` stay out (whole classes the preview never builds, not stand-ins) | compile them into `libRIBVector` too (pulls the Procedures loader in) |
| **P-Q6** | The tessellation lock (FR-003 lists it on the ray side) | Geometry owns it (`std::mutex` in `surface.cpp`, same scopes); `CRenderer::tesselateMutex` deleted | `IRayContext::lockTessellation()`/`unlock` answered by the core's mutex |
| **P-Q7** | Shading boundary version | `kRendererServicesVersion` 2.1.0 -> 2.2.0, one entry (breaking, allowed under 2.x); renumbered by whichever of 031 and 032 merges second (R15) | no step |
| **P-Q8** | The optical reference of the lens phase | the raytracer's existing construction (the exact thin lens): `generateRay` keeps its bits, only Reyes's depth-of-field images change from the formula | a model with the RISpec focal-length magnification explicit (identical in screen units, R9) |
| **P-Q9** | US7 AS2 names "reyes, zbuffer, stochastic and raytrace" | zbuffer has no depth of field (nor motion blur), and "stochastic" is the Reyes hider: agreement is asserted for Reyes and raytrace; zbuffer stays sharp | add depth of field to zbuffer (a new capability, out of this spec) |
| **P-Q10** | Orthographic depth of field (FR-024) | keep it off as today (`renderer.cpp:447` forces the aperture to 0 for both hiders); the analytic test asserts 0; no orthographic DoF scene exists | enable the telecentric thin lens `A·|1 − z/f|` (a behaviour change, stop-and-ask by FR-030) |
| **P-Q11** | What the lens unification does to Reyes | accept: Reyes's blur radius becomes physical (about 26-28 % smaller in the test scenes), the near field is mirrored as through a real lens, the blur is a true disk for any `PixelSamples` aspect, and the bucket bound matches the scatter. Reyes keeps its smooth (decoupled) shading; it loses the 1.35x over-blur | keep Reyes's softer look (contradicts FR-022's single reference); unify only the screen factor and keep the diagonal (still 1.41x) |
| **P-Q12** | The frustum test | `IDicer::inFrustum`, answered by `CReyes` with `CRenderer::inFrustrum` (existing DB-011 line; no numeric risk) | copy the planes into `CGeometryFrame` and the test into Geometry |
| **P-Q13** | The new tests ([contracts/tests.md](contracts/tests.md)) | approve the seven interface tests and the two analytic lens tests; `DoF_BlurDiameter_*` optional | drop or change any |
| **P-Q14** | Figures in the spec | correct SC-007 to deltas from `e83d005d` (344 -> 173 under P-Q3 (a), 176 under (b)), SC-005 under (b), and US7 AS2 under P-Q9, in spec.md's Clarifications once answered | leave the spec as is and record the measured values in the plan only |

**Stop-and-ask points during implementation** (FR-030 and the rules): any sampler-stream or `PixelSamples` default
change; any change to how the raytracer shades lens samples; any move toward orthographic depth of field (out of scope, P-Q10); any include-fallout
file in Hiders, Texturing, Graphics State or Shading other than `photonMap.cpp`; any differing image byte before the
lens phase; any `Motion_Replay_*` difference; any depth-of-field reference (image by image).

## Phases

Each phase ends with the §2 checkpoint of [quickstart.md](quickstart.md) (manifest diff, `MaxBlockAvgDiff`,
architecture with that phase's allowlist deletions, motion replay, `^determinism$`) and a pause for the maintainer.

| Phase | Content | Gate / proof |
|---|---|---|
| **1. Baselines** | Manifests (suite, sweep, noise) on macOS and Linux; archcheck report; metric; `nm -u` of `libRIBVector`; the include-fallout probe (R7); the test-edit inventory; timing baseline; `tools/` | Linux baseline exists before Phase 2 |
| **2. Tests first** | After approval of P-Q13: the `geometry_contract` programs and the two architecture checks, shown red (`OPENRENDER_031_RED_TESTS`) | red evidence recorded |
| **3. Foundations** | `common/shadingDim.h`; `geometryFrame.{h,cpp}` installed by the core (`initTesselations` takes the thread count); `IObjectSink` and `instantiate`; `CGeometryHost` additions and `CRendererContext` overrides; `blobby.cpp` -> `CDeclarations::find`; `object.cpp` -> host lookup; `raytraced()` from the frame value; `curvesCreate` deleted | byte check; DB-004 lines for `riInterface.h`, `rendererContext.h`, `blobby.cpp`, `object.cpp` deleted |
| **4. The ray side** | `IRayContext`; `CHider` implements it; `intersect`, `shade`, `cluster`, `materialize`, `create`, `checkRayGuard`, region and crossing hooks, tessellation, **changed in place**: the ray-side bodies stay in `geometryDispatch.cpp` for now and the stubs track the new signatures, so `libRIBVector` and the preview link at this checkpoint; `setChildren` parameter; the Geometry tessellation lock; `debug.h` and the dead blocks (DB-025) | byte check; `libRIBVector` and `Preview_*` green; the matching DB-006/DB-004/DB-025 lines deleted |
| **5. The dicing side** | `IDicer`; `CReyes` implements it; every `dice`/`splitToChildren`/`probe`, changed in place (bodies in `geometryDispatch.cpp`, stubs tracking); the forwarding entries and `IShadingProducer` retired; `kRendererServicesVersion` 2.2.0 | byte check; `libRIBVector` and `Preview_*` green; the DB-006 link lines for `CReyes`/`CShadingContext` deleted |
| **6. Pairs and the preview library** | **one atomic step**: the bodies of `geometryDispatch.cpp` move to their classes' files (so `surface.cpp` becomes `CSurface`'s key-function owner) at the same time as `surface.cpp` and `surfaceCrossings.cpp` join `libRIBVector` and the three stubs and both pair files are deleted; the `rendererDeclarations` pair; the residual list; every remaining Geometry include of `renderer.h`/`shading.h`/`shader.h`/`reyes.h`; `photonMap.cpp` (P-Q3 (a)); `nm -u` check | byte check; `Geometry_DiceThroughDicer`, `Geometry_IntersectThroughRayContext`, `Geometry_RibVectorNoStubs`, `Geometry_NoHiderShadingNames`, `SceneInterface_RibVectorResidual`, `Preview_*` green; DB-006, DB-017 empty |
| **7. Reyes on the shared layer** | `FrameParams` copies; Reyes and the stochastic rasterizer on `FrameParams` and `motion::cameraRotationAt` | byte check; the 11 DB-011 lines deleted; `Motion_CameraSlerpEquivalence` green |
| **8. Interface-phase gate** | Full validation on both platforms (§6 of the quickstart): suite, determinism set, sweep and its manifest, replay, timing; registry, metric and allowlist for DB-004, DB-006, DB-011, DB-017, DB-025 | **the maintainer accepts the gate before the lens phase** |
| **9. Lens: one function** | The analytic tests and the bucket-bound test red, then `LensModel::circleOfConfusion`, Reyes's signed per-axis scatter, the bucket bound and the grid bounds covering it (FR-031), dicing; old factors removed; D9 text corrected; orthographic stays off (P-Q10) | analytic and bucket-bound tests green; everything outside research R11 byte-identical; each changed DoF image presented and approved; the #112 observation reported |
| **10. Lens: raytracer noise** | The FR-025 investigation and its report (`lens-investigation.md`); any proposed sampling or filtering change goes to the maintainer first | report delivered; approved changes only |
| **11. Close** | DB-009 Resolved (or a history entry if Phase 9/10 is stopped); DEVNOTES, HIDER_PARITY, `claude-md-draft.md`; final validation on both platforms | sweep and suite green on both |

**Why the bodies move only in Phase 6**: `CSurface::intersect` is `CSurface`'s key function, so moving it into
`surface.cpp` moves the vtable, and `surface.cpp` cannot join `libRIBVector` before it stops naming `CReyes` (end of
Phase 5). Changing signatures in place until then keeps `libRIBVector`, the preview and the data viewer linking at
every checkpoint; the two `geometry_contract` programs that link `libRIBVector` alone turn green in Phase 6.

**Ordering notes**: Phase 7 is independent of Phases 3 to 6 and may run earlier if the maintainer prefers; it must
finish before Phase 8. No phase depends on a merge of 030, 032 or 056 (FR-018, SC-011).

## The other specs of this round (030, 032, 056)

Their `plan.md` files are still templates; this is from their `spec.md` and is re-checked at analyze. Detail in
[research.md](research.md) R14. In short:

- **030** shares `renderer.cpp` (different lines: 031 never touches hider selection), `hider.{h,cpp}` (030 may add
  registration; 031 adds `IRayContext` and drops `IShadingProducer`), the `reyes.cpp` DB-011 block and the Hiders
  metric row (different lines; the second merger re-measures), and `src/ri/CMakeLists.txt`'s `ribVector_sources`
  (030 removes `hcshader.cpp`).
- **032** shares DB-025 (031 deletes the `surface.cpp` line; whichever merges second moves the entry), the residual
  list (the `rendererStatics.cpp` line's "needed by" cell: a textual conflict the second merger resolves), the
  services version (whichever merges second takes the next MINOR), and, under P-Q3 (a), `photonMap.cpp` and its two
  DB-011 lines (both delete them). **032 does not edit `surface.cpp`** (its FR-016), which corrects the brief.
- **056** shares `csgSolid.cpp` (its one line at `:273` sits inside `CCSGSolid::build`, whose signature 031 changes;
  the line itself is untouched), `shading.cpp`/`shading.h` (different regions) and `trace.cpp` (different lines).

## Project Structure

### Documentation (this feature)

```text
specs/031-geometry-hider-agnostic/
├── spec.md
├── plan.md              # this file
├── research.md          # R1-R17
├── data-model.md
├── quickstart.md
├── contracts/
│   ├── geometry-interfaces.md
│   ├── host-and-sink.md
│   ├── shading-boundary.md
│   ├── pairs-and-ribvector.md
│   ├── reyes-shared-layer.md
│   ├── lens-model.md
│   ├── allowlist-end-state.md
│   └── tests.md
├── checklists/requirements.md
└── tasks.md             # /speckit-tasks (not created here)
```

### Source Code (repository root)

```text
src/common/shadingDim.h                         new (Infra)
src/ri/geometry/
├── rayContext.h, dicer.h, objectSink.h         new (Geometry-owned interfaces)
├── geometryFrame.{h,cpp}                        new (the frame value)
├── surfaceCrossings.{h,cpp}                     renamed from surfaceCrossingDispatch.{h,cpp}
├── geometryHost.h                               extended
└── object, surface, curves, points, delayed, quadrics, polygons, patches, patchgrid, bsplinePatchgrid,
    subdivision*, csg*, dlobject, implicitSurface, blobby, objectMisc   edited
src/ri/hiders/  hider.{h,cpp}, shadingProducer.cpp, reyes.{h,cpp}, stochastic.cpp, stochasticQuad.h, trace.cpp,
                sampler.{h,cpp} (lens phase), photonMap.cpp (P-Q3 (a))
src/libshader/shading/  shading.{h,cpp}, rendererServices.h; shadingProducer.h deleted
src/ri/state/   frameParams.{h,cpp}, camera.{h,cpp} (lens phase)
src/ri/parse/riInterface.h
src/ri/render/  renderer.{h,cpp}, rendererFrame.cpp, rendererContext.{h,cpp}, rendererDeclarations.cpp,
                rendererMutexes.cpp; geometryDispatch.cpp and rendererDeclarationsDispatch.cpp deleted
src/ri/vector/  three stubs deleted (irradianceDispatchStub.cpp stays, spec 032)
src/ri/CMakeLists.txt
tests/geometry_contract/                         new
tests/architecture/  allowlist.txt, ribvector-core-residual.txt, metrics-baseline.txt, two new checks
```

**Structure Decision**: the existing single-project layout; the new Geometry interfaces sit in `src/ri/geometry/`
next to `geometryHost.h`, the one existing Geometry-owned service.

## Complexity Tracking

No constitution violation needs justifying. Two deliberate non-minimal choices, both reversible:

| Choice | Why | Simpler alternative rejected because |
|---|---|---|
| `CGeometryFrame` as an installed slot, plus a `frame()` accessor on both interfaces | `raytraced()` and the static tessellation setup have no interface to ask; the accessor satisfies FR-002's "exposed by both" with one source | passing the frame through every signature: no caller can pass it to `raytraced()` |
| `geometryFrame.h`'s copy of `camera2pixels` | Geometry may not include `renderer.h`; the expression is copied character for character so the rounding is the same | reading the frame in `renderer.h`'s copy: changes a core inline every hider uses, outside this spec's need |
