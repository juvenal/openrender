---

description: "Task list for LLVM JIT Opcode-Coverage Parity Sweep"
---

# Tasks: LLVM JIT Opcode-Coverage Parity Sweep

**Input**: Design documents from `/specs/011-jit-opcode-parity/`

**Prerequisites**: `plan.md`, `spec.md`, `research.md`, `data-model.md`, `contracts/`, `quickstart.md` — all present and complete.

**Tests**: Explicitly requested by this feature (FR-006 coverage guard, Constitution Principle III TDD requirement) — test/repro tasks are included and sequenced Red-before-Green.

**Organization**: Tasks are grouped by user story (spec.md, US1-US5). Per user direction, checkpoints exist both at story boundaries AND at high-risk sub-milestones inside stories (the D2 colorspace relocation, the kHandledOpcodes table migration, and gather()'s CFG scaffolding). **Execution must stop at every checkpoint below and wait for explicit user confirmation before continuing to the next block** — this is a binding instruction for whoever executes this task list (including `/speckit-implement`), not just a suggestion.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (US1-US5)
- File paths are exact where the source location is already confirmed (plan.md/research.md/data-model.md); tasks that require locating a not-yet-identified macro target (e.g. `MFROMEXPR`'s expansion) say so explicitly — this is a Phase-1-implementation detail per `research.md` D1, not an unresolved planning question.

## Path Conventions

Single C++ project. `src/common/`, `src/ri/`, `src/libshader/{compiler,shading}/`, `tests/{libshader,visual}/` at repository root — see `plan.md`'s Source Code tree for the full annotated layout.

---

## Phase 1: Setup

**Purpose**: Establish a known-good baseline before any change.

- [ ] T001 Verify branch `011-jit-opcode-parity` is checked out and `cmake --build build --config Release` succeeds cleanly — no code changes yet.
- [ ] T002 [P] Record baseline test status: run `ctest --test-dir build -L libshader --output-on-failure` and `ctest --test-dir build -L visual -E slow --output-on-failure`; save pass/fail counts as the reference point for detecting regressions this feature introduces (distinct from pre-existing failures like stale `.slo` deploy-tree artifacts).

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: The `kHandledOpcodes[]` single-source-of-truth table (D3) that both `emitFunction()` and the Story 4 coverage-guard test consult. This blocks every user story: the `op-wrapper-abi.md` contract requires every new opcode case (Story 1 and Story 3's) to be table-keyed, and the coverage-guard's correctness requires *every already-handled* opcode to be in the table too — otherwise the guard would report currently-working opcodes as "missing" just because they're still dispatched via the old `if/else-if` chain.

**⚠️ CRITICAL**: No user story work can begin until this phase is complete and its checkpoint is confirmed.

- [ ] T003 Add a `static const char* const kHandledOpcodes[]` table in `src/libshader/compiler/llvmEmitter.cpp` (or a new sibling header if cleaner), seeded with every opcode mnemonic the current `if/else-if` chain in `emitFunction()` already recognizes (~30+ entries, trimmed of `opcodes.cpp`'s string-padding).
- [ ] T004 Refactor `emitFunction()`'s dispatch in `src/libshader/compiler/llvmEmitter.cpp` so every existing case is keyed off `kHandledOpcodes` instead of bare string-literal `if (op == "...")` comparisons. Zero behavior change — this is a pure structural refactor (Constitution Principle I), not a fix.
- [ ] T005 Resolve the `gatherHeader`/`gatherhdr` lowercase/mixed-case mismatch in `src/libshader/compiler/irBuilder.cpp:261` as part of this refactor pass — this determines whether `gather()`-family opcodes can even be compared against the table correctly, and feeds directly into Phase 3 (US2) triage accuracy.
- [ ] T006 Build and run `ctest --test-dir build -L libshader --output-on-failure` and `ctest --test-dir build -L visual -E slow --output-on-failure`; confirm identical results to the T002 baseline (zero regression from the table refactor alone).

**Checkpoint (Foundational)**: `kHandledOpcodes[]` exists and drives every existing dispatch case; build and full existing test suite behave identically to baseline. **STOP — wait for user confirmation before any new-opcode work begins.**

---

## Phase 3: User Story 2 - Reachability inventory (Priority: P1)

**Goal**: Produce the verified reachable-opcode list that Story 3's fix scope and Story 4's coverage-guard expected set both depend on.

**Independent Test**: A list of confirmed-reachable, currently-unhandled constructs, each backed by a minimal demonstrating shader, reviewable independently of any fix being implemented yet.

- [ ] T007 [P] [US2] Write minimal `.sl` triage repros for matrix-arithmetic candidates (`mulmm, addmm, submm, divmm, negm, movemm, mfromf, mfromv, ...`) in `tests/libshader/triage/matrix_ops.sl`; compile with `oshader` (non-JIT, IR text output) and grep for each literal mnemonic to confirm frontend emission.
- [ ] T008 [P] [US2] Write minimal `.sl` triage repro for `gather()`/`gatherElse`/`gatherEnd` in `tests/libshader/triage/gather.sl`; compile (post-T005 fix) and inspect IR/compiler output to determine the actual failure mode (silent-wrong-output vs. hard compile/render failure) per spec.md's Edge Cases.
- [ ] T009 [P] [US2] Write minimal `.sl` triage repros for comparison/logic candidates (`veql, vneql, meql, mneql, fegt, not, xor, nxor, ...`) in `tests/libshader/triage/comparison_logic.sl`; compile and grep to confirm emission.
- [ ] T010 [P] [US2] Write minimal `.sl` triage repros for array move-op candidates (`ffroma, vfroma, mfroma, sfroma, ftoa, vtoa, ...`, including uniform `u*froma` variants) in `tests/libshader/triage/array_ops.sl`; compile and grep to confirm emission.
- [ ] T011 [US2] Consolidate T007-T010 results into a Reachability Inventory table (new `specs/011-jit-opcode-parity/triage-results.md`), classifying every original ~48 candidate as reachable (with its repro reference) or not-reachable (with the specific reason: string-padding artifact, dead grammar path, etc.) — per `data-model.md`'s Reachability Inventory entity.

**Checkpoint (US2 — mandatory review gate)**: Present the confirmed-reachable-vs-not opcode list to the user for review. **STOP — do not proceed to Phase 4 or Phase 6/7 until the user confirms the inventory is accurate.**

---

## Phase 4: User Story 4, part 1 of 2 - Coverage-guard test authoring (Priority: P2, sequenced early for TDD)

**Goal**: Get the dynamic coverage-guard test (FR-006) written and confirmed RED before any Story 1/3 fix lands — required by Constitution Principle III (TDD, NON-NEGOTIABLE) and the plan's explicit TDD sequencing note. Positioned here (ahead of Story 1 in document order, despite Story 1 being P1) because it structurally depends on Phase 3/US2's confirmed-reachable list and must exist in a failing state before the fixes it will later confirm.

**Independent Test**: Deliberately introduce a locally uncommitted reachable-but-unhandled construct and confirm the suite fails, naming it — but at this point in sequencing, the test is exercised via its *expected* Red state against the real, still-unfixed `cfrom`/`mfrom`/`ctransform`/Story-3 gaps, not a deliberately-injected one (that comes later, in Phase 8/T043).

- [ ] T012 [US4] Add a new `libshader`-labeled ctest target (e.g. `tests/libshader/test_opcode_coverage.cpp`) that reads `kHandledOpcodes` directly (no source-text parsing) and compares it against the US2-confirmed reachable set (T011), per `contracts/coverage-guard-contract.md`'s pass/fail contract and named-mnemonic failure-message requirement.
- [ ] T013 [US4] Register the new test in the relevant `tests/libshader/CMakeLists.txt` with the `libshader` ctest label; build and run it — confirm it currently **fails**, naming `cfrom`, `mfrom`, `ctransform`, and every US2-confirmed-reachable Story-3 opcode as missing (TDD Red phase).

**Checkpoint (US4 part 1)**: Coverage-guard test exists, runs under `ctest -L libshader`, and is confirmed RED with the expected missing-opcode names. **STOP — wait for user confirmation before Phase 5 (Story 1 implementation) begins.**

---

## Phase 5: User Story 1 - `cfrom`/`mfrom`/`ctransform` parity fix (Priority: P1) 🎯 MVP

**Goal**: JIT output for the explicit-colorspace color constructor, the explicit-space matrix constructor, and `ctransform()` matches the interpreter backend.

**Independent Test**: Render a bare-sphere scene with a diagnostic shader once pinned to `rslo`, once to `slo`; before the fix the renders diverge, after they match within visual-regression tolerance.

### Sub-milestone: D2 relocation (colorspace functions out of `ri`)

- [ ] T014 [US1] Create `src/common/colorSpace.h` declaring `convertColorFrom(float*, const float*, ECoordinateSystem)` and `convertColorTo(float*, const float*, ECoordinateSystem)`, and `src/common/colorSpace.cpp` with the function bodies relocated **verbatim** from `src/ri/init.cpp:67` and `:228` (no logic changes — FR-009).
- [ ] T015 [US1] Add `colorSpace.h`/`colorSpace.cpp` to `src/common/CMakeLists.txt`'s `openrendercommon` sources, alongside the existing `rslConstants.cpp` precedent.
- [ ] T016 [US1] Remove the `convertColorFrom`/`convertColorTo` definitions from `src/ri/init.cpp`; add the `colorSpace.h` include so `ri`'s own callers still resolve.
- [ ] T017 [US1] Update `src/libshader/shading/execute.cpp:53` to replace the raw `extern` declarations with `#include "colorSpace.h"`.
- [ ] T018 [US1] Update `src/libshader/shading/shaderOpcodes.h:504`'s `CFROMEXPR` macro and `src/libshader/shading/shaderFunctions.h:514,536-537`'s `CTRANSFORMEXPR`-adjacent macros to resolve via the relocated `colorSpace.h` declarations — same call, new header, no behavior change.
- [ ] T019 [US1] Build; run `ctest --test-dir build -L libshader --output-on-failure` and `ctest --test-dir build -L visual -E slow --output-on-failure` (only interpreter/`rslo` code paths touched so far); confirm zero behavioral change vs. the T002/T006 baseline.

**Checkpoint (US1 sub-milestone — relocation)**: `convertColorFrom`/`convertColorTo` now live in `src/common/`; `ri` and the interpreter both compile and behave identically to baseline. **STOP — wait for user confirmation before adding any JIT-side wrapper code.**

### Sub-milestone: JIT wrappers + emitter cases

- [ ] T020 [US1] Add `op_cfrom(float* dst, int sd, const char* space, const float* src, int ss, int n, const int* tags)` to `src/libshader/shading/rslOps.h`/`.cpp`, cloned from `op_pfrom` (`rslOps.cpp:493`): resolve `cSystem` via `ctx->jitFindCoordinateSystem`, then call `convertColorFrom` per active tag — per `contracts/op-wrapper-abi.md`.
- [ ] T021 [US1] Locate `MFROMEXPR`'s target function in `shaderOpcodes.h` (Phase-1 implementation detail per `research.md` D1) and add `op_mfrom` to `rslOps.h`/`.cpp` following T020's pattern, delegating to that same function.
- [ ] T022 [US1] Add `op_ctransform` to `rslOps.h`/`.cpp`, delegating to `convertColorTo` (confirmed distinct from `convertColorFrom` per `research.md` D1) via the same `jitFindCoordinateSystem`-based space resolution.
- [ ] T023 [US1] In `src/libshader/compiler/llvmEmitter.cpp`: add `cfrom`/`mfrom` entries to `kHandledOpcodes` and their `emitFunction()` cases (clone of the `pfrom` block, `llvmEmitter.cpp:942-978`, calling `declareOp(mod, "op_cfrom"/"op_mfrom", ty)` + `B.CreateCall(...)` — zero raw IR construction).
- [ ] T024 [US1] In `llvmEmitter.cpp`: remove `ctransform` from the `pfrom`-family condition list; add its own `kHandledOpcodes` entry + `emitFunction()` case calling `op_ctransform`.
- [ ] T025 [US1] Verify `op_cfrom`/`op_mfrom`/`op_ctransform` each resolve at JIT bind time via the existing `DynamicLibrarySearchGenerator` (build + a minimal JIT-shader smoke run). Only add `jitSymbolRetain.cpp`-style retention if one fails to resolve (verify-on-add per `research.md` D5) — do not add it preemptively.
- [ ] T026 [US1] Repro per `quickstart.md` step 1: render a bare untextured sphere + `show_st.sl` pinned to `rslo`, then to `slo`; confirm the images now match for the color-constructor case (previously diverged). Repeat with matrix-constructor and `ctransform()`-exercising diagnostic shaders per spec.md's three Acceptance Scenarios.
- [ ] T027 [P] [US1] Add new `add_visual_test(<name>-slo, ...)` case(s) to `tests/visual/CMakeLists.txt` (pattern ~line 247-287) covering `cfrom`/`mfrom`/`ctransform`, using the `rslo` render as the golden/reference image.
- [ ] T028 [US1] Build; run `ctest --test-dir build -L libshader --output-on-failure` (confirm the T012/T013 coverage-guard test's `cfrom`/`mfrom`/`ctransform` entries are now GREEN) and `ctest --test-dir build -L visual --output-on-failure` (new + existing cases pass).

**Checkpoint (US1 complete — MVP)**: `cfrom`/`mfrom`/`ctransform` produce JIT output matching the interpreter; their coverage-guard entries are GREEN; new visual-regression cases pass. **STOP — wait for user confirmation before starting Phase 6.**

---

## Phase 6: User Story 3a - Broader sweep: matrix arithmetic, comparison/logic, array move (Priority: P2)

**Goal**: Every US2-confirmed-reachable opcode in these three categories reaches JIT/interpreter parity. `gather()` is deliberately excluded — see Phase 7.

**Independent Test**: For each confirmed-reachable construct in scope, render a minimal shader exercising it under both backends and confirm the outputs match.

- [ ] T029 [P] [US3] For each US2-confirmed-reachable matrix-arithmetic opcode: locate its interpreter macro/target function in `shaderOpcodes.h`/`shaderFunctions.h`; add a matching `op_*` wrapper to `rslOps.h`/`.cpp` delegating to that same function — no new math.
- [ ] T030 [P] [US3] For each US2-confirmed-reachable comparison/logic opcode: same delegation pattern as T029; check first whether an existing `op_*` can be reused with inverted/swapped args (e.g. `not`) before adding a new function.
- [ ] T031 [P] [US3] For each US2-confirmed-reachable array move-op opcode: locate the interpreter's array-indexing/stride helper in the `.rslo` path and delegate to it from a new `op_*` wrapper, rather than reimplementing array-stride math in the emitter.
- [ ] T032 [US3] Add `kHandledOpcodes` + `emitFunction()` cases in `llvmEmitter.cpp` for every wrapper added in T029-T031, following the `declareOp`+`CreateCall` pattern.
- [ ] T033 [P] [US3] Add corresponding `add_visual_test(<name>-slo, ...)` cases to `tests/visual/CMakeLists.txt` for matrix arithmetic, comparison/logic, and array move ops.
- [ ] T034 [US3] Build; run `ctest --test-dir build -L libshader --output-on-failure` (confirm these opcodes' coverage-guard entries now GREEN) and `ctest --test-dir build -L visual --output-on-failure`.

**Checkpoint (US3a complete)**: matrix arithmetic, comparison/logic, and array-move opcodes reach parity; their coverage-guard entries are GREEN. **STOP — wait for user confirmation before starting `gather()` (Phase 7).**

---

## Phase 7: User Story 3b - Broader sweep: `gather()` (Priority: P2, higher risk/uncertain scope)

**Goal**: Fix `gather()`/`gatherElse`/`gatherEnd` — split from Phase 6 because it needs new loop/CFG scaffolding (not a simple wrapper clone) and its failure mode/reachability is only confirmed by Phase 3/T008.

**Independent Test**: Render a shader using `gather()` for ambient occlusion or indirect illumination sampling under the JIT backend; output matches the interpreter backend.

- [ ] T035 [US3] Gate check: if Phase 3's triage (T008/T011) found `gather()` unreachable, or this phase's scope proves infeasible within a reasonable checkpoint, **stop here**, record the descope decision with rationale in `research.md`/`spec.md`'s Assumptions, and skip T036-T041.
- [ ] T036 [US3] Study `illuminance`/`endilluminance`'s scope-tracking structure (`llvmEmitter.cpp:629,678`) as the template for lowering a looping RSL construct with a body block.
- [ ] T037 [US3] Design and implement equivalent scope-tracking scaffolding for `gather`/`gatherElse`/`gatherEnd` in `llvmEmitter.cpp`, delegating the actual GI/AO sampling to whatever function the interpreter's existing `gather` handling calls — no new sampling math.
- [ ] T038 [US3] Add the corresponding `op_*` wrapper(s) in `rslOps.h`/`.cpp` for the delegation target identified in T037.
- [ ] T039 [US3] Add `kHandledOpcodes` + `emitFunction()` entries for `gather`/`gatherElse`/`gatherEnd`.
- [ ] T040 [P] [US3] Add `add_visual_test(gather-slo, ...)` case(s) to `tests/visual/CMakeLists.txt` covering ambient-occlusion/indirect-illumination sampling.
- [ ] T041 [US3] Build; run `ctest --test-dir build -L libshader --output-on-failure` (confirm the `gather` family's coverage-guard entries GREEN) and `ctest --test-dir build -L visual --output-on-failure`.

**Checkpoint (US3b complete)**: `gather()` family reaches parity, or is explicitly and documentedly descoped per T035. **STOP — wait for user confirmation before Phase 8.**

---

## Phase 8: User Story 4, part 2 of 2 - Coverage-guard finalization (Priority: P2)

**Goal**: Confirm the guard is fully green against real state and genuinely dynamic (catches future gaps, not just this feature's original inventory).

**Independent Test**: Deliberately introduce (locally, uncommitted) a new reachable construct with no JIT handling — including one outside this feature's original inventory — and confirm the suite fails, naming it; then confirm it passes cleanly with the change reverted.

- [ ] T042 [US4] Build; run `ctest --test-dir build -L libshader --output-on-failure`; confirm the coverage-guard test now passes 100% (every US2-confirmed-reachable opcode present in `kHandledOpcodes`, adjusted for any T035 descope).
- [ ] T043 [US4] Per spec.md Acceptance Scenario 3: locally add one new reachable-but-unhandled construct (or temporarily remove one `kHandledOpcodes` entry); confirm the test fails, naming that specific construct; then revert the local change and confirm the suite is green again.

**Checkpoint (US4 complete)**: Coverage-guard test passes against real state and is confirmed to catch a deliberately-introduced gap it wasn't specifically written for. **STOP — wait for user confirmation before Phase 9.**

---

## Phase 9: User Story 5 - Converge JIT-only helpers onto delegation (Priority: P3)

**Goal**: `jitArea`/`jitCalculateNormal`/`jitDepth`/`jitDuVector`/`jitDvVector` (`shading.cpp`) stop being hand-reimplemented duplicates and call the same function the interpreter uses, like `op_specular_batch` already does.

**Independent Test**: Render a shader using `area()`, `calculatenormal()`, `depth()`, `Du()`, `Dv()` under the JIT backend before/after; output is unchanged; by inspection, only one implementation of each remains.

- [ ] T044 [P] [US5] Refactor `jitArea` (`shading.cpp`) to call the same function/macro body `AREAEXPR` (`shaderFunctions.h`) expands to, rather than its hand-reimplemented copy.
- [ ] T045 [P] [US5] Refactor `jitCalculateNormal` (`shading.cpp`) to delegate to the interpreter's calculate-normal implementation.
- [ ] T046 [P] [US5] Refactor `jitDepth` (`shading.cpp`) to delegate to the interpreter's depth implementation.
- [ ] T047 [P] [US5] Refactor `jitDuVector`/`jitDvVector` (`shading.cpp`) to delegate to the interpreter's `Du`/`Dv` implementations.
- [ ] T048 [US5] Build; render a shader exercising all five constructs under the JIT backend before/after T044-T047; confirm output is byte-identical.
- [ ] T049 [US5] Run `ctest --test-dir build -L visual --output-on-failure`; confirm zero regressions.

**Checkpoint (US5 complete)**: all five helpers delegate to the interpreter's implementation; visual output unchanged. **STOP — wait for user confirmation before Phase 10 (Polish).**

---

## Phase 10: Polish & Cross-Cutting Concerns

**Purpose**: FR-011 performance verification and FR-010 doc corrections — deferred to last since they depend on every construct fixed in Phases 5-7 existing.

- [ ] T050 [P] Register the FR-011 manual-only performance-comparison `ctest` assertion (label `perf-manual`, exact name per `research.md` D4) per `quickstart.md` step 4; confirm it is excluded from default/CI `ctest` invocations.
- [ ] T051 [P] Manually run the `perf-manual` benchmark for each construct fixed under Phases 5-7 (`cfrom`, `mfrom`, `ctransform`, matrix arithmetic, comparison/logic, array move, `gather` if not descoped); confirm JIT time ≤ 90% of interpreter time per FR-011/SC-006; record results.
- [ ] T052 [P] Update `DEVNOTES_DETAILS/BUGS.md`: extend the `cfrom` entry to cover `mfrom`/`ctransform`; move to Resolved.
- [ ] T053 [P] Correct `DEVNOTES_DETAILS/OSHADER_UPDATES.md` and `CLAUDE.md` gotcha #3: remove the false "unrecognised opcode warning" claim; remove/adjust the `jitSymbolRetain.cpp` reference unless T025/T038 ended up genuinely needing it.
- [ ] T054 Run the full `quickstart.md` validation guide end-to-end as a final sanity check (all 5 steps).
- [ ] T055 Run `ctest --test-dir build -L visual --output-on-failure` (full suite, including `motion-3-reyes`) and `ctest --test-dir build -L libshader --output-on-failure` one final time; confirm everything green.

**Final checkpoint**: Feature complete. **STOP — wait for user confirmation before considering this feature done and before any commit.**

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies.
- **Foundational (Phase 2)**: Depends on Setup — BLOCKS all user stories.
- **US2 / Phase 3**: Depends on Foundational (needs T005's `gatherHeader` fix for accurate `gather` triage).
- **US4 part 1 / Phase 4**: Depends on Foundational (needs `kHandledOpcodes`) AND US2/Phase 3 (needs the confirmed-reachable set) — this is why it is sequenced before US1 in this document despite US1 being P1: the coverage-guard test must exist and be RED before Phase 5's fixes land (Constitution Principle III).
- **US1 / Phase 5**: Depends on Foundational + US4-part-1 (Red state established). Independently testable/shippable once its own checkpoint passes — does not require US3 or US5.
- **US3a / Phase 6** and **US3b / Phase 7**: Depend on Foundational + US2 (confirmed-reachable set). Independent of US1. US3b depends on US3a only in document ordering, not technically — it could run first or in parallel with different staffing, but is sequenced after US3a here per the user's split (simpler/lower-risk work checkpointed before the higher-risk `gather()` scaffolding).
- **US4 part 2 / Phase 8**: Depends on US1 (Phase 5) + US3a (Phase 6) + US3b (Phase 7, or its T035 descope) all being complete — this is where the guard's 100%-green state is confirmed.
- **US5 / Phase 9**: Depends only on Foundational. Not dependent on US1/US3/US4 content-wise, but sequenced last (P3, lowest priority, pure risk-reduction with no currently-known bug) per spec.md's priority ordering.
- **Polish (Phase 10)**: Depends on all prior phases (perf benchmarks need every fixed construct; doc fixes reference the merged bug entries).

### Parallel Opportunities

- T007-T010 (US2 triage repros, four different opcode categories) run in parallel.
- T029-T031 (US3a wrapper additions, three different opcode categories) run in parallel.
- T044-T047 (US5 helper refactors, four different functions in the same file but independent bodies) can be developed in parallel then merged.
- T050-T053 (Polish: perf test registration, benchmark runs, and the three doc-file corrections) run in parallel.
- Once Foundational + US2 are both complete, US1 (Phase 5) and US3a/US3b (Phases 6-7) have no technical dependency on each other and could be staffed in parallel — they are presented sequentially here only because the user requested sequential checkpoints, not because of a code dependency.

---

## Parallel Example: Phase 3 (User Story 2 triage)

```bash
Task: "Write matrix-arithmetic triage repro in tests/libshader/triage/matrix_ops.sl"
Task: "Write gather() triage repro in tests/libshader/triage/gather.sl"
Task: "Write comparison/logic triage repro in tests/libshader/triage/comparison_logic.sl"
Task: "Write array move-op triage repro in tests/libshader/triage/array_ops.sl"
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Phase 1 (Setup) → Phase 2 (Foundational) → Phase 3 (US2 triage, needed for T012's guard-test content) → Phase 4 (US4 part 1, Red guard) → Phase 5 (US1).
2. **STOP and VALIDATE** at Phase 5's checkpoint: `cfrom`/`mfrom`/`ctransform` parity achieved, coverage guard partially green, documented workaround (`Option "shaderformat" "default" ["rslo"]`) no longer necessary for this family.
3. This is the smallest fully-shippable increment — matches spec.md's own MVP framing for US1.

### Incremental Delivery

1. Setup + Foundational + US2 + US4-part-1 → Red-state guard ready.
2. Add US1 → validate independently → MVP.
3. Add US3a → validate independently.
4. Add US3b (`gather()`) → validate independently, or descope with documented rationale.
5. Add US4-part-2 → confirm guard fully green and dynamic.
6. Add US5 → validate independently (no behavior change expected).
7. Polish → FR-011 benchmarks + doc corrections + full final validation.

Each step adds value without breaking previous steps; every checkpoint is a valid stopping point if priorities shift.

---

## Notes

- [P] tasks touch different files/opcode categories with no dependencies between them.
- [Story] label maps each task to its spec.md user story for traceability.
- **Every checkpoint in this document is a mandatory stop** — do not proceed to the next phase or sub-milestone without explicit user confirmation, per the user's instruction for this task list.
- Tests are written before the corresponding fix where TDD sequencing applies (T012/T013's coverage-guard test before Phases 5-7's fixes; T007-T010's triage repros before Phase 3's inventory is finalized).
- Commit only when the user explicitly asks — this feature's standing instruction (from `/speckit-specify` through `/speckit-plan`) is no automatic commits at any step, including at checkpoints.
- Avoid: reimplementing shading math independently in any `op_*` wrapper (FR-007, non-negotiable); changing interpreter (`.rslo`) behavior (FR-009); checking the coverage guard at render runtime (FR-006).
