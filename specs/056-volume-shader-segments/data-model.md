# Data Model: Volume Shader Segments (spec 056)

Every type here is Shading's (`src/libshader/shading/`), apart from the deletions in Graphics State and Geometry
listed at the end. There is no persistent state, no new RIB syntax, no new attribute or option (A10), and no change to
`CRendererServices`, `ITracer` or `kRendererServicesVersion` (stays 2.1.0).

## Spawning surface (`CSegmentSpawn`, new, `bundles.h`)

The surface a traced ray leaves. Captured once, when a builtin builds a segment bundle, from the context's current
state.

| Field | Type | Source | Meaning |
|---|---|---|---|
| `surface` | `const CSurface *` (opaque) | `currentShadingState->currentSurface` | for `setShadedObject()` and the observers; never dereferenced by Shading |
| `object` | `openrender::shading::ObjectHandle` | `objectHandle()` | the handle the services take (`attribute()`, messages) |
| `attributes` | `const CAttributes *` | `objectAttributes()` | the light list, `attribute()` reads, the `Interior`/`Exterior` slots |

Validation: if `attributes` is null (no object: the imager or a bare execution) or the current state is a volume
segment state (`volumeSegment`), the builtin attaches no volume shader (FR-009, FR-027 of spec 026).

## Segment ray (`CSegmentRay : CRay`, new, `bundles.h`)

`CTraceRay`, `CTransmissionRay` and `CGatherRay` derive from it instead of `CRay`. The rays come from raw `ralloc`
memory, so the builtin writes both fields explicitly for every ray.

| Field | Type | Set by | Meaning |
|---|---|---|---|
| `segmentStart` | `float` | builtin: `0`; bundle: `ray->t` when it queues a continuing ray | distance along `dir` from `from` where the current segment starts |
| `segmentLimit` | `float` | builtin | the furthest a miss may reach: `C_INFINITY` for `trace()`, the call's `maxDist` for `gather()`, the ray's initial `t` (the destination less the bias) for `transmission()`/`shadow()` |

## Segment bundle (`CSegmentBundle : CRayBundle`, new, `bundles.h`)

The base of the three Shading bundles. `CRayBundle` (the protocol type in `shading.h`, which `CPrimaryBundle` also
derives from) changes only by an in-class initialiser: `CShaderInstance *postShader = nullptr;`.

| Member | Type | Meaning |
|---|---|---|
| `context` | `CShadingContext &` | the context that built the bundle; the step runs on it |
| `spawn` | `CSegmentSpawn` | the spawning surface |
| `postShader` (inherited) | `CShaderInstance *` | the segment's volume shader for this submission; null means no step |
| `levelTraced` | `int` | FALSE at submission; set TRUE by the bundle's `postTraceAction()`; a miss call with it FALSE is the max-depth refusal |
| `begin(n, rays, volume)` | method | one submission: sets `numRays`, `rays`, `depth = 0`, `last = 0`, `postShader = volume`, `levelTraced = FALSE` (replaces the five hand-written field resets at `executeMisc.cpp:802-806, 820-824, 952-956, 969-973, 988-1001` and `shading.cpp:4436-4451`) |
| `attenuateHits(n, rays, Ci, Oi)` | protected | fills each ray's `segStart`/`segEnd` (a hit: `segmentStart`, `ray->t`) and calls `context.shadeVolumeSegments(...)` in place on `Ci`/`Oi` |
| `attenuateMisses(n, rays, Ci, Oi)` | protected | the same, with `segEnd` from the miss rule: `min(ray->t, segmentLimit)` when below `C_INFINITY`, else `C_INFINITY` ("to the world-bound exit"); `Ci`/`Oi` start at 0 |

The bundle computes no `P`, `I` or world-bound exit itself: `rendererWorldBmin()/Bmax()` are `protected` members of the
`final` context (`shading.h:1038-1052`) and stay so. `shadeVolumeSegments` derives `P`, `I` and the exit inside the
context. The per-call scratch for `segStart`/`segEnd` (1 float per ray each) and the miss `Ci`/`Oi` (3 floats per ray) is
allocated from `threadMemory` inside a `memBegin`/`memEnd` checkpoint that the bundle opens around the step. Otherwise it
would pile up until the spawning `shade()` returns.

### Segment endpoint rules

| Case | Endpoint `e` | Origin `s` |
|---|---|---|
| hit | `ray->t` | `segmentStart` |
| miss, `min(ray->t, segmentLimit) < C_INFINITY` | that minimum (a clip plane, a gather `maxdist`, a transmission destination) | `segmentStart` |
| miss, nothing finite | `max(s, tFar(world bound))`; `s` if the ray never enters the bound, the bound is empty, or there are no services | `segmentStart` |

`P = from + e·dir`, `I = (e - s)·dir`, both computed by `shadeVolumeSegments` from the `segStart`/`segEnd` the bundle
passes (`segEnd = C_INFINITY` means "the world-bound exit"). `I` is always finite (FR-007, SC-009). The world bound is
`CRendererServices::worldBmin()/worldBmax()`, in camera space (research R5), read inside the context.

### Per-bundle behaviour (`postShader != null`; with null, every path is byte-identical to today)

