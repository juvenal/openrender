# Implementation Plan: Graphics State Handles

**Branch**: `025-graphics-state-handles` | **Date**: 2026-10-25 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/025-graphics-state-handles/spec.md`. Clarifications
from the 2026-10-25 session and from the maintainer's answers to the plan questions (session
2026-10-26) are applied as decided. Revised 2026-10-26 for FR-025 and the measured baselines.

## Summary

Graphics State (`src/ri/state`) stops holding other domains' objects. It resolves DB-008 and moves
the registry part of DB-021. The work, in the order the brief (plan step V5) gives:

1. **Handles.**
   - Two empty State-owned bases, `CShaderGroupRefTarget` and `CMapRefTarget`.
   - A header-only RAII handle, `ShaderGroupRef`/`MapRef`.
   - `CShaderInstance` and `CPhotonMap` derive from the new bases, which the dependency table
     allows (Shading → State and Hiders → State).
   - The five shader slots and `COptions::imager` hold `ShaderGroupRef` (phase 4); the new `bxdf`
     slot follows with the last-bound record in phase 5. The photon maps hold `MapRef`; light-list
     entries stay non-owning.
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
6. **Light order (FR-025, maintainer decision 2026-10-26).**
   - The copy constructor and `restore(shading)` copy the active-light list in order through one
     helper. Today they prepend, so every `AttributeBegin` and copy-on-write clone reverses the
     list.
   - This is the only output-changing step, so it lands in its own phase after every
     behaviour-preserving phase.

Tests are written first:
- a State-only test that compiles `src/ri/state` with a narrow include path and links only
  libRIBCommon;
- a vocabulary consistency test;
- a parameter-equivalence test, whose part A is an API-neutral oracle that is green on the
  starting tree;
- `State_LightOrder`, written and measured red on `dfffba0` (4 of 9 checks fail);
- a ribOut statement-order characterization test, green on both trees.

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
- New label `state`: `State_Isolation`, `State_LastBound`, `State_PatternTable`,
  `State_ChannelRegistry`, `State_LightOrderRestore`, `State_VocabularyConsistency` and
  `State_LightOrder`.
- `shading_parity`: `ShadingParity_ShaderParameterEquivalence` (part A) and
  `ShadingParity_ShaderParameterEquivalenceDirect` (part B, a separate test) and
  `ShadingParity_ShaderParameterBxdfIgnored` (FR-005: a bound `bxdf` changes neither answer).
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
- byte-identical renders in every phase except the FR-025 light-order phase (FR-022);
- a reference re-render only with the maintainer's explicit approval at that time (FR-025);
- no threshold widening;
- no task depends on the merge;
- `cpujob` for every build and test: one job at a time machine-wide, `nice -n 19`, at most `-j4`.

**Scale/Scope**:
- about 20 source files changed (roughly 60 slot sites);
- 7 State files added: `stateHandles.h`, `stateVocabulary.h`, `patternTable.h`,
  `shaderParameterHooks.{h,cpp}` and `channelRegistry.{h,cpp}`;
- 1 Shading header (`shaderRefs.h`) and 1 Shading unit (`shaderParameters.{h,cpp}`) added;
- 11 new test executables (9 added in phase 2: the 6 `state` tests other than `State_LightOrder`, the Direct equivalence test, the bxdf-ignored test and `RibOut_ShaderStatementOrder`; 2 written at the plan stage: `State_LightOrder` and equivalence part A) and their fixture shaders added;
- the architecture data files and the registry updated.

**Measurements** (macOS, `build-025` at `dfffba0` plus the new test files, 2026-10-26; details
in [validation.md](validation.md)):
- **Architecture check**: PASS, with `ALLOWED DB-008 x 7`, the TOTAL metric at 721/110 (baseline
  721/110), and no NOTE or WARNING.
- **Full suite** (`-LE determinism-sweep -j4`): **784/784 passed** for the suite as it stood before the red `State_LightOrder` was in the build; the tasks stage re-takes the baseline total (T003).
- **Part A**: 64/64 green. The precedence-swap mutation gives 4 failures.
- **`State_LightOrder`**: red, with 4 of 9 checks failing. With the FR-025 fix applied in a
  scratch copy it is 9/9 green.
- **FR-025 impact**:
  - **Scenes**: 82 of the 489 `examples/rib` scenes change light order for at least one object,
    and 76 of them are rendered by tests.
  - **Tests concerned**: 66 visual and parity tests and 71 determinism-sweep tests.
  - **Visual and parity results**: with the fix, all 66 pass at unchanged thresholds, and every
    `MaxBlockAvgDiff` equals the starting tree's.
  - **Output images**: all 95 that could be compared are **bit-identical** to the starting tree's,
    so **no reference re-render is needed**.
  - Research R5 and the FR-025 section below have the detail.
- **libshader_shading flat-namespace imports**: 7 (spec 024's value).
- **Slowest three visual scenes**: timed for SC-005 (validation.md §0.6).
- **Linux**: not yet measured (quickstart §4).

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
    a production State target is not part of 025 (maintainer, 2026-10-26).
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
├── spec.md, plan.md, research.md, data-model.md, quickstart.md, validation.md
├── tools/                      # FR-025 probe/scan scripts, probe and fix-sketch diffs, measured data
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
src/ri/CMakeLists.txt           # + state/channelRegistry.cpp in ri_sources AND ribOut_sources (ribOut compiles
│                               #   rendererDeclarations.cpp); + state/shaderParameterHooks.cpp in ri_sources
│                               #   (ribVector inherits ri_sources; ribOut has no attributes.cpp)
src/ri/render/
├── stateConversions.h          # NEW, core-private: ParameterType <-> EDisplayParameterType helper + static_asserts
├── renderer.h                  # - declaredChannels, displayChannels, declareDisplayChannel x2, resetDisplayChannelUsage
├── rendererStatics.cpp         # - the two channel statics
├── rendererDeclarations.cpp    # begin/endChannelRegistry, declaration hook, out-of-line retrieveDisplayChannel forwarder
├── rendererShadingServices.cpp # install/uninstall the shader-parameter hooks
├── rendererContext.cpp         # Ri*V shader binders, WorldBegin defaults, RiImagerV, RiDisplayChannelV, display-param types (RiLightSourceV/RiIlluminate need no edit)
├── rendererDisplay.cpp         # registry calls; ParameterType conversion
├── renderer.cpp                # lazy MapRef binding; imagerShader = shaderInstanceOf(o->imager)
└── geometryDispatch.cpp        # attributes->displacement() presence check
src/ri/hcshader.{h,cpp}         # only if the CShaderInstance base change needs it (expected: none)
tests/unit/state/               # NEW: test_light_order.cpp + CMakeLists.txt (written at the plan stage);
│                               #      phase 2: test_state_isolation.cpp, test_state_last_bound.cpp,
│                               #      test_state_pattern_table.cpp, test_state_channel_registry.cpp,
│                               #      test_state_light_restore.cpp, test_state_vocabulary.cpp, state_test_doubles.h
tests/CMakeLists.txt            # + add_subdirectory(unit/state) (plan stage)
tests/unit/rib_parse/test_ribout_shader_order.cpp   # NEW
tests/shading_parity/test_shader_parameter_equivalence.cpp + CMake entry  # NEW, written at the plan stage (part A)
tests/shading_parity/shader_parameter_fixtures.h, test_shader_parameter_equivalence_direct.cpp, test_shader_parameter_bxdf_ignored.cpp  # phase 2: fixtures shared with part A; part B as a separate test
tests/unit/csg/test_interior_exterior_selection.cpp, tests/imager/*.cpp      # reach the object through the handle
tests/architecture/allowlist.txt                     # - 7 DB-008 lines
DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md, DEVNOTES.md, CLAUDE.md
```

