# Implementation Plan: Graphics State Handles

**Branch**: `025-graphics-state-handles` | **Date**: 2026-10-25 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/025-graphics-state-handles/spec.md`. Clarifications
from session 2026-10-25 are applied as decided.

## Summary

Graphics State (`src/ri/state`) stops holding other domains' objects. It resolves DB-008 and moves
the registry part of DB-021. The work, in the order the brief (plan step V5) gives:

1. **Handles.**
   - Two empty State-owned bases, `CShaderGroupRefTarget` and `CMapRefTarget`.
   - A header-only RAII handle, `ShaderGroupRef`/`MapRef`.
   - `CShaderInstance` and `CPhotonMap` derive from the new bases, which the dependency table
     allows (Shading → State and Hiders → State).
   - The five shader slots, a new `bxdf` slot and `COptions::imager` hold `ShaderGroupRef`; the
     photon maps hold `MapRef`; light-list entries stay non-owning.
   - Each resolving domain gets its object back through its own inline `static_cast` accessor:
     `shaderInstanceOf`/`lightInstanceOf` in Shading, `photonMapOf` in Hiders.
2. **Parameter logic.** `checkParameters()`'s union and `findParameter()`'s precedence move
   verbatim into Shading. State reaches them through an installed function-pointer hook, as with
   `variableLookup.h`. The renderer core installs it with the shading services at `RiBegin`.
   `CAttributes::findParameter` stays as a forwarder, so `pl.cpp`, `blobby.cpp` and `rib.y` are not
   edited.
3. **The `bxdf` slot, the last-bound record and the scoped pattern-node table** (D5, FR-010 to
   FR-015). The table is copy-on-write, so scoping follows from the existing attribute clone.
4. **A state vocabulary header** with scoped `EDisplayParameterType` and `EBlobbyOrder`. The core
   converts explicitly where the two vocabularies meet.
5. **The channel registry** becomes the State class `CChannelRegistry`.
   - Its declare-from-string path reaches `CRenderer::declareVariable` through a second hook in
     `variableLookup.h`.
   - `CRenderer::retrieveDisplayChannel` remains an out-of-line forwarder, so `texture3d.cpp` has
     no diff.

Tests are written first:
- a State-only test that compiles `src/ri/state` with a narrow include path and links only
  libRIBCommon;
- a vocabulary consistency test;
- a parameter-equivalence test, whose part A is an API-neutral oracle that is green on the
  starting tree;
- two characterization tests, green on both trees: light order and ribOut statement order.

Afterwards the guardrails are updated:
- the allowlist loses its 7 DB-008 lines and gains none;
- DB-008 moves to Resolved;
- DB-021 stays Open, with revised evidence.

## Technical Context

**Language/Version**: C++20 (`-Werror`). CMake ≥ 3.19.

**Primary Dependencies**: no new dependency. It uses `libRIBCommon` (`CRefCounter`, diagnostics,
`CVariable`/`parseVariable`, `CTrie`/`CArray`), `libshader_shading` and `libRIBRender`.

**Storage**: N/A. In-memory state only.

**Testing**: ctest.
- New label `state`: `State_Isolation`, `State_VocabularyConsistency` and
  `State_LightOrderCharacterization`.
- `shading_parity`: `ShadingParity_ShaderParameterEquivalence`.
- `RibOut_ShaderStatementOrder`, under `tests/unit/rib_parse`.
- Also `architecture`, `visual`, `^determinism$`, the full suite, and the sweep once before merge.

**Target Platform**: macOS (Apple Silicon, Clang, vcpkg) and Linux (Ubuntu 24.04, GCC 13,
`-Werror`).

**Project Type**: renderer library and CLI.

**Domains touched**:

| Domain | What changes |
|---|---|
| Graphics State | the main subject |
| Shading | the parameter logic, the resolution accessors, a new base for `CShaderInstance` |
| Hiders | `CPhotonMap`'s base, `photonMapOf` at 3 files |
| Geometry | `surface.cpp:605` presence check; transitive-include fixes only |
| Renderer core | `rendererContext.cpp`, `renderer.cpp`, `rendererDeclarations.cpp`, `rendererDisplay.cpp`, `rendererShadingServices.cpp`, `rendererStatics.cpp`, `geometryDispatch.cpp`, `renderer.h` |
| Consumers | tests; the preview, Python and Lua bindings only if transitive includes need it |

Scene Interface (`rib.y`, `ribOut`), Texturing (`texture3d.cpp`) and Display Outputs (`dsply.h`)
are **not edited**.

**Performance Goals**: no measurable change (SC-005). The slot accessors return `const&`, the
resolution is an inline `static_cast`, and the hot loops touch no reference count. The parameter
set stays a stored field.

**Constraints**:
- byte-identical renders (FR-022), including today's light order (research R5);
- no threshold widening;
- no task depends on the merge;
- `cpujob` for every build and test: one job at a time machine-wide, `nice -n 19`, at most `-j4`.

**Scale/Scope**:
- about 20 source files changed (roughly 60 slot sites);
- 7 State files added: `stateHandles.h`, `stateVocabulary.h`, `patternTable.h`,
  `shaderParameterHooks.{h,cpp}` and `channelRegistry.{h,cpp}`;
- 1 Shading header (`shaderRefs.h`) and 1 Shading unit (`shaderParameters.{h,cpp}`) added;
- 5 test executables and their fixture shaders added;
- the architecture data files and the registry updated.

**Measurements**: the builds needed for the baselines have not run. Quickstart §0 lists them, and
none blocks the design:
- the architecture report on `dfffba0`;
- the suite baseline;
- the part A oracle and its mutation;
- the characterization tests;
- the timings of the slowest three scenes;
- the spec 024 import count.

## Constitution Check

*GATE: must pass before Phase 0 research; re-checked after Phase 1 design.*

| Principle | Status | Notes |
|---|---|---|
| I. Clean code | PASS | Five near-identical attach/detach blocks collapse into one RAII handle. Private slots with one binding API replace public fields. The parameter logic lives in one Shading unit. |
| II. C++20 / C17 | PASS | Scoped enums, `explicit operator bool`, `std::vector`/`std::string` in the pattern table. No compiler extensions. |
| III. TDD (non-negotiable) | PASS (by plan order) | See "Test design". The red tests fail by build failure on the starting tree (as in 024's T-P5), shown before their code lands. Two characterization tests are green on both trees by design and are labelled so. The FR-007 oracle is proven against the starting tree, and a mutation is shown red. |
| IV. CLI | N/A | No CLI change. |
| V. Minimal dependencies | PASS | None added. |
| VI. Linux and macOS | PASS | Both platforms are measured before and after (quickstart §0, §2, §4). Linux builds with GCC 13 `-Werror`. |
| VII. Docs | PASS | `DOMAIN_BOUNDARIES.md`, the allowlist header, DEVNOTES, and CLAUDE.md's Architecture and Known gotchas sections. No user-facing site page changes, because rendering does not change. |
| VIII. Domain separation | PASS | See below. |

**Principle VIII — Domain Separation and Modular Boundaries** (every plan):

- [x] **Domains touched are named**:
  - Graphics State (`src/ri/state/`);
  - Shading (`src/libshader/shading/`);
  - Hiders (`photonMap.h`, `photon.cpp`, `radiance.cpp`, `irradianceDispatch.cpp`);
  - Geometry (`surface.cpp`; includes only);
  - the renderer core (`src/ri/render/`);
  - Consumers (`tests/`).

  Scene Interface, Texturing and Display Outputs are read, not edited.
- [x] **No new cross-domain dependency outside the table, and none on the renderer core from
      inside a domain.**
  - **Added**:
    - Shading → State (`shader.h` includes `stateHandles.h`);
    - Hiders → State (`photonMap.h` includes `stateHandles.h`);
    - Core → Shading (`shaderParameters.h`, `shaderRefs.h`);
    - Core → State (`channelRegistry.h`, `shaderParameterHooks.h`).

    All are allowed.
  - **Removed**: the 7 State → {Shading, Display Outputs, Geometry, Hiders} include edges.
  - **The two hooks** are State-owned function-pointer interfaces, implemented by Shading and the
    core and installed by the core. They add no edge (rule 2's inversion).
  - **The `CRenderer::retrieveDisplayChannel` forwarder stays out of line**, so Texturing's
    edges are unchanged.
- [x] **Every added or altered cross-domain dependency has its interface change specified**:
  - [contracts/state-handles.md](contracts/state-handles.md);
  - [contracts/attribute-scope.md](contracts/attribute-scope.md);
  - [contracts/shader-parameter-hooks.md](contracts/shader-parameter-hooks.md);
  - [contracts/channel-registry.md](contracts/channel-registry.md);
  - [contracts/state-vocabulary.md](contracts/state-vocabulary.md).
- [x] **No hider-specific special case**: the map handle is generic. Hiders resolve it themselves,
      and State names no hider.
- [x] **Plug-in interfaces**: none changed. `dsply.h` and `blobbyField.h` stay byte-identical, so
      there is no Semantic Versioning bump (DB-003 and DB-014 untouched).
- [x] **Registered violations are not extended, and reductions are noted**:
  - **DB-008** goes from 7 edges to 0 and is Resolved.
  - **DB-021** keeps its 1 edge and stays Open. Only its evidence text changes; the registry
    itself is no longer core state.
  - **DB-010** keeps the same 35 edges. Its evidence notes that the forwarder now forwards to the
    State registry.
  - **DB-013** is not extended. The unchecked cast moves verbatim from State into Shading's
    `lightInstanceOf()`, and its History says so.
  - **DB-017** is not extended. Nothing is added to the `*Dispatch`/`*DispatchStub` pairs; the
    parameter hook is installed from `rendererShadingServices.cpp`, not from
    `rendererDeclarationsDispatch.cpp`.
  - **DB-023** is not extended. The State-only test proves State builds on a narrow include path;
    see Q3 for doing the same in production.
  - **DB-018** is touched but not extended. The message-passing functions it names (on `dfffba0`
    at `shading.cpp:1556-1613,2753-2770`; the registry's own line numbers are older) change form from `attributes->surface` to
    `shaderInstanceOf(attributes->surface())`. Shading still reads the same fields directly; no
    new site is added.
  - **The allowlist** only shrinks: 7 lines are deleted, and none is added or swapped.

**Post-design re-check (after Phase 1)**: PASS, unchanged. The contracts add no edge beyond the
allowed ones listed above.

## Project Structure

### Documentation (this feature)

```text
specs/025-graphics-state-handles/
├── spec.md, plan.md, research.md, data-model.md, quickstart.md
├── contracts/state-handles.md, attribute-scope.md, shader-parameter-hooks.md,
│             channel-registry.md, state-vocabulary.md
├── checklists/requirements.md
└── tasks.md                    # /speckit-tasks (not created here)
```

### Source code touched

```text
src/ri/state/
├── stateHandles.h              # NEW: CShaderGroupRefTarget, CMapRefTarget, TStateRef, ShaderGroupRef, MapRef
├── stateVocabulary.h           # NEW: EDisplayParameterType, EBlobbyOrder
├── patternTable.h              # NEW: CPatternTable (copy-on-write)
├── shaderParameterHooks.{h,cpp}# NEW: SShaderParameterHooks, install/get
├── channelRegistry.{h,cpp}     # NEW: CChannelRegistry, begin/end/channelRegistry()
├── variableLookup.{h,cpp}      # + installVariableDeclaration / declareVariableThroughHook
├── attributes.{h,cpp}          # - shader.h, photonMap.h; private ShaderGroupRef slots + bxdf, record,
│                               #   patterns, MapRef maps, non-owning light entries, bind*/setDefaultSurface,
│                               #   checkParameters/findParameter forward to the hook
└── options.{h,cpp}             # - shader.h, dsply.h, blobbyField.h; ShaderGroupRef imager; EDisplayParameterType
src/libshader/shading/
├── shader.h                    # CShaderInstance : public CShaderGroupRefTarget
├── shaderRefs.h                # NEW: shaderInstanceOf, lightInstanceOf (DB-013 cast)
├── shaderParameters.{h,cpp}    # NEW: usedShaderParameters, findShaderParameter, shaderParameterHooks()
├── shading.cpp, executeMisc.cpp, execute.cpp   # slot and light reads through the accessors
└── CMakeLists.txt              # + shaderParameters.cpp
src/ri/hiders/
├── photonMap.h                 # CPhotonMap : … public CMapRefTarget; + photonMapOf
└── photon.cpp, radiance.cpp, irradianceDispatch.cpp   # photonMapOf(attributes->globalMap)
src/ri/geometry/surface.cpp     # attributes->displacement() presence check
src/ri/render/
├── renderer.h                  # - declaredChannels, displayChannels, declareDisplayChannel x2, resetDisplayChannelUsage
├── rendererStatics.cpp         # - the two channel statics
├── rendererDeclarations.cpp    # begin/endChannelRegistry, declaration hook, out-of-line retrieveDisplayChannel forwarder
├── rendererShadingServices.cpp # install/uninstall the shader-parameter hooks
├── rendererContext.cpp         # Ri*V binders, WorldBegin defaults, RiImagerV, RiIlluminate, RiDisplayChannelV, display-param types
├── rendererDisplay.cpp         # registry calls; ParameterType conversion
├── renderer.cpp                # lazy MapRef binding; imagerShader = shaderInstanceOf(o->imager)
└── geometryDispatch.cpp        # attributes->displacement() presence check
src/ri/hcshader.{h,cpp}         # only if the CShaderInstance base change needs it (expected: none)
tests/unit/state/               # NEW: test_state_isolation.cpp, test_state_vocabulary.cpp,
│                               #      test_light_order.cpp, counting test doubles, CMakeLists.txt
tests/unit/rib_parse/test_ribout_shader_order.cpp   # NEW
tests/shading_parity/test_shader_parameter_equivalence.cpp (+ fixture .sl)  # NEW
tests/unit/csg/test_interior_exterior_selection.cpp, tests/imager/*.cpp      # reach the object through the handle
tests/architecture/allowlist.txt                     # - 7 DB-008 lines
DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md, DEVNOTES.md, CLAUDE.md
```

**Structure decision**: the existing layout is kept.
- New State code goes in `src/ri/state/` and new Shading code in `src/libshader/shading/`. Both
  directories are already classified in `domain-map.txt`, so no domain-map change is needed.
- `tests/unit/state/` is under `tests/`, which archcheck does not analyse as a domain.
- The State-only test compiles the State sources a second time with a narrow include path. This
  follows the `tests/framebuffer` precedent; see research R11 and Q3.

## Design

The details are in [research.md](research.md). In brief:

1. **Ownership (R1)**: three kinds of holder.
   - The shader slots and the imager **adopt**.
   - Copies and the photon maps **retain**.
   - Light entries are **non-owning**, as today.

   The copy constructor's seven manual `attach` lines and the destructor's matching `detach`
   lines are deleted, because the handle members do this now. A State-only counting double checks
   every step against data-model.md's mapping table (US1.3).
2. **Handles and resolution (R2, R3)**:
   - Only inline `static_cast` accessors owned by the resolving domain get the object back.
   - The DB-013 cast moves verbatim into `lightInstanceOf()`, so pointer values are unchanged.
   - No out-of-line State symbol is added for Shading to import.
3. **Parameters (R4)**:
   - The installed hook covers both operations: `0`/`nullptr` with no hook, and the bxdf slot is
     excluded.
   - Installed in `rendererShadingServicesInit()`, never in `rendererDeclarations.cpp`, which
     ribVector also compiles.
   - The `bind*` setters recompute exactly where the core's separate calls did.
4. **Record and table (R6, R7)**:
   - Slots are private.
   - `bindSurface`/`bindBxdf` set the record even when the handle is empty.
   - `setDefaultSurface` (`RiWorldBegin` only) leaves the record untouched (FR-024).
   - The pattern table is copy-on-write, so it is O(1) per clone, and value semantics give FR-013's
     scoping.
5. **Light order (R5)**: today's reversal on clone and on restore is preserved and pinned by a
   characterization test, as a finding (see Q1).
6. **Vocabulary (R9)**: scoped enums, with explicit core conversions and `static_assert`s, plus a
   runtime consistency test.
7. **Registry (R8)**:
   - `CChannelRegistry` keeps the same storage, error strings, codes, order and ownership.
   - A session instance follows `initDeclarations`/`shutdownDeclarations`.
   - Declaration reaches the core through a `variableLookup.h` hook.
   - The out-of-line forwarder keeps `texture3d.cpp` unchanged.
8. **Transitive includes (R12)**: fixed only with infrastructure or own-domain headers. Stop and
   report if Geometry, Texturing or Scene Interface would need a foreign header.
9. **Guardrails and docs (R13)**: as in the Constitution Check.

**Order of work**:

1. **Baselines** (quickstart §0).
2. **Red tests** (§1).
3. **The parameter hook first.** The slots are still raw `CShaderInstance*`.
   - The logic moves to Shading, and State forwards through the hook.
   - `ctest -L architecture` is unchanged and green.
   - This must come before the handles: while `checkParameters()`/`findParameter()` live in
     `attributes.cpp` they need `CShaderInstance`'s full definition. Dropping `shader.h` from
     `attributes.h` first would force a new, non-allowlisted `attributes.cpp -> shader.h` edge.
4. **Handles, maps and the vocabulary**, with no behaviour change:
   - delete the 7 DB-008 allowlist lines first, so T-A1 is red;
   - the State headers;
   - the `CShaderInstance`/`CPhotonMap` bases;
   - every slot site;
   - `ctest -L architecture` goes green with all 7 State edges gone.
5. **The record and the pattern table.**
6. **The registry.**
7. **Registry text and docs, then Linux.**

Each phase ends with `ctest -L architecture` and the suite labels its changes touch. Phases 3 and 5
to 7 keep the check green throughout.

## Test design (red first)

| Id | Test | Red on `dfffba0` because | Covers |
|---|---|---|---|
| T-S1 | `State_Isolation` (label `state`): State sources and counting doubles, narrow include path, links only `openrendercommon` | build failure: `shader.h`, `dsply.h`, `blobbyField.h` and `photonMap.h` do not resolve; the new headers do not exist | US1.3, US3.1–3.4, US4.1–4.4, US5.1–5.3, FR-001, FR-004, FR-010–FR-017, SC-006, the empty-bind record |
| T-S2 | `State_VocabularyConsistency` (label `state`) | build failure (the header is missing); then a value mutation is shown red | FR-008, SC-007 |
| T-P1 | `ShadingParity_ShaderParameterEquivalence` part B | build failure (the `libshader::` API is missing) | FR-005–FR-007 |
| T-P1a | the same test, part A (API-neutral oracle) | **green by design** on both trees; a precedence-swap mutation is shown red | FR-007 ("fails on a build where the answer differs"), US2.2 |
| T-L1 | `State_LightOrderCharacterization` (links ribRender): lights in world and nested blocks, `RiIlluminate` off/on, a duplicate add, pointer identity against `RiLightSource` handles | **characterization, green on both trees** | FR-004, US2.3, R3, R5 |
| T-R1 | `RibOut_ShaderStatementOrder`: the seven shader statements in several orders among other attribute statements, written by ribOut and compared line by line | **characterization, green on both trees** (clarification Q3) | US3.5, FR-023 |
| T-A1 | `ctest -L architecture` after deleting the 7 DB-008 lines at the start of the handles phase (step 4) | 7 `VIOLATION` lines until that phase ends; any other violation means the design is wrong | FR-019, SC-001, SC-002, SC-009 |

Existing guards whose assertions do not change (US2.4, FR-022):
- `test_interior_exterior_selection` (it binds counting doubles instead of the `0x1`/`0x2`
  sentinels, which a retaining handle would dereference);
- the photon-map frame-release and session tests;
- the imager tests;
- the visual, shading-parity and determinism suites;
- `Parity_transparency-matte-aov` and the AOV scenes.

## Risks

- **Reference-count drift** (a double attach from leftover manual lines, or a leak from a retain
  where an adopt belongs). Mitigated by the mapping table and by T-S1's step-by-step counts. The
  photon tests and a leak-free `stats.numAttributes` balance at `RiEnd` are a second line.
- **Light order.** A change from "tidying" the prepend loops would alter float summation order and
  the `ColorDeterminism_*` bits. T-L1 fails first.
- **Transitive includes** surfacing a real foreign need in Geometry, Texturing or Scene Interface.
  The spec measured none. If one appears, the work stops and is reported; it is never worked
  around with a forbidden include.
- **The hook installation order.** An attribute block built before `rendererShadingServicesInit()`
  would store `usedParameters = 0`. `CRendererContext`'s constructor calls `beginRenderer()`
  before `new CAttributes` (`rendererContext.cpp:109,139`), and T-P1a checks a fresh `RiBegin`
  block.
- **archcheck and doubly compiled State units** (research R11): verified on the first finished run.
  The fallback is Q3.
- **Known flaky tests** (memory: `reference_ctest_flaky_tests.md`). A red one is re-run in
  isolation before it is written off. `ShadingParity_AmbientAccumulation` is a real heap bug, not
  noise.

## Open questions for the maintainer

1. **Light order versus spec wording (US2.3).**
   - **The finding**: the copy constructor and `restore(shading)` reverse the active-light list,
     so inside every `AttributeBegin` or copy-on-write clone lights run in reverse RIB order
     (research R5). The spec says "lights run in RIB order".
   - **Recommendation**: preserve today's order exactly (FR-004 "as today", FR-022), pin it with
     T-L1, and amend US2.3 to read "in the same order as the starting tree". File a GitHub issue
     for the reversal; a fix changes output and belongs to its own `fix/` branch.
2. **FR-003's "MUST NOT call back into them"** versus the parameter hook.
   - **The issue**: State calls Shading's logic through a State-owned function pointer, exactly
     as `pl.cpp` already calls `CRenderer::retrieveVariable` through `variableLookup.h`. This adds
     no edge.
   - **Recommendation**: confirm that FR-003 means no direct reference, and that FR-006 leaves the
     mechanism to the plan, as written.
3. **Production State target (DB-023).**
   - **The issue**: the State-only test compiles the State sources a second time.
   - **The alternative**: a production `ribState_obj` OBJECT library with the same narrow include
     path, linked into ribRender, ribVector and the test. It would compile each State source once,
     and DB-023's tracking line ("every remediation spec narrows include paths for its domain")
     asks for it.
   - **Recommendation**: keep 025 to the test-only target, and take `ribState_obj` up only if
     quickstart §2 shows archcheck mishandling the doubly compiled units. It touches the
     shared/static library composition, which this spec does not otherwise need. The maintainer
     may prefer doing it now.
4. **The bxdf slot in the parameter answers.** Excluded in 025 (research R4). Recommendation:
   confirm it, and leave the decision to spec 035, which binds the first Bxdf.
5. **FR-001's wording on light entries.**
   - **The issue**: FR-001 lists "each entry of the active-light list" among the handles, and says
     a handle "is retained when an attribute block is copied or restored". Today light entries are
     never attached; `CRenderer::allLights` owns them (research R1). The plan follows FR-001's
     overriding "MUST match today's" and keeps them non-owning.
   - **Recommendation**: amend FR-001 to say light-list entries hold a State-owned non-owning
     handle, as today. The analyze stage would otherwise flag the contradiction.

None of these changes a reference-doc decision (D1–D30). D5 is implemented as written: the later of
Surface and Bxdf, saved and restored with the attribute stack, and ribOut statement order checked.
D9 is out of scope (FR-012).

## Complexity Tracking

None. The Constitution Check has no violation to justify.
