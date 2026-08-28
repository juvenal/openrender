---

description: "Task list for Solid CSG Operations (013-solid-csg-operations)"
---

# Tasks: Solid CSG Operations

**Input**: Design documents from `/specs/013-solid-csg-operations/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/solid-rib-interface.md, quickstart.md

**Tests**: Included. Constitution Principle III (Test-Driven Development) is
NON-NEGOTIABLE for this project, and `research.md` Decision 6 / `plan.md`
Constitution Check commit explicitly to writing and approving failing
boolean-kernel unit tests before any `SolidBegin`/`SolidEnd` integration
code.

**Organization**: Tasks are grouped by user story (from `spec.md`) to enable
independent implementation and testing of each story.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (US1, US2, US3)
- File paths are exact, relative to the repository root

## Path Conventions

Single existing C++ project. All source changes are under `src/ri/`; new
unit tests under `tests/unit/csg/`; new visual-regression scenes under
`examples/rib/` (following the existing scene-example pattern referenced by
`ctest -L visual`); documentation under `site/`.

---

## Phase 1: Setup

**Purpose**: Establish a known-good baseline before any CSG code lands, so
FR-018/SC-005 ("zero regression on existing scenes") has something concrete
to be measured against.

- [ ] T001 Build the unmodified `013-solid-csg-operations` worktree
      (`cmake --build build --config Release`) and run
      `ctest --test-dir build -L visual --output-on-failure` and
      `ctest --test-dir build -L libshader --output-on-failure` to record a
      passing baseline before any of the tasks below begin.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: The plumbing every user story depends on — RIB block-state
enforcement, CSG tree capture, the resolved-boundary container type, and the
tessellation-tolerance attribute. No boolean-combination logic yet (that is
US1-scoped); a lone `"primitive"` block must already round-trip through this
phase unchanged.

**⚠️ CRITICAL**: No user story work can begin until this phase is complete.

- [ ] T002 Add the missing `RENDERMAN_SOLID_PRIMITIVE_BLOCK` push/pop to
      `RiSolidBegin`/`RiSolidEnd` in `src/ri/ri.cpp` (currently only call
      `check()`, unlike `RiObjectBegin`'s `blocks.push(currentBlock);
      currentBlock = RENDERMAN_OBJECT_BLOCK;` at the same site,
      `research.md` "RIB block-state enforcement gap") — this is what lets
      `check()`'s existing scope-mask machinery enforce FR-014 later.
- [ ] T003 [P] Add `currentSolid` / `savedSolids` state to
      `src/ri/rendererContext.h`, mirroring the existing `instance` /
      `instanceStack` members (~line 225-236).
- [ ] T004 Create the CSG Tree node type (`operation`, `operands`,
      `leafObjects`, `outerXform`, `parent` fields per `data-model.md`) in
      `src/ri/csgTree.h` / `src/ri/csgTree.cpp` (depends on T003).
- [ ] T005 Implement `RiSolidBegin`/`RiSolidEnd` capture/open/close in
      `src/ri/rendererContext.cpp` (replacing the unimplemented stub at
      lines 5502-5513): validate the operation-type string against
      `"primitive"`/`"union"`/`"intersection"`/`"difference"` (FR-001,
      reject invalid values per FR-013), push/pop `currentSolid` against
      `savedSolids`, and reject an unmatched or scope-mismatched
      `SolidEnd` (FR-014) (depends on T002, T003, T004).
- [ ] T006 Add the third `addObject()` capture gate — while `currentSolid`
      is open, divert each captured primitive's `CObject*` into the active
      CSG tree node instead of calling `CRenderer::render()`, alongside the
      existing instance/delayed gates — in `src/ri/rendererContext.cpp`
      (`addObject()`, line 457) (depends on T005).
- [ ] T007 [P] Add `"primitive"`-leaf validation in `src/ri/csgTree.cpp`:
      reject a nested `SolidBegin`/`SolidEnd` inside a `"primitive"` block
      (FR-019) and reject an `RiProcedural` captured directly inside a
      `"primitive"` block (`research.md` "Delayed/procedural primitives...
      resolved as rejected"), both via `error(CODE_BADTOKEN, ...)`
      (depends on T004).
- [ ] T008 Create the `CSolidObject : CObject` Resolved Solid Boundary
      container in `src/ri/solidObject.h` / `src/ri/solidObject.cpp`
      (`data-model.md`): owns a `children`/`sibling` list of `CPolygonMesh`
      Boundary Fragments, computes `bmin`/`bmax` as the union of fragment
      bounds, and presents them to every hider through the existing generic
      `CObject` dispatch — no new virtual methods (depends on T004).
- [ ] T009 Implement the outer-block coordinate-space transform (`research.md`
      Decision 5) in `src/ri/csgTree.cpp`: compose each captured leaf's
      `from` with the inverse of the outermost `SolidBegin`'s `from` to
      bring it into that block's local frame before resolution; give the
      resolved `CSolidObject` that same outer-block `CXform*` (depends on
      T004, T008).
- [ ] T010 Implement the two trivial tree-resolution shortcuts in
      `src/ri/csgTree.cpp`: an empty solid block (no captured leaves/
      operands) resolves to no geometry, not an error (FR-016), and a
      boolean block with exactly one operand resolves to that operand's
      boundary unchanged, skipping BSP combination entirely (FR-017)
      (depends on T004, T008).
- [ ] T011 [P] Add `Attribute "solid" "float tessellationtolerance"` via the
      existing four-layer attribute pattern (`CLAUDE.md`): token constant in
      `src/ri/ri.h`, RIB parsing in `RiAttributeV()`
      (`src/ri/rendererContext.cpp`), storage in `src/ri/attributes.h` /
      `src/ri/attributes.cpp`, pre-declaration in
      `src/ri/rendererDeclarations.cpp` (`contracts/solid-rib-interface.md`).
- [ ] T012 [P] Create the `tests/unit/csg/` directory and wire a new ctest
      label (extending the existing `-L libshader`-style unit-test pattern,
      `research.md` Decision 6 / `quickstart.md` §1) so
      `ctest --test-dir build -L csg` runs boolean-kernel unit tests once
      they exist.

**Checkpoint**: Foundation ready — a scene with a single
`SolidBegin "primitive"`/`SolidEnd` block (no boolean operation) already
round-trips to ordinary geometry with no regression, and `check()` correctly
rejects malformed block usage. User story implementation can now begin.

---

## Phase 3: User Story 1 - Compose shapes with boolean solid operations (Priority: P1) 🎯 MVP

**Goal**: `"union"`/`"intersection"`/`"difference"` solid blocks resolve two
or more overlapping primitives to one correct, smooth composite boundary,
identically across every hider.

**Independent Test**: Render a box/sphere pair wrapped first in `"union"`,
then `"intersection"`, then `"difference"`; each result matches the
geometrically expected boundary with no seams, gaps, or duplicate surfaces
(`quickstart.md` §2-3).

### Tests for User Story 1 ⚠️

> **Write these first — approve them failing (Red) before any implementation task below.**

- [ ] T013 [P] [US1] Unit test: two axis-aligned unit boxes with a known
      overlap sub-volume — assert expected face count and enclosed volume
      for union, intersection, and difference in
      `tests/unit/csg/test_boolean_boxes.cpp`.
- [ ] T014 [P] [US1] Unit test: a sphere combined with a box — validates
      curved-vs-flat boundary handling and that
      `Attribute "solid" "float tessellationtolerance"` changes output
      triangle density as expected, in
      `tests/unit/csg/test_boolean_sphere_box.cpp`.
- [ ] T015 [P] [US1] Unit test: an explicit coplanar-face pair — validates
      `C_EPSILON`-consistent classification (`common/algebra.h`, `1e-6`,
      `research.md` Decision 3 risk) in
      `tests/unit/csg/test_boolean_coplanar.cpp`.

### Implementation for User Story 1

- [ ] T016 [US1] Build a BSP tree over a tessellated operand mesh in
      `src/ri/csgBoolean.h` / `src/ri/csgBoolean.cpp` (`research.md`
      Decision 3) (depends on T013-T015 failing red, T008).
- [ ] T017 [US1] Implement classify/clip/merge boolean combination
      (union/intersection/difference) over two BSP trees, using the
      `C_EPSILON` classification epsilon, in `src/ri/csgBoolean.cpp`
      (depends on T016).
- [ ] T018 [US1] Implement `difference` as intersection with the second
      operand's complement — reverse winding/normals on faces retained from
      the subtracted operand, since the visible cut surface is its
      inward-facing side — in `src/ri/csgBoolean.cpp` (depends on T017).
- [ ] T019 [US1] Extract the flatness/chordal-deviation adaptive stopping
      criterion out of `CTesselationPatch::tesselate`
      (`src/ri/surface.cpp:1858-1897`) into a form callable without a
      traced ray: strip out the ray-footprint half of the stopping
      criterion (`surface.cpp:724-759`), keep the `uFlat < uAvg && vFlat <
      vAvg` chordal-deviation test driven from a tolerance value alone, in
      `src/ri/surface.h` / `src/ri/surface.cpp` (`research.md` Decision 4)
      (depends on T011).
- [ ] T020 [P] [US1] Add mesh tessellation for quadric leaf operands (never
      tessellated before this feature — `CSphere::intersect` etc. currently
      raytrace via pure algebraic root-solving,
      `src/ri/quadrics.cpp:200-`) using the extracted flatness criterion, in
      `src/ri/surface.cpp` (depends on T019).
- [ ] T021 [US1] Wire `src/ri/csgTree.cpp` leaf tessellation to call the
      extracted flatness criterion for NURBS/Bézier/quadric operands,
      reading the tolerance from `Attribute "solid"
      "tessellationtolerance"` when set, defaulting to the primitive's own
      object-space bound diagonal otherwise (depends on T019, T020, T011).
- [ ] T022 [US1] Compute an analytic per-vertex shading normal
      (`crossvv(dPdu, dPdv)` at each sample vertex's exact parametric
      coordinates) for NURBS- and quadric-sourced fragments, and store it as
      the `"N"` `CONTAINER_VERTEX` primvar on the resulting `CPolygonMesh`
      (`CPolygonTriangle::sample`/`interpolate` already interpolates any
      supplied `"N"` generically, `polygons.cpp:280-314,379-405`) in
      `src/ri/surface.cpp` and `src/ri/solidObject.cpp` (`research.md`
      Decision 4b) (depends on T019, T008).
- [ ] T023 [US1] Construct Boundary Fragments so each BSP output fragment
      carries its *originating leaf's* `CAttributes*` (`research.md`
      Decision 2, `data-model.md`) in `src/ri/csgBoolean.cpp` /
      `src/ri/solidObject.cpp` (depends on T017, T008).
- [ ] T024 [US1] At the outermost `RiSolidEnd`, resolve the CSG tree via
      `csgBoolean` into a `CSolidObject` and re-enter `addObject()` exactly
      like any other primitive, in `src/ri/rendererContext.cpp` (depends on
      T017, T023, T006).
- [ ] T025 [P] [US1] Add a visual-regression scene combining a sphere and a
      box with `"union"`, `"intersection"`, and `"difference"`, rendered
      with the raytrace, REYES, and `hidden` (z-buffer) hiders, in
      `examples/rib/` (`quickstart.md` §2-3, SC-001/SC-002).
- [ ] T026 [P] [US1] Add a visual-regression scene validating boundary
      smoothness on curved operands: tight vs. loose
      `tessellationtolerance` on a sphere/sphere union, a NURBS-operand
      variant, and a subdivision-surface-operand variant showing the
      accepted normal-quality asymmetry, in `examples/rib/`
      (`quickstart.md` §7, `research.md` Decision 4/4b) (depends on T022).

**Checkpoint**: User Story 1 is fully functional and independently
testable — boolean composition of two primitives is correct and
hider-identical, with curvature-adaptive, smooth-shaded boundaries.

---

## Phase 4: User Story 2 - Nest solid operations to build complex shapes (Priority: P2)

**Goal**: Solid blocks nested to arbitrary depth resolve to one correct
composite boundary regardless of nesting depth or operation mix.

**Independent Test**: Render a solid tree at least four levels deep
combining all three operation types
(`difference(union(A,B), intersection(C,D))`) and compare the resulting
silhouette against the manually reasoned expected shape (`quickstart.md`
§5).

### Tests for User Story 2 ⚠️

> **Write these first — approve them failing (Red) before the implementation task below.**

- [ ] T027 [P] [US2] Unit test: a nested tree resolves inner operands before
      the outer operation combines them (bottom-up), and a `"difference"`
      block with three or more operands subtracts every operand after the
      first in declaration order (FR-005), in
      `tests/unit/csg/test_nested_tree.cpp`.

### Implementation for User Story 2

- [ ] T028 [US2] Implement bottom-up recursive resolution over nested
      `CSGTreeNode` operands — each boolean node resolves its children
      (themselves possibly boolean nodes) before combining — in
      `src/ri/csgTree.cpp` (depends on T017, T027).
- [ ] T029 [P] [US2] Add a visual-regression scene with a solid tree at
      least four operation levels deep mixing union/intersection/difference,
      in `examples/rib/` (`quickstart.md` §5, SC-003).
- [ ] T030 [US2] Verify a `SolidBegin`/`SolidEnd` block declared inside
      `RiObjectBegin`/`RiObjectEnd` resolves once at object-definition time
      and each `RiObjectInstance` replay reuses that resolved result under
      its own transform (`research.md` Decision 1, `addObject()` gate
      precedence) — add a regression scene exercising this in
      `examples/rib/` and confirm no special-casing was needed in
      `src/ri/rendererContext.cpp` (depends on T006, T024).

**Checkpoint**: User Stories 1 and 2 both work independently — nesting of
arbitrary depth and mixed operations resolves correctly.

---

## Phase 5: User Story 3 - Shade a solid's interior differently from its exterior (Priority: P3)

**Goal**: A solid's Interior and/or Exterior shader governs its appearance
depending on whether the camera or a traced ray is inside or outside its
resolved volume.

**Independent Test**: Render a `"difference"` solid (sphere minus a smaller
centered sphere) with an Interior shader assigned, viewed so the cut-away
interior is visible; the interior-facing surface visibly differs from the
exterior-facing surface (`quickstart.md` §4).

### Implementation for User Story 3

> No dedicated unit test: Interior/Exterior consumption is a shading-pipeline
> behavior that requires ray/camera traversal to exercise meaningfully, so it
> is validated via the visual-regression scene below (SC-004), consistent
> with `research.md` Decision 6 scoping unit tests to the boolean kernel
> only.

- [ ] T031 [P] [US3] Consult `attributes->interior` and apply its shading
      effect when a camera or traced ray is inside a Boundary Fragment's
      resolved volume (FR-010), in `src/ri/shading.cpp` /
      `src/ri/shaderFunctions.h`.
- [ ] T032 [P] [US3] Consult `attributes->exterior` when viewed from outside
      a Boundary Fragment's resolved volume (FR-011), falling back to
      ordinary surface/atmosphere shading when neither Interior nor
      Exterior is set (FR-012), in `src/ri/shading.cpp`.
- [ ] T033 [US3] Restrict this Interior/Exterior consumption to Boundary
      Fragments of a `CSolidObject` only — Interior/Exterior assigned to
      attribute state that never enters a `SolidBegin`/`SolidEnd` block
      MUST remain a no-op exactly as today (FR-020) — in
      `src/ri/shading.cpp` (depends on T031, T032).
- [ ] T034 [P] [US3] Add a visual-regression scene: a `"difference"` solid
      (sphere minus sphere) with an Interior shader assigned, viewed from an
      angle revealing the cut-away interior, confirming visibly distinct
      interior vs. exterior appearance, in `examples/rib/`
      (`quickstart.md` §4, SC-004).

**Checkpoint**: All three user stories are independently functional.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Diagnostics, non-regression, and documentation that span all
three user stories.

- [ ] T035 [P] Add error/diagnostic scenes confirming a clear
      `CODE_BADTOKEN` diagnostic (not a crash, not a silently wrong image)
      for: `SolidBegin "bogus"`, an unmatched `SolidEnd`, a
      `SolidBegin "primitive"` block containing a nested
      `SolidBegin`/`SolidEnd`, and an `RiProcedural` directly inside a
      `SolidBegin "primitive"` block, in `examples/rib/` (`quickstart.md`
      §8, SC-006).
- [ ] T036 Run the full existing visual-regression suite
      (`ctest --test-dir build -L visual --output-on-failure`) and confirm
      every pre-existing scene (none of which uses
      `SolidBegin`/`SolidEnd`) renders unchanged relative to the T001
      baseline (FR-018/SC-005).
- [ ] T037 [P] Add a new Hugo page under `site/` documenting
      `SolidBegin`/`SolidEnd`, `Attribute "solid"`, and Interior/Exterior
      usage, using the scenes from T025/T026/T029/T034/T035 as worked
      examples, following the existing `site/` content structure
      (Constitution Principle VII).
- [ ] T038 Run the full `quickstart.md` validation sequence end-to-end (all
      8 sections) and record the results.

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — start immediately.
- **Foundational (Phase 2)**: Depends on Setup completion — BLOCKS all user
  stories.
- **User Story 1 (Phase 3)**: Depends on Foundational only. Delivers the
  MVP — this is the feature's entire reason to exist per `spec.md`'s
  priority ordering.
- **User Story 2 (Phase 4)**: Depends on Foundational; its resolution driver
  (T028) also depends on User Story 1's boolean-combination code (T017)
  being in place, since nesting recursively calls the same combination step.
- **User Story 3 (Phase 5)**: Depends on Foundational only for its own
  tasks (T031-T034 touch shading, not CSG resolution) but is only
  meaningfully testable once a composite boundary exists from US1/US2 —
  sequence it after US1 in practice even though it has no hard code
  dependency on US1/US2 files.
- **Polish (Phase 6)**: Depends on all three user stories being complete.

### Within Each User Story

- Tests are written and approved failing (Red) before implementation tasks
  in the same phase.
- Boolean-kernel core (BSP build → classify/clip/merge → difference
  winding) before tessellation-quality work (flatness extraction, analytic
  normals), since the kernel needs *some* tessellated mesh to operate on
  first; tessellation quality is then layered on before the boundary is
  handed to `addObject()`.
- Visual-regression scenes come last in each story, once the underlying
  mechanism they exercise exists.

### Parallel Opportunities

- T003, T007, T011, T012 (Foundational) touch disjoint files and can run in
  parallel once T002/T004 land.
- T013, T014, T015 (US1 tests) are independent files — write in parallel.
- T020 (US1) is independent of T016-T018 (different concern: quadric
  tessellation vs. boolean kernel) and can run in parallel once T019 lands.
- T025, T026 (US1 visual scenes) can run in parallel with each other once
  their respective dependencies (T024, T022) land.
- T031, T032 (US3) touch the same two files but distinct, independent
  branches (interior vs. exterior) — treat as parallel-safe for review
  purposes, serialize the actual edit if working solo.
- T035, T037 (Polish) are independent of each other and of T036.

---

## Parallel Example: User Story 1

```bash
# Tests first, in parallel (different files):
Task: "Unit test: box/box union/intersection/difference in tests/unit/csg/test_boolean_boxes.cpp"
Task: "Unit test: sphere/box + tessellationtolerance in tests/unit/csg/test_boolean_sphere_box.cpp"
Task: "Unit test: coplanar-face epsilon handling in tests/unit/csg/test_boolean_coplanar.cpp"