**Structure decision**: the existing layout is kept.
- New State code goes in `src/ri/state/` and new Shading code in `src/libshader/shading/`. Both
  directories are already classified in `domain-map.txt`, so no domain-map change is needed.
- `tests/unit/state/` is under `tests/`, which archcheck does not analyse as a domain.
- The State-only tests compile the State sources a second time with a narrow include path. This
  follows the `tests/framebuffer` precedent; see research R11. Because archcheck exits 2 on any
  missing in-scope object (including those of `EXCLUDE_FROM_ALL` targets), the State-only library
  and executables are defined only behind the CMake option `OPENRENDER_STATE_ONLY_TESTS`, off by
  default until the handles phase switches it on.

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
5. **Light order (R5, FR-025)**:
   - One `copyLightList()` helper, used by the copy constructor and by `restore(shading)`, copies
     the list in order.
   - The behaviour-preserving phases keep today's prepend loops, now over handle types.
   - The fix lands in its own phase, the only one allowed to change output.
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
5. **The `bxdf` slot, the record and the pattern table.**
6. **The registry.**
7. **Light order (FR-025): the only output-changing phase.**
   - `copyLightList()` replaces the two prepend loops, and `State_LightOrder` turns green.
   - Re-run the FR-025 affected tests and compare them with the baselines in validation.md §0.5
     (diff values and the bitwise image comparison). Then run the full suite and the determinism
     set.
   - **If any reference result changes**, list the scene, the old and new diff, and the reason
     (light order), then **stop and ask the maintainer**. No re-render without that approval, and
     no threshold change. The plan-stage measurement predicts no change.
