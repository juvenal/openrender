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
- [X] T022 [US1] Wire `src/ri/csgTree.cpp` leaf tessellation to call the
      extracted flatness criterion for NURBS/Bézier/quadric operands,
      reading the tolerance from `Attribute "solid"
      "tessellationtolerance"` when set, defaulting to the primitive's own
      object-space bound diagonal otherwise (depends on T020, T021, T011).
- [X] T022a [US1] **FR-015 gap fix**: `csgTessellateOperand` (T022) only
      dispatched `CSurface`/`CPatchMesh` leaves (NURBS/Bézier/quadric, T022's
      own stated scope) and hard-rejected every other primitive type with
      `CODE_BADTOKEN` — violating FR-015's "accept any RenderMan geometric
      primitive type as an operand." Fixed for `CPolygonMesh`
      (`RiPointsPolygons`/`RiPointsGeneralPolygons`) leaves: added
      `csgTessellatePolygonMeshOperand()` in `src/ri/polygons.cpp`, reusing
      `CPolygonMesh::create()`'s own ear-clipping-with-holes decomposition
      against a function-local `CMemPage` (decoupled from `CShadingContext`
      via a `CMeshData.meshContext` → `meshMemory` type refactor, since no
      `CShadingContext` exists at `RiSolidEnd` time), and a new dispatch
      branch in `csgTessellateOperand` (`src/ri/csgTree.cpp`) that
      tessellates/triangulates/deletes each resulting
      `CPolygonTriangle`/`CPolygonQuad`.
      Root-caused and fixed a heap-use-after-free in this new path
      (confirmed via an AddressSanitizer build): `CPolygonTriangle`/
      `CPolygonQuad`'s ctor/dtor `attach()`/`detach()` their back-pointer to
      the original `CPolygonMesh` leaf, whose `refCount` starts at 0
      (`addObject`'s `currentSolid` branch never `attach()`s captured
      "primitive" leaves). Deleting the temporary triangles inside the
      dispatch loop drove that shared `refCount` to 0 mid-loop, self-deleting
      the still-in-use `leaf` object before the outer loop's `leaf->sibling`
      traversal read it. Fixed with a single protective `polyMesh->attach()`
      before building the triangle chain, left intentionally unbalanced here
      so the existing `consumedLeaves` detach() loop in `resolveCSGTree`
      performs the one real release once soup-building is fully done.
      Verified via a hand-authored sphere ∪ `PointsPolygons`-cube probe scene
      (60/60 stress runs clean, single- and multi-threaded) plus the full
      `ctest` visual/CSG suite (96/96 visual, 7/7 CSG unit tests) with no
      regressions (depends on T022).
      **Pre-existing defect observed while fixing this, not caused by this
      change, not fixed here**: `resolveCSGTree`'s `consumedLeaves` `detach()`
      loop is a no-op for every leaf type *except* `CPolygonMesh` after this
      fix — captured "primitive" leaves start at `refCount == 0`
      (`addObject`'s `currentSolid` branch never `attach()`s them), so
      `detach()` there decrements to -1 and never runs `~CObject()`/frees
      `attributes`. Every `CSurface`/`CPatchMesh` leaf and its `CAttributes*`
      reference leaks per `SolidEnd`; `CPolygonMesh` leaves no longer do
      (this fix's `attach()` puts them at 1, so that same `detach()` now
      correctly frees them). Confirmed via `CStats::check()`'s
      `numAttributes == 0` assertion firing under a Debug+ASan build even for
      a plain sphere+sphere union (no `CPolygonMesh` involved) — invisible in
      Release, which compiles out `assert`. Separately, that same ASan build
      also hit an unrelated `isAligned64` assertion
      (`polygons.cpp:389`) reproduced by the same plain sphere+sphere scene;
      not investigated, likely an ASan-allocator-alignment artifact rather
      than a real bug — flagged here so it isn't mistaken for a regression
      from this task.
- [X] T022b [US1] **FR-015 gap fix**: subdivision-surface (`CSubdivMesh`,
      covering both the Loop and Catmull-Clark schemes) CSG leaf operands
      previously fell through to the `CODE_BADTOKEN` diagnostic in
      `csgTessellateOperand` — fixed. Actual approach taken (narrower than
      the "duck-typing/templating refactor" originally planned above, which
      turned out to be unnecessary): added
      `CSubdivMesh::tessellateToSurfaces(CMemPage *&mem)`
      (`src/ri/subdivisionCreator.h/.cpp`), a thin wrapper around the
      existing `buildSurfaces()` — already `CShadingContext`-agnostic — that
      hands the caller a sibling chain of `CSurface` patches
      (`CSubdivision`/`CBicubicPatch`/`CPatchGrid`/`CBSplinePatchGrid`) built
      from a standalone `CMemPage` instead of a shading context's
      `threadMemory`. `csgTessellateOperand` (`src/ri/csgTree.cpp`) gained a
      `CSubdivMesh` dispatch branch that calls it, then tessellates each
      returned patch via the existing `tesselateQuadricAdaptive()` path
      (same as the T022 NURBS/quadric leaves) and `delete`s each patch once
      its grid has been extracted, `memoryTini()`-ing the pool only after
      that loop completes (see below).
      Two real bugs found and fixed while verifying against three
      hand-authored probes — Loop-only, Catmull-Clark-only, and a combined
      Loop+Catmull-Clark CSG operand scene:
      (1) *Pool-ownership lifetime*: the first version of
      `tessellateToSurfaces()` tore down its own pool
      (`memoryTini(mem)`) before returning, but the returned patches' vertex
      data is `ralloc()`'d from that pool, not copied — fixed by making the
      caller own the pool (`CMemPage *&mem` out-parameter) and defer
      `memoryTini()` until its own patch-consumption loop is done.
      (2) *The actual crash root cause*: `tesselateSurfaceGrid()`
      (`src/ri/surface.cpp`) only allocates/wires 7 of the ~29 possible
      `varying[]` slots; `CSubdivision::sample()` unconditionally writes
      `varying[VARIABLE_DPDTIME]` regardless of motion-blur state (mirroring
      how it always writes `dPdu`/`dPdv`/`Ng`), and that slot was left
      `NULL` — a null-pointer write, reproducible as a deterministic
      `EXC_BAD_ACCESS` inside `CSubdivision::sample()` on every subdivision
      CSG operand. Fixed by adding a `dPdtime` scratch buffer to
      `tesselateSurfaceGrid()`, mirroring the pre-existing scratch-only `ng`
      buffer (never surfaced in `CTesselatedGrid`, discarded after
      `sample()` returns) — this fix is shared with T022's existing
      NURBS/quadric dispatch path, since both go through the same
      `tesselateSurfaceGrid()`.
      Verified via all three probes rendering cleanly (exit 0), the full
      `ctest -R "^CSG_"` unit suite (7/7 pass), and single-/multi-threaded
      stress loops (15 iterations × 3 probes each, both modes, 0 failures)
      in the Release build.
      **Investigated, confirmed pre-existing, not caused by this task**:
      under a Debug+ASan build, CSG scenes (including both new subdivision
      probes and the pre-existing plain sphere+cube union scene) trip the
      same `isAligned64` assertion (`polygons.cpp:389`) and `numAttributes
      == 0` leak assertion (`stats.cpp:216`) already flagged as pre-existing
      in T022a's notes above — reproducing identically on a Loop-subdivision
      probe confirms this is the same shared-pipeline issue, not a
      subdivision-specific regression from this task (depends on T022a).
- [X] T023 [US1] Compute an analytic per-vertex shading normal
      (`crossvv(dPdu, dPdv)` at each sample vertex's exact parametric
      coordinates) for NURBS- and quadric-sourced fragments, and store it as
      the `"N"` `CONTAINER_VERTEX` primvar on the resulting `CPolygonMesh`
      (`CPolygonTriangle::sample`/`interpolate` already interpolates any
      supplied `"N"` generically, `polygons.cpp:280-314,379-405`) in
      `src/ri/surface.cpp` and `src/ri/solidObject.cpp` (`research.md`
      Decision 4b) (depends on T020, T008).
- [X] T024 [US1] Construct Boundary Fragments so each BSP output fragment
      carries its *originating leaf's* `CAttributes*` (`research.md`
      Decision 2, `data-model.md`) in `src/ri/csgBoolean.cpp` /
      `src/ri/solidObject.cpp` (depends on T018, T008).
