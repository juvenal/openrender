# Contract: Geometry ↔ Hider ↔ Shading Boundaries (internal)

**Feature**: 021-deferred-csg-evaluation

This contract pins down how FR-010 to FR-013 (separation of concerns) are kept.
It lists what each domain may assume about the others.

## Geometry domain (`src/ri/geometry/`)

0. **Naming requirement.** The region-predicate type, its methods, the
   `CObject` hook member, `materialize()`, `regionCoverage()`, `honorsRegion()`, `countCrossings()` and the
   `CRasterGrid` coverage-mask field MUST have generic names: no `csg`, `Csg`,
   `CSG` or `Solid` anywhere in the identifier. For example: `CRegionPredicate`,
   `accepts()`, `classifyBound()`, `region`, `materialize()`, `regionCoverage()`, `coverage`. The
   hider (`reyes.cpp`) references these names, and the separation-of-concerns
   grep gate ([quickstart §7](../quickstart.md)) matches `csg`/`Csg`/`CSG`
   anywhere inside an identifier. Only the CSG *implementation*, which lives in
   `src/ri/geometry/`, carries CSG names.
1. **`CObject` region hook** (new, generic):
   - A nullable pointer to a region predicate on `CObject`.
   - `CObject::setChildren()` copies it to children that have none, so the
     surfaces a mesh-type object creates lazily inherit it.
   - `accepts(ctx, P, Ng)`: a pure function of the camera-space point P. `ctx` is
     the caller's `CShadingContext`, which may be NULL for quadric, polygon and
     bilinear surfaces. `Ng` is the accepting surface's undisplaced geometric
     normal at P, in camera space. It is only the transverse axis of the
     two-sided coincident-face rule (P ± ε·N̂g), so its sign does not matter:
     REYES (`sample()`) and raytrace (intersectors) may hand opposite signs and
     still agree. Among coincident leaves that all report a boundary, only the
     lowest-indexed operand's leaf accepts.
   - `classifyBound(ctx, bmin, bmax)`: returns `ALL_IN`, `ALL_OUT` or `MIXED`.
   - A null pointer means today's behavior, bit for bit.
   - `CObject::materialize(ctx)` (default no-op) lets geometry force a mesh-type
     object to create its children. It is idempotent.
   - `CObject::honorsRegion()` (default `TRUE`) reports whether the object can
     take part in a solid: whether its own rendering consults the region hook and
     it encloses volume. `CImplicit`, `CDLObject`, `CPoints`, `CCurve`,
     `CCurveMesh` and `CDelayedInstance` return `FALSE`; a solid drops such
     operands with a deduplicated warning.
   - Geometry never hands the hiders a leaf whose operand contributes nothing:
     the leaves of operands *resolved* as empty or open are left out of the
     solid's children. Identical shapes that cancel by parity resolve as a
     multi-surface operand; their leaves stay, but the coincident-face check
     rejects every point of them.
   - `CObject::regionCoverage(ctx, udiv, vdiv, coverage)` (default no-op in
     `object.cpp`; `CSurface` override in the renderer-only
     `render/geometryDispatch.cpp`, with a no-op stub in
     `vector/geometryDispatchStub.cpp`, because `surface.cpp` is not part of the
     shared ribVector library) fills a grid's per-micropolygon coverage mask from
     **undisplaced** camera-space P and Ng at the grid's u/v, using the object's
     own `sample()`. It snapshots and restores the grid's u/v/time. A quad is absent iff
     none of its four vertices is accepted.
2. **`CPatch::dice()`** (`surface.cpp:142`) consults `object->region` alongside
   the trim hook. The two combine: a point must pass both. (`CSurface::dice()`
   itself is in `geometryDispatch.cpp:332`.)
   - `classifyBound()` runs on the bound of the undisplaced probe points, padded
     by the largest distance between neighbouring probes.
   - `ALL_OUT`: the patch is skipped.
   - `ALL_IN`: the patch is diced normally.
   - `MIXED`: the patch dices at the seam rate, a micropolygon edge of at most
     min(√ShadingRate, 1) pixel, and splits only while it would exceed the
     grid-size limit. `maxEyeSplits` does not apply. Hitting the safety cap of 24
     split levels keeps the patch.
   - Trim behavior is unchanged: straddlers are still dropped at the eyesplit cap.
