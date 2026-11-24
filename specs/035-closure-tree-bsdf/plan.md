# Implementation Plan: Closure Tree and BSDF Interface

**Branch**: `035-closure-tree-bsdf` | **Date**: 2026-11-23 | **Spec**: [spec.md](spec.md)

**Input**: the feature specification `specs/035-closure-tree-bsdf/spec.md`, clarified on 2026-11-23.

**Project rules followed**: the `## Spec-kit agent rules` section of the main checkout's `CLAUDE.md`. The worktree's copy is identical; the main copy's last commit is newer (`9e08ff72`).

**Hooks**: the optional `before_plan` and `after_plan` auto-commit hooks were declined. The agent never commits.

## Summary

A shaded point can hand its integrator a **closure tree** instead of only a lit colour. The tree is a plain-data, OSL-shaped description of how the point scatters. The pieces:

- **The tree** (D17): component, multiply and add nodes in the batch arena, with a flattened lobe list and a generic evaluator.
- **The registry** (D14): OSL's standard names. The first set is `diffuse`, `oren_nayar`, `microfacet`, `reflection`, `refraction`, `transparent`, `holdout`, `uniform_edf`, and `emission` as a deprecated alias.
- **One BSDF table** (§2.6, §7.4): evaluate, sample and pdf, each with forward and reverse densities, explicit delta lobes and transport modes. It is built on a shared BRDF math library (D19).
- **A `Spectrum` type** (D18) in Infra.
- **`closuremode`** (D13): attribute over option over `color`. It chooses what a `Surface` binding hands back:
  - `color`: today's code, byte for byte;
  - `both`: colour plus trees;
  - `closures`: trees, with the lighting gated off at four funnels the interpreter and the JIT already share.
- **A `Bxdf` statement** (D5, D9): it binds one of four built-in C++ Bxdfs, behind a versioned C table and a Shading-owned registry.

The trees leave the shading step through a Shading-owned consumer interface. In 035 only tests install a consumer; spec 042 installs it for real. Until then, three named interim rules, IR-017, IR-018 and IR-026, say what renders, and 042 replaces each one by name.

**Version numbers**:

- New: the closure ABI and the Bxdf table, both at 1.0.0 and staying 1.x.
- Unchanged: `kRendererServicesVersion` (2.6.1), the hider table and the integrator table (2.0.0).

**Byte identity**: with no `closuremode`, no `Bxdf` and no consumer, every image, file, hash, shader byte, instance key and draw is unchanged.

## Technical Context

**Language/Version**: C++20, with C17 for the two ABI headers (`closureAbi.h`, `bxdfAbi.h`, as `hiderAbi.h` is); CMake ≥ 3.19; `-Werror`.

**Primary Dependencies**:

- No new dependency.
- Spec 037's `orplugin.h` and `checkPluginVersion` (in-process overload).
- Spec 039's keyed sample stream, in the tests only.
- Spec 033's `IShaderBackend`/`ShadingSystem`.
- Spec 057's area-light funnel.

**Storage**: N/A, memory only. The trees live in the batch arena. The 24 new scenes and their references are generated files (`specs/035-closure-tree-bsdf/tools/gen_scenes.py`).

**Testing**:

- ctest labels `closures` (the ABI and the BSDF), `closuremode` (modes and gate) and `bxdf` (the statement).
- The existing `architecture`, `determinism-sweep`, everyday and `shading_parity` sets.
- In-process programs, with a third-backend double (`cldbl`, which delegates `execute` to the real `rslo`/`slo` backends for a wrapped test shader) and consumer and light doubles.
- The 24 determinism-sweep image entries run shipped, non-closure-capable shaders, so they prove only the interim behaviours (IR-017 and IR-026) across threads, hiders and formats (IR-018 needs a capable Surface, which no shipped shader is: `Closure_Interim_IR018_*` covers it in process); the closure path itself is observed only by the in-process tests (maintainer, 2026-11-23).

**Target Platform**: macOS 13.3+ (Apple Clang, vcpkg) and Ubuntu 24.04 (GCC 13 `-Werror`), validated on both.

**Project Type**: a renderer library, plus the `orender` CLI.

**Performance Goals**:

- The `color` path is unchanged code: no new branch per point and no allocation. Only one batch-level plan lookup is added.
- Under the tree modes, building a tree costs O(nodes) per point, within a fixed byte budget per point.

**Constraints**:

- Byte identity (FR-027).
- Determinism at any thread count and across hiders (FR-028).
- No new allowlist line, no registry entry, no `CRenderer` under `src/libshader` (FR-025).
- No threshold widened, no test disabled, no reference re-captured.
- No git in tests or builds.

**Scale/Scope**:

- 24 new source files under `src/libshader/closures/` (the list below) and 1 under `src/common/`.
- About 30 existing files modified (listed below).
- About 90 new tests (the exact count is recorded by tasks.md T120's `ctest -N` lists), plus 24 sweep entries.

All NEEDS CLARIFICATION items are resolved in [research.md](research.md). The open items were decided by the maintainer on 2026-11-23 (below).

## Constitution Check

*GATE: must pass before Phase 0 research; re-checked after Phase 1 design.*

| Principle | Status |
|---|---|
| I. Clean code | The surface-step change is one plan function plus one step function in a new file; `shade()`'s own edit is about 15 lines. Each interim rule is one named function |
| II. Language standards | C++20; the ABI headers are strict C17 (as `hiderAbi.h`, `capability.h`); no extensions |
| III. TDD | Every phase starts with red tests: layout, registry, BSDF numerics, modes, gate, statement. Tasks write each test before its code |
| IV. CLI | Reachable from RIB through `orender` (`Option`/`Attribute "shade"`, `Bxdf`); diagnostics on stderr |
| V. Minimal dependencies | None added; the math is written from papers (R7) |
| VI. Platforms | macOS and Linux; the LP64 layout is pinned on both (`Closure_Layout`) |
| VII. Docs | `DEVNOTES_DETAILS/CLOSURES.md`, the RIB guide, DEVNOTES and a drafted CLAUDE.md addition (FR-029). The Hugo site page is listed as an implement task |

**Principle VIII — Domain Separation and Modular Boundaries**:

- [x] **Domains touched are named**:
  - Shading: the closure model, registries, BSDF, gate, step, consumer;
  - Graphics State: the mode storage and declaration;
  - Scene Interface: the `Bxdf` statement, tokens, `ribOut`;
  - Infra: `Spectrum`, the tokens;
  - the renderer core: `RiBxdfV`, the parsing of `closuremode`, the per-frame `ClosureFrame`;
  - the bindings (consumers).
  - Hiders, Geometry, Transport, Texturing and Display Outputs: **untouched**.
- [x] **No new cross-domain dependency outside the tables**:
  - Shading → Graphics State: `CAttributes::closureMode`, `governingMaterial()`, `bxdf()`; allowed.
  - Shading → Infra: `Spectrum`, `orplugin.h`, `pluginAbi.h`; allowed.
  - Core → Shading: the Bxdf registry and `ClosureFrame`; the core may depend on every domain.
  - No domain depends on the core. The mode's option value reaches Shading through a setter the core calls, not through a Shading read of the core (R10).
- [x] **Every added cross-domain interface is specified**: contracts/closure-abi.md, contracts/bxdf.md, contracts/closuremode.md, contracts/consumer-and-backend.md, and data-model.md.
- [x] **No hider-specific special case**: the mode, the gate and the interim rules are implemented once, in Shading, in the common `shade()` entry, so every camera hider inherits them (rule 3, FR-022). The photon hider never calls `shade()`.
- [x] **Plug-in interface changes**:
  - shadeop, procedural and display interfaces: unchanged.
  - The two new internal tables start at 1.0.0, with history tables (D7, D8, D22; the maintainer's rule of 2026-11-23).
  - `IShaderBackend` gains two members with default bodies. It is an internal C++ seam, unversioned since spec 033.
  - `kRendererServicesVersion` is unchanged.
- [x] **Registered violations are not extended**: DB-018 (message passing) is untouched; no file of DB-004, DB-011, DB-023 or DB-024 gains an edge. `CONSUMER` edges: the Python and Lua helpers emit text only, so no new include.

**Gate result**: PASS. No violation, so Complexity Tracking is empty.

**Post-design re-check**:

- PASS. The design adds no include from Shading into Hiders, Geometry or the core; `libRSLRuntime` stays self-contained (no `frameParams()` read, R10).
- The new headers live under `src/libshader/` (Shading) and `src/common/` (Infra), both already classified in `domain-map.txt`.

## Project Structure

### Documentation (this feature)

```text
specs/035-closure-tree-bsdf/
├── spec.md
├── plan.md              # this file
├── research.md          # R1–R23, the decisions and their alternatives
├── data-model.md        # entities, fields, state transitions
├── quickstart.md        # validation guide
├── contracts/
│   ├── closure-abi.md          # nodes, registry, BSDF table, numerical acceptance
│   ├── bxdf.md                 # Bxdf table, registry, built-ins, RiBxdf/RIB/ribOut/bindings
│   ├── closuremode.md          # statements, four layers, resolution, surface step, interim rules, gate, -P:N
│   └── consumer-and-backend.md # IClosureConsumer, IShaderBackend additions, ClosureFrame
├── checklists/requirements.md
└── tools/gen_scenes.py  # (implement) the 24 sweep scenes and the closure test scenes
```

### Source code: new files

```text
src/common/spectrum.h                              Infra, header-only (D18)
src/libshader/closures/                            Shading, compiled into libRSLRuntime
├── closureAbi.h          C17: nodes, categories, params, OrBsdfTable, OR_CLOSURE_ABI_VERSION 1.0.0
├── closureRegistry.{h,cpp}  registry, built-in entries, aliases, interned strings
├── closureTree.{h,cpp}   builder over threadMemory, budget, flatten, generic evaluator, accessors, copy
├── bsdfMath.{h,cpp}      frames, GGX/Beckmann D and G, VNDF sampling, Fresnel, Walter refraction, Oren-Nayar, MIS
├── bsdfComponents.cpp    the five BSDF closures' OrBsdfComponentOps
├── bsdf.{hpp,cpp}        the tree-level OrBsdfTable (prepare/evaluate/sample/pdf) and C++ wrapper
├── bxdfAbi.h             C17: OrBxdfTable, OR_BXDF_ABI_VERSION 1.0.0
├── bxdfRegistry.{h,cpp}  registry, version check, refusal reasons, prefix rule
├── bxdfInstance.{h,cpp}  CBxdfInstance, bxdfInstanceOf
├── builtinBxdfs.cpp      diffuse, plastic, conductor, dielectric
├── closureMode.h         ClosureMode (mirror of EClosureMode), resolve()
├── closureFrame.{h,cpp}  ClosureFrame (scene mode, shared once-per-frame set)
├── closureConsumer.h     IClosureConsumer, ClosureBatch
├── closureInterim.{h,cpp}  IR-017, IR-018, IR-026, interimSurfaceFor()
└── closureStep.{h,cpp}   planSurfaceStep, runSurfaceStep, deliverClosures
tests/closures/           CMakeLists.txt, the cldbl backend double (delegating `execute` to the real `rslo`/`slo` backends), consumer/light doubles, the tests of quickstart §2–§4
examples/rib/tests/closures-*.rib    generated scenes (24 sweep scenes plus the test scenes)
DEVNOTES_DETAILS/CLOSURES.md         the guide (FR-029)
```

**Structure decision**:

- The closure model is one Shading subdirectory compiled into the existing shared `libRSLRuntime`, so there is one process-wide registry (gotcha 22) and no new library or link edge.
- `Spectrum` is a header in Infra.
- The tests are a new `tests/closures/` directory, beside `tests/shading_system/`, whose `test_backend.h` pattern the `cldbl` double extends.

## Existing files and APIs that 035 modifies

The coordinator asked for this list, to set the boundary with spec 054. Line numbers are those of `b9dec4e5`.

### Shading (`src/libshader/shading/`)

| File | Change | API |
|---|---|---|
| `shading.h` | `CShadingState::lightingSuppressed`; `CShadingContext::setClosureConsumer`/`closureConsumer`, static `setDefaultClosureConsumer`, `setClosureFrame`/`closureFrame`; private members | **internal API, additive** |
| `shading.cpp` | (1) `shade()`: the inline fast test, plus one `planSurfaceStep` call inserted at `:1196` (after the shader selection `:1159-1194`, before the `usedParameters` line `:1198` and `groupPrepare` `:1290`); the surface block `:1553-1592` wrapped so that `runSurfaceStep` takes it under a non-`color` plan; one `deliverClosures` call before `memEnd(threadMemory)` `:1617`; (2) the gate at the top of `iterateLights` (`saveCat` overload, `:2053`); (3) the gate in `prepareAmbient` (`:2508`); (4) the gate in `gatherHeaderBegin` (`:4907`); (5) the point-based branch of `jitOcclusion`/`jitIndirectDiffuse` | none public |
| `irradianceCalls.cpp` | the gate at the top of `libshader::irradianceLookup` | none |
| `giFunctions.h` | the gate in `IDEXPR`'s point-based branch | none |
| `shaderBackend.h` | `IShaderBackend::emitsClosures`, `executeClosures` (default bodies); header sentence | **internal seam, additive** |
| `shaderParameters.cpp` | `usedShaderParameters`: when a Bxdf governs, the surface slot is replaced by `interimSurfaceFor()`: the captured `defaultsurface`, or `PARAMETER_N` for the no-shader path (A′, Q-R1) | the hook's answer changes only for Bxdf-governed attributes |
| `CMakeLists.txt` | the new `../closures/*.cpp` sources in `libRSLRuntime` | — |

### Graphics State (`src/ri/state/`)

| File | Change |
|---|---|
| `stateVocabulary.h` | `enum class EClosureMode` |
| `attributes.h`, `attributes.cpp` | the `closureMode` field; constructor, copy (`:265`), `restore` (`:503`), `find` (`:721`); the comment of `bxdf()` (`:144`) |
| `options.h`, `options.cpp` | the `closureMode` field; constructor, copy, query |
| `declarations.cpp` | `declare(RI_CLOSUREMODE, "string")` (`:189`) |

### Infra (`src/common/`)

- `riTokens.cpp` and `riVocabulary.h`: `RI_CLOSUREMODE`, and `RI_BXDF` if the grammar needs a token constant.

### Scene Interface (`src/ri/parse/`, `src/ri/output/`)

| File | Change |
|---|---|
| `ri.h` | `RI_CLOSUREMODE`; `RiBxdf`/`RiBxdfV` declarations (**public RI API, additive**) |
| `ri.cpp` | the `RiBxdf`/`RiBxdfV` entry points; a new validity mask, `VALID_ATTRIBUTE_BLOCKS` without `RENDERMAN_OBJECT_BLOCK`, for `RiBxdf` (Q-R4; the other masks are unchanged) |
| `riInterface.h`, `riInterface.cpp` | `virtual RiBxdfV`, whose default ignores the statement (**back-end API, additive**) |
| `rib.l`, `rib.y` | the `Bxdf` keyword; two rules |
| `ribOut.h`, `ribOut.cpp` | `RiBxdfV`; an `Option`/`Attribute "shade"` case only if the fallback does not cover it |

### Renderer core (`src/ri/render/`)

| File | Change |
|---|---|
| `rendererContext.h`, `rendererContext.cpp` | `RiBxdfV`; the `closuremode` token in `RiAttributeV`'s `RI_SHADE` block (`:3923`); a new `RI_SHADE` branch in `RiOptionV` (beside `:2106`); at `RiWorldBegin` (`:995`), the capture of the `defaultsurface` handle **only when `setDefaultSurface()` bound it** (A′, Q-R1), handed to a `CBxdfInstance` already in the bxdf slot (B2) **before** `setDefaultSurface()` is called (R1); the Bxdf colour parameters converted with `COptions::convertColor` (RISpec §4.1.3) |
| `renderer.cpp` | `ClosureFrame` created in `beginFrame()` **before** the hider-creation loop, and set on each context in that loop next to `setSamplingConfig` (`:854`); deleted in `endFrame()` and on the abort path |

### Bindings

- `src/python/prman.py` and `src/lua/prman.lua`: `Bxdf`.

### Tests

| File | Change |
|---|---|
| `tests/CMakeLists.txt` | add `tests/closures` |
| `tests/visual/determinism-scenes.cmake` | append a `BEGIN spec 035` block of 24 entries |
| `tests/shading_parity/test_shader_parameter_bxdf_ignored.cpp` | **header comment only**: "until spec 035" becomes "a foreign object in the slot is ignored". Its assertions are unchanged and still hold, because the checked `bxdfInstanceOf` ignores the RSL instance it binds (R15, R16; Q13, approved) |
| `tests/unit/state/test_state_last_bound.cpp` | unchanged: it binds State doubles with no shader-parameter hook, and the checked accessor ignores them anyway |
| `tests/unit/rib_parse/test_ribout_shader_order.cpp` | extended with a real `Bxdf` statement |

### Docs (implement phase)

- `DEVNOTES.md`, `DEVNOTES_DETAILS/RIB_GUIDE.md`, `DEVNOTES_DETAILS/RENDER_ARCHITECTURE_REFERENCES.md` §13, and a CLAUDE.md addition drafted for the maintainer.

### Explicitly NOT modified (the 054 boundary)

- **Hiders**: everything under `src/ri/hiders/`, including `shadingProducer.cpp`, `hider.{h,cpp}`, `reyes.cpp`, `raytracer.cpp` and `situation`.
- **Shading**:
  - `shade()`'s sampling, derivative, interpolation, key and displacement code (`shading.cpp:1074-1548`). The **one exception** is the inline fast test plus one `planSurfaceStep` call, inserted at `:1196`, between the shader selection and the `usedParameters` line (`:1198`). That call ORs bits into `usedParameters` only when trees are built.
  - `shade()`'s atmosphere, camera-hit and unwinding code (`:1594-1626`);
  - `shaderGlobals.h`, `pointSource.h`, `situation.{h,cpp}`, `rendererServices.h`;
  - `execute.cpp`, `rslOps.*`, `rslBuiltins.*`, `llvmJit.*`, `shader.cpp`, `shaderGroup.cpp`, `shadingSystem.*`;
  - the compiler (`src/libshader/compiler/`).
- **Other domains**: Geometry, Transport, Texturing, Display Outputs.
- **Shaders**: every `.sl` file.

**Coordination with 054**:

- Both specs edit `shade()` in `shading.cpp`, at disjoint ranges. 054 owns the cell sampling (before `:1548`) and the producer. 035 owns the surface block (`:1553-1592`), the hand-off before `:1617` and the plan call after `:1194`.
- 054 will need the geometry record of a cell's corners. 035's `ClosureBatch` reads `varying` after `src.interpolate`, so it follows whatever 054 puts there.
- Whichever spec merges second rebases its few lines.
- Agreed point for 042: the trees keep a stable node order (A6); neither 035 nor 054 implements tree interpolation.

## Phasing (for `/speckit-tasks`)

**Prerequisite** (coordinator or maintainer, not a task; agents do not merge): sync master into the branch before the first implement phase, so it includes #199, #200, #197 and #115 (merged by 2026-11-23, services 2.6.1) and #204, and whichever of #113 and #203 have landed.

| Phase | Content |
|---|---|
| 0 | Re-read the funnels on the synced tree (R23) |
| 1 | `Spectrum`; `closureAbi.h` layout and registry; builder, flatten, generic evaluator, accessors, arena (US1) |
| 2 | BRDF math library, the five BSDF components, the tree-level table, the numerical tests (US2) |
| 3 | The four layers of `closuremode`; `ClosureFrame`; the plan, step, gate and interim rules; the backend seam; the `cldbl` harness (a wrapper that delegates `execute` to the real `rslo`/`slo` backends; first task: prove an in-process render through it, R-5); the mode and gate tests across the three hiders and both backends (US3, US5) |
| 4 | The Bxdf table, registry, built-ins, `RiBxdf`, RIB, `ribOut`, bindings; IR-026 with A′; the RISpec-modelled built-in parameters, `Cs`/`Os` point inputs, and the Object-block refusal (US4) |
| 5 | The 24 sweep scenes; determinism; docs and doc tests (US6); full validation on macOS and Linux |

No task depends on the merge (FR-030).

## Decisions approved by the maintainer (2026-11-23)

Every item below was approved as recommended. The approved answers are recorded in spec.md ("Plan-stage answers") and drive the contracts.

The RISpec 3.2 review that follows refined three of them, and the maintainer approved those refinements on 2026-11-23:
- Q1 by Q-R1: the default surface is RISpec's implementation default, never a user `Surface`;
- Q7 by Q-R3: the standard shaders' parameters and `Cs`;
- Q9 by Q-R4: `RiBxdf` is refused in an Object block.

The rows below read with those refinements.

### Decided before `/speckit-tasks` (approved)

| # | Question | Recommendation | Where |
|---|---|---|---|
| Q1 | What does FR-026's "default surface" render? **A**: the surface `RiWorldBegin` left (`defaultsurface`, a pre-world `Surface`, or the no-shader path), captured on the Bxdf instance. **B**: always the no-shader path (`Cs*|I.N|`, `Os`). **C**: whatever the Bxdf shadows in the surface slot | **A**: it is what "default surface" names; it needs no shader load and no key; its acceptance is exact (an object with no material statement in a scene with no enclosing `Surface`). C is the cheapest; B is the simplest. **Final (Q-R1)**: A′, which is `defaultsurface` when `RiWorldBegin` bound it, else the no-shader path, never a user `Surface` | R16 |
| Q2 | A Bxdf binds through its own Shading registry into a `CBxdfInstance`, not through `ShadingSystem`/`ShaderGroupBuilder`, and **takes no instance key** (A10 read as "the slot holds one layer") | Yes. Routing through `bind()` advances `shaderOrdinal` and changes every later key and draw (FR-016) | R15 |
| Q3 | The scene mode reaches Shading through a per-frame `ClosureFrame` that the core sets on contexts (beside `setSamplingConfig`), with no services change. The alternative is a services member `closureMode()`, `kRendererServicesVersion` 2.7.0 | The per-frame object; it also carries the cross-thread "once per frame" set | R10 |
| Q4 | `-P:N` with a tree mode: replace A13's default (server renders `color` with a warning) with "same as local" | "Same as local". Servers parse the RIB themselves; A13's default would make `closures` images differ between `-P:N` and local | R12 |
| Q5 | (a) FR-026 dedup key: per Bxdf name per frame, or one per frame. (b) `both` with a capable shader and no consumer: silent, or one warning per frame | (a) per Bxdf name; (b) silent (FR-021 says it behaves as `color`) | R17 |
| Q6 | Two versions (closure ABI, which includes the BSDF table, and the Bxdf table), or a third for the BSDF table | Two (FR-009, SC-010 name two) | R14 |
| Q7 | The built-in Bxdf names (`diffuse`, `plastic`, `conductor`, `dielectric`), defaults and ranges, and the conductor's Fresnel convention (`microfacet` with `eta = 0` read as `F = 1`; the alternative is adding a conductor component beyond the clarified first registry) | As tabled; the `eta = 0` convention | R21, contracts/bxdf.md |
| Q15 | **Spec deviation**: SC-006 says trees, evaluations and samples are bit-identical "over at least 24 new determinism-sweep entries", but the sweep runs `orender` images and cannot observe a tree | Amend SC-006 to "24 sweep image entries plus 6 in-process tree and BSDF checks": `ClosureDet_ThreadIdentity_{reyes,zbuffer,raytrace}`, `ClosureDet_BatchShape`, `ClosureDet_HiderParity`, `ClosureDet_BsdfRepeat`. The sweep entries prove only the interim behaviours (shipped shaders are never closure-capable): accepted 2026-11-23 | R22 |

### Confirmations (approved)

| # | Question | Recommendation | Where |
|---|---|---|---|
| Q8 | A bad `closuremode` value keeps the previous value on **both** sides (`CODE_RANGE`). Only the option side of `shaderformat` does that today; its attribute side falls back to `"slo"` (Discrepancy D-2) | Keep the previous value on both sides; `shaderformat` unchanged | R11 |
| Q9 | `Bxdf` handle optional (the `LightSource` grammar rule; a bare-string first parameter needs brackets) or mandatory (the RIS convention) | Optional, as FR-013 writes it | R20 |
| Q10 | Gated `occlusion()`/`indirectdiffuse()` return 0; `gather()` runs with 0 samples. FR-019 no longer names `indirectspecular`: the builtin does not exist in this tree (Discrepancy D-1, resolved in spec.md) | As stated | R9 |
| Q11 | One tree per real point (the raytrace derivative helpers excluded) for SC-002's count; trees delivered for every ray type, with `rayType` in the record; a `closures` object casts shadows and appears in reflections with `Ci = 0` and its shader's own `Oi` (Q-R5); SC-007's scene keeps it out of the `color` objects' rays | As stated | R19 |
| Q12 | The tree budget: 1024 bytes per real point | 1024 | R4 |
| Q13 | `test_shader_parameter_bxdf_ignored.cpp` (spec 025, "until spec 035") keeps its assertions, because the checked accessor ignores the RSL instance it binds. Only its header comment changes, and a new test, `ShadingParity_ShaderParameterBxdfGoverned`, pins the new rule | Accept the comment-only edit | R15, R16 |
| Q14 | The interim-rule IDs (IR-017, IR-018, IR-026) appear in code, tests and the guide, not in the message text | Not in the text | R17 |

## RISpec 3.2 conformance

The maintainer's rule (2026-11-23): where RISpec 3.2 defines a behaviour, the decision follows it. Where RISpec is silent, the decision is kept and the row says "not defined". Bxdfs, closures and integrators all postdate 3.2.

- **Source**: `docs/references/RISpec3_2.md`. Section numbers are RISpec's own.
- **Verdicts**: "Conforms" or "Not defined". The review first found five conflicts. Each went back to the maintainer as a question (Q-R1 to Q-R5, below), and each was approved in RISpec's favour on 2026-11-23. Every row now conforms or is not defined.

| # | Decision | RISpec 3.2 | Verdict |
|---|---|---|---|
| 1 | Default mode `color` is today's code, byte for byte (FR-018, FR-027) | §4.2 "All geometric primitives use the current surface shader for computing the color"; §12.1 the surface shader computes `Ci`, `Oi` | Conforms |
| 2 | `closuremode` as `Attribute "shade"` | §4.4: implementation-specific attributes "that affect primitive appearance" are set with `RiAttribute` | Conforms. `both` and `closures` depart from §4.2/§12.1 only when the user opts in through this implementation-specific attribute |
| 3 | `closuremode` as `Option "shade"`: a new implementation-specific option | §4.1.4 `RiOption`; §4 "When `RiWorldBegin` is invoked, all rendering options are frozen". openRender rejects `RiOption` inside a world block (`VALID_OPTION_BLOCKS`, `ri.cpp:87`), and `ClosureFrame` reads the option at frame begin | Conforms |
| 4 | Options saved and restored by `FrameBegin`/`FrameEnd` | §4 "The values of all of the rendering options are saved when `RiFrameBegin` is called, and these values are restored when `RiFrameEnd` is called" | Conforms: `COptions`' copy carries `closureMode` |
| 5 | The mode and the Bxdf binding are saved and restored by `AttributeBegin`/`AttributeEnd` and by every begin-end block that saves attributes | §4 "All begin-end pairs (except `RiTransformBegin`... and `RiMotionBegin`...) implicitly save and restore attributes"; §4.2 `RiAttributeBegin` | Conforms |
| 6 | An instance's mode and Bxdf, and US3 AS6's "through `ObjectBegin`/`ObjectInstance`" | §5.10 "All of an object's attributes are inherited at the time it is instanced, not at the time at which it is created"; `RiObjectInstance`: "The object inherits the current set of attributes". openRender does this (`CDelayedInstance::instantiate`) | Conforms. The spec's earlier wording ("the mode of their definition scope") was corrected by Q-R2 (approved 2026-11-23) |
| 7 | `RiBxdf` is refused inside an `ObjectBegin` block: its own validity mask is `VALID_ATTRIBUTE_BLOCKS` without `RENDERMAN_OBJECT_BLOCK` (Q-R4) | §5.10 "shader assignments or other attributes are not allowed within an Object block" | Conforms. `Surface` and `Attribute` still accept it there, an older deviation outside 035 that the coordinator files as its own issue |
| 8 | Bad value: `CODE_RANGE`, keep the previous value; unknown token: `CODE_BADTOKEN` (Q8) | §7.2 an error goes to the error handler; with `RiErrorPrint` "the rendering system will attempt to ignore the erroneous information and continue rendering". The codes are `ri.h`'s `RIE_*` | Conforms |
| 9 | `closuremode` pre-declared as `string`; inline `"string closuremode"` accepted | §2.1/§4: parameter tokens are declared or inline; implementations may predeclare their own | Conforms |
| 10 | FR-026's substitute is A′ (Q-R1): the `defaultsurface` instance when `RiWorldBegin` bound it, otherwise the no-shader path; never a user-bound `Surface` | §4.2.4 "Initially the current surface shader is set to an implementation-dependent default surface shader (but not "null")"; Table 4.6 "Surface: default surface (implementation-dependent)"; §4 `RiBegin` sets "all graphics state variables to their default values" | Conforms (Q-R1 approved; option A's fallback to a user's pre-world `Surface` is gone) |
| 11 | The no-shader path (`Cs*\|I.N\|`, `Os`) as a substitute when no `defaultsurface` was loaded | §4.2.4 and `RiSurface`: an undefined shader falls back to "some implementation-dependent default surface shader (but not "null")" | Conforms: the path writes a non-null colour, so it is an implementation-dependent default |
| 12 | IR-017: a non-capable Surface under `both`/`closures` renders as `color` | §4.2, §12.1 | Conforms |
| 13 | `closures` on a capable Surface (IR-018, and with a consumer): `Ci = 0`, `Oi` = the shader's own `Oi` (Q-R5). A Bxdf with a consumer: `Ci = 0`, `Oi = Os` | §12.1 "The color and opacity of the outgoing ray is Ci and Oi... If either Ci or Oi are not set, they default to black and opaque". Appendix A: every standard surface writes `Oi = Os` | `Ci = 0`: not defined (closure modes are post-3.2). `Oi`: conforms. The surface shader's `Oi` is kept (§12.1, Q-R5 approved), and a Bxdf with no shader writes `Os`, as Appendix A's shaders do |
| 14 | The lighting gate: under `closures` no light runs; `illuminance` runs zero times; `ambient`/`diffuse`/`specular`/`phong` give 0 | §4.2.3 "The lights in this list illuminate subsequent surfaces"; §13.3: `illuminance` iterates "every nonambient light source"; §15.6 `diffuse`/`specular`/`phong` are defined through `illuminance` and `ambient()` through the ambient lights | Conforms. The result equals RISpec's definitions over an empty light list, and the list is emptied only under the opt-in attribute (row 2). `color` and `both` are unchanged |
| 15 | `specularbrdf()` is not gated | §15.6: a pure function of `L`, `N`, `V`, roughness; it touches no light | Conforms |
| 16 | `trace()`, `environment()`, `shadow()`, `texture()` are not gated | §15.6 `trace()`, §15.7 | Conforms |
| 17 | `occlusion()`, `indirectdiffuse()`, `gather()` gated to 0 and 0 samples (Q10) | Not in RISpec 3.2 (no occurrence of `gather`, `occlusion` or `indirectdiffuse` in the text) | Not defined; kept |
| 18 | Light categories (`__category`) untouched; the gate runs no light of any category | §13.3 defines `__category` and the `-` prefix | Conforms. The `color`/`both` baseline is the bug-fix session's (#199, #200), unchanged by 035 |
| 19 | `__nondiffuse` / `__nonspecular` | Not in RISpec 3.2 (a PRMan convention) | Not defined; untouched (#197 is the baseline) |
| 20 | A bound area light keeps its emitter registry and draws; a `closures` receiver draws none | §4.2.3 defines the area light and its primitive list; nothing about receivers | Conforms (binding unchanged) |
| 21 | `Spectrum` of 3 channels; node weights are 3 floats | §4.1.3 `RiColorSamples`: without the Spectral Color capability, an implementation "will still accept multiple component colors, but will immediately convert them to RGB... and do all internal calculations with 3 component colors" | Conforms. Decision added: Bxdf `color` parameters are read with the current `ColorSamples` count and converted with `COptions::convertColor`, the path `RiColor` takes (`rendererContext.cpp:2281`) |
| 22 | `diffuse`/`plastic`/`conductor` take RISpec `matte`/`plastic`/`metal`'s names and defaults, with the base colour from `Cs`, `Oi = Os`, no `Ka`, and `alpha = roughness` (Q-R3) | Bxdfs are not defined. But §4.2.4 Table 4.8 and Appendix A define `matte`, `metal` and `plastic`: their colour is the primitive's `Cs` (§4.2.1 "All geometric primitives inherit the current color and opacity"), their parameters are `Ka`/`Kd`/`Ks`/`roughness`/`specularcolor` with stated defaults, and `roughness` feeds an implementation-dependent `specularbrdf` (§15.6) | Conforms (Q-R3 approved). `Ka` is dropped because ambient is lighting work with no closure lobe. `ior` on `plastic` and the Oren-Nayar `roughness` on `diffuse` are extensions RISpec does not define. `dielectric` has no RISpec counterpart: not defined, kept |
| 23 | Bxdf `N` faceforwarded and normalised | Appendix A: every standard surface uses `faceforward(normalize(N), I)` | Conforms |
| 24 | The `Bxdf` statement, its handle, its registry, refusals, `sl:`/`osl:` | Not defined (RIS, post-3.2). The C binding follows §2.1's conventions (`RiBxdf(..., RI_NULL)` and `RiBxdfV(n, tokens, params)`) | Not defined; kept |
| 25 | Surface/Bxdf last-bound record (D5); a Bxdf-governed primitive does not run the current surface shader | §4.2: every primitive uses the current surface shader. A scene without `Bxdf` keeps that | Not defined (Bxdf is post-3.2); kept. Holds for every RISpec scene |
| 26 | `ribOut` writes the statements at their positions | §2.2 bytestream protocol: one request per keyword with its arguments | Conforms; `Bxdf` is an extension keyword |
| 27 | `-P:N` same as local (Q4), interim rules and dedup (Q5), versions (Q6), budget (Q12), interim IDs (Q14), consumer, ABI layout, BSDF conventions | Not defined | Not defined; kept |

### Conflicts raised by the RISpec review: all approved as recommended (maintainer, 2026-11-23)

Each conflict below is resolved in favour of RISpec. The resolutions are folded into spec.md, research.md, data-model.md, quickstart.md and the contracts.

| # | Conflict | Approved answer | Applied to |
|---|---|---|---|
| Q-R1 | Under Q1 option A, a Bxdf object can fall back to a user's pre-world `Surface`. RISpec's "default surface shader" is the implementation-dependent initial one, never a user-bound shader (§4.2.4, Table 4.6) | **A′**: the substitute is the implementation's default surface: the `defaultsurface` instance when `RiWorldBegin` bound it, otherwise (a pre-world `Surface` was given, or `defaultsurface` could not be loaded) the built-in no-shader path, which RISpec also allows (row 11). Never a user-bound `Surface`. No extra shader load and no instance key (Q2 holds). Acceptance: equal to the object with no material statement in a scene with no `Surface` anywhere (`defaultsurface` case), or to the same object under the no-shader path | `interimSurfaceFor()`, and the core's capture at `RiWorldBegin`: capture only a `setDefaultSurface()` binding |
| Q-R2 | The spec's edge case and US3 AS6 say instances take the mode "of their definition scope". RISpec §5.10 says at `ObjectInstance` time, and openRender already does that | Follow RISpec. An instance's `closuremode` and its Bxdf binding are those current at `ObjectInstance`. Reword the edge case and AS6; `ClosureMode_Scope` tests instance-time inheritance | spec text and one test; no code beyond what instancing already does |
| Q-R3 | The built-in Bxdfs that model standard shaders take their colour from `baseColor` and use their own names and defaults (`roughness` 0.3 for `plastic`). RISpec's `matte`/`plastic`/`metal` use `Cs`, `Kd`/`Ks`/`roughness`/`specularcolor` and defaults `Kd` 1 (matte), `Kd` .5/`Ks` .5/`roughness` .1 (plastic), `Ks` 1/`roughness` .1 (metal) | For `diffuse`, `plastic` and `conductor`, take RISpec's parameter names and defaults: `diffuse`: `Kd` 1; `plastic`: `Kd` .5, `Ks` .5, `roughness` .1, `specularcolor` 1; `conductor`: `Ks` 1, `roughness` .1. The base colour is the point's `Cs` (§4.2.1). `Oi = Os` (Appendix A). `Ka` is not a parameter: ambient is lighting work with no closure lobe, so it gets the unknown-parameter diagnostic. Keep `ior` (`plastic`, `dielectric`) as an extension. Map `roughness` to the GGX `alpha = roughness`, not `roughness^2` (see the note below the table). `Cs` and `Os` are point inputs, like `N`, so the Bxdf stays declarative (FR-015). `diffuse`'s Oren-Nayar `roughness` becomes an extension parameter. `dielectric` is unchanged | `builtinBxdfs.cpp`'s four tables; `OrBxdfPoint` gains `Cs` and `Os` (the shade step fills them from `varying`); contracts/bxdf.md; research R21 |
| Q-R4 | `Bxdf` (and `Surface`, `Attribute`) are accepted inside an `ObjectBegin` block. RISpec §5.10 forbids shader and attribute assignments there | For the **new** statement, follow RISpec: `RiBxdf` is not valid in an Object block (a new validity mask that excludes `RENDERMAN_OBJECT_BLOCK`, so `check()` reports it as any out-of-block request). Leave `Surface` and `Attribute` as they are: changing them changes existing scenes. The coordinator files that older deviation as a separate issue; 035 does not fix it | `ri.cpp` (one mask); one test |
| Q-R5 | IR-018 and the `closures` path with a consumer replace the `Oi` a capable Surface shader computed with the object's `Os`. RISpec §12.1 makes `Oi` the surface shader's output, and the gate does not touch the non-lighting work that computes it (a texture-driven cut-out, for instance) | Keep the shader's own `Oi` for a capable Surface under `closures` (`Ci = 0` as approved). Use `Os` only where no shader runs (a Bxdf, as Appendix A's shaders do). This amends the clarify answer of FR-018 for its opacity half | `runSurfaceStep` (one assignment); FR-018's wording; `Closure_Interim_IR018_*` expectations |

**Note on Q-R3's roughness mapping:**
- RISpec leaves `specularbrdf` implementation-dependent (§15.6).
- openRender's highlight is `pow(N.H, 10/roughness)` (CLAUDE.md gotcha 25): exponent 100 at the default 0.1, which corresponds to a microfacet `alpha` of about 0.14.
- `alpha = roughness` (0.1) therefore keeps a similar highlight width, while `roughness^2` (0.01) would be nearly a mirror.

### Derived, flagged for visibility

- A Bxdf-governed object **with** a consumer writes `Ci = 0`, `Oi = Os`, and its Surface slot does not run (R18). RISpec row 13: this conforms.
- A capable Surface under `closures` writes `Ci = 0` and keeps its shader's `Oi` (Q-R5).
- A Bxdf bound before `WorldBegin` receives the `defaultsurface` handle lazily when `RiWorldBegin` binds it (analyze B2, maintainer 2026-11-23), so it renders RISpec §4.2.4's initial default like the no-material object.
- Tree weights carry no `Os`: the consumer applies `Oi = Os` as Appendix A does (analyze B1).
- **`plastic`'s `ior`** (R21, maintainer 2026-11-23): defaults to 0, meaning no Fresnel term, so the highlight matches the standard `plastic` shader. `ior > 0` enables Fresnel; a negative value is refused.
- **Roughness** (R21, maintainer 2026-11-23): `alpha = roughness` for all four built-ins, `dielectric` included.
- FR-018's "no extra call is made" is read as "no out-of-line call, no tree, no consumer call". The `color` fast test is inline in `shade()` (R8).

## Risks

| ID | Risk | Mitigation |
|---|---|---|
| R-1 | `shade()` is shared with spec 054; a merge conflict there could silently change the `color` path | 035's edit is three call sites at disjoint ranges; `SC-001`'s full suite and sweep are re-run after any rebase |
| R-2 | The node layout is derived from reference §2.4, not measured against `oslclosure.h`; a mismatch surfaces only in spec 049 | `Closure_Layout` pins the block offset (16) as the compatibility figure and prints the table for 049; the layout is a closure-ABI 1.x item, which may still step MINOR while 1.x is unstable |
| R-3 | The bug-fix session (#200, #197, #115, #113) edits `shading.{h,cpp}` near `jitIlluminanceBegin` | 035 gates only the shared funnels; a master sync is Phase 0; the funnels are re-read after it |
| R-4 | Byte identity of the IR-026 substitute (A′) depends on `usedShaderParameters` matching the "no material" case exactly (the derivative path changes `du`/`dv` if extra parameters enter) | `Closure_Interim_IR026_ByteIdentity` renders both forms on all three hiders, `rslo` and `slo` |
| R-5 | No in-process render through a registered third backend exists yet (`ShadingSystem_ThirdBackend` binds and executes it at system level only) | The first task of Phase 3 builds and proves the harness before any mode test depends on it |
| R-6 | The chi-square and furnace tests could be flaky if their tolerances are guessed | The tolerances come from each estimator's standard error with fixed seeds, are written in the test and are never widened (SC-004) |
| R-7 | Beckmann visible-normal sampling (Heitz and d'Eon 2014) is intricate | Fall back to `D*cos` sampling, which changes no interface (R7) |
| R-8 | `microfacet`'s `refract == 2` meaning is recalled, not verified against `stdosl.h` | Marked "verify at implement"; spec 049 compares; the value is a registry detail under the unstable 1.x |
| R-9 | A `closures` object visible in `color` objects' shadows or reflections would make SC-007's "0 pixels" fail by construction | The scene is designed to isolate it (Q11); SC-007 now states the isolation (maintainer, 2026-11-23) |

## Complexity Tracking

No constitution violation; nothing to justify.
