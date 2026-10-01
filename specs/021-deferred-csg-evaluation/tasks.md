---

description: "Task list for spec 021 — deferred, on-demand CSG evaluation"
---

# Tasks: Deferred, On-Demand Evaluation of Solid (CSG) Geometry

**Input**: Design documents from `/specs/021-deferred-csg-evaluation/`

**Prerequisites**: [plan.md](plan.md), [spec.md](spec.md), [research.md](research.md), [data-model.md](data-model.md), [contracts/](contracts/), [quickstart.md](quickstart.md)

**Tests**: REQUIRED. Constitution Principle III (TDD) is non-negotiable: every new capability gets a failing test before it is implemented. Tasks marked **(verification)** or **(guard)** exercise behavior that already exists or was delivered earlier, so they may pass as soon as they are written. They are listed in Dependencies → Within Phases.

**Organization**: Tasks are grouped by user story. User Story 1 depends on the Foundational phase; Stories 2–4 build on the CSG solid that User Story 1 introduces (see Dependencies).

**Workflow**:
- All work happens in the worktree `../openrender-worktrees/021-deferred-csg-evaluation`. The main checkout stays on `master`.
- At every **Checkpoint**, stage the phase's files and stop for the maintainer to review and commit. Never commit.
- Run builds, renders and ctest under `nice -n 15` with `taskpolicy -b` on macOS.
- Run the visual suite sequentially or with low `-j`, to avoid throttling timeouts.

**Class and hook names** (data-model.md):
- `CCSGDefinition`: shared, immutable template tree
- `CCSGSolid`: the scene `CObject`
- `CCSGInstanceState`: per-instance state
- `CCSGRegion`: implements the generic `CRegionPredicate`
- generic names the hider may reference (contract §0): `CRegionPredicate`, `CObject::region`, `CObject::materialize()`, `CSurface::countCrossings()`, `CRasterGrid::coverage`

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel: different files, no dependency on incomplete tasks.
- **[Story]**: US1–US4, mapping to the spec's user stories.

## Path Conventions

Single project at the repository root: `src/ri/...`, `tests/...`, `examples/rib/tests/...`.

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: build wiring, measurement harness, test scenes, diagnostic guards and the baseline, all before any behavior change.

- [ ] T001 Create compilable stub pairs (file header block in the existing style; empty declarations only):
  - `src/ri/geometry/csgDefinition.{h,cpp}`, `src/ri/geometry/csgSolid.{h,cpp}`, `src/ri/geometry/csgClassify.{h,cpp}`, `src/ri/geometry/csgCaps.{h,cpp}`
  - the renderer-only `src/ri/geometry/surfaceCrossingDispatch.{h,cpp}` and its stub `src/ri/vector/surfaceCrossingDispatchStub.cpp`, mirroring the existing `csgTesselationDispatch.cpp` / `vector/csgTesselationDispatchStub.cpp` split

  Add the five geometry `.cpp` files to the ribRender source list in `src/ri/CMakeLists.txt` next to `geometry/csgTree.cpp` (~L64). Add them to the ribVector source-exclusion list (consumed by libribpreview) beside `geometry/csgTree.cpp`/`geometry/solidObject.cpp` (~L344/L369), extending the comment at ~L295. Add the stub to the ribVector list next to `vector/csgTesselationDispatchStub.cpp` (~L373). Confirm `cmake --build build` stays warning-free (`-Werror`).
- [ ] T002 [P] Create the seam-metric CLI `tests/visual/test_csg_seam_metric.cpp`, modeled on `tests/visual/test_radial_histogram.cpp`. It reads RGBA TIFFs via libtiff, prints one CSV summary line per check, exits PASS/FAIL, and supports `--help` describing every mode and option (constitution IV).

  **Region options** (every mode): `--include <shape>` and `--exclude <shape>`, where shape is `disc:cx,cy,r`, `annulus:cx,cy,r0,r1`, `rect:x0,y0,x1,y1`, `frame`, or `mask:<png>`; and `--margin <px>` (default 2), which shrinks every region boundary to absorb pixel-filter falloff.

  **Modes**:
  - **(a) `--annuli`**: "must be clear" regions (alpha < 0.01), "must be solid" regions (alpha ≥ 0.99), and seam radii with a pixel tolerance. Report stray-pixel counts and the maximum radial deviation of the seam transition from each analytic radius.
  - **(b) `--edge-compare <candidate> <reference> --tolerance <px>`**: maximum distance between corresponding seam/silhouette edges of two renders.
  - **(c) `--region-compare <a> <b>`**: block-average diff within the region.
  - **(d) `--region-chroma <image> --rgb-share <r> <g> <b> --tolerance <t>`**: mean chromaticity (each channel ÷ (R+G+B)) within the region must match the expected share within tolerance. Also accepts per-channel `--min-share c=v` / `--max-share c=v`.
  - **(e) `--projected-curve <image> --camera <fov> <w> <h> <4x4 world-to-camera> --circle <cx,cy,cz,nx,ny,nz,r>…`**: sample each exact 3D intersection circle, project it through the camera, and require the detected seam edge (alpha or luminance discontinuity) within `--tolerance` px of every visible projected sample.
  - **(f) `--enclosed-holes <image> [--opaque <alpha>]`**: count non-opaque pixels (alpha < `--opaque`, default 0.99) not connected to the image border by a flood fill. These are see-through gaps, with legitimate openings removed via `--exclude`.
- [ ] T003 Register the `test_csg_seam_metric` executable in `tests/visual/CMakeLists.txt`, next to `test_radial_histogram` (~L40), linking `TIFF::TIFF` with cxx_std_20.
- [ ] T004 Create the render-and-measure harness: the `cmake -P` driver `tests/visual/csg_metric_test.cmake` and the macro `add_csg_metric_test(NAME <n> RENDERS <rib>[:<out.tif>]… [METRIC <args…>]… [LOG_PASS <regex>]… [LOG_FAIL <regex>]… [STATS <label> <op> <value|other-render>]… [COPY <dir>]… [LABELS …])` in `tests/visual/CMakeLists.txt`. The driver:
  - copies each `COPY` directory into the per-test scratch working directory
  - renders every RIB with the same environment `add_visual_test` uses, failing on any render error
  - applies `LOG_PASS`/`LOG_FAIL` to the combined stderr itself, so CTest's `PASS_REGULAR_EXPRESSION` never overrides the exit code
  - runs every `METRIC` invocation of `test_csg_seam_metric` and propagates its exit code
  - extracts end-of-frame statistics by label for `STATS` comparisons (`==`, `<=`, `>`, ratio-to-other-render)
