---

description: "Task list for spec 039: sampling foundation"
---

# Tasks: Sampling Foundation

**Input**: design documents in `specs/039-sampling-foundation/`:
- plan.md (Phases 0–9, gate, risks, plan decisions 1–4)
- spec.md (FR-001–FR-039, FR-024a, SC-001–SC-010, 13 clarifications)
- research.md (R1–R10, M1–M5)
- data-model.md
- contracts/sampler-api.md, rib-options.md, shading-runtime.md, test-harnesses.md
- quickstart.md
- tools/perf/

**Tests**: REQUIRED (constitution III). Each phase writes its tests first and shows them failing at a **TEST-APPROVAL** pause before any implementation task in that phase.

**Exceptions**: the harness phases (7, 8) and the flip (9) build verification tooling or apply decisions. They are reviewed at their phase PAUSE.

**Organization**:
- Phases follow plan.md's sequence, not strict story order. The sampler library (US2) is the foundation for every other story, and the flip has to come last (research §R5).
- Story labels:
  - **US1**: determinism;
  - **US2**: the sampler library;
  - **US3**: correlated camera samples;
  - **US4**: convergence;
  - **US5**: options;
  - **US6**: transport sub-streams.
- Every phase ends in a **review PAUSE**. Results are reported, changes are staged with `git add`, and the maintainer reviews and commits.
- **The agent never commits, merges, pushes or rebases. It declines every spec-kit auto-commit hook.**

**Working directory**: `/Volumes/Projects/Development/CLI/openrender-worktrees/039-sampling-foundation`. Build tree: `build/`. Paths are repo-relative. `$S` is the session scratchpad.

**[RUN]** marks a step that builds, runs ctest, renders or times. The implementing agent runs it, and only after the coordinator confirms that no other build, test or render job is running on the machine.
- Build: `nice -n 19 cmake --build build --config Release -j4`.
- Test: `nice -n 19 ctest --test-dir build … -j4`.
- Renders and timings: `nice -n 19`, run serially.
- Never use a higher `-j`, or a bare `-j`.

**Hard rules**:
- **Never widen a test threshold.** A threshold that would have to be widened is a stop-and-report.
- **Allowlist hygiene**: after each phase, `[RUN] ctest -L architecture`.
  - Delete only lines the check reports as **STALE**, and record each one in DB-011's or DB-005's history.
  - A **new** VIOLATION is a design error: stop and report it. Never allowlist it.
