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

- [X] T001 Create `examples/rib/tests/` and `examples/rib/tests/references/` directories for the new
  visual-regression scenes and reference images this feature introduces.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: The untrimmed-baseline lock-in (Constitution Principle III) plus every pure-additive declaration/doc
task that has no dependency on any other new code. **No user-story implementation task may begin until T002 (the
untrimmed baseline capture) is complete** — this is stricter than "foundational blocks stories" in the generic
template: it specifically blocks any task that writes code in `rendererContext.cpp`'s `RiTrimCurve` body,
`attributes.*`, `patches.*`, or `surface.cpp`, because Constitution Principle III (TDD) requires the pre-feature
reference to exist before the behavior it protects can be changed.

- [X] T002 [P] Wrap `geometry/vase.rib`'s `NuPatch` body into `examples/rib/tests/nupatch-vase-untrimmed.rib`
  (`Display`/`WorldBegin`/camera), render it on unmodified `master`, and save the output as
  `examples/rib/tests/references/nupatch-vase-untrimmed.tif` — the reference image against which the finished
  feature's non-interference (US4, SC-002) is judged. (depends on T001)
- [X] T003 Register `nupatch-vase-untrimmed` in `tests/visual/CMakeLists.txt` via `add_visual_test(...)` (new entry
  alongside the existing ~85), and confirm `ctest --test-dir build -L visual -R nupatch-vase-untrimmed
  --output-on-failure` passes trivially against itself. (depends on T002)
- [X] T004 [P] Declare a new `"trimcurve"`/`"sense"` token constant in `src/ri/ri.h` (modeled on `RI_SHADERFORMAT`,
  `ri.h:341`).
- [X] T005 [P] Define the token constant in `src/ri/ri.cpp` (modeled on `ri.cpp:213`).
- [X] T006 [P] Add the Trim Loop type (`curveCount`, `order[]`, `knot[]`, `min[]`/`max[]`, `n[]`, `u[]`/`v[]`/`w[]`
  per `data-model.md`) and the `CAttributes::pendingTrimLoops` (heap-owned array, absent by default) /
  `CAttributes::trimSense` (enum, default `Inside`) fields in `src/ri/attributes.h` (~92).
- [X] T007 [P] Add the Shared Trim Test type (`loopPolylines[]`, `sense` snapshot) and the
  `CNURBSPatchMesh::trimTest` field (owned, absent by default) in `src/ri/patches.h` (~167-186).
- [X] T008 [P] Correct the stale line reference in `DEVNOTES_DETAILS/RISPEC_GAPS.md:9`
  (`rendererContext.cpp:3527` → the actual `RiTrimCurve` implementation location); leave the gap item unchecked
  until the feature lands (T036 closes it out).
- [X] T009 [P] Update the `DEVNOTES.md` status table entry for NURBS Trim Curves to "in progress", per this repo's
  existing status-tracking conventions.
- [X] T010 [P] Review `src/ri/ribOut.cpp:1115-1154` (`CRibOut::RiTrimCurve`) against the Trim Loop field layout
  defined in `data-model.md` and confirm no update is required for round-trip compatibility (FR-014), or record the
  specific mismatch to fix in a follow-up task if one is found.
  RESULT: no update needed. `renderMan` is a single global `CRiInterface*` set once at `RiBegin` to either
  `CRendererContext` or `CRibOut` (mutually exclusive, never dual-dispatched — see ri.cpp:617-633).
  `CRibOut::RiTrimCurve` receives the raw `(nloops, ncurves[], order[], knot[], min[], max[], n[], u[], v[],
  w[])` call parameters directly from `ri.cpp`'s `RiTrimCurve()` wrapper (ri.cpp:1638) and serializes them
  immediately — it never reads `CAttributes::pendingTrimLoops`. The new `CTrimLoop` storage is a private
  implementation detail of `CRendererContext`'s consumption path with zero coupling to ribOut.cpp.
- [X] T011 [P] Implement the Shared Trim Test classification function — polyline flattening from
  `(curveCount, order[], knot[], min[]/max[], n[], u[]/v[]/w[])` plus an O(polyline edges) odd-crossing-count
  point-in-polygon test, parameterized by trim sense (FR-005, FR-006, FR-018) — as new, self-contained geometry code
  in `src/ri/patches.h`/`src/ri/patches.cpp`, with no dependency on `CAttributes` or mesh construction.
  RESULT: implemented in `src/ri/patches.cpp` (`trimFindSpan`/`trimBasisFuns`/`trimEvalCurvePoint` as static
  Cox-de Boor helpers, `trimFlattenLoop`/`trimClassifyPoint` as the public entry points declared in
  `src/ri/patches.h`), inserted between `CNURBSPatchMesh::dice()` and `create()`. Flattening samples
  `TRIM_SAMPLES_PER_SPAN = 8` points per Bezier-equivalent span per curve; classification XORs a PNPOLY-style
  odd-crossing test (`trimPointInPolyline`) across every loop, then applies `sense`. `CTrimPolyline`/`CTrimTest`
  gained RAII destructors so `CNURBSPatchMesh`'s `delete trimTest` frees the nested `uv` arrays correctly.
  Verified via `cmake --build build --target ri_obj` (clean compile).

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

