# Contract: `CHider`, the hiders as owners and tracers (Hiders, renderer core)

**Status**: plan-stage proposal, assuming the recommended answers to P-Q1, P-Q2, P-Q6, P-Q7, P-Q10, P-Q11 and P-Q12
([../research.md](../research.md)).

**Requirements**: FR-007, FR-009, FR-010, FR-016, FR-017, FR-019, FR-021; SC-004, SC-007.
**Files**: new `src/ri/hiders/hider.{h,cpp}`, `src/ri/hiders/shadingProducer.cpp`, `src/ri/hiders/surfacePointSource.h`;
edited `reyes.{h,cpp}`, `stochastic.cpp`, `zbuffer.cpp`, `raytracer.{h,cpp}`, `photon.{h,cpp}`, `trace.cpp`,
`irradianceDispatch.cpp` (none of its code changes; it already reaches the context through a parameter);
deleted `hiderTracer.{h,cpp}`.

## `hider.h`

```cpp
#include "shading.h"           // CShadingContext (Shading; Hiders -> Shading is allowed)
#include "tracer.h"            // ITracer
#include "shadingProducer.h"   // IShadingProducer

// The base of every hider (spec 029). It owns the shading context of its render thread, runs the thread's
// rendering loop, traces the context's rays and produces its shading batches. One hider per render thread,
// created and destroyed by the renderer core with its context.
class CHider : public openrender::shading::ITracer, public openrender::shading::IShadingProducer {
    public:
        explicit CHider(int thread);          // constructs the context, installs itself as its tracer and producer
        virtual ~CHider();                    // the context is destroyed after the derived hider's destructor body
        virtual void renderingLoop() = 0;     // moved from CShadingContext (shading.h:366)
        CShadingContext &shadingContext() { return context; }   // inline: no link symbol (Geometry calls it)
        const CShadingContext &shadingContext() const { return context; }

        // ITracer (tracer.h; signatures unchanged). `ctx` is always this hider's own context (asserted in debug).
        void traceRay(CShadingContext &ctx, CRay *ray) override;            // today's CShadingContext::trace(CRay *)
        void traceBundle(CShadingContext &ctx, CRayBundle *bundle) override; // today's traceEx

        // IShadingProducer (shadingProducer.h): the forwarding entry's target (shadingProducer.cpp)
        void shadeSurface(CShadingContext &ctx, CSurface *surface, int u, int v, EShadingDim dim,
                          unsigned int usedParameters, int displaceOnly) override;

    protected:
        void traceGroups(CRayBundle *bundle); // today's CShadingContext::trace(CRayBundle *): trace, group by object,
                                              // shade, postShade/post, continue (trace.cpp:47-245, verbatim)
        virtual bool rasterizes() const { return false; } // CReyes: true (the frame's hider kind, FR-019)

        CShadingContext context;              // declared first: built before, destroyed after, the rest of the hider

};
```

The bodies of `traceRay`, `traceBundle` and `traceGroups` stay in **`trace.cpp`** with their expressions
unchanged; only the receiver of each member access changes (`context.`), plus:

- `traceBundle` = `CRayLevel saved; if (!context.enterRayLevel(*bundle, CRenderer::maxRayDepth, saved)) {
  bundle->postShade(bundle->numRays, bundle->rays); return; } traceGroups(bundle); context.leaveRayLevel(saved);`
  (`traceEx`'s order: the depth test, the push, the trace, the pop);
- `traceRay` counts through `context.countTracedRay()`;
- the object hash is the context's (`context.objectHash()`, P-Q16 (a)), and its empty-entry sentinel stays the
  context's address: `traceGroups` compares and resets against `(CSurface *)&context`, never the hider's `this`
  (the trap of research R2);
- `object->intersect(&context, ray)`, `object->cluster(&context)`, `shadingGroups->object->shade(&context, ...)`
  keep the Geometry virtuals' `CShadingContext *` signatures (DB-017 `trace.cpp -> CObject::cluster`, line 387,
  stays matched).

`hider.cpp` and `shadingProducer.cpp` reference no `CRenderer::` symbol (a new file would need new DB-011 lines,
FR-028); both join `ribVector`'s `REMOVE_ITEM` list (`src/ri/CMakeLists.txt:365-397`), as `trace.cpp` is.

## The producer (`shadingProducer.cpp`)

`CHider::shadeSurface` builds a `CSurfacePointSource` over the surface (inline adapter, `surfacePointSource.h`:
each member forwards to the unchanged `CSurface` virtual, `object.h:193-194`, and to `CObject::attributes` and
`stableId`), fills the per-batch `ShaderGlobals` by the rules of
[point-source-and-globals.md](point-source-and-globals.md), sets the shaded object
(`setShadedObject(surface, ObjectHandle{static_cast<const CObject *>(surface)}, surface->attributes)` happens
inside `shade()` from the source), and calls `context.shade(globals, source)`.

