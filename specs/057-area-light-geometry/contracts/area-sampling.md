# Contract: area sampling in Geometry, and the shadow-ray exclusion gate

**Requirements**: FR-006-FR-010, FR-017, FR-029; SC-003; US2, US6, US7.
**Domain**: Geometry only (it gains no Hider, Shading or core dependency). **Files**: new
`src/ri/geometry/emitterRegistry.{h,cpp}` (sets, leaves, tables, resolution), `src/ri/geometry/areaSampling.{h,cpp}`
(the per-kind samplers), `object.{h,cpp}` (`collectEmitters`), `polygons.cpp`, `subdivisionCreator.cpp`,
`subdivisionLoop.cpp`, `quadrics.cpp`, `patches.cpp` (P3), `delayed.cpp`, `csgSolid.cpp`, `surface.cpp` (the gate),
`rayContext.h` (the exclusion accessor).

## Collecting a light's leaves

```cpp
// object.h
class CEmitterCollector {                      // Geometry
    public:
        void addLeaf(CSurface *surface, CAreaTable &&table, double area);
        void unsupported(const CObject *object);   // counted, reported once per light
        void csgLeaf();                            // counted, reported once per light (D-4)
};

// CObject: default reports the object as unsupported
virtual void collectEmitters(IRayContext &context, CEmitterCollector &out);
```