- [ ] T005 [P] Create the six hero scenes `examples/rib/tests/csg-hero-{raytrace,reyes,zbuffer}-{800,1600}.rib`:
  - sphere `1.50 -1.50 1.50 360` minus `Cylinder 0.65 -2 2 360`
  - `Translate 0 0 9`, `fov 38`, `raydistant` light, `plastic` surface
  - Formats 800×500 and 1600×1000, `Display` RGBA
  - A header comment documenting the analytic radii. At 800×500: clear through-hole disc r=45.6 px, near seam ring r=61.7 px, sphere silhouette r=122.7 px. Scale ×2 at 1600×1000.
- [ ] T006 [P] Create the six sphere-minus-cube scenes `examples/rib/tests/csg-sphere-cube-difference-{raytrace,reyes,zbuffer}-{800,1600}.rib` at **800×500 and 1600×1000** (matching spec SC-002/SC-003), copying the geometry and camera of `examples/rib/tests/csg-sphere-cube-difference-reyes.rib`. Record in each header the world-to-camera matrix and the exact intersection circles (sphere ∩ each cube face plane) used by `--projected-curve`.
- [ ] T007 [P] Create the coincident-face scenes `examples/rib/tests/csg-coincident-{union,difference}-{raytrace,reyes,zbuffer}.rib`:
  - two unit boxes (six bilinear patches each) sharing one face for the union
  - a box minus a box flush with one face for the difference
  - a camera angle that exposes the shared or flush face
- [ ] T008 [P] Create the implied-cap scenes `examples/rib/tests/csg-open-quadric-caps-{raytrace,reyes,zbuffer}.rib`: a sphere r=1 intersected with `Cylinder 0.6 -0.5 0.5 180` (an open half-sweep, a single-quadric operand), oriented so that both flat sweep-gap caps and one end cap lie on the composite boundary and face the camera.
- [ ] T009 [P] Create the procedural-operand scenes:
  - archives: `examples/rib/tests/archives/csg-sphere-operand.rib` (contains `Sphere 1.50 -1.50 1.50 360`) and `examples/rib/tests/archives/csg-open-cylinder-operand.rib` (contains `Cylinder 0.65 -2 2 360`)
  - `examples/rib/tests/csg-procedural-operand-raytrace.rib`: hero geometry, with the sphere supplied by `Procedural "DelayedReadArchive" ["archives/csg-sphere-operand.rib"] [-1.5 1.5 -1.5 1.5 -1.5 1.5]` inside `SolidBegin "primitive"`
  - `examples/rib/tests/csg-procedural-open-cylinder-raytrace.rib`: hero geometry, with the open cylinder supplied by `archives/csg-open-cylinder-operand.rib`
  - inline twins `examples/rib/tests/csg-procedural-operand-inline-raytrace.rib` and `examples/rib/tests/csg-procedural-open-cylinder-inline-raytrace.rib`

  **Path note**: visual tests run with `WORKING_DIRECTORY` set to a per-test scratch directory (`tests/visual/CMakeLists.txt:171`). Registrations of these scenes therefore pass `COPY examples/rib/tests/archives` to `add_csg_metric_test` (T004).
- [ ] T010 [P] Create the remaining scenes:
  - `examples/rib/tests/csg-offscreen-stats.rib`: one hero solid placed behind the camera, its sphere operand supplied by `Procedural "DelayedReadArchive" ["archives/csg-sphere-operand.rib"]`, plus a plain visible sphere; `Option "statistics" "endofframe" [2]`.
  - `examples/rib/tests/csg-tolerance-inert-raytrace.rib`: the hero scene with `Attribute "solid" "float tessellationtolerance" [0.01]` set three times on different operands.
  - `examples/rib/tests/csg-secondary-rays-{raytrace,reyes}.rib`: the hero solid over a ground plane, lit so its shadow shows the through-hole, with a mirror plane reflecting it. The REYES variant uses shader-traced shadows and reflections.
  - `examples/rib/tests/csg-hero-partial-stats.rib`: the hero solid framed so `ScreenWindow` shows only its left half; `Option "statistics" "endofframe" [2]`.
  - `examples/rib/tests/csg-hero-stats-{800,1600}-reyes.rib`: the REYES hero at both resolutions with `Option "statistics" "endofframe" [2]`.
  - `examples/rib/tests/csg-tiny-solid-stats.rib`: the hero solid scaled to cover under 4×4 pixels; `Option "statistics" "endofframe" [2]`.
  - `examples/rib/tests/csg-polymesh-box-difference-raytrace.rib`: a `PointsPolygons` cube minus a sphere.
  - REYES twins `examples/rib/tests/csg-nurbs-sphere-union-reyes.rib` and `examples/rib/tests/csg-subdivision-sphere-union-reyes.rib` of the existing raytrace scenes, plus `examples/rib/tests/csg-{nurbs,subdivision}-sphere-alone-raytrace.rib` (the free-form operand alone, same camera).
- [ ] T011 [P] Create `examples/rib/tests/csg-hero-two-shaders-{raytrace,reyes}.rib`: the hero scene with `Surface "plastic"` + gold `Color [1 0.8 0.3]` on the sphere operand and `Surface "matte"` + blue `Color [0.2 0.35 1]` on the cylinder operand (FR-016).
- [ ] T012 [P] Create the edge-case scenes:
  - `examples/rib/tests/csg-hero-displaced-{raytrace,reyes}.rib`: the hero scene with `Displacement "dented"` on the sphere operand and a matching `Attribute "displacementbound"`.
  - `examples/rib/tests/csg-hero-translucent-{raytrace,reyes}.rib`: the hero scene with the sphere operand at `Opacity [0.5 0.5 0.5]`.
- [ ] T013 Register diagnostic guards for the four malformed scenes `examples/rib/tests/csg-malformed-{bogus-operation,nested-in-primitive,unmatched-solidend,procedural-in-primitive}.rib` via `add_csg_metric_test`. Use `LOG_PASS` regexes for **today's** diagnostics, including the current procedural rejection, and require the renderer not to crash. These are guards: green on master, so later changes can't silently alter diagnostics (FR-019).
- [ ] T014 Record master baselines in `specs/021-deferred-csg-evaluation/baseline-timings.md`:
  - throttled wall-clock render time for every existing `examples/rib/tests/csg-*.rib` visual scene, the `blobby-csg-*` scenes and the T005/T006 scenes
  - the T002 tool's metric output on master for T005/T006/T007 (the expected-red baseline; the hero scene should reproduce the 213 / 63 / 38 stray pixels at 1600×1000)

**Checkpoint (Phase 1)**: harness, scenes and diagnostic guards exist; baseline recorded; build warning-free; no behavior change. Stage and pause for commit.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: the geometry-domain core, the generic region hook with inheritance, mesh materialization, crossing queries and statistics counters. **No rendering behavior changes in this phase.** Every existing visual test must still pass.

