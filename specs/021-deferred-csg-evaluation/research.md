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

**Decision**: Each operand answers `classify(ctx, P) → IN / OUT`. An operand is
the leaves of one `"primitive"` block, or all the primitives placed directly in a
boolean block, which together form one operand positioned first, as in spec 013.

The operand's member surfaces are its resolved per-instance surfaces:
procedurals expanded, and mesh-type leaves materialized (Decision 15). They are
held in a `CCSGResolvedOperand { members, surfaces, kind, bound }`, whose kind
comes from a pure `deriveOperandKind(members, surfaces)` built on the closedness
test (Decision 7). Members are kept because closedness is a property of a
declared primitive, not of the pieces a mesh splits into (`CNURBSPatchMesh`
emits one patch per knot span, `patches.cpp:2172–2190`).
Unit tests build resolved operands directly; instance resolution calls the same
function.

`ctx` is the caller's `CShadingContext`. Every call site (`CPatch::dice`,
`CObject::regionCoverage` called from `CReyes::shadeGrid`, `intersect`) already
has one. The predicate is `accepts(ctx, P, Ng)`: `Ng` is the accepting surface's
undisplaced geometric normal at P, which every caller already has (the quadric
and polygon intersects compute it, `CTesselationPatch` carries it, and
`regionCoverage` requests `PARAMETER_NG` from `sample()`). The two-sided ON rule
(Decision 4) uses it only as a transverse axis, so its sign convention does not
matter. `ctx` may be NULL for quadric, polygon and bilinear surfaces; free-form
surfaces and mesh materialization require it.

