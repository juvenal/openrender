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
and every subsequent story adding scene pairs that are expected to fail (Red) against
unmodified code and pass (Green) once that story's fix lands.

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

- [ ] T001 Build the renderer (`cmake --build build --config Release`) and run the existing
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

- [ ] T002 [P] Create the `examples/rib/tests/parity/` directory that will hold every new
      cross-hider scene pair added across Phases 3-11.
- [ ] T003 [P] Read `DEVNOTES_DETAILS/HIDER_PARITY.md`'s current Alignment Status checklist in
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
unmodified code (flat shading; depth of field, since D1's `sampleDisk()` fix predates this
feature; AOV capture without transparency; the raytrace depth-filter's current default mode).
Effects that are known to diverge until a later story's fix lands (transparency, matte, all
depth-filter non-default modes, motion blur, displacement) get their scene pairs added within
the story that fixes them, in Phases 5-8, so each story's Red→Green transition stays honest.

- [ ] T004 [US1] Duplicate `test_visual_render.cpp`'s `TiffImage`/`readTiff`/`compareTiffs`
      block-average diff code into a new `tests/visual/test_hider_parity.cpp`, per
      research.md's decision to follow the same duplication-over-shared-header convention
      `test_radial_histogram.cpp` already uses.
- [ ] T005 [US1] Extend `test_hider_parity.cpp`'s `main()` to accept two
      `(orender_path, rib_path, output_tif_name)` triples, run `orender` twice, and diff the
      two fresh outputs against each other with a per-scene threshold argument (candidate vs.
      candidate, not candidate vs. static reference).
- [ ] T006 [US1] Add an `add_parity_test(SCENE_NAME RIB_A OUTPUT_A RIB_B OUTPUT_B [THRESHOLD])`
      CMake macro to `tests/visual/CMakeLists.txt`, modeled on the existing `add_visual_test`
      macro (same scratch-dir pattern, `VISUAL_ENV`, `TIMEOUT 360`), adding a `parity` ctest
      label alongside `visual;regression`.
- [ ] T007 [P] [US1] Author the flat-shading parity scene pair
      `examples/rib/tests/parity/flatshade-reyes.rib` / `flatshade-raytrace.rib`.
- [ ] T008 [P] [US1] Author the depth-of-field parity scene pair
      `examples/rib/tests/parity/dof-reyes.rib` / `dof-raytrace.rib`.
- [ ] T009 [P] [US1] Author the AOV-without-transparency parity scene pair
      `examples/rib/tests/parity/aov-reyes.rib` / `aov-raytrace.rib`.
- [ ] T010 [P] [US1] Author the depth-filter-default-mode parity scene pair
      `examples/rib/tests/parity/depthdefault-reyes.rib` / `depthdefault-raytrace.rib`.
- [ ] T011 [US1] Register T007-T010's scene pairs in `tests/visual/CMakeLists.txt` via
      `add_parity_test`, each tagged with its effect's initial threshold
      (data-model.md Entity 2: Per-effect parity threshold).
- [ ] T012 [US1] Document each effect's threshold and rationale in
      `tests/visual/parity-thresholds.md`, including the two documented residuals from
      data-model.md Entity 8 (shading-interpolation, DOF-occlusion-model) with their bounding
      thresholds (FR-028).
- [ ] T013 [US1] Run `ctest --test-dir build -L parity --output-on-failure`, confirm T007-T010
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

*Depends on Phase 3 (harness must exist to measure T020 below).*

- [ ] T014 [US2] Create `src/ri/sampler.h` defining `CSampler`'s sample-value struct
      (jitterX, jitterY, timeStratum, lensU, lensV, importance) and class interface per
      `contracts/sampler-contract.md`.
- [ ] T015 [US2] Implement `src/ri/sampler.cpp`: `CSampler::nextSample()`, folding in
      `sampleDisk()` (`random.h:172`) as the lens-point source, using one canonical jitter
      constant (replacing the `0.5001011` vs `0.5` drift) and one canonical CoC/lens formula
      set (S1).
- [ ] T016 [US2] Wire `CStochastic::rasterBegin` (`stochastic.cpp:191`) to construct/consume a
      `CSampler` instance instead of its ad hoc `CPixel` jitter fields
      (`jx,jy,jt,jtStratum,jdx,jdy,jimp`, `stochastic.h:76-92`) and direct
      `apertureGenerator`/`sampleDisk()` call.
