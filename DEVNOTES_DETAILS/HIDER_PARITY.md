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

Remaining gap (18 s vs 1.8 s): the XTREME outer loop still visits every sample
in the grid's full swept bound to run the (cheap) grid-level stratum check, per
overlapping grid. If that ever matters, the gated Tier-3 idea is a fast path for
pure optical-axis camera rotations (`relTrans ≈ 0`): inverse-rotate the *sample*
once per pixel and test against the static t=0 quads with tight static bounds
(near-static cost).

Pre-existing, unrelated: `Visual_camera-motion-huge-reyes` fails because its
CMake entry points at `references/camera-motion-huge.tif`, which was never
checked in.
