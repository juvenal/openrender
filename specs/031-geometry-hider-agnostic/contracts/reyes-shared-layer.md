# Contract: Reyes reads its camera motion and lens values from the shared layer

**Status**: approved 2026-11-07. Byte-identical by construction; finished and proven before the lens phase.
**Requirements**: FR-012, FR-013, FR-014; US4; SC-002, SC-006. **Files**: `src/ri/state/frameParams.{h,cpp}`,
`src/ri/render/rendererFrame.cpp`, `src/ri/hiders/reyes.{h,cpp}`, `stochastic.cpp`, `stochasticQuad.h`,
`tests/unit/motion/test_frame_params_equal_core.cpp` (approved-test edit).

## `FrameParams` gains two stored copies

```cpp
struct FrameParams {
    ...
    float aperture, focaldistance, invFocaldistance, focallength, fstop;
    float cocFactorScreen, cocFactorSamples; // spec 031: copies of the core's values (renderer.cpp:455-456); removed by the lens phase
    FrameCameraMotion camera;
};
```

`beginFrameParams` copies them from `CRenderer::cocFactorScreen`/`cocFactorSamples`; the default value sets 0.
`Motion_FrameParamsEqualCore` gains the two `bits(...)` checks (an approved-test edit).

## The reads, site by site (expressions unchanged; inputs relocated)

| Site | Today | After |
|---|---|---|
| `reyes.cpp:172`, `:616`, `:1153`, `:1466`, `:1553`, `:1738`, `:1886` | `CRenderer::aperture != 0` | `fp.aperture != 0` |
| `reyes.cpp:617-618` | `cocScreen(zmin)` (macro over `CRenderer::invFocaldistance`, `cocFactorScreen`) | `absf((1 / zmin) - fp.invFocaldistance) * fp.cocFactorScreen` (the macro's text) |
| `reyes.cpp:1164`, `:1468-1469`, `:1887-1888` | `CSampler::circleOfConfusion(z, CRenderer::invFocaldistance, CRenderer::cocFactorSamples)` | `CSampler::circleOfConfusion(z, fp.invFocaldistance, fp.cocFactorSamples)` |
| `reyes.cpp:1426-1437`, `:1599-1608`, `:1636-1646` | `CRenderer::cameraHasRotation`; `slerpq(Rjt, identQ, CRenderer::relRotQ, t); qtoR(M, Rjt);` `t * CRenderer::relTrans[k]` | `fp.camera.hasRotation`; `motion::cameraRotationAt(M, fp.camera.relRotQ, t)`; `t * fp.camera.relTrans[k]` |
| `stochastic.cpp:165-170`, `:213-219` | `CRenderer::cameraRotationOnly`, the conjugate of `CRenderer::relRotQ`, `slerpq`+`qtoR` | `fp.camera.rotationOnly`, the conjugate of `fp.camera.relRotQ`, `motion::cameraRotationAt(M, invRotQ, pixel->jt)` |
| `stochasticQuad.h:381`, `:497-510`, `:575-578`, `:856-875` | `CRenderer::cameraRotationOnly`, `cameraHasRotation`, `relRotQ`, `relTrans` | `fp.camera.*`, `motion::cameraRotationAt` |

`fp` is a `const openrender::FrameParams &` taken once per bucket (`rasterBegin`, and once in each `CReyes`
function that reads it), never a per-sample `frameParams()` call. The `identQ` constants disappear with the
inlined SLERPs.

## Rules

- `Motion_CameraSlerpEquivalence` already pins `motion::cameraRotationAt` to Reyes's `slerpq`+`qtoR` bit for bit;
  it must stay green. `Motion_Replay_*` is never re-recorded (FR-014, FR-020).
- `rasterCameraMotionTransform` (`reyes.h:66-84`) is not touched: its reads (`dSampledx`, `pixelLeft`,
  `invImagePlane`, `imagePlane`) are other DB-011 lines, not this spec's.
- Deleted allowlist lines (11): `reyes.cpp -> CRenderer::aperture, cameraHasRotation, cocFactorSamples,
  cocFactorScreen, invFocaldistance, relRotQ, relTrans`; `stochastic.cpp -> CRenderer::cameraHasRotation,
  cameraRotationOnly, relRotQ, relTrans`. The Hiders `CRenderer::` metric is lowered to the measured value.
- The comments that name the statics (`reyes.h:61`, `sampler.h:90-94`) are corrected; `sampler.h`'s D9 text is
  rewritten in the lens phase.
