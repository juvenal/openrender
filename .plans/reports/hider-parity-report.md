# Stochastic vs. Raytrace Hider Parity — Investigation Report

Investigation scope: `src/ri/raytracer.{h,cpp}`, `src/ri/stochastic.{h,cpp}`, `src/ri/renderer.cpp`, `src/ri/rendererContext.cpp`, `src/ri/options.cpp`, `src/ri/attributes.cpp`, `src/ri/zbuffer.h`, `DEVNOTES_DETAILS/HIDER_PARITY.md`. All findings are cited to file:line and verbatim code.

---

## 1. Analysis Result

Each premise is evaluated against current code, not against `HIDER_PARITY.md` — that doc is itself stale (see §1.6).

### 1.1 "Raytrace doesn't use stochastic techniques for motion blur/DOF, yet loses little quality and runs far faster"

**Partially wrong.** `CRaytracer` *does* use stochastic (jittered, stratified) sampling for both motion blur and DOF — it's not a deterministic/analytic method:

- `CRaytracer::sample()` (raytracer.cpp) draws `pixelXsamples × pixelYsamples` jittered sub-pixel positions per pixel, with time-sample jitter for motion, and an explicit code comment stating the time-stratification formula "match[es] the stochastic hider's formula exactly."
- `CRaytracer::computeSamples()` draws a jittered lens-sample position per ray for DOF.

So both hiders are stochastic samplers over the same variables (pixel, time, lens). What differs is *what gets resampled per sample*: raytrace re-*shades* on every one of those samples (ray-hit shading), while stochastic re-*visibility-samples* on every one of those samples but shades once per micropolygon-grid vertex at `ShadingRate` resolution and interpolates. That's the real mechanism behind "faster, small quality loss" — see §1.4.

### 1.2 "Raytrace does automatic 4 samples via jittered rays"

**Correct only as a description of the shared default — not a raytrace-specific behavior.** `options.cpp:276-277`:

```cpp
pixelXsamples = 2;
pixelYsamples = 2;
```

2×2 = 4 is `COptions`'s **default `PixelSamples`**, consumed identically by both hiders (`CRenderer::pixelXsamples`/`pixelYsamples`, populated once in `beginFrame()`). `CRaytracer::sample()` computes ray count as `pixelXsamples * pixelYsamples` — it reads the same option, not a hardcoded constant. If you `RiPixelSamples(4,4)`, raytrace casts 16 rays/pixel, not 4. This is very likely the origin of the "automatic 4 samples" belief: at the default settings, raytrace *does* shoot exactly 4 rays/pixel — but so does stochastic's visibility sampling, and the count moves with the RIB setting for both.

### 1.3 "Raytrace ignores PixelSamples, ShadingRate, ShadingInterpolation → PixelFilter may be irrelevant"

**Mixed — three different answers, not one:**

| Setting | Honored by raytrace? | Evidence |
|---|---|---|
| `PixelSamples` | **Yes**, identically to stochastic | `xsamples = pixelXsamples * pixelYsamples` in `sample()`; same global `CRenderer::pixelXsamples/Ysamples` |
| `PixelFilter` | **Yes**, identically to stochastic | `splatSamples()` uses the same `CRenderer::pixelFilterKernel`/`pixelFilter()` machinery as `CStochastic::rasterEnd()` |
| `ShadingRate` | **No — by architecture, not omission** | `CRaytracer::drawGrid()`/`drawObject()` are empty stubs (raytracer.h). Raytrace never dices a micropolygon grid; it shades once per ray-hit. `ShadingRate` is a *dicing-density* control with no raytrace analog — there's no grid to dice finer or coarser. |
| `ShadingInterpolation` | **No — but this is true for BOTH hiders**, not raytrace-specific | `CRendererContext::RiShadingInterpolation()` body is a single comment: `// Unimplemented: renderer always uses smooth shading interpolation.` It's a global no-op regardless of hider. |

So the premise "raytrace ignores these settings → PixelFilter must be irrelevant too" doesn't hold: `PixelFilter` is fully honored by raytrace via the exact same reconstruction-filter code path as stochastic. The chain breaks at `ShadingRate`, and that break is a real architectural fact (no grid to dice), not a parity gap that can be silently closed.

