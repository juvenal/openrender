---

description: "Task list for spec 021 — deferred, on-demand CSG evaluation"
---

# Tasks: Deferred, On-Demand Evaluation of Solid (CSG) Geometry

**Input**: Design documents from `/specs/021-deferred-csg-evaluation/`

**Prerequisites**: [plan.md](plan.md), [spec.md](spec.md), [research.md](research.md), [data-model.md](data-model.md), [contracts/](contracts/), [quickstart.md](quickstart.md)

**Tests**: REQUIRED. Constitution Principle III (TDD) is non-negotiable: every new capability gets a failing test before it is implemented. Tasks marked "verification" exercise behavior an earlier phase already delivered, so they may pass as soon as they are written.

**Organization**: Tasks are grouped by user story. User Story 1 depends on the Foundational phase; Stories 2–4 build on the CSG solid that User Story 1 introduces (see Dependencies).

**Workflow**:
- All work happens in the worktree `../openrender-worktrees/021-deferred-csg-evaluation`. The main checkout stays on `master`.
- At every **Checkpoint**, stage the phase's files and stop for the maintainer to review and commit. Never commit.
- Run builds, renders and ctest under `nice -n 15` with `taskpolicy -b` on macOS.
- Run the visual suite sequentially or with low `-j`, to avoid throttling timeouts.

**Class names** (data-model.md):
- `CCSGDefinition`: shared, immutable tree
- `CCSGSolid`: the scene `CObject`
- `CCSGInstanceState`: per-instance state
- `CCSGRegion`: implements the generic `CRegionPredicate`
- `CRegionPredicate`, `CSurface::region` and `CRasterGrid::coverage`: generic names, which the hider may reference (contract §0)

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel: different files, no dependency on incomplete tasks.
- **[Story]**: US1–US4, mapping to the spec's user stories.

## Path Conventions

Single project at the repository root: `src/ri/...`, `tests/...`, `examples/rib/tests/...`.

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: build wiring, measurement harness, test scenes and the baseline, all before any behavior change.

- [ ] T001 Create compilable stub pairs `src/ri/geometry/csgDefinition.{h,cpp}`, `src/ri/geometry/csgSolid.{h,cpp}`, `src/ri/geometry/csgClassify.{h,cpp}` and `src/ri/geometry/csgCaps.{h,cpp}` (file header block in the existing style; empty declarations only). Add the four `.cpp` files to the ribRender source list in `src/ri/CMakeLists.txt` next to `geometry/csgTree.cpp` (~L64). Add them to the libribpreview exclusion list beside `geometry/csgTree.cpp`/`geometry/solidObject.cpp` (~L344/L369), extending the comment at ~L295. Confirm `cmake --build build` stays warning-free (`-Werror`).
- [ ] T002 [P] Create the seam-metric CLI `tests/visual/test_csg_seam_metric.cpp`, modeled on `tests/visual/test_radial_histogram.cpp`. It reads RGBA TIFFs via libtiff, prints one CSV summary line per check, and exits PASS/FAIL. It supports `--help` describing every mode (constitution IV), and four modes:
  - **(a) `--annuli`**: given a center, list of "must be clear" discs (alpha < 0.01 everywhere), "must be solid" annuli (alpha ≥ 0.99 everywhere) and seam radii with a tolerance in pixels, report stray-pixel counts and the maximum radial deviation of the alpha/edge transition from each analytic seam radius.
  - **(b) `--edge-compare <candidate> <reference> --tolerance <px>`**: report the maximum distance between corresponding silhouette/seam edges (from the alpha edge and luminance gradient) of two renders.
  - **(c) `--region-compare <a> <b> --mask-annulus ...`**: block-average diff restricted to a region.
  - **(d) `--region-color <image> --annulus <cx> <cy> <r_in> <r_out> --rgb <r> <g> <b> --tolerance <t>`**: mean color inside an annulus must lie within tolerance of the expected color.
- [ ] T003 Register the `test_csg_seam_metric` executable in `tests/visual/CMakeLists.txt`, next to `test_radial_histogram` (~L40), linking `TIFF::TIFF` with cxx_std_20.
- [ ] T004 [P] Create the six hero scenes `examples/rib/tests/csg-hero-{raytrace,reyes,zbuffer}-{800,1600}.rib`:
  - sphere `1.50 -1.50 1.50 360` minus `Cylinder 0.65 -2 2 360`
  - `Translate 0 0 9`, `fov 38`, `raydistant` light, `plastic` surface
  - Formats 800×500 and 1600×1000, `Display` RGBA
  - A header comment documenting the analytic radii. At 800×500: clear through-hole disc r=45.6 px, near seam ring r=61.7 px, sphere silhouette r=122.7 px. Scale ×2 at 1600×1000.
- [ ] T005 [P] Create the six sphere-minus-cube scenes `examples/rib/tests/csg-sphere-cube-difference-{raytrace,reyes,zbuffer}-{800,1600}.rib` (square formats 800 and 1600), copying the geometry and camera of `examples/rib/tests/csg-sphere-cube-difference-reyes.rib`.
- [ ] T006 [P] Create the coincident-face scenes `examples/rib/tests/csg-coincident-{union,difference}-{raytrace,reyes,zbuffer}.rib`:
  - two unit boxes (six bilinear patches each) sharing one face for the union
  - a box minus a box flush with one face for the difference
  - a camera angle that exposes the shared or flush face