Overrides: polygon mesh and polygon (exact face table), subdivision meshes (create the limit patches as `intersect` does,
under the tessellation lock, and add each patch with a tessellated table), quadrics (analytic warp, or tessellated table for
paraboloid/hyperboloid/torus and for any non-uniform scale or skew of the quadric's transform), delayed object and delayed
instance (expand through `CGeometryHost::expandDelayedObject/Instance` if not `processed`, then recurse into the children),
solid (recurse into the leaves, `csgLeaf()` once per leaf), patches and NURBS (P3, decision D-3). Points, curves, blobbies,
implicit and DSO objects keep the default.

## Resolving and drawing

```cpp
class CEmitterSet {
    public:
        // Resolve once (std::call_once): collectEmitters over the roots in RIB order, build the leaf CDF.
        // Reports, once per light: unsupported primitives, CSG leaves, and an empty set (A7).
        void resolve(IRayContext &context) const;
        double totalArea() const;                  // after resolve()
        // A point uniformly by area over every leaf: u1 picks the leaf (then the face/cell) in its CDF and is rescaled
        // (double), (u1', u2) place the point. pdfA = 1 / totalArea(); pdfA = 0 and no point for an empty set.
        void sample(double u1, double u2, CAreaSample &out) const;
};
```

- **Pure** (FR-006): after `resolve()`, `sample()` reads immutable tables only; the same `(u1, u2)` gives bit-identical
  output on any thread (SC-003).
- **Space and time**: tables in camera space at shutter open (A8 evaluates at the receiver's time in stage B, Hiders).
- **What the camera sees** (FR-007): the exact samplers sample the faces and the analytic surface a ray hits; the tessellated
  sampler's `(u,v)` is evaluated on the true surface by stage B. `Orientation`/`ReverseOrientation` are honoured by stage B
  (`sample()` flips `N` as for shading). Displacement is not applied (A6). Trims (P3): a `K x K` cell wholly outside the
  trim is dropped; a straddling cell is kept (documented).
- `K = 8` per patch (subdivision limit patches, P3 patches, the three non-analytic quadrics). Area agreement with a fine
  reference tessellation is a measured test, not a promise of the analytic area.

## The exclusion gate (FR-017)

```cpp
// rayContext.h, in IRayContext (non-virtual; the hider sets the field for the length of one occlusion trace)
public:
    const CShaderGroupRefTarget *excludedEmitter() const { return emitterExclusion; }
protected:
    const CShaderGroupRefTarget *emitterExclusion = nullptr;   // stateHandles.h (Graphics State)
```

Added after the flag test in `CSurface::checkRayGuard`, `CSurface::intersect` and `CTesselationPatch::intersect`:

```cpp
if (attributes->areaLight != nullptr && attributes->areaLight == context.excludedEmitter())
    return /* TRUE for checkRayGuard */;
```

- Every primitive created in the light's scope carries the identity (research R4), so a shadow ray of that light passes
  through every one of them that reaches these three gates (supported emitters and the other `CSurface` kinds alike), at
  any angle, with no bias.
- Outside an occlusion trace the field is `nullptr`, and `attributes->areaLight` is `nullptr` for every object outside
  an area-light scope: no existing ray changes (FR-026).
- `dlobject.cpp` and `implicitSurface.cpp` gates are not changed: those primitives are never emitters, and inside a scope
  they still block their light's samples (documented).


## As built (Phase 3, approved at the test stage)

Files: `src/ri/geometry/areaSampling.{h,cpp}` (new), `emitterRegistry.{h,cpp}`, `object.{h,cpp}`, `polygons.{h,cpp}`,
`subdivision.h`, `subdivisionCreator.h`, `subdivisionLoop.h`, `patches.h` (`CBicubicPatch`), `patchgrid.h`, `delayed.{h,cpp}`,
`csgSolid.{h,cpp}`. Geometry still includes nothing of Shading, Hiders or the core.

```cpp
// areaSampling.h
struct CAreaSample { const CSurface *leaf; float u, v; float P[3]; float Ng[3]; float pdfA; };   // Ng unit, along dPdu x dPdv
enum class EAreaMap { Affine, PolygonTriangle, BilinearQuad };
class CAreaTable;                      // faces (flat triangles), cdf, area, the map to (u, v), an optional evaluating surface
CAreaTable makeTriangleTable(v0, v1, v2);                        // exact: CPolygonTriangle's parametrization
CAreaTable makeQuadTable(surface, v0, v1, v2, v3);               // exact when planar and convex along a diagonal, else tessellated
CAreaTable makeTessellatedTable(surface, K = kAreaTessellationK = 8);   // K x K cells, two flat triangles each
size_t areaPickBin(cdf, u, rescaled);                            // never a zero-width bin; u rescaled into the bin in double

// object.h
class CEmitterCollector { addLeaf(CSurface *, CAreaTable &&, double area); unsupported(const CObject *); csgLeaf(); };  // abstract
virtual void CObject::collectEmitters(IRayContext &, CEmitterCollector &);   // default: unsupported(this)
void CObject::collectChildEmitters(IRayContext &, CEmitterCollector &);      // a container's collectEmitters
void CSurface::collectTessellatedEmitter(CEmitterCollector &);                // the free-form leaves call it

// emitterRegistry.h (CEmitterSet)
void resolve(IRayContext &) const;               // std::call_once; also the three reports below
double totalArea() const; int numLeaves() const; const CSurface *leafSurface(int) const; double leafArea(int) const;
void sample(double u1, double u2, CAreaSample &) const;   // pure; pdfA == 0 and leaf == nullptr for an empty set
mutable int numUnsupported, numCsgLeaves;        // public counters
const char *name() const;                        // the light shader's name (declareLight's optional fourth argument)
```

- **Leaf and table design.** A polygon mesh's leaves are its triangles and quads (created as a ray hit creates them,
  `materialize`), each with its own table; a leaf of area 0 is kept in the set with an empty bin and is never chosen. Exact
  samplers for triangles and planar convex quads (a quad is two triangles chosen by area and mapped back to its `(u,v)` by
  inverse bilinear); a `K = 8` tessellated table for non-planar quads, subdivision limit patches (`CSubdivision`,
  `CBicubicPatch`, `CPatchGrid`) and later the free-form patches. A tessellated leaf returns the **true surface** point and normal
  at the drawn `(u,v)` (`CSurface::samplePoint`); its density is `1 / (tessellated area)`. A `CLoopSubdivMesh`'s only child is
  the refined `CPolygonMesh`, so its leaves are exact triangles. `blobbyCreate()` polygonizes a blobby into a `CPolygonMesh`
  when it is created, so a blobby is a mesh emitter.
- **Which class a statement creates.** `RiPolygon`, `RiGeneralPolygon`, `RiPointsPolygons` and `RiPointsGeneralPolygons` each create
  one `CPolygonMesh` (`rendererContext.cpp`); `create()` transforms the vertex list to camera space in place and triangulates
  (a 4-vertex single loop becomes one `CPolygonQuad`, any other polygon `CPolygonTriangle`s).
- **The lock.** `collectEmitters` of a mesh takes no lock of its own: it calls `materialize()`, which is `if (children == NULL) create()`
  and `create()` takes the mesh's own `mutex` (the lock `intersect` takes). For `CSubdivMesh` and `CLoopSubdivMesh` likewise
  (their `create()` locks `mutex`); a delayed object or instance expands through `CGeometryHost::expandDelayedObject/Instance`
  (serialized, idempotent, `processed` tested again under the host's lock); a solid through `CCSGSolid::resolve()`.
- **Reports** (once per light, from `resolve()`, `warning(CODE_BADTOKEN, ...)`, never `CODE_LOG`):
  `area light "<name>": <n> primitive(s) cannot emit (...)`, `area light "<name>": <n> CSG leaf(ves) emit from the whole leaf,
  without their solid's region`, and `area light "<name>" has no emitting geometry`.
- **Lifetime.** The set attaches every leaf in `addLeaf` and lets go with the registry (before `root->destroy()`); see
  plan.md "Phase 4 default".