**⚠️ CRITICAL**: No user-story work can begin until this phase is complete.

### Tests for Phase 2 (write first, confirm they FAIL) ⚠️

- [ ] T015 [P] Create `tests/unit/csg/test_count_crossings.cpp`. With a NULL context, assert the **unsigned** crossing counts (undisplaced surface; ignoring visibility flags, LOD, backface, displacement and orientation attributes) for rays with known answers against:
  - `CSphere`, `CDisk`, `CCone`, `CParaboloid`, `CCylinder`, `CHyperboloid`, `CToroid` (including partial `thetamax` and z-cuts)
  - `CPolygonTriangle`, `CPolygonQuad`, `CBilinearPatch`

  Also test the pure Newton-refinement core in `surfaceCrossingDispatch.h` with analytic evaluation callbacks: a bicubic patch representing a plane exactly, and a rational NURBS sphere. Crossing parameters must converge to the analytic intersection within 1e-5 (FR-007a).
- [ ] T016 [P] Create `tests/unit/csg/test_classify_quadrics.cpp`. Assert analytic implied-closed classification (IN/OUT) for single-quadric operands:
  - closed sphere and torus
  - cylinder capped at z-min/z-max
  - z-cut sphere
  - half-sweep cylinder (sweep-gap planes)
  - cone, paraboloid, hyperboloid
  - a lone `CDisk`, which is an empty operand: silently, always OUT

  Use points on both sides of every implied cap.
- [ ] T017 [P] Create `tests/unit/csg/test_parity_robustness.cpp`. Assert parity classification (odd crossings ⇒ IN) for multi-surface operands built from directly constructible surfaces:
  - a six-`CBilinearPatch` box
  - the same box with deliberately mixed vertex orders
  - the same box with one patch under `ReverseOrientation`
  - a `CCylinder` plus two `CDisk`s (author-closed; no doubled caps)
  - two overlapping spheres in one operand (the overlap classifies OUT)

  Include grazing, edge and vertex rays; the fallback direction plus majority-of-three must still classify correctly. Also assert the FR-021 closedness rules with the error handler captured:
  - a single closed rational-NURBS-sphere patch ⇒ no diagnostic
  - a single open NURBS sheet ⇒ exactly one "operand is not closed" diagnostic and an empty operand
  - a single `CBilinearPatch` ⇒ the same diagnostic and an empty operand
  - a lone `CDisk` ⇒ silent and empty
- [ ] T018 [P] Create `tests/unit/csg/test_boundary_rule.cpp`. For union, intersection and difference (2 and 3 operands), the 4-level nested tree, single-operand and empty collapse, and **direct leaves in a boolean block** (one first operand, parity-combined), assert:
  - the boundary rule at points on each leaf
  - static orientation parity
  - short-circuit evaluation (a counter shows irrelevant operands are not classified)
  - `classifyBound()` `ALL_IN`/`ALL_OUT`/`MIXED`
  - **analytic seam location**: for the sphere-cube difference, sample 64 points on each exact intersection circle; at ±δ (δ = 1e-4·r) along the sphere's surface, perpendicular to the curve, the predicate must flip, with the matching face-side check. Do the same for the hero's two circles at z = ±√(1.5² − 0.65²).
  - **tangency**: two spheres touching at a point, and one sphere internally tangent to another; neither produces boundary points away from the contact.
- [ ] T019 [P] Create `tests/unit/csg/test_classify_determinism.cpp`. Classify the same point set over analytic, polygon and bilinear operands in forward order, in shuffled order, and from 8 `std::thread`s concurrently; assert bit-identical results (FR-008).
- [ ] T020 [P] Create `tests/unit/csg/test_csg_stats.cpp`. Assert that `CStats` exposes `numCsgClassifications`, `numCsgSeamSplits`, `numCsgInstancesResolved` and `numCsgProceduralExpansions`, zero-initialized per frame; that the end-of-frame statistics at level ≥ 2 print them, zeros included, whenever the frame contains a solid, with the exact labels `CSG classifications:`, `CSG seam splits:`, `CSG instances resolved:` and `CSG procedural expansions:`; and that one `classify()` call increments `numCsgClassifications` by exactly 1.
- [ ] T021 [P] Create `tests/unit/csg/test_region_inheritance.cpp`. Assert that `CObject::setChildren()` copies the parent's `region` to every child that has none, does not overwrite a child's own `region`, and leaves `region` null throughout when the parent has none.
- [ ] T022 [P] Rewrite `tests/unit/csg/test_boolean_boxes.cpp`, `test_boolean_sphere_box.cpp` and `test_boolean_sphere_sphere.cpp` as classification and boundary-rule tests over the same scenarios. Build `CCSGDefinition` directly, with no Ri capture interception.
- [ ] T023 [P] Rewrite the remaining kernel suites, building `CCSGDefinition` directly:
  - `tests/unit/csg/test_boolean_coplanar.cpp`: two-sided ±ε rule
  - `test_nested_tree.cpp`
  - `test_multi_primitive_leaf.cpp`: use two **disjoint** spheres, and assert that two coincident identical spheres in one block form an empty operand (the documented parity consequence)

  Update `tests/unit/csg/csgTestUtils.h`: remove `#include "csgBoolean.h"` and the `computeVolume`/volume-sign helpers; add analytic point-set builders.
- [ ] T024 In `tests/unit/csg/CMakeLists.txt`, register T015–T021 with labels `csg;unit`, keeping the rewritten suites' registrations. Build, run `ctest --test-dir build -L csg`, and confirm the new and rewritten tests FAIL (red) for the right reasons.

### Implementation for Phase 2

- [ ] T025 In `src/ri/geometry/object.{h,cpp}`:
  - add the generic `CRegionPredicate` interface (`accepts(CShadingContext *ctx, const float *P)`, `classifyBound(CShadingContext *ctx, const float *bmin, const float *bmax)` returning `ALL_IN`/`ALL_OUT`/`MIXED`)
  - add a nullable `CObject::region` member
  - propagate `region` in `CObject::setChildren()` (object.cpp:130) to children that have none
  - add `virtual void CObject::materialize(CShadingContext *)` (default no-op)
  - add `virtual int CSurface::countCrossings(CShadingContext *ctx, const float *from, const float *dir) const`, returning an unsigned count, default 0

  All names must be generic (contract §0). Make T021 green.