- [ ] T007 [P] Create the implied-cap scenes `examples/rib/tests/csg-open-quadric-caps-{raytrace,reyes,zbuffer}.rib`: a sphere r=1 intersected with `Cylinder 0.6 -0.5 0.5 180` (an open half-sweep, a single-quadric operand), oriented so that both flat sweep-gap caps and one end cap lie on the composite boundary and face the camera.
- [ ] T008 [P] Create the procedural-operand scenes:
  - `examples/rib/tests/archives/csg-sphere-operand.rib` (contains `Sphere 1.50 -1.50 1.50 360`)
  - `examples/rib/tests/csg-procedural-operand-raytrace.rib` (hero geometry, with the sphere supplied by `Procedural "DelayedReadArchive" ["archives/csg-sphere-operand.rib"] [-1.5 1.5 -1.5 1.5 -1.5 1.5]` inside `SolidBegin "primitive"`)
  - its inline twin `examples/rib/tests/csg-procedural-operand-inline-raytrace.rib`

  **Path note**: visual tests run with `WORKING_DIRECTORY` set to a per-test scratch directory (`tests/visual/CMakeLists.txt:171`). The relative archive path therefore resolves only because the T055 registration copies `examples/rib/tests/archives/` into that scratch directory at configure time (`file(COPY …)`).
- [ ] T009 [P] Create the remaining scenes:
  - `examples/rib/tests/csg-offscreen-stats.rib`: one hero solid placed behind the camera, plus a plain visible sphere; with `Option "statistics" "endofframe" [1]`.
  - `examples/rib/tests/csg-tolerance-inert-raytrace.rib`: the hero scene with `Attribute "solid" "float tessellationtolerance" [0.01]` set three times on different operands.
  - `examples/rib/tests/csg-secondary-rays-raytrace.rib`: the hero solid over a ground plane, lit so its shadow shows the through-hole, with a mirror plane reflecting it.
  - `examples/rib/tests/csg-secondary-rays-reyes.rib`: its REYES twin, with shader-traced shadows and reflections.
  - `examples/rib/tests/csg-hero-partial-stats.rib`: the hero solid framed so `ScreenWindow` shows only its left half, with `Option "statistics" "endofframe" [1]`.
- [ ] T010 [P] Create `examples/rib/tests/csg-hero-two-shaders-{raytrace,reyes}.rib`: the hero scene with `Surface "plastic"` + gold `Color [1 0.8 0.3]` on the sphere operand and `Surface "matte"` + blue `Color [0.2 0.35 1]` on the cylinder operand (FR-016).
- [ ] T011 [P] Create the edge-case scenes:
  - `examples/rib/tests/csg-hero-displaced-{raytrace,reyes}.rib`: the hero scene with `Displacement "dented"` on the sphere operand and a matching `Attribute "displacementbound"`.
  - `examples/rib/tests/csg-hero-translucent-{raytrace,reyes}.rib`: the hero scene with the sphere operand at `Opacity [0.5 0.5 0.5]`, so the hole walls are seen through it.
- [ ] T012 Record master baselines in `specs/021-deferred-csg-evaluation/baseline-timings.md`:
  - wall-clock render time, throttled, for every existing `examples/rib/tests/csg-*.rib` visual scene, the `blobby-csg-*` scenes and the T004/T005 scenes
  - the T002 tool's metric output on master for T004/T005/T006 (the expected-red baseline; the hero scene should reproduce the 213 / 63 / 38 stray pixels at 1600×1000)

**Checkpoint (Phase 1)**: harness and scenes exist, baseline recorded, build warning-free, no behavior change. Stage and pause for commit.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: the geometry-domain core, the generic region hook and the statistics counters. **No rendering behavior changes in this phase.** Every existing visual test must still pass.

**⚠️ CRITICAL**: No user-story work can begin until this phase is complete.

### Tests for Phase 2 (write first, confirm they FAIL) ⚠️

