# Research: Deferred, On-Demand Evaluation of Solid (CSG) Geometry

**Feature**: 021-deferred-csg-evaluation | **Date**: 2026-09-30 | **Spec**: [spec.md](spec.md)

All findings below were gathered by reading the code and rendering on `master`
at `6680e71`. File references are project-relative.

## Baseline: what exists and why it produces the reported defects

**Current pipeline (spec 013)** — resolution is eager, at the outermost `RiSolidEnd`:

1. `CRendererContext::addObject()` (`src/ri/render/rendererContext.cpp`, the
   `currentSolid != NULL` branch) diverts every primitive declared inside an open
   solid block into `currentSolid->leafObjects`.
2. The outermost `RiSolidEnd` calls `resolveCSGTree()` (`rendererContext.cpp:5826`
   → `src/ri/geometry/csgTree.cpp:728`).
3. `csgTessellateOperand()` tessellates each leaf with a camera-blind tolerance
   (`kCsgDefaultToleranceFraction = 0.001` of the leaf's bound diagonal, or
   `Attribute "solid" "tessellationtolerance"`), dispatching per type:
   `CSurface` → `tesselateQuadricAdaptive`, `CPatchMesh`, `CNURBSPatchMesh`,
   `CPolygonMesh` → `csgTessellatePolygonMeshOperand` (`polygons.cpp`),
   `CLoopSubdivMesh` → `buildPolygonMesh`, `CSubdivMesh` → `tessellateToSurfaces`.
4. `csgCombine()` (`src/ri/geometry/csgBoolean.{h,cpp}`) runs a float BSP boolean
   (Laidlaw–Trumbore–Hughes / Naylor) with a plane epsilon.
5. `csgPolygonsToFragments()` / `csgBuildMeshForAttributeGroup()` emit one
   `CPolygonMesh` per distinct `CAttributes*` with **unwelded** per-triangle
   vertices, cloning attributes and setting `ATTRIBUTES_FLAGS_SOLID_FRAGMENT`.
6. `CSolidObject` (`src/ri/geometry/solidObject.{h,cpp}`) holds the fragments and
   instantiates them lazily on first `dice()`/`intersect()` via
   `processDelayedSolid()` (`rendererContext.cpp:485`) under
   `CRenderer::delayedMutex`. It is deliberately **not** `OBJECT_DUMMY`, because
   `CShadingContext::trace()` (`src/ri/hiders/trace.cpp:332`) skips `intersect()`
   on dummy objects.

**Measured defects (Step 0)** — hero scene (sphere r=1.5 minus open `Cylinder 0.65
-2 2 360`, camera looking down the hole):

- About 16 triangular spikes on the near seam ring.
- A thin crack arc inside the far opening. Opaque pixels inside the analytically
  computed clear through-hole disc:

  | Hider | 800×500 | 1600×1000 |
  |---|---|---|
  | Raytrace | 9 | 213 |
  | REYES | 0 | 63 |
  | Z-buffer | 0 | 38 |

- Nine see-through gap pixels in the Z-buffer render at 1600×1000.
- Polygonal openings.

The stock `csg-sphere-cube-difference` scene shows irregular polyline seams with
spurs. All three hiders produce identical defects because they render one
pre-baked mesh. Spec 013's own research carried "BSP float robustness at coplanar
faces / slivers" as a known risk; the spurs and slivers match it.

**Root causes**:

