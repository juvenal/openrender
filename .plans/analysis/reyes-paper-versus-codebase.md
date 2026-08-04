# REYES Paper vs. openRender Codebase — Comparative Analysis Doc

## Context

The user wants a deep comparison between the original 1987 REYES paper
("The Reyes Image Rendering Architecture," Cook/Carpenter/Catmull,
`docs/references/papers/37402.37414.pdf`) and openRender's actual REYES
hider implementation: where the code diverges from the paper, where it
improves on it, and where design/implementation/neglect limitations leave
capability on the table relative to either the paper's ideal or the
codebase's own stated intentions (FIXMEs, disabled code paths, documented
residuals).

This is a research/writing deliverable, not a code change. Research is
already complete — the paper was read in full (all 8 pages, including
Figure 3's algorithm pseudocode and Figure 1's pipeline diagram), and three
parallel Explore agents mapped: (1) the bound/split/dice pipeline, (2) the
shading/sampling/filtering pipeline, and (3) the hider-parity documentation,
spec 008 convergence work, git history, and FIXME/TODO trail. The user
chose to receive the result as a new deep-dive doc under
`DEVNOTES_DETAILS/`, matching the existing convention (`HIDER_PARITY.md`,
`OSHADER_UPDATES.md`, etc.), linked from `DEVNOTES.md`.

## Deliverable

Create `DEVNOTES_DETAILS/REYES_PAPER_COMPARISON.md` and add one line for it
under `DEVNOTES.md`'s existing `DEVNOTES_DETAILS/*.md` guide list (matching
the format of the other entries there). No other files change — this is a
pure documentation addition.

## Document outline

**1. Introduction** — one paragraph: what's being compared and why (map
CLAUDE.md's `CReyes`/`CStochastic`/`CZbuffer` hierarchy against the paper's
single monolithic algorithm; note the RIB `Hider "reyes"` string actually
selects `CStochastic`, `CZbuffer` is a separate deterministic fast path that
shares the same `CReyes` bound/split/dice base).

**2. Where the implementation is faithful to the paper** (validates the
architecture is a real REYES, not REYES-in-name-only):
- Recursive bound→split→dice loop (`CPatch::dice`/`splitToChildren` in
  `src/ri/surface.cpp`) mirrors Figure 3's algorithm almost exactly,
  including the eye-plane split-until-cullable handling.
- `ShadingRate`-driven dicing (`CObject::estimateDicing`,
  `src/ri/object.cpp:348`) — projected screen-space edge length ÷
  `ShadingRate`, same principle as paper section 2.3.
- Bucket architecture (`CReyes::CBucket`, `src/ri/reyes.h`/`reyes.cpp`) —
  screen-tiled, per-bucket memory checkpoint/rollback — is a direct
  realization of paper section 5's bounded-memory implementation
  compromise.
- Grid-vectorized shading (interpreter's `DEFOPCODE` masked per-vertex loop
  in `src/libshader/shading/execute.cpp`) directly realizes paper principle
  2 ("vectorization").
- `CStochastic`'s jittered point sampling + z-buffer visibility
  (`CSampler`, `src/ri/sampler.h`; per-sample `CFragment` chain,
  `src/ri/stochastic.cpp`) is a direct implementation of paper section 2.2.
- Motion blur / DOF as extra stochastic sampling dimensions (jittered time
  stratum, jittered lens position) matches paper section 4's extensions.

**3. Where the codebase improves on the paper** (modern refinements the
1987 design didn't have or couldn't afford):
- SLERP-based camera-rotation motion blur (vs. paper's implicit linear
  motion-sample model) — traces the correct arc, not a chord.
- QMC (Sobol-sequence) lens sampling for DOF vs. plain jittering.
- Adaptive `ShadingRate` coarsening under heavy motion blur/DOF
  (`src/ri/surface.cpp:338-369`) — not in the paper, a real optimization.
- Per-time-stratum motion bounds (235.6s → 18.2s measured, per
  `HIDER_PARITY.md`) and further pure-rotation fast path (→8.1s).
- A-buffer-style depth-sorted fragment compositing (`CCompositor`) for true
  order-independent transparency, beyond the paper's simpler sketch.
- Hierarchical occlusion culling (`COcclusionCuller`), multi-threaded
  striped bucket dispatch (`dispatchReyes`) — both absent from the
  single-threaded 1987 paper.
- Ray tracing callable from inside REYES grid-shading (`trace()`,
  `gather()`, `occlusion()` RSL builtins reaching
  `CShadingContext::traceTransmission/traceReflection` from
  `CReyes::shadeGrid`) — note this is framed as an *improvement in
  capability* but is really the biggest **philosophical** divergence from
  the paper's explicit "minimal ray tracing" design goal (paper
  Introduction, bullet 4) — call this out clearly as dual-natured: gives
  users a capability the original architects deliberately avoided
  architecting for, at the cost of the geometric-locality guarantees the
  paper leaned on.

**4. Where the codebase diverges from the paper's specific mechanics**
(neutral differences, not regressions):
- Shading is deferred behind the first-visibility test rather than run
  immediately after dicing (`RASTER_UNSHADED` flag,
  `drawPixelCheck()`/`recordPixel()` shade-on-demand pattern) — inverts the
  paper's dice→shade→sample ordering to avoid the "many shading
  calculations never used" cost the paper explicitly accepts (section 2.3).
  Note the nuance: it still shades the *whole grid* at once when triggered,
  so grid-wide vectorization is preserved — this is a hybrid, not a naive
  per-micropolygon lazy shade.
- Motion is captured via two dice-time position samples interpolated at
  raster time, not by re-dicing per time sample — reasonable optimization,
  worth naming as a deliberate divergence from a literal reading of the
  paper's model.
