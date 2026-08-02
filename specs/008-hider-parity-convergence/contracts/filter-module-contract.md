# Contract: Shared pixel-filter module (R4)

## Responsibilities

Single owner of the splat/gather accumulate-and-normalize step around the
already-shared `CRenderer::pixelFilterKernel` (kernel evaluation itself is
not duplicated today — only the accumulation loop around it is). Consumed
by `CStochastic`, `CRaytracer`, and `CZbuffer`.

## Interface

```cpp
// src/ri/pixelFilter.h  (exact filename TBD at implementation time —
// research.md flags this as the one file name not yet fixed)
class CPixelFilterAccumulator {
public:
    explicit CPixelFilterAccumulator(const CRenderer::PixelFilterKernel &kernel);

    // Splat: contribute one sub-pixel sample's weighted color to every
    // output pixel the kernel's support radius touches.
    void splat(int sampleX, int sampleY, const CColor &color, float weight);

    // Gather: read back one output pixel's normalized (weight-divided)
    // final color once all contributing samples have been splatted.
    CColor gather(int pixelX, int pixelY) const;
};
```

## Preconditions

- `kernel` is the existing, already-shared `CRenderer::pixelFilterKernel`
  instance computed once in `beginFrame` — this contract does not construct
  a new kernel type.
- Caller has already produced per-sub-pixel-sample `(color, weight)` pairs
  via its own shading/compositing path (this module is downstream of R3's
  compositor output, not a replacement for it).

## Postconditions

- FR-021: introducing this module produces byte-identical rendered output
  for every scene in the existing 33+-scene visual-regression suite that
  currently passes — this is a pure code-motion refactor, not a behavior
  change.
- FR-020: exactly one shared implementation of the accumulate/normalize
  step, used by all three hiders (`CStochastic`, `CRaytracer`, `CZbuffer`),
  independently verifiable by inspecting source (SC-004 applies to R1-R4
  collectively per spec.md Story 7/8 framing).

## Consumers

- `CStochastic::rasterEnd` (reyes bucket accumulation)
- `CRaytracer`'s per-ray-hit pixel accumulation
- `CZbuffer::rasterEnd` (currently its own simple opaque filter/gather,
  zbuffer.cpp:160-198 — folded into this shared module)

## Non-goals

- Does not change the filter kernel's mathematical definition — only where
  the accumulate/normalize loop around it lives.
- Does not attempt to unify `CZbuffer`'s simpler (non-stochastic) filtering
  needs with reyes/raytrace's supersampled case beyond what the shared
  accumulator already supports generically (a z-buffer pixel typically has
  one contributing sample; the module must handle that degenerate case
  correctly, not specially).
