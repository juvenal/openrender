# Contract: the boundary version, 3.0.0 (Shading)

**Requirements**: FR-015; gotcha 28's rule. **Research**: R11, Q8.
**File**: `src/libshader/shading/rendererServices.h:70-97` (the constant and its history).

## The constant

`inline constexpr InterfaceVersion kRendererServicesVersion{3, 0, 0};`, introduced in Phase 3 (the first MAJOR
change, `getPhotonMap`), and covering every boundary change of spec 029 with one history entry (Q8 (a)).

## History entry (text to add)

```
//   3.0.0 (spec 029): MAJOR.
//          Changed:  getPhotonMap() returns openrender::shading::IPhotonLookup * (was CPhotonMap *).
//                    tracer() is a fallback: Shading asks the context's own tracer first (set by the hider
//                    that owns the context) and the services' only for a context with none; the renderer's
//                    implementation answers nullptr. [Q2 (b): "Removed: tracer()" instead.]
//                    ShaderGlobals: BatchShape::Span means 2-D ray hits only; the presence rules are stated
//                    per direction (shade()'s input carries the per-batch fields only).
//          Added:    BatchShape::PointSet; IPhotonLookup (photonLookup.h); IIlluminationHook
//                    (illuminationHook.h); IPointSource (pointSource.h); IShadingProducer
//                    (shadingProducer.h).
//          ITracer:  signatures unchanged; implemented by the hiders themselves (CHiderTracer removed).
```

## Every implementer's `static_assert` (MAJOR 2 -> 3)

| File:line | Kind |
|---|---|
| `src/ri/render/rendererServicesImpl.h:35` | source |
| `src/ri/hiders/hiderTracer.h:25` | source: bumped to 3 in Phase 3 (the build breaks otherwise), then deleted with the file in Phase 6 |
| `src/ri/hiders/hider.h` (new) | source, written against 3 |
| `tests/shading_contract/contract_services.h:49` | TEST-APPROVAL |
| `tests/shading_contract/contract_forwarder.h:31` | TEST-APPROVAL |
| `tests/shading_contract/recording_tracer.h:31` | TEST-APPROVAL |
| `tests/shading_contract/case_globals_double.cpp:33` | TEST-APPROVAL |

The services classes in `tests/shading_parity` carry no `static_assert` (they change only their `getPhotonMap`
signature, [photon-lookup.md](photon-lookup.md)).

## What does not change

Every other member of `CRendererServices`, `serviceTypes.h` and `tracer.h`'s `ITracer` signatures. `tracer.h`'s
comment that says the depth rule is reachable "only by calling `context.traceEx(bundle)`" is rewritten to name
`enterRayLevel`/`leaveRayLevel` ([shading-context.md](shading-context.md)), and the one that names
`CHiderTracer` to name the hiders.
