# REYES Paper vs. openRender: A Comparative Analysis

This doc compares openRender's REYES hider implementation against "The Reyes
Image Rendering Architecture" (Cook, Carpenter, Catmull; SIGGRAPH 1987,
`docs/references/papers/37402.37414.pdf`), the founding paper for the
architecture openRender's rasterizer is built on. The goal is to separate
three kinds of divergence: places the code has genuinely improved on a
40-year-old design, places it diverges neutrally on implementation detail,
and places it falls short of either the paper's own stated goals or the
codebase's own stated intentions (FIXMEs, disabled code, documented
residuals). It complements [HIDER_PARITY.md](HIDER_PARITY.md), which tracks
convergence *between* openRender's own hiders rather than against the
original architecture.

The paper describes one monolithic algorithm. openRender splits it across a
small class hierarchy: `CShadingContext` (`src/ri/shading.h`) is the abstract
hider base; `CReyes` (`src/ri/reyes.h`/`reyes.cpp`) owns the bound → split →
dice → bucket-dispatch loop common to both rasterizing hiders; `CStochastic`
(`src/ri/stochastic.cpp`) — selected by RIB `Hider "reyes"` (or the
deprecated `"hidden"`/`"stochastic"` aliases) — adds jittered point sampling,
motion blur, and depth of field on top of `CReyes`; `CZbuffer`
(`src/ri/zbufferQuad.h`) is a separate deterministic fixed-sample fast path
sharing the same `CReyes` dicing base but with no motion/DOF support.
`CRaytracer` (`src/ri/raytrace.cpp`) is architecturally a sibling of
`CReyes`, not a subclass — it shades per ray-hit via on-demand
`CTesselationPatch` tessellation rather than the classic grid-dice pipeline,
so it is out of scope for most of this comparison except where it bears on
REYES design tradeoffs directly.

## Where the implementation is faithful to the paper

- **Recursive bound/split/dice loop.** `CPatch::dice()` and
  `CPatch::splitToChildren()` (`src/ri/surface.cpp`) implement the same
  bound → cull → (split | dice) recursion as the paper's Figure 3 pseudocode,
  including splitting primitives that straddle the eye plane before they can
  be safely diced.
- **`ShadingRate`-driven dicing.** `CObject::estimateDicing()`
  (`src/ri/object.cpp:348`) computes projected screen-space perimeter length
  and divides by `ShadingRate` to get grid resolution — the same governing
  principle as paper section 2.3, which dices primitives so that
  "micropolygons are approximately half a pixel on a side in screen space,"
  adaptively estimated from the primitive's projected size.
- **Bucket architecture.** `CReyes::CBucket` and the per-bucket
  memory-checkpoint/rollback around `rasterEnd()` are a direct realization of
  paper section 5's bounded-memory compromise: rendering a scene of
  arbitrary complexity in fixed memory by processing the screen in tiles
  rather than holding the whole frame's geometry at once.
- **Grid-vectorized shading.** The bytecode interpreter's masked per-vertex
  `DEFOPCODE` loop (`src/libshader/shading/execute.cpp`) shades an entire
  micropolygon grid's worth of vertices per opcode dispatch — a direct
  realization of the paper's design principle 2, "vectorization" (amortize
  interpreter/shader-call overhead by operating on arrays of points, not one
  point at a time).
- **Jittered point sampling + z-buffer visibility.** `CSampler`
  (`src/ri/sampler.h`) generates jittered subpixel sample positions per
  `CStochastic::rasterBegin`, and each sample keeps a depth-sorted
  `CFragment` chain — the same stochastic-sampling-plus-z-buffer visibility
  model as paper section 2.2.
- **Motion blur / DOF as extra sampling dimensions.** Both are implemented as
  jittered strata over time and lens position respectively (time stratum in
  `CSampler`, `CStochastic::apertureGenerator` for lens jitter) rather than
  as separate rendering passes — matching the paper section 4 extensions,
  which frame both effects as "just" additional stochastic sampling
  dimensions on top of the same point-sampling machinery.

## Where the codebase improves on the paper

