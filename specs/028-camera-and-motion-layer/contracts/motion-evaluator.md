# Contract: the motion evaluator

**Owner**: Geometry. **File**: `src/ri/geometry/motion.h` (header-only, inline).
**Includes**: `common/algebra.h`, `common/mathSpec.h` (Infra), `core/ray.h` (Infra, `CRay`),
`state/xform.h` and `state/frameParams.h` (Graphics State). Never `renderer.h`,
`rendererContext.h`, `shading.h` or a hider header (FR-012).
**Users in 028**: every primitive family (phases 5 to 11). **Later user**: Reyes (spec 031).
**Requirements**: FR-011 to FR-016, FR-018, FR-019, FR-021(a); maintainer decisions 2, 3 and 5.

## The two-key model: one definition

```cpp
namespace openrender::motion {

    // Shutter open is t = 0, shutter close t = 1. Linear and unclamped: t outside 0..1
    // extrapolates, exactly as every site does today. The only definition of the model.
    template <class S> inline S atTime(S key0, S key1, S t) { return key0 * (S(1) - t) + key1 * t; }

    // Per-site precision (decision 3): instantiations of the template, named at the call site.
    inline float atTimeD(float k0, float k1, float t) { return (float)atTime<double>(k0, k1, t); } // double-evaluated
    inline float atTimeF(float k0, float k1, float t) { return atTime<float>(k0, k1, t); }         // float-evaluated
    inline void atTimeV(float *dst, const float *k0, const float *k1, float t);                    // 3 x atTimeD
    // Static object: k1 == nullptr returns k0 itself (its own data, unchanged); else fills scratch.
    inline const float *keyAtTime(float *scratch, const float *k0, const float *k1, float t);
}
```

The polygons' fused sites call `atTime<double>(spatial0, spatial1, ctime)` on their own double
expressions and cast the result to float, as they do today.

**`ctime` types.** `atTimeD` and `atTimeF` take `float t`. A grid-sampling site that declares
`const double ctime` (`patchgrid.cpp:243`, the quadrics' `sample()`) initialises it from a float
array (`*time++`), so `ctime` holds a float value exactly and `atTimeD(.., ctime)` narrows it
without loss. Each such site's initialiser is checked when it is migrated (tasks T064 to T070,
T095); a `ctime` computed in double that is not exactly a float is stopped and reported.
**`bsplinePatchgrid.cpp:233-236` is a mixed shape and does not fit either entry point**: its
`const float ctime` makes `vertex1[j] * ctime` a float product (rounded to float) beside a double
`vertex0[j] * (1.0 - ctime)`, which neither `atTimeD` (both double) nor `atTimeF` (both float)
reproduces. **Maintainer decision (2026-10-27): that one lerp stays in place** in 028 (its `dPdtime`
scale still goes through `toShutterRate`); no entry point is added, and it is noted for a later
precision-unification phase.

**Expression shape** (research R9): `key0 * (S(1) - t) + key1 * t`, in this operand order, is the
shape of every starting-tree site (`interpolatev`, the quadrics, the patches, the polygons, the
patch grids, `subdivision.cpp`). A site whose shape differs is not migrated onto `atTime` without
the maintainer's approval in a later output-changing phase (FR-014).

## Shutter keys and the dPdtime scale

```cpp
    inline int sampleKey(unsigned int up);   // 1 if (up & PARAMETER_END_SAMPLE), else 0
    inline float shutterRate();              // frameParams().invShutterTime (decision 5)
    inline void toShutterRate(float *v);     // mulvf(v, shutterRate())
```

`sampleKey` replaces the begin/end selections (`(moving == FALSE) || (up & PARAMETER_BEGIN_SAMPLE)`
picks key 0) where a family writes them; a family keeps its own `moving` test. Every `dPdtime` site
computes `difference(key1, key0)` as today and calls `toShutterRate` instead of
`mulvf(dest, CRenderer::invShutterTime)`. A loop may hoist `shutterRate()` (same value).

## Grid transforms at time (moved verbatim from `objectMisc.h:66-139`)

```cpp
    inline void transformAtTime(float *P, int n, const float *from0, const float *from1, const float *time);
    inline void vtransformAtTime(float *v, int n, const float *from0, const float *from1, const float *time);
    inline void ntransformAtTime(float *N, int n, const float *to0, const float *to1, const float *time);
```

Each is `mulmp`/`mulmv`/`mulmn` by both matrices, then `atTimeV` at `time[i]`. Used by the quadrics'
`transformPoints()` macro (`quadrics.cpp:48-80`).

## The ray transform and the camera SLERP (FR-019, FR-021)

```cpp
    // M = qtoR(slerp(identity, q, t)). The primitive Reyes's copies also compute (R8).
    inline void cameraRotationAt(float *M, const float *q, float t);

    // Object space ray at ray->time. Moved verbatim from objectMisc.h:178-232. With
    // xform->cameraMotion and frameParams().camera.hasRotation, the direction is
    // toWorld * cameraRotationAt(conj(relRotQ), t) * ray->dir; otherwise both endpoints are
    // transformed by xform->to and xform->next->to and interpolated with atTimeV.
    inline void transformRay(float *oFrom, float *oDir, const CXform *xform, const CRay *ray);
```