## The hiders

| Class | Today | After |
|---|---|---|
| `CReyes` | `: public CShadingContext` (`reyes.h:106`) | `: public CHider`; `rasterizes()` returns true; its bare uses of inherited members (77, `reyes.cpp`) read `context.`; `reyes.cpp:1043` passes `&context`; `reyes.cpp:1994` reads `static_cast<CReyes *>(CRenderer::contexts[i])` |
| `CStochastic`, `CZbuffer` | through `CReyes` | unchanged hierarchy; `stochastic.cpp`'s 13 uses read `context.` |
| `CRaytracer` | `: public CShadingContext` (`raytracer.h:145`) | `: public CHider`; `raytracer.cpp:652` calls `traceGroups(&primaryBundle)`, `:687` `traceRay(context, ray)`; 15 uses read `context.` |
| `CPhotonHider` | `: public CShadingContext` (`photon.h:41`), friend, overrides the hook virtuals | `: public CHider, public IIlluminationHook`; installs itself with `context.setIlluminationHook(this)`; `photon.cpp:178` sets the phony object with `setShadedObject`; `:211` reads `context.samplingConfig()`; `:219` passes `&context`; `:607` `traceRay(context, &ray)` |

`CRaytracer` keeps its own `camera` member (`raytracer.h:166`), which today shadows the context's protected
`camera`; after the change nothing shadows anything.

## The renderer core

- `CRenderer::contexts` becomes `CHider **` with the same name (P-Q6 (a); `_ZN9CRenderer8contextsE` stays, DB-011
  line 232 stays matched). `renderer.cpp:815-845`: `contexts[i] = new CRaytracer(...)`, ...;
  `contexts[i]->shadingContext().setSamplingConfig(...)`, `.updateState()`.
- `renderer.cpp:1029`: `dynamic_cast<CReyes *>(contexts[0])`, now from `CHider *` (a downcast of a
  polymorphic type, valid; a `dynamic_cast` from the now non-polymorphic `CShadingContext` would not compile).
- `renderer.cpp:1052` (`rendererDispatchThread`): `CRenderer::activeContext = &contexts[w]->shadingContext();`
  then `contexts[w]->renderingLoop();` (P-Q7 (a)); the writes at `reyes.cpp:229` and `raytracer.cpp:420` go.
- `renderer.cpp:1068`, `rendererJobs.cpp:88-89`, `rendererDeclarationsDispatch.cpp:82`, `imager.cpp:130-134`:
  through `shadingContext()`.
- `rendererStatics.cpp:174` (`frameTracer`), `renderer.h:364-368`, `renderer.cpp:47, 780-781, 866`: deleted.
- `rendererServicesImpl.h:104` (`tracer()` override): deleted (P-Q2 (a)); `:67-72` `getPhotonMap` returns
  `IPhotonLookup *` ([photon-lookup.md](photon-lookup.md)); `:35` keeps asserting MAJOR 2 (the version becomes 2.1.0, [renderer-services-version.md](renderer-services-version.md)).
- `hcshader.cpp`: the hook calls (`:120, 135, 435, 450`) through `context->illuminationHook()` with a null test;
  the attribute reads (`:143, 458`) through `context->objectAttributes()`; the probe rays (`:199, 522`) through
  `context->tracer()->traceRay(*context, &ray)` (not `libshader::traceRay`, which reports to the replay
  observer).
- `geometryDispatch.cpp` (core): `rasterizer->shadingContext()` at `:101, 416, 455, 492, 587-600, 698, 733, 774`.

## Geometry (FR-010: call sites only, no interface change)

| File:line | Today | After |
|---|---|---|
| `surface.cpp:264` | `r->currentShadingState` | `r->shadingContext().currentShadingState` |
| `surface.cpp:304, 332, 334` | `r->displaceEstimate(...)` | `r->shadingContext().displaceEstimate(...)` (same `shade` symbol, DB-006 line 112) |
| `surface.cpp:452` | `classifyBound(r, ...)` | `classifyBound(&r->shadingContext(), ...)` |
| `curves.cpp:720`, `patches.cpp:1604, 1882`, `polygons.cpp:1314`, `subdivisionCreator.cpp:1868`, `subdivisionLoop.cpp:354` | `create(r)` / `create(rasterizer)` | `create(&r->shadingContext())` |

Every virtual keeps `CShadingContext *` or `CReyes *` as today (DB-017's lines are keyed on those mangled names).
The accessor is inline, so no new link symbol; Geometry already includes `reyes.h` (DB-006), which includes
`hider.h`, so no new include line (archcheck counts direct includes).
