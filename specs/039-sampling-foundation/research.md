# Research: Sampling Foundation (spec 039)

The inventory was taken on `c4f1485`, whose `src/` and `tests/` are identical to master `c62334d`. Code was read through CodeGraph, using the main checkout's index, which has the same sources, and through targeted greps. No build, render or test was run during plan.

Measurements that plan depends on are marked **PENDING MEASUREMENT** (§M). The coordinator runs them when the machine is free, and the maintainer decides what to do with each outcome.

---

## R1. Inventory of randomness consumers (current code)

Every render-time randomness site, grouped by the source it draws from. "Fate" refers to the plan phases in `plan.md`.

### R1.1 Per-context Mersenne Twister (`CShadingContext::urand()/irand()`, `shading.h:327–361`)

The generator is seeded `randomInit(5489*(thread+1))` (`shading.cpp:446`). These sites draw from it:

| Site | Domain (archcheck class) | Draws | Fate |
|---|---|---|---|
| `stochastic.cpp:178` (`CSampler` urandFn) | Hiders | film jitter, time and importance per subpixel | camera slots (Phase 5) |
| `raytracer.cpp:518–519, 639–640` | Hiders | film jitter, time, lens | camera slots (Phase 5) |
| `trace.cpp:304` `ray->jimp = urand()` | Hiders | importance of **every** traced ray, primary included | primary: camera importance slot; secondary: transport sub-stream (Phase 6) |
| `executeMisc.cpp:735–739` (`sampleRay` macro), `:823`, `:957` | Shading | trace/transmission origin and direction jitter, motion time | transport sub-stream (Phase 6) |
| `shading.cpp:3873–3889` (gather) | Shading | gather origin jitter, motion time | transport sub-stream (Phase 6) |
| `scriptFunctions.h:272–277` (`RANDOMFEXP`), `rslOps.cpp:834–846` (`op_random_f/v`) | Shading | RSL `random()`/`urandom()` on the interpreter and the JIT | call-site key (Phase 6) |
| `irradianceDispatch.cpp:321–322, 422–423, 656` | Hiders (GI cache) | hemisphere stratum jitter, cache-insertion coin | transport sub-stream (Phase 6); the cache itself stays order-dependent until 040 |
| `photon.cpp:284, 419, 499, 645, 691, 741` | Hiders | wavelength, photon time, Russian-roulette decisions | keyed by global photon index (Phase 6) |
| `hcshader.cpp:76–78, 410–411, 479–483` | Core (`src/ri/hcshader.cpp`) | hard-coded light sample positions | transport sub-stream (Phase 6) |
| `geometryDispatch.cpp:141–148` (`CObject::cluster`) | Core (`src/ri/render/`) | jittered BVH cluster centres | keyed by object id and iteration (Phase 6) |
| `geometryDispatch.cpp:640–645` (`rasterizer->irand()`) | Core | point selection in a dice helper | keyed by object id (Phase 6) |

### R1.2 Per-context `CSobol<N>` members (`random2d/3d/4d`, `shading.h:365–367`), unscrambled and shared

| Site | Domain | Fate |
|---|---|---|
| `executeMisc.cpp:809, 944`; `shading.cpp:3878, 3881` (`sampleHemisphere`/`sampleCosineHemisphere` with `random4d`) | Shading | transport sub-stream |
| `irradianceDispatch.cpp:319, 420` (`random2d`) | Hiders | transport sub-stream |
| `texture.cpp:1403, 1563, 1578, 2141` (`random2d`); `:1673, 1853` (`random4d`) | Texturing (reached through the `CShadingContext*` parameter, part of DB-010) | the lookup takes a key parameter (Phase 6) |

### R1.3 Hider- and photon-owned `CSobol` members

- `stochastic.h:108` `apertureGenerator`, seeded `CRenderer::frame` at `stochastic.cpp:71`. This is the reyes lens sample. Fate: lens camera slot. Removing it removes `stochastic.cpp`'s only `CRenderer::frame` reference, so the matching allowlist line is a shrink candidate (§R9).
- `photon.h:69–71` `gen2/gen3/gen4` (used at `photon.cpp:244, 356`). Fate: keyed by photon index.
- `radiance.h:114` `generator` (`CSobol<4>`). Fate: keyed local stream.

### R1.4 Process-global `_urand()/_irand()`

Both are defined at `random.h:210–217` as a `thread_local std::mt19937` seeded from `std::random_device`. This source is nondeterministic even at `-t:1`.

| Site | Domain | Fate |
|---|---|---|
| `object.cpp:88–91` (`getDisp`) | Geometry | constant-keyed local stream. Today's max-displacement bound is itself random from run to run. |
| `pointHierarchy.cpp:303–312, 385–393` | Texturing | keyed by node index |
| `radiance.cpp:1070, 1085, 1222, 1283–1285` | Hiders (GI) | keyed local stream |
| `init.cpp:64` `#define urand() _urand()` (shader init code) | Core | keyed by (shader, call site) |

### R1.5 Other sources

- `file_base.cpp:67`: libc `rand()` dither, inside the display driver. Fate: an inline pixel hash (Phase 9).
- `sampler.cpp:59`: `std::mt19937 rng(bucketSeed(...))`, the `correlatedSampleTable` path. Fate: deleted (Phase 9).
- `precomp.cpp:63, 148, 152`: libc `rand()` in the offline table generator. It never runs during a render, so it is excluded (spec assumption, SC-007).

### R1.6 `common/random.{h,cpp}` helpers

`sampleHemisphere`, `sampleCosineHemisphere` and `sampleSphere` take a `CSobol<N>&` (`random.cpp:41, 79, 118`). Their callers are `executeMisc.cpp`, `shading.cpp`, `hcshader.cpp`, `photon.cpp` and `radiance.cpp`.

These functions are exported from the shared `libRIBCommon`. New overloads take the 2D/3D sample values directly. The `CSobol&` overloads are deleted in Phase 9, once they have no callers. Because no install consumer outside the tree uses them, this is not an ABI promise. The package's exported headers are checked in Phase 9 (task-level).

---

## R2. Sampler backends

### R2.1 Owen-scrambled Sobol (default)

- **Decision**: ZSobol-style design, after Ahmed and Wonka 2020 and PBRT-v4's `ZSobolSampler`.
  - The sample index is `mortonIndex(px, py) · 2^k + sampleIndex`, where `2^k` is the power-of-two ceiling of the pixel's sample count.
  - The index's base-4 digits are permuted per dimension by a hash of (dimension, seed). This decorrelates dimensions and pixels.
  - Every `get1D` reads Sobol dimension 0, and every `get2D` reads dimensions (0, 1), each with Owen scrambling keyed by hash(dimension, seed). The scramble is Burley 2020's hash-based nested uniform scramble, a Laine–Karras-style permutation with a stronger hash.
  - Dimensions beyond the tables are therefore just more hashes (FR-006), and every `get2D` tuple is a scrambled (0,2) pair (FR-029).
- **Rationale**: this is the reference design the spec names. It needs only the first two Sobol dimensions, so it does not depend on direction tables beyond dimension 1, and padding comes free. The pixel-blue-noise property of Morton ordering helps at low sample counts.
- **Alternatives considered**:
  - Global Sobol with a large direction table (Joe–Kuo, 21201 dimensions). Rejected: dimensions would run out in deep `gather` trees, and padding would be needed anyway.
  - Cycles' Sobol-Burley, which shuffles the index per pixel with no Morton ordering. Its stratification is equivalent. ZSobol was picked because the spec cites it first, and its per-pixel decorrelation needs no extra state.
- **Reference values (US2-1)**: with scrambling disabled (a test-only flag), dimension 0 is the van der Corput sequence and dimension 1 is the Sobol dimension-1 sequence. Both have published closed forms. The existing `sobolTables.h` is not used by the new sampler, and is removed in Phase 9 together with `CSobol`, if nothing else includes it.

### R2.2 PMJ02

