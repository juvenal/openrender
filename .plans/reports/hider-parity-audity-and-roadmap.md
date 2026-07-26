# Reyes vs. Raytrace Hider — Architectural Audit & Parity Roadmap

## Context

Goal: images from the `reyes` (CStochastic) and `raytrace` (CRaytracer) hiders should be
as close to identical as physically possible — including DOF, motion blur, and
transparency — at comparable render speed. Prior parity work (unified pixel-filter
kernel, stratified time sampling, du/dv derivative unification) proves the
"share-the-math" approach works; this audit maps what remains and how to get there.
Tracking doc: `DEVNOTES_DETAILS/HIDER_PARITY.md` (filtering/jitter done; motion blur,
transparency, displacement pending).

---

## Phase 1 — Current State

### Hider selection & threading
`CRenderer::beginFrame()` picks the hider via a strcmp chain (`src/ri/renderer.cpp:914-958`).
Both reyes and raytrace use the same bucket job dispatcher (`dispatchReyes`), one
`CShadingContext` subclass instance per thread.

### Shared base: CShadingContext (`src/libshader/shading/shading.h:263`)
Owns the entire shading pipeline (shader execution, lights, derivatives, message
passing) AND the ray-tracing core: `trace(CRayBundle*)` / `trace(CRay*)`
(`src/ri/trace.cpp:47,285`) with lazy per-object BVH + on-demand tessellation
(`CSurface::intersect`, `src/ri/object.cpp:532-573`). Crucially, **both hiders use this
same trace core** — reyes for `trace()/transmission()/gather()` shadeops, raytrace for
primary rays — so secondary effects (shadows, reflections) are already identical.

### Reyes hider (CReyes → CStochastic)
- `CReyes` (`src/ri/reyes.h:65`, `reyes.cpp`): bucket framework. Objects → `drawObject()`
  (screen bound + CoC expansion, `reyes.cpp:528-686`), diced front-to-back under
  occlusion probes (`render()`, `reyes.cpp:296-481`; `probeArea` → `COcclusionCuller`).
  Grids carry a flat vertex buffer in **sample space**:
  `[sx, sy, camZ, Ci(3), Oi(3), CoC][+extras]`, ×2 blocks when moving
  (`numVertexSamples`, `reyes.cpp:161-164`).
- Deferred shading: grids are first only displaced (`shadeGrid(grid, TRUE)` = P-only);
  full shading runs lazily the first time a sample passes the depth test
  (`drawPixelCheck`, `src/ri/stochasticQuad.h:342-366`) — occlusion-culled shading.
- `CStochastic` (`stochastic.h:39`): per-sample framebuffer `CPixel` with jitter `jx/jy`,
  stratified time `jt`, Sobol rejection-sampled aperture disk `jdx/jdy`
  (`stochastic.cpp:128-231`); per-sample depth-sorted **fragment linked list** for
  transparency (`findSample/updateOpaque/updateTransparent`, `stochastic.cpp:261-392`);
  `rasterEnd` composites front-to-back, applies depth filters (min/max/avg/mid,
  `stochastic.cpp:780-918`), and pixel-filters subsamples with the shared precomputed
  kernel (`stochastic.cpp:925-1001`). Also hosts deep-shadow output
  (`deepShadowCompute`, `stochastic.cpp:1302`).
- Rasterization kernel: `stochasticPrimitives.h` (13k lines) macro-expands ~2^N variants
  over grid flags (`RASTER_MOVING/TRANSPARENT/FOCALBLUR/MATTE/LOD/XTREME…`,
  `reyes.h:40-58`); the real kernel is `stochasticQuad.h` (point-in-quad, bilinear u/v,
  z lerp, fragment insert).

### Raytrace hider (CRaytracer)
- `raytracer.cpp:308-377`: same bucket loop; per bucket `sample()` generates jittered
  primary rays in 8×8 tiles (`raytracer.cpp:385-479`), stratified time copied from the
  stochastic formula (`raytracer.cpp:432-444`), traces via the shared `trace()`,
  transparency handled by **continuation rays** (`CPrimaryBundle::postShade`,
  `raytracer.cpp:92-259`; re-trace from `t+ε`, `trace.cpp:216-222`), then splats through
  the pixel filter with per-pixel weight normalization (`splatSamples`,
  `raytracer.cpp:579-657`).
- Ray-hit shading: rays are hash-batched per object (`trace.cpp:100-204`) →
  `CSurface::shade` (`object.cpp:638-659`) sets u/v/time/du(ray differential)/I →
  `CShadingContext::shade` SHADING_2D path shades **3 points per hit** (main + u-offset
  + v-offset) for derivatives (`shading.cpp:778-859`).

