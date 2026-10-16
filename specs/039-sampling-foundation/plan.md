# Implementation Plan: Sampling Foundation

**Branch**: `039-sampling-foundation` | **Date**: 2026-10-15 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/039-sampling-foundation/spec.md`. Clarified on 2026-10-15 (10 answers).

## Summary

This plan replaces every render-time random source with a counter-based sampler in `libRIBCommon` (the architecture check's `Infra` class).

**Sampler.** Its draws are pure functions of (key, dimension, index), with three backends:
- Owen-scrambled Sobol in ZSobol style, the default;
- PMJ02;
- an Independent hash.

**Identity.** The sampler is driven by identity carried through the renderer:
- **camera samples**: an absolute sample-grid `SampleId`, shared by reyes and raytrace;
- **rays**: a `SampleKey` and depth on `CRay`, which is an Infra type;
- **reyes grid points**: a cell key built from a new hierarchical `CObject::stableId` plus (u, v);
- **randomness that is not a sample**: compiler-assigned call-site ordinals for every stochastic RSL builtin, identical on the interpreter and the JIT, plus a local keyed stream for clustering and the like.

**Phasing.** Conversion happens behind an internal `"legacy"` default, so the suite stays green phase by phase. A single final phase flips the default, deletes the legacy generators and `correlatedSampleTable`, changes the display dither to a pixel hash, re-renders the stochastic references once, removes the non-GI thread pins and tightens parity thresholds.

**Verification.** The existing determinism harness gains cross-configuration comparison, and a new convergence harness measures RMSE against sample count. Design detail is in [research.md](research.md), [data-model.md](data-model.md) and [contracts/](contracts/).

## Technical Context

**Language/Version**: C++20, built with `-Werror`. CMake ≥ 3.19.

**Primary Dependencies**: no new dependency.
- libtiff, which already exists, is used by the harnesses.
- LLVM is used as before by the JIT emitter. The `op_*` ABI changes, but no LLVM API change is involved.

**Storage**: N/A. Checked-in test artifacts:
- the convergence references, 64×64 float TIFF under `tests/convergence/reference/`;
- `thread-pins.md` and `rerendered-references.md` in the spec directory.

**Testing**:
- ctest labels `sampling` (new, unit), `determinism` (extended), `determinism-sweep` (new), `convergence` (new, opt-in), `visual`, `architecture`, `libshader` and `common`.
- TDD: the sampler tests are written and fail first (Principle III).

**Target Platform**: macOS (primary) and Linux (Ubuntu 24.04, GCC 13). Bit-identity is required within one platform and build only.

**Project Type**: renderer (libraries + CLI).

**Performance Goals**: median render time on each of the six `tools/perf/` scenes, at `-t:1` and at the default thread count, at most 2% above the master baseline (SC-010, research §M2).

**Constraints**:
- No new `CRenderer::` reference from any domain file (the gate below).
- No new mutable global shared across a boundary (rule 1).
- Display drivers stay ABI-only.
- Thresholds are never widened.
- References are re-rendered exactly once.

**Scale/Scope**:
- About 60 randomness call sites in 25 files (research §R1).
- About 230 non-GI scenes enter the determinism sweep.
- 16 thread pins.
- 114 parity tests to re-measure.

## Constitution Check

*GATE: must pass before Phase 0 research, and is re-checked after Phase 1 design.*

**Principles I–VII**

- **I/II**: C++20. The new code is small and pure (hash and scramble functions).
- **III**: test-first.
  - Phase 2 writes the `Sampling_*` tests before the library.
  - Each conversion phase starts with a failing determinism or parity test that runs with `Option "sampling" "sobol"`.
- **IV**: no new CLI flag. The behaviour is reachable through RIB `Option`, and the existing CLI options `-t` and `-P` are exercised.
- **V**: no new dependency. Both PMJ02 tables and Sobol are generated in code.
- **VI**: macOS and Linux. The GCC 13 `-Werror` validation follows the established rsync/Maestri path.
- **VII**: the docs listed in FR-038 are updated, **including the Hugo site** (`docs/site/content/manual/reference/options.md`, `multithreading.md`, `network-parallel-rendering.md`). The new RIB option and the determinism change are user-facing (task T109).

**Principle VIII — Domain Separation and Modular Boundaries**

- [x] **Domains touched**:
  - **Infra** (`libRIBCommon`): the sampler, keys, `CKeyedStream`, the `CRay` fields, and the new `sampleHemisphere` overloads.
  - **Hiders**: `stochastic`, `raytracer`, `sampler`, `trace`, `photon`, `irradianceDispatch`, `radiance`.
  - **Shading**: the runtime, `CShadingState`, the interpreter and JIT entry points, and the compiler ordinals.
  - **Texturing**: `texture.cpp` filter taps and `pointHierarchy.cpp`.
  - **Geometry**: `object.h` `stableId`, child-ID assignment, `getDisp`, and per-element discriminators in `sample()`.
  - **Scene Interface**: `ribOut.cpp` and token declarations.
  - **Graphics State**: `COptions`.
  - **Display Outputs**: the `file_base.cpp` dither.
  - **Renderer core**: `CSamplingConfig`, ID scopes in `CRendererContext`, photon job indices, `geometryDispatch.cpp`'s `cluster`/`shade`, `hcshader.cpp`, `init.cpp` and `renderer.cpp`.
- [x] **No new disallowed dependency.** Every new dependency is `X → Infra`, which `* → Infra` allows, or `Core → X`, which is allowed. Texture lookups take a key from Shading, and Shading → Texturing is allowed. No domain gains a `CRenderer::` reference: configuration is injected through `CSamplingConfig` (research §R4).
- [x] **Interface changes specified**: the sampler API, RIB options and shading runtime ABI are in [contracts/](contracts/). The `CRay` and `CShadingState` fields are in [data-model.md](data-model.md).
- [x] **No hider-specific special case.** One `CSampler` absolute-grid function serves reyes and raytrace, so effects stay uniform. Zbuffer keeps its fixed positions because it draws no camera samples, not because of a special case.
- [x] **Plug-in interfaces**:
  - The display driver ABI is unchanged; the dither is computed inside the driver, so there is no semver bump.
  - No shadeop or procedural interface changes.
  - The JIT `op_*` ABI is internal and not a constitution plug-in boundary. Its change is documented, and the build rebuilds every `.slo`.
- [x] **Registered violations are not extended.**
  - Shading's read of `object->stableId` is an inline field access through `shading.cpp`'s already-registered DB-006 include. It creates no new include and no link symbol, and 029 removes it.
  - Hiders' writes into `CShadingState` go through includes Hiders → Shading already allows.
  - **Reductions**:
    - the two DB-011 `correlatedSampleTable` lines, which are certain;
    - DB-011 `stochastic.cpp → CRenderer::frame`, which is probable;
    - any `jitter`/`pixelXsamples`/`pixelYsamples` reference that moves wholly into `CSamplingConfig`, which is possible and checked in Phase 9;
    - the removal of `_urand`'s `thread_local` process-global state and of the per-context Mersenne Twister. That is the "RNG part of V1/DB-005" (FR-024a), recorded in DB-005's history.
  - **Allowlist growth: none permitted.** Phase 9's archcheck run must show only STALE deletions.

**Gate result: PASS.** No violation needs justifying, and the Complexity Tracking table is empty.

**Binding decisions**: the plan relies on these and changes none.

- **D1**: hider = visibility, integrator = transport. Hiders own the camera slots, and the transport side owns sub-streams.
- **D2**: after 039, the classic integrator's byte-identical baseline is the re-rendered output of Phase 9.
- **D20** (§8.1): cell keys and pixel-sample keys both feed sub-streams. This is the index mapping in §R3.5, and specs 042/054 rely on it.
- **D27**: native JIT lowering is deferred. The call-site ordinal is a plain constant argument in today's emitter, and §10's lowering carries it forward unchanged.
- **D30**: the sampler is infrastructure (`libRIBCommon`). No amendment is needed.

Reference sections:

- **§2.6**: 035's `Sample2D u` comes from `get2D` on an integrator-owned `SubstreamKey`.
- **§4.3**: 041's `Sample3D`/`Sample4D` are compositions of consecutive slots.
- **§5.4**: `ISamplingService` is [contracts/sampler-api.md](contracts/sampler-api.md).

## Project Structure

### Documentation (this feature)

```text
specs/039-sampling-foundation/
├── spec.md, checklists/requirements.md
├── plan.md                    # this file
├── research.md                # inventory, decisions, PENDING MEASUREMENTS (M1–M5)
├── data-model.md
├── contracts/
│   ├── sampler-api.md
│   ├── rib-options.md
│   ├── shading-runtime.md
│   └── test-harnesses.md
├── quickstart.md
├── tools/perf/                # SC-010 perf-only scenes + m2-baseline-c62334d.txt
├── thread-pins.md             # Phase 9
├── rerendered-references.md   # Phase 9
└── tasks.md                   # /speckit-tasks (not created here)
```

### Source Code (repository root)

```text
src/common/                      # Infra (libRIBCommon)
├── sampling.h, sampling.cpp     # NEW: keys, backends, camera slots, CKeyedStream
├── samplingPmj02.cpp            # NEW: deterministic PMJ02 table generation
├── random.h, random.cpp         # new Sample2D/3D overloads; CSobol, _urand/_irand removed (Phase 9)
└── sobolTables.h                # removed in Phase 9 if unused
src/ri/core/ray.h                # CRay: + SampleKey key, uint16 depth
src/ri/state/options.{h,cpp}     # COptions: samplingBackend, samplingSeed
src/ri/parse/ri.h, src/common/riVocabulary.h, src/common/riTokens.cpp   # tokens
src/ri/render/                   # Core
├── rendererContext.cpp          # RiOptionV, addObject ID scopes
├── rendererDeclarations.cpp     # pre-declarations
├── renderer.cpp, rendererStatics.cpp   # CSamplingConfig; correlatedSampleTable removed
├── geometryDispatch.cpp         # cluster()/irand → CKeyedStream; child IDs
└── init.cpp                     # shader-init urand → keyed
src/ri/output/ribOut.cpp         # declare + emit Option "sampling"
src/ri/geometry/                 # object.h stableId; object.cpp getDisp; child IDs; element ids
src/ri/hiders/                   # sampler.{h,cpp}, stochastic, raytracer, trace, photon,
                                 # irradianceDispatch, radiance