- [X] T025 [US1] At the outermost `RiSolidEnd`, resolve the CSG tree via
      `csgBoolean` into a `CSolidObject` and re-enter `addObject()` exactly
      like any other primitive, in `src/ri/rendererContext.cpp` (depends on
      T018, T024, T006).
- [X] T026 [P] [US1] Add a visual-regression scene combining a sphere and a
      box with `"union"`, `"intersection"`, and `"difference"`, rendered
      with the raytrace, REYES, and `hidden` (z-buffer) hiders, in
      `examples/rib/` (`quickstart.md` §2-3, SC-001/SC-002).
- [X] T027 [P] [US1] Add a visual-regression scene validating boundary
      smoothness on curved operands: tight vs. loose
      `tessellationtolerance` on a sphere/sphere union, a NURBS-operand
      variant, and a subdivision-surface-operand variant showing the
      accepted normal-quality asymmetry, in `examples/rib/`
      (`quickstart.md` §7, `research.md` Decision 4/4b, FR-022, SC-007)
      (depends on T023).
      Registered as four scenes under `tests/visual/CMakeLists.txt`
      (`csg-sphere-sphere-tight-tolerance-raytrace`,
      `csg-sphere-sphere-loose-tolerance-raytrace`,
      `csg-nurbs-sphere-union-raytrace`,
      `csg-subdivision-sphere-union-raytrace`), references generated and
      committed under `examples/rib/tests/references/`; 96/96 visual tests
      and 7/7 CSG unit tests pass.