- Seam precision is fixed before any view exists (the tolerance is relative to
  the object's size, not its screen size).
- Seam vertices are computed independently on each side of the seam, in float.
- Epsilon classification drops slivers.
- T-junctions and unwelded vertices come out of BSP splitting.

---

## Decision 1: Resolution is geometry-owned and on demand (revisits spec 013 Decision 1)

**Decision**: A solid stays its CSG tree over the original captured primitives
for the whole frame. The composite surface is resolved only when, and only where,
the rendering pipeline asks the solid for geometry, through the same generic
`CObject` interface every primitive already implements:

- REYES and Z-buffer: `dice()`, driven by buckets.
- Raytrace: `intersect()`, driven by rays.

**Rationale**:

- Spec 013 rejected "resolve lazily inside `dice()`/`intersect()`" because it read
  that as per-hider resolution. That reading conflated *who triggers* resolution
  with *who performs* it.
- Every quadric, patch and NURBS surface is already resolved lazily by exactly
  these calls: nobody pre-tessellates a sphere. The resolution logic lives
  entirely in the geometry domain, and the hiders stay generic.
- `CSolidObject` itself already defers its work to first
  `dice()`/`intersect()`. This decision pushes the *whole* resolution into that
  deferred path, instead of only the fragment instantiation.

**Alternatives considered**:

- *Keep eager resolution, just raise the tessellation density*: rejected.
  Precision would still be view-independent, the BSP robustness issues remain, and
  memory and time grow for off-screen solids (violates FR-001 to FR-004).
- *Run the existing BSP kernel lazily, per bucket*: rejected.
  - It keeps every seam defect: independent float seam vertices, slivers,
    T-junctions.
  - It adds cross-bucket inconsistency, because each bucket would tessellate its
    region on its own, which breaks FR-008.

## Decision 2: Representation — CSG as spatial trimming, not mesh booleans

**Decision**: Each leaf primitive renders as itself, at its own native,
view-dependent precision, restricted to the part of it that lies on the
composite boundary.

- That restriction is a geometry-owned **region predicate**: "does surface point
  P of leaf L belong to the composite boundary?"
- The predicate is attached to each leaf surface.
- It is consulted wherever the existing pipeline already consults NURBS trim
  curves (spec 009's `CSurface::trimAccepts()`/`hasTrim()`, `object.h:139-144`).
- No composite mesh is ever built.

**Rationale**:

- The seam is where two leaves' predicates flip. Each side is trimmed by an
  exact (or native-precision) containment test against the *other* operand. Both
  sides therefore converge on the same true intersection curve as resolution
  increases.
- Remaining error shrinks with pixel size (FR-007, FR-009) instead of being frozen
  at a tessellation tolerance.
- Per-operand shaders (FR-016) are free: a surviving surface piece *is* the
  operand's own surface, with its own attributes.
- The trim precedent proves the hook shape works in both REYES dicing
  (`surface.cpp:324-362`) and raytrace hit acceptance (`surface.cpp:857`,
  `:1024`), with no trim-specific code in any hider.

**Alternatives considered**:

- *Per-sample CSG in the hider*: the PRMan-classic approach, where the hider keeps
  a depth-sorted surface list per sample and evaluates CSG along the view ray.
  Rejected: it puts boolean logic in the visibility domain, violating FR-012.
- *Implicit / signed-distance-field CSG plus polygonization*: rejected. It
  destroys sharp edges and flat faces, and imposes a sampling resolution
  unrelated to the operands.
- *Exact boundary-representation surface–surface intersection curves feeding
  per-type trimming*: rejected. It needs an intersection solver for every pair of
  primitive types, which is disproportionate for v1.

## Decision 3: Point classification — "is P inside operand O?"

**Decision**: Each operand (the leaves of one `"primitive"` block, or the
directly-captured leaves of a boolean block) answers `classify(P) → IN / OUT`:

1. **Bound rejection first**: if P lies outside the operand's bound, the answer is
   OUT, with no further work.
2. **Single-quadric operands**: analytic implied-closed-solid test (see
   Decision 7). Sphere and torus are closed on their own. Cylinder, cone,
   paraboloid and hyperboloid are capped at their parametric ends; partial
   sweeps and z-cuts are closed by planes. All of this is exact, in the quadric's
   object space.
3. **Otherwise**: an **unsigned crossing count (parity)** of a ray from P
   against all of the operand's surfaces. An odd count means IN.
   - Parity ignores surface orientation. Author-closed shells built from
     inconsistently oriented pieces therefore classify correctly: for example, six
     bilinear patches in arbitrary vertex order, or one patch under
     `ReverseOrientation`.
   - **Consequence**: overlapping closed shapes inside one `"primitive"` block
     combine by parity. Where they overlap, the region is enclosed twice and counts
     as outside. Authors combine overlapping shapes with a `"union"` block, which
     is what blocks are for. Spec 013 never defined this case (its FR-002 only says
     a block is "a single operand"), and its BSP kernel produced undefined output
     for overlapping shells, so no defined behavior regresses. The spec states it
     as an edge case.
   - This uses a new per-surface capability, `countCrossings(from, dir)`, which
     returns the **unsigned** number of times the ray crosses the undisplaced
     surface. It is implemented:
     - analytically for quadrics (every root in the u/v range, not just the
       nearest — `CSphere::intersect()` in `quadrics.cpp:167` already iterates
       roots in order)
     - exactly for flat polygons (`CPolygonTriangle`/`CPolygonQuad`)
     - analytically for bilinear patches
     - for free-form surfaces (bicubic, patch meshes, NURBS, subdivision), by
       ray–patch intersection seeded from the surface's tessellation and
       Newton-refined to float precision, so seam accuracy is at least what the
       surface renders with on its own (FR-007a).
   - Classification rays bypass every rendering-side ray filter: visibility
     attribute flags, LOD, backface culling and displacement tessellation
     (`CSurface::checkRayGuard()`, `src/ri/render/geometryDispatch.cpp:239`). They
     are a geometry query, not a render ray.
   - **Robustness**: the ray direction is a fixed, deterministic,
     axis-irrational direction. If a crossing is near-grazing
     (|N·dir| < ε) or within ε of a surface edge, the query is recast with a second
     fixed direction, and the majority of three decides. The result is a pure
     function of P, which guarantees seam continuity across buckets and threads
     (FR-008).

**Rationale**:

- Analytic tests are exact and cheap for the common primitives. Crossing parity
  covers every surface type uniformly, including user-closed multi-surface
  operands (a can built from a cylinder and two disks, a box from six bilinear
  patches, a polygonized blobby), and it does not depend on how the author
  oriented each piece.
- Bypassing ray filters keeps classification independent of shading and
  visibility attributes (separation of concerns).

**Alternatives considered**:

- *Reuse `CObject::intersect()` with a flag and repeatedly advance `tmin`*:
  rejected. `intersect()` updates `ray->t`/`ray->object` state and is entangled
  with attribute flags, LOD and backface rejection. A dedicated read-only
  crossing query is simpler and faster.
- *Classify against a cached coarse tessellation*: rejected. It re-introduces a
  precision mismatch at the seam: the trimmed side would follow the polygon, the
  other side the true surface.
- *Signed winding number* (signs from surface normals; non-zero ⇒ IN): rejected.
  It requires every surface's orientation to be consistent, and RIB authoring
  does not guarantee that. A box with one reversed patch, or with mixed vertex
  orders, would cancel to zero and classify inside points as outside. Its one
  advantage, merging overlapping shells within a single block, is already served
  by `"union"` blocks.

## Decision 4: Boundary rule, orientation, coincident faces

**Decision**:

- **Boundary rule**: a point P on leaf L is on the composite boundary iff the CSG
  tree evaluated with L's operand forced IN differs from the tree evaluated with
  it forced OUT, with every other operand classified at P.
  - Evaluation short-circuits: operands whose value cannot change the result are
    never classified.
- **Orientation**: static per leaf. A leaf is *flipped* iff its operand sits at an
  odd number of subtrahend positions (non-first operands of a `difference`) on its
  path to the root.
  - The flip is delivered as geometry: the leaf's cloned attributes get
    `ATTRIBUTES_FLAGS_INSIDE` toggled.
  - Every existing sampler and intersector already orients normals by
    `(ATTRIBUTES_FLAGS_INSIDE ^ xform->flip)`, as seen in `CSphere::intersect()`.
    So shading and backface handling see an ordinary outward-facing surface
    (FR-011) with zero shading-side changes.
- **Coincident faces (FR-022)**: when a classification is within ε of an operand
  boundary (ON), classify at P ± ε·N instead, and use the two-sided rule: P is on
  the boundary iff the composite membership differs on the two sides, and it faces
  the OUT side.
  - ε is deterministic and scene-relative: `C_EPSILON` scaled by the solid's
    bound size.
  - This removes internal skins (union of flush boxes) and leftover skins
    (difference flush with a face) without flicker.

**Rationale**: for union/intersection/difference trees in which every operand
appears once, the composite is monotone in each operand, so orientation is a
static property. The pivotal-operand test is exact and handles arbitrary
nesting.

**Alternatives considered**: *orientation by signed volume per result* — this is
spec 013's approach, and it is meaningless without a mesh.

## Decision 5: REYES / Z-buffer integration

**Decision**:

- The solid's `dice()` hands each leaf (and any caps) to the normal
  split/dice path, with the region predicate attached.
- The `CSurface::dice()` probe loop (`surface.cpp:145-362`) gains a region stage,
  parallel to (and combinable with) the trim stage.
- **Uniform-patch shortcut**: if a (sub-)patch's probe bound does not overlap the
  bound of any other operand's boundary surfaces, its classification is uniform.
  One predicate evaluation keeps or drops the whole patch, with no per-vertex
  work.
- **Straddling patches** keep splitting. Termination is **screen-size-based**:
  split until the sub-patch dices into micropolygons no larger than a seam
  threshold, rather than stopping at `CRenderer::maxEyeSplits`.
  - The trim path *drops* straddlers at the eyesplit cap; for CSG that drop would
    itself create exactly the reported gaps.
  - A generous safety depth cap exists; hitting it *keeps* the patch instead of
    dropping it.
- **Final grids** carry a **per-micropolygon coverage mask**, computed from the
  P-only sampling that `CReyes::drawGrid()` (`src/ri/hiders/reyes.cpp:695`) already
  performs (`shadeGrid(grid, TRUE)`).
  - A quad is absent iff none of its four vertices is accepted. This is
    conservative: straddling quads are kept, which yields sub-pixel overlap,
    never a gap.
  - The rasterizers already skip any quad whose per-quad `bounds` miss the bucket
    (`stochasticQuad.h:703`, `zbufferQuad.h:40`). An absent quad gets an empty
    bound where the per-quad bounds are computed (`reyes.cpp` ~1452/1705), so
    **no rasterizer loop changes**.
  - The mask is a generic `CRasterGrid` field ("geometry says this micropolygon
    does not exist"), with no CSG knowledge in the hider (FR-012).
- Shading stays lazy, as today (`RASTER_UNSHADED`): only surviving grids reach the
  shader, and only when a bucket needs them.

**Rationale**:

- Bucket-driven splitting already defers work to the buckets that need it
  (FR-002, FR-004).
- The mask reuses existing culling.
- The seam threshold makes seam error track screen resolution (FR-007, FR-009).

**Alternatives considered**:

- *Reuse the trim path verbatim*: rejected, because straddlers get dropped.
- *Stitch or weld grids along seams*: rejected. It needs cross-grid topology,
  which REYES does not have.

## Decision 6: Raytrace integration

**Decision**:

- The solid is an ordinary BVH node with a CSG-aware bound, and it is not
  `OBJECT_DUMMY` (`trace.cpp:332`).
- On first visit it populates `children` with its leaves (plus caps).
- Each leaf applies the region predicate at every point where it would *accept*
  a hit:
  - the analytic quadric root loops (`quadrics.cpp`)
  - polygon intersectors (`polygons.cpp`)
  - the `CTesselationPatch` candidate test at `surface.cpp:857` and `:1024`, next
    to `trimAccepts`
- A rejected candidate simply does not update `ray->t`, so the same leaf's next
  root, and the rest of the traversal, proceed naturally.
- Secondary rays (shadow, reflection, refraction, occlusion, transmission,
  photon) use the same path (FR-013).
- **Displaced leaves**: the predicate is evaluated at the *undisplaced* surface
  point for the hit's (u,v), so the solid's shape is defined by undisplaced
  operands (spec edge case).

**Rationale**: this is the same shape as trim hit rejection, which is already
proven. It is exact for quadrics and polygons.

## Decision 7: Implied closure of open operands (FR-021)

**Decision**:

- Implied closure is defined for operands consisting of **exactly one quadric**:
  the hero scene's case, and the common authoring pattern.
- **Classification**: the quadric's implied solid (Decision 3.2).
- **Rendering**: the solid synthesizes **cap surfaces** for that operand:
  - planar bilinear patches that generously cover each open cross-section: end
    circles at z-min/z-max or height ends, and the two half-planes of a partial
    sweep
  - each carries the operand's attributes and transform
  - each is trimmed by the quadric's own implied-solid cross-section predicate,
    so only the true cap shape survives
- Caps then go through the boundary rule like any leaf, so a cap appears only
  where it lies on the composite boundary. In the hero scene, the tube's caps lie
  outside the sphere and never render.
- **Operands with several surfaces** are classified by the jointly-enclosed
  volume (crossing parity, Decision 3) with **no** auto-caps. The author closes them, exactly
  as spec 013 requires. Auto-capping them would double-count author-provided caps,
  e.g. a cylinder plus two disks.
- **A lone zero-volume surface** (a single disk, a single open bilinear patch) is
  an empty operand.
- **A lone open free-form surface** (a single open NURBS sheet) has no
  well-defined implied solid. It is classified by crossing parity, and a one-time
  diagnostic says the operand is not closed.

**Spec alignment**: confirmed by the maintainer during planning (2026-09-30).
FR-021 was amended to this exact scope, and a Clarifications bullet was added to
the spec.

**Alternatives considered**:

- *Generalized winding number as "implied closure" for any open surface*: it
  gives a classification, but caps cannot be rendered for arbitrary open
  surfaces, so the composite would show holes.
- *Always auto-cap every open quadric*: it double-counts author-closed operands.

## Decision 8: Procedural operands (FR-024/FR-025)

**Decision**:

- The capture-time rejection is removed: `csgValidateProceduralCapture()`
  (`csgTree.cpp:90`) and the `"delayed"` gate in `addObject()`.
- A procedural declared in a solid block becomes an *unexpanded operand member*.
- When a solid instance is first needed (its first `dice()` or `intersect()`), its
  procedural operand members are expanded under `CRenderer::delayedMutex`. This
  mirrors `CDelayedObject::intersect()` and `processDelayedObject()`
  (`src/ri/render/geometryDispatch.cpp:679`).
- **Capture**: `addObject()` gains an *operand capture* mode, diverting the
  expansion's primitives into that operand's member list. Nested procedurals are
  captured and then expanded recursively.
- A `SolidBegin` issued during expansion reports the existing nested-in-primitive
  diagnostic (`csgValidateNestedSolidBegin`) and is ignored.
- An empty expansion yields an empty operand, with FR-025 semantics.
- Each operand is expanded at most once per solid instance. It is never expanded
  if the instance is never needed.
- A counter records expansions (SC-006).

**Rationale**: this reuses the existing delayed-object machinery and locking. An
instance "needed" at its first `dice()`/`intersect()` is exactly FR-024's
trigger.

## Decision 9: Instancing and object lifetime

**Decision**:

- At `RiSolidEnd`, the capture tree becomes an immutable, reference-counted
  **CSG definition**: tree plus template operand members.
- A **CSG solid** object (replacing `CSolidObject`) references the definition,
  with its own transform and attributes. `instantiate()` (for `RiObjectInstance`)
  creates another solid over the same definition with a composed transform, as
  `CSolidObject::instantiate()` does today.
- **Per-instance state** is built on first use, under `delayedMutex`:
  - leaves instantiated with the composed transform, via each leaf's
    `instantiate()`
  - procedural expansions
  - caps
  - per-leaf region adapters
- All geometry is already in camera space (object bounds are transformed at
  construction), so classification queries need no extra transform beyond each
  primitive's own.

## Decision 10: `Attribute "solid" "tessellationtolerance"` (FR-023)

**Decision**:

- Keep the token and the declaration in all four attribute layers:
  1. `ri.h`/`ri.cpp`
  2. `RiAttributeV()` in `rendererContext.cpp`
  3. `CAttributes::tessellationTolerance` (`attributes.h:165`)
  4. `initDeclarations()`
- Stop reading its value.
- The first occurrence per render emits a notice through the error channel at a
  level that is not swallowed. Do not use `CODE_LOG`, which silently drops
  messages; follow the `CODE_BADTOKEN`-level precedent from the hider rename.
- A per-render flag is reset at frame begin.

## Decision 11: Retiring the BSP path

**Decision**: remove from the render path:

- `csgBoolean.{h,cpp}` (`CCSGBSPNode`, `csgCombine`)
- `csgTessellateOperand`, `csgResolveNode`, `csgPolygonsToFragments`,
  `csgBuildMeshForAttributeGroup`, `csgRebaseFragments`
- `CSolidObject` and `processDelayedSolid`

The `csgBoolean` files are deleted outright. The only consumers are
`csgTree.cpp`, `rendererContext.cpp` and `tests/unit/csg/`. The adaptive
tessellation helpers that spec 013 added (`tesselateQuadricAdaptive`,
`tesselatePatchMeshAdaptive`, `tesselateNURBSPatchMeshAdaptive`,
`csgTessellatePolygonMeshOperand`, `tessellateToSurfaces`) are removed if no
non-CSG caller remains; this is checked per helper during implementation. Any
still used elsewhere stay.

## Decision 12: Statistics (SC-006)

**Decision**: add counters to `CStats` (`src/ri/core/stats.h`), reported with the
existing statistics output:

- `numCsgClassifications` (point classifications performed)
- `numCsgSeamSplits` (straddle-driven splits)
- `numCsgInstancesResolved` (solid instances whose per-instance state was built)
- `numCsgProceduralExpansions`

"No refinement work for an off-screen solid" means `numCsgInstancesResolved`,
`numCsgClassifications` and `numCsgProceduralExpansions` stay 0 for it.

## Decision 13: Test strategy (constitution III — TDD)

**Decision**: tests are written first, fail, and then pass.

- **Unit (`tests/unit/csg/`)**: the kernel suites are rewritten as
  classification and boundary-rule suites over the **same scenarios**:
  `test_boolean_boxes`, `_coplanar`, `_sphere_box`, `_sphere_sphere`,
  `test_nested_tree`, `test_multi_primitive_leaf`. They now assert:
  - point classification (IN/OUT) at chosen points
  - the boundary predicate and orientation at points on each leaf
  - coincident-face two-sided behavior
  - operand bounds
  - `csgTestUtils.h` loses its volume-sign helpers and gains analytic point sets.

  `test_interior_exterior_selection` is kept, with its provenance updated.
  `test_quadric_tesselation`, `test_patchmesh_tesselation` and
  `test_tesselation_flatness` test the adaptive-tessellation helpers from
  spec 013. They are retired together with any helper that Decision 11 removes,
  and kept for any helper that survives. New suites cover:
  - analytic implied-closed quadric classification and caps
  - crossing-parity robustness (`test_parity_robustness`): grazing and edge rays,
    mixed vertex orders, a reversed patch, overlapping shells in one block, and the
    "not closed" diagnostic
  - analytic seam location (points straddling the exact intersection curves) and
    tangency, in `test_boundary_rule`
  - determinism: identical classification regardless of query order or thread
  - `countCrossings` per primitive type, including Newton-refined free-form
    agreement with the surface's own tessellation (FR-007a)
  - CSG statistics counters (`test_csg_stats`)
  - procedural operands and lazy resolution
- **Visual**: existing `examples/rib/tests/csg-*.rib` and `blobby-csg-*` scenes
  keep their registrations. References are regenerated deliberately, and only
  where seams improve, with before/after crops recorded. New scenes:
  - the hero scene, in all three hiders at 800×500 and 1600×1000
  - a two-shader hero scene (per-operand shaders, FR-016)
  - an open-operand cap scene (a partial-sweep cylinder intersected, caps
    visible)
  - a coincident-face scene (flush boxes: union and difference)
  - displaced-operand and translucent-operand hero variants
  - REYES twins of the free-form (NURBS, subdivision) union scenes, plus
    free-form-alone references
  - a procedural-operand equivalence scene (`DelayedReadArchive` sphere vs.
    inline sphere), with the archive copied into the test's scratch directory
  - a missing-archive procedural scene (renamed from
    `csg-malformed-procedural-in-primitive.rib`)
  - secondary-ray scenes (raytrace and REYES)
  - off-screen and partial-view statistics scenes
- **Seam metric tool** (`test_csg_seam_metric`, with `--help`): the Step 0
  measurement is promoted into a test helper. It has four modes:
  - stray pixels in analytically computed regions, plus radial seam deviation
  - edge comparison between two renders
  - region comparison
  - region color

  Together they gate SC-001 to SC-003, SC-008 and FR-016.
- **Parity**: the existing cross-hider parity registrations cover SC-004. The
  hero, sphere-cube, coincident, caps, displaced and translucent scenes are
  added to them.
- **Regression**: full `ctest -L visual` for non-CSG scenes (FR-020, SC-005).
  Known flaky and pre-existing failures are not misattributed:
  - flaky: ImagerRegressionRender, Parity_transparency-matte-aov,
    ShadingParity_AmbientAccumulation, NotRequired_motion-subdiv-translate-photon
  - already failing on master: ImagerGuardTests, ShaderCompilerImmutability

## Decision 14: Performance posture (SC-007: under 10×)

**Decision**: correctness first. Cost is bounded structurally:

- Only operands whose bounds overlap a patch are ever consulted.
- Uniform patches are classified once.
- Per-vertex classification happens only on seam-straddling grids.
- The tree evaluation short-circuits.

Timing is measured against the Step 0 baselines (raytrace 0.31 s / 1.42 s,
REYES 0.19 s / 0.40 s, Z-buffer 0.16 s / 0.34 s) on the same machine, throttled
the same way.

## Out of scope (confirmed)

- **Motion-blurred operands**: classification uses shutter-open positions,
  matching today's behavior.
- **Performance tuning** beyond the 10× bound.
- **Applying the new per-micropolygon coverage mask to NURBS trim curves**: that
  would change non-CSG output (FR-020). Trims keep their current path; this is
  noted as a possible follow-up.