- **SLERP-based camera-rotation motion blur.** The paper's motion model is
  implicitly linear (interpolate sample positions between shutter-open and
  shutter-close). A pure linear (LERP) interpolation of a rotating camera's
  transform traces a chord across the rotation arc, not the arc itself —
  measurably wrong for a 90° turn (~29% shorter path). openRender detects
  camera rotation (`CRenderer::cameraHasRotation`) and uses quaternion SLERP
  (`slerpq()` in `common/mathSpec.h`) instead.
- **QMC lens sampling for DOF.** `CStochastic::apertureGenerator` uses a
  2D Sobol sequence (`CSobol<2>`) for lens-position sampling rather than
  plain jittering, giving lower-discrepancy (less noisy) DOF convergence than
  the paper's baseline jitter scheme.
- **Adaptive `ShadingRate` coarsening under motion/DOF.**
  `src/ri/surface.cpp:338-369` widens the effective shading rate when a
  primitive has significant motion or defocus blur, since fine shading detail
  is wasted once it's about to be blurred away — an optimization the paper's
  1987 hardware/scene-complexity budget never afforded.
- **Per-time-stratum motion bounds.** A measured optimization (documented in
  [HIDER_PARITY.md](HIDER_PARITY.md)) that bounds motion-blurred primitives
  per time stratum instead of over the whole shutter interval, cutting a
  heavy rotation-blur scene from 235.6s to 18.2s, with a further
  pure-rotation fast path bringing it to 8.1s. Well beyond anything the paper
  describes.
- **A-buffer-style depth-sorted fragment compositing.** `CCompositor`
  (`src/ri/compositor.cpp`) resolves transparency order-independently via a
  per-sample depth-sorted fragment list, more sophisticated than the paper's
  simpler compositing sketch.
- **Hierarchical occlusion culling and multi-threaded bucket dispatch.**
  `COcclusionCuller` and `CReyes`'s striped multi-threaded bucket dispatch
  have no counterpart in the paper's single-threaded 1987 design.
- **Ray tracing callable from REYES grid-shading.** RSL's `trace()`,
  `gather()`, and `occlusion()` builtins (`src/ri/giFunctions.h`) reach
  `CShadingContext::traceTransmission()`/`traceReflection()`
  (`executeMisc.cpp`) directly from `CReyes::shadeGrid()`. This is dual-natured:
  it's a genuine capability improvement — reflections, refraction, and
  ambient occlusion the paper's authors didn't have — but it is also the
  single biggest **philosophical** divergence from the paper. The paper's
  Introduction explicitly lists "minimal ray tracing" as a design goal,
  precisely because ray tracing breaks the geometric-locality guarantee the
  whole REYES architecture (bucket-local dicing, bounded-memory tiling) is
  built around: a ray can hit geometry anywhere in the scene, not just inside
  the current bucket. openRender embraces the capability at the cost of the
  guarantee the original architects deliberately declined to give up — worth
  reading as an improvement in what users can do, not a validation of the
  paper's own tradeoff.

## Where the codebase diverges from the paper's specific mechanics

These are neutral implementation differences, not regressions.

- **Deferred (shade-on-first-visibility) shading.** The paper's pipeline
  shades a grid immediately after dicing, accepting that many shaded points
  will turn out to be hidden and their shading work wasted (explicitly
  acknowledged in section 2.3). openRender instead defers shading behind the
  first visibility test — grids carry a `RASTER_UNSHADED` flag and are only
  shaded once a sample's `drawPixelCheck()`/`recordPixel()` path first needs
  color from them. The nuance worth preserving: when triggered, the *whole
  grid* is still shaded at once, so grid-wide vectorization (the paper's
  principle 2) is preserved — this is a hybrid of "shade eagerly" and
  "shade lazily," not a naive per-micropolygon lazy shade.
- **Two-sample motion interpolation instead of re-dicing per time sample.**
  Motion is captured by dicing at two time samples and interpolating vertex
  positions at raster time, rather than re-dicing the primitive at every
  sampled time value. A reasonable optimization, but a deliberate departure
  from a literal reading of the paper's per-sample-time dicing model.
- **Raytrace-path tessellation ignores `ShadingRate` entirely.**
  `CTesselationPatch` (the tessellation cache backing `CRaytracer`) sizes
  itself from ray-differential screen footprint, not `ShadingRate`. The
  paper never had to reconcile two competing dicing philosophies because it
  only had one pipeline; openRender's raytrace/reyes split introduces an
  inconsistency the paper's single-pipeline model has no analog for. (Also
  listed under Limitations below, since it's called out as a known parity
  gap in the codebase's own docs.)

