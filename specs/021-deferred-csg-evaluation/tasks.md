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
- Every phase has two stops: one after its test tasks (the maintainer approves the red tests before implementation, constitution III) and one at its end.
- Run builds, renders and ctest under `nice -n 15` with `taskpolicy -b` on macOS.
- Run the visual suite sequentially or with low `-j`, to avoid throttling timeouts.

**Class and hook names** (data-model.md):
- `CCSGDefinition`: shared, immutable template tree
- `CCSGSolid`: the scene `CObject`
- `CCSGInstanceState`: per-instance state
- `CCSGRegion`: implements the generic `CRegionPredicate`
- generic names the hider may reference (contract §0): `CRegionPredicate`, `CObject::region`, `CObject::materialize()`, `CSurface::countCrossings()`, `CObject::regionCoverage()`, `CObject::honorsRegion()`, `CRasterGrid::coverage`

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

  Add the five geometry `.cpp` files to the shared `ri_sources` list in `src/ri/CMakeLists.txt` next to `geometry/csgTree.cpp` (~L64). Add them to the ribVector source-exclusion list (consumed by libribpreview) beside `geometry/csgTree.cpp`/`geometry/solidObject.cpp` (~L344/L369), extending the comment at ~L295. Add the stub to the ribVector list next to `vector/csgTesselationDispatchStub.cpp` (~L373). Confirm `cmake --build build` stays warning-free (`-Werror`).
- [ ] T002 [P] Create the seam-metric CLI `tests/visual/test_csg_seam_metric.cpp`, modeled on `tests/visual/test_radial_histogram.cpp`. It reads RGBA TIFFs via libtiff, prints one CSV summary line per check, exits PASS/FAIL, and supports `--help` describing every mode and option (constitution IV).

  **Region options** (every mode): `--include <shape>` and `--exclude <shape>`, where shape is `disc:cx,cy,r`, `annulus:cx,cy,r0,r1`, `rect:x0,y0,x1,y1`, `frame`, or `mask:<png>`; and `--margin <px>` (default 2), which shrinks every region boundary to absorb pixel-filter falloff.

  **Modes**:
  - **(a) `--annuli`**: "must be clear" regions (alpha < 0.01), "must be solid" regions (alpha ≥ 0.99), and seam radii with a pixel tolerance. Report stray-pixel counts and the maximum radial deviation of the seam transition from each analytic radius.
  - **(b) `--edge-compare <candidate> <reference> --tolerance <px>`**: maximum distance between corresponding seam/silhouette edges of two renders.
  - **(c) `--region-compare <a> <b>`**: block-average diff within the region.
  - **(d) `--region-chroma <image> --rgb-share <r> <g> <b> --tolerance <t>`**: mean chromaticity (each channel ÷ (R+G+B)) within the region must match the expected share within tolerance. Also accepts per-channel `--min-share c=v` / `--max-share c=v`.
  - **(e) `--projected-curve <image> --camera <fov> <w> <h> <4x4 object-to-camera> --circle <cx,cy,cz,nx,ny,nz,r> [--clip-box <x0,y0,z0,x1,y1,z1>] [--facing <cx,cy,cz>]…`**: sample each exact 3D intersection circle, given in the solid's object space. Skip samples outside its `--clip-box`, so arcs bounded by another operand can be expressed. With `--facing`, keep a sample only when (p − c)·(eye − p) > 0 (the front side of a convex operand centred at c). Project the remaining samples through the camera and require the detected seam edge (alpha or luminance discontinuity) within `--tolerance` px of every one.
  - **(f) `--enclosed-holes <image> [--opaque <alpha>]`**: count non-opaque pixels (alpha < `--opaque`, default 0.99) not connected to the image border by a flood fill. These are see-through gaps, with legitimate openings removed via `--exclude`.
  - **(g) `--luminance-ratio <image> --numerator <shape> --denominator <shape> [--min <r>] [--max <r>]`**: the mean Rec.709 luminance of un-premultiplied RGB within the numerator region, divided by that within the denominator region, must lie in [min, max]. Used where alpha cannot tell regions apart (shadows, reflections).
- [ ] T003 Register the `test_csg_seam_metric` executable in `tests/visual/CMakeLists.txt`, next to `test_radial_histogram` (~L40), linking `TIFF::TIFF` with cxx_std_20.
- [ ] T004 Create the render-and-measure harness: the `cmake -P` driver `tests/visual/csg_metric_test.cmake` and the macro `add_csg_metric_test(NAME <n> RENDERS <rib>[:<out.tif>]… [METRIC <args…>]… [LOG_PASS <regex>]… [LOG_FAIL <regex>]… [STATS <label> <op> <value|other-render>]… [COPY <dir>]… [LABELS …])` in `tests/visual/CMakeLists.txt`. `LABELS` defaults to `csg;visual`. The driver:
  - copies each `COPY` directory into the per-test scratch working directory
  - renders every RIB with the same environment `add_visual_test` uses, running `orender -x 3` with `ORENDER_INSTR_LEVEL` unset, because `orender` sets the log level to NONE at startup (orender.cpp:611–612) and INFO-level statistics print only at `-x 3` or above, and failing on any render error
  - captures stdout and stderr together (statistics and warnings go to stdout, errors to stderr; `ri.cpp:2354–2378`) and applies `LOG_PASS`/`LOG_FAIL` to that combined output itself, so CTest's `PASS_REGULAR_EXPRESSION` never overrides the exit code
  - runs every `METRIC` invocation of `test_csg_seam_metric` and propagates its exit code
  - extracts end-of-frame statistics by label for `STATS` comparisons (`==`, `<=`, `>`, ratio-to-other-render)
- [ ] T005 [P] Create the six hero scenes `examples/rib/tests/csg-hero-{raytrace,reyes,zbuffer}-{800,1600}.rib`:
  - sphere `1.50 -1.50 1.50 360` minus `Cylinder 0.65 -2 2 360`
  - `Translate 0 0 9`, `fov 38`, `raydistant` light, `plastic` surface
  - Formats 800×500 and 1600×1000, `Display` RGBA
  - A header comment documenting the analytic radii. At 800×500: clear through-hole disc r=45.6 px, near seam ring r=61.7 px, sphere silhouette r=122.7 px. Scale ×2 at 1600×1000.
- [ ] T006 [P] Create the six sphere-minus-cube scenes `examples/rib/tests/csg-sphere-cube-difference-{raytrace,reyes,zbuffer}-{800,1600}.rib` at **800×500 and 1600×1000** (matching spec SC-002/SC-003), copying the geometry and camera of `examples/rib/tests/csg-sphere-cube-difference-reyes.rib`. Record in each header the object-to-camera matrix and the four exact seam arcs used by `--projected-curve`, in the solid's object space: the circles on the planes y = ±0.5 and z = ±0.5 (centres (0,±0.5,0) and (0,0,±0.5), normals along y or z, r = √0.75), each with `--clip-box 0.1,-0.5,-0.5,1.1,0.5,0.5` (the cube) and `--facing 0,0,0` (the sphere centre). The arcs meet at x = √0.5. The cube's x = 0.1 face lies entirely inside the sphere and the x = 1.1 plane misses it, so neither carries a seam.
- [ ] T007 [P] Create the coincident-face scenes `examples/rib/tests/csg-coincident-{union,difference}-{raytrace,reyes,zbuffer}.rib`:
  - two unit boxes (six bilinear patches each) sharing one face for the union
  - a box minus a box flush with one face for the difference
  - a camera angle that exposes the shared or flush face
  - expected twins with no solid block, same camera and shaders: `examples/rib/tests/csg-coincident-union-expected-raytrace.rib` (one 2×1×1 box) and `examples/rib/tests/csg-coincident-difference-expected-raytrace.rib` (the box that remains after the flush subtraction), each built from plain bilinear patches
