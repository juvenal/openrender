# Hider Parity: Stochastic vs. Raytrace

The goal is for the `stochastic` and `raytrace` hiders to produce pixel-identical images for the same scene. The two hiders operate very differently internally (micropolygon rasterization vs. ray intersection), so achieving parity requires aligning each stage of the shading pipeline.

## Alignment Status

- [x] **Unified Pixel Filtering (closed, 2026-08, R4):** All three rasterizing hiders read the same global `CRenderer::pixelFilterKernel` precomputed in `beginFrame`, and now also share the accumulate/normalize arithmetic through `CPixelFilterAccumulator` (`src/ri/pixelFilter.h`) — `CStochastic::rasterEnd`, `CRaytracer`'s per-ray-hit splat, and `CZbuffer::rasterEnd` each call the same `splat()`/`normalizeByWeight()` static helpers instead of hand-rolling the weighted multiply-add loop. Channel-order marshalling and any derived/computed channel (e.g. zbuffer's coverage test) stays at each call site; each hider's existing normalize policy (zbuffer never, stochastic only in continuous mode, raytrace always) is preserved exactly as a caller-side choice. Pure code-motion refactor — FR-021 verified byte-identical (MD5) across precomputed, continuous, and motion+DOF scenes for all three hiders (spec 008 Phase 9/US7, T063-T069); full 44-scene visual suite passes 100%.
- [x] **Sampling Distribution:** Both hiders respect `Option "hider" "float jitter"` for sample positions.
- [x] **Depth-of-Field Lens Sampling:** Both hiders sample the lens aperture disk through the single shared `sampleDisk()` in `src/ri/random.h` (square-to-disk rejection sampling), templated on the caller's RNG source — `CStochastic` via its `CSobol<2> apertureGenerator` (`stochastic.cpp`), `CRaytracer` via its per-instance `urand()` (`raytracer.cpp`). Fixes a prior raytracer defect where `r = urand() * aperture` biased samples toward the aperture center instead of sampling its area uniformly.
- [x] **Object/Surface Motion Blur (verified, 2026-08, closes D10):** `CRaytracer`'s tessellation-path intersection kernels (`patches.cpp`, `polygons.cpp`, `quadrics.cpp`) already interpolate geometry on the ray's shutter time (`cRay->time` in patches/polygons, `rv->time` in quadrics); this item tracks verification against the stochastic hider, not a missing feature. Confirmed via 7 new cross-hider parity scenes (spec 008 Phase 8/US6, `tests/visual/CMakeLists.txt`): `Parity_motion-{patches,polygons,quadrics}-{translate,deform}` (transform-motion via `MotionBegin`-wrapped `Translate`, and deform-motion via `MotionBegin`-wrapped geometry-snapshot/parameter interpolation, e.g. quadrics' radius) plus `Parity_dof-motion` (combined DOF+motion). All 7 pass; measured divergence 3.6-17.1/255 (standard threshold 20, one scene at 25 for margin) — residual is ordinary silhouette antialiasing on a curved/interpolated edge, not a correctness bug. No code changes were required in the intersection kernels.
  - Scope note: this covers **object/surface** motion (geometry moving within camera space over the shutter). It does **not** cover **camera** motion blur (interpolating the camera-to-world transform itself over the shutter) — that is a separate mechanism, not assessed by this work. (Note: camera motion blur scenes such as `Visual_camera-motion-small-dof-raytrace` already pass in the existing visual suite and HIDER_PARITY.md's own optimization notes below describe a working camera-motion lerp path in the raytracer — this scope note is not a claim that camera motion blur is missing or broken, only that it was not part of this verification pass.)
- [x] **Shading Interpolation & Derivatives (documented residual, 2026-08, D3/D4):** Reyes shades at micropolygon-grid vertices and interpolates across the grid; raytrace shades once per ray-hit at the exact intersection point. Shading derivatives (`Du`/`Dv`) and interpolated variables (`s`/`t`/`u`/`v`) are therefore never bit-identical between the two hiders — this is the algorithmic/physical divergence the audit (`hider-parity-audity-and-roadmap.md`) explicitly flags as **not closable by refactor**: converging the two would mean giving raytrace reyes's grid-interpolation model (or vice versa), which is Option C's shared-grid/hybrid-hider territory (`PATH-TRACING_HIDER.md`), out of scope for this spec. It is a bounded, permanent residual, not a bug: `Parity_flatshade`/`Parity_aov` gate it at thresholds (20/25) sized to its measured worst-case (~5.67/~10.08) so a *regression* beyond that residual still fails the suite, even though the residual itself is not eliminated (spec 008 Phase 3/US1, `tests/visual/parity-thresholds.md`).
- [x] **Displacement Parity (default behavior, 2026-08):** Raytrace now displaces by default when a `Displacement` shader is bound, matching reyes — previously it required an explicit `Attribute "trace" "displacements" [1]` opt-in (`ATTRIBUTES_FLAGS_DISPLACEMENTS` defaulted off; gating condition in `src/libshader/shading/shading.cpp:676-683`). The existing `Attribute "trace" "displacements" [0]` mechanism still opts a primitive *out* if needed. **This is a default-behavior change**, not a silent absorption — scenes relying on the old "raytrace never displaces unless asked" behavior will now see displacement unless they add the opt-out. Cross-hider parity gated by `Parity_displacement` (`tests/visual/CMakeLists.txt`).
  - Dicing/tessellation *levels* are still NOT shared between the two hiders (the checkbox's original ask). Reyes dices micropolygon grids by `ShadingRate` (`estimateDicing()`, `surface.cpp:334-371`); raytrace's `CTesselationPatch::intersect()` (`surface.cpp:634-780`) instead derives its adaptive resolution purely from ray-differential screen footprint (`requiredR = cRay->da * t + cRay->db`, with `da`/`db` set uniformly per-frame in `raytracer.cpp:600-679` from `CRenderer::dxdPixel`/`imagePlane`) — `attributes->shadingRate` never enters that formula. For `ShadingRate` at or near its default of 1 the two happen to converge to comparable density, but a scene using a non-default `ShadingRate` for performance would see raytrace ignore it entirely while reyes honors it, a latent parity gap. Truly sharing one dicing-rate formula/tessellation cache between the two adaptive-resolution models is a substantially larger refactor (bordering on Option C's shared-grid territory) than this default-flip closes, and is not attempted here.
  - The parity test scene originally used the built-in `dented` shader (6-octave 1/f turbulence, `shaders/dented.sl`) but its high frequency content aliased into each hider's independently-computed dicing grid: reyes and raytrace tessellate the same sphere at slightly different parametric points, so even with both sides genuinely displaced the noise field sampled at those two grids diverges by about as much as the pre-fix "one side not displaced at all" bug did (~2.6-2.7/255 either way) — the metric could not tell the two cases apart. Fixed by giving the test its own low-frequency shader, `shaders/bump_lowfreq.sl` (single noise octave, large period), which both hiders' grids converge closely on; residual diff is now dominated by ordinary silhouette antialiasing (~2.3-2.7/255, 48-53/1200 blocks) vs. ~204/255 (240/1200 blocks) for a simulated regression — an ~80x separation, gated at threshold 6.
- [x] **Transparency Handling (closed, 2026-08, R3):** `opacityThreshold`, matte, and comp/non-comp AOV logic are now aligned through a single shared `CCompositor::composite()` (`src/ri/compositor.{h,cpp}`) — `CStochastic::rasterEnd`'s fragment-list walk and `CRaytracer`'s continuation-ray path (`CPrimaryBundle::postShade`) both populate a `CompositeSample` and call the same front-to-back "over" routine, rather than duplicating opacity/matte logic per hider. Gated by `Parity_transparency`/`Parity_matte`/`Parity_transparency-matte-aov` (spec 008 Phase 5/US3, T023-T036); deep-shadow map output is unaffected since the fragment-list data structure itself was not changed.

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

**Depth-filter avg/mid threading race (found 2026-08, during T041 verification):**
same-binary-vs-itself reruns of `depthfilter-avg-reyes.rib`/`depthfilter-mid-reyes.rib`
show a small but non-zero pixel diff (~2000-4000/76800 px, edge pixels near the
overlapping-sphere silhouettes) even with `PixelSamples`/scene held fixed. Root
cause isolated by elimination: `Hider "reyes" "jitter" [0]` did NOT remove it
(rules out per-thread jitter-stream drift, the mechanism above), but `-t:1`
did — confirming a genuine multi-threaded bucket-compositing race in
`CStochastic::rasterEnd`'s Stage-2 grid reduction (`stochastic.cpp`'s
`switch(CRenderer::depthFilter)` block), not mere floating-point
non-associativity. `min`/`max` (comparison-based reductions) are unaffected;
only `avg`/`mid` (summation-based) show it, consistent with a race in the
per-bucket `fbs`/`fb2` accumulation buffers at bucket-boundary pixels. Confirmed
pre-existing (present in code before and unrelated to the 008-hider-parity-convergence
T040/T041 `evaluateDepth` extraction — reproduces identically on both sides of
that refactor). Not fixed here; tracked as a residual defect, out of scope for
this spec.