- **No task depends on the merge.** Registry entries record "spec 039, <date>". Issue closure is the maintainer's `Closes #NN` line in the merge commit.
- **Legacy default**: through Phase 8, `CSamplingConfig.backend` defaults to the internal `Legacy` value (today's streams). New-path tests opt in with `Option "sampling" "string sampler" ["sobol"]`, or with `pmj02`/`independent`.
  - Every phase must leave the existing visual, parity and `-slo` suites green **unchanged**: under `Legacy`, no render may change.
- **Red tests that do not compile yet**: add them as `EXCLUDE_FROM_ALL`, and show the red state at TEST-APPROVAL. The implementing task removes the exclusion.
- **Out of scope**: GitHub #55 (JIT optional trace parameters; fixed on its own branch after 039) and spec 040 (GI caches).
  - Interpreter/JIT identity tests use only the plain forms of `trace`/`transmission`/`visibility`/`occlusion`/`indirectdiffuse`, and `gather`.
  - Arena and stats injection (DB-016 follow-up) stays in spec 029.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: can run in parallel (different files, no dependency on unfinished tasks).
- **[Story]**: US1–US6 from spec.md.

---

## Phase 0: Baseline confirmation (measurements M1 and M2 already done)

**Purpose**: confirm the tree the measurements were taken against, and keep the results where implement can find them.

- [X] T001 [RUN] Confirm the baseline is pristine.
  - `git diff --stat c62334d -- src tests CMake CMakeLists.txt shaders examples` must print nothing; only `specs/` may differ.
  - Configure `build/` if it is absent, then build. Record `git rev-parse HEAD` and the build type in a new `specs/039-sampling-foundation/validation.md`.
- [X] T002 Copy the M1 results (identical in every configuration) and the M2 baseline reference (`specs/039-sampling-foundation/tools/perf/m2-baseline-c62334d.txt`) into the "Baselines" section of `validation.md`.
  - Note that `-P` with `-t` disables threads in the children (research §M1, caveat a).
  - Note that SC-010 compares medians of 5 or more interleaved runs.
- [X] T003 [RUN] Run the full suite once on the baseline: `nice -n 19 ctest --test-dir build -j4 --output-on-failure`.
  - Record pass/fail counts and any known-flaky failures in `validation.md` (memory: ImagerRegressionRender, Parity_transparency-matte-aov, Parity_depthdefault, ShadingParity_AmbientAccumulation; re-run each in isolation before writing it off).
  - This is the "unchanged under `Legacy`" reference for Phases 1–8.
- [X] T004 [RUN] Stat every compiled shader against `oshader` (memory: `.slo` staleness): `build/shaders/*.{rslo,slo}` must be newer than `build/src/oshader/oshader`. Record the result.

**PAUSE 0**: report T001–T004.

---

## Phase 1: Setup

- [X] T005 Create `tests/unit/sampling/CMakeLists.txt`.
  - It builds an empty `test_sampling` executable linked to `openrendercommon` (the **shared** library; CLAUDE.md gotcha 22), registered with label `sampling`.
  - Add `add_subdirectory(unit/sampling)` to `tests/CMakeLists.txt`.
- [X] T006 [P] Create the `tests/convergence/` skeleton:
  - `CMakeLists.txt` with an empty `test_convergence` target linked to libtiff and label `convergence`;
  - `README.md`;
  - `reference/.gitkeep`;
  - `add_subdirectory(convergence)` in `tests/CMakeLists.txt`.
  - The label is never included in the default run.
- [X] T007 [P] Add a `determinism-sweep` label placeholder comment to `tests/visual/CMakeLists.txt`, next to `add_depth_determinism_test`. No test is registered yet.
- [X] T008 [RUN] Build, then `ctest -L "sampling|architecture"`. Expected: green, with an empty `sampling` set. Confirm that archcheck does not classify `tests/` paths. If it does, stop and report.

**PAUSE 1**.

---

## Phase 2: User Story 2 — the sampler library (Priority: P1, foundational)

**Goal**: pure 1D/2D draws for three backends, keys, camera slots and `CKeyedStream` in `libRIBCommon` (contracts/sampler-api.md, research §R2–§R3).

**Independent test**: `ctest -L sampling` passes with no renderer involved.

### Tests first

- [X] T009 [P] [US2] `tests/unit/sampling/test_reference.cpp`, test `Sampling_SobolReference` (G5).
  - `sobolUnscrambled(i)` for i = 0..63 must equal the van der Corput sequence (dimension 0) and the Sobol dimension-1 sequence (dimension 1), checked against closed-form expected values written in the test.
- [X] T010 [P] [US2] `tests/unit/sampling/test_range_purity.cpp`, tests `Sampling_Range` and `Sampling_Purity` (G1, G2). For every backend:
  - every value is in [0, 1 − 2⁻²⁴] (`Sampling_Range`; spec FR-004: "MUST never equal 1.0");
  - 8 threads evaluate the same 2¹⁶ (key, dimension, index) triples in shuffled orders, and the results are bit-identical to a serial pass (`Sampling_Purity`).
- [X] T011 [P] [US2] `tests/unit/sampling/test_stratification.cpp` (G3, G4).
  - `Sampling_NetProperty`: for 8 random keys and every 2D `dim` in 0..63, the first 2^m indices form a (0, m, 2)-net for every m ≤ 12. The check counts one point per elementary interval of every shape.
  - `Sampling_PMJ02Property`: the same check for PMJ02 prefixes up to 4096.
  - `Sampling_PMJ02Construction` (`test_pmj02_table.cpp`, amendment): the 64 PMJ02 patterns follow Christensen et al.'s generation strategy, which a randomised Sobol sequence does not: for even k (N = 4^j, k = 0..10), sample N + i lies in the same j-digit cell as sample i and in the diagonally opposite half of it in both x and y; the odd-k locality count is printed for information only, until it is confirmed from the paper. The same checker is run on Sobol points as a negative control and must report violations. Also: all patterns distinct, deterministic, thread-safe on first touch.
- [X] T012 [P] [US2] `tests/unit/sampling/test_uniformity.cpp` (G6).
  - `Sampling_Uniformity`: chi-square with 64 bins over 2¹⁶ draws in each of 16 dimensions, for every backend. The 48 checks are one family, Bonferroni-corrected: each must reach p ≥ 0.01/48, which holds the false-failure chance of the whole test near 1% (an uncorrected p ≥ 0.01 per check would fail a correct generator about 38% of the time over 48 checks).
  - `Sampling_Discrepancy`: the 2D star discrepancy of Sobol and PMJ02 must be below Independent's at N = 64, 256 and 1024.
- [X] T013 [P] [US2] `tests/unit/sampling/test_independence.cpp` (G7, G8).
  - `Sampling_DrawIndependence`: values for (callSite A, occurrence n) stay unchanged when draws are added at other call sites or occurrences. Sub-streams with different call sites must have distinct `seedHash`.
  - `Sampling_Padding`: `dim` up to 2²⁰ and `index` up to 2²⁴ return valid values, with no error. No repeated 2D tuple appears across 1024 dimensions for a fixed key.
  - `Sampling_Composite3D4D` (FR-008): `get3D` = `get2D(d) ⧺ get1D(d+1)` and `get4D` = `get2D(d) ⧺ get2D(d+1)` on one sub-stream. These are deterministic, every component is in range, and the result equals the composition done by hand.
- [X] T014 [P] [US2] `tests/unit/sampling/test_convergence_smooth.cpp`, test `Sampling_ConvergenceSmooth` (G9; SC-003 part a).
  - Integrate f(x, y) = sin(πx)·sin(πy) over [0, 1]² with N = 2^k (k = 2..12), over 32 keys each.
  - Fit the log-log RMSE slope. The default backend must reach ≤ −0.9; Independent is expected near −0.5 and must reach ≤ −0.4.
- [X] T015 [P] [US2] `tests/unit/sampling/test_camera_grid.cpp`, test `Sampling_CameraGridConsistency` (research §R3.1).
  - Run the reyes formula (`absSub = left + j`, with subpixel `left`) and the raytrace formula (`absSub = left·nx − xSampleOffset + col`, with pixel `left`) over a 3×3-bucket grid with margins, `nx = ny = 3`, `xSampleOffset = ySampleOffset = 2`, including negative coordinates at the border.
  - Both must give equal `SampleId`s. `floorDiv` and `floorMod` round toward −∞.
  - Amendment (`test_morton.cpp`, research §R2.1): `Sampling_MortonIndex` checks `mortonIndex(px, py)` against hand-computed values (negative pixels wrap modulo 65536). `Sampling_CameraSobolBlocks` checks that the Sobol camera index is the 64-bit `mortonIndex·2^k + sampleIndex` (2^k the power-of-two ceiling of nx·ny; 65536 pixels per axis is the stated limit), including an 8K frame (7680×4320) at 256 and 4096 spp, where pixel pairs picked to collide under a 32-bit index keep distinct indices and draw different samples, that neighbouring pixels draw disjoint blocks, and that an aligned group of pixels draws a (0, m, 2)-net of film and lens samples and fills 2^m strata of time and importance.
- [X] T016 [P] [US2] `tests/unit/sampling/test_keys.cpp`, test `Sampling_Keys` (data-model SampleKey):
  - `path` never changes when only the camera `sampleIndex` changes;
  - `full` does change;
  - `childKey` derives both;
  - for a cell key, `path == full`;
  - `substream` seeds from `path` when `pixelSampleIndex ≥ 0` and from `full` otherwise, with `indexBase = pixelSampleIndex·N`.
  - `tests/unit/sampling/test_keyed_stream.cpp`, test `Sampling_KeyedStream` (research §R3.10): `CKeyedStream` gives an identical sequence for the same key, `next1D` stays in [0, 1 − 2⁻²⁴] and `nextU32` spans all 32 bits, different keys give different streams (no equal draw at the same position, no linear correlation), and concurrent streams match serial ones.
- [X] T017 [RUN] [US2] **TEST-APPROVAL**: every T009–T016 target fails to compile (`EXCLUDE_FROM_ALL`) or fails. Show it.

### Implementation

- [X] T018 [US2] `src/common/sampling.h`: declare the API from contracts/sampler-api.md:
  - `Backend`, `SampleId`, `SampleKey {full, path, pixelSampleIndex, kind}`, `SubstreamKey`, `Sample2D`, `CSamplingConfig`;
  - `cameraKey`, `cellKey`, `childKey`, `substream`, `get1D`, `get2D`, the inline helpers `get3D`/`get4D` composed from consecutive slots of one sub-stream (FR-008; §4.3's `Sample3D`/`Sample4D`), the camera-slot accessors, `CKeyedStream`, `sobolUnscrambled`.
  - The hot functions are `inline`. There is no mutable global state (rule 1).
- [X] T019 [US2] `src/common/sampling.cpp`: hashing (SplitMix64/PCG-style `mix`), the float conversion `(bits >> 8)·0x1p-24f` (research §R2.4), and the Independent backend (§R2.3).
- [X] T020 [US2] `src/common/sampling.cpp`: Owen-scrambled Sobol (research §R2.1).
  - Camera sequence: 64-bit index `morton(px, py)·2^k + sampleIndex` (`detail::mortonIndex`, 16 bits per axis, so 65536 pixels per axis is the stated limit), with a 64-bit Sobol draw.
  - Nested uniform (Owen) permutation of the index (bits, not base-4 digits), keyed by hash(dim, seed); it keeps aligned index blocks aligned.
  - Dimensions 0 and 1 only, with Burley 2020 hash-based nested-uniform scrambling keyed by hash(dim, seed).
  - Dimensions beyond the native tables are padded by hashing (FR-006).
- [X] T021 [US2] `src/common/samplingPmj02.cpp`: deterministic PMJ02 generation (research §R2.2).
  - 64 patterns × 4096 points, generated under `std::call_once` from a fixed internal seed.
  - Each pattern is a genuine progressive multi-jittered (0,2) sequence, built by doubling with the diagonal strategy of Christensen et al. 2018 and the elementary-interval search of Pharr 2019; one `call_once` per pattern.
  - Pattern chosen by hash(dim, seed); the index is shuffled within each dyadic block [2^t, 2^(t+1)) so that every prefix maps onto itself; Owen scramble.
  - Index ≥ 4096 falls back to scrambled Sobol for that draw.
- [X] T022 [US2] `src/common/sampling.cpp`: the camera slots.
  - Film2D = 0, Lens2D = 1, Time1D = 2, Importance1D = 3 (data-model "Dimension cursor").
  - `floorDiv`/`floorMod` helpers.
  - `CKeyedStream` (key plus a local counter, never stored beyond its frame, research §R3.10).
  - The test-only `sobolUnscrambled`.
- [X] T023 [US2] Add `sampling.cpp` and `samplingPmj02.cpp` to both the shared and static `libRIBCommon` targets in `src/common/CMakeLists.txt`, and install `sampling.h` if the other common headers are installed.
- [X] T024 [US2] Remove `EXCLUDE_FROM_ALL` from T009–T016.
- [X] T025 [RUN] [US2] Build, then `ctest -L "sampling|common|architecture"`. All must be green, with no new allowlist line.

**PAUSE 2**: report the measured slopes and discrepancies from T012 and T014.

---

## Phase 3: User Story 5 — scene options and config injection (Priority: P3, needed early)

**Goal**: `Option "sampling"` through all layers, and `CSamplingConfig` reaching every shading context, while the default stays `Legacy` (contracts/rib-options.md, research §R4).

### Tests first

- [X] T026 [P] [US5] `tests/unit/rib_parse/test_sampling_option.cpp` (new executable beside the existing one), test `SamplingOption_Parse`:
  - `"sampler" ["pmj02"]`, `["independent"]` and `["sobol"]` set the backend;
  - `"seed" [-7]` is stored as uint32 4294967289;
  - with no option set, the internal default is `Legacy` (until Phase 9).
- [X] T027 [P] [US5] Same file, test `SamplingOption_UnknownName`: `["foo"]` prints a diagnostic containing `Unknown sampler "foo"` and `sobol, pmj02, independent`, at `CODE_BADTOKEN` (never `CODE_LOG`), and the render continues with the default.
- [X] T028 [P] [US5] `tests/unit/rib_parse/test_sampling_option_roundtrip.cpp` (in-process; gotcha 20: reset the lexer with `riblex_destroy()` between sessions), test `SamplingOption_RibRoundTrip`: a RIB with both tokens goes through RIB output (`libRIBOut`) and back. The re-parsed options are equal (FR-027).
- [X] T029 [P] [US5] `tests/unit/common/`, test `Common_VocabularyConsistency` (existing): no change to the test itself. It must fail if the new tokens appear in fewer than all three of `ri.h`, `riVocabulary.h` and `riTokens.cpp`. Show this by temporarily adding a token to one file only, then revert.
- [X] T030 [RUN] [US5] **TEST-APPROVAL**.

### Implementation

- [X] T031 [US5] Add the tokens `RI_SAMPLING`, `RI_SAMPLER` and `RI_SEED` (only those not already present) to `src/ri/parse/ri.h`, `src/common/riVocabulary.h` and `src/common/riTokens.cpp` (CLAUDE.md gotcha 22).
- [X] T032 [US5] Pre-declare `"sampler"` as `string` and `"seed"` as `int` under the `"sampling"` option in `initDeclarations()` (`src/ri/render/rendererDeclarations.cpp`).
- [X] T033 [US5] `src/ri/state/options.{h,cpp}`: add `samplingBackend` (enum, default `Legacy` for now) and `samplingSeed` (uint32, default 0). Make them queryable through `COptions::find()` and RSL `option()`.
- [X] T034 [US5] `RiOptionV()` in `src/ri/render/rendererContext.cpp`: parse both tokens. An unknown name gets the T027 diagnostic through `CODE_BADTOKEN`, and the default is kept.
- [X] T035 [US5] `src/ri/output/ribOut.cpp`: declare and emit both tokens (FR-027).
- [X] T036 [US5] Renderer core: build an immutable `CSamplingConfig {backend, seed, frame, nx, ny, jitter}` in `CRenderer::beginFrame` (`src/ri/render/renderer.cpp`), and pass it to every shading context the core creates. Add a `CShadingContext` member, set by the core (constructor argument or setter).
  - **No hider or shading file may read a new `CRenderer::` symbol** (gate).
- [X] T037 [RUN] [US5] Build, then:
  - `ctest -L "common|architecture"` and the `SamplingOption_*` tests;
  - **the full visual suite, which must be unchanged against T003** (the default is still `Legacy`).

- [X] T037a [US5] **Tests first, TEST-APPROVAL** for the RSL half of T033 (added after Phase 3): RSL `option("sampling:sampler", s)` (string) and `option("sampling:seed", n)` (the signed integer, as a float).
  - `tests/unit/rib_parse/test_sampling_option_rsl.cpp`: `SamplingOption_RslQuery` (defaults, set values, unknown names, an existing option as a control) and `SamplingOption_RslQueryNoServices` (no services installed on the shading state: every name is "not found"), on a real shading context through `CShadingContext::queryOption()`, the call both the interpreter and the JIT make.
  - `shaders/sampling_option_probe.sl`, `examples/rib/tests/sampling-option-{pmj02,default}-{rslo,slo}.rib` and the exact pixel read-back driver `tests/visual/test_pixel_probe.cpp`: `SamplingOption_RslQuery_{pmj02,default}_{rslo,slo}` render the probe under the interpreter and the JIT and check the centre pixel as numbers (R found/"pmj02", G found, B the shifted seed).
- [X] T037b [US5] Add `virtual samplingSampler()` and `samplingSeed()` to `CRendererServices` (`libshader/shading/rendererServices.h`), implement it in `CRendererServicesImpl` from `COptions`/`CRenderer`'s frame copy, and answer `"sampling:sampler"` and `"sampling:seed"` in `CShadingContext::options()`. No new `CRenderer::` reference from `src/libshader` or the hiders.
- [X] T037c [RUN] [US5] Full suite against the 618 baseline plus the new tests; `ctest -L architecture` green with no new allowlist line. **PAUSE** before Phase 4.

**PAUSE 3**.

---

## Phase 4: User Story 6 — identity plumbing (Priority: P3, foundational for US1/US3)

**Goal**:
- stable object identity;
- `CRay` key and depth;
- per-point keys, batch keys and occurrence counters in `CShadingState`;
- compiler call-site ordinals on both backends;
- `.rslo` version bump and `op_*` renames.

All of it is inert under `Legacy` (research §R3, contracts/shading-runtime.md).

### Tests first

- [X] T038 [P] [US6] `tests/unit/sampling/test_object_ids.cpp` (with `scene_probe.h`), test `Sampling_ObjectIds` (registered as five cases, `Sampling_ObjectIds_{scene_loads,instances,delayed,tessellation,splits}`, one Ri session per process). Through `RiBegin(RI_NULL)` in-process (gotcha 20: reset the lexer with `riblex_destroy()` between sessions):
  - two scene loads give identical `stableId`s;
  - two `ObjectInstance`s of one master differ;
  - `DelayedReadArchive` children equal `mix(parent, k)`;
  - tessellation children equal `mix(parent, childIndex)`;
  - split pieces inherit their parent's ID.
- [X] T039 [P] [US6] `src/libshader/tests/test_call_sites.cpp`, test `LibShader_CallSiteOrdinals` (C1, C3): compile one shader with two `random()` calls in a loop, a `trace`, a `gather` and an `illuminance` both to `.rslo` and with `--jit` to `.slo`. The call-site ordinal lists must be identical.
- [X] T040 [P] [US6] Same file, test `LibShader_RandomNotPure` (C2): after the IR passes, two textually identical `random()` calls are still two calls, and a `random()` inside a loop is not hoisted by `UniformLifting`.
- [X] T041 [P] [US6] `src/libshader/tests/test_stale_shaders.cpp` (a second source file and executable: it needs the renderer and the JIT, which the compiler-only T039/T040 executable must not link), test `LibShader_StaleShaderRejected` (the `.rslo` half) and `LibShader_StaleShaderRejectedJit` (the `.slo` half) (research §R3.6):
  - a `.rslo` with the previous format version is rejected with a diagnostic;
  - a `.slo` referencing the old `op_random_f` name fails at bind time with an unresolved-symbol diagnostic. The fixtures are generated by the test, not checked in.
- [X] T042 [P] [US6] `tests/unit/sampling/test_cell_keys.cpp`, test `Sampling_CellKeyUniqueness` (registered per family, `Sampling_CellKeyUniqueness_{patch,mesh,quadric,nurbs,subdivision,points,curves}`) (research §R3.3, V2). The probe is the in-process test hook `CShadingContext::pointKeyObserver` rather than an AOV (see validation.md). For each grid-shaded primitive family (patch, polygon mesh, quadric, NURBS, subdivision, points, curves), render a small scene under `sobol`, and read the keys through the test hook `CShadingContext::pointKeyObserver` (null in production). (The wording "a probe shader that writes its point key to an AOV" is superseded: see validation.md.) No two shading points of one object may share a key.
- [X] T043 [RUN] [US6] **TEST-APPROVAL**.

### Implementation

- [X] T044 [US6] `src/ri/geometry/object.h`: add `uint64_t stableId` to `CObject`, with a default constructed from the parent where one is given.
- [X] T045 [US6] `src/ri/render/rendererContext.cpp`: an ID-scope stack. The root scope is `hash(frame)`, and `addObject` assigns `mix(scope, ordinal++)`.
  - `processDelayedObject`/`processDelayedInstance` push `(parent.stableId, 0)`. They are serialized by `delayedMutex`, which was verified in research §R3.2.
  - Each instancing call gets its own ordinal, and its copies get `mix(instanceId, k)`.
  - Never derive an ID from `CRenderer::getGlobalID` or from render-time creation order (FR-016).
- [X] T046 [US6] Geometry child IDs:
  - `setChildren` and mesh tessellation children: `mix(parent, childIndex)`;
  - split pieces inherit;
  - CSG leaves captured through `CGeometryHost`: `mix(parent, k)`.
  - Files: `src/ri/geometry/*.cpp` and `src/ri/render/geometryDispatch.cpp`. For `cluster()` dummy nodes, use `mix(parent, 0 | 1)`.
- [X] T047 [US6] `src/ri/geometry/points.cpp`: `CPoints::sample()` writes the per-vertex `elementId = start + i` (research §R3.3). The cell key reads it where it is present.
- [X] T048 [US6] `src/ri/core/ray.h`: add `SampleKey key` and `uint16_t depth` to `CRay` (Infra).
- [X] T049 [US6] `src/libshader/shading/shading.h`: add to `CShadingState` the members `pointKeys[numVertices]`, `batchKey`, `callSiteOccurrence` (lazily allocated in `threadMemory` and reset per invocation) and `currentShaderKey` (data-model "Per-point shading identity").
- [X] T050 [US6] `src/libshader/shading/shading.cpp` `shade()`: for grid points, fill `pointKeys` with `cellKey(object->stableId, u, v, elementId, frame, seed)` after `object->sample()`, and set `batchKey` from the grid origin (research §R3.7).
  - Read `object->stableId` inline only: it is an existing include, and no new symbol may appear (gate).
  - Derivative-offset tail vertices copy their real vertex's key (research §R3.10a).
- [X] T051 [US6] `src/ri/hiders/trace.cpp` shading-group loop: copy `ray->key` into `pointKeys` before `object->shade()`, and set `batchKey = hash(bucketX, bucketY, stableId, depth, chunkIndex)`. Nested shading derives from the parent `batchKey` (research §R3.7).
- [X] T052 [US6] Compiler (`src/libshader/compiler/`): assign `callSite` ordinals on the final IR (C1) and keep `random`/`urandom` non-pure (C2).
  - Emit the ordinal as an extra `.rslo` operand, and **bump the `.rslo` format version** (I1).
- [X] T053 [US6] Runtime loader (`src/libshader/runtime/`): read the new operand, and reject older-version `.rslo` files with a diagnostic (T041).
- [X] T054 [US6] JIT ABI (`src/libshader/shading/rslOps.{h,cpp}`, `src/libshader/compiler/llvmEmitter.cpp`):
  - add a `uint32_t callSite` argument to `op_random_f/v` and to the trace-family, gather, occlusion, indirectdiffuse and texture-family entries;
  - **rename each changed entry with a `_v2` suffix** (J1);
  - the emitter passes the ordinal as a constant.
  - Under `Legacy`, the new argument is ignored and the values stay unchanged.
- [X] T055 [US6] Remove `EXCLUDE_FROM_ALL` from T038–T042.
- [X] T056 [RUN] [US6] Build, which rebuilds every shader. Then:
  - T004's stat check;
  - `ctest -L "sampling|libshader|common|architecture"`;
  - **the full visual and `-slo` suites, which must be unchanged against T003**. This also proves the `_v2` symbols resolve at bind time (gotcha 3). If one does not resolve, wire `addProcessSymbol()` and report it.

**PAUSE 4**.

### Phase 4 follow-up: format versioning (maintainer decision; Phase 4 is committed as 659c770)

The call-site operand and the `_v2` ABI make Phase 4 a MAJOR change of both shader formats, so `.rslo` goes to 2.0.0 (not 1.1.0) and `.slo` gets its own version, 2.0.0. Rule (documented in `src/common/rsloFormat.h`, `contracts/shading-runtime.md`, `research.md`): MAJOR = an older or newer loader cannot read the file; MINOR = additive, a loader accepts `file.major == supported.major && file.minor <= supported.minor`; PATCH = never checked.

- [X] T056a [P] [US6] `src/libshader/tests/test_format_versions.cpp` (ctest `LibShader_Format_*`, nine entries), `src/common/sloFormat.h` (the constants, so the test compiles): old/newer `.rslo` refused with a diagnostic naming the file's version, the supported version and `oshader`; a lower-minor `.rslo` loads (reported as skipped while the supported minor is 0); the current `.rslo` loads; `oshader --jit` writes the `openrender.slo.format` node (three i32 operands); a `.slo` with format 1.0.x, with no node (counts as 1.0.0), or with a newer major or minor is refused before linking with a diagnostic naming both versions and `oshader --jit`; the current `.slo` loads. `test_stale_shaders.cpp` is untouched.
- [X] T056b [RUN] [US6] **TEST-APPROVAL** (approved, committed as adc32fd).
- [X] T056c [US6] Implement the rule: `src/common/rsloFormat.h` (2.0.0, rule comment), both `rslo.l` (major equal, minor <=, message naming file and supported versions and saying to recompile with oshader), `src/common/sloFormat.h`, the `openrender.slo.format` node in `llvmEmitter.cpp`, the check in the JIT loader before add/link (a missing node is 1.0.0; `error(CODE_BADFILE, ...)` naming shader, file, both versions, `oshader --jit`), real parsing wherever `std::stoi` of the version string remains; docs (contract, research).
- [X] T056d [RUN] [US6] Regenerate the 108 `tests/shaders/references/*.rslo`; prove the only differences against 659c770 are the 108 `#!version` lines (1.1.0 to 2.0.0); full suite and `-L architecture`.

**PAUSE 4b**.

---

## Phase 5: User Story 3 — correlated camera samples (Priority: P2)

**Goal**: both hiders draw film, lens, time and importance from the same absolute-grid `SampleId` under the new backends. The zbuffer hider is unchanged (FR-009, FR-017, research §R3.1).

### Tests first

- [X] T057 [P] [US3] `tests/unit/sampling/test_camera_slots.cpp`, test `Sampling_CameraSlotsEqual`: the reyes and raytrace `CSampler` paths (driven through a test seam on `CSampler`) produce bit-identical film, lens, time and importance values for every subpixel of a bucket, margins included, for each new backend.
  - The seam (declared in `src/ri/hiders/sampler.h`, stub bodies until T060): `CSampler::reyesAbsSub(origin, index)`, `CSampler::raytraceAbsSub(originPixels, samplesPerPixel, sampleOffset, index)` (the two unit conversions) and `CSampler::cameraSample(config, absSubX, absSubY, wantLens)`; `CSampleValue` gains a `key` (the camera key). T060-T062 make the hiders call them.
  - Besides path equality it checks the values against the slots computed independently from `common/sampling.h` (jitter mapping, time stratum `row*nx+col`, transposed importance stratum `col*ny+row`, camera key, lens), for Sobol, PMJ02 and Independent, jitter 0, 0.37 and 1, two grids, with and without lens.
- [X] T058 [P] [US3] `tests/visual/CMakeLists.txt`, test `CameraSamples_JitterZeroUnchanged` (registered as `_reyes` and `_raytrace`; driver `tests/visual/test_render_compare.cpp`; plus the control `CameraSamples_SobolEngaged_{reyes,raytrace}`: with `jitter 1` the sobol render must differ from the legacy one, so the option cannot be silently ignored): under `sobol`, every `"jitter" [0]` scene's film, time and importance positions equal today's. The test renders `color-determinism-opaque-reyes.rib` and `depth-analytic-min-raytrace.rib` with `--prepend` (T079 adds that flag later), so for now it runs a hand-written copy with the option line, comparing bit for bit with the `Legacy` render.
- [X] T059 [RUN] [US3] **TEST-APPROVAL** (approved with the concentric lens mapping; committed as cec5e0d).

### Implementation

- [X] T060 [US3] `src/ri/hiders/sampler.{h,cpp}`: add the shared absolute-subpixel `SampleId` function. Keep `jitter·(u−0.5)+0.5001011` for film, time and importance; only `u` comes from the camera slots. Lens comes from the lens slot through `sampleDisk()`.
  - Under `Legacy`, behaviour is unchanged.
- [X] T061 [US3] `src/ri/hiders/stochastic.{h,cpp}`: under the new backends, compute `absSub = left + j / top + i`, and take every camera value from the slots. Do not use `apertureGenerator` on the new path.
- [X] T062 [US3] `src/ri/hiders/raytracer.cpp` `sample()` and `computeSamples()`: under the new backends, compute `absSub = left·nx − xSampleOffset + (i+x)` (and likewise for y), using the same stratum index and transposed importance index as reyes.
  - Put the camera importance slot in `cRay->jimp`, and the `SampleKey` and depth 0 in `cRay->key`.
- [X] T063 [US3] `src/ri/hiders/trace.cpp:304`: under the new backends, do not overwrite `jimp` for a ray that carries a camera importance. Secondary rays draw `jimp` from their child sub-stream.
- [X] T064 [RUN] [US3] Build, then:
  - T057/T058;
  - `ctest -L visual`, which must be unchanged under `Legacy`;
  - every parity test once, by hand, with `sobol` prepended: report the diffs (no threshold change yet).

**PAUSE 5**.

---

## Phase 6: User Story 1/6 — transport and other consumers (Priority: P1)

**Goal**: every randomness site in research §R1 draws from the sampler or a `CKeyedStream` under the new backends (FR-010, FR-018–FR-024).

### Tests first

- [X] T065 [P] [US1] `tests/shading_parity/` (or `tests/visual/`), tests `SamplingJitParity_*` (SC-008). A probe shader writes to AOVs:
  - `random()` and `urandom()`, uniform and varying, called in a loop;
  - the sample directions of the **plain** forms of `trace`, `transmission`, `visibility`, `occlusion` and `indirectdiffuse`, and of `gather`.
  - The `.rslo` and `.slo` renders under `sobol` must be bit-identical.
  - The shader contains no `if`-gated uniform assignment (GitHub #8) and no optional parameters (GitHub #55).
  - `Du(random())` must be 0 (research §R3.10a).
  - Successive `random()` calls at one point (two call sites, and two iterations of one loop) return distinct values (FR-019).
  - **As written** (validation.md "Phase 6 tests"): the probe output is the colour of the pixel, not AOVs (an AOV needs a declared display channel; one shader, `shaders/sampling_jit_probe.sl`, picks one value per scene with a uniform `channel`, generated from `examples/rib/tests/sampling-jit-probe.rib.in`); `Du(random())`, distinctness and range are in `shaders/sampling_random_probe.sl` (`SamplingJitParity_RandomProbe_{rslo,slo}`). `random()` cannot be assigned to a uniform variable (compile error), so there is no uniform draw to probe from source. Added: `SamplingJitDeterminism_*` (a scene rendered twice is bit-identical) and `SamplingJitEngaged_*` (seed 0 against seed 1 must differ).
- [X] T066 [P] [US6] `tests/unit/sampling/`, test `Sampling_ShaderDrawIndependence` (US6-1): a shader with two `trace()` loops and an `illuminance()` loop. Adding a draw to the first loop leaves the AOVs of the second loop and of `illuminance` bit-identical.
- [X] T067 [P] [US1] Write the SC-007 scan, `tests/visual/check_no_global_randomness.sh`, implementing the grep from quickstart §9 (it excludes `src/precomp/` and comments; the pattern also matches uses of the context members `random2d`/`random3d`/`random4d`), with an explicit allowlist `tests/visual/no_global_randomness_allowlist.txt` (one `path | reason | owner` line per file). The scan fails on a match in a file that is not on the allowlist and on a stale allowlist line (a file with no match any more), as archcheck does. **Registered now** as `Sampling_NoGlobalRandomness` (label `sampling`), and red today: it lists the call sites Phase 6 converts. The allowlist holds only the legacy generators that Phase 9 deletes (T090-T093, T095); nothing Phase 6 converts is on it. It goes green when T070-T077 are done and the allowlist is shrunk to match, and T104 empties it.
- [X] T068 [P] [US1] Using the existing `test_depth_determinism` with extra arguments, tests `Sampling_ThreadSweep_{quadlight,sphere-random,sphere-trace,sphere-gather}`: under `sobol`, `-t:1` against `-t:4` must be bit-identical. These fail under today's per-context streams.
  - Use a hand-written copy with the option line added **and the scene's `Option "limits" "numthreads" [1]` pin removed**. Otherwise `-t:4` would still render single-threaded and the test would pass vacuously.
  - Also add `Sampling_ProbeIndependence`, covering the midpoint-probe edge case. A raytrace scene with a shaded stochastic surface (a copy of `examples/rib/tests/depth-analytic-mid-raytrace.rib`, plus a `random()`-based surface shader), rendered under `sobol` once with `depthfilter midpoint` and once with `min`, gives a bit-identical colour output. The midpoint probe's draw no longer shifts any other value.
- [X] T069 [RUN] [US1] **TEST-APPROVAL** (approved and committed as 059c975, with the SC-007 scan and its allowlist as 37f3f75).

### Implementation

- [X] T070 [US6] `src/common/random.{h,cpp}`: add `sampleHemisphere`, `sampleCosineHemisphere` and `sampleSphere` overloads that take `Sample2D`/`Sample3D` values. Keep the `CSobol&` overloads until Phase 9.
- [X] T071 [US6] `src/libshader/shading/executeMisc.cpp` (the `sampleRay` macro, transmission and cone sampling, motion time), `shading.cpp` (gather ray setup, `jitTraceBatch`, `jitOcclusionBatch`) and `shaderPl.cpp`:
  - under the new backends, draw from `substream(pointKey, shaderKey, callSite, occurrence, depth, N)` with the slot layout `0 = origin jitter`, `1 = direction`, `2 = time`;
  - spawned rays get `childKey`, and a secondary ray's `jimp` is drawn from its child sub-stream (`trace.cpp`, the `urand()` that T063 left for non-camera rays; `Sampling_NoGlobalRandomness` lists it).
- [X] T072 [US6] `src/libshader/shading/scriptFunctions.h` (`RANDOMFEXP` and the like) and `rslOps.cpp` (`op_random_f_v2`/`op_random_v_v2`):
  - varying draws use `pointKeys[i]`, and uniform draws use `batchKey`;
  - `occurrence` is incremented for active real lanes only;
  - tail vertices mirror their real vertex.
- [X] T073 [US6] `src/ri/texture/texture.cpp`: the stochastic `lookup` and `shadow` paths at lines ~1403, 1563, 1578, 1673, 1853 and 2141 take a `SubstreamKey` from Shading instead of reading `context->random2d`/`random4d` (contract T1). Update the callers in Shading.
  - **As built**: the lookups draw from the context's draw cursor (`drawNext2()`/`drawNext4()`), which Shading binds to the lookup's sub-stream (`DRAW_BIND`, `bindDraws()`) before every lookup, in the interpreter macros and in the JIT entries. No `CTexture`/`CEnvironment` signature changes. See contract T1.
- [X] T074 [P] [US6] `src/ri/hcshader.cpp`: draw light samples from transport sub-streams keyed by (pointKey, light ordinal, sample index).
- [X] T075 [P] [US6] Photons (as built: `CJob::firstPhoton` is stamped by `dispatchPhoton`; each chunk gives its photons keys `cellKey(hash(light ordinal), index)` and resets the occurrence counters; emission and every bounce draw from the photon's sub-stream through the draw cursor, dimension by dimension in event order, instead of `childKey(key, bounce)`; `gen2`/`gen3`/`gen4` remain only on the Legacy path):
  - the core's job dispatcher stamps `firstPhotonIndex` on every `PHOTON_BUNDLE` (`src/ri/render/renderer.cpp` or wherever `dispatchJob` lives);
  - `src/ri/hiders/photon.{h,cpp}` keys emission by `hash(seed, frame, lightOrdinal, firstPhotonIndex + k)`, and bounce decisions by `childKey(key, bounce)`;
  - `gen2`/`gen3`/`gen4` are unused on the new path.
- [X] T076 [P] [US6] `src/ri/hiders/irradianceDispatch.cpp` (lines ~319–322, 420–423, 656) and `src/ri/hiders/radiance.{h,cpp}` (lines ~1070–1285, plus the `generator` member): use transport sub-streams and `CKeyedStream`. The caches stay order-dependent (spec 040).
- [X] T077 [P] [US1] `CKeyedStream` sites:
  - `src/ri/render/geometryDispatch.cpp` `cluster()` (keyed by `stableId` and iteration), and lines ~640–645 (keyed by `stableId`);
  - `src/ri/geometry/object.cpp` `getDisp` (constant key);
  - `src/ri/texture/pointHierarchy.cpp` (node index);
  - `src/ri/render/init.cpp` (`#define urand()`, keyed by shader and call site).
  - These do not depend on the backend: the keyed path is used under `Legacy` too **only if** T003's suite stays unchanged. Otherwise gate it on the backend and record why.
- [X] T078 [RUN] [US1] Build, then:
  - T065, T066 and T068 (T067 is not registered yet);
  - `ctest -L "visual|libshader|sampling|architecture"`, which must be unchanged under `Legacy`;
  - stat the shaders again (T004).

**PAUSE 6**.

---

## Phase 7: User Story 1 — determinism harness (Priority: P1)

**Goal**: cross-configuration bit-identity over every non-GI scene (FR-030, contracts/test-harnesses.md).

### Tests first

- [X] T078a [US1] Self-tests of the harness, `DeterminismHarness_*` (label `determinism`; `tests/visual/check_determinism_harness.sh`, scenes `determinism-harness-noisy.rib` and `determinism-harness-two-outputs.rib`), written against the interface T079 must provide: `identical` (a thread independent scene under sobol with `--no-dither` gives exit 0), `detects` (a Legacy random scene without a pin gives exit 1 and lists between 1 and 5 `pixel (x, y)` lines per output), `render_failure` (exit 2), `multiple_outputs` (two displays, each named in the report), `print_prepend`, `print_keep_pins`, `print_no_dither` (a new `--print-scene` flag prints the prepared copy and exits 0 without rendering, so the prepend, the pin removal and the Quantize line can be checked), `usage_unchanged` (the original usage still works; green today).
- [X] T078b [RUN] [US1] **TEST-APPROVAL** (approved with `--print-scene` and the `pixel (x, y)` report format; committed as 46e5c76).

### Implementation

- [X] T079 [US1] `tests/visual/test_depth_determinism.cpp`: add `--configs "<opts>|…"` (an empty entry means the default thread count), `--outputs a.tif,…` (every display decoded through libtiff, at any depth, `z` included) and `--prepend "<RIB line>"` (writes a temporary copy of the scene, keeping its archive paths valid).
  - The `--prepend` copy also **drops every `Option "limits" "numthreads"` line**, so pinned non-GI scenes really run multithreaded (`--keep-pins` disables this, for GI scenes only).
  - Add `--no-dither`, which prepends `Quantize "rgba" 255 0 255 0` (dither amplitude 0). Until T095 replaces libc `rand()`, 8-bit dithered outputs differ between configurations for a reason this phase does not fix. A scene that sets its own non-zero-dither `Quantize` is listed for the Phase 9 run (T103) only.
  - Compare bit for bit with the first configuration's outputs, and report up to 5 differing pixels per output.
  - Exit codes: 0 identical, 1 different, 2 render or decode failure.
  - The existing usage stays unchanged.
- [X] T080 [US1] `tests/visual/CMakeLists.txt`: add `add_determinism_sweep(<name> <rib> <outputs…>)`, with default configurations `-t:1||-t:2|-t:4|-P:2`. The `-P` entry never carries `-t` (research §M1, caveat a). The function prepends `Option "sampling" "string sampler" ["sobol"]`, and passes `--no-dither` until Phase 9 (T103 removes it).
- [X] T081 [US1] Create `tests/visual/determinism-scenes.cmake`, listing every non-GI visual and parity scene with all of its outputs (FR-031).
  - GI scenes (the `occlusion()`/`indirectdiffuse()` scenes under the default `maxerror`, and photon-map scenes) appear only as comments pointing to spec 040.
  - Include it from `tests/visual/CMakeLists.txt`.
- [X] T082 [US1] Give `DepthFilter_Determinism_*` and `ColorDeterminism_*` an added `-t:1|-t:2|-t:4` cross-configuration registration. Keep the existing registrations.
- [X] T083 [US1] Convert T058 and T068 to `--prepend`, and delete their hand-written copies.
  - **As built**: `test_render_compare` gained `--prepend-a:`/`--prepend-b:` for T058; T068 uses `test_depth_determinism --configs` with a two-line prepend (the sobol line and a float `Quantize`). See validation.md "Phase 7 implementation".
- [ ] T084 [RUN] [US1] (run; **not met**: 19 of 413 scenes differ, 18 in a second run, 24 distinct; 7.0 min; classification in validation.md) Build, then `ctest -L determinism-sweep -j4`. Every scene must be identical. Record the wall-clock time in `validation.md` (M3).
  - Apply the 10-minute rule (FR-030): if it takes more than 10 minutes, the sweep keeps only the `determinism-sweep` label, and a representative subset (the existing tests plus one scene per primitive and effect family) also gets `determinism`.
  - Any failing scene is a stop-and-report, classified by cause.
- [X] T084a [RUN] [US1] Root-cause group C of T084 (diagnosis only; no fix). Group C: the sweep scenes that differ only with jitter on, a few values each, intermittent at one thread count.
  - Leading hypothesis to confirm or reject: fragments at equal depth, or straddling sample edges, resolved in thread or bucket order, as in #48.
  - **Result** (evidence in validation.md "T084a"): two causes, neither in the sampler.
    - **Reyes scenes** (`blobby-*`, `bunny-reyes`): the hypothesis is confirmed in its arrival-order form. Two opaque fragments of different colour reach one sample at *exactly* equal depth, and `drawPixel()` keeps the first (`z < pixel->z`), so the grid that is rasterized first wins.
    - **Raytrace scenes** (`csg-hero-*-raytrace` and related): the hypothesis is rejected. The cause is a read of uninitialised `ralloc` memory in the derivative of the `transmission()` target, which makes the shadow ray garbage for some silhouette samples. Filling `ralloc` memory with any byte removes the nondeterminism. The same defect also explains most of the scenes disabled under #61.
  - If a fix is needed it is test-first: write the tests, show them failing, and wait for TEST-APPROVAL before any implementation. The C scenes stay registered and red.
- [ ] T084b [US1] Cause 1 of T084a: a missed derivative lane (GitHub #61). **Tests written and red; waiting for TEST-APPROVAL (T084b-approve).**
  - [X] T084b-tests: `tests/shading_parity/test_derivative_inactive_lanes.cpp`, test `ShadingParity_DerivativeInactiveLanes`. It fills the inactive offset lanes of a ralloc'd source array with two different bytes (0x00, 0x5A) and requires, for `duFloat`/`dvFloat`/`duVector`/`dvVector` (through the public `jitDu*`/`jitDv*` entry points the interpreter shares):
    - the whole result array is bit-identical between the two fills;
    - a real lane whose offset lane is inactive has derivative 0;
    - lanes with an active offset lane keep the plain difference quotient (the control).
    Red today: 8 of 18 checks fail, the control passes.
  - [ ] T084b-approve [RUN] **TEST-APPROVAL**.
  - [ ] T084b-fix: in `src/libshader/shading/executeMisc.cpp`, give a tail lane that the lane tags mark inactive the value of its real lane (derivative 0) in `duFloat`/`dvFloat`/`duVector`/`dvVector` and their capital-letter forms.
  - [ ] T084b-rerun [RUN]: repeat runs as in T084a on the cause 1 scenes, with a temporary `ralloc` fill byte as the proof, and the sweep entries.
  - [ ] T084b-reenable: re-enable each #61-disabled sweep entry the fix makes reliably green. Check `csg-hero-stats-1600-reyes` and `csg-procedural-operand-raytrace` too, and keep them disabled unless they are shown fixed. Update the exclusion lists in spec.md and validation.md to match.
- [ ] T084c [US1] Cause 2 of T084a: equal-depth fragments resolved by arrival order. **Investigation done, tests written and red; waiting for TEST-APPROVAL (T084c-approve).**
  - [X] T084c-why: why the two z values are exactly equal. Result in validation.md "T084b and T084c": not duplicate or overlapping geometry; a degenerate micropolygon (a triangle corner, v0 == v2) gets the depth of its apex vertex, so two such cells at the same mesh vertex tie exactly. Reported for a decision before the fix.
  - [X] T084c-tests: `examples/rib/tests/ties-coincident-{reyes,zbuffer}.rib`, tests `DeterminismTies_{reyes,zbuffer}`: two coincident opaque triangles of different colour, 8x8 buckets, `-t:1` against eight `-t:4` renders, bit-identical. Red today under both hiders (about 7000 of 65536 values differ in every comparison).
  - [ ] T084c-approve [RUN] **TEST-APPROVAL**.
  - [ ] T084c-fix: break equal-z ties by a deterministic key (stableId, then micropolygon index) instead of arrival order, in the shared path so reyes and zbuffer use the same code (the #50 rule).
  - [ ] T084c-measure [RUN]: measure and report which existing reference tests move at tie pixels, and by how much. No threshold or reference is touched without a separate approval.
  - [ ] T084c-recheck [RUN]: re-check `csg-hero-displaced-reyes` and the three scenes that never reproduced (`blobby-blend-far-reyes`, `csg-offscreen-stats-reyes`, `csg-hero-partial-stats-reyes`). They stay registered and red.

**PAUSE 7**.

---

## Phase 8: User Story 4 — convergence harness (Priority: P2)

**Goal**: RMSE against sample count, and the SC-003 part b bars (FR-033, contracts/test-harnesses.md).

- [ ] T085 [US4] `tests/convergence/test_convergence.cpp`.
  - It substitutes `@SAMPLER@`, `@SPP@` and `@SAMPLES@`, and renders the ladder 1, 4, 16, 64, 256.
  - It computes RMSE against the reference over all channels, and fits the log-log slope by least squares.
  - It prints the scene × sampler × rung table. `--bar render` checks for 0.15 steeper than Independent with top-rung RMSE no higher; `--bar nonworse` checks for slope and top-rung RMSE no worse.
- [ ] T086 [P] [US4] Scene templates in `tests/convergence/scenes/`, each 64×64 with float output:
  - `dof-reyes.rib.in` and `dof-raytrace.rib.in` (`@SPP@`, lens);
  - `motion-reyes.rib.in` (`@SPP@`, time; bar `nonworse`);
  - `arealight-raytrace.rib.in` (quadlight-style, `@SAMPLES@`);
  - `occlusion-raytrace.rib.in` (`Attribute "irradiance" "maxerror" [0]`, so no cache; `@SAMPLES@`);
  - `glossy-raytrace.rib.in` (`trace` with `samples`/`samplecone`, interpreter only because of GitHub #55; `@SAMPLES@`).
  - Never vary `PixelSamples` for shading-side integrals under reyes.
- [ ] T087 [US4] `tests/convergence/CMakeLists.txt`:
  - one `Convergence_<scene>` test per template, label `convergence`, with bar `render` except `motion-reyes`, which uses `nonworse`;
  - a `convergence-reference` custom target that is **not** in ctest, rendering each scene with `independent` at 4096 spp or more into `tests/convergence/reference/`.
- [ ] T088 [RUN] [US4] Build `convergence-reference`, serially under `nice -n 19`. Record each scene's time in `validation.md` (M4). Any scene over 30 minutes drops to 32×32, not to fewer samples.
  - Check in the references and record the exact generation command in `tests/convergence/README.md`.
- [ ] T089 [RUN] [US4] `ctest -L convergence`. Record the table and slopes in `validation.md`. A failed bar is a stop-and-report; never loosen the bar.

**PAUSE 8**.

---

## Phase 9: Flip, cleanup and verification (cross-cutting; US1, US3, US5)

**Purpose**: make the new path the default, delete the legacy machinery, re-render the references once, remove the pins, tighten the thresholds, and verify SC-001–SC-010.

### Flip and deletion

- [ ] T090 Change the default backend to `Sobol` (`src/ri/state/options.cpp`, `CSamplingConfig`). Remove `Legacy` from the `Backend` enum and from `RiOptionV`: `"legacy"` becomes an unknown name, which gets the T027 diagnostic.
- [ ] T091 Delete the per-context Mersenne Twister and `random2d/3d/4d` from `CShadingContext` (FR-024a): `urand()`/`irand()`, `next_state()`, `state[]`, `next`, `randomInit()`, the `MATRIX_A`/`TWIST` macros, and the members in `src/libshader/shading/shading.{h,cpp}`.
  - Also delete every `Legacy` branch added in Phases 4–6.
- [ ] T092 [P] Delete `CSobol` and its `sampleHemisphere`/`sampleCosineHemisphere`/`sampleSphere` overloads (`src/common/random.{h,cpp}`), `_urand`/`_irand`, `apertureGenerator` (`stochastic.h:108`, and the `CRenderer::frame` use at `stochastic.cpp:71`), `photon.h` `gen2`/`gen3`/`gen4`, and `radiance.h` `generator`.
  - Delete `src/common/sobolTables.h` if nothing else includes it.
  - Check the installed package headers so that nothing exported still names a removed symbol.
- [ ] T093 [P] Delete `CRenderer::correlatedSampleTable` (`renderer.h:410`, `rendererStatics.cpp:208`, `renderer.cpp:331`), the `OPENRENDER_CORRELATED_SAMPLE_TABLE` read, `CSampler::generateBucketTable` with `bucketSeed`, and both table branches in `stochastic.cpp`/`raytracer.cpp`.
  - Add `SamplingEnv_CorrelatedTableIgnored` (US3-3): one parity scene rendered with and without `OPENRENDER_CORRELATED_SAMPLE_TABLE=1` gives bit-identical output.
- [ ] T094 [P] `tests/visual/CMakeLists.txt`: delete `VISUAL_ENV_CORRELATED` and the `Parity_motion-patches-translate-correlated` and `Parity_motion-quadrics-translate-correlated` registrations.
- [ ] T095 [P] `src/display/file/file_base.cpp:67`: replace the libc `rand()` dither with an inline 32-bit hash `h(x, y, channel)`, giving `d = qamp·(2·h − 1)`.
  - The hash is written in that file. There is no `libRIBCommon` link, no ABI change and no semver bump (FR-025).
- [ ] T096 [RUN] Build, then `ctest -L "sampling|libshader|common|architecture"`.
  - Run archcheck and delete exactly the STALE lines it reports: expected at least the two DB-011 `correlatedSampleTable` lines, probably DB-011 `stochastic.cpp → CRenderer::frame`.
  - Grep the hiders for each remaining `CRenderer::jitter`/`pixelXsamples`/`pixelYsamples` reference.
  - Lower `tests/architecture/metrics-baseline.txt` only if a NOTE asks for it. Any new VIOLATION is a stop-and-report.

### Re-render, pins, thresholds

- [ ] T097 [RUN] Run the **full** suite (every label, not only `visual`), and list every failing test whose failure is a sampling-only or dither-only change. This includes byte-exact comparisons against checked-in references outside `visual`: `tests/unit/texture_tile/test_render_byte_identical.sh` against `tests/unit/texture_tile/fixtures/references/`, the tile-concurrency references, and imager references. A structural failure, as opposed to new noise, is a stop-and-report.
- [ ] T098 [RUN] Re-render those references **once**, in every reference directory T097 found, using each test's own command and environment. A byte-exact reference is regenerated with the exact command its test documents (for example `-t:1` where the test pins it). Write `specs/039-sampling-foundation/rerendered-references.md` with one row per reference: path, test, and reason ("camera sampler", "8-bit dither hash", "RSL random() keyed", "transport sub-streams").
- [ ] T099 Write `specs/039-sampling-foundation/thread-pins.md` from research §R8, with one row per pin: file, reason, fate.
  - **Remove** the non-GI pins:
    - the 4 `-t:1` arguments for `quadlight`, `quadlight-slo`, `spherelight` and `spherelight-slo` (`tests/visual/CMakeLists.txt:1008–1023`);
    - `numthreads 1` in `sphere-{random,trace,transmission,visibility}-reyes{,-slo}.rib` (8 files);
    - the `-t:1` arguments in `tests/imager/integration/test_imager_render.cpp:186–187`.
  - **Keep**, with a pointer to spec 040: the 4 `sphere-{occlusion,indirectdiffuse}-reyes{,-slo}.rib` pins.
  - Review each `tests/unit/texture_tile/*` pin. Keep a pin that isolates cache behaviour, and record why. A `test_tile_source{,_synthesized}_concurrency.sh` tolerance MAY become exact, and is **never** widened.
  - Leave `tools/perf/sphere-trace-reyes.rib`'s pin unchanged, so the SC-010 baseline stays comparable.
- [ ] T100 [RUN] Re-measure every parity test: worst-case diff over 5 runs (M5). Record the values in `tests/visual/parity-thresholds.md`.
  - Tighten every threshold whose margin allows it. The targets are 8 for `motion-patches-translate` and 6 for `motion-quadrics-translate`; they are targets, not gates.
  - **Never widen.** If `Parity_dof` (threshold 60; 53.6 under the old correlated table) exceeds 60, stop and ask the maintainer.
- [ ] T101 [RUN] Full suite: `nice -n 19 ctest --test-dir build -j4 --output-on-failure`. Everything must be green, including `determinism`, `determinism-sweep` (if opt-in, run it explicitly), `convergence` (explicitly), and `SamplingOption_*` against the new default.
  - `git diff c4f1485 -- tests/visual/CMakeLists.txt tests/visual/parity-thresholds.md` must show no widened threshold.

### Verification of the success criteria

- [ ] T102 [RUN] **SC-010 performance check (manual, not ctest)**, following contracts/test-harnesses.md "Performance" and quickstart §10:
  - bash, ctest's visual environment, a scratch copy of `specs/039-sampling-foundation/tools/perf/`;
  - `nice -n 19 build/src/orender/orender -t:1 <rib>` and `… <rib>` (default threads);
  - Perl `Time::HiRes`;
  - **5 or more interleaved repetitions (rep-major)**.
  - Record the medians, min/max, spread and MAD in the same columns as `tools/perf/m2-baseline-c62334d.txt`, both in a new `specs/039-sampling-foundation/tools/perf/m2-after-<HEAD>.txt` and in research.md §M2 "After".
  - **Pass**: every scene's median, at both `t1` and `tdef`, is at most 2% above the baseline median. Compare medians only, never single runs.
  - A failure is a stop-and-report, with a profile of the hot draw path.
- [ ] T103 [RUN] SC-001 and SC-009:
  - Remove `--no-dither` from `add_determinism_sweep` (T080), now that T095 hashes the dither. Run `ctest -L determinism-sweep` with dither on, covering US1-5 and the scenes deferred by T079.
  - Run it once per backend, using the sweep with `--prepend` set to `pmj02` and to `independent`.
  - Run one seed pair (`seed 0` against `seed 1`): different noise, the same within each seed.
  - **Frames** (edge case): a 2-frame RIB rendered in full, then frame 2 alone with `-f 2`. Frame 2 must be bit-identical between the two.
  - **Crop** (edge case): a `CropWindow` render must be bit-identical to the same pixels of the full render. If filter margins at the crop edge make this impossible, stop and report with the differing-pixel map.
  - Register these checks as `Sampling_FrameReproducible` and `Sampling_CropConsistent` under `determinism`.
- [ ] T104 `Sampling_NoGlobalRandomness` (registered in Phase 6, T067) must pass with an **empty** allowlist: delete the lines that remain (the legacy generators T090-T095 removed) and `[RUN]` it: zero matches outside `src/precomp/` and comments (SC-007).
- [ ] T105 [RUN] Linux validation (memory: GCC 13 `-Werror`; rsync plus the Maestri path). Build and run `ctest -L "sampling|common|libshader|architecture|determinism"` on Ubuntu 24.04, plus `ctest -L architecture` with `--platform linux`. If the Linux path is unavailable, record "pending Linux validation" in `validation.md` and report it.

### Documentation and registry

- [ ] T106 [P] `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md`:
  - DB-011 history: the deleted lines, "spec 039, <date>";
  - DB-005 history: the per-context RNG state is gone, "the RNG part of V1";
  - DB-016's follow-up records the split: 039 removed randomness from per-context state, and arena/stats injection stays with 029;
  - DB-017's "injected RNG" direction is noted as done for `cluster()`.
- [ ] T107 [P] `DEVNOTES_DETAILS/HIDER_PARITY.md`: replace the correlated-table section (Option B, T084) with a correlated-by-default note. Update the #48 paragraph, since the per-thread jitter streams and the dither are now gone.
- [ ] T108 [P] `DEVNOTES_DETAILS/BUGS.md`: the midpoint-probe stream-shift note (`probeDepth`) is resolved by its own sub-stream.
- [ ] T109 [P] Document `Option "sampling"` (values, default, seed, diagnostic, and the limitation that uniform `random()` under raytrace depends on bucket size, FR-021):
  - in `DEVNOTES_DETAILS/RIB_GUIDE.md`;
  - in the Hugo site, as constitution VII requires: `docs/site/content/manual/reference/options.md` (the option), `multithreading.md` and `network-parallel-rendering.md` (non-GI renders are now bit-identical at any `-t`/`-P`; GI caches wait for spec 040), and `raytraced-shadows-and-reflections.md` or `sl-functions.md` where `random()` semantics are described.
- [ ] T110 [P] `DEVNOTES.md`: add a 039 status row. Point to `tests/visual/parity-thresholds.md`, `rerendered-references.md` and `thread-pins.md`.
- [ ] T111 [RUN] Final quickstart pass, §1–§10. Record every result in `validation.md`. Stage everything with `git add`.

**PAUSE 9 (final)**: report SC-001–SC-010 one by one against the evidence in `validation.md`.

---

## Dependencies & execution order

### Phase dependencies

- Phase 0 → 1 → 2 (the sampler library blocks everything else).
- Phase 3 depends on 2: `CSamplingConfig` and the `Backend` enum.
- Phase 4 depends on 2 and 3: keys need the config; ordinals need nothing else.
- Phase 5 depends on 4: `CRay` key, depth and `SampleId`.
- Phase 6 depends on 4: `pointKeys`, ordinals and `childKey`. It can overlap Phase 5 file-wise, but run them in order so each PAUSE has one subject.
- Phase 7 depends on 5 and 6: the sweep is only meaningful once every consumer is converted. The harness code (T079) can be written earlier.
- Phase 8 depends on 5 and 6.
- Phase 9 depends on everything. The flip comes last because FR-034 requires a single re-render (research §R5).

### Within each phase

Tests first → TEST-APPROVAL → implementation → [RUN] verification → PAUSE.

### Parallel opportunities

- Phase 2: T009–T016 are all [P] (separate test files). T020 and T021 touch separate files.
- Phase 4: T038–T042 [P]; T044–T048 (Geometry, core, Infra) are independent of T052–T054 (compiler and JIT).
- Phase 6: T074–T077 [P] (separate domains and files).
- Phase 8: the T086 templates [P].
- Phase 9: T092–T095 [P]; T106–T110 [P].

## Parallel example: Phase 6

```text
T074 [US6] hcshader.cpp light sub-streams
T075 [US6] photon job firstPhotonIndex + photon.cpp keys
T076 [US6] irradianceDispatch.cpp / radiance.cpp
T077 [US1] CKeyedStream sites (cluster, getDisp, pointHierarchy, init.cpp)
```

## Implementation strategy

- **MVP**: Phases 0–2. The tested sampler library (US2) stands alone and changes no render.
- **Incremental**: Phases 3–6 add the option, identity and consumers behind `Legacy`, so each PAUSE leaves the suite unchanged and green.
- Phases 7 and 8 prove determinism and convergence on the new path before it becomes the default.
- **Flip** (Phase 9): one default change, one deletion sweep and one re-render, with the success criteria verified in `validation.md`.
- The agent stages; the maintainer commits at each PAUSE.
