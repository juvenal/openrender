# Feature Specification: Sampling Foundation

**Feature Branch**: `039-sampling-foundation`

**Created**: 2026-10-15

**Status**: Draft

**Input**: Replace every per-thread, per-context and process-global random source in the renderer with a counter-based, per-sample sampler that lives in `libRIBCommon`. A sample identity (pixel x/y, sample index, frame, global seed) plus a per-path dimension cursor gives `get1D()`/`get2D()` values that are pure functions of (identity, dimension). Three backends are provided: Owen-scrambled Sobol (default; hash-based Burley 2020 scrambling, Morton-ordered per-pixel indexing as in PBRT-v4 ZSobol and Cycles Sobol-Burley), PMJ02 (progressive multi-jittered (0,2), deterministic table generation, padded/shuffled higher dimensions), and an independent PCG-style hash as reference and debug. The backend and seed are chosen with `Option "sampling"`. Hiders own the camera dimensions; the transport side owns every other dimension through decorrelated sub-streams. The goal is bit-identical non-GI renders at any thread or process count, with thread pins removed, correlated camera samples across hiders by default, and a measured convergence improvement. Full brief: the maintainer's master plan, entry 039 (summarised in **Context** below).

## Context

**Why.** Renders today depend on thread scheduling. The root causes, measured on `transparency-matte-aov` (2026-10-06):

- Each shading context owns a Mersenne Twister seeded `5489*(thread+1)` and never reseeded, while buckets are handed to threads dynamically.
- `_urand()` is seeded from `std::random_device`.
- Shared `CSobol` sequences are unscrambled.
- The file display driver dithers with libc `rand()`.

No two default-thread renders of that scene were pixel-identical (worst 8×8 block difference 12–15). At `-t:1` the renders were bit-identical. With `OPENRENDER_CORRELATED_SAMPLE_TABLE=1` at 8 threads the difference fell to 0.14–0.17.

**Prerequisites, all merged (2026-10-13 → 10-15).** Each one blocked a success criterion of this spec, though none is RNG-related:

- `fix/stochastic-depthfilter-race`: average/midpoint background depth depended on the `noObjects` early-out.
- #47/#49, `fix/raytrace-midpoint-depth`.
- #50, `fix/reyes-watertight-coverage`.
- #51, `fix/reyes-tjunction-cracks`, which needed #54.
- #48, resolved by #50. `fix/reyes-ulp-determinism` added the `ColorDeterminism_*` tests.

**Binding decisions relied on** (`DEVNOTES_DETAILS/RENDER_ARCHITECTURE_REFERENCES.md` §0). This spec needs **no change** to any of them.

- **D1**: hider = visibility, integrator = transport. Dimension ownership follows the same split: hiders own the camera dimensions, the transport side owns the rest.
- **D2**: `Integrator "classic"` must be byte-identical to "today". After this spec, "today" means the post-039 renders. This spec re-baselines the stochastic references once (FR-034), so spec 030's byte-identity baseline is the post-039 output.
- **D20**: under Bxdf integrators, material is evaluated per shading cell and lighting per visibility sample. Transport sub-streams must therefore be derivable from a shading-cell identity as well as from a pixel-sample identity (FR-015).
- **D30**: domain placement. The sampler is infrastructure: `libRIBCommon`, which the architecture check classifies as `Infra`, and on which every domain may depend. No constitution amendment is needed.

**Reference sections**:

- §2.6: `Sample2D u` comes from this spec's stream (integrator-owned dimensions), never a generic RNG.
- §4.3: the light service's `Sample3D`/`Sample4D` come from this stream.
- §5.4: `ISamplingService` is this spec.
- §8.1: the shading-rate meaning that the shading-cell identity has to respect.

## Clarifications

### Session 2026-10-15

