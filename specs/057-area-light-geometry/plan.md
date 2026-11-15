# Implementation Plan: Area Light Geometry

**Branch**: `057-area-light-geometry` | **Date**: 2026-11-14 | **Spec**: [spec.md](spec.md) (clarified 2026-11-13)
**Input**: `specs/057-area-light-geometry/spec.md`; master `278b0394`; CLAUDE.md (Spec-kit agent rules, gotchas 10, 11, 17,
22-24, 26, 28, 35-39); `DEVNOTES.md`; `DEVNOTES_DETAILS/{DOMAIN_BOUNDARIES,RISPEC_GAPS,RENDER_ARCHITECTURE_REFERENCES}.md`
(§4.2, §5.6, §6.2, D10, D1, D28); specs 031 (`IRayContext`, FR-021) and 039 (sub-streams).

## Summary

`AreaLightSource` binds every primitive created in its scope to the light: a non-owning slot on `CAttributes`, and a
per-frame emitter registry that the core fills. Geometry samples a light's emitters uniformly by area from two numbers. The
sampling is exact for polygons, meshes and the analytic quadrics. Subdivision limit patches and the other quadrics use a
fixed tessellated table.

For a bound light, `iterateLights` runs the light shader once per sample, with the light-surface globals swapped into the
varying table that the interpreter and the JIT both read. It scales each pushed `Cl` by `V / (n p_A)` and applies no other
factor (Decision A). Occlusion uses a new `ITracer::occluded` probe, which ignores the light's own scope by identity.
Controls come from `Option`/`Attribute "arealight"`. No scene without `AreaLightSource` changes.