- [ ] T026 Override `materialize()` as `if (children == NULL) create(context);` in the five mesh types:
  - `CPolygonMesh` (`src/ri/geometry/polygons.{h,cpp}`)
  - `CPatchMesh` and `CNURBSPatchMesh` (`src/ri/geometry/patches.{h,cpp}`)
  - `CSubdivMesh` (`src/ri/geometry/subdivisionCreator.{h,cpp}`)
  - `CLoopSubdivMesh` (`src/ri/geometry/subdivisionLoop.{h,cpp}`)

  Each `create()` already calls `setChildren(context, …)` (polygons.cpp:1939, patches.cpp:1710/2203, subdivisionCreator.cpp:2256, subdivisionLoop.cpp:483) and is mutex-guarded and idempotent.
- [ ] T027 [P] Implement `countCrossings` in `src/ri/geometry/quadrics.{h,cpp}` for every quadric class, analytically, counting every root in the valid u/v range (unlike `CSphere::intersect()` at quadrics.cpp:167, which stops at the nearest).
- [ ] T028 [P] Implement `countCrossings` in `src/ri/geometry/polygons.{h,cpp}` for `CPolygonTriangle`/`CPolygonQuad` (exact). Sequence after T026, which edits the same files.
- [ ] T029 Implement free-form crossings:
  - in `src/ri/geometry/surfaceCrossingDispatch.{h,cpp}`: the pure Newton-refinement core over a surface-evaluation callback, plus a per-surface seed grid built from the surface's own evaluation, built once under a mutex and immutable afterwards, with no `CTesselationPatch` cache
  - in `src/ri/vector/surfaceCrossingDispatchStub.cpp`: a stub returning 0
  - `countCrossings` overrides that forward to it: `CBicubicPatch`/`CNURBSPatch` in `src/ri/geometry/patches.cpp` (`CBilinearPatch` is analytic, implemented directly there), `CPatchGrid` in `src/ri/geometry/patchgrid.cpp`, `CBSplinePatchGrid` in `src/ri/geometry/bsplinePatchgrid.cpp`, and `CSubdivision` in `src/ri/geometry/subdivision.cpp`

  Sequence after T026 (shared patches files).
- [ ] T030 Implement `CCSGDefinition` in `src/ri/geometry/csgDefinition.{h,cpp}`. It holds the **template** structure only: flattened nodes, operand template member lists (direct leaves of a boolean block form one first operand), the `leafFlip[]` parity, and the CSG-aware template bound. Include:
  - empty-tree and single-operand collapse (spec 013 FR-016/FR-017)
  - the FR-021 closedness helper: a quadric is closed iff it is a full sphere or full torus; other single surfaces and meshes are closed iff every parametric boundary edge collapses to a point or coincides with another boundary edge, or (polygon and subdivision meshes) every edge is shared by exactly two faces

  The operand *kind* and caps are derived per instance after expansion (T046, T066), not here.
- [ ] T031 Implement classification in `src/ri/geometry/csgClassify.{h,cpp}`:
  - operand `classify(ctx, P)` over an instance's resolved operand: bound rejection first; analytic implied-closed test for a single-quadric operand; an empty operand (lone disk, or a lone open non-quadric surface, which emits the one-time "operand is not closed" diagnostic at a level `CODE_LOG` does not swallow) is always OUT; otherwise deterministic fixed-direction parity over the operand's surfaces via `countCrossings`, with grazing/edge fallback and majority-of-three
  - the boundary rule with short-circuit evaluation
  - the two-sided ±ε ON rule (ε = `C_EPSILON` × solid bound size)
  - `CCSGRegion` implementing `CRegionPredicate`

  Make T015–T019, T022 and T023 green.
- [ ] T032 Add the four counters to `CStats` in `src/ri/core/stats.h`. Initialize them, and in `src/ri/core/stats.cpp` print them at level ≥ 2, zeros included, whenever the frame contains a solid, with the T020 labels. Increment `numCsgClassifications` in `src/ri/geometry/csgClassify.cpp` (sequence after T031; same file). Make T020 green.
- [ ] T033 Phase 2 gate:
  - `ctest -L csg` is green, including the T013 guards
  - the full `ctest -L visual` (sequential) is unchanged versus master, apart from the known flaky and pre-existing failures listed in quickstart.md §3
  - the quickstart §7 grep gate returns no matches

**Checkpoint (Phase 2)**: geometry core complete and tested; generic hooks in place; zero rendering change. Stage and pause for commit.

---

## Phase 3: User Story 1 — Boolean solids render with crack-free, smooth seams (Priority: P1) 🎯 MVP

**Goal**: the hero, sphere-minus-cube, coincident-face and free-form scenes render with no cracks, slivers, spikes or facets in all three hiders, at both resolutions. Each face keeps its operand's shader, and existing capabilities are preserved.

**Independent Test**: `add_csg_metric_test` checks on the T005–T012 scenes: 0 stray pixels, 0 enclosed holes, seams within 1 px of the projected exact curves, correct per-operand chromaticity (quickstart.md §1).

### Tests for User Story 1 (write first, confirm they FAIL unless marked guard) ⚠️

- [ ] T034 [US1] Register `CsgSeam_hero_{raytrace,reyes,zbuffer}_{800,1600}` via `add_csg_metric_test`:
  - `--annuli`: clear disc at the analytic radius, solid annulus, seam radii, tolerance 1 px, margin 2
  - `--enclosed-holes --exclude disc:<hole>`

  Labels `csg;visual`. Confirm red: they must reproduce the T014 baseline failures.
- [ ] T035 [US1] Register `CsgSeam_spherecube_{raytrace,reyes,zbuffer}_{800,1600}` for the T006 scenes: `--enclosed-holes` plus `--projected-curve` against the exact sphere ∩ cube-face circles from each scene header, tolerance 1 px, **in every hider, raytrace included**. Confirm red: the spurs exceed 1 px on master.
- [ ] T036 [US1] Register `CsgSeam_coincident_{union,difference}_{raytrace,reyes,zbuffer}` via `add_csg_metric_test`: `--enclosed-holes`, plus `--region-compare` against a hand-checked expected silhouette mask (no internal or leftover skin; FR-022). Confirm red.
- [ ] T037 [US1] (guard) Register `CsgPerOperandShader_hero_{raytrace,reyes}` for the T011 scenes with `--region-chroma`: in the hole-wall annulus (45.6–61.7 px at 800×500) blue share ≥ 0.5; in the outer annulus (62–122 px) red share ≥ 0.4 and blue share ≤ 0.2 (FR-016). This is a preservation guard; per-operand shaders already work on master.
- [ ] T038 [US1] (guard) Register `CsgSeam_freeform_{nurbs,subdiv}` and `CsgSeam_polymesh_box` via `add_csg_metric_test` (scenes from T010):
  - `--enclosed-holes`
  - REYES vs. raytrace `--edge-compare` ≤ 1 px
  - `--region-compare --exclude rect:<seam bbox>` of each free-form operand against its `*-alone-raytrace` render

  These guard the mesh and free-form paths across the switch-over; their red test is T015.
