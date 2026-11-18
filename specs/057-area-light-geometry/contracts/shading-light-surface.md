# Contract: light-surface execution in Shading, the sampler and occlusion interfaces, photon emission

**Requirements**: FR-011-FR-017, FR-023, FR-025, FR-028; SC-002, SC-004, SC-008; US3, US4, US8.
**Domains**: Shading (owns the interfaces and the execution), Hiders (implement them), Geometry (gate, contracts/area-sampling.md).
**Files**: new `src/libshader/shading/areaLightSampler.h`; `tracer.h` (`occluded`), `rendererServices.h` (version 2.4.0; 2.5.0 after the 033 sync, plan.md D-10),
`shading.{h,cpp}` (`iterateLights`, `setAreaLightSampler`), `src/ri/hiders/hider.{h,cpp}`, new `src/ri/hiders/areaLights.cpp`,
`src/ri/hiders/trace.cpp` (`occluded`), `src/ri/hiders/photon.cpp`, `src/common/sampling.h` (call sites).

## `IAreaLightSampler` (Shading-owned, implemented by `CHider`)

```cpp
// libshader/shading/areaLightSampler.h. Version: kRendererServicesVersion (2.4.0 on this branch; 2.5.0 after the 033 sync).
namespace openrender::shading {
    enum class AreaLightMode : uint8_t { Unset, Full, Fast, Off };   // same values as the state vocabulary's

    struct AreaLightInfo {
        bool bound = false;          // the light was created by AreaLightSource
        int lightSamples = 0;        // light level (0: unset)
        AreaLightMode lightMode = AreaLightMode::Unset;
        int sceneSamples = 0;        // Option level; 0 = unset in the header, the hider always fills it from the registry
        AreaLightMode sceneMode = AreaLightMode::Full;
        double totalArea = 0;        // 0: no emitting geometry (the light contributes nothing, A7)
    };

    class IAreaLightSampler {
        public:
            virtual ~IAreaLightSampler() = default;
            // Is `light` bound, and its light and scene controls. Resolves the light's emitter set on first use.
            virtual AreaLightInfo areaLight(CShadingContext &context, const CShaderGroupRefTarget *light) = 0;
            // For m points: draw a point on the light's emitters from (u1[i], u2[i]), evaluate the light-surface
            // globals at time[i] into out[VARIABLE_P|N|NG|DPDU|DPDV|U|V|S|T] (camera space, the emitter's own
            // orientation, normals and st) and write pdfA[i] (0: no point; the caller then contributes nothing).
            // Re-enters Shading: never. Thread-safety: per render thread, its own context.
            virtual void sampleEmitters(CShadingContext &context, const CShaderGroupRefTarget *light, int m, const float *u1, const float *u2,
                                        const float *time, float **out, float *pdfA) = 0;
    };
}
```

