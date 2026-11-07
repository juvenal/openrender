# Implementation Plan: Hider and Integrator Registry

**Branch**: `030-hider-and-integrator-registry` | **Date**: 2026-11-07 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/030-hider-and-integrator-registry/spec.md` (clarified
2026-11-06). **Base**: master `e83d005d` (spec 029 merged).

**Rules followed**: the `## Spec-kit agent rules` of `CLAUDE.md` (the worktree copy and the main
checkout's copy are identical, last changed by `14d8ce2d`); constitution v1.3.0, Principle VIII;
`DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md`. No build or test was run for this stage.

## Summary

Hiders stop being chosen by two `strcmp` chains in the renderer core. Each built-in hider defines a
versioned C registration table (name, aliases with optional deprecation text, provided capabilities,
dispatch kind, optional pre-display setup, create/destroy) in its own source file; a Hiders-owned registry
collects the built-ins through a call-graph-reachable list (so a plain static link keeps them, FR-006) plus
static registrars for outside hiders. An integrator registry is built the same way in a new **Transport**
domain (`src/ri/transport/`), with `classic` as the only implementation and the default: a pass-through
`configure`/`prepare`/`finish` lifecycle the core really calls. `RiIntegrator` and the RIB `Integrator`
statement select it through every layer. Hiders declare five capabilities (string + version) that replace
the core-state flags (`HIDER_RGBAZ_ONLY`, `HIDER_NODISPLAY`, `HIDER_ILLUMINATIONHOOK`), the TSM name check
and the `dynamic_cast<CReyes *>`; integrators declare what they require; an unsupported pair **refuses the
frame** before any hider or display is built, through five small core sites, so `RiWorldEnd` passes an
empty frame. `hcshader` is deleted. Reyes reaches its sibling threads through a peer group instead of
`CRenderer::contexts`. Three DB-011 allowlist lines go, DB-013 is resolved, and renders stay byte-identical.

## Technical Context

**Language/Version**: C++20 (renderer), C17 (the two C registration headers and the capability header);
CMake ≥ 3.19, `-Werror` (Clang on macOS, GCC 13 on Ubuntu 24.04).

**Primary Dependencies**: none new. Uses the spec 037 plug-in convention (`orplugin.h`, `OrTableHeader`,
`checkPluginVersion`/`checkPluginTable`, `SPluginFamily`), the spec 028 per-frame window, the spec 029
`CHider` base, the spec 038 RI layers and token lists.

**Storage**: N/A (memory only; no new persistent state; no file format change).

**Testing**: CTest (`ctest --test-dir build ...` through `cpujob`): existing labels `visual`, `parity`,
`determinism`, `determinism-sweep`, `architecture`, `link`, `common`, `state`, `plugin_abi`, the Python and
Lua helper tests; new tests listed below for the maintainer's approval. Byte-identity baseline recorded on
`e83d005d` on macOS and Linux before any source change.

**Target Platform**: macOS (Apple Silicon, deployment target 13.3) and Ubuntu 24.04; no Windows work.

**Project Type**: C++ renderer library + CLI (`orender`) + bindings.

**Performance Goals**: no measurable change (SC-010): two lookups and one pair check per frame; one
virtual call replaces a `dynamic_cast` per submitted object; `hasIlluminationHook()` stays one load.

**Constraints**: byte-identical images and `MaxBlockAvgDiff` on both platforms (FR-035); allowlist
shrink-only (FR-030); no MAJOR version step before spec 055 (both new interfaces start at 2.0.0, Q2); no
task depends on the merge (FR-033); no threshold widened, no approved test or reference edited; the
constitution amendment only with the maintainer's approval at implement.

**Scale/Scope**: about 15 new files (Hiders 5, Transport 9, Core 2), about 25 edited source files, the
deletion of `hcshader.{h,cpp}` (798 lines), 3 allowlist lines deleted, 1 registry entry resolved.

No NEEDS CLARIFICATION remains: the clarify answers are applied as decisions and every choice the spec left
to the plan is settled in [research.md](research.md) (R1-R17), with the ones the maintainer may overrule
listed under "Open questions" (all answered by the maintainer on 2026-11-07, "Maintainer answers").

## Constitution Check

*GATE: checked before Phase 0 and re-checked after Phase 1 (below).*

**Principle VIII - Domain Separation and Modular Boundaries**:

- [x] **Domains touched are named**: Hiders (registry, tables, peer group, `CHider::drawObject`),
      **Transport** (new: capability vocabulary, integrator interface and registry, `classic`), Scene
      Interface (`RiIntegrator`, grammar, lexer, `ribOut`, `CRiInterface`), Graphics State (`COptions`
      fields), Infra (`kindRegistry.h`, a `checkPluginVersion` overload, two RI tokens), Shading (one comment
      in `shaderRefs.h`), renderer core (selection, the per-frame slot, the refused frame, `hcshader`
      deletion), Consumers (Python/Lua helpers, tests, `archcheck`).
- [x] **No new cross-domain dependency outside the allowed table**, conditional on the amendment: the only
      new edge kinds are Hiders → Transport (capability vocabulary) and Core → Transport (allowed, composition
      root). Hiders → Transport becomes allowed by the D30 amendment (FR-029), which the maintainer approves at
      implement; the ordering gate (research R11) keeps every such include out of the tree until then.
      Transport includes only Infra in this spec. No domain gains a dependency on the renderer core: the
      peer group and the frame struct are Hiders-owned types the core fills; `frameCamera()` (Core) is passed
      in, not called.
- [x] **Every added or altered cross-domain dependency has its interface specified**:
      [contracts/hider-registration.md](contracts/hider-registration.md),
      [contracts/integrator-interface.md](contracts/integrator-interface.md),
      [contracts/ri-integrator.md](contracts/ri-integrator.md),
      [contracts/frame-transport.md](contracts/frame-transport.md),
      [contracts/architecture-and-records.md](contracts/architecture-and-records.md).
- [x] **No hider-specific special case** for a primitive or an effect: the opposite, three special cases go
      (the TSM name test, the `CReyes` cast, the name-keyed flag writes); behaviour now follows declared
      capabilities.
- [x] **Changed plug-in interfaces versioned**: no existing family (display, shadeop, dlo, implicit,
      dynamicload) changes. The two new interfaces carry `OR_ABI_VERSION(2, 0, 0)` and a version-history
      table each, checked through one new `SPluginFamily` each (FR-011). `CRendererServices` is not changed,
      so `kRendererServicesVersion` stays 2.1.0. Neither new interface is dlopened in this spec, so the
      "breaking MINOR on a dlopened family" question does not arise.
- [x] **Registered violations not extended; reductions noted**: DB-013 resolved; DB-011 -3 lines
      (164 → 161) and the Hiders metric lowered; DB-017, DB-020 untouched; no allowlist line added or re-keyed.

**Other principles**: III (TDD) - tests are written first and listed for approval below; II - the C
headers are strict C17, the rest C++20; IV - `orender`'s exit code follows the error rules (no new CLI
flag); V - no dependency; VI - macOS and Linux both validated; VII - no Hugo page is required by the spec,
the legacy HTML page is corrected (FR-028).

**Gate result: PASS**, with one condition carried into tasks: the Transport amendment must be approved
before the first Transport file lands (if it is refused, the design has no legal home and implement stops).

## Project Structure

### Documentation (this feature)

```text
specs/030-hider-and-integrator-registry/
├── spec.md
├── checklists/requirements.md
├── plan.md              # this file
├── research.md          # Phase 0: R1-R17
├── data-model.md        # Phase 1
├── quickstart.md        # Phase 1: validation scenarios
├── contracts/
│   ├── hider-registration.md
│   ├── integrator-interface.md
│   ├── ri-integrator.md
│   ├── frame-transport.md
│   └── architecture-and-records.md
└── tasks.md             # Phase 2 (speckit-tasks; not created here)
```

### Source code (repository root)

