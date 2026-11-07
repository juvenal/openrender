# Data Model: Geometry, Hider-Agnostic (spec 031)

Plan-stage proposal, assuming the recommended answers in [plan.md](plan.md) "Questions for the maintainer". The
declarations are in [contracts/](contracts/); the evidence is in [research.md](research.md).

## Entities and ownership

```text
CRenderer (renderer core, composition root)
 ├─ builds and installs, once per frame:
 │    FrameParams       (Graphics State, spec 028)   + cocFactorScreen, cocFactorSamples (Phase 7; gone in the lens phase)
 │    CGeometryFrame    (Geometry, new)              the frame limits dicing and ray tessellation read
 ├─ contexts: CHider *[numThreads]
 │    CHider (Hiders) : ITracer, IRayContext          (IShadingProducer removed)
 │     ├─ context: CShadingContext (Shading, final)   scratch, varyings, shading state behind IRayContext
 │     CReyes : CHider, IDicer                        (CStochastic, CZbuffer : CReyes)
 │     CRaytracer : CHider                            CPhotonHider : CHider, IIlluminationHook
 └─ context: CRendererContext : CRiInterface, CGeometryHost
                 CRiInterface : IObjectSink           (Scene Interface; also CPreviewContext, CRibOut, ...)
                 CGeometryHost                        (+ delayed expansion, + coordinate-system lookup)

Geometry (src/ri/geometry/) names only:  IRayContext, IDicer, IObjectSink, CGeometryHost, CGeometryFrame,
                                         EShadingDim (Infra), FrameParams / LensModel / motion.h
```

| Entity | Domain | Owner / lifetime | Implemented by | Notes |
|---|---|---|---|---|
| `IRayContext` | Geometry (`rayContext.h`) | interface; one implementer per render thread | `CHider` | thread, scratch, varyings, shade, displace, save/restore, `frame()` |
| `IDicer` | Geometry (`dicer.h`) | interface | `CReyes` | `rayContext()`, draw*, `probe`, `inFrustum`, `frame()` |
| `IObjectSink` | Geometry (`objectSink.h`) | interface, header-only | `CRiInterface` (all its back-ends) | one method, `addObject(CObject *)` |
| `CGeometryHost` | Geometry (`geometryHost.h`, spec 021) | installed by `CRendererContext` at construction | `CRendererContext` | gains `expandDelayedObject`, `expandDelayedInstance`, `findCoordinateSystem` |
| `CGeometryFrame` | Geometry (`geometryFrame.{h,cpp}`) | value installed by the core next to `beginFrameParams` until the end of `endFrame` | — (a value) | stored copies of core statics; default all 0 |
| `EShadingDim` | Infra (`src/common/shadingDim.h`) | enum | — | moved from `shading.h` (same name, values, order) |
| `FrameParams` | Graphics State (spec 028) | installed in `beginFrameParams` | — | gains `cocFactorScreen`, `cocFactorSamples` (copies) for Phase 7; both removed in the lens phase |
| `LensModel` | Graphics State (`camera.h`, spec 028) | value | — | lens phase: `circleOfConfusion(z)`, `fromFrame(fp)`; `cocFactorScreen()` removed |
| `CShadingContext` | Shading | a `CHider` member | — | loses `shade(CSurface *, ...)`, `displace`, `displaceEstimate`, `setProducer`, `producerObject` |
| `IShadingProducer` | Shading | — | — | **deleted** (`shadingProducer.h`) |

## `CGeometryFrame`: fields (stored copies, never recomputed)

| Field | Type | Copied from (core static) | Read by |
|---|---|---|---|
| `numThreads` | `int` | `CRenderer::numThreads` (after the network override) | `CTesselationPatch` (constructor, destructor, `tesselate`); `initTesselations` takes it as a parameter |
| `maxGridSize` | `int` | `CRenderer::maxGridSize` | `CPatch::probe`/`buildLayout`/`layoutNode`/`splitToChildren`, `CCurve::dice`, `CPoints::dice`, `CTesselationPatch` |
| `maxEyeSplits` | `int` | `CRenderer::maxEyeSplits` | `CPatch::probe` |
| `flags` | `unsigned int` | `CRenderer::flags` | motion blur / focal blur tests in `CPatch::probe`, `CCurve::dice` |
| `raytracingFlags` | `unsigned int` | `CRenderer::raytracingFlags` | `CObject::raytraced()` |
| `projection` | `int` | `CRenderer::projection` | `cull` (`surface.cpp:118`), `estimateDicing`, `camera2pixels` |
| `imagePlane`, `dPixeldx`, `dPixeldy`, `pixelLeft`, `pixelTop` | `float` | the statics of the same names | `estimateDicing`, `camera2pixels` |
| `clipMin` | `float` | `CRenderer::clipMin` | `CCurve::dice` |
| `cocFactorPixels`, `invFocaldistance` | `float` | the statics | `minCocPixels` (until the lens phase) |
| `lens` (lens phase) | `openrender::LensModel` | `LensModel::fromFrame(frameParams())` | `minCocPixels` |

