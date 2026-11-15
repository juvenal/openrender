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