8. **Registry text and docs, then Linux.**

Each phase ends with `ctest -L architecture` and the suite labels its changes touch. The check stays
green throughout phases 3 and 5 to 8.

**Expected red through phases 3 to 6**: `State_LightOrder` is the one test written red at the start
(phase 2) that stays red until phase 7. Every phase report lists it as the known red test. Any other
red test is a regression.

## Test design (red first)

| Id | Test | Red on `dfffba0` because | Covers |
|---|---|---|---|
| T-S1 | the State-only tests (label `state`): `State_Isolation` (handles and counts, US1), `State_LastBound` (US3), `State_PatternTable` (US4), `State_ChannelRegistry` (US5) and `State_LightOrderRestore` (FR-025, disabled until phase 7). They compile the State sources with counting doubles, on a narrow include path, and link only `openrendercommon` | build failure: `shader.h`, `dsply.h`, `blobbyField.h` and `photonMap.h` do not resolve; the new headers do not exist | US1.3, US3.1–3.4, US4.1–4.4, US5.1–5.3, FR-001, FR-004, FR-010–FR-017, SC-006, the empty-bind record, and FR-025's direct `restore()` order check (which the Ri-level save/restore round trip cannot isolate) |
| T-S2 | `State_VocabularyConsistency` (label `state`) | build failure (the header is missing); then a value mutation is shown red | FR-008, SC-007 |
| T-P1 | `ShadingParity_ShaderParameterEquivalenceDirect` (part B, a separate test sharing part A's fixtures header) | build failure (the `libshader::` API is missing) | FR-005–FR-007 |
| T-P2 | `ShadingParity_ShaderParameterBxdfIgnored` (a bound `bxdf` changes neither `usedShaderParameters` nor `findShaderParameter`) | build failure (`CAttributes::bindBxdf` is missing); enabled with the `bxdf` slot in phase 5 | FR-005, clarification 2026-10-26 |
| T-P1a | the same test, part A (API-neutral oracle, `tests/shading_parity/test_shader_parameter_equivalence.cpp`, already written) | **green by design** on both trees: 64/64 on `dfffba0`. The precedence-swap mutation gives 4 failures (measured) | FR-007 ("fails on a build where the answer differs"), US2.2 |
| T-L1 | `State_LightOrder` (`tests/unit/state/test_light_order.cpp`, links ribRender, already written): lights in the world block, a single and a double `AttributeBegin`, a copy-on-write clone, `RiIlluminate` off/on, a save/restore round trip, pointer identity against the `RiLightSource` handles | **measured red by assertion**: 4 of 9 checks fail on `dfffba0` (single push, copy-on-write clone, and the two that follow). Green with the fix (9/9, scratch copy) | FR-004, FR-025, SC-010, US2.3, R3, R5 |
| T-R1 | `RibOut_ShaderStatementOrder`: the seven shader statements in several orders among other attribute statements, written by ribOut and compared line by line | **characterization, green on both trees** (clarification Q3) | US3.5, FR-023 |
| T-A1 | `ctest -L architecture` after deleting the 7 DB-008 lines at the start of the handles phase (step 4) | 7 `VIOLATION` lines until that phase ends; any other violation means the design is wrong | FR-019, SC-001, SC-002, SC-009 |

Existing guards whose assertions do not change (US2.4, FR-022):
- `test_interior_exterior_selection` (it binds counting doubles instead of the `0x1`/`0x2`
  sentinels, which a retaining handle would dereference);
- the photon-map frame-release and session tests (`photonmap_FrameCacheRelease`, `photonmap_TwoFrameSession`, `photonmap_ObjectDefinitionRelease`) pin frame-end release and session reuse of the maps now held by `MapRef`;
- the imager tests (`ImagerOptionTests`, `ImagerGuardTests`, `ImagerExecutionTests`) pin the `imager` slot's binding and use;
- the visual, shading-parity and determinism suites pin rendered output and bit-identity;
- `Parity_transparency-matte-aov` and the AOV scenes pin the channel registry's behaviour.

## Risks

- **Reference-count drift** (a double attach from leftover manual lines, or a leak from a retain
  where an adopt belongs). Mitigated by the mapping table and by T-S1's step-by-step counts. The
  photon tests and a leak-free `stats.numAttributes` balance at `RiEnd` are a second line.
- **Light order.**
  - Changing the prepend loops before phase 7 would mix an output change into a
    behaviour-preserving phase.
  - In phase 7 the fix changes float summation order in 82 scenes. The plan-stage measurement saw
    no 8-bit output change, but a reference *could* still differ on Linux or after other changes.
    Any such case is listed and brought to the maintainer, never absorbed by a threshold.
- **Transitive includes** surfacing a real foreign need in Geometry, Texturing or Scene Interface.
  The spec measured none. If one appears, the work stops and is reported; it is never worked
  around with a forbidden include.
- **The hook installation order.** An attribute block built before `rendererShadingServicesInit()`
  would store `usedParameters = 0`. `CRendererContext`'s constructor calls `beginRenderer()`
  before `new CAttributes` (`rendererContext.cpp:109,139`), and T-P1a checks a fresh `RiBegin`
  block.
- **archcheck and doubly compiled State units** (research R11): the State-only targets are behind
  `OPENRENDER_STATE_ONLY_TESTS`, off until the handles phase, because archcheck needs every in-scope
  object built. The first run with the option on verifies the duplicate edges. If archcheck
  mishandles them, implementation stops and reports it.
- **Known flaky tests** (memory: `reference_ctest_flaky_tests.md`). A red one is re-run in
  isolation before it is written off. `ShadingParity_AmbientAccumulation` is a real heap bug, not
  noise.

## Decisions on the plan questions (maintainer, 2026-10-26)

Recorded as clarifications in spec.md (session 2026-10-26):

1. **Light order**: fixed in 025 as FR-025 / SC-010, in its own phase (phase 7). See research R5
   and the FR-025 section below.
2. **FR-003** "MUST NOT call back" means no direct reference. The State-owned hooks
   (`shaderParameterHooks.h`, `variableLookup.h`) are fine.
3. **No production State build target** (`ribState_obj`) in 025. The State-only test compiles the
   State sources itself (research R11). If archcheck mishandles that, implementation stops and
   reports it.
4. **The bxdf slot** stays out of the parameter answers until spec 035 (FR-005).
5. **FR-001**: light-list entries hold a non-owning pointer to the State-owned target type, as today.

## FR-025 affected scenes and references

Measured on `dfffba0` at the plan stage, macOS. Raw data is in `validation.md` §0.5.

- **82 scenes** change the light order of at least one object; 76 of them are rendered by a test.
  The cause in every case is the same: each `AttributeBegin` or copy-on-write clone reverses the
  list of a block that holds two or more lights.
- **Tests that compare against a stored reference image** (`Visual_*`, `Sampling_ThreadSweep_*`):
  - with the fix, every one passes at its unchanged threshold;
  - each `MaxBlockAvgDiff` is identical to the starting tree's;
  - every compared output image is bit-identical (95 images).
  - **No reference needs re-rendering.** The float-level summation-order change does not survive
    the 8-bit output in these scenes.
- **Parity tests** (hider against hider, in the same tree) pass with identical diffs.
- **The 71 `DeterminismSweep_*` tests** compare configurations within one tree, so a light-order
  change cannot fail them. They are re-run with the fix (validation.md §0.5).
- **Not observed by the probe**:
  - light order inside procedurals expanded at render time. Of the 7 procedural scenes, 2 have
    two or more lights; one of them is already in the list;
  - the second frame of photon scenes where the known `CPhotonHider` race killed it in one of the
    runs.

  Both are covered by running the full suite in phase 7.
- **If phase 7 (or the Linux run) shows a reference change after all**: the plan lists it with the
  scene, its old and new diffs and the reason, and stops for the maintainer's approval. It never
  re-renders on its own and never touches a threshold.

The affected scenes:
- **Lights, scene files and gather**: `bowl`, `gumbo`, `quadlight`, `quadlight-slo`,
  `quadrics-a`, `quadrics-b`, `spherelight`, `spherelight-slo`, `spotlight`, `spotlight-reyes`,
  `spotlight-raytrace`, `sphere-gather-reyes`, `sphere-gather-reyes-slo`, `vasegallery`.
- **Blobby**: `blobby-parse-smoke`, `blobby-repeller-{heights,shaping}-{reyes,raytrace,zbuffer}`,
  `blobby-tolerance-inherited-{reyes,raytrace,zbuffer}`.
- **CSG**:
  - `csg-hero-{reyes,raytrace,zbuffer}-{800,1600}`, `csg-hero-two-shaders-{reyes,raytrace}`;
  - `csg-open-quadric-caps-{reyes,raytrace,zbuffer}`;
  - `csg-procedural-open-cylinder{,-inline}-raytrace`;
  - `csg-secondary-rays-{reyes,raytrace}`;
  - `csg-sphere-cube-difference-{reyes,raytrace,zbuffer}-{800,1600}`;
  - `csg-sphere-sphere-{loose,tight}-tolerance-raytrace`, `csg-tolerance-inert-raytrace`.
- **Trimmed NURBS**: `nupatch-vase-trimmed-{hole,hole-raytrace,multiloop}`,
  `nupatch-vase-trimmed-malformed-{badweight,instanced,unclosed}`.
- **Parity scenes**:
  - `parity/displacement-{reyes,raytrace}`, `parity/dof-motion-{reyes,raytrace}`;
  - `parity/motion-{patches,polygons,quadrics}-{deform,translate}-{reyes,raytrace}`;
  - `parity/motion-subdiv-rotate-{reyes,raytrace}`,
    `parity/motion-subdiv-translate-{reyes,raytrace,photon,wire}`.
- **Subdivision photon scenes**: `subdiv-{facevarying-seam,hierarchical-override,loop,new-tags}-photon`.
- **Torus**: `torus-{reyes,raytrace}`.

None of these changes a reference-doc decision (D1–D30). D5 is implemented as written: the later of
Surface and Bxdf, saved and restored with the attribute stack, and ribOut statement order checked.
D9 is out of scope (FR-012).

## Complexity Tracking

None. The Constitution Check has no violation to justify.