- [ ] T039 [US1] Register `CsgSeam_hero_displaced_raytrace` (T012 scene): `--enclosed-holes --exclude disc:<hole>`, and seam radii within 1 px of the undisplaced analytic radii. Confirm red. This is the failing test for T050's displaced-leaf handling.
- [ ] T040 [US1] Register `CsgSeamSplitCounter` via `add_csg_metric_test` `STATS`: `csg-hero-stats-800-reyes.rib` must report `CSG seam splits: > 0`, and a single-operand hero variant must report `= 0`. Confirm red: the counter is never incremented before T053.
- [ ] T041 [US1] Register `CsgInstance_seams` for `examples/rib/tests/csg-object-instance-reuse-raytrace.rib`: `--enclosed-holes`, and each instance's bounding rect has opaque coverage (`--include rect:` per instance). Confirm red. This is the failing test for `CCSGSolid::instantiate()`.
- [ ] T042 [P] [US1] Create `tests/unit/csg/test_implied_caps.cpp`. For each single-quadric kind (cylinder ends, z-cut sphere, half-sweep cylinder, cone base, paraboloid top, hyperboloid ends), assert the number, plane and orientation of the synthesized cap patches, and that each cap's predicate accepts exactly the cross-section inside the lateral surface. Register it in `tests/unit/csg/CMakeLists.txt`. Confirm red.
- [ ] T043 [P] [US1] Create `tests/unit/csg/test_capture_path.cpp`: `RiSolidBegin`…`RiSolidEnd` yields exactly one `addObject(CCSGSolid*)` whose definition holds the expected operands (including direct leaves of a boolean block as the first operand). Register it. Confirm red.
- [ ] T044 [P] [US1] Update `tests/unit/csg/test_interior_exterior_selection.cpp` to assert that `ATTRIBUTES_FLAGS_SOLID_FRAGMENT` and the orientation flip (`ATTRIBUTES_FLAGS_INSIDE` toggled per `leafFlip`) are set on each leaf's cloned attributes, not on fragment meshes, and that Interior/Exterior selection at `shading.cpp:1019` behaves as before (spec 013 FR-009 to FR-012, FR-020). Confirm red.
- [ ] T045 [P] [US1] Create `tests/unit/csg/test_lazy_resolution.cpp`: after `SolidEnd` only the `CCSGDefinition` exists (no `CCSGInstanceState`, no leaf instantiation or materialization, no classification), and the first `dice()`/`intersect()` resolves exactly once. Register it. Confirm red.

### Implementation for User Story 1

- [ ] T046 [US1] Implement `CCSGSolid` and `CCSGInstanceState` in `src/ri/geometry/csgSolid.{h,cpp}`:
  - a `CObject` that is not `OBJECT_DUMMY` (trace.cpp:332), bounded by the template bound under its transform
  - on the first `dice()`/`intersect()`, build `CCSGInstanceState` once under `CRenderer::delayedMutex`, mirroring `CDelayedObject::intersect()` (geometryDispatch.cpp:679):
    - instantiate each template leaf with the composed transform via its `instantiate()`
    - `materialize(context)` the mesh-type leaves and collect every `CSurface` descendant as that operand's classification surfaces
    - derive each operand's **kind** (single-quadric / multi-surface / empty / open) from the resolved members
    - clone each leaf's attributes, set `ATTRIBUTES_FLAGS_SOLID_FRAGMENT`, and toggle `ATTRIBUTES_FLAGS_INSIDE` per `leafFlip`
    - set each leaf's `region` to its `CCSGRegion` (mesh children inherit it via `setChildren`)
  - set `children` to all instance leaves (meshes, surfaces, and caps later), and make `dice()` call `CObject::dice(r)` (geometryDispatch.cpp:76), so every leaf is clipped and queued per bucket by `drawObject`
  - `instantiate()` creates a new `CCSGSolid` over the same definition with a composed transform
  - increment `numCsgInstancesResolved`

  Make T044 and T045 green.
- [ ] T047 [US1] Switch `RiSolidEnd` to the new path:
  - in `src/ri/geometry/csgTree.{h,cpp}`, replace `resolveCSGTree()` with a function that builds a `CCSGDefinition` and calls `context->addObject(new CCSGSolid(...))`
  - delete the now-unused static helpers (`csgResolveNode`, `csgToSoup`, `csgTessellateOperand`, `csgTriangulateGrid`, `csgMakeGridVertex`, `csgEmitTriangle`, `csgTriangleIsDegenerate`, `csgSignedVolume`, `csgFlipAll`, `csgPolygonsToFragments`, `csgBuildMeshForAttributeGroup`, `csgRebaseFragments`, `csgResolveTolerance`), the constant `kCsgDefaultToleranceFraction` (csgTree.cpp:47) and `#include "csgBoolean.h"`, to keep `-Werror` happy
  - update the call site in `src/ri/render/rendererContext.cpp` (~L5826)
  - leave `csgBoolean.*`, `solidObject.*` and `processDelayedSolid` in place but unreferenced; Phase 7 deletes them

  Make T043 green.

  **Expected-red note**: from this task until T057, the existing `csg-*`/`blobby-csg-*` reference-image tests are expected red (old BSP-era references). Never regenerate them to make a gate pass.
- [ ] T048 [US1] Raytrace hit acceptance for quadrics, in `src/ri/geometry/quadrics.cpp`: in every quadric `intersect()` root loop, before accepting a candidate, `continue` to the next root when `region && !region->accepts(context, P)`, where P is the undisplaced camera-space hit point. Leave `ray->t`/`ray->object` untouched on rejection.
- [ ] T049 [P] [US1] Apply the same region check at hit acceptance in `src/ri/geometry/polygons.cpp` (`CPolygonTriangle`/`CPolygonQuad` intersect paths).
- [ ] T050 [US1] Apply the same region check in `src/ri/geometry/surface.cpp` at the `CTesselationPatch` candidate acceptance (~L857 and ~L1024, alongside `trimAccepts`). For displaced leaves, evaluate the predicate at the undisplaced surface point for the hit's (u,v), via the object's own sample/interpolate. Make T039 green.
- [ ] T051 [US1] **Raytrace exit check**: the raytrace variants of T034, T035 and T036, plus T039 and T041, are green, and T037/T038 stay green. REYES and Z-buffer variants are expected red until T055.
- [ ] T052 [US1] Add the generic coverage mask (prerequisite: T034–T036 red):
  - a `CRasterGrid::coverage` field in `src/ri/hiders/reyes.h` (null ⇒ every micropolygon present), allocated and freed with the grid
  - in `src/ri/hiders/reyes.cpp`, where per-quad `bounds` are computed (~L1452 and ~L1705), give a quad marked absent an empty bound (`bounds[0] > bounds[1]`), so `stochasticQuad.h:703` and `zbufferQuad.h:40` drop it with **no rasterizer loop changes**

  Use generic names only (contract §0).