```text
src/ri/hiders/                      # Hiders
├── hiderAbi.h            NEW  C table, OR_HIDER_ABI_VERSION 2.0.0, version history
├── hiderAbi.hpp          NEW  SHiderFrame, hiderOf/frameOf, OR_REGISTER_HIDER
├── hiderPeers.h          NEW  CHiderPeers (header-only)
├── hiderRegistry.{h,cpp} NEW  CHiderRegistry, hiderRegistry(), kHiderFamily, orHiderAddPending() (out of line: the
│                               pending list must not be a header-reached static, or a shared-library consumer
│                               registers into its own copy)
├── builtinHiders.cpp     NEW  registerBuiltinHiders(): raytrace, reyes, zbuffer, photon
├── hider.h               EDIT virtual void drawObject(CObject *) {}
├── reyes.{h,cpp}         EDIT ctor takes peers; :1992-1993 reads peers; drawObject override
├── stochastic.{h,cpp}    EDIT ctor takes peers; preDisplaySetup deleted; reyes table
├── zbuffer.{h,cpp}       EDIT ctor takes peers; preDisplaySetup deleted; zbuffer table
├── raytracer.{h,cpp}     EDIT preDisplaySetup deleted; raytrace table
└── photon.{h,cpp}        EDIT preDisplaySetup deleted; photon table (ctor/dtor/store path untouched)

src/ri/transport/                   # Transport (NEW domain, D30)
├── capability.h               NEW  OrCapability, vocabulary, reserved names
├── capabilityCheck.{h,cpp}    NEW  transportCheckPair
├── integratorAbi.h            NEW  C table, OR_INTEGRATOR_ABI_VERSION 2.0.0, version history
├── integratorAbi.hpp          NEW  OR_REGISTER_INTEGRATOR
├── integratorRegistry.{h,cpp} NEW  CIntegratorRegistry, integratorRegistry(), kIntegratorFamily
├── builtinIntegrators.cpp     NEW  registerBuiltinIntegrators(): classic
└── classicIntegrator.{h,cpp}  NEW  classic table, lifecycle counters for tests

src/ri/render/                      # renderer core
├── rendererTransport.{h,cpp}  NEW  SFrameTransport, begin/prepare/finish/endFrameTransport
├── renderer.cpp               EDIT chains -> registry; includes; render() drawObject; renderFrame/endFrame guards
├── renderer.h                 EDIT drop 5 HIDER_* constants (keep BREAK=2, DONE=8)
├── rendererContext.{h,cpp}    EDIT RiHiderV via registry; RiIntegratorV; getShader fallback deleted; includes
├── rendererStatics.cpp        EDIT drop the 5 hider includes and hcshader.h
├── rendererDisplay.cpp        EDIT :201, :627, :643, :770 read traits
├── rendererServicesImpl.h     EDIT hasIlluminationHook reads traits
└── (rendererJobs.cpp           unchanged: only HIDER_BREAK/HIDER_DONE)

src/ri/hcshader.{h,cpp}             DELETE
src/ri/state/options.{h,cpp}        EDIT integrator, integratorHandle, integratorParameters
src/ri/parse/{ri.h,ri.cpp,riInterface.h,rib.y,rib.l}   EDIT RiIntegrator(V), grammar, lexer
src/ri/output/ribOut.{h,cpp}        EDIT RiIntegratorV
src/common/{riVocabulary.h,riTokens.cpp}, src/ri/parse/ri.h   EDIT RI_INTEGRATOR, RI_CLASSIC
src/common/kindRegistry.h           NEW  TKindNameTable (header-only)
src/common/pluginAbi.{h,cpp}        EDIT checkPluginVersion overload for built-ins (Q5)
src/libshader/shading/shaderRefs.h  EDIT comment of lightInstanceOf() only
src/python/prman.py, src/lua/prman.lua   EDIT Integrator writers
src/ri/CMakeLists.txt               EDIT sources, transport/*.h glob, ribVector REMOVE_ITEM list, hcshader lines/comments
tests/architecture/                 EDIT domain-rules, domain-map, allowlist (-3), metrics, archcheck class list; NEW 2 fixtures
.specify/memory/constitution.md     EDIT at implement, maintainer-approved amendment 1.3.0 -> 1.4.0
.specify/templates/plan-template.md EDIT (amendment sync) Principle VIII domain list
doc/Documentation/Hardcoded_shaders.html  EDIT (or delete, Q6)
DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md, DEVNOTES_DETAILS/PLUGIN_ABI.md, NEWS, CLAUDE.md, DEVNOTES.md   EDIT (records; CLAUDE.md/DEVNOTES.md at the end)
tests/unit/registry/ (NEW), tests/link/ (NEW test), tests/RIB or tests/unit/rib_parse (NEW cases), tests/python, tests/lua (NEW cases)
```

**Structure decision**: Transport is a directory of `ribRender`, like Hiders (research R11); hider kinds
stay in Hiders, integrator kinds and the capability vocabulary in Transport, the per-frame wiring in the
core.

## Phase outline for tasks (no task may depend on the merge)

1. **Baseline**: record images and `MaxBlockAvgDiff` on `e83d005d` (macOS, Linux).
2. **Tests first (red)**, after the maintainer approves the list below.
3. **Governance gate**: the constitution amendment (maintainer approval), `domain-rules.txt`,
   `domain-map.txt`, the `archcheck` class, the two fixtures. Nothing under `src/ri/transport/` before this.