# After the boolean kernel (T016-T018) and flatness extraction (T019) land:
Task: "Quadric leaf tessellation in src/ri/surface.cpp"          # T020, parallel with T021 prep
Task: "Visual scene: sphere/box union/intersection/difference across hiders"   # T025
Task: "Visual scene: curved-operand boundary smoothness"                        # T026
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup (baseline).
2. Complete Phase 2: Foundational (block-state, capture, container,
   attribute plumbing — CRITICAL, blocks everything else).
3. Complete Phase 3: User Story 1 (boolean composition, curvature-adaptive
   smooth boundaries).
4. **STOP and VALIDATE**: run `quickstart.md` §1-3 and §7 independently.
5. This alone delivers the feature's foundational value (`spec.md`: "Without
   correct boolean composition, there is no CSG support at all").

### Incremental Delivery

1. Setup + Foundational → foundation ready, no user-visible behavior change
   yet beyond a lone `"primitive"` block round-tripping.
2. Add User Story 1 → validate independently → MVP.
3. Add User Story 2 → validate independently (nesting, `quickstart.md` §5).
4. Add User Story 3 → validate independently (Interior/Exterior,
   `quickstart.md` §4).
5. Polish → error diagnostics, non-regression sweep, documentation.

### Notes

- [P] tasks touch different files with no dependency between them.
- Commit after each task or logical group, per the repository's normal
  workflow.
- Verify each Red test actually fails before writing the corresponding
  implementation (Constitution Principle III is non-negotiable for this
  feature — `plan.md` Constitution Check marks it the highest-risk gate).
- Avoid resolving CSG lazily inside any hider's `dice()`/`intersect()` —
  every task above is scoped to keep resolution entirely within
  `RiSolidEnd`/the geometry domain, per the plan's hard architectural
  constraint.