- [ ] T013 [P] Create `tests/unit/csg/test_count_crossings.cpp`. Assert the **unsigned** crossing counts (undisplaced surface; ignoring visibility flags, LOD, backface, displacement and the surface's orientation attributes) for rays with known answers against:
  - `CSphere`, `CDisk`, `CCone`, `CParaboloid`, `CCylinder`, `CHyperboloid`, `CToroid` (including partial `thetamax` and z-cuts)
  - `CPolygonTriangle`, `CPolygonQuad`
  - `CBilinearPatch`, `CBicubicPatch`, `CNURBSPatch`, `CPatchGrid`, `CBSplinePatchGrid`, `CSubdivision`

  For the free-form types (`CBicubicPatch`, `CNURBSPatch`, `CPatchGrid`, `CBSplinePatchGrid`, `CSubdivision`), also assert that the Newton-refined crossing positions agree with the same surface's own `CTesselationPatch` intersection to within that tessellation's tolerance (FR-007a).
- [ ] T014 [P] Create `tests/unit/csg/test_classify_quadrics.cpp`. Assert analytic implied-closed classification (IN/OUT) for single-quadric operands:
  - closed sphere and torus
  - cylinder capped at z-min/z-max
  - z-cut sphere
  - half-sweep cylinder (sweep-gap planes)
  - cone, paraboloid, hyperboloid
  - a lone `CDisk`, which is an empty operand (always OUT)

  Use points on both sides of every implied cap.
- [ ] T015 [P] Create `tests/unit/csg/test_parity_robustness.cpp`. Assert parity classification (odd crossings ⇒ IN) for multi-surface operands:
  - a six-`CBilinearPatch` box
  - the same box with **deliberately mixed vertex orders**
  - the same box with **one patch under `ReverseOrientation`**
  - a `CCylinder` plus two `CDisk`s (author-closed; no doubled caps)
  - a polygon-mesh cube
  - a polygonized blobby
  - **two overlapping spheres in one operand** (the overlap classifies OUT, per the spec edge case)

  Include query points whose default ray grazes an edge or vertex or runs parallel to a face; the fallback direction plus majority-of-three vote must still classify them correctly. Also assert that a single open `CNURBSPatch` operand emits exactly one "operand is not closed" diagnostic per solid (captured via the error handler) and still classifies deterministically (FR-021).
- [ ] T016 [P] Create `tests/unit/csg/test_boundary_rule.cpp`. For union, intersection and difference (2 and 3 operands), the 4-level nested tree from `csg-nested-tree-4-level-raytrace.rib`, and single-operand and empty collapse, assert:
  - the boundary rule (pivotal-operand test) at points on each leaf
  - static orientation parity (odd count of subtrahend positions ⇒ flipped)
  - short-circuit evaluation: a counter shows irrelevant operands are not classified
  - `classifyBound()` uniform results (`ALL_IN`/`ALL_OUT`/`MIXED`) for bounds clear of, inside of, and straddling other operands
  - **analytic seam location** (SC-002): for the sphere-cube difference, sample 64 points on each exact intersection circle (sphere ∩ each cube face plane). At each, offset ±δ (δ = 1e-4·r) along the sphere's surface, perpendicular to the curve. The sphere leaf's boundary predicate must flip between the two offsets, and the matching face-side check must hold. Do the same for the hero's two circles at z = ±√(1.5² − 0.65²).
  - **tangency**: two spheres touching at one point, and one sphere internally tangent to another; neither produces boundary points away from the contact.
- [ ] T017 [P] Create `tests/unit/csg/test_classify_determinism.cpp`. Classify the same point set in forward order, in shuffled order, and from 8 `std::thread`s concurrently; assert bit-identical results (FR-008).
- [ ] T018 [P] Create `tests/unit/csg/test_csg_stats.cpp`. Assert that `CStats` exposes `numCsgClassifications`, `numCsgSeamSplits`, `numCsgInstancesResolved` and `numCsgProceduralExpansions`; that they are zero-initialized per frame and printed by the end-of-frame statistics; and that one `classify()` call increments `numCsgClassifications` by exactly 1.
- [ ] T019 [P] Rewrite `tests/unit/csg/test_boolean_boxes.cpp`, `test_boolean_sphere_box.cpp` and `test_boolean_sphere_sphere.cpp` as classification and boundary-rule tests over the same scenarios. Replace volume-sign assertions with IN/OUT and on-boundary assertions at analytic points.
- [ ] T020 [P] Rewrite `tests/unit/csg/test_boolean_coplanar.cpp` (two-sided ±ε rule: no internal skin for flush-box union, no leftover skin for flush difference), `test_nested_tree.cpp` and `test_multi_primitive_leaf.cpp` the same way. Update `tests/unit/csg/csgTestUtils.h`: remove the `computeVolume`/volume-sign helpers, add analytic point-set builders.
- [ ] T021 In `tests/unit/csg/CMakeLists.txt`, register T013–T018 as executables with labels `csg;unit`, following the existing `add_executable`/`add_test` pattern. Keep the rewritten suites' registrations. Build, then run `ctest --test-dir build -L csg` and confirm the new and rewritten tests FAIL (red) for the right reasons.

### Implementation for Phase 2

- [ ] T022 In `src/ri/geometry/object.h`, add:
  - the generic `CRegionPredicate` interface (`accepts(const float *P)`, and `classifyBound(const float *bmin, const float *bmax)` returning `ALL_IN`/`ALL_OUT`/`MIXED`)
  - a nullable `CSurface::region` member (default null ⇒ today's behavior)
  - the virtual `int CSurface::countCrossings(const float *from, const float *dir) const`, which returns an **unsigned** crossing count

  All names must be generic, with no `csg`/`CSG`/`Solid` (contracts/geometry-region-interface.md §0).
- [ ] T023 [P] Implement `countCrossings` in `src/ri/geometry/quadrics.{h,cpp}` for every quadric class, analytically. Count every root in the valid u/v range (not just the nearest, unlike `CSphere::intersect()` at quadrics.cpp:167).
- [ ] T024 [P] Implement `countCrossings` in `src/ri/geometry/polygons.{h,cpp}` for `CPolygonTriangle`/`CPolygonQuad` (exact).
- [ ] T025 [P] Implement `countCrossings`:
  - `src/ri/geometry/patches.{h,cpp}`: `CBilinearPatch` analytically; `CBicubicPatch`/`CNURBSPatch` tessellation-seeded and Newton-refined to float precision (research D3)
  - `src/ri/geometry/patchgrid.{h,cpp}`: `CPatchGrid`
  - `src/ri/geometry/bsplinePatchgrid.{h,cpp}`: `CBSplinePatchGrid`
  - `src/ri/geometry/subdivision.{h,cpp}`: `CSubdivision`
- [ ] T026 Implement `CCSGDefinition` in `src/ri/geometry/csgDefinition.{h,cpp}` (data-model.md):
  - build from a closed `CSGTreeNode` tree: flattened nodes, operands with their kind (single-quadric / multi-surface / has-procedurals / empty), `leafFlip[]` parity, and the CSG-aware bound
  - empty-tree and single-operand collapse (spec 013 FR-016/FR-017)
  - lone zero-volume operand ⇒ empty
  - lone open free-form operand ⇒ one-time "operand is not closed" diagnostic (FR-021), reported at a level `CODE_LOG` does not swallow
- [ ] T027 Implement classification in `src/ri/geometry/csgClassify.{h,cpp}`:
  - operand `classify(P)`: bound rejection first; analytic implied-closed test for single-quadric operands; otherwise deterministic fixed-direction **parity** over the operand's surfaces (`countCrossings`), with grazing/edge fallback and majority-of-three
  - the boundary rule with short-circuit evaluation
  - the two-sided ±ε ON rule (ε = `C_EPSILON` × solid bound size)
  - `CCSGRegion` implementing `CRegionPredicate` (`accepts`, `classifyBound`)

  Make T013–T017, T019 and T020 green.
- [ ] T028 Add the four counters to `CStats` in `src/ri/core/stats.h`. Initialize and print them in `src/ri/core/stats.cpp` with the existing end-of-frame statistics output. Increment `numCsgClassifications` from `src/ri/geometry/csgClassify.cpp`. Make T018 green.
- [ ] T029 Phase 2 gate:
  - `ctest -L csg` is green
  - the full `ctest -L visual` (sequential) is unchanged versus master, apart from the known flaky and pre-existing failures listed in quickstart.md §3
  - the quickstart §7 grep gate returns no matches

**Checkpoint (Phase 2)**: geometry core complete and tested; generic region hook in place; zero rendering change. Stage and pause for commit.

---

## Phase 3: User Story 1 — Boolean solids render with crack-free, smooth seams (Priority: P1) 🎯 MVP

**Goal**: the hero, sphere-minus-cube, coincident-face and free-form scenes render with no cracks, slivers, spikes or facets in all three hiders, at both resolutions. Each face keeps its operand's shader.

**Independent Test**: the T002 seam metric on the T004/T005/T006/T007/T010 scenes and the free-form scenes: 0 stray pixels, 0 see-through gaps, seams within 1 px, correct per-operand colors (quickstart.md §1).

### Tests for User Story 1 (write first, confirm they FAIL) ⚠️

- [ ] T030 [US1] Register the hero seam tests `CsgSeam_hero_{raytrace,reyes,zbuffer}_{800,1600}` in `tests/visual/CMakeLists.txt`. Each renders a T004 scene, then runs `test_csg_seam_metric --annuli` with the analytic radii, requiring 0 stray pixels in the clear disc, 0 see-through pixels in the solid annulus, and seam deviation ≤ 1 px. Labels: `csg;visual`. Confirm red on current code: they must reproduce the T012 baseline failures.
- [ ] T031 [US1] Register `CsgSeam_spherecube_{raytrace,reyes,zbuffer}_{800,1600}` in `tests/visual/CMakeLists.txt` for the T005 scenes. Require 0 see-through gaps (alpha check inside the object silhouette). The REYES and Z-buffer variants also run `--edge-compare` against the raytrace render at the same resolution, with tolerance 1 px. The raytrace seam's own accuracy is pinned analytically by T016. Confirm red.
- [ ] T032 [US1] Register `CsgSeam_coincident_{union,difference}_{raytrace,reyes,zbuffer}` in `tests/visual/CMakeLists.txt`, requiring 0 see-through pinholes along the shared or flush face and no internal or leftover skin (FR-022). Confirm red.
- [ ] T033 [US1] Register `CsgPerOperandShader_hero_{raytrace,reyes}` in `tests/visual/CMakeLists.txt` for the T010 scenes, using `test_csg_seam_metric --region-color`. The hole-wall annulus (45.6–61.7 px at 800×500) must be the cylinder's blue, and the outer annulus (62–122 px) the sphere's gold, each within tolerance (FR-016, US3 scenario 2). Confirm red.
- [ ] T034 [US1] Create REYES twins `examples/rib/tests/csg-nurbs-sphere-union-reyes.rib` and `examples/rib/tests/csg-subdivision-sphere-union-reyes.rib` of the existing raytrace scenes. Register `CsgSeam_freeform_{nurbs,subdiv}` in `tests/visual/CMakeLists.txt`, requiring (FR-007a):
  - 0 see-through gaps
  - REYES vs. raytrace `--edge-compare` ≤ 1 px
  - `--region-compare` of the free-form operand's untouched region vs. the same surface rendered without a solid block (add `examples/rib/tests/csg-{nurbs,subdivision}-sphere-alone-raytrace.rib`)

  Confirm red.
- [ ] T035 [P] [US1] Create `tests/unit/csg/test_implied_caps.cpp`. For each single-quadric kind (cylinder ends, z-cut sphere, half-sweep cylinder, cone base, paraboloid top, hyperboloid ends), assert:
  - the number, plane and orientation of the synthesized cap patches
  - that each cap's predicate accepts exactly the cross-section inside the lateral surface

  Register it in `tests/unit/csg/CMakeLists.txt` (`csg;unit`). Confirm red.

### Implementation for User Story 1

- [ ] T036 [US1] Implement `CCSGSolid` and `CCSGInstanceState` in `src/ri/geometry/csgSolid.{h,cpp}` (data-model.md):
  - a `CObject` that is not `OBJECT_DUMMY` (trace.cpp:332), bounded by the definition bound under its transform
  - `dice()`/`intersect()` build the `CCSGInstanceState` once under `CRenderer::delayedMutex`, mirroring `CDelayedObject::intersect()` (geometryDispatch.cpp:679): instantiate each template leaf with the composed transform via its `instantiate()`; clone each leaf's attributes and set `ATTRIBUTES_FLAGS_SOLID_FRAGMENT`; toggle `ATTRIBUTES_FLAGS_INSIDE` per `leafFlip`; attach a `CCSGRegion` to each leaf's `region`
  - set `children` to the leaves for raytracing (`setChildren`)
  - `dice()` forwards to each leaf's `dice()`
  - `instantiate()` creates a new `CCSGSolid` over the same definition with a composed transform (as `CSolidObject::instantiate()` does)
  - increment `numCsgInstancesResolved`
- [ ] T037 [US1] Switch `RiSolidEnd` to the new path:
  - in `src/ri/geometry/csgTree.{h,cpp}`, replace `resolveCSGTree()` with a function that builds a `CCSGDefinition` from the closed tree and calls `context->addObject(new CCSGSolid(...))`
  - delete the now-unused static helpers (`csgResolveNode`, `csgToSoup`, `csgTessellateOperand`, `csgTriangulateGrid`, `csgMakeGridVertex`, `csgEmitTriangle`, `csgTriangleIsDegenerate`, `csgSignedVolume`, `csgFlipAll`, `csgPolygonsToFragments`, `csgBuildMeshForAttributeGroup`, `csgRebaseFragments`, `csgResolveTolerance`), to keep `-Werror` happy
  - update the call site in `src/ri/render/rendererContext.cpp` (~L5826)
  - leave `csgBoolean.*`, `solidObject.*` and `processDelayedSolid` in place but unreferenced; Phase 7 deletes them

  **Expected-red note**: from this task until T047, the existing `csg-*`/`blobby-csg-*` visual tests are expected red, because their references were rendered by the old BSP path. Never regenerate them to make a gate pass.
- [ ] T038 [US1] Raytrace hit acceptance for quadrics, in `src/ri/geometry/quadrics.cpp`: in every quadric `intersect()` root loop, before accepting a candidate, `continue` to the next root when `region && !region->accepts(P)`, where P is the undisplaced camera-space hit point. Leave `ray->t`/`ray->object` untouched on rejection.
- [ ] T039 [P] [US1] Apply the same region check at hit acceptance in `src/ri/geometry/polygons.cpp` (`CPolygonTriangle`/`CPolygonQuad` intersect paths).
- [ ] T040 [US1] Apply the same region check in `src/ri/geometry/surface.cpp` at the `CTesselationPatch` candidate acceptance (~L857 and ~L1024, alongside `trimAccepts`). For displaced leaves, evaluate the predicate at the undisplaced surface point for the hit's (u,v), via the object's own sample/interpolate.
- [ ] T041 [US1] **Raytrace exit check**: `CsgSeam_hero_raytrace_*`, `CsgSeam_spherecube_raytrace_*`, `CsgSeam_coincident_*_raytrace`, `CsgPerOperandShader_hero_raytrace` and the raytrace parts of `CsgSeam_freeform_*` are green. REYES and Z-buffer variants are expected red until T045.
- [ ] T042 [US1] Add the generic coverage mask (prerequisite: T030–T034 red):
  - a `CRasterGrid::coverage` field in `src/ri/hiders/reyes.h` (null ⇒ every micropolygon present), allocated and freed with the grid
  - in `src/ri/hiders/reyes.cpp`, where per-quad `bounds` are computed (~L1452 and ~L1705), give a quad marked absent an empty bound (`bounds[0] > bounds[1]`), so the existing skip tests in `stochasticQuad.h:703` and `zbufferQuad.h:40` drop it with **no rasterizer loop changes**

  Use generic names only (contract §0).
- [ ] T043 [US1] Add the REYES region stage to `CSurface::dice()` in `src/ri/geometry/surface.cpp` (~L145–362), parallel to and combinable with the trim stage (both must pass; trim behavior unchanged):
  - `region->classifyBound()` on the probe bound: `ALL_OUT` returns; `ALL_IN` dices normally (uniform shortcut)
  - `MIXED` forces a split (`udiv = 0`), terminated by screen size (split until the patch dices into micropolygons ≤ the seam threshold), not by `CRenderer::maxEyeSplits`
  - at the safety depth cap, **keep** (draw) the patch, never drop it
  - increment `numCsgSeamSplits`
- [ ] T044 [US1] Fill the coverage mask in `CReyes::drawGrid()` (`src/ri/hiders/reyes.cpp:695`): after the existing P-only `shadeGrid(nGrid, TRUE)`, if `object->region` is set, evaluate `accepts()` per vertex and mark each quad absent iff none of its four vertices is accepted. The code uses only the generic names; the quickstart §7 grep gate must still return no matches.
- [ ] T045 [US1] **REYES / Z-buffer exit check**: all `CsgSeam_hero_*`, `CsgSeam_spherecube_*`, `CsgSeam_coincident_*`, `CsgPerOperandShader_hero_*` and `CsgSeam_freeform_*` tests are green. Tune the seam threshold in `surface.cpp` if needed. Save 4×-zoom crops of each seam to the scratchpad and inspect them.
- [ ] T046 [US1] Implement implied-closure caps in `src/ri/geometry/csgCaps.{h,cpp}` (research D7, FR-021). For single-quadric operands, synthesize planar `CBilinearPatch` caps that generously cover each open cross-section: end circles at z-min/z-max or height ends, z-cut circles, and the two sweep-gap half-planes. Caps carry the operand's attributes and transform. Each cap's region is "inside the quadric's cross-section" ∧ the boundary rule. Wire them into `CCSGInstanceState` in `src/ri/geometry/csgSolid.cpp` (`caps[op][]`, plus `children` for raytrace and `dice()` for REYES). Make T035 green. Then register the cap visual tests `csg-open-quadric-caps-{raytrace,reyes,zbuffer}` in `tests/visual/CMakeLists.txt` (`add_visual_test`). Generate their references only after visually confirming the caps are correct and seamless (4× crops), and record that approval in `specs/021-deferred-csg-evaluation/reference-regeneration.md`.
- [ ] T047 [US1] Deliberately regenerate the references for the existing CSG visual scenes whose seams improved (`csg-sphere-cube-*`, `csg-sphere-sphere-*-tolerance-raytrace`, `csg-subdivision-sphere-union-raytrace`, `csg-nurbs-sphere-union-raytrace`, `csg-nested-tree-4-level-raytrace`, `csg-object-instance-reuse-raytrace`, `csg-sphere-sphere-difference-interior-raytrace-inside`, `blobby-csg-difference-{reyes,zbuffer}`), one scene at a time. For each, save before/after 4× seam crops and record the comparison and the reason in `specs/021-deferred-csg-evaluation/reference-regeneration.md`. FR-007a for the free-form scenes is already enforced automatically by T034. This ends the T037 expected-red window.

**Checkpoint (Phase 3, MVP)**: User Story 1 is fully functional. The hero scene shows smooth seams with no cracks in every hider, each face carries its operand's shader, and every existing CSG visual test is green against its deliberately regenerated reference. Stage and pause for commit.

---

## Phase 4: User Story 2 — The same solid looks the same in every visibility method (Priority: P2)

**Goal**: raytrace, REYES and Z-buffer images agree within the existing parity thresholds, including for secondary rays, displaced operands and translucent operands (FR-013, FR-015, SC-004, SC-008).

**Independent Test**: `ctest -L parity -R csg` is green (quickstart.md §2).

### Tests for User Story 2 ⚠️

- [ ] T048 [US2] Add cross-hider parity registrations in `tests/visual/CMakeLists.txt` (label `parity`, thresholds from `tests/visual/parity-thresholds.md`) for:
  - the hero (800×500), the sphere-cube difference, the coincident union/difference and the open-quadric-caps scenes
  - the displaced-operand pair `csg-hero-displaced-{raytrace,reyes}` and the translucent-operand pair `csg-hero-translucent-{raytrace,reyes}` (T011), each also requiring 0 see-through gaps via `test_csg_seam_metric`

  Displacement on an operand also exercises the displaced-leaf handling in T040.
- [ ] T049 [US2] Register `csg-secondary-rays-raytrace` and `csg-secondary-rays-reyes` (T009) in `tests/visual/CMakeLists.txt`, plus a seam-metric check that the shadow shows a clear through-hole and the reflection shows smooth seams, in both hiders (FR-013).
- [ ] T050 [US2] Verification test: register `CsgUntouchedRegion_hero` in `tests/visual/CMakeLists.txt`. Render the hero scene and the same scene with a plain `Sphere`, no solid block (add `examples/rib/tests/csg-hero-plain-sphere-raytrace.rib`). Compare them with `test_csg_seam_metric --region-compare` outside the hole's annulus (SC-008).

### Implementation for User Story 2

- [ ] T051 [US2] Investigate and fix every divergence surfaced by T048–T050, inside the geometry domain only: for example, displaced-leaf undisplaced evaluation in `src/ri/geometry/surface.cpp`, seam-threshold or ε calibration in `src/ri/geometry/csgClassify.cpp`, cap orientation in `src/ri/geometry/csgCaps.cpp`. No hider or shading edits beyond T042/T044. Re-run until green.

**Checkpoint (Phase 4)**: all hiders agree. Stage and pause for commit.

---

## Phase 5: User Story 3 — Everything solids could already do keeps working (Priority: P2)

**Goal**: spec 013 behavior is preserved (nesting, per-operand shaders, Interior/Exterior, instancing, diagnostics, empty and single-operand blocks). Procedural operands are newly supported (FR-024/FR-025). The tolerance attribute is inert (FR-023).

**Independent Test**: `ctest -R "CSG|Csg|csg"` is green (quickstart.md §3 and §5).

### Tests for User Story 3 (write first, confirm they FAIL) ⚠️

- [ ] T052 [P] [US3] Update `tests/unit/csg/test_interior_exterior_selection.cpp` to assert that `ATTRIBUTES_FLAGS_SOLID_FRAGMENT` is set on each leaf's cloned attributes, not on fragment meshes, and that Interior/Exterior selection at `shading.cpp:1019` behaves as before (spec 013 FR-009 to FR-012, FR-020).
- [ ] T053 [P] [US3] Create `tests/unit/csg/test_procedural_operand.cpp`. Assert:
  - a `Procedural` inside `SolidBegin "primitive"` is captured without error
  - it is not expanded at `SolidEnd`
  - it is expanded once, on the solid instance's first use, into that operand
  - nested procedurals expand recursively
  - a `SolidBegin` emitted during expansion reports the nested-in-primitive diagnostic and is ignored
  - an empty expansion follows FR-025 (an empty first operand of difference/intersection ⇒ nothing; otherwise no change)
  - `numCsgProceduralExpansions` counts correctly

  Register it in `tests/unit/csg/CMakeLists.txt`.
- [ ] T054 [US3] Rename `examples/rib/tests/csg-malformed-procedural-in-primitive.rib` to `examples/rib/tests/csg-procedural-missing-archive.rib` and rewrite its header comment for the new behavior: the procedural (still pointing at `nonexistent.rib`) is **accepted at capture** (FR-024); expansion reports exactly one archive-open diagnostic; the operand is empty under FR-025; the render completes. In `tests/visual/CMakeLists.txt`, move its registration out of the malformed-diagnostic group and register it with a `PASS_REGULAR_EXPRESSION` on the archive diagnostic and a `FAIL_REGULAR_EXPRESSION` on the old "cannot be declared inside a SolidBegin block" text.
- [ ] T055 [US3] Register `csg-procedural-operand-raytrace` in `tests/visual/CMakeLists.txt` against its inline twin (SC-010), comparing the two renders with `test_csg_seam_metric --region-compare` over the whole frame. Add a configure-time `file(COPY ${CMAKE_SOURCE_DIR}/examples/rib/tests/archives DESTINATION <the test's scratch dir>)` so the relative `archives/csg-sphere-operand.rib` path resolves under `WORKING_DIRECTORY` (T008 path note).
- [ ] T056 [US3] Register `csg-tolerance-inert-raytrace` (T009) in `tests/visual/CMakeLists.txt`. Assert that stderr contains exactly one tessellation-tolerance notice (CTest `PASS_REGULAR_EXPRESSION`/`FAIL_REGULAR_EXPRESSION`), and that the image matches the hero raytrace render (`--region-compare`).

### Implementation for User Story 3

- [ ] T057 [US3] Support procedural operands (research D8):
  - remove the procedural rejection (`csgValidateProceduralCapture()` at `src/ri/geometry/csgTree.cpp:90`, its call site and the `delayed` gate inside an open solid block in `CRendererContext::addObject()`, `src/ri/render/rendererContext.cpp`)
  - add an operand-capture mode to `CRendererContext` (`rendererContext.{h,cpp}`) that diverts `addObject()` into a given operand's member list
  - in `src/ri/geometry/csgSolid.cpp`, expand procedural members during `CCSGInstanceState` resolution under `delayedMutex`, reusing `processDelayedObject()` with recursion for nested procedurals
  - route a `SolidBegin` during expansion to `csgValidateNestedSolidBegin()`
  - implement empty-operand semantics in `csgDefinition.cpp`/`csgClassify.cpp`
  - increment `numCsgProceduralExpansions`

  Make T053–T055 green.
- [ ] T058 [US3] Make `tessellationtolerance` inert (research D10):
  - remove every remaining read of `CAttributes::tessellationTolerance`
  - keep parsing in all four attribute layers
  - in `RiAttributeV()` (`src/ri/render/rendererContext.cpp`), emit the notice on the first occurrence per render, using a per-render flag reset at frame begin and a level that `CODE_LOG` does not swallow
  - update the field comment in `src/ri/state/attributes.h:165` to say it is inert

  Make T056 green.
- [ ] T059 [US3] Verify `csg-object-instance-reuse-raytrace` (every instance resolves its own `CCSGInstanceState` over the shared `CCSGDefinition`) and `csg-nested-tree-4-level-raytrace`; fix `src/ri/geometry/csgSolid.cpp` as needed.
- [ ] T060 [US3] Verification: re-run every existing CSG visual scene (the T047 list) and the T030–T034 seam tests after the US3 changes; all remain green with no reference changes.
- [ ] T061 [US3] Verify the malformed-block diagnostics are unchanged (`csg-malformed-bogus-operation`, `csg-malformed-nested-in-primitive`, `csg-malformed-unmatched-solidend`), along with empty-block and single-operand-block behavior.

**Checkpoint (Phase 5)**: `ctest -R "CSG|Csg|csg"` is green. Stage and pause for commit.

---

## Phase 6: User Story 4 — Solids only cost work where they are actually seen (Priority: P3)

**Goal**: nothing resolves, classifies or expands for solids or parts of solids that never contribute (FR-001 to FR-004, SC-006).

**Independent Test**: the off-screen statistics are zero, and the partial-view statistics scale down (quickstart.md §4).

### Tests for User Story 4 (write first, confirm they FAIL) ⚠️

- [ ] T062 [P] [US4] Create `tests/unit/csg/test_lazy_resolution.cpp`. Assert:
  - after `SolidEnd` only the `CCSGDefinition` exists: no `CCSGInstanceState`, no leaf instantiation, no classification
  - the first `dice()`/`intersect()` resolves exactly once
  - a never-needed solid's procedural operands are never expanded

  Register it in `tests/unit/csg/CMakeLists.txt`.
- [ ] T063 [US4] Register `CsgOffscreenStats` in `tests/visual/CMakeLists.txt`. Render `csg-offscreen-stats.rib` (T009) and match the end-of-frame statistics: `numCsgInstancesResolved`, `numCsgClassifications` and `numCsgProceduralExpansions` are all 0 for the off-screen solid (`PASS_REGULAR_EXPRESSION`).
- [ ] T064 [US4] Register `CsgPartialVisibilityStats` in `tests/visual/CMakeLists.txt`, driven by a new script `tests/visual/csg_partial_stats.cmake` run as `cmake -P`. The script renders `csg-hero-raytrace-800.rib` (full view) and `csg-hero-partial-stats.rib` (left half only), extracts `numCsgSeamSplits` from each statistics output, and fails unless `partial ≤ 0.75 × full` (US4 scenario 2, FR-004 "part of a solid").

### Implementation for User Story 4

- [ ] T065 [US4] Remove any eager work found by T062–T064 in `src/ri/geometry/csgSolid.cpp`, `src/ri/geometry/csgDefinition.cpp` and `src/ri/geometry/csgTree.cpp`. For example, compute the solid's bound from the definition without instantiating leaves, and never touch `CCSGInstanceState` from frustum culling or bound queries.

**Checkpoint (Phase 6)**: laziness is proven. Stage and pause for commit.

---

## Phase 7: Polish & Cross-Cutting Concerns

- [ ] T066 Delete `src/ri/geometry/csgBoolean.{h,cpp}`, `src/ri/geometry/solidObject.{h,cpp}` and `CRendererContext::processDelayedSolid` (`src/ri/render/rendererContext.{h,cpp}`). Remove them from both source lists in `src/ri/CMakeLists.txt` (~L62, ~L126, ~L344, ~L369), along with the related libribpreview comment text (~L295–300).
- [ ] T067 For each spec 013 tessellation helper (`tesselateQuadricAdaptive`, `tesselatePatchMeshAdaptive`, `tesselateNURBSPatchMeshAdaptive`, `csgTessellatePolygonMeshOperand`, `tessellateToSurfaces`), grep for remaining non-CSG callers. Delete every helper left unused. Retire the matching suites (`tests/unit/csg/test_quadric_tesselation.cpp`, `test_patchmesh_tesselation.cpp`, `test_tesselation_flatness.cpp`) and their `tests/unit/csg/CMakeLists.txt` registrations; keep any whose helper survives.
- [ ] T068 [P] Update `docs/site/content/manual/reference/solid-csg-operations.md` (Hugo, constitution VII): deferred evaluation, seam quality, open single-quadric operands and implied caps, multi-surface operands closed by the author, **overlapping shapes in one "primitive" block combine by parity** (use `"union"` to merge), procedural operands, the inert tolerance attribute with its notice, and the new statistics counters (SC-009).
- [ ] T069 [P] Update `DEVNOTES.md` (status table entry for spec 021, superseding 013's resolution) and `DEVNOTES_DETAILS/RISPEC_GAPS.md` (CSG section).
- [ ] T070 [P] Update the `CLAUDE.md` "Known gotchas":
  - CSG is spatial trimming via the generic region hook
  - the hook must keep generic names because `reyes.cpp` references it
  - classification queries bypass ray filters and use orientation-independent parity
  - trims still drop straddlers, while CSG keeps them

  Also add a superseded-by-021 note under Decisions 1, 3 and 4 of `specs/013-solid-csg-operations/research.md`.
- [ ] T071 Performance (SC-007): time every scene listed in `specs/021-deferred-csg-evaluation/baseline-timings.md` on the same machine and build, throttled. Record the new times and ratios in the same file; every ratio must be under 10×.
- [ ] T072 Full regression: run `ctest --test-dir build -L visual` sequentially, then a full `ctest`. Classify every failure against quickstart.md §3's known-flaky / pre-existing list, and re-run flaky failures in isolation before attributing them.
- [ ] T073 Run every quickstart.md check (§1–§7) end to end and record the results in `specs/021-deferred-csg-evaluation/validation.md`.
- [ ] T074 Linux validation (constitution VI): hand the branch to the maintainer's Linux validation path (Maestri / Ubuntu 24.04) and confirm the build is warning-free and `ctest -L csg` plus the CSG visual and parity tests pass there.

**Checkpoint (Phase 7)**: feature complete. Stage and pause for commit.

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: no dependencies.
- **Foundational (Phase 2)**: depends on Phase 1 (T001 stubs; the T002/T003 tool is needed only by later visual tests). **Blocks every user story.**
- **US1 (Phase 3)**: depends on Phase 2. It introduces `CCSGSolid` (T036), the switch-over (T037) and the coverage mask (T042), which every later story builds on.
- **US2 (Phase 4)**: depends on US1 (it needs a working solid in all hiders).
- **US3 (Phase 5)**: depends on US1. It can run in parallel with US2, but both touch `tests/visual/CMakeLists.txt`, so serialize those registration tasks.
- **US4 (Phase 6)**: depends on US1. T062/T063 also exercise procedural operands, so run it after T057 (US3).
- **Polish (Phase 7)**: depends on all stories.

### Within Phases

- Tests are written first and must FAIL before implementation (constitution III). Exceptions are verification tasks (T050, T060), which check behavior already delivered.
- T022 comes before T023–T025 (the virtual must exist) and before T027 (`CCSGRegion` implements the interface).
- T026 comes before T027. T027 makes T013–T017, T019 and T020 green. T028 makes T018 green.
- US1 order: T030–T035 (red) → T036 → T037 → (T038, T039, T040) → T041 → T042 → T043 → T044 → T045 → T046 → T047.
- The window from T037 to T047 is expected red for the existing CSG visual tests (old references).
- Any two tasks that edit `tests/visual/CMakeLists.txt` or `tests/unit/csg/CMakeLists.txt` run sequentially.

### Parallel Opportunities

- Phase 1: T002 and T004–T011 (different files).
- Phase 2 tests: T013–T020 (different files); implementation: T023–T025, plus T028 alongside T026/T027.
- US1: T039 alongside T038/T040 (different files); T035 alongside T030–T034 (different CMakeLists).
- US3: T052 and T053.
- Polish: T068–T070.

---

## Parallel Example: Phase 2 tests

```bash
Task: "Create tests/unit/csg/test_count_crossings.cpp (T013)"
Task: "Create tests/unit/csg/test_classify_quadrics.cpp (T014)"
Task: "Create tests/unit/csg/test_parity_robustness.cpp (T015)"
Task: "Create tests/unit/csg/test_boundary_rule.cpp (T016)"
Task: "Create tests/unit/csg/test_classify_determinism.cpp (T017)"
Task: "Create tests/unit/csg/test_csg_stats.cpp (T018)"
```

## Parallel Example: User Story 1

```bash
Task: "Region check at hit acceptance in src/ri/geometry/polygons.cpp (T039)"
Task: "Create tests/unit/csg/test_implied_caps.cpp (T035)"
```

---

## Implementation Strategy

### MVP First (User Story 1)

1. Phase 1 (harness and baseline) → Phase 2 (geometry core; no behavior change).
2. Phase 3 (US1): `CCSGSolid` and switch-over, then the raytrace path (T041 gate), then the coverage mask and REYES/Z-buffer path (T045 gate), then caps, then deliberate reference regeneration.
3. **Stop and validate**: the hero scene is crack-free and smooth in every hider, and every existing CSG test is green. This is the MVP.

### Incremental Delivery

US1 (seams) → US2 (parity, secondary rays, displaced and translucent operands) → US3 (preservation, procedurals, tolerance attribute) → US4 (laziness proof) → Polish (retire the BSP path, docs, performance, Linux). Each checkpoint is a review-and-commit pause.

---

## Notes

- [P] means different files and no dependency on an incomplete task.
- Never regenerate a reference image to make a test pass. Regenerate only after visual approval, recorded in `reference-regeneration.md` (T046, T047).
- Do not misattribute known flaky or pre-existing failures (quickstart.md §3).
- Keep hiders and shading free of CSG identifiers (contract §0; quickstart §7 grep gate).