- Raytrace-path tessellation (`CTesselationPatch`) uses ray-differential
  screen footprint for resolution, entirely ignoring `ShadingRate` — a
  hider-vs-hider inconsistency, not present in the paper's single-pipeline
  model (paper never had two competing dicing philosophies to reconcile).

**5. Limitations — design, implementation, or apparent neglect** (the
"held itself back" section the user specifically asked for):
- **Disabled adaptive re-dicing**: `src/ri/surface.cpp:227` has a
  `#if 1 / #else` guarding dead code that compares numerical vs. analytical
  derivatives and doubles probe density on >50% relative error — a
  paper-style refinement loop that exists in source but is short-circuited
  off. Worth flagging as unfinished work, not intentional design.
- Forward-difference (not centered) derivative estimate in that same
  disabled block (`surface.cpp:236` FIXME) — an accuracy compromise on top
  of already-disabled code.
- Cull-flag combination bug noted in-source: `surface.cpp:295`
  `// FIXME: implies if either end is culled we cull - wrong`.
- Acknowledged, unresolved grid-dispatch timing tradeoff
  (`surface.cpp:403`): early dispatch avoids retaining costly grids but
  risks work that would otherwise be culled.
- **Patch-crack prevention (verified)**: the paper's own dicing scheme
  snaps grid vertices to power-of-2 u/v boundaries specifically to prevent
  cracks at patch seams (paper section 2.3, "vertices differ in u and v by
  integer multiples of powers of 1/2 ... necessary for the prevention of
  patch cracks"). Confirmed directly in `CObject::estimateDicing`
  (`src/ri/object.cpp:465-479`): `udivf = uMax / shadingRate` /
  `vdivf = vMax / shadingRate`, clamped to `[1, 10000]` — a plain float
  division with no power-of-2 rounding anywhere in the function. openRender
  does not implement the paper's crack-prevention technique; `DEVNOTES.md`
  confirms this indirectly by describing crack stitching as "currently
  handled via displacement bounds" — a workaround standing in for the
  paper's more principled fix. State this as a confirmed finding, not a
  hedge.
- **Silently unfiltered non-composited AOV channels**:
  `src/ri/stochastic.cpp:821-823` — an empty function body behind a
  `numExtraNonCompChannels > 0` guard; a real, currently-incomplete
  pixel-filter gap, distinct from the intentionally-permanent D3/D4/D9
  residuals.
- **Depth-filter threading race**: a known, deliberately-unfixed
  multi-threaded race in `CStochastic::rasterEnd`'s avg/mid depth-filter
  bucket-boundary accumulation (found during spec 008 T041 verification,
  explicitly tracked as out-of-scope residual, not a silent unknown bug —
  but still a real correctness gap worth surfacing prominently since it's
  a case of "known and shipped anyway").
- **Acknowledged-wrong alpha compositing**: `stochastic.cpp:665`,
  `(O[0]+O[1]+O[2])/3` with an in-source comment "I know this is wrong but
  this is more useful" — a documented but unresolved correctness
  shortcut.
- **Cross-hider `ShadingRate` inconsistency** (also listed in §4, but
  belongs here too as a limitation): raytrace ignoring `ShadingRate`
  entirely is called a "latent parity gap" in `HIDER_PARITY.md` itself and
  explicitly deferred — a case of the maintainers' own docs naming the
  limitation and choosing not to fix it (bordering on "Option C" hybrid
  hider territory).
- **Tessellation cache eviction (verified, nuanced)**: `DEVNOTES.md`'s open
  issue "Purging tessellations for raytracing (no cache eviction mechanism
  found)" is only half accurate. `CTesselationPatch::purgeTesselations()`
  (`src/ri/surface.cpp:2062-2109`) is real and wired in — triggered from
  `intersect()` whenever `tesselationUsedMemory[level][thread] >
  tesselationMaxMemory[level]` (`surface.cpp:727-730`), it LRU-sorts active
  per-thread tessellation payloads via `lastRefNumber`
  (`tesselationQuickSort`) and frees the oldest half. But it only frees the
  `threadTesselation[thread]` payload inside each `CTesselationEntry` — it
  never removes the owning `CTesselationPatch` node from the global
  `tesselationList` linked list it walks to find candidates. So raw
  tessellation *memory* is bounded and actively evicted, but the
  linked-list bookkeeping of *which patches have ever been tessellated*
  grows unboundedly for the life of the render — almost certainly what the
  DEVNOTES issue is actually pointing at, just phrased imprecisely. State
  this distinction plainly in the doc rather than picking one side.
- No literal bucket-level disk-paging tier (paper section 5 mentions
  buckets "may be kept in memory or on disk"); openRender relies on the
  bucket memory checkpoint/rollback plus OS virtual memory rather than an
  explicit overflow-to-disk path — a minor, likely-non-issue-at-modern-RAM-
  sizes gap worth one sentence, not a major finding.

**6. Summary table** — a compact table (paper mechanic | openRender
status: faithful / improved / diverged / limited | file:line) for quick
scanning, since this doc will likely be a reference future contributors
skim rather than read end-to-end.

## Verification

This is a documentation task — "testing" means: re-check the handful of
specific claims flagged above as needing verification before asserting them
as fact (the patch-crack power-of-2 snapping question, and the
tessellation-purge doc-vs-code tension) by reading the exact code in
`src/ri/surface.cpp` (`CPatch::dice`, `estimateDicing`) rather than relying
solely on the agent summaries, since those are the two claims in the report
with the least direct evidence. Also do a final read of the produced
`REYES_PAPER_COMPARISON.md` against the actual PDF page images already
captured in this conversation to make sure page/section references (e.g.
"paper section 2.3," "Figure 3") are accurate before treating the doc as
finished.
