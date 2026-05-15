# Hider Parity: Stochastic vs. Raytrace

The goal is for the `stochastic` and `raytrace` hiders to produce pixel-identical images for the same scene. The two hiders operate very differently internally (micropolygon rasterization vs. ray intersection), so achieving parity requires aligning each stage of the shading pipeline.

## Alignment Status

- [x] **Unified Pixel Filtering:** Both hiders utilize the global `CRenderer::pixelFilterKernel` precomputed in `beginFrame`, ensuring consistent anti-aliasing.
- [x] **Sampling Distribution:** Both hiders respect `Option "hider" "float jitter"` for sample positions.
- [ ] **Motion Blur Implementation:** `CRaytracer` needs to implement support for moving surfaces (interpolation of vertex positions over time) to match the stochastic hider's temporal sampling.
- [ ] **Shading Interpolation & Derivatives:** Ensure that shading derivatives (Du, Dv) and variables like `s`, `t`, `u`, `v` are computed consistently. Stochastic hider shades at micro-polygon vertices, while Raytrace shades at intersection points.
- [ ] **Displacement Parity:** Both hiders should use the same dicing/tessellation levels for displaced surfaces. Raytracer currently stubs some advanced displacement cases.
- [ ] **Transparency Handling:** Align the `opacityThreshold` and `transmission` logic in `CRaytracer` with the fragment-based blending used in `CStochastic`.

## Possible Optimization

(To be documented.)