3. **Hit acceptance** in every intersector (quadric root loops, polygon
   intersectors, the analytic `CBilinearPatch::intersect` at `patches.cpp:193`,
   `CTesselationPatch` candidate tests) calls `accepts()` on the undisplaced
   camera-space point and its geometric normal before accepting a hit. Quadric
   root loops run in object space, so they convert P and the normal first.
   `CTesselationPatch` reads the region of the surface being tessellated, since
   its own `region` is always null. A rejected candidate leaves
   `ray->t`/`ray->object` untouched.
4. **`CSurface::countCrossings(ctx, from, dir)`**: a read-only geometric query
   that returns the **unsigned** number of crossings (operands classify by
   parity, independent of surface orientation). It never consults ray flags,
   LOD, backface culling or displacement.
   - The free-form implementations forward to the renderer-only
     `surfaceCrossingDispatch.cpp`, a pure Newton core with an immutable
     per-surface seed grid that does not use the per-thread `CTesselationPatch`
     cache.
   - The ribVector build links a stub
     (`vector/surfaceCrossingDispatchStub.cpp`).
5. **Orientation** of flipped leaves is conveyed only through the leaf's cloned
   attributes (`ATTRIBUTES_FLAGS_INSIDE`), which the sampler and intersector
   already honor.

## Hider domain (`src/ri/hiders/`)

1. The hiders receive **no** CSG types, calls or flags.
2. **`CReyes::shadeGrid()`, P-only branch** (`reyes.cpp:748`, ~L955, reached from
   `drawGrid()` at :695): when `object->region` is set, it allocates the grid's
   generic **coverage mask** and calls `object->regionCoverage(this, udiv, vdiv,
   grid->coverage)` **before** `displace()`. The grid's own P cannot be used,
   because it is displaced and converted to sample space right after. The mask is
   null when the hook is null, computed once, and kept with the grid through its
   later full shading. Motion-blurred grids use the shutter-open sample. This is
   the only hider-side change, expressed in generic terms: "geometry reports which
   micropolygons exist."
3. **Per-quad bound computation** (the quad branch of `insertGrid()`, `reyes.cpp`
   ~1705; point grids are untouched) gives an absent quad
   the bound `{INT_MAX, INT_MIN, INT_MAX, INT_MIN}`, in `bounds[]` and in each of
   the quad's per-time-stratum entries of `grid->quadStratumBounds` (~1715). The
   rasterizer loops (`stochasticQuad.h`, `zbufferQuad.h`) are unchanged: their
   trivial rejects skip it, `bounds[1] < left` (`stochasticQuad.h:703`,
   `zbufferQuad.h:40`) and, on the motion-blurred xtreme path, which culls by the
   stratum entries alone, `qsb[1]` (`stochasticQuad.h:518–534`).
4. **Raytrace traversal** (`trace.cpp`) is unchanged. The solid is an ordinary
   non-dummy BVH node.

## Shading domain (`src/libshader/shading/`)

1. **No changes.** Shading receives ordinary surface points: P, N (already
   oriented), dPdu/dPdv, u/v, and the leaf's own attributes.
2. Interior/Exterior selection (`shading.cpp:1019`) keeps keying on
   `ATTRIBUTES_FLAGS_SOLID_FRAGMENT`. Geometry sets that flag on each leaf's
   cloned attributes, instead of on the fragment meshes it set it on before.

## Verification of this contract

- **Grep gate** ([quickstart §7](../quickstart.md)): no identifier containing
  `csg`, `Csg` or `CSG` (headers, types, functions, members such as
  `numCsgClassifications`) in non-comment lines under `src/ri/hiders/` or
  `src/libshader/shading/`:
  `grep -rnHE '[A-Za-z0-9_]*(Csg|CSG|csg)[A-Za-z0-9_]*' src/ri/hiders src/libshader/shading | grep -vE ':[0-9]+:[[:space:]]*(//|\*|/\*)'`
  must print nothing.
- **SC-008**: an untouched operand region matches the same shape rendered without
  a solid block.
- **FR-020 / SC-005**: every non-CSG visual regression scene is unchanged.