- [ ] T017 [US2] Wire `CRaytracer::computeSamples` (`raytracer.cpp:487-528`) to
      construct/consume a `CSampler` instance instead of its own `urand()`-driven jitter/time/
      lens generation.
- [ ] T018 [US2] Replace reyes's per-vertex `cocSamples()` (`reyes.cpp:1067`, called from
      `copyPoints`) with a call into `CSampler`'s canonical CoC formula, so reyes and raytrace
      derive circle-of-confusion from one formula set (S1/FR-006).
- [ ] T019 [US2] Run `ctest --test-dir build -R "disk_sampling|radial_histogram" --output-on-failure`
      and confirm both continue to pass unmodified (FR-008) — gate before proceeding further.
- [ ] T020 [US2] Run `ctest --test-dir build -L parity --output-on-failure` on the T008 DOF
      scene pair and confirm the previously-known pixel-jitter-constant divergence no longer
      measurably contributes (FR-007).
- [ ] T021 [US2] Run `ctest --test-dir build -L visual --output-on-failure` and the FR-030
      perf timing commands on `camera-dof.rib` and `motion-1-{reyes,raytrace}.rib`; confirm no
      regression >2-3% versus the T001 baseline.

**Checkpoint**: One shared per-sample generator in place; DOF/jitter parity measurably improved.

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

- [ ] T022 [US3] Read `CPrimaryBundle`'s continuation-ray compositing loop in
      `src/ri/raytracer.cpp` in full — the research.md open item (`codegraph_explore`
      previously surfaced `CGatherBundle::postShade` instead, a different class) — to confirm
      its exact current per-hit color/opacity/AOV state shape before writing the adapter.
- [ ] T023 [US3] Create `src/ri/compositor.h` defining the `CompositeSample` struct and
      `CCompositor` class interface per `contracts/compositor-contract.md`.
- [ ] T024 [US3] Implement `src/ri/compositor.cpp`: `CCompositor::composite()` (front-to-back
      over, opacity threshold, matte carve-out, `compChannelOrder`/`nonCompChannelOrder` AOV
      rules), ported from `CStochastic::rasterEnd`'s `NonCompositeSampleLoop()`/
      `compositeSampleLoop()` macro logic (`stochastic.cpp:445-714`).
- [ ] T025 [US3] Wire `CStochastic::rasterEnd`'s fragment-list walk to populate a
      `CompositeSample` from each walked `CFragment` node and feed it to `CCompositor`,
      without altering `CFragment`'s layout (FR-010) — deep-shadow's direct reads
      (`stochastic.cpp:1302-1415`) must remain unaffected.
- [ ] T026 [US3] Wire `CRaytracer`'s continuation-ray path (per T022's findings) to populate a
      `CompositeSample` per hit and feed it to the same `CCompositor`, routing extra AOV
      channels through `compChannelOrder`/`nonCompChannelOrder` (`raytracer.cpp:221`) instead
      of first-hit-only capture (S4/FR-011).
- [ ] T027 [P] [US3] Author the transparency parity scene pair
      `examples/rib/tests/parity/transparency-reyes.rib` / `transparency-raytrace.rib`
      (several stacked semi-transparent surfaces).
- [ ] T028 [P] [US3] Author the matte parity scene pair
      `examples/rib/tests/parity/matte-reyes.rib` / `matte-raytrace.rib` (matte object
      partially covering non-matte geometry).
