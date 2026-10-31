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
// the hiders themselves from spec 029); reached through CRendererServices::tracer().
// Version: kRendererServicesVersion (rendererServices.h).
class ITracer {
    public:
        virtual ~ITracer() = default;

        // Trace one ray and do not shade it.
        // In:  from, dir (unit length), t (maximum distance), tmin (bias), time, flags, da, db,
        //      key, depth (spec 039).
        // Out: object (null on a miss), u, v, N, t (the hit distance).
        // Thread-safe: called by every render thread, each with its own context.
        // Re-enters Shading: never.
        virtual void traceRay(CShadingContext &context, CRay *ray) = 0;

        // Trace a bundle one level deeper and shade what it hits: the contract of today's
        // CShadingContext::traceEx(). Past the maximum ray depth, every ray is reported through
        // bundle->postShade(numRays, rays) (a miss). Otherwise the rays are traced, and the
        // bundle's postTraceAction()/postShade()/post() protocol runs until the bundle has no
        // rays left.
        // Thread-safe: as traceRay.
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

- **`CRecordingTracer`**: appends every request (kind, label, bundle depth, path depth, every ray's
  inputs) to a vector. It optionally forwards to an inner tracer.
  - In the standalone program it has no inner tracer. It never calls the bundle protocol there, so the
    stub's traps stay unreached.
  - For `traceRay` it may answer canned hits (`object` = a sentinel pointer, `t`).
- **`CCannedTracer`** (rendered program only): drives the real bundle protocol with canned answers.
  - It calls `postTraceAction()`.
  - It calls `postShade(n, rays, varying)` with canned `Ci`/`Oi` written into
    `context.currentShadingState->varying` for "hit" rays, and `postShade(n, rays)` for "miss" rays.
  - It calls `post()`, repeating while `bundle->numRays > 0`, as `trace.cpp:77-247` does.