- [ ] T053 [US1] Add the REYES region stage to `CPatch::dice()` in `src/ri/geometry/surface.cpp:142` (~L145–362), parallel to and combinable with the trim stage (both must pass; trim behavior unchanged):
  - `object->region->classifyBound()` on the probe bound: `ALL_OUT` returns; `ALL_IN` dices normally
  - `MIXED` forces a split, terminated by screen size (split until the patch dices into micropolygons ≤ the seam threshold), not by `CRenderer::maxEyeSplits`
  - at the safety depth cap, **keep** the patch
  - increment `numCsgSeamSplits`

  Make T040 green.
- [ ] T054 [US1] Fill the coverage mask in `CReyes::drawGrid()` (`src/ri/hiders/reyes.cpp:695`): after the existing P-only `shadeGrid(nGrid, TRUE)`, if `object->region` is set, evaluate `accepts(this, P)` per vertex, and mark a quad absent iff none of its four vertices is accepted. Generic names only; the quickstart §7 grep gate must still return no matches.
- [ ] T055 [US1] **REYES / Z-buffer exit check**: all T034–T041 tests are green in every hider. Tune the seam threshold in `src/ri/geometry/surface.cpp` if needed. Save 4×-zoom crops of each seam to the scratchpad as supporting evidence.
- [ ] T056 [US1] Implement implied-closure caps in `src/ri/geometry/csgCaps.{h,cpp}` (research D7, FR-021). For operands resolved as single-quadric, synthesize planar `CBilinearPatch` caps that generously cover each open cross-section (end circles at z-min/z-max or height ends, z-cut circles, the two sweep-gap half-planes), carrying the operand's attributes and transform. Each cap's region is "inside the quadric's cross-section" ∧ the boundary rule. Wire them into `CCSGInstanceState` in `src/ri/geometry/csgSolid.cpp` (`caps[op][]`, included in `children`). Make T042 green. Then register `csg-open-quadric-caps-{raytrace,reyes,zbuffer}` as `add_visual_test` scenes in `tests/visual/CMakeLists.txt`; generate their references only after visual confirmation (4× crops), and record the approval in `specs/021-deferred-csg-evaluation/reference-regeneration.md`.
- [ ] T057 [US1] Deliberately regenerate the references for **all 20** registered existing CSG visual scenes, one scene at a time, saving before/after 4× seam crops and recording the comparison and reason in `specs/021-deferred-csg-evaluation/reference-regeneration.md`:
  - `csg-sphere-cube-{union,difference,intersection}-{raytrace,reyes,zbuffer}`
  - `csg-sphere-sphere-{tight,loose}-tolerance-raytrace`
  - `csg-subdivision-sphere-union-raytrace`, `csg-nurbs-sphere-union-raytrace`
  - `csg-nested-tree-4-level-raytrace`, `csg-object-instance-reuse-raytrace`
  - `csg-sphere-sphere-difference-interior-raytrace`, `csg-sphere-sphere-difference-interior-raytrace-inside`
  - `blobby-csg-difference-{raytrace,reyes,zbuffer}`

  This ends the T047 expected-red window.

**Checkpoint (Phase 3, MVP)**: User Story 1 is fully functional. The hero scene is crack-free and smooth in every hider, faces keep their operands' shaders, and every existing CSG test is green against its deliberately regenerated reference. Stage and pause for commit.

---

## Phase 4: User Story 2 — The same solid looks the same in every visibility method (Priority: P2)

**Goal**: raytrace, REYES and Z-buffer agree within the existing parity thresholds, including for secondary rays, displaced operands and translucent operands (FR-013, FR-015, SC-004, SC-008).

**Independent Test**: `ctest -L parity -R csg` is green (quickstart.md §2).

### Tests for User Story 2 ⚠️

- [ ] T058 [US2] Add cross-hider parity registrations in `tests/visual/CMakeLists.txt` (label `parity`, thresholds from `tests/visual/parity-thresholds.md`), keeping any pair already registered (~L2560–2566) instead of duplicating it:
  - **three-hider scenes**, pairs raytrace↔reyes and raytrace↔zbuffer: `csg-sphere-cube-{union,difference,intersection}`, `blobby-csg-difference`, `csg-hero-*-800`, `csg-coincident-{union,difference}`, `csg-open-quadric-caps`
  - **two-hider scenes**, pair raytrace↔reyes: `csg-hero-two-shaders`, `csg-hero-displaced`, `csg-hero-translucent`, `csg-secondary-rays`

  The translucent pair additionally runs `--enclosed-holes --opaque 0.45`; the displaced pair runs `--enclosed-holes`. Both are via `add_csg_metric_test`.
- [ ] T059 [US2] Register `CsgSecondaryRays_{raytrace,reyes}` via `add_csg_metric_test` for `csg-secondary-rays-{raytrace,reyes}.rib`: `--enclosed-holes --opaque 0.01 --include disc:<shadow-through-hole>`, which requires the hole's shadow to be clear (lit), plus `--annuli` on the reflected seam rings (FR-013).
- [ ] T060 [US2] (verification) Register `CsgUntouchedRegion_hero`: render the hero scene and the same scene with a plain `Sphere`, no solid block (add `examples/rib/tests/csg-hero-plain-sphere-raytrace.rib`), and compare with `--region-compare --exclude annulus:<hole annulus>` (SC-008).

### Implementation for User Story 2

- [ ] T061 [US2] Investigate and fix every divergence surfaced by T058–T060, inside the geometry domain only: for example, displaced-leaf evaluation in `src/ri/geometry/surface.cpp`, seam-threshold or ε calibration in `src/ri/geometry/csgClassify.cpp`, cap orientation in `src/ri/geometry/csgCaps.cpp`. No hider or shading edits beyond T052/T054. Re-run until green.

**Checkpoint (Phase 4)**: all hiders agree. Stage and pause for commit.

---

## Phase 5: User Story 3 — Everything solids could already do keeps working (Priority: P2)

**Goal**: spec 013 behavior is preserved. Procedural operands are newly supported (FR-024/FR-025). The tolerance attribute is inert (FR-023).

**Independent Test**: `ctest -R "CSG|Csg|csg"` is green (quickstart.md §3 and §5).

### Tests for User Story 3 (write first, confirm they FAIL) ⚠️