`CShadingContext::setAreaLightSampler(IAreaLightSampler *)` (with `areaLightSampler()` and a private pointer member beside
`setTracer()`'s, in `shading.h`; approved with the interface, services 2.4.0) is called by `CHider` at the start of its rendering loop,
**only when the frame has a bound light** (`emitterRegistry().numLights() > 0`; the registry is complete by then, the
world block being over); otherwise the pointer stays null and `iterateLights` pays one null test per light, so classic-only
frames keep their cost (the performance goal of plan.md).
A context with no sampler (one that no hider owns: the imager's, the contract doubles') runs every light as a classic
light; the contract double implements both members so the bound path is testable without a hider.

`CHider` answers `areaLight()` from `emitterRegistry().find(light)` (Geometry) and `sampleEmitters()` by
`set->sample(u1, u2)` per point, then grouping the points by leaf and calling the leaf's `sample()` and `interpolate()` on
a scratch varying array with no shader locals bound (the ray-hit evaluation of `CSurface::shade`, without shading).

## `ITracer::occluded` (2.4.0)

```cpp
// tracer.h, added to ITracer
// Trace one shadow probe and report whether something blocks it. In: the CRay inputs of traceRay (the caller sets
// flags = ATTRIBUTES_FLAGS_TRANSMISSION_VISIBLE, t = distance to the target - bias, tmin = bias); excludeLight: the
// identity of an area light whose scope's primitives must not block (nullptr: none). Out: true if ray->object != nullptr
// after the trace. Never tests ray->t against infinity (the `visibility()` defect, GitHub #169, research R1).
// Default: traceRay(context, ray) and the test, ignoring excludeLight. CHider overrides it to apply the exclusion.
// Declared in tracer.h; the default body is DEFINED OUT OF LINE in traceCalls.cpp, because tracer.h only forward-declares CRay
// (the body needs ray->object) and CShaderGroupRefTarget (Graphics State, forward-declared too).
virtual bool occluded(CShadingContext &context, CRay *ray, const CShaderGroupRefTarget *excludeLight);
// traceCalls.cpp:  bool ITracer::occluded(CShadingContext &context, CRay *ray, const CShaderGroupRefTarget *) {
//                      traceRay(context, ray); return ray->object != nullptr; }
```

`CHider::occluded` sets `emitterExclusion = excludeLight`, calls `traceRay`, resets it to `nullptr`. No volume step
(spec 056 probe semantics), no shading. Every camera hider (reyes, zbuffer, raytrace) answers it through `CHider`, so
occlusion is one code path (Principle VIII rule 3).

## `iterateLights` (Shading): the bound branch

Inside the existing loop over `currentAttributes->lightSources`, after the category test and before `prepare()`, with the
`lightSideN` snapshot already taken (unchanged position):

```text
info = sampler ? sampler->areaLight(*this, light) : {}
if !info.bound: classic path, byte for byte as today
else:
    if ss->currentAttributes->areaLight == light: skip                      # FR-011/FR-017 receiver skip
    (n, mode) = resolve(receiver attributes, info)                           # FR-018, data-model 6
    if mode == Off or info.totalArea == 0 or n == 0: skip                    # A7 warns once per light
    if mode == Fast: n = max(1, ceil(n / 4))
    saved = (ss->varying, ss->shadingDim)
    for k in 0..n-1:
        for each real vertex i (tail vertices copy their real vertex):
            sub = substream(pointKeys[i], lightShaderKey(light), kAreaLightSampleSite, 0, currentRayDepth, n)
            (u1[i], u2[i]) = get2D(backend, sub, 0, k)
        sampler->sampleEmitters(*this, light, numVertices, u1, u2, varying[VARIABLE_TIME], lightBuf, pdfA)
        composite = copy of saved.varying with P, N, NG, DPDU, DPDV, U, V, S, T -> lightBuf
        ss->varying = composite; ss->shadingDim = SHADING_0D
        head = ss->lights
        ss->locals[ACCESSOR_LIGHTSOURCE] = light->prepare(shaderStateMemory, composite, numVertices)
        light->illuminate(this, ss->locals[ACCESSOR_LIGHTSOURCE])
        (ss->varying, ss->shadingDim) = saved
        for each CShadedLight e pushed since head, each vertex i active in e with Cl != 0 and pdfA[i] > 0:
            V = occluded(ray from Ps[i] to lightBuf.P[i], key childKey(pointKeys[i], depth+1, kAreaLightShadowSite, lightKey32, k),
                         excludeLight = light) ? 0 : 1
            e.Cl[i] *= V / (n * pdfA[i])
        vertices with pdfA[i] == 0: e.Cl[i] = 0
```

Rules:
- **Factors** (FR-012, Decision A): the renderer applies only `1/(n p_A)` and the binary visibility; no emitter cosine, no
  `1/r^2`. The light shader keeps its own (`arealight.sl`: `N.normalize(L) / (L.L)`).
- **Both backends** (FR-013): the interpreter and the JIT read `currentShadingState->varying` when the light runs, so the
  swap serves both; no emitter, `.slo` or `.rslo` change.
- **Receiver untouched** (FR-011): the light writes into the light-surface buffers; the receiver's `P`, `N` and varyings are
  never written. `Ps` is the receiver's point.
- **Derivatives**: the run is 0-D, so `Du`/`Dv`/`area()` of light-surface globals are 0 on every hider (documented).
- **Same points on a rerun**: `iterateLights` runs again on a cache miss (diffuse, specular, illuminance); the draws are a
  fixed function of `(point, light, k)`, so every rerun at a point sees the same emitter points and no other draw moves.
- **Categories, `__nonspecular`, `__nondiffuse`, `Illuminate`, RIB order** (FR-015): unchanged; they act on the entries the
  bound light pushes like on a classic light's.
- **Illumination hook on** (photon pass): `iterateLights` is not on the path (below).
- **Camera view of an emitter** (FR-025): nothing here; the emitter's pixels are its own `Surface` shader's.

## Photon emission (US8)

In `CPhotonHider`'s light loop, for a light whose `areaLight().bound` is true: draw emitter points with
`kAreaLightPhotonSite` and the photons' cell keys, fill the composite table through `sampleEmitters()` as above, run
`prepare()`/`illuminate()`. The hook operands carry one element (`hookOperand()`, #68), so while a bound light's chunk runs
`illuminateBegin()`/`illuminateEnd()` read each photon's origin and normal from the emitter buffers the photon hider
filled itself (a member set for the chunk), not from the hook operands; classic lights keep the hook path untouched and
emit exactly as before. The bound light's emitted power is scaled by `1/pdfA`. `CRenderer::allLights` and its order are unchanged (spec 040's photon key includes the light index).

## Version history entry (`rendererServices.h`)

`2.4.0 (spec 057)` (renumbered `2.5.0` at the 033 sync, after 033's `2.4.0` entry): `ITracer::occluded` (with a default) and `IAreaLightSampler`, installed on the context by the hider that
owns it. Non-breaking for implementers and doubles.