src/ri/hcshader.cpp              # light samples → sub-streams
src/ri/texture/texture.cpp, pointHierarchy.cpp
src/libshader/shading/           # shading.{h,cpp}, executeMisc.cpp, rslOps.{h,cpp},
                                 # scriptFunctions.h, shaderPl.cpp
src/libshader/compiler/          # call-site ordinals; .rslo operand; llvmEmitter constants
src/display/file/file_base.cpp   # dither pixel hash
tests/unit/sampling/             # NEW: Sampling_* (label sampling)
tests/convergence/               # NEW: test_convergence, scenes, reference/, README.md
tests/visual/                    # test_depth_determinism.cpp (extended),
                                 # determinism-scenes.cmake (NEW), CMakeLists.txt,
                                 # parity-thresholds.md
tests/architecture/allowlist.txt # STALE deletions only
examples/rib/tests/              # pin removals; convergence + JIT-parity scenes
```

**Structure decision**: the existing layout is kept. Only `tests/unit/sampling/` and `tests/convergence/` are new directories. Both are tests, so archcheck's `domain-map.txt` is unaffected; this is verified at Phase 2.

## Phases

Each phase ends green on `ctest -L "visual|common|libshader|architecture|sampling"` before the maintainer's phase review.

| # | Phase | Delivers | Spec |
|---|---|---|---|
| 0 | **Measurements** (coordinator) | Done. M1: identical in every configuration. M2: baseline for the six `tools/perf/` scenes (research §M). | SC-001, SC-010 |
| 1 | Setup | `tests/unit/sampling/` and `tests/convergence/` skeletons; labels | — |
| 2 | Sampler library (US2, test-first) | `sampling.{h,cpp}` with all three backends; `CKeyedStream`; G1–G9 tests | FR-001–008, FR-029, SC-002, SC-003a |
| 3 | Options (US5) | Tokens in the 3 files, `RiOptionV`, `COptions`, `initDeclarations`, `ribOut`, `CODE_BADTOKEN` diagnostic; `CSamplingConfig` injected into contexts. Internal default stays `legacy`. | FR-026–028 |
| 4 | Identity plumbing | `CObject::stableId` with scopes and child rules (V1 verified at plan: expansions run under `delayedMutex`); points' `elementId` and the per-family key-uniqueness test (V2); `CRay` key and depth; `CShadingState` keys; compiler call-site ordinals in `.rslo` and JIT; ordinal-equality and purity unit tests | FR-011–016, FR-019 (plumbing) |
| 5 | Camera samples (US3) | Absolute-grid `SampleId` in `CSampler`, used by both hiders under `sobol`/`pmj02`/`independent`; raytrace importance slot; reyes lens slot (with `apertureGenerator` removed under the new backends). Cross-hider slot-equality unit test. | FR-009, FR-017 (new path) |
| 6 | Transport and other consumers | `trace`/`transmission`/`visibility`/`gather`/`occlusion`/`indirectdiffuse`; `random`/`urandom` (interpreter and JIT); texture/shadow taps; `hcshader`; photons (job `firstPhotonIndex`); irradiance; `cluster`, `getDisp`, point hierarchy, radiance and shader init on `CKeyedStream`; JIT-parity tests (SC-008) | FR-010, FR-018–024 |
| 7 | Determinism harness | `--configs`/`--outputs`/`--prepend` mode; `determinism-scenes.cmake`, run with `sobol` forced; the 10-minute rule (M3) sets default vs opt-in | FR-030, SC-001 (opt-in) |
| 8 | Convergence harness | `test_convergence`; scene templates; `convergence-reference` target; references checked in (M4) | FR-033, SC-003b |
| 9 | **Flip** | Default → `sobol`. Delete `legacy`, the per-context Mersenne Twister and `random2d/3d/4d` (FR-024a), `CSobol`, `_urand`/`_irand`, `correlatedSampleTable` and its env var, `VISUAL_ENV_CORRELATED` and the `-correlated` variants. Dither → pixel hash. Re-render the references once (`rerendered-references.md`). Remove the non-GI pins (`thread-pins.md`). Re-measure parity (M5) and tighten. Performance check against M2. Archcheck STALE deletions plus the registry history (DB-011, DB-005, DB-016 split). Docs (FR-038). Linux GCC 13 validation. | FR-017, FR-024a, FR-025, FR-032, FR-034–039, SC-001, SC-004–010 |

**Why the flip is last**: the spec requires a single re-render (FR-034). The suite must also be green at each phase, so every render-changing conversion has to exist (opt-in) before the default changes. This is the same pattern as spec 054 (D20).

## Pending measurements (no build or test was run in plan)

The full procedures and the plan change each outcome triggers are in research §M.

| ID | What | When | Blocks |
|---|---|---|---|
| **M1** | `-P:N` against `-t:1` bit-identity, today's code, `jitter 0`, float output | **DONE**: all 7 scenes are identical in every configuration. `-P` stays in SC-001 (research §M1 results). | — |
| **M2** | performance baseline | **DONE**: six perf-only scenes in `tools/perf/`, at `t1` and `tdef`, median of 5 interleaved repetitions; MAD ≤ 0.5% (research §M2, `tools/perf/m2-baseline-c62334d.txt`). The `perf-manual` set was invalid and is kept only as history. | — |
| M3 | Determinism-sweep runtime | Phase 7 | whether the sweep is default or opt-in |
| M4 | Convergence ground-truth cost | Phase 8 | reference resolution |
| M5 | Parity worst-case diffs under the new default | Phase 9 | SC-004 thresholds |

## Risks

| Risk | Mitigation |
|---|---|
| Procedural expansion order (V1) | Verified at plan: expansions run under the global `delayedMutex` (research §R3.2), so the residual risk is low. |
| **(u, v) is not unique for points, curves or blobby (V2)**, so noise would be correlated | A per-element discriminator in `sample()`, and a unit test that renders `random()` on each primitive type and checks that the values are distinct |
| **Call-site ordinals differ between `.rslo` and `.slo`** | Ordinals are assigned once on the final IR; `LibShader_CallSiteOrdinals` tests it |
| **Camera grid units differ between hiders** (reyes `left` is in subpixels, raytrace `left` is in pixels) | One shared absolute-subpixel function; `Sampling_CameraGridConsistency` (research §R3.1) |
| **Derivative-offset tail vertices** (3× vertices under ray shading) | Tail vertices copy their real vertex's key and occurrence on both backends (research §R3.10a) |
| **Stale `.rslo`/`.slo` after the format and ABI change** | Dependency tracking rebuilds every shader. On top of that, the `.rslo` format version is bumped (old files are rejected with a diagnostic) and the changed `op_*` entries are renamed with a version suffix (a stale `.slo` fails at bind time instead of reading garbage; research §R3.6). |
| **`op_*` symbols dead-stripped on macOS** (gotcha 3) | The `-slo` suite at Phase 6. If a symbol does not resolve, wire up `addProcessSymbol()`. |
| **`Parity_dof` margin.** Under correlation it measured 53.6 against a threshold of 60 (T084, D9 model difference), and the new sampler could move it. | M5. The threshold is never widened. If it exceeds 60, stop and ask the maintainer; the D9 residual belongs to spec 028. |
| SC-003's bar on motion blur (time stays grid-stratified) | Resolved: "no worse than Independent" there (Plan decisions 2) |
| **Performance (2%)** | Inline hot paths, no tables on the default path; Phase 9 measures against M2 |
| `-P` with an explicit `-t` runs single-threaded children | The harness writes the `-P` entry without `-t` (research §M1, caveat a) |
| Performance noise (single runs reach about 3%) | SC-010 compares medians of 5 or more interleaved repetitions, never single runs (research §M2) |
| **Large reference churn** (every stochastic scene, plus every 8-bit dithered output) | One documented re-render; `rerendered-references.md`; no threshold widened |

## Plan decisions (maintainer, 2026-10-15)

These are recorded in the spec's Clarifications section.

1. **JIT optional parameters**: FR-018 and SC-008 are restricted to the forms both backends implement: plain `trace`/`transmission`/`visibility`/`occlusion`/`indirectdiffuse`, and `gather`. The JIT gap is **GitHub #55**, fixed by the maintainer on its own `fix/` branch once 039 is complete. It is not a 039 task.
2. **SC-003 bars**: the 0.15-steeper bar applies to lens/DOF, area-light shadows, occlusion and glossy trace. For motion blur (and film position), the bar is "no worse than Independent".
3. **Determinism sweep**: the research §M3 rule applies. If it takes more than 10 minutes at `-j4`, it is opt-in, and a representative subset stays under `determinism` (FR-030).

4. **Performance set (Q4, decided)**: the six perf-only copies in `tools/perf/` (research §M2), timed at `-t:1` and at the default thread count, as the median of 5 interleaved repetitions. The baseline was measured on `c62334d`.

## Open questions for the maintainer

None.

## Complexity Tracking

No constitution violations to justify.