## Limitations — design, implementation, or apparent neglect

- **Disabled adaptive re-dicing.** `src/ri/surface.cpp:227` guards a block
  (`#if 1 / #else`) of dead code that would compare numerical vs. analytical
  derivatives and double probe density when they disagree by more than 50% —
  a paper-style refinement loop that exists in source but is compiled out.
  Reads as unfinished work left mid-flight, not an intentional design
  decision.
- **Forward-difference derivative estimate in that same disabled block.**
  `src/ri/surface.cpp:236` carries a FIXME noting the derivative estimate
  uses a forward difference rather than a centered one — an accuracy
  compromise stacked on top of code that's already dead.
- **Cull-flag combination bug, acknowledged in-source.**
  `src/ri/surface.cpp:295`: `// FIXME: implies if either end is culled we
  cull - wrong`.
- **Unresolved grid-dispatch timing tradeoff.** `src/ri/surface.cpp:403` has
  a FIXME plus companion comment weighing early dispatch of a grid close to
  the current bucket (avoids retaining costly grid memory) against the risk
  of doing shading work that a later cull would have avoided — acknowledged,
  not resolved.
- **No patch-crack prevention (confirmed).** Paper section 3, walking through
  a sphere's patch-dice routine, states it "creates a rectangular grid of
  micropolygons so that the vertices differ in u and v by integer multiples
  of powers of 1/2. This is done to obviate CAT filtering, but in this case
  it is also necessary for the prevention of patch cracks." — i.e. the
  power-of-2 snap is dual-purpose: texture-filtering optimization *and*
  crack prevention. `CObject::estimateDicing()` (`src/ri/object.cpp`, the
  `udivf = uMax / shadingRate` / `vdivf = vMax / shadingRate` computation,
  clamped to `[1, 10000]`) is a plain float division with no power-of-2
  rounding anywhere in the function — confirmed directly by reading the
  code, not inferred. openRender does not implement the paper's
  crack-prevention technique; `DEVNOTES.md` corroborates this indirectly by
  describing crack mitigation as currently handled via displacement bounds —
  a workaround standing in for the paper's more principled fix.
- **Silently unfiltered non-composited AOV channels.**
  `src/ri/stochastic.cpp:821-823` is an empty function body behind a
  `numExtraNonCompChannels > 0` guard — a real, currently-incomplete
  pixel-filter gap, distinct from the intentionally-permanent D3/D4/D9
  residuals documented in [HIDER_PARITY.md](HIDER_PARITY.md).
- **Depth-filter threading race (known, shipped anyway).** A multi-threaded
  race in `CStochastic::rasterEnd`'s avg/mid depth-filter bucket-boundary
  accumulation, found during spec 008 T041 verification and explicitly
  tracked as an out-of-scope residual rather than fixed. Worth surfacing
  prominently here since it's a real correctness gap, not a hypothetical
  one.
- **Acknowledged-wrong alpha compositing.** `src/ri/stochastic.cpp:665`
  computes alpha as `(O[0]+O[1]+O[2])/3` with an in-source comment reading
  "I know this is wrong but this is more useful" — a documented, deliberate,
  and still-unresolved correctness shortcut.
- **Cross-hider `ShadingRate` inconsistency.** As noted above, `CRaytracer`
  ignores `ShadingRate` entirely, sizing its tessellation from ray
  differentials instead. [HIDER_PARITY.md](HIDER_PARITY.md) itself names
  this a "latent parity gap" and defers it — a case of the maintainers'
  own docs naming the limitation and choosing not to fix it, bordering on
  needing the "Option C" hybrid hider discussed in
  [PATH-TRACING_HIDER.md](PATH-TRACING_HIDER.md) rather than a
  straightforward patch.