Validation: `numThreads >= 1` and `maxGridSize >= 1` whenever a frame is installed (asserted in debug at install);
with nothing installed every field is 0, and only `raytracingFlags` may be read there (by `raytraced()`, which then
answers false, as `CRenderer::raytracingFlags` does outside a frame today).

## State transitions

**A delayed object or instance** (`processed`: `FALSE` -> `TRUE`, once):

```text
dice(IDicer &) or intersect(IRayContext &, CRay *)
  processed == FALSE ? -> geometryHost()->expandDelayedObject(this)
                            lock CRenderer::delayedMutex
                            processed == FALSE ? -> processDelayedObject(this, ...); setChildren(children); processed = TRUE
                            unlock
  dice only: CObject::dice(dicer)   (draw the children)
no host installed: CODE_BUG once, the object stays unexpanded
```

**The frame values** (per frame, core-driven):

```text
beginFrame: ... DoF block, flags, raytracingFlags, beginClipping, numThreads override
            -> CTesselationPatch::initTesselations(geoCacheSize, numThreads)   (thread count now a parameter)
            -> preDisplaySetup, beginDisplays (may still set flags bits)
            -> installGeometryFrame(&value)            (new, next to beginFrameParams)
            -> beginFrameParams(samplingConfig)          (spec 028, unchanged position)
            -> contexts[i] = new <hider>(i, ...)
endFrame:   delete contexts; endFrameParams(); ...; root->destroy(); ...; shutdownTesselations()
            -> installGeometryFrame(nullptr)            (new, last)
```

**A shading call from Geometry** (replaces the forwarding entry):

```text
Geometry: rc.varyings()[VARIABLE_U] ... = ...;  rc.shade(surface, u, v, dim, up)      (or displace / dicer.probe)
CHider:   shadeSurface(surface, u, v, dim, up, displaceOnly)  (FALSE / TRUE / 3, as today)
            -> CSurfacePointSource + ShaderGlobals via classifyShading() -> context.shade(globals, source)
```

## Signature map (every Geometry virtual and entry point that changes)

| Today | After |
|---|---|
| `virtual void CObject::intersect(CShadingContext *, CRay *) = 0` | `virtual void intersect(IRayContext &, CRay *) = 0` |
| `virtual void CObject::dice(CReyes *)` | `virtual void dice(IDicer &)` |
| `virtual void CObject::instantiate(CAttributes *, CXform *, CRiInterface *) const = 0` | `... IObjectSink *) const = 0` |
| `void CObject::cluster(CShadingContext *)` | `void cluster(IRayContext &)` |
| `void CObject::setChildren(CShadingContext *, CObject *, bool = false)` | `void setChildren(CObject *, bool = false)` |
| `virtual void CObject::materialize(CShadingContext *)` | `virtual void materialize(IRayContext &)` |
| `virtual void CObject::regionCoverage(CShadingContext *, ...)` | `virtual void regionCoverage(IRayContext *, ...)` |
| `virtual int CRegionPredicate::accepts(CShadingContext *, ...)`, `classifyBound(...)` | `IRayContext *` (may be null) |
| `virtual void CSurface::shade(CShadingContext *, int, CRay **)` | `virtual void shade(IRayContext &, int, CRay **)` |
| `virtual int CSurface::countCrossings(CShadingContext *, ...) const` | `IRayContext *` (may be null) |
| `bool CSurface::checkRayGuard(CRay *, CShadingContext *)` | `bool checkRayGuard(CRay *, IRayContext &)` |
| `CPatch::dice/buildLayout/splitToChildren/probe/layoutNode(CReyes *, ...)` | `(IDicer &, ...)` |
| `CTesselationPatch::initTesselation/tesselate/splitToChildren/sampleTesselation/purgeTesselations(CShadingContext *, ...)` | `(IRayContext &, ...)` |
| `CCurve::splitToChildren(CReyes *)` and overrides | `(IDicer &)` |
| mesh `create(CShadingContext *)` (`CPolygonMesh`, `CPatchMesh`, `CNURBSPatchMesh`, `CSubdivMesh`, `CLoopSubdivMesh`, `CCurveMesh`) | `create(IRayContext &)` |
| `CCSGSolid::resolve/build(CShadingContext *)` | `(IRayContext &)` |
| `classifyOperand`, `classifyOperandAlong`, `CCSGEvaluator::classify/accepts/classifyBound`, `countFreeFormCrossings` | `IRayContext *` |
| `CRendererContext::processDelayedObject(CShadingContext *, CDelayedObject *, ...)`, `processDelayedInstance(CShadingContext *, ...)` | context parameter removed (core) |
| `curvesCreate(..., CRendererContext *)` (`curves.h:155`, declared, never defined) | deleted |