**Operands the solid cannot trim**: `CImplicit` (`Geometry "implicit"`) and
`CDLObject` (`Geometry "dlo"`) derive from `CSurface` but have their own
`intersect()`/`dice()` that bypass every trimming hook, and count no crossings.
`CPoints` dices straight to `drawPoints` (`geometryDispatch.cpp:555`, the
untouched point-grid path), `CCurve` reaches `drawGrid` without the `CPatch`
region stage, and `CCurveMesh` and `CDelayedInstance` (an `ObjectInstance`
inside a solid, which spec 013 also rejected) are not among the materializable
mesh types. All of them would render untrimmed, ignore the boolean, or enclose no
volume. They report `honorsRegion() == FALSE` (a generic `CObject` virtual).
Defensively, any member that is neither a `CSurface`, one of the five mesh types
with `materialize()`, nor a `CDelayedObject` is treated the same way. Instance
resolution drops them with one warning naming the primitive, deduplicated per
(definition, operand) per frame like the "not closed" warning: they are not
supported as solid operands (FR-018). This replaces spec 013's "Unsupported CSG
operand type" error, which goes with `csgTessellateOperand`.

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
   - The same parity rule applies to direct leaves of a boolean block (spec edge
     case).
   - This uses a new per-surface capability, `countCrossings(ctx, from, dir)`,
     which returns the **unsigned** number of times the ray crosses the
     undisplaced surface. It is implemented:
     - analytically for quadrics (every root in the u/v range, not just the
       nearest — `CSphere::intersect()` in `quadrics.cpp:167` already iterates
       roots in order)
     - exactly for flat polygons (`CPolygonTriangle`/`CPolygonQuad`)
     - analytically for bilinear patches
     - for free-form surfaces (`CBicubicPatch`, `CNURBSPatch`, `CPatchGrid`,
       `CBSplinePatchGrid`, `CSubdivision`), by a **pure Newton-refinement core
       over a surface-evaluation callback**. The core is seeded from a
       per-surface grid of the surface's own evaluation, built once under a
       mutex and immutable afterwards. It deliberately does **not** use the
       per-thread `CTesselationPatch` cache: that cache needs a context and is
       not shareable across threads. Refined crossings are accurate to float
       precision, which meets FR-007a.
     - **Placement**: the core lives in the renderer-only
       `src/ri/geometry/surfaceCrossingDispatch.cpp`, with a stub in
       `src/ri/vector/surfaceCrossingDispatchStub.cpp`. This mirrors the existing
       `csgTesselationDispatch.cpp`/stub split. The surface classes' files
       (patches.cpp, patchgrid.cpp, bsplinePatchgrid.cpp, subdivision.cpp) also
       compile into ribVector, which excludes `surface.cpp`; they only forward to
       the dispatch function, so libribpreview keeps linking.
     - Unknown `CSurface` types (for example `CImplicit`) return 0 by default and
       so contribute nothing.
   - Classification rays bypass every rendering-side ray filter: visibility
     attribute flags, LOD, backface culling and displacement tessellation
     (`CSurface::checkRayGuard()`, `src/ri/render/geometryDispatch.cpp:239`). They
     are a geometry query, not a render ray.
   - **Robustness**: the ray direction is a fixed, deterministic,
     axis-irrational direction. If a crossing is near-grazing
     (|N̂·d̂| < 1e-4) or within a parametric distance of 1e-5 of a surface edge, the
     query is recast with a second fixed direction, and the majority of three
     decides. Both thresholds are named constants in `csgClassify.cpp`. The result is a pure
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
  - **Emptiness propagates bottom-up** (FR-025a): a subtree is empty if it is a
    union of empties, an intersection with an empty operand, or a difference
    whose first operand is empty. Empty subtrees are removed from unions and
    later difference positions before the passthrough and implied-closure tests,
    so `union(openCylinder, intersection(B, empty))` is a passthrough.
  - **Operands that contribute nothing never render.** The forced IN/OUT test
    assumes P really lies on its own operand's boundary, which is false for an
    empty or open operand (a lone disk, an open sheet, an open mesh): in
    `union(A, Disk)` a disk point outside A would otherwise be accepted. So the
    leaves of operands resolved as empty or open are left out of the solid's
    children entirely (Decision 9, step 7). For identical shapes that cancel by
    parity inside one operand, the coincident-face check below catches it,
    because it also counts the other surfaces of the leaf's own operand.
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
  boundary (ON), including another surface of the leaf's own operand, classify
  at P ± ε·N̂g instead, where Ng is the normal passed
  to `accepts()` (the leaf's own undisplaced geometric normal; at a coincident
  face it is ± the other face's normal), and use the two-sided rule: P is on
  the boundary iff the composite membership differs on the two sides. Ng is only
  a transverse axis; its sign is never used, so REYES (`sample()` applies only
  `xform->flip`) and raytrace (intersectors apply `INSIDE ^ xform->flip`) agree.
  - Where coincident leaves of several operands all report a boundary at the
    same place, only the leaf of the **lowest-indexed** such operand accepts. Exactly
    one face renders, identically in every hider, with no flicker.
  - Facing is not decided here. Each leaf's orientation is its static `leafFlip`
    (above).
  - ε is deterministic and scene-relative: `C_EPSILON` scaled by the solid's
    bound size.
  - This removes internal skins (union of flush boxes) and leftover skins
    (difference flush with a face) without flicker.
- **Empty operands (FR-025a)**: an operand that encloses no volume (an empty
  nested block, an empty expansion, a lone disk, a lone open non-quadric surface,
  or shapes that cancel by parity) classifies every point as OUT. The tree
  evaluation then gives the empty-set semantics with no special casing: no change
  in a union or as a later operand of a difference, and nothing from an
  intersection or from a difference whose first operand it is. The CSG-aware
  bound agrees: an empty operand has an empty bound, so an intersection with it,
  or a difference with it first, is culled whole. An empty nested block therefore
  stays in its position, unlike spec 013's resolver, which dropped it
  (`csgTree.cpp:511`).

**Rationale**: for union/intersection/difference trees in which every operand
appears once, the composite is monotone in each operand, so orientation is a
static property. The pivotal-operand test is exact and handles arbitrary
nesting.

**Alternatives considered**: *orientation by signed volume per result* — this is
spec 013's approach, and it is meaningless without a mesh.

## Decision 5: REYES / Z-buffer integration

**Decision**:

- The solid's `dice()` calls `CObject::dice(r)`, which hands each leaf (and any
  caps) to the normal per-bucket split/dice path via `drawObject`, with the
  region predicate attached (Decision 9). `CSurface::dice()` itself lives in
  `geometryDispatch.cpp:332`.
- The `CPatch::dice()` probe loop (`surface.cpp:142`, ~L145-362) gains a region
  stage, parallel to (and combinable with) the trim stage.