**FR-023 probe result (required task, done).** On a fresh build of current master, no occluder casts a shadow through
`quadlight.sl`, flagged or not, because `visibility()` always returns 1, under both the interpreter and the JIT (filed as GitHub #169, D-2). The flag
itself works: `transmission()` is shadowed by a flagged occluder and not by an unflagged one. The defect is described in
[research.md R1](research.md) and filed as #169. 057 neither depends on it nor fixes it.

## Technical Context

**Language/Version**: C++20 (`-Werror`), and RSL for the probe shaders; CMake >= 3.19.
**Primary Dependencies**: none new. The LLVM JIT is unchanged, with no emitter change (research R5).
**Storage**: N/A; memory only (a per-frame registry).
**Testing**:
- CTest, with a new label `arealight` plus `state`, `geometry`, `common`, `visual`, `parity`, `determinism` and `shading_parity`.
- Image tests run through `test_visual_render` and render-pair byte compares.
- Geometry-only unit tests.

**Target Platform**: macOS (Apple Silicon, Clang) and Ubuntu 24.04 (GCC 13); Linux is validated in the last phase.
**Project Type**: renderer library and CLI (`orender`).
**Performance Goals**:
- Scenes without `AreaLightSource`: no measurable change. The added cost is one null test per object in three ray gates and
  one null-pointer test per light per `iterateLights` (the hider installs the sampler only in frames with a bound light).
- Bound lights: about `n` light runs plus `n` shadow rays per shading point.

**Constraints**:
- Byte-identical images without `AreaLightSource` (FR-026).
- No new allowlist line (FR-029).
- Identical images at any thread count (FR-014).
- No change to any shader's compiled bytes (FR-021).
- Minimal change to shader loading and instance creation (FR-032).

**Scale/Scope**: about 25 files touched, 6 new source files, about 20 new tests (contracts/tests.md).

## Constitution Check

*GATE: passed before Phase 0; re-checked after Phase 1 (see the end of this section).*

- **I Clean code**: one bound-light branch in `iterateLights`, and per-kind samplers behind one Geometry entry. No magic
  numbers: the call sites, `K` and the `fast` divisor are named constants.
- **II C++20**: `std::call_once` and `std::vector`; nothing platform-specific.
- **III TDD**: every test in contracts/tests.md is written, seen red, and **approved by the maintainer at its phase's
  TEST-APPROVAL pause before the first implementation task**. The exceptions: (a) the acceptance tests that are green on
  arrival because an earlier phase built the machinery (thread identity, self-exclusion, the soft-shadow probes, the
  occluder flag), verified instead by deliberately breaking the code; (b) the soft-shadow image tests, the hider parity
  tests and the old/new agreement test, whose threshold exists only after a noise measurement: they are registered at
  their approval pauses (thresholds, reference images, agreement) and carry no red state.
- **IV CLI**: the feature is driven by RIB through `orender`; errors go to stderr through the usual codes.
- **V Dependencies**: none added.
- **VI Platforms**: macOS and Linux; the Linux GCC 13 validation is in the last phase.
- **VII Docs**: the site manual's lighting page and the RIB guide (FR-033).

**Principle VIII** (research R2 walks the allowed-dependency table edge by edge):

- [x] Domains touched:
  - Graphics State: the slot, the controls, the vocabulary.
  - Geometry: the registry type, the samplers, the gate, the `IRayContext` accessor.
  - Shading: the interfaces and the execution.
  - Hiders: they implement the interfaces, and the photon hider emits from bound lights.
  - Scene Interface: the tokens.
  - Renderer core: the statements, `addObject`, the frame lifecycle, the declarations.
- [x] No dependency outside the allowed table, and no domain depends on the core. The registry is a Geometry type that the
      core fills; Shading asks the hider through a Shading-owned interface.
- [x] Interface changes are specified in contracts/area-sampling.md, contracts/shading-light-surface.md
      (`kRendererServicesVersion` 2.4.0, decision D-10) and contracts/binding-and-registry.md.
- [x] No hider-specific special case. Occlusion and sampling are `CHider` members shared by every camera hider, and the
      light run is 0-D on every hider.
- [x] No plug-in ABI changes: display, shadeop, dlo and implicit are untouched.
- [x] No registered violation is extended. No `CRenderer::` line is added to `rendererContext.cpp` or `photon.cpp`: the
      hider reads `emitterRegistry()`, not the core.

Post-design re-check: unchanged; all items hold.

## Design overview (by layer; details in the contracts)

1. **Binding** ([contracts/binding-and-registry.md](contracts/binding-and-registry.md)):
   - `RiAreaLightSourceV` sets `CAttributes::areaLight`. `"null"` clears it, and so does the pop at the end of the block.
   - `addObject` records the world-block roots.
   - The registry is created in `beginFrame()`, installed for Geometry and the hiders, and released in `endFrame()`.
   - The `pendingInstanceKey` leak is fixed in the two light statements (research R11).
2. **Area sampling** ([contracts/area-sampling.md](contracts/area-sampling.md)):
   - `CObject::collectEmitters`, resolved once per light on first use (`call_once`), with leaf and face CDFs.
   - `sample(u1, u2)` is pure.
   - An emitter's identity is its attributes' `areaLight` (research R3, R4).
3. **Light-surface execution** ([contracts/shading-light-surface.md](contracts/shading-light-surface.md)), research R5 and R7:
   - `IAreaLightSampler`, Shading-owned, implemented by `CHider`.
   - The composite varying table.
   - Per-sample weighting `V/(n p_A)`.
   - The receiver skip.
   - Fixed sub-streams at `0xFF20`.
4. **Visibility and controls**:
   - `ITracer::occluded` and Geometry's exclusion gate (research R6).
   - `Option`/`Attribute "arealight"` ([contracts/controls.md](contracts/controls.md), research R8).
5. **More primitives**: the quadrics (P2), then patches and NURBS (P3) per decision D-3.
6. **Photon emission** (research R9), then **deprecation and documents** (US9, US11).

## Phases foreseen

The tasks stage fixes the task lists. There is a pause at every phase boundary.

| Phase | Content | Stories / FR | Visible change |
|---|---|---|---|
| 1 | Setup: the plan-stage build and probe (done); `LightKey_NoLeakAcrossFailedLight` red; the leak fix; full suite | FR-004, SC-010 | none: no tracked scene hits the leak (research R11) |
| 2 | Binding: the attribute slot, the statements, the registry type, install and uninstall, recording in `addObject`. Binding tests red, then green | US1, FR-001-FR-004 (FR-005 is checked in Phase 4 by `AreaLightBinding_Illuminate`) | none |
| 3 | Geometry sampling for polygons, meshes and subdivision surfaces; the generic tessellated table; Geometry-only tests | US2, FR-006-FR-010, SC-003 | none |
| 4 | Shading: `IAreaLightSampler`, `ITracer::occluded` (2.4.0), the exclusion gate, the bound-light branch of `iterateLights`. Irradiance probes (interpreter and JIT), constant-`Cl` probe, globals probe | US3, FR-011-FR-016, SC-002 (quad) | bound lights only |
| 5 | Controls through the four attribute layers; read-back, the `ribOut` round trip, errors | US5, FR-018-FR-020 | bound lights only |
| 6 | Soft shadows on reyes, zbuffer and raytrace (quad, mesh and subdivision emitters); self-exclusion; thread identity; noise measurement; default count confirmed; parity thresholds fixed | US4, FR-017, FR-023, FR-031, SC-001, SC-004 | new scenes only |
| 7 | Quadrics (analytic and tessellated); disk irradiance probe | US6, SC-002 (disk) | new scenes only |
| 8 | Photon emission from a bound light; smoke test | US8, SC-008 | none for existing scenes |
| 9 | Deprecation notes (a comment-only edit verified not to change the compiled bytes, else documentation only); old/new quad agreement (D-1) | US9, FR-021, FR-022, SC-006 | new tests only |
| 10 | Optional (D-3): patches, patch meshes, NURBS | US7 | new scenes only |
| 11 | Documents (FR-033), then validation: full suite, default determinism set, sweep (`-j2`), `-L architecture`, Linux GCC 13 `-Werror` | US10, US11, SC-007, SC-009 | none |

**033 sync point**: when spec 033 merges into master, master is merged into this branch at the first phase boundary after
that. The items in the next section are re-applied before the next phase starts. This sync is planned, not a failure
(FR-032).

## Files and interfaces 033 will change

Spec 033 (the `ShadingSystem` facade; shader loading moves into Shading) was not read, per the coordinator's rule. The
table lists what 057 edits in code that 033 is expected to restructure, and how each 057 change is re-applied after 033
lands.

| File / function | 057 change | Re-application after 033 |
|---|---|---|
| `src/ri/render/rendererContext.cpp`: `RiLightSourceV`, `RiAreaLightSourceV` | Both: `pendingInstanceKey = 0` on the failure return. `RiAreaLightSourceV` on success: `attributes->areaLight = cShader` and `declareLight(...)`; on failure: clear the slot | **Settled by the coordinator (2026-11-14)**: 033 leaves these two statements, `extractLightHandle` and `takeInstanceKey` unchanged, so the leak fix stays as written. At the sync only verify it is intact (`LightKey_NoLeakAcrossFailedLight` guards it). The slot and `declareLight` lines follow the statement's success branch; only the light's identity is stored, so a facade handle is fine |
| `rendererContext.cpp`: `extractLightHandle`, `takeInstanceKey` | **Not edited** | Nothing to re-apply: 033 leaves both unchanged |
| `rendererContext.cpp`: `getShader` | **Not edited** | 033 rewrites `getShader` and the binding into `ShadingSystem::bind`. 057 does not depend on its body; only the statements' failure return (row above) must still be reached when the load fails |
| `rendererContext.cpp`: `addObject` (world-block branch) | One `record(o)` call | Not affected by loading; re-applied in place |
| `src/libshader/shading/shading.cpp`: `CShadingContext::iterateLights` | The bound-light branch inside the `lightSources` loop: query, receiver skip, controls, per-sample swap, run, weighting, occlusion | Re-applied as one block inside whatever function then iterates the active lights and calls a light's `prepare()`/`illuminate()`. It needs only the current state's `varying` table and `shadingDim`, `pointKeys`, the `lights` list, the light's identity and shader key, and the context's `IAreaLightSampler`/`ITracer`. **033 changes `lightInstanceOf()` to return `CShaderInstance *`: retype the local that holds the light in the bound block at the sync** |
| `shading.h`: `CShadingContext` | `setAreaLightSampler()` and a pointer member (beside `setTracer()`); the hider installs it at the start of its rendering loop when the frame has a bound light | Re-applied wherever 033 lets the hider install its interfaces on the context it owns |
| `shader.cpp`: `CProgrammableShaderInstance::prepare`/`illuminate` (the light's illuminate entry) | **Not edited**: 057 only calls them, with the composite table | If 033 replaces the entry with a facade call, the composite table is passed the same way: the swap is of `currentShadingState->varying`, which both backends read |
| `tracer.h`, `rendererServices.h` | `ITracer::occluded`; `kRendererServicesVersion` 2.3.0 -> 2.4.0 | 033 steps master to 2.4.0 first: at the sync 057 renumbers to 2.5.0 and moves its history line after 033's (D-10) |
| New `areaLightSampler.h` | Shading-owned interface | If 033 groups Shading's boundary interfaces under the facade, this interface moves with `ITracer` |
| `src/ri/hiders/hider.{h,cpp}` | `CHider` implements `IAreaLightSampler` and overrides `occluded` | Re-applied on the class that owns the context after 033 |
| `src/ri/hiders/photon.cpp`: light loop | The bound-light branch before `prepare()`/`illuminate()`; varying `from`/`N` stepping in `illuminateBegin` | Re-applied in place if 033 routes the light run through the facade |

## Other specs

- **040 (GI determinism)**:
  - At the plan stage (2026-11-14) 040's branch held one design commit and no code; re-check at implementation start.
  - 057 reserves the call sites `0xFF20`-`0xFF2F` (research R7).
  - 057 does not change light enumeration. Bound lights are pushed onto `CRenderer::allLights` by the same `getShader`
    path, in RIB order. So the photon key `cellKey(mix(0xF07011, i), ...)` (`photon.cpp:204`), which includes the light
    index, is unchanged for every existing scene.
  - 040 does not edit `iterateLights`.
- **048 (OIIO)**: no logic overlap, but 048 adds an `Option` through the same four layers as `Option "arealight"`: expect a
  textual collision in the token lists, `options.{h,cpp}`, `rendererDeclarations.cpp` and `RiOptionV`; keep both additions.
- **041 (light service)**: the registry, `CEmitterSet::sample` and `IAreaLightSampler` are what 041 wraps as a non-delta
  light kind with area pdfs. Nothing of 041's interface is anticipated (A2).

## Decisions for the maintainer

| # | Decision | Options | Recommendation |
|---|---|---|---|
| D-1 | The old-vs-new `quadlight` agreement test (SC-006/FR-022), given that `visibility()` never occludes on master (R1) | (a) The agreement tests use `quadlight.rib`'s geometry with no occluding object: its sphere is unflagged, so neither form shadows. The occluded comparison waits for the `visibility()` fix (GitHub #169). (b) Hold SC-006 until the fix lands | **(a)**. It blocks only the occluded variant. The intensity mapping is `I_new = I_old / area` (area 4 for the 2x2 quad). `quadlight.sl` takes its cosine at the quad's centre, a model difference checked against the measured noise |
| D-2 | The `visibility()` defect itself | File it and fix it in a separate change (the fix moves the `quadlight`/`spherelight` references, a re-capture the maintainer approves); or fold it into 057 | **File separately**: filed as GitHub #169. 057 neither needs it nor fixes it |
| D-3 | P3: patches, patch meshes, NURBS | Keep as optional Phase 10 through the generic tessellated sampler (trims: fully trimmed cells are dropped, cells that straddle a trim are kept); or cut to a follow-up issue | **Keep as optional Phase 10**; cut it if the earlier phases overrun, and record the cut in the spec |
| D-4 | CSG leaves as emitters | Sample the whole leaf without its CSG region and warn once; or report CSG leaves as unsupported | **Sample without the region and warn once**. US1-3 binds the leaves, and regions are out of scope |
| D-5 | Routing (research R2): a Geometry-owned registry, installed per frame and filled by the core, with a Shading-owned `IAreaLightSampler` answered by `CHider` | Or the core answers through `CRendererServices` | **As designed**: the evaluation needs the hider's `IRayContext` |
| D-6 | Self-exclusion mechanism (research R6): an `IRayContext` field set by `CHider::occluded` and tested in three Geometry gates | A field on `CRay`; or continuing the ray past an emitter hit | **The `IRayContext` field**: rays are allocated from raw memory at several sites, so a `CRay` field risks being left unset |
| D-7 | Defaults: `samples` 16, `mode full`, and `fast` = `max(1, ceil(n/4))` | Other counts after measurement | **As stated**, with a stated deviation: FR-018/FR-019 ask for the count and the `fast` rule to be measured at plan, but the new estimator does not exist yet, so the plan fixes the rule and Phase 6 measures it (penumbra noise, light stream alone varied). The plan-time numbers (research R10) are the old form's total run-to-run noise, not area-light noise |
| D-8 | Derivatives in a bound light: the light runs as a 0-D batch, so `Du`/`Dv` of the light-surface globals are 0 on every hider | Keep the receiver's dimension. Grid neighbours are then unrelated samples, giving hider-dependent garbage | **0-D** |
| D-9 | Whether a failed `AreaLightSource` (missing or wrong-type shader) also closes the scope | Keep the previous light's scope open | **Close it**, as `"null"` does; the error is already reported |
| D-10 | Interface version: a non-breaking MINOR step (`occluded` has a default) | - | **2.4.0** on this branch (master is 2.3.0 since spec 031), renumbered to **2.5.0** at the 033 sync (033 merges first and steps master to 2.4.0) |

## Decisions approved (2026-11-14)

The maintainer approved every recommendation above. The tasks (tasks.md) are written to these, and the wording below is
binding on the implementation:

- **D-1**: the old-versus-new `quadlight` agreement test (`AreaLight_QuadlightAgreement{,-slo}`) runs with **no occluding
  object**; the occluded variant waits for the `visibility()` fix, GitHub #169. The new form's intensity is the old one's
  divided by the quad's area (`I_new = I_old / area`; area 4 for the 2x2 quad).
- **D-2**: the `visibility()` defect is **filed as GitHub #169**; 057 does not fix it. Wherever the plan, spec, research or
  tests mention the defect they cite #169.
- **D-3**: patches, patch meshes and NURBS stay as **optional Phase 10** through the tessellated sampler (approximate
  trims: wholly trimmed cells dropped, straddling cells kept). Cut if the earlier phases overrun; the go or cut is a
  decision at the Phase 9 pause, and a cut is recorded in the spec (US7, FR-009, A12).
- **D-4**: CSG leaves as emitters are **sampled whole, without their CSG region**, with one warning per light.
- **D-5**: a **Geometry-owned emitter registry filled by the core**; `CHider` answers the Shading-owned `IAreaLightSampler`.
- **D-6**: self-exclusion by a **field on `IRayContext`** (`excludedEmitter()`), not on `CRay`.
- **D-7**: defaults **16 samples, mode `full`**; `fast` = `max(1, ceil(n/4))`; confirmed by measurement in Phase 6 (a
  maintainer pause there fixes the figures; a different count is a deviation recorded in the spec).
- **D-8**: `Du`/`Dv` of the light-surface globals are **0** in a bound light (the run is 0-D).
- **D-9**: a failed `AreaLightSource` (missing or wrong-type shader) **closes the scope** as `"null"` does.
- **D-10 (corrected by the coordinator)**: master's `kRendererServicesVersion` is already **2.3.0** (spec 031). Spec 033
  merges first and steps it to 2.4.0. 057's step for `ITracer::occluded` is the next MINOR after 033's: it is written as
  **2.4.0 on this branch now** (built against master's 2.3.0) and **renumbered to 2.5.0**, with its history entry moved
  after 033's 2.4.0, in the 033 sync (a conditional task block in tasks.md). Every "2.2.0" in spec, plan and contracts
  has been replaced accordingly.

**Maintainer decisions on the tasks stage's additions (2026-11-14, tasks.md approved)**:

- The JIT-off build tree (`build-nojit`) is in, as FR-028 requires.
- No ThreadSanitizer tree, unless the Phase 6 `AreaLight_ThreadIdentity` test fails; then it is a conditional task behind its own pause (tasks.md `tsancond`).
- Kept: the extra edge-case tests, the `ShadingContract_AreaLight{Interp,Jit}` contract-double test and the null-in-production `setEmitterRegistryObserver` hook. The possible photon observer hook stays behind its Phase 8 pause.
- The Linux tree `~/Projects/Development/CLI/openrender-057` is confirmed.

**Coordination with 033** (coordinator, after comparing both plans): 033 changes `lightInstanceOf()` to return
`CShaderInstance *`, so the local in 057's `iterateLights` block is retyped at the sync. 033 rewrites `getShader` and the
binding into `ShadingSystem::bind` but leaves `RiLightSourceV`, `RiAreaLightSourceV`, `extractLightHandle` and
`takeInstanceKey` unchanged, so 057's leak fix there stays as written. Neither spec touches the other's `photon.cpp` or
`addObject` changes. **Spec 048** also adds a new `Option` (token lists, `options.h/.cpp`, `rendererDeclarations.cpp`,
`RiOptionV`): expect a textual collision in those files, resolved by keeping both additions.

## Project Structure

### Documentation (this feature)

```text
specs/057-area-light-geometry/
├── spec.md, checklists/requirements.md   # specify/clarify
├── plan.md                               # this file
├── research.md                           # R1-R12: probe, routing, samplers, execution, occlusion, sub-streams, controls, photon, measurements, leak
├── data-model.md
├── quickstart.md
├── contracts/
│   ├── binding-and-registry.md
│   ├── area-sampling.md
│   ├── shading-light-surface.md
│   ├── controls.md
│   └── tests.md
└── tools/fr023-probe/                    # the probe scenes, shaders, run.sh and the BMP metrics
```

### Source code (repository root)

```text
src/common/sampling.h                     # call-site constants 0xFF20-0xFF22 (+ reserved range)
src/common/riVocabulary.h, riTokens.cpp   # arealight tokens (gotcha 22)
src/ri/ri.h, ri.cpp                       # arealight tokens
src/ri/state/attributes.{h,cpp}           # areaLight slot, controls
src/ri/state/options.{h,cpp}              # scene controls
src/ri/state/stateVocabulary.h            # EAreaLightMode
src/ri/geometry/emitterRegistry.{h,cpp}   # NEW: registry, sets, leaves, resolution
src/ri/geometry/areaSampling.{h,cpp}      # NEW: exact, analytic and tessellated samplers
src/ri/geometry/object.{h,cpp}            # collectEmitters
src/ri/geometry/rayContext.h              # excludedEmitter()
src/ri/geometry/surface.cpp               # the three gates
src/ri/geometry/{polygons,subdivisionCreator,subdivisionLoop,quadrics,delayed,csgSolid,patches}.cpp
src/ri/render/rendererContext.cpp         # statements, addObject, RiOptionV/RiAttributeV
src/ri/render/rendererFrame.cpp           # registry lifecycle
src/ri/render/rendererDeclarations.cpp    # declarations
src/ri/render/rendererShadingServices.cpp # arealight:samples/mode queries
src/libshader/shading/areaLightSampler.h  # NEW: IAreaLightSampler
src/libshader/shading/areaLightControls.h # NEW: pure resolveAreaLightControls (precedence, fast), shared by iterateLights and its unit test
src/libshader/shading/tracer.h            # occluded()
src/libshader/shading/rendererServices.h  # 2.4.0 (2.5.0 after the 033 sync)
src/libshader/shading/shading.{h,cpp}     # iterateLights bound branch, setAreaLightSampler
src/ri/hiders/hider.{h,cpp}, trace.cpp    # CHider implements the sampler, occluded
src/ri/hiders/areaLights.cpp              # NEW: CHider::areaLight/sampleEmitters
src/ri/hiders/photon.cpp                  # bound-light emission
tests/arealight/                          # NEW: tests, probe shaders, scenes; tests/visual/arealight-scenes.cmake registers the image tests
specs/057-area-light-geometry/tools/      # fr023-probe (done), gen_scenes.py, probe_expected.py, sync_linux.sh
examples/rib/tests/arealight-*.rib        # NEW scenes; references under examples/rib/tests/references/
shaders/{quadlight,spherelight}.sl        # comment-only deprecation note if byte-neutral (FR-021)
docs: RISPEC_GAPS.md, DEVNOTES.md, RIB_GUIDE.md, site lighting page, DOMAIN_BOUNDARIES.md (history only),
      RENDER_ARCHITECTURE_REFERENCES.md §13, CLAUDE.md (gotcha + Recent Changes, at the documents phase)
```

**Structure Decision**: the existing single-project layout. New code goes into the domain directories that own it
(Principle VIII). New tests go under `tests/arealight/`, with their scenes in `examples/rib/tests/`.

## Complexity Tracking

No constitution violation to justify.