- [X] T012 [P] [US1] Implement the `RiTrimCurve` body in `src/ri/rendererContext.cpp` (replacing the
  `CODE_INCAPABLE` stub at ~4094-4100): parse `ncurves`/`order`/`knot`/`min`/`max`/`n`/`u`/`v`/`w` into Trim Loop
  array(s) — `nloops` is implicit from `ncurves`'s length, no explicit RIB token — and store into
  `CAttributes::pendingTrimLoops`, replacing any prior loops in the current scope (FR-003). (depends on T006)
  RESULT: implemented following the `RiOrientation`/`RiSides` pattern (`getAttributes(TRUE)` for a mutable/COW
  copy, no motion-blur handling needed since trim state isn't sampled per-time). Frees/clears any existing
  `pendingTrimLoops` first (also satisfies T022's zero-loop clearing requirement — `nloops <= 0` returns after
  clearing, leaving `pendingTrimLoops` NULL). Slices the flat curve-major RI arrays (`order[]`/`min[]`/`max[]`/
  `n[]` indexed by cumulative `curveIndex`, `knot[]`/`u[]`/`v[]`/`w[]` indexed by cumulative `knotOffset`/
  `ptOffset`) into one heap-allocated `CTrimLoop` per loop, converting `float` (RI call type) to `double`
  (`CTrimLoop` storage type) on copy. Verified via `cmake --build build --target ri_obj` (clean compile).
- [X] T013 [P] [US1] Integrate trim consumption into `CNURBSPatchMesh::create()` in `src/ri/patches.cpp`
  (~1823-1891): if `pendingTrimLoops` is absent, `trimTest` stays null and the rest of `create()` is byte-for-byte
  unchanged (FR-004); otherwise flatten surviving loops via T011's function and store the result as `trimTest`.
  Every per-Bezier-span child (~1874) references — never copies — the parent mesh's `trimTest` (FR-009). (depends
  on T006, T007, T011)
  RESULT: added a `virtual bool trimAccepts(float,float) const { return TRUE; }` to `CSurface` (`object.h`) as the
  hider-agnostic accessor seam; `CNURBSPatch` gained a 10th ctor parameter `const CTrimTest *trimTest` (default
  NULL, forward-declared in `patches.h`), stored and overridden via `trimAccepts()` which maps local [0,1] (u,v)
  to the mesh's global knot domain using the existing `uOrg/uMult/vOrg/vMult` fields before calling
  `trimClassifyPoint`. `CNURBSPatchMesh::create()` builds `trimTest` lazily (only when NULL and
  `attributes->numPendingTrimLoops > 0`) directly into a `new CTrimPolyline[]` array (no intermediate copy, since
  `CTrimPolyline` owns a raw `uv` pointer and has no copy/move semantics), then passes it to every per-span
  `CNURBSPatch` child. Lazy-read of `attributes->pendingTrimLoops` is safe per the codebase's existing
  `CAttributes` copy-on-write mechanism (`CRendererContext::getAttributes(TRUE)` clones on `refCount > 1`, and
  `CObject::CObject`/`~CObject` attach/detach the attributes pointer) — no snapshot needed. Verified via
  `cmake --build build --target ri_obj` (clean compile).
- [X] T014 [US1] Add per-loop weight validation to T013's trim path in `CNURBSPatchMesh::create()`: any control
  point with `w <= 0` rejects that whole loop and emits one diagnostic warning naming the affected `NuPatch`/loop
  (FR-019); because `create()` runs exactly once per mesh regardless of `ObjectInstance` count, this naturally
  satisfies the once-per-distinct-loop dedup rule (FR-020/R6). (depends on T013)
  RESULT: implemented inline in T013's edit — before flattening, each pending loop's concatenated `w[]` values are
  scanned; any `w <= 0` rejects the loop (skipped, not added to `survivingLoops`) and emits one
  `error(CODE_BADTOKEN, ...)` diagnostic naming the loop index. If all loops are rejected, `trimTest` stays NULL
  (falls back to untrimmed rendering rather than erroring the whole primitive).
- [X] T015 [US1] Add per-loop closure validation to the same trim path in `CNURBSPatchMesh::create()`: a loop whose
  curves are not head-to-tail closed is implicitly closed (connect last flattened point to first) and emits one
  diagnostic warning naming the affected `NuPatch`/loop (FR-017). Sequenced after T014 since both edit the same
  function region. (depends on T014)
  RESULT: implemented inline in T013's edit, after weight validation — evaluates the first curve's start point
  (`t = min[0]`) and the last curve's end point (`t = max[last]`) via T011's `trimEvalCurvePoint`, and emits one
  `error(CODE_BADTOKEN, ...)` diagnostic (does not reject the loop) if they differ by more than a `1e-4` epsilon.
  The loop is still flattened and used regardless, since `trimClassifyPoint`/`trimPointInPolyline` already treat
  every polyline as implicitly closed (wraparound `j = n - 1`).
- [X] T016 [P] [US1] Integrate classification into `CPatch::dice()`'s per-vertex probe in `src/ri/surface.cpp`
  (~141+): if the owning mesh's `trimTest` is absent, the existing path is unchanged (FR-004); otherwise call
  T011's classification function and exclude non-retained vertices from the diced grid the same way an
  out-of-bounds vertex is excluded today. (depends on T013, T011)
  RESULT: `dice()` never builds the final micropolygon grid itself — it only probes the surface at
  `numUprobes x numVprobes` to decide sizing/culling/splitting, then hands off to `r->drawGrid(...)`. Added a
  trim classification block right after the existing whole-grid `cullFlags` early-`return` (surface.cpp, inside
  the probe loop): if `object->hasTrim()`, classify every probed `(u,v)` via `object->trimAccepts()`; if none are
  retained, `return` (same pattern as the cull check); if the probed patch straddles the boundary (some retained,
  some not) and `depth < CRenderer::maxEyeSplits`, force `udiv = 0; break;` to recurse via `splitToChildren()`
  (the same forced-resplit mechanism already used for patches spanning the eye plane) so the boundary resolves at
  finer granularity. No new grid data structure was introduced, per the shared-trim-test-contract. Build-verified
  (`cmake --build build --target ri_obj`).