4. **Transport**: capability vocabulary, pair check, integrator ABI, registry, `classic`; `pluginAbi` overload.
5. **Hiders**: hider ABI, registry, peer group, `drawObject` virtual, the four tables, constructors,
   `preDisplaySetup` deletions.
6. **Core**: `rendererTransport`, `beginFrame`/`renderFrame`/`endFrame`/`render()`, the read sites, `RiHiderV`
   via the registry, includes, `HIDER_*` constants; allowlist -3, metrics.
7. **RI surface**: tokens (three lists), `RiIntegrator(V)`, grammar, lexer, `CRiInterface`, `ribOut`,
   `COptions`, `RiIntegratorV`, Python/Lua.
8. **hcshader retirement**: files, fallback, CMake, domain map, `lightInstanceOf()` comment, legacy page, NEWS.
9. **Validation**: byte identity (macOS, Linux), the sweep, timing (SC-010), the architecture check on both
   platforms; records (DOMAIN_BOUNDARIES, PLUGIN_ABI, then CLAUDE.md and DEVNOTES.md last).

Each phase ends with `ctest --test-dir build -L architecture` (agent rules).

## New tests (approved at plan level 2026-11-07; each is shown again at the implement test pause; no existing test or reference is edited)

| Proposed name | Label(s) | What it proves | Req. |
|---|---|---|---|
| `Registry_HiderLookup` | `registry` | names, plain and deprecated aliases resolve; enumeration lists the four built-ins and aliases in order | FR-001, FR-002, FR-015 |
| `Registry_HiderCollisions` | `registry` | duplicate name, alias = name, alias = alias, unnamed table: refused with the diagnostic, first stays (local registry instance) | FR-004 |
| `Registry_HiderAliasDiagnostics` | `registry` | `Hider "stochastic"` warns once per statement with today's text; `hidden` silent; both store `reyes` | FR-002, FR-023 |
| `Registry_UnknownHiderFallback` | `registry` | `Hider "bogus"`: today's error at frame begin, reyes image; with a TSM display the TSM error still appears | FR-005, FR-007 |
| `Registry_TestDoubleHider` | `registry` | a hider registered from `tests/unit/registry/` by name and alias renders a frame; constructed once per thread; dispatched as declared | FR-003, US2 |
| `Registry_IntegratorLookup` | `registry` | `classic` registered and default; enumeration; collisions refused | FR-010, FR-015 |
| `Registry_PairCheck` | `registry` | pure check: accept, missing, version shortfall (both versions in the text), optional absent | FR-012 |
| `Registry_PairRefusal_<hider>` (4) | `registry` | test-double integrators with each built-in hider: frame refused, one error, no output file, a following frame renders, Debug asserts hold | FR-012, FR-024, SC-006 |
| `Registry_ClassicPairs` | `registry` | `classic` with each built-in hider accepted; lifecycle counted once per rendered frame, zero on a refused one | FR-013, FR-016 |
| `Registry_UnknownIntegratorFallback` | `registry` | `Integrator "bogus"`: error listing names, classic image, `orender` exit -1; with `Hider "bogus"` both fall back | FR-024, US4 AS5 |
| `Registry_IntegratorParameters` | `registry` | unaccepted parameters diagnosed per name; string `handle` consumed; non-string `handle` rejected; frame renders | FR-020, FR-021 |
| `Registry_IntegratorImageIdentity` | `registry`, `visual` | `Integrator "classic"` vs no statement byte-identical for reyes, zbuffer, raytrace (photon: maps identical) | SC-005 |
| `RibParse_IntegratorStatement` | `rib_parse` | parsing, option-block validity (world-block error equals `Hider`'s), last wins, storage in `COptions` | FR-017, FR-018 |
| `RibOut_IntegratorRoundTrip` | `scene_interface` | parse → write preserves the statement and its parameters; preview ignores it silently | FR-019 |
| `Common_VocabularyConsistency` | `common` | *existing, unchanged*: it covers the new tokens automatically | FR-017 |
| `tests/python/test_prman_integrator.py`, `tests/lua/test_prman_integrator.lua` (new files, registered beside the existing helper tests) | as the existing helper tests | `Integrator(...)` writes the statement; the approved `test_prman.py`/`test_prman.lua` stay as they are | FR-017 |
| `Link_StaticConsumerRegistries` | `link` | plain static link finds every built-in hider and `classic`, renders with each (the approved `Link_StaticConsumer` stays as it is, Q7) | FR-006, SC-008 |
| `Architecture_Fixture_transport-allowed` | `architecture` | Transport → Shading and Hiders → Transport pass | FR-030 |
| `Architecture_Fixture_transport-cycle` | `architecture` | Transport → Hiders is a `VIOLATION` and a `CYCLE` | FR-030, US6 AS1 |
| `Hcshader_MissingShader` | `registry` | `LightSource "spherelight"` with an empty shader path: `Failed to find shader "spherelight"` (`CODE_NOSHADER`) | FR-026, US5 AS2 |
| `Hcshader_NoReferences` | `architecture` | a `cmake -P` grep: no `hcshader`/`CSphereLight`/`CQuadLight` outside history records; `tests/architecture/unit/test_archcheck_parsers.cpp` (in-memory prefix-matching data) is exempt (Q8) | SC-007 |
| `Registry_CoreNamesNoHider` | `architecture` | a `cmake -P` grep of the five core files for hider headers, class names, `strcmp(hider` | FR-007, SC-003 |

The label `registry` is new (one `ctest -L registry` entry for CLAUDE.md's Testing section at the end).

**Existing tests and references this design touches** (checked for the plan):
- `Common_VocabularyConsistency` (`tests/unit/common/vocabulary_consistency.cmake`) compares the three token
  lists as sets, with no hard-coded count: adding `RI_INTEGRATOR`/`RI_CLASSIC` to all three needs no edit.
- `archcheck`'s rules parser does not require a row per class (a class without a row may depend only on
  Infra), and the metrics parser accepts any subset of classes: adding `Class::Transport` changes no
  existing fixture's data files. Whether a fixture's pass regex or the determinism test's output depends on
  the class count is verified when the class is added; if one does, that edit comes back for approval
  before it is made.
- `tests/architecture/unit/test_archcheck_parsers.cpp:130-131` uses `src/ri/hcshader.cpp` as in-memory
  example data for prefix matching (its own map string, not the real `domain-map.txt`). It keeps passing
  after the deletion. `Hcshader_NoReferences` exempts that file rather than editing an approved test (Q8).
- No test relies on the C++ `spherelight`/`quadlight` substitute: the visual tests (including
  `Sampling_ThreadSweep_quadlight`, `tests/visual/CMakeLists.txt:4141-4160`, `ENVIRONMENT "${VISUAL_ENV}"`)
  and the convergence scenes (`tests/convergence/CMakeLists.txt:25`) all run with
  `SHADERS=${CMAKE_BINARY_DIR}/shaders`, where the build compiles both shaders; no in-process test names
  either light. FR-027 needs no repoint; the byte-identity run is the proof.

## Overlap with the specs of this round (031, 032, 056)

Full table in [research.md R15](research.md#r15-overlap-with-the-specs-planned-in-the-same-round-031-032-056).
In short: no dependency either way. Shared files are edited in disjoint regions: `renderer.cpp` (030 the
selection lines and the `drawObject` call; 031 `FrameParams` fields near `:456`/`:805`, which 030 leaves
in place); `CHider` (030 one virtual; 032 the sampler installation); `reyes.cpp` (030 the sibling loop
and constructor; 031 the camera statics and the thread-count reads, which 030 deliberately leaves to 031);
`photon.cpp` (030 only `preDisplaySetup` and the appended table; 032 the store path, untouched by 030).
Allowlist: 030 deletes `:127`, `:211`, `:263`, disjoint from 031's and 032's lines (`:211` sits between
031's `:210` and `:213`: a textual merge only). Metrics and registry text: deltas, re-measured on the tree
each spec merges into. Only 030 amends the constitution and touches `domain-rules.txt`, adding a Transport
row and one Hiders target without changing a row 032 relies on. `kRendererServicesVersion` is not stepped
by 030. 056 has no spec text yet, so its overlap cannot be verified (research R15).

## Maintainer answers (2026-11-07, plan approved)

| Q | Answer | Where applied |
|---|---|---|
| Q1 | Refused-frame design R8 approved as written | research R8; contracts/frame-transport.md |
| Q2 | Both interfaces start at **2.0.0**; version 1 = "chosen by name in the core" / "no integrator" | research R10; contracts/hider-registration.md, contracts/integrator-interface.md |
| Q3 | `HIDER_PHOTONMAP_OVERWRITE` deleted with no replacement capability (set-only at `photon.cpp:118`, defined at `renderer.h:51`) | research R3; data-model.md; spec R17 |
| Q4 | Unaccepted integrator parameters are **errors** (`orender` exits -1, like an unknown `Option`) | research R9; contracts/integrator-interface.md, contracts/ri-integrator.md |
| Q5 | Built-in tables checked through the new **`checkPluginVersion` overload** (one version check, gotcha 27) | research R10; contracts/hider-registration.md |
| Q6 | `doc/Documentation/Hardcoded_shaders.html` **replaced by a retirement note**, not deleted | research R12; contracts/architecture-and-records.md |
| Q7 | New **`Link_StaticConsumerRegistries`**; the approved `Link_StaticConsumer` untouched; spec wording updated (US2 AS3, FR-006) | research R13; spec.md |
| Q8 | `tests/architecture/unit/test_archcheck_parsers.cpp`'s test data **exempted** from the no-reference check; the approved test is not edited | "Existing tests" note; `Hcshader_NoReferences` row |
| Tests | The 22-row new-test list and the `registry` label approved at plan level; each test is still shown at the implement test pause before it is applied | "New tests" |
| Amendment | Constitution 1.3.0 → 1.4.0 (D30) stays an implement-time approval point | research R11; contracts/architecture-and-records.md |

Spec corrections from R16-R19 were applied to spec.md (Clarifications, "2026-11-07 (plan-stage
corrections)"). The questions as they were asked are kept below for the record.

## Open questions for the maintainer (answered 2026-11-07, see the table above)

- **Q1 (approval, FR-024)**: the refused-frame design (research R8, contracts/frame-transport.md): the
  pair is checked before displays and hiders, a refused frame builds neither, the world block runs against
  an empty frame, `renderFrame()` returns at once, `endFrame()` skips the hider deletion. Five core sites,
  no hider/Geometry/Shading change. Judged not too invasive, so the plan did not stop; approve or pick an
  alternative (R8 lists the rejected ones).
- **Q2 (versions)**: both new interfaces start at **2.0.0** (version 1 = "chosen by name in the core" /
  "no integrator"), so the milestone ends at MAJOR 2 with no MAJOR step. Confirm, or start at 1.0.0.
- **Q3 (FR-008)**: `HIDER_PHOTONMAP_OVERWRITE` is never read anywhere; the plan deletes it without a
  capability. Confirm, or keep a declared-but-unread `photon-map-output`.
- **Q4 (FR-021)**: unaccepted integrator parameters are reported as **errors** (`orender` exits -1, like an
  unknown `Option`), not warnings. Confirm, or prefer warnings.
- **Q5 (FR-011)**: built-in tables are version-checked through a new **overload** of `checkPluginVersion`
  that takes the declared version (the module form shares the same verdict code). Confirm, or check
  built-ins by `checkPluginTable` only and leave `checkPluginVersion` to spec 051.
- **Q6 (FR-028)**: replace `doc/Documentation/Hardcoded_shaders.html` with a retirement note (plan), or
  delete it.
- **Q7 (FR-006)**: prove static registration with a **new** `Link_StaticConsumerRegistries` (plan, leaves the
  approved test untouched), or extend `Link_StaticConsumer` as the spec's wording says.
- **Q8 (SC-007)**: `tests/architecture/unit/test_archcheck_parsers.cpp` keeps `src/ri/hcshader.cpp` as
  example data for prefix matching; the plan exempts it from the "no reference" check instead of editing
  the approved test. Confirm, or approve changing the example path.
- **Also for approval**: the new-test list above; the constitution amendment text (contracts/architecture-and-records.md)
  at implement.

## Constitution Check (post-design)

Re-checked against the contracts: the only cross-domain edges the design adds are Core → Hiders/Transport
(allowed), Hiders → Transport (`hiderAbi.h` includes `transport/capability.h`; allowed after the
amendment), and Hiders/Transport → Infra (allowed). No Transport file includes a Hiders, Scene Interface,
Graphics State or Core header; Graphics State stores the integrator choice as plain strings and a typed list
and includes no Transport header; Shading is untouched except a comment. The renderer core still names no
concrete hider. **PASS**, with the amendment condition above.

## Complexity Tracking

No constitution violation to justify. The one addition that needs a reason is the Infra header
`kindRegistry.h`: the hider and integrator registries share their name/alias rules, and the only place both
domains may reach is Infra (Transport may not depend on Hiders, and putting the rules in Transport would make
Hiders reuse an integrator-domain helper for a non-integrator purpose).
