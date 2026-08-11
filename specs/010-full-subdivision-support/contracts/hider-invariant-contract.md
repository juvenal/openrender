# Contract: No Hider-Specific Subdivision Code (FR-012/FR-013)

**Feature**: `010-full-subdivision-support`

Unlike `contracts/shared-trim-test-contract.md` in spec 009 (which documented a *new* shared abstraction built for
that feature), this contract documents an **existing invariant** — every hider already reaches subdivision-surface
geometry purely through `CObject`/`CSurface` virtual dispatch, with zero subdivision-specific branches anywhere
under a hider file — and defines the regression check (FR-013) that keeps it true as this feature's five other
tiers (facevarying fix, new tags, crease fix, hierarchical edits, Loop scheme) land.

## The invariant

No file under any of:

```
src/ri/stochastic.cpp
src/ri/reyes.cpp
src/ri/zbuffer.cpp
src/ri/raytracer.cpp
src/ri/trace.cpp
src/ri/photon.cpp
src/ri/show.cpp
```

may contain a branch, type check, or downcast referencing any subdivision-specific type — today (`CSubdivMesh`,
`CSubdivision`) or any type this feature introduces (a hierarchical-edit type, a Loop-scheme type). Every hider
reaches subdivision geometry exclusively through the `CObject`/`CSurface` contract declared at `src/ri/object.h:
60-144`:

```
60:class CObject : public CRefCounter {
75:    virtual void intersect(CShadingContext *, CRay *) = 0;
76:    virtual void dice(CReyes *);
77:    virtual void instantiate(CAttributes *, CXform *, CRiInterface *) const = 0;
117:class CSurface : public CObject {
123:    virtual void intersect(CShadingContext *, CRay *);
124:    virtual void dice(CReyes *);
127:    virtual int moving() const;
128:    virtual void sample(int, int, float **, float ***, unsigned int &) const;
129:    virtual void interpolate(int, float **, float ***) const;
130:    virtual void shade(CShadingContext *, int, CRay **);
139:    virtual bool trimAccepts(float /*u*/, float /*v*/) const { return TRUE; }
144:    virtual bool hasTrim() const { return FALSE; }
```

`trimAccepts()`/`hasTrim()` (spec 009's additions) are the precedent for how a per-primitive-type behavior gets a
safe, generic default on the base class instead of a hider-side type check — any new subdivision-specific behavior
this feature needs at the hider boundary (there should be none; see below) must follow the same pattern, not add a
`dynamic_cast`.

## What this feature must NOT do at any hider file

- **Facevarying fix (User Story 2)**: entirely inside `CSVertex`/`CSFace`/`CSEdge`'s `computeVarying()` chain —
  `subdivisionCreator.cpp`. No hider touches facevarying storage directly.
- **New tags (User Story 3)**: parsed and stored entirely inside `subdivisionCreator.cpp`'s `create()`. No hider
  file parses or queries a subdivision tag.
- **Crease fix (User Story 4)**: whatever the root cause turns out to be, any fix must land in
  `subdivisionCreator.cpp`/`subdivision.cpp` only (FR-006's own gate: root-cause first, and Acceptance Scenario 3
  requires "the fix lives entirely in the geometry layer").
- **Hierarchical edits (User Story 5)**: override resolution lives in the geometry layer only (FR-008); the RIB
  grammar/RI-entry-point/RIB-output/preview/binding surface (`contracts/hierarchical-subdivision-contract.md`) is
  new *parallel* code alongside the existing single-level path, not a hider-side change.
- **Loop scheme (User Story 6)**: scheme selection happens once, in `RiSubdivisionMeshV` (`rendererContext.cpp`)
  and the geometry layer; no hider file branches on scheme (FR-011).

## Consumers (every hider, by construction, needs no change)

### `CStochastic`/`CReyes` (REYES/stochastic)
- **File**: `src/ri/stochastic.cpp`, `src/ri/reyes.cpp`
- **Call site**: dispatches to `CObject::dice(CReyes *)` — never downcasts to `CSubdivMesh`.
- **Contract**: receives diced micropolygon grids identically for every primitive type, including subdivision
  surfaces and (after this feature) Loop-scheme surfaces and resolved hierarchical-edit meshes.

### `CZbuffer`
- **File**: `src/ri/zbuffer.cpp`
- **Call site**: same `dice()`/rasterize path as REYES — no subdivision-specific code exists or is added.

### `CRaytracer`/`CTesselationPatch` (ray-tracing)
- **File**: `src/ri/raytracer.cpp`, `src/ri/surface.cpp` (`CTesselationPatch`)
- **Call site**: `CObject::intersect(CShadingContext *, CRay *)`, and for moving geometry,
  `CTesselationPatch::sampleTesselation()`/`intersect()` (`surface.cpp:1393-1513`, `1164-1319`) per research.md R1
  — reached via `object.cpp:533-574`'s generic dispatch.
- **Contract**: this is the consumer User Story 1 verifies, not modifies. Zero new code expected here across all
  six tiers of this feature.

### `CTrace` (shadow/shading rays)
- **File**: `src/ri/trace.cpp`
- **Call site**: same `intersect()` contract as `CRaytracer`.

### `CPhotonHider`
- **File**: `src/ri/photon.cpp`
- **Contract**: reaches subdivision geometry the same way; per spec.md's Clarifications, photon-hider motion blur
  is out of scope for this feature (like `CShow`) but every *other* capability (facevarying, new tags, hierarchical
  edits, Loop scheme) must work identically here too, since none of them needs a new photon-hider mechanism.

### `CShow` (debug/visualization hider)
- **File**: `src/ri/show.cpp`
- **Contract**: same generic dispatch; per spec.md's Edge Cases, `CShow` test scenes for this feature are authored
  as deliverables but not required to pass, since `CShow` itself is a separate, pre-existing non-functional gap.

## The regression check (FR-013/SC-008)

A grep-based check with zero expected matches, scoped precisely to the seven hider files above (explicitly
**excluding** `src/preview/libribpreview/previewContext.cpp:92`'s pre-existing `dynamic_cast<CSubdivMesh *>` — that
file is the standalone preview/wireframe tool, not a hider, and its downcast is a legitimate, out-of-scope,
unrelated consumer):

```bash
grep -rln 'CSubdiv\|CLoopSubdiv\|CHierarchical' \
  src/ri/stochastic.cpp src/ri/reyes.cpp src/ri/zbuffer.cpp \
  src/ri/raytracer.cpp src/ri/trace.cpp src/ri/photon.cpp src/ri/show.cpp
```

Zero output = invariant holds. This check runs before and after this feature lands (SC-008), and should be wired
into the same place spec 009's equivalent check (if any) or a new lightweight CI/ctest step lives, per Constitution
Principle III/Development Workflow's "Integration tests MUST cover error handling and edge cases" and the general
code-review-requirements expectation that constitution compliance is verified before merge.

## Invariant this contract protects

Every hider stays generic. Adding subdivision-surface completeness (or, later, any other primitive/algorithm) never
requires touching hider code — this is what makes the geometry/hider/shading separation load-bearing rather than
aspirational, and it is the same guarantee that let spec 009's NURBS trim curves land without a single hider-file
change.