### 1.4 "In scenarios where stochastic should shine, improvement over raytrace is only marginal at high compute cost"

**Scene- and shader-cost dependent — not a general truth.** The actual trade-off:

- **Stochastic**: shades once per micropolygon-grid vertex at `ShadingRate` density (default 1.0, i.e. ~1 shaded sample/pixel-area), then supersamples *visibility only* at `PixelSamples`. Shading cost is decoupled from `PixelSamples` — you can crank visibility samples for smoother edges/DOF/motion without paying more shader evaluations. The cost is that shading itself can alias into the coarse grid (a shading pattern finer than the dicing shows jaggies stochastic's visibility supersampling can't fix — this is the exact trap called out in this repo's own testing notes: *"Cranking PixelSamples alone does not supersample shading on a REYES/stochastic render... Use the raytrace hider for shading ground-truth"*).
- **Raytrace**: shades on every ray-hit, so `PixelSamples` multiplies shading cost linearly — but shading is genuinely supersampled, alias-free, ground-truth.

So: for **cheap shaders** or **low `PixelSamples`**, raytrace's linear shading cost is negligible and it wins or ties on both speed and quality — consistent with what you're observing. For **expensive shaders** (heavy texture lookups, ray-traced GI/shadows inside the shader, deep procedural math) or **high `PixelSamples`**, stochastic's shade-once-supersample-visibility split becomes a real, non-marginal win, and shading-aliasing becomes the counter-argument for raytrace. Neither hider is strictly "better" — the win depends on shader cost × sample count, which is exactly why tuning settings can make the gap "huge" in either direction depending which regime you land in.

### 1.5 Motion blur scope: camera motion vs. deforming-geometry motion

Two genuinely different mechanisms exist, and only one is a real raytrace gap:

- **Camera motion blur** (`MotionBegin`/`MotionEnd` around the camera transform): fully shared and working in both hiders. `addObject()` (rendererContext.cpp) clones a private per-object xform with a `next` pointer computed via `fromWorld1 * toWorld`; `transform()` (objectMisc.h) LERPs `xform->to`/`xform->next->to` at sample time `t`. `raytracer.cpp` documents this explicitly: camera motion blur is *"handled entirely by the per-object xform interpolation... No explicit ray transformation is needed here."* Both hiders get this for free from the same code.
- **Deforming/moving-surface motion blur** (multi-sample vertex data on the object itself): genuinely missing in `CRaytracer`. `DEVNOTES.md` lists this open: *"[ ] Moving raytraced surface (CRaytracer lacks native motion blur support)."* This is the one real, still-open motion gap — not a general "raytrace has no motion blur" as the premise implied.

### 1.6 `HIDER_PARITY.md` is itself out of date

The doc's checklist (`DEVNOTES_DETAILS/HIDER_PARITY.md`) checks off *Unified Pixel Filtering* and *Sampling Distribution (jitter)*, and leaves *Motion Blur Implementation* unchecked with the note "CRaytracer needs to implement support for moving surfaces" — accurate as far as it goes. But it doesn't mention `PixelSamples` or DOF at all (both are in fact already at parity), and its "Possible Optimization" section is empty. If this doc is where the "raytrace ignores most settings" impression came from, it's explainable: the doc undersells what's already implemented and doesn't distinguish camera-motion (done) from object-motion (open). Recommend updating it once the spec plans below land — flagged here rather than fixed silently, since you may want to fold it into the spec-kit work instead.

### 1.7 DOF sampling: a concrete, independently real quality bug

Not part of the original premises, but found during this pass and directly relevant to Question 2:

- `CStochastic::rasterBegin()` samples the lens aperture via `CSobol<2> apertureGenerator` with **rejection sampling** onto the unit disk — correct, low-discrepancy, area-uniform.
- `CRaytracer::computeSamples()` samples the lens via naive polar coordinates: `r = urand() * aperture; theta = urand() * 2π`. This is **not area-uniform** — points cluster toward the disk center (the Jacobian of polar→Cartesian is `r dr dθ`, so uniform `r` under-samples the outer annulus). Concretely this biases bokeh shape/falloff and out-of-focus energy distribution toward the lens center, most visible as tighter-than-correct bokeh circles and slightly under-blurred backgrounds.
- Fix is a one-line, same-cost change: `r = aperture * sqrtf(urand())`.

---

## 2. Answers to Your Questions

### Q1 — Can raytrace obey the same RIB settings as stochastic, with the same defaults?

**Already true for `PixelSamples`, `PixelFilter`, `Shutter`/motion time, jitter, and camera motion** — same global state, same defaults (`PixelSamples 2 2`, `PixelFilter "catmull-rom" 2 2`), same code paths. Nothing to build there; if you're seeing divergence at those settings today, it's more likely a rendering-difference from shading-once-vs-per-hit (§1.4) than a settings-parity bug — worth a matched-settings A/B render to confirm before assuming otherwise (see note at end of §3).

**`ShadingRate` cannot obey the same semantics** — it's a dicing-density control and raytrace doesn't dice. It *could* be given a raytrace-specific meaning (e.g., driving a shading-cache/interpolation grid over ray hits — see Q3), but that's a new feature, not exposing an existing one.

**`ShadingInterpolation`** is moot — it's unimplemented everywhere, this isn't a raytrace-specific ask.

**Net:** three of four settings already have full parity today. The fourth (`ShadingRate`) needs a deliberate design decision, not a bug fix — see Spec Plan A in §3.

### Q2 — How to raise raytrace motion-blur/DOF quality without stochastic's full compute cost?

Given raytrace's current output is "already ok, not great, but ok," the highest-leverage, lowest-risk moves are ones that improve *sample quality* at the *same sample count*, not ones that add more rays:

1. **Fix DOF disk sampling bias** (§1.7) — `r = aperture * sqrtf(urand())`. Same ray count, corrects lens-sample distribution, directly improves bokeh accuracy. Zero cost.
2. **Reuse stochastic's `CSobol<2>` low-discrepancy sampler** for both the lens and sub-pixel jitter in raytrace, instead of independent `urand()` draws per axis. Low-discrepancy sequences reduce variance (visible noise) at equal sample count vs. independent uniform jitter — this is likely the single biggest "quality per ray" lever available without touching the ray budget.
3. **Add native deforming-surface motion blur to `CRaytracer`** (the one real open gap, §1.5) — currently a moving *object* (not moving camera) renders raytrace with no blur at all, which is a correctness gap, not just a quality one, for any scene using vertex-level `MotionBegin/End`.
4. **Optional, higher effort:** adaptive/importance sampling — bias ray density toward pixels with high sample-to-sample variance (edges, DOF circle-of-confusion boundaries) instead of uniform `PixelSamples` everywhere. Bigger win, bigger implementation (needs a variance-estimation pass), so scope separately from 1-3.

Items 1-3 are additive, independent, and don't require touching stochastic at all — good candidates for the spec plans below.

### Q3 — Is a shared-code design possible between the two hiders?

**Yes, and much of it already exists** — this isn't starting from zero:

- **Already shared:** `CRenderer::pixelFilterKernel`/`pixelFilter()`, `CRenderer::pixelXsamples/Ysamples`, `CRenderer::jitter`, camera-motion xform interpolation (`addObject`/`transform`), time-sample stratification formula.
- **Newly shareable (from Q2 fixes):** the lens-sampling routine (`CSobol<2>` rejection-sampled disk) — currently duplicated with two different, non-equivalent implementations in `stochastic.cpp` and `raytracer.cpp`. Extracting one `sampleLensDisk()` helper both hiders call removes the DOF bug at the source (can't drift out of sync again) and is a small, self-contained refactor.
- **Structurally harder to share:** the shading dispatch itself. Stochastic shades grids (`CProgrammableShaderInstance` bound per-grid-vertex via micropolygon dicing); raytrace shades single ray-hit points. These are different call shapes into the same shader runtime, not different runtimes — the shared substrate is `libshader_runtime`/`libshader_shading`, already factored out per `DEVNOTES.md`'s Phase A/B libshader extraction. Full unification of shading dispatch is a large, invasive change with real regression risk (this is deliberately **not** proposed as a spec plan below — flagging it as a non-goal for now, worth a separate discussion if you want it later).

**Recommendation:** pursue the small, low-risk shared-code wins (lens sampler, possibly a shared stratified-sampling helper) as part of the Q2 spec plan rather than a big-bang unification. That gets you convergence *and* maintainability benefit without the risk of a shading-pipeline merge.

---

## 3. Draft Spec Plans (independent, parallelizable)

Each is scoped to stand alone — no plan requires another to land first, and none touches the same code region as another (checked below each one). None of these have been formalized into `specs/NNN-.../` via spec-kit yet — flagging that as a deliberate choice pending your review of this draft; happy to run `speckit.specify` on any of these once you confirm scope.

### Spec Plan A — Raytrace-native `ShadingRate` semantics
**Goal:** give `ShadingRate` a meaningful effect under the raytrace hider instead of being silently ignored.
**Scope:** define what "coarser shading" means with no dicing grid — most natural mapping is a **shading cache/reuse radius**: hits within a `ShadingRate`-derived screen-space radius of an already-shaded point reuse (or cheaply interpolate) that shade instead of re-invoking the shader. Needs a design decision on cache eviction/keying (surface ID + screen bucket) before implementation.
**Touches:** `raytracer.cpp` (`CRaytracer::sample`/new caching layer), `rendererContext.cpp` (`RiShadingRate` — read attribute, no change needed there). Does not touch `stochastic.cpp` or the shared filter/sample-count code.
**Risk:** medium — a shading cache is new state, needs careful invalidation logic to avoid stale-shade artifacts across moving geometry/animated shaders.
**Independent of:** B, C, D (separate file, separate concern).

### Spec Plan B — Correct + shared lens (DOF) sampling
**Goal:** fix the raytrace DOF center-bias (§1.7) and de-duplicate the disk-sampling code between hiders.
**Scope:** extract stochastic's `CSobol<2>`-based rejection-sampled disk routine into a shared helper (e.g. `common/` or a small `sampling.h` under `src/ri/`); call it from both `CStochastic::rasterBegin()` and `CRaytracer::computeSamples()`, replacing the biased polar formula.
**Touches:** new small shared file + two call sites (`stochastic.cpp`, `raytracer.cpp`). Read-only with respect to `renderer.cpp`/`rendererContext.cpp`.
**Risk:** low — self-contained numerical fix, easy to validate visually (bokeh shape on an out-of-focus point-light test scene) and against existing DOF visual-regression tests.
**Independent of:** A, C, D. This is the smallest, lowest-risk plan — good candidate to do first as a proof of the "shared helper" pattern the others can follow.

### Spec Plan C — Low-discrepancy stratified sampling for raytrace pixel/time jitter
**Goal:** reduce raytrace noise at equal `PixelSamples` by replacing independent `urand()` jitter with a Sobol/stratified sequence for sub-pixel and time sampling, matching (or sharing) stochastic's approach.
**Scope:** audit `CRaytracer::sample()`'s jitter formula against `CStochastic::rasterBegin()`'s `jx/jy/jt` computation; where they already claim to match (per the existing code comment), verify the *sequence generator* matches too, not just the formula shape. Introduce shared sequence generation if not already shared.
**Touches:** `raytracer.cpp` (`sample()`), possibly `stochastic.cpp` if promoting to a shared generator.
**Risk:** low-medium — mostly numerical, but changes visible noise pattern, so needs a visual-regression baseline update (expected, not a bug).
**Independent of:** A, B, D. Slight sequencing note: doing B first establishes the "shared sampling helper" location/pattern this plan can reuse, but C does not require B to be merged — can be developed in parallel and reconciled at merge time.

### Spec Plan D — Native deforming-surface motion blur in `CRaytracer`
**Goal:** close the one confirmed real motion-blur gap — moving/deforming geometry (not camera) currently renders unblurred under raytrace.
**Scope:** raytrace needs to interpolate multi-sample (`MotionBegin/End`) vertex data at each ray's sampled time `t`, analogous to what REYES dicing already does per-grid-vertex. Since raytrace has no grid, this likely means either (a) building the BVH/intersection acceleration structure per time-sample bucket, or (b) interpolating vertex positions at ray-generation time before intersection test, mirroring the *existing* camera-motion xform-interpolation pattern but applied to object-local vertex data instead of the camera transform.
**Touches:** `raytracer.cpp`, object/geometry intersection code (likely `objectMisc.h` / wherever primary-ray intersection is dispatched — not yet fully mapped in this investigation; scoping this precisely is the first task of the spec, not assumed here).
**Risk:** highest of the four — touches the intersection/acceleration-structure path, correctness-sensitive, needs its own visual-regression scenes (moving sphere/deforming patch under raytrace vs. stochastic ground truth).
**Independent of:** A, B, C — different subsystem (intersection, not sampling/shading-rate).

**Suggested validation step before any of these land:** a matched-settings A/B render (same `PixelSamples`, `ShadingRate`, `PixelFilter`, DOF, motion) between stochastic and raytrace on a scene exercising all four axes, to get a concrete baseline for "how much does each fix actually close the gap" — closes the "did I just see stale/default-mismatched behavior" possibility flagged during this investigation, and gives each spec plan an objective before/after.

---

## 4. A-Buffer Spec Plan — Held Pending Source Material

Before drafting this one, two scoping corrections worth confirming with you, since they change what the spec should actually propose:

1. **There is no z-buffer inside `CStochastic` to replace.** `CZbuffer` (`src/ri/zbuffer.h`) is a **separate, independently-selectable hider** (`Hider "zbuffer"`) — a sibling of `CStochastic`, not a stage nested inside it. What `CStochastic` actually does at the end of its pipeline (`CStochastic::rasterEnd()`, `stochastic.cpp`) is **not** a z-buffer at all: it's depth-sorted **fragment-list compositing** — each of `pixelXsamples × pixelYsamples` jittered point-samples per pixel holds a linked list of `CFragment`s (color/opacity/z), inserted in depth order and alpha-composited front-to-back (handling mattes, AOVs, and `DEPTH_MIN/MAX/MID` filter modes), followed by a separate reconstruction-filter pass across the sample grid into final pixel color. This is the Cook/Carpenter/Catmull 1987 REYES stochastic-visibility design — architecturally later and different from, though descended from, Carpenter's 1984 A-buffer. So "replace the z-buffer" should really be read as "replace/augment this point-sampled fragment compositing with Carpenter's analytic coverage-mask compositing" — worth confirming that's the intent before a spec locks it in.

2. **Carpenter's 1984 A-buffer is a visibility/edge-antialiasing algorithm, not a DOF/motion-blur algorithm.** Its core mechanism — per-micropolygon analytic sub-pixel coverage bitmasks composited via bitwise AND/OR/XOR — solves geometric edge aliasing cheaply. It does not have a native notion of stochastic time or lens samples; PRMan's actual history is the opposite direction (moved from A-buffer toward stochastic point-sampling specifically *to* get cheap, correct motion blur and DOF, which analytic coverage masks don't naturally extend to). So this spec should **not** be pitched as a fix for the DOF/motion quality questions above (§2 Q2) — that's Spec Plans B/C/D's job. The A-buffer's value proposition here is edge-AA quality/cost at *static-geometry, single-time-sample* pixels, coexisting with (not replacing) the existing stochastic time/lens sampling for the motion/DOF cases.

**I'd like the paper before drafting this spec**, as you offered — reconstructing Carpenter '84's exact coverage-mask/run-length encoding and compositing rules from memory risks getting a subtle detail wrong (mask resolution, compositing order edge cases, the exact run-length representation) in a spec that's meant to guide real implementation. Once I have it, I'll scope a plan covering: mask resolution choice, per-micropolygon coverage computation (likely reusing existing tessellation/`Ng` computation in `patches.cpp`), compositing-operator implementation, and — critically — how/whether it coexists with the current fragment-list path for the motion-blur/DOF case (dual-path vs. unified), plus a decision on whether this becomes a third `Hider` option, a `CStochastic` sub-mode, or gated by a new attribute/option.

If you'd rather I sketch the coverage-mask design at a high level now from the general literature (not the specific 1984 paper) so you have something to react to sooner, I can — but given the risk of subtle inaccuracy, my recommendation is to wait for the paper.