- [ ] T029 [P] [US3] Author the combined transparency+matte+AOV parity scene pair
      `examples/rib/tests/parity/transparency-matte-aov-reyes.rib` / `-raytrace.rib` (the
      FR-002 combined-effect scene for this story's interacting features).
- [ ] T030 [US3] Register T027-T029 in `tests/visual/CMakeLists.txt` via `add_parity_test` with
      initial thresholds.
- [ ] T031 [US3] Run the existing deep-shadow-map test scenes and confirm output is
      byte-for-byte unchanged (FR-010, Story 3 acceptance scenario 4).
- [ ] T032 [US3] Run `ctest --test-dir build -L parity --output-on-failure` on T027-T029 and
      confirm transparency/matte/AOV divergence is within threshold on both hiders.
- [ ] T033 [US3] Run `ctest --test-dir build -L visual --output-on-failure` and the FR-030 perf
      timing check; confirm no regression versus baseline.

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

- [ ] T034 [US4] Extend `CCompositor` (`src/ri/compositor.h`/`.cpp`) with the static
      `evaluateDepth(candidates, mode, zvisibilityThreshold)` function per
      `contracts/compositor-contract.md`, porting reyes's `DEPTH_MID`/min/max/avg dispatch and
      `checkZThreshold()` logic from `CStochastic::rasterEnd` (`stochastic.cpp:609-640`).
- [ ] T035 [US4] Replace `CStochastic::rasterEnd`'s inline depth-filter dispatch with a call to
      `CCompositor::evaluateDepth`; confirm reyes's own output is unchanged (regression check).
- [ ] T036 [US4] Add a raytrace-side z-channel collection path (`src/ri/raytracer.cpp`) that
      gathers per-hit `(z, opacity)` candidates along the continuation-ray path and calls
      `CCompositor::evaluateDepth`, replacing raytrace's current always-first-hit z output
      (FR-012).
- [ ] T037 [US4] Wire the same `zvisibilityThreshold` exclusion into the raytrace-side call
      (FR-013), matching reyes's `checkZThreshold()` semantics.
- [ ] T038 [P] [US4] Author depth-filter-mode parity scene pairs for min/max/avg/mid under
      `examples/rib/tests/parity/` exercising overlapping geometry at different depths.
- [ ] T039 [US4] Register T038's scene pairs in `tests/visual/CMakeLists.txt` via
      `add_parity_test`.
- [ ] T040 [US4] Run `ctest --test-dir build -L parity --output-on-failure` on T038 and T010
      (default-mode scene); confirm all pass, and confirm T010's default-mode output is
      bit-for-bit unchanged versus pre-Story-4 raytrace output (FR-014).
- [ ] T041 [US4] Run `ctest --test-dir build -L visual --output-on-failure` and the FR-030 perf
      timing check; confirm no regression versus baseline.

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

- [ ] T042 [US5] Flip the displacement-gating condition in
      `src/libshader/shading/shading.cpp:676-683`
      (`(usedParameters & PARAMETER_RAYTRACE) && !(currentAttributes->flags & ATTRIBUTES_FLAGS_DISPLACEMENTS)`)
      so raytrace displaces by default unless the existing `Attribute "trace" "displacements"`
      opt-out is explicitly set (FR-015/FR-016).
- [ ] T043 [US5] Confirm the existing opt-out attribute mechanism still suppresses raytrace
      displacement when explicitly set (FR-016).
- [ ] T044 [P] [US5] Author the displacement parity scene pair
      `examples/rib/tests/parity/displacement-reyes.rib` / `displacement-raytrace.rib` (the
      raytrace variant has no `Attribute "trace" "displacements"` set) and register it via
      `add_parity_test`.
- [ ] T045 [US5] Run `ctest --test-dir build -L visual --output-on-failure`, identify any
      existing reference images whose raytrace output changes due to this default flip, and
      regenerate only those references (FR-024's documented exception).
- [ ] T046 [US5] Document this default-behavior change in
      `DEVNOTES_DETAILS/HIDER_PARITY.md`'s Displacement Parity checkbox/notes (FR-017),
      explicitly calling out that it is a default change, not a silent absorption.
- [ ] T047 [US5] Re-run `ctest --test-dir build -L visual --output-on-failure` (expecting only
      T045's regenerated scenes to differ from the prior run) and the FR-030 perf timing check
      on a displacement-heavy scene.

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

- [ ] T048 [P] [US6] Author patches translate/deform parity scene pairs
      `examples/rib/tests/parity/motion-patches-translate-{reyes,raytrace}.rib` and
      `motion-patches-deform-{reyes,raytrace}.rib`.
- [ ] T049 [P] [US6] Author polygons translate/deform parity scene pairs
      `examples/rib/tests/parity/motion-polygons-translate-{reyes,raytrace}.rib` and
      `motion-polygons-deform-{reyes,raytrace}.rib`.
- [ ] T050 [P] [US6] Author quadrics translate/deform parity scene pairs
      `examples/rib/tests/parity/motion-quadrics-translate-{reyes,raytrace}.rib` and
      `motion-quadrics-deform-{reyes,raytrace}.rib`.
- [ ] T051 [US6] Register T048-T050's six scene pairs in `tests/visual/CMakeLists.txt` via
      `add_parity_test` with motion-effect thresholds.
- [ ] T052 [US6] Run `ctest --test-dir build -L parity --output-on-failure -R motion` on all
      six scene pairs; for each primitive type that fails, investigate and fix the correctness
      bug in that primitive's ray-time interpolation path (`patches.cpp`, `polygons.cpp`, or
      `quadrics.cpp` respectively, per FR-019) — repeat until all six pass.
- [ ] T053 [US6] Author the combined DOF+motion parity scene pair
      `examples/rib/tests/parity/dof-motion-reyes.rib` / `dof-motion-raytrace.rib` (the FR-002
      combined-effect scene for this story, exercising Phase 4's converged lens sampling
      together with this story's converged motion blur) and register it via `add_parity_test`.
- [ ] T054 [US6] Update `DEVNOTES.md:39` and `DEVNOTES_DETAILS/HIDER_PARITY.md`'s Motion Blur
      Implementation checkbox to remove the "incomplete/unverified" flag now that all covered
      primitive types pass (FR-019 acceptance scenario 3).
- [ ] T055 [US6] Run `ctest --test-dir build -L visual --output-on-failure` and the FR-030 perf
      timing check on `motion-1-{reyes,raytrace}.rib`; confirm no regression versus baseline.

**Checkpoint**: Raytraced motion blur verified/fixed for all three primitive types, translate
and deform, matching reyes.

---

## Phase 9: User Story 7 - One shared pixel-filter module (Priority: P4)

**Goal**: One shared component combines weighted sub-pixel samples into a final pixel value
for reyes, raytrace, and zbuffer.

**Independent Test**: Inspect the source for one shared filter-combination component; render
each hider before and after and confirm pixel output is unchanged.

*Independent of Phases 4-8 (touches the splat/gather step, not sampling/compositing logic),
but sequenced after them since it also edits `stochastic.cpp`/`raytracer.cpp`.*

- [ ] T056 [US7] Create the shared pixel-filter module (filename TBD per
      `contracts/filter-module-contract.md`, e.g. `src/ri/pixelFilter.h`/`.cpp`) defining
      `CPixelFilterAccumulator`, wrapping the existing `CRenderer::pixelFilterKernel`.
- [ ] T057 [US7] Wire `CStochastic::rasterEnd`'s splat/gather accumulation to use
      `CPixelFilterAccumulator` instead of its own inline loop.
- [ ] T058 [US7] Wire `CRaytracer`'s per-ray-hit pixel accumulation to use the same
      `CPixelFilterAccumulator`.
- [ ] T059 [US7] Wire `CZbuffer::rasterEnd` (`zbuffer.cpp:160-198`)'s simple opaque
      filter/gather to use the same `CPixelFilterAccumulator`, handling its
      single-contributing-sample case as the accumulator's generic degenerate case (not a
      special path).
- [ ] T060 [US7] Run `ctest --test-dir build -L visual --output-on-failure` (all 33+ scenes
      across all three hiders) and confirm byte-identical output versus pre-change baseline
      (FR-021) — this story changes structure only, not results.

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
the refactors to avoid rebasing structural churn under in-flight sampling/compositing work.*

- [ ] T061 [US8] Remove `drawObject`/`drawGrid`/`drawPoints` declarations from
      `CShadingContext` (`src/libshader/shading/shading.h`) and its stub definition
      (`shading.cpp:568`), narrowing the shared engine to shading/tracing only (FR-022).
- [ ] T062 [US8] Add/confirm `drawObject`/`drawGrid`/`drawPoints` as `CReyes`'s own virtuals in
      `src/ri/reyes.h`/`reyes.cpp` (the one owner, per `contracts/hider-contract.md`).
- [ ] T063 [US8] Change `CObject::dice()`'s parameter type from `CShadingContext*` to
      `CReyes*` in `src/ri/object.h`/`object.cpp` (`object.cpp:80-91`), matching its body's
      `rasterizer->drawObject(cObject)` call.
- [ ] T064 [US8] Update every `CSurface`-derived `dice()` override's parameter type to
      `CReyes*` across the ~27-type set (`patches.cpp`, `polygons.cpp`, `quadrics.cpp`,
      `points.cpp`, NURBS/implicit-surface/dynamic-load-object files, and the remaining
      `CSurface` subclasses) — mechanical and compiler-enforced; a successful build after this
      change is proof every override was updated.
- [ ] T065 [US8] Delete `CRaytracer`'s no-op `drawObject`/`drawGrid`/`drawPoints` stub
      overrides (`raytracer.h:76-94`) (FR-023).
- [ ] T066 [US8] Delete `CPhotonHider`'s identical no-op stub overrides (`photon.h:41-59`) —
      required for the build to compile once the base declarations are gone.
- [ ] T067 [US8] Full clean build (`cmake --build build --config Release`) confirming the
      ~27-type ripple compiles cleanly with no missed override.
- [ ] T068 [US8] Run `ctest --test-dir build -L visual --output-on-failure` (full 33+-scene
      suite across reyes, raytrace, and zbuffer) and confirm 100% pass with no output change
      (FR-024, Story 8 acceptance scenario 3).

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

- [ ] T069 [US9] Add a per-bucket generation mode to `CSampler` (`src/ri/sampler.h`/`.cpp`):
      `generateBucketTable(bucketId, sampleCount)` per `contracts/sampler-contract.md`, reusing
      Phase 4's per-sample formulas without duplication.
- [ ] T070 [US9] Wire `CStochastic::rasterBegin` to consume the per-bucket table verbatim for a
      given bucket instead of drawing its own per-sample stream, when the table is available.
- [ ] T071 [US9] Wire `CRaytracer::computeSamples` to consume the same per-bucket table
      verbatim for the corresponding bucket, so both hiders' noise patterns correlate for the
      same scene (FR-025).
- [ ] T072 [US9] Confirm no new RIB token, option, or user-facing determinism guarantee is
      introduced (FR-027) — a code-review checklist item, not a new test.
- [ ] T073 [US9] Tighten at least one previously-loose parity threshold (transparency or
      motion, per FR-026/SC-008) in `tests/visual/parity-thresholds.md` and its
      `add_parity_test` registration, using the now-correlated noise to justify the tighter
      bound.
- [ ] T074 [US9] Run `ctest --test-dir build -L parity --output-on-failure` (full parity suite)
      and confirm zero false failures against unmodified code with the tightened threshold
      (SC-008).

**Checkpoint**: All nine user stories complete; parity thresholds tightened where correlated
sampling permits.

---

## Phase 12: Polish & Cross-Cutting Concerns

**Purpose**: Final sign-off across every story.

- [ ] T075 [P] Run the full `quickstart.md` validation guide end-to-end (visual suite, parity
      suite, lens-sampling gates, manual perf timing) as a final sign-off.
- [ ] T076 Final pass over `DEVNOTES_DETAILS/HIDER_PARITY.md`'s Alignment Status checklist,
      confirming all items are now `[x]` (Motion Blur Implementation, Shading Interpolation &
      Derivatives residual note, Displacement Parity, Transparency Handling), per
      FR-017/FR-019.
- [ ] T077 Final FR-030/SC-007 performance comparison: re-run the T001 baseline timing commands
      on `camera-dof.rib` and `motion-1-{reyes,raytrace}.rib` and confirm cumulative regression
      across all refactors stays within 2-3% of the original pre-feature baseline.

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
  spec.md's P3 grouping. Its combined-effect task (T053) additionally depends on Phase 4.
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
- Within Phase 5: T027-T029 (three independent RIB scene files).
- Within Phase 8: T048-T050 (three independent primitive-type RIB scene sets).
- T075 (Phase 12) can run alongside T076/T077 since it only reads existing state.
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
  repo's existing test-authoring convention — not a new unit-test framework.
- Commit after each phase's Checkpoint.
- Verify `ctest -L visual` and the relevant `-L parity` scenes at every checkpoint before
  starting the next phase.