- **Tessellation cache eviction — verified, more nuanced than DEVNOTES
  suggests.** `DEVNOTES.md` lists an open issue, "Purging tessellations for
  raytracing (no cache eviction mechanism found)." Reading
  `CTesselationPatch::purgeTesselations()` (`src/ri/surface.cpp`) directly
  shows this is only half accurate: the mechanism exists and is wired in —
  `intersect()` calls it whenever
  `tesselationUsedMemory[level][thread] > tesselationMaxMemory[level]`, it
  LRU-sorts active per-thread tessellation payloads by `lastRefNumber`
  (`tesselationQuickSort()`) and frees the oldest half. But it only frees
  the `threadTesselation[thread]` payload inside each tessellation-cache
  entry — it never removes the owning `CTesselationPatch` node from the
  global `tesselationList` linked list it walks to find eviction
  candidates. Raw tessellation *memory* is bounded and actively evicted;
  the linked-list bookkeeping of "every patch ever tessellated" grows
  unboundedly for the render's lifetime. That's almost certainly what the
  DEVNOTES issue is actually pointing at, just phrased as a blanket
  absence rather than this specific residual.
- **No literal disk-paging tier for buckets.** Paper section 5 notes buckets
  "may be kept in memory or on disk." openRender relies on the bucket
  memory checkpoint/rollback plus ordinary OS virtual memory rather than an
  explicit bucket-overflow-to-disk path. A minor gap, and likely a
  non-issue at modern RAM sizes — noted for completeness, not as a real
  finding.

## Summary table

| Paper mechanic | openRender status | Reference |
|---|---|---|
| Bound/split/dice recursion (Fig. 3) | Faithful | `src/ri/surface.cpp` (`CPatch::dice`, `splitToChildren`) |
| `ShadingRate`-driven grid resolution | Faithful | `src/ri/object.cpp:348` (`CObject::estimateDicing`) |
| Bounded-memory bucket tiling | Faithful | `src/ri/reyes.h`/`reyes.cpp` (`CReyes::CBucket`) |
| Vectorized (grid-wide) shading | Faithful | `src/libshader/shading/execute.cpp` (`DEFOPCODE` loop) |
| Jittered point sampling + z-buffer visibility | Faithful | `src/ri/sampler.h`, `src/ri/stochastic.cpp` |
| Motion blur / DOF as sampling dimensions | Faithful | `CSampler`, `CStochastic::apertureGenerator` |
| Linear motion interpolation | Improved (SLERP for rotation) | `common/mathSpec.h` (`slerpq`), gated by `CRenderer::cameraHasRotation` |
| DOF lens sampling | Improved (QMC/Sobol vs. plain jitter) | `src/ri/stochastic.cpp` (`apertureGenerator`, `CSobol<2>`) |
| Fixed shading rate under blur | Improved (adaptive coarsening) | `src/ri/surface.cpp:338-369` |
| Motion bounding over full shutter | Improved (per-stratum bounds) | see [HIDER_PARITY.md](HIDER_PARITY.md) |
| Simple compositing | Improved (A-buffer depth-sorted) | `src/ri/compositor.cpp` (`CCompositor`) |
| Single-threaded, no ray tracing | Improved (multi-threaded, occlusion culling) | `COcclusionCuller`, `CReyes` dispatch |
| "Minimal ray tracing" design goal | Diverged (philosophically) | `src/ri/giFunctions.h`, `executeMisc.cpp` |
| Shade immediately after dicing | Diverged (deferred, grid-wide when triggered) | `RASTER_UNSHADED`, `drawPixelCheck()`/`recordPixel()` |
| Re-dice per time sample | Diverged (2-sample interpolation) | `src/ri/surface.cpp` |
| One dicing philosophy | Diverged (raytrace ignores `ShadingRate`) | `CTesselationPatch`, `src/ri/surface.cpp` |
| Power-of-2 vertex snapping (crack prevention) | Limited (not implemented) | `src/ri/object.cpp` (`estimateDicing`) |
| Adaptive re-dicing on derivative error | Limited (implemented but disabled) | `src/ri/surface.cpp:227,236` |
| Cull-flag combination | Limited (acknowledged bug) | `src/ri/surface.cpp:295` |
| Non-comp AOV channel filtering | Limited (unimplemented, empty body) | `src/ri/stochastic.cpp:821-823` |
| Depth-filter thread safety | Limited (known race, unfixed) | `CStochastic::rasterEnd` (avg/mid depth filter) |
| Alpha compositing correctness | Limited (acknowledged-wrong shortcut) | `src/ri/stochastic.cpp:665` |
| Tessellation cache eviction | Limited (memory bounded, list nodes leak) | `src/ri/surface.cpp` (`purgeTesselations`) |
| Bucket disk-paging | Limited (not implemented, low priority) | n/a |
