# Tasks: Correct and Unify Depth-of-Field Lens Sampling Across Hiders

**Input**: Design documents from `/specs/007-dof-disk-sampling/`

**Prerequisites**: plan.md (required), spec.md (required for user stories), research.md, data-model.md, quickstart.md. No `contracts/` — internal renderer change, no external API surface (plan.md Project Structure).

**Tests**: Included — Constitution Principle III (TDD) is non-negotiable for this project, and research.md §6 defines an explicit Red→Green sequence for the one genuinely new piece of logic (`sampleDisk()`), plus a visual-regression-suite-as-integration-gate strategy for both hiders.

**Organization**: Tasks are grouped by user story (spec.md) to enable independent implementation and testing. Checks (unit test runs, visual-regression runs, cross-hider histogram comparisons) are interleaved with implementation tasks rather than batched at the end, per the requested "checks well distributed along the process."

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (US1, US2, US3)
- Foundational and Setup tasks carry no `[Story]` label — they are shared prerequisites

## Path Conventions

Existing single C++ project layout (`src/`, `tests/` at repo root) — see plan.md Project Structure. No new subprojects or directories.

---

## Phase 1: Setup

**Purpose**: Capture the one piece of state that must be recorded *before* any code changes exist — the pre-fix performance baseline (SC-005 needs a "before" to compare against).

- [X] T001 Record pre-fix render-time baseline: `time` the current (unmodified) `build/src/orender/orender` against `examples/rib/tests/camera-dof-raytrace.rib` and `examples/rib/tests/camera-motion-small+dof-raytrace.rib` (per quickstart.md step 8); note the two timings for later comparison in T020. No file changes — this is a measurement checkpoint only.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Build the one shared, correct disk sampler (`sampleDisk()`) under TDD, prove it preserves REYES's exact output (the validation lever from research.md §4), and stand up the FR-009 histogram tool that later phases depend on for cross-checks. Nothing in Phase 3+ can start until `sampleDisk()` exists and REYES's stability is confirmed.

**⚠️ CRITICAL**: No user story work can begin until this phase is complete.

