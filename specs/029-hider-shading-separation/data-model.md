# Data Model: Hider / Shading Separation (spec 029)

Plan-stage proposal; the shapes assume the recommended answers in [research.md](research.md). The full
declarations are in [contracts/](contracts/).

## Entities and ownership

```text
CRenderer (core, composition root)
 └─ contexts: CHider *[numThreads]                      (one per render thread, per frame; Q6)
     CHider (Hiders)  : ITracer, IShadingProducer       (owns, traces, produces)
      ├─ context: CShadingContext (Shading, final)      (by value; built first, destroyed last)
      │    ├─ tracer   -> the owning CHider             (setTracer; Q2)
      │    ├─ producer -> the owning CHider             (setProducer; Q1)
      │    ├─ hook     -> the owning CPhotonHider, or null (setIlluminationHook)
      │    ├─ CShadingState* (current, free list)       (surface, object handle, attributes; Q3)
      │    └─ traceObjectHash                           (stays; reached by the hider via objectHash(); Q16)
     CReyes : CHider  (CStochastic, CZbuffer : CReyes)
     CRaytracer : CHider
     CPhotonHider : CHider, IIlluminationHook
```

| Entity | Domain | Owner / lifetime | Notes |
|---|---|---|---|
| `CShadingContext` | Shading | a `CHider` member, or a test's local | no virtuals, no foreign friends, `final`; constructible alone |
| `CHider` | Hiders | `CRenderer::contexts[i]`, from `beginFrame()` to `endFrame()` | `renderingLoop()`, `shadingContext()`, the `ITracer` and `IShadingProducer` overrides |
| `ITracer` | Shading (spec 026, unchanged signatures) | implemented by `CHider` | the context's slot first, the services' `tracer()` as the fallback for a context with no owner (Q2 (a)) |
| `IIlluminationHook` | Shading | implemented by `CPhotonHider` | null on every other context |
| `IPhotonLookup` | Shading | implemented by `CPhotonMap` | returned by `getPhotonMap()` |
| `IPointSource` | Shading | `CSurfacePointSource` (Hiders), on the stack for one shading call | wraps one `CSurface *` and the requested parameters |
| `IShadingProducer` | Shading | implemented by `CHider` | the forwarding entry's target |
| `ShaderGlobals` | Shading (spec 026; changed in 2.1.0) | built by the producer on the stack | per-batch fields on input to `shade()` |
| `CRayLevel` | Shading | a hider's local across one `traceBundle` | saved state and label |
| bundles (`CTraceBundle`, `CTransmissionBundle`, `CGatherBundle` and their rays) | Shading (moved) | Shading's stack or heap, as today | relocation, no change |

## `CShadingState`: the shaded object (Q3 (a))

| Field | Type | Written by | Read by |
|---|---|---|---|
| `currentSurface` | `const CSurface *` (opaque) | `shade()` (from the source), `setShadedObject()` | the observers' reports |
| `currentObject` | `ObjectHandle` | the same | `objectHandle()` -> every service request |
| `currentAttributes` | `const CAttributes *` | the same | `objectAttributes()`; the attribute reads in `shading.cpp`; `hcshader.cpp` |

Invariant: the three are set and restored together (`shade()` saves and restores them where it saves
`currentObject` today, `shading.cpp:1022-1023, 1389`). The handle is `ObjectHandle{static_cast<const CObject *>(surface)}`,
the value `CRendererServicesImpl` casts back. `nullptr`/empty everywhere for the imager and a bare execution.

## The ray level (FR-017)

| State | Owner | Rule |
|---|---|---|
| `currentRayDepth`, `currentRayLabel`, `freeStates` | the context (private) | changed only by `enterRayLevel`/`leaveRayLevel` |
| `maxRayDepth` | the frame (`CRenderer::maxRayDepth`, read by the hider in `trace.cpp`) | passed to `enterRayLevel`; compared there once |
| `inShadow` | the context (private) | set by `traceTransmission`; read through `inShadowRay()` |
| `traceObjectHash` | the context (private; `objectHash()` for the owner, Q16 (a)) | reset to the context's sentinel after each pass |