### Visual effects, per hider

| Effect | Reyes | Raytrace |
|---|---|---|
| **DOF** | Per-vertex CoC (`cocSamples`, `renderer.h:39`; factors `renderer.cpp:602-615`) stored at vertex[9]; per-sample screen-space offset `jdx/jdy × CoC` (`stochasticQuad.h:547-571,827-851`); bounds expanded (`reyes.cpp:613-627,1345-1362,1566-1598`). Screen-space scatter approximation — no occlusion change behind blurred edges. | Physical lens: ray origin jittered on aperture, focus at `focaldistance` (`computeSamples`, `raytracer.cpp:513-537`). **Bug/divergence: `r = urand()*aperture` without √ → center-heavy bokeh**, vs stochastic's uniform Sobol disk. |
| **Motion blur** | Shade/displace at t=0 and t=1 only (`PARAMETER_BEGIN/END_SAMPLE`, `reyes.cpp:954,1008`); per-sample LERP of raster position **and shaded color** at `jt` (`stochasticQuad.h` MOVING variants); camera rotation via SLERP unproject/rotate/reproject (`stochasticQuad.h:445-544,765-825`; bounds `reyes.cpp:1311-1342,1600-1634`). | Stratified `ray->time`; geometry intersected with time-lerped vertices (`patches.cpp:234-240`, `polygons.cpp:180-184,747-753`, `quadrics.cpp` many); camera motion via per-object xform lerp in `transform()` (`objectMisc.h:180-218`); shading evaluated **at the hit time**. `DEVNOTES.md:39` still flags moving raytraced surfaces as incomplete — coverage is partial/unverified for the tessellation path. |
| **Transparency** | Depth-sorted fragment list per sample; front-to-back accumulation with `opacityThreshold` stack culling; matte encoded as **negative opacity** (`stochastic.cpp:335-392,516-560`); `zvisibilityThreshold` respected for z; feeds deep shadows. | Iterative continuation rays composited via residual opacity `ropacity` (`raytracer.cpp:186-222`). **Extra AOVs taken from first hit only** (`raytracer.cpp:221`); no `zvisibilityThreshold`; no depth-filter modes (z = first-hit t); matte handled differently (`raytracer.cpp:105,195`). |

---

## Phase 2 — Dependency & Reuse Audit

### Dependency map (bounds shown as ownership)

```
                       CRenderer (static globals: options, filter kernel, thresholds,
                       sampleOrder/AOV layout, buckets, displays, job dispatch)
                          ▲ read directly by everything below (~40 statics)
                          │
      ┌───────────────────┴────────────────────────┐
      │            CShadingContext                 │  src/libshader/shading/ + src/ri/trace.cpp
      │  shade()/execute()/lights/derivatives      │  ← the real shared engine
      │  trace()/traceEx() + BVH + tessellation    │
      └──────┬──────────────────────────┬──────────┘
             │                          │
        CReyes (bucket raster fw)   CRaytracer ──── CPrimaryBundle (transparency
       ┌─────┴─────┐                 (primary rays,   compositing, AOV copy)
   CStochastic   CZbuffer            splat filter)
   (+COcclusionCuller,
    fragments, deep shadows)
   uses stochasticPrimitives.h / stochasticQuad.h / stochasticPoint.h
```

### Beneficial reuse (keep)
1. **Shading engine** — one shader executor for both hiders means Ci/Oi match by
   construction for identical inputs. Recent proof: grid du/dv now uses the same
   footprint formula as the ray path (`shading.cpp:893-930`).
2. **Trace core + tessellation** — shadows/reflections/indirect identical across hiders.
3. **Pixel filter kernel** — precomputed once in `beginFrame` (`renderer.cpp:840-861`),
   consumed by both (HIDER_PARITY item, done).
4. **AOV channel tables** (`CRenderer::sampleOrder/compChannelOrder/…`), math libs.

### Bad / risky reuse
1. **The hider "interface" is really the reyes interface**: `drawObject/drawGrid/drawPoints`
   are pure virtuals on `CShadingContext` that `CRaytracer` stubs out (`raytracer.h:90-94`).
2. **CRenderer static-global soup**: both hiders re-derive projection/jitter/filter math
   from raw statics; three independent pixel-filter implementations exist
   (`stochastic.cpp:925`, `raytracer.cpp:613`, zbuffer). `CRendererServices` (Phase B)
   exists but hiders bypass it.
3. **Duplicated-by-copy sampling code**: jitter/time/lens generation written twice with
   hand-copied magic constants (stochastic `0.5001011` vs raytrace pixel jitter `+0.5`,
   `stochastic.cpp:163-170` vs `raytracer.cpp:429-430`) — drift has already happened.
