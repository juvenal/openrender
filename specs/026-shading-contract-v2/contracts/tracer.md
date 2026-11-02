# Contract: `ITracer` and the tracer service (Shading)

**Decisions**: every plan question referenced here (Q1-Q14) was decided on 2026-10-30. The maintainer accepted each recommended answer; they are recorded in spec.md, Clarifications, "Session 2026-10-30 (plan-stage decisions)". Where this file says "(Qn)", the design described is the decided one.

**Requirements**: FR-010, FR-011, FR-012; SC-005, SC-011.
**Files**:

- `src/libshader/shading/tracer.h` (new);
- `src/libshader/shading/traceCalls.{h,cpp}` (new);
- `src/ri/hiders/hiderTracer.{h,cpp}` (new, Hiders);
- `src/ri/render/renderer.{h,cpp}` and `rendererStatics.cpp`: the frame slot.

## `tracer.h`

```cpp
#include "serviceTypes.h"

class CShadingContext;
class CRay;       // src/ri/core/ray.h (Infra)
class CRayBundle; // shading.h

// Traces rays for Shading. Owned by Shading; implemented by the hiders (an adapter in 026,
// the hiders themselves from spec 029). Version: kRendererServicesVersion (rendererServices.h).
// Introduced in 2.0.0. Obtained from CRendererServices::tracer() (rendererServices.h); the renderer's
// implementation, CHiderTracer, forwards to the hider's trace() and traceEx(). With no tracer Shading
// warns once per kind and treats every ray as a miss.
//
// The fields of CRay are in src/ri/core/ray.h and those of CRayBundle in shading.h (class CRayBundle).
//
// Space and units: every vector is in camera space (the "current" system); distances are camera-space
// units; `time` is the normalised shutter time, 0 <= time <= 1.
class ITracer {
    public:
        virtual ~ITracer() = default;

        // Trace one ray and do not shade it.
        // In (set by the caller):
        //   from, dir      origin and direction; dir must be of unit length;
        //   t              the maximum distance to look for a hit; tmin the bias (the ray starts at tmin);
        //   time           the ray's time (0..1);
        //   flags          the visibility mask of the ray: ATTRIBUTES_FLAGS_*_VISIBLE bits; a primitive
        //                  is a candidate only if its attributes carry one of them (primary, photon,
        //                  specular, diffuse, transmission);
        //   da, db         the linear ray differential: the ray's footprint at distance t is da * t + db;
        //   key, depth     the ray's sample identity (spec 039) and its depth, 0 for a primary ray;
        //   cameraImportance  true if jimp already holds the camera importance (trace() then keeps it).
        // Out:
        //   object   the intersected surface, or nullptr for a miss. Shading may dereference a non-null
        //            object (to read its attributes and to shade it), so it must point at a live surface
        //            for the rest of the call;
        //   u, v     the parametric coordinates of the hit;
        //   N        the surface normal at the hit, camera space;
        //   t        overwritten with the hit distance on a hit; unchanged on a miss;
        //   jimp     written (the ray's jittered importance, drawn from key and depth) unless
        //            cameraImportance is true, in which case it is kept;
        //   invDir   written: 1 / dir, component-wise, in double precision.
        //            jimp and invDir are working data of the hiders: only the hiders read them, Shading
        //            never does, so a tracer that does not use them may leave them unwritten.
        // A miss leaves u, v and N unspecified, and sets object to nullptr.
        // Thread-safety: called by every render thread, each with its own context.
        // Re-enters Shading: never.
        virtual void traceRay(CShadingContext &context, CRay *ray) = 0;

        // Trace a bundle one level deeper and shade what it hits: the contract of today's
        // CShadingContext::traceEx(). The bundle (shading.h) holds numRays, rays (an array of CRay *),
        // a label, a transparency depth, and the protocol below. The ray inputs are those of traceRay.
        //
        // The depth rule and the new shading state live in the context: the current ray depth
        // (currentRayDepth) and the list of free shading states (freeStates) are private to
        // CShadingContext, while newState() is public. An implementer outside the context therefore gets
        // the depth rule and the state recycling only by calling context.traceEx(bundle), the one public
        // entry that applies them (CHiderTracer does exactly that); one that does not call it must keep
        // and apply its own depth count, and cannot read or change the context's.
        // The maximum ray depth is the Option "trace" "maxdepth" (default 5), compared with the
        // context's current ray depth. At the maximum (no deeper level is allowed), nothing is traced:
        // the bundle gets ONE call bundle->postShade(numRays, rays), the miss form (without a varying
        // array), for all its rays, and no postTraceAction() or post() call.
        // Below the maximum:
        //   1. every ray is traced (the renderer's clip planes shorten ray->t), as traceRay does;
        //   2. bundle->postTraceAction() is called; it returns FALSE to stop (the pass ends, nothing is
        //      shaded or reported, and the call returns) and TRUE to go on;
        //   3. the rays are split by what they hit. Hits (ray->object != nullptr) are grouped per
        //      object, shaded in chunks of at most maxGridSize() rays in a new shading state one ray
        //      level deeper, and for each chunk bundle->postShade(n, rays, varying) is called: `rays` are
        //      the n rays of the chunk, and `varying` is that state's array of per-variable buffers,
        //      indexed by VARIABLE_*, where the result of ray k is at varying[VARIABLE_CI] + 3 * k
        //      (colour, 3 floats) and varying[VARIABLE_OI] + 3 * k (opacity, 3 floats), and likewise for
        //      any other variable the bundle asked for. A tracer that builds its own varying array (a
        //      double) learns which variables to fill from its bundle: CTraceBundle reads Ci and Oi,
        //      CTransmissionBundle reads Oi, and CGatherBundle reads the variables named by the shader's
        //      gather outputs. The array has one buffer per global variable
        //      (CRendererServices::numGlobalVariables(), indexed by VARIABLE_*), each at least
        //      numFloats(variable) * n floats for a varying variable (n <= maxGridSize()) and numFloats
        //      for a uniform or constant one, both from CRendererServices::globalVariable(i). Misses
        //      (object == nullptr) are reported with bundle->postShade(n, rays), without a varying array;
        //   4. bundle->post() is called;
        //   5. numRays and rays are RE-READ from the bundle: when numRays is 0 the call returns;
        //      otherwise every remaining ray is continued (tmin = t + a small epsilon, t = infinity)
        //      and traced, and the sequence repeats from step 2. The bundle rewrites its rays array in
        //      place between passes.
        // Thread-safety: as traceRay.
        // Re-enters Shading: yes. Hits are shaded through shade() on `context`, in a new shading
        // state one ray level deeper, and a shader there may trace again.
        virtual void traceBundle(CShadingContext &context, CRayBundle *bundle) = 0;
};
```