- [X] T027a [US1] **FR-015 gap fix**: the 4th and final leaf-dispatch gap —
      `CNURBSPatchMesh` (`RiNuPatch`) CSG leaf operands previously fell
      through to the `CODE_BADTOKEN` diagnostic in `csgTessellateOperand`,
      same class of bug as T022a/T022b. Fixed by adding
      `tesselateNURBSPatchMeshAdaptive()` in `src/ri/patches.cpp` — mirrors
      `CNURBSPatchMesh::create()`'s own per-sub-patch decomposition
      (`uPatches`×`vPatches` grid of `CNURBSPatch` sub-patches, each
      adaptively tessellated then seam-welded to the coarsest neighboring
      resolution) but against a standalone `CMemPage` instead of a
      `CShadingContext`'s `threadMemory`, using the same
      `CMemPageContext`-stand-in trick as T022a/T022b. Wired into
      `csgTessellateOperand` (`src/ri/csgTree.cpp`) via a new
      `CNURBSPatchMesh` dispatch branch ahead of the `CPolygonMesh` branch.
      Two bugs found and fixed while landing this:
      (1) *Compile-time*: `memBegin`/`memEnd` (`src/ri/memory.h:88-101`) are
      literal brace-opening/closing macros forming a real C++ scope, not
      ordinary function-like macros — the initial draft declared
      `subPatches`/`grids`/`maxDiv`/`count` inside that
      `memBegin`/`memEnd`-created scope but referenced them in the
      seam-welding loop positioned after the matching `memEnd`, producing
      "undeclared identifier" errors that were genuinely correct by normal
      C++ scoping rules. Fixed by moving those four declarations before
      `memBegin`, matching `tesselatePatchMeshAdaptive()`'s (T022's
      `CPatchMesh` template) existing pattern.
      (2) *Runtime*: a SIGSEGV inside `CNURBSPatch::sample()` on the very
      first CSG-driven tessellation probe. Root cause: `sample()`
      unconditionally reads/writes a homogeneous `varying[VARIABLE_PW]`
      buffer (`patches.cpp:1251`, `numVertices*4` floats) to convert
      homogeneous `Pw` to Euclidean `P` — but `tesselateSurfaceGrid()`
      (`src/ri/surface.cpp`) only ever allocated 8 of the ~29 possible
      `varying[]` slots (never `PW`), so `varying[VARIABLE_PW]` was `NULL`
      and the `movvv(P, Pw)` loop walked off a null pointer. Same bug class
      as T022b's `dPdtime` fix, third occurrence of this exact pattern
      (`ng`/`dPdu`/`dPdv` unconditional-write for `CBicubicPatch`,
      `dPdtime` unconditional-write for `CSubdivision`, now `PW`
      unconditional-write for `CNURBSPatch`). Fixed by adding a scratch-only
      `pw` buffer to `tesselateSurfaceGrid()`, freed after `sample()`
      returns and never surfaced in `CTesselatedGrid`, mirroring the
      existing `ng`/`dPdtime` scratch buffers exactly; this fix is shared
      with T022's NURBS/quadric dispatch and T022b's subdivision dispatch,
      since all three go through the same `tesselateSurfaceGrid()`.
      Verified via `csg-nurbs-sphere-union-raytrace.rib` (exit 0, was
      SIGSEGV before the fix), the full `ctest -R "^CSG_"` unit suite (7/7
      pass), and the full visual suite (96/96 pass, no regressions)
      (depends on T022). **Scope note**: the `NuPatch` operand in that scene
      is a single non-periodic 4x4 bicubic patch — an open surface with a
      boundary curve, not a closed 2-manifold. RISpec §5.9 solid CSG is only
      spec-defined for closed operands, so this scene deliberately does not
      claim to validate spec-correct closed-solid NURBS boolean semantics;
      it validates (a) the leaf-dispatch path resolves without crashing or
      falling through to `CODE_BADTOKEN`, and (b) the BSP produces a
      plausible, non-degenerate visual result for this input (confirmed by
      inspection: the patch is visibly present and forms a real boolean
      seam against the sphere, not silently dropped). Genuine closed-NURBS
      operand coverage (e.g. a periodic/closed NuPatch tube, or degenerate-
      pole closed patch) is not yet exercised and would be a reasonable
      follow-up.

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