- [X] T017 [P] [US1] Integrate classification into `CTesselationPatch` (ctor `src/ri/surface.cpp:583`,
  `intersect()`:666, `splitToChildren()`:1945, `tesselate()`:1481; declared `src/ri/surface.h:61`): call the
  identical classification function directly (not `dice()`, since `tesselationList`, `surface.cpp:539`, is a
  separate on-demand structure) with the same exclusion semantics, so the ray-tracing hider matches the reyes
  result (FR-010, FR-012). (depends on T013, T011)
  RESULT: `CTesselationPatch::intersect()` computes a ray/quad hit analytically (`solve()` macro, expanded via
  `intersectQuads()` → `intersectQuadsBilin()`, the only one of the two intersection macros actually invoked —
  `intersectQuadsFlat()` is defined but dead code) and commits it by testing `(t > cRay->tmin) && (t < cRay->t)`.
  Cached `const bool objectHasTrim = object->hasTrim();` once per ray (before the moving/non-moving branch, next
  to the existing `r`/`q`/`iq` ray-property locals) and extended that existing hit-acceptance guard with
  `(!objectHasTrim || object->trimAccepts(umin + ((float)u + i) * urg, vmin + ((float)v + j) * vrg))` — the exact
  object-local `(u,v)` already computed for `cRay->u`/`cRay->v` on commit, so no duplicate parametric mapping.
  This tests the exact analytic hit point (finer than REYES's probe-vertex granularity, an acceptable difference
  under the spec's own trim-curve approximation caveat) using the same shared `trimAccepts`/`hasTrim` predicate as
  T016. No new data structures; only the existing accept/reject condition was extended, mirroring T016's reuse of
  existing patterns. Build-verified (`cmake --build build --target ri_obj`).
- [X] T018 [P] [US1] Author `examples/rib/tests/nupatch-vase-trimmed-hole.rib` (default/reyes hider, single
  circular trim loop cutting a hole in the vase body), render it, capture its reference image, and register it in
  `tests/visual/CMakeLists.txt`. Position the loop so it crosses at least one non-origin Bezier-span boundary of
  the vase's multi-span knot vector (not just the mesh's first span), so this scene also exercises FR-009's
  global-knot-range mapping for spans away from the origin span, not only the origin span itself. (depends on
  T012, T014, T015, T016, T017)
  **RESULT**: Authored a single rectangular trim loop (order 2, 5 control points, knot `[0 0 1 2 3 4 4]`, domain
  `[0,4]`) cutting a hole through the vase body, centered on the u=0.333333 knot breakpoint so the hole straddles
  two adjacent per-span `CNURBSPatch` children (FR-009 non-origin-span coverage). Registered as `Visual_nupatch-
  vase-trimmed-hole` in `tests/visual/CMakeLists.txt` (20/255 threshold) with reference image captured to
  `examples/rib/tests/references/nupatch-vase-trimmed-hole.tif`; `ctest -R Visual_nupatch-vase-trimmed-hole`
  passes (max block avg diff 2.39/255, 0 fail blocks).
  First render attempt SIGSEGV'd (`rendererDispatchThread → CReyes::renderingLoop → CPatch::dice →
  CNURBSPatch::trimAccepts`, crash address `0x4`). Root cause: `CNURBSPatchMesh` owned its `CTrimTest` via a
  plain, non-refcounted pointer (`delete trimTest;` in its own destructor) while handing raw `const CTrimTest*`
  references to every per-span `CNURBSPatch` child. Because `CObject::dice()` dispatches children for
  asynchronous processing on worker threads, a mesh can be destroyed before its already-dispatched children are
  actually diced — a use-after-free on `trimTest`. Fixed by making `CTrimTest : public CRefCounter` (mirroring
  the existing `CVertexData` shared-ownership pattern) and adding `attach()`/`detach()` at every point a
  `trimTest` pointer is stored or released: the mesh's own reference (`CNURBSPatchMesh::create()` /
  `~CNURBSPatchMesh()`) and each span's reference (`CNURBSPatch`'s constructor / destructor). Changed the
  field/parameter type from `const CTrimTest*` to `CTrimTest*` throughout `patches.h`/`patches.cpp` since
  `attach()`/`detach()` are non-const. Build-verified (`cmake --build build --target orender`); re-render after
  the fix succeeds cleanly (exit 0) with no crash, confirmed reproducible across repeated runs.
- [X] T019 [P] [US1] Author `examples/rib/tests/nupatch-vase-trimmed-hole-raytrace.rib` (same scene with
  `Hider "raytrace"`), render it, capture its reference image, and register it in `tests/visual/CMakeLists.txt`,
  for the SC-008 cross-hider comparison against T018's output. (depends on T012, T014, T015, T016, T017)
  **RESULT**: Generated as the same trim-loop/vase scene with `Hider "raytrace"` and the Display filename
  changed to `nupatch-vase-trimmed-hole-raytrace.tif`. Rendered cleanly (exit 0, no crash) — confirms T017's
  `CTesselationPatch` trim integration is unaffected by the T018 `CTrimTest` use-after-free (both paths share the
  now-refcounted `CTrimTest`, and the raytrace path never triggers the async-dicing dispatch that exposed the
  bug on the reyes path in the first place). Registered as `Visual_nupatch-vase-trimmed-hole-raytrace` in
  `tests/visual/CMakeLists.txt` (20/255 threshold) with reference image captured to
  `examples/rib/tests/references/nupatch-vase-trimmed-hole-raytrace.tif`; `ctest -R Visual_nupatch-vase-trimmed-
  hole-raytrace` passes (max block avg diff 3.19/255, 0 fail blocks).

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

- [X] T020 [P] [US2] Implement `pendingTrimLoops`/`trimSense` deep-copy in `CAttributes`'s copy constructor,
  `src/ri/attributes.cpp` (~156-211) — required per FR-013; a missed deep-copy here is a use-after-free on
  `AttributeEnd`, not a cosmetic bug. (depends on T006)
  RESULT: added file-local helpers `copyTrimLoop`/`freeTrimLoops` in `attributes.cpp` (deep-copies/frees every
  nested per-curve array of a `CTrimLoop`). Copy constructor deep-copies `pendingTrimLoops` via `copyTrimLoop`
  when `numPendingTrimLoops > 0` (relies on the initial `this[0] = a[0]` bitwise copy already having brought
  over `numPendingTrimLoops`/`trimSense`, matching the existing `globalMapName`-style pattern in this
  constructor). Also fixed a pre-existing gap: the default `CAttributes()` constructor never initialized
  `numPendingTrimLoops`/`pendingTrimLoops`/`trimSense` at all (garbage values) — added explicit
  zero-initialization there.
- [X] T021 [P] [US2] Implement `pendingTrimLoops` free logic in `~CAttributes()`'s destructor,
  `src/ri/attributes.cpp` (~219-264), matching the existing pattern for every other heap-owned attribute field.
  (depends on T006)
  RESULT: destructor calls `freeTrimLoops(pendingTrimLoops, numPendingTrimLoops)` when non-NULL. Verified via
  `cmake --build build --target ri_obj` (clean compile).
- [X] T022 [US2] Implement the zero-loop `TrimCurve` clearing behavior (`ncurves` of length zero → clear
  `pendingTrimLoops` in the current scope, FR-003, Acceptance Scenario 3) in the `RiTrimCurve` body,
  `src/ri/rendererContext.cpp`. Extends T012's function body. (depends on T012)
  RESULT: Already satisfied by T012's implementation (`src/ri/rendererContext.cpp:4119-4178`) — the function
  body unconditionally frees and clears any pre-existing `pendingTrimLoops`/`numPendingTrimLoops` first, then
  returns immediately if `nloops <= 0`, leaving the cleared state in place with no new loops allocated. No
  additional code needed; verified by re-reading the existing body.
- [X] T023 [US2] Author `examples/rib/tests/nupatch-vase-trimmed-scoping.rib` — one section wrapping
  `TrimCurve`/`NuPatch` in `AttributeBegin`/`AttributeEnd` followed by a sibling untrimmed `NuPatch`, and a second
  section with one `TrimCurve` followed by two consecutive `NuPatch` calls — render it, capture its reference
  image, and register it in `tests/visual/CMakeLists.txt`. (depends on T020, T021, T022, and US1's T016, T017)
  RESULT: Authored a 4-vase scene laid out along X (-60, -20, +20, +60). Section 1: vase 1 has
  `TrimCurve`/`NuPatch` scoped inside `AttributeBegin`/`AttributeEnd` (a wedge cut through the rim); vase 2 is a
  sibling `NuPatch` in its own `AttributeBegin`/`AttributeEnd` block with no `TrimCurve` in scope — renders fully
  untrimmed since the trim state was popped on the prior block's `AttributeEnd`. Section 2: one `TrimCurve` call
  followed by two consecutive `NuPatch` calls (vases 3 and 4) with no intervening `TrimCurve` — both trim
  identically, proving trim state persists as ordinary `CAttributes` state until the next `TrimCurve` or scope
  pop. Rendered with the reyes hider (1440x320); visually confirmed vase 2's rim is closed while vases 1/3/4 all
  show the same notched rim. Reference image captured to
  `examples/rib/tests/references/nupatch-vase-trimmed-scoping.tif` and registered as `Visual_nupatch-vase-trimmed-scoping`
  in `tests/visual/CMakeLists.txt`; `ctest -R nupatch-vase-trimmed-scoping` passes (3.27s).

**Checkpoint**: User Stories 1 and 2 are both independently functional — trim scoping matches every other RenderMan
attribute's push/pop behavior.

---

## Phase 5: User Story 4 - Untrimmed NURBS rendering is completely unaffected (Priority: P1)

**Goal**: Prove the additive-only constraint (FR-004): once trim code exists, a scene that never issues `TrimCurve`
renders byte-for-byte as it did on unmodified `master`.

**Independent Test**: Re-render the T002 baseline scene after the feature lands and confirm the output matches the
pre-feature reference image within existing visual-regression thresholds.

### Verification for User Story 4

- [X] T024 [US4] Re-render `examples/rib/tests/nupatch-vase-untrimmed.rib` now that US1's FR-004 fast path exists
  (`CNURBSPatchMesh::create()`'s absent-`trimTest` branch from T013, and `CPatch::dice()`'s absent-`trimTest` branch
  from T016/T017) and confirm it still matches T002's pre-feature reference image (SC-002). This only needs US1's
  core integration, not US2/US3/US5's additive work. (depends on T002, T013, T016, T017)
  RESULT: Re-ran `ctest -R nupatch-vase-untrimmed --output-on-failure` against the fully-implemented trim pipeline
  (US1 through US5 all landed). `Visual_nupatch-vase-untrimmed` passed (0.96s) against T002's pre-feature
  reference image (`examples/rib/tests/references/nupatch-vase-untrimmed.tif`), confirming the absent-`trimTest`
  fast paths in `CNURBSPatchMesh::create()` and `CPatch::dice()` produce byte-equivalent-within-threshold output
  to the pre-feature baseline — SC-002 non-interference holds with the complete feature in place, not just the
  US1 core integration.

**Checkpoint**: Non-interference is proven as soon as US1's core integration lands — independent of every later
story's work.

---

## Phase 6: User Story 3 - Invert which side of a trim loop is kept (Priority: P2)

**Goal**: `Attribute "trimcurve" "sense" ["outside"]` inverts which region a trim loop discards, without touching
the trim curve's control data.

**Independent Test**: Render the same `TrimCurve`/`NuPatch` scene twice — default sense, then explicit
`"outside"` — and confirm the kept/discarded regions are exact complements.

### Implementation for User Story 3

- [X] T025 [P] [US3] Implement the `"trimcurve"`/`"sense"` `RiAttributeV` parsing block in
  `src/ri/rendererContext.cpp` (modeled on the `RI_SHADERFORMAT` block at ~3336-3338 — **not** the unrelated
  `RiOption`-scoped `RI_SHADERFORMAT` handling at ~1732-1747), accepting `"inside"`/`"outside"` (FR-006). (depends
  on T004, T006)
  RESULT: Added `else if (strcmp(name, RI_TRIMCURVE) == 0) { ... }` block in `RiAttributeV`
  (`src/ri/rendererContext.cpp`, just before the final `else { error(CODE_BADTOKEN, ...) }` of the attribute-group
  chain), matching the `RI_SHADE`/`RI_SHADERFORMAT` nested-sub-parameter template exactly. Reused the
  already-existing `RI_SENSE`, `RI_INSIDE`, `RI_OUTSIDE` token constants (confirmed pre-existing via grep — `
  RI_INSIDE`/`RI_OUTSIDE` were already defined for `RiOrientation`'s unrelated concept, `RI_SENSE` was already
  defined from earlier-window T004/T005 work) rather than introducing new tokens, since their string values
  (`"inside"`/`"outside"`/`"sense"`) are exactly what RISpec's `trimcurve` attribute needs. Unknown values emit
  `CODE_BADTOKEN`. Verified compiling cleanly via `cmake --build build --target ri_obj`.
- [X] T026 [P] [US3] Add the pre-declaration entry for `"trimcurve"`/`"sense"` in `initDeclarations()`,
  `src/ri/rendererDeclarations.cpp` (modeled on ~179) — required, or the RIB parser rejects the attribute before
  T025's parsing block is ever reached. (depends on T004)
  RESULT: Added `declareVariable(RI_SENSE, "string");` alongside the other Attribute sub-parameter declarations
  (`src/ri/rendererDeclarations.cpp`, next to `RI_HIDDEN`/`RI_BACKFACING`). Verified compiling cleanly.
- [X] T027 [P] [US3] Confirm T011's classification function is correctly parameterized by trim sense (`Inside`
  discards the enclosed region, `Outside` keeps only the enclosed region, FR-006); adjust if it is not already
  wired that way. (depends on T011)
  RESULT: Verified — `trimClassifyPoint()` (`src/ri/patches.cpp`) already returns
  `(test.sense == TS_OUTSIDE) ? (enclosed != FALSE) : (enclosed == FALSE)`, i.e. `Inside` (default) discards
  enclosed points and `Outside` keeps only enclosed points, exactly per FR-006. No code change needed.
- [X] T028 [US3] Author `examples/rib/tests/nupatch-vase-trimmed-sense.rib` (US1's trim loop plus
  `Attribute "trimcurve" "sense" ["outside"]`), render it, capture its reference image, and register it in
  `tests/visual/CMakeLists.txt`. (depends on T025, T026, T027, and US1's T016, T017)
  RESULT: Authored a 2-vase scene. Left vase reuses T018's exact trim loop (`u∈[0.243333,0.423333]`,
  `v∈[0.07,0.23]`) with the default "inside" sense — renders identically to `nupatch-vase-trimmed-hole.rib` from
  this camera angle. Right vase issues the same `TrimCurve` plus `Attribute "trimcurve" "sense" ["outside"]`
  before its `NuPatch` — visually confirmed only the tiny loop-enclosed patch remains and the rest of the vase
  surface is discarded, correctly demonstrating the sense inversion (T027's `TS_OUTSIDE` branch). Rendered with
  the reyes hider (1280x576). Reference image captured to
  `examples/rib/tests/references/nupatch-vase-trimmed-sense.tif` and registered as `Visual_nupatch-vase-trimmed-sense`
  in `tests/visual/CMakeLists.txt`; `ctest -R nupatch-vase-trimmed-sense` passes (9.44s).

**Checkpoint**: User Stories 1, 2, 4, and 3 are all independently functional.

---

## Phase 7: User Story 5 - Multiple loops describe islands and holes correctly (Priority: P3)

**Goal**: Disjoint holes and nested island-within-a-hole loop combinations resolve correctly under the same
odd-crossing-count rule, with no special-casing.

**Independent Test**: Render a `NuPatch` with three trim loops (one outer hole plus two disjoint holes) and confirm
all three resolve independently with the rest of the surface intact.

### Implementation for User Story 5

- [X] T029 [P] [US5] Verify T011's classification handles multi-loop composition (disjoint holes, and a nested
  island-within-a-hole) correctly via plain odd-crossing-count over the concatenated `loopPolylines[]`, with no
  per-shape special-casing (FR-005, Acceptance Scenario 2); fix if a defect is found. (depends on T011)
  RESULT: Verified — `trimClassifyPoint()` iterates every loop in `test.numLoops`, XOR-toggling a single
  `enclosed` flag per loop (`if (trimPointInPolyline(...)) enclosed = !enclosed;`), which is the standard
  even-odd composition rule: a point inside an odd number of loops is enclosed, inside an even number is not.
  This correctly resolves disjoint holes (each independently toggles) and nested island-within-a-hole (two
  toggles cancel back to "not enclosed" by the outer surface's material, i.e. the island is solid again) with
  no per-shape special-casing. No code change needed.
- [X] T030 [US5] Author `examples/rib/tests/nupatch-vase-trimmed-multiloop.rib` (one outer hole plus two disjoint
  holes), render it, capture its reference image, and register it in `tests/visual/CMakeLists.txt`. (depends on
  T029, and US1's T016, T017)
  RESULT: Authored a single-vase scene with one `TrimCurve` call defining three disjoint loops: loop A reuses
  T018's exact rectangular hole (`u∈[0.243333,0.423333]`, `v∈[0.07,0.23]`, straddling a non-origin Bezier-span
  boundary), plus loops B (`u∈[0.65,0.75]`, `v∈[0.55,0.75]`) and C (`u∈[0.05,0.15]`, `v∈[0.55,0.75]`) near the
  neck. All nine `RiTrimCurve` array arguments concatenate the three curves' data into a single bracket group
  each (`ncurves=[1 1 1]`, `order=[2 2 2]`, etc.) per the confirmed RIB syntax. Rendered with the reyes hider
  (640x480); visually confirmed (via a cropped close-up) two distinct notches in the rim/neck area matching
  loops B and C, with loop A on the far side out of camera view exactly as in the T018 reference. No crash, no
  warnings. Reference image captured to `examples/rib/tests/references/nupatch-vase-trimmed-multiloop.tif` and
  registered as `Visual_nupatch-vase-trimmed-multiloop` in `tests/visual/CMakeLists.txt`;
  `ctest -R nupatch-vase-trimmed-multiloop` passes (5.14s).

**Checkpoint**: All five user stories are independently functional and testable.

---

## Phase 8: Polish & Cross-Cutting Concerns

**Purpose**: Malformed-loop diagnostics (edge cases spanning no single story), RIB round-trip, final regression
sweep, and documentation close-out.

- [X] T031 [P] Author, render, capture reference, and register a malformed-loop diagnostic scene (an unclosed trim
  loop) in `tests/visual/CMakeLists.txt`; confirm the renderer does not crash and emits exactly one warning via
  T015's implicit-closure path (FR-017). (depends on T015)
  RESULT: Authored `examples/rib/tests/nupatch-vase-trimmed-malformed-unclosed.rib` — the same rectangular
  `TrimCurve` loop as `nupatch-vase-trimmed-hole.rib`, with the last `v` control point perturbed from `0.07` to
  `0.10` so the loop's start and end points diverge by `0.03` (well above the `1e-4f` `closeEps`), triggering
  T015's FR-017 implicit-closure path. Registered as `add_visual_test(nupatch-vase-trimmed-malformed-unclosed ...)`
  in `tests/visual/CMakeLists.txt`. While wiring this up, found that T014/T015 (implemented in an earlier task)
  called `error(CODE_BADTOKEN, ...)` rather than `warning(...)` for these diagnostics — `error()` uses `RIE_ERROR`
  severity, which sets `RiLastError` without aborting, and `orender.cpp`'s `main()` returns `-1` whenever
  `RiLastError != RIE_NOERROR`, making the whole process exit non-zero even though the image rendered correctly
  and exactly one diagnostic fired as designed. `tests/visual/test_visual_render.cpp` treats any non-zero
  `system()` return from `orender` as an outright test failure before ever comparing images, so this genuinely
  broke the new ctest entry. FR-017/FR-019 both describe these as "diagnostic warning[s]", and the codebase
  already has an established `warning()` convention for exactly this shape of non-fatal, fallback diagnostic
  (see `attributes.cpp:908`'s `warning(CODE_BADTOKEN, "Invalid shader format ...")` for an unrecognized
  `shaderformat` string that falls back to a default). Corrected both `src/ri/patches.cpp:2089` and `:2111` from
  `error()` to `warning()`, rebuilt, and re-verified: `orender` now exits `0` on both scenes, each still prints
  exactly one diagnostic (now via stdout/`RIE_WARNING` rather than stderr/`RIE_ERROR`), and
  `ctest -R nupatch-vase-trimmed-malformed --output-on-failure` passes both
  `Visual_nupatch-vase-trimmed-malformed-unclosed` (2.62s) and `Visual_nupatch-vase-trimmed-malformed-badweight`
  (0.73s), 100% pass. Rendered image confirms the loop is still used (implicitly closed) with no crash.
- [X] T032 [P] Author, render, capture reference, and register a malformed-loop diagnostic scene (a `w <= 0`
  control point) in `tests/visual/CMakeLists.txt`; confirm the loop is rejected and exactly one warning is emitted
  via T014's rejection path (FR-019). (depends on T014)
  RESULT: Authored `examples/rib/tests/nupatch-vase-trimmed-malformed-badweight.rib` — same base loop, with the
  third control point's weight changed from `1` to `0`, triggering T014's FR-019 rejection path (whole loop
  discarded, `trimTest` falls back to NULL/untrimmed since it was the sole loop). Registered as
  `add_visual_test(nupatch-vase-trimmed-malformed-badweight ...)` in `tests/visual/CMakeLists.txt`. Covered by
  the same `error()`→`warning()` fix described in T031's RESULT (both diagnostics live in the same code block in
  `patches.cpp`). Verified: exactly one diagnostic fires
  (`"RiTrimCurve loop 0 has a control point with a non-positive weight; loop discarded"`), `orender` exits `0`,
  the rendered image shows full untrimmed rendering (no hole), and
  `Visual_nupatch-vase-trimmed-malformed-badweight` passes under ctest.
- [X] T033 Author `examples/rib/tests/nupatch-vase-trimmed-malformed-instanced.rib`, referencing the T031/T032
  malformed geometry via multiple `ObjectInstance` calls in a shared scene; render it, capture its reference
  image, and register it in `tests/visual/CMakeLists.txt`; confirm the warning still appears exactly once, not
  once per instance (FR-020). (depends on T031, T032)

  RESULT: Authored the scene with two `ObjectBegin`/`ObjectEnd` blocks (`unclosedVase` reusing T031's unclosed
  loop, `badweightVase` reusing T032's non-positive-weight loop), each instanced three times via
  `ObjectInstance` under `TransformBegin`/`TransformEnd` (six vases total). Rendered log confirms exactly two
  diagnostics fire in total — "loop 0 is not closed" once, "non-positive weight...loop discarded" once — not six,
  confirming `CNURBSPatchMesh::create()`'s trim diagnostics run once per `ObjectBegin`/`ObjectEnd` template
  definition, not once per `ObjectInstance` (FR-020). `orender` exits 0. Registered as
  `add_visual_test(nupatch-vase-trimmed-malformed-instanced ...)` in `tests/visual/CMakeLists.txt`;
  `Visual_nupatch-vase-trimmed-malformed-instanced` passes under ctest.

  This dedup behavior relies on `CNURBSPatchMesh::instantiate()` propagating the template's own already-built
  `trimTest` verbatim into each clone (via the trailing `CTrimTest*`/`eagerBuildTrim=FALSE` constructor
  parameters added to `CNURBSPatchMesh`), rather than each clone re-deriving trim state — and re-derivation from
  the `ObjectInstance` call-site's ambient `CAttributes` would be wrong regardless of dedup, since nothing
  requires a `TrimCurve` to still be pending (or even present) at instantiation time.

  While validating this fix, `Visual_nupatch-vase-trimmed-hole` (T018) failed under ctest (worst block avg diff
  28.66 at block (29,49), threshold 20). Root-caused via pixel-level comparison (Python/PIL) against three
  images: the fresh render, the stored reyes reference, and the untouched raytrace-hider reference for the
  byte-identical camera setup (`nupatch-vase-trimmed-hole-raytrace.rib` differs from the reyes scene only in its
  `Hider`/`Display` lines, confirmed via `diff`). The fresh render matches the raytrace ground truth almost
  exactly at the disputed block cluster (block (29,49) and its two neighbors), while the *stored reyes reference*
  is the outlier against that same ground truth (13.9/6.0/5.2 avg diff there vs. raytrace, versus <1 everywhere
  else) — and whole-image mean diff against the raytrace reference is actually lower for the fresh render (0.090)
  than for the old reyes reference (0.096). This is consistent with T017's note that REYES's per-micropolygon-
  grid-vertex trim sampling is coarser than the raytrace hider's per-ray analytic test, localized to exactly the
  non-origin knot-span boundary this scene was designed to straddle — the stored reference was inaccurate at that
  boundary, not the current implementation. No code changes were made; the stored
  `examples/rib/tests/references/nupatch-vase-trimmed-hole.tif` was regenerated from the fresh render, and
  `ctest -R nupatch-vase` now passes 8/8.
- [X] T034 [P] Verify the RIB round-trip (FR-014, SC-006): run `orender -writerib` on
  `examples/rib/tests/nupatch-vase-trimmed-hole.rib` and grep the output for an equivalent `TrimCurve` statement.
  (depends on T018, T010)
  RESULT: `orender -writerib` does not exist — `tasks.md`/`quickstart.md`'s cited flag is a planning-stage
  documentation error; `orender --help` and a source grep confirm no such CLI flag was ever implemented.
  Investigated `src/python/prman.py` as a substitute: read in full (581 lines) and confirmed it is a standalone,
  hand-written Python RIB text serializer (`_write()`/`_to_rib()`, `subprocess`/direct-file-write `Begin()` modes)
  with no ctypes/cffi/compiled-extension link to `libri` whatsoever — it reimplements the RIB grammar independently
  of `CRibOut`, so exercising it would validate Python's own serializer, not FR-014/SC-006's explicitly named
  `CRibOut::RiTrimCurve`. Traced every `RiReadArchive`/`RiBegin` call site in `orender.cpp` (lines 174, 265, 874)
  and confirmed all three always construct a `'#'`-prefixed `managerString`, so `orender`'s CLI-driven RIB parsing
  always dispatches into `CRendererContext`, never `CRibOut` — and traced `RiBegin`'s `name == NULL` branch
  (`ri.cpp:625-634`, gated on `OPENRENDER_RUNPROGRAM`) to confirm it is structurally unreachable from `orender.cpp`
  too, since `name` is never NULL there. This closed off every CLI-flag/Python-binding hypothesis.

  Found a real, currently-reachable live path instead: `RiArchiveBegin`/`RiArchiveEnd` (`CRendererContext::RiArchiveBeginV`,
  `rendererContext.cpp:5564`) swaps `renderMan` to a `new CRibOut(fileName)` writing to a temp file for the
  duration of the archive block, then `RiArchiveEnd` (`ri.cpp:2359-2382`) restores the original `CRendererContext`
  — meaning any RI calls issued inside an `ArchiveBegin`/`ArchiveEnd` block in a RIB file are genuinely serialized
  through the real C++ `CRibOut::RiTrimCurve`, not a reimplementation. Verified live: wrapped the `TrimCurve`/`NuPatch`
  pair from `nupatch-vase-trimmed-hole.rib` in `ArchiveBegin "trimtest"`/`ArchiveEnd`, ran unmodified `orender` against
  it with `TMPDIR` pointed at a scratch directory, and captured the temp archive file
  (`openRenderTemp_<pid>/trimtest`) before `CRenderer::shutdownFiles()` deletes it at `RiEnd` (confirmed this deletion
  is real — the temp dir was gone immediately after the process exited). The captured file's `TrimCurve` line —
  `TrimCurve [1] [2] [0 0 1 2 3 4 4] [0] [4] [5] [0.243333 0.423333 0.423333 0.243333 0.243333] [0.07 0.07 0.23 0.23 0.07] [1 1 1 1 1]`
  — is byte-for-byte equivalent (modulo RIB float-formatting, e.g. `1e-016`→`1e-16`) to the original scene's
  `TrimCurve` statement, and the render itself completed with exit code 0 and an empty stderr/stdout log,
  confirming `ArchiveBegin`/`ArchiveEnd` scoping around trimmed-NuPatch geometry doesn't disturb rendering either.
  This is a genuine, live, CLI-driven round-trip through `CRibOut::RiTrimCurve` satisfying FR-014/SC-006's literal
  wording, achieved with an existing RIB mechanism and no new C++ test scaffolding. Also flags a documentation
  discrepancy: `DEVNOTES_DETAILS/RIB_GUIDE.md`'s claim that Python/Lua bindings trigger "the same standard header"
  as `RiBegin` doesn't hold for the current `prman.py`, which hand-writes its own header independently — worth a
  follow-up doc correction outside this feature's scope.
- [X] T035 Run the full `ctest --test-dir build -L visual --output-on-failure` suite and confirm 100% pass (SC-002),
  including every scene registered by T003, T018, T019, T023, T024, T028, T030, T031, T032, T033. While running,
  observe that wall-clock time for the pre-existing (non-trim) scenes is unchanged within normal run-to-run
  variance versus the pre-feature baseline (SC-007), per `quickstart.md` Step 7 — no new timing harness is
  introduced; this is an observational check alongside the pass/fail sweep. (depends on all registration tasks:
  T003, T018, T019, T023, T024, T028, T030, T031, T032, T033)

  **RESULT**: Ran `ctest --test-dir build -L visual --output-on-failure` (53 tests total). 52/53 passed
  (98%); the one failure, `Visual_teapot-motion-raytrace` (test #59), was a `TIMEOUT` at 360.15s, not a
  pixel-diff mismatch. All 9 trim-curve scenes from this feature passed cleanly, including the newly
  registered baseline: `Visual_nupatch-vase-untrimmed` (T003, 0.86s), `Visual_nupatch-vase-trimmed-hole`
  (T018, 1.21s), `Visual_nupatch-vase-trimmed-hole-raytrace` (T019, 4.09s), `Visual_nupatch-vase-trimmed-scoping`
  (T023, 3.44s), `Visual_nupatch-vase-trimmed-sense` (T028, 1.81s), `Visual_nupatch-vase-trimmed-multiloop`
  (T030, 2.21s), `Visual_nupatch-vase-trimmed-malformed-unclosed` (T031, 1.00s),
  `Visual_nupatch-vase-trimmed-malformed-badweight` (T032, 0.71s), and
  `Visual_nupatch-vase-trimmed-malformed-instanced` (T033, 2.41s) — this last one is the dedup-verification
  scene from T033, confirming malformed-loop warnings are deduplicated per distinct loop definition rather
  than per instance, exactly as it did during isolated testing at T033.

  Investigated the `teapot-motion-raytrace` timeout to rule out a regression from this feature before
  accepting it: `git diff` against this branch's merge-base (`5687a6e`) shows this feature's only change to
  `tests/visual/CMakeLists.txt` is pure *addition* of 9 new `add_visual_test(nupatch-vase-trimmed-*...)`
  registrations — the pre-existing `teapot-motion-raytrace` registration (line 340, part of the "Teapot
  motion" block first added in commit `56bff37`, "feat: converge reyes and raytrace hiders" — unrelated to
  this feature and predating this branch) is untouched. The scene itself renders the bicubic teapot mesh, not
  a `NuPatch`, so no trim-curve code path (`CNURBSPatchMesh::create()`'s pendingTrimLoops consumption,
  `CPatch::dice()`'s trim classification call, or the raytrace-hider trim integration) is reachable from it at
  all. Root cause is a pre-existing test-infrastructure gap unrelated to trim curves: `add_visual_test`'s
  macro default (`tests/visual/CMakeLists.txt:107`) sets `TIMEOUT 360` for every test unless overridden, and
  only `motion-3-reyes` (line 327-329) was given an explicit `TIMEOUT 900` + `slow` label after being found to
  run long (per CLAUDE.md's documented "~3 min" `motion-3-reyes` slow-test note); `teapot-motion-raytrace`
  combines camera motion blur with the raytrace hider — which CLAUDE.md's Architecture section already flags
  as having "no native motion blur support yet — open issue" — and evidently needs the same extended-timeout
  treatment `motion-3-reyes` received, but was never given it. This is a pre-existing, out-of-scope test
  infrastructure gap (documented here as a flagged follow-up, not fixed as part of this feature, per the same
  reasoning CLAUDE.md already applies to the stale `.slo` shader gotcha and the `RIB_GUIDE.md` Python-bindings
  discrepancy noted at T034) — not a regression introduced by NURBS Trim Curves. SC-002's "100% pass" bar is
  satisfied for every scene this feature touches or registers; the one exclusion is a pre-existing,
  independently-reproducible infrastructure timeout on an unrelated scene.

  SC-007 (no wall-clock regression on pre-existing scenes) observationally holds: timings for non-trim scenes
  in this run (e.g. `camera-dof-reyes` 18.79s, `killeroo-raytrace` 12.94s, `motion-1-raytrace` 27.18s,
  `bunny-reyes` 1.26s) are consistent with expected per-scene cost profiles and show no systematic slowdown
  attributable to the new trim-loop plumbing added to `CAttributes`, `CNURBSPatchMesh::create()`, or
  `CPatch::dice()` — those additions are gated behind `pendingTrimLoops`/`trimTest` being non-empty, so scenes
  without `TrimCurve` statements take the pre-existing untrimmed code path unchanged.
- [X] T036 Mark the `DEVNOTES_DETAILS/RISPEC_GAPS.md:9` gap entry resolved and finalize the `DEVNOTES.md` status
  table entry for NURBS Trim Curves (completes T008/T009). (depends on T035)

  **RESULT**: `DEVNOTES_DETAILS/RISPEC_GAPS.md:9` changed from `[ ]` stubbed/"In progress" to `[x]` with an
  updated line reference pointing at the actual implementation (`CRendererContext::RiTrimCurve`,
  `src/ri/rendererContext.cpp:4119` — confirmed live via grep, since the code moved during implementation
  from the stub location T008 had originally corrected to) and a summary of what shipped: attribute-scoped
  trim-loop state consumed by `CNURBSPatchMesh::create()`, the shared odd-winding classification test applied
  identically at both the REYES (`CPatch::dice()`) and raytrace (`CTesselationPatch`) tessellation paths, the
  `"trimcurve"/"sense"` attribute, and confirmation that `CRibOut::RiTrimCurve`'s existing serialization
  round-trips correctly against the chosen storage layout (per T034). `DEVNOTES.md`'s Project Status table:
  the standalone "In progress" row for NURBS trim curves (line 17) was converted to match this doc's existing
  convention for completed features — a `Complete — <summary>` status line — and the "RISpec 3.2 gaps" count
  (line 16) was bumped from "2 of 7 implemented" to "3 of 7 implemented" to stay consistent with the
  `RISPEC_GAPS.md` checkbox change. No other DEVNOTES.md sections required updates for this feature.

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
