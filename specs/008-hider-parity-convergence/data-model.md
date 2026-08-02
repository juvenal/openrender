# Phase 1 Data Model: Reyes/Raytrace Hider Parity Convergence

Entities below are drawn directly from spec.md's Key Entities section,
elaborated with fields/relationships/validation rules grounded in the actual
source structures identified in `research.md` (`CPixel`, `CFragment`,
`CPrimaryBundle`, `CRenderer::pixelFilterKernel`). This is a rendering-engine
feature — entities are in-memory C++ structures and test fixtures, not
persisted database records.

## 1. Parity scene pair

A RIB scene (or a matched pair of RIB scenes, one per hider, differing only
in `Hider` declaration) exercising one documented visual effect, rendered
once per hider and diffed by the Story 1 harness.

| Field | Type | Notes |
|---|---|---|
| `name` | string | Scene identifier, e.g. `parity-dof`, `parity-motion-patches-translate` |
| `rib_reyes` | file path | RIB variant selecting the reyes hider (or a shared RIB + `Hider "hidden"` override) |
| `rib_raytrace` | file path | RIB variant selecting the raytrace hider |
| `effect_tag` | enum | One of: `flat-shading`, `dof`, `motion-translate`, `motion-deform`, `transparency`, `matte`, `aov`, `combined` |
| `primitive_type` | enum (motion scenes only) | `patches` \| `polygons` \| `quadrics` — required when `effect_tag` starts with `motion-`, per FR-002/FR-018 |
| `threshold_ref` | → Per-effect parity threshold | Which threshold this pair is checked against |

**Validation rules**:
- Every `effect_tag` in {`flat-shading`, `dof`, `transparency`, `matte`, `aov`}
  MUST have at least one scene pair (FR-002).
- Every `effect_tag` of `motion-translate`/`motion-deform` MUST exist for
  each of the three `primitive_type` values independently (FR-002, FR-018) —
  six motion pairs minimum (2 motion kinds × 3 primitive types), not one
  generic moving-geometry scene.
- At least one `effect_tag = combined` pair MUST exist (FR-002).

## 2. Per-effect parity threshold

A documented numeric tolerance keyed by `effect_tag`, consumed by the parity
harness's diff step (reusing `test_visual_render.cpp`'s block-average
`compareTiffs` metric, per research.md's Test-infrastructure decision).

| Field | Type | Notes |
|---|---|---|
| `effect_tag` | enum | Same domain as Parity scene pair's `effect_tag` |
| `max_block_diff` | number (0-255 scale) | Threshold passed to the parity driver, same units as the existing `VISUAL_THRESHOLD`/per-scene override convention in `tests/visual/CMakeLists.txt` |
| `residual_ref` | → Documented residual, optional | Set when the threshold is loosened specifically to account for a named, out-of-scope algorithmic difference (SC-003) rather than measurement noise |
| `rationale` | string | One-line justification, mirroring the existing per-scene threshold comments in `HIDER_PARITY.md`'s determinism-caveat section |

**Validation rules**:
- `flat-shading`, `matte`, and depth-filter/z-visibility-tagged pairs use the
  tightest threshold band (SC-002: "at or below threshold on 100% of pairs",
  no residual attached).
- `dof`, `motion-*`, `transparency` thresholds MAY carry a `residual_ref`
  (SC-003) but MUST still be an explicit numeric bound, never "diff
  suppressed" or "check skipped" (FR-028).
- After Option B (per-bucket sample table) lands, at least one threshold in
  this table with no `residual_ref` MUST be tightened from its Option-A value
  without the harness beginning to fail on unmodified code (FR-026/SC-008).

## 3. Shared per-sample generator (`CSampler`)

Owns the fields already present ad hoc on `CStochastic::CPixel`
(`jx,jy`, `jt`/`jtStratum`, `jdx,jdy`, `jimp`) plus the raytracer's
equivalent per-ray jitter/time/lens values, unified into one struct + one
generation method pair.

| Field | Type | Notes |
|---|---|---|
| `jitterX`, `jitterY` | float | Sub-pixel jitter offset; single canonical constant replaces the `0.5001011` vs `0.5` drift (D2) |
| `timeStratum` | float | Motion-blur time sample, stratified |
| `lensPoint` | (float, float) | Produced by `sampleDisk()` (unchanged, per research.md R2) |
| `importance` | float | Sample weight |

**Relationships**: `CSampler` is a member owned by each hider
(`CStochastic`, `CRaytracer`), not by the shared `CShadingContext` base
(research.md R2 rationale — sampling is not a shading/tracing concern).
`CSampler` wraps a caller-supplied RNG source per hider (reyes:
`CSobol<2> apertureGenerator`; raytrace: its own `urand()`); R2 does not force
one shared RNG *type*, only one shared sample-field *formula* set.

**Validation rules**: `sampleDisk()`'s existing signature and template-on-RNG
design are preserved unchanged (research.md R2 alternatives-considered) — it
is called *by* `CSampler`, not replaced.

**State/mode**: `CSampler` has two generation modes, corresponding to Option
A (R2, per-sample-on-demand) and Option B (per-bucket table, pre-generated
once and consumed verbatim by both hiders for that bucket) — see entity 7
below. Option B is additive; it does not remove or change Option A's
per-sample API (research.md, Option B decision).

## 4. Shared transparency/matte compositor

Operates on an adapter struct, not on `CFragment` or `CPrimaryBundle`
directly (research.md R3 decision) — this is the load-bearing design
constraint from FR-010.

**`CompositeSample` (adapter struct, new)**:

| Field | Type | Notes |
|---|---|---|
| `color` | color | Sample color |
| `opacity` | color/float | Sample opacity |
| `isMatte` | bool | Matte carve-out flag |
| `extraChannels` | map/array | AOV channel values, keyed per `compChannelOrder`/`nonCompChannelOrder` |
| `z` | float | Depth, consumed by entity 6 (Depth-filter mode) |