| Bundle | `postTraceAction` | hits: `postShade(n, rays, varying)` | misses: `postShade(n, rays)` with `levelTraced` | misses with `!levelTraced` (refusal) |
|---|---|---|---|---|
| `CTraceBundle` | sets `levelTraced` | record each hit's own Oi → step on Ci/Oi → classify on the **recorded** Oi, composite with the attenuated values; a queued ray gets `segmentStart = ray->t` | step with 0/0 → depth 0: `dest += Ci'·m`; depth > 0: `color += (1 - opacity)·Ci'`, `dest += color·m` | today's path |
| `CTransmissionBundle` | sets `levelTraced` | classify on own Oi; the step runs on every hit (an opaque hit is not queued, so its result is discarded); composite as today with the attenuated Oi; queued ray gets `segmentStart = ray->t` | step with 0/0 → depth 0: `dest += (1 - Oi')·m`; depth > 0: `opacity += (1 - opacity)·Oi'`, `dest += (1 - opacity)·m` | today's path |
| `CGatherBundle` | sets `levelTraced` before the non-shade outputs | step on Ci/Oi, then record the outputs (so `surface:Ci`/`surface:Oi` are attenuated; other outputs and tags unchanged) | step with 0/0, record only the outputs whose global entry is `VARIABLE_CI`/`VARIABLE_OI` (approved Q1, spec FR-007) | today's path (no-op) |

## Shading state (`CShadingState`, changed, `shading.h`)

| Field | Change | Meaning |
|---|---|---|
| `postShader` | kept; comment changed | the volume shader of the segments this ray level's rays travel, copied by `enterRayLevel`; **read by no shading code** (research R9) |
| `volumeSegment` | new, `int`, FALSE in `newState()` | TRUE while the state runs a segment step; the three trace builtins attach no volume shader to rays built in it |
| `lightSideN` | new, `const float *`, null in `newState()` | during `iterateLights`' light loop: a snapshot of the surface's `N` (one vector per vertex, in a buffer allocated with the state like `Ns`); null otherwise; the side normal of a `shadow()` called from a light |

## Trace location (`CShadingContext::CTraceLocation`, changed, `shading.h:1119`)

| Field | Change | Meaning |
|---|---|---|
| `sideN` | new, `vector` | the side normal (research R7). Equals `N` for `trace()`, `transmission()`, `visibility()`, `environment()`; for `shadow()` it is `lightSideN[v]` inside a light loop, else `varying[VARIABLE_N][v]`. Written at all six fill sites: `giFunctions.h:68`, `shaderFunctions.h:1880`, `:1986`, `shading.cpp:3191`, `:3203`, `:3359` |

## Context members (`CShadingContext`, `shading.h` / `shading.cpp` / `executeMisc.cpp`)

| Member | Change |
|---|---|
| `shadeVolumeSegments(CShaderInstance *, const CSegmentSpawn &, int n, CRay *const *rays, const float *segStart, const float *segEnd, float *Ci, float *Oi)` | new, public (bundles call it): the step of research R2. It derives `P = from + segEnd·dir` and `I = (segEnd - segStart)·dir` per point, using the world-bound exit (slab test, double precision, clamped to `>= segStart`; zero-length when the ray never enters the bound, the bound is empty or there are no services) wherever `segEnd` is `C_INFINITY`. Any `n >= 0`: the step works through the points in chunks of at most `maxGridSize()` itself, because `tracer.h` promises chunking only for hits, and a tracer, the canned contract double included (`recording_tracer.h:223-224`), may report every miss of a bundle in one call. `Ci`/`Oi` in and out, 3 floats per point |
| `traceReflection` | volume selection gated on `spawn` and `volumeSegment`; side test on `sideN`; submits with `begin()`; writes `segmentStart`/`segmentLimit` |
| `traceTransmission` | gains the interior/exterior split (one bundle when the shaders are equal, as today); side test on `sideN`; submits with `begin()`; writes the segment fields |
| `gatherHeaderBegin` / `gatherSample` | the bundle is built with the context; selection gated as above; `segmentLimit = maxDist` |
| `iterateLights` | sets `lightSideN` around the light loop |
| `incidentParameter` / `oppositeParameter` | pick `Exterior`/`Interior` from vertex 0's `sign(I·N)` (research R10) |
| `shade(const ShaderGlobals &, IPointSource &)` | the selection block (`shading.cpp:1384-1400`) and the post-shader execution (`:1452-1456`) are deleted |

## `CGatherVariable` (changed, `shaderPl.h:219`)

| Member | Change |
|---|---|
| `globalEntry()` | new, virtual, default `-1`; `CShaderVectorVariable`/`CShaderFloatVariable` return their `entry` when it names a global variable. Used only by the gather miss step (approved Q1) |

## Deleted

| Item | Location | Note |
|---|---|---|
| `selectVolumeShader` | `src/ri/state/attributes.h:251-263` | FR-011 |
| `ATTRIBUTES_FLAGS_SOLID_FRAGMENT` | `src/ri/state/attributes.h:59` | FR-011; bit 26 free; the stale "spec 013" comment goes with it (FR-020) |
| the flag write | `src/ri/geometry/csgSolid.cpp:273` | the clone at `:272` stays (operand flip) |
| `CSG_InteriorExteriorSelection` | `tests/unit/csg/test_interior_exterior_selection.cpp`, `tests/unit/csg/CMakeLists.txt:64` | retirement approved at clarify |

## State transitions (one submitted segment bundle)

```text
built (spawn captured, volume chosen by side)
  -> begin(): levelTraced = FALSE
  -> [tracer at max depth]  postShade(all, rays)  -> refusal: no step, today's result      -> done
  -> [tracer below max]     postTraceAction(): levelTraced = TRUE
        -> per chunk: postShade(n, rays, varying)  -> step on hits -> classify (own Oi) -> composite
        -> per chunk: postShade(n, rays)           -> step on misses (0/0) -> composite
        -> post(): queued (transparent) rays have segmentStart = their hit t
        -> [rays left] continue the loop (next segment)  |  [none] done
```
