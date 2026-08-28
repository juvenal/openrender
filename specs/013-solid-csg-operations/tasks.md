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

- [X] T001 Build the unmodified `013-solid-csg-operations` worktree
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

- [X] T002 Add the missing `RENDERMAN_SOLID_PRIMITIVE_BLOCK` push/pop to
      `RiSolidBegin`/`RiSolidEnd` in `src/ri/ri.cpp` (currently only call
      `check()`, unlike `RiObjectBegin`'s `blocks.push(currentBlock);
      currentBlock = RENDERMAN_OBJECT_BLOCK;` at the same site,
      `research.md` "RIB block-state enforcement gap") — this is what lets
      `check()`'s existing scope-mask machinery enforce FR-014 later.
- [X] T003 [P] Add `currentSolid` / `savedSolids` state to
      `src/ri/rendererContext.h`, mirroring the existing `instance` /
      `instanceStack` members (~line 225-236).
- [X] T004 Create the CSG Tree node type (`operation`, `operands`,
      `leafObjects`, `outerXform`, `parent` fields per `data-model.md`) in
      `src/ri/csgTree.h` / `src/ri/csgTree.cpp` (depends on T003).
- [X] T005 Implement `RiSolidBegin`/`RiSolidEnd` capture/open/close in
      `src/ri/rendererContext.cpp` (replacing the unimplemented stub at
      lines 5502-5513): validate the operation-type string against
      `"primitive"`/`"union"`/`"intersection"`/`"difference"` (FR-001,
      reject invalid values per FR-013), push/pop `currentSolid` against
      `savedSolids`, and reject an unmatched or scope-mismatched
      `SolidEnd` (FR-014) (depends on T002, T003, T004).
- [X] T006 Add the third `addObject()` capture gate — while `currentSolid`
      is open, divert each captured primitive's `CObject*` into the active
      CSG tree node instead of calling `CRenderer::render()`, alongside the
      existing instance/delayed gates — in `src/ri/rendererContext.cpp`
      (`addObject()`, line 457) (depends on T005).
- [X] T007 [P] Add `"primitive"`-leaf validation in `src/ri/csgTree.cpp`:
      reject a nested `SolidBegin`/`SolidEnd` inside a `"primitive"` block
      (FR-019) and reject an `RiProcedural` captured directly inside a
      `"primitive"` block (FR-021, `research.md` "Delayed/procedural
      primitives... resolved as rejected"), both via
      `error(CODE_BADTOKEN, ...)` (depends on T004).
- [X] T008 Create the `CSolidObject : CObject` Resolved Solid Boundary
      container in `src/ri/solidObject.h` / `src/ri/solidObject.cpp`
      (`data-model.md`): owns a `children`/`sibling` list of `CPolygonMesh`
      Boundary Fragments, computes `bmin`/`bmax` as the union of fragment
      bounds, and presents them to every hider through the existing generic
      `CObject` dispatch — no new virtual methods (depends on T004).
- [X] T009 Implement the outer-block coordinate-space transform (`research.md`
      Decision 5) in `src/ri/csgTree.cpp`: compose each captured leaf's
      `from` with the inverse of the outermost `SolidBegin`'s `from` to
      bring it into that block's local frame before resolution; give the
      resolved `CSolidObject` that same outer-block `CXform*` (depends on
      T004, T008).
- [X] T010 Implement the two trivial tree-resolution shortcuts in
      `src/ri/csgTree.cpp`: an empty solid block (no captured leaves/
      operands) resolves to no geometry, not an error (FR-016), and a
      boolean block with exactly one operand resolves to that operand's
      boundary unchanged, skipping BSP combination entirely (FR-017)
      (depends on T004, T008).
- [X] T011 [P] Add `Attribute "solid" "float tessellationtolerance"` via the
      existing four-layer attribute pattern (`CLAUDE.md`): token constant in
      `src/ri/ri.h`, RIB parsing in `RiAttributeV()`
      (`src/ri/rendererContext.cpp`), storage in `src/ri/attributes.h` /
      `src/ri/attributes.cpp`, pre-declaration in
      `src/ri/rendererDeclarations.cpp` (`contracts/solid-rib-interface.md`).
- [X] T012 [P] Create the `tests/unit/csg/` directory and wire a new ctest
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

- [X] T013 [P] [US1] Unit test: two axis-aligned unit boxes with a known
      overlap sub-volume — assert expected face count and enclosed volume
      for union, intersection, and difference in
      `tests/unit/csg/test_boolean_boxes.cpp`.
- [X] T014 [P] [US1] Unit test: a `"primitive"` block containing two raw
      primitives (e.g. two overlapping boxes declared directly, with no
      nested `SolidBegin`) — assert they are captured and tessellated as a
      single opaque CSG leaf (one `leafObjects` set, one combined BSP tree),
      consistent with `spec.md`'s Edge Cases, in
      `tests/unit/csg/test_multi_primitive_leaf.cpp`.
- [ ] T015 [P] [US1] Unit test: a sphere combined with a box — validates
      curved-vs-flat boundary handling and that
      `Attribute "solid" "float tessellationtolerance"` changes output
      triangle density as expected, in
      `tests/unit/csg/test_boolean_sphere_box.cpp`.
      PARTIALLY DONE: the kernel-level assertions (union/intersection
      volume conservation on curved operands, spanning-triangle clipping,
      and tessellation-density-in/density-out via a test-local `slices`/
      `stacks` helper) are written and green. The task's actual acceptance
      criterion — that the real `Attribute "solid" "float
      tessellationtolerance"` RIB attribute drives output density — is not
      yet testable because that attribute isn't wired to anything (that's
      T022's job). Left unchecked on purpose so this doesn't get lost;
      extend this test with a tessellationtolerance-attribute-driven
      density assertion once T022 lands, then check this off.
- [X] T016 [P] [US1] Unit test: an explicit coplanar-face pair — validates
      `C_EPSILON`-consistent classification (`common/algebra.h`, `1e-6`,
      `research.md` Decision 3 risk) in
      `tests/unit/csg/test_boolean_coplanar.cpp`.

### Implementation for User Story 1

- [X] T017 [US1] Build a BSP tree over a tessellated operand mesh in
      `src/ri/csgBoolean.h` / `src/ri/csgBoolean.cpp` (`research.md`
      Decision 3) (depends on T013, T014, T015, T016 failing red, T008,
      T020, T021 — T015/T016's curved- and coplanar-operand cases cannot go
      Green until tessellation lands too; see the ordering note below).
      Done as the pure BSP kernel (`CCSGBSPNode::build`/`csgSplitPolygon`)
      driven directly by hand-built polygon soup in the T013-T016 unit
      tests, ahead of and independent from the real tessellation pipeline
      (T020-T022) that will feed it in production; a real coplanar-routing
      bug (`clipPolygons` sending both coplanar-front and coplanar-back
      polygons into `frontList` instead of routing coplanar-back into
      `backList`) was caught and fixed by T016.
- [X] T018 [US1] Implement classify/clip/merge boolean combination
      (union/intersection/difference) over two BSP trees, using the
      `C_EPSILON` classification epsilon, in `src/ri/csgBoolean.cpp`
      (depends on T017).
- [X] T019 [US1] Implement `difference` as intersection with the second
      operand's complement — reverse winding/normals on faces retained from
      the subtracted operand, since the visible cut surface is its
      inward-facing side — in `src/ri/csgBoolean.cpp` (depends on T018).
- [X] T020 [US1] Extract the flatness/chordal-deviation adaptive stopping
      criterion out of `CTesselationPatch::tesselate`
      (`src/ri/surface.cpp:1858-1897`) into a form callable without a
      traced ray, in `src/ri/surface.h` / `src/ri/surface.cpp`
      (`research.md` Decision 4). **Implementation note**: standalone
      prototyping (documented in research.md Decision 4) showed the
      existing `uFlat < uAvg && vFlat < vAvg` formula does not converge
      toward zero with refinement over a fixed domain and has no tolerance
      value that makes it terminate correctly for curved input, so it was
      not reused as-is. Replaced with a new function,
      `tesselationSagittaWithinTolerance()`, using a per-cell
      midpoint-sagitta test (empirically confirmed O(1/div²) convergence)
      driven from an absolute tolerance alone; the ray-footprint half of
      the original stopping criterion (`surface.cpp:724-759`) plays no
      part in it. Unit-tested in
      `tests/unit/csg/test_tesselation_flatness.cpp` (flat plane, coarse
      vs. refined sphere octant, monotonic convergence) (depends on T011).
- [X] T021 [P] [US1] Add mesh tessellation for quadric leaf operands (never
      tessellated before this feature — `CSphere::intersect` etc. currently
      raytrace via pure algebraic root-solving,
      `src/ri/quadrics.cpp:200-`) using the extracted flatness criterion, in
      `src/ri/surface.cpp` (depends on T020).
      **Implementation note**: implemented as `tesselateQuadricAdaptive()`
      (`src/ri/surface.h`/`.cpp`), calling `CSurface::sample()` directly
      through a minimal standalone `varying[]` harness (no
      `CShadingContext` exists at `RiSolidEnd` time — allocates a
      `VARIABLE_CONSTANTWIDTH+1`-slot array, populates `VARIABLE_U`/
      `VARIABLE_V` over the confirmed normalized `[0,1]×[0,1]` full-domain
      convention, `VARIABLE_TIME=0.0f` unconditionally since motion-path
      code in e.g. `CSphere::sample` reads it regardless of whether the
      object actually moves). `up = PARAMETER_P` optionally OR'd with
      `PARAMETER_DPDU|PARAMETER_DPDV` via a `computeDerivatives` flag.
      `CTesselatedGrid.P/dPdu/dPdv` are **world-space**, not object-space —
      `CSurface::sample()` applies `xform->from` unconditionally
      (`transformPoints()` macro, `quadrics.cpp:504` and siblings), which is
      the correct contract for CSG: operands under one solid block can carry
      different transforms, so a common space is required before boolean
      combination, and this comes for free with no extra transform step in
      T022/T025.
      Probes resolutions doubling from 4 to a 128 cap; at each probe
      resolution `tesselationSagittaWithinTolerance()` validates the
      *coarser* `probeDiv/2` candidate mesh already embedded (per its own
      documented contract) at the probe's even-indexed rows/columns — so on
      pass, that `probeDiv/2` candidate is extracted via a lossless strided
      subgrid copy (`extractEvenSubgrid()`, no re-sampling: the embedded
      even-indexed samples were evaluated at exactly the candidate's own
      parametric coordinates) and shipped, not the finer probe itself. This
      keeps the external candidate-resolution range at `[2, 64]`, matching
      the original design, while avoiding shipping up to 4x more triangles
      than the tolerance actually required into the downstream BSP. Returns
      a `CTesselatedGrid { div, P, dPdu, dPdv }` (caller-owned `new[]`
      arrays). Unit-tested in
      `tests/unit/csg/test_quadric_tesselation.cpp` against a real
      `CSphere` captured via a test-only `CRendererContext::addObject`
      interceptor (same precedent as T014's test): resolution bounds,
      every sampled position lying on the analytic unit sphere to float
      precision (independent of tessellation resolution), derivatives
      omitted/present per the flag and radially tangent when present,
      monotonically non-decreasing resolution under a tighter tolerance,
      and a translated sphere's sampled centroid shifting with it
      (confirms the world-space contract empirically, not just by code
      inspection).
      **Known follow-up for T022/T025** (not fixed here — out of scope for
      a grid-producing function): a full sphere's parametric grid has
      genuinely degenerate cells — the `v=0`/`v=1` pole rows collapse to a
      single point, and the `u=0`/`u=360°` seam column duplicates the same
      world position (visible in this task's own world-space test as a
      ~0.7% centroid bias from double-counting the seam point at a coarse
      resolution). Naive `(div+1)²` → `2·div²` triangulation of this grid
      emits zero-area triangles at the poles and along the seam, which will
      feed degenerate/NaN plane data into `csgCombine()`'s BSP splitter.
      Whoever writes the triangulation (T022 or T025) must drop cells with
      coincident corners rather than assume every grid cell is a valid
      quad.
- [ ] T022 [US1] Wire `src/ri/csgTree.cpp` leaf tessellation to call the
      extracted flatness criterion for NURBS/Bézier/quadric operands,
      reading the tolerance from `Attribute "solid"
      "tessellationtolerance"` when set, defaulting to the primitive's own
      object-space bound diagonal otherwise (depends on T020, T021, T011).
- [ ] T023 [US1] Compute an analytic per-vertex shading normal
      (`crossvv(dPdu, dPdv)` at each sample vertex's exact parametric
      coordinates) for NURBS- and quadric-sourced fragments, and store it as
      the `"N"` `CONTAINER_VERTEX` primvar on the resulting `CPolygonMesh`
      (`CPolygonTriangle::sample`/`interpolate` already interpolates any
      supplied `"N"` generically, `polygons.cpp:280-314,379-405`) in
      `src/ri/surface.cpp` and `src/ri/solidObject.cpp` (`research.md`
      Decision 4b) (depends on T020, T008).
- [ ] T024 [US1] Construct Boundary Fragments so each BSP output fragment
      carries its *originating leaf's* `CAttributes*` (`research.md`
      Decision 2, `data-model.md`) in `src/ri/csgBoolean.cpp` /
      `src/ri/solidObject.cpp` (depends on T018, T008).
- [ ] T025 [US1] At the outermost `RiSolidEnd`, resolve the CSG tree via
      `csgBoolean` into a `CSolidObject` and re-enter `addObject()` exactly
      like any other primitive, in `src/ri/rendererContext.cpp` (depends on
      T018, T024, T006).
- [ ] T026 [P] [US1] Add a visual-regression scene combining a sphere and a
      box with `"union"`, `"intersection"`, and `"difference"`, rendered
      with the raytrace, REYES, and `hidden` (z-buffer) hiders, in
      `examples/rib/` (`quickstart.md` §2-3, SC-001/SC-002).
- [ ] T027 [P] [US1] Add a visual-regression scene validating boundary
      smoothness on curved operands: tight vs. loose
      `tessellationtolerance` on a sphere/sphere union, a NURBS-operand
      variant, and a subdivision-surface-operand variant showing the
      accepted normal-quality asymmetry, in `examples/rib/`
      (`quickstart.md` §7, `research.md` Decision 4/4b, FR-022, SC-007)
      (depends on T023).

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

- [ ] T028 [P] [US2] Unit test: a nested tree resolves inner operands before
      the outer operation combines them (bottom-up), and a `"difference"`
      block with three or more operands subtracts every operand after the
      first in declaration order (FR-005), in
      `tests/unit/csg/test_nested_tree.cpp`.

### Implementation for User Story 2

- [ ] T029 [US2] Implement bottom-up recursive resolution over nested
      `CSGTreeNode` operands — each boolean node resolves its children
      (themselves possibly boolean nodes) before combining — in
      `src/ri/csgTree.cpp` (depends on T018, T028).
- [ ] T030 [P] [US2] Add a visual-regression scene with a solid tree at
      least four operation levels deep mixing union/intersection/difference,
      in `examples/rib/` (`quickstart.md` §5, SC-003).
- [ ] T031 [US2] Verify a `SolidBegin`/`SolidEnd` block declared inside
      `RiObjectBegin`/`RiObjectEnd` resolves once at object-definition time
      and each `RiObjectInstance` replay reuses that resolved result under
      its own transform (`research.md` Decision 1, `addObject()` gate
      precedence) — add a regression scene exercising this in
      `examples/rib/` and confirm no special-casing was needed in
      `src/ri/rendererContext.cpp` (depends on T006, T025).

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

### Tests for User Story 3 ⚠️

> **Write this first — approve it failing (Red) before the implementation tasks below.**

- [ ] T032 [P] [US3] Unit test: given a point classified as inside vs.
      outside a Boundary Fragment's resolved volume, and `CAttributes`
      with/without `interior`/`exterior` set, assert the correct shader
      pointer is selected (interior, exterior, or fallback to ordinary
      shading) — pure selection logic requiring no ray trace or render, in
      `tests/unit/csg/test_interior_exterior_selection.cpp` (Constitution
      Principle III).

### Implementation for User Story 3

- [ ] T033 [P] [US3] Consult `attributes->interior` and apply its shading
      effect when a camera or traced ray is inside a Boundary Fragment's
      resolved volume (FR-010), in `src/ri/shading.cpp` /
      `src/ri/shaderFunctions.h` (depends on T032 failing red).
- [ ] T034 [P] [US3] Consult `attributes->exterior` when viewed from outside
      a Boundary Fragment's resolved volume (FR-011), falling back to
      ordinary surface/atmosphere shading when neither Interior nor
      Exterior is set (FR-012), in `src/ri/shading.cpp` (depends on T032
      failing red).
- [ ] T035 [US3] Restrict this Interior/Exterior consumption to Boundary
      Fragments of a `CSolidObject` only — Interior/Exterior assigned to
      attribute state that never enters a `SolidBegin`/`SolidEnd` block
      MUST remain a no-op exactly as today (FR-020) — in
      `src/ri/shading.cpp` (depends on T033, T034).
- [ ] T036 [P] [US3] Add a visual-regression scene: a `"difference"` solid
      (sphere minus sphere) with an Interior shader assigned, viewed from an
      angle revealing the cut-away interior, confirming visibly distinct
      interior vs. exterior appearance, in `examples/rib/`
      (`quickstart.md` §4, SC-004).

**Checkpoint**: All three user stories are independently functional.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Diagnostics, non-regression, and documentation that span all
three user stories.

- [ ] T037 [P] Add error/diagnostic scenes confirming a clear
      `CODE_BADTOKEN` diagnostic (not a crash, not a silently wrong image)
      for: `SolidBegin "bogus"`, an unmatched `SolidEnd`, a
      `SolidBegin "primitive"` block containing a nested
      `SolidBegin`/`SolidEnd`, and an `RiProcedural` directly inside a
      `SolidBegin "primitive"` block, in `examples/rib/` (`quickstart.md`
      §8, SC-006).
- [ ] T038 Run the full existing visual-regression suite
      (`ctest --test-dir build -L visual --output-on-failure`) and confirm
      every pre-existing scene (none of which uses
      `SolidBegin`/`SolidEnd`) renders unchanged relative to the T001
      baseline (FR-018/SC-005).
- [ ] T039 [P] Add a new Hugo page under `site/` documenting
      `SolidBegin`/`SolidEnd`, `Attribute "solid"`, and Interior/Exterior
      usage, using the scenes from T026/T027/T030/T036/T037 as worked
      examples, following the existing `site/` content structure
      (Constitution Principle VII).
- [ ] T040 Run the full `quickstart.md` validation sequence end-to-end (all
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
  (T029) also depends on User Story 1's boolean-combination code (T018)
  being in place, since nesting recursively calls the same combination step.
- **User Story 3 (Phase 5)**: Depends on Foundational only for its own
  tasks (T032-T036 touch shading and shader selection, not CSG resolution)
  but is only meaningfully testable end-to-end once a composite boundary
  exists from US1/US2 — sequence it after US1 in practice even though it
  has no hard code dependency on US1/US2 files.
- **Polish (Phase 6)**: Depends on all three user stories being complete.

### Within Each User Story

- Tests are written and approved failing (Red) before implementation tasks
  in the same phase.
- Boolean-kernel logic (BSP build → classify/clip/merge → difference
  winding, T017-T019) and tessellation (flatness extraction T020, quadric
  tessellation T021) land together before any curved- or coplanar-operand
  test (T015, T016) can go Green — T013 (box/box) and T014 (multi-primitive
  leaf, also flat-faceted) are the only US1 tests that can go Green from
  T017-T019 alone. T022's tolerance-wiring and T023's analytic normals then
  layer on top before the boundary is handed to `addObject()`.
- Visual-regression scenes come last in each story, once the underlying
  mechanism they exercise exists.

### Parallel Opportunities

- T003, T007, T011, T012 (Foundational) touch disjoint files and can run in
  parallel once T002/T004 land.
- T013, T014, T015, T016 (US1 tests) are independent files — write in
  parallel.
- T021 (US1, quadric tessellation) is independent of T018-T019
  (classify/clip/merge, difference winding) and can be developed in
  parallel with them once T020 lands — but T017 (BSP build) and T020/T021
  must be co-designed (the "tessellated operand mesh" contract they share),
  not strictly sequential, since T015/T016 need both sides to be Green.
- T026, T027 (US1 visual scenes) can run in parallel with each other once
  their respective dependencies (T025, T023) land.
- T033, T034 (US3) touch the same two files but distinct, independent
  branches (interior vs. exterior) — treat as parallel-safe for review
  purposes, serialize the actual edit if working solo.
- T037, T039 (Polish) are independent of each other and of T038.

---

## Parallel Example: User Story 1

```bash
# Tests first, in parallel (different files):
Task: "Unit test: box/box union/intersection/difference in tests/unit/csg/test_boolean_boxes.cpp"
Task: "Unit test: multi-primitive '\"primitive\"' leaf in tests/unit/csg/test_multi_primitive_leaf.cpp"
Task: "Unit test: sphere/box + tessellationtolerance in tests/unit/csg/test_boolean_sphere_box.cpp"
Task: "Unit test: coplanar-face epsilon handling in tests/unit/csg/test_boolean_coplanar.cpp"

# BSP build (T017) and tessellation (T020/T021) are co-designed together;
# once the boolean kernel (T017-T019) and flatness extraction (T020) land:
Task: "Quadric leaf tessellation in src/ri/surface.cpp"          # T021, parallel with T022 prep
Task: "Visual scene: sphere/box union/intersection/difference across hiders"   # T026
Task: "Visual scene: curved-operand boundary smoothness"                        # T027
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