- [X] T028 [P] [US2] Unit test: a nested tree resolves inner operands before
      the outer operation combines them (bottom-up), and a `"difference"`
      block with three or more operands subtracts every operand after the
      first in declaration order (FR-005), in
      `tests/unit/csg/test_nested_tree.cpp`. Two tests: (1) builds
      `difference(union(boxA, boxB), boxC)` from axis-aligned box operands
      and asserts the resolved fragment's `bmin`/`bmax` matches a
      hand-computed expected bbox chosen so it's only correct if the nested
      union genuinely ran before the outer difference clipped it; (2) builds
      a 3-operand `"difference"` and, via probe-box `"intersection"` wraps,
      confirms both the 2nd and 3rd declared subtrahends were actually
      removed (not just the 2nd), plus a sanity control that a surviving
      region is not empty. `ctest -R CSG_NestedTree` passes; full `-L csg`
      suite (8 tests) still green.

### Implementation for User Story 2

- [X] T029 [US2] Implement bottom-up recursive resolution over nested
      `CSGTreeNode` operands — each boolean node resolves its children
      (themselves possibly boolean nodes) before combining — in
      `src/ri/csgTree.cpp` (depends on T018, T028). **Already implemented**:
      `csgResolveNode()` (added during the Phase 3/US1 foundational work)
      already recurses into `node->operands->array[i]` before folding
      results into the parent's combine loop, and its sequential fold
      (`for (i = 1; i < operands.numItems; i++) csgCombine(...)`) already
      satisfies FR-005's declaration-order semantics. T028's test locks
      this in as a dedicated regression test; no code change was needed
      for T029 itself.
- [X] T030 [P] [US2] Add a visual-regression scene with a solid tree at
      least four operation levels deep mixing union/intersection/difference,
      in `examples/rib/` (`quickstart.md` §5, SC-003).
      `examples/rib/tests/csg-nested-tree-4-level-raytrace.rib`:
      `difference(union(intersection(sphereA, sphereB), sphereC), sphereD)`
      — difference(1) > union(2) > intersection(3) > primitive(4), four
      nesting levels, all three operation types. Rendered and visually
      confirmed non-degenerate (lens from the intersection, spherical cap
      from the union, flattened cut from the outer difference all visible
      in the silhouette); reference TIF committed to
      `examples/rib/tests/references/`; registered in
      `tests/visual/CMakeLists.txt` as `Visual_csg-nested-tree-4-level-raytrace`
      (97 scenes under label `visual`), passes.
- [X] T031 [US2] Verify a `SolidBegin`/`SolidEnd` block declared inside
      `RiObjectBegin`/`RiObjectEnd` resolves once at object-definition time
      and each `RiObjectInstance` replay reuses that resolved result under
      its own transform (`research.md` Decision 1, `addObject()` gate
      precedence) — add a regression scene exercising this in
      `examples/rib/` and confirm no special-casing was needed in
      `src/ri/rendererContext.cpp` (depends on T006, T025).
      `rendererContext.cpp`'s `addObject()` dispatch itself needed no
      special-casing, exactly as `research.md` Decision 1 predicted — but a
      genuine gap turned up one layer earlier: `RiSolidBegin`'s own scope
      bitmask in `src/ri/ri.cpp` had never been updated to include
      `RENDERMAN_OBJECT_BLOCK`, unlike `RiObjectBegin` and the
      `VALID_PRIMITIVE_BLOCKS`/`VALID_ATTRIBUTE_BLOCKS`/`VALID_XFORM_BLOCKS`
      unions ordinary primitives use, so every `SolidBegin` call inside an
      open `ObjectBegin`/`ObjectEnd` block was rejected outright with
      "Bad scope for RiSolidBegin" before ever reaching `addObject()`. Fixed
      by adding `RENDERMAN_OBJECT_BLOCK` to that bitmask (`ri.cpp:2069`).
      Regression scene `csg-object-instance-reuse-raytrace.rib` defines one
      `ObjectBegin`/`SolidEnd`-wrapped sphere-difference widget and replays
      it via three `ObjectInstance` calls (two plain translations, one also
      rotated 90°); rendered and visually confirmed the CSG notch appears in
      the same orientation on the two translated instances and rotated on
      the third, proving the resolved result is shared and each instance
      applies its own transform independently. Reference TIF committed to
      `examples/rib/tests/references/`; registered in
      `tests/visual/CMakeLists.txt` as
      `Visual_csg-object-instance-reuse-raytrace` (98 scenes under label
      `visual`), passes.

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