- [ ] T062 [P] [US3] Create `tests/unit/csg/test_procedural_operand.cpp`. Assert:
  - a `Procedural` inside `SolidBegin "primitive"` is captured without error
  - it is not expanded at `SolidEnd`
  - a never-needed solid's procedural is never expanded
  - it is expanded once on the instance's first use, into that operand
  - nested procedurals expand recursively
  - a `SolidBegin` emitted during expansion reports the nested-in-primitive diagnostic and is ignored
  - an empty expansion follows FR-025
  - an archive containing only the hero's open `Cylinder` gives an operand resolved as single-quadric, with implied closure and caps (F8)
  - `numCsgProceduralExpansions` counts correctly

  Register it in `tests/unit/csg/CMakeLists.txt`.
- [ ] T063 [US3] Rename `examples/rib/tests/csg-malformed-procedural-in-primitive.rib` to `examples/rib/tests/csg-procedural-missing-archive.rib` and rewrite its header comment: the procedural (still pointing at `nonexistent.rib`) is **accepted at capture** (FR-024), expansion reports exactly one archive-open diagnostic, and the procedural contributes nothing, so the operand is the block's `Sphere 1 -1 1 360` and **the render shows that sphere**. Update its T013 registration: `LOG_PASS` on the archive diagnostic, `LOG_FAIL` on the old "cannot be declared inside a SolidBegin block" text, and `--enclosed-holes` plus non-empty coverage. Red until T066.
- [ ] T064 [US3] Register `CsgProcedural_sphere` and `CsgProcedural_open_cylinder` via `add_csg_metric_test` with `COPY examples/rib/tests/archives`. Each renders the procedural scene and its inline twin and compares them with `--region-compare --include frame` (SC-010, F8).
- [ ] T065 [US3] Register `CsgToleranceInert` via `add_csg_metric_test`: `LOG_PASS` on exactly one tessellation-tolerance notice (`LOG_FAIL` on a second), plus `--region-compare --include frame` against the hero raytrace render (FR-023).

### Implementation for User Story 3

- [ ] T066 [US3] Support procedural operands (research D8):
  - remove the rejection call `csgValidateProceduralCapture(currentSolid)` in `RiProcedural()` (`src/ri/render/rendererContext.cpp:5201`) and its definition (`src/ri/geometry/csgTree.cpp:90`)
  - add an operand-capture mode to `CRendererContext` (`rendererContext.{h,cpp}`) that diverts `addObject()` into a given operand's member list
  - in `src/ri/geometry/csgSolid.cpp`, expand procedural members during `CCSGInstanceState` resolution under `delayedMutex`, reusing `processDelayedObject()` with recursion, **before** operand kinds and caps are derived
  - route a `SolidBegin` during expansion to `csgValidateNestedSolidBegin()`
  - empty-operand semantics
  - increment `numCsgProceduralExpansions`

  Make T062–T064 green.
- [ ] T067 [US3] Make `tessellationtolerance` inert (research D10):
  - remove every remaining read of `CAttributes::tessellationTolerance`
  - keep parsing in all four attribute layers
  - in `RiAttributeV()` (`src/ri/render/rendererContext.cpp`), emit the notice on the first occurrence per render, using a per-render flag reset at frame begin and a level that `CODE_LOG` does not swallow
  - update the field comment in `src/ri/state/attributes.h:165`

  Make T065 green.
- [ ] T068 [US3] (verification) Re-verify `csg-object-instance-reuse-raytrace` and `csg-nested-tree-4-level-raytrace` after the US3 changes; fix `src/ri/geometry/csgSolid.cpp` if needed.
- [ ] T069 [US3] (verification) Re-run every T057 scene and the T034–T041 tests after the US3 changes; all stay green with no reference changes.
- [ ] T070 [US3] (verification) Confirm the T013 diagnostic guards for `csg-malformed-{bogus-operation,nested-in-primitive,unmatched-solidend}` are still green (FR-019), along with empty-block and single-operand-block behavior.

**Checkpoint (Phase 5)**: `ctest -R "CSG|Csg|csg"` is green. Stage and pause for commit.

---

## Phase 6: User Story 4 — Solids only cost work where they are actually seen (Priority: P3)

**Goal**: nothing resolves, classifies or expands for solids or parts of solids that never contribute; refinement tracks the view (FR-001 to FR-004, SC-006).

**Independent Test**: the CSG statistics counters behave as specified (quickstart.md §4).

### Tests for User Story 4 ⚠️

All four are verification of laziness delivered by T045/T046/T053/T066. Any that fails drives T075.

- [ ] T071 [US4] Register `CsgOffscreenStats` via `add_csg_metric_test` `STATS` (with `COPY examples/rib/tests/archives`) for `csg-offscreen-stats.rib`: `CSG instances resolved:`, `CSG classifications:` and `CSG procedural expansions:` are all `== 0` (SC-006).
- [ ] T072 [US4] Register `CsgPartialVisibilityStats` via `STATS` ratio: `CSG seam splits:` of `csg-hero-partial-stats.rib` must be ≤ 0.75 × that of `csg-hero-raytrace-800.rib` (US4 scenario 2, FR-004).
- [ ] T073 [US4] Register `CsgResolutionScaling` via `STATS`: `CSG seam splits:` of `csg-hero-stats-1600-reyes.rib` must be > that of `csg-hero-stats-800-reyes.rib` (FR-003 proxy).
- [ ] T074 [US4] Register `CsgTinySolidStats` via `STATS`: `CSG seam splits:` for `csg-tiny-solid-stats.rib` must be `<= 16` (spec edge case, very small solids).

### Implementation for User Story 4

- [ ] T075 [US4] Remove any eager work found by T071–T074 in `src/ri/geometry/csgSolid.cpp`, `src/ri/geometry/csgDefinition.cpp`, `src/ri/geometry/csgTree.cpp` and the seam threshold in `src/ri/geometry/surface.cpp`. For example, compute the solid's bound from the template without instantiating, never touch `CCSGInstanceState` from culling or bound queries, and stop seam splitting at pixel precision.

**Checkpoint (Phase 6)**: laziness and view-driven refinement proven. Stage and pause for commit.

---

## Phase 7: Polish & Cross-Cutting Concerns

- [ ] T076 Delete `src/ri/geometry/csgBoolean.{h,cpp}`, `src/ri/geometry/solidObject.{h,cpp}` and `CRendererContext::processDelayedSolid` (`src/ri/render/rendererContext.{h,cpp}`). Remove them from `src/ri/CMakeLists.txt`: `geometry/csgBoolean.cpp` at L62 (ribRender list only), and `geometry/solidObject.cpp` at L126 and in the ribVector exclusion list at L369. Remove the related libribpreview comment text (~L295–300). `geometry/csgTree.cpp` (L344) stays.
- [ ] T077 For each spec 013 tessellation helper (`tesselateQuadricAdaptive`, `tesselatePatchMeshAdaptive`, `tesselateNURBSPatchMeshAdaptive`, `csgTessellatePolygonMeshOperand`, `tessellateToSurfaces`), grep for remaining non-CSG callers and delete every helper left unused. In the same pass:
  - delete `src/ri/geometry/csgTesselationDispatch.cpp` and `src/ri/vector/csgTesselationDispatchStub.cpp`, with their CMake entries (L63, L343, L373)
  - remove the friend declaration at `src/ri/geometry/polygons.h:73`
  - retire `tests/unit/csg/test_quadric_tesselation.cpp`, `test_patchmesh_tesselation.cpp` and `test_tesselation_flatness.cpp` with their registrations, keeping any whose helper survives