State transitions of one `traceBundle` (unchanged from `traceEx`): `enterRayLevel` -> (`false`: one
`postShade(numRays, rays)`, stop) / (`true`: `traceGroups` loop: trace, `postTraceAction`, group, shade, `postShade`,
`post`, continue while `numRays > 0`) -> `leaveRayLevel`.

## `ShaderGlobals` as `shade()`'s input (Q4 (a))

| Field | Filled on input | Source |
|---|---|---|
| `numPoints`, `shape` (`Grid`/`Span`/`PointSet`), `gridU`, `gridV` | yes | the call's dimension and vertex counts |
| `object` | yes | the surface's `CObject *` upcast |
| `rayType`, `hitMode`, `transport` | yes | `classifyShading()` (corrected, below) of the call's situation |
| `pass` | yes | `displaceOnly` |
| `pathDepth` | yes | `rayDepth()` |
| `shadingCellFootprint` | yes | `attributes->shadingRate` |
| `shutterOpen`, `dtime` | yes | `FrameParams` |
| `shader`, `activeTags`, `numDerivativeTail` | no (default) | not known before the shaders are chosen; `shade()` computes the tail itself |
| every per-point `Strided` field | no (absent) | the context's varyings, written before the call or sampled inside it |

## The situation table, corrected to the code (Q5 (a))

Changes against spec 026's table (`specs/026-shading-contract-v2/contracts/shader-globals.md`, kept as the
record of 026) are in **bold**.

| # | Situation | Ray type | Hit mode read | Transport | Pass |
|---|---|---|---|---|---|
| 1 | REYES/zbuffer grid (camera) | `None` | `camerahitmode`, after the shaders (`shading.cpp:1372-1380`) | Radiance | Full |
| 2 | REYES points/curves (0-D), **depth 0** | `None` | none | Radiance | Full |
| 3 | raytrace camera hit (2-D, depth 0) | `Camera` | `specularhitmode` (`:912`) | Radiance | Full |
| 4 | secondary hit (2-D, depth >= 1, not in a shadow), **in any frame, raster frames included** | `Secondary` | `specularhitmode` | Radiance (**Importance under the photon hook**) | Full |
| 5 | shaded shadow/transmission hit (**2-D or 0-D**) | `Shadow` | `transmissionhitmode` (`:883-898`), **at every dimension** | Radiance (**Importance under the photon hook**) | Full |
| 6 | implicit-surface hit (0-D) **not in a shadow** | `Camera` (depth 0) / `Secondary` | none | Radiance | Full |
| 7 | DSO-object hit | as rows 3-5 | as rows 3-5 | Radiance | Full |
| 8 | displacement-only pass | `None` | none | Radiance | DisplaceOnly |
| 9 | displacement bound / grid-size estimate | `None` | none | Radiance | DisplaceEstimate |
| 10 | photon emission (light shaders; **0-D, depth 0, not in a shadow**) | `Photon` | none | Importance | Full |
| 11-14 | as spec 026 (no `shade()` call) | | | | |

Rule order in `classifyShading()`: rows 9, 8; row 10; row 5; row 1; row 2; rows 3/4/6/7.

## Validation rules

- FR-019: for every batch a rendered scene shades, the producer's `rayType`/`hitMode`/`transport`/`pass` equal
  `classifyShading()` of the same situation (by construction, the producer calls it) and the observed branch of
  `shade()` matches the row (`ShadingContract_ProducerSituations` and `ShadingContract_Situations`).
- FR-020: the derivative-offset points and the grid du/dv are computed by the unchanged code in `shading.cpp`.
- FR-023: images, the `MaxBlockAvgDiff` list, the sweep and the replay equal the Phase 1 records.