- [ ] T008 [P] Create the implied-cap scenes `examples/rib/tests/csg-open-quadric-caps-{raytrace,reyes,zbuffer}.rib`: a sphere r=1 intersected with `Cylinder 0.6 -0.5 0.5 180` (an open half-sweep, a single-quadric operand), oriented so that both flat sweep-gap caps and one end cap lie on the composite boundary and face the camera. The sphere operand is `Color [1 0.8 0.3]` and the cylinder operand `Color [0.2 0.35 1]`; the header records each visible cap's image region for `--region-chroma`.
- [ ] T009 [P] Create the procedural-operand scenes:
  - archives: `examples/rib/tests/archives/csg-sphere-operand.rib` (contains `Sphere 1.50 -1.50 1.50 360`) and `examples/rib/tests/archives/csg-open-cylinder-operand.rib` (contains the T008 half-sweep `Cylinder 0.6 -0.5 0.5 180`, whose caps are visible)
  - `examples/rib/tests/csg-procedural-operand-raytrace.rib`: hero geometry, with the sphere supplied by `Procedural "DelayedReadArchive" ["archives/csg-sphere-operand.rib"] [-1.5 1.5 -1.5 1.5 -1.5 1.5]` inside `SolidBegin "primitive"`
  - `examples/rib/tests/csg-procedural-open-cylinder-raytrace.rib`: the T008 implied-cap geometry (sphere r=1 intersected with the half-sweep cylinder), with the cylinder supplied by `Procedural "DelayedReadArchive" ["archives/csg-open-cylinder-operand.rib"] [-0.6 0.6 -0.6 0.6 -0.5 0.5]` (the bound must enclose the whole cylinder). Its caps lie on the composite boundary and face the camera, so the comparison with the inline twin observes them.
  - inline twins `examples/rib/tests/csg-procedural-operand-inline-raytrace.rib` (hero geometry) and `examples/rib/tests/csg-procedural-open-cylinder-inline-raytrace.rib` (identical to the T008 raytrace scene)

  **Path note**: visual tests run with `WORKING_DIRECTORY` set to a per-test scratch directory (`tests/visual/CMakeLists.txt:171`). Registrations of these scenes therefore pass `COPY examples/rib/tests/archives` to `add_csg_metric_test` (T004).
