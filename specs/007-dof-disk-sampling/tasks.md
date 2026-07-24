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

- [ ] T001 Record pre-fix render-time baseline: `time` the current (unmodified) `build/src/orender/orender` against `examples/rib/tests/camera-dof-raytrace.rib` and `examples/rib/tests/camera-motion-small+dof-raytrace.rib` (per quickstart.md step 8); note the two timings for later comparison in T020. No file changes — this is a measurement checkpoint only.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Build the one shared, correct disk sampler (`sampleDisk()`) under TDD, prove it preserves REYES's exact output (the validation lever from research.md §4), and stand up the FR-009 histogram tool that later phases depend on for cross-checks. Nothing in Phase 3+ can start until `sampleDisk()` exists and REYES's stability is confirmed.

**⚠️ CRITICAL**: No user story work can begin until this phase is complete.

- [ ] T002 Write failing unit test `tests/test_disk_sampling.cpp` (new file, flat in `tests/`, same pattern as `tests/test_64bit_portability.cpp`): assert generated samples always satisfy `x² + y² < 1` and are finite (covers FR-007's aperture-edge/near-pinhole extremes), and assert area-uniformity by binning `r²` (not `r`) into equal-width buckets and checking near-flat bin counts (research.md §6 step 1). `sampleDisk()` does not exist yet — this must fail to compile/link (TDD Red).
- [ ] T003 Register the `test_disk_sampling` executable and a `DiskSampling`-labeled CTest entry in `tests/CMakeLists.txt`, following the existing `test_64bit_portability` registration pattern (depends on T002).
- [ ] T004 Check: run `ctest --test-dir build -R DiskSampling --output-on-failure` and confirm it fails to build (Red) — this is the TDD gate proving the test was written before the implementation (depends on T003).
- [ ] T005 [P] Write `tests/visual/test_radial_histogram.cpp` (new file, FR-009): a standalone CLI tool mirroring `tests/visual/test_visual_render.cpp`'s libtiff-based structure, taking a rendered TIF plus `--center x y --radius r --bins n`, emitting `r_lo,r_hi,energy,energy/annulus_area` rows to stdout; supports a two-file mode (candidate vs. REYES ground-truth) for the FR-006/Clarification-Q1 cross-check (research.md §5, data-model.md "Radial energy histogram"). Independent of the `sampleDisk()` track (different files) — safe to do in parallel with T002-T008.
- [ ] T006 [P] Register the `test_radial_histogram` executable in `tests/visual/CMakeLists.txt`, linked against `TIFF::TIFF` with `cxx_std_20`, matching `test_visual_render`'s target setup (depends on T005).
- [ ] T007 Implement `sampleDisk<Sampler>(float *R, Sampler &&sampler)` in `src/ri/random.h`, alongside the existing `sampleHemisphere`/`sampleCosineHemisphere`/`sampleSphere` rejection-sampling functions: loop generating `sampler(s)` → map to `[-1,1]²` → accept when `x²+y² < 1` (research.md §3 exact code). TDD Green — implements the logic T002 tests (depends on T004).
- [ ] T008 Check: run `ctest --test-dir build -R DiskSampling --output-on-failure` and confirm it now passes (Green) (depends on T007).
- [ ] T009 Refactor `src/ri/stochastic.cpp`'s rejection-sampling loop (~line 160-188) to call `sampleDisk(aperture, [this](float *s) { apertureGenerator.get(s); })`, removing the inlined loop it replaces. `src/ri/stochastic.h`'s `apertureGenerator` member (`CSobol<2>`) is unchanged (depends on T008).
- [ ] T010 Check: run `ctest --test-dir build -L visual --output-on-failure -R "camera-dof-reyes|camera-motion-small-dof-reyes"` and confirm both pass against their **existing, unmodified** reference images with zero diff beyond the existing threshold — the proof that REYES's algorithm/sequence was extracted faithfully, not altered (research.md §4; depends on T009). If this fails, fix T009 before proceeding — do not touch the raytracer yet.

**Checkpoint**: `sampleDisk()` exists, is unit-tested, and REYES's output is proven bit-for-bit unaffected. The histogram tool is built and ready. User story work can now begin.

---

## Phase 3: User Story 1 - Correct depth-of-field blur when raytracing (Priority: P1) 🎯 MVP

**Goal**: Fix the raytrace hider's center-biased lens sampling so out-of-focus blur is area-uniform, matching the physically-correct look REYES already produces (spec.md US1).

**Independent Test**: Render a strong-DOF scene with the raytrace hider and confirm the blur circle shows uniform radial energy distribution, not a center hot spot (spec.md US1 Independent Test).

- [ ] T011 [US1] Fix `src/ri/raytracer.cpp`'s `CRaytracer::computeSamples()` (~line 519-526): replace the buggy `theta = urand() * 2*PI` / `r = urand() * CRenderer::aperture` polar-mapping block with `sampleDisk(aperture, [this](float *s) { s[0] = urand(); s[1] = urand(); })`, then scale `from[COMP_X]`/`from[COMP_Y]` by `aperture[0] * CRenderer::aperture` / `aperture[1] * CRenderer::aperture` (research.md §3 call-site sketch; depends on T010).
- [ ] T012 [US1] Check: run `ctest --test-dir build -L visual --output-on-failure -R "camera-dof-raytrace|camera-motion-small-dof-raytrace"` and confirm both now **fail** against their old (pre-fix, buggy-baseline) reference images — this failure is the proof the center-bias fix actually changed the raytracer's output (quickstart.md step 4; depends on T011).
- [ ] T013 [US1] Render `examples/rib/tests/camera-dof-raytrace.rib` and `examples/rib/tests/camera-motion-small+dof-raytrace.rib` with the fixed `build/src/orender/orender` to produce candidate TIFs (depends on T011).
- [ ] T014 [US1] Check: cross-check both candidate TIFs against the existing (untouched) `camera-dof-reyes.tif` / `camera-motion-small+dof-reyes.tif` reference images using `build/tests/visual/test_radial_histogram`'s two-file mode; confirm `energy/annulus_area` curves match within tolerance (flat, not center-peaked) per FR-006/SC-001/SC-002 (depends on T013, T006).
- [ ] T015 [US1] Copy the validated candidate TIFs into `examples/rib/tests/references/`, replacing the stale `camera-dof-raytrace.tif` and `camera-motion-small+dof-raytrace.tif` baselines (depends on T014).
- [ ] T016 [US1] Check: re-run `ctest --test-dir build -L visual --output-on-failure -R "camera-dof-raytrace|camera-motion-small-dof-raytrace"` and confirm both now **pass** against the newly-committed references (SC-003; depends on T015).
- [ ] T017 [US1] Check: run `build/tests/visual/test_radial_histogram` single-file mode on the new raytrace TIFs; confirm the `energy/annulus_area` curve is roughly flat across bins, not decaying with radius (direct visual confirmation of SC-001; depends on T015).

**Checkpoint**: Raytrace DOF is fixed, validated against REYES ground truth, and its regression references are updated and green. This alone is a shippable MVP.

---

## Phase 4: User Story 2 - Consistent depth-of-field results across hiders (Priority: P2)

**Goal**: Confirm the two hiders now produce statistically equivalent DOF blur for identical settings, and that nothing else in the renderer regressed (spec.md US2).

**Independent Test**: Render the same DOF scene with both hiders and compare blur-circle size/distribution for equivalence (spec.md US2 Independent Test).

- [ ] T018 [US2] Check: run the full suite `ctest --test-dir build -L visual --output-on-failure` and confirm 100% pass — all DOF scenes (both hiders) against updated/unchanged references, and all non-DOF (pinhole) scenes unaffected (FR-005, SC-003; depends on T016, T010).
- [ ] T019 [US2] Check: run `build/tests/visual/test_radial_histogram` two-file mode comparing the committed `camera-dof-raytrace.tif` vs. `camera-dof-reyes.tif` (and the motion-blur+DOF pair) as a durable cross-hider consistency record — distinct from T014's pre-commit gate, this is the SC-002 regression-level confirmation (depends on T015).
- [ ] T020 [US2] Check: re-time `camera-dof-raytrace.rib`/`camera-motion-small+dof-raytrace.rib` renders with the fixed binary and compare against the T001 baseline; confirm ≤1% regression (SC-005; depends on T001, T015).

**Checkpoint**: Both hiders verified statistically consistent; full regression suite green; performance within bound.

---

## Phase 5: User Story 3 - Maintainable, non-duplicated lens sampling (Priority: P3)

**Goal**: Confirm and document that lens-disk sampling now has exactly one authoritative implementation shared by both hiders (spec.md US3).

**Independent Test**: Inspect the renderer's source and confirm a single shared implementation is used by both hiders, not two (spec.md US3 Independent Test).

- [ ] T021 [US3] Audit: grep `src/ri/` for any remaining independent lens/aperture-disk sampling logic outside `sampleDisk()` in `src/ri/random.h`; confirm `stochastic.cpp` and `raytracer.cpp` are the only two call sites and both route through it (SC-004; depends on T009, T011).
- [ ] T022 [US3] Update `DEVNOTES_DETAILS/HIDER_PARITY.md` Alignment Status: add a new `[x]` bullet documenting DOF/lens-sampling parity as closed, naming `sampleDisk()` in `src/ri/random.h` as the single shared implementation (FR-008; depends on T021).

**Checkpoint**: Single-implementation guarantee verified and documented.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: End-to-end sign-off and cleanup.

- [ ] T023 Run the full `quickstart.md` validation sequence end-to-end (steps 1-9) as the final combined gate before considering the feature done (depends on T017, T018, T019, T020, T022).
- [ ] T024 [P] Clean up any scratch/temporary render outputs produced during T013/T019 that live outside `examples/rib/tests/references/` (e.g. working-directory TIFs from manual render/cross-check steps).

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — can start immediately.
- **Foundational (Phase 2)**: Depends on Setup completion — BLOCKS all user stories. Contains two parallel-safe tracks: the `sampleDisk()`/REYES-refactor track (T002→T004→T007→T008→T009→T010) and the histogram-tool track (T005→T006), which don't touch the same files.
- **User Story 1 (Phase 3)**: Depends on Foundational (specifically T010, the REYES-stability gate — research.md §6 explicitly sequences the raytracer fix *after* this proof) and T006 (tool needed for T014's cross-check).
- **User Story 2 (Phase 4)**: Depends on User Story 1 completion (T015/T016) — it validates the state US1 produces, plus reuses the T001 baseline.
- **User Story 3 (Phase 5)**: Depends on both call sites being migrated (T009 from Foundational, T011 from US1).
- **Polish (Phase 6)**: Depends on all prior phases.

### Parallel Opportunities

- T005/T006 (histogram tool) can run in parallel with T002-T010 (`sampleDisk()`/REYES track) — disjoint files.
- Within Foundational, T002 and T005 can start together at the very beginning.
- T024 is parallel-safe with nothing else outstanding — it's cleanup, do it last regardless.

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

- Checks (T004, T008, T010, T012, T014, T016, T017, T018, T019, T020, T023) are interleaved with implementation tasks throughout, not batched at the end, per the request to keep verification distributed along the process.
- The `stochastic.{h,cpp}` → `reyes` file reorganization requested alongside this fix is deliberately **not** in this task list — recorded as an out-of-scope follow-up in research.md §7 per explicit user decision during planning.
- `[P]` tasks touch different files with no ordering dependency on each other.
- `[Story]` labels map every Phase 3+ task to spec.md's US1/US2/US3 for traceability.
- Verify each Red check (T004) actually fails, and each stability/regression check actually passes, before moving on — these are load-bearing gates, not formalities.