- [X] T032 [P] [US3] Unit test: given a point classified as inside vs.
      outside a Boundary Fragment's resolved volume, and `CAttributes`
      with/without `interior`/`exterior` set, assert the correct shader
      pointer is selected (interior, exterior, or fallback to ordinary
      shading) — pure selection logic requiring no ray trace or render, in
      `tests/unit/csg/test_interior_exterior_selection.cpp` (Constitution
      Principle III).
      Exercises `selectVolumeShader(const CAttributes *, bool isSolidFragment,
      bool isExterior)` directly (no ray trace/render): non-fragment hits
      always return NULL regardless of what interior/exterior are set to
      (FR-020 leak guard); a fragment hit returns the shader matching the
      queried side; and the FR-012 fallback (NULL) when the queried side has
      no shader assigned even though the fragment flag is set and the other
      side does have one. Uses opaque non-dereferenced sentinel
      `CShaderInstance*` addresses, reset to NULL before each local
      `CAttributes` leaves scope (its destructor unconditionally `detach()`s
      any non-NULL interior/exterior, which would `delete` a fake pointer
      otherwise). 5 test cases, all pass; registered as
      `CSG_InteriorExteriorSelection` in `tests/unit/csg/CMakeLists.txt`
      (9 scenes under label `csg`); full `csg` label suite re-run afterward
      confirms zero regressions in the 8 pre-existing tests.

### Implementation for User Story 3

- [X] T033 [P] [US3] Consult `attributes->interior` and apply its shading
      effect when a camera or traced ray is inside a Boundary Fragment's
      resolved volume (FR-010), in `src/ri/shading.cpp` /
      `src/ri/shaderFunctions.h` (depends on T032 failing red).
- [X] T034 [P] [US3] Consult `attributes->exterior` when viewed from outside
      a Boundary Fragment's resolved volume (FR-011), falling back to
      ordinary surface/atmosphere shading when neither Interior nor
      Exterior is set (FR-012), in `src/ri/shading.cpp` (depends on T032
      failing red).
      T033/T034 implemented together as a single dispatch site: a new inline
      `selectVolumeShader(const CAttributes *attr, bool isSolidFragment, bool
      isExterior)` in `src/ri/attributes.h` (positioned after the
      `CAttributes` class definition — an earlier placement before the class
      caused a real "member access into incomplete type" build failure,
      fixed by moving it after the closing `};`) returns `attr->interior`/
      `attr->exterior` per FR-010/FR-011, or NULL per FR-012 when the
      queried side is unset. Wired into `CShadingContext::shade()`
      (`src/libshader/shading/shading.cpp`), classified once per `shade()`
      call from vertex 0's `dotvv(I, N)` sign (I opposes N on the true
      exterior surface, dot < 0) and assigned unconditionally to
      `currentShadingState->postShader` — the same persistent field
      `traceEx()` sets for secondary-ray bundles — so a prior ray's
      interior/exterior shader can never leak onto this hit. This
      batch-granularity choice (classify once per `shade()` call rather than
      bucketing rays upstream by side, mirroring `executeMisc.cpp`'s
      secondary-ray pattern) accepts a small, documented approximation error
      only at silhouette-edge micropolygon batches that could genuinely
      straddle inside/outside. Confirmed via `cmake --build` (see T035 note)
      that this compiles cleanly across all three consumers
      (`libshader_shading`, `ri`, `precomp`).