4. **Duplicated transparency/matte compositing**: fragment-list macros vs
   `CPrimaryBundle::postShade`, with divergent matte, AOV, and threshold semantics.
5. **stochasticPrimitives.h macro combinatorics**: every parity fix touching
   interpolation must be re-applied across expanded variants (camera-rotation fix is
   duplicated verbatim at `stochasticQuad.h:493-544` and `765-825`).

### Exact divergence points (homogeneity impact)
| # | Divergence | Where |
|---|---|---|
| D1 | Lens sample distribution (uniform disk vs center-heavy polar) | `stochastic.cpp:175-192` vs `raytracer.cpp:523-526` |
| D2 | Pixel jitter constant (0.5001011 vs 0.5) | `stochastic.cpp:163` vs `raytracer.cpp:429` |
| D3 | Shading density & interpolation: grid-vertex shading + bilinear Ci vs per-hit shading | `stochasticQuad.h` vs `shading.cpp:778` |
| D4 | Motion-blur shading time: endpoint color lerp vs hit-time evaluation | `stochasticQuad.h:127-157` vs `object.cpp:652` |
| D5 | Displacement skipped for ray hits unless `Attribute "trace" "displacements"` | `shading.cpp:678-682` |
| D6 | Transparent-hit AOVs first-hit-only; no comp-channel compositing | `raytracer.cpp:221` |
| D7 | Depth filters (min/max/avg/mid) & `zvisibilityThreshold` unimplemented in raytrace | `stochastic.cpp:780-918` vs none |
| D8 | Matte semantics (negative-opacity fragments vs ropacity carve-out) | `stochastic.cpp:516+` vs `raytracer.cpp:105,195` |
| D9 | DOF occlusion model (screen scatter vs true lens rays) — physically irreconcilable | design-level |
| D10 | Raytraced object motion blur partial (tessellation-path verification open) | `DEVNOTES.md:39`, `HIDER_PARITY.md:9` |

Performance shape: reyes amortizes shading per grid and occlusion-culls unshaded grids;
raytrace shades 3 points per hit sample with no cross-sample reuse → cost scales with
`PixelSamples`. Ray batching by object (`trace.cpp:100`) recovers grid-sized shader
batches, so the gap is mostly redundant shading, not shader dispatch.

---

## Phase 3 — Refactoring Recommendations

