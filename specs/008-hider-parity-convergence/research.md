# Phase 0 Research: Reyes/Raytrace Hider Parity Convergence

No `NEEDS CLARIFICATION` markers remain in the Technical Context (both
clarification sessions already resolved the three scope-level ambiguities;
see spec.md's Clarifications section). This document instead resolves the
open **design** questions each refactor/share item needs before Phase 1
data-model/contracts can be written precisely, grounded in direct source
inspection (`codegraph_explore` over `src/ri/`, `src/libshader/shading/`).

## R1 — Hider contract split (`drawObject`/`drawGrid`/`drawPoints`)

**Decision**: `CObject::dice(CShadingContext *rasterizer)` — declared in
`object.h`, defined in `object.cpp:80` — must change its parameter type to
`CReyes *rasterizer`, because its body calls `rasterizer->drawObject(cObject)`
(`object.cpp:87`). This is the one non-mechanical consequence of R1: every
`CSurface` subclass that overrides `dice()` (patches, polygons, quadrics,
points, NURBS meshes, implicit surfaces, dynamic-load objects — the same
~27-type set that overrides `intersect()`, confirmed via blast-radius query)
must update its override's parameter type to match. `drawGrid`/`drawPoints`
themselves need no such ripple: both are called only from inside
`reyes.cpp` (`shadeGrid`, `drawGrid`, `drawPoints` bodies), never from a
generic `CShadingContext*` context.

**Rationale**: The call flow is `CReyes::drawObject` (reyes.cpp:567,
`object->dice(this)`, where `this` is a `CReyes*`) → `CObject::dice`
(object.cpp:80) → `rasterizer->drawObject(cObject)` (recursive re-entry for
split/refined children). `dice()` is only ever invoked with a `CReyes*`
underneath (bucket-rasterization tessellation), never with a bare
`CShadingContext*` — the raytrace/photon hiders never call `dice()`; they
call `intersect()` instead (`patches.cpp`'s `CNURBSPatchMesh::intersect`
lazily calls `create()` then tests children directly, with no `dice()`/
`drawObject` involvement). So narrowing `dice()`'s parameter to `CReyes*` is
zero-risk: it only formalizes an invariant that already holds in practice.
`intersect()` stays on `CShadingContext*` since it is genuinely hider-agnostic
(any hider, including a future path-tracing one, casts rays).

`CStochastic` and `CZbuffer` already inherit `CReyes`
(`stochastic.cpp:59`/`zbuffer.cpp:46`, confirmed via constructor initializer
lists), so both automatically retain `drawObject`/`drawGrid`/`drawPoints`
with zero additional code once the methods move down from
`CShadingContext`. A future `abuffer` hider (backlogged, zero current
codebase references) gets the same contract for free by subclassing
`CReyes` — R1 must implement the split once at the `CReyes` level, not
per-subclass, so this genuinely generalizes.

`CRaytracer` (`raytracer.h:76-94`) and `CPhotonHider` (`photon.h:41-59`) both
currently carry identical no-op stub overrides
(`void drawObject(CObject *) {}`, etc.) and both inherit `CShadingContext`
directly. FR-023 names only the raytrace hider, but the mechanical
consequence of moving the base declaration off `CShadingContext` touches
both — `CPhotonHider`'s stubs stop overriding anything and must be deleted
in the same change, or the build fails (no virtual to override). This is an
implicit-but-required part of R1, not scope creep.

**Alternatives considered**:
- *Keep `dice()` on `CShadingContext*`, `dynamic_cast` to `CReyes*` inside
  `dice()`'s body.* Rejected: adds a runtime cast and a failure mode
  (`nullptr` dereference if ever called from a non-`CReyes` context) where a
  compile-time parameter-type change gives the same safety for free and
  documents the real invariant.
- *Introduce a narrow `IRasterizable` interface with just `drawObject`,
  implemented by `CReyes` and passed to `dice()`.* Rejected: over-engineering
  for a single-consumer method; `CReyes*` already *is* that narrow interface
  in practice, and the spec's own framing ("mechanical... call sites only
  ever invoke these on reyes-family hiders") supports the simpler direct
  type.

## R2 — Shared `CSampler`

**Decision**: `CSampler` owns per-pixel-sample generation: jitter (x,y),
time stratum, lens/aperture point, and importance weight — the exact field
set already present ad hoc on `CStochastic::CPixel`
(`stochastic.h:76-92`: `jx,jy`, `jt`+`jtStratum`, `jdx,jdy`, `jimp`) plus
`CRaytracer::computeSamples`'s equivalent per-ray jitter/time/lens
generation (`raytracer.cpp:487+`, its own `urand()`-driven paths). `CSampler`
absorbs `sampleDisk()` (`random.h:172`) as its lens-point implementation —
call sites change from each hider owning its own RNG-plus-`sampleDisk()`
pairing to both hiders asking one `CSampler` instance for a fully-formed
sample. The single canonical jitter constant replaces the two
independently-drifted ones (`0.5001011` in one hider's code vs. `0.5` in the
other's — D2 in the audit).

**Rationale**: `CPixel`'s field list is already, in effect, the target
`CSampler` output struct — R2 is primarily a "move + rename + single
canonical formula" change, not new design. `CStochastic::apertureGenerator`
(a `CSobol<2>` member, `stochastic.h:108`) becomes the RNG source `CSampler`
wraps for reyes; `CRaytracer`'s own `urand()` becomes the RNG source
`CSampler` wraps for raytrace — R2 does not force both hiders onto one RNG
*type* (that convergence is Option B's job, not R2's), only onto one
*formula* per sample field.

**Alternatives considered**:
- *Templatize `CSampler` on the RNG type at the call site (as `sampleDisk()`
  already is).* Adopted in part — `CSampler` keeps `sampleDisk()`'s existing
  template-on-RNG design for the lens-point method specifically, since that
  is the already-validated, already-fixed asset this spec must not redo.
- *Fold `CSampler` construction into `CShadingContext`'s constructor.*
  Rejected: `CShadingContext` is being narrowed (R1) to shading+tracing only;
  adding a sampling-only member there would reintroduce the same
  "unrelated concern on the shared base" problem R1 removes elsewhere.
  `CSampler` is owned by each hider (`CStochastic`/`CRaytracer`) as a member,
  not by the shared base.

## R3 — Shared transparency/matte compositor

**Decision**: Extract the "combine one transparent/matte sample into a
running front-to-back result" step from `CStochastic::rasterEnd`
(stochastic.cpp:445-714 — the `NonCompositeSampleLoop()`,
`checkZThreshold()`/`checkMatteZThreshold()`, and `compositeSampleLoop()`
macro-driven logic) into a shared compositor consumed by both
`CStochastic::rasterEnd`'s fragment-list walk and the raytracer's
continuation-ray path. The compositor operates on a **per-sample value
struct** (color, opacity, matte flag, extra-channel values) that both call
sites populate from their own native storage — reyes populates it from a
walked `CFragment` node, raytrace populates it from its own per-ray-hit
state — so the compositor itself never touches `CFragment` and cannot alter
its layout (satisfying FR-010's hard constraint that deep-shadow's direct
fragment-list reads, `stochastic.cpp:1302-1415`, must be unaffected).

**Rationale**: `CFragment` (`stochastic.h:61-70`) is a linked-list node with
`color`, `opacity`, `accumulatedOpacity`, `z`, `next`/`prev`, `extraSamples`.
The compositor's job — front-to-back "over" with `opacityThreshold`,
matte-as-negative-opacity carve-out, and `compChannelOrder`/
`nonCompChannelOrder` AOV rules — is expressible purely in terms of the
per-sample values already present in a `CFragment`, without needing the
list-node pointers themselves. This is why an adapter-struct boundary (not a
shared base class, not a template over "anything list-like") is the right
cut: it lets raytrace's currently-different internal representation
(`CPrimaryBundle`, `raytracer.cpp:39-62` — `maxPrimaryRays`, `allSamples`,
`rayBase[i].samples`, `sampleOrder`, `sampleDefaults`) feed the same
compositor without being restructured to look like a fragment list.

**Open item carried to task-decomposition**: `CPrimaryBundle`'s actual
continuation-ray compositing body (its equivalent of `rasterEnd`'s
walk-and-composite loop) was not located at the source-line level during
this research pass — `codegraph_explore` surfaced `CGatherBundle::postShade`/
`postTraceAction` (`bundles.cpp:274-317`, a *different* class used for
photon/gather rays) instead of `CPrimaryBundle`'s own method. The adapter-struct
design above does not depend on that method's exact current shape — it only
depends on `CPrimaryBundle` already having *some* per-hit color/opacity/AOV
state to adapt from, which its constructor (raytracer.cpp:39-62) confirms.
Locating and reading `CPrimaryBundle`'s actual per-hit compositing loop is
deferred to the R3 implementation task, where it must be read in full before
the adapter is written.

**Alternatives considered**:
- *Have the compositor walk `CFragment` directly and add a second walk
  method for raytrace's structure.* Rejected: this is exactly today's
  duplication (D6/D8), just moved into one file — it doesn't produce "one
  shared component" per FR-009/SC-004, only one shared *file* with two
  implementations in it.
- *Restructure raytrace's per-hit storage to mimic `CFragment`'s linked
  list.* Rejected: unnecessary churn on a structure with its own
  constraints (`CPrimaryBundle`'s `maxPrimaryRays`-bounded array design
  suits ray continuation, not list traversal), and FR-010 only constrains
  the reyes side's structure — there's no requirement to unify the
  *storage*, only the *compositing logic*.

## R4 — Shared pixel-filter module

**Decision**: One module wrapping the already-shared
`CRenderer::pixelFilterKernel` (precomputed once in `beginFrame`, per
`HIDER_PARITY.md`'s own "Unified Pixel Filtering" checkbox — already `[x]`)
with the splat/gather accumulation and weight-normalization step each hider
currently repeats around that kernel. Since the kernel itself is already
unified, R4's scope is narrowly the accumulate-and-normalize loop, consumed
by `CStochastic`, `CRaytracer`, and `CZbuffer` (whose `rasterEnd`,
zbuffer.cpp:160-198, does its own simple opaque filter/gather today).

**Rationale**: Lowest-risk of the four refactors (spec's own Story 7
priority P4, "kernel itself is already shared") — confirmed by
`HIDER_PARITY.md` marking pixel filtering `[x]` already. This is a pure
structure-only move (FR-021: "MUST NOT change any hider's rendered output").

**Alternatives considered**: None substantive — the design is dictated by
what's already shared vs. duplicated; no reasonable alternative changes the
scope.

## S1 — Canonical lens/CoC model

**Decision**: Lives inside `CSampler` (R2) per spec.md's own framing. Reyes's
`cocSamples()` (per-vertex circle-of-confusion, called from
`CReyes::copyPoints`, reyes.cpp:1067) and raytrace's per-ray lens-point
computation both derive from one formula set owned by `CSampler`, built on
`sampleDisk()`'s already-fixed area-uniform disk logic. No new research
needed beyond R2's design above — S1 is R2's lens/CoC facet, not a separate
component.

## S2 — Displacement parity by default

**Decision**: Flip the raytrace-side default at the exact gating condition
found in `shading.cpp:676-683` — `(usedParameters & PARAMETER_RAYTRACE) &&
!(currentAttributes->flags & ATTRIBUTES_FLAGS_DISPLACEMENTS)` (the
`IGNORE_DISPLACEMENTS_FOR_DICING`-gated skip). Per the resolved
clarification, the new default is "displace unless explicitly opted out" —
so this condition's sense flips: displacement is skipped only when the
existing `Attribute "trace" "displacements"` mechanism explicitly disables
it, not by default. FR-016's opt-out is the existing attribute mechanism,
unchanged in shape — only the polarity of the *default* changes.

**Alternatives considered**: *Add a new RIB-level default-override
attribute.* Rejected: FR-029 forbids introducing new user-facing RIB tokens;
the existing opt-out attribute already covers the escape hatch this needs.

## S3 — Depth-filter modes + z-visibility threshold

**Decision**: Extract reyes's `DEPTH_MID` branch and sibling min/max/avg
logic from inside `CStochastic::rasterEnd` (stochastic.cpp:609-640 for the
`DEPTH_MID` second-sample search; the surrounding filter-mode dispatch in
the same function) into a shared depth-filter function taking a list of
candidate `(z, opacity)` pairs and a mode enum, called by both
`CStochastic::rasterEnd` and a new raytrace-side call site that currently
has none (raytrace's `z` today is always first-hit distance). The same
`zvisibilityThreshold` exclusion (`checkZThreshold`: `opacity[i] >
zvisibilityThreshold[i]`) is applied identically on both sides.

**Rationale**: This is a pure port — reyes's logic is already correct and
tested; the gap is raytrace having no equivalent path at all. No new
algorithm design needed, only relocating reyes's existing logic to a shared
call site and wiring raytrace's per-hit-distance collection into it.

## S4 — Transparent-hit AOV compositing in raytrace

**Decision**: Once R3's compositor exists, wire the raytracer's
continuation-ray loop to route extra AOV channels through it using the
existing `compChannelOrder`/`nonCompChannelOrder` tables (already present,
`raytracer.cpp:221`, and already used by reyes's `copySamples`,
reyes.cpp:1078-1146, `CRenderer::sampleOrder`-driven channel copy). S4 is
not a separate design — it is R3's compositor applied to the raytrace side's
first-hit-only gap; no new tables or formats are introduced (FR-011,
FR-029).

## S5 — Raytraced motion-blur verification

**Decision**: No new motion-blur infrastructure (per the resolved
clarification — verification/bug-fix only). The existing lazy-split pattern
in `patches.cpp` (`CNURBSPatchMesh::intersect`, line 1797, and `dice`, line
1809, both lazily call the same `create()` at line 1822) is the precedent
that intersection kernels already interpolate on `ray->time` consistently
across the tessellation boundary — S5's work is to add the per-primitive-type
(patches/polygons/quadrics) × (translate/deform) parity scenes (User Story
6/FR-018) and fix whatever the harness surfaces, not to design new
interpolation machinery.

## Test infrastructure: "use existing tests as a basis"

**Decision**: Confirmed via reading `tests/visual/test_visual_render.cpp` and
`tests/visual/test_radial_histogram.cpp` in full:
- `test_visual_render.cpp` runs `orender` **once** against a RIB and diffs the
  single fresh output against a **static reference TIF**, using a block-average
  (8×8 blocks, `BLOCK_SIZE=8`) per-channel max-diff metric
  (`compareTiffs`/`DiffResult`, lines 106-163). This is the wrong shape for
  cross-hider parity (there is no single "correct" reference — both hiders'
  outputs are compared to *each other*).
- `test_radial_histogram.cpp` already demonstrates the needed shape: a
  "two-file mode" that reads two independently-produced TIFs and compares them
  directly against each other (candidate vs. ground-truth, ±20%-per-bin), with
  its own header comment noting it deliberately duplicates
  `test_visual_render.cpp`'s `TiffImage`/reader code rather than sharing a
  header — "same approach as test_visual_render.cpp — no new dependency".
- **Decision**: `test_hider_parity.cpp` follows the same duplication-over-shared-header
  convention (consistent with the codebase's existing precedent, not introducing a
  new library boundary), reusing `test_visual_render.cpp`'s exact `TiffImage`/
  `readTiff`/`compareTiffs` block-average logic verbatim, but with a `main()`
  that takes **two** `(orender_path, rib_path, output_tif_name)` triples — one
  per hider — runs `orender` twice, then diffs the two fresh outputs against
  each other with a per-scene threshold, exactly mirroring
  `test_radial_histogram.cpp`'s two-file mode but for the block-average metric
  instead of the radial-energy metric.
- New CMake macro `add_parity_test(SCENE_NAME RIB_A OUTPUT_A RIB_B OUTPUT_B
  [THRESHOLD])` in `tests/visual/CMakeLists.txt`, modeled line-for-line on the
  existing `add_visual_test` macro (same scratch-dir pattern, same
  `VISUAL_ENV`, same `TIMEOUT 360`), differing only in invoking
  `test_hider_parity` instead of `test_visual_render` and adding a `parity`
  ctest label alongside `visual;regression`.
- `tests/test_disk_sampling.cpp` and `test_radial_histogram.cpp` are reused
  **unmodified** — FR-008 requires their pass/fail intent be unchanged by R2/S1,
  so they are acceptance gates for R2's `CSampler`, not scenes to rewrite.

**Alternatives considered**: *Write a Python/shell diff script instead of a
C++ CLI tool.* Rejected: breaks the "use existing ones as a basis" instruction
and the CLI-first constitution principle's C++/native-tool convention already
established by the two precedent drivers; introduces a new language/runtime
dependency (Principle V) for no benefit over duplicating ~150 lines of
already-working C++.

## Option B — Shared per-bucket sample table

**Decision**: `CSampler` (R2) gains a per-bucket generation mode: instead of
each hider calling `CSampler` per-sample against its own RNG stream, one
`CSampler` pass generates a table of (position, time, lens) values for an
entire bucket once, and both hiders consume that table verbatim for the same
bucket. This is additive on top of R2's per-sample API (same formulas, same
`CSampler` ownership), not a redesign — R2 must be structured so its
per-sample generation logic is reusable by a batch/table-producing entry
point without duplication.

**Rationale**: Per the resolved clarification, this stays strictly internal —
no RIB token, no documented determinism guarantee (FR-027). Its only
consumer is the Story 1 parity harness, to correlate noise and permit
tightening at least one currently-loose threshold (FR-026/SC-008).

**Alternatives considered**: *Persist the sample table to disk for
cross-process reuse.* Rejected: over-engineered for an internal,
single-process, single-render-invocation testing aid; no requirement calls
for cross-process or cross-run reuse.