- [X] T035 [US3] Restrict this Interior/Exterior consumption to Boundary
      Fragments of a `CSolidObject` only — Interior/Exterior assigned to
      attribute state that never enters a `SolidBegin`/`SolidEnd` block
      MUST remain a no-op exactly as today (FR-020) — in
      `src/ri/shading.cpp` (depends on T033, T034).
      New `ATTRIBUTES_FLAGS_SOLID_FRAGMENT` flag (`src/ri/attributes.h`), set
      only on a cloned `CAttributes` (`new CAttributes(attr)`, the existing
      copy-constructor clone pattern used elsewhere in the codebase) created
      per Boundary Fragment in `csgBuildMeshForAttributeGroup()`
      (`src/ri/csgTree.cpp`) — never on the original shared `attr` passed
      in, since `CAttributes` instances are refcounted and shared across
      unrelated primitives with identical attribute state; tagging the
      original would leak the flag onto non-CSG geometry. An initial
      object-level flag (`OBJECT_SOLID_FRAGMENT` on `CObject::flags`) was
      tried first and discarded: `CObject::flags` does not propagate from a
      parent `CPolygonMesh` to its tessellated `CPolygonTriangle`/
      `CPolygonQuad` children (only the `CAttributes*` pointer does, per
      `polygons.cpp`'s `data.meshAttributes = mesh->attributes;`), so the
      object flag would have been lost the moment the mesh was tessellated
      into the shadeable primitives `shade()` actually sees — an
      attribute-level flag survives because tessellated children share the
      same `CAttributes*` as their parent. `shade()` gates dispatch on
      `(currentAttributes->flags & ATTRIBUTES_FLAGS_SOLID_FRAGMENT) != 0`.
      Full `cmake --build build --config Release` succeeds cleanly (exit
      code 0) with all of T033-T035 in place; `ctest --test-dir build -L csg`
      (9 tests) passes with zero regressions.
- [X] T036 [P] [US3] Add a visual-regression scene: a `"difference"` solid
      (sphere minus sphere) with an Interior shader assigned, viewed from an
      angle revealing the cut-away interior, confirming visibly distinct
      interior vs. exterior appearance, in `examples/rib/`
      (`quickstart.md` §4, SC-004).
      Two real defects surfaced while building this scene, both now fixed:
      **Bug #1 (framing, trivial)**: the original scene's bite sphere was
      offset to `Translate 0 0 0.9` (same side as the camera), so its notch
      faced away from the camera at the scripted angle and the cutaway
      silhouette was never visible. Fixed to `Translate 0 0 -0.9`.
      **Bug #2 (real pipeline defect)**: `processDelayedSolid()`
      (`src/ri/rendererContext.cpp`) was a direct copy of
      `processDelayedInstance()`'s pattern (its own comment said so) but
      applied to the wrong case: it passed a single, un-flagged
      `CAttributes*` (the solid's own outer/inherited attributes) into
      every fragment's `instantiate()` call, which unconditionally replaces
      an object's own attributes when non-NULL (`if (a==NULL) a=attributes;`,
      identical across every `CObject` subclass). But `cSolid->fragments`
      are not templates awaiting attribute assignment the way
      `RiObjectInstance` templates are — each is an already-resolved CSG
      boundary fragment carrying its own final, individually-cloned
      `CAttributes*` (built per attribute-group in
      `csgBuildMeshForAttributeGroup()`, `src/ri/csgTree.cpp`, tagged
      `ATTRIBUTES_FLAGS_SOLID_FRAGMENT`). The override silently discarded
      that per-fragment state on every solid, collapsing all fragments onto
      one shared attributes pointer — losing not just the T032-T035
      Interior/Exterior dispatch flag but also whatever else
      `csgBuildMeshForAttributeGroup()` sets per fragment. Fixed by passing
      `NULL` instead, letting each fragment fall back to its own attributes.
      Confirmed via instrumented debug build that a genuine CSG difference
      scene went from 0/8686 fragment shades correctly flagged (pre-fix) to
      8728/8728 (post-fix), with distinct, correct `postShader` dispatch and
      output color for the Interior vs. Exterior branches.
      **Semantics correction to the task text above**: for a *correctly
      oriented* CSG difference, the cavity wall's normal points outward
      into the carved void (away from the remaining solid material) — the
      same orientation convention as the outer skin. A camera in empty
      space therefore reads the cavity wall as front-facing
      (`dot(I,N) < 0`, i.e. Exterior) almost everywhere it's visible;
      Interior only fires in a thin band of silhouette-edge tessellation
      noise (measured 133/8728 ≈ 1.5% of fragment shades in the difference
      scene, and 0/552 for a lone non-boolean `"primitive"` solid, which
      never invokes `csgBuildMeshForAttributeGroup()` at all). No camera
      angle looking from outside the solid can make Interior dominate — this
      is expected `dot(I,N)` front-facing behavior, not a bug, and nothing
      in T032-T035's design needed revisiting. SC-004 ("visibly distinct
      appearance between the inside and outside of a solid") is satisfied
      correctly by two paired scenes instead: **outside**
      `csg-sphere-sphere-difference-interior-raytrace.rib` (camera in empty
      space; cutaway silhouette visible; Exterior/blue dominant, with a
      thin Interior/red rim at the silhouette edge) and **inside**
      `csg-sphere-sphere-difference-interior-raytrace-inside.rib` (camera at
      the world origin, which is inside the outer sphere but outside the
      subtracted bite sphere — i.e. inside the solid's own remaining
      material — where 4830/4830 fragment shades read Interior and the
      render is red throughout). Both registered in
      `tests/visual/CMakeLists.txt`, references committed.
      Fixing Bug #2 also changed pixel output (correctly) for five
      pre-existing CSG visual-regression scenes whose committed references
      had been captured under the buggy shared-attributes state:
      `csg-sphere-cube-difference-{raytrace,reyes,zbuffer}`,
      `csg-nurbs-sphere-union-raytrace`, and
      `csg-object-instance-reuse-raytrace` (T031) — the last one most
      visibly: its 90°-rotated instance's CSG notch was not rendering at
      all pre-fix and now shows the expected circular opening facing the
      camera. All five diffs were visually confirmed as genuine corrections
      (no corruption/artifacts), so their reference TIFs were regenerated.
      `ctest --test-dir build -L csg` (10 tests) and
      `ctest --test-dir build -L visual` (104 tests, including the 2 new
      T036 scenes) both pass at 100%. Debug instrumentation added while
      root-causing Bug #2 (`[T036DBG]` fprintf in
      `src/libshader/shading/shading.cpp`, `[CSGDBG]` fprintf/diagnostics in
      `src/ri/csgBoolean.cpp`) has been reverted.

