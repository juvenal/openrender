# Contract: Geometry ↔ Hider ↔ Shading Boundaries (internal)

**Feature**: 021-deferred-csg-evaluation

This contract pins down how FR-010 to FR-013 (separation of concerns) are kept.
It lists what each domain may assume about the others.

## Geometry domain (`src/ri/geometry/`)

0. **Naming requirement.** The region-predicate type, its methods, the
   `CObject` hook member, `materialize()`, `countCrossings()` and the
   `CRasterGrid` coverage-mask field MUST have generic names: no `csg`, `Csg`,
   `CSG` or `Solid` anywhere in the identifier. For example: `CRegionPredicate`,
   `accepts()`, `classifyBound()`, `region`, `materialize()`, `coverage`. The
   hider (`reyes.cpp`) references these names, and the separation-of-concerns
   grep gate ([quickstart §7](../quickstart.md)) matches `csg`/`Csg`/`CSG`
   anywhere inside an identifier. Only the CSG *implementation*, which lives in
   `src/ri/geometry/`, carries CSG names.
1. **`CObject` region hook** (new, generic):
   - A nullable pointer to a region predicate on `CObject`.
   - `CObject::setChildren()` copies it to children that have none, so the
     surfaces a mesh-type object creates lazily inherit it.
   - `accepts(ctx, P)`: a pure function of the camera-space point P. `ctx` is the
     caller's `CShadingContext`, which may be NULL for quadric, polygon and
     bilinear surfaces.
   - `classifyBound(ctx, bmin, bmax)`: returns `ALL_IN`, `ALL_OUT` or `MIXED`.
   - A null pointer means today's behavior, bit for bit.
   - `CObject::materialize(ctx)` (default no-op) lets geometry force a mesh-type
     object to create its children. It is idempotent.
2. **`CPatch::dice()`** (`surface.cpp:142`) consults `object->region` alongside
   the trim hook. The two combine: a point must pass both. (`CSurface::dice()`
   itself is in `geometryDispatch.cpp:332`.)
   - `ALL_OUT`: the patch is skipped.
   - `ALL_IN`: the patch is diced normally.
   - `MIXED`: the patch splits. Termination is screen-size-based; hitting the
     safety cap keeps the patch.
   - Trim behavior is unchanged: straddlers are still dropped at the eyesplit cap.
3. **Hit acceptance** in every intersector (quadric root loops, polygon
   intersectors, `CTesselationPatch` candidate tests) calls `accepts()` on the
   undisplaced point before accepting a hit. A rejected candidate leaves
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
2. **`CReyes::drawGrid()`**: after the P-only sampling it already does, it asks
   the object's region hook, when one is set, for per-vertex acceptance
   (`region->accepts(this, P)`). From
   that it fills the grid's generic **coverage mask** (null when the hook is
   null). This is the only hider-side change. It is expressed in generic terms:
   "geometry reports which micropolygons exist."
3. **Per-quad bound computation** (`reyes.cpp` ~1452/1705) gives absent quads an
   empty bound. The rasterizer loops (`stochasticQuad.h`, `zbufferQuad.h`) are
   unchanged; they already skip quads outside the bucket.
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
