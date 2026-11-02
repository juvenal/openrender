# Contract: the boundary version, 2.0.0 -> 2.1.0 (Shading)

**Requirements**: FR-015. **Decision**: the maintainer's versioning policy (spec.md, Clarifications, Session
2026-11-02), which replaces the plan stage's P-Q8. **Research**: R11.
**File**: `src/libshader/shading/rendererServices.h:73-97` (the rule comment, the history and the constant).

## The policy

MAJOR is a breaking change, MINOR a non-breaking addition, PATCH a bug fix only. **MAJOR stays at 2 until spec
055**: nothing versioned has been distributed, and the milestone ends at MAJOR 2 (MAJOR 1 is the pre-versioning
interface of specs 024, 025 and 027). Until the milestone closes, 2.x is unstable: a MINOR step may break.

## The constant

`inline constexpr InterfaceVersion kRendererServicesVersion{2, 1, 0};`, set in Phase 3 (the first boundary change,
`getPhotonMap`), with one history entry that covers every boundary change of spec 029. Later phases of 029 add their
lines to the same entry (nothing ships between phases).

## Rule comment (text to add after the PATCH line, `rendererServices.h:79`)

```
//   Until the milestone closes (spec 055) MAJOR stays at 2 and 2.x is unstable: nothing versioned has been
//   distributed, so a MINOR step may also make a change listed under MAJOR above. Such an entry says so
//   ("Breaking within 2.x"). The rules above apply as written from the release that closes the milestone.
```

## History entry (text to add above the 2.0.0 entry)

```
//   2.1.0  spec 029. Breaking within 2.x (allowed before spec 055, see the rule above):
//          Changed:  getPhotonMap() returns openrender::shading::IPhotonLookup * (was CPhotonMap *).
//                    tracer() is a fallback: Shading asks the context's own tracer first (set by the hider
//                    that owns the context) and the services' only for a context with none; the renderer's
//                    implementation answers nullptr.
//                    ShaderGlobals: BatchShape::Span means 2-D ray hits only; the presence rules are stated
//                    per direction (shade()'s input carries the per-batch fields only).
//          Added:    BatchShape::PointSet (appended); IPhotonLookup (photonLookup.h); IIlluminationHook
//                    (illuminationHook.h); IPointSource (pointSource.h); IShadingProducer
//                    (shadingProducer.h).
//          ITracer:  signatures unchanged; implemented by the hiders themselves (CHiderTracer removed).
```

The new headers say "Introduced in 2.1.0".

## Implementers' `static_assert`s: unchanged

Every implementer keeps asserting MAJOR 2, so none of these lines is edited:

| File:line | Kind |
|---|---|
| `src/ri/render/rendererServicesImpl.h:35` | source |
| `src/ri/hiders/hiderTracer.h:25` | source, deleted with the file in Phase 6 |
| `tests/shading_contract/contract_services.h:49` | test |
| `tests/shading_contract/contract_forwarder.h:31` | test |
| `tests/shading_contract/recording_tracer.h:31` | test |
| `tests/shading_contract/case_globals_double.cpp:33` | test |

The new `src/ri/hiders/hider.h` asserts MAJOR 2 as well. The test doubles change only their `getPhotonMap`
signature ([photon-lookup.md](photon-lookup.md)), which is a TEST-APPROVAL edit of its own.

## What does not change

Every other member of `CRendererServices`, `serviceTypes.h`, and the signatures of `tracer.h`'s `ITracer`.
`tracer.h`'s comment that says the depth rule is reachable "only by calling `context.traceEx(bundle)`" is rewritten to
name `enterRayLevel`/`leaveRayLevel` ([shading-context.md](shading-context.md)), and the one that names
`CHiderTracer` to name the hiders.