- Q: Is "a scene where some lookup reads an order-dependent cache" the right boundary for the bit-identity guarantee, with GI scenes keeping their thread pins until spec 040 while their sample draws are still converted here? → A: Yes. A GI scene is any scene in which some lookup reads an order-dependent cache: the irradiance cache with max error > 0, photon maps, or any cache filled during the render. Default `occlusion()`/`indirectdiffuse()` scenes count as GI and keep their pins until 040. Their sample draws are still converted in 039.
- Q: What convergence result counts as a pass for the default sampler? → A: For a smooth analytic integrand, a fitted log-log RMSE slope of −0.9 or steeper (independent ≈ −0.5). For each render scene, a slope at least 0.15 steeper than the independent sampler's, and RMSE at the highest sample count no worse than the independent sampler's.
- Q: How much render-time slowdown is acceptable from the new sampler? → A: At most 2% slower median render time on the performance scenes, the same bar as spec 023.
- Q: What does the `jitter` option do under the new sampler? → A: The formula `jitter·(u−0.5)+0.5001011` stays; only the source of `u` changes. `jitter 0` keeps today's fixed film, time and importance positions. Lens samples draw from the sampler.
- Q: How is uniform `random()` keyed under raytrace? → A: By the batch's deterministic contents: bucket, object identity and ray depth. It does not depend on thread or process count, but it may change with bucket size, and that is documented.
- Q: Are the option spellings accepted? → A: Yes. `Option "sampling" "string sampler"` takes `"sobol"` (default), `"pmj02"` or `"independent"`, and `"int seed"` defaults to 0. An unknown name gets a visible diagnostic, and the default sampler is used.
- Q: Are SC-004's thresholds of 8 and 6 hard gates? → A: No, they are targets. The gate is "tightened where measured margin allows, never widened", and the measured values are recorded at implement.
- Q: Which spec deletes the per-context Mersenne Twister and `random2d/3d/4d` members? → A: 039 deletes the members it makes dead; 029 no longer does.
- Q: How is `-P:N` bit-identity handled, given it was never measured? → A: Plan measures it early, with today's code and `jitter 0`. If the outputs differ, the maintainer decides at that point: either file a `fix/` prerequisite, or limit SC-001 to `-t` and record the `-P` gap.
- Q: How is the DB-016 follow-up split between 039 and 029? → A: 039 removes randomness from per-context state. Arena and stats injection stay in 029.
- Q (plan): Which call forms must be bit-identical between interpreter and JIT, given that the JIT ignores the optional parameters of the trace family? → A: Only the forms both backends implement: the plain `trace`/`transmission`/`visibility`/`occlusion`/`indirectdiffuse` calls, and `gather`. The JIT gap is GitHub #55. The maintainer has it fixed on its own `fix/` branch once 039 is complete; it is not part of 039.
- Q (plan): Where does SC-003's "0.15 steeper than Independent" bar apply? → A: Where the backend owns the stratification: lens/DOF, area-light shadows, occlusion and glossy trace. For motion blur (and film position), which stay grid-stratified under the kept jitter mapping, the bar is "no worse than Independent".
- Q (plan): Which scenes measure SC-010's 2% budget? → A: Six perf-only copies (larger `Format` only) of `camera-dof-reyes`, `camera-dof-raytrace`, `motion-3-reyes`, `quadlight`, `sphere-gather-reyes` and `sphere-trace-reyes`, in `tools/perf/`. They are timed at `-t:1` and at the default thread count, as the median of 5 interleaved repetitions. The baseline was measured on `c62334d` (research §M2).
- Q (plan): Does the full determinism sweep run by default? → A: Only if it takes 10 minutes or less at `-j4`. Otherwise it is opt-in (`determinism-sweep`), and a representative subset stays in the default `determinism` label.

## User Scenarios & Testing *(mandatory)*

The users are the people who render with openRender (scene authors, pipeline TDs), the maintainers who own the regression suite and the parity thresholds, and the contributors who will build integrators (030), BSDF sampling (035) and the light service (041) on top of this stream.

### User Story 1 - The same scene renders the same image at any thread or process count (Priority: P1)

A user renders a scene once with one thread, once with eight threads and once split across local processes. Today the images differ, sometimes by a full 255 in a block. That forces the test suite to pin a dozen scenes to one thread and makes any multithreaded render impossible to reproduce. After this feature, every scene that does not use an order-dependent global-illumination cache produces bit-identical output in every channel (colour, alpha, depth, every AOV) regardless of thread count, process count or bucket scheduling.

**Why this priority**: This is the problem the spec exists to solve. Determinism is a precondition for trustworthy regression tests, for the D2 byte-identity baseline of the integrator work, and for debugging any later sampling-dependent change.

**Independent Test**: Render each non-GI visual and parity scene at `-t:1`, at two or more other thread counts and with `-P:N`, then compare every output channel bit for bit.

**Acceptance Scenarios**:

1. **Given** any non-GI visual or parity scene, **When** it is rendered at `-t:1`, `-t:2`, the default thread count and `-P:2` on the same build and platform, **Then** all outputs are bit-identical in every channel and AOV, including depth (except the scenes disabled under GitHub #60 and #61; see Assumptions).
2. **Given** the same scene rendered twelve times at the default thread count, **When** the outputs are compared, **Then** all twelve are bit-identical.
3. **Given** a scene that used to need `Option "limits" "numthreads" [1]` or a `-t:1` test argument only because of RNG scheduling, **When** the pin is removed, **Then** the scene's visual test passes at the default thread count against its reference.
4. **Given** a scene whose lookups read an order-dependent GI cache (see FR-031), **When** it is rendered multithreaded, **Then** it is documented as outside this guarantee, keeps its pin and points to spec 040.
5. **Given** a render in a 16-bit or 8-bit format with dithering on, **When** it is repeated at a different thread count, **Then** the dithered output is bit-identical.

---

### User Story 2 - A tested, selectable sampler library (Priority: P1)

A contributor needs well-distributed sample values for any purpose: pixel jitter, lens position, a light sample, a BSDF direction. Today they would draw from a per-thread generator whose values depend on how many draws came before. After this feature, they ask the sampler for dimension *d* of a given sample identity and get a value that depends only on that identity and dimension. Three backends are available, each validated by unit tests against its defining mathematical property.

**Why this priority**: Every other story consumes this library. Its correctness has to be established in isolation, before any render depends on it.

**Independent Test**: Run the sampler unit tests alone (no renderer, no scene). They check reference values, the stratification property of each backend, distribution quality, and purity across threads and evaluation order.

**Acceptance Scenarios**:

1. **Given** the Sobol generator with scrambling disabled (a test-only mode, not a user-selectable backend), **When** the first points of the leading dimensions are generated, **Then** they equal the published reference values for the direction numbers in use, exactly.
2. **Given** the Owen-scrambled Sobol backend and any power-of-two sample count, **When** the points of one pixel returned by any single `get2D` draw are checked, **Then** every elementary interval of the (0, m, 2)-net holds exactly one point. The property is guaranteed per `get2D` tuple, not for an arbitrary pairing of two separate 1D dimensions.
3. **Given** the PMJ02 backend, **When** any power-of-two prefix of a pixel's samples is checked, **Then** it satisfies the (0,2) property: one point per elementary interval of every shape.
4. **Given** any backend, **When** the same identity and dimension are evaluated in different threads, in different orders, or in separate processes, **Then** the values are bit-identical.
5. **Given** any backend, **When** per-dimension uniformity is tested with chi-square and 2D star discrepancy is measured, **Then** the Sobol and PMJ02 backends show lower discrepancy than the independent backend at the same sample count, and every backend passes uniformity.

---

### User Story 3 - Reyes and raytrace see the same camera samples (Priority: P2)

A maintainer compares the reyes and raytrace hiders on the parity scenes. Today each hider draws its pixel jitter, lens and time samples from its own stream. Parity thresholds therefore absorb RNG noise as well as real structural divergence, and an internal environment variable (`OPENRENDER_CORRELATED_SAMPLE_TABLE`) exists only to separate the two. After this feature, the same sample identity yields the same camera sample in every hider by default. The parity thresholds then measure structural divergence alone, and the diagnostic variable and its separate "-correlated" test variants are retired.

**Why this priority**: It turns parity tests into sharper instruments and removes a diagnostic side path. It depends on Story 2 and is independent of Story 1's thread work.

**Independent Test**: Run the parity suite with correlated camera samples as the default, re-measure each test's diff, and compare against today's thresholds.

**Acceptance Scenarios**:

1. **Given** a reyes and a raytrace render of the same parity scene, **When** pixel (x, y) sample *i* is generated in each, **Then** its film, lens, time and importance values are identical. (The zbuffer hider draws no random camera samples; its fixed sample positions are unchanged.)
2. **Given** the parity suite after this feature, **When** each test is re-measured, **Then** every test passes, no threshold is wider than today, and a test whose measured margin allows it gets a tightened threshold.
3. **Given** `OPENRENDER_CORRELATED_SAMPLE_TABLE` set in the environment, **When** a scene renders, **Then** the output is unaffected (the variable is no longer read), and the separate correlated parity variants no longer exist.

---

### User Story 4 - Renders converge faster for the same sample count (Priority: P2)

A user raises `PixelSamples`, or a shader's sample count, to clean up depth-of-field, motion blur, soft shadows, ambient occlusion or glossy reflections. With independent random samples, error falls roughly as N^-0.5. With well-stratified low-discrepancy samples it falls faster: close to N^-1 for smooth integrands, and in between for integrands with visibility edges. After this feature a convergence harness measures the improvement and keeps it from regressing.

**Why this priority**: It is the quality payoff of the sampler choice. The harness also guards against a future change silently degrading sample quality.

**Independent Test**: Run the convergence harness. It renders each convergence scene at a ladder of sample counts with each backend, computes RMSE against an in-house ground truth, and reports the fitted log-log slope per scene and backend.

**Acceptance Scenarios**:

1. **Given** the DOF, motion-blur, area-light-shadow, occlusion and glossy-trace convergence scenes, **When** the harness runs, **Then** it reports per scene and per backend the RMSE at each sample count and the fitted slope.
2. **Given** a scene whose noise comes from shading-side integration (area-light shadows, occlusion, glossy trace), **When** the harness varies the sample count, **Then** it varies the knob that actually changes that integral's sample count: the shader's sample count, or the raytrace hider where `PixelSamples` supersamples shading. It never varies `PixelSamples` under reyes, where `PixelSamples` does not supersample shading.
3. **Given** the default backend, **When** its slopes are compared with the independent backend's, **Then** it meets the bars of SC-003:
   - a slope of −0.9 or steeper on the smooth analytic integrand;
   - on the DOF, area-light-shadow, occlusion and glossy-trace scenes, a slope at least 0.15 steeper than the independent backend's, with no higher RMSE at the highest sample count;
   - on the motion-blur scene, a slope and a top-rung RMSE no worse than the independent backend's.

---

### User Story 5 - Scene authors choose the sampler and the seed (Priority: P3)

A scene author selects a backend for a scene, for example the independent sampler to get uncorrelated noise or PMJ02 for comparison, and sets a seed to get a different but reproducible noise pattern. RIB output written by openRender keeps these options.

**Why this priority**: The default suits almost everyone, so this is a control surface. The independent backend is still needed by the convergence harness's ground truth and for debugging.

**Independent Test**: Render one scene with each backend and with two seeds. Check that each combination is deterministic, that different seeds give different noise, and that a RIB-out round trip preserves the options.

**Acceptance Scenarios**:

1. **Given** `Option "sampling" "string sampler" ["pmj02"]`, **When** the scene renders, **Then** the PMJ02 backend supplies every sample, and the render is deterministic across thread counts.
2. **Given** two renders that differ only in `Option "sampling" "int seed"`, **When** compared, **Then** their noise patterns differ. **Given** two renders with the same seed, **Then** they are bit-identical.
3. **Given** an unknown sampler name, **When** the scene is parsed, **Then** a visible diagnostic names the bad value and the valid ones, the default backend is used, and the render completes.
4. **Given** a scene with `Option "sampling"` written back out through RIB output, **When** the output is re-read, **Then** the same backend and seed are in effect.

---

### User Story 6 - Future transport code gets its samples from one place (Priority: P3)

A contributor building the integrators (030), BSDF sampling (035) or the light service (041) needs sample values for light selection, light positions, BSDF directions and bounce decisions. These values must be well distributed per pixel, decorrelated between bounces and call sites, and stable no matter how many draws other code makes. After this feature, the transport side derives sub-streams from (parent identity, ray depth, call site, loop/sample index). The same sub-stream mechanism serves today's `trace`/`gather`/`illuminance`/light call sites and tomorrow's integrators.

**Why this priority**: It is forward compatibility. Today's call sites already exercise the mechanism, so the value is delivered in this spec, but the main beneficiaries come later.

**Independent Test**: A unit test inserts an extra draw at one call site and checks that the values drawn at every other call site, depth and loop index are unchanged.

**Acceptance Scenarios**:

1. **Given** a shading point that runs two `trace()` loops and an `illuminance()` loop, **When** an extra draw is added to the first loop, **Then** the values seen by the second loop and by `illuminance()` are unchanged.
2. **Given** a secondary ray spawned at depth *k*, **When** its hit is shaded, **Then** its samples come from a sub-stream keyed by the parent identity and depth *k*, decorrelated from the parent's own dimensions.

---

### Edge Cases

- **Dimension count beyond a backend's tables.** Deep ray trees and many call sites can exceed the Sobol direction-number table or the PMJ02 pair set. The extra dimensions are padded by per-dimension scrambling or shuffling. They are never wrapped onto earlier dimensions, never an error, and never silently correlated.
- **Sample counts that are not powers of two** (`PixelSamples 3 3`, a shader asking for 12 samples). Values stay valid, deterministic and unbiased. Only the net and (0,2) guarantees are tied to power-of-two counts.
- **Sample index beyond the table size** for very high sample counts: valid values continue (padding or index extension). There is no repetition and no failure.
- **Lazily expanded procedurals, mesh children, CSG leaves and object instances.** Their stable object identity derives from the parent's identity and the child or instance index. It never comes from expansion or creation order, which depends on thread timing. Two instances of the same master get different identities, so they never show identical noise.
- **Uniform `random()`** (one value per shader invocation) under reyes grids and under raytrace shading batches. The value must come from a key that does not depend on batch composition, thread or scheduling (FR-021).
- **The midpoint-depth probe** (`probeDepth`, #47) used to draw one value from the per-thread stream and shift it for later shading (BUGS.md). After this feature that draw comes from its own sub-stream and changes no other value.
- **Several frames in one RIB.** The frame number is part of the identity, so consecutive frames are decorrelated, and re-rendering frame *n* alone reproduces it exactly.
- **Crop windows and `-P:N` bucket splits.** The identity uses absolute raster pixel coordinates, never bucket-relative ones, so a pixel's samples are the same whichever bucket, process or crop renders it.
- **Zbuffer hider.** It draws no random camera samples today: it uses fixed sample positions (`zbufferQuad.h`'s 0.5001011 offset) and has no lens or time dimensions. It keeps that behaviour, and its images do not change because of this spec.
- **`jitter 0` scenes** (21 example scenes, including the depth-analytic, coverage and colour-determinism tests). Film, time and importance stay at today's fixed positions, so these scenes' sample positions do not change. Only their lens samples, if any, come from the sampler.
- **Pixel filter wider than one pixel.** Samples are keyed by the pixel they are generated for, not by the pixels they splat into.
- **Seed = 0 and negative seeds.** Both are valid. Any integer seed gives a deterministic, distinct pattern.
- **Interpreter and JIT on the same shading point.** `random()`/`urandom()` and every trace/gather draw return bit-identical values on both backends.

## Requirements *(mandatory)*

### Functional Requirements

**Sampler library (`libRIBCommon`)**

- **FR-001**: The sampler MUST live in `libRIBCommon` (architecture-check class `Infra`) and MUST depend on no domain.
- **FR-002**: A **sample identity** MUST consist of absolute raster pixel x and y, the sample index within the pixel, the frame number and the global seed. A **dimension cursor** MUST travel with the identity along a path.
- **FR-003**: `get1D(identity, dimension)` and `get2D(identity, dimension)` MUST be pure functions of their arguments and the selected backend. They MUST NOT depend on hidden mutable state, per-thread state, a global or per-thread counter, draw history, evaluation order, wall-clock time or `std::random_device`.
- **FR-004**: Every value MUST lie in [0, 1) and MUST never equal 1.0.
- **FR-005**: The library MUST provide three backends:
  - **(a) Owen-scrambled Sobol**, the default: hash-based nested uniform scrambling (Burley 2020) with Morton-ordered per-pixel sample indexing, equivalent in design to PBRT-v4's ZSobol sampler and Cycles' Sobol-Burley.
  - **(b) PMJ02**: progressive multi-jittered (0,2) sequences. Tables MUST be generated deterministically, and higher dimension pairs padded by hashed per-pair shuffling.
  - **(c) Independent**: a counter-based PCG-style hash of (identity, dimension), for reference and debugging.
- **FR-006**: Dimensions beyond a backend's native tables MUST be padded by per-dimension scrambling or shuffling. They MUST NOT be wrapped, repeated, or treated as an error.
- **FR-007**: The library MUST derive **decorrelated sub-streams** from (parent identity, ray depth, call site, loop/sample index), and MUST guarantee **draw independence**: adding or removing a draw at one call site never changes a value drawn at another call site, depth or loop index.
- **FR-008**: Consumers MUST be able to draw 1D and 2D tuples. 3- and 4-dimensional tuples (the light service's `Sample3D`/`Sample4D`, §4.3) are composed from consecutive 1D/2D dimensions of one sub-stream.

**Dimension ownership (Principle VIII, D1)**

- **FR-009**: Hiders MUST own the camera dimensions through fixed slots: film (2D), lens (2D), time (1D) and importance (1D). The slot layout MUST be identical in every hider that draws camera samples (reyes and raytrace). For the same identity, those hiders MUST receive the same camera sample.
  - The `jitter` mapping `jitter·(u−0.5)+0.5001011` (`CSampler`) MUST be kept for the film, time and importance values; only the source of `u` changes. `jitter 0` therefore keeps today's fixed positions.
  - The lens sample MUST draw from the sampler and is not scaled by `jitter`.
- **FR-010**: Every other dimension (light selection and light samples, BSDF and scattering samples, gather/trace/occlusion directions, path and bounce decisions) MUST belong to the transport side and come from sub-streams (FR-007). Until spec 030, the transport side is today's `trace`/`transmission`/`gather`/`occlusion`/`indirectdiffuse`/`illuminance`/light-shader call sites. From 030 on it is the integrator, and 035's BSDF `sample()` and 041's light service MUST draw from these sub-streams, not from any generic random source.
- **FR-011**: The hider-facing camera-sample shape and the per-shading-point identity fields MUST map one-to-one onto the master-plan briefs of specs 026 and 028, so those specs carry the identity without reshaping it:
  - spec 026's `ShaderGlobals` is batch/grid-shaped and carries path depth, so the identity is per point within a batch;
  - spec 028's `ICamera::generateRay(x, y, t, lensUV)` consumes this spec's film, lens and time slots.

**Identity plumbing**

- **FR-012**: Rays and the trace, transmission and gather bundles MUST carry a sample identity and a ray depth. A secondary ray's identity MUST derive from its parent's identity, its depth and the spawning call site.
- **FR-013**: Ray-hit shading MUST copy the identity into per-point shading state, next to `time`.
- **FR-014**: Reyes grid shading MUST key its points by (stable object identity, the point's parametric u/v on the object, frame, seed). This key MUST be independent of bucket, thread, grid batching, dicing split order and grid position in the shading queue.
- **FR-015**: Transport sub-streams MUST be derivable from a shading-cell identity (FR-014) and from a pixel-sample identity (FR-012/FR-013). Under D20, material evaluation is per cell and lighting is per visibility sample, and specs 042 and 054 need both.
- **FR-016**: Every object MUST receive a **stable object identity** in scene-description order. Procedural expansions, mesh children, CSG leaves and instances MUST derive theirs hierarchically from the parent identity and the child or instance index, never from render-time creation order. Distinct instances MUST get distinct identities. The identity MUST NOT be obtained through the renderer core's global-ID service, which is a registered DB-001 symbol.

**Consumers converted**

- **FR-017**: The reyes (stochastic) and raytrace hiders MUST take their camera samples from the sampler. The zbuffer hider draws none and is unchanged. `CRenderer::correlatedSampleTable` and the `OPENRENDER_CORRELATED_SAMPLE_TABLE` environment variable MUST be removed. Correlated camera samples become the default and only behaviour.
- **FR-018**: Every sample drawn by `trace`, `transmission`, reflection or refraction via `trace`, `gather`, `occlusion` and `indirectdiffuse` MUST come from the sampler, on both backends.
  - For the call forms both backends implement, the values MUST be bit-identical on the interpreter (`.rslo`) and the JIT (`.slo`) for the same identity. Those forms are plain `trace`/`transmission`/`visibility`/`occlusion`/`indirectdiffuse`, and `gather`.
  - The JIT ignores these builtins' optional parameters (`samples`, `samplecone`, ...), so forms using them are outside the identity guarantee. That gap is GitHub #55, fixed separately on its own `fix/` branch.
- **FR-019**: RSL `random()` and `urandom()`, in all their return types and in both uniform and varying forms, MUST derive from the shading point's identity, the call site and the enclosing loop iteration (the structural key of FR-007). They MUST NOT derive from a running per-point draw counter, which would depend on how inactive SIMD lanes are handled and could differ between interpreter and JIT. Successive calls at one point (different call sites or loop iterations) MUST return distinct values, and every value MUST be bit-identical on interpreter and JIT.
- **FR-020**: Stochastic filter samples of texture, shadow and deep-shadow lookups MUST be keyed by the lookup's shading-point identity.
- **FR-021**: Uniform-variable random values (one per shader invocation) MUST come from a key that does not depend on thread, scheduling or batch composition. Under reyes the key is the grid's object identity and parametric origin. Under raytrace it is keyed by the batch's deterministic contents: bucket, object identity and ray depth. The value does not depend on thread or process count. It MAY change when the bucket size changes, and that limitation MUST be documented (`RIB_GUIDE.md` and the shading documentation).
- **FR-022**: The hard-coded light shaders (`hcshader`) MUST draw from transport sub-streams until spec 030 replaces them.
- **FR-023**: Photon emission samples MUST be keyed by the global photon index, the light and the seed. Making the photon map order-independent belongs to spec 040.
- **FR-024**: Every other renderer randomness site MUST draw from a deterministic keyed source instead of per-context or process-global state. This covers `_urand`/`_irand`, the per-context Mersenne Twister, `std::random_device` and `rand()`, in shader initialisation, `getDisp`, BVH and point-hierarchy clustering, and the midpoint-depth probe's importance draw. After this spec, no code that runs during a render reads randomness from per-thread state, a process-wide counter, wall-clock time, `std::random_device` or libc `rand()`.
- **FR-024a**: This spec MUST delete the per-context Mersenne Twister state (`urand()`/`irand()`, `next_state()`, the state array) and the `random2d/3d/4d` members of `CShadingContext` once they have no readers. Spec 029 no longer carries this.
- **FR-025**: Display dithering MUST be a deterministic hash of the pixel coordinates and channel. The driver MUST compute it from data it already receives through its ABI. Display drivers stay limited to their own published ABI header (Principle VIII) and MUST NOT link `libRIBCommon` for this.

**Scene options**

- **FR-026**: `Option "sampling" "string sampler"` MUST accept `"sobol"` (default, FR-005a), `"pmj02"` (FR-005b) and `"independent"` (FR-005c). `Option "sampling" "int seed"` MUST accept any integer, with a default of `0`. These spellings are confirmed by the maintainer.
- **FR-027**: Both tokens MUST be implemented in all four option layers: token constant, RIB parsing, option storage/query, and pre-declaration in `initDeclarations()`. They MUST also be declared and emitted by RIB output (`ribOut.cpp`'s declaration table), so a RIB round trip preserves them.
- **FR-028**: An unrecognised sampler name MUST produce a visible diagnostic that names the value and the valid choices. Per project experience, a diagnostic class such as `CODE_BADTOKEN` is visible; `CODE_LOG` swallows warnings. The default backend is then used and the render continues.

**Verification**

- **FR-029**: Sampler unit tests MUST cover:
  - Sobol reference values;
  - the net property of scrambled Sobol and the (0,2) property of PMJ02, per `get2D` tuple, for every power-of-two prefix;
  - per-dimension chi-square uniformity and 2D star discrepancy against the independent backend;
  - purity: the same identity and dimension give bit-identical values across runs, across threads evaluating concurrently, and in any evaluation order;
  - draw independence (FR-007);
  - padding beyond native tables (FR-006).
- **FR-030**: The existing determinism harness (`tests/visual/test_depth_determinism.cpp`, label `determinism`, which drives `DepthFilter_Determinism_*` and `ColorDeterminism_*`) MUST be extended, not duplicated:
  - It MUST compare colour, alpha, depth and every AOV bit for bit across configurations (`-t:1`, at least two other thread counts, `-P:N`), not only across repeated runs at one thread count.
  - It MUST cover every non-GI visual and parity scene.
  - The `determinism` label MUST stay separately selectable, and the full sweep MUST have a documented runtime.
  - If the full sweep takes more than 10 minutes at `-j4`, it MUST be opt-in under its own label (`determinism-sweep`), and a representative subset (the existing tests plus one scene per primitive and effect family) stays in the default `determinism` label.
- **FR-031**: A **GI scene** is a scene in which some lookup reads an order-dependent cache: the irradiance cache with a non-zero max error (the default is 0.6, so `occlusion()`/`indirectdiffuse()` scenes qualify by default), photon maps, or any other cache populated during the render. All other scenes are non-GI. GI scenes are outside SC-001's bit-identity guarantee and keep their thread pins, each with a pointer to spec 040. Their sample draws are still converted by this spec (FR-018, FR-023).
- **FR-032**: Every thread pin MUST be inventoried with its reason and its fate. The pins are the `-t:1` test arguments in `tests/visual/CMakeLists.txt`, the `Option "limits" "numthreads" [1]` lines in `examples/rib/`, and the `-t:1` uses in `tests/unit/texture_tile/` and `tests/imager/`. A pin that exists only because of RNG scheduling on a non-GI scene MUST be removed. A GI-scene pin stays, with a pointer to 040. A pin that isolates something else stays, with its reason recorded. A test that currently compares a multithreaded render against a `-t:1` render with a tolerance MAY become an exact comparison. Its tolerance MUST NOT be widened.
- **FR-033**: A convergence harness MUST measure RMSE against sample count for each backend. It covers at least a DOF scene, a motion-blur scene, an area-light-shadow scene, an occlusion scene and a glossy-trace scene, and fits the log-log slope per scene and backend.
  - Ground truth MUST be rendered in-house at 4096 or more samples per pixel with the independent backend, so it is not correlated with the backend under test.
  - The harness MUST vary the knob that controls each integral's sample count: `PixelSamples` for camera-side effects (DOF, motion), and the shader's sample count or the raytrace hider for shading-side integrals.
  - The harness MAY be excluded from the default test run (a separate label), but it MUST be runnable from ctest.
- **FR-034**: Stochastic visual and parity references MUST be re-rendered **once**, in one documented step, after the sampler is the default. The spec directory MUST record the list of re-rendered references and the reason for each.
- **FR-035**: Every parity test MUST pass with correlated camera samples. Thresholds MUST be re-measured. A threshold MUST be tightened where the measured margin allows it, and MUST NOT be widened. The `-correlated` parity variants and `VISUAL_ENV_CORRELATED` MUST be removed as redundant. Their base tests carry the tightened thresholds.

**Principle VIII and documentation**

- **FR-036**: This spec MUST NOT add any cross-domain dependency edge or any allowlist line. It MUST NOT grow registered violations DB-001, DB-005, DB-006, DB-011 or DB-017. Hiders MUST NOT name transport-specific types; Geometry and Texturing MUST NOT reach Shading or Hiders to obtain random values.
- **FR-037**: The allowlist lines this spec makes stale MUST be deleted, and the affected registry entries' history MUST record the reduction (spec 039 and the date). At minimum these are the two DB-011 `CRenderer::correlatedSampleTable` lines (`raytracer.cpp`, `stochastic.cpp`).
- **FR-038**: Documentation MUST be updated:
  - the `DEVNOTES.md` status table;
  - `HIDER_PARITY.md` (the correlated-table section and the #48 note);
  - `BUGS.md` (the midpoint-probe stream-shift note);
  - `RIB_GUIDE.md` (the new options);
  - the Hugo site (constitution VII): `docs/site/content/manual/reference/options.md`, `multithreading.md` and `network-parallel-rendering.md`;
  - `DOMAIN_BOUNDARIES.md` (the DB-016 follow-up that names 039: record which part this spec delivers);
  - `tests/visual/parity-thresholds.md`.
- **FR-039**: Median render time MUST NOT rise by more than 2% on the spec's six sampling-heavy performance scenes: the perf-only copies in `specs/039-sampling-foundation/tools/perf/` of `camera-dof-reyes`, `camera-dof-raytrace`, `motion-3-reyes`, `quadlight`, `sphere-gather-reyes` and `sphere-trace-reyes`.
  - The bar is the same as spec 023's (2%).
  - The time is measured at `-t:1` and at the default thread count, as the median of at least 5 interleaved repetitions, on the same machine and Release build configuration, with the default sampler.
  - The baseline is `tools/perf/m2-baseline-c62334d.txt` (research §M2). The `perf-manual` scenes are too short to resolve 2%.

### Key Entities

- **Sample identity**: absolute pixel x and y, sample index, frame and global seed. It names one camera sample, or one shading cell when combined with an object identity. It travels on rays and into shading state.
- **Dimension cursor**: the next free dimension along one path. Hiders use fixed camera slots; sub-streams start their own cursor.
- **Sub-stream key**: (parent identity, ray depth, call site, loop/sample index). It yields a decorrelated stream for one transport use, independent of every other draw.
- **Stable object identity**: assigned in scene-description order, and derived hierarchically for procedural children, mesh children, CSG leaves and instances.
- **Shading-cell identity**: (stable object identity, parametric u/v, frame, seed). This is the key for grid-shaded points, and for per-cell material evaluation under D20.
- **Sampler backend**: Owen-scrambled Sobol, PMJ02 or independent. One is active per render, selected by `Option "sampling"`.
- **Camera sample**: the film, lens, time and importance values produced for one sample identity, shared by every hider that draws camera samples (reyes and raytrace). `jitter` scales the film, time and importance values as today.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: 100% of non-GI visual and parity scenes produce bit-identical output in every channel (colour, alpha, depth, every AOV) at `-t:1`, at two or more other thread counts and at `-P:2`, on the same build and platform, **except the four scenes disabled under GitHub #60 and #61** (see Assumptions: defects that predate 039 and are independent of the sampler). Zero thread pins remain on non-GI scenes.
- **SC-002**: The sampler unit tests all pass:
  - Sobol reference values match exactly.
  - The net property (scrambled Sobol) and the (0,2) property (PMJ02) hold, per `get2D` tuple, for every power-of-two prefix up to at least 4096 samples.
  - Chi-square uniformity passes in every tested dimension.
  - 2D star discrepancy of Sobol and PMJ02 is below the independent backend's at 64 samples or more.
  - Purity holds across at least 8 concurrent threads and shuffled evaluation order.
- **SC-003**: In the convergence harness, the default backend meets both of these bars:
  - For the smooth analytic control integrand in the sampler unit tests, its fitted log-log RMSE slope is −0.9 or steeper. The independent backend is ≈ −0.5.
  - For each render scene where the backend owns the stratification (DOF lens, area-light shadows, occlusion, glossy trace), its slope is at least 0.15 steeper than the independent backend's, and its RMSE at the highest sample count is no higher than the independent backend's.
  - For the motion-blur scene, its slope and its RMSE at the highest sample count are both no worse than the independent backend's. Time and film position stay grid-stratified under the kept jitter mapping (clarify Q4), whichever backend supplies `u`.
- **SC-004**: Every parity test passes with correlated camera samples. No threshold is wider than before this spec. Every test whose re-measured worst-case diff leaves margin has a tightened threshold. The gate is "tightened where measured margin allows, never widened". The former `-correlated` variants' thresholds (8 for motion-patches-translate, 6 for motion-quadrics-translate) are targets, not gates. The measured values are recorded at implement.
- **SC-005**: Stochastic references are re-rendered exactly once, and a recorded list gives each one's reason. After the re-render the full visual suite passes with no threshold widened.
- **SC-006**: The architecture check passes. The allowlist only shrinks: at least the two DB-011 `correlatedSampleTable` lines are gone, and no line is added.
- **SC-007**: An inventory search of render-time code finds zero uses of per-context or process-global random state, `std::random_device`, libc `rand()` or wall-clock seeding. Offline tools that never run inside a render are listed and excluded.
- **SC-008**: A test shader that writes `random()`, `urandom()` and sample directions from the plain call forms of `trace()`/`transmission()`/`visibility()`/`occlusion()`/`indirectdiffuse()`, and from `gather()`, to AOVs produces bit-identical AOVs on the interpreter and the JIT. Forms that use optional parameters are excluded (GitHub #55). The test shader avoids the shape of GitHub #8, a uniform assignment gated by `if`, which the JIT executes unconditionally; that is a known defect this spec does not own.
- **SC-009**: Each of the three backends renders every non-GI scene deterministically, as in SC-001. Two seeds give different noise, and the same seed gives bit-identical output.
- **SC-010**: On each of the six performance scenes in `tools/perf/`, at both `-t:1` and the default thread count, the median of at least 5 interleaved repetitions after this feature is at most 2% above the baseline median in research §M2 (master `c62334d`), on the same machine and Release build configuration. Single runs are never compared.

## Assumptions

- "Bit-identical" means on one platform and build configuration. Cross-platform (macOS vs Linux) or cross-compiler identity is not required.
- Excluded from every "bit-identical at any thread count" claim in this spec (the user story above, acceptance scenario 1, FR-030 and FR-031, SC-001 and SC-009): the four scenes disabled in the determinism sweep under two defects that predate 039 and are independent of the sampler. GitHub #60 is a thread race in `sphere-shader-parameter-reyes` and `sphere-shader-parameter-reyes-slo` (a deterministic shader whose output differs at `-t:4`, also with `jitter 0` and the Legacy sampler). GitHub #61 (an uninitialised derivative lane in shading that makes `transmission()` renders nondeterministic) is fixed for ray-hit shading by T084b; `csg-hero-zbuffer-1600` and `csg-hero-stats-1600-reyes` stay disabled for its grid-shading part. Those four scenes stay in the sweep list, registered and disabled; they become subject to the claim again when #60 and #61 are fixed.
- `-P:N` (local multi-process) rendering splits work by bucket. With pixel-keyed identity it should need no special handling, but its bit-identity has never been measured.
  - Plan MUST measure it early, with today's code and `jitter 0`, before relying on it.
  - If outputs differ for reasons unrelated to random numbers (such as bucket merging or filter splats at bucket edges), the maintainer decides at that point: either file a `fix/` prerequisite, or limit SC-001 and FR-030 to `-t` and record the `-P` gap.
- Changing the default from per-thread Mersenne Twister to scrambled Sobol changes every stochastic image. That is intended, and covered by the single re-render (FR-034).
- `Sample3D`/`Sample4D` (§4.3) are compositions of consecutive 1D/2D dimensions. No native 3D/4D backend is required.
- Spec 026's `ShaderGlobals` and spec 028's `ICamera` are not yet implemented. FR-011 is satisfied by matching their briefs (the master plan and §5.5), and by recording the mapping in the plan.
- The DB-016 follow-up ("`globalArena`/`stats` become injected per-context state in specs 029 and 039") is split as confirmed by the maintainer: 039 removes randomness from per-context state, and arena and statistics injection stays in 029. FR-038's `DOMAIN_BOUNDARIES.md` update records this split.
- The brief's "022 allowlist shrinks (RNG part of V1)" refers to audit finding V1, now DB-005 (`CShadingContext`). The master plan describes V1's RNG part as "no per-thread RNG state remains". No allowlist line was found that is specific to that RNG state, because the per-thread generator is inline in `shading.h`. The measurable shrink is therefore the DB-011 lines in FR-037. The plan re-measures and adds any further stale lines.
- The master-plan brief of spec 029 said 029 removes the per-context Mersenne Twister and `random2d/3d/4d` members. The maintainer decided that 039 deletes them, because it moves every reader off them (FR-024a). The 029 brief no longer carries this item.
- `precomp`'s use of libc `rand()` generates tables offline and never runs during a render. It is out of scope for FR-024 and is listed under SC-007's exclusions.
- The convergence harness and the full determinism sweep may run longer than the default suite. They get their own labels so a developer can skip them, and they stay runnable from ctest.

## Dependencies

- Spec 023 (common infrastructure floor, `libRIBCommon` shared library): merged.
- Prerequisite fixes, all merged: `fix/stochastic-depthfilter-race`, #47/#49, #50, #51 (with #54), and #48 (resolved by #50).
- Downstream: 029 depends on this spec. Specs 030, 035, 040, 041, 042 and 054 consume the identity and sub-streams.

## Out of Scope

- Order-independent GI caches (photon map, irradiance cache): spec 040.
- The steady reyes/raytrace gap in `transparency-matte-aov`: a separate `fix/` investigation.
- Integrators (030), BSDF sampling (035), the light service (041) and shading-rate unification (054). This spec only provides the stream they draw from.
- Adaptive or progressive sampling, and blue-noise screen-space dithering of sample patterns across pixels.