- **Uniform-patch shortcut**: if a (sub-)patch's probe bound does not overlap the
  bound of any other operand's boundary surfaces, its classification is uniform.
  One predicate evaluation keeps or drops the whole patch, with no per-vertex
  work.
- **Bound classification uses undisplaced geometry.** `classifyBound()` runs on
  the bound of the *undisplaced* probe points, sampled with the object's own
  `sample()` from a snapshot of the probe u/v (as the trim stage snapshots
  `trimU`/`trimV`, `surface.cpp:172`), padded by the largest distance between
  neighbouring probe points to cover curvature between probes. The displaced
  probe bound from `displaceEstimate()` is not used for this, because
  displacement can move points across an operand boundary.
- **Straddling (`MIXED`) patches** dice at the **seam rate**: micropolygon edge
  ≤ min(√ShadingRate, 1) pixel. They split, counting `numCsgSeamSplits`, only
  while the patch at that rate would exceed the grid-size limit.
  `CRenderer::maxEyeSplits` does not apply to them.
  - The trim path *drops* straddlers at the eyesplit cap; for CSG that drop would
    itself create exactly the reported gaps.
  - The safety cap is 24 split levels (about float-mantissa precision); hitting
    it *keeps* the patch instead of dropping it.
- **Final grids** carry a **per-micropolygon coverage mask**, computed from
  **undisplaced** camera-space P. The P-only grid P cannot be used: in
  `CReyes::shadeGrid()`'s P-only branch (`reyes.cpp:748`, ~L955, reached from
  `drawGrid()` at :695) it has already been displaced and converted to sample
  space (`displace()`, then `camera2samples()`).
  - A generic hook, `virtual void CObject::regionCoverage(CShadingContext*, int
    udiv, int vdiv, unsigned char *coverage)` (default no-op in `object.cpp`;
    `CSurface` override in the renderer-only `render/geometryDispatch.cpp`, with a
    no-op stub in `vector/geometryDispatchStub.cpp`), snapshots the grid's
    u/v/time, calls the object's own `sample()` for undisplaced P and Ng,
    evaluates `region->accepts(ctx, P, Ng)` per vertex, fills the mask and restores u/v/time. The hider calls it before
    `displace()` when `object->region` is set. Motion-blurred grids use the
    shutter-open sample. The mask is kept with the grid through its later full
    shading.
  - The override cannot live in `surface.cpp`: that file is excluded from the
    shared ribVector library (`src/ri/CMakeLists.txt:370`), so the `CSurface`
    vtable there would reference an undefined symbol. Every other render-only
    `CSurface` method uses the same `geometryDispatch.cpp` / stub split.
  - A quad is absent iff none of its four vertices is accepted. This is
    conservative: straddling quads are kept, which yields sub-pixel overlap,
    never a gap.
  - An absent quad gets the bound `{INT_MAX, INT_MIN, INT_MAX, INT_MIN}` where the
    per-quad bounds are computed, in the quad branch of `insertGrid()`
    (`reyes.cpp` ~1705). Point grids (`RASTER_POINT` branch, ~1380–1460) are
    untouched. The rasterizers' first
    existing trivial reject, `bounds[1] < left` (`stochasticQuad.h:703`,
    `zbufferQuad.h:40`), then skips it. The motion-blurred xtreme path culls by
    the per-time-stratum bounds `grid->quadStratumBounds` alone
    (`stochasticQuad.h:518–534`), never by `bounds[]`, so the same value is also
    written into each absent quad's stratum entries (computed at `reyes.cpp`
    ~1715). Together, **no rasterizer loop changes**.
  - The mask is a generic `CRasterGrid` field ("geometry says this micropolygon
    does not exist"), with no CSG knowledge in the hider (FR-012).
- Shading stays lazy, as today (`RASTER_UNSHADED`): only surviving grids reach the
  shader, and only when a bucket needs them.

**Rationale**:

- Bucket-driven splitting already defers work to the buckets that need it
  (FR-002, FR-004).
- The mask reuses existing culling.
- The seam rate makes seam error track screen resolution (FR-007, FR-009): at most
  one micropolygon, which is at most 1 pixel.

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
  - the analytic bilinear-patch intersector, `CBilinearPatch::intersect`
    (`patches.cpp:193`, its `solve()` acceptance), which never goes through
    `CTesselationPatch`. Boxes built from `Patch "bilinear"`, and the implied
    caps, depend on it.
  - the `CTesselationPatch` candidate test at `surface.cpp:857` and `:1024`, next
    to `trimAccepts`. The region comes from the surface being tessellated, not
    the tessellation patch, which is assigned as a child directly, without
    `setChildren()` (`geometryDispatch.cpp:282–322`), so its own `region` is null.
  - the quadric root loops work in object space, so the check computes the
    camera-space P as `from + t·dir` and transforms the object-space normal, as
    the accepted hit does (`quadrics.cpp:276`)
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

- Implied closure applies only to an operand that takes part in a boolean with
  at least one other **non-empty** operand. Empty operands that change nothing
  are removed first (FR-025a), so `difference(openCylinder, empty)` and
  `union(openCylinder, Disk)` are single-operand solids. A whole solid that
  collapses to one operand is a passthrough: the operand renders as authored, with no caps (FR-019, spec 013
  FR-017).
- Implied closure is defined for operands consisting of **exactly one quadric**:
  the hero scene's case, and the common authoring pattern.
- **Classification**: the quadric's implied solid (Decision 3.2).
- **Rendering**: the solid synthesizes **cap surfaces** for that operand:
  - planar bilinear patches that generously cover each open cross-section: end
    circles at z-min/z-max or height ends (a paraboloid with zmin > 0 has both),
    and the two half-planes of a partial sweep (for a torus, flat circular
    segments)
  - **a torus cut across its tube** (partial phi) is the exception: its two cut
    circles lie at different heights and radii, so no flat face closes it. It is
    closed by a `CHyperboloid` from (R + r·cosφmin, 0, r·sinφmin) to
    (R + r·cosφmax, 0, r·sinφmax) over the torus's own `thetamax`, a cone-shaped
    surface. Its implied solid is the circular segment between the tube arc and
    that chord, revolved; classification tests the point's (ρ, z) cross-section
    against that segment
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
- **Closedness test** (applied per instance, after procedural expansion and mesh
  materialization; see Decision 8 and Decision 15):
  - A quadric is closed only when it is a full sphere or a full torus. Every
    other single quadric is auto-closed as above, except a disk, which encloses
    no volume and is a silently empty operand.
  - A single non-quadric **member** (one declared surface or mesh, however many
    pieces it splits into) is closed when every parametric boundary edge of its
    pieces either collapses to a point or coincides with another boundary edge of
    the same member. Polygon and subdivision meshes are closed when every edge is
    shared by exactly two faces. An operand with several members is
    multi-surface: parity, no closedness test.
- **A lone non-quadric surface that is not closed** (a single open NURBS sheet, a
  single bilinear patch) has no well-defined implied solid. It **contributes
  nothing** (an empty operand) and emits an "operand is not closed" warning
  (`warning(CODE_BADTOKEN, …)`, never `CODE_LOG`), once per offending operand per
  frame. It is emitted at instance resolution under `delayedMutex`, deduplicated
  per (definition, operand), so further instances stay silent and no atomic guard
  is needed. This is deterministic and
  testable, and the boundary-edge test avoids false diagnostics on closed
  single-patch shapes such as a NURBS sphere (seam plus poles).

**Spec alignment**: confirmed by the maintainer during planning (2026-09-30).
FR-021 was amended to this exact scope, and a Clarifications bullet was added to
the spec. The closedness test and the empty-plus-diagnostic rule for open
non-quadric surfaces were added during analysis (2026-09-30).

**Alternatives considered**:

- *Generalized winding number as "implied closure" for any open surface*: it
  gives a classification, but caps cannot be rendered for arbitrary open
  surfaces, so the composite would show holes.
- *Always auto-cap every open quadric*: it double-counts author-closed operands.

## Decision 8: Procedural operands (FR-024/FR-025)

**Decision**:

- The capture-time rejection is removed: the whole
  `if (currentSolid != NULL) { csgValidateProceduralCapture(currentSolid); return; }`
  branch in `RiProcedural()` (`rendererContext.cpp:5200`), not just the call,
  since the early `return` alone would still drop the procedural. Its definition
  (`csgTree.cpp:90`) goes too. `addObject()` then routes the `CDelayedObject`
  into the `leafObjects` of whatever solid block is open, so a procedural placed
  directly in a boolean block joins its first operand, like any direct leaf
  (FR-024).
- A procedural declared in a solid block becomes an *unexpanded operand member*.
- When a solid instance is first needed (its first `dice()` or `intersect()`), its
  procedural operand members are expanded under `CRenderer::delayedMutex`, as
  `CDelayedObject::intersect()` does (`src/ri/render/geometryDispatch.cpp:679`).
- **Capture-aware expansion**: `processDelayedObject()` (`rendererContext.cpp:421`)
  cannot be reused. It targets the delayed object's own children, resets
  `delayed = NULL` on return (so later leaves of the same operand would land in
  the scene), and calls `setChildren()` before any region is set (so the
  expansion would render untrimmed). A new helper,
  `CRendererContext::expandIntoOperand(ctx, cDelayed, holder, flagged)`:
  - saves `delayed`, `currentAttributes` and `currentXform`
  - sets `delayed = holder`, the current attributes to a copy of `flagged`, and
    the current xform to the delayed object's xform
  - runs the subdivision function, then restores all three to their saved values
  - does not call `setChildren()`

  Each instance first instantiates its template `CDelayedObject`
  (`delayed.h:68`) into the operand holder with the composed xform (Decision 9,
  step 2). Resolution then loops over the operand's members, replacing each
  `CDelayedObject` with its expansion, until none remains; nested procedurals are
  handled by the same loop. Region, materialization and operand type therefore
  only ever see real geometry.
- **Attributes**: the expansion runs with a flagged clone of the procedural's
  attributes as the current attributes, so every expanded object inherits
  `ATTRIBUTES_FLAGS_SOLID_FRAGMENT` and the leaf's `ATTRIBUTES_FLAGS_INSIDE`
  parity, exactly as instantiated leaves do.
- **Nested solid blocks**: `csgValidateNestedSolidBegin()` cannot be reused as is.
  It needs a parent node, `currentSolid` is NULL at render time, and the matching
  root-level `RiSolidEnd` would call `resolveCSGTree()` and create a new solid.
  Instead, an `expandingOperand` flag (set by `expandIntoOperand()` for the
  duration of an expansion, saved and restored around it) and a counter
  `swallowedSolidDepth`, both in `CRendererContext`, guard them. A `SolidBegin`
  inside an ordinary procedural, outside any operand expansion, stays legal and
  is not swallowed:
  - `RiSolidBegin` still validates the operation name
  - at the first nesting level it reports the nested-in-primitive message through
    a parent-less helper, `csgReportNestedSolidBegin()`, which
    `csgValidateNestedSolidBegin()` also calls
  - it then increments the counter and creates no node
  - `addObject()` discards objects while the counter is above 0
  - `RiSolidEnd` only decrements it and resolves nothing
- An empty expansion yields an empty operand, with FR-025a semantics (the empty
  set in its position).
- Each operand is expanded at most once per solid instance. It is never expanded
  if the instance is never needed.
- **Operand kind is derived after expansion.** Single-quadric / multi-surface /
  empty / open, and therefore implied closure and caps, are decided per instance
  from the *expanded* members. An archive holding a single open quadric thus
  behaves "exactly as if written there directly" (FR-024), caps included.
  `CCSGDefinition` stores only the template structure.
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
- **Transforms**: captured leaves carry absolute transforms (the full
  local-to-world transform active when they were declared). The definition
  therefore rebases each template leaf into the outer frame,
  `outerXform⁻¹ · leafXform`, which is the logic of today's `csgRebaseFragments`
  (`csgTree.cpp:687–722`), moved rather than deleted. The template bound is
  computed in that local frame (each leaf's camera-space bound transformed by
  `outerXform⁻¹`). Each solid's bound is that local bound transformed by the
  solid's own xform, and instantiating a leaf with the solid's xform rebuilds
  the absolute transform for a direct placement and the composed one for an
  instance. Without the rebase, the outer transform would be applied twice.
- **Per-instance state** (`CCSGInstanceState`) is built on first use, under
  `delayedMutex`, in this order:
  0. Unsupported members (Decision 3: `honorsRegion()` false, or not a surface,
     mesh or procedural) are dropped with one deduplicated warning per offending
     operand per frame, for template and expanded members alike.
  1. **Capture target**: `instantiate()` hands its output to `addObject()`
     (e.g. `quadrics.cpp:512`). For each operand, the context's `delayed` points
     at a temporary holder, as `processDelayedSolid()` does
     (`rendererContext.cpp:485`), so the output lands in the operand's member
     list, not in the scene.
  2. **Flagged clones**: each template leaf's attributes are cloned, with
     `ATTRIBUTES_FLAGS_SOLID_FRAGMENT` set and `ATTRIBUTES_FLAGS_INSIDE` toggled
     per `leafFlip`, and the leaf is instantiated with that clone and the solid's
     xform (`instantiate()` uses the attributes passed in,
     `quadrics.cpp:508`). Procedural members are expanded here too (Decision 8).
  3. `region` set on every member to its `CCSGRegion`.
  4. Depth-first materialization (Decision 15).
  5. Every `CSurface` descendant collected as the operand's classification
     surfaces.
  6. Operand kinds derived from the resolved members (Decisions 7 and 8), with
     the "not closed" warning.
  7. Caps, carrying the operand's flagged attributes. The members of operands
     resolved as empty or open are left out of `children` and get no caps
     (Decision 4). If the tree then reduces to one non-empty operand, the
     instance is a passthrough: that operand renders as authored, with no caps
     (FR-019), and the `region` that steps 3 and 4 already set on its members and
     copied to their mesh children is cleared on all of them.

  Mesh children copy their parent's `attributes` pointer when `create()` runs
  (`polygons.cpp:1313`, `patches.cpp:1650`, `subdivisionCreator.cpp:630`), and
  `setChildren()` copies only `region`. Steps 2 and 3 must therefore precede
  step 4, or mesh-type operands would render without the region, the
  solid-fragment flag or the orientation flip.
- **Dicing**: `CCSGSolid` sets `children` to all instance leaves (mesh-type
  objects, surfaces, caps), and its `dice()` calls `CObject::dice(r)`
  (`geometryDispatch.cpp:76`). Every leaf is therefore clipped and queued per
  bucket by `CReyes::drawObject`, as with any other object. Calling each leaf's
  `dice()` directly would bypass that per-bucket clipping and dice every leaf
  whole on first touch, which would violate FR-002/FR-004.
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
- The first occurrence per frame emits a notice with `warning(CODE_BADTOKEN, …)`.
  Warnings are always printed, to stdout (`ri.cpp:2367–2370`). Do not use
  `CODE_LOG`, which silently drops messages.
- A per-frame flag is reset at `RiWorldBegin`. "Render" in FR-023 means one
  frame, one `WorldBegin`/`WorldEnd` (spec Assumptions).

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
still used elsewhere stay. These go with them:
- `src/ri/geometry/csgTesselationDispatch.cpp` and
  `src/ri/vector/csgTesselationDispatchStub.cpp`, which exist only for those
  helpers (`src/ri/CMakeLists.txt` L63, L343, L373)
- the friend declaration at `polygons.h:73`
- csgTree.cpp's `kCsgDefaultToleranceFraction` (L47) and its
  `#include "csgBoolean.h"`, both removed at the switch-over, since they become
  unused and would trip `-Werror`
- the `#include "csgBoolean.h"` in `tests/unit/csg/csgTestUtils.h`

## Decision 12: Statistics (SC-006)

**Decision**: add counters to `CStats` (`src/ri/core/stats.h`) as `atomic_int32`,
like the blobby counters (`stats.h:103–107`), because classification runs on
every render thread. Like the other
subsystem sections, they print at statistics level ≥ 2 (`stats.cpp:150`), but
**always, zeros included, whenever the frame contains a solid**: a per-frame
flag, reset at `RiWorldBegin`, set by every outermost `RiSolidEnd` inside the world block, single-operand
collapses included (so a collapsed solid still prints its zeros), and by every
`CCSGSolid::instantiate()` (a solid defined in an `ObjectBegin` outside the
world block and instanced inside the frame). The counters are INFO-level
messages: `orender` sets the log level to NONE at startup (`orender.cpp:611–612`),
so they print only with `-x 3` or `ORENDER_INSTR_LEVEL=info`. They use the exact
labels `CSG classifications:`, `CSG seam splits:`, `CSG instances resolved:` and
`CSG procedural expansions:`. The statistics test scenes use
`Option "statistics" "endofframe" [2]`. The counters are:

- `numCsgClassifications` (point classifications performed)
- `numCsgSeamSplits` (straddle-driven splits)
- `numCsgInstancesResolved` (solid instances whose per-instance state was built)
- `numCsgProceduralExpansions`

"No refinement work for an off-screen solid" means `numCsgInstancesResolved`,
`numCsgClassifications`, `numCsgSeamSplits` and `numCsgProceduralExpansions` stay
0 for it.

## Decision 13: Test strategy (constitution III — TDD)

**Decision**: tests are written first, fail, and then pass. Guard and
verification tests, which already pass, are labeled as such in `tasks.md`.

- **Unit (`tests/unit/csg/`)**: the kernel suites are rewritten as
  classification and boundary-rule suites over the **same scenarios**:
  `test_boolean_boxes`, `_coplanar`, `_sphere_box`, `_sphere_sphere`,
  `test_nested_tree`, `test_multi_primitive_leaf`.
  - They build `CCSGDefinition` directly, with no Ri capture interception. The
    capture path gets its own test, `test_capture_path`, after the switch-over.
  - `test_multi_primitive_leaf` uses disjoint spheres. It also asserts that two
    coincident identical spheres in one block form an empty operand under
    parity.
  - Unit tests use directly constructible surfaces (quadrics, polygons,
    bilinear patches) with a NULL context. Free-form crossings are tested
    through the pure Newton core with analytic callbacks. Mesh-type and
    free-form operands are covered render-side.

  The rewritten suites now assert:
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
    closedness helper on directly constructible surfaces. The NURBS closedness
    cases run render-side through `add_csg_metric_test` log checks in Phase 3,
    consistent with free-form operands being covered render-side: a new closed
    rational quadratic NURBS sphere scene must print no diagnostic, and the
    existing `csg-nurbs-sphere-union-raytrace.rib`, whose NURBS operand is in
    fact a single open, non-rational bicubic sheet, must print exactly one and
    lose that operand (its reference is regenerated under FR-021).
  - empty operands as the empty set in their position (FR-025a), in
    `test_boundary_rule`
  - analytic seam location (points straddling the exact intersection curves),
    tangency, and direct leaves of a boolean block, in `test_boundary_rule`
  - determinism: identical classification regardless of query order or thread
  - `countCrossings` per primitive type, plus the free-form Newton core against
    analytic callbacks (FR-007a)
  - region inheritance through `setChildren` (`test_region_inheritance`)
  - CSG statistics counters (`test_csg_stats`)
  - capture path, leaf-attribute flags, lazy resolution, implied caps, and
    procedural operands (including an archive-supplied open cylinder)
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
    `csg-malformed-procedural-in-primitive.rib`; it renders the block's sphere)
  - an archive-supplied open-cylinder scene with an inline twin
  - secondary-ray scenes (raytrace and REYES)
  - statistics scenes: off-screen solid with a procedural operand, partial
    view, resolution scaling, and a tiny solid
  - a polygon-mesh box difference scene
- **Diagnostic guards**: all four `csg-malformed-*` scenes are registered from
  Phase 1, with log regexes for today's diagnostics (FR-019).
- **Seam metric tool** (`test_csg_seam_metric`, with `--help`): the Step 0
  measurement is promoted into a test helper. Every mode shares include/exclude
  regions and a `--margin` for pixel-filter falloff. The modes are:
  - `--annuli`
  - `--edge-compare`
  - `--region-compare`
  - `--region-chroma`
  - `--projected-curve`, which checks the seam against the exact intersection
    curves projected through the camera, in every hider
  - `--enclosed-holes`
  - `--luminance-ratio`, for shadows and reflections, which alpha cannot see

  Together they gate SC-001 to SC-003, SC-008, FR-007a and FR-016.
- **Harness** (Decision 16): `add_csg_metric_test` plus a `cmake -P` driver
  combines renders, metrics, log regexes and statistics comparisons.
- **Parity**: the existing cross-hider parity registrations cover SC-004.
  Three-hider scenes get the pairs raytrace↔reyes and raytrace↔zbuffer, and
  two-hider scenes get one pair; existing pairs are kept, not duplicated.
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

## Decision 15: Mesh-type operands — region inheritance and materialization

**Decision**:

- `region` lives on `CObject`, generic and nullable, not on `CSurface`.
- `CObject::setChildren()` (`object.cpp:130`) copies the parent's `region` to
  every child that has none. All five mesh types create their surfaces lazily in
  a non-virtual `create(CShadingContext*)` that ends in `setChildren(context, …)`:
  - `CPolygonMesh` (`polygons.cpp:1939`)
  - `CPatchMesh` (`patches.cpp:1710`)
  - `CNURBSPatchMesh` (`patches.cpp:2203`)
  - `CSubdivMesh` (`subdivisionCreator.cpp:2256`)
  - `CLoopSubdivMesh` (`subdivisionLoop.cpp:483`, which builds a nested
    `CPolygonMesh`)

  Their surfaces, nested ones included, therefore inherit the predicate with no
  per-mesh-type rendering code.
- A new `virtual void CObject::materialize(CShadingContext*)` (default no-op) is
  overridden by the five mesh types as `if (children == NULL) create(context);`.
  `create()` is already mutex-guarded and idempotent (`polygons.cpp:1840`).
  `CCSGInstanceState` materializes **depth-first**: `materialize(ctx)` on each
  member, then on each of its children. One level is not enough, because
  `CLoopSubdivMesh::create()` sets its children to a nested `CPolygonMesh` whose
  triangles do not exist yet (`subdivisionLoop.cpp:483`). It then collects every
  `CSurface` descendant as that operand's classification surfaces.
- Materialization happens after the flagged attribute clone and the `region`
  are set (Decision 9), because mesh children copy both at `create()` time.

**Rationale**:
- Most authored operands are mesh-type `CObject`s, not `CSurface`s:
  `PointsPolygons`, `PatchMesh`, `NuPatch` meshes, subdivision meshes,
  polygonized blobbies.
- `setChildren` is the single point every mesh type already goes through, so
  inheritance there is generic and automatic.

**Alternatives considered**: per-mesh-type region plumbing in each `create()`.
Rejected: five copies of the same logic, and it is easy to miss one.

## Decision 16: Test harness for render-and-measure tests

**Decision**:
- A `cmake -P` driver, `tests/visual/csg_metric_test.cmake`, plus an
  `add_csg_metric_test(...)` macro in `tests/visual/CMakeLists.txt`.
- **Why it's needed**: `add_visual_test` only compares one render against a
  reference, and `add_parity_test` only diffs two renders.
- **What the driver does**:
  - renders one or more RIBs, with the same environment as `add_visual_test`
  - copies support directories (archives) into the per-test scratch working
    directory
  - runs `orender -x 3` with `ORENDER_INSTR_LEVEL` unset, because statistics
    are INFO-level messages and `orender` sets the log level to NONE at startup
    (`orender.cpp:611–612`), so without `-x 3` they never print
  - captures stdout and stderr together (statistics and warnings go to stdout,
    errors to stderr; `ri.cpp:2354–2378`) and applies log regexes to that
    combined output itself, so CTest's `PASS_REGULAR_EXPRESSION` never overrides
    the exit code
  - runs `test_csg_seam_metric` modes and propagates their exit code. Alpha-only
    modes cannot see shadows or reflections, so the tool also has a
    `--luminance-ratio` mode for secondary-ray checks.
  - extracts end-of-frame statistics by label for `==`, `<=`, `>` and
    ratio-to-other-render comparisons

**Rationale**: about 25 tests need multi-render, metric, log and statistics
combinations, so one driver keeps them uniform and correct.

## Out of scope (confirmed)

- **Motion-blurred operands**: classification uses shutter-open positions,
  matching today's behavior.
- **Performance tuning** beyond the 10× bound.
- **Applying the new per-micropolygon coverage mask to NURBS trim curves**: that
  would change non-CSG output (FR-020). Trims keep their current path; this is
  noted as a possible follow-up.
