# Research: Solid CSG Operations

**Feature**: `013-solid-csg-operations` | **Date**: 2026-08-26

This document resolves the open technical questions for implementing RenderMan
Spec 3.2 §5.9 `SolidBegin`/`SolidEnd` CSG under the hard architectural
constraint set by the plan input: **CSG boundary resolution happens exactly
once, purely in the geometry domain, before any hider touches the result**,
and the resolved output must dice correctly for REYES/z-buffer and intersect
correctly for raytrace/photon/future path-tracing through the same shared
primitive interface, with zero hider-specific CSG code.

## Decision 1: Where resolution happens — the `addObject()` chokepoint

**Decision**: Intercept `RiSolidBegin`/`RiSolidEnd` in `CRendererContext`
(currently an unimplemented error stub, `rendererContext.cpp:5502-5513`) by
adding `currentSolid` / `savedSolids` state that mirrors the existing
`instance` / `instanceStack` mechanism used by `RiObjectBegin`/`RiObjectEnd`
(`rendererContext.cpp:5515-5549`). While a solid block is open, `addObject()`
(`rendererContext.cpp:457`) diverts each captured primitive's `CObject*` into
the active CSG tree node instead of calling `CRenderer::render()`. At the
close of the **outermost** `SolidBegin`/`SolidEnd`, the captured tree is
resolved into boundary geometry and that single result is handed to
`addObject()` again, so it falls through to the exact same
`CRenderer::render(CObject*)` call (`renderer.cpp:1096-1133`) that every other
primitive uses — the call that registers a primitive with both the raytrace
object tree (`root->children`) and the REYES rasterizer
(`reyesContext->drawObject()`).

**Rationale**: `addObject()` is already the single normalized entry point
every RenderMan primitive passes through, and it already handles two other
"don't dispatch yet, capture for later" cases (`instance` for
`RiObjectBegin`/`End`, `delayed` for procedurals). A third gate for CSG is a
small, consistent extension of an existing pattern rather than new
architecture. Re-entering `addObject()` with the resolved primitive — instead
of calling `CRenderer::render()` directly — means a solid block declared
inside an `RiObjectBegin`/`RiObjectEnd` pair is captured into the object
definition exactly like ordinary geometry would be, with no special-casing:
CSG resolves once at `RiSolidEnd` (during object definition), and each
`RiObjectInstance` replay reuses that one resolved result under a different
transform, the same way it reuses any other primitive in the object. This
directly satisfies the spec's instancing edge case ("no special-casing").

