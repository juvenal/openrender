# Contract: Shading's Geometry-facing entries retired, and the boundary version

**Status**: approved 2026-11-07 (P-Q7: 2.2.0; the next MINOR if spec 032 steps the version first). **Requirements**: FR-005, FR-006; gotchas 28, 35.
**Files**: `src/libshader/shading/shading.{h,cpp}`, `shadingProducer.h` (deleted), `rendererServices.h`,
`src/ri/hiders/hider.{h,cpp}`, `shadingProducer.cpp`, `reyes.cpp`.

## Removed from Shading

| Item | Where | Replaced by |
|---|---|---|
| `void CShadingContext::shade(CSurface *, int, int, EShadingDim, unsigned int, int displaceOnly)` | `shading.h:428`, `shading.cpp:883-900` | `IRayContext::shade`/`displace`, `IDicer::probe`, answered by `CHider::shadeSurface` |
| `inline displace(...)`, `inline displaceEstimate(...)` | `shading.h:443-444` | same |
| `setProducer()`, `producerObject` | `shading.h` | — (the hider calls its own `shadeSurface`) |
| `openrender::shading::IShadingProducer` | `shadingProducer.h` (file deleted) | `CHider::shadeSurface` (protected, no longer an override) |
| `EShadingDim` definition | `shading.h:84-89` | `src/common/shadingDim.h`, included by `shading.h` (same name and values) |

What stays: `shade(const ShaderGlobals &, IPointSource &)`, `setShadedObject`, `fillPointKeys`, every observer;
`CSurfacePointSource` and `classifyShading()` keep their rules (spec 029's producer rules apply unchanged to
`shadeSurface`). The `no shading producer is installed` warning disappears with the entry (no test matches it).

## `kRendererServicesVersion` 2.1.0 -> 2.2.0 (renumbered 2.3.0 at the merge)

> **Renumbered at merge preparation (2026-11-12).** Spec 032 merged first and stepped the version to 2.2.0, so spec 031's step is the next MINOR, **2.3.0**; the history entry below is in `rendererServices.h` as "2.3.0  spec 031", above spec 032's "2.2.0" entry, and its text is unchanged.

History entry (`rendererServices.h`), one for the spec:

```text
2.3.0 (spec 031; written as 2.2.0 before the renumbering): Removed the Geometry-facing CShadingContext::shade(CSurface *, ...), displace(),
       displaceEstimate(), setProducer() and IShadingProducer (shadingProducer.h): Geometry reaches the hider that
       owns the context through its own IRayContext/IDicer, and the hider shades through
       shade(ShaderGlobals, IPointSource). EShadingDim is defined in common/shadingDim.h (same name and values).
       Breaking; allowed under the 2.x policy (MAJOR stays 2 until spec 055).
```

Every implementer's `static_assert(kRendererServicesVersion.major == 2)` is unchanged. If spec 032 steps the
version first, this entry takes the next MINOR (R14).