## `traceCalls.h`: the one place Shading hands a ray to the tracer

```cpp
namespace libshader {

    // The kinds a missing tracer warns about, once each per process (CODE_NOTSTARTED).
    enum class TraceKind : uint8_t { Trace, Transmission, Visibility, Gather, Occlusion, IndirectDiffuse };

    // context.getServices()->tracer()->traceRay(context, ray). With no services or no tracer:
    // warn once for `kind`, then ray->object = nullptr (a miss).
    void traceRay(CShadingContext &context, CRay *ray, TraceKind kind);

    // ...->traceBundle(context, bundle). With no services or no tracer: warn once for `kind`,
    // then bundle->postShade(bundle->numRays, bundle->rays), the path traceEx() takes past the
    // maximum ray depth.
    void traceBundle(CShadingContext &context, CRayBundle *bundle, TraceKind kind);

} // namespace libshader
```

Both helpers call `CShadingContext::traceRequestObserver` first when it is set (a test hook, null in
production, Q7; [../research.md](../research.md) R14).

Call sites rewritten to go through them. They are shared by the interpreter and the JIT, so both
backends issue identical requests:

| Site | Today | Kind |
|---|---|---|
| `executeMisc.cpp:793` | `trace(cRay)`, the transmission/visibility probe | `Transmission` or `Visibility`, chosen by the caller |
| `executeMisc.cpp:805, 823` | `traceEx(&bundle)` in `traceTransmission` | `Transmission` |
| `executeMisc.cpp:939` | `trace(cRay)`, the reflection probe | `Trace` |
| `executeMisc.cpp:954, 971, 990, 999` | `traceEx(&exteriorBundle/&interiorBundle)` in `traceReflection` | `Trace` |
| `shading.cpp:4487, 4498` | `traceEx(lastGather)` in `gatherSample` | `Gather` |
| `irradianceDispatch.cpp:371` / `:471` (Hiders, Q6) | `context->trace(&ray)` in `CIrradianceCache::sample` | `Occlusion` / `IndirectDiffuse` |