It composes only what `CRendererContext::addObject` synthesized: an object with its own transform
motion has `cameraMotion == false` and gets no camera rotation (GitHub #106 preserved, FR-021(b)).

## Plug-in time

```cpp
    inline float pluginTime(const CRay &ray);   // ray.time: the unclamped shutter fraction (R11)
```

## What the evaluator does not do

- It never feeds the CSG region predicate or `regionCoverage` an interpolated position: they keep
  classifying at shutter open, undisplaced (`csgDefinition.cpp:137-145`, FR-015).
- It holds no state, allocates nothing and takes no lock.
- It does not touch Reyes's motion code (`reyes.cpp`, `stochastic*`, `zbuffer*`) or the dicing
  probes in `surface.cpp`/`object.cpp` (spec 031, FR-018).

## Site map (what each phase moves)

| Phase | File: site | Starting-tree code | Evaluator call |
|---|---|---|---|
| 5 | `objectMisc.h:178-232` | `transform(oFrom, oDir, xform, ray)` | `transformRay` (deleted from `objectMisc.h`) |
| 5 | `dlobject.cpp:99`, `quadrics.cpp:197,687,1057,1500,1956,2402,2957` | `transform(oFrom, oDir, xform, rv)` | `motion::transformRay(...)` |
| 6 | `quadrics.cpp` intersect parameters (`:200-203` and the six others) | `(float)(p * (1.0 - t) + next * (double)t)` | `atTimeD(p, next, t)` |
| 6 | `quadrics.cpp` `sample()` (`:327-331` and the six others) | `ctime` lerp of the parameters | `atTimeD` |
| 6 | `quadrics.cpp:48-80` | `transform/vtransform/ntransform(..., time)` | `*transformAtTime` (deleted from `objectMisc.h`) |
| 6 | `quadrics.cpp` dPdtime (14 reads) | `mulvf(dest, CRenderer::invShutterTime)` | `toShutterRate(dest)` |
| 7 | `patches.cpp:228-234` | `interpolatev(t0, P00, P00 + 4 * vertexSize, time)` | `atTimeV` |
| 7 | `patches.cpp:372-374, 692-695` | key lerp, `float` keys | `atTimeD` |
| 7 | `patches.cpp:1022-1024` | key lerp, **`double` keys** into a `double *` | `(float)atTime<double>(...)` |
| 7 | `patches.cpp:481, 793, 1311` | dPdtime scale | `toShutterRate` |
| 8 | `polygons.cpp:183-187, 730-736` | `interpolatev` | `atTimeV` |
| 8 | `polygons.cpp:309-372, 879-922` | fused double lerps, `interpolatev(normal)` | `atTime<double>`, `atTimeV` |
| 8 | `polygons.cpp:482, 1046` | dPdtime scale | `toShutterRate` |
| 9 | `implicitSurface.cpp:142, 155, 176` | `ray->time` | `pluginTime(*ray)` |
| 10 | `surface.cpp:1453-1600` (`CTesselationPatch::intersect`, moving branch) | `interpolatev(P.., cP0, cP1, timev)` | `atTimeV` |
| 11 | `curves.cpp:385, 517`; `points.cpp:241`; `patchgrid.cpp`; `bsplinePatchgrid.cpp` | dPdtime scale, key lerps, key selection | `toShutterRate`, `atTimeD`, `sampleKey` |
| 11 | `subdivision.cpp:176-191` (Q2) | float key lerp | `atTimeF` |
| 11 | `objectMisc.h:145-172` | `interpolate()`, `interpolate3()` (no caller) | deleted |

Exact line ranges are re-read at implementation time; the table is the plan's map, not a patch.

## Tests

| Test | Links | Covers |
|---|---|---|
| `Motion_TwoKeyModel` | State-only library + `libRIBCommon` | decision 3: every entry point is the template; equality with verbatim copies of `interpolatev`, a quadric parameter line, a polygon fused line and `subdivision.cpp:186`; `t = 0`/`1` exact; extrapolation; `keyAtTime` pointer identity for static keys |
| `Motion_Isolation` | same, narrow include path (no `src/ri/render`, `src/libshader`, `src/ri/hiders`, nor `src/ri/core`, which holds the Shading shims; `ray.h` and `ri_config.h` are staged copies, as spec 025 staged `ri_config.h`) | US2 AS5, FR-012: a test double supplies a `CRay`, a `CXform` and an installed `FrameParams`; `transformRay` and `toShutterRate` work with no hider, `CRenderer` or shading context in the link line |
| `Motion_CameraSlerpEquivalence` | same | FR-021(a), research R8 |
| `Motion_TransformRayEquivalence` | same | FR-019: `transformRay` against a verbatim copy of `objectMisc.h:178-232`, `memcmp`, for object motion, camera translation, camera rotation and rotation-only |
| `Motion_Replay_<family>` | `ribRender` | US2 AS1-AS4, FR-013, FR-014, FR-016, FR-018: see [replay-format.md](replay-format.md) |
