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

### Per-time-stratum motion bounds (implemented, 2026-07)

`motion-3-reyes.rib` (90° camera rotation over the full shutter) rendered in
~236 s wall / 455 s CPU while `motion-3-raytrace.rib` took 1.8 s. The cost was
structural, not tessellation: dicing sizes grids from the t=0 probe positions,
but every moving quad's raster bound must cover its **entire shutter sweep**, so
the stochastic rasterizer tested every sample in a quarter-circle band (10⁴–10⁵
samples per quad) with an expensive unproject→SLERP→re-project of 4 vertices per
test — and ~99.99 % of tests failed because a sample's fixed `jt` places the quad
elsewhere on its arc. The raytracer is O(#samples) regardless of motion
magnitude (each ray carries a stratified time; camera motion is one transform
lerp during intersection).

Fix: sample times are deterministically stratified by sub-pixel index
(`CStochastic::rasterBegin`), so `insertGrid` now samples each moving quad grid's
vertex raster positions at each stratum's endpoints + midpoint (19 times for
3×3 PixelSamples; capped at `RASTER_MAX_TIME_STRATA` = 16 strata) and stores
per-quad and grid-level bounds **per time stratum**. The rasterizer
(`stochasticQuad.h`, XTREME and SLOW_RASTER moving paths) culls each sample
against the bounds of the sample's own stratum (`CPixel::jtStratum`) before any
per-sample vertex deformation. Culling is conservative (±2-sample slack for time
jitter spill and polyline-vs-arc slack) and consumes no RNG.

Results (M-series macOS, 8 threads):

| Scene | Before | After |
|---|---|---|
| motion-3-reyes | 235.6 s wall / 455 s CPU | 18.2 s wall / 29.6 s CPU (13×) |
| motion-3-raytrace | 1.8 s | 1.8 s (unchanged) |

Side effect: the old full-shutter arc bounds were sampled at only
t = 0/0.25/0.5/0.75/1, under-covering the true arc between samples and silently
dropping legitimate hits; the denser stratum sampling recovers them, making
motion-3-reyes measurably closer to the raytrace ground truth (max block diff
131 → 81). The `references/motion-3-reyes.tif` reference was regenerated
accordingly.

### Pure-rotation inverse-sample fast path (implemented, 2026-07)

When the camera's relative shutter motion is a pure rotation
(`CRenderer::cameraRotationOnly`, i.e. `relTrans ≈ 0`), the raster-space motion
is a z-independent homography. Instead of forward-transforming 4 quad vertices
per (sample × quad), `rasterBegin` now inverse-rotates each sample's ray once
(`CPixel::xcentRot/ycentRot`) and the rasterizer tests the **static t=0 quads**
against the pre-rotated sample; the interpolated static depth is mapped to the
sample's time by `CPixel::zScale` (exact — for a rotation `z_jt = z0 / (R⁻¹·dir)_z`).
Applies to the MOVING (non-DOF) quad variants only; DOF+motion keeps the
stratum path because the aperture offset lives in the time-jt raster frame.

| Scene | Tiers 1–2 | + fast path |
|---|---|---|
| motion-3-reyes | 18.2 s wall / 29.6 s CPU | 8.1 s wall / 4.5 s CPU |

Total vs. the original 235.6 s: **29× wall, 100× CPU**. The residual wall time
is dominated by serial phases (parse, dice/insertGrid, bucket sync), not the
raster inner loop.

### Visual-test determinism caveat (measured, 2026-07)

The stochastic hider is **not run-to-run deterministic**: bucket→thread
assignment shifts the per-thread jitter streams, so extreme-blur speckle is a
different (equally valid) noise realization each run. Measured same-binary
run-to-run max 8×8-block diffs: motion-3-reyes ~79/255, camera-motion-huge-reyes
~70/255, camera-motion-small+dof-reyes ~64/255, camera-motion-small+dof-raytrace
~97/255. The visual-test thresholds for those four scenes were raised above
their measured variance (100/100/90/110) — at the default 20 they were
coin-flips regardless of code changes.