- [ ] T010 [P] Create the remaining scenes:
  - `examples/rib/tests/csg-offscreen-stats-{raytrace,reyes}.rib`: one hero solid placed behind the camera, its sphere operand supplied by `Procedural "DelayedReadArchive" ["archives/csg-sphere-operand.rib"]`, plus a plain visible sphere; `Option "statistics" "endofframe" [2]`.
  - `examples/rib/tests/csg-tolerance-inert-raytrace.rib`: the hero scene with `Attribute "solid" "float tessellationtolerance" [0.01]` set three times on different operands.
  - `examples/rib/tests/csg-secondary-rays-{raytrace,reyes}.rib`: the hero solid over a ground plane, lit so its shadow shows the through-hole, with a mirror plane reflecting it. The REYES variant uses shader-traced shadows and reflections. Each header records the analytic image regions used by `--luminance-ratio`: the lit spot the hole lets through the solid's shadow, the solid's shadow body, open lit ground, the reflected hole disc and the reflected sphere ring.
  - `examples/rib/tests/csg-hero-stats-{800,1600}-reyes.rib`: the REYES hero at both resolutions with `Option "statistics" "endofframe" [2]`.
  - `examples/rib/tests/csg-hero-partial-stats-reyes.rib`: identical to `csg-hero-stats-800-reyes.rib` plus `CropWindow 0 0.5 0 1`, so only the left half of the same frame is rendered, with the same camera and micropolygon size.
  - `examples/rib/tests/csg-hero-single-operand-stats-800-reyes.rib`: `csg-hero-stats-800-reyes.rib` with the `difference` block holding only the sphere's `primitive` block (a single-operand collapse, so no region predicate).
  - `examples/rib/tests/csg-tiny-solid-stats-reyes.rib`: the REYES hero solid scaled to cover under 4×4 pixels; `Option "statistics" "endofframe" [2]`.
  - `examples/rib/tests/csg-polymesh-box-difference-{raytrace,reyes}.rib`: a `PointsPolygons` cube minus a sphere.
  - `examples/rib/tests/csg-polymesh-open-raytrace.rib`: a `PointsPolygons` box with one face missing (an open mesh) as the second operand of a `difference` with a sphere.
  - `examples/rib/tests/csg-nurbs-sheet-expected-raytrace.rib`: the existing `csg-nurbs-sphere-union-raytrace.rib` with the open NURBS sheet removed and no solid block (a plain `Sphere 0.7 -0.7 0.7 360`, same camera and shaders): what that scene must render once its open operand contributes nothing.
  - `examples/rib/tests/csg-hero-instanced-outside-world-reyes.rib`: the hero solid defined in an `ObjectBegin`/`ObjectEnd` placed before `WorldBegin`, instanced once inside the world, `Option "statistics" "endofframe" [2]`.
  - `examples/rib/tests/csg-nurbs-sheet-instanced-raytrace.rib`: the solid of the existing `csg-nurbs-sphere-union-raytrace.rib` (whose NURBS operand is an open sheet) inside `ObjectBegin`/`ObjectEnd`, instanced twice.
  - `examples/rib/tests/csg-nurbs-rational-sphere-union-{raytrace,reyes}.rib`: `Sphere 0.7 -0.7 0.7 360` unioned with a **closed rational quadratic NURBS sphere** of radius 0.5 centred at (0.45, 0, −0.35). u is order 3 with knots `[0 0 0 .25 .25 .5 .5 .75 .75 1 1 1]` (9 control points); v is order 3 with knots `[0 0 0 .5 .5 1 1 1]` (5 control points); homogeneous `"Pw"`. Each header records the object-to-camera matrix and the exact intersection circle of the two spheres, for `--projected-curve`. (The existing `csg-nurbs-sphere-union-raytrace.rib` is *not* a sphere: its NURBS operand is a single open, non-rational 4×4 bicubic sheet. It becomes the open-sheet case, T038/T057.)
  - `examples/rib/tests/csg-nurbs-rational-sphere-alone-raytrace.rib`: the rational NURBS sphere alone, same camera. Its header, and the subdivision `*-alone` scene's, record the image rectangle of the free-form operand's untouched visible area (outside the 0.7 sphere's silhouette and away from the seam) for T038's `--include`.
  - the REYES twin `examples/rib/tests/csg-subdivision-sphere-union-reyes.rib` of the existing raytrace scene, plus `examples/rib/tests/csg-subdivision-sphere-alone-raytrace.rib` (the subdivision operand alone, same camera).
- [ ] T011 [P] Create `examples/rib/tests/csg-hero-two-shaders-{raytrace,reyes}.rib`: the hero scene with `Surface "plastic"` + gold `Color [1 0.8 0.3]` on the sphere operand and `Surface "matte"` + blue `Color [0.2 0.35 1]` on the cylinder operand (FR-016).
- [ ] T012 [P] Create the edge-case scenes:
  - `examples/rib/tests/csg-hero-displaced-{raytrace,reyes}.rib`: the hero scene with `Displacement "bump_lowfreq" "float Km" [0.05]` on the sphere operand and `Attribute "displacementbound" "float sphere" [0.05] "string coordinatesystem" ["object"]`. `bump_lowfreq` really moves P (`P = P − N̂·noise·Km`, always inward, at most Km); `dented` does not qualify, because it only rewrites N. The header records the projected maximum displacement at the seam depth (Km × pixels per unit, about 4.7 px at 800×500).
  - `examples/rib/tests/csg-hero-translucent-{raytrace,reyes}.rib`: the hero scene with the sphere operand at `Opacity [0.5 0.5 0.5]`.
- [ ] T013 Register diagnostic guards for the four malformed scenes `examples/rib/tests/csg-malformed-{bogus-operation,nested-in-primitive,unmatched-solidend,procedural-in-primitive}.rib` via `add_csg_metric_test`. Use `LOG_PASS` regexes for **today's** diagnostics, including the current procedural rejection, and require the renderer not to crash. Labels `csg;visual` (the T004 default), so the T033 gate includes them. These are guards: green on master, so later changes can't silently alter diagnostics (FR-019).
- [ ] T014 Record master baselines in `specs/021-deferred-csg-evaluation/baseline-timings.md`:
  - throttled wall-clock render time for every existing `examples/rib/tests/csg-*.rib` visual scene, the `blobby-csg-*` scenes and the T005/T006 scenes
  - the T002 tool's metric output on master for T005/T006/T007 (the expected-red baseline; the hero scene should reproduce the 213 / 63 / 38 stray pixels at 1600×1000), including the `--annuli` maximum radial seam deviation for every hero leg
  - the master result of every Phase 3 test marked "confirm red" (T034–T036, T038's `--projected-curve` and closedness legs, T039–T042 render checks). Any leg that already passes on master is recorded here as a guard leg, not a red test.

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
  - cone, paraboloid (including zmin > 0), hyperboloid
  - a partial-`thetamax` torus (flat circular-segment sweep-gap caps) and a partial-phi torus (closed by the cone-shaped surface joining its two cut circles: the implied solid is the circular segment between the tube arc and that chord, revolved)
  - a lone `CDisk`, which is an empty operand: silently, always OUT

  Use points on both sides of every implied cap.
- [ ] T017 [P] Create `tests/unit/csg/test_parity_robustness.cpp`. Assert parity classification (odd crossings ⇒ IN) for multi-surface operands built from directly constructible surfaces:
  - a six-`CBilinearPatch` box
  - the same box with deliberately mixed vertex orders
  - the same box with one patch under `ReverseOrientation`
  - a `CCylinder` plus two `CDisk`s (author-closed; no doubled caps)
  - two overlapping spheres in one operand (the overlap classifies OUT)

  Include grazing, edge and vertex rays; the fallback direction plus majority-of-three must still classify correctly. The grazing and edge thresholds are named constants in `csgClassify.cpp` (|N̂·d̂| < 1e-4; parametric distance to an edge < 1e-5); exercise both. Also assert `deriveOperandKind(members, surfaces)` (T030) on directly constructible surfaces, grouping surfaces by member:
  - one member made of six `CBilinearPatch`es forming a box (as a patch mesh would): closed, multi-surface parity applies, no diagnostic
  - one member made of two adjacent `CBilinearPatch`es forming an open strip: not closed, an empty operand flagged for the diagnostic (the same rule a multi-span open `NuPatch` or an open `PointsPolygons` must follow)
  - a single `CBilinearPatch` member: not closed, empty
  - six separate members forming the same box: multi-surface parity, no closedness test (FR-021 applies to a single member only) The NURBS closedness cases run render-side in T038 (research D13), so the Phase 2 gate only covers tests that can be green in Phase 2.
- [ ] T018 [P] Create `tests/unit/csg/test_boundary_rule.cpp`, building `CCSGResolvedOperand`s directly (T030). For union, intersection and difference (2 and 3 operands), the 4-level nested tree, single-operand and empty-tree collapse, and **direct leaves in a boolean block** (one first operand, parity-combined), assert:
  - the boundary rule at points on each leaf
  - static orientation parity
  - short-circuit evaluation: a test-double `CSurface` subclass whose `countCrossings` counts its calls shows that irrelevant operands are never queried (no production counter needed)
  - **single-operand passthrough (FR-019)**: a solid that collapses to one operand, including one open `Cylinder`, yields that operand unchanged: no region predicate and no implied caps. Empty operands that change nothing are removed first (FR-025a), so `difference(openCylinder, empty)` and `union(openCylinder, Disk)` are passthroughs too. A single-operand block *nested* in a boolean with another non-empty operand still takes part in it, so implied closure applies there.
  - **operands that contribute nothing never render (FR-021, FR-025a)**: in `union(A, Disk)`, no point of the disk leaf is accepted, including points outside A; the leaves of an open-sheet operand are never accepted; and for two identical spheres in one operand (cancelling by parity), no point of either sphere is accepted, because the coincident-face check also counts the other surfaces of the leaf's own operand
  - **empty operands (FR-025a)**, for every empty source (empty nested block, lone disk, lone open non-quadric, cancelling shapes): `difference(empty, B)` and `intersection(A, empty)` yield nothing; `union(A, empty)` and `difference(A, empty)` yield A
  - `classifyBound()` `ALL_IN`/`ALL_OUT`/`MIXED`
  - **analytic seam location**: for the sphere-cube difference, sample 64 points on each exact intersection circle; at ±δ (δ = 1e-4·r) along the sphere's surface, perpendicular to the curve, the predicate must flip, with the matching face-side check. Do the same for the hero's two circles at z = ±√(1.5² − 0.65²).
  - **tangency**: two spheres touching at a point, and one sphere internally tangent to another; neither produces boundary points away from the contact.
- [ ] T019 [P] Create `tests/unit/csg/test_classify_determinism.cpp`. Classify the same point set over analytic, polygon and bilinear operands in forward order, in shuffled order, and from 8 `std::thread`s concurrently; assert bit-identical results (FR-008).
- [ ] T020 [P] Create `tests/unit/csg/test_csg_stats.cpp`. Assert that `CStats` exposes `numCsgClassifications`, `numCsgSeamSplits`, `numCsgInstancesResolved` and `numCsgProceduralExpansions`, zero-initialized per frame; that the end-of-frame statistics at level ≥ 2 print them, zeros included, whenever the frame contains a solid (an outermost `SolidEnd` ran in this frame, single-operand collapses included), with the exact labels `CSG classifications:`, `CSG seam splits:`, `CSG instances resolved:` and `CSG procedural expansions:`; and that one `classify()` call increments `numCsgClassifications` by exactly 1.
- [ ] T021 [P] Create `tests/unit/csg/test_region_inheritance.cpp`. Assert that `CObject::setChildren()` copies the parent's `region` to every child that has none, does not overwrite a child's own `region`, and leaves `region` null throughout when the parent has none. Include a nested case: a grandchild created through an intermediate mesh (as `CLoopSubdivMesh` creates an inner `CPolygonMesh`) inherits the region.
- [ ] T022 [P] Rewrite `tests/unit/csg/test_boolean_boxes.cpp`, `test_boolean_sphere_box.cpp` and `test_boolean_sphere_sphere.cpp` as classification and boundary-rule tests over the same scenarios. Build `CCSGDefinition` directly, with no Ri capture interception.
- [ ] T023 [P] Rewrite the remaining kernel suites, building `CCSGDefinition` directly:
  - `tests/unit/csg/test_boolean_coplanar.cpp`: two-sided ±ε rule
  - `test_nested_tree.cpp`
  - `test_multi_primitive_leaf.cpp`: use two **disjoint** spheres, and assert that two coincident identical spheres in one block form an empty operand (the documented parity consequence)

  Update `tests/unit/csg/csgTestUtils.h`: remove `#include "csgBoolean.h"` and the `computeVolume`/volume-sign helpers; add analytic point-set builders.
- [ ] T024 In `tests/unit/csg/CMakeLists.txt`, register T015–T021 with labels `csg;unit`, keeping the rewritten suites' registrations. Build, run `ctest --test-dir build -L csg`, and confirm the new and rewritten tests FAIL (red) for the right reasons.

**Checkpoint (red tests — maintainer approval)**: stage the new and rewritten tests with their confirmed-red evidence, and stop for the maintainer to approve the tests before any implementation task below starts (constitution III: Tests written → User approved → Tests fail → Then implement).

### Implementation for Phase 2

- [ ] T025 In `src/ri/geometry/object.{h,cpp}`:
  - add the generic `CRegionPredicate` interface (`accepts(CShadingContext *ctx, const float *P, const float *Ng)`, where `Ng` is the accepting surface's undisplaced geometric normal at P in camera space; only its direction matters, not its sign, because the two-sided ON rule uses it as a transverse axis; `classifyBound(CShadingContext *ctx, const float *bmin, const float *bmax)` returning `ALL_IN`/`ALL_OUT`/`MIXED`)
  - add a nullable `CObject::region` member
  - propagate `region` in `CObject::setChildren()` (object.cpp:130) to children that have none
  - add `virtual void CObject::materialize(CShadingContext *)` (default no-op)
  - add `virtual int CObject::honorsRegion() const` (default `TRUE`), overridden to return `FALSE` where an object's own rendering bypasses every trimming hook or encloses no volume: `CImplicit` (`src/ri/geometry/implicitSurface.h`), `CDLObject` (`src/ri/geometry/dlobject.h`), `CPoints` (`src/ri/geometry/points.h`; its `dice()` goes straight to `drawPoints`, geometryDispatch.cpp:555), `CCurve` and `CCurveMesh` (`src/ri/geometry/curves.h`; curves reach `drawGrid` without the `CPatch` region stage), and `CDelayedInstance` (`src/ri/geometry/delayed.h`; an `ObjectInstance` inside a solid, which spec 013 also rejected)
  - add `virtual int CSurface::countCrossings(CShadingContext *ctx, const float *from, const float *dir) const`, returning an unsigned count, default 0

  All names must be generic (contract §0). Make T021 green.
- [ ] T026 Override `materialize()` as `if (children == NULL) create(context);` in the five mesh types:
  - `CPolygonMesh` (`src/ri/geometry/polygons.{h,cpp}`)
  - `CPatchMesh` and `CNURBSPatchMesh` (`src/ri/geometry/patches.{h,cpp}`)
  - `CSubdivMesh` (`src/ri/geometry/subdivisionCreator.{h,cpp}`)
  - `CLoopSubdivMesh` (`src/ri/geometry/subdivisionLoop.{h,cpp}`)

  Each `create()` already calls `setChildren(context, …)` (polygons.cpp:1939, patches.cpp:1710/2203, subdivisionCreator.cpp:2256, subdivisionLoop.cpp:483) and is mutex-guarded and idempotent. `materialize()` itself is one level deep; T046's collector recurses, which reaches the inner `CPolygonMesh` that `CLoopSubdivMesh::create()` builds.
- [ ] T027 [P] Implement `countCrossings` in `src/ri/geometry/quadrics.{h,cpp}` for every quadric class, analytically, counting every root in the valid u/v range (unlike `CSphere::intersect()` at quadrics.cpp:167, which stops at the nearest).
- [ ] T028 [P] Implement `countCrossings` in `src/ri/geometry/polygons.{h,cpp}` for `CPolygonTriangle`/`CPolygonQuad` (exact). Sequence after T026, which edits the same files.
- [ ] T029 Implement free-form crossings:
  - in `src/ri/geometry/surfaceCrossingDispatch.{h,cpp}`: the pure Newton-refinement core over a surface-evaluation callback, plus a per-surface seed grid built from the surface's own evaluation, built once under a mutex and immutable afterwards, with no `CTesselationPatch` cache
  - in `src/ri/vector/surfaceCrossingDispatchStub.cpp`: a stub returning 0
  - `countCrossings` overrides that forward to it: `CBicubicPatch`/`CNURBSPatch` in `src/ri/geometry/patches.cpp` (`CBilinearPatch` is analytic, implemented directly there), `CPatchGrid` in `src/ri/geometry/patchgrid.cpp`, `CBSplinePatchGrid` in `src/ri/geometry/bsplinePatchgrid.cpp`, and `CSubdivision` in `src/ri/geometry/subdivision.cpp`

  Sequence after T026 (shared patches files).
- [ ] T030 Implement `CCSGDefinition` in `src/ri/geometry/csgDefinition.{h,cpp}`. It holds the **template** structure only: flattened nodes, operand template member lists (direct leaves of a boolean block form one first operand), the `leafFlip[]` parity, and the CSG-aware template bound. Include:
  - **rebasing**: captured leaves carry absolute transforms. Move `csgRebaseFragments`' logic (csgTree.cpp:687–722) here: each template leaf's xform becomes `outerXform⁻¹ · leafXform`, and the template bound is computed in that same local frame (each leaf's camera-space bound transformed by `outerXform⁻¹`). Without this, `instantiate()` with the solid's xform applies the outer transform twice.
  - blocks nested inside `"primitive"` contribute no geometry; only the primitive block's own leaves form the operand, as spec 013's resolver does (csgTree.cpp:494–497)
  - the `CCSGResolvedOperand { members, surfaces, kind, bound }` struct and a pure `deriveOperandKind(members, surfaces)` built on the closedness helper below. Closedness is decided per **member** (a quadric, a surface, or a whole mesh), not per surface: a mesh emits one surface per span or face (`CNURBSPatchMesh::create`, patches.cpp:2172–2190), so a member is closed iff every boundary edge of its surfaces collapses to a point or coincides with another boundary edge of the same member. One member ⇒ single (quadric or non-quadric) rules; several members ⇒ multi-surface parity. Unit tests build resolved operands directly; T046 step 6 calls it.
  - **passthrough**: when the whole solid collapses to one operand, it yields that operand unchanged, with no region predicate and no implied caps (FR-019, spec 013 FR-017); the "contains a solid" statistics flag is still set
  - empty-tree and single-operand collapse (spec 013 FR-016/FR-017). An empty nested block stays in its position as an empty operand (FR-025a), unlike spec 013's resolver, which dropped it (csgTree.cpp:511); only a wholly empty tree yields no solid.
  - the FR-021 closedness helper: a quadric is closed iff it is a full sphere or full torus; any other single member (surface or mesh) is closed iff every parametric boundary edge of its surfaces collapses to a point or coincides with another boundary edge of that member, or (polygon and subdivision meshes) every edge is shared by exactly two faces

  The operand *kind* and caps are not stored in the definition; they are derived per instance after expansion (T046, T066), with `deriveOperandKind()`.
- [ ] T031 Implement classification in `src/ri/geometry/csgClassify.{h,cpp}`:
  - operand `classify(ctx, P)` over a `CCSGResolvedOperand` (T030): bound rejection first; analytic implied-closed test for a single-quadric operand; an empty operand (FR-025a: lone disk, lone open non-quadric surface, empty nested block, empty expansion) is always OUT, which yields the empty-set semantics with no special casing; otherwise deterministic fixed-direction parity over the operand's surfaces via `countCrossings`, with the grazing/edge fallback (thresholds from T017) and majority-of-three. The "operand is not closed" warning is emitted at instance resolution (T046), not here.
  - the boundary rule with short-circuit evaluation
  - the two-sided ±ε ON rule at P ± ε·N̂g, using the `Ng` passed to `accepts()` as a transverse axis (sign irrelevant; ε = `C_EPSILON` × solid bound size). P is on the boundary iff composite membership differs between the two sides. The ON check considers every other surface within ε, **including the other surfaces of the leaf's own operand**, so identical shapes that cancel by parity are correctly rejected. Where coincident leaves of several operands all report a boundary there, only the leaf of the **lowest-indexed** such operand accepts, so exactly one face renders, identically in every hider. Orientation is never read from `Ng`; it is the leaf's static `leafFlip`.
  - `CCSGRegion` implementing `CRegionPredicate`

  Make T015–T019, T022 and T023 green.
- [ ] T032 Add the four counters to `CStats` in `src/ri/core/stats.h` as `atomic_int32`, like the blobby counters (stats.h:103–107), since classification runs on every render thread. Initialize them, and in `src/ri/core/stats.cpp` print them at level ≥ 2, zeros included, whenever the frame contains a solid, with the T020 labels. "Contains a solid" is a per-frame flag, reset at `RiWorldBegin`, set by every outermost `RiSolidEnd` inside the world block (`src/ri/render/rendererContext.cpp`), single-operand collapses included. T046 also sets it from `CCSGSolid::instantiate()`, which does not exist yet in this phase. Increment `numCsgClassifications` in `src/ri/geometry/csgClassify.cpp` (sequence after T031; same file). Make T020 green.
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

  Labels `csg;visual`. Confirm red for every leg that fails in the T014 baseline (stray pixels, holes, or radial deviation > 1 px); a leg already passing on master is a guard leg, as recorded in `baseline-timings.md`.
- [ ] T035 [US1] Register `CsgSeam_spherecube_{raytrace,reyes,zbuffer}_{800,1600}` for the T006 scenes: `--enclosed-holes` plus `--projected-curve` against the exact sphere ∩ cube-face circles from each scene header, tolerance 1 px, **in every hider, raytrace included**. Confirm red: the spurs exceed 1 px on master.
- [ ] T036 [US1] Register `CsgSeam_coincident_{union,difference}_{raytrace,reyes,zbuffer}` via `add_csg_metric_test`: `--enclosed-holes`, plus `--region-compare <render> <expected> --include frame` against the T007 `*-expected-raytrace` render (no internal or leftover skin; FR-022), at the existing parity threshold for the REYES and Z-buffer legs. Confirm red.
- [ ] T037 [US1] (guard) Register `CsgPerOperandShader_hero_{raytrace,reyes}` for the T011 scenes with `--region-chroma`: in the hole-wall annulus (45.6–61.7 px at 800×500) blue share ≥ 0.5; in the outer annulus (62–122 px) red share ≥ 0.4 and blue share ≤ 0.2 (FR-016). This is a preservation guard; per-operand shaders already work on master.
- [ ] T038 [US1] (guard) Register `CsgSeam_freeform_{nurbs,subdiv}` and `CsgSeam_polymesh_box` via `add_csg_metric_test` (scenes from T010; `nurbs` uses the `csg-nurbs-rational-sphere-*` scenes):
  - `--enclosed-holes`
  - REYES vs. raytrace `--edge-compare` ≤ 1 px
  - `--region-compare --include rect:<untouched free-form area> --exclude rect:<seam bbox>` of each free-form operand against its `*-alone-raytrace` render, using the rectangle recorded in the T010 headers (the whole frame cannot match, because the union also contains the 0.7 sphere)
  - for both rational-NURBS-sphere-union scenes, `--projected-curve` against the exact intersection circle from their headers (FR-007a bullet 2: a rational NURBS sphere represents a sphere exactly). This leg is a **red** test, not a guard: the raytrace leg maps to T047/T048/T050 and is confirmed at T051; the REYES leg maps to T053/T054 and is confirmed at T055.

  Also register the NURBS closedness checks (research D13):
  - `CsgClosedness_nurbs_closed`: `LOG_FAIL` on "operand is not closed" for `csg-nurbs-rational-sphere-union-raytrace.rib` (a guard)
  - `CsgClosedness_nurbs_open`: `LOG_PASS` on that warning and `LOG_FAIL` on a second occurrence, for the existing `csg-nurbs-sphere-union-raytrace.rib`, whose NURBS operand is a single open sheet, plus `--region-compare --include frame` against `csg-nurbs-sheet-expected-raytrace.rib` (T010): the sheet must not render at all. Red until T047.
  - `CsgClosedness_nurbs_instanced`: `LOG_PASS` on exactly one "operand is not closed" warning, `LOG_FAIL` on a second, for `csg-nurbs-sheet-instanced-raytrace.rib` (two instances, one warning per offending operand per frame). Red until T047.
  - `CsgClosedness_polymesh_open`: `LOG_PASS` on exactly one warning for `csg-polymesh-open-raytrace.rib` (an open mesh is empty plus a warning). Red until T047.

  The remaining legs guard the mesh and free-form paths across the switch-over; their red test is T015.
- [ ] T039 [US1] Register `CsgSeam_hero_displaced_raytrace` (T012 scene): `--enclosed-holes --exclude disc:<hole>` with the hole disc shrunk by the projected maximum displacement recorded in the T012 header, and seam radii within 1 px plus that displacement of the undisplaced analytic radii (membership follows the undisplaced shape; displacement then moves the surviving surface by at most Km). Confirm red. This is the failing test for T050's displaced-leaf handling.
- [ ] T040 [US1] Register `CsgSeamSplitCounter` via `add_csg_metric_test` `STATS`: `csg-hero-stats-800-reyes.rib` must report `CSG seam splits: > 0`, and `csg-hero-single-operand-stats-800-reyes.rib` (T010) must report `= 0`. `csg-hero-instanced-outside-world-reyes.rib` must print the counters at all (the "contains a solid" flag set through `CCSGSolid::instantiate()`), with `CSG instances resolved: > 0`; this leg is red until T046. Confirm red: the counter is never incremented before T053.
- [ ] T041 [US1] Register `CsgInstance_seams` for `examples/rib/tests/csg-object-instance-reuse-raytrace.rib`: `--enclosed-holes`, and each instance's bounding rect has opaque coverage (`--include rect:` per instance). Confirm red. This is the failing test for `CCSGSolid::instantiate()`.
- [ ] T042 [P] [US1] Create `tests/unit/csg/test_implied_caps.cpp`. For each single-quadric kind (cylinder ends, z-cut sphere, half-sweep cylinder, cone base, paraboloid top, and bottom when zmin > 0, hyperboloid ends, partial-`thetamax` torus sweep gaps, and the cone-shaped `CHyperboloid` cap of a partial-phi torus), assert the number, plane (or, for the torus phi cap, the two circles) and orientation of the synthesized caps, and that each cap's predicate accepts exactly the cross-section inside the lateral surface. Register it in `tests/unit/csg/CMakeLists.txt`. Also register `CsgCaps_{raytrace,reyes,zbuffer}` via `add_csg_metric_test` for the T008 scenes: `--enclosed-holes`, plus `--region-chroma` on each cap region recorded in the T008 header (blue share ≥ 0.5, the cylinder operand's colour). Confirm red.
- [ ] T043 [P] [US1] Create `tests/unit/csg/test_capture_path.cpp`: `RiSolidBegin`…`RiSolidEnd` yields exactly one `addObject(CCSGSolid*)` whose definition holds the expected operands (including direct leaves of a boolean block as the first operand). Also assert that template leaf xforms are rebased into the outer frame (a leaf declared under its own `Translate` keeps its offset relative to `outerXform`, not doubled), that the template bound is in that local frame, and that a block nested inside `"primitive"` contributes no geometry. Register it. Confirm red.
- [ ] T044 [P] [US1] Update `tests/unit/csg/test_interior_exterior_selection.cpp` to assert that `ATTRIBUTES_FLAGS_SOLID_FRAGMENT` and the orientation flip (`ATTRIBUTES_FLAGS_INSIDE` toggled per `leafFlip`) are set on each leaf's cloned attributes, not on fragment meshes, and that Interior/Exterior selection at `shading.cpp:1019` behaves as before (spec 013 FR-009 to FR-012, FR-020). Confirm red.
- [ ] T045 [P] [US1] Create `tests/unit/csg/test_lazy_resolution.cpp`: after `SolidEnd` only the `CCSGDefinition` exists (no `CCSGInstanceState`, no leaf instantiation or materialization, no classification), and the first `dice()`/`intersect()` resolves exactly once. Also assert that a member whose `honorsRegion()` is false (a test-double subclass) is dropped at resolution with one "not supported as a solid operand" warning, and that its operand becomes empty when nothing else remains (FR-018). It goes through `RiSolidEnd`, so it turns green at T047. Register it. Confirm red.

**Checkpoint (red tests — maintainer approval)**: stage the new and rewritten tests with their confirmed-red evidence, and stop for the maintainer to approve the tests before any implementation task below starts (constitution III: Tests written → User approved → Tests fail → Then implement).

### Implementation for User Story 1

- [ ] T046 [US1] Implement `CCSGSolid` and `CCSGInstanceState` in `src/ri/geometry/csgSolid.{h,cpp}`:
  - a `CObject` that is not `OBJECT_DUMMY` (trace.cpp:332), bounded by the definition's local template bound (T030) transformed by its own xform
  - on the first `dice()`/`intersect()`, build `CCSGInstanceState` once under `CRenderer::delayedMutex`, mirroring `CDelayedObject::intersect()` (geometryDispatch.cpp:679), **in this order** (research D9):
    0. drop operand members that are not supported as solid operands (FR-018): any member whose `honorsRegion()` is false (implicit, DSO, points, curves, curve meshes, object instances), and, defensively, any member that is neither a `CSurface`, one of the five mesh types with `materialize()`, nor a `CDelayedObject` (procedural). Emit one `warning(CODE_BADTOKEN, …)` naming the primitive, deduplicated per (definition, operand) per frame, as the "not closed" warning is. Dropped members are never rendered. This replaces spec 013's "Unsupported CSG operand type" error, deleted at T047.
    1. **capture target**: `instantiate()` hands its output to `addObject()` (e.g. quadrics.cpp:512). For each operand, point the context's `delayed` at a temporary holder, as `processDelayedSolid()` does (rendererContext.cpp:485), so the output lands in that operand's member list, not in the scene. T066 reuses this target for procedural expansion.
    2. **flagged clones**: for each template leaf, clone its attributes, set `ATTRIBUTES_FLAGS_SOLID_FRAGMENT`, toggle `ATTRIBUTES_FLAGS_INSIDE` per `leafFlip`, and call `leaf->instantiate(clone, solid->xform, context)` (`instantiate()` uses the attributes passed in, e.g. quadrics.cpp:508). Because T030 rebased the template leaves, this rebuilds the absolute transform for a direct placement and the composed one for each `ObjectInstance`. Mesh children copy their parent's `attributes` when `create()` runs, so this must precede step 4.
    3. set `region` on every operand member to its `CCSGRegion`
    4. materialize depth-first: `materialize(context)` on each member, then on each of its children. Mesh children are thereby created with the flagged attributes and inherit the region via `setChildren`, including the inner `CPolygonMesh` of a `CLoopSubdivMesh`.
    5. collect every `CSurface` descendant as that operand's classification surfaces
    6. build each operand's `CCSGResolvedOperand` (members and their surfaces) and derive its **kind** (single-quadric / multi-surface / empty / open) with `deriveOperandKind(members, surfaces)` (T030), so closedness is judged per member, not per surface.
    7. **operands that contribute nothing never render** (FR-021, FR-025a): leave the members of every operand resolved as empty or open out of `children`, with no caps. Then, if the tree reduces to a single non-empty operand once empties that change nothing are removed (an empty later difference operand, an empty union operand), the instance is a **passthrough** (FR-019): that operand renders as authored, with no region and no implied caps. Emit the "operand is not closed" warning (`warning(CODE_BADTOKEN, …)`) once per offending operand per frame, deduplicated per (definition, operand) so further instances stay silent (FR-021).
  - set `children` to the instance leaves of the operands that contribute (meshes, surfaces, and caps later), and make `dice()` call `CObject::dice(r)` (geometryDispatch.cpp:76), so every leaf is clipped and queued per bucket by `drawObject`
  - `instantiate()` creates a new `CCSGSolid` over the same definition with a composed transform, and sets the per-frame "contains a solid" statistics flag (T032), so a solid defined in an `ObjectBegin` outside the world block and instanced inside the frame still prints its counters
  - increment `numCsgInstancesResolved`

  Make T044 green; it builds the definition and the solid directly, without `RiSolidEnd`. Also makes T040's instanced-outside-world leg green once T047 routes `RiSolidEnd` to `CCSGSolid`.
- [ ] T047 [US1] Switch `RiSolidEnd` to the new path:
  - in `src/ri/geometry/csgTree.{h,cpp}`, replace `resolveCSGTree()` with a function that builds a `CCSGDefinition` and calls `context->addObject(new CCSGSolid(...))`
  - delete the now-unused static helpers (`csgResolveNode`, `csgToSoup`, `csgTessellateOperand`, `csgTriangulateGrid`, `csgMakeGridVertex`, `csgEmitTriangle`, `csgTriangleIsDegenerate`, `csgSignedVolume`, `csgFlipAll`, `csgPolygonsToFragments`, `csgBuildMeshForAttributeGroup`, `csgResolveTolerance`; `csgRebaseFragments`' logic has moved to T030), the constant `kCsgDefaultToleranceFraction` (csgTree.cpp:47) and `#include "csgBoolean.h"`, to keep `-Werror` happy
  - update the call site in `src/ri/render/rendererContext.cpp` (~L5826)
  - leave `csgBoolean.*`, `solidObject.*` and `processDelayedSolid` in place but unreferenced; Phase 7 deletes them

  Make T043, T045 and T038's `CsgClosedness_nurbs_open` green. T041 additionally needs the raytrace region checks (T048–T050) and is confirmed at T051.

  **Expected-red note**: from this task until T057, the existing `csg-*`/`blobby-csg-*` reference-image tests are expected red (old BSP-era references). From this task until T055, the REYES legs of T037 (`CsgPerOperandShader_hero_reyes`) and T038 (the REYES `--edge-compare`) are expected red too, because REYES dices CSG leaves without a region stage until T053/T054. From this task until T048–T050, the raytrace legs of T037/T038 are red too, because the intersectors do not consult the region yet; T051 confirms they are green again. Never regenerate references to make a gate pass.
- [ ] T048 [US1] Raytrace hit acceptance for quadrics, in `src/ri/geometry/quadrics.cpp`: in every quadric `intersect()` root loop, before accepting a candidate, `continue` to the next root when `region && !region->accepts(context, P, Ng)`. The root loops work in object space, so compute the camera-space P as `from + t·dir` from the camera-space ray, and Ng by transforming the object-space normal to camera space as the accepted hit does at the end (`mulmn(rv->N, xform->to, tmp)`, quadrics.cpp:276). Leave `ray->t`/`ray->object` untouched on rejection.
- [ ] T049 [P] [US1] Apply the same region check at hit acceptance in `src/ri/geometry/polygons.cpp` (`CPolygonTriangle`/`CPolygonQuad` intersect paths) and in `src/ri/geometry/patches.cpp` at `CBilinearPatch::intersect` (patches.cpp:193, inside its `solve()` acceptance macro). Bilinear patches have their own analytic intersector and never reach `CTesselationPatch`; the sphere-cube cube, the coincident-face boxes and the implied caps are all bilinear patches, so without this the raytrace legs of T035, T036 and T042 cannot pass.
- [ ] T050 [US1] Apply the same region check in `src/ri/geometry/surface.cpp` at the `CTesselationPatch` candidate acceptance (~L857 and ~L1024, alongside `trimAccepts`). Read the region from the surface being tessellated (the same object whose `trimAccepts()` is consulted there), never from the `CTesselationPatch` itself: tessellation patches are assigned as `children` directly, without `setChildren()` (geometryDispatch.cpp:282–322), so their own `region` is always null and the check would silently pass. For displaced leaves, evaluate the predicate at the undisplaced surface point and normal for the hit's (u,v), via the object's own sample/interpolate. Make T039 green.
- [ ] T051 [US1] **Raytrace exit check**: the raytrace variants of T034, T035 and T036, plus T039 and T041, are green, and the raytrace guard legs of T037/T038 are green again; T038's raytrace `--projected-curve` leg and closedness checks turn green. REYES and Z-buffer variants, including the REYES legs of T037/T038, are expected red until T055.
- [ ] T052 [US1] Add the generic coverage mask (prerequisite: T034–T036 red):
  - a nullable `CRasterGrid::coverage` field in `src/ri/hiders/reyes.h` (null ⇒ every micropolygon present), allocated only by T054's call site when a region is set, and freed with the grid
  - in `src/ri/hiders/reyes.cpp`, where per-quad `bounds` are computed in the quad branch of `insertGrid()` (~L1705; the `RASTER_POINT` branch around ~L1380–1460 handles point grids and is untouched), give a quad marked absent the bound `{INT_MAX, INT_MIN, INT_MAX, INT_MIN}`, so the existing first trivial reject (`bounds[1] < left`, stochasticQuad.h:703 and zbufferQuad.h:40) skips it. Write the same value into each of that quad's per-time-stratum entries in `grid->quadStratumBounds` (computed at ~L1715): the motion-blurred xtreme path culls by those entries alone (`qsb[1]`, stochasticQuad.h:518–534), never by `bounds[]`. Together this needs **no rasterizer loop changes**

  Use generic names only (contract §0).
- [ ] T053 [US1] Add the REYES region stage to `CPatch::dice()` in `src/ri/geometry/surface.cpp:142` (~L145–362), parallel to and combinable with the trim stage (both must pass; trim behavior unchanged):
  - `object->region->classifyBound()` on the bound of the **undisplaced** probe points: sample them with the object's own `sample()` from a snapshot of the probe u/v (as the trim stage snapshots `trimU`/`trimV`, surface.cpp:172), and pad the bound by the largest distance between neighbouring probe points. `ALL_OUT` returns; `ALL_IN` dices normally.
  - `MIXED` dices at the **seam rate**: micropolygon edge ≤ min(√ShadingRate, 1) pixel. It splits, incrementing `numCsgSeamSplits`, only while the patch at the seam rate would exceed the grid-size limit; `CRenderer::maxEyeSplits` does not apply.
  - at the safety cap of 24 split levels (about float-mantissa precision), **keep** the patch

  Make T040 green.
- [ ] T054 [US1] Fill the coverage mask from **undisplaced** P through a generic geometry hook. The P-only grid P is already displaced and in sample space (`displace()`, then `camera2samples()`), so the mask cannot be computed from it.
  - add `virtual void CObject::regionCoverage(CShadingContext *ctx, int udiv, int vdiv, unsigned char *coverage)` (default no-op) in `src/ri/geometry/object.{h,cpp}`, and its `CSurface` override in the renderer-only `src/ri/render/geometryDispatch.cpp`, with a no-op stub in `src/ri/vector/geometryDispatchStub.cpp` (`surface.cpp` is excluded from the shared ribVector library, src/ri/CMakeLists.txt:370, so an override there would leave the ribVector vtable unresolved). The override snapshots the grid's u/v/time, calls the object's own `sample()` with `PARAMETER_P | PARAMETER_NG` for undisplaced camera-space P and Ng, evaluates `region->accepts(ctx, P, Ng)` per vertex, mark a quad absent iff none of its four vertices is accepted, then restore u/v/time
  - in `CReyes::shadeGrid()`'s P-only branch (`src/ri/hiders/reyes.cpp:748`, ~L955, reached from `drawGrid()` at :695 via `shadeGrid(nGrid, TRUE)`), when `object->region` is set, allocate `grid->coverage` and call `object->regionCoverage(this, udiv, vdiv, grid->coverage)` **before** `displace()`. Motion-blurred grids use the shutter-open sample. The mask is computed once and kept with the grid through its later full shading.

  Generic names only; the quickstart §7 grep gate must still return no matches.
- [ ] T055 [US1] **REYES / Z-buffer exit check**: all T034–T041 tests are green in every hider, including T038's REYES `--projected-curve` leg. Verify the T053 seam rule holds the 1 px tolerance; fix defects in `src/ri/geometry/surface.cpp` rather than loosening any tolerance. Save 4×-zoom crops of each seam to the scratchpad as supporting evidence.
- [ ] T056 [US1] Implement implied-closure caps in `src/ri/geometry/csgCaps.{h,cpp}` (research D7, FR-021). For operands resolved as single-quadric, synthesize planar `CBilinearPatch` caps that generously cover each open cross-section (end circles at z-min/z-max or height ends, including a paraboloid's bottom when zmin > 0, z-cut circles, the two sweep-gap half-planes, and a partial-`thetamax` torus's circular-segment sweep gaps), carrying the operand's attributes and transform. A partial-phi torus is closed by a `CHyperboloid` from (R + r·cosφmin, 0, r·sinφmin) to (R + r·cosφmax, 0, r·sinφmax) over the torus's own `thetamax`. Each cap's region is "inside the quadric's cross-section" ∧ the boundary rule. Wire them into `CCSGInstanceState` in `src/ri/geometry/csgSolid.cpp` (`caps[op][]`, included in `children`). Make T042 green, including its `CsgCaps_*` render checks. Then register `csg-open-quadric-caps-{raytrace,reyes,zbuffer}` as `add_visual_test` scenes in `tests/visual/CMakeLists.txt`; generate their references only after visual confirmation (4× crops), and record the approval in `specs/021-deferred-csg-evaluation/reference-regeneration.md`.
- [ ] T057 [US1] Review all 20 registered existing CSG visual scenes one at a time, and regenerate the reference only for those **whose output changed** (SC-005). Save before/after 4× seam crops and record the comparison and reason in `specs/021-deferred-csg-evaluation/reference-regeneration.md`. A changed render is regenerated only when it is a seam improvement or a result FR-021/FR-025a requires; anything else is a bug to fix, never a reference to regenerate. Expected FR-021 case: `csg-nurbs-sphere-union-raytrace`, whose NURBS operand is a single open sheet and now contributes nothing; also rewrite its header comment to say so. The list:
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

- [ ] T058 [US2] (verification) Add cross-hider parity registrations in `tests/visual/CMakeLists.txt` (label `parity`, thresholds from `tests/visual/parity-thresholds.md`), keeping any pair already registered instead of duplicating it. For `blobby-csg-difference`, keep the existing reyes↔raytrace and reyes↔zbuffer pairs (~L2560–2566) instead of adding raytrace↔zbuffer:
  - **three-hider scenes**, pairs raytrace↔reyes and raytrace↔zbuffer: `csg-sphere-cube-{union,difference,intersection}`, `csg-sphere-cube-difference-*-{800,1600}`, `blobby-csg-difference`, `csg-hero-*-{800,1600}`, `csg-coincident-{union,difference}`, `csg-open-quadric-caps`
  - **two-hider scenes**, pair raytrace↔reyes: `csg-hero-two-shaders`, `csg-hero-displaced`, `csg-hero-translucent`, `csg-secondary-rays`, `csg-polymesh-box-difference`, `csg-nurbs-rational-sphere-union`, `csg-subdivision-sphere-union`

  The translucent pair additionally runs `--enclosed-holes --opaque 0.45 --exclude disc:<hole>`; the displaced pair runs `--enclosed-holes --exclude disc:<hole>` (the hero's through-hole is a legitimate opening). Both are via `add_csg_metric_test`.
- [ ] T059 [US2] (verification) Register `CsgSecondaryRays_{raytrace,reyes}` via `add_csg_metric_test` for `csg-secondary-rays-{raytrace,reyes}.rib`, using `--luminance-ratio` with the regions recorded in each scene header, because alpha cannot tell lit, shadowed and reflected pixels apart (FR-013):
  - shadow-hole spot ÷ open lit ground ≥ 0.8 (the hole lets light through the shadow)
  - shadow body ÷ open lit ground ≤ 0.5
  - reflected hole disc ÷ reflected sphere ring ≤ 0.3 (the background shows through the reflected hole)
- [ ] T060 [US2] (verification) Register `CsgUntouchedRegion_hero`: render the hero scene and the same scene with a plain `Sphere`, no solid block (add `examples/rib/tests/csg-hero-plain-sphere-raytrace.rib`), and compare at 800×500 with `--region-compare --exclude disc:400,250,64` (everything inside the near seam ring, r = 61.7 px, plus a 2 px margin), so only the untouched sphere surface is compared (SC-008).

**Checkpoint (red tests — maintainer approval)**: stage the new and rewritten tests with their confirmed-red evidence, and stop for the maintainer to approve the tests before any implementation task below starts (constitution III: Tests written → User approved → Tests fail → Then implement).

### Implementation for User Story 2

- [ ] T061 [US2] T058–T060 are verification of US1's work and may pass when written. If any fails, that failure is the red test for this task: investigate and fix every divergence surfaced by T058–T060, inside the geometry domain only: for example, displaced-leaf evaluation in `src/ri/geometry/surface.cpp`, seam-threshold or ε calibration in `src/ri/geometry/csgClassify.cpp`, cap orientation in `src/ri/geometry/csgCaps.cpp`. No hider or shading edits beyond T052/T054. Re-run until green.

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
  - a `SolidBegin` emitted during expansion reports exactly one nested-in-primitive diagnostic, its contents are absent from the operand, its `SolidEnd` resolves nothing, and no extra `CCSGSolid` is created
  - an empty expansion follows FR-025a
  - the T009 open-cylinder archive (a single open quadric) gives an operand resolved as single-quadric, with implied closure and caps
  - expanded objects carry `ATTRIBUTES_FLAGS_SOLID_FRAGMENT` and the leaf's `ATTRIBUTES_FLAGS_INSIDE` parity
  - `numCsgProceduralExpansions` counts correctly

  Register it in `tests/unit/csg/CMakeLists.txt`.
- [ ] T063 [US3] Rename `examples/rib/tests/csg-malformed-procedural-in-primitive.rib` to `examples/rib/tests/csg-procedural-missing-archive.rib` and rewrite its header comment: the procedural (still pointing at `nonexistent.rib`) is **accepted at capture** (FR-024), expansion reports exactly one archive-open diagnostic, and the procedural contributes nothing, so the operand is the block's `Sphere 1 -1 1 360` and **the render shows that sphere**. Update its T013 registration: `LOG_PASS` on the archive diagnostic, `LOG_FAIL` on the old "cannot be declared inside a SolidBegin block" text, and `--enclosed-holes` plus non-empty coverage. Red until T066.
- [ ] T064 [US3] Register `CsgProcedural_sphere` and `CsgProcedural_open_cylinder` via `add_csg_metric_test` with `COPY examples/rib/tests/archives`. Each renders the procedural scene and its inline twin and compares them with `--region-compare --include frame` (SC-010).
- [ ] T065 [US3] Register `CsgToleranceInert` via `add_csg_metric_test`: `LOG_PASS` on exactly one tessellation-tolerance notice (`LOG_FAIL` on a second), plus `--region-compare --include frame` against the hero raytrace render (FR-023).

**Checkpoint (red tests — maintainer approval)**: stage the new and rewritten tests with their confirmed-red evidence, and stop for the maintainer to approve the tests before any implementation task below starts (constitution III: Tests written → User approved → Tests fail → Then implement).

### Implementation for User Story 3

- [ ] T066 [US3] Support procedural operands (research D8):
  - remove the whole `if (currentSolid != NULL) { csgValidateProceduralCapture(currentSolid); return; }` branch in `RiProcedural()` (`src/ri/render/rendererContext.cpp:5200`), so `addObject()` routes the `CDelayedObject` into `leafObjects` of any open solid block. A procedural placed directly in a boolean block therefore joins that block's first operand, like any direct leaf (FR-024), and delete the function's definition (`src/ri/geometry/csgTree.cpp:90`)
  - add a capture-aware helper `CRendererContext::expandIntoOperand(CShadingContext *, CDelayedObject *, CObject *holder, CAttributes *flagged)` in `src/ri/render/rendererContext.{h,cpp}`. `processDelayedObject()` (rendererContext.cpp:421) cannot be reused, because it targets the delayed object's own children, resets `delayed = NULL` and calls `setChildren()` before any region is set. The helper:
    - saves `delayed`, `currentAttributes` and `currentXform`
    - sets `delayed = holder`, the current attributes to a copy of `flagged` (so every expanded object inherits `ATTRIBUTES_FLAGS_SOLID_FRAGMENT` and the leaf's `ATTRIBUTES_FLAGS_INSIDE` parity) and the current xform to the delayed object's xform
    - runs the subdivision function, then restores all three to their **saved** values, never `NULL`
    - does not call `setChildren()`
  - in `src/ri/geometry/csgSolid.cpp`, during `CCSGInstanceState` resolution under `delayedMutex` (after T046 step 2, which instantiates each template `CDelayedObject` (`delayed.h:68`) into the holder with the composed xform, and **before** step 3): loop over the operand's members, replacing each `CDelayedObject` with its expansion, until none remains. Nested procedurals are handled by the same loop, so steps 3–6 only ever see real geometry; T046 step 0's filter also runs over expanded members.
  - guard nested blocks during expansion with an `expandingOperand` flag, set by `expandIntoOperand()` for the duration of the expansion and saved/restored around it, and a counter `swallowedSolidDepth`, both in `CRendererContext` (`rendererContext.{h,cpp}`). A `SolidBegin` inside an ordinary procedural (not an operand expansion) is legal and is not swallowed:
    - while `expandingOperand` is set, `RiSolidBegin` still validates the operation name
    - at the first nesting level it reports the nested-in-primitive message through a new parent-less helper `csgReportNestedSolidBegin()` in `src/ri/geometry/csgTree.cpp`, which `csgValidateNestedSolidBegin()` now calls
    - it then increments the counter and creates no node
    - `addObject()` discards objects while the counter is above 0
    - `RiSolidEnd` only decrements the counter, so it never resolves anything
  - an expansion that yields nothing is an empty operand (FR-025a); T031's always-OUT rule covers it with no extra code
  - increment `numCsgProceduralExpansions`

  Make T062–T064 green.
- [ ] T067 [US3] Make `tessellationtolerance` inert (research D10):
  - remove every remaining read of `CAttributes::tessellationTolerance`
  - keep parsing in all four attribute layers
  - in `RiAttributeV()` (`src/ri/render/rendererContext.cpp`), emit the notice with `warning(CODE_BADTOKEN, …)` (always printed, to stdout; never `CODE_LOG`) on the first occurrence per frame, using a per-frame flag reset at `RiWorldBegin`
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

- [ ] T071 [US4] Register `CsgOffscreenStats_{raytrace,reyes}` via `add_csg_metric_test` `STATS` (with `COPY examples/rib/tests/archives`) for `csg-offscreen-stats-{raytrace,reyes}.rib`: `CSG instances resolved:`, `CSG classifications:`, `CSG seam splits:` and `CSG procedural expansions:` are all `== 0` (SC-006).
- [ ] T072 [US4] Register `CsgPartialVisibilityStats` via `STATS` ratio: `CSG seam splits:` of `csg-hero-partial-stats-reyes.rib` must be ≤ 0.75 × that of `csg-hero-stats-800-reyes.rib` (US4 scenario 2, FR-004).
- [ ] T073 [US4] Register `CsgResolutionScaling` via `STATS`: `CSG seam splits:` of `csg-hero-stats-1600-reyes.rib` must be > that of `csg-hero-stats-800-reyes.rib` (FR-003 proxy).
- [ ] T074 [US4] Register `CsgTinySolidStats` via `STATS`: `CSG seam splits:` for `csg-tiny-solid-stats-reyes.rib` must be `<= 16` (spec edge case, very small solids).

**Checkpoint (red tests — maintainer approval)**: stage the new and rewritten tests with their confirmed-red evidence, and stop for the maintainer to approve the tests before any implementation task below starts (constitution III: Tests written → User approved → Tests fail → Then implement).

### Implementation for User Story 4

- [ ] T075 [US4] Remove any eager work found by T071–T074 in `src/ri/geometry/csgSolid.cpp`, `src/ri/geometry/csgDefinition.cpp`, `src/ri/geometry/csgTree.cpp` and the seam rate in `src/ri/geometry/surface.cpp`. For example, compute the solid's bound from the template without instantiating, never touch `CCSGInstanceState` from culling or bound queries, and stop seam splitting at pixel precision.

**Checkpoint (Phase 6)**: laziness and view-driven refinement proven. Stage and pause for commit.

---

## Phase 7: Polish & Cross-Cutting Concerns

- [ ] T076 Delete `src/ri/geometry/csgBoolean.{h,cpp}`, `src/ri/geometry/solidObject.{h,cpp}` and `CRendererContext::processDelayedSolid` (`src/ri/render/rendererContext.{h,cpp}`). Remove them from `src/ri/CMakeLists.txt`: `geometry/csgBoolean.cpp` at L62 (the shared `ri_sources` list, so it leaves both ribRender and ribVector), and `geometry/solidObject.cpp` at L126 and in the ribVector exclusion list at L369. Remove the related libribpreview comment text (~L295–300) and the names `solidObject`, `csgBoolean` and `csgTesselationDispatch` from the header comment at L13–16. `geometry/csgTree.cpp` (L344) stays.
- [ ] T077 For each spec 013 tessellation helper (`tesselateQuadricAdaptive`, `tesselatePatchMeshAdaptive`, `tesselateNURBSPatchMeshAdaptive`, `csgTessellatePolygonMeshOperand`, `tessellateToSurfaces`), grep for remaining non-CSG callers and delete every helper left unused. In the same pass:
  - delete `src/ri/geometry/csgTesselationDispatch.cpp` and `src/ri/vector/csgTesselationDispatchStub.cpp`, with their CMake entries (L63, L343, L373)
  - remove the friend declaration at `src/ri/geometry/polygons.h:73`
  - retire `tests/unit/csg/test_quadric_tesselation.cpp`, `test_patchmesh_tesselation.cpp` and `test_tesselation_flatness.cpp` with their registrations, keeping any whose helper survives
- [ ] T078 [P] Update `docs/site/content/manual/reference/solid-csg-operations.md` (Hugo, constitution VII) to cover:
  - deferred evaluation and seam quality
  - open single-quadric operands and implied caps, including the cone-shaped cap of a torus cut across its tube
  - closedness rules and the "not closed" diagnostic
  - multi-surface operands closed by the author
  - overlapping shapes, and direct leaves of a boolean block, combining by parity
  - procedural operands
  - empty operands as the empty set (FR-025a)
  - the inert tolerance attribute and its notice
  - the CSG statistics section (SC-009), including that the counters need `orender -x 3` (or `ORENDER_INSTR_LEVEL=info`), because `orender` defaults to log level NONE
  - single-operand solids are a passthrough (no implied caps), and implicit, DSO (`"dlo"`), point, curve and object-instance primitives are not supported as solid operands
- [ ] T079 [P] Update `DEVNOTES.md` (status table entry for spec 021, superseding 013's resolution) and `DEVNOTES_DETAILS/RISPEC_GAPS.md` (CSG section).
- [ ] T080 [P] Update the `CLAUDE.md` "Known gotchas":
  - CSG is spatial trimming via the generic region hook, inherited through `setChildren`
  - the hook must keep generic names because `reyes.cpp` references it
  - classification bypasses ray filters and uses orientation-independent parity
  - free-form crossings go through the renderer-only `surfaceCrossingDispatch`/stub split
  - trims still drop straddlers, while CSG keeps them
  - the REYES coverage mask comes from undisplaced P via the generic `CObject::regionCoverage()` hook, called before `displace()`, because the grid's own P is displaced and in sample space
  - `instantiate()` goes through `addObject()`, so per-instance leaves need a capture target (`delayed`), and procedural expansion uses `expandIntoOperand()`, not `processDelayedObject()`
  - captured leaves carry absolute transforms, so a definition rebases them into the outer frame (former `csgRebaseFragments`)
  - render-only `CSurface` overrides such as `regionCoverage` live in `render/geometryDispatch.cpp` with a stub in `vector/geometryDispatchStub.cpp`, because `surface.cpp` is not in the shared ribVector library

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
  - T038 `CsgClosedness_nurbs_open`, `_nurbs_instanced`, `_polymesh_open` → T047
  - T038 `--projected-curve` legs → raytrace: T047/T048/T050 (confirmed at T051); REYES: T053/T054 (confirmed at T055)
  - T043 and T045 → T047
  - T044 → T046
  - T040's instanced-outside-world leg → T046/T047
  - T039 → T050
  - T034–T036 → T048–T054 (bilinear-patch acceptance in T049)
  - T040 → T053
  - T041 → T046/T047 plus T048–T050 (confirmed at T051)
  - T042 → T056 (raytrace caps also need T049)
  - T062–T065 → T066/T067
- Guard and verification tasks (may pass when written): T013, `CsgClosedness_nurbs_closed` (T038), T037, T038 (except its `--projected-curve` and open-operand closedness legs), T058, T059, T060, T068–T074, and any T034 leg recorded as a guard in T014.
- Phase 2 order: T025 → T026 → (T027, T028, T029) → T030 → T031 → T032. T028 and T029 follow T026 because they share files.
- US1 order: T034–T045 → T046 → T047 → (T048, T049, T050) → T051 → T052 → T053 → T054 → T055 → T056 → T057.
- T047 to T057 is the expected-red window for existing reference-image tests; T047 to T055 also covers the REYES legs of T037/T038.
- The T033 Phase 2 gate (`ctest -L csg`) contains only tests that are green in Phase 2: the unit suites and the T013 guards. Every render-side check that starts red is registered in Phase 3 or later.
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