**Relationships**:
- Reyes side: `CStochastic::rasterEnd`'s fragment-list walk populates one
  `CompositeSample` per walked `CFragment` node (`stochastic.h` fields:
  `color`, `opacity`, `accumulatedOpacity`, `z`) and feeds it to the shared
  compositor; `CFragment`'s own layout is untouched (FR-010).
- Raytrace side: the continuation-ray path (`CPrimaryBundle`,
  `raytracer.cpp:39-62`) populates the same `CompositeSample` shape per ray
  hit from its own `allSamples`/`rayBase[i].samples` state and feeds it to
  the same compositor (FR-009, FR-011).
- The compositor's internal running-result state (accumulated color/opacity)
  is owned by the compositor call, not by either hider's native structure.

**Validation rules**: The compositor is the single place `opacityThreshold`,
matte-as-negative-opacity, and `compChannelOrder`/`nonCompChannelOrder`
rules are evaluated (SC-004: "exactly one shared implementation,
independently verifiable by inspecting source").

**Open dependency**: `CPrimaryBundle`'s exact current per-hit compositing
loop body is deferred to the R3 implementation task per research.md's
"Open item carried to task-decomposition" note — the adapter shape above is
independent of that loop's exact current code, since it only assumes
`CPrimaryBundle` already holds per-hit color/opacity/AOV state (confirmed via
its constructor).

## 5. Shared pixel-filter module

Wraps the already-shared `CRenderer::pixelFilterKernel` with one
accumulate-and-normalize step.

| Field | Type | Notes |
|---|---|---|
| `kernel` | → `CRenderer::pixelFilterKernel` | Unchanged, already shared (`HIDER_PARITY.md`'s Unified Pixel Filtering item is already `[x]`) |
| `accumulatedColor` | color | Running weighted sum per output pixel |
| `accumulatedWeight` | float | Running weight sum, for final normalization |

**Relationships**: Consumed by `CStochastic`, `CRaytracer`, and `CZbuffer`'s
respective `rasterEnd`/gather steps (R4). Pure refactor — FR-021 requires
zero output change for scenes already passing the visual-regression suite.

## 6. Depth-filter mode

An enum + shared evaluation function, not a stored entity with identity —
included here because it's named as a Key Entity in spec.md and has
non-trivial validation rules.

| Value | Semantics |
|---|---|
| `min` | Nearest unoccluded z among contributing samples |
| `max` | Farthest unoccluded z |
| `avg` | Weighted average z |
| `mid` | reyes's existing `DEPTH_MID` two-sample search (`stochastic.cpp:609-640`) |

**Relationships**: Takes a list of `CompositeSample.z`/`opacity` pairs
(entity 4) plus a `zvisibilityThreshold` per FR-013, applying the same
`checkZThreshold` exclusion rule (`opacity[i] > zvisibilityThreshold[i]`) on
both hiders (research.md S3).

**Validation rules**: `raytrace`'s default depth-filter behavior (whichever
mode was implicit before this feature) MUST remain unchanged when no
non-default mode is explicitly configured (FR-014) — the shared function's
default-mode branch must reproduce raytrace's pre-existing default output
bit-for-bit on scenes that don't configure a mode.

## 7. Shared per-bucket sample table (Option B)

| Field | Type | Notes |
|---|---|---|
| `bucketId` | identifier | Which raster bucket this table covers |
| `samples` | array of (jitterX, jitterY, timeStratum, lensPoint) | One entry per pixel-sample slot in the bucket, generated once by `CSampler` in table mode (entity 3) |

**Relationships**: Consumed identically (verbatim, not re-derived) by both
`CStochastic` and `CRaytracer` when rendering the same bucket (FR-025) — this
is what correlates their noise patterns. Purely additive to `CSampler`;
does not replace its per-sample API used by Option A's hiders when table mode
is off.

**Validation rules**: Internal only — FR-027 forbids any new RIB
token/option/determinism guarantee surfacing from this table's existence.
Enabling it must not be observable to scene authors except through the (now
tighter) parity thresholds it permits (entity 2, FR-026).

## 8. Documented residual

| Field | Type | Notes |
|---|---|---|
| `name` | string | e.g. `shading-density-interpolation`, `dof-occlusion-model` |
| `description` | string | The algorithmic/physical reason full convergence is out of scope (grid-vertex-interpolated shading vs. per-hit shading; screen-space DOF scatter vs. true lens-ray occlusion) |
| `affected_effect_tags` | list of enum | Which Parity scene pair `effect_tag`s this residual applies to |
| `bounding_threshold_ref` | → Per-effect parity threshold | The threshold this residual justifies loosening |

**Validation rules**: A residual is documentation + a threshold number, never
a suppressed/skipped check (FR-028) — every `effect_tag` with a residual
still has a concrete `max_block_diff` in entity 2.

## Entity relationship summary

```
Parity scene pair ──uses──> Per-effect parity threshold ──optionally justified by──> Documented residual
Shared per-sample generator ──produces──> Shared per-bucket sample table (Option B, additive)
Shared per-sample generator ──feeds lens point into──> Depth-filter mode (via CompositeSample.z from ray/grid samples)
Shared transparency/matte compositor ──produces──> CompositeSample stream ──consumed by──> Depth-filter mode, Shared pixel-filter module
```

No entity here is a persisted record — all are C++ in-memory structures
(`CSampler`, `CompositeSample`, filter-module accumulator) or test-harness
fixtures (scene pairs, thresholds, residuals as data-driven CMake/ctest
registrations, mirroring the existing `add_visual_test` per-scene threshold
override pattern).