- **Decision**: generate progressive multi-jittered (0,2) sequences (Christensen, Kensler and Kilpatrick 2018).
  - *Implemented (Phase 2, amendment)*: each pattern is built by doubling, as in the paper. For an even step the new sample for existing sample i goes in the same cell and the diagonally opposite sub-quadrant; the remaining digits are found by a depth-first search over the occupancy of the elementary intervals (one boolean each, after Pharr 2019). All 64 patterns take about 57 ms in total on the first-touch path (about 0.9 ms each, built on first use). Because only the *prefixes* of a PMJ02 sequence are (0,2)-nets, the runtime index shuffle permutes each dyadic block [2^t, 2^(t+1)) among itself rather than whole aligned blocks.
  - Generation is deterministic: a fixed internal PCG seed, once per process, lazily, under `std::call_once`.
  - Tables hold `NUM_PMJ_PATTERNS = 64` patterns of 4096 points each, about 2 MB as `float[2]`. Cycles uses the same layout.
  - A dimension pair picks a pattern by hash(dimension, seed). The point index is XOR-shuffled within power-of-two blocks (Cycles' scheme), and the values are Owen-scrambled by hash(pixel, dimension).
  - An index of 4096 or more falls back to the scrambled-Sobol path for that draw. This is documented as the index extension (spec edge case).
- **Rationale**: this is the spec's named backend. Table generation is fully deterministic and needs no stored binary blob.
- **Alternatives considered**: shipping a precomputed table file. Rejected because generation is cheap (milliseconds) and a stored blob would need its own format and licence trail.

### R2.3 Independent

- **Decision**: `value = toUnitFloat(hash64(identityKey, dimension))`, using SplitMix64/PCG-style mixing. It has no state.
- **Rationale**: it is the reference for convergence slopes and the backend that generates ground truth (FR-033).

### R2.4 Float conversion and the [0, 1) range

- **Decision**: take the high 24 bits, as `bits >> 8`, and multiply by `0x1p-24f`. This gives values in [0, 1 − 2⁻²⁴], so the result is never 1.0 (FR-004).

### R2.5 Cost

- **Decision**: no table lookups on the default path beyond two direction-number columns, and `inline` hot functions in the header. Expected cost is a few ns per draw.
- **Rationale**: the SC-010 budget is 2% (measured in §M2 and at Phase 9).

---

## R3. Sample identity and keys

### R3.1 Camera samples use absolute sample-grid coordinates

Both hiders generate a margin of subpixel samples around each bucket (`xSampleOffset`/`ySampleOffset`, for the pixel filter). Neighbouring buckets regenerate the same margin samples. Today the two hiders also index strata differently:

- Reyes uses a phase-shifted `pxi = (ny − ySampleOffset + i) mod ny` (`stochastic.cpp:197–206`).
- Raytrace uses `sub_y = (j + y) mod ny` from the bucket-local origin (`raytracer.cpp:571–574`), and passes importance index 0 (`:587`). It then overwrites `jimp` with `urand()` in `trace()`.

**Decision**: one shared function in the Hiders' `CSampler` takes **absolute subpixel coordinates** `(absSubX, absSubY)`. Each hider computes them in its own units, because the bucket origin is in different units in the two hiders:

- **Reyes**: `left`/`top` (`stochastic.cpp:151–152`) are already in subpixel units. They come from `reyes.cpp:249`, `tbucketLeft = bucketPixelLeft·nx − xSampleOffset`. Therefore `absSubX = left + j` and `absSubY = top + i`.
- **Raytrace**: `left`/`top` are in pixels (`cRay->x = left + (i+x − xSampleOffset + jitter)·invXsamples`, `raytracer.cpp:589`). Therefore `absSubX = left·nx − xSampleOffset + (i+x)` and `absSubY = top·ny − ySampleOffset + (j+y)`.

Both hiders then compute:

```
SampleId.px    = floorDiv(absSubX, nx)
SampleId.py    = floorDiv(absSubY, ny)
sampleIndex    = floorMod(absSubY, ny)·nx + floorMod(absSubX, nx)
```

`floorDiv` and `floorMod` round toward −∞, because margin samples have negative coordinates at the image border. A unit test (`Sampling_CameraGridConsistency`) runs both formulas over a bucket grid with margins and asserts that they give equal SampleIds. A margin sample therefore gets the same values in every bucket and in both hiders.

The jitter mapping is unchanged (clarify Q4): `film = jitter·(u−0.5)+0.5001011` within the stratum, `time = (strataIndex + jitter·(u−0.5)+0.5001011)/N`, and `importance` likewise with reyes' transposed index. Only `u` comes from the sampler slots.

**Lens mapping (decided at the Phase 5 test approval)**: the lens point is the lens slot's `(u, v)` through the **Shirley–Chiu concentric map** of the unit square onto the unit disk, not through `sampleDisk()`. Reason: `sampleDisk()` is a rejection loop that needs a fresh draw after every rejection, and a camera slot is a stateless function of the `SampleId`: a rejected first attempt would repeat forever (or need an invented retry index that collides with other samples). The concentric map is area preserving and continuous, needs no rejection, and keeps the (0,2) stratification of the Lens2D slot on the lens. Sobol, PMJ02 and Independent all use it; `Legacy` keeps its rejection `sampleDisk()` unchanged (deleted with it in Phase 9). Implemented in `CSampler::cameraSample()` and checked by `Sampling_CameraSlotsEqual` against an independent double-precision implementation.

**Primary-ray importance**: raytrace puts the Importance1D slot value in `CPrimaryRay::jimp` and sets `CRay::cameraImportance`, so `trace()` keeps it (a continuation after transparency keeps it too) instead of drawing `urand()`. Secondary rays still draw `jimp` from `urand()` until Phase 6 gives them a child sub-stream.

**Consequences**:
- Raytrace's stratum indexing and its importance change on purpose (FR-009). Raytrace primary rays keep the camera importance slot: `trace()` no longer overwrites `jimp` for rays that already carry one.
- **This determines the SC-003 question in plan.md §Open questions**: with `jitter 1`, film and time remain grid-stratified whichever backend supplies `u`.

### R3.2 Stable object identity

**Decision**: `CObject` gains a 64-bit `stableId`, assigned as follows.

- **Top-level objects**: the renderer core (`CRendererContext::addObject`) assigns `hash(scopeId, ordinal++)`. The root scope is `hash(frame)`.
- **Procedural expansions** (`dlobject`, `DelayedReadArchive`, `RunProgram`, `RiProcedural`): the expansion pushes a scope `(parent.stableId, childOrdinal=0)`, so the objects it adds get `hash(parent, k)`.
  - **V1, verified**: `CDelayedObject::dice/intersect` and `CDelayedInstance::dice/intersect` (`geometryDispatch.cpp:695–760`) call `CRendererContext::processDelayedObject/Instance` while holding the global `CRenderer::delayedMutex`, behind a double-checked `processed` flag. Expansions are therefore serialized, and a scope pushed and popped inside `processDelayedObject` is safe.
  - The ordinal is local to one expansion, so it is deterministic. Which thread performs the expansion makes no difference.
- **`ObjectInstance`**: each instancing call is a top-level object (its own ordinal). The copies it creates take `hash(instanceId, k)`, so two instances of one master differ.
- **Tessellation and mesh children** (`setChildren`, subdivision faces, patch-mesh patches, CSG leaves captured through `CGeometryHost`): `hash(parent, childIndex)`, in the order the parent creates them. That order is deterministic, because the parent builds them single-threaded under `tesselateMutex`.
- **Split pieces** (dicing splits a patch into sub-ranges): they **inherit** the parent's `stableId`. Their u/v are already in the parent's parameter space, so (id, u, v) stays unique.
- **`cluster()` dummy nodes**: `hash(parent, 0|1)` (front/back). They are never shaded and only key the next clustering level.

`stableId` is never taken from `CRenderer::getGlobalID`, a DB-001 symbol (FR-016).

### R3.3 Shading-cell identity (grid points)

**Decision**: `cellKey = hash(object.stableId, bits(u), bits(v), elementId, frame, seed)`, computed in `CShadingContext::shade()` after `object->sample()` has filled `u`/`v`.

- `elementId` is 0, except for primitives whose vertices share (u, v) within one object.
- **V2, partly verified**:
  - `CPoints::sample()` (`points.cpp:201`) never writes `u`/`v`, so all points of one object would share whatever `u`/`v` holds. Points use `elementId = start + i`, the point index within the `CPoints` object, which `sample()` already receives.
  - `CCubicCurve`/`CLinearCurve::sample()` (`curves.cpp:226, 445`) read the dicer's `v`, which is along the curve, with `u` across it. (u, v) is unique within one curve, and each curve of a curves primitive is a child with its own identity.
  - The blobby/implicit types and `CDLObject` are shaded through rays only (`implicitSurface.cpp:257`, `dlobject.cpp:175`), so they use ray keys, never cell keys.
  - Phase 4 confirms the remaining grid-shaded primitive families with a unit test. The test renders one small scene per family under `sobol` and reads the keys the shading engine reports through the in-process hook `CShadingContext::pointKeyObserver` (null in production; no shading-language accessor for a point key exists and `random()` does not draw from the keys before Phase 6). It asserts that no two shading points of one object share a key, and that a point shaded twice at the same (u, v) keeps its key.
- Where (u, v) is not unique, the primitive writes the discriminator in its own `sample()`. This change is internal to Geometry.

Shading reads `object->stableId` as an inline data-member access through `shading.cpp`'s existing include of `object.h`, which is registered under DB-006. It creates **no new include edge and no link symbol**, so archcheck sees no change. Spec 029's `ShaderGlobals` later carries the key and removes the read.

**Known limit (Phase 6 follow-up): a one-ULP edge risk for non-dyadic splits.** The key contains the bit patterns of `u` and `v`, so two grids of one surface that share an edge give that edge's vertices the same value only if both compute bit-identical `u`/`v` for it. The dicer fills them with a double accumulation, `u = umin; u += ustep` cast to `float` (`surface.cpp`, for example around lines 295, 1698 and 2310), not by snapping the last vertex to `umax`, so an edge vertex could in principle differ from its twin in the last bit when the split position is not exactly representable. Measured with `pointKeyObserver` on split surfaces, counting vertices of different grids whose `(u, v)` agree to 1e-5: a bicubic patch in 16 grids gave 234 such pairs and one in 256 grids 4770, and a sphere in 80 grids 1753; in every case 0 pairs differed in the bit pattern of `(u, v)` or in the key, and no vertex had `u` or `v` equal to -0.0. Those splits are dyadic (the midpoints are exactly representable), which is why they cannot show the effect. A non-dyadic split, such as a NURBS patch cut at its knot spans, is not tested: the scene needs a patch with irregular knots and a way to tell shared-edge vertices from near coincidences, and the measurement only becomes meaningful once `random()` displacement across grids is in use. If it ever shows up (a crack in a displaced NURBS), the fix is to snap the last vertex of a row to `umax` / `vmax` in the dicer, not to quantise the key. Two separate objects that touch (the patches of a mesh, separate polygons) have different `stableId`s, so their shared edges draw different values by design.

### R3.4 Ray-hit identity

**Decision**:
- `CRay` (`src/ri/core/ray.h`, archcheck class `Infra`) gains `SampleKey key` and `uint16_t depth`.
- Primary rays get the camera `SampleId`. Secondary rays get `child(key, depth+1, callSite, occurrence, sampleIndex)` when they are spawned, inside the shared ray-setup helpers in Shading.
- `trace.cpp`'s shading-group loop (Hiders) copies `ray->key` into the new per-vertex key array of `CShadingState` before calling `object->shade()`. The copy happens next to the `time` copy that `CSurface::shade` performs (FR-013).
- Geometry is not touched for ray hits.

### R3.5 Transport sub-streams and their index mapping

**Decision**: one draw is `sampler.get2D(SubstreamKey, dimension, index)`.

`SampleKey` carries **two** hashes, because a mixed hash cannot have one of its inputs removed afterwards:
- `full`, which includes the camera `sampleIndex`;
- `path`, which never includes it.

`childKey` derives both. A pixel-sample sub-stream seeds from `path` and offsets its index by `pixelSampleIndex·N`. A cell sub-stream seeds from `full` (for cells, `full` and `path` are equal).

- `SubstreamKey = hash(pointKey, shaderKey, callSite, occurrence, depth)`.
- `dimension` counts the 1D/2D slots this call site consumes (for example 0 for origin jitter, 1 for direction, 2 for time).
- `index` is the sample index within the call: the gather or trace sample loop.

Two cases for the index:

- **Pixel-sample points** (ray hits): `index = cameraSampleIndex · N + i` and the scramble seed omits `cameraSampleIndex`. All of one pixel's `trace`/`gather` samples then form one stratified set across its camera samples, which is where the convergence improvement in SC-003 comes from.
- **Cell points** (reyes grids): `index = i`, seed = `cellKey`.

**Rationale**: this follows D20. Material is per cell and lighting per visibility sample, so later integrators can key lighting by pixel sample and material by cell (FR-015).

### R3.6 Call-site ordinals in the compiler (RSL)

**Decision**: the compiler numbers each **stochastic call site** on the final IR, after the passes. The stochastic builtins are:
- `random`, `urandom`;
- `trace`, `transmission`, `visibility`, `gather`, `occlusion`, `indirectdiffuse`;
- `texture`, `shadow` and `environment` when filtered stochastically;
- the light iteration in `illuminance`/`solar`/`illuminate`.

The ordinal is carried explicitly to both backends:

- **Interpreter**: an extra integer operand on those instructions in `.rslo`.
- **JIT**: a constant argument to the `op_*` runtime entry.

`occurrence` is the per-point execution count of that call site within one shader invocation. That count is the iteration index of the enclosing loops, and is counted only for active lanes on both backends (FR-019, FR-007). `shaderKey = hash(shader name, instance ordinal in the attribute's light/shader list)`.

**Constraints and risks**:
- `random()`/`urandom()` MUST stay non-pure for CSE, DCE and `UniformLifting`. Phase 4 adds a compiler unit test for this.
- The `.rslo` instruction format and the JIT runtime ABI (`op_random_f/v` and the trace-family entries) change. Every `.rslo`/`.slo` is rebuilt by the existing dependency tracking ("rebuilding `oshader` recompiles every shader"), so the stale-bitcode trap is covered.
- CLAUDE.md gotcha 3 (macOS dead-stripping): the changed `op_*` symbols must still resolve at bind time. Running the `-slo` suite proves it.
- **Stale compiled shaders.** A hand-placed old shader would otherwise read garbage arguments silently (CLAUDE.md, "ABI/signature mismatch"). Two guards:
  - Both shader formats are versioned with semantic versioning (contract V1): MAJOR = an older or newer loader cannot read the file (operand layout, JIT ABI or `op_*` signature changes); MINOR = additive, a loader reads `file.major == supported.major && file.minor <= supported.minor`; PATCH is never checked. This change is a MAJOR one for both, so `.rslo` goes **1.0 to 2.0** and `.slo` (new, `openrender.slo.format` node, a missing node counting as 1.0.0) to **2.0**, and the loaders refuse an older file with a diagnostic that names both versions and the remedy.
  - Every changed `op_*` entry is **renamed** with a version suffix, for example `op_random_f_v2`, so a stale `.slo` that got past the format check still fails at JIT bind time with an unresolved symbol instead of running.
  - This involves no language-level change, so D12's stamp is unaffected.
- A compiler unit test compiles one shader both ways and compares the ordinal lists, which must be identical.

**Alternative considered**: number the call sites at load time, in instruction order, separately on each backend. Rejected, because it relies on `.rslo` serialization and the JIT walk visiting instructions in the same order, a property no test guarantees today.

### R3.5a Shader instance identity (Phase 6 follow-up)

The sub-stream key of a draw contains the shader's name and the **instance's own identity**, `CShaderInstance::instanceKey`. A light created with a RIB handle (`LightSource "name" 2`, or a string handle) takes a key from that handle: the RIB parser passes it on as the extra parameter `"__handleid"` (a number) or `"__handlename"` (a string), which `CRendererContext::RiLightSourceV`/`RiAreaLightSourceV` take out of the parameter list again (the RIB writer drops them as well). Every other shader instance (surface, displacement, atmosphere, imager, a light without a handle, the hardcoded lights) takes the ordinal in the order the scene creates shader instances. The occurrence counters are keyed by the same instance key and the call site, so what an instance draws does not depend on what any other instance drew, nor on whether another instance of the same shader is declared before or after it (`Sampling_InstanceIndependence_*`).

**Known limit**: the ordinal of an instance that is created while rendering (a procedural or a delayed archive that declares shaders) is its place in the order the expansions run, which depends on which bucket first needs them. Handles are not subject to this; give such lights a handle.

### R3.7 Uniform `random()` (FR-021)

**Decision**: an invocation-level `batchKey` in `CShadingState`.
- **Reyes grids**: `hash(stableId, bits(u₀), bits(v₀), frame, seed)`, where (u₀, v₀) is the grid's first vertex. This is the grid's parametric origin, independent of thread.
- **Raytrace shading groups**: `hash(bucketX, bucketY, stableId, depth, chunkIndex)`.
  - `trace.cpp:101–206` buckets rays by object through `traceObjectHash`, keyed on the object pointer. The group list itself is built in first-encounter ray order (`shadeNext`), which is deterministic, but the key does not rely on group position.
  - `chunkIndex` is the index of the `maxGridSize` chunk within that object's ray list for this bundle pass.
- **Secondary rays spawned from a reyes grid** (and nested shading at depth ≥ 1): `hash(parent batchKey, callSite, occurrence, depth, chunkIndex)`.

The value depends on bucket size, which clarify Q5 accepted; this limitation is documented.

**Not reachable from RSL source (found while writing the Phase 6 tests)**: the compiler rejects assigning `random()`/`urandom()` to a uniform variable or using one as a uniform parameter default (`Can not assign to uniform variable`), so a uniform `random()` call cannot be written in a shader and the `batchKey` draw has no source-level test. The Phase 6 probes (`sampling_random_probe.sl`, `sampling_jit_probe.sl`) therefore draw only varying values, and `SamplingJitParity_*` says nothing about the uniform form.

### R3.8 Texture-filter samples

**Decision**: the stochastic texture, shadow and deep-shadow lookups in `texture.cpp` take a `SubstreamKey` argument, built by Shading from (pointKey, callSite, occurrence), in place of reading `context->random2d/random4d`. Texturing draws from the Infra sampler, which is allowed (`* → Infra`). The `CShadingContext*` parameter itself is unchanged; it belongs to DB-010 and spec 027.

### R3.9 Photons

The photon hider takes `PHOTON_BUNDLE` jobs from `CRenderer::dispatchJob` (`photon.cpp:118`), each carrying `numPhotons`.

**Decision**:
- The core's job dispatcher stamps each bundle with a sequential `firstPhotonIndex`. Its sequence number is deterministic, whichever thread takes the job.
- A photon's key is `hash(seed, frame, lightOrdinal, firstPhotonIndex + k)`. Bounce decisions use `child(key, bounce)`.
- Storing photons in the map in an order-independent way is spec 040.

### R3.10a Derivative-offset tail vertices

Under ray-hit shading, `numVertices` can be 3 × `numRealVertices`: real vertices, then a +du block, then a +dv block (the layout documented at `shading.cpp` near `jitTraceBatch`).

**Decision**: tail vertex `numRealVertices·b + i` (b = 1, 2) **copies** the `pointKey` and `callSiteOccurrence` of real vertex `i`. Consequences:
- `random()` gives a tail vertex the same value as its real vertex, so `Du(random())` and `Dv(random())` are 0.
  - This is a statement about **ray-hit shading** only. A reyes grid has no tail vertices: its `Du()`/`Dv()` are finite differences between neighbouring grid vertices, each with its own key and so its own draw, and are not 0.
- This matches the JIT's existing tail replication for the trace builtins.
- Both backends apply the same rule.

### R3.10 Non-sample keyed streams

For randomness that is not a sample (BVH clustering, `getDisp`, point-hierarchy clustering, radiance-cache internals), a small **`CKeyedStream(key)`** in `libRIBCommon` hands out `hash(key, counter++)`.

Its counter is local to one call frame and is never shared or stored, so it is deterministic and passes SC-007's inventory. The keys are:
- `cluster`: (stableId, iteration);
- `getDisp`: a constant;
- point hierarchy: (node index);
- radiance: (cache entry index).

None of these keys needs the user seed, except the radiance cache, which is GI.

---

## R4. Configuration flow (no new `CRenderer::` reference)

- **Decision**: the renderer core builds one immutable `CSamplingConfig` (backend, seed, frame, pixel sample counts) at `beginFrame`. It passes the config to every shading context it creates, through a new constructor argument or a setter called by the core, and stores it as a `CShadingContext` member.
  - Hiders and Shading read their own member.
  - Geometry and Texturing receive keys, never the config.
- **Rationale**: every new `CRenderer::` static read from a Hiders or Shading file would be a new DB-011 or DB-001 allowlist line, which FR-036 forbids. Injecting the config instead follows constitution rule 1.
- **Alternative considered**: new `CRenderer` statics like the existing `jitter`. Rejected for the allowlist reason above.

---

## R5. Opt-in during conversion, then one flip

- **Decision**: until the last phase, the default sampler value is an internal `"legacy"` value, meaning today's streams. It is not documented. Tests and the determinism suite opt in with `Option "sampling" "string sampler" ["sobol"]`.
  - Phase 9 changes the default to `"sobol"` and deletes `"legacy"` and every legacy path: the Mersenne Twister, the `CSobol` members, `_urand`/`_irand`, `correlatedSampleTable` and the env var.
  - Phase 9 also changes the dither and re-renders the references exactly once (FR-034).
- **Rationale**: the suite stays green at every phase boundary, and a single re-render remains possible. This is the same pattern as spec 054 (D20: opt-in, default flipped in its last phase).
- **Alternative considered**: convert in place and re-render at the end. Rejected because intermediate phases would have red stochastic tests, and nothing could be validated phase by phase.

---

## R6. Display dither (Display Outputs)

- **Decision**: in `file_base.cpp`, `d = qamp·(2·h(x, y, channel) − 1)`, where `h` is an inline 32-bit integer hash written in that file. The driver already receives `x` and `y` through its `data()` callback. There is no `libRIBCommon` link and no ABI change, so there is no semver bump.
- **Rationale**: FR-025, and display drivers may depend only on their own ABI header (Principle VIII).

---

## R7. Verification harnesses

### R7.1 Determinism (FR-030)

- **Decision**: extend `tests/visual/test_depth_determinism.cpp`, keeping its current mode unchanged, with:
  - `--configs "<a>|<b>|..."`: each configuration is a list of orender options, for example `-t:1|-t:2|-t:4|-P:2`;
  - `--outputs a.tif,b.tif,...`: every display the scene writes, including AOVs and `z`;
  - `--prepend "<RIB line>"`: the harness writes a temporary copy of the scene with this line first, for example `Option "sampling" "string sampler" ["sobol"]`. This forces a backend on unmodified scenes, because orender has no option-injection flag. The scene's own `ReadArchive` paths stay valid because the copy is written next to the original or the search path is set.

  Every output is decoded through libtiff and compared bit for bit with the first configuration's output.
- **Registration**: a CMake function `add_determinism_sweep(<scene> <outputs...>)` is called from a list in `tests/visual/determinism-scenes.cmake`. The list holds every non-GI visual and parity scene, each forced to the default sampler. It runs under labels `determinism` and `determinism-sweep`, and the existing `DepthFilter_*`/`ColorDeterminism_*` tests keep the `determinism` label.
- **Runtime**: PENDING MEASUREMENT (§M3).

### R7.2 Convergence (FR-033)

- **Decision**: a new `tests/convergence/test_convergence` driver (C++, libtiff).
  - It substitutes `@SPP@` (pixel samples) or `@SAMPLES@` (shader sample count) and `@SAMPLER@` into a scene template, renders the ladder 1, 4, 16, 64 and 256, and computes RMSE against the reference.
  - It fits the log-log slope by least squares and checks SC-003.
  - Ground truth is generated once at 4096 spp or more with the independent backend, by a custom target that is **not** in ctest (`convergence-reference`). It is checked in under `tests/convergence/reference/` at 64×64 float, and the generation command is recorded in the README.
- **Scenes**:
  - DOF: lens slot, under reyes and raytrace.
  - Motion blur: time slot.
  - Area-light shadows: `quadlight` samples, under raytrace.
  - Occlusion with `maxerror 0`, so no cache is involved: shader samples, under raytrace.
  - Glossy `trace` with `samples`/`samplecone`: interpreter only, because the JIT ignores those parameters (GitHub #55, research §R10).
- **Labels**: `convergence`, excluded from the default run.

---

## R8. Thread-pin inventory (FR-032), provisional fates

| Pin | Reason today | Provisional fate |
|---|---|---|
| `quadlight`, `quadlight-slo`, `spherelight`, `spherelight-slo` (`-t:1`, `tests/visual/CMakeLists.txt:1008–1023`) | stochastic area-light samples | remove (non-GI) |
| `sphere-{random,trace,transmission,visibility}-reyes{,-slo}.rib` (`numthreads 1`, 8 files) | RNG scheduling | remove (non-GI) |
| `sphere-{occlusion,indirectdiffuse}-reyes{,-slo}.rib` (4 files) | irradiance cache (`maxerror` 0.6 by default) | **keep**, pointer to 040 (GI) |
| `tests/unit/texture_tile/*` `-t:1` (5 scripts) | byte-identical texture-cache comparisons | review each in Phase 9. Keep where the pin isolates cache behaviour (for example `concurrent_write`'s one thread per process, by design); where it only stood in for determinism, it MAY become a default-thread comparison. |
| `tests/imager/integration/test_imager_render.cpp:186–187` | byte-identical imager comparison | remove (non-GI) |
| `test_tile_source{,_synthesized}_concurrency.sh` (tolerance against `-t:1`) | multithreaded noise | MAY become exact; the tolerance is never widened |

---

## R9. Architecture-check effect

- **Certain shrinks**: `DB-011 link src/ri/hiders/raytracer.cpp -> CRenderer::correlatedSampleTable` and the same line for `stochastic.cpp`.
- **Probable shrink**: `DB-011 link src/ri/hiders/stochastic.cpp -> CRenderer::frame` (`allowlist.txt:346`). Its only use is `apertureGenerator(CRenderer::frame)`, which goes away, because the frame now arrives in `CSamplingConfig`.
- **Possible shrinks, not promised**: `CRenderer::jitter`, `pixelXsamples` and `pixelYsamples` in the hiders, if every use of them moves into `CSamplingConfig`. Phase 9 greps for each one.
- **No growth**:
  - every new dependency goes to `Infra` (`libRIBCommon`, `ray.h`) or comes from `Core`;
  - Shading's read of `object->stableId` and Hiders' write to `CShadingState` fields are inline data accesses through includes that are already registered, so they create no new edge;
  - no new `CRenderer::` reference is added in a domain file (§R4).

---

## R10. Known pre-existing gaps that limit FR-018 and SC-008

- **JIT trace ignores optional parameters.** `jitTraceBatch` (`shading.cpp:2808–2875`) hardcodes `coneAngle = 0` and `numSamples = 1` and ignores the optional parameter list (`samples`, `samplecone`, `bias`, `maxdist`). It then calls the same shared `traceReflection`/`traceTransmission` as the interpreter. The plain call forms of `trace`/`transmission`/`visibility` therefore share sampling code, but multi-sample or cone forms can't be bit-identical between backends today. `jitOcclusionBatch` likewise applies `COcclusionLookup::init()` defaults, ignoring optional parameters (the comment at `shading.cpp:2950–2965`).
  - **Decision (maintainer, plan)**: FR-018 and SC-008 are restricted to the forms both backends implement (plain `trace`/`transmission`/`visibility`/`occlusion`/`indirectdiffuse`, and `gather`).
  - The gap is filed as **GitHub #55** ("JIT trace/transmission/visibility/occlusion/indirectdiffuse ignore their optional parameters"). The maintainer has it fixed on its own `fix/` branch once 039 is complete; it is not part of 039.
- **GitHub #8** (JIT uniform fast path): SC-008's test shader avoids `if`-gated uniform assignments.

---

## M. Pending measurements (coordinator runs these; no build or test was run in plan)

### M1. `-P:N` bit-identity on today's code — MEASURED 2026-10-15 (results below)

**Purpose**: find out whether multi-process rendering is bit-identical to single-process rendering with **no randomness involved** (`jitter 0`, float output, so no dither). The answer decides whether SC-001 can include `-P`.

**Binary**: the main checkout's existing build.
1. Confirm it is current master: `git -C /Volumes/Projects/Development/CLI/openrender log -1 --format=%h` must print `c62334d`. Also confirm `build/` was built from it by checking that `build/src/orender/orender` is newer than its sources. Record both.
2. If it is not current, rebuild with `nice -n 19 cmake --build build -j4` once the machine is free.

**Environment**: copy the exact `ENVIRONMENT` that ctest gives the visual tests. Read it from ctest, so that nothing points at the `openrender/` deploy tree.

**Run it with bash** (save it as a script, or use `bash -c`). Under zsh, `$opts` does not word-split, so `-P:2 -t:1` reaches orender as a single argument (a bug the coordinator found while running it).

```bash
#!/usr/bin/env bash
R=/Volumes/Projects/Development/CLI/openrender
B=$R/build
OR=$B/src/orender/orender
W=$(mktemp -d /private/tmp/m1-pn.XXXX) && cd "$W"
# Export VISUAL_ENV exactly as ctest sets it (SHADERS=$B/shaders, ORENDERHOME, DISPLAYS, GEOMETRIES, TEXTURES):
eval "$(ctest --test-dir "$B" --show-only=json-v1 -R '^ColorDeterminism_opaque$' | python3 -c '
import json,sys,shlex
t=json.load(sys.stdin)["tests"][0]
for p in t.get("properties",[]):
    if p["name"]=="ENVIRONMENT":
        for kv in p["value"]:
            k,v=kv.split("=",1); print("export %s=%s" % (k, shlex.quote(v)))')"
env | grep -E '^(SHADERS|ORENDERHOME|DISPLAYS|GEOMETRIES|TEXTURES)=' # record these

# Extra scene: filter wider than a pixel, small buckets (bucket-edge splats)
sed -e 's/^PixelSamples 4 4$/PixelSamples 4 4\nPixelFilter "gaussian" 3 3\nOption "limits" "bucketsize" [16 16]/' \
    -e 's/color-determinism-opaque\.tif/wide-filter.tif/' \
    $R/examples/rib/tests/color-determinism-opaque-reyes.rib > "$W/wide-filter-reyes.rib"
grep -n 'PixelFilter\|bucketsize\|Display' "$W/wide-filter-reyes.rib"      # check the edit took

run_scene () {   # $1 = rib path, $2 = short name
  local rib=$1 s=$2
  local out=$(grep -o '"[^"]*\.tif"' "$rib" | head -1 | tr -d '"')
  for cfg in "t1a:-t:1" "t1b:-t:1" "p2t1:-P:2 -t:1" "p2:-P:2" "p4t1:-P:4 -t:1"; do
    tag=${cfg%%:*}; opts=${cfg#*:}
    nice -n 19 "$OR" $opts "$rib" >"$s.$tag.log" 2>&1 || echo "$s $tag RENDER FAILED (see $s.$tag.log)"
    mv "$out" "$s.$tag.tif"
  done
  for tag in t1b p2t1 p2 p4t1; do
    if tiffcmp "$s.t1a.tif" "$s.$tag.tif" >"$s.$tag.cmp" 2>&1; then echo "$s $tag IDENTICAL"
    else echo "$s $tag DIFFERS ($(grep -c . "$s.$tag.cmp") tiffcmp lines; first: $(head -1 "$s.$tag.cmp"))"; fi
  done
}

for s in color-determinism-opaque-reyes color-determinism-transparent-reyes \
         depthfilter-determinism-avg-reyes depthfilter-determinism-mid-reyes \
         depth-analytic-min-raytrace coverage-alpha-sides2-raytrace; do
  run_scene "$R/examples/rib/tests/$s.rib" "$s"
done
run_scene "$W/wide-filter-reyes.rib" wide-filter-reyes
```

Notes on the procedure:
- `-P:4 -t:1` is used instead of `-P:4` at the default thread count, so that four children do not oversubscribe the fanless machine. `-P:2` at default threads is kept to cover threads inside children.
- `Option "limits" "bucketsize"` is the accepted spelling: `RI_BUCKETSIZE` is handled at `rendererContext.cpp:1623`, and `examples/rib/bunny.rib` uses it.
- The two raytrace scenes write `z`/alpha float outputs and use pixel-centre samples (they are `test_depth_analytic` scenes). Their first `.tif` is what is compared.

Self-check: `t1a` against `t1b` must be identical. If it is not, the measurement is void (unexpected nondeterminism at one thread).

**Outcomes and what they change in the plan**:

| Result | Plan change |
|---|---|
| All `p*` identical to `t1a` | `-P:N` stays in SC-001 and FR-030 as written. |
| `p2t1` identical but `p2`/`p4` differ | The difference is threading inside the children, the same class as #48/#50. Classify it before 039 Phase 1. |
| `p2t1` differs | The cause is process assembly: bucket merge, filter splats across process boundaries, or ordering. **The maintainer decides**: a `fix/` prerequisite before 039's Phase 9, or limiting SC-001 and FR-030 to `-t` and recording the `-P` gap in HIDER_PARITY.md and the spec. |

#### M1 results (coordinator, 2026-10-15)

- **Binary and setup**:
  - Master `c62334d`, main checkout build; a no-op rebuild confirmed it was current.
  - Configurations compared against `t1a` (`-t:1`): `t1b` (`-t:1`), `p2t1` (`-P:2 -t:1`), `p2` (`-P:2`), `p4t1` (`-P:4 -t:1`), plus an extra `tN` (`-t:4`).
  - Compared with `tiffcmp` on float TIFFs.
  - **Negative control**: opaque against transparent reported differences, so the comparison is live.
  - `-P` really spawns children: three orender processes were seen during a `-P:2` render.

| Scene | t1b | p2t1 | p2 | p4t1 | tN (-t:4) |
|---|---|---|---|---|---|
| color-determinism-opaque-reyes | IDENTICAL | IDENTICAL | IDENTICAL | IDENTICAL | IDENTICAL |
| color-determinism-transparent-reyes | IDENTICAL | IDENTICAL | IDENTICAL | IDENTICAL | IDENTICAL |
| depthfilter-determinism-avg-reyes | IDENTICAL | IDENTICAL | IDENTICAL | IDENTICAL | IDENTICAL |
| depthfilter-determinism-mid-reyes | IDENTICAL | IDENTICAL | IDENTICAL | IDENTICAL | IDENTICAL |
| depth-analytic-min-raytrace | IDENTICAL | IDENTICAL | IDENTICAL | IDENTICAL | IDENTICAL |
| coverage-alpha-sides2-raytrace | IDENTICAL | IDENTICAL | IDENTICAL | IDENTICAL | IDENTICAL |
| wide-filter-reyes (gaussian 3×3, 16×16 buckets) | IDENTICAL | IDENTICAL | IDENTICAL | IDENTICAL | IDENTICAL |

- **Caveats**:
  - **(a) Threads under `-P`.** orender turns threads off whenever `-P` is combined with an explicit `-t` ("Using threads is currently not possible for network or multiprocess renders"), so `p2t1` and `p4t1` ran single-threaded children. Plain `-P:2` children keep default threading, and that row is identical too.
  - **(b) Shell.** The procedure must run under bash (see the note above the script).
  - **(c) Scope.** Only `jitter 0` scenes were measured, by design: this isolates assembly from randomness.
- **Outcome**: the first row of the outcome table applies. `-P:N` stays in SC-001 and FR-030 as written, and no `fix/` prerequisite is needed.
- **For the determinism harness**: the `-P` entry in the default configuration list must not carry `-t` (Caveat a), so it is written `-P:2`, not `-P:2 -t:1`.

### M2. Performance baseline — MEASURED 2026-10-15 (scene set chosen by the maintainer)

**Scene set (SC-010, FR-039)**: six sampling-heavy scenes. The originals are all short today; `motion-3-reyes` renders in 0.44 s at `-t:1`, so the earlier "about 18 s" note was stale. Each scene therefore has a **perf-only copy** whose only change is a larger `Format`. The two `tests/` scenes have no `Format` line, so one was added after `Display`.

The copies are checked in under `specs/039-sampling-foundation/tools/perf/`:

| Perf copy | Original | Format | Exercises |
|---|---|---|---|
| `camera-dof-reyes.rib` | `examples/rib/camera-dof-reyes.rib` | 1200×900 (3×) | reyes lens slot |
| `camera-dof-raytrace.rib` | `examples/rib/camera-dof-raytrace.rib` | 1600×1200 (4×) | raytrace camera slots |
| `motion-3-reyes.rib` | `examples/rib/motion-3-reyes.rib` | 800×600 (2×) | time slot, camera-rotation motion |
| `quadlight.rib` | `examples/rib/quadlight.rib` | 960×720 (3×) | 16-sample area-light `visibility()` sub-streams |
| `sphere-gather-reyes.rib` | `examples/rib/tests/sphere-gather-reyes.rib` | 1920×1440 (added) | `gather()` hemisphere sub-streams (cell keys) |
| `sphere-trace-reyes.rib` | `examples/rib/tests/sphere-trace-reyes.rib` | 3840×2880 (added) | plain `trace()` sub-streams (stand-in for glossy trace) |

- `sphere-trace-reyes` pins `Option "limits" "numthreads" [1]`, so its `tdef` equals its `t1`. The pin stays in the perf copy to keep the copy identical apart from `Format`. If Phase 9 removes the pin from the original under FR-032, the perf copy keeps it, so that the before and after timings stay comparable.
- The copies' displays write `file` TIFFs into the current working directory. Run them from a scratch directory.

**Procedure**:
- Run under bash, with the environment exactly as ctest sets it for the visual tests (`SHADERS=build/shaders` and so on; see the M1 snippet).
- Run `nice -n 19 orender [-t:1] <rib>` from inside `tools/perf/` (or a scratch copy of it). `t1` is `-t:1`; `tdef` is the default thread count.
- Measure wall-clock time with Perl `Time::HiRes`.
- Do 5 repetitions, **interleaved** (rep-major: for each rep, for each scene, `t1` then `tdef`), so that drift spreads across all scenes.
- The raw times are in `tools/perf/m2-baseline-c62334d.txt` (columns: scene, cfg, rep, seconds).

**Baseline (master `c62334d`, main build verified current)**: seconds. Spread is max − min as a percentage of the median; MAD is the median absolute deviation as a percentage of the median.

| Scene | t1 median | min | max | spread % | MAD % | tdef median | min | max | spread % | MAD % |
|---|---|---|---|---|---|---|---|---|---|---|
| camera-dof-reyes | 3.914 | 3.908 | 4.031 | 3.1 | 0.1 | 2.019 | 2.013 | 2.061 | 2.4 | 0.3 |
| camera-dof-raytrace | 2.548 | 2.540 | 2.556 | 0.6 | 0.2 | 1.324 | 1.320 | 1.341 | 1.6 | 0.2 |
| motion-3-reyes | 4.485 | 4.475 | 4.494 | 0.4 | 0.0 | 2.307 | 2.304 | 2.318 | 0.6 | 0.1 |
| quadlight | 2.329 | 2.309 | 2.359 | 2.2 | 0.5 | 1.195 | 1.194 | 1.199 | 0.4 | 0.0 |
| sphere-gather-reyes | 2.837 | 2.827 | 2.851 | 0.9 | 0.2 | 1.547 | 1.542 | 1.571 | 1.9 | 0.3 |
| sphere-trace-reyes | 2.170 | 2.164 | 2.212 | 2.2 | 0.3 | 2.168 | 2.160 | 2.208 | 2.2 | 0.4 |

**Interpretation**:
- MAD is 0.5% or less everywhere, so a median of 5 resolves a 2% change.
- Single runs reach about 3% above the median (camera-dof-reyes `t1` max), so **SC-010 compares medians of 5 or more interleaved repetitions, never single runs**.
- The "after" is measured the same way in Phase 9, on the same machine and the same Release build configuration, with the default sampler. It passes if every scene's median, in both `t1` and `tdef`, is at most 2% above the baseline median.

**History: the original `perf-manual` measurement (invalid as a 2% baseline).** Five `ctest -L perf-manual` repetitions on `c62334d`. The scenes are JIT-against-interpreter comparisons that take about 0.055 s each, mostly process start-up. Per-scene medians are rslo / slo seconds, with spread % in parentheses:

| Scene | rslo median (spread) | slo median (spread) |
|---|---|---|
| cfrom | 0.0548 (44%) | 0.0573 (41%) |
| ctransform | 0.0554 (114%) | 0.0572 (22%) |
| matrixops | 0.0568 (7%) | 0.0709 (11%) |
| comparisonlogic | 0.0538 (14%) | 0.0597 (12%) |
| arrayops | 0.0547 (12%) | 0.0719 (16%) |
| gather | 0.2200 (8%) | 0.2333 (46%) |

**After (Phase 9, T102) — MEASURED 2026-10-20: SC-010 NOT MET (stop-and-report).** Same procedure: bash, the visual-test environment, a scratch copy of `tools/perf/`, `nice -n 19 orender [-t:1]`, Perl `Time::HiRes`, 5 repetitions interleaved rep-major, the default sampler (`sobol`). Build: a Release build directory of the worktree (`build-release`, `CMAKE_BUILD_TYPE=Release`, vcpkg toolchain, like the baseline's; the worktree's `build` has an empty build type, and measured the same within noise). Raw times: `tools/perf/m2-after-b9dde7e-phase9.txt` (the commit plus the staged Phase 9 changes).

| Scene | t1 median | min | max | spread % | MAD % | vs baseline | tdef median | min | max | spread % | MAD % | vs baseline |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| camera-dof-reyes | 5.872 | 5.817 | 7.461 | 28.0 | 0.5 | +50.0% | 3.039 | 2.986 | 3.653 | 21.9 | 0.7 | +50.5% |
| camera-dof-raytrace | 5.575 | 5.523 | 6.200 | 12.1 | 0.9 | +118.8% | 2.868 | 2.854 | 3.386 | 18.6 | 0.5 | +116.7% |
| motion-3-reyes | 4.993 | 4.971 | 5.594 | 12.5 | 0.4 | +11.3% | 2.726 | 2.666 | 3.253 | 21.5 | 2.2 | +18.2% |
| quadlight | 5.613 | 5.539 | 5.669 | 2.3 | 0.7 | +141.0% | 2.896 | 2.825 | 3.191 | 12.6 | 2.2 | +142.4% |
| sphere-gather-reyes | 5.827 | 5.793 | 5.845 | 0.9 | 0.2 | +105.4% | 3.151 | 3.031 | 3.276 | 7.8 | 3.7 | +103.7% |
| sphere-trace-reyes | 6.475 | 6.443 | 6.574 | 2.0 | 0.5 | +198.4% | 6.508 | 6.460 | 6.845 | 5.9 | 0.5 | +200.2% |

Every scene and configuration is far above the 2% budget (+11% to +200%). **Control** (same machine and session, interleaved, `-t:1`, 3 repetitions): master's current Release build (`eda9eb0`, without spec 039) reproduces the baseline, so the slowdown is spec 039's, not the machine's:

| Scene | master `eda9eb0` | spec 039 | ratio |
|---|---|---|---|
| camera-dof-reyes | 4.157 | 6.003 | 1.44 |
| camera-dof-raytrace | 2.623 | 5.539 | 2.11 |
| motion-3-reyes | 4.508 | 4.994 | 1.11 |
| quadlight | 2.318 | 5.580 | 2.41 |
| sphere-gather-reyes | 2.881 | 5.781 | 2.01 |
| sphere-trace-reyes | 2.223 | 6.513 | 2.93 |

Start-up is not it: an empty scene takes 0.02 s in both builds. By backend (`-t:1`, one run each): sphere-trace-reyes sobol 6.55 s, pmj02 5.23 s, independent 4.27 s; quadlight 5.54 / 4.63 / 3.66; camera-dof-raytrace 5.53 / 4.62 / 3.83. So the keyed-draw machinery costs even under `independent`, and the Owen-scrambled Sobol points add about 50% on top.

**Profile of the hot draw path** (macOS `sample`, 4 s of a `-t:1` render under sobol; counts are samples at the top of the stack, idle `__ulock_wait` threads excluded):
- `sphere-trace-reyes` (3840 x 2880, reyes, camera-dominated): `sampling::detail::sobolGet2D64` 1272, `CSampler::cameraSample` 278, `cameraImportance` 193, `cameraTime` 184, `cameraFilm` 50 (plus DYLD stubs for the three, ~33) — about 2000 of about 2900 busy samples are the camera slots: every subpixel draws film, time, lens and importance as separate 64-bit nested-uniform scrambles (`nestedUniformScramble64`), through cross-library calls. `CFileOutputBase::write` 295 (the per-value dither hash on a large frame), `sobolGet2D` 97, `traceReflection` 60.
- `quadlight` (raytrace, 16-sample area light, shading-dominated): `sobolGet2D` 650, `CShadingContext::drawSubstream` 632 (the sub-stream hash and the per-point occurrence counters, once per call per point), `traceTransmission` 480, `execute` 257, `trace` 241, `get1D` 158 + 34, `keyedRandom` 41.
Candidate remedies, none applied: compute a subpixel's four camera slots from one scramble of the index (one `nestedUniformScramble64` per subpixel instead of four), inline the camera slot functions into the hiders' library, cache the per-point `drawSubstream` key across the samples of one call, and hash the dither once per pixel rather than per value.

**Phase 10 (2026-10-20): the four remedies, one at a time.** Each step was measured the m2 way (5 repetitions, rep-major, `-t:1` and default threads, Perl `Time::HiRes`), with **master's Release build `eda9eb0` interleaved run by run**, so each row compares spec 039 with master under the same machine load (the load average was 7 to 16 during these runs, from other jobs, so absolute times move and the baseline file is not a fair comparison; the ratio to master is). Raw times: `tools/perf/m2-phase10-{a,b,c,d,d2}.txt`. After every step the six perf scenes' outputs were byte-identical to Phase 9's (every remedy changes how values are computed, not what they are), `ctest -L "sampling|determinism" -LE determinism-sweep` passed but for the two crop tests (fixed later in Phase 10), and `ctest -L convergence` passed 8/8.

Remedy (a) is not literally "one scramble per subpixel": giving the four camera slots one shared index scramble would make film, lens, time and importance draws from one permutation of one point set, correlated by construction, which the padded design must not do. What it does instead: one `cameraDraws()` per subpixel (the sequence seed and the Morton index once, not four times) and a byte-table form of Sobol dimension 1 for 64-bit indices (8 lookups instead of a loop over up to 64 index bits), the same bits.

Spec 039 against master, median, per step:

| Scene | cfg | Phase 9 (t1 only) | after a | after b | after c | after d | after d2 |
|---|---|---|---|---|---|---|---|
| camera-dof-reyes | t1 | +44% | +20.7% | +18.6% | +16.4% | +28.8% | +20.9% |
| camera-dof-reyes | tdef | — | +20.0% | +20.1% | +20.2% | +15.1% | +22.5% |
| camera-dof-raytrace | t1 | +111% | +43.6% | +35.4% | +41.1% | +24.2% | +44.4% |
| camera-dof-raytrace | tdef | — | +37.0% | +38.5% | +37.8% | +33.7% | +42.0% |
| motion-3-reyes | t1 | +11% | +4.0% | +5.5% | +11.7% | +0.2% | +4.0% |
| motion-3-reyes | tdef | — | +9.3% | +7.8% | +7.3% | +17.4% | +6.3% |
| quadlight | t1 | +141% | +122.8% | +138.3% | +118.0% | +105.2% | +111.0% |
| quadlight | tdef | — | +131.0% | +133.1% | +125.2% | +137.4% | +114.9% |
| sphere-gather-reyes | t1 | +101% | +80.3% | +81.8% | +79.5% | +26.5% | +76.6% |
| sphere-gather-reyes | tdef | — | +72.1% | +71.9% | +78.3% | +43.6% | +58.1% |
| sphere-trace-reyes | t1 | +193% | +73.9% | +73.5% | +72.9% | +54.5% | +73.2% |
| sphere-trace-reyes | tdef | — | +76.2% | +81.5% | +76.3% | +65.4% | +64.6% |

Final state (`d2`), seconds:

| Scene | cfg | master `eda9eb0` | spec 039 | vs master |
|---|---|---|---|---|
| camera-dof-reyes | t1 | 4.155 | 5.026 | +20.9% |
| camera-dof-reyes | tdef | 2.117 | 2.593 | +22.5% |
| camera-dof-raytrace | t1 | 2.667 | 3.851 | +44.4% |
| camera-dof-raytrace | tdef | 1.430 | 2.031 | +42.0% |
| motion-3-reyes | t1 | 4.787 | 4.979 | +4.0% |
| motion-3-reyes | tdef | 2.411 | 2.563 | +6.3% |
| quadlight | t1 | 2.505 | 5.285 | +111.0% |
| quadlight | tdef | 1.353 | 2.908 | +114.9% |
| sphere-gather-reyes | t1 | 3.012 | 5.319 | +76.6% |
| sphere-gather-reyes | tdef | 1.816 | 2.872 | +58.1% |
| sphere-trace-reyes | t1 | 2.257 | 3.909 | +73.2% |
| sphere-trace-reyes | tdef | 2.282 | 3.757 | +64.6% |

**SC-010 is still not met** (budget: 2%). Remedy (a) gave almost all of the gain (the camera-bound scenes: sphere-trace-reyes from +193% to about +70%, camera-dof-raytrace from +111% to about +40%); (b), (c) and (d) are within noise. **Where the time goes now** (macOS `sample`, 3 s at `-t:1`, busy samples at the top of the stack):
- sphere-trace-reyes (camera-bound): `cameraDraws` 614 and `sobolGet2D64` 523 (the nested uniform scrambles of four slots per subpixel: one 64-bit index scramble and one or two output scrambles each), `CSampler::cameraSample` 241, `CFileOutputBase::write` 322 (8-bit conversion and dither of a 3840 x 2880 frame).
- sphere-gather-reyes: `rayDraw2D` 1119 (the per-sample Owen-scrambled Sobol 2D draws of `gather()`'s directions), `trace` 340.
- quadlight: `traceTransmission` 842 (its inlined per-sample draws), `drawSubstream` 498 (16 `visibility()` calls per point, each a sub-stream hash), `get1D` 168.
- camera-dof-raytrace: `trace` 547, `sobolGet2D64` 390.
The remaining cost is the Owen scrambling itself (Laine-Karras permutations of the index and of each coordinate) and the per-call sub-stream hash: what the design computes, not overhead around it. Closing the budget would need a cheaper scramble (for example a per-pixel precomputed index permutation, or a hash-based shuffle instead of the nested uniform one), which changes the sequence and so every image: a design decision, not a remedy.

**Phase 11 design reference check (2026-10-20): PBRT-v4's scrambles.** Source: PBRT-v4, `src/pbrt/util/lowdiscrepancy.h` and `src/pbrt/samplers.cpp` (github.com/mmp/pbrt-v4, master, read 2026-10-20).
- `enum class RandomizeStrategy { None, PermuteDigits, FastOwen, Owen };`
- `SobolSampler::Create`, `PaddedSobolSampler::Create` and `ZSobolSampler::Create` all read `parameters.GetOneString("randomization", "fastowen")`: **FastOwen is the default** for all three.
- `FastOwenScrambler::operator()`: `v = ReverseBits32(v); v ^= v * 0x3d20adea; v += seed; v *= (seed >> 16) | 1; v ^= v * 0x05526c56; v ^= v * 0x53a22864; return ReverseBits32(v);` (a Laine-Karras-style hash between two bit reversals).
- `OwenScrambler::operator()`: the full Owen scramble, a loop over 31 bits with one `MixBits` hash per bit.
- **Finding: spec 039 already uses the hash-based form.** `detail::nestedUniformScramble()` is `reverseBits32(laineKarras(reverseBits32(x), seed))`, and `laineKarras()` is Burley 2020's Laine-Karras hash (`x += seed` and four `x ^= x * c` rounds): the same family and cost as PBRT-v4's FastOwen (one add, one seed-dependent odd multiply and three multiply-xors), not PBRT's per-bit `OwenScrambler`. Replacing it with PBRT's FastOwen would not make the draws cheaper. Reported before any change (Phase 11, D-P11-1).

**Phase 11 (2026-10-20/21): measured.** Same method as Phase 10 (m2 scenes, 5 repetitions rep-major, master `eda9eb0`'s Release build interleaved run by run; spec 039 over master, median). Raw times: `tools/perf/m2-phase11-{a,b,c,x}.txt`.

| Scene | cfg | Phase 10 end | after T122 (seeds, rbit) | after T123 (one hash per call) | end of Phase 11 | experiment: no index scramble (bound for (ii)) |
|---|---|---|---|---|---|---|
| camera-dof-reyes | t1 | +20.9% | +17.8% | +17.0% | +16.9% | +10.9% |
| camera-dof-reyes | tdef | +22.5% | +17.4% | +18.5% | +16.7% | +11.6% |
| camera-dof-raytrace | t1 | +44.4% | +30.2% | +29.5% | +28.5% | +14.2% |
| camera-dof-raytrace | tdef | +42.0% | +28.5% | +28.9% | +28.1% | +13.1% |
| motion-3-reyes | t1 | +4.0% | +5.8% | +4.9% | +4.5% | +3.2% |
| motion-3-reyes | tdef | +6.3% | +4.4% | +4.3% | +4.1% | +1.4% |
| quadlight | t1 | +111.0% | +114.9% | +98.8% | +100.1% | +39.0% |
| quadlight | tdef | +114.9% | +128.1% | +97.2% | +96.3% | +38.7% |
| sphere-gather-reyes | t1 | +76.6% | +70.1% | +60.5% | +58.9% | +14.9% |
| sphere-gather-reyes | tdef | +58.1% | +67.4% | +54.4% | +54.1% | +13.9% |
| sphere-trace-reyes | t1 | +73.2% | +43.9% | +43.8% | +42.6% | +13.1% |
| sphere-trace-reyes | tdef | +64.6% | +46.1% | +43.4% | +44.5% | +13.1% |

- T122 (the camera's per-dimension seeds once per frame; `__builtin_bitreverse32` where available, the portable form otherwise, `Sampling_BitReverse` comparing the two over 1.2 million inputs): images byte-identical; the camera-bound scenes gain most (sphere-trace-reyes +73% to +44%).
- T123 (one finalizer round per call for the shading sub-stream key and per draw for its seeds, `mixFast`/`dimSeedsFast`; the camera keeps its per-frame seeds): quadlight +115% to +99%, sphere-gather-reyes +70% to +60%.
- T124 (the raytrace raster position summed from the pixel): no measurable cost.
- **SC-010 at the end of Phase 11: not met** (+4% to +100% over master).
- **What option (ii) could save, measured as a bound** (temporary build, reverted): removing the per-dimension index scramble altogether, which is more than (ii) removes (ZSobol replaces it with a hashed permutation of the Morton digits, which costs a little), gives the last column: +1% to +39%. Even that bound leaves every scene but motion-3-reyes above the 2% budget (quadlight +39%, camera-dof-raytrace +13-14%, sphere-trace and sphere-gather +13-15%, camera-dof-reyes +11-12%). So (ii) would save roughly 10 to 60 points per scene but could not reach SC-010 alone; the rest is the output scrambles and the per-call keyed draws themselves, against master's single `urand()` per value.

**Phase 12 design check (2026-10-21): PBRT-v4's ZSobol index padding.** Source: PBRT-v4 `src/pbrt/samplers.h`, `ZSobolSampler::GetSampleIndex()`, `Get1D()`, `Get2D()` (github.com/mmp/pbrt-v4, master, read 2026-10-21). `GetSampleIndex()` walks the base-4 digits of the pixel's Morton index from the most significant down, and for each digit picks one of the 24 permutations of {0, 1, 2, 3} with `MixBits(higherDigits ^ (0x55555555u * dimension)) >> 24) % 24` (a 64-bit hash per digit), then remaps the digit; with an odd power of two samples the last bit is XORed with a hashed parity bit. `Get1D`/`Get2D` then hash (dimension, seed) and apply the chosen randomizer (FastOwen by default) to the Sobol point. **Cost against ours**: one 64-bit hash per base-4 digit of the index, 12 to 16 digits for our 64-bit camera index (a 3840 x 2880 frame at 16 samples already has a 28-bit index), against our per-dimension `nestedUniformScramble64()` (two Laine-Karras rounds, one `finalize64` and hardware bit reversals) and, for the shading draws, one Laine-Karras round. ZSobol-style padding is a better-distributed padding, not a cheaper one; Phase 11's bound for removing the index scramble entirely does not carry over to it. Reported before any change (Phase 12, T129). **Decision (maintainer, 2026-10-21, D-P12-7): option (ii) is dropped** for that reason; Phase 12 does (a), a per-frame cache of the camera index scramble's high half (same images), then (c), per-scene SC-010 budgets.

**Phase 12 (2026-10-21): (a) measured, and SC-010 as per-scene budgets (approved at PAUSE 12; (a) reverted).** (a), the per-frame cache of the camera index scramble's high half (T130): `CameraSeeds` keeps, per camera slot, the permuted high half and the low half's seed for a high half of 0; an index with any other high half is scrambled in full. Images byte-identical to Phase 11 at `-t:1` and default threads, convergence identical to the baseline (T131). Same method as Phase 11 (master `eda9eb0`'s Release build interleaved run by run, 5 repetitions rep-major), run as **three independent sessions** back to back so the noise could be measured (raw times `tools/perf/m2-phase12-a{1,2,3}.txt`; load average 3.4 to 5.9 during the runs, the same for both builds since they alternate).

| Scene | cfg | end of Phase 11 | after (a), sessions 1 / 2 / 3 | after (a), pooled | noise A / B / C | budget |
|---|---|---|---|---|---|---|
| camera-dof-reyes | t1 | +16.9% | +19.8 / +19.1 / +16.6 | +17.0% | 2.8 / 1.1 / 3.9 | 21% |
| camera-dof-reyes | tdef | +16.7% | +18.5 / +19.0 / +17.0 | +17.7% | 1.3 / 0.9 / 3.7 | 22% |
| camera-dof-raytrace | t1 | +28.5% | +27.9 / +30.0 / +28.8 | +28.6% | 1.4 / 0.7 / 1.7 | 31% |
| camera-dof-raytrace | tdef | +28.1% | +27.0 / +25.6 / +28.3 | +26.5% | 1.8 / 0.6 / 5.1 | 32% |
| motion-3-reyes | t1 | +4.5% | +6.4 / +4.3 / +3.7 | +5.4% | 1.7 / 2.1 / 2.2 | 8% |
| motion-3-reyes | tdef | +4.1% | +7.7 / +3.2 / +4.2 | +4.7% | 3.0 / 1.1 / 3.3 | 9% |
| quadlight | t1 | +100.1% | +96.4 / +98.0 / +95.7 | +96.8% | 1.2 / 1.8 / 4.1 | 101% |
| quadlight | tdef | +96.3% | +96.7 / +98.4 / +96.0 | +97.0% | 1.4 / 1.1 / 3.3 | 101% |
| sphere-gather-reyes | t1 | +58.9% | +58.1 / +53.4 / +62.8 | +59.1% | 5.7 / 3.6 / 6.8 | 66% |
| sphere-gather-reyes | tdef | +54.1% | +53.3 / +54.4 / +54.2 | +53.7% | 0.7 / 0.7 / 1.9 | 56% |
| sphere-trace-reyes | t1 | +42.6% | +41.0 / +39.2 / +39.5 | +39.9% | 1.1 / 2.0 / 5.3 | 46% |
| sphere-trace-reyes | tdef | +44.5% | +37.0 / +42.1 / +42.0 | +41.1% | 4.1 / 3.9 / 5.5 | 47% |

- **(a) gives no measurable gain**: every scene is within its noise of the end of Phase 11. **Reverted for simplicity** (maintainer, D-P12-10, 2026-10-21; T135): `sampling.h` and the tests are back to their Phase 11 content; the raw times stay as the record. The cached part (one Laine-Karras round on the high half and one `finalize64`) is small next to what remains per draw: the low half's index scramble, the Sobol digits and the output scrambles.
- **Noise, three measures, in percentage points of overhead**: A, the largest distance of one session's overhead (median over median) from the pooled one; B, master's own session-to-session spread (largest over smallest master median, minus 1), an A/A measure of the machine; C, twice the median absolute deviation of the per-repetition paired ratios (spec 039 over master in the same repetition) across all 15 repetitions. The noise is the largest of the three.
- **Rule (D-P12-5; approved with the budgets, D-P12-8)**: per scene and configuration, the budget is the pooled overhead over the 15 interleaved repetitions plus the noise, rounded up to a whole percent; a scene whose result is 2% or less keeps 2%. A scene's budget is the larger of its `t1` and `tdef` values. Pass: a later measurement by the same method (3 sessions of 5 interleaved repetitions, pooled median against master) at or under the budget in both configurations.
- **Result**: no scene meets 2%. Budgets, approved by the maintainer at PAUSE 12 (D-P12-8, 2026-10-21; **superseded on 2026-10-22 by the re-derivation from the final timing, "Phase 13" below, D-P13-8**): camera-dof-reyes 22%, camera-dof-raytrace 32%, motion-3-reyes 9%, quadlight 101%, sphere-gather-reyes 66%, sphere-trace-reyes 47% (spec.md SC-010, amended; validation.md T133).
- The comparison is against master `eda9eb0` interleaved in the same session, as every measurement since Phase 9, now SC-010's baseline (D-P12-9), not against the `c62334d` baseline of §M2 that the old SC-010 named: the same-session control removes the machine's day-to-day drift (measured against the §M2 baseline instead, the same spec 039 times read 2 to 9 points higher, the `vs-baseline` column of the session tables).

**Phase 13 (2026-10-22): SC-010 re-timed on the final tree (T151): met.** Same method as Phase 12 (T132): Release, the six scenes, `-t:1` and the default thread count, 3 sessions of 5 repetitions interleaved run by run with master `eda9eb0`'s Release build, pooled medians and the three noise measures. The tree: the staged tree after T135 (remedy (a) reverted), T145 (`rasterQuadDepth()`) and the Phase 13 edits. Raw times: `tools/perf/m2-final-cb6923f-{1,2,3}.txt`. Load average 2.7 to 4.3, except a spike to about 19 at the end of session 2 from work outside these jobs; interleaving keeps both builds under it, and session 1 (load under 3.4) agrees with the other two.

| Scene | cfg | pooled overhead (sessions 1 / 2 / 3) | noise (pts) | budget: Phase 12 → re-derived (D-P13-8) | |
|---|---|---|---|---|---|
| camera-dof-reyes | t1 | +18.2% (+17.8 / +17.6 / +18.2) | 2.9 | 22% (unchanged) | met |
| camera-dof-reyes | tdef | +18.5% (+21.7 / +18.5 / +17.7) | 3.1 | 22% (unchanged) | met |
| camera-dof-raytrace | t1 | +31.7% (+31.2 / +30.3 / +32.7) | 2.1 | 32% → **34%** | met |
| camera-dof-raytrace | tdef | +30.5% (+30.6 / +30.6 / +30.1) | 1.8 | 32% → **34%** | met |
| motion-3-reyes | t1 | +4.2% (+4.2 / +4.4 / +4.6) | 1.5 | 9% → **6%** | met |
| motion-3-reyes | tdef | +4.1% (+4.5 / +4.2 / +4.6) | 1.9 | 9% → **6%** | met |
| quadlight | t1 | +99.0% (+98.7 / +98.2 / +99.4) | 1.9 | 101% (unchanged) | met |
| quadlight | tdef | +97.6% (+99.5 / +96.0 / +97.4) | 3.1 | 101% (unchanged) | met |
| sphere-gather-reyes | t1 | +60.4% (+60.1 / +61.2 / +62.4) | 2.0 | 66% → **63%** | met |
| sphere-gather-reyes | tdef | +55.9% (+56.7 / +54.3 / +56.3) | 2.1 | 66% → **63%** | met |
| sphere-trace-reyes | t1 | +43.7% (+43.0 / +45.4 / +44.3) | 2.8 | 47% → **51%** | met |
| sphere-trace-reyes | tdef | +45.0% (+45.7 / +46.5 / +41.6) | 5.0 | 47% → **51%** | met |

- Every scene is within its Phase 12 budget in both configurations, and within the budget re-derived from these numbers (D-P13-8, 2026-10-22: pooled overhead plus the largest noise measure, rounded up, the larger configuration). No code was changed.
- camera-dof-raytrace moved from +28.6% / +26.5% (T132) to +31.7% / +30.5%, about 3 points, more than its measured noise, and is now 0.3 points under its budget at `t1`. The clamp is not on its path (the raytrace hider does not rasterize quads), and master's own median fell (2.665 s to 2.606 s at `t1`) while spec 039's stayed level (3.428 s to 3.433 s): a shift on master's side of the ratio, not a cost spec 039 added. The margin against the Phase 12 budget was thin (0.3 points); with the re-derived budget (34%, D-P13-8) it is 2.3 points.

**Phase 13 (2026-10-22): the budgets re-derived from the final timing (D-P13-8, T159).** The rule of Phase 12, unchanged, applied to the T151 numbers above: per scene and configuration, pooled overhead plus the largest of the three noise measures, rounded up to a whole percent; the larger of `t1` and `tdef`; 2% kept by a scene at 2% or less (none is). No new timing.

| Scene | Phase 12 budget | `t1`: overhead + noise | `tdef`: overhead + noise | Re-derived budget |
|---|---|---|---|---|
| camera-dof-reyes | 22% | 18.16 + 2.92 = 21.08 | 18.52 + 3.13 = 21.65 | **22%** |
| camera-dof-raytrace | 32% | 31.73 + 2.14 = 33.87 | 30.49 + 1.82 = 32.30 | **34%** |
| motion-3-reyes | 9% | 4.21 + 1.50 = 5.71 | 4.11 + 1.88 = 5.99 | **6%** |
| quadlight | 101% | 99.00 + 1.92 = 100.92 | 97.56 + 3.13 = 100.69 | **101%** |
| sphere-gather-reyes | 66% | 60.42 + 1.99 = 62.41 | 55.93 + 2.13 = 58.05 | **63%** |
| sphere-trace-reyes | 47% | 43.65 + 2.76 = 46.41 | 45.02 + 5.00 = 50.02 | **51%** |

These are SC-010's budgets (spec.md, amended 2026-10-22). Two values sit close to a boundary: motion-3-reyes `tdef` at 5.99 (budget 6%) and sphere-trace-reyes `tdef` at 50.02 (budget 51%).

### M3. Determinism-sweep runtime — PENDING MEASUREMENT (at implement, Phase 7); rule decided

The sweep covers roughly 230 non-GI scenes × 5 configurations. Its runtime decides whether `determinism-sweep` stays in the default `ctest` run or sits beside `convergence` as an opt-in label.
- **Rule (accepted by the maintainer, plan)**: if it takes more than 10 minutes at `-j4`, it is opt-in, and the default run keeps a representative subset. The existing tests plus one scene per primitive and effect family stay under `determinism`. This is now in FR-030.

### M4. Convergence ground-truth cost — PENDING MEASUREMENT (at implement, Phase 8)

This is the cost of 4096-spp references at 64×64 for five scenes. If one scene takes more than 30 minutes, lower its resolution (32×32) rather than its sample count.

### M5. Parity re-measurement — PENDING MEASUREMENT (at implement, Phase 9)

This is the worst-case diff over five runs for every parity test under the new default. It sets SC-004's tightened thresholds. 8 and 6 are targets, not gates.