The ray-traced `environment()` and `shadow()` reach `traceReflection`/`traceTransmission`
(`shading.cpp:3296-3298`), as do the JIT's `jitTraceBatch` (`:3432-3434`) and the interpreter's
`giFunctions.h:98-154`.

Calls that stay direct:

- the hiders' own rays: `raytracer.cpp:652, 687`, `photon.cpp:607`, `trace.cpp:74`;
- the core's hard-coded lights (`hcshader.cpp:199, 522`);
- `radiance.cpp:1107` (`CRenderer::trace`).

None of these is a Shading call site or an RSL builtin.

## `CHiderTracer` (Hiders)

```cpp
#include "tracer.h"
#include "shading.h"

class CHiderTracer final : public ITracer {
    public:
        void traceRay(CShadingContext &context, CRay *ray) override { context.trace(ray); }
        void traceBundle(CShadingContext &context, CRayBundle *bundle) override { context.traceEx(bundle); }
};
static_assert(openrender::shading::kRendererServicesVersion.major == 2);
```

- `CShadingContext::trace(CRay *)` and `traceEx(CRayBundle *)` are unchanged, still defined in
  `trace.cpp`.
- Hiders -> Shading is allowed.
- The adapter holds no state.

## Installation and the frame slot

- `CRenderer::frameTracer` (`ITracer *`, `rendererStatics.cpp`): `nullptr` between frames.
- `beginFrame()`: `frameTracer = &hiderTracer;` (a function-local static `CHiderTracer`). This runs
  right after `textureSystem = createTextureSystem(...)` (`renderer.cpp:764`), before
  `beginFrameParams()` and before any context is constructed (`:799`).
- `endFrame()`: `frameTracer = nullptr;` right after the contexts are deleted (`:843-847`), next to
  `endFrameParams()`.
- `CRendererServicesImpl::tracer()` returns `CRenderer::frameTracer`.

**No two hiders can overwrite each other** (FR-010). This is argued here from the code, not tested (accepted, 2026-10-31):

1. **One hider class per frame.** All of a frame's contexts are created by the same
   `strcmp(hider, ...)` chain (`renderer.cpp:800-823`); a photon pass is its own frame.
2. **The adapter is stateless.** All trace state is per context (`currentRayDepth`,
   `currentRayLabel`, `freeStates`, `traceObjectHash`), and the context is the call's argument.
3. **The slot is written only while no render thread exists.** The imager runs inside the frame
   (`rendererDisplay.cpp:315`).

## Edges

- **Shading after the change**: the shading library references no `CShadingContext::trace(CRay *)`
  and no `traceEx`.
- **DB-001**: `allowlist.txt:15, 16, 21` go stale and are deleted (Q1).
- **Imports**: macOS flat-namespace imports 6 -> 4, a subset of the starting set (SC-011).
- **DB-005**: unchanged. `bundles.h` is still included, and the bundles are still constructed in
  Shading (spec 029).

## Doubles (`tests/shading_contract/`)

- **`CRecordingTracer`**: appends every request (bundle or single ray, label, bundle depth, path depth,
  every ray's inputs; no "kind" of call site) to a vector. It optionally forwards to an inner tracer.
  - In the standalone program it has no inner tracer. It never calls the bundle protocol there, so the
    stub's traps stay unreached.
  - For `traceRay` it may answer canned hits (`object` = a sentinel pointer, `t`).
- **`CCannedTracer`** (rendered program only): drives the real bundle protocol with canned answers.
  - It calls `postTraceAction()`.
  - It calls `postShade(n, rays, varying)` for "hit" rays with canned `Ci`/`Oi`, and `postShade(n, rays)` for
    "miss" rays. The `varying` array is the double's own scratch: one zero-filled buffer for each of the
    first `VARIABLE_CONSTANTWIDTH + 1` globals, each `4 * numRays + 16` floats, not the context's shading state
    (nothing is shaded). Only `Ci` and `Oi` are filled.
  - It calls `post()`, repeating while `bundle->numRays > 0`, as `trace.cpp:77-247` does.