### Decouple / Remove
- **R1** Split the hider contract: keep `CShadingContext` as the shading+trace engine;
  move `drawObject/drawGrid/drawPoints` down into `CReyes` (raytracer's stubs disappear).
  Purely mechanical; `renderer.cpp` call sites only ever call these on reyes-family hiders.
- **R2** Extract a shared `CSampler` (per pixel-sample: jitter xy, time stratum, lens
  point, importance) owned by the core and consumed by both `CStochastic::rasterBegin`
  and `CRaytracer::sample`. Single home for the magic constants; kills D1/D2 forever.
- **R3** Extract one transparency/matte compositor (front-to-back "over" with
  opacityThreshold, matte, comp/non-comp AOV rules) used by `rasterEnd` and
  `CPrimaryBundle::postShade`. Kills D6/D8 drift.
- **R4** One pixel-filter splat/gather module (kernel eval + normalization policy) for
  all three consumers.

### Share / Integrate (lift into core)
- **S1** Canonical lens/CoC model: derive reyes `cocSamples` and raytrace lens rays from
  one set of formulas; fix raytrace disk sampling (√r or reuse the Sobol rejection disk).
- **S2** Displacement parity: default raytrace displacement to match reyes (or a
  documented parity option), sharing dicing rates through the tessellation cache (D5).
- **S3** Implement depth-filter modes + `zvisibilityThreshold` once; consume in both (D7).
- **S4** Transparent-hit AOV compositing in raytracer via the existing
  `compChannelOrder/nonCompChannelOrder` tables (D6).
- **S5** Finish/verify raytraced object motion blur on the tessellation path (D10) —
  the intersection kernels already interpolate on `ray->time`.

---

## Phase 4 — Adversarial Validation

**Evidence the plan works (for):**
- Precedent: filter-kernel unification, stratified-time copy (`raytracer.cpp:432-435`
  documents it eliminated graininess), and du/dv unification (`shading.cpp:893-897`)
  each measurably closed a gap without architectural surgery.
- The shared trace core already guarantees secondary-effect parity; the remaining gap is
  confined to primary visibility + compositing — exactly what R2-R4/S1-S5 target.
- `CRendererServices` scaffolding exists as an injection point; no new architecture needed.

**Evidence for caution (against):**
- **D3/D4 are algorithmic, not implementational.** Reyes shades pre-visibility at grid
  vertices and bilinearly interpolates; raytrace shades post-visibility per hit. No
  shared kernel makes lerped Ci equal point-evaluated Ci for nonlinear shaders. Parity
  here is asymptotic (ShadingRate→0), never bit-exact. Forcing either side to emulate
  the other is a rewrite (Option C), not a refactor.
- **D9 is physical.** Screen-space CoC scatter cannot reveal occluded background the way
  lens rays do. Only blur radius and bokeh distribution can be matched.
- **Macro kernels are perf-tuned and MT-fragile** (see CLAUDE.md gotcha #6 — the
  nullBucket early-out bug). Converting fragment macros to shared functions risks both
  perf regressions and races; mitigate with header-inline templates and the visual suite,
  and keep the fragment-list data structure untouched (deep shadows read it directly,
  `stochastic.cpp:1302-1415`).
- **Motion-blur memory layout is load-bearing**: the ×2 endpoint vertex block
  (`reyes.cpp:161-164`) is assumed throughout 13k lines of expanded raster variants.
  Multi-time shading in reyes is out of scope for a refactor.
- **Test gap**: only teapot-* raytrace visual tests exist; there is no same-scene
  cross-hider comparison today, so the safety net must be built *first*.

**Verdict:** shared-kernel convergence (below, Option A/B) is well-evidenced; full
unification (Option C) belongs in the planned path-tracing hider effort
(`DEVNOTES_DETAILS/PATH-TRACING_HIDER.md`), not here.

---

## Phase 5 — Strategic Action Plan

### Options
- **A. Conservative — Shared-kernel convergence (recommended):** keep both
  architectures; execute R1-R4 + S1-S5; add a cross-hider A/B test harness. Closes every
  divergence except the algorithmic pair D3/D4/D9, which become bounded, documented
  residuals. Low-moderate risk, weeks-scale.
- **B. Moderate — Unified sample front-end:** Option A plus one per-bucket sample table
  (position/time/lens per pixel-sample) generated by `CSampler` and consumed verbatim by
  both hiders, so the *noise patterns correlate* and A/B diff thresholds can be tightened
  dramatically. Adds determinism/replay benefits. Recommended as A's second stage.
- **C. Aggressive — Hybrid hider:** one pipeline with pluggable visibility (raster vs
  ray) over a shared sample→shade→composite spine; grid shading becomes a shading cache
  for ray hits (shade-on-grid, interpolate at hits) converging look *and* speed
  (PRMan-style raytraced Reyes). Large rewrite; fold into the future `pathtracer` hider
  instead of retrofitting.

### Recommended execution order (Option A, then B)
1. **Parity harness first**: add `tests/visual` scene pairs rendered with both hiders
   (flat-shade, DOF, motion, transparency, matte, AOVs) diffed against each other with
   per-effect thresholds — this is the regression net for everything after.
2. **Cheap definite fixes** (each independently verifiable): raytrace disk-sampling √r
   (D1), jitter constant (D2), transparent-hit AOVs (D6), depth filters +
   zvisibilityThreshold (D7), matte alignment (D8), displacement default/option (D5).
3. **Extract `CSampler`** (R2) and switch both hiders to it.
4. **Extract compositor + filter modules** (R3/R4); re-verify deep shadows.
5. **Verify/finish raytraced motion blur** on the tessellation path (S5, D10); update
   `HIDER_PARITY.md` checkboxes as items land.
6. **Stage B**: per-bucket shared sample table; tighten A/B thresholds.

### Verification
- `ctest --test-dir build -L visual --output-on-failure` (existing 33+ scenes must not
  regress; note the pre-existing stale-`.slo` deploy-tree failures).
- New cross-hider A/B tests from step 1 gate every subsequent step.
- Perf: time `examples/rib/camera-dof.rib` (and a motion scene) under both hiders before
  and after each extraction; raster hot loops must not regress >2-3%.

### Key files to touch
`src/ri/raytracer.cpp`, `src/ri/stochastic.cpp`, `src/ri/stochasticQuad.h`,
`src/ri/reyes.{h,cpp}`, `src/libshader/shading/shading.{h,cpp}`, `src/ri/renderer.cpp`
(sampler/compositor wiring), new `src/ri/sampler.{h,cpp}` + `src/ri/compositor.{h,cpp}`
(names TBD), `tests/visual/CMakeLists.txt` + new RIB pairs,
`DEVNOTES_DETAILS/HIDER_PARITY.md`.