- [ ] T078 [P] Update `docs/site/content/manual/reference/solid-csg-operations.md` (Hugo, constitution VII) to cover:
  - deferred evaluation and seam quality
  - open single-quadric operands and implied caps
  - closedness rules and the "not closed" diagnostic
  - multi-surface operands closed by the author
  - overlapping shapes, and direct leaves of a boolean block, combining by parity
  - procedural operands
  - the inert tolerance attribute and its notice
  - the CSG statistics section (SC-009)
- [ ] T079 [P] Update `DEVNOTES.md` (status table entry for spec 021, superseding 013's resolution) and `DEVNOTES_DETAILS/RISPEC_GAPS.md` (CSG section).
- [ ] T080 [P] Update the `CLAUDE.md` "Known gotchas":
  - CSG is spatial trimming via the generic region hook, inherited through `setChildren`
  - the hook must keep generic names because `reyes.cpp` references it
  - classification bypasses ray filters and uses orientation-independent parity
  - free-form crossings go through the renderer-only `surfaceCrossingDispatch`/stub split
  - trims still drop straddlers, while CSG keeps them

  Also add a superseded-by-021 note under Decisions 1, 3 and 4 of `specs/013-solid-csg-operations/research.md`.
- [ ] T081 Performance (SC-007): time every scene in `specs/021-deferred-csg-evaluation/baseline-timings.md` on the same machine and build, throttled. Record new times and ratios there; every ratio must be under 10×.
- [ ] T082 Full regression: run `ctest --test-dir build -L visual` sequentially, then a full `ctest`. Classify every failure against quickstart.md §3's known-flaky / pre-existing list, and re-run flaky failures in isolation before attributing them.
- [ ] T083 Run every quickstart.md check (§1–§7) end to end and record the results in `specs/021-deferred-csg-evaluation/validation.md`.
- [ ] T084 Linux validation (constitution VI): hand the branch to the maintainer's Linux validation path (Maestri / Ubuntu 24.04) and confirm the build is warning-free and `ctest -L csg` plus the CSG visual and parity tests pass there.

**Checkpoint (Phase 7)**: feature complete. Stage and pause for commit.

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: no dependencies.
- **Foundational (Phase 2)**: depends on Phase 1. **Blocks every user story.**
- **US1 (Phase 3)**: depends on Phase 2. It introduces `CCSGSolid` (T046), the switch-over (T047) and the coverage mask (T052), which every later story builds on.
- **US2 (Phase 4)**: depends on US1.
- **US3 (Phase 5)**: depends on US1. It can run alongside US2, but serialize `tests/visual/CMakeLists.txt` edits.
- **US4 (Phase 6)**: depends on US1 and on T066 (US3), because T071 uses a procedural operand.
- **Polish (Phase 7)**: depends on all stories.

### Within Phases

- Tests precede the implementation they cover and must FAIL first (constitution III). Mapping of red test → implementation:
  - T021 → T025
  - T015 → T027–T029
  - T015–T019, T022, T023 → T030/T031
  - T020 → T032
  - T043 → T047
  - T044 and T045 → T046
  - T039 → T050
  - T034–T036 → T048–T054
  - T040 → T053
  - T041 → T046
  - T042 → T056
  - T062–T065 → T066/T067
- Guard and verification tasks (may pass when written): T013, T037, T038, T060, T068–T074.
- Phase 2 order: T025 → T026 → (T027, T028, T029) → T030 → T031 → T032. T028 and T029 follow T026 because they share files.
- US1 order: T034–T045 → T046 → T047 → (T048, T049, T050) → T051 → T052 → T053 → T054 → T055 → T056 → T057.
- T047 to T057 is the expected-red window for existing reference-image tests.
- Any two tasks that edit `tests/visual/CMakeLists.txt` or `tests/unit/csg/CMakeLists.txt` run sequentially.

### Parallel Opportunities

- Phase 1: T002 and T005–T012 (different files).
- Phase 2 tests: T015–T023 (different files). Implementation: T027 alongside T028/T029 after T026.
- US1: T042–T045 (different test files); T049 alongside T048/T050.
- US3: T062 alongside T063.
- Polish: T078–T080.

---

## Parallel Example: Phase 2 tests

```bash
Task: "Create tests/unit/csg/test_count_crossings.cpp (T015)"
Task: "Create tests/unit/csg/test_classify_quadrics.cpp (T016)"
Task: "Create tests/unit/csg/test_parity_robustness.cpp (T017)"
Task: "Create tests/unit/csg/test_boundary_rule.cpp (T018)"
Task: "Create tests/unit/csg/test_region_inheritance.cpp (T021)"
```

## Parallel Example: User Story 1

```bash
Task: "Create tests/unit/csg/test_implied_caps.cpp (T042)"
Task: "Create tests/unit/csg/test_capture_path.cpp (T043)"
Task: "Update tests/unit/csg/test_interior_exterior_selection.cpp (T044)"
Task: "Create tests/unit/csg/test_lazy_resolution.cpp (T045)"
```

---

## Implementation Strategy

### MVP First (User Story 1)

1. Phase 1 (harness, scenes, guards, baseline) → Phase 2 (geometry core; no behavior change).
2. Phase 3 (US1): red tests → `CCSGSolid` and switch-over → raytrace path (T051 gate) → coverage mask and REYES/Z-buffer path (T055 gate) → caps → deliberate reference regeneration (T057).
3. **Stop and validate**: the hero scene is crack-free and smooth in every hider, and every existing CSG test is green. This is the MVP.

### Incremental Delivery

US1 (seams) → US2 (parity, secondary rays, displaced and translucent operands) → US3 (procedurals, tolerance attribute, preservation) → US4 (laziness and view-driven refinement) → Polish (retire the BSP path, docs, performance, Linux). Each checkpoint is a review-and-commit pause.

---

## Notes

- [P] means different files and no dependency on an incomplete task.
- Never regenerate a reference image to make a test pass. Regenerate only after visual approval, recorded in `reference-regeneration.md` (T056, T057).
- Do not misattribute known flaky or pre-existing failures (quickstart.md §3).
- Keep hiders and shading free of CSG identifiers (contract §0; quickstart §7 grep gate).