**Alternatives considered**:
- *Resolve lazily inside `dice()`/`intersect()`*: rejected outright — this is
  precisely the per-hider resolution the plan input forbids ("no hider
  dependency... independent of hider or shading backend").
- *Give hiders a shared CSG-resolution helper they each call before
  dicing/intersecting*: rejected — still couples resolution timing to hider
  invocation, and duplicates the "resolve once" guarantee across call sites
  instead of getting it for free from a single `RiSolidEnd`.

## Decision 2: Boundary representation — per-leaf mesh fragments under a container, not one merged mesh

**Decision**: The result of resolving a solid tree is a new lightweight
container `CObject` whose `children` are one or more `CPolygonMesh` fragments,
each fragment carrying the `CAttributes*` (and therefore the Interior/Exterior
shader assignment, FR-009/FR-020) of the specific input leaf that contributed
those triangles. No single merged mesh is produced.

**Rationale**: `CObject` carries exactly one `CAttributes*`. A composite that
combines operands with different shaders (surface, or Interior/Exterior)
cannot be represented as one mesh with one attribute set — the clarified
FR-009 ("face keeps its operand's shader") is not satisfiable that way. A
per-leaf-fragment container is, however, both correct and cheap to produce:
the boolean algorithm chosen below (BSP-tree classification, Decision 3)
naturally emits output faces as convex fragments of *input* faces, so each
output fragment already knows which input leaf — and therefore which
`CAttributes*` — it came from, at zero extra bookkeeping cost. `CObject`
already supports a `children`/`sibling` list (`object.h:87-90`) with no new
virtual methods required: a container that dices/intersects by walking its
children is exactly the pattern `CLoopSubdivMesh` already uses to present a
refined result to every hider through the same generic dispatch
(`subdivisionLoop.h:26-30`) — the strongest existing precedent in this
codebase for "a resolved concrete boundary every hider reaches identically."

**Alternatives considered**:
- *One merged `CPolygonMesh` with per-triangle attribute overrides*: rejected
  — no such per-face attribute mechanism exists in `CObject`/`CAttributes`
  today, and inventing one would touch every hider's shading-attribute lookup
  path, violating the "zero hider-specific CSG code" constraint by leaking
  CSG concerns into the general attribute system.
- *Force a single shader across the whole composite*: rejected in
  clarification (`/speckit-clarify` session, "Face keeps its operand's
  shader").

## Decision 3: Boolean algorithm — BSP-tree CSG on tessellated operand meshes

**Decision**: Each solid-tree leaf is first tessellated into a concrete
triangle mesh (Decision 4 covers density). Each operand mesh is used to build
a BSP tree; boolean combination (union/intersection/difference) is performed
by the classic Laidlaw–Trumbore–Hughes / Naylor polyhedral-CSG technique:
classify and clip one tree's polygons against the other, merge, and repeat
per internal tree node, bottom-up for nested operations. `difference` is
implemented as intersection with the second operand's *complement* — meaning
retained faces from the subtracted operand are geometrically kept but with
winding order and normal reversed, since the visible cut surface is the
inward-facing side of the subtracted primitive.

**Rationale**: Principle V (Minimal Dependencies) rules out pulling in an
external exact-arithmetic boolean-mesh library (e.g. libigl, CGAL). BSP-tree
CSG is self-contained, has been published and battle-tested since the 1980s
specifically for polyhedral set operations, and its complexity is bounded and
well understood, which matters given the clarified "correctness-first, no
explicit performance target" scope for v1. It also composes cleanly for
nested trees (`union(A, difference(B, C))`): resolve inner nodes first,
recurse outward, matching the plan's `currentSolid` tree structure directly.

**Alternatives considered**:
- *Corefine-and-classify triangle-triangle intersection (à la Cork/libigl)*:
  more numerically robust in principle for coplanar/near-degenerate cases,
  but requires either exact/rational arithmetic or a carefully engineered
  epsilon framework to avoid the well-known robustness failures of naive
  float-based triangle-triangle intersection; a from-scratch implementation
  is materially higher-risk for a v1 feature than BSP-tree CSG, for a
  correctness-first target that does not need it to also be fast.
- *Voxel/implicit-surface CSG (SDF sampling + marching cubes)*: naturally
  robust and simple to combine, but destroys sharp edges/flat faces and
  requires choosing a sampling resolution independent of the source
  geometry's own precision — a worse fit for RenderMan primitives that are
  expected to stay exact (e.g. box CSG should keep flat faces and straight
  edges, not become faceted noise).

**Known risk carried forward** (not blocking Phase 0, tracked for
implementation): BSP-tree CSG has known robustness edge cases at exactly
coplanar faces and degenerate slivers. Mitigate with a single, documented
epsilon consistent with `C_EPSILON` (`common/algebra.h`, `1e-6`) for
classification, and cover coplanar-face scenarios explicitly in the unit-test
suite (Decision 6) rather than discovering them via visual-test flakiness.

## Decision 4: Operand tessellation density — reuse flatness-based adaptive refinement, not a fixed uniform density

**Decision**: At `RiSolidEnd` time, each leaf operand is tessellated into a
triangle mesh by a new, standalone flatness/chordal-deviation adaptive
stopping test, `tesselationSagittaWithinTolerance()`
(`src/ri/surface.h`/`.cpp`), driven from a tolerance value alone — with no
dependency on a traced ray.

This is a **per-cell midpoint-sagitta test**, not a literal reuse of
`CTesselationPatch::tesselate`'s existing `uFlat < uAvg && vFlat < vAvg`
formula (`surface.cpp:1858-1897`). That formula was prototyped standalone
against a synthetic sphere octant before writing any production code (div =
2..64): `uFlat`, normalized the same way the shipped code normalizes it
(`/div²`), converges to a **nonzero constant** (~0.124-0.125) as div
increases, rather than shrinking toward zero. That formula measures
whole-patch chordal deviation over a *fixed* domain — a property of the
domain's curvature, not of tessellation density — so no fixed normalization
of it converges with `div`, and it has no tolerance value that makes an
adaptive-refinement loop built on it terminate correctly for curved input.
It is fine for its original purpose (a self-relative, per-level "is this
grid flatter than average for itself" comparison feeding ray-footprint
subsampling) but is not usable against an externally supplied absolute
tolerance.

The replacement metric instead compares, per grid cell, the analytic surface
point at the cell's parametric midpoint against the bilinear average of that
cell's four sampled corners (the "sagitta" of the cell). The same standalone
prototyping (div = 2..128 on the same synthetic sphere octant) confirmed
`maxSagitta` shrinks as O(1/div²) — the correct, expected convergence order
for a quadratic surface approximation error — with `maxSagitta * div²`
settling to a stable constant (~0.617). This is what makes the test usable
against a fixed absolute tolerance: refinement doubles `div` until the
worst-case per-cell sagitta drops below `tolerance`. A corner-only test
(no probe point outside the candidate mesh's own vertex set) cannot detect
this error at all — four corners sampled exactly on the surface are always
"flat" by any corner-based measure even when the true surface bulges between
them — so the implementation samples one grid at `2*div` per iteration: the
even-indexed samples are the candidate mesh at resolution `div`, and the
odd-indexed samples are exactly the cell midpoints needed as sagitta probes,
avoiding a second `CSurface::sample()` call per iteration. On pass, the
finer `2*div` grid (already paid for) is emitted as the final mesh rather
than being discarded in favor of the coarser candidate.

The ray-footprint half of the original function's stopping criterion
(`surface.cpp:724-759`), which has no meaning outside of a traced ray, plays
no part in the new function — there is no ray at `RiSolidEnd` time. This applies uniformly
to any primitive with a parametric (u,v) surface evaluation, which already
covers NURBS/Bézier patches (`CNURBSPatchMesh`/`CBezierPatch`,
`patches.cpp:2132-2205`) and, newly for CSG purposes, quadrics — which are
otherwise never tessellated at all today (`CSphere::intersect` etc. raytrace
via pure algebraic root-solving, `quadrics.cpp:200-`) and must be tessellated
into mesh form regardless, since BSP classification (Decision 3) requires
discrete polygon fragments. Subdivision-surface operands continue to use
`CLoopSubdivMesh`'s existing subdivision-level refinement (see the
normals note below for its limits), driven to a higher level than an
on-screen REYES dice would pick, since CSG resolution is not screen-space
adaptive.

The tolerance itself is derived by default from the primitive's own
object-space bound diagonal (as before), not from `ShadingRate` or camera
distance — neither is known at CSG-resolution time, which happens once at
geometry-build time before any hider is chosen. The existing optional
attribute, `Attribute "solid" "float tessellationtolerance"`, now maps
directly onto this flatness tolerance (`contracts/solid-rib-interface.md`),
letting a scene author tighten it per-primitive when the default is
insufficient for a specific composite.

**Rationale**: Reusing the flatness criterion instead of a uniform,
bound-diagonal-only density directly answers the requirement that resolved
CSG volumes look smooth, particularly where the original operands were
NURBS patches, quadrics, or subdivision surfaces: density concentrates where
curvature is high (e.g. a tight NURBS fold, a sphere's silhouette) and stays
sparse on flat or gently-curved regions, rather than spending a uniform
triangle budget everywhere or under-tessellating the highest-curvature spot
at whatever density looked adequate on a test sphere. It is also proven,
in-tree code (Principle V) rather than a new heuristic invented for this
feature — extraction is real integration work (the ray-footprint term must
be removed, since there is no ray or camera at `RiSolidEnd` time), but it is
adapting an existing, already-validated algorithm, not authoring a new one.

This remains the direct, named cost of the plan's hard constraint: resolving
CSG once, in the geometry domain, before any hider exists, means the
boundary's tessellation fidelity is necessarily baked in before camera
distance or `ShadingRate` are known — a curved primitive's CSG silhouette
becomes a fixed (if curvature-adaptive) polygonal approximation regardless
of how close the camera ends up. The alternative (defer tessellation
density to dice-time, per-hider) is exactly the lazy, hider-coupled
resolution the plan input explicitly forbids. This tradeoff is recorded
here and in `plan.md` Technical Context / Complexity Tracking so it is a
stated design decision, not a surprise discovered via a failing visual test
against SC-001's "no visible seams or gaps at final render resolution."

**Alternatives considered**:
- *Defer operand tessellation to each hider's own dice/intersect call*:
  rejected — reintroduces the hider dependency the plan forbids, and would
  require every hider to run (or call out to) CSG logic itself.
- *Fixed/uniform density derived only from bound diagonal, no adaptivity*:
  rejected as the primary mechanism (though it remains the fallback for
  primitive types with no parametric surface evaluation, e.g. arbitrary
  polygon meshes, which need no further tessellation at all since they are
  already flat-faceted by definition) — wastes triangles on flat regions and
  risks under-tessellating a composite's highest-curvature spot at a density
  tuned by eye against a simpler test case.
- *Tessellate at a very high fixed density unconditionally*: rejected — same
  waste concern, without even the adaptivity benefit.

## Decision 4b: Shading-normal smoothness — analytic per-vertex normals for NURBS/quadric operands

**Decision**: When tessellating a NURBS or quadric leaf operand (Decision 4),
the resulting `CPolygonMesh` fragment is given an analytic per-vertex
shading normal — `crossvv(N, dPdu, dPdv)` evaluated at each sample vertex's
*exact* parametric coordinates on the original smooth surface — stored as
the mesh's `"N"` vertex-class primvar. `CPolygonMesh` already interpolates
any supplied `"N"` primvar barycentrically across a facet during shading
(`CPolygonTriangle::sample`/`interpolate`, `polygons.cpp:280-314,379-405`);
nothing changes there. The always-computed `VARIABLE_NG` geometric normal
(flat per-facet cross product, `polygons.cpp:351-369,437-449`) is left as-is
and continues to serve its existing geometric-normal role (e.g.
`normalFix()`); only the interpolated shading normal is upgraded.

Subdivision-surface operands do **not** get this treatment: `CLoopSubdivMesh`
has no analytic Loop limit-normal evaluation today (linear averaging only,
explicitly disclaimed as "no eigenbasis / extraordinary-vertex limit
evaluation," `subdivisionLoop.cpp:22-27,44-48`), and implementing that is
out of scope for a CSG feature — it is subdivision-surface math unrelated to
boolean resolution, and would also affect ordinary (non-CSG) subdivision
rendering, which this feature does not touch. Subdivision-surface CSG
operands rely on Decision 4's density increase alone to mitigate visible
faceting; this is a known, accepted v1 limitation, not an oversight —
matching the same class of deferred-scope decision already made for
OpenSubdiv-style improvements in this codebase's project history.

**Rationale**: This is the highest-leverage, lowest-cost lever for "smooth
final surfaces": `CPolygonMesh`'s generic vertex-primvar interpolation
already exists and already handles `"N"` when present — the only new work
is *computing* an analytically-correct normal at tessellation time instead
of leaving shading to fall back on flat per-facet normals. It makes shading
look smooth largely independent of raw polygon count, which keeps Decision
4's adaptive-but-still-cost-conscious tessellation from having to
over-tessellate purely to hide faceted shading.

**Alternatives considered**:
- *Flat per-facet normals only, rely purely on density*: rejected — needs
  meaningfully higher polygon counts to avoid visible faceting on curved
  silhouettes than the analytic-normal approach, cutting against a
  correctness-first-but-not-wasteful v1.
- *Implement analytic Loop limit-normal evaluation for subdivision
  operands as part of this feature*: rejected as in-scope for v1 — real,
  separable subdivision-surface math, better scoped as its own future spec
  (mirrors how OpenSubdiv-derived improvements were already deferred per
  `project_subdivision_surfaces_openSubdiv_intent` project history).

## Decision 5: Common coordinate space for combining operands

**Decision**: All leaf operands captured under a solid tree are transformed
into the local space of the **outermost** `SolidBegin` — i.e., the `CXform*`
that is current at the moment the outermost `SolidBegin` is invoked — before
BSP classification. Concretely, each leaf's own `from` (local→world)
transform is composed with the inverse of the outer block's `from` to bring
it into the outer block's local frame. The resolved container `CObject`
produced at `RiSolidEnd` is then given that same outer-block `CXform*`,
exactly as an ordinary primitive declared at that scope would carry it.

**Rationale**: Every `CObject` already carries its own `CXform*` under the
project's column-major, `from`=local→world / `to`=`from`⁻¹ convention
(`CLAUDE.md` Coordinate conventions). Resolving into the outer block's local
space (rather than baking to world space) preserves the existing behavior of
"a primitive declared under a transform carries that transform" and keeps
solids fully compatible with instancing (Decision 1): an instanced solid's
resolved boundary is defined once in the object's own local frame and
correctly re-transformed per `RiObjectInstance`, the same as any other
captured primitive. Baking to world space would silently break that.

**Alternatives considered**:
- *Resolve in world space*: simpler to reason about for a single top-level
  solid, but wrong for a solid declared inside `RiObjectBegin`/`RiObjectEnd`
  — world space does not exist yet at object-definition time, and baking
  it in would prevent correct per-instance re-transformation.

## Decision 6: Test strategy (Principle III, TDD non-negotiable)

**Decision**: The boolean kernel (BSP build + classify/clip/merge) is unit
tested in isolation, independent of any hider, using primitive combinations
with known, hand-computable results: two axis-aligned boxes with a known
overlap volume/face count for union/intersection/difference; a sphere against
a box (validates curved-vs-flat boundary and the tessellation-tolerance
path); and an explicit coplanar-face pair (validates the epsilon-handling
called out as a risk in Decision 3). These are written and approved before
any `SolidBegin`/`SolidEnd` renderer-integration code, per the Red-Green-
Refactor cycle. Renderer-level integration (RIB parsing through to a resolved
`CObject` reaching `addObject()`) and end-to-end visual-regression scenes
(REYES and raytrace hiders producing matching CSG silhouettes) are added
after the kernel is proven, extending the existing `ctest -L visual` /
`ctest -L libshader` patterns.

**Rationale**: Directly satisfies Principle III. The boolean kernel is the
single highest-risk, most algorithmically dense piece of this feature and is
naturally decomposable into pure, hider-independent unit tests — exactly the
kind of critical path the constitution requires coverage for before
integration work begins.

## Resolved open questions

- **Delayed/procedural primitives (`RiProcedural`) as direct CSG operands**:
  resolved as **rejected with a clear diagnostic**, consistent with the
  precedent already established for FR-019 ("primitive" leaf rejecting a
  nested solid block). A procedural's geometry does not exist yet at
  `RiSolidEnd` time (it is expanded lazily, potentially hider-side, per
  `delayed`/`CDelayedObject` in `rendererContext.cpp`), so it cannot be
  tessellated for BSP classification without either forcing eager expansion
  (which may itself have hider-dependent behavior, e.g. detail-based
  culling) or deferring CSG resolution past geometry-build time (forbidden
  by the plan's hard constraint). Forcing eager, hider-independent expansion
  is out of scope for v1; error at `RiSolidEnd` tree-resolution time instead.
- **`addObject()` gate precedence**: the solid gate (`currentSolid`) is
  checked before the instance/delayed gates during capture (a leaf declared
  inside a solid block is always captured into the CSG tree first,
  regardless of whether an `RiObjectBegin`/`RiProcedural` context is also
  active — nesting `SolidBegin` inside those is out of this feature's scope
  per the spec's stated primitive-leaf semantics). The *resolved* composite
  produced at the outermost `RiSolidEnd` re-enters `addObject()` from
  scratch, so it correctly falls through to whichever of the instance/
  delayed/normal gates applies at that point (see Decision 1).
- **RIB block-state enforcement gap**: `RiSolidBegin`/`RiSolidEnd` in
  `src/ri/ri.cpp` currently call `check()` for scope validation but, unlike
  `RiObjectBegin` (`ri.cpp`, `blocks.push(currentBlock); currentBlock =
  RENDERMAN_OBJECT_BLOCK;`), never push/pop `RENDERMAN_SOLID_PRIMITIVE_BLOCK`
  onto the block-state stack. Confirmed by direct inspection
  (`src/ri/ri.cpp:2062-2076`). This must be added as an explicit
  implementation task — without it, `check()`'s scope-mask machinery cannot
  enforce FR-014 (nested solid block validity) or FR-019 ("primitive" leaf
  rejecting a nested solid block); those checks would otherwise have to be
  reimplemented ad hoc from `CRendererContext`'s own `currentSolid` depth,
  duplicating validation logic that `check()` already exists to centralize.

## Technical Context resolution

All `NEEDS CLARIFICATION` markers in the Technical Context are resolved as
follows (see `plan.md`):

- **Language/Version**: C++20 (repository-wide standard, Principle II).
- **Primary Dependencies**: none new — BSP-tree CSG is implemented in-tree
  (Decision 3), per Principle V.
- **Testing**: ctest, extending existing `-L libshader`-style unit tests for
  the boolean kernel and `-L visual` scene regression for end-to-end
  coverage (Decision 6).
- **Target Platform**: Linux/macOS (Principle VI, no new platform surface).
- **Project Type**: existing single-repo C++ renderer; this feature adds to
  `src/ri/`.
- **Performance Goals**: correctness-first, no explicit target for v1
  (clarified in `/speckit-clarify` session 2026-08-26).
- **Constraints**: CSG boundary resolution MUST occur exactly once, in the
  geometry domain, before any hider-specific code runs (plan input, hard
  constraint).
- **Scale/Scope**: bounded by existing scene-complexity norms already
  exercised by the visual-regression suite; no new scale target introduced.