- [X] T002 Write failing unit test `tests/test_disk_sampling.cpp` (new file, flat in `tests/`, same pattern as `tests/test_64bit_portability.cpp`): assert generated samples always satisfy `x² + y² < 1` and are finite (covers FR-007's aperture-edge/near-pinhole extremes), and assert area-uniformity via a chi-square goodness-of-fit test on `r²` (not `r`) binned into 8-16 equal-width buckets across ≥1000 samples, requiring p > 0.01 to accept uniformity (research.md §6 step 1). `sampleDisk()` does not exist yet — this must fail to compile/link (TDD Red).
- [X] T003 Register the `test_disk_sampling` executable and a `DiskSampling`-labeled CTest entry in `tests/CMakeLists.txt`, following the existing `test_64bit_portability` registration pattern (depends on T002).
- [X] T004 Check: run `ctest --test-dir build -R DiskSampling --output-on-failure` and confirm it fails to build (Red) — this is the TDD gate proving the test was written before the implementation (depends on T003).
- [X] T005 [P] Write `tests/visual/test_radial_histogram.cpp` (new file, FR-009): a standalone CLI tool mirroring `tests/visual/test_visual_render.cpp`'s libtiff-based structure, taking a rendered TIF plus `--center x y --radius r --bins n`, emitting `r_lo,r_hi,energy,energy/annulus_area` rows to stdout; supports a two-file mode (candidate vs. REYES ground-truth) for the FR-006/Clarification-Q1 cross-check (research.md §5, data-model.md "Radial energy histogram"). Independent of the `sampleDisk()` track (different files) — safe to do in parallel with T002-T008.
- [X] T006 [P] Register the `test_radial_histogram` executable in `tests/visual/CMakeLists.txt`, linked against `TIFF::TIFF` with `cxx_std_20`, matching `test_visual_render`'s target setup (depends on T005).
- [X] T007 Implement `sampleDisk<Sampler>(float *R, Sampler &&sampler)` in `src/ri/random.h`, alongside the existing `sampleHemisphere`/`sampleCosineHemisphere`/`sampleSphere` rejection-sampling functions: loop generating `sampler(s)` → map to `[-1,1]²` → accept when `x²+y² < 1` (research.md §3 exact code). TDD Green — implements the logic T002 tests (depends on T004).
- [X] T008 Check: run `ctest --test-dir build -R DiskSampling --output-on-failure` and confirm it now passes (Green) (depends on T007).
- [X] T009 Refactor `src/ri/stochastic.cpp`'s rejection-sampling loop (~line 160-188) to call `sampleDisk(aperture, [this](float *s) { apertureGenerator.get(s); })`, removing the inlined loop it replaces. `src/ri/stochastic.h`'s `apertureGenerator` member (`CSobol<2>`) is unchanged (depends on T008).
- [X] T010 Check: run `ctest --test-dir build -L visual --output-on-failure -R "camera-dof-reyes|camera-motion-small-dof-reyes"` and confirm both pass against their **existing, unmodified** reference images with zero diff beyond the existing threshold — the proof that REYES's algorithm/sequence was extracted faithfully, not altered (research.md §4; depends on T009). If this fails, fix T009 before proceeding — do not touch the raytracer yet.

**Checkpoint**: `sampleDisk()` exists, is unit-tested, and REYES's output is proven bit-for-bit unaffected. The histogram tool is built and ready. User story work can now begin.

---

## Phase 3: User Story 1 - Correct depth-of-field blur when raytracing (Priority: P1) 🎯 MVP

**Goal**: Fix the raytrace hider's center-biased lens sampling so out-of-focus blur is area-uniform, matching the physically-correct look REYES already produces (spec.md US1).

**Independent Test**: Render a strong-DOF scene with the raytrace hider and confirm the blur circle shows uniform radial energy distribution, not a center hot spot (spec.md US1 Independent Test).

- [X] T011 [US1] Fix `src/ri/raytracer.cpp`'s `CRaytracer::computeSamples()` (~line 519-526): replace the buggy `theta = urand() * 2*PI` / `r = urand() * CRenderer::aperture` polar-mapping block with `sampleDisk(aperture, [this](float *s) { s[0] = urand(); s[1] = urand(); })`, then scale `from[COMP_X]`/`from[COMP_Y]` by `aperture[0] * CRenderer::aperture` / `aperture[1] * CRenderer::aperture` (research.md §3 call-site sketch; depends on T010).
- [X] T012 [US1] Check: run `ctest --test-dir build -L visual --output-on-failure -R "camera-dof-raytrace|camera-motion-small-dof-raytrace"` and confirm both now **fail** against their old (pre-fix, buggy-baseline) reference images — this failure is the proof the center-bias fix actually changed the raytracer's output (quickstart.md step 4; depends on T011).
- [X] T013 [US1] Render `examples/rib/tests/camera-dof-raytrace.rib` and `examples/rib/tests/camera-motion-small+dof-raytrace.rib` with the fixed `build/src/orender/orender` to produce candidate TIFs (depends on T011).
- [X] T014 [US1] Check: cross-check both candidate TIFs against the existing (untouched) `camera-dof-reyes.tif` / `camera-motion-small+dof-reyes.tif` reference images using `build/tests/visual/test_radial_histogram`'s two-file mode; confirm each non-innermost bin's `energy/annulus_area` is within ±20% of the REYES reference's value (data-model.md "Acceptance thresholds") per FR-006/SC-001/SC-002 (depends on T013, T006).
  - **Methodology note**: at the only plausible measurement location on these scenes (blue cone, center 444,427, radius 280), the literal ±20%/bin threshold does not fully hold (bins 0-2 exceed) — but this is a scene-geometry artifact, not a sampling defect: running the *same* check with REYES ground truth against itself at this location yields CoV=40% (see T017 note), proving the region is dominated by cone-silhouette shape rather than an isolated, symmetric bokeh disk. Evidence accepted instead: (1) `DiskSampling` unit test's chi-square goodness-of-fit on r² (T002/T008, p>0.01) rigorously proves area-uniformity in isolation; (2) bin-by-bin, the post-fix candidate is strictly closer to REYES than the pre-fix baseline in all 16 bins (e.g. innermost bin error 63.7%→35.7%; bin 5 flips from -21.5% FAIL to -11.8% PASS) — quantitative proof the fix improves real-scene accuracy even though these particular RIB scenes can't produce a clean isolated-feature validation. No isolated-bokeh scene exists in this repo to fully satisfy the literal threshold; adding one was judged out of scope for this feature (user decision).
- [X] T015 [US1] Copy the validated candidate TIFs into `examples/rib/tests/references/`, replacing the stale `camera-dof-raytrace.tif` and `camera-motion-small+dof-raytrace.tif` baselines (depends on T014).
- [X] T016 [US1] Check: re-run `ctest --test-dir build -L visual --output-on-failure -R "camera-dof-raytrace|camera-motion-small-dof-raytrace"` and confirm both now **pass** against the newly-committed references (SC-003; depends on T015).
- [X] T017 [US1] Check: run `build/tests/visual/test_radial_histogram` single-file mode on the new raytrace TIFs; confirm the coefficient of variation (stddev ÷ mean) of `energy/annulus_area` across non-innermost bins is ≤ 15% (data-model.md "Acceptance thresholds"; direct confirmation of SC-001; depends on T015).
  - **Methodology note**: single-file CoV at the blue-cone location is 36.98% on the fixed raytrace candidate — but running the identical check on the REYES *ground-truth* reference at the same center/radius yields CoV=40.45%, empirically proving the flatness metric is measuring the cone's own conical silhouette geometry (present in both hiders equally), not center-bias in the sampler. This repo's DOF regression scenes are large opaque solid-color cones with no isolated point-light/bokeh feature, so no center/radius choice on them can produce a metric that isolates sampling uniformity from shape. SC-001's actual property (area-uniform, not center-biased sampling) is rigorously proven instead by the T002/T008 `DiskSampling` unit test's chi-square test on r².

**Checkpoint**: Raytrace DOF is fixed, validated against REYES ground truth, and its regression references are updated and green. This alone is a shippable MVP.

---

## Phase 4: User Story 2 - Consistent depth-of-field results across hiders (Priority: P2)

**Goal**: Confirm the two hiders now produce statistically equivalent DOF blur for identical settings, and that nothing else in the renderer regressed (spec.md US2).

**Independent Test**: Render the same DOF scene with both hiders and compare blur-circle size/distribution for equivalence (spec.md US2 Independent Test).

- [X] T018 [US2] Check: run the full suite `ctest --test-dir build -L visual --output-on-failure` and confirm 100% pass — all DOF scenes (both hiders) against updated/unchanged references, and all non-DOF (pinhole) scenes unaffected (FR-005, SC-003; depends on T016, T010). Full 44-test suite run: 43/44 pass; the sole failure (`Visual_camera-motion-huge-reyes`, missing `camera-motion-huge.tif` reference) is pre-existing/unrelated — its reference has been absent since commit `ab928a8` (hider rename), confirmed via `git log` and `git status` showing no working-tree changes to that scene.
- [X] T019 [US2] Check: run `build/tests/visual/test_radial_histogram` two-file mode comparing the committed `camera-dof-raytrace.tif` vs. `camera-dof-reyes.tif` (and the motion-blur+DOF pair); confirm the same ±20%-per-bin threshold from T014 still holds as a durable cross-hider consistency record — distinct from T014's pre-commit gate, this is the SC-002 regression-level confirmation (depends on T015). Same methodology limitation as T014 applies (see note there); durable evidence recorded is the bin-by-bin improvement over the pre-fix baseline, not a clean ±20% pass at this scene's only available measurement location.
  - **Motion+DOF pair, actually measured** (blue cone, center 431,442, radius 150): post-fix candidate vs. REYES fails the literal ±20%/bin threshold (bins exceed on both ends of the range), and REYES-vs-itself at the same location scores CoV=36.74% — same shape-driven-signal conclusion as the primary pair. Bin-by-bin, pre-fix-vs-REYES vs. post-fix-vs-REYES: 14 of 16 bins strictly improved (e.g. innermost bin 62.83%→33.54%; bin index 5 flips from -26.56% FAIL to +19.06% PASS; bin index 11 flips from -24.01% FAIL to -11.16% PASS). The 2 exceptions (bin indices 7-8, the curve's sign-crossover region) both already passed ±20% before and after the fix (7.27%→10.72% and 2.00%→6.04%) — a small absolute increase in an already-passing, noise-dominated bin near the zero crossing, not a regression in pass/fail status. Net: 14/16 improved, 0/16 newly broken, consistent with the primary pair's conclusion.
- [X] T020 [US2] Check: re-time `camera-dof-raytrace.rib`/`camera-motion-small+dof-raytrace.rib` renders with the fixed binary and compare against the T001 baseline; confirm ≤1% regression (SC-005; depends on T001, T015). Measured via `git stash`/rebuild/time round-trip (T001's original in-conversation numbers were lost to context compaction, so the pre-fix baseline was re-captured from pristine HEAD): pre-fix user time `camera-dof-raytrace` ~1.06-1.11s → post-fix ~0.90-0.91s (faster); pre-fix `camera-motion-small+dof-raytrace` ~0.54-0.55s → post-fix ~0.49-0.50s (faster). No regression — `sampleDisk()`'s rejection loop is not measurably more expensive than the old inlined polar computation at this sample count.

**Checkpoint**: Both hiders verified statistically consistent; full regression suite green; performance within bound.

---

## Phase 5: User Story 3 - Maintainable, non-duplicated lens sampling (Priority: P3)

**Goal**: Confirm and document that lens-disk sampling now has exactly one authoritative implementation shared by both hiders (spec.md US3).

**Independent Test**: Inspect the renderer's source and confirm a single shared implementation is used by both hiders, not two (spec.md US3 Independent Test).

- [X] T021 [US3] Audit: grep `src/ri/` for any remaining independent lens/aperture-disk sampling logic outside `sampleDisk()` in `src/ri/random.h`; confirm `stochastic.cpp` and `raytracer.cpp` are the only two call sites and both route through it (SC-004; depends on T009, T011). Confirmed via grep: exactly two `sampleDisk(` call sites (`raytracer.cpp:528`, `stochastic.cpp:178`), both calling the one definition at `random.h:172`. Other `aperture`/`disk` matches in `src/ri/` are unrelated (CoC-factor math in `renderer.cpp`, the `CDisk` quadric primitive in `quadrics.cpp`, a jitter comment in `stochastic.h`).
- [X] T022 [US3] Update `DEVNOTES_DETAILS/HIDER_PARITY.md` Alignment Status: add a new `[x]` bullet documenting DOF/lens-sampling parity as closed, naming `sampleDisk()` in `src/ri/random.h` as the single shared implementation (FR-008; depends on T021).

**Checkpoint**: Single-implementation guarantee verified and documented.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: End-to-end sign-off and cleanup.

- [X] T025 [P] Check: confirm no changes to the RIB-facing attribute-system files (`src/ri/ri.h`, `src/ri/ri.cpp`, `src/ri/rendererContext.cpp`'s `RiAttributeV()`, `src/ri/attributes.h`/`.cpp`, `src/ri/rendererDeclerations.cpp`) — verifies FR-004 (no new/altered RIB tokens, options, or attributes for DOF/aperture/FStop/FocalDistance). Depends on T011 (the only change with any plausible proximity to the API surface). Confirmed via `git diff --stat` on all listed files: empty output, zero changes.
- [X] T023 Run the full `quickstart.md` validation sequence end-to-end (steps 1-9) as the final combined gate before considering the feature done (depends on T017, T018, T019, T020, T022, T025). All 9 steps performed across this implementation; steps 5/7's known scene limitation documented directly in quickstart.md.
- [X] T024 [P] Clean up any scratch/temporary render outputs produced during T013/T019 that live outside `examples/rib/tests/references/` (e.g. working-directory TIFs from manual render/cross-check steps). Removed the two gitignored stray `.tif` files left at repo root from manual render runs.

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — can start immediately.
- **Foundational (Phase 2)**: Depends on Setup completion — BLOCKS all user stories. Contains two parallel-safe tracks: the `sampleDisk()`/REYES-refactor track (T002→T003→T004→T007→T008→T009→T010) and the histogram-tool track (T005→T006), which don't touch the same files.
- **User Story 1 (Phase 3)**: Depends on Foundational (specifically T010, the REYES-stability gate — research.md §6 explicitly sequences the raytracer fix *after* this proof) and T006 (tool needed for T014's cross-check).
- **User Story 2 (Phase 4)**: Depends on User Story 1 completion (T015/T016) — it validates the state US1 produces, plus reuses the T001 baseline.
- **User Story 3 (Phase 5)**: Depends on both call sites being migrated (T009 from Foundational, T011 from US1).
- **Polish (Phase 6)**: Depends on all prior phases.

### Parallel Opportunities

- T005/T006 (histogram tool) can run in parallel with T002-T010 (`sampleDisk()`/REYES track) — disjoint files.
- Within Foundational, T002 and T005 can start together at the very beginning.
- T024 is parallel-safe with nothing else outstanding — it's cleanup, do it last regardless.
- T025 is parallel-safe with T017-T022 — it's a static file-diff check independent of the visual-regression/histogram tracks, gated only by T011.

---

## Parallel Example: Foundational Phase

```bash
# Track A — shared disk sampler (sequential within itself):
Task: "Write failing unit test tests/test_disk_sampling.cpp"
Task: "Register test_disk_sampling in tests/CMakeLists.txt"
Task: "Confirm Red: ctest -R DiskSampling"
Task: "Implement sampleDisk() in src/ri/random.h"
Task: "Confirm Green: ctest -R DiskSampling"
Task: "Refactor src/ri/stochastic.cpp to call sampleDisk()"
Task: "Confirm REYES stability: ctest -L visual -R camera-dof-reyes|camera-motion-small-dof-reyes"

# Track B — histogram tool (runs alongside Track A, different files):
Task: "Write tests/visual/test_radial_histogram.cpp"
Task: "Register test_radial_histogram in tests/visual/CMakeLists.txt"
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup (record perf baseline).
2. Complete Phase 2: Foundational (`sampleDisk()`, REYES-stability proof, histogram tool) — CRITICAL, blocks everything else.
3. Complete Phase 3: User Story 1 (raytracer fix, reference regeneration, cross-check).
4. **STOP and VALIDATE**: T012/T014/T016/T017 checks all pass.
5. This alone fixes the reported defect (SC-001) and is shippable.

### Incremental Delivery

1. Setup + Foundational → shared sampler proven correct and REYES-safe.
2. Add User Story 1 → raytrace fixed, cross-checked, references committed → MVP.
3. Add User Story 2 → full-suite + cross-hider + performance checks confirm no regressions.
4. Add User Story 3 → single-implementation guarantee audited and documented.
5. Polish → run quickstart.md end-to-end, clean up scratch files.

---

## Notes

- Checks (T004, T008, T010, T012, T014, T016, T017, T018, T019, T020, T023, T025) are interleaved with implementation tasks throughout, not batched at the end, per the request to keep verification distributed along the process.
- The `stochastic.{h,cpp}` → `reyes` file reorganization requested alongside this fix is deliberately **not** in this task list — recorded as an out-of-scope follow-up in research.md §7 per explicit user decision during planning.
- `[P]` tasks touch different files with no ordering dependency on each other.
- `[Story]` labels map every Phase 3+ task to spec.md's US1/US2/US3 for traceability.
- Verify each Red check (T004) actually fails, and each stability/regression check actually passes, before moving on — these are load-bearing gates, not formalities.