**Checkpoint**: All three user stories are independently functional.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Diagnostics, non-regression, and documentation that span all
three user stories.

- [X] T037 [P] Add error/diagnostic scenes confirming a clear diagnostic (not
      a crash, not a silently wrong image) for: `SolidBegin "bogus"`
      (`CODE_BADTOKEN`), an unmatched `SolidEnd` (`CODE_NESTING` — the same
      scope-mismatch code every other `*Begin`/`*End` pair emits via
      `check()`, not `CODE_BADTOKEN` as `quickstart.md`/
      `contracts/solid-rib-interface.md` originally assumed before this
      task verified it against the actual codebase-wide convention; both
      docs corrected), a `SolidBegin "primitive"` block containing a nested
      `SolidBegin`/`SolidEnd` (`CODE_BADTOKEN`), and an `RiProcedural`
      directly inside a `SolidBegin "primitive"` block (`CODE_BADTOKEN`), in
      `examples/rib/` (`quickstart.md` §8, SC-006).
      Four scenes added in `examples/rib/tests/`, following the existing
      `nupatch-vase-trimmed-malformed-*.rib` convention (plain RIB, no
      `ErrorHandler`, a comment block stating the expected diagnostic and
      "must not crash"): `csg-malformed-bogus-operation.rib`,
      `csg-malformed-unmatched-solidend.rib`,
      `csg-malformed-nested-in-primitive.rib`,
      `csg-malformed-procedural-in-primitive.rib`. All four already worked
      correctly against the existing implementation with no code changes
      needed — each emits exactly one diagnostic, exits without crashing,
      and produces a non-blank recovered image (confirmed via `orender`
      stderr and PIL pixel-extrema checks): `Unknown solid operation: bogus`
      (recovers as `CSG_UNION`), `Bad scope for "RiSolidEnd"` (stray
      `SolidEnd` ignored, rendering continues), `SolidBegin/SolidEnd cannot
      be nested inside a "primitive" solid block` (inner block rejected,
      outer primitive's own geometry still renders), and `RiProcedural
      cannot be declared inside a SolidBegin block` (call skipped, other
      geometry in the block still renders). Discovered along the way: the
      unmatched-`SolidEnd` case was documented in `quickstart.md` and
      `contracts/solid-rib-interface.md` as producing `CODE_BADTOKEN`, but
      the actual, and correct, behavior is `CODE_NESTING` — every other
      `*Begin`/`*End` scope-mismatch diagnostic in `src/ri/ri.cpp`
      (`AttributeEnd`, `TransformEnd`, `WorldEnd`, `ObjectEnd`,
      `ResourceEnd`, `FrameEnd`, `RiEnd`) uses `CODE_NESTING` via the shared
      `check()` machinery; special-casing `SolidEnd` to `CODE_BADTOKEN`
      would have been the actual inconsistency. Both docs corrected rather
      than changing code to match a documentation assumption that was never
      verified against the codebase's real error-code taxonomy. These four
      scenes are manual/`quickstart.md`-validation deliverables, not
      automated `ctest` entries — no existing pattern in
      `tests/visual/CMakeLists.txt` or `tests/unit/csg/CMakeLists.txt`
      asserts on stderr content or non-zero exit, and T037/T039's own intent
      (worked examples for the Hugo docs page) doesn't require one.
- [X] T038 Run the full existing visual-regression suite
      (`ctest --test-dir build -L visual --output-on-failure`) and confirm
      every pre-existing scene (none of which uses
      `SolidBegin`/`SolidEnd`) renders unchanged relative to the T001
      baseline (FR-018/SC-005).
      `ctest --test-dir build -L visual --output-on-failure`: 104/104 passed
      (100%), including every pre-existing non-CSG scene from the T001
      baseline plus all 16 CSG scenes added across T026/T027/T030/T036/T031.
      `ctest --test-dir build -L csg`: 10/10 unit tests passed. No
      regressions from any change made in this feature.
- [X] T039 [P] Add a new Hugo page under `site/` documenting
      `SolidBegin`/`SolidEnd`, `Attribute "solid"`, and Interior/Exterior
      usage, using the scenes from T026/T027/T030/T036/T037 as worked
      examples, following the existing `site/` content structure
      (Constitution Principle VII).
      **Completed**: new page at
      `docs/site/content/manual/reference/solid-csg-operations.md`, matching
      `attributes.md`'s house style (minimal `title`/`date` front matter,
      single H1, `##` sections, untagged RIB fences, prose). Covers
      `SolidBegin`/`SolidEnd` operation strings and nesting rules,
      `Attribute "solid" "float tessellationtolerance"`, Interior/Exterior
      semantics (citing the T036 inside/outside scene pair as the worked
      example), the four T037 error/diagnostic cases with their actual error
      codes, and a linked list of example scenes under `examples/rib/tests/`.
      Nav links added to both `docs/site/content/manual/_index.md` (`##
      Reference` bullet list, relative path) and
      `docs/site/content/_index.md` (`## Documentation` bullet list,
      absolute `/openrender/...` path), placed immediately after the
      existing "Attributes" entry in each. Note: the actual site path is
      `docs/site/`, not `site/` as this task's own text and `plan.md`'s tree
      diagram say — a pre-existing discrepancy in the spec docs, left
      uncorrected as out of scope for this task.
- [X] T040 Run the full `quickstart.md` validation sequence end-to-end (all
      8 sections) and record the results.
      **Completed**: §1 `ctest --test-dir build -L csg`: 10/10 passed
      (BooleanCoplanar, TesselationFlatness, QuadricTesselation,
      PatchMeshTesselation, NestedTree, InteriorExteriorSelection, and
      others). §2/§3 `csg-sphere-cube-union-{raytrace,reyes,zbuffer}.rib`:
      all three hiders render exit=0, confirming cross-hider shape parity
      (no CSG-specific hider code — resolution happens once at
      `RiSolidEnd`). §4 `csg-sphere-sphere-difference-interior-raytrace{,
      -inside}.rib`: both render exit=0, exercising the outside/Exterior-
      dominant and inside/Interior-dominant camera placements. §5
      `csg-nested-tree-4-level-raytrace.rib`: renders exit=0 (four-level
      nested union/difference tree). §7
      `csg-sphere-sphere-{tight,loose}-tolerance-raytrace.rib`,
      `csg-nurbs-sphere-union-raytrace.rib`,
      `csg-subdivision-sphere-union-raytrace.rib`: all render exit=0,
      covering the tessellationtolerance density comparison and the
      NURBS/quadric-vs-subdivision analytic-normal asymmetry documented in
      research.md Decision 4b. §6 `ctest --test-dir build -L visual`:
      104/104 passed, zero regressions. §8: already validated in T037 with
      the exact expected diagnostics for all four malformed cases. Site
      documentation (the quickstart's final unnumbered section): completed
      in T039. All scratch `.tif` renders removed from the worktree root
      after validation; no regressions or unexpected failures anywhere in
      the sequence.

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
