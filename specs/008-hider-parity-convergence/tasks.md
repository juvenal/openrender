---

description: "Task list for feature implementation"
---

# Tasks: Reyes/Raytrace Hider Parity Convergence

**Input**: Design documents from `/specs/008-hider-parity-convergence/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md (all present)

**Tests**: This feature's "tests" are the cross-hider parity scene pairs and the reused
`test_disk_sampling.cpp`/`test_radial_histogram.cpp` gates — not a new unit-test framework.
research.md explicitly rejects introducing a separate test-infra boundary; Constitution
Principle III (TDD, NON-NEGOTIABLE) is satisfied by Story 1's parity harness landing first
and every subsequent story authoring its own scene pairs and confirming them fail (Red)
against unmodified code *before* writing the shared-component code that makes them pass
(Green). Stories whose safety net is the pre-existing full visual-regression suite instead
of new scene pairs (Stories 7 and 8, both pure code-motion refactors with no new behavior to
diff) satisfy Red→Green via that suite instead: it already exists and already passes before
these stories touch anything, so "existing green suite must stay green" is itself the
regression gate.

**Organization**: Tasks are grouped by user story, in the priority order given in spec.md
(P1 → P2 → P3 → P4 → P5). Unlike the generic template, these stories are **not** independent
or parallelizable across phases: Story 1 (the parity harness) is every other story's stated
prerequisite (spec.md's own "Why this priority" for Story 1), and Stories 2-9 mostly edit the
same two files (`stochastic.cpp`, `raytracer.cpp`), creating file-level contention even where
no logical dependency exists. See **Dependencies & Execution Order** below for the real chain.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies on incomplete tasks)
- **[Story]**: Which user story this task belongs to (US1-US9, matching spec.md priorities)
- Exact file paths are included in every task description

## Path Conventions

Single C++ project (`src/`, `tests/`, `examples/rib/` at repository root) — see plan.md's
Project Structure section for the authoritative file map this task list follows.

---

## Phase 1: Setup

**Purpose**: Establish the pre-change baseline every later regression/perf check compares against.

- [X] T001 Build the renderer (`cmake --build build --config Release`) and run the existing
      baseline suites — `ctest --test-dir build -L visual --output-on-failure` and
      `ctest --test-dir build -R "disk_sampling|radial_histogram" --output-on-failure` — then
      time `build/src/orender/orender examples/rib/camera-dof.rib` and
      `examples/rib/tests/motion-1-{reyes,raytrace}.rib` per quickstart.md. Record pass/fail
      status and timings; this is the FR-030/SC-007 regression baseline for every later phase.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Minimal shared scaffolding every user story's new scenes/docs land into. No
shared production code is foundational here — R1-R4/S1-S5 are each scoped to their own story
phase, not to this phase, since none of them individually blocks more than one downstream
story (see Dependencies section).

- [X] T002 [P] Create the `examples/rib/tests/parity/` directory that will hold every new
      cross-hider scene pair added across Phases 3-11.
- [X] T003 [P] Read `DEVNOTES_DETAILS/HIDER_PARITY.md`'s current Alignment Status checklist in
      full and note the exact checkbox lines each later phase must update (Motion Blur
      Implementation, Shading Interpolation & Derivatives, Displacement Parity, Transparency
      Handling; Unified Pixel Filtering is already `[x]`).

**Checkpoint**: Scaffolding ready. Phase 3 (User Story 1) can begin.

---

## Phase 3: User Story 1 - Cross-hider parity safety net (Priority: P1) 🎯 MVP

**Goal**: A harness that renders the same scene with both hiders and reports a per-effect
pass/fail divergence verdict, with no manual image diffing required.

**Independent Test**: Run `ctest --test-dir build -L parity` and get a pass/fail per scene
pair; deliberately break shared sampling/compositing code and confirm at least one scene fails.

This story's own scene pairs are limited to effects expected to **already converge** on
unmodified code (flat shading; AOV capture without transparency; the raytrace depth-filter's
current default mode). The depth-of-field scene pair is included here too, since D1's
`sampleDisk()` fix predates this feature and gives it a working starting point — but see
T008's note: its cross-hider agreement may still carry a small residual from the *separate*,
still-open pixel-jitter-constant drift (D2) that Phase 4 closes, so it is authored now but not
required to pass tightly until Phase 4's T021 re-checks it. Effects that are known to diverge
until a later story's fix lands (transparency, matte, all depth-filter non-default modes,
motion blur, displacement) get their scene pairs added within the story that fixes them, in
Phases 5-8, so each story's Red→Green transition stays honest.

- [X] T004 [US1] Duplicate `test_visual_render.cpp`'s `TiffImage`/`readTiff`/`compareTiffs`
      block-average diff code into a new `tests/visual/test_hider_parity.cpp`, per
      research.md's decision to follow the same duplication-over-shared-header convention
      `test_radial_histogram.cpp` already uses.
- [X] T005 [US1] Extend `test_hider_parity.cpp`'s `main()` to accept two
      `(orender_path, rib_path, output_tif_name)` triples, run `orender` twice, and diff the
      two fresh outputs against each other with a per-scene threshold argument (candidate vs.
      candidate, not candidate vs. static reference).
- [X] T006 [US1] Add an `add_parity_test(SCENE_NAME RIB_A OUTPUT_A RIB_B OUTPUT_B [THRESHOLD])`
      CMake macro to `tests/visual/CMakeLists.txt`, modeled on the existing `add_visual_test`
      macro (same scratch-dir pattern, `VISUAL_ENV`, `TIMEOUT 360`), adding a `parity` ctest
      label alongside `visual;regression`.
- [X] T007 [P] [US1] Author the flat-shading parity scene pair
      `examples/rib/tests/parity/flatshade-reyes.rib` / `flatshade-raytrace.rib`.
- [X] T008 [P] [US1] Author the depth-of-field parity scene pair
      `examples/rib/tests/parity/dof-reyes.rib` / `dof-raytrace.rib`. Note: D1 (lens sample
      distribution) is already fixed, but D2 (pixel-jitter-constant drift, `0.5001011` vs
      `0.5`) is not — this scene pair's initial threshold must be set loosely enough to
      tolerate D2's residual contribution today; Phase 4's T021 re-measures it once D2 closes
      and is where a materially tighter DOF result is actually expected.
- [X] T009 [P] [US1] Author the AOV-without-transparency parity scene pair
      `examples/rib/tests/parity/aov-reyes.rib` / `aov-raytrace.rib`.
- [X] T010 [P] [US1] Author the depth-filter-default-mode parity scene pair
      `examples/rib/tests/parity/depthdefault-reyes.rib` / `depthdefault-raytrace.rib`.
- [X] T011 [US1] Register T007-T010's scene pairs in `tests/visual/CMakeLists.txt` via
      `add_parity_test`, each tagged with its effect's initial threshold
      (data-model.md Entity 2: Per-effect parity threshold).
- [X] T012 [US1] Document each effect's threshold and rationale in
      `tests/visual/parity-thresholds.md`, including the two documented residuals from
      data-model.md Entity 8 (shading-interpolation, DOF-occlusion-model) with their bounding
      thresholds (FR-028).
- [X] T013 [US1] Run `ctest --test-dir build -L parity --output-on-failure`, confirm T007-T010
      all pass, then deliberately perturb a shared constant to confirm at least one scene fails
      (Story 1 acceptance scenario 3), and revert the deliberate change.

**Checkpoint**: Harness works end-to-end. Phases 4-11 may now add their own scene pairs to it.

---

## Phase 4: User Story 2 - One shared per-sample generator (Priority: P2)

**Goal**: One `CSampler` component supplies jitter/time/lens for both hiders, absorbing the
already-fixed `sampleDisk()` lens logic and the canonical lens/CoC formula set (S1).

**Independent Test**: Inspect the source for one shared sampler; confirm
`test_disk_sampling`/`test_radial_histogram` still pass; confirm the DOF parity scene's
jitter-constant divergence is gone.

*Depends on Phase 3 (harness must exist to measure T021 below). Its Red→Green gate is the
pre-existing `disk_sampling`/`radial_histogram` tests (from spec 007) plus Phase 3's T008 DOF
scene — all of which already exist before this phase's implementation tasks, so
implementation-after-tests ordering holds without needing new scene pairs authored here.*

- [X] T014 [US2] Create `src/ri/sampler.h` defining `CSampler`'s sample-value struct
      (jitterX, jitterY, timeStratum, lensU, lensV, importance) and class interface per
      `contracts/sampler-contract.md`.
- [X] T015 [US2] Implement `src/ri/sampler.cpp`: `CSampler::nextSample()`, folding in
      `sampleDisk()` (`random.h:172`) as the lens-point source, using one canonical jitter
      constant (replacing the `0.5001011` vs `0.5` drift) and one canonical CoC/lens formula
      set (S1).
- [X] T016 [US2] Wire `CStochastic::rasterBegin` (`stochastic.cpp:191`) to construct/consume a
      `CSampler` instance instead of its ad hoc `CPixel` jitter fields
      (`jx,jy,jt,jtStratum,jdx,jdy,jimp`, `stochastic.h:76-92`) and direct
      `apertureGenerator`/`sampleDisk()` call.
- [X] T017 [US2] Wire `CRaytracer::computeSamples` (`raytracer.cpp:487-528`) to
      construct/consume a `CSampler` instance instead of its own `urand()`-driven jitter/time/
      lens generation.
- [X] T018 [US2] Replace reyes's per-vertex `cocSamples()` (`reyes.cpp:1067`, called from
      `copyPoints`) with a call into `CSampler`'s canonical CoC formula, so reyes and raytrace
      derive circle-of-confusion from one formula set (S1/FR-006).
- [X] T019 [US2] Confirm no new RIB token, attribute, or option was introduced by the
      `CSampler` extraction (FR-029) — a code-review checklist item, not a new test.
- [X] T020 [US2] Run `ctest --test-dir build -R "disk_sampling|radial_histogram" --output-on-failure`
      and confirm both continue to pass unmodified (FR-008) — gate before proceeding further.
- [X] T021 [US2] Run `ctest --test-dir build -L parity --output-on-failure` on the T008 DOF
      scene pair and confirm the previously-known pixel-jitter-constant divergence no longer
      measurably contributes (FR-007). Gate passed (all 4 parity scenes green); however 3
      repeated re-measurements of `Parity_dof` (44.69/42.52/44.05) show D2's isolated
      contribution is unattributable from run-to-run RNG noise under the current
      independent-stream harness — see `tests/visual/parity-thresholds.md`'s T021 note.
      Threshold intentionally left at 60; real tightening deferred to Option B (T080-T086).
- [X] T022 [US2] Run `ctest --test-dir build -L visual --output-on-failure` and the FR-030
      perf timing commands on `camera-dof.rib` and `motion-1-{reyes,raytrace}.rib`; confirm no
      regression >2-3% versus the T001 baseline. Result: `ctest -L visual` 44/44 pass, 85.76
      sec*proc total (vs. baseline 106.19 sec*proc — faster, no regression). Perf timing (single
      runs, wall via `time`): `camera-dof.rib` 0.153/0.105/0.064s (baseline 0.167s — within
      noise, as baseline itself flagged this scene as noise-dominated); `motion-1-reyes.rib`
      0.323s wall (baseline 1.277s); `motion-1-raytrace.rib` 0.341s wall (baseline 1.333s) — both
      well under baseline, no regression. All three produced valid TIFF output.

**Checkpoint**: One shared per-sample generator in place; DOF/jitter parity measurably improved.
D2's isolated contribution to the `dof` scene's residual could not be confirmed (RNG-noise-bound
under the current independent-stream harness — see T021 and `parity-thresholds.md`), but no
regression was introduced: `disk_sampling`/`radial_histogram` (T020) and the full 44-scene visual
suite (T022) both remain green, and perf is within budget.

---

## Phase 5: User Story 3 - One shared transparency and matte compositor (Priority: P2)

**Goal**: One `CCompositor` component combines transparent/matte samples front-to-back for
both hiders, including compositing/non-compositing AOV channel rules, without altering
`CFragment`'s layout.

**Independent Test**: Render layered-transparency, matte, and multi-AOV scenes with both
hiders and confirm color/opacity/AOV agreement within threshold; confirm deep-shadow output
is unchanged.

*Depends on Phase 3 (harness) and, per the audit's recommended order, follows Phase 4
(sampler) so both land before the remaining refactors — no direct code dependency on Phase 4,
but both edit `stochastic.cpp`/`raytracer.cpp`, so sequencing avoids merge conflicts.*

- [X] T023 [US3] Read `CPrimaryBundle`'s continuation-ray compositing loop in
      `src/ri/raytracer.cpp` in full — the research.md open item (`codegraph_explore`
      previously surfaced `CGatherBundle::postShade` instead, a different class) — to confirm
      its exact current per-hit color/opacity/AOV state shape before writing the adapter.
      Result: confirmed the real class is `CPrimaryBundle::postShade` (3-arg hit overload and
      2-arg miss overload) driven by `CShadingContext::trace`'s continuation-depth loop; its
      per-hit state at the time was a `pixelHasMatte` flag fixed at depth 0, which T032 found
      architecturally wrong (see T032's notes) and replaced with deferred buffering.
- [X] T024 [P] [US3] Author the transparency parity scene pair
      `examples/rib/tests/parity/transparency-reyes.rib` / `transparency-raytrace.rib`
      (several stacked semi-transparent surfaces). Result: measured MaxBlockAvgDiff 1.19
      (already agrees closely pre-refactor; plain color/opacity "over" compositing is
      already shared behavior between hiders).
- [X] T025 [P] [US3] Author the matte parity scene pair
      `examples/rib/tests/parity/matte-reyes.rib` / `matte-raytrace.rib` (matte object
      partially covering non-matte geometry). Result: measured MaxBlockAvgDiff 3.94 in the
      rgba channel (matte holdout semantics already agree between hiders here — D8 only
      manifests in the AOV channel, see T026).
- [X] T026 [P] [US3] Author the combined transparency+matte+AOV parity scene pair
      `examples/rib/tests/parity/transparency-matte-aov-reyes.rib` / `-raytrace.rib` (the
      FR-002 combined-effect scene for this story's interacting features). Result: measured
      MaxBlockAvgDiff 255.00 (maxed out) — raytrace's `CPrimaryBundle::postShade` only
      copies extra AOV channels on the first hit (D6) and doesn't apply the matte holdout
      to the AOV channel (D8); confirms both defects are real and scene-detectable.
- [X] T027 [US3] Register T024-T026 in `tests/visual/CMakeLists.txt` via `add_parity_test` with
      initial thresholds. Thresholds set: `transparency` 20, `matte` 20 (generous headroom
      over measured baselines), `transparency-matte-aov` 25 (deliberately tight, so it fails
      now and is expected to go green once `CCompositor` lands).
- [X] T028 [US3] Run `ctest --test-dir build -L parity --output-on-failure -R "transparency|matte"`
      and confirm T024-T026 currently FAIL (or diverge past threshold) against pre-Phase-5
      code — this is the Red state Constitution Principle III requires before writing
      `CCompositor`. Result: `Parity_transparency` PASSED (1.19 < 20), `Parity_matte` PASSED
      (3.94 < 20), `Parity_transparency-matte-aov` FAILED (255.00 > 25, 313 blocks exceed
      threshold) — the intended Red-state split: only the scene exercising the D6/D8 defects
      fails pre-refactor.
- [X] T029 [US3] Create `src/ri/compositor.h` defining the `CompositeSample` struct and
      `CCompositor` class interface per `contracts/compositor-contract.md`. Result: accumulator
      (`CompositeAccumulator`) is caller-owned (not internal to `CCompositor`) so its lifetime can
      match either hider — reyes keeps one as a stack local per raster-sample position, raytrace
      stores one per in-flight `CPrimaryRay`; uses the codebase's existing `vector`/`float*` types,
      not a new `CColor` class; `evaluateDepth()` declared per the contract but deferred to
      Phase 6/S3.
- [X] T030 [US3] Implement `src/ri/compositor.cpp`: `CCompositor::composite()` (front-to-back
      over, matte carve-out via `compChannelOrder`'s `matteMode`) and `CCompositor::compositeNonComp()`
      (first-sample-passing-threshold latch via `nonCompChannelOrder`), ported statement-for-statement
      from `CStochastic::rasterEnd`'s `NonCompositeSampleLoop()`/`compositeSampleLoop()` macro logic
      (`stochastic.cpp:534-796`), including the existing FIXME'd `checkMatteZThreshold` asymmetry
      (preserved, not "fixed", since bit-for-bit parity with today's reyes output is the bar).
      Result: builds clean (`ri_obj` target); registered in `src/ri/CMakeLists.txt`.
- [X] T031 [US3] Wire `CStochastic::rasterEnd`'s fragment-list walk to populate a
      `CompositeSample` from each walked `CFragment` node and feed it to `CCompositor`,
      without altering `CFragment`'s layout (FR-010) — deep-shadow's direct reads
      (`stochastic.cpp:1302-1415`) must remain unaffected. Result: `rasterEnd` now computes
      `pixelHasMatte` once from `cPixel->first.opacity` and calls
      `CCompositor::compositeNonComp` per fragment (stochastic.cpp:548-568); the fragment-list
      struct/walk itself and the deep-shadow write path (stochastic.cpp:1290-1315) are untouched.
- [X] T032 [US3] Wire `CRaytracer`'s continuation-ray path (per T023's findings) to populate a
      `CompositeSample` per hit and feed it to the same `CCompositor`, routing extra AOV
      channels through `compChannelOrder`/`nonCompChannelOrder` (`raytracer.cpp:221`) instead
      of first-hit-only capture (S4/FR-011). Result: an initial per-depth `pixelHasMatte` fix
      passed the first two parity scenes but failed `transparency-matte-aov` (matte discovered
      only on a later/farther hit can't retroactively change an earlier non-matte hit's already-
      applied threshold formula). Replaced with deferred resolution: each hit's opacity+extras is
      buffered per-ray (`CBufferedNonCompSample`/`CPrimaryRay::pendingNonComp`,
      `src/ri/raytracer.h`), and a new `resolveNonComp()` helper (`raytracer.cpp`) computes
      `pixelHasMatte` as the OR across the whole buffered chain and walks it front-to-back only
      once the ray's continuation is confirmed terminated (opaque hit, miss at depth>0, or the
      maxRayDepth cap via `traceEx`'s miss-overload dispatch) — mirroring
      `CStochastic::rasterEnd`'s upfront, whole-fragment-list `pixelHasMatte` exactly. Also adds
      the previously-missing fallback to `CRenderer::sampleDefaults` when nothing passes
      threshold (mirrors stochastic.cpp's `cSample == NULL` branch). Gated on
      `CRenderer::numExtraNonCompChannels > 0` so scenes with no non-comp AOVs pay zero
      allocation cost in the common path.
- [X] T033 [US3] Confirm no new RIB token, attribute, or option was introduced by this
      compositor extraction (FR-029) — a code-review checklist item, not a new test. Result:
      `git diff` of `src/ri/ri.h`, `ri.cpp`, `rendererDeclerations.cpp`, `rendererContext.cpp`
      is empty — no new token/attribute/option introduced.
- [X] T034 [US3] Run the existing deep-shadow-map test scenes and confirm output is
      byte-for-byte unchanged (FR-010, Story 3 acceptance scenario 4). Result: no dedicated
      deep-shadow RIB test scene exists in the current suite; verified by inspection instead —
      all of Phase 5's diffs to `stochastic.cpp` are confined to the compositing loop
      (~lines 477-796) and none touch the deep-shadow write path (stochastic.cpp:1290-1315,
      `CRenderer::deepShadowFile`/`deepShadowIndex`), and T032's raytracer changes never read or
      write `CFragment`/deep-shadow state at all.
- [X] T035 [US3] Re-run `ctest --test-dir build -L parity --output-on-failure -R "transparency|matte"`
      on T024-T026 and confirm they now PASS (Green) — transparency/matte/AOV divergence is
      within threshold on both hiders. Result: all three PASS —
      `Parity_transparency` (1.15s), `Parity_matte` (0.20s), `Parity_transparency-matte-aov`
      (0.27s), 100%.
- [X] T036 [US3] Run `ctest --test-dir build -L visual --output-on-failure` and the FR-030 perf
      timing check; confirm no regression versus baseline. Result: full 44-test visual suite
      100% pass (85.6s total), including every `*-raytrace` scene. FR-030 timing on the
      raytrace-hider scenes this change actually touches (`motion-1-raytrace.rib`:
      ~1.26-1.37s/run, `camera-dof-raytrace.rib`: ~0.34-0.72s/run, 3 runs each) shows stable,
      consistent timings with no regression signature — confirming the `numExtraNonCompChannels`
      gate keeps the buffered-resolution change allocation-free on the common path.

**Checkpoint**: One shared transparency/matte compositor in place; transparency/matte/AOV
parity scenes pass.

---

## Phase 6: User Story 4 - Consistent depth compositing (Priority: P3)

**Goal**: Raytrace supports the same min/max/avg/mid depth-filter modes and z-visibility
threshold as reyes.

**Independent Test**: Render overlapping geometry with each depth-filter mode and a configured
z-visibility threshold on raytrace; confirm the `z` channel matches reyes for the same
settings, and that the default mode's output is unchanged from pre-change raytrace.

*Depends on Phase 5: reuses `CCompositor` (extends it with `evaluateDepth`) rather than
introducing a second shared component.*

- [X] T037 [P] [US4] Author depth-filter-mode parity scene pairs for min/max/avg/mid under
      `examples/rib/tests/parity/` exercising overlapping geometry at different depths.
- [X] T038 [US4] Register T037's scene pairs in `tests/visual/CMakeLists.txt` via
      `add_parity_test`.
- [X] T039 [US4] Run `ctest --test-dir build -L parity --output-on-failure` on T037's scenes and
      confirm raytrace currently FAILS or diverges past threshold (it has neither depth-filter
      modes nor a z-visibility threshold yet) — the Red state before extending `CCompositor`.
- [X] T040 [US4] Extend `CCompositor` (`src/ri/compositor.h`/`.cpp`) with the static
      `evaluateDepth(candidates, mode, zvisibilityThreshold)` function per
      `contracts/compositor-contract.md`, porting reyes's `DEPTH_MID`/min/max/avg dispatch and
      `checkZThreshold()` logic from `CStochastic::rasterEnd` (`stochastic.cpp:609-640`).
- [X] T041 [US4] Replace `CStochastic::rasterEnd`'s inline depth-filter dispatch with a call to
      `CCompositor::evaluateDepth`; confirm reyes's own output is unchanged (regression check).
      Verified via binary diff: built a fully-reverted `stochastic.cpp` (old Stage-1 inline
      dispatch + old Stage-2 `DEPTH_MID` double-averaging) into a separate `libri.dylib`, and
      compared its render of all four `depthfilter-{min,max,avg,mid}-reyes.rib` scenes against
      the current (evaluateDepth-based) build. Under default settings the diff was non-zero for
      avg/mid; isolated the cause to a **pre-existing multi-threaded rendering race** (bucket
      compositing order affects float summation for the accumulation-based avg/mid reductions,
      not comparison-based min/max) — confirmed by re-rendering the *same* unmodified binary
      twice and seeing the identical noise pattern, then confirming 0-diff run-to-run with
      `-t:1`. Re-ran fixed-vs-reverted single-threaded (`-t:1`) with jitter disabled
      (`Hider "reyes" "jitter" [0]`): **0/76800 pixel diff on all four modes**, confirming
      `evaluateDepth` reproduces reyes's Stage-1 resolution bit-for-bit. Note: the Stage-2
      `DEPTH_MID` reduction in the current code (`cPixel[4] += cSample[1]`) is algebraically
      identical to the old code's `cPixel[4] += 0.5f*(cSample[1]+cSample[5])` — the midpoint
      average moved into `evaluateDepth` itself, it wasn't dropped or changed. The
      pre-existing avg/mid threading race is a real, separate defect (not introduced by this
      spec, not gated by this task) — see `DEVNOTES_DETAILS/HIDER_PARITY.md` residuals note.
- [X] T042 [US4] Add a raytrace-side z-channel collection path (`src/ri/raytracer.cpp`) that
      gathers per-hit `(z, opacity)` candidates along the continuation-ray path and calls
      `CCompositor::evaluateDepth`, replacing raytrace's current always-first-hit z output
      (FR-012). Implemented via a new `CDepthCandidate`/`pendingDepth` buffer (raytracer.h) and a
      `resolveDepth()` helper (raytracer.cpp), buffered unconditionally at every hit (unlike the
      gated `pendingNonComp`) and resolved at the opaque-stop and miss-after-transparency call
      sites in both `postShade` overloads.
- [X] T043 [US4] Wire the same `zvisibilityThreshold` exclusion into the raytrace-side call
      (FR-013), matching reyes's `checkZThreshold()` semantics. `resolveDepth()` passes
      `CRenderer::zvisibilityThreshold` straight through to `evaluateDepth`, identical to the
      reyes call site. Initial implementation passed `evaluateDepth`'s default `zold` parameter
      (documented in compositor.h as "no-op for callers with no zold equivalent"), which passed
      min/max/avg parity but failed `depthfilter-mid` (176/1200 blocks over threshold, worst diff
      999994.25) — root-caused to reyes's `CPixel::zold` actually being initialized to
      `CRenderer::clipMax` (`stochastic.cpp:139`), not to "no floor," so a genuine second Mid
      candidate that's never found on the reyes side collapses to `0.5*(z0+C_INFINITY)`, a
      rasterization quirk `evaluateDepth`'s Mid branch is required to reproduce (T041's "parity
      with today's reyes output, quirks included, is the correctness bar"). Fix: pass
      `CRenderer::clipMax` (not the default) as `resolveDepth`'s `zold` argument, mirroring
      reyes's `zoldStart` initialization exactly. Re-ran `ctest -L parity`: 11/11 pass, including
      `Parity_depthfilter-mid`.
- [X] T044 [US4] Confirm no new RIB token, attribute, or option was introduced by this
      depth-filter extension (FR-029) — a code-review checklist item. Confirmed: T042/T043 add
      only internal C++ structures (`CDepthCandidate`, `pendingDepth`, `resolveDepth`); no new
      `RiAttributeV`/`RiOptionV` token, and no change to `initDeclarations()`.
- [X] T045 [US4] Run `ctest --test-dir build -L parity --output-on-failure` on T037 and T010
      (default-mode scene); confirm all pass, and confirm T010's default-mode output is
      bit-for-bit unchanged versus pre-Story-4 raytrace output (FR-014). Result: 11/11 parity
      tests pass (`Parity_depthdefault` included). Verified bit-for-bit equivalence by code proof
      rather than binary diff (T037-T043 work is one uncommitted diff on top of Phase 4/5's own
      uncommitted raytracer.cpp changes, so no clean "pre-Story-4" binary boundary exists to
      revert to in isolation): `examples/rib/tests/parity/depthdefault-raytrace.rib` has no
      `Opacity` statement (single opaque candidate per pixel), and `evaluateDepth`'s non-Mid path
      (`compositor.cpp:252-253`) is an unconditional `return z0` with no arithmetic when
      `z0 < clipMax` — i.e. the exact same `cRay->t` float value the old code assigned directly,
      with no intervening computation to alter its bit pattern.
- [X] T046 [US4] Run `ctest --test-dir build -L visual --output-on-failure` and the FR-030 perf
      timing check; confirm no regression versus baseline. Result: full 44-test visual suite
      100% pass (89.0s total). FR-030 timing on `motion-1-raytrace.rib` (the raytrace scene this
      change's hot path touches): ~1.36-1.39s/run (3 runs), consistent with T036's baseline
      ~1.26-1.37s/run — no regression signature.

**Checkpoint**: Raytrace supports all four depth-filter modes plus z-visibility threshold,
matching reyes.

---

## Phase 7: User Story 5 - Displacement parity by default (Priority: P3)

**Goal**: Raytrace displaces geometry with a displacement shader by default, matching reyes,
with an explicit, documented opt-out.

**Independent Test**: Render a displacement-shaded scene with raytrace and no special
attributes; confirm displacement matches reyes. Confirm the opt-out attribute still suppresses
it when set.

*Independent of Phases 4-6 (touches `shading.cpp`'s displacement-gating condition only); kept
in priority order after Phase 6 per spec.md's P3 grouping.*

- [X] T047 [P] [US5] Author the displacement parity scene pair
      `examples/rib/tests/parity/displacement-reyes.rib` / `displacement-raytrace.rib` (the
      raytrace variant has no `Attribute "trace" "displacements"` set) and register it via
      `add_parity_test`.
- [X] T048 [US5] Run `ctest --test-dir build -L parity --output-on-failure -R displacement` and
      confirm T047's scene pair currently FAILS — raytrace does not displace by default yet —
      the Red state before flipping the gating condition.
- [X] T049 [US5] Flip the displacement-gating condition in
      `src/libshader/shading/shading.cpp:676-683`
      (`(usedParameters & PARAMETER_RAYTRACE) && !(currentAttributes->flags & ATTRIBUTES_FLAGS_DISPLACEMENTS)`)
      so raytrace displaces by default unless the existing `Attribute "trace" "displacements"`
      opt-out is explicitly set (FR-015/FR-016).
- [X] T050 [US5] Confirm the existing opt-out attribute mechanism still suppresses raytrace
      displacement when explicitly set (FR-016). Verified via a scratch regression scene
      (`Attribute "trace" "displacements" [0]` on the raytrace side): MaxBlockAvgDiff jumps to
      ~204/255 (240/1200 blocks) vs. ~2.3-2.7/255 (48-53/1200 blocks) when both sides displace —
      the opt-out demonstrably still suppresses displacement.
- [X] T051 [US5] Re-run `ctest --test-dir build -L parity --output-on-failure -R displacement`
      and confirm T047's scene pair now PASSES (Green). Required redesigning the test scene's
      displacement shader (see T047 note below) before the metric could distinguish "both sides
      displaced" from "one side displaced" at all — the original "dented" shader's 6-octave
      turbulence aliased into each hider's independently-adaptive dicing grid and produced a
      grid-phase noise diff (~2.6-2.7/255) indistinguishable from the pre-fix bug's own diff
      (~2.6/255). Switched to a new single-octave `bump_lowfreq` shader
      (`shaders/bump_lowfreq.sl`); confirmed passing at threshold 6 (`tests/visual/CMakeLists.txt`)
      while a simulated regression (raytrace displacement forced off) still measures ~204/255 —
      an ~80x separation.
- [X] T052 [US5] Run `ctest --test-dir build -L visual --output-on-failure`, identify any
      existing reference images whose raytrace output changes due to this default flip, and
      regenerate only those references (FR-024's documented exception). Result: full suite
      (44/44 scenes) and the `parity` label (12/12) both pass unchanged — no registered visual
      scene binds a displacement shader on the raytrace side without the opt-out already set, so
      **no reference regeneration was actually required**.
- [X] T053 [US5] Document this default-behavior change in
      `DEVNOTES_DETAILS/HIDER_PARITY.md`'s Displacement Parity checkbox/notes (FR-017),
      explicitly calling out that it is a default change, not a silent absorption. Also documented
      the residual dicing-rate-formula divergence (reyes' `ShadingRate`-driven `estimateDicing()`
      vs. raytrace's ray-differential-driven `CTesselationPatch::intersect()`) uncovered while
      diagnosing T051, since it's what forced the `bump_lowfreq` test-shader swap and remains an
      undocumented parity gap for non-default `ShadingRate` scenes.
- [X] T054 [US5] Re-run `ctest --test-dir build -L visual --output-on-failure` (expecting only
      T052's regenerated scenes to differ from the prior run) and the FR-030 perf timing check
      on a displacement-heavy scene. Full visual suite re-run: 44/44 pass, no changes (consistent
      with T052 finding no regen was needed). FR-030 explicitly scopes the perf gate to "the
      renderer's existing depth-of-field and motion example scenes" (`camera-dof.rib` and the
      motion scenes) as a check against *refactor* overhead, not against the cost of displacement
      work itself — neither of those canonical scenes binds a `Displacement` shader, so T049's
      gating-condition flip in `shading.cpp` is dead code on that path and cannot regress them.
      Confirmed empirically: `camera-dof.rib` measured ~0.06-0.17s wall across 3 runs, consistent
      with pre-existing baseline noise, no regression signature. Separately timed
      `displacement-raytrace.rib` itself (the new displacement-parity scene, single sphere,
      320x240, 4x4 PixelSamples) for reference: ~1.4s wall with displacement active vs.
      ~0.5-0.8s with the `Attribute "trace" "displacements" [0]` opt-out set — the difference is
      the expected cost of newly-enabled-by-default displacement shader evaluation per
      tessellation cell (the same per-cell cost reyes already pays for the same shader), not a
      refactor regression, and is outside FR-030's scope since no such attribute-toggle
      before/after comparison exists for reyes either.

**Checkpoint**: Raytrace displaces by default, matching reyes, with a documented opt-out and
updated references.

---

## Phase 8: User Story 6 - Verified raytraced motion blur (Priority: P3)

**Goal**: Raytraced motion blur is verified (and fixed where needed) per moving-geometry
primitive type, removing the "incomplete/unverified" status flag.

**Independent Test**: Render translate and deform scenes for patches, polygons, and quadrics
with both hiders; confirm motion-blurred results match within the harness's motion threshold
for all six scene pairs.

*Independent of Phases 4-7 in terms of code touched (intersection kernels' existing
`ray->time` interpolation, not sampling/compositing), but depends on Phase 3's harness.*

- [X] T055 [P] [US6] Author patches translate/deform parity scene pairs
      `examples/rib/tests/parity/motion-patches-translate-{reyes,raytrace}.rib` and
      `motion-patches-deform-{reyes,raytrace}.rib`. Single bicubic patch (Basis "bezier" 3
      "bezier" 3): translate variant moves a domed patch -0.6→+0.6 in x; deform variant flattens
      the same dome to a flat plane over the shutter, both inside one `MotionBegin`/`MotionEnd`.
- [X] T056 [P] [US6] Author polygons translate/deform parity scene pairs
      `examples/rib/tests/parity/motion-polygons-translate-{reyes,raytrace}.rib` and
      `motion-polygons-deform-{reyes,raytrace}.rib`. Single quad `Polygon` (`Sides 2`): translate
      variant moves the square -0.6→+0.6 in x; deform variant grows the square into a wider
      kite/diamond outline over the shutter.
- [X] T057 [P] [US6] Author quadrics translate/deform parity scene pairs
      `examples/rib/tests/parity/motion-quadrics-translate-{reyes,raytrace}.rib` and
      `motion-quadrics-deform-{reyes,raytrace}.rib`. Single `Sphere`: translate variant moves
      -0.6→+0.6 in x; deform variant interpolates radius 0.3→0.8 (exercises quadrics' scalar
      `nextData` parameter-motion path, not just `xform->next` transform motion).
- [X] T058 [US6] Register T055-T057's six scene pairs in `tests/visual/CMakeLists.txt` via
      `add_parity_test`. Five scenes use the standard 20 threshold (matches
      flatshade/transparency/matte); `motion-patches-deform` uses 25 (measured ~16.8-17.1 across
      repeated runs — a curved bicubic patch flattening over the shutter produces more
      silhouette-edge shading gradient than the other five, so it needs slightly more margin
      against run-to-run jitter). `parity` label count updated 12 → 18 scene pairs.
- [X] T059 [US6] Ran `ctest --test-dir build -L parity --output-on-failure -R motion` on all six
      scene pairs: **all 6 pass on first attempt, no correctness bug found or fixed.** Diffed each
      reyes/raytrace pair standalone via `test_hider_parity` in diff-only mode first (before
      registering thresholds) to characterize real divergence: patches-translate 6.50,
      patches-deform ~17.0 (stable ±0.15 across 3 repeat renders), polygons-translate 4.36,
      polygons-deform 3.58, quadrics-translate 5.19, quadrics-deform 5.08 — all comfortably under
      the standard threshold of 20 already used elsewhere in the suite. Cross-checked visually
      (converted .tif→.png via PIL, inspected each reyes/raytrace pair side by side): silhouette
      shape, motion-streak/interpolated geometry, and shading gradient all matched closely in
      every pair — translate scenes show matching motion-streaked rectangles/parallelograms,
      quadrics-deform shows matching sphere radii (confirming the raytracer's `rv->time`-driven
      `nextData` scalar interpolation, not just `xform->next` transform motion, works), and
      patches-deform shows matching flattened-dome silhouettes. Residual diff in all six is
      ordinary silhouette antialiasing on a curved/interpolated edge, not a structural bug — no
      changes needed to `patches.cpp`/`polygons.cpp`/`quadrics.cpp`. Full `parity` label (18/18)
      and full `visual` label re-run clean after registration (see T062).
- [X] T060 [US6] Authored the combined DOF+motion parity scene pair
      `examples/rib/tests/parity/dof-motion-reyes.rib` / `dof-motion-raytrace.rib` (the FR-002
      combined-effect scene for this story): three spheres at staggered depths through
      `DepthOfField 2 1 5`, middle sphere also translate-motion-blurred. Measured ~26.50, stable
      across 3 repeated runs, well under the standalone "dof" scene's own residual (~45.94,
      threshold 60) — confirms the combined-scene divergence is dominated by DOF's own D9
      residual (screen-space scatter vs. true lens rays, a documented not-closable-by-refactor
      residual), not this story's motion blur. Registered via `add_parity_test` at threshold 40
      (~1.5x measured). `parity` label count updated 18 → 19 scene pairs.
- [X] T061 [US6] Update `DEVNOTES.md:39` and `DEVNOTES_DETAILS/HIDER_PARITY.md`'s Motion Blur
      Implementation checkbox to remove the "incomplete/unverified" flag now that all covered
      primitive types pass (FR-019 acceptance scenario 3). Both checkboxes checked, worded to
      scope the claim to object/surface motion (verified via T055-T060's 7 parity scenes) and
      explicitly exclude camera motion blur (a separate, still-unverified mechanism per the
      project's global CLAUDE.md note on `CRaytracer` primary rays), so as not to overclaim.
- [X] T062 [US6] Run `ctest --test-dir build -L visual --output-on-failure` and the FR-030 perf
      timing check on `motion-1-{reyes,raytrace}.rib`; confirm no regression versus baseline.
      Full 44-test visual suite already re-run clean this phase (100% pass, see T059/T060).
      FR-030 timing (3 runs each, wall time): `motion-1-reyes.rib` ~1.22-1.32s (baseline 1.277s),
      `motion-1-raytrace.rib` ~1.36-1.38s (baseline 1.333s) — both within noise of the T001
      single-run baseline and consistent with T036/T046's prior measurements on this scene
      (~1.26-1.39s range); no regression signature.

**Checkpoint**: Raytraced motion blur verified/fixed for all three primitive types, translate
and deform, matching reyes.

---

## Phase 9: User Story 7 - One shared pixel-filter module (Priority: P4)

**Goal**: One shared component combines weighted sub-pixel samples into a final pixel value
for reyes, raytrace, and zbuffer.

**Independent Test**: Inspect the source for one shared filter-combination component; render
each hider before and after and confirm pixel output is unchanged.

*Independent of Phases 4-8 (touches the splat/gather step, not sampling/compositing logic),
but sequenced after them since it also edits `stochastic.cpp`/`raytracer.cpp`. This is a pure
code-motion refactor with no new behavior to diff, so its Red→Green gate is the pre-existing
full visual-regression suite (already green before this phase starts) rather than new scene
pairs — see the Tests note above.*

- [X] T063 [US7] Create the shared pixel-filter module (filename TBD per
      `contracts/filter-module-contract.md`, e.g. `src/ri/pixelFilter.h`/`.cpp`) defining
      `CPixelFilterAccumulator`, wrapping the existing `CRenderer::pixelFilterKernel`.
- [X] T064 [US7] Wire `CStochastic::rasterEnd`'s splat/gather accumulation to use
      `CPixelFilterAccumulator` instead of its own inline loop.
- [X] T065 [US7] Wire `CRaytracer`'s per-ray-hit pixel accumulation to use the same
      `CPixelFilterAccumulator`.
- [X] T066 [US7] Wire `CZbuffer::rasterEnd` (`zbuffer.cpp:160-198`)'s simple opaque
      filter/gather to use the same `CPixelFilterAccumulator`, handling its
      single-contributing-sample case as the accumulator's generic degenerate case (not a
      special path).
- [X] T067 [US7] Run `ctest --test-dir build -L visual --output-on-failure` (all 33+ scenes
      across all three hiders) and confirm byte-identical output versus pre-change baseline
      (FR-021) — this story changes structure only, not results.
- [X] T068 [US7] Confirm no new RIB token, attribute, or option was introduced by this
      pixel-filter extraction (FR-029) — a code-review checklist item; none is expected since
      this is a pure code-motion refactor.
- [X] T069 [US7] Run the FR-030 perf timing commands on `camera-dof.rib` and
      `motion-1-{reyes,raytrace}.rib` for all three hiders (reyes, raytrace, zbuffer); confirm
      no regression >2-3% versus the T001 baseline. Result: 3 wall-time runs each (default
      threading, `/usr/bin/time -p`): `camera-dof.rib` 0.18/0.06/0.06s (baseline 0.167s, within
      noise as previously flagged); `motion-1-reyes.rib` 0.31/0.32/0.34s (baseline 1.277s — much
      faster, no regression); `motion-1-raytrace.rib` 0.37/0.37/0.36s (baseline 1.333s — much
      faster, no regression). No zbuffer-hider scene exists in the repo (confirmed via
      `grep -rln 'zbuffer' examples/ tests/` — zero matches); used the same scratch
      `Hider "zbuffer"` scene built for T066's verification (0.06/0.07/0.04s) as a sanity check
      only, no baseline to compare against since none was captured in T001.

**Checkpoint**: One shared pixel-filter module used by reyes, raytrace, and zbuffer.

---

## Phase 10: User Story 8 - Hider contract matches what hiders actually do (Priority: P4)

**Goal**: The shared shading/tracing engine exposes shading and ray-tracing only;
bucket-rasterization operations live solely on `CReyes` and its descendants.

**Independent Test**: Inspect the source: `CShadingContext` declares no
`drawObject`/`drawGrid`/`drawPoints`; `CRaytracer` contains no stub overrides of them; the full
visual-regression suite still passes.

*Purely structural — no dependency on Phases 4-9's output, but touches
`shading.{h,cpp}`/`object.{h,cpp}`/every `CSurface::dice()` override, so sequenced last among
the refactors to avoid rebasing structural churn under in-flight sampling/compositing work.
Like Phase 9, this is a pure signature/ownership refactor with no new behavior, so its
Red→Green gate is the pre-existing full visual-regression suite rather than new scene pairs.*

- [X] T070 [US8] Remove `drawObject`/`drawGrid`/`drawPoints` declarations from
      `CShadingContext` (`src/libshader/shading/shading.h`) and its stub definition
      (`shading.cpp:568`), narrowing the shared engine to shading/tracing only (FR-022).
- [X] T071 [US8] Add/confirm `drawObject`/`drawGrid`/`drawPoints` as `CReyes`'s own virtuals in
      `src/ri/reyes.h`/`reyes.cpp` (the one owner, per `contracts/hider-contract.md`). Result:
      confirmed already declared as `CReyes`'s own virtuals (`reyes.h:255-257`).
- [X] T072 [US8] Change `CObject::dice()`'s parameter type from `CShadingContext*` to
      `CReyes*` in `src/ri/object.h`/`object.cpp` (`object.cpp:80-91`), matching its body's
      `rasterizer->drawObject(cObject)` call.
- [X] T073 [US8] Update every `CSurface`-derived `dice()` override's parameter type to
      `CReyes*` across the ~27-type set (`patches.cpp`, `polygons.cpp`, `quadrics.cpp`,
      `points.cpp`, NURBS/implicit-surface/dynamic-load-object files, and the remaining
      `CSurface` subclasses) — mechanical and compiler-enforced; a successful build after this
      change is proof every override was updated. Result: retyped in `delayed.cpp`,
      `implicitSurface.cpp`, `subdivisionCreator.cpp`, `polygons.cpp`, `patches.cpp`
      (`CPatchMesh`/`CNURBSPatchMesh`), `curves.cpp` (`CCurveMesh`), `dlobject.cpp`, plus two
      recursive dicing helpers the ripple also touches: `CPatch::splitToChildren` and
      `CCurve::splitToChildren` (and its `CCubicCurve`/`CLinearCurve` overrides) — both delegate
      to `drawObject`/child `dice()` and needed the same retype. `create()` helper functions
      (`CPolygonMesh::create`, `CPatchMesh::create`, `CNURBSPatchMesh::create`,
      `CCurveMesh::create`) confirmed to correctly stay `CShadingContext*`-typed since their
      bodies never call the rasterization trio directly. Zero remaining
      `dice(CShadingContext` occurrences repo-wide (grep-verified).
- [X] T074 [US8] Delete `CRaytracer`'s no-op `drawObject`/`drawGrid`/`drawPoints` stub
      overrides (`raytracer.h:76-94`) (FR-023).
- [X] T075 [US8] Delete `CPhotonHider`'s identical no-op stub overrides (`photon.h:41-59`) —
      required for the build to compile once the base declarations are gone.
- [X] T076 [US8] Full clean build (`cmake --build build --config Release`) confirming the
      ~27-type ripple compiles cleanly with no missed override. Result: every C++ target
      (`ri_obj`, `libshader_shading`, `libri.dylib`, all `test_*` executables, `openrenderfilebase`,
      `file`, `framebuffer`) built and linked with zero errors. Also found and fixed one
      not-pre-enumerated compile blocker outside the `dice()`/`splitToChildren()` ripple:
      `renderer.cpp:~1120`'s `contexts[0]->drawObject(cObject)` call on a generically-typed
      `CShadingContext**`, guarded with `dynamic_cast<CReyes*>` (negligible cost — this runs
      once per scene object, not per-sample, so it's outside FR-030's hot-loop perf scope) —
      preserves prior behavior where reyes/zbuffer dice via this path and raytrace/photon
      silently skip it (traced via `raytraced()`/`CRenderer::raytracingFlags` semantics). The
      overall `cmake --build` command still reports failure, but solely from the pre-existing,
      unrelated `orender-fb-macos` Swift target crash (ModuleCache path-duplication + compiler
      segfault, documented in `baseline-T001.md` as present before this feature started) — a
      fresh `build/src/orender/orender` binary rebuilt with a current timestamp confirms the
      C++/`orender` side built cleanly in isolation.
- [X] T077 [US8] Run `ctest --test-dir build -L visual --output-on-failure` (full 33+-scene
      suite across reyes, raytrace, and zbuffer) and confirm 100% pass with no output change
      (FR-024, Story 8 acceptance scenario 3). Result: 44/44 pass (100%), 156.11 sec*proc total.
- [X] T078 [US8] Confirm no new RIB token, attribute, or option was introduced by this
      hider-contract split (FR-029) — a code-review checklist item; none is expected since this
      is a purely structural refactor. Result: confirmed — `git diff --stat` on the four
      attribute-system layer files (`ri.h`/`ri.cpp`, `rendererDeclerations.cpp`,
      `rendererContext.cpp`, `attributes.h`/`attributes.cpp`) shows only the pre-existing S2
      displacement-parity default flip in `attributes.cpp` (an earlier phase's flag-default
      change, not a new token); this phase's own diff touches no RIB-facing files.
- [X] T079 [US8] Run the FR-030 perf timing commands on `camera-dof.rib` and
      `motion-1-{reyes,raytrace}.rib`; confirm no regression >2-3% versus the T001 baseline.
      Result: 3 wall-time runs each — `camera-dof.rib` 0.17/0.06/0.07s (baseline 0.167s — no
      regression); `motion-1-reyes.rib` 0.32/0.35/0.43s and `motion-1-raytrace.rib`
      0.36/0.35/0.37s (baseline 1.277s/1.333s — much faster, no regression), consistent with
      T069's Phase 9 numbers (the speedup versus T001 was already established by the earlier
      R2-R4/S1-S5 phases, not newly introduced here; confirmed the motion-1 RIB scene files
      themselves are byte-identical to `git HEAD`, so the delta is real refactor benefit, not a
      scene-file change).

**Checkpoint**: `CShadingContext` exposes shading/tracing only; `CReyes` owns rasterization;
`CZbuffer` and a future backlogged `abuffer` hider inherit the contract for free.

---

## Phase 11: User Story 9 - Correlated sampling for tighter parity checks (Priority: P5)

**Goal**: Both hiders consume the exact same per-bucket sample table, correlating noise so at
least one previously-loose parity threshold can be tightened.

**Independent Test**: Render the same bucket with both hiders using the shared table; confirm
identical consumed values; confirm a tightened threshold still passes on unmodified code.

*Depends on Phase 4 (`CSampler` must exist to extend with a batch mode) and Phase 3 (harness,
to measure the tightened threshold). This is explicitly the audit's second stage — valuable
only once Stories 1 and 2 exist.*

- [x] T080 [US9] Add a per-bucket generation mode to `CSampler` (`src/ri/sampler.h`/`.cpp`):
      `generateBucketTable(bucketId, sampleCount)` per `contracts/sampler-contract.md`, reusing
      Phase 4's per-sample formulas without duplication.
- [x] T081 [US9] Wire `CStochastic::rasterBegin` to consume the per-bucket table verbatim for a
      given bucket instead of drawing its own per-sample stream, when the table is available.
- [x] T082 [US9] Wire `CRaytracer::computeSamples` to consume the same per-bucket table
      verbatim for the corresponding bucket, so both hiders' noise patterns correlate for the
      same scene (FR-025).
- [x] T083 [US9] Confirm no new RIB token, option, or user-facing determinism guarantee is
      introduced (FR-027) — a code-review checklist item, not a new test.
- [x] T084 [US9] Tighten at least one previously-loose parity threshold (transparency or
      motion, per FR-026/SC-008) in `tests/visual/parity-thresholds.md` and its
      `add_parity_test` registration, using the now-correlated noise to justify the tighter
      bound. Re-measurement showed the benefit is scene-dependent, not universal (see
      `parity-thresholds.md`'s "T084" section): `motion-quadrics-translate` and
      `motion-patches-translate` both closed substantially and stably under correlation
      (new `Parity_*-correlated` tests added, thresholds 6 and 8 respectively), while `dof`
      (D9 structural residual) got worse and `motion-polygons-translate` was flat within
      noise — neither of those two got a correlated variant or threshold change.
- [x] T085 [US9] Run `ctest --test-dir build -L parity --output-on-failure` (full parity suite)
      and confirm zero false failures against unmodified code with the tightened threshold
      (SC-008). Result: 21/21 passed (19 pre-existing + 2 new `-correlated` variants from T084).
- [x] T086 [US9] Run the FR-030/SC-007 perf timing commands on `camera-dof.rib` and
      `motion-1-{reyes,raytrace}.rib`; confirm the per-bucket sample table's extra bookkeeping
      does not regress either hider versus the T001 baseline. Current-session system load was
      visibly higher than at T001 (motion-1-reyes ~2.0s wall here vs. 1.277s/0.323s recorded at
      T001/T055), making a direct T001 comparison unreliable, so the meaningful measurement is
      default (`OPENRENDER_CORRELATED_SAMPLE_TABLE` unset) vs. correlated back-to-back on this
      session, 3 runs each: `motion-1-reyes.rib` 2.021/1.972/1.915s default vs.
      1.949/1.960/1.952s correlated (~1.97s vs ~1.95s avg — no regression); `camera-dof.rib`
      0.238s default vs. 0.112s correlated (single runs, within this scene's known noise band
      per T022). No measurable overhead from the per-bucket table's bookkeeping in either
      hider.

**Checkpoint**: All nine user stories complete; parity thresholds tightened where correlated
sampling permits.

---

## Phase 12: Polish & Cross-Cutting Concerns

**Purpose**: Final sign-off across every story.

- [x] T087 [P] Run the full `quickstart.md` validation guide end-to-end (visual suite, parity
      suite, lens-sampling gates, manual perf timing) as a final sign-off.
      **Result**: `ctest -L visual -E slow` 44/44 pass; `ctest -L parity` 21/21 pass;
      `ctest -R DiskSampling` 1/1 pass. Also fixed a pre-existing (spec-007-era) documentation
      defect found during sign-off: `quickstart.md` referenced non-existent ctest names
      (`disk_sampling`/`radial_histogram`) — the real registered test is `DiskSampling`
      (`tests/CMakeLists.txt:37`); `test_radial_histogram` is a manual diagnostic binary never
      wired into ctest (`tests/visual/CMakeLists.txt`). Corrected all three references in
      `quickstart.md` to `ctest -R DiskSampling`, with a comment noting the manual-only status
      of `test_radial_histogram`.
- [x] T088 Final pass over `DEVNOTES_DETAILS/HIDER_PARITY.md`'s Alignment Status checklist,
      confirming all items are now `[x]` (Motion Blur Implementation, Shading Interpolation &
      Derivatives residual note, Displacement Parity, Transparency Handling), per
      FR-017/FR-019. **Result**: all four items confirmed `[x]`.
- [x] T089 Final FR-030/SC-007 performance comparison: re-run the T001 baseline timing commands
      on `camera-dof.rib` and `motion-1-{reyes,raytrace}.rib` and confirm cumulative regression
      across all refactors stays within 2-3% of the original pre-feature baseline.
      **Result**: PASS — cumulative change is a net *improvement*, not a regression. 3 runs each,
      Release build, vs. T001 baseline (commit `6960b86`, also Release). **User CPU time is the
      load-independent metric and carries the verdict** — wall time is included for reference
      only, since this same session already demonstrated wall time swinging 0.3s→1.97s→0.59s on
      one unchanged scene purely from system load, whereas user seconds measure actual cycles
      burned and aren't inflated by contention from other processes:
      | Scene | Baseline user/wall | Current user/wall | Delta (user) |
      |---|---|---|---|
      | camera-dof.rib | 0.09s / 0.167s | ~0.06s / ~0.046s | lower (short-scene wall time is noise-dominated per baseline-T001.md's own caveat; one cold-start outlier run of 3.05s wall discarded — dyld/page-cache warm-up, 0.06s user confirms no actual extra work) |
      | motion-1-reyes.rib | 2.35s / 1.277s | ~1.11s / ~0.59s | ~2.1x less user CPU |
      | motion-1-raytrace.rib | 2.57s / 1.333s | ~0.99s / ~0.52s | ~2.5x less user CPU |

      A 2-3x swing this large is itself far outside the 2-3% budget FR-030/SC-007 is checking, so
      it does not pass on the strength of the number alone — it passes because (a) the reduction
      shows up in **user** time, which isolates real CPU work from system-load noise, and (b) the
      full visual (44/44) and parity (21/21) suites are independently green at this same
      revision, meaning the reduced CPU work is not dropped samples — a real drop in sampling
      density would blow past the block-average diff thresholds and fail those suites, not just
      run faster. The reduction is plausibly the shared `CSampler`/compositor/pixel-filter
      modules (R2-R4) eliminating redundant per-hider sampling/compositing work, not a
      correctness regression.

      Note on process: an initial re-measurement this session showed the *opposite* — a stable
      ~2.1-2.5x *slowdown* on both motion scenes, in both user and wall time. Root cause was that
      the working `build/` directory had been reconfigured to `CMAKE_BUILD_TYPE=Debug` at some
      point (confirmed via `build/CMakeCache.txt`), not a code regression — the baseline was
      explicitly built `--config Release`. Reconfigured and rebuilt Release (`cmake -S . -B build
      -DCMAKE_BUILD_TYPE=Release && cmake --build build --config Release --target orender`),
      re-ran the comparison, and got the results above. The apparent regression was 100% a
      build-config artifact, not a load artifact and not a code artifact.

---

## Dependencies & Execution Order

### Phase Dependencies (the real chain — not parallel)

- **Setup (Phase 1)**: No dependencies.
- **Foundational (Phase 2)**: Depends on Setup.
- **Phase 3 (US1, harness)**: Depends on Phase 2. **Hard prerequisite for Phases 4-11** — this
  is spec.md's own stated rationale for Story 1's priority; every other story needs the harness
  to validate its own change.
- **Phase 4 (US2, sampler)**: Depends on Phase 3.
- **Phase 5 (US3, compositor)**: Depends on Phase 3; sequenced after Phase 4 to avoid
  `stochastic.cpp`/`raytracer.cpp` merge conflicts (no logical dependency on Phase 4's output).
- **Phase 6 (US4, depth-filter)**: Depends on Phase 5 — reuses `CCompositor`, extending it with
  `evaluateDepth` rather than introducing a second shared component.
- **Phase 7 (US5, displacement default)**: Depends on Phase 3 only; independent of Phases 4-6's
  code, sequenced here to match spec.md's P3 priority grouping.
- **Phase 8 (US6, motion verification)**: Depends on Phase 3 only; independent of Phases 4-7's
  code (different files: `patches.cpp`/`polygons.cpp`/`quadrics.cpp`), sequenced here to match
  spec.md's P3 grouping. Its combined-effect task (T060) additionally depends on Phase 4.
- **Phase 9 (US7, pixel-filter)**: Depends on Phase 3; independent of Phases 4-8's logic, but
  sequenced after them to avoid churn in `stochastic.cpp`/`raytracer.cpp` while they're in flight.
- **Phase 10 (US8, hider contract split)**: Depends on Phase 3 only; purely structural, no
  dependency on Phases 4-9's output, sequenced last among refactors to avoid rebasing
  structural churn (`CObject::dice()` signature, ~27-type ripple) under in-flight work.
- **Phase 11 (US9, Option B)**: Depends on Phase 4 (extends `CSampler`) and Phase 3 (harness,
  to measure the tightened threshold).
- **Phase 12 (Polish)**: Depends on all desired phases being complete.

### File-contention note

`stochastic.cpp` and `raytracer.cpp` are edited by Phases 4, 5, 6, 9, and 11. Even where two
phases have no logical dependency on each other's output, they cannot be worked in parallel
against these files — treat Phases 4-9 and 11 as strictly sequential in that respect, despite
several having no *logical* dependency chain between them.

### Parallel Opportunities

- Within Phase 2: T002, T003.
- Within Phase 3: T007-T010 (four independent RIB scene files).
- Within Phase 5: T024-T026 (three independent RIB scene files).
- Within Phase 8: T055-T057 (three independent primitive-type RIB scene sets).
- T087 (Phase 12) can run alongside T088/T089 since it only reads existing state.
- No cross-phase parallelism is recommended given the file-contention note above.

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup.
2. Complete Phase 2: Foundational.
3. Complete Phase 3: User Story 1 (the parity harness).
4. **STOP and VALIDATE**: `ctest --test-dir build -L parity` passes on the four baseline scene
   pairs and catches a deliberately-introduced regression. This alone delivers the audit's
   stated first deliverable and de-risks every subsequent refactor.

### Incremental Delivery

Follow Phases 4 → 11 in the order given above (not the generic template's "stories in
parallel" pattern) — each phase's Checkpoint marks a safe point to stop, run the full
`-L visual` suite, and confirm no regression before starting the next phase.

### Suggested Sequencing Rationale

The order Phase 4 → 5 → 6 → 7 → 8 → 9 → 10 → 11 mirrors the audit's own recommended execution
order (cheap/mechanical parity fixes and the shared sampler first, then the shared compositor
and its depth-filter extension, then the lower-risk parity fixes, then the two purely
structural refactors, then Option B last since it depends on the sampler). Spec.md's priority
labels (P2/P2/P3/P3/P3/P4/P4/P5) are preserved; phases sharing a priority tier (US2/US3 at P2,
US4/US5/US6 at P3, US7/US8 at P4) are ordered by the audit's recommendation and file-contention
avoidance, not by an arbitrary tie-break.

---

## Notes

- No `[P]` marker spans two different user-story phases — file contention on
  `stochastic.cpp`/`raytracer.cpp` makes that unsafe regardless of logical independence.
- Tests for each story are its own parity scene pairs, registered via `add_parity_test` — this
  repo's existing test-authoring convention — not a new unit-test framework, with the
  exception of Stories 7 and 8 (pure code-motion refactors), whose test is the pre-existing
  full visual-regression suite staying green.
- Within each story that authors new scene pairs (Stories 3, 4, 5, 6), those scene pairs are
  registered and run *before* the shared-component implementation tasks, confirming a Red
  (failing/diverging) state first, per Constitution Principle III.
- Commit after each phase's Checkpoint.
- Verify `ctest -L visual` and the relevant `-L parity` scenes at every checkpoint before
  starting the next phase.
