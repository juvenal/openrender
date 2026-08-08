---

description: "Task list for NURBS Trim Curves (RiTrimCurve)"
---

# Tasks: NURBS Trim Curves (RiTrimCurve)

**Input**: Design documents from `/specs/009-nurbs-trim-curves/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/attribute-contract.md,
contracts/shared-trim-test-contract.md, quickstart.md — all present and read.

**Tests**: No standalone unit-test framework exists for this area; verification is via the project's visual-regression
suite (`ctest -L visual`), per `quickstart.md`. Test tasks below are therefore visual-regression scenes + reference
images, not a separate `tests/` tree.

**Organization**: Tasks are grouped by user story (priority order: US1, US2, US4, US3, US5) so each story can be
implemented and tested independently. A second, cross-cutting **Dependency Levels** view (L0–L7) is provided in
"Dependencies & Execution Order" below — every task in a level has zero dependency on any other task in that same
level, so all tasks sharing a level can be worked in parallel regardless of which phase they appear in above.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files/functions, no dependency on an incomplete task)
- **[Story]**: Maps the task to US1/US2/US3/US4/US5 from spec.md; Setup/Foundational/Polish tasks carry no story label
- File paths are exact; line-number anchors are current-`master` references from plan.md/data-model.md/contracts/

<!-- Sample tasks from the template have been replaced with the actual task list below. -->

## Phase 1: Setup

**Purpose**: Scaffolding for the new visual-regression scenes this feature requires.

- [ ] T001 Create `examples/rib/tests/` and `examples/rib/tests/references/` directories for the new
  visual-regression scenes and reference images this feature introduces.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: The untrimmed-baseline lock-in (Constitution Principle III) plus every pure-additive declaration/doc
task that has no dependency on any other new code. **No user-story implementation task may begin until T002 (the
untrimmed baseline capture) is complete** — this is stricter than "foundational blocks stories" in the generic
template: it specifically blocks any task that writes code in `rendererContext.cpp`'s `RiTrimCurve` body,
`attributes.*`, `patches.*`, or `surface.cpp`, because Constitution Principle III (TDD) requires the pre-feature
reference to exist before the behavior it protects can be changed.

- [ ] T002 [P] Wrap `geometry/vase.rib`'s `NuPatch` body into `examples/rib/tests/nupatch-vase-untrimmed.rib`
  (`Display`/`WorldBegin`/camera), render it on unmodified `master`, and save the output as
  `examples/rib/tests/references/nupatch-vase-untrimmed.tif` — the reference image against which the finished
  feature's non-interference (US4, SC-002) is judged. (depends on T001)
- [ ] T003 Register `nupatch-vase-untrimmed` in `tests/visual/CMakeLists.txt` via `add_visual_test(...)` (new entry
  alongside the existing ~85), and confirm `ctest --test-dir build -L visual -R nupatch-vase-untrimmed
  --output-on-failure` passes trivially against itself. (depends on T002)
- [ ] T004 [P] Declare a new `"trimcurve"`/`"sense"` token constant in `src/ri/ri.h` (modeled on `RI_SHADERFORMAT`,
  `ri.h:341`).
- [ ] T005 [P] Define the token constant in `src/ri/ri.cpp` (modeled on `ri.cpp:213`).
- [ ] T006 [P] Add the Trim Loop type (`curveCount`, `order[]`, `knot[]`, `min[]`/`max[]`, `n[]`, `u[]`/`v[]`/`w[]`
  per `data-model.md`) and the `CAttributes::pendingTrimLoops` (heap-owned array, absent by default) /
  `CAttributes::trimSense` (enum, default `Inside`) fields in `src/ri/attributes.h` (~92).
- [ ] T007 [P] Add the Shared Trim Test type (`loopPolylines[]`, `sense` snapshot) and the
  `CNURBSPatchMesh::trimTest` field (owned, absent by default) in `src/ri/patches.h` (~167-186).
- [ ] T008 [P] Correct the stale line reference in `DEVNOTES_DETAILS/RISPEC_GAPS.md:9`
  (`rendererContext.cpp:3527` → the actual `RiTrimCurve` implementation location); leave the gap item unchecked
  until the feature lands (T036 closes it out).
- [ ] T009 [P] Update the `DEVNOTES.md` status table entry for NURBS Trim Curves to "in progress", per this repo's
  existing status-tracking conventions.
- [ ] T010 [P] Review `src/ri/ribOut.cpp:1115-1154` (`CRibOut::RiTrimCurve`) against the Trim Loop field layout
  defined in `data-model.md` and confirm no update is required for round-trip compatibility (FR-014), or record the
  specific mismatch to fix in a follow-up task if one is found.
- [ ] T011 [P] Implement the Shared Trim Test classification function — polyline flattening from
  `(curveCount, order[], knot[], min[]/max[], n[], u[]/v[]/w[])` plus an O(polyline edges) odd-crossing-count
  point-in-polygon test, parameterized by trim sense (FR-005, FR-006, FR-018) — as new, self-contained geometry code
  in `src/ri/patches.h`/`src/ri/patches.cpp`, with no dependency on `CAttributes` or mesh construction.

**Checkpoint**: Untrimmed baseline is locked in; token, storage fields, doc corrections, ribOut review, and the
standalone classification function all exist. User-story implementation may now begin.

---

## Phase 3: User Story 1 - Cut a hole or boundary out of a NURBS surface (Priority: P1) 🎯 MVP

**Goal**: A `TrimCurve` loop declared before a `NuPatch` removes the enclosed region from the rendered surface,
identically across the reyes and ray-tracing hiders, while the rest of the surface is unaffected.

**Independent Test**: Render a scene containing a single `NuPatch` preceded by a `TrimCurve` loop that cuts a simple
closed shape out of the surface's parameter domain; confirm the trimmed region is absent while the untrimmed portion
matches an equivalent untrimmed scene, verified identically across the reyes and ray-tracing hiders.

### Implementation for User Story 1

- [ ] T012 [P] [US1] Implement the `RiTrimCurve` body in `src/ri/rendererContext.cpp` (replacing the
  `CODE_INCAPABLE` stub at ~4094-4100): parse `ncurves`/`order`/`knot`/`min`/`max`/`n`/`u`/`v`/`w` into Trim Loop
  array(s) — `nloops` is implicit from `ncurves`'s length, no explicit RIB token — and store into
  `CAttributes::pendingTrimLoops`, replacing any prior loops in the current scope (FR-003). (depends on T006)
- [ ] T013 [P] [US1] Integrate trim consumption into `CNURBSPatchMesh::create()` in `src/ri/patches.cpp`
  (~1823-1891): if `pendingTrimLoops` is absent, `trimTest` stays null and the rest of `create()` is byte-for-byte
  unchanged (FR-004); otherwise flatten surviving loops via T011's function and store the result as `trimTest`.
  Every per-Bezier-span child (~1874) references — never copies — the parent mesh's `trimTest` (FR-009). (depends
  on T006, T007, T011)
- [ ] T014 [US1] Add per-loop weight validation to T013's trim path in `CNURBSPatchMesh::create()`: any control
  point with `w <= 0` rejects that whole loop and emits one diagnostic warning naming the affected `NuPatch`/loop
  (FR-019); because `create()` runs exactly once per mesh regardless of `ObjectInstance` count, this naturally
  satisfies the once-per-distinct-loop dedup rule (FR-020/R6). (depends on T013)
- [ ] T015 [US1] Add per-loop closure validation to the same trim path in `CNURBSPatchMesh::create()`: a loop whose
  curves are not head-to-tail closed is implicitly closed (connect last flattened point to first) and emits one
  diagnostic warning naming the affected `NuPatch`/loop (FR-017). Sequenced after T014 since both edit the same
  function region. (depends on T014)
- [ ] T016 [P] [US1] Integrate classification into `CPatch::dice()`'s per-vertex probe in `src/ri/surface.cpp`
  (~141+): if the owning mesh's `trimTest` is absent, the existing path is unchanged (FR-004); otherwise call
  T011's classification function and exclude non-retained vertices from the diced grid the same way an
  out-of-bounds vertex is excluded today. (depends on T013, T011)
- [ ] T017 [P] [US1] Integrate classification into `CTesselationPatch` (ctor `src/ri/surface.cpp:552`,
  `intersect()`:635, `splitToChildren()`:1914, `tesselate()`:1450; declared `src/ri/surface.h:61`): call the
  identical classification function directly (not `dice()`, since `tesselationList`, `surface.cpp:539`, is a
  separate on-demand structure) with the same exclusion semantics, so the ray-tracing hider matches the reyes
  result (FR-010, FR-012). (depends on T013, T011)
- [ ] T018 [P] [US1] Author `examples/rib/tests/nupatch-vase-trimmed-hole.rib` (default/reyes hider, single
  circular trim loop cutting a hole in the vase body), render it, capture its reference image, and register it in
  `tests/visual/CMakeLists.txt`. Position the loop so it crosses at least one non-origin Bezier-span boundary of
  the vase's multi-span knot vector (not just the mesh's first span), so this scene also exercises FR-009's
  global-knot-range mapping for spans away from the origin span, not only the origin span itself. (depends on
  T012, T014, T015, T016, T017)
- [ ] T019 [P] [US1] Author `examples/rib/tests/nupatch-vase-trimmed-hole-raytrace.rib` (same scene with
  `Hider "raytrace"`), render it, capture its reference image, and register it in `tests/visual/CMakeLists.txt`,
  for the SC-008 cross-hider comparison against T018's output. (depends on T012, T014, T015, T016, T017)

**Checkpoint**: User Story 1 is fully functional and independently testable — a single trim loop correctly cuts a
hole under both the reyes and ray-tracing hiders.

---

## Phase 4: User Story 2 - Trim curves apply for as long as they remain the current attribute (Priority: P1)

**Goal**: Trim state follows ordinary `CAttributes` push/pop scoping — `AttributeBegin`/`AttributeEnd` correctly
isolates it, and repeated `NuPatch` calls with no intervening scope change or new `TrimCurve` all see the same trim.

**Independent Test**: Render `AttributeBegin`/`TrimCurve`/`NuPatch`/`AttributeEnd` followed by a sibling untrimmed
`NuPatch`, and confirm only the first surface is trimmed; separately, render one `TrimCurve` followed by two
consecutive `NuPatch` calls and confirm both are trimmed identically.

### Implementation for User Story 2

- [ ] T020 [P] [US2] Implement `pendingTrimLoops`/`trimSense` deep-copy in `CAttributes`'s copy constructor,
  `src/ri/attributes.cpp` (~156-211) — required per FR-013; a missed deep-copy here is a use-after-free on
  `AttributeEnd`, not a cosmetic bug. (depends on T006)
- [ ] T021 [P] [US2] Implement `pendingTrimLoops` free logic in `~CAttributes()`'s destructor,
  `src/ri/attributes.cpp` (~219-264), matching the existing pattern for every other heap-owned attribute field.
  (depends on T006)
- [ ] T022 [US2] Implement the zero-loop `TrimCurve` clearing behavior (`ncurves` of length zero → clear
  `pendingTrimLoops` in the current scope, FR-003, Acceptance Scenario 3) in the `RiTrimCurve` body,
  `src/ri/rendererContext.cpp`. Extends T012's function body. (depends on T012)
- [ ] T023 [US2] Author `examples/rib/tests/nupatch-vase-trimmed-scoping.rib` — one section wrapping
  `TrimCurve`/`NuPatch` in `AttributeBegin`/`AttributeEnd` followed by a sibling untrimmed `NuPatch`, and a second
  section with one `TrimCurve` followed by two consecutive `NuPatch` calls — render it, capture its reference
  image, and register it in `tests/visual/CMakeLists.txt`. (depends on T020, T021, T022, and US1's T016, T017)

**Checkpoint**: User Stories 1 and 2 are both independently functional — trim scoping matches every other RenderMan
attribute's push/pop behavior.

---

## Phase 5: User Story 4 - Untrimmed NURBS rendering is completely unaffected (Priority: P1)

**Goal**: Prove the additive-only constraint (FR-004): once trim code exists, a scene that never issues `TrimCurve`
renders byte-for-byte as it did on unmodified `master`.

**Independent Test**: Re-render the T002 baseline scene after the feature lands and confirm the output matches the
pre-feature reference image within existing visual-regression thresholds.

### Verification for User Story 4

- [ ] T024 [US4] Re-render `examples/rib/tests/nupatch-vase-untrimmed.rib` now that US1's FR-004 fast path exists
  (`CNURBSPatchMesh::create()`'s absent-`trimTest` branch from T013, and `CPatch::dice()`'s absent-`trimTest` branch
  from T016/T017) and confirm it still matches T002's pre-feature reference image (SC-002). This only needs US1's
  core integration, not US2/US3/US5's additive work. (depends on T002, T013, T016, T017)

**Checkpoint**: Non-interference is proven as soon as US1's core integration lands — independent of every later
story's work.

---

## Phase 6: User Story 3 - Invert which side of a trim loop is kept (Priority: P2)

**Goal**: `Attribute "trimcurve" "sense" ["outside"]` inverts which region a trim loop discards, without touching
the trim curve's control data.

**Independent Test**: Render the same `TrimCurve`/`NuPatch` scene twice — default sense, then explicit
`"outside"` — and confirm the kept/discarded regions are exact complements.

### Implementation for User Story 3

- [ ] T025 [P] [US3] Implement the `"trimcurve"`/`"sense"` `RiAttributeV` parsing block in
  `src/ri/rendererContext.cpp` (modeled on the `RI_SHADERFORMAT` block at ~3336-3338 — **not** the unrelated
  `RiOption`-scoped `RI_SHADERFORMAT` handling at ~1732-1747), accepting `"inside"`/`"outside"` (FR-006). (depends
  on T004, T006)
- [ ] T026 [P] [US3] Add the pre-declaration entry for `"trimcurve"`/`"sense"` in `initDeclarations()`,
  `src/ri/rendererDeclarations.cpp` (modeled on ~179) — required, or the RIB parser rejects the attribute before
  T025's parsing block is ever reached. (depends on T004)
- [ ] T027 [P] [US3] Confirm T011's classification function is correctly parameterized by trim sense (`Inside`
  discards the enclosed region, `Outside` keeps only the enclosed region, FR-006); adjust if it is not already
  wired that way. (depends on T011)
- [ ] T028 [US3] Author `examples/rib/tests/nupatch-vase-trimmed-sense.rib` (US1's trim loop plus
  `Attribute "trimcurve" "sense" ["outside"]`), render it, capture its reference image, and register it in
  `tests/visual/CMakeLists.txt`. (depends on T025, T026, T027, and US1's T016, T017)

**Checkpoint**: User Stories 1, 2, 4, and 3 are all independently functional.

---

## Phase 7: User Story 5 - Multiple loops describe islands and holes correctly (Priority: P3)

**Goal**: Disjoint holes and nested island-within-a-hole loop combinations resolve correctly under the same
odd-crossing-count rule, with no special-casing.

**Independent Test**: Render a `NuPatch` with three trim loops (one outer hole plus two disjoint holes) and confirm
all three resolve independently with the rest of the surface intact.

### Implementation for User Story 5

- [ ] T029 [P] [US5] Verify T011's classification handles multi-loop composition (disjoint holes, and a nested
  island-within-a-hole) correctly via plain odd-crossing-count over the concatenated `loopPolylines[]`, with no
  per-shape special-casing (FR-005, Acceptance Scenario 2); fix if a defect is found. (depends on T011)
- [ ] T030 [US5] Author `examples/rib/tests/nupatch-vase-trimmed-multiloop.rib` (one outer hole plus two disjoint
  holes), render it, capture its reference image, and register it in `tests/visual/CMakeLists.txt`. (depends on
  T029, and US1's T016, T017)

**Checkpoint**: All five user stories are independently functional and testable.

---

## Phase 8: Polish & Cross-Cutting Concerns

**Purpose**: Malformed-loop diagnostics (edge cases spanning no single story), RIB round-trip, final regression
sweep, and documentation close-out.

- [ ] T031 [P] Author, render, capture reference, and register a malformed-loop diagnostic scene (an unclosed trim
  loop) in `tests/visual/CMakeLists.txt`; confirm the renderer does not crash and emits exactly one warning via
  T015's implicit-closure path (FR-017). (depends on T015)
- [ ] T032 [P] Author, render, capture reference, and register a malformed-loop diagnostic scene (a `w <= 0`
  control point) in `tests/visual/CMakeLists.txt`; confirm the loop is rejected and exactly one warning is emitted
  via T014's rejection path (FR-019). (depends on T014)
- [ ] T033 Author `examples/rib/tests/nupatch-vase-trimmed-malformed-instanced.rib`, referencing the T031/T032
  malformed geometry via multiple `ObjectInstance` calls in a shared scene; render it, capture its reference
  image, and register it in `tests/visual/CMakeLists.txt`; confirm the warning still appears exactly once, not
  once per instance (FR-020). (depends on T031, T032)
- [ ] T034 [P] Verify the RIB round-trip (FR-014, SC-006): run `orender -writerib` on
  `examples/rib/tests/nupatch-vase-trimmed-hole.rib` and grep the output for an equivalent `TrimCurve` statement.
  (depends on T018, T010)
- [ ] T035 Run the full `ctest --test-dir build -L visual --output-on-failure` suite and confirm 100% pass (SC-002),
  including every scene registered by T003, T018, T019, T023, T024, T028, T030, T031, T032, T033. While running,
  observe that wall-clock time for the pre-existing (non-trim) scenes is unchanged within normal run-to-run
  variance versus the pre-feature baseline (SC-007), per `quickstart.md` Step 7 — no new timing harness is
  introduced; this is an observational check alongside the pass/fail sweep. (depends on all registration tasks:
  T003, T018, T019, T023, T024, T028, T030, T031, T032, T033)
- [ ] T036 Mark the `DEVNOTES_DETAILS/RISPEC_GAPS.md:9` gap entry resolved and finalize the `DEVNOTES.md` status
  table entry for NURBS Trim Curves (completes T008/T009). (depends on T035)

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — start immediately.
- **Foundational (Phase 2)**: T002/T003 depend on T001; T004-T011 depend on nothing and can start immediately
  alongside T001. **T002 (untrimmed baseline capture) blocks every task that writes trim implementation code** —
  not just User Story 4 — per Constitution Principle III.
- **User Story 1 (Phase 3)**: Depends on Foundational (T006, T007, T011 specifically).
- **User Story 2 (Phase 4)**: Depends on Foundational (T006) and, for its test scene (T023), on US1's tessellation
  integration (T016, T017).
- **User Story 4 (Phase 5)**: Depends only on T002 and US1's core integration (T013, T016, T017) — it does **not**
  need US2/US3/US5, so it can close out as soon as US1 lands.
- **User Story 3 (Phase 6)**: Depends on Foundational (T004, T011) and, for its test scene (T028), on US1's
  tessellation integration.
- **User Story 5 (Phase 7)**: Depends on Foundational (T011) and, for its test scene (T030), on US1's tessellation
  integration.
- **Polish (Phase 8)**: T031-T034 depend on their respective story tasks; T035 depends on every registration task;
  T036 depends on T035.

### User Story Dependencies

- **User Story 1 (P1)**: No dependency on any other story — the MVP.
- **User Story 2 (P1)**: Independently testable; its test scene additionally exercises US1's tessellation
  integration but adds no new tessellation code itself.
- **User Story 4 (P1)**: Independently testable; depends only on US1's core integration, not on US2/US3/US5.
- **User Story 3 (P2)**: Independently testable; its test scene reuses US1's trim loop but adds no new
  classification code beyond confirming sense parameterization.
- **User Story 5 (P3)**: Independently testable; confirms an emergent property of US1's classification function
  rather than adding new logic.

### Within Each User Story

- Attribute/storage plumbing before mesh integration before tessellation integration before test scenes.
- A story's test-scene task is always its last task (convergence point — needs the full chain working).

### Dependency Levels (cross-cutting DAG)

Every task in a level has **zero dependency on any other task in that same level** and depends only on tasks in
strictly lower levels — this is derived directly from the file/data ownership in `data-model.md` and the
producer/consumer relationships in `contracts/*.md`, not from story-priority order. All tasks in a level can be
worked simultaneously regardless of which phase/story they belong to above.

| Level | Tasks | Count |
|---|---|---|
| **L0** | T001, T004, T005, T006, T007, T008, T009, T010, T011 | 9 |
| **L1** | T002, T012, T013, T020, T021, T025, T026, T027, T029 | 9 |
| **L2** | T003, T014, T016, T017, T022 | 5 |
| **L3** | T015, T023, T024, T028, T030, T032 | 6 |
| **L4** | T018, T019, T031 | 3 |
| **L5** | T033, T034 | 2 |
| **L6** | T035 | 1 |
| **L7** | T036 | 1 |

Exact per-level task lists (derivation for the table above — every task's level is exactly one more than the
highest level among its own dependencies):

- **L0** (9, all mutually independent — different files, pure additive declarations, no shared state): T001, T004,
  T005, T006, T007, T008, T009, T010, T011.
- **L1** (9, each depends only on one or more L0 tasks): T002 (needs T001), T012 (needs T006), T013 (needs T006,
  T007, T011), T020 (needs T006), T021 (needs T006), T025 (needs T004, T006), T026 (needs T004), T027 (needs T011),
  T029 (needs T011).
- **L2** (5, each depends on exactly one L1 task): T003 (needs T002), T014 (needs T013), T016 (needs T013, T011),
  T017 (needs T013, T011), T022 (needs T012).
- **L3** (6, each depends on at least one L2 task): T015 (needs T014), T023 (needs T020, T021, T022, T016, T017),
  T024 (needs T002, T013, T016, T017), T028 (needs T025, T026, T027, T016, T017), T030 (needs T029, T016, T017),
  T032 (needs T014).
- **L4** (3, each depends on at least one L3 task): T018 (needs T012, T014, T015, T016, T017), T019 (same as T018),
  T031 (needs T015).
- **L5** (2, each depends on at least one L4 task): T033 (needs T031, T032), T034 (needs T018, T010).
- **L6** (1): T035 (needs every registration task: T003, T018, T019, T023, T024, T028, T030, T031, T032, T033).
- **L7** (1): T036 (needs T035).

The table and the bulleted list above are the same grouping — the table is generated directly from the bulleted
derivation, so either can be used when assigning parallel work.

### Parallel Opportunities

- All L0 tasks (9) can run together — see per-level example below.
- Within User Story 1: T012 and T013 (L1) run in parallel; T016 and T017 (L2) run in parallel once T013 lands;
  T018 and T019 (L4) run in parallel once T012/T014/T015/T016/T017 land.
- User Story 4's entire verification (T024) can proceed the moment US1's T013/T016/T017 land — no need to wait for
  US2, US3, or US5.
- User Stories 2, 3, and 5's non-scene implementation tasks (T020/T021, T025/T026/T027, T029) can all be worked in
  parallel with each other and with User Story 1's own implementation, since none of them touch a file User Story 1
  touches until their respective test-scene tasks converge.

---

## Parallel Example: Level L0 (Foundational)

```bash
Task: "Create examples/rib/tests/ and examples/rib/tests/references/ directories"           # T001
Task: "Declare trimcurve/sense token constant in src/ri/ri.h"                                # T004
Task: "Define the token constant in src/ri/ri.cpp"                                           # T005
Task: "Add Trim Loop type + pendingTrimLoops/trimSense fields in src/ri/attributes.h"         # T006
Task: "Add Shared Trim Test type + trimTest field in src/ri/patches.h"                        # T007
Task: "Correct stale line reference in DEVNOTES_DETAILS/RISPEC_GAPS.md:9"                     # T008
Task: "Update DEVNOTES.md status table entry"                                                # T009
Task: "Review src/ri/ribOut.cpp:1115-1154 for Trim Loop layout compatibility"                 # T010
Task: "Implement Shared Trim Test classification function in src/ri/patches.h/.cpp"           # T011
```

## Parallel Example: Level L1

```bash
Task: "Capture untrimmed-NuPatch baseline scene + reference image"                           # T002
Task: "Implement RiTrimCurve body in src/ri/rendererContext.cpp"                              # T012
Task: "Integrate trim consumption into CNURBSPatchMesh::create() in src/ri/patches.cpp"       # T013
Task: "Implement pendingTrimLoops/trimSense deep-copy in CAttributes copy ctor"                # T020
Task: "Implement pendingTrimLoops free logic in ~CAttributes() destructor"                     # T021
Task: "Implement trimcurve/sense RiAttributeV parsing block"                                   # T025
Task: "Add trimcurve/sense pre-declaration entry in rendererDeclarations.cpp"                  # T026
Task: "Confirm classification function is parameterized by trim sense"                         # T027
Task: "Verify classification handles multi-loop composition"                                   # T029
```

## Parallel Example: User Story 1

```bash
# T012 and T013 first (both only need Foundational):
Task: "Implement RiTrimCurve body in src/ri/rendererContext.cpp"                              # T012
Task: "Integrate trim consumption into CNURBSPatchMesh::create()"                             # T013

# T016 and T017 once T013 lands:
Task: "Integrate classification into CPatch::dice() in src/ri/surface.cpp"                    # T016
Task: "Integrate classification into CTesselationPatch in src/ri/surface.cpp"                 # T017

# T018 and T019 once the full US1 chain lands:
Task: "Author + capture nupatch-vase-trimmed-hole.rib"                                        # T018
Task: "Author + capture nupatch-vase-trimmed-hole-raytrace.rib"                                # T019
```

---

## Implementation Strategy

### MVP First (User Story 1, with User Story 4 as its safety net)

1. Complete Phase 1: Setup (T001).
2. Complete Phase 2: Foundational (T002-T011) — **T002's baseline capture is the hard gate**, not just a checklist
   item.
3. Complete Phase 3: User Story 1 (T012-T019).
4. Complete Phase 5: User Story 4's verification (T024) — proves US1 didn't disturb existing rendering.
5. **STOP and VALIDATE**: `ctest -L visual -R nupatch-vase` for the untrimmed + trimmed-hole scenes; confirm SC-001
   and SC-002 for this subset.
6. Deploy/demo if ready — the hole-cutting MVP is already spec-compliant on its own.

### Incremental Delivery

1. Setup + Foundational → foundation ready, baseline locked in.
2. Add User Story 1 → validate independently → MVP.
3. Add User Story 4's verification → confirms non-interference as soon as it's checkable (needs only US1).
4. Add User Story 2 → validate independently → attribute scoping confirmed correct.
5. Add User Story 3 → validate independently → sense inversion confirmed correct.
6. Add User Story 5 → validate independently → multi-loop composition confirmed correct.
7. Polish: malformed-loop diagnostics, RIB round-trip, full suite, docs close-out.

### Maximum-Parallelism Strategy (levels, not stories)

With enough concurrent capacity, ignore story boundaries and work strictly by level instead — this is the fastest
path since it exposes 9 parallel tasks at L0 and L1 alike, versus a handful at a time under story-sequential
delivery:

1. L0 (9 tasks) → L1 (9 tasks) → L2 (5 tasks) → L3 (6 tasks) → L4 (3 tasks) → L5 (2 tasks) → L6 (1 task) → L7
   (1 task).
2. Within each level, assign every listed task to a different contributor/agent; none of them can conflict by
   construction (that is the level invariant).
3. This converges on the same finished feature as the story-sequential path, just with a shorter critical path
   (8 levels deep vs. sequentially completing 5 story phases).

---

## Notes

- `[P]` tasks touch different files or clearly-separated regions of the same file with no completed-task
  dependency between them at that point — verified against `data-model.md`/`contracts/*.md`, not assumed.
- Where a real convergence point exists (e.g. `CNURBSPatchMesh::create()` in T013 needing the `CAttributes` field,
  the `trimTest` field, and the classification function all to exist first), it is marked as depending on all three
  rather than being labeled `[P]` to inflate the parallelism count.
- T014 and T015 are intentionally sequential (not `[P]`) despite both being "validation" work, because they edit
  the same function region of `CNURBSPatchMesh::create()` — a real same-file conflict risk, not a fabricated one.
- FR-015 (Python/Lua bindings continue to function without signature changes) has no dedicated task: this feature
  never touches `src/python/`, `src/lua/`, or any RI call signature (per plan.md's Project Structure), so FR-015 is
  satisfied by non-modification rather than by a verification step.
- Commit after each task or logical group, per this repo's standard workflow.
- Stop at any story checkpoint to validate that story independently before continuing.
